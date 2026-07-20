#include "internal.h"
#include "log.h"

void server_log_banner(const server_t *srv)
{
    char version[64], port[64], ip[64], tun[64], server_ip[TUND_IP_STR_LEN];
    snprintf(version, sizeof(version), "TunD Server v%s", TUND_VERSION);
    snprintf(port, sizeof(port), "Listening on UDP port %u", srv->port);
    snprintf(ip, sizeof(ip), "Virtual IP: %s",
             ip_to_str_buf(htonl(TUND_SERVER_IP), server_ip, sizeof(server_ip)));
    snprintf(tun, sizeof(tun), "TUN: %s", srv->tun.ifname);
    char subnet[] = "Subnet: 10.9.0.0/24";

    int width = (int)strlen(version);
    if ((int)strlen(port) > width) width = (int)strlen(port);
    if ((int)strlen(ip) > width) width = (int)strlen(ip);
    if ((int)strlen(subnet) > width) width = (int)strlen(subnet);
    if ((int)strlen(tun) > width) width = (int)strlen(tun);

    int inner = width + 4;
    char line[128];
    memset(line, '=', (size_t)inner);
    line[inner] = '\0';

    LOG_INFO("╔%s╗", line);
    LOG_INFO("║  %-*s║", inner - 2, version);
    LOG_INFO("║  %-*s║", inner - 2, port);
    LOG_INFO("║  %-*s║", inner - 2, ip);
    LOG_INFO("║  %-*s║", inner - 2, subnet);
    LOG_INFO("║  %-*s║", inner - 2, tun);
    LOG_INFO("╚%s╝", line);
}

void server_log_startup_checklist(const server_t *srv)
{
    char server_ip[TUND_IP_STR_LEN];
    LOG_INFO("Startup checklist:");
    LOG_INFO("Virtual LAN ready: %s/24 on %s.",
             ip_to_str_buf(htonl(TUND_SERVER_IP), server_ip, sizeof(server_ip)),
             srv->tun.ifname);
    LOG_INFO("Listening for clients on UDP %u.", srv->port);
    LOG_INFO("Use the same shared key on every client.");
    LOG_INFO("Allow inbound UDP %u on this server if clients cannot connect.", srv->port);
    LOG_INFO("Waiting for peers...");
}
