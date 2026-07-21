#include "internal.h"
#include "log.h"
#include "version.h"

void client_log_banner(const client_t *cli) {
    char virt_ip[TUND_IP_STR_LEN], server_ip[TUND_IP_STR_LEN];
    LOG_INFO("TunD Client v%s", TUND_VERSION);
    LOG_INFO("Virtual IP: %s", ip_to_str_buf(cli->virt_ip, virt_ip, sizeof(virt_ip)));
    LOG_INFO("Server: %s:%u",
             ip_to_str_buf(cli->server_addr.sin_addr.s_addr, server_ip, sizeof(server_ip)),
             ntohs(cli->server_addr.sin_port));
    LOG_INFO("TUN: %s", cli->tun.ifname);
    LOG_INFO("Name: %s", cli->name);
    LOG_INFO("Press Ctrl+C to disconnect.");
}

void client_log_startup_checklist(const client_t *cli) {
    char virt_ip[TUND_IP_STR_LEN], server_ip[TUND_IP_STR_LEN];
    LOG_INFO("Startup checklist:");
    LOG_INFO("Connected to server %s:%u.",
             ip_to_str_buf(cli->server_addr.sin_addr.s_addr, server_ip, sizeof(server_ip)),
             ntohs(cli->server_addr.sin_port));
    LOG_INFO("Virtual IP assigned: %s on %s.",
             ip_to_str_buf(cli->virt_ip, virt_ip, sizeof(virt_ip)), cli->tun.ifname);
    LOG_INFO("Shared key accepted by server.");
    LOG_INFO("Waiting for peer updates and tunnel traffic.");
}
