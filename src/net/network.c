#include "network.h"
#include "log.h"
#include "tui.h"

#ifdef _WIN32
static int ensure_winsock(void) {
    static int initialized = 0;
    if (initialized) return 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG_ERROR("WSAStartup failed");
        return -1;
    }
    initialized = 1;
    return 0;
}

#else
#define ensure_winsock() 0
#endif

socket_t net_create_socket(uint16_t bind_port) {
    if (ensure_winsock() < 0) return (socket_t)-1;

    socket_t sockfd;
    struct sockaddr_in addr;
    int optval = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == (socket_t)-1) {
        LOG_ERROR("Cannot create UDP socket: %s. Check OS socket permissions and try again.",
                  sock_errstr(SOCK_Error()));
        return (socket_t)-1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(bind_port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Cannot bind UDP port %u: %s. If another TunD/server is running, stop it or "
                  "choose a different port with -p.",
                  bind_port, sock_errstr(SOCK_Error()));
        sock_close(sockfd);
        return (socket_t)-1;
    }

    LOG_INFO("UDP socket bound to port %u (fd=" SOCKET_FMT ")", bind_port, SOCKET_ARG(sockfd));
    return sockfd;
}

int net_send(socket_t sockfd, uint8_t *buf, int len, const struct sockaddr_in *dest) {
    if (len < TUND_HDR_SIZE) return -1;
    proto_sign(buf, len, g_auth_key0, g_auth_key1);
    ssize_t sent = sendto(sockfd, (const char *)buf, (size_t)len, 0, (const struct sockaddr *)dest,
                          sizeof(*dest));
    if (sent < 0) {
        char dest_ip[TUND_IP_STR_LEN];
        LOG_ERROR("sendto(%s:%u) failed: %s",
                  ip_to_str_buf(dest->sin_addr.s_addr, dest_ip, sizeof(dest_ip)),
                  ntohs(dest->sin_port), sock_errstr(SOCK_Error()));
        return -1;
    }
    return 0;
}

int net_recv(socket_t sockfd, uint8_t *buf, int bufsize, struct sockaddr_in *from) {
    socklen_t fromlen = sizeof(*from);
    ssize_t n =
        recvfrom(sockfd, (char *)buf, (size_t)bufsize, 0, (struct sockaddr *)from, &fromlen);
    if (n < 0) {
        int err = SOCK_Error();
        if (err == SOCK_EAGAIN || err == SOCK_EWOULDBLOCK || err == SOCK_EINTR)
            return NET_RECV_NO_DATA;
        LOG_ERROR("recvfrom() failed: %s", sock_errstr(err));
        return NET_RECV_ERROR;
    }
    if (!proto_verify(buf, (int)n, g_auth_key0, g_auth_key1)) {
        char from_ip[TUND_IP_STR_LEN];
        LOG_DEBUG("Dropped unauthenticated packet from %s:%u",
                  ip_to_str_buf(from->sin_addr.s_addr, from_ip, sizeof(from_ip)),
                  ntohs(from->sin_port));
        return NET_RECV_AUTH_FAILED;
    }
    return (int)n;
}

bool net_addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_family == b->sin_family && a->sin_port == b->sin_port &&
           a->sin_addr.s_addr == b->sin_addr.s_addr;
}

int net_resolve(const char *host, uint16_t port, struct sockaddr_in *out) {
    if (ensure_winsock() < 0) return -1;

    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0) {
        LOG_ERROR(
            "Cannot resolve server '%s': %s. Check the hostname/IP and DNS/network connectivity.",
            host, gai_strerror(err));
        return -1;
    }

    memcpy(out, res->ai_addr, sizeof(*out));
    out->sin_port = htons(port);
    freeaddrinfo(res);

    char out_ip[TUND_IP_STR_LEN];
    LOG_DEBUG("Resolved %s -> %s:%u", host,
              ip_to_str_buf(out->sin_addr.s_addr, out_ip, sizeof(out_ip)), port);
    return 0;
}

int net_poll_peers(socket_t sockfd, peer_t *peers, int max_peers, pthread_mutex_t *lock,
                   tui_peer_t *tui_peers, int *npeers) {
    int ret = platform_poll_one(sockfd, 1000);
    if (ret < 0) {
        int err = SOCK_Error();
        if (err == SOCK_EINTR) return 0;
        LOG_ERROR("poll() failed: %s", sock_errstr(err));
        return -1;
    }
    if (ret == 0) {
        pthread_mutex_lock(lock);
        *npeers = 0;
        for (int i = 0; i < max_peers; i++) {
            tui_peers[*npeers].virt_ip = peers[i].virt_ip;
            tui_peers[*npeers].last_seen = peers[i].last_seen;
            tui_peers[*npeers].bytes_in = peers[i].bytes_in;
            tui_peers[*npeers].bytes_out = peers[i].bytes_out;
            tui_peers[*npeers].rtt_ms = peers[i].rtt_ms;
            tui_peers[*npeers].has_rtt = peers[i].has_rtt;
            tui_peers[*npeers].active = peers[i].active;
            memcpy(tui_peers[*npeers].name, peers[i].name, TUND_NAME_LEN);
            if (peers[i].active) (*npeers)++;
        }
        pthread_mutex_unlock(lock);
        return 0;
    }
    return 1;
}
