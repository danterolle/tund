#include "../src/core/server/internal.h"
#include "test_support.h"

#include <string.h>

static void register_peer(server_t *srv, const struct sockaddr_in *addr,
                          const char *name)
{
    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_register(buf, name);
    server_handle_packet(srv, buf, len, addr);
}

static void test_register_assigns_new_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);

    test_init_server(&srv);
    test_reset_io();
    register_peer(&srv, &addr, "alpha");

    CHECK(srv.peer_count == 1);
    CHECK(srv.peers[0].active);
    CHECK(srv.peers[0].virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(srv.peers[0].name, "alpha") == 0);
    CHECK(test_send_count == 2);
    CHECK(test_sends[0].type == MSG_ASSIGN);
    CHECK(test_sends[0].payload_len == 10);
    CHECK(test_sends[1].type == MSG_PEER_LIST);
    CHECK(test_sends[1].payload_len == 0);
    test_destroy_server(&srv);
}

static void test_register_reconnect_refreshes_existing_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    time_t first_seen;

    test_init_server(&srv);
    register_peer(&srv, &addr, "alpha");
    first_seen = srv.peers[0].last_seen;

    test_reset_io();
    register_peer(&srv, &addr, "ignored");

    CHECK(srv.peer_count == 1);
    CHECK(srv.peers[0].active);
    CHECK(srv.peers[0].virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(srv.peers[0].name, "alpha") == 0);
    CHECK(srv.peers[0].last_seen >= first_seen);
    CHECK(test_send_count == 1);
    CHECK(test_sends[0].type == MSG_ASSIGN);
    test_destroy_server(&srv);
}

static void test_second_register_sends_peer_list_and_join(void)
{
    server_t srv = {0};
    struct sockaddr_in first = test_addr(0xC0000201, 10001);
    struct sockaddr_in second = test_addr(0xC0000202, 10002);

    test_init_server(&srv);
    register_peer(&srv, &first, "alpha");

    test_reset_io();
    register_peer(&srv, &second, "beta");

    CHECK(srv.peer_count == 2);
    CHECK(srv.peers[1].active);
    CHECK(srv.peers[1].virt_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(srv.peers[1].name, "beta") == 0);
    CHECK(test_send_count == 3);
    CHECK(test_sends[0].type == MSG_ASSIGN);
    CHECK(test_sends[1].type == MSG_PEER_LIST);
    CHECK(test_sends[1].payload_len == sizeof(msg_peer_entry_t));
    const msg_peer_entry_t *entry =
        (const msg_peer_entry_t *)(test_sends[1].buf + TUND_HDR_SIZE);
    CHECK(entry->virt_ip == htonl(TUND_IP_START));
    CHECK(strcmp(entry->name, "alpha") == 0);
    CHECK(test_sends[2].type == MSG_PEER_JOIN);
    CHECK(test_sends[2].dest.sin_addr.s_addr == first.sin_addr.s_addr);
    CHECK(test_sends[2].dest.sin_port == first.sin_port);
    test_destroy_server(&srv);
}

static void test_disconnect_marks_peer_inactive_and_broadcasts_leave(void)
{
    server_t srv = {0};
    struct sockaddr_in first = test_addr(0xC0000201, 10001);
    struct sockaddr_in second = test_addr(0xC0000202, 10002);
    uint8_t buf[TUND_MAX_PKT];

    test_init_server(&srv);
    register_peer(&srv, &first, "alpha");
    register_peer(&srv, &second, "beta");

    test_reset_io();
    int len = proto_build_disconnect(buf);
    server_handle_packet(&srv, buf, len, &first);

    CHECK(srv.peer_count == 1);
    CHECK(!srv.peers[0].active);
    CHECK(srv.peers[1].active);
    CHECK(test_send_count == 1);
    CHECK(test_sends[0].type == MSG_PEER_LEAVE);
    CHECK(test_sends[0].dest.sin_addr.s_addr == second.sin_addr.s_addr);
    CHECK(test_sends[0].dest.sin_port == second.sin_port);
    test_destroy_server(&srv);
}

static void test_keepalive_replies_with_ack(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];
    uint64_t timestamp = 0;

    test_init_server(&srv);
    register_peer(&srv, &addr, "alpha");

    test_reset_io();
    int len = proto_build_keepalive(buf, 0x0102030405060708ULL);
    server_handle_packet(&srv, buf, len, &addr);

    CHECK(test_send_count == 1);
    CHECK(test_sends[0].type == MSG_KEEPALIVE_ACK);
    CHECK(proto_read_keepalive_timestamp(test_sends[0].buf + TUND_HDR_SIZE,
                                         test_sends[0].payload_len, &timestamp));
    CHECK(timestamp == 0x0102030405060708ULL);
    CHECK(test_sends[0].dest.sin_addr.s_addr == addr.sin_addr.s_addr);
    CHECK(test_sends[0].dest.sin_port == addr.sin_port);
    test_destroy_server(&srv);
}

static void test_keepalive_ack_updates_rtt(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];

    test_init_server(&srv);
    register_peer(&srv, &addr, "alpha");
    int len = proto_build_keepalive_ack(buf, now_ms() - 10);
    server_handle_packet(&srv, buf, len, &addr);

    CHECK(srv.peers[0].has_rtt);
    CHECK(srv.peers[0].rtt_ms <= 1000);
    test_destroy_server(&srv);
}

static void test_invalid_packets_do_not_register_peer(void)
{
    server_t srv = {0};
    struct sockaddr_in addr = test_addr(0xC0000201, 10001);
    uint8_t buf[TUND_MAX_PKT];

    test_init_server(&srv);
    int len = proto_build_register(buf, "alpha");
    buf[0] = 0;
    server_handle_packet(&srv, buf, len, &addr);
    CHECK(srv.peer_count == 0);

    len = proto_build_register(buf, "alpha");
    server_handle_packet(&srv, buf, TUND_HDR_SIZE - 1, &addr);
    CHECK(srv.peer_count == 0);
    test_destroy_server(&srv);
}

int main(void)
{
    test_register_assigns_new_peer();
    test_register_reconnect_refreshes_existing_peer();
    test_second_register_sends_peer_list_and_join();
    test_disconnect_marks_peer_inactive_and_broadcasts_leave();
    test_keepalive_replies_with_ack();
    test_keepalive_ack_updates_rtt();
    test_invalid_packets_do_not_register_peer();

    return test_finish("server handler tests");
}
