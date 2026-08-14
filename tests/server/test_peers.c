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

static void test_full_peer_list_is_paginated(void) {
    server_t srv = {0};
    int expected_entries = TUND_MAX_PEERS - 1;
    int received_entries = 0;

    test_init_server(&srv);
    for (int i = 0; i < TUND_MAX_PEERS; i++)
        test_set_server_peer(&srv, i, TUND_IP_START + (uint32_t)i, 0xC0000201 + (uint32_t)i,
                             (uint16_t)(10001 + i), "peer");

    test_reset_io();
    server_send_peer_list(&srv, 0);

    CHECK(test_send_count == proto_peer_list_page_count((uint16_t)expected_entries));
    for (int i = 0; i < test_send_count; i++) {
        proto_peer_list_page_t page;
        CHECK(proto_read_peer_list_page(test_sends[i].buf + TUND_HDR_SIZE,
                                        test_sends[i].payload_len, &page));
        CHECK(page.page_index == i);
        CHECK(page.total_entries == expected_entries);
        CHECK(test_sends[i].dest.sin_port == srv.peers[0].real_addr.sin_port);

        uint32_t entry_ip = 0;
        char entry_name[TUND_NAME_LEN];
        bool entry_online = false;
        CHECK(proto_read_peer_entry(page.entries, page.entry_count * TUND_PEER_ENTRY_SIZE, 0,
                                    &entry_ip, entry_name, &entry_online));
        CHECK(entry_ip == htonl(TUND_IP_START + 1 + (uint32_t)received_entries));
        CHECK(entry_online);
        received_entries += page.entry_count;
    }
    CHECK(received_entries == expected_entries);
    test_destroy_server(&srv);
}

int main(void) {
    test_alloc_ip();
    test_find_helpers();
    test_broadcast_updates_recipients();
    test_full_peer_list_is_paginated();

    return sitest_finish("server peer tests");
}
