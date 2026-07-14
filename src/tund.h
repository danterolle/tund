#ifndef TUND_H
#define TUND_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <pthread.h>

typedef SOCKET socket_t;
#define SOCKET_FMT "%llu"
#define SOCKET_ARG(s) ((unsigned long long)(s))
#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_EAGAIN      WSAEWOULDBLOCK
#define SOCK_EINTR       WSAEINTR
#define SOCK_Error()     ((int)WSAGetLastError())
#define SOCK_INVALID     INVALID_SOCKET

static inline int sock_close(socket_t s) { return closesocket(s); }
static inline int sock_valid(socket_t s) { return s != INVALID_SOCKET; }

static inline const char *sock_errstr(int err)
{
    static char buf[256];
    buf[0] = '\0';
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, sizeof(buf), NULL);
    if (buf[0] == '\0')
        snprintf(buf, sizeof(buf), "WSA error %d", err);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == '.'))
        buf[--n] = '\0';
    return buf;
}

static inline void platform_sleep(unsigned int sec)
{
    Sleep(sec * 1000);
}

#define localtime_r(t, tm) localtime_s((tm), (t))

static inline uint64_t now_ms(void)
{
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (uint64_t)(cnt.QuadPart * 1000 / freq.QuadPart);
}

static inline int platform_poll_one(socket_t fd, int timeout_ms)
{
#ifdef _WIN32
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(0, &readfds, NULL, NULL, &tv);
#else
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    return poll(&pfd, 1, timeout_ms);
#endif
}

#else  /* POSIX */

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <poll.h>

typedef int socket_t;
#define SOCKET_FMT "%d"
#define SOCKET_ARG(s) (s)
#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_EAGAIN      EAGAIN
#define SOCK_EINTR       EINTR
#define SOCK_Error()     (errno)
#define SOCK_INVALID     (-1)

static inline int sock_close(socket_t s) { return close(s); }
static inline int sock_valid(socket_t s) { return s >= 0; }

static inline const char *sock_errstr(int err) { return strerror(err); }

static inline void platform_sleep(unsigned int sec) { sleep(sec); }

static inline int platform_poll_one(socket_t fd, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    return poll(&pfd, 1, timeout_ms);
}

static inline uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

#endif

#include "protocol.h"

#define TUND_MAX_PEERS    253     /* 10.9.0.2 .. 10.9.0.254 */
#define TUND_VERSION      "1.3"

enum log_level {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
};

static const char *log_level_str[] UNUSED = {
    "DEBUG", "INFO ", "WARN ", "ERROR"
};

static const char *log_level_color[] UNUSED = {
    "\033[36m",   /* cyan   */
    "\033[32m",   /* green  */
    "\033[33m",   /* yellow */
    "\033[31m",   /* red    */
};

#define LOG_RESET "\033[0m"

extern int g_log_level;
extern volatile bool g_tui_active;
extern time_t g_start_time;

#define LOG(level, fmt, ...) do {                                           \
    if ((level) >= g_log_level) {                                           \
        time_t _t = time(NULL);                                             \
        struct tm _tm;                                                      \
        localtime_r(&_t, &_tm);                                             \
        fprintf(stderr, "%s[%02d:%02d:%02d] [%s] " fmt LOG_RESET "\n",     \
                log_level_color[(level)],                                    \
                _tm.tm_hour, _tm.tm_min, _tm.tm_sec,                        \
                log_level_str[(level)],                                      \
                ##__VA_ARGS__);                                              \
    }                                                                       \
} while (0)

#define LOG_DEBUG(fmt, ...)  LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   LOG(LOG_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   LOG(LOG_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  LOG(LOG_ERROR, fmt, ##__VA_ARGS__)

typedef struct {
    bool                active;
    uint32_t            virt_ip;                    /* network byte order */
    char                name[TUND_NAME_LEN];
    struct sockaddr_in  real_addr;                  /* actual UDP endpoint */
    time_t              last_seen;
} peer_t;

typedef enum {
    MODE_SERVER,
    MODE_CLIENT,
} tund_mode_t;

typedef struct {
    tund_mode_t   mode;
    uint16_t        port;
    char            server_ip[64];                  /* client only: server addr */
    char            client_name[TUND_NAME_LEN];
    char            access_key[128];
    int             log_level;
    bool            tui_mode;
} config_t;

static inline const char *ip_to_str(uint32_t ip_nbo)
{
    struct in_addr a;
    a.s_addr = ip_nbo;
    return inet_ntoa(a);
}

extern volatile int g_running;
extern uint64_t g_auth_key0;
extern uint64_t g_auth_key1;

#endif /* TUND_H */
