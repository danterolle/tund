#include "../src/core/server/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_SENDS 8

static int failures = 0;
static int send_count = 0;
static int send_len[MAX_SENDS];
static uint8_t send_type[MAX_SENDS];
static uint16_t send_payload_len[MAX_SENDS];
static uint8_t send_buf[MAX_SENDS][TUND_MAX_PKT];
static struct sockaddr_in send_dest[MAX_SENDS];
static int tun_write_count = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

void log_msg(enum log_level level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

int net_send(socket_t sockfd, uint8_t *buf, int len,
             const struct sockaddr_in *dest)
{
    (void)sockfd;
    if (send_count < MAX_SENDS) {
        send_len[send_count] = len;
        memcpy(&send_dest[send_count], dest, sizeof(send_dest[send_count]));
        if (len > 0 && len <= TUND_MAX_PKT)
            memcpy(send_buf[send_count], buf, (size_t)len);
        if (len >= TUND_HDR_SIZE)
            proto_read_hdr(buf, &send_type[send_count],
                           &send_payload_len[send_count]);
    }
    send_count++;
    return 0;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    (void)dev;
    (void)buf;
    (void)len;
    tun_write_count++;
    return len;
}

static struct sockaddr_in test_addr(uint32_t host_ip, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(host_ip);
    addr.sin_port = htons(port);
    return addr;
}

static void init_server(server_t *srv)
{
    srv->sockfd = (socket_t)42;
    srv->tun.fd = -1;
    pthread_mutex_init(&srv->peers_lock, NULL);
}

static void destroy_server(server_t *srv)
{
    pthread_mutex_destroy(&srv->peers_lock);
}

static void reset_capture(void)
{
    send_count = 0;
    memset(send_len, 0, sizeof(send_len));
    memset(send_type, 0, sizeof(send_type));
    memset(send_payload_len, 0, sizeof(send_payload_len));
    memset(send_buf, 0, sizeof(send_buf));
    memset(send_dest, 0, sizeof(send_dest));
    tun_write_count = 0;
}

static void register_peer(server_t *srv, const struct sockaddr_in *addr,
                          const char *name)
{
    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_register(buf, name);
    server_handle_packet(srv, buf, len, addr);
}

static void test_register_assigns_new_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);

    init_server(&srv);
    reset_capture();
    register_peer(&srv, &addr, "alpha");

    CHECK(srv.peer_count == 1);
    CHECK(srv.peers[0].active);
    CHECK(srv.peers[0].virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(srv.peers[0].name, "alpha") == 0);
    CHECK(send_count == 2);
    CHECK(send_type[0] == MSG_ASSIGN);
    CHECK(send_payload_len[0] == 10);
    CHECK(send_type[1] == MSG_PEER_LIST);
    CHECK(send_payload_len[1] == 0);
    destroy_server(&srv);
}

static void test_register_reconnect_refreshes_existing_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    time_t first_seen;

    init_server(&srv);
    register_peer(&srv, &addr, "alpha");
    first_seen = srv.peers[0].last_seen;

    reset_capture();
    register_peer(&srv, &addr, "ignored");

    CHECK(srv.peer_count == 1);
    CHECK(srv.peers[0].active);
    CHECK(srv.peers[0].virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(srv.peers[0].name, "alpha") == 0);
    CHECK(srv.peers[0].last_seen >= first_seen);
    CHECK(send_count == 1);
    CHECK(send_type[0] == MSG_ASSIGN);
    destroy_server(&srv);
}

static void test_second_register_sends_peer_list_and_join(void)
{
    server_t srv = {0};
    struct sockaddr_in first = test_addr(0xC0000201, 10001);
    struct sockaddr_in second = test_addr(0xC0000202, 10002);

    init_server(&srv);
    register_peer(&srv, &first, "alpha");

    reset_capture();
    register_peer(&srv, &second, "beta");

    CHECK(srv.peer_count == 2);
    CHECK(srv.peers[1].active);
    CHECK(srv.peers[1].virt_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(srv.peers[1].name, "beta") == 0);
    CHECK(send_count == 3);
    CHECK(send_type[0] == MSG_ASSIGN);
    CHECK(send_type[1] == MSG_PEER_LIST);
    CHECK(send_payload_len[1] == sizeof(msg_peer_entry_t));
    const msg_peer_entry_t *entry =
        (const msg_peer_entry_t *)(send_buf[1] + TUND_HDR_SIZE);
    CHECK(entry->virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(entry->name, "alpha") == 0);
    CHECK(send_type[2] == MSG_PEER_JOIN);
    CHECK(send_dest[2].sin_addr.s_addr == first.sin_addr.s_addr);
    CHECK(send_dest[2].sin_port == first.sin_port);
    destroy_server(&srv);
}

static void test_disconnect_marks_peer_inactive_and_broadcasts_leave(void)
{
    server_t srv = {0};
    struct sockaddr_in first = test_addr(0xC0000201, 10001);
    struct sockaddr_in second = test_addr(0xC0000202, 10002);
    uint8_t buf[TUND_MAX_PKT];

    init_server(&srv);
    register_peer(&srv, &first, "alpha");
    register_peer(&srv, &second, "beta");

    reset_capture();
    int len = proto_build_disconnect(buf);
    server_handle_packet(&srv, buf, len, &first);

    CHECK(srv.peer_count == 1);
    CHECK(!srv.peers[0].active);
    CHECK(srv.peers[1].active);
    CHECK(send_count == 1);
    CHECK(send_type[0] == MSG_PEER_LEAVE);
    CHECK(send_dest[0].sin_addr.s_addr == second.sin_addr.s_addr);
    CHECK(send_dest[0].sin_port == second.sin_port);
    destroy_server(&srv);
}

static void test_keepalive_replies_with_ack(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];
    uint64_t timestamp = 0;

    init_server(&srv);
    register_peer(&srv, &addr, "alpha");

    reset_capture();
    int len = proto_build_keepalive(buf, 0x0102030405060708ULL);
    server_handle_packet(&srv, buf, len, &addr);

    CHECK(send_count == 1);
    CHECK(send_type[0] == MSG_KEEPALIVE_ACK);
    CHECK(proto_read_keepalive_timestamp(send_buf[0] + TUND_HDR_SIZE,
                                         send_payload_len[0], &timestamp));
    CHECK(timestamp == 0x0102030405060708ULL);
    CHECK(send_dest[0].sin_addr.s_addr == addr.sin_addr.s_addr);
    CHECK(send_dest[0].sin_port == addr.sin_port);
    destroy_server(&srv);
}

static void test_keepalive_ack_updates_rtt(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];

    init_server(&srv);
    register_peer(&srv, &addr, "alpha");
    int len = proto_build_keepalive_ack(buf, now_ms() - 10);
    server_handle_packet(&srv, buf, len, &addr);

    CHECK(srv.peers[0].has_rtt);
    CHECK(srv.peers[0].rtt_ms <= 1000);
    destroy_server(&srv);
}

static void test_invalid_packets_do_not_register_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];

    init_server(&srv);
    int len = proto_build_register(buf, "alpha");
    buf[0] = 0;
    server_handle_packet(&srv, buf, len, &addr);
    CHECK(srv.peer_count == 0);

    len = proto_build_register(buf, "alpha");
    server_handle_packet(&srv, buf, TUND_HDR_SIZE - 1, &addr);
    CHECK(srv.peer_count == 0);
    destroy_server(&srv);
}

int main(void)
{
    test_register_assigns_new_peer();
    test_register_reconnect_refreshes_existing_peer();
    test_second_register_sends_peer_list_and_join();
    test_disconnect_marks_peer_inactive_and_broadcasts_leave();
    test_keepalive_replies_with_ack();
    test_keepalive_ack_updates_rtt();
    test_invalid_packets_do_not_register_peer();

    if (failures != 0) {
        fprintf(stderr, "%d server handler test(s) failed\n", failures);
        return 1;
    }

    puts("server handler tests passed");
    return 0;
}
