#include "../src/core/client/internal.h"

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

bool g_tui_active = false;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);

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

static void init_client(client_t *cli)
{
    cli->server_addr = test_addr(0xC0000201, TUND_PORT);
    cli->virt_ip = htonl(TUND_IP_START);
    cli->netmask = htonl(TUND_NETMASK);
    pthread_mutex_init(&cli->peers_lock, NULL);
}

static void destroy_client(client_t *cli)
{
    pthread_mutex_destroy(&cli->peers_lock);
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

static void build_peer_list(uint8_t *buf)
{
    msg_peer_entry_t entries[2];
    memset(entries, 0, sizeof(entries));
    entries[0].virt_ip = htonl(TUND_IP_START + 1);
    snprintf(entries[0].name, sizeof(entries[0].name), "alpha");
    entries[0].status = 1;
    entries[1].virt_ip = htonl(TUND_IP_START + 2);
    snprintf(entries[1].name, sizeof(entries[1].name), "beta");
    entries[1].status = 1;

    proto_write_hdr(buf, MSG_PEER_LIST, sizeof(entries));
    memcpy(buf + TUND_HDR_SIZE, entries, sizeof(entries));
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

static void test_peer_list_updates_table(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    init_client(&cli);
    build_peer_list(buf);
    client_handle_server_packet(&cli, buf,
                                TUND_HDR_SIZE + 2 * (int)sizeof(msg_peer_entry_t));

    CHECK(cli.peer_count == 2);
    CHECK(cli.peers[0].active);
    CHECK(cli.peers[0].virt_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);
    CHECK(cli.peers[1].active);
    CHECK(cli.peers[1].virt_ip == htonl(TUND_IP_START + 2));
    CHECK(strcmp(cli.peers[1].name, "beta") == 0);
    destroy_client(&cli);
}

static void test_peer_join_and_leave(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint32_t peer_ip = htonl(TUND_IP_START + 1);

    init_client(&cli);
    int len = proto_build_peer_join(buf, peer_ip, "alpha");
    client_handle_server_packet(&cli, buf, len);
    CHECK(cli.peer_count == 1);
    CHECK(cli.peers[0].active);
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);

    len = proto_build_peer_leave(buf, peer_ip);
    client_handle_server_packet(&cli, buf, len);
    CHECK(cli.peer_count == 0);
    CHECK(!cli.peers[0].active);
    destroy_client(&cli);
}

static void test_data_writes_tun_and_accounts_peer(void)
{
    client_t cli = {0};
    uint8_t ip_pkt[20];
    uint8_t buf[TUND_MAX_PKT];
    uint32_t peer_ip = htonl(TUND_IP_START + 1);

    init_client(&cli);
    client_update_peer(&cli, peer_ip, "alpha", true);
    build_ipv4_packet(ip_pkt, peer_ip, cli.virt_ip);
    int len = proto_build_data(buf, ip_pkt, sizeof(ip_pkt));
    reset_capture();

    client_handle_server_packet(&cli, buf, len);

    CHECK(tun_write_count == 1);
    CHECK(tun_write_len == (int)sizeof(ip_pkt));
    CHECK(memcmp(tun_write_buf, ip_pkt, sizeof(ip_pkt)) == 0);
    CHECK(cli.peers[0].bytes_in == sizeof(ip_pkt));
    CHECK(cli.peers[0].bytes_out == 0);
    destroy_client(&cli);
}

static void test_keepalive_replies_with_ack(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint8_t type = 0;
    uint16_t payload_len = 0;
    uint64_t timestamp = 0;

    init_client(&cli);
    int len = proto_build_keepalive(buf, 0x0102030405060708ULL);
    reset_capture();

    client_handle_server_packet(&cli, buf, len);

    CHECK(net_send_count == 1);
    CHECK(net_send_dest.sin_addr.s_addr == cli.server_addr.sin_addr.s_addr);
    CHECK(net_send_dest.sin_port == cli.server_addr.sin_port);
    CHECK(proto_read_hdr(net_send_buf, &type, &payload_len) == 0);
    CHECK(type == MSG_KEEPALIVE_ACK);
    CHECK(payload_len == 8);
    CHECK(proto_read_keepalive_timestamp(net_send_buf + TUND_HDR_SIZE,
                                         payload_len, &timestamp));
    CHECK(timestamp == 0x0102030405060708ULL);
    CHECK(net_send_len == TUND_HDR_SIZE + 8);
    destroy_client(&cli);
}

static void test_keepalive_ack_updates_rtt(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint64_t sent_at = now_ms() - 10;

    init_client(&cli);
    int len = proto_build_keepalive_ack(buf, sent_at);
    client_handle_server_packet(&cli, buf, len);

    CHECK(cli.has_server_rtt);
    CHECK(cli.server_rtt_ms <= 1000);
    destroy_client(&cli);
}

static void test_disconnect_requests_stop(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    init_client(&cli);
    tund_stop_flag_store(&g_running, true);
    int len = proto_build_disconnect(buf);

    client_handle_server_packet(&cli, buf, len);

    CHECK(!tund_is_running());
    destroy_client(&cli);
}

static void test_truncated_packet_is_ignored(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    init_client(&cli);
    build_peer_list(buf);
    reset_capture();
    client_handle_server_packet(&cli, buf, TUND_HDR_SIZE + 1);

    CHECK(cli.peer_count == 0);
    CHECK(tun_write_count == 0);
    CHECK(net_send_count == 0);
    destroy_client(&cli);
}

int main(void)
{
    test_peer_list_updates_table();
    test_peer_join_and_leave();
    test_data_writes_tun_and_accounts_peer();
    test_keepalive_replies_with_ack();
    test_keepalive_ack_updates_rtt();
    test_disconnect_requests_stop();
    test_truncated_packet_is_ignored();

    if (failures != 0) {
        fprintf(stderr, "%d client handler test(s) failed\n", failures);
        return 1;
    }

    puts("client handler tests passed");
    return 0;
}
