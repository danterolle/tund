#ifndef PEERFORGE_NET_H
#define PEERFORGE_NET_H

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET peerforge_socket_t;
#define PEERFORGE_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
typedef int peerforge_socket_t;
#define PEERFORGE_INVALID_SOCKET (-1)
#endif

int peerforge_net_init(void);
void peerforge_net_cleanup(void);
void peerforge_socket_close(peerforge_socket_t sock);
peerforge_socket_t peerforge_socket_open(void);
int peerforge_resolve_server(const char *host, uint16_t port, struct sockaddr_in *out);
int peerforge_wait_readable(peerforge_socket_t sock, int timeout_ms);
uint64_t peerforge_now_ms(void);

#endif /* PEERFORGE_NET_H */
