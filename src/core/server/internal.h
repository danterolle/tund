#ifndef TUND_SERVER_INTERNAL_H
#define TUND_SERVER_INTERNAL_H

#include "server.h"

typedef struct {
    int idx;
    uint32_t virt_ip;
    char name[TUND_NAME_LEN];
    struct sockaddr_in real_addr;
} server_peer_snapshot_t;

uint32_t server_alloc_ip(const server_t *srv);
int server_find_peer_by_vip(const server_t *srv, uint32_t virt_ip);
int server_find_peer_by_addr(const server_t *srv, const struct sockaddr_in *addr);
int server_find_free_slot(const server_t *srv);
void server_snapshot_peer_locked(const peer_t *peer, int idx, server_peer_snapshot_t *out);
int server_snapshot_broadcast_locked(const server_t *srv, server_peer_snapshot_t *out,
                                     int max_peers, int exclude_idx);
int server_snapshot_peer_by_vip_locked(const server_t *srv, uint32_t virt_ip,
                                       server_peer_snapshot_t *out);
void server_add_peer_bytes_out(server_t *srv, const server_peer_snapshot_t *peer,
                               uint64_t bytes_out);
void server_send_peer_snapshots(server_t *srv, const server_peer_snapshot_t *peers, int peer_count,
                                uint8_t *buf, int len, uint64_t bytes_out);
void server_broadcast(server_t *srv, uint8_t *buf, int len, int exclude_idx, uint64_t bytes_out);
void server_send_peer_list(server_t *srv, int peer_idx);
void server_handle_data(server_t *srv, const uint8_t *payload, uint16_t plen,
                        const struct sockaddr_in *from);
void server_handle_keepalive(server_t *srv, const uint8_t *payload, uint16_t plen,
                             const struct sockaddr_in *from);
void server_handle_keepalive_ack(server_t *srv, const uint8_t *payload, uint16_t plen,
                                 const struct sockaddr_in *from);
void server_handle_packet(server_t *srv, uint8_t *buf, int len, const struct sockaddr_in *from);
void *server_timeout_thread(void *arg);
void *server_tun_thread(void *arg);
void server_log_banner(const server_t *srv);
void server_log_startup_checklist(const server_t *srv);

#endif /* TUND_SERVER_INTERNAL_H */
