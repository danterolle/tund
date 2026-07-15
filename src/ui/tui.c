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
#define TUI_HOME        "\033[H"
#define TUI_CLEAR_REST  "\033[J"
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

#define TUI_WIDTH 76
#define TUI_MAX_EVENTS 7
#define TUI_EVENT_MESSAGE_LEN 128
#define TUI_EVENT_DISPLAY_LEN 57

typedef struct {
    time_t ts;
    int level;
    char message[TUI_EVENT_MESSAGE_LEN];
} tui_event_t;

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

static void tui_rule(void)
{
    tui_printf(" %s", TUI_GRAY);
    for (int i = 0; i < TUI_WIDTH; i++)
        tui_write("─");
    tui_printf("%s\n", TUI_RESET);
}

static void tui_render_header(const char *version, const char *mode)
{
    tui_printf(" %s◆ Tund%s %sv%s%s %s— %s%s\n",
               TUI_BOLD TUI_CYAN, TUI_RESET,
               TUI_YELLOW, version, TUI_RESET,
               TUI_GRAY, mode, TUI_RESET);
    tui_rule();
}

static void tui_section(const char *title)
{
    tui_printf(" %s%s%s\n", TUI_BOLD TUI_CYAN, title, TUI_RESET);
}

static void tui_kv(const char *label, const char *value)
{
    tui_printf(" %s%-10s%s %s\n", TUI_GRAY, label, TUI_RESET, value);
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

static void tui_format_rtt(bool has_rtt, uint64_t rtt_ms, char *buf, size_t len)
{
    if (!has_rtt) {
        snprintf(buf, len, "-");
    } else if (rtt_ms > 9999) {
        snprintf(buf, len, ">9999ms");
    } else {
        snprintf(buf, len, "%llums", (unsigned long long)rtt_ms);
    }
}

static void tui_truncate_event(const char *message, char *buf, size_t len)
{
    if (strlen(message) < len) {
        snprintf(buf, len, "%s", message);
        return;
    }

    if (len < 4) {
        if (len > 0)
            buf[0] = '\0';
        return;
    }

    memcpy(buf, message, len - 4);
    memcpy(buf + len - 4, "...", 4);
}

void tui_init(void)
{
    pthread_mutex_lock(&tui_events_lock);
    tui_event_start = 0;
    tui_event_count = 0;
    tui_active = true;
    pthread_mutex_unlock(&tui_events_lock);

    tui_write(TUI_ALT_BUF);
    tui_write(TUI_CLEAR);
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

static void tui_render_peer_table(int peer_count, const tui_peer_t *peers, int npeers)
{
    char title[32];
    snprintf(title, sizeof(title), "Peers (%d)", peer_count);
    tui_section(title);
    tui_printf(" %s%-8s %-15s %-18s %7s %7s %7s %s%s\n",
               TUI_GRAY, "Status", "Virtual IP", "Name", "RTT", "In", "Out", "Last", TUI_RESET);

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        char ip_str[16], ago_str[32], rtt_str[16], in_str[16], out_str[16];
        inet_ntop(AF_INET, &peers[i].virt_ip, ip_str, sizeof(ip_str));
        tui_timeago(peers[i].last_seen, ago_str, sizeof(ago_str));
        tui_format_rtt(peers[i].has_rtt, peers[i].rtt_ms, rtt_str, sizeof(rtt_str));
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

        tui_printf(" %s %-7s %-15s %-18.18s %7s %7s %7s %s\n",
                   dot, status, ip_str, peers[i].name, rtt_str, in_str, out_str, ago_str);
    }
    if (!any)
        tui_printf(" %s(no peers yet)%s\n", TUI_GRAY, TUI_RESET);
}

static const char *tui_event_level_color(int level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG: return TUI_CYAN;
    case LOG_LEVEL_INFO:  return TUI_GREEN;
    case LOG_LEVEL_WARN:  return TUI_YELLOW;
    case LOG_LEVEL_ERROR: return TUI_RED;
    default:        return TUI_GRAY;
    }
}

static const char *tui_event_level_label(int level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO ";
    case LOG_LEVEL_WARN:  return "WARN ";
    case LOG_LEVEL_ERROR: return "ERROR";
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

    tui_section("Events");

    if (count == 0) {
        tui_printf(" %s(no recent events)%s\n", TUI_GRAY, TUI_RESET);
        return;
    }

    for (int i = 0; i < count; i++) {
        struct tm tm;
        char ts[9];
        char message[TUI_EVENT_DISPLAY_LEN];
        localtime_r(&events[i].ts, &tm);
        snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        tui_truncate_event(events[i].message, message, sizeof(message));

        const char *color = tui_event_level_color(events[i].level);
        tui_printf(" %s %s%s%s %s\n",
                   ts, color, tui_event_level_label(events[i].level), TUI_RESET,
                   message);
    }
}

static void tui_render_footer(const char *hint)
{
    tui_rule();
    tui_printf(" %s%s%s\n", TUI_YELLOW, hint, TUI_RESET);
}

static void tui_finish_frame(void)
{
    tui_write(TUI_CLEAR_REST);
    fflush(stderr);
}

void tui_render_server(uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_HOME);

    tui_render_header(TUND_VERSION, "Server");

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    char value[128];
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
             "Ctrl+C quit · UDP %u · subnet 10.9.0.0/24 · authenticated, not encrypted",
             port);
    tui_render_footer(value);
    tui_finish_frame();
}

void tui_render_client(const char *server_addr, uint16_t port,
                       const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       bool has_server_rtt, uint64_t server_rtt_ms, time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers)
{
    tui_write(TUI_HOME);

    tui_render_header(TUND_VERSION, "Client");

    char uptime[16];
    tui_uptime(start_time, uptime, sizeof(uptime));
    char ip_str[16], mask_str[16];
    inet_ntop(AF_INET, &virt_ip, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &netmask, mask_str, sizeof(mask_str));

    char line[192], rtt_str[16], value[160];
    tui_format_rtt(has_server_rtt, server_rtt_ms, rtt_str, sizeof(rtt_str));
    tui_section("Connection");
    snprintf(line, sizeof(line), "%s:%u", server_addr, port);
    tui_kv("Server", line);
    tui_kv("RTT", rtt_str);
    tui_kv("TUN", tun_name);
    snprintf(line, sizeof(line), "%s/%s", ip_str, mask_str);
    tui_kv("Virtual", line);
    tui_kv("Uptime", uptime);
    tui_rule();

    tui_render_peer_table(peer_count, peers, npeers);
    tui_rule();
    tui_render_events();
    snprintf(value, sizeof(value),
             "Ctrl+C quit · server %s:%u · authenticated, not encrypted",
             server_addr, port);
    tui_render_footer(value);
    tui_finish_frame();
}
