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

#define TUI_BOLD    "\033[1m"
#define TUI_CYAN    "\033[36m"
#define TUI_YELLOW  "\033[33m"
#define TUI_GREEN   "\033[32m"
#define TUI_RED     "\033[31m"
#define TUI_GRAY    "\033[90m"
#define TUI_RESET   "\033[0m"

static const char *tui_logo[] = {
    "  o",
    " /|\\",
    "o-o-o",
    " \\|/",
    "  o",
};

static const int tui_logo_n = 5;

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

static void tui_timeago(time_t ts, char *buf, size_t len)
{
    int ago = (int)(time(NULL) - ts);
    if (ago < 2)
        snprintf(buf, len, "just now");
    else if (ago < 60)
        snprintf(buf, len, "%ds ago", ago);
    else if (ago < 3600)
        snprintf(buf, len, "%dm %ds ago", ago / 60, ago % 60);
    else
        snprintf(buf, len, "%dh %dm ago", ago / 3600, (ago % 3600) / 60);
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

static void tui_render_logo(const char *version, const char *mode)
{
    for (int i = 0; i < tui_logo_n; i++) {
        tui_printf("  %s%s%s", TUI_CYAN, tui_logo[i], TUI_RESET);
        if (i == 1)
            tui_printf("  %s%s v%s %s\xe2\x80\x94 %s%s", TUI_BOLD TUI_CYAN, "Tund", version, TUI_GRAY, mode, TUI_RESET);
        tui_printf("\n");
    }
}

static void tui_render_peer_table(int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_printf("  %s────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);
    tui_printf("  %sPeers: %s%d%s\n", TUI_GRAY, TUI_YELLOW, peer_count, TUI_RESET);
    tui_printf("  %s%-5s %-15s %-22s %s%s\n", TUI_GRAY, " ", "Virtual IP", "Name", "Last Seen", TUI_RESET);

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        char ip_str[16], ago_str[32];
        inet_ntop(AF_INET, &peers[i].virt_ip, ip_str, sizeof(ip_str));
        tui_timeago(peers[i].last_seen, ago_str, sizeof(ago_str));

        int ago = (int)(time(NULL) - peers[i].last_seen);
        const char *dot;
        if (ago < 10) {
            dot = TUI_GREEN "\xe2\x97\x8f" TUI_RESET;    /* green circle */
        } else if (ago < 30) {
            dot = TUI_YELLOW "\xe2\x97\x8f" TUI_RESET;   /* yellow circle */
        } else {
            dot = TUI_RED "\xe2\x97\x8f" TUI_RESET;       /* red circle */
        }

        tui_printf("  %s  %-15s %-22s %s\n", dot, ip_str, peers[i].name, ago_str);
    }
    if (!any)
        tui_printf("  %s(no peers yet)%s\n", TUI_GRAY, TUI_RESET);
}

static void tui_render_footer(const char *hint)
{
    tui_printf("  %s────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);
    tui_printf("  [%s%s%s]\n", TUI_YELLOW, hint, TUI_RESET);
}

void tui_render_server(uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_CLEAR);

    tui_render_logo(TUND_VERSION, "Server Mode");
    tui_printf("  %s────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    char line[128];
    snprintf(line, sizeof(line),
             "  Port: %s%u%s   TUN: %s%s%s   IP: %s%s/%s%s",
             TUI_YELLOW, port, TUI_RESET,
             TUI_YELLOW, tun_name, TUI_RESET,
             TUI_YELLOW, ip_str, mask_str, TUI_RESET);
    tui_write(line);
    tui_printf("\n");

    tui_printf("  Uptime: %s%s%s\n", TUI_YELLOW, uptime, TUI_RESET);

    tui_render_peer_table(peer_count, peers, npeers);
    tui_render_footer("Ctrl+C to stop");
}

void tui_render_client(const char *server_addr, uint16_t port,
                       const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_CLEAR);

    tui_render_logo(TUND_VERSION, "Client Mode");
    tui_printf("  %s────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    char line[192];
    snprintf(line, sizeof(line),
             "  Server: %s%s:%u%s   TUN: %s%s%s",
             TUI_YELLOW, server_addr, port, TUI_RESET,
             TUI_YELLOW, tun_name, TUI_RESET);
    tui_write(line);
    tui_printf("\n");

    snprintf(line, sizeof(line),
             "  Virtual IP: %s%s/%s%s   Uptime: %s%s%s",
             TUI_YELLOW, ip_str, mask_str, TUI_RESET,
             TUI_YELLOW, uptime, TUI_RESET);
    tui_write(line);
    tui_printf("\n");

    tui_render_peer_table(peer_count, peers, npeers);
    tui_render_footer("Ctrl+C to disconnect");
}
