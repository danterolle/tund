#include "net.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

int peerforge_net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return -1;
    }
#endif
    return 0;
}

void peerforge_net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void peerforge_socket_close(peerforge_socket_t sock) {
    if (sock == PEERFORGE_INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

peerforge_socket_t peerforge_socket_open(void) {
    return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int peerforge_resolve_server(const char *host, uint16_t port, struct sockaddr_in *out) {
    char port_buf[16];
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    snprintf(port_buf, sizeof(port_buf), "%u", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, port_buf, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "resolve failed for %s:%u\n", host, port);
        return -1;
    }

    memcpy(out, res->ai_addr, sizeof(*out));
    freeaddrinfo(res);
    return 0;
}

int peerforge_wait_readable(peerforge_socket_t sock, int timeout_ms) {
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

#ifdef _WIN32
    return select(0, &rfds, NULL, NULL, &tv);
#else
    return select(sock + 1, &rfds, NULL, NULL, &tv);
#endif
}

uint64_t peerforge_now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}
