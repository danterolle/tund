#ifndef TUND_SITEST_H
#define TUND_SITEST_H

#include "../src/core/client/client.h"
#include "../src/core/server/server.h"
#include "network.h"
#include "tun.h"

#include <stddef.h>

#define SITEST_MAX_SENDS 8
#define SITEST_MAX_TUN_WRITES 8

typedef struct {
    int len;
    uint8_t type;
    uint16_t payload_len;
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in dest;
} sitest_send_record_t;

typedef struct {
    int len;
    uint8_t buf[TUND_MAX_PAYLOAD];
} sitest_tun_write_record_t;

extern int sitest_failures;
extern int sitest_send_count;
extern sitest_send_record_t sitest_sends[SITEST_MAX_SENDS];
extern int sitest_tun_write_count;
extern sitest_tun_write_record_t sitest_tun_writes[SITEST_MAX_TUN_WRITES];

#define CHECK(expr) sitest_check((expr), __FILE__, __LINE__, #expr)

void sitest_check(bool ok, const char *file, int line, const char *expr);
int sitest_finish(const char *suite_name);

void sitest_reset_io(void);
struct sockaddr_in sitest_addr(uint32_t host_ip, uint16_t port);
void sitest_build_ipv4_packet(uint8_t *pkt, uint32_t src, uint32_t dst);

void sitest_init_client(client_t *cli);
void sitest_destroy_client(client_t *cli);
void sitest_init_server(server_t *srv);
void sitest_destroy_server(server_t *srv);
void sitest_set_server_peer(server_t *srv, int idx, uint32_t vip_host,
                           uint32_t real_host, uint16_t port,
                           const char *name);

#endif /* TUND_SITEST_H */
