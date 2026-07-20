#include "internal.h"
#include "log.h"
#include "network.h"

static int build_peer_list_from_snapshot(uint8_t *buf, const msg_peer_entry_t *entries,
                                         int entry_count, int *sent_count) {
    uint8_t *payload = buf + TUND_HDR_SIZE;
    int offset = 0;
    int count = 0;

    for (int i = 0; i < entry_count; i++) {
        if (offset + (int)sizeof(msg_peer_entry_t) > TUND_MAX_PAYLOAD) break;
        memcpy(payload + offset, &entries[i], sizeof(entries[i]));
        offset += (int)sizeof(msg_peer_entry_t);
        count++;
    }

    *sent_count = count;
    proto_write_hdr(buf, MSG_PEER_LIST, (uint16_t)offset);
    return TUND_HDR_SIZE + offset;
}

static void server_handle_register(server_t *srv, const uint8_t *payload, uint16_t plen,
                                   const struct sockaddr_in *from, uint64_t sequence) {
    uint32_t vip = 0;
    char peer_name[TUND_NAME_LEN] = "";
    struct sockaddr_in peer_addr;
    msg_peer_entry_t peer_entries[TUND_MAX_PEERS];
    struct sockaddr_in join_dests[TUND_MAX_PEERS];
    int peer_entry_count = 0;
    int join_dest_count = 0;
    bool reconnect = false;

    memset(&peer_addr, 0, sizeof(peer_addr));
    memset(peer_entries, 0, sizeof(peer_entries));
    memset(join_dests, 0, sizeof(join_dests));

    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        if (!proto_replay_accept(&srv->peers[idx].replay, sequence)) {
            snprintf(peer_name, sizeof(peer_name), "%s", srv->peers[idx].name);
            pthread_mutex_unlock(&srv->peers_lock);
            LOG_DEBUG("Dropped replayed REGISTER from %s", peer_name);
            return;
        }
        srv->peers[idx].last_seen = time(NULL);
        vip = srv->peers[idx].virt_ip;
        snprintf(peer_name, sizeof(peer_name), "%s", srv->peers[idx].name);
        memcpy(&peer_addr, &srv->peers[idx].real_addr, sizeof(peer_addr));
        reconnect = true;
    } else {
        idx = server_find_free_slot(srv);
        vip = (idx >= 0) ? server_alloc_ip(srv) : 0;
        if (idx < 0 || vip == 0) {
            pthread_mutex_unlock(&srv->peers_lock);
            LOG_WARN(idx < 0 ? "Peer table full, rejecting connection"
                             : "IP pool exhausted, rejecting connection");
            return;
        }

        peer_t *p = &srv->peers[idx];
        p->active = true;
        p->virt_ip = vip;
        p->bytes_in = p->bytes_out = p->rtt_ms = 0;
        p->has_rtt = false;
        proto_replay_reset(&p->replay);
        proto_replay_accept(&p->replay, sequence);
        memset(p->name, 0, TUND_NAME_LEN);
        if (plen > 0)
            memcpy(p->name, payload,
                   (size_t)((plen < TUND_NAME_LEN - 1) ? plen : TUND_NAME_LEN - 1));
        else
            snprintf(p->name, TUND_NAME_LEN, "peer-%d", idx);
        memcpy(&p->real_addr, from, sizeof(*from));
        p->last_seen = time(NULL);
        srv->peer_count++;

        snprintf(peer_name, sizeof(peer_name), "%s", p->name);
        memcpy(&peer_addr, &p->real_addr, sizeof(peer_addr));

        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (!srv->peers[i].active || i == idx) continue;

            msg_peer_entry_t *entry = &peer_entries[peer_entry_count++];
            entry->virt_ip = srv->peers[i].virt_ip;
            memset(entry->name, 0, TUND_NAME_LEN);
            strncpy(entry->name, srv->peers[i].name, TUND_NAME_LEN - 1);
            entry->status = 1;

            memcpy(&join_dests[join_dest_count++], &srv->peers[i].real_addr, sizeof(join_dests[0]));
        }
    }
    pthread_mutex_unlock(&srv->peers_lock);

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_assign(buf, vip, htonl(TUND_NETMASK), TUND_TUN_MTU);
    if (net_send(srv->sockfd, buf, len, &peer_addr) < 0)
        LOG_WARN("Failed to send ASSIGN to %s peer %s", reconnect ? "reconnecting" : "new",
                 peer_name);

    if (reconnect) {
        LOG_INFO("Peer %s reconnected (refreshed)", peer_name);
        return;
    }

    char vip_str[TUND_IP_STR_LEN], from_ip[TUND_IP_STR_LEN];
    LOG_INFO("★ New peer: %s → %s [%s:%u]", peer_name, ip_to_str_buf(vip, vip_str, sizeof(vip_str)),
             ip_to_str_buf(from->sin_addr.s_addr, from_ip, sizeof(from_ip)), ntohs(from->sin_port));

    int sent_peer_count = 0;
    len = build_peer_list_from_snapshot(buf, peer_entries, peer_entry_count, &sent_peer_count);
    if (net_send(srv->sockfd, buf, len, &peer_addr) < 0)
        LOG_WARN("Failed to send peer list to %s", peer_name);
    else
        LOG_DEBUG("Sent peer list (%d peers) to %s", sent_peer_count, peer_name);

    len = proto_build_peer_join(buf, vip, peer_name);
    for (int i = 0; i < join_dest_count; i++) {
        if (net_send(srv->sockfd, buf, len, &join_dests[i]) < 0)
            LOG_WARN("Peer join broadcast for %s failed", peer_name);
    }
}

static bool server_accept_registered_sequence(server_t *srv, const struct sockaddr_in *from,
                                              uint64_t sequence) {
    char peer_name[TUND_NAME_LEN] = "";
    bool known = false;
    bool accepted = true;

    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        known = true;
        accepted = proto_replay_accept(&srv->peers[idx].replay, sequence);
        snprintf(peer_name, sizeof(peer_name), "%s", srv->peers[idx].name);
    }
    pthread_mutex_unlock(&srv->peers_lock);

    if (known && !accepted) LOG_DEBUG("Dropped replayed packet from %s", peer_name);
    return accepted;
}

static void server_handle_disconnect(server_t *srv, const struct sockaddr_in *from) {
    server_peer_snapshot_t leave_peers[TUND_MAX_PEERS];
    int leave_count;
    uint32_t vip;

    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        return;
    }

    peer_t *p = &srv->peers[idx];
    char peer_ip[TUND_IP_STR_LEN];
    LOG_INFO("✦ Peer disconnected: %s (%s)", p->name,
             ip_to_str_buf(p->virt_ip, peer_ip, sizeof(peer_ip)));

    vip = p->virt_ip;
    p->active = false;
    srv->peer_count--;
    leave_count = server_snapshot_broadcast_locked(srv, leave_peers, TUND_MAX_PEERS, -1);
    pthread_mutex_unlock(&srv->peers_lock);

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_peer_leave(buf, vip);
    server_send_peer_snapshots(srv, leave_peers, leave_count, buf, len, 0);
}

void server_handle_packet(server_t *srv, uint8_t *buf, int len, const struct sockaddr_in *from) {
    uint8_t type;
    uint16_t payload_len;
    int hdr = proto_read_hdr(buf, &type, &payload_len);
    uint64_t sequence = 0;
    if (hdr < 0 || TUND_HDR_SIZE + payload_len > len || !proto_read_sequence(buf, &sequence)) {
        char from_ip[TUND_IP_STR_LEN];
        LOG_DEBUG("Invalid packet from %s:%u",
                  ip_to_str_buf(from->sin_addr.s_addr, from_ip, sizeof(from_ip)),
                  ntohs(from->sin_port));
        return;
    }

    if (type != MSG_REGISTER && !server_accept_registered_sequence(srv, from, sequence)) return;

    uint8_t *payload = buf + TUND_HDR_SIZE;
    switch (type) {
    case MSG_REGISTER:
        server_handle_register(srv, payload, payload_len, from, sequence);
        break;
    case MSG_DATA:
        server_handle_data(srv, payload, payload_len, from);
        break;
    case MSG_KEEPALIVE:
        server_handle_keepalive(srv, payload, payload_len, from);
        break;
    case MSG_KEEPALIVE_ACK:
        server_handle_keepalive_ack(srv, payload, payload_len, from);
        break;
    case MSG_DISCONNECT:
        server_handle_disconnect(srv, from);
        break;
    default:
        LOG_DEBUG("Unknown message type 0x%02X", type);
        break;
    }
}
