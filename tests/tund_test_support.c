#include "tund_test_support.h"
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int tund_test_send_count = 0;
tund_test_send_record_t tund_test_sends[TUND_TEST_MAX_SENDS];
int tund_test_tun_write_count = 0;
tund_test_tun_write_record_t tund_test_tun_writes[TUND_TEST_MAX_TUN_WRITES];

bool g_tui_active = false;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);

void log_msg(enum log_level level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

int net_send(socket_t sockfd, uint8_t *buf, int len,
             const struct sockaddr_in *dest)
{
    (void)sockfd;
    if (tund_test_send_count < TUND_TEST_MAX_SENDS) {
        tund_test_send_record_t *record = &tund_test_sends[tund_test_send_count];
        record->len = len;
        memcpy(&record->dest, dest, sizeof(record->dest));
        if (len > 0 && len <= (int)sizeof(record->buf))
            memcpy(record->buf, buf, (size_t)len);
        if (len >= TUND_HDR_SIZE)
            proto_read_hdr(buf, &record->type, &record->payload_len);
    }
    tund_test_send_count++;
    return 0;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    (void)dev;
    if (tund_test_tun_write_count < TUND_TEST_MAX_TUN_WRITES) {
        tund_test_tun_write_record_t *record =
            &tund_test_tun_writes[tund_test_tun_write_count];
        record->len = len;
        if (len > 0 && len <= (int)sizeof(record->buf))
            memcpy(record->buf, buf, (size_t)len);
    }
    tund_test_tun_write_count++;
    return len;
}

void tund_test_reset_io(void)
{
    tund_test_send_count = 0;
    memset(tund_test_sends, 0, sizeof(tund_test_sends));
    tund_test_tun_write_count = 0;
    memset(tund_test_tun_writes, 0, sizeof(tund_test_tun_writes));
}

struct sockaddr_in tund_test_addr(uint32_t host_ip, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(host_ip);
    addr.sin_port = htons(port);
    return addr;
}

void tund_test_build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst)
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

void tund_test_init_client(client_t *cli)
{
    cli->server_addr = tund_test_addr(0xC0000201, TUND_PORT);
    cli->virt_ip = htonl(TUND_IP_START);
    cli->netmask = htonl(TUND_NETMASK);
    atomic_init(&cli->last_server_seen, (uint_fast64_t)time(NULL));
    pthread_mutex_init(&cli->peers_lock, NULL);
}

void tund_test_destroy_client(client_t *cli)
{
    pthread_mutex_destroy(&cli->peers_lock);
}

void tund_test_init_server(server_t *srv)
{
    srv->sockfd = (socket_t)42;
    srv->tun.fd = -1;
    pthread_mutex_init(&srv->peers_lock, NULL);
}

void tund_test_destroy_server(server_t *srv)
{
    pthread_mutex_destroy(&srv->peers_lock);
}

void tund_test_set_server_peer(server_t *srv, int idx, uint32_t vip_host,
                               uint32_t real_host, uint16_t port,
                               const char *name)
{
    bool was_active = srv->peers[idx].active;
    srv->peers[idx].active = true;
    srv->peers[idx].virt_ip = htonl(vip_host);
    srv->peers[idx].real_addr = tund_test_addr(real_host, port);
    memset(srv->peers[idx].name, 0, sizeof(srv->peers[idx].name));
    if (name)
        snprintf(srv->peers[idx].name, TUND_NAME_LEN, "%s", name);
    if (!was_active)
        srv->peer_count++;
}
