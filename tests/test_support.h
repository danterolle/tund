#ifndef TUND_TEST_SUPPORT_H
#define TUND_TEST_SUPPORT_H

#include "../src/core/client/client.h"
#include "../src/core/server/server.h"
#include "network.h"
#include "tun.h"

#include <stddef.h>

#define TEST_MAX_SENDS 8
#define TEST_MAX_TUN_WRITES 8

typedef struct {
    int len;
    uint8_t type;
    uint16_t payload_len;
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in dest;
} test_send_record_t;

typedef struct {
    int len;
    uint8_t buf[TUND_MAX_PAYLOAD];
} test_tun_write_record_t;

extern int test_failures;
extern int test_send_count;
extern test_send_record_t test_sends[TEST_MAX_SENDS];
extern int test_tun_write_count;
extern test_tun_write_record_t test_tun_writes[TEST_MAX_TUN_WRITES];

#define CHECK(expr) test_check((expr), __FILE__, __LINE__, #expr)

void test_check(bool ok, const char *file, int line, const char *expr);
int test_finish(const char *suite_name);

void test_reset_io(void);
struct sockaddr_in test_addr(uint32_t host_ip, uint16_t port);
void test_build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst);

void test_init_client(client_t *cli);
void test_destroy_client(client_t *cli);
void test_init_server(server_t *srv);
void test_destroy_server(server_t *srv);
void test_set_server_peer(server_t *srv, int idx, uint32_t vip_host,
                          uint32_t real_host, uint16_t port,
                          const char *name);

#endif /* TUND_TEST_SUPPORT_H */
