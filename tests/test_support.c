#include "test_support.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int test_failures = 0;
int test_send_count = 0;
test_send_record_t test_sends[TEST_MAX_SENDS];
int test_tun_write_count = 0;
test_tun_write_record_t test_tun_writes[TEST_MAX_TUN_WRITES];

bool g_tui_active = false;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);

void test_check(bool ok, const char *file, int line, const char *expr)
{
    if (!ok) {
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expr);
        test_failures++;
    }
}

int test_finish(const char *suite_name)
{
    if (test_failures != 0) {
        fprintf(stderr, "%d %s failed\n", test_failures, suite_name);
        return 1;
    }

    printf("%s passed\n", suite_name);
    return 0;
}

void log_msg(enum log_level level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

int net_send(socket_t sockfd, uint8_t *buf, int len,
             const struct sockaddr_in *dest)
{
    (void)sockfd;
    if (test_send_count < TEST_MAX_SENDS) {
        test_send_record_t *record = &test_sends[test_send_count];
        record->len = len;
        memcpy(&record->dest, dest, sizeof(record->dest));
        if (len > 0 && len <= (int)sizeof(record->buf))
            memcpy(record->buf, buf, (size_t)len);
        if (len >= TUND_HDR_SIZE)
            proto_read_hdr(buf, &record->type, &record->payload_len);
    }
    test_send_count++;
    return 0;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    (void)dev;
    if (test_tun_write_count < TEST_MAX_TUN_WRITES) {
        test_tun_write_record_t *record = &test_tun_writes[test_tun_write_count];
        record->len = len;
        if (len > 0 && len <= (int)sizeof(record->buf))
            memcpy(record->buf, buf, (size_t)len);
    }
    test_tun_write_count++;
    return len;
}

void test_reset_io(void)
{
    test_send_count = 0;
    memset(test_sends, 0, sizeof(test_sends));
    test_tun_write_count = 0;
    memset(test_tun_writes, 0, sizeof(test_tun_writes));
}

struct sockaddr_in test_addr(uint32_t host_ip, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(host_ip);
    addr.sin_port = htons(port);
    return addr;
}

void test_build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst)
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

void test_init_client(client_t *cli)
{
    cli->server_addr = test_addr(0xC0000201, TUND_PORT);
    cli->virt_ip = htonl(TUND_IP_START);
    cli->netmask = htonl(TUND_NETMASK);
    pthread_mutex_init(&cli->peers_lock, NULL);
}

void test_destroy_client(client_t *cli)
{
    pthread_mutex_destroy(&cli->peers_lock);
}

void test_init_server(server_t *srv)
{
    srv->sockfd = (socket_t)42;
    srv->tun.fd = -1;
    pthread_mutex_init(&srv->peers_lock, NULL);
}

void test_destroy_server(server_t *srv)
{
    pthread_mutex_destroy(&srv->peers_lock);
}

void test_set_server_peer(server_t *srv, int idx, uint32_t vip_host,
                          uint32_t real_host, uint16_t port,
                          const char *name)
{
    bool was_active = srv->peers[idx].active;
    srv->peers[idx].active = true;
    srv->peers[idx].virt_ip = htonl(vip_host);
    srv->peers[idx].real_addr = test_addr(real_host, port);
    memset(srv->peers[idx].name, 0, sizeof(srv->peers[idx].name));
    if (name)
        snprintf(srv->peers[idx].name, TUND_NAME_LEN, "%s", name);
    if (!was_active)
        srv->peer_count++;
}
