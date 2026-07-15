#include "internal.h"

static void server_handle_register(server_t *srv, const uint8_t *payload,
                                   uint16_t plen, const struct sockaddr_in *from)
{
    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        srv->peers[idx].last_seen = time(NULL);
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_INFO("Peer %s reconnected (refreshed)", srv->peers[idx].name);

        uint8_t buf[TUND_MAX_PKT];
        int len = proto_build_assign(buf, srv->peers[idx].virt_ip,
                                     htonl(TUND_NETMASK), TUND_TUN_MTU);
        if (net_send(srv->sockfd, buf, len, from) < 0)
            LOG_WARN("Failed to send ASSIGN to reconnecting peer %s", srv->peers[idx].name);
        return;
    }

    idx = server_find_free_slot(srv);
    uint32_t vip = (idx >= 0) ? server_alloc_ip(srv) : 0;
    if (idx < 0 || vip == 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN(idx < 0 ? "Peer table full, rejecting connection" : "IP pool exhausted, rejecting connection");
        return;
    }

    peer_t *p = &srv->peers[idx];
    p->active = true;
    p->virt_ip = vip;
    p->bytes_in = p->bytes_out = p->rtt_ms = 0;
    p->has_rtt = false;
    memset(p->name, 0, TUND_NAME_LEN);
    if (plen > 0)
        memcpy(p->name, payload, (size_t)((plen < TUND_NAME_LEN - 1) ? plen : TUND_NAME_LEN - 1));
    else
        snprintf(p->name, TUND_NAME_LEN, "peer-%d", idx);
    memcpy(&p->real_addr, from, sizeof(*from));
    p->last_seen = time(NULL);
    srv->peer_count++;
    pthread_mutex_unlock(&srv->peers_lock);

    char vip_str[TUND_IP_STR_LEN], from_ip[TUND_IP_STR_LEN];
    LOG_INFO("★ New peer: %s → %s [%s:%u]",
             p->name, ip_to_str_buf(vip, vip_str, sizeof(vip_str)),
             ip_to_str_buf(from->sin_addr.s_addr, from_ip, sizeof(from_ip)),
             ntohs(from->sin_port));

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_assign(buf, vip, htonl(TUND_NETMASK), TUND_TUN_MTU);
    if (net_send(srv->sockfd, buf, len, from) < 0)
        LOG_WARN("Failed to send ASSIGN to new peer %s", p->name);
    server_send_peer_list(srv, idx);
    len = proto_build_peer_join(buf, vip, p->name);
    server_broadcast(srv, buf, len, idx, 0);
}

static void server_handle_disconnect(server_t *srv, const struct sockaddr_in *from)
{
    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        return;
    }

    peer_t *p = &srv->peers[idx];
    char peer_ip[TUND_IP_STR_LEN];
    LOG_INFO("✦ Peer disconnected: %s (%s)",
             p->name, ip_to_str_buf(p->virt_ip, peer_ip, sizeof(peer_ip)));

    uint32_t vip = p->virt_ip;
    p->active = false;
    srv->peer_count--;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_peer_leave(buf, vip);
    server_broadcast(srv, buf, len, -1, 0);
    pthread_mutex_unlock(&srv->peers_lock);
}

void server_handle_packet(server_t *srv, uint8_t *buf, int len,
                          const struct sockaddr_in *from)
{
    uint8_t type;
    uint16_t payload_len;
    int hdr = proto_read_hdr(buf, &type, &payload_len);
    if (hdr < 0 || TUND_HDR_SIZE + payload_len > len) {
        char from_ip[TUND_IP_STR_LEN];
        LOG_DEBUG("Invalid packet from %s:%u",
                  ip_to_str_buf(from->sin_addr.s_addr, from_ip, sizeof(from_ip)),
                  ntohs(from->sin_port));
        return;
    }

    uint8_t *payload = buf + TUND_HDR_SIZE;
    switch (type) {
    case MSG_REGISTER:      server_handle_register(srv, payload, payload_len, from); break;
    case MSG_DATA:          server_handle_data(srv, payload, payload_len, from); break;
    case MSG_KEEPALIVE:     server_handle_keepalive(srv, payload, payload_len, from); break;
    case MSG_KEEPALIVE_ACK: server_handle_keepalive_ack(srv, payload, payload_len, from); break;
    case MSG_DISCONNECT:    server_handle_disconnect(srv, from); break;
    default:                LOG_DEBUG("Unknown message type 0x%02X", type); break;
    }
}
