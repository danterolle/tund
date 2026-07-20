#ifndef TUND_TEST_SUPPORT_H
#define TUND_TEST_SUPPORT_H

#include "sitest.h"
#include "../src/core/client/client.h"
#include "../src/core/server/server.h"
#include "network.h"
#include "tun.h"

#define TUND_TEST_MAX_SENDS      8
#define TUND_TEST_MAX_TUN_WRITES 8

typedef struct {
    int len;
    uint8_t type;
    uint16_t payload_len;
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in dest;
} tund_test_send_record_t;

typedef struct {
    int len;
    uint8_t buf[TUND_MAX_PAYLOAD];
} tund_test_tun_write_record_t;

extern int tund_test_send_count;
extern tund_test_send_record_t tund_test_sends[TUND_TEST_MAX_SENDS];
extern int tund_test_tun_write_count;
extern tund_test_tun_write_record_t tund_test_tun_writes[TUND_TEST_MAX_TUN_WRITES];

void tund_test_reset_io(void);
struct sockaddr_in tund_test_addr(uint32_t host_ip, uint16_t port);
void tund_test_build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst);

void tund_test_init_client(client_t *cli);
void tund_test_destroy_client(client_t *cli);
void tund_test_init_server(server_t *srv);
void tund_test_destroy_server(server_t *srv);
void tund_test_set_server_peer(server_t *srv, int idx, uint32_t vip_host, uint32_t real_host,
                               uint16_t port, const char *name);

#endif /* TUND_TEST_SUPPORT_H */
