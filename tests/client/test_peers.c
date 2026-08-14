#include "../../src/core/client/internal.h"
#include "test_support.h"

#include <string.h>

static void test_update_peer_lifecycle(void) {
    client_t cli = {0};
    uint32_t vip = htonl(TUND_IP_START);

    test_init_client(&cli);
    client_update_peer(&cli, vip, "alpha", true);
    CHECK(cli.peer_count == 1);
    CHECK(cli.peers[0].active);
    CHECK(cli.peers[0].virt_ip == vip);
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);
    CHECK(cli.peers[0].last_seen != 0);

    cli.peers[0].bytes_in = 10;
    cli.peers[0].bytes_out = 20;
    client_update_peer(&cli, vip, "beta", true);
    CHECK(cli.peer_count == 1);
    CHECK(strcmp(cli.peers[0].name, "beta") == 0);
    CHECK(cli.peers[0].bytes_in == 10);
    CHECK(cli.peers[0].bytes_out == 20);

    client_update_peer(&cli, vip, "ignored", false);
    CHECK(cli.peer_count == 0);
    CHECK(!cli.peers[0].active);

    client_update_peer(&cli, htonl(TUND_IP_START + 1), "missing", false);
    CHECK(cli.peer_count == 0);
    test_destroy_client(&cli);
}

static void test_update_peer_truncates_names(void) {
    client_t cli = {0};
    char long_name[64];

    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    test_init_client(&cli);
    client_update_peer(&cli, htonl(TUND_IP_START), long_name, true);
    CHECK(cli.peer_count == 1);
    CHECK(strlen(cli.peers[0].name) == TUND_NAME_LEN - 1);
    CHECK(cli.peers[0].name[TUND_NAME_LEN - 1] == '\0');
    test_destroy_client(&cli);
}

static void test_update_peer_table_full(void) {
    client_t cli = {0};

    test_init_client(&cli);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        cli.peers[i].active = true;
        cli.peers[i].virt_ip = htonl(TUND_IP_START + (uint32_t)i);
    }
    cli.peer_count = TUND_MAX_PEERS;

    client_update_peer(&cli, htonl(TUND_IP_END + 1), "overflow", true);
    CHECK(cli.peer_count == TUND_MAX_PEERS);
    for (int i = 0; i < TUND_MAX_PEERS; i++)
        CHECK(cli.peers[i].virt_ip == htonl(TUND_IP_START + (uint32_t)i));
    test_destroy_client(&cli);
}

static void test_traffic_accounting(void) {
    client_t cli = {0};
    uint32_t first = htonl(TUND_IP_START);
    uint32_t second = htonl(TUND_IP_START + 1);

    test_init_client(&cli);
    client_update_peer(&cli, first, "alpha", true);
    client_update_peer(&cli, second, "beta", true);

    client_add_peer_traffic(&cli, first, 12, 34);
    CHECK(cli.peers[0].bytes_in == 12);
    CHECK(cli.peers[0].bytes_out == 34);
    CHECK(cli.peers[1].bytes_in == 0);
    CHECK(cli.peers[1].bytes_out == 0);

    client_add_peer_traffic(&cli, htonl(TUND_IP_START + 10), 50, 60);
    CHECK(cli.peers[0].bytes_in == 12);
    CHECK(cli.peers[0].bytes_out == 34);

    client_add_broadcast_traffic(&cli, 100);
    CHECK(cli.peers[0].bytes_out == 134);
    CHECK(cli.peers[1].bytes_out == 100);

    cli.peers[1].active = false;
    client_add_broadcast_traffic(&cli, 100);
    CHECK(cli.peers[0].bytes_out == 234);
    CHECK(cli.peers[1].bytes_out == 100);
    test_destroy_client(&cli);
}

static void deliver_peer_list_page(client_t *cli, uint16_t page_index, uint16_t total_entries) {
    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_peer_list_page(buf, 1, page_index, total_entries);
    CHECK(len > 0);

    uint16_t first_entry = (uint16_t)(page_index * TUND_PEER_LIST_PAGE_ENTRIES);
    uint16_t entry_count = proto_peer_list_page_entry_count(total_entries, page_index);
    uint8_t *entries = buf + TUND_HDR_SIZE + TUND_PEER_LIST_PAGE_HEADER_SIZE;
    for (uint16_t i = 0; i < entry_count; i++)
        CHECK(proto_write_peer_entry(entries + i * TUND_PEER_ENTRY_SIZE, TUND_PEER_ENTRY_SIZE,
                                     htonl(TUND_IP_START + first_entry + i), "peer", true));

    client_handle_peer_list_page(cli, buf + TUND_HDR_SIZE, (uint16_t)(len - TUND_HDR_SIZE));
}

static void test_maximum_peer_list(void) {
    client_t cli = {0};
    uint16_t total_entries = TUND_MAX_PEERS - 1;
    uint16_t page_count = proto_peer_list_page_count(total_entries);

    test_init_client(&cli);
    for (int page_index = page_count - 1; page_index >= 0; page_index--)
        deliver_peer_list_page(&cli, (uint16_t)page_index, total_entries);

    CHECK(cli.peer_count == total_entries);
    CHECK(cli.peer_list.received_entries == total_entries);
    CHECK(cli.peer_list.received_pages == (1ULL << page_count) - 1);
    test_destroy_client(&cli);
}

static void test_incomplete_peer_list_requests_retry(void) {
    client_t cli = {0};
    uint64_t now = 1000;

    test_init_client(&cli);
    test_reset_io();
    client_request_peer_list_if_due(&cli, now);

    CHECK(test_send_count == 1);
    CHECK(test_sends[0].type == MSG_PEER_LIST_REQUEST);
    CHECK(test_sends[0].payload_len == 0);

    client_request_peer_list_if_due(&cli, now + 1);
    CHECK(test_send_count == 1);
    cli.peer_list.complete = true;
    client_request_peer_list_if_due(&cli, now + TUND_PEER_LIST_RETRY_INTERVAL * 1000);
    CHECK(test_send_count == 1);
    test_destroy_client(&cli);
}

int main(void) {
    test_update_peer_lifecycle();
    test_update_peer_truncates_names();
    test_update_peer_table_full();
    test_traffic_accounting();
    test_maximum_peer_list();
    test_incomplete_peer_list_requests_retry();

    return sitest_finish("client peer tests");
}
