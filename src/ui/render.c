#include "tui_internal.h"

void tui_write(const char *s) {
    fputs(s, stderr);
}

void tui_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void tui_rule(void) {
    tui_printf(" %s", TUI_GRAY);
    for (int i = 0; i < TUI_WIDTH; i++) tui_write("─");
    tui_printf("%s\n", TUI_RESET);
}

void tui_render_header(const char *version, const char *mode) {
    tui_printf(" %s◆ TunD%s %sv%s%s %s— %s%s\n", TUI_BOLD TUI_CYAN, TUI_RESET, TUI_YELLOW, version,
               TUI_RESET, TUI_GRAY, mode, TUI_RESET);
    tui_rule();
}

void tui_section(const char *title) {
    tui_printf(" %s%s%s\n", TUI_BOLD TUI_CYAN, title, TUI_RESET);
}

void tui_kv(const char *label, const char *value) {
    tui_printf(" %s%-10s%s %s\n", TUI_GRAY, label, TUI_RESET, value);
}

void tui_uptime(time_t start, char *buf, size_t len) {
    int secs = (int)(time(NULL) - start);
    snprintf(buf, len, "%02d:%02d:%02d", secs / 3600, (secs % 3600) / 60, secs % 60);
}

static void tui_timeago(time_t ts, char *buf, size_t len) {
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

static void tui_format_bytes(uint64_t bytes, char *buf, size_t len) {
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

void tui_format_rtt(bool has_rtt, uint64_t rtt_ms, char *buf, size_t len) {
    if (!has_rtt)
        snprintf(buf, len, "-");
    else if (rtt_ms > 9999)
        snprintf(buf, len, ">9999ms");
    else
        snprintf(buf, len, "%llums", (unsigned long long)rtt_ms);
}

void tui_render_peer_table(int peer_count, const tui_peer_t *peers, int npeers) {
    char title[32];
    snprintf(title, sizeof(title), "Peers (%d)", peer_count);
    tui_section(title);
    tui_printf(" %s%-8s %-15s %-18s %7s %7s %7s %s%s\n", TUI_GRAY, "Status", "Virtual IP", "Name",
               "KA RTT", "In", "Out", "Last", TUI_RESET);

    bool any = false;
    for (int i = 0; i < npeers; i++) {
        if (!peers[i].active) continue;
        any = true;
        char ip[16], ago[32], rtt[16], in[16], out[16];
        inet_ntop(AF_INET, &peers[i].virt_ip, ip, sizeof(ip));
        tui_timeago(peers[i].last_seen, ago, sizeof(ago));
        tui_format_rtt(peers[i].has_rtt, peers[i].rtt_ms, rtt, sizeof(rtt));
        tui_format_bytes(peers[i].bytes_in, in, sizeof(in));
        tui_format_bytes(peers[i].bytes_out, out, sizeof(out));

        int age = (int)(time(NULL) - peers[i].last_seen);
        const char *dot = age < 10   ? TUI_GREEN "●" TUI_RESET
                          : age < 30 ? TUI_YELLOW "●" TUI_RESET
                                     : TUI_RED "●" TUI_RESET;
        const char *status = age < 10 ? "online" : age < 30 ? "stale" : "timeout";
        tui_printf(" %s %-7s %-15s %-18.18s %7s %7s %7s %s\n", dot, status, ip, peers[i].name, rtt,
                   in, out, ago);
    }
    if (!any) tui_printf(" %s(no peers yet)%s\n", TUI_GRAY, TUI_RESET);
}

void tui_render_footer(const char *hint) {
    tui_rule();
    tui_printf(" %s%s%s\n", TUI_YELLOW, hint, TUI_RESET);
}

void tui_finish_frame(void) {
    tui_write(TUI_CLEAR_REST);
    fflush(stderr);
}
