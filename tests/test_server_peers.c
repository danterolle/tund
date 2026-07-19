#include "../src/core/server/internal.h"
#include "sitest.h"

#include <string.h>

static void test_alloc_ip(void)
{
    server_t srv = {0};

    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START));

    sitest_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START + 1));

    sitest_set_server_peer(&srv, 1, TUND_IP_START + 2, 0xC0000202, 10002, "beta");
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

    sitest_set_server_peer(&srv, 3, TUND_IP_START + 3, 0xC0000203, 12003, "gamma");
    srv.peers[4].virt_ip = htonl(TUND_IP_START + 4);
    srv.peers[4].real_addr = sitest_addr(0xC0000204, 12004);

    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 3)) == 3);
    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 4)) == -1);
    CHECK(server_find_peer_by_vip(&srv, htonl(TUND_IP_START + 5)) == -1);

    addr = sitest_addr(0xC0000203, 12003);
    CHECK(server_find_peer_by_addr(&srv, &addr) == 3);
    addr = sitest_addr(0xC0000203, 12004);
    CHECK(server_find_peer_by_addr(&srv, &addr) == -1);
    addr = sitest_addr(0xC0000204, 12004);
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

    sitest_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    sitest_set_server_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "beta");
    sitest_set_server_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "gamma");
    srv.peers[0].bytes_out = 10;
    srv.peers[1].bytes_out = 20;
    srv.peers[2].bytes_out = 30;

    memset(buf, 0xAB, sizeof(buf));
    sitest_reset_io();
    server_broadcast(&srv, buf, 24, 1, 100);

    CHECK(sitest_send_count == 2);
    CHECK(sitest_sends[1].len == 24);
    CHECK(sitest_sends[1].dest.sin_addr.s_addr == srv.peers[2].real_addr.sin_addr.s_addr);
    CHECK(sitest_sends[1].dest.sin_port == srv.peers[2].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_out == 110);
    CHECK(srv.peers[1].bytes_out == 20);
    CHECK(srv.peers[2].bytes_out == 130);
}

static void test_peer_list_payload(void)
{
    server_t srv = {0};
    uint8_t type = 0;
    uint16_t payload_len = 0;

    sitest_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "target");
    sitest_set_server_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "alpha");
    sitest_set_server_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "beta");

    sitest_reset_io();
    server_send_peer_list(&srv, 0);

    CHECK(sitest_send_count == 1);
    CHECK(proto_read_hdr(sitest_sends[0].buf, &type, &payload_len) == 0);
    CHECK(type == MSG_PEER_LIST);
    CHECK(payload_len == 2 * sizeof(msg_peer_entry_t));
    CHECK(sitest_sends[0].len == TUND_HDR_SIZE + (int)payload_len);

    const msg_peer_entry_t *entries =
        (const msg_peer_entry_t *)(sitest_sends[0].buf + TUND_HDR_SIZE);
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

    return sitest_finish("server peer tests");
}
