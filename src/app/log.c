#include "log.h"
#include "tui.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define localtime_r(t, tm) localtime_s((tm), (t))
#endif

#define LOG_RESET "\033[0m"

extern volatile bool g_tui_active;

static const char *log_level_str[LOG_LEVEL_COUNT] = {
    "DEBUG", "INFO ", "WARN ", "ERROR"
};

static const char *log_level_color[LOG_LEVEL_COUNT] = {
    "\033[36m", "\033[32m", "\033[33m", "\033[31m"
};

void log_msg(enum log_level level, const char *fmt, ...)
{
    if ((int)level < g_log_level)
        return;

    int idx = (level >= 0 && level < LOG_LEVEL_COUNT) ? (int)level : LOG_LEVEL_ERROR;
    char msg[8192];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (g_tui_active && tui_events_active()) {
        tui_add_event(idx, msg);
        return;
    }

    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    fprintf(stderr, "%s[%02d:%02d:%02d] [%s] %s" LOG_RESET "\n",
            log_level_color[idx],
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            log_level_str[idx],
            msg);
}
