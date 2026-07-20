#ifndef TUND_H
#define TUND_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <time.h>

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
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(0, &readfds, NULL, NULL, &tv);
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
#define TUND_VERSION      "1.15"
#define TUND_IP_STR_LEN   INET_ADDRSTRLEN
#define TUND_RTT_SMOOTHING_WEIGHT 7  /* EWMA: 7/8 previous, 1/8 latest sample */

extern bool g_tui_active;
extern time_t g_start_time;

typedef atomic_bool tund_stop_flag_t;

/* Stop flags only request loop termination; they do not publish thread data. */
static inline void tund_stop_flag_init(tund_stop_flag_t *flag, bool value)
{
    atomic_init(flag, value);
}

static inline bool tund_stop_flag_load(tund_stop_flag_t *flag)
{
    return atomic_load_explicit(flag, memory_order_relaxed);
}

static inline void tund_stop_flag_store(tund_stop_flag_t *flag, bool value)
{
    atomic_store_explicit(flag, value, memory_order_relaxed);
}

typedef struct {
    bool                active;
    uint32_t            virt_ip;                    /* network byte order */
    char                name[TUND_NAME_LEN];
    struct sockaddr_in  real_addr;                  /* actual UDP endpoint */
    time_t              last_seen;
    uint64_t            bytes_in;
    uint64_t            bytes_out;
    uint64_t            rtt_ms;
    bool                has_rtt;
    proto_replay_window_t replay;
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

static inline const char *ip_to_str_buf(uint32_t ip_nbo, char *buf, size_t len)
{
    struct in_addr a;
    a.s_addr = ip_nbo;
    if (!inet_ntop(AF_INET, &a, buf, (socklen_t)len))
        snprintf(buf, len, "<invalid>");
    return buf;
}

static inline uint64_t smooth_rtt_ms(uint64_t current, uint64_t sample, bool has_current)
{
    if (!has_current)
        return sample;
    return (current * TUND_RTT_SMOOTHING_WEIGHT + sample +
            TUND_RTT_SMOOTHING_WEIGHT / 2) /
           (TUND_RTT_SMOOTHING_WEIGHT + 1);
}

extern tund_stop_flag_t g_running;
extern uint64_t g_auth_key0;
extern uint64_t g_auth_key1;

static inline bool tund_is_running(void)
{
    return tund_stop_flag_load(&g_running);
}

static inline void tund_request_stop(void)
{
    tund_stop_flag_store(&g_running, false);
}

#endif /* TUND_H */
