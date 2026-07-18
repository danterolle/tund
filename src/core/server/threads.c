#include "internal.h"

void *server_timeout_thread(void *arg)
{
    server_t *srv = (server_t *)arg;
    unsigned int slept = 0;

    while (tund_is_running() && !tund_stop_flag_load(&srv->timeout_quit)) {
        platform_sleep(1);
        slept++;
        if (slept < TUND_KEEPALIVE_INTERVAL) continue;
        slept = 0;
        if (!tund_is_running() || tund_stop_flag_load(&srv->timeout_quit)) break;

        time_t now = time(NULL);
        uint8_t keepalive_buf[TUND_MAX_PKT];
        int keepalive_len = proto_build_keepalive(keepalive_buf, now_ms());
        pthread_mutex_lock(&srv->peers_lock);

        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active &&
                (now - srv->peers[i].last_seen) > TUND_PEER_TIMEOUT) {
                char peer_ip[TUND_IP_STR_LEN];
                LOG_WARN("✦ Peer timed out: %s (%s)",
                         srv->peers[i].name,
                         ip_to_str_buf(srv->peers[i].virt_ip, peer_ip, sizeof(peer_ip)));
                uint32_t vip = srv->peers[i].virt_ip;
                srv->peers[i].active = false;
                srv->peer_count--;
                uint8_t buf[TUND_MAX_PKT];
                int len = proto_build_peer_leave(buf, vip);
                server_broadcast(srv, buf, len, -1, 0);
            } else if (srv->peers[i].active) {
                if (net_send(srv->sockfd, keepalive_buf, keepalive_len,
                             &srv->peers[i].real_addr) < 0)
                    LOG_WARN("Keepalive probe to %s failed", srv->peers[i].name);
            }
        }

        pthread_mutex_unlock(&srv->peers_lock);
    }
    return NULL;
}

void *server_tun_thread(void *arg)
{
    server_t *srv = (server_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PAYLOAD];
    uint8_t msg_buf[TUND_MAX_PKT];

    while (tund_is_running() && !tund_stop_flag_load(&srv->tun_quit)) {
        int n = tun_read(&srv->tun, pkt_buf, sizeof(pkt_buf));
        if (n <= 0) {
            if (!tund_is_running() || tund_stop_flag_load(&srv->tun_quit)) break;
            continue;
        }

        uint32_t dst_ip = proto_get_dst_ip(pkt_buf, (uint16_t)n);
        if (dst_ip == 0) continue;

        uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);
        int msg_len = proto_build_data(msg_buf, pkt_buf, (uint16_t)n);
        pthread_mutex_lock(&srv->peers_lock);
        if (dst_ip == broadcast_ip) {
            server_broadcast(srv, msg_buf, msg_len, -1, (uint64_t)n);
        } else {
            int idx = server_find_peer_by_vip(srv, dst_ip);
            if (idx >= 0) {
                if (net_send(srv->sockfd, msg_buf, msg_len, &srv->peers[idx].real_addr) < 0)
                    LOG_WARN("Failed to forward TUN packet to %s", srv->peers[idx].name);
                else
                    srv->peers[idx].bytes_out += (uint64_t)n;
            }
        }
        pthread_mutex_unlock(&srv->peers_lock);
    }
    return NULL;
}
