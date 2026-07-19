#include "protocol.h"
#include "server.h"
#include "../../src/core/server/internal.h"
#include "network.h"
#include "tun.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t g_auth_key0 = 0;
uint64_t g_auth_key1 = 0;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);
bool g_tui_active = false;
time_t g_start_time = 0;

void log_msg(enum log_level level, const char *fmt, ...)
{
    if (level < LOG_LEVEL_WARN)
        return;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    (void)dev;
    (void)buf;
    return len;
}

typedef struct {
    uint16_t port;
    const char *key;
    int duration_sec;
} peerforge_server_options_t;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s -k <key> [-p port] [-t seconds]\n"
            "\n"
            "Options:\n"
            "  -k <key>      Shared network key, 12+ characters (required)\n"
            "  -p <port>     UDP port (default: %u)\n"
            "  -t <seconds>  Maximum runtime (default: 20)\n",
            prog, TUND_PORT);
}

static bool parse_long(const char *value, long min, long max, long *out)
{
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < min || parsed > max)
        return false;
    *out = parsed;
    return true;
}

static bool parse_options(int argc, char **argv, peerforge_server_options_t *opts)
{
    opts->port = TUND_PORT;
    opts->key = NULL;
    opts->duration_sec = 20;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        }
        if (i + 1 >= argc) {
            usage(argv[0]);
            return false;
        }

        long value = 0;
        if (strcmp(argv[i], "-k") == 0) {
            opts->key = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            if (!parse_long(argv[++i], 1, 65535, &value))
                return false;
            opts->port = (uint16_t)value;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (!parse_long(argv[++i], 1, 3600, &value))
                return false;
            opts->duration_sec = (int)value;
        } else {
            usage(argv[0]);
            return false;
        }
    }

    if (!opts->key || strlen(opts->key) < 12) {
        usage(argv[0]);
        return false;
    }
    return true;
}

static int run_server(const peerforge_server_options_t *opts)
{
    server_t srv = {0};
    srv.sockfd = SOCK_INVALID;
    srv.tun.fd = -1;
    srv.port = opts->port;
    tund_stop_flag_init(&srv.timeout_quit, false);
    tund_stop_flag_init(&srv.tun_quit, false);
    pthread_mutex_init(&srv.peers_lock, NULL);

    srv.sockfd = net_create_socket(opts->port);
    if (!sock_valid(srv.sockfd)) {
        pthread_mutex_destroy(&srv.peers_lock);
        return 1;
    }

    fprintf(stderr, "peerforge server listening on UDP %u\n", opts->port);

    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    uint64_t deadline = now_ms() + (uint64_t)opts->duration_sec * 1000;
    int result = 0;

    while (now_ms() < deadline) {
        int ready = platform_poll_one(srv.sockfd, 100);
        if (ready < 0) {
            int err = SOCK_Error();
            if (err == SOCK_EINTR)
                continue;
            fprintf(stderr, "poll failed: %s\n", sock_errstr(err));
            result = 1;
            break;
        }
        if (ready == 0)
            continue;

        int n = net_recv(srv.sockfd, buf, sizeof(buf), &from);
        if (n > 0)
            server_handle_packet(&srv, buf, n, &from);
    }

    sock_close(srv.sockfd);
    pthread_mutex_destroy(&srv.peers_lock);
    return result;
}

int main(int argc, char **argv)
{
    peerforge_server_options_t opts;
    if (!parse_options(argc, argv, &opts))
        return 2;

    proto_key_from_passphrase(opts.key, &g_auth_key0, &g_auth_key1);
    return run_server(&opts);
}
