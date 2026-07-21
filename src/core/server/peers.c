#include "internal.h"
#include "log.h"
#include "network.h"

uint32_t server_alloc_ip(const server_t *srv) {
    for (uint32_t ip_h = TUND_IP_START; ip_h <= TUND_IP_END; ip_h++) {
        uint32_t ip_n = htonl(ip_h);
        bool taken = false;
        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active && srv->peers[i].virt_ip == ip_n) {
                taken = true;
                break;
            }
        }
        if (!taken) return ip_n;
    }
    return 0;
}

int server_find_peer_by_vip(const server_t *srv, uint32_t virt_ip) {
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && srv->peers[i].virt_ip == virt_ip) return i;
    }
    return -1;
}

int server_find_peer_by_addr(const server_t *srv, const struct sockaddr_in *addr) {
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active &&
            srv->peers[i].real_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            srv->peers[i].real_addr.sin_port == addr->sin_port)
            return i;
    }
    return -1;
}

int server_find_free_slot(const server_t *srv) {
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (!srv->peers[i].active) return i;
    }
    return -1;
}

void server_snapshot_peer_locked(const peer_t *peer, int idx, server_peer_snapshot_t *out) {
    out->idx = idx;
    out->virt_ip = peer->virt_ip;
    snprintf(out->name, sizeof(out->name), "%s", peer->name);
    memcpy(&out->real_addr, &peer->real_addr, sizeof(out->real_addr));
}

int server_snapshot_broadcast_locked(const server_t *srv, server_peer_snapshot_t *out,
                                     int max_peers, int exclude_idx) {
    int count = 0;
    for (int i = 0; i < TUND_MAX_PEERS && count < max_peers; i++) {
        if (srv->peers[i].active && i != exclude_idx)
            server_snapshot_peer_locked(&srv->peers[i], i, &out[count++]);
    }
    return count;
}

int server_snapshot_peer_by_vip_locked(const server_t *srv, uint32_t virt_ip,
                                       server_peer_snapshot_t *out) {
    int idx = server_find_peer_by_vip(srv, virt_ip);
    if (idx >= 0) server_snapshot_peer_locked(&srv->peers[idx], idx, out);
    return idx;
}

void server_add_peer_bytes_out(server_t *srv, const server_peer_snapshot_t *peer,
                               uint64_t bytes_out) {
    if (bytes_out == 0) return;

    pthread_mutex_lock(&srv->peers_lock);
    if (peer->idx >= 0 && peer->idx < TUND_MAX_PEERS) {
        peer_t *current = &srv->peers[peer->idx];
        if (current->active && current->virt_ip == peer->virt_ip &&
            current->real_addr.sin_addr.s_addr == peer->real_addr.sin_addr.s_addr &&
            current->real_addr.sin_port == peer->real_addr.sin_port)
            current->bytes_out += bytes_out;
    }
    pthread_mutex_unlock(&srv->peers_lock);
}

void server_send_peer_snapshots(server_t *srv, const server_peer_snapshot_t *peers, int peer_count,
                                uint8_t *buf, int len, uint64_t bytes_out) {
    for (int i = 0; i < peer_count; i++) {
        if (net_send(srv->sockfd, buf, len, &peers[i].real_addr) < 0) {
            char peer_ip[TUND_IP_STR_LEN];
            LOG_WARN("Send to %s (%s) failed", peers[i].name,
                     ip_to_str_buf(peers[i].virt_ip, peer_ip, sizeof(peer_ip)));
        } else {
            server_add_peer_bytes_out(srv, &peers[i], bytes_out);
        }
    }
}

void server_broadcast(server_t *srv, uint8_t *buf, int len, int exclude_idx, uint64_t bytes_out) {
    server_peer_snapshot_t peers[TUND_MAX_PEERS];
    int peer_count;

    pthread_mutex_lock(&srv->peers_lock);
    peer_count = server_snapshot_broadcast_locked(srv, peers, TUND_MAX_PEERS, exclude_idx);
    pthread_mutex_unlock(&srv->peers_lock);

    for (int i = 0; i < peer_count; i++) {
        if (net_send(srv->sockfd, buf, len, &peers[i].real_addr) < 0) {
            char peer_ip[TUND_IP_STR_LEN];
            LOG_WARN("Broadcast to %s (%s) failed", peers[i].name,
                     ip_to_str_buf(peers[i].virt_ip, peer_ip, sizeof(peer_ip)));
        } else {
            server_add_peer_bytes_out(srv, &peers[i], bytes_out);
        }
    }
}

void server_send_peer_list(server_t *srv, int peer_idx) {
    uint8_t buf[TUND_MAX_PKT];
    msg_peer_entry_t entries[TUND_MAX_PEERS];
    server_peer_snapshot_t target;
    uint8_t *payload = buf + TUND_HDR_SIZE;
    int offset = 0, count = 0;
    bool has_target = false;

    memset(entries, 0, sizeof(entries));
    memset(&target, 0, sizeof(target));

    pthread_mutex_lock(&srv->peers_lock);
    if (peer_idx >= 0 && peer_idx < TUND_MAX_PEERS && srv->peers[peer_idx].active) {
        server_snapshot_peer_locked(&srv->peers[peer_idx], peer_idx, &target);
        has_target = true;
    }
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != peer_idx) {
            if (offset + (int)sizeof(msg_peer_entry_t) > TUND_MAX_PLAINTEXT) break;
            msg_peer_entry_t *entry = &entries[count];
            entry->virt_ip = srv->peers[i].virt_ip;
            memset(entry->name, 0, TUND_NAME_LEN);
            strncpy(entry->name, srv->peers[i].name, TUND_NAME_LEN - 1);
            entry->status = 1;
            offset += (int)sizeof(msg_peer_entry_t);
            count++;
        }
    }
    pthread_mutex_unlock(&srv->peers_lock);

    if (!has_target) return;

    for (int i = 0; i < count; i++)
        memcpy(payload + i * (int)sizeof(msg_peer_entry_t), &entries[i], sizeof(entries[i]));
    proto_write_hdr(buf, MSG_PEER_LIST, (uint16_t)offset);
    if (net_send(srv->sockfd, buf, TUND_HDR_SIZE + offset, &target.real_addr) < 0)
        LOG_WARN("Failed to send peer list to %s", target.name);
    else
        LOG_DEBUG("Sent peer list (%d peers) to %s", count, target.name);
}
