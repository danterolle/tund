#ifndef PEERFORGE_CLIENT_H
#define PEERFORGE_CLIENT_H

#include "net.h"
#include "protocol.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    peerforge_socket_t sock;
    uint32_t virt_ip;
    bool assigned;
} peerforge_client_t;

typedef struct {
    struct sockaddr_in server;
    uint8_t key[TUND_KEY_SIZE];
    int timeout_ms;
} peerforge_session_t;

void peerforge_client_init(peerforge_client_t *client);
int peerforge_client_open(peerforge_client_t *client);
void peerforge_client_close(peerforge_client_t *client);
void peerforge_client_drain(peerforge_client_t *client, const peerforge_session_t *session);
int peerforge_register_client(peerforge_client_t *client, int index,
                              const peerforge_session_t *session);
int peerforge_keepalive_round(peerforge_client_t *clients, int count,
                              const peerforge_session_t *session);
int peerforge_data_probe(peerforge_client_t *clients, int count, int pairs,
                         const peerforge_session_t *session);

#endif /* PEERFORGE_CLIENT_H */
