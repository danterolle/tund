#ifndef PEERFORGE_OPTIONS_H
#define PEERFORGE_OPTIONS_H

#include "protocol.h"

#include <stdbool.h>
#include <stdint.h>

#define PEERFORGE_MAX_CLIENTS ((int)(TUND_IP_END - TUND_IP_START + 1))

typedef struct {
    const char *server;
    const char *key;
    uint16_t port;
    int clients;
    int timeout_ms;
    int keepalives;
    int data_pairs;
} peerforge_options_t;

void peerforge_usage(const char *prog);
bool peerforge_parse_options(int argc, char **argv, peerforge_options_t *opts);

#endif /* PEERFORGE_OPTIONS_H */
