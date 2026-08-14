#ifndef TUND_CLIENT_INTERNAL_H
#define TUND_CLIENT_INTERNAL_H

#include "client.h"

typedef enum {
    CLIENT_REGISTER_OK = 0,
    CLIENT_REGISTER_RETRY = -1,
} client_registration_result_t;

void client_update_peer(client_t *cli, uint32_t vip, const char *name, bool online);
void client_handle_peer_list_page(client_t *cli, const uint8_t *payload, uint16_t payload_len);
void client_request_peer_list_if_due(client_t *cli, uint64_t now);
void client_add_peer_traffic(client_t *cli, uint32_t vip, uint64_t bytes_in, uint64_t bytes_out);
void client_add_broadcast_traffic(client_t *cli, uint64_t bytes_out);
void client_log_peers(client_t *cli);
void client_log_banner(const client_t *cli);
void client_log_startup_checklist(const client_t *cli);
void client_mark_server_seen(client_t *cli);
bool client_server_timed_out(const client_t *cli, time_t now);
bool client_is_connected(client_t *cli);
void client_set_connected(client_t *cli, bool connected);
client_registration_result_t client_register(client_t *cli, int attempts);
int client_reconnect(client_t *cli);
int client_tunnel_open(client_t *cli);
int client_tunnel_start(client_t *cli);
void client_tunnel_stop(client_t *cli);
void client_handle_server_packet(client_t *cli, uint8_t *buf, int len);

#endif /* TUND_CLIENT_INTERNAL_H */
