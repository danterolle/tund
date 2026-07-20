#include "internal.h"
#include "log.h"
#include "network.h"

void server_handle_keepalive(server_t *srv, const uint8_t *payload, uint16_t plen,
                             const struct sockaddr_in *from) {
    uint64_t sent_at = 0;
    if (!proto_read_keepalive_timestamp(payload, plen, &sent_at)) {
        LOG_DEBUG("Ignoring malformed keepalive");
        return;
    }

    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    char peer_name[TUND_NAME_LEN] = "";
    if (idx >= 0) {
        srv->peers[idx].last_seen = time(NULL);
        snprintf(peer_name, sizeof(peer_name), "%s", srv->peers[idx].name);
    }
    pthread_mutex_unlock(&srv->peers_lock);
    if (idx < 0) return;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_keepalive_ack(buf, sent_at);
    if (net_send(srv->sockfd, buf, len, from) < 0)
        LOG_WARN("Keepalive reply to %s failed", peer_name);
}

void server_handle_keepalive_ack(server_t *srv, const uint8_t *payload, uint16_t plen,
                                 const struct sockaddr_in *from) {
    uint64_t sent_at = 0;
    if (!proto_read_keepalive_timestamp(payload, plen, &sent_at)) return;
    uint64_t now = now_ms();
    if (now < sent_at) return;

    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        uint64_t sample = now - sent_at;
        srv->peers[idx].last_seen = time(NULL);
        srv->peers[idx].rtt_ms =
            smooth_rtt_ms(srv->peers[idx].rtt_ms, sample, srv->peers[idx].has_rtt);
        srv->peers[idx].has_rtt = true;
    }
    pthread_mutex_unlock(&srv->peers_lock);
}
