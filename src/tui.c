#include "tui.h"
#include "tund.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#define TUI_CLEAR       "\033[2J\033[H"
#define TUI_HIDE_CURSOR "\033[?25l"
#define TUI_SHOW_CURSOR "\033[?25h"
#define TUI_ALT_BUF     "\033[?1049h"
#define TUI_MAIN_BUF    "\033[?1049l"
#define TUI_CLR_LINE    "\033[K"

static void tui_write(const char *s) { fputs(s, stderr); }
static void tui_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void tui_uptime(time_t start, char *buf, size_t len)
{
    time_t now = time(NULL);
    int secs = (int)(now - start);
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    snprintf(buf, len, "%02d:%02d:%02d", h, m, s);
}

void tui_init(void)
{
    tui_write(TUI_ALT_BUF);
    tui_write(TUI_HIDE_CURSOR);
}

void tui_shutdown(void)
{
    tui_write(TUI_SHOW_CURSOR);
    tui_write(TUI_MAIN_BUF);
}

void tui_render_server(uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_CLEAR);

    tui_printf("  \033[36mTund v%s \033[90m— Server Mode\033[0m\n", TUND_VERSION);
    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    tui_printf("  Port: \033[33m%u\033[0m   TUN: \033[33m%s\033[0m   IP: \033[33m%s/%s\033[0m\n",
               port, tun_name, ip_str, mask_str);
    tui_printf("  Uptime: \033[33m%s\033[0m\n", uptime);

    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");
    tui_printf("  Peers: \033[33m%d\033[0m\n", peer_count);
    tui_printf("  \033[90m%-15s %-22s %s\033[0m\n", "Virtual IP", "Name", "Last Seen");

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        inet_ntop(AF_INET, &peers[i].virt_ip, ip_str, sizeof(ip_str));
        time_t now = time(NULL);
        int ago = (int)(now - peers[i].last_seen);
        const char *color = (ago < 30) ? "32" : "31";
        tui_printf("  \033[%sm%-15s\033[0m %-22s %s%d s ago\033[0m\n",
                   color, ip_str, peers[i].name,
                   (ago >= 30) ? "\033[31m" : "", ago);
    }
    if (!any)
        tui_printf("  \033[90m(no peers yet)\033[0m\n");

    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");
    tui_printf("  [\033[33mCtrl+C\033[0m to stop]\n");
}

void tui_render_client(const char *server_addr, uint16_t port,
                       const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_CLEAR);

    tui_printf("  \033[36mTund v%s \033[90m— Client Mode\033[0m\n", TUND_VERSION);
    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    tui_printf("  Server: \033[33m%s:%u\033[0m   TUN: \033[33m%s\033[0m\n", server_addr, port, tun_name);
    tui_printf("  Virtual IP: \033[33m%s/%s\033[0m   Uptime: \033[33m%s\033[0m\n",
               ip_str, mask_str, uptime);

    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");
    tui_printf("  Peers: \033[33m%d\033[0m\n", peer_count);
    tui_printf("  \033[90m%-15s %-22s %s\033[0m\n", "Virtual IP", "Name", "Last Seen");

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        inet_ntop(AF_INET, &peers[i].virt_ip, ip_str, sizeof(ip_str));
        time_t now = time(NULL);
        int ago = (int)(now - peers[i].last_seen);
        const char *color = (ago < 30) ? "32" : "31";
        tui_printf("  \033[%sm%-15s\033[0m %-22s %s%d s ago\033[0m\n",
                   color, ip_str, peers[i].name,
                   (ago >= 30) ? "\033[31m" : "", ago);
    }
    if (!any)
        tui_printf("  \033[90m(no peers connected)\033[0m\n");

    tui_printf("  \033[90m────────────────────────────────────────────────\033[0m\n");
    tui_printf("  [\033[33mCtrl+C\033[0m to disconnect]\n");
}
