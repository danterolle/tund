#include "../../src/core/server/internal.h"
#include "test_support.h"

#include <string.h>

static void test_alloc_ip(void) {
    server_t srv = {0};

    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START));

    test_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START + 1));

    test_set_server_peer(&srv, 1, TUND_IP_START + 2, 0xC0000202, 10002, "beta");
    CHECK(server_alloc_ip(&srv) == htonl(TUND_IP_START + 1));

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        srv.peers[i].active = true;
        srv.peers[i].virt_ip = htonl(TUND_IP_START + (uint32_t)i);
    }
    CHECK(server_alloc_ip(&srv) == 0);
}

static void test_find_helpers(void) {
    server_t srv = {0};
    struct sockaddr_in addr;

    test_set_server_peer(&srv, 3, TUND_IP_START + 3, 0xC0000203, 12003, "gamma");
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
    for (int i = 0; i < TUND_MAX_PEERS; i++) srv.peers[i].active = true;
    CHECK(server_find_free_slot(&srv) == -1);
}

static void test_broadcast_updates_recipients(void) {
    server_t srv = {0};
    uint8_t buf[TUND_MAX_PKT];

    test_init_server(&srv);
    test_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "alpha");
    test_set_server_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "beta");
    test_set_server_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "gamma");
    srv.peers[0].bytes_out = 10;
    srv.peers[1].bytes_out = 20;
    srv.peers[2].bytes_out = 30;

    memset(buf, 0xAB, sizeof(buf));
    test_reset_io();
    server_broadcast(&srv, buf, 24, 1, 100);

    CHECK(test_send_count == 2);
    CHECK(test_sends[1].len == 24);
    CHECK(test_sends[1].dest.sin_addr.s_addr == srv.peers[2].real_addr.sin_addr.s_addr);
    CHECK(test_sends[1].dest.sin_port == srv.peers[2].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_out == 110);
    CHECK(srv.peers[1].bytes_out == 20);
    CHECK(srv.peers[2].bytes_out == 130);
    test_destroy_server(&srv);
}

static void test_peer_list_payload(void) {
    server_t srv = {0};
    uint8_t type = 0;
    uint16_t payload_len = 0;

    test_init_server(&srv);
    test_set_server_peer(&srv, 0, TUND_IP_START, 0xC0000201, 10001, "target");
    test_set_server_peer(&srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "alpha");
    test_set_server_peer(&srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "beta");

    test_reset_io();
    server_send_peer_list(&srv, 0);

    CHECK(test_send_count == 1);
    CHECK(proto_read_hdr(test_sends[0].buf, &type, &payload_len) == 0);
    CHECK(type == MSG_PEER_LIST);
    CHECK(payload_len == 2 * TUND_PEER_ENTRY_SIZE);
    CHECK(test_sends[0].len == TUND_HDR_SIZE + (int)payload_len);

    uint32_t entry_ip = 0;
    char entry_name[TUND_NAME_LEN];
    bool entry_online = false;
    CHECK(proto_read_peer_entry(test_sends[0].buf + TUND_HDR_SIZE, payload_len, 0, &entry_ip,
                                entry_name, &entry_online));
    CHECK(entry_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(entry_name, "alpha") == 0);
    CHECK(entry_online);
    CHECK(proto_read_peer_entry(test_sends[0].buf + TUND_HDR_SIZE, payload_len, 1, &entry_ip,
                                entry_name, &entry_online));
    CHECK(entry_ip == htonl(TUND_IP_START + 2));
    CHECK(strcmp(entry_name, "beta") == 0);
    CHECK(entry_online);
    test_destroy_server(&srv);
}

int main(void) {
    test_alloc_ip();
    test_find_helpers();
    test_broadcast_updates_recipients();
    test_peer_list_payload();

    return sitest_finish("server peer tests");
}
