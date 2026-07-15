#ifndef TUND_LOG_H
#define TUND_LOG_H

enum log_level {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_COUNT = 4,
};

extern int g_log_level;

void log_msg(enum log_level level, const char *fmt, ...);

#define LOG_DEBUG(...) log_msg(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  log_msg(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  log_msg(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif /* TUND_LOG_H */
