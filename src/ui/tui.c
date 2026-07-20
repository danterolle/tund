#include "tui_internal.h"
#include "tund.h"

#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

void tui_init(void) {
    tui_events_start();
    tui_write(TUI_ALT_BUF);
    tui_write(TUI_CLEAR);
    tui_write(TUI_HIDE_CURSOR);
}

void tui_shutdown(void) {
    tui_events_stop();
    tui_write(TUI_SHOW_CURSOR);
    tui_write(TUI_MAIN_BUF);
}

void tui_render_server(uint16_t port, const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       time_t start_time, int peer_count, const tui_peer_t *peers, int npeers) {
    tui_write(TUI_HOME);
    tui_render_header(TUND_VERSION, "Server");

    char uptime[16], ip_str[16], mask_str[16], value[128];
    tui_uptime(start_time, uptime, sizeof(uptime));
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    tui_section("Connection");
    snprintf(value, sizeof(value), "%u", port);
    tui_kv("UDP", value);
    tui_kv("TUN", tun_name);
    snprintf(value, sizeof(value), "%s/%s", ip_str, mask_str);
    tui_kv("Virtual", value);
    tui_kv("Uptime", uptime);
    tui_rule();

    tui_render_peer_table(peer_count, peers, npeers);
    tui_rule();
    tui_render_events();
    snprintf(value, sizeof(value),
             "Ctrl+C quit · UDP %u · subnet 10.9.0.0/24 · authenticated, not encrypted", port);
    tui_render_footer(value);
    tui_finish_frame();
}

void tui_render_client(const char *server_addr, uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask, bool has_server_rtt,
                       uint64_t server_rtt_ms, time_t start_time, int peer_count,
                       const tui_peer_t *peers, int npeers) {
    tui_write(TUI_HOME);
    tui_render_header(TUND_VERSION, "Client");

    char uptime[16], ip_str[16], mask_str[16], line[192], rtt_str[16], value[160];
    tui_uptime(start_time, uptime, sizeof(uptime));
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));
    tui_format_rtt(has_server_rtt, server_rtt_ms, rtt_str, sizeof(rtt_str));

    tui_section("Connection");
    snprintf(line, sizeof(line), "%s:%u", server_addr, port);
    tui_kv("Server", line);
    tui_kv("Keepalive", rtt_str);
    tui_kv("TUN", tun_name);
    snprintf(line, sizeof(line), "%s/%s", ip_str, mask_str);
    tui_kv("Virtual", line);
    tui_kv("Uptime", uptime);
    tui_rule();

    tui_render_peer_table(peer_count, peers, npeers);
    tui_rule();
    tui_render_events();
    snprintf(value, sizeof(value), "Ctrl+C quit · server %s:%u · authenticated, not encrypted",
             server_addr, port);
    tui_render_footer(value);
    tui_finish_frame();
}
