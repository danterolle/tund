#include "internal.h"

void client_log_banner(const client_t *cli)
{
    char version[64], ip[64], server[64], tun[64], name[64], virt_ip[TUND_IP_STR_LEN], server_ip[TUND_IP_STR_LEN];
    snprintf(version, sizeof(version), "TunD Client v%s", TUND_VERSION);
    snprintf(ip, sizeof(ip), "Virtual IP: %s", ip_to_str_buf(cli->virt_ip, virt_ip, sizeof(virt_ip)));
    snprintf(server, sizeof(server), "Server: %s:%u",
             ip_to_str_buf(cli->server_addr.sin_addr.s_addr, server_ip, sizeof(server_ip)),
             ntohs(cli->server_addr.sin_port));
    snprintf(tun, sizeof(tun), "TUN: %s", cli->tun.ifname);
    snprintf(name, sizeof(name), "Name: %s", cli->name);

    int width = (int)strlen(version);
    if ((int)strlen(ip) > width) width = (int)strlen(ip);
    if ((int)strlen(server) > width) width = (int)strlen(server);
    if ((int)strlen(tun) > width) width = (int)strlen(tun);
    if ((int)strlen(name) > width) width = (int)strlen(name);

    int inner = width + 4;
    char line[128];
    memset(line, '=', (size_t)inner);
    line[inner] = '\0';

    LOG_INFO("╔%s╗", line);
    LOG_INFO("║  %-*s║", inner - 2, version);
    LOG_INFO("║  %-*s║", inner - 2, ip);
    LOG_INFO("║  %-*s║", inner - 2, server);
    LOG_INFO("║  %-*s║", inner - 2, tun);
    LOG_INFO("║  %-*s║", inner - 2, name);
    LOG_INFO("╚%s╝", line);
    LOG_INFO("Press Ctrl+C to disconnect.");
}

void client_log_startup_checklist(const client_t *cli)
{
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
