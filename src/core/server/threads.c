#include "internal.h"
#include "log.h"
#include "network.h"
#include "tun.h"

void *server_timeout_thread(void *arg) {
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
        server_peer_snapshot_t timed_out_peers[TUND_MAX_PEERS];
        server_peer_snapshot_t leave_peers[TUND_MAX_PEERS];
        server_peer_snapshot_t keepalive_peers[TUND_MAX_PEERS];
        int timed_out_count = 0;
        int leave_count = 0;
        int keepalive_count = 0;

        pthread_mutex_lock(&srv->peers_lock);

        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active && (now - srv->peers[i].last_seen) > TUND_PEER_TIMEOUT) {
                server_snapshot_peer_locked(&srv->peers[i], i, &timed_out_peers[timed_out_count++]);
                srv->peers[i].active = false;
                srv->peer_count--;
            } else if (srv->peers[i].active) {
                server_snapshot_peer_locked(&srv->peers[i], i, &keepalive_peers[keepalive_count++]);
            }
        }
        if (timed_out_count > 0)
            leave_count = server_snapshot_broadcast_locked(srv, leave_peers, TUND_MAX_PEERS, -1);

        pthread_mutex_unlock(&srv->peers_lock);

        for (int i = 0; i < timed_out_count; i++) {
            char peer_ip[TUND_IP_STR_LEN];
            LOG_WARN("✦ Peer timed out: %s (%s)", timed_out_peers[i].name,
                     ip_to_str_buf(timed_out_peers[i].virt_ip, peer_ip, sizeof(peer_ip)));
            uint8_t buf[TUND_MAX_PKT];
            int len = proto_build_peer_leave(buf, timed_out_peers[i].virt_ip);
            server_send_peer_snapshots(srv, leave_peers, leave_count, buf, len, 0);
        }

        for (int i = 0; i < keepalive_count; i++) {
            if (net_send(srv->sockfd, keepalive_buf, keepalive_len, &keepalive_peers[i].real_addr) <
                0)
                LOG_WARN("Keepalive probe to %s failed", keepalive_peers[i].name);
        }
    }
    return NULL;
}

void *server_tun_thread(void *arg) {
    server_t *srv = (server_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PLAINTEXT];
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
        server_peer_snapshot_t peers[TUND_MAX_PEERS];
        server_peer_snapshot_t dst_peer;
        int peer_count = 0;
        bool send_broadcast = false;
        bool send_unicast = false;

        pthread_mutex_lock(&srv->peers_lock);
        if (dst_ip == broadcast_ip) {
            peer_count = server_snapshot_broadcast_locked(srv, peers, TUND_MAX_PEERS, -1);
            send_broadcast = true;
        } else {
            send_unicast = server_snapshot_peer_by_vip_locked(srv, dst_ip, &dst_peer) >= 0;
        }
        pthread_mutex_unlock(&srv->peers_lock);

        if (send_broadcast) {
            server_send_peer_snapshots(srv, peers, peer_count, msg_buf, msg_len, (uint64_t)n);
        } else if (send_unicast) {
            if (net_send(srv->sockfd, msg_buf, msg_len, &dst_peer.real_addr) < 0)
                LOG_WARN("Failed to forward TUN packet to %s", dst_peer.name);
            else
                server_add_peer_bytes_out(srv, &dst_peer, (uint64_t)n);
        }
    }
    return NULL;
}
