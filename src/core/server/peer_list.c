#include "internal.h"
#include "log.h"
#include "network.h"

static int snapshot_peer_list(server_t *srv, int peer_idx, server_peer_snapshot_t *target,
                              server_peer_snapshot_t *entries, uint32_t *snapshot_id) {
    int entry_count = 0;

    pthread_mutex_lock(&srv->peers_lock);
    if (peer_idx < 0 || peer_idx >= TUND_MAX_PEERS || !srv->peers[peer_idx].active) {
        pthread_mutex_unlock(&srv->peers_lock);
        return -1;
    }

    srv->peer_list_generation++;
    if (srv->peer_list_generation == 0) srv->peer_list_generation++;
    *snapshot_id = srv->peer_list_generation;
    server_snapshot_peer_locked(&srv->peers[peer_idx], peer_idx, target);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != peer_idx)
            server_snapshot_peer_locked(&srv->peers[i], i, &entries[entry_count++]);
    }
    pthread_mutex_unlock(&srv->peers_lock);
    return entry_count;
}

void server_send_peer_list(server_t *srv, int peer_idx) {
    server_peer_snapshot_t entries[TUND_MAX_PEERS];
    server_peer_snapshot_t target;
    uint32_t snapshot_id = 0;
    int entry_count = snapshot_peer_list(srv, peer_idx, &target, entries, &snapshot_id);
    if (entry_count < 0) return;

    uint16_t total_entries = (uint16_t)entry_count;
    uint16_t page_count = proto_peer_list_page_count(total_entries);
    int sent_pages = 0;

    for (uint16_t page_index = 0; page_index < page_count; page_index++) {
        uint8_t buf[TUND_MAX_PKT];
        int len = proto_build_peer_list_page(buf, snapshot_id, page_index, total_entries);
        if (len < 0) {
            LOG_ERROR("Failed to build peer list page %u/%u", (unsigned int)(page_index + 1),
                      (unsigned int)page_count);
            return;
        }

        uint16_t first_entry = (uint16_t)(page_index * TUND_PEER_LIST_PAGE_ENTRIES);
        uint16_t page_entries = proto_peer_list_page_entry_count(total_entries, page_index);
        uint8_t *payload = buf + TUND_HDR_SIZE + TUND_PEER_LIST_PAGE_HEADER_SIZE;
        for (uint16_t i = 0; i < page_entries; i++) {
            const server_peer_snapshot_t *entry = &entries[first_entry + i];
            if (!proto_write_peer_entry(payload + i * TUND_PEER_ENTRY_SIZE, TUND_PEER_ENTRY_SIZE,
                                        entry->virt_ip, entry->name, true)) {
                LOG_ERROR("Failed to encode peer list page %u/%u", (unsigned int)(page_index + 1),
                          (unsigned int)page_count);
                return;
            }
        }

        if (net_send(srv->sockfd, buf, len, &target.real_addr) < 0)
            LOG_WARN("Failed to send peer list page %u/%u to %s", (unsigned int)(page_index + 1),
                     (unsigned int)page_count, target.name);
        else
            sent_pages++;
    }

    LOG_DEBUG("Sent peer list (%d peers, %d/%u pages) to %s", entry_count, sent_pages,
              (unsigned int)page_count, target.name);
}
