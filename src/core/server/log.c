#include "internal.h"
#include "log.h"
#include "version.h"

void server_log_banner(const server_t *srv) {
    char server_ip[TUND_IP_STR_LEN];
    LOG_INFO("TunD Server v%s", TUND_VERSION);
    LOG_INFO("Listening on UDP port %u", srv->port);
    LOG_INFO("Virtual IP: %s", ip_to_str_buf(htonl(TUND_SERVER_IP), server_ip, sizeof(server_ip)));
    LOG_INFO("Subnet: 10.9.0.0/24");
    LOG_INFO("TUN: %s", srv->tun.ifname);
}

void server_log_startup_checklist(const server_t *srv) {
    char server_ip[TUND_IP_STR_LEN];
    LOG_INFO("Startup checklist:");
    LOG_INFO("Virtual LAN ready: %s/24 on %s.",
             ip_to_str_buf(htonl(TUND_SERVER_IP), server_ip, sizeof(server_ip)), srv->tun.ifname);
    LOG_INFO("Listening for clients on UDP %u.", srv->port);
    LOG_INFO("Use the same shared key on every client.");
    LOG_INFO("Allow inbound UDP %u on this server if clients cannot connect.", srv->port);
    LOG_INFO("Waiting for peers...");
}
