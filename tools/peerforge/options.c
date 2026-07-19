#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void peerforge_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s -s <server> -k <key> [-p port] [-n clients] [-t ms] [-K rounds] [-d pairs]\n"
            "\n"
            "Options:\n"
            "  -s <server>   TunD server address (required)\n"
            "  -k <key>      Shared network key, 12+ characters (required)\n"
            "  -p <port>     UDP port (default: %u)\n"
            "  -n <clients>  Simulated clients, 1-%d (default: 32)\n"
            "  -t <ms>       Per-client receive timeout (default: 3000)\n"
            "  -K <rounds>   Keepalive rounds after registration (default: 1)\n"
            "  -d <pairs>    Client-to-client DATA probes after registration (default: 0)\n",
            prog, TUND_PORT, PEERFORGE_MAX_CLIENTS);
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

static void set_defaults(peerforge_options_t *opts)
{
    opts->server = NULL;
    opts->key = NULL;
    opts->port = TUND_PORT;
    opts->clients = 32;
    opts->timeout_ms = 3000;
    opts->keepalives = 1;
    opts->data_pairs = 0;
}

bool peerforge_parse_options(int argc, char **argv, peerforge_options_t *opts)
{
    set_defaults(opts);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            peerforge_usage(argv[0]);
            exit(0);
        }
        if (i + 1 >= argc) {
            peerforge_usage(argv[0]);
            return false;
        }

        long value = 0;
        if (strcmp(argv[i], "-s") == 0) {
            opts->server = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0) {
            opts->key = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            if (!parse_long(argv[++i], 1, 65535, &value)) return false;
            opts->port = (uint16_t)value;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (!parse_long(argv[++i], 1, PEERFORGE_MAX_CLIENTS, &value)) return false;
            opts->clients = (int)value;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (!parse_long(argv[++i], 1, 60000, &value)) return false;
            opts->timeout_ms = (int)value;
        } else if (strcmp(argv[i], "-K") == 0) {
            if (!parse_long(argv[++i], 0, 1000, &value)) return false;
            opts->keepalives = (int)value;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (!parse_long(argv[++i], 0, 1000000, &value)) return false;
            opts->data_pairs = (int)value;
        } else {
            peerforge_usage(argv[0]);
            return false;
        }
    }

    if (!opts->server || !opts->key || strlen(opts->key) < 12) {
        peerforge_usage(argv[0]);
        return false;
    }
    return true;
}
