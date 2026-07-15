#ifndef TUND_CLIENT_INTERNAL_H
#define TUND_CLIENT_INTERNAL_H

#include "client.h"
#include "network.h"
#include "tun.h"
#include "tui.h"

void client_update_peer(client_t *cli, uint32_t vip, const char *name, bool online);
void client_add_peer_traffic(client_t *cli, uint32_t vip,
                             uint64_t bytes_in, uint64_t bytes_out);
void client_add_broadcast_traffic(client_t *cli, uint64_t bytes_out);
void client_log_peers(client_t *cli);
void client_log_banner(const client_t *cli);
void client_log_startup_checklist(const client_t *cli);
int client_register(client_t *cli);
void client_handle_server_packet(client_t *cli, uint8_t *buf, int len);

#endif /* TUND_CLIENT_INTERNAL_H */
