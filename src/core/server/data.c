#include "internal.h"

void server_handle_data(server_t *srv, const uint8_t *payload,
                        uint16_t plen, const struct sockaddr_in *from)
{
    if (plen < 20) {
        LOG_DEBUG("DATA packet too short (%u bytes)", plen);
        return;
    }

    uint32_t dst_ip = proto_get_dst_ip(payload, plen);
    if (dst_ip == 0)
        return;

    pthread_mutex_lock(&srv->peers_lock);
    int sender_idx = server_find_peer_by_addr(srv, from);
    if (sender_idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN("Dropped DATA from an unregistered endpoint");
        return;
    }
    srv->peers[sender_idx].bytes_in += plen;

    uint32_t server_vip = htonl(TUND_SERVER_IP);
    if (dst_ip == server_vip) {
        pthread_mutex_unlock(&srv->peers_lock);
        tun_write(&srv->tun, payload, plen);
        return;
    }

    uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);
    if (dst_ip == broadcast_ip) {
        uint8_t buf[TUND_MAX_PKT];
        int len = proto_build_data(buf, payload, plen);
        server_broadcast(srv, buf, len, sender_idx, plen);
        pthread_mutex_unlock(&srv->peers_lock);
        tun_write(&srv->tun, payload, plen);
        return;
    }

    int dst_idx = server_find_peer_by_vip(srv, dst_ip);
    if (dst_idx < 0) {
        char dst_ip_str[TUND_IP_STR_LEN];
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_DEBUG("No peer for %s, dropping packet",
                  ip_to_str_buf(dst_ip, dst_ip_str, sizeof(dst_ip_str)));
        return;
    }

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_data(buf, payload, plen);
    if (net_send(srv->sockfd, buf, len, &srv->peers[dst_idx].real_addr) < 0)
        LOG_WARN("Failed to forward data to %s", srv->peers[dst_idx].name);
    else
        srv->peers[dst_idx].bytes_out += plen;
    pthread_mutex_unlock(&srv->peers_lock);
}
