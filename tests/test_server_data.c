#include "../src/core/server/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int net_send_count = 0;
static int net_send_len = 0;
static uint8_t net_send_buf[TUND_MAX_PKT];
static struct sockaddr_in net_send_dest;
static int tun_write_count = 0;
static int tun_write_len = 0;
static uint8_t tun_write_buf[TUND_MAX_PAYLOAD];

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
    net_send_count++;
    net_send_len = len;
    memcpy(&net_send_dest, dest, sizeof(net_send_dest));
    if (len > 0 && len <= (int)sizeof(net_send_buf))
        memcpy(net_send_buf, buf, (size_t)len);
    return 0;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    (void)dev;
    tun_write_count++;
    tun_write_len = len;
    if (len > 0 && len <= (int)sizeof(tun_write_buf))
        memcpy(tun_write_buf, buf, (size_t)len);
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

static void set_peer(server_t *srv, int idx, uint32_t vip_host,
                     uint32_t real_host, uint16_t port, const char *name)
{
    srv->peers[idx].active = true;
    srv->peers[idx].virt_ip = htonl(vip_host);
    srv->peers[idx].real_addr = test_addr(real_host, port);
    snprintf(srv->peers[idx].name, TUND_NAME_LEN, "%s", name);
    srv->peer_count++;
}

static void setup_server(server_t *srv)
{
    memset(srv->peers, 0, sizeof(srv->peers));
    srv->peer_count = 0;
    set_peer(srv, 0, TUND_IP_START, 0xC0000201, 10001, "sender");
    set_peer(srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "target");
    set_peer(srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "other");
}

static void reset_capture(void)
{
    net_send_count = 0;
    net_send_len = 0;
    memset(net_send_buf, 0, sizeof(net_send_buf));
    memset(&net_send_dest, 0, sizeof(net_send_dest));
    tun_write_count = 0;
    tun_write_len = 0;
    memset(tun_write_buf, 0, sizeof(tun_write_buf));
}

static void build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst)
{
    memset(pkt, 0, 20);
    pkt[0] = 0x45;
    pkt[8] = 64;
    pkt[9] = 253;
    uint16_t total = htons(20);
    memcpy(pkt + 2, &total, sizeof(total));
    memcpy(pkt + 12, &src, sizeof(src));
    memcpy(pkt + 16, &dst, sizeof(dst));
}

static void check_forwarded_data_message(const uint8_t *ip_pkt)
{
    uint8_t type = 0;
    uint16_t payload_len = 0;

    CHECK(proto_read_hdr(net_send_buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == 20);
    CHECK(net_send_len == TUND_HDR_SIZE + 20);
    CHECK(memcmp(net_send_buf + TUND_HDR_SIZE, ip_pkt, 20) == 0);
}

static void test_unicast_forwarding(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    setup_server(&srv);
    build_ipv4_packet(pkt, srv.peers[0].virt_ip, srv.peers[1].virt_ip);
    reset_capture();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(net_send_count == 1);
    CHECK(tun_write_count == 0);
    CHECK(net_send_dest.sin_addr.s_addr == srv.peers[1].real_addr.sin_addr.s_addr);
    CHECK(net_send_dest.sin_port == srv.peers[1].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == 0);
    check_forwarded_data_message(pkt);
}

static void test_server_destination_writes_tun(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    setup_server(&srv);
    build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_SERVER_IP));
    reset_capture();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(net_send_count == 0);
    CHECK(tun_write_count == 1);
    CHECK(tun_write_len == (int)sizeof(pkt));
    CHECK(memcmp(tun_write_buf, pkt, sizeof(pkt)) == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
}

static void test_broadcast_forwarding(void)
{
    server_t srv = {0};
    uint8_t pkt[20];
    uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);

    setup_server(&srv);
    build_ipv4_packet(pkt, srv.peers[0].virt_ip, broadcast_ip);
    reset_capture();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(net_send_count == 2);
    CHECK(tun_write_count == 1);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[0].bytes_out == 0);
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == sizeof(pkt));
    CHECK(memcmp(tun_write_buf, pkt, sizeof(pkt)) == 0);
    check_forwarded_data_message(pkt);
}

static void test_unknown_destination_drops_after_accounting(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    setup_server(&srv);
    build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_IP_START + 42));
    reset_capture();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(net_send_count == 0);
    CHECK(tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == 0);
}

static void test_unregistered_and_short_packets_drop(void)
{
    server_t srv = {0};
    uint8_t pkt[20];
    struct sockaddr_in stranger = test_addr(0xC00002FE, 20000);

    setup_server(&srv);
    build_ipv4_packet(pkt, htonl(TUND_IP_START), htonl(TUND_IP_START + 1));
    reset_capture();

    server_handle_data(&srv, pkt, sizeof(pkt), &stranger);
    CHECK(net_send_count == 0);
    CHECK(tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);

    server_handle_data(&srv, pkt, 19, &srv.peers[0].real_addr);
    CHECK(net_send_count == 0);
    CHECK(tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);
}

int main(void)
{
    test_unicast_forwarding();
    test_server_destination_writes_tun();
    test_broadcast_forwarding();
    test_unknown_destination_drops_after_accounting();
    test_unregistered_and_short_packets_drop();

    if (failures != 0) {
        fprintf(stderr, "%d server data test(s) failed\n", failures);
        return 1;
    }

    puts("server data tests passed");
    return 0;
}
