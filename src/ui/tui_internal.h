#ifndef TUND_TUI_INTERNAL_H
#define TUND_TUI_INTERNAL_H

#include "tui.h"

#include <stddef.h>

#define TUI_CLEAR       "\033[2J\033[H"
#define TUI_HOME        "\033[H"
#define TUI_CLEAR_REST  "\033[J"
#define TUI_HIDE_CURSOR "\033[?25l"
#define TUI_SHOW_CURSOR "\033[?25h"
#define TUI_ALT_BUF     "\033[?1049h"
#define TUI_MAIN_BUF    "\033[?1049l"

#define TUI_BOLD   "\033[1m"
#define TUI_CYAN   "\033[36m"
#define TUI_YELLOW "\033[33m"
#define TUI_GREEN  "\033[32m"
#define TUI_RED    "\033[31m"
#define TUI_GRAY   "\033[90m"
#define TUI_RESET  "\033[0m"

#define TUI_WIDTH 76

void tui_write(const char *s);
void tui_printf(const char *fmt, ...);
void tui_rule(void);
void tui_render_header(const char *version, const char *mode);
void tui_section(const char *title);
void tui_kv(const char *label, const char *value);
void tui_uptime(time_t start, char *buf, size_t len);
void tui_format_rtt(bool has_rtt, uint64_t rtt_ms, char *buf, size_t len);
void tui_render_peer_table(int peer_count, const tui_peer_t *peers, int npeers);
void tui_render_events(void);
void tui_render_footer(const char *hint);
void tui_finish_frame(void);
void tui_events_start(void);
void tui_events_stop(void);

#endif /* TUND_TUI_INTERNAL_H */
