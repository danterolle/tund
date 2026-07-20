#include "client.h"
#include "net.h"
#include "options.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>

static void close_clients(peerforge_client_t *clients, int count) {
    for (int i = 0; i < count; i++) peerforge_client_close(&clients[i]);
}

static int run_peerforge(const peerforge_options_t *opts) {
    peerforge_session_t session;
    if (peerforge_resolve_server(opts->server, opts->port, &session.server) < 0) return 1;
    proto_key_from_passphrase(opts->key, &session.key0, &session.key1);
    session.timeout_ms = opts->timeout_ms;

    peerforge_client_t *clients = calloc((size_t)opts->clients, sizeof(*clients));
    if (!clients) return 1;
    for (int i = 0; i < opts->clients; i++) peerforge_client_init(&clients[i]);

    uint64_t started = peerforge_now_ms();
    int registered = 0;
    for (int i = 0; i < opts->clients; i++) {
        if (peerforge_client_open(&clients[i]) == 0 &&
            peerforge_register_client(&clients[i], i, &session) == 0) {
            registered++;
        }
        for (int j = 0; j <= i; j++) {
            if (clients[j].sock != PEERFORGE_INVALID_SOCKET)
                peerforge_client_drain(&clients[j], &session);
        }
    }
    uint64_t registered_ms = peerforge_now_ms() - started;

    int keepalive_ok = 0;
    for (int round = 0; round < opts->keepalives; round++)
        keepalive_ok += peerforge_keepalive_round(clients, opts->clients, &session);

    int data_ok = peerforge_data_probe(clients, opts->clients, opts->data_pairs, &session);

    printf("registered: %d/%d in %llums\n", registered, opts->clients,
           (unsigned long long)registered_ms);
    if (opts->keepalives > 0)
        printf("keepalive acks: %d/%d\n", keepalive_ok, registered * opts->keepalives);
    if (opts->data_pairs > 0) printf("data probes: %d/%d\n", data_ok, opts->data_pairs);

    close_clients(clients, opts->clients);
    free(clients);

    if (registered != opts->clients) return 1;
    if (opts->keepalives > 0 && keepalive_ok != registered * opts->keepalives) return 1;
    if (opts->data_pairs > 0 && data_ok != opts->data_pairs) return 1;
    return 0;
}

int main(int argc, char **argv) {
    peerforge_options_t opts;
    if (!peerforge_parse_options(argc, argv, &opts)) return 2;

    if (peerforge_net_init() < 0) return 1;
    int result = run_peerforge(&opts);
    peerforge_net_cleanup();
    return result;
}
