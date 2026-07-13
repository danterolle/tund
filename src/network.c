#include "network.h"

#ifdef _WIN32
static int ensure_winsock(void)
{
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

static const char* wsa_strerror(int err)
{
    static char buf[256];
    buf[0] = '\0';
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, sizeof(buf), NULL);
    if (buf[0] == '\0')
        snprintf(buf, sizeof(buf), "WSA error %d", err);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == '.'))
        buf[--n] = '\0';
    return buf;
}
#else
#define ensure_winsock() 0
#endif

#ifdef _WIN32
#define sock_errstr(err) wsa_strerror(err)
#else
#define sock_errstr(err) strerror(err)
#endif

socket_t net_create_socket(uint16_t bind_port)
{
    if (ensure_winsock() < 0)
        return (socket_t)-1;

    socket_t sockfd;
    struct sockaddr_in addr;
    int optval = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == (socket_t)-1) {
        LOG_ERROR("socket(UDP) failed: %s", sock_errstr(SOCK_Error()));
        return (socket_t)-1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&optval, sizeof(optval));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(bind_port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("bind(port %u) failed: %s", bind_port, sock_errstr(SOCK_Error()));
        sock_close(sockfd);
        return (socket_t)-1;
    }

    LOG_INFO("UDP socket bound to port %u (fd=" SOCKET_FMT ")", bind_port, SOCKET_ARG(sockfd));
    return sockfd;
}

int net_send(socket_t sockfd, uint8_t *buf, int len,
             const struct sockaddr_in *dest)
{
    if (len < TUND_HDR_SIZE) return -1;
    proto_sign(buf, len, g_auth_key0, g_auth_key1);
    ssize_t sent = sendto(sockfd, (const char *)buf, (size_t)len, 0,
                          (const struct sockaddr *)dest, sizeof(*dest));
    if (sent < 0) {
        LOG_ERROR("sendto(%s:%u) failed: %s",
                  inet_ntoa(dest->sin_addr), ntohs(dest->sin_port),
                  sock_errstr(SOCK_Error()));
        return -1;
    }
    return 0;
}

int net_recv(socket_t sockfd, uint8_t *buf, int bufsize,
             struct sockaddr_in *from)
{
    socklen_t fromlen = sizeof(*from);
    ssize_t n = recvfrom(sockfd, (char *)buf, (size_t)bufsize, 0,
                         (struct sockaddr *)from, &fromlen);
    if (n < 0) {
        int err = SOCK_Error();
        if (err == SOCK_EAGAIN || err == SOCK_EWOULDBLOCK || err == SOCK_EINTR)
            return 0;
        LOG_ERROR("recvfrom() failed: %s", sock_errstr(err));
        return -1;
    }
    if (!proto_verify(buf, (int)n, g_auth_key0, g_auth_key1))
        return 0;
    return (int)n;
}

bool net_addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_family == b->sin_family && a->sin_port == b->sin_port &&
           a->sin_addr.s_addr == b->sin_addr.s_addr;
}

int net_resolve(const char *host, uint16_t port, struct sockaddr_in *out)
{
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0) {
        LOG_ERROR("Cannot resolve '%s': %s", host, gai_strerror(err));
        return -1;
    }

    memcpy(out, res->ai_addr, sizeof(*out));
    out->sin_port = htons(port);
    freeaddrinfo(res);

    LOG_DEBUG("Resolved %s -> %s:%u",
              host, inet_ntoa(out->sin_addr), port);
    return 0;
}
