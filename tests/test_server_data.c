#include "../src/core/server/internal.h"
#include "tund_test_support.h"

#include <string.h>

static void setup_server(server_t *srv) {
    memset(srv->peers, 0, sizeof(srv->peers));
    srv->peer_count = 0;
    tund_test_set_server_peer(srv, 0, TUND_IP_START, 0xC0000201, 10001, "sender");
    tund_test_set_server_peer(srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "target");
    tund_test_set_server_peer(srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "other");
}

static void check_forwarded_data_message(int send_idx, const uint8_t *ip_pkt) {
    uint8_t type = 0;
    uint16_t payload_len = 0;

    CHECK(proto_read_hdr(tund_test_sends[send_idx].buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == 20);
    CHECK(tund_test_sends[send_idx].len == TUND_HDR_SIZE + 20);
    CHECK(memcmp(tund_test_sends[send_idx].buf + TUND_HDR_SIZE, ip_pkt, 20) == 0);
}

static void test_unicast_forwarding(void) {
    server_t srv = {0};
    uint8_t pkt[20];

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, srv.peers[0].virt_ip, srv.peers[1].virt_ip);
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(tund_test_send_count == 1);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(tund_test_sends[0].dest.sin_addr.s_addr == srv.peers[1].real_addr.sin_addr.s_addr);
    CHECK(tund_test_sends[0].dest.sin_port == srv.peers[1].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == 0);
    check_forwarded_data_message(0, pkt);
    tund_test_destroy_server(&srv);
}

static void test_server_destination_writes_tun(void) {
    server_t srv = {0};
    uint8_t pkt[20];

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_SERVER_IP));
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 1);
    CHECK(tund_test_tun_writes[0].len == (int)sizeof(pkt));
    CHECK(memcmp(tund_test_tun_writes[0].buf, pkt, sizeof(pkt)) == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    tund_test_destroy_server(&srv);
}

static void test_broadcast_forwarding(void) {
    server_t srv = {0};
    uint8_t pkt[20];
    uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, srv.peers[0].virt_ip, broadcast_ip);
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(tund_test_send_count == 2);
    CHECK(tund_test_tun_write_count == 1);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[0].bytes_out == 0);
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == sizeof(pkt));
    CHECK(memcmp(tund_test_tun_writes[0].buf, pkt, sizeof(pkt)) == 0);
    check_forwarded_data_message(0, pkt);
    tund_test_destroy_server(&srv);
}

static void test_unknown_destination_drops_after_accounting(void) {
    server_t srv = {0};
    uint8_t pkt[20];

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_IP_START + 42));
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == 0);
    tund_test_destroy_server(&srv);
}

static void test_spoofed_source_drops_without_accounting(void) {
    server_t srv = {0};
    uint8_t pkt[20];

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, srv.peers[1].virt_ip, srv.peers[2].virt_ip);
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);
    CHECK(srv.peers[1].bytes_out == 0);
    CHECK(srv.peers[2].bytes_out == 0);
    tund_test_destroy_server(&srv);
}

static void test_unregistered_and_short_packets_drop(void) {
    server_t srv = {0};
    uint8_t pkt[20];
    struct sockaddr_in stranger = tund_test_addr(0xC00002FE, 20000);

    tund_test_init_server(&srv);
    setup_server(&srv);
    tund_test_build_ipv4_packet(pkt, htonl(TUND_IP_START), htonl(TUND_IP_START + 1));
    tund_test_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &stranger);
    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);

    server_handle_data(&srv, pkt, 19, &srv.peers[0].real_addr);
    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);

    pkt[0] = 0x65;
    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);
    CHECK(tund_test_send_count == 0);
    CHECK(tund_test_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);
    tund_test_destroy_server(&srv);
}

int main(void) {
    test_unicast_forwarding();
    test_server_destination_writes_tun();
    test_broadcast_forwarding();
    test_unknown_destination_drops_after_accounting();
    test_spoofed_source_drops_without_accounting();
    test_unregistered_and_short_packets_drop();

    return sitest_finish("server data tests");
}
