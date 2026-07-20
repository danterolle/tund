#ifndef TUND_NETWORK_H
#define TUND_NETWORK_H

#include "tund.h"
#include "tui.h"

#define NET_RECV_NO_DATA     0
#define NET_RECV_ERROR       -1
#define NET_RECV_AUTH_FAILED -2

socket_t net_create_socket(uint16_t bind_port);
int net_send(socket_t sockfd, uint8_t *buf, int len, const struct sockaddr_in *dest);
int net_recv(socket_t sockfd, uint8_t *buf, int bufsize, struct sockaddr_in *from);
int net_resolve(const char *host, uint16_t port, struct sockaddr_in *out);
bool net_addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b);
int net_poll_peers(socket_t sockfd, peer_t *peers, int max_peers, pthread_mutex_t *lock,
                   tui_peer_t *tui_peers, int *npeers);

#endif /* TUND_NETWORK_H */
