#include "internal.h"
#include "log.h"

void server_handle_data(server_t *srv, const uint8_t *payload,
                        uint16_t plen, const struct sockaddr_in *from)
{
    server_peer_snapshot_t broadcast_peers[TUND_MAX_PEERS];
    server_peer_snapshot_t dst_peer;
    int broadcast_count = 0;
    bool send_broadcast = false;
    bool send_unicast = false;
    bool write_tun = false;

    if (plen < 20) {
        LOG_DEBUG("DATA packet too short (%u bytes)", plen);
        return;
    }

    uint32_t src_ip = proto_get_src_ip(payload, plen);
    uint32_t dst_ip = proto_get_dst_ip(payload, plen);
    if (src_ip == 0 || dst_ip == 0)
        return;

    pthread_mutex_lock(&srv->peers_lock);
    int sender_idx = server_find_peer_by_addr(srv, from);
    if (sender_idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN("Dropped DATA from an unregistered endpoint");
        return;
    }
    if (src_ip != srv->peers[sender_idx].virt_ip) {
        char src_ip_str[TUND_IP_STR_LEN];
        char assigned_ip_str[TUND_IP_STR_LEN];
        LOG_WARN("Dropped DATA with spoofed source %s from peer %s (%s)",
                 ip_to_str_buf(src_ip, src_ip_str, sizeof(src_ip_str)),
                 srv->peers[sender_idx].name,
                 ip_to_str_buf(srv->peers[sender_idx].virt_ip,
                               assigned_ip_str, sizeof(assigned_ip_str)));
        pthread_mutex_unlock(&srv->peers_lock);
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
        broadcast_count = server_snapshot_broadcast_locked(srv,
                                                           broadcast_peers,
                                                           TUND_MAX_PEERS,
                                                           sender_idx);
        send_broadcast = true;
        write_tun = true;
        pthread_mutex_unlock(&srv->peers_lock);
    } else if (server_snapshot_peer_by_vip_locked(srv, dst_ip, &dst_peer) < 0) {
        char dst_ip_str[TUND_IP_STR_LEN];
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_DEBUG("No peer for %s, dropping packet",
                  ip_to_str_buf(dst_ip, dst_ip_str, sizeof(dst_ip_str)));
        return;
    } else {
        send_unicast = true;
        pthread_mutex_unlock(&srv->peers_lock);
    }

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_data(buf, payload, plen);
    if (send_broadcast) {
        for (int i = 0; i < broadcast_count; i++) {
            if (net_send(srv->sockfd, buf, len,
                         &broadcast_peers[i].real_addr) < 0) {
                char peer_ip[TUND_IP_STR_LEN];
                LOG_WARN("Broadcast to %s (%s) failed",
                         broadcast_peers[i].name,
                         ip_to_str_buf(broadcast_peers[i].virt_ip,
                                       peer_ip, sizeof(peer_ip)));
            } else {
                server_add_peer_bytes_out(srv, &broadcast_peers[i], plen);
            }
        }
    } else if (send_unicast) {
        if (net_send(srv->sockfd, buf, len, &dst_peer.real_addr) < 0)
            LOG_WARN("Failed to forward data to %s", dst_peer.name);
        else
            server_add_peer_bytes_out(srv, &dst_peer, plen);
    }
    if (write_tun)
        tun_write(&srv->tun, payload, plen);
}
