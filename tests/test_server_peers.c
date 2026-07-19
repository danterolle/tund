#include "../src/core/server/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int send_count = 0;
static int last_send_len = 0;
static uint8_t last_send_buf[TUND_MAX_PKT];
static struct sockaddr_in last_send_dest;

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
    send_count++;
    last_send_len = len;
    memcpy(&last_send_dest, dest, sizeof(last_send_dest));
    if (len > 0 && len <= (int)sizeof(last_send_buf))
        memcpy(last_send_buf, buf, (size_t)len);
    return 0;
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

static void reset_net_capture(void)
{
    send_count = 0;
    last_send_len = 0;
    memset(last_send_buf, 0, sizeof(last_send_buf));
    memset(&last_send_dest, 0, sizeof(last_send_dest));
}

static void set_peer(server_t *srv, int idx, uint32_t vip_host,
                     uint32_t real_host, uint16_t port, const char *name)
{
    srv->peers[idx].active = true;
    srv->peers[idx].virt_ip = htonl(vip_host);
    srv->peers[idx].real_addr = test_addr(real_host, port);
    snprintf(srv->peers[idx].name, TUND_NAME_LEN, "%s", name);
}

static void test_alloc_ip(void)
{
    server_t srv = {0};

    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START));

    set_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START + 1));

    set_peer(&srv, 1, TUND_IP_START + 2, 0xC0000202, 10002, "beta");
    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START + 1));

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        srv.peers[i].active = true;
        srv.peers[i].virt_ip = htonl(TUND_IP_START + (uint32_t)i);
    }
    CHECK(server_alloc_ip(&srv) == 0);
}

static void test_find_helpers(void)
{
    server_t srv = {0};
    struct sockaddr_in addr;

    set_peer(&srv, 3, TUND_IP_START + 3, 0xC0000203, 12003, "gamma");
    srv.peers[4].virt_ip = htonl(TUND_IP_START + 4);
    srv.peers[4].real_addr = test_addr(0xC0000204, 12004);

    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 3)) == 3);
    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 4)) == -1);
    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 5)) == -1);

    addr = test_addr(0xC0000203, 12003);
    CHECK(server_find_peer_by_addr(&srv, &addr) == 3);
    addr = test_addr(0xC0000203, 12004);
    CHECK(server_find_peer_by_addr(&srv, &addr) == -1);
    addr = test_addr(0xC0000204, 12004);
    CHECK(server_find_peer_by_addr(&srv, &addr) == -1);

    CHECK(server_find_free_slot(&srv) == 0);
    for (int i = 0; i < TUND_MAX_PEERS; i++)
        srv.peers[i].active = true;
    CHECK(server_find_free_slot(&srv) == -1);
}

static void test_broadcast_updates_recipients(void)
{
    server_t srv = {0};
    uint8_t buf[TUND_MAX_PKT];

    set_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    set_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "beta");
    set_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "gamma");
    srv.peers[0].bytes_out = 10;
    srv.peers[1].bytes_out = 20;
    srv.peers[2].bytes_out = 30;

    memset(buf, 0xAB, sizeof(buf));
    reset_net_capture();
    server_broadcast(&srv, buf, 24, 1, 100);

    CHECK(send_count == 2);
    CHECK(last_send_len == 24);
    CHECK(last_send_dest.sin_addr.s_addr == srv.peers[2].real_addr.sin_addr.s_addr);
    CHECK(last_send_dest.sin_port == srv.peers[2].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_out == 110);
    CHECK(srv.peers[1].bytes_out == 20);
    CHECK(srv.peers[2].bytes_out == 130);
}

static void test_peer_list_payload(void)
{
    server_t srv = {0};
    uint8_t type = 0;
    uint16_t payload_len = 0;

    set_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "target");
    set_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "alpha");
    set_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "beta");

    reset_net_capture();
    server_send_peer_list(&srv, 0);

    CHECK(send_count == 1);
    CHECK(proto_read_hdr(last_send_buf, &type, &payload_len) == 0);
    CHECK(type == MSG_PEER_LIST);
    CHECK(payload_len == 2 * sizeof(msg_peer_entry_t));
    CHECK(last_send_len == TUND_HDR_SIZE + (int)payload_len);

    const msg_peer_entry_t *entries =
        (const msg_peer_entry_t *)(last_send_buf + TUND_HDR_SIZE);
    CHECK(entries[0].virt_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(entries[0].name, "alpha") == 0);
    CHECK(entries[0].status == 1);
    CHECK(entries[1].virt_ip == htonl(TUND_IP_START + 2));
    CHECK(strcmp(entries[1].name, "beta") == 0);
    CHECK(entries[1].status == 1);
}

int main(void)
{
    test_alloc_ip();
    test_find_helpers();
    test_broadcast_updates_recipients();
    test_peer_list_payload();

    if (failures != 0) {
        fprintf(stderr, "%d server peer test(s) failed\n", failures);
        return 1;
    }

    puts("server peer tests passed");
    return 0;
}
