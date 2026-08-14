#include "internal.h"
#include "log.h"
#include "network.h"

void server_send_peer_list(server_t *srv, int peer_idx) {
    uint8_t buf[TUND_MAX_PKT];
    server_peer_snapshot_t target;
    uint8_t *payload = buf + TUND_HDR_SIZE;
    int offset = 0, count = 0;
    bool has_target = false;

    memset(&target, 0, sizeof(target));

    pthread_mutex_lock(&srv->peers_lock);
    if (peer_idx >= 0 && peer_idx < TUND_MAX_PEERS && srv->peers[peer_idx].active) {
        server_snapshot_peer_locked(&srv->peers[peer_idx], peer_idx, &target);
        has_target = true;
    }
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != peer_idx) {
            if (!proto_write_peer_entry(payload + offset, TUND_MAX_PLAINTEXT - (size_t)offset,
                                        srv->peers[i].virt_ip, srv->peers[i].name, true))
                break;
            offset += TUND_PEER_ENTRY_SIZE;
            count++;
        }
    }
    pthread_mutex_unlock(&srv->peers_lock);

    if (!has_target) return;

    proto_write_hdr(buf, MSG_PEER_LIST, (uint16_t)offset);
    if (net_send(srv->sockfd, buf, TUND_HDR_SIZE + offset, &target.real_addr) < 0)
        LOG_WARN("Failed to send peer list to %s", target.name);
    else
        LOG_DEBUG("Sent peer list (%d peers) to %s", count, target.name);
}
