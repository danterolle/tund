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

#define TUI_MAX_EVENTS 5
#define TUI_EVENT_MESSAGE_LEN 128

typedef struct {
    time_t ts;
    int level;
    char message[TUI_EVENT_MESSAGE_LEN];
} tui_event_t;

static const char *tui_logo[] = {
    "  o",
    " /|\\",
    "o-o-o",
    " \\|/",
    "  o",
};

static const int tui_logo_n = 5;
static pthread_mutex_t tui_events_lock = PTHREAD_MUTEX_INITIALIZER;
static tui_event_t tui_events[TUI_MAX_EVENTS];
static int tui_event_start = 0;
static int tui_event_count = 0;
static volatile bool tui_active = false;

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

static void tui_format_bytes(uint64_t bytes, char *buf, size_t len)
{
    const char *units[] = {"B", "KB", "MB", "GB"};
    double value = (double)bytes;
    int unit = 0;

    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        unit++;
    }

    if (unit == 0)
        snprintf(buf, len, "%lluB", (unsigned long long)bytes);
    else
        snprintf(buf, len, "%.1f%s", value, units[unit]);
}

void tui_init(void)
{
    pthread_mutex_lock(&tui_events_lock);
    tui_event_start = 0;
    tui_event_count = 0;
    tui_active = true;
    pthread_mutex_unlock(&tui_events_lock);

    tui_write(TUI_ALT_BUF);
    tui_write(TUI_HIDE_CURSOR);
}

void tui_shutdown(void)
{
    pthread_mutex_lock(&tui_events_lock);
    tui_active = false;
    pthread_mutex_unlock(&tui_events_lock);

    tui_write(TUI_SHOW_CURSOR);
    tui_write(TUI_MAIN_BUF);
}

bool tui_events_active(void)
{
    return tui_active;
}

void tui_add_event(int level, const char *message)
{
    pthread_mutex_lock(&tui_events_lock);

    int idx = (tui_event_start + tui_event_count) % TUI_MAX_EVENTS;
    if (tui_event_count == TUI_MAX_EVENTS) {
        idx = tui_event_start;
        tui_event_start = (tui_event_start + 1) % TUI_MAX_EVENTS;
    } else {
        tui_event_count++;
    }

    tui_events[idx].ts = time(NULL);
    tui_events[idx].level = level;
    snprintf(tui_events[idx].message, sizeof(tui_events[idx].message), "%s", message);

    pthread_mutex_unlock(&tui_events_lock);
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
    tui_printf(" %s────────────────────────────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);
    tui_printf(" %sPeers: %s%d%s\n", TUI_GRAY, TUI_YELLOW, peer_count, TUI_RESET);
    tui_printf(" %s%-8s %-15s %-18s %-8s %-8s %s%s\n",
               TUI_GRAY, "Status", "Virtual IP", "Name", "In", "Out", "Last Seen", TUI_RESET);

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        char ip_str[16], ago_str[32], in_str[16], out_str[16];
        inet_ntop(AF_INET, &peers[i].virt_ip, ip_str, sizeof(ip_str));
        tui_timeago(peers[i].last_seen, ago_str, sizeof(ago_str));
        tui_format_bytes(peers[i].bytes_in, in_str, sizeof(in_str));
        tui_format_bytes(peers[i].bytes_out, out_str, sizeof(out_str));

        int ago = (int)(time(NULL) - peers[i].last_seen);
        const char *dot;
        const char *status;
        if (ago < 10) {
            dot = TUI_GREEN "\xe2\x97\x8f" TUI_RESET;
            status = "online";
        } else if (ago < 30) {
            dot = TUI_YELLOW "\xe2\x97\x8f" TUI_RESET;
            status = "stale";
        } else {
            dot = TUI_RED "\xe2\x97\x8f" TUI_RESET;
            status = "timeout";
        }

        tui_printf(" %s %-7s %-15s %-18s %-8s %-8s %s\n",
                   dot, status, ip_str, peers[i].name, in_str, out_str, ago_str);
    }
    if (!any)
        tui_printf(" %s(no peers yet)%s\n", TUI_GRAY, TUI_RESET);
}

static const char *tui_event_level_color(int level)
{
    switch (level) {
    case LOG_DEBUG: return TUI_CYAN;
    case LOG_INFO:  return TUI_GREEN;
    case LOG_WARN:  return TUI_YELLOW;
    case LOG_ERROR: return TUI_RED;
    default:        return TUI_GRAY;
    }
}

static const char *tui_event_level_label(int level)
{
    switch (level) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO ";
    case LOG_WARN:  return "WARN ";
    case LOG_ERROR: return "ERROR";
    default:        return "LOG  ";
    }
}

static void tui_render_events(void)
{
    tui_event_t events[TUI_MAX_EVENTS];
    int count;

    pthread_mutex_lock(&tui_events_lock);
    count = tui_event_count;
    for (int i = 0; i < count; i++) {
        int idx = (tui_event_start + i) % TUI_MAX_EVENTS;
        events[i] = tui_events[idx];
    }
    pthread_mutex_unlock(&tui_events_lock);

    tui_printf(" %s─────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);
    tui_printf(" %sEvents%s\n", TUI_GRAY, TUI_RESET);

    if (count == 0) {
        tui_printf(" %s(no recent events)%s\n", TUI_GRAY, TUI_RESET);
        return;
    }

    for (int i = 0; i < count; i++) {
        struct tm tm;
        char ts[9];
        localtime_r(&events[i].ts, &tm);
        snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

        const char *color = tui_event_level_color(events[i].level);
        tui_printf(" %s %s%s%s %s\n",
                   ts, color, tui_event_level_label(events[i].level), TUI_RESET,
                   events[i].message);
    }
}

static void tui_render_footer(const char *hint)
{
    tui_printf(" %s─────────────────────────────────────────────────%s\n", TUI_GRAY, TUI_RESET);
    tui_printf(" [%s%s%s]\n", TUI_YELLOW, hint, TUI_RESET);
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
    tui_render_events();
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
    tui_render_events();
    tui_render_footer("Ctrl+C to disconnect");
}
