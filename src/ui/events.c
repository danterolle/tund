#include "tui_internal.h"
#include "log.h"

#include <pthread.h>

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

bool tui_events_active(void)
{
    return tui_active;
}

void tui_events_start(void)
{
    pthread_mutex_lock(&tui_events_lock);
    tui_event_start = 0;
    tui_event_count = 0;
    tui_active = true;
    pthread_mutex_unlock(&tui_events_lock);
}

void tui_events_stop(void)
{
    pthread_mutex_lock(&tui_events_lock);
    tui_active = false;
    pthread_mutex_unlock(&tui_events_lock);
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

static const char *tui_event_level_color(int level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG: return TUI_CYAN;
    case LOG_LEVEL_INFO:  return TUI_GREEN;
    case LOG_LEVEL_WARN:  return TUI_YELLOW;
    case LOG_LEVEL_ERROR: return TUI_RED;
    default:              return TUI_GRAY;
    }
}

static const char *tui_event_level_label(int level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO ";
    case LOG_LEVEL_WARN:  return "WARN ";
    case LOG_LEVEL_ERROR: return "ERROR";
    default:              return "LOG  ";
    }
}

static void tui_truncate_event(const char *message, char *buf, size_t len)
{
    if (strlen(message) < len) {
        snprintf(buf, len, "%s", message);
        return;
    }
    if (len < 4) {
        if (len > 0) buf[0] = '\0';
        return;
    }
    memcpy(buf, message, len - 4);
    memcpy(buf + len - 4, "...", 4);
}

void tui_render_events(void)
{
    tui_event_t events[TUI_MAX_EVENTS];
    int count;

    pthread_mutex_lock(&tui_events_lock);
    count = tui_event_count;
    for (int i = 0; i < count; i++)
        events[i] = tui_events[(tui_event_start + i) % TUI_MAX_EVENTS];
    pthread_mutex_unlock(&tui_events_lock);

    tui_section("Events");
    if (count == 0) {
        tui_printf(" %s(no recent events)%s\n", TUI_GRAY, TUI_RESET);
        return;
    }

    for (int i = 0; i < count; i++) {
        struct tm tm;
        char ts[9], message[TUI_EVENT_DISPLAY_LEN];
        localtime_r(&events[i].ts, &tm);
        snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        tui_truncate_event(events[i].message, message, sizeof(message));
        const char *color = tui_event_level_color(events[i].level);
        tui_printf(" %s %s%s%s %s\n", ts, color,
                   tui_event_level_label(events[i].level), TUI_RESET, message);
    }
}
