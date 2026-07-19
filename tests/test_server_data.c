#include "../src/core/server/internal.h"
#include "sitest.h"

#include <string.h>

static void setup_server(server_t *srv)
{
    memset(srv->peers, 0, sizeof(srv->peers));
    srv->peer_count = 0;
    sitest_set_server_peer(srv, 0, TUND_IP_START, 0xC0000201, 10001, "sender");
    sitest_set_server_peer(srv, 1, TUND_IP_START + 1, 0xC0000202, 10002, "target");
    sitest_set_server_peer(srv, 2, TUND_IP_START + 2, 0xC0000203, 10003, "other");
}

static void check_forwarded_data_message(int send_idx, const uint8_t *ip_pkt)
{
    uint8_t type = 0;
    uint16_t payload_len = 0;

    CHECK(proto_read_hdr(sitest_sends[send_idx].buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == 20);
    CHECK(sitest_sends[send_idx].len == TUND_HDR_SIZE + 20);
    CHECK(memcmp(sitest_sends[send_idx].buf + TUND_HDR_SIZE, ip_pkt, 20) == 0);
}

static void test_unicast_forwarding(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    sitest_init_server(&srv);
    setup_server(&srv);
    sitest_build_ipv4_packet(pkt, srv.peers[0].virt_ip, srv.peers[1].virt_ip);
    sitest_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(sitest_send_count == 1);
    CHECK(sitest_tun_write_count == 0);
    CHECK(sitest_sends[0].dest.sin_addr.s_addr == srv.peers[1].real_addr.sin_addr.s_addr);
    CHECK(sitest_sends[0].dest.sin_port == srv.peers[1].real_addr.sin_port);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == 0);
    check_forwarded_data_message(0, pkt);
    sitest_destroy_server(&srv);
}

static void test_server_destination_writes_tun(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    sitest_init_server(&srv);
    setup_server(&srv);
    sitest_build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_SERVER_IP));
    sitest_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(sitest_send_count == 0);
    CHECK(sitest_tun_write_count == 1);
    CHECK(sitest_tun_writes[0].len == (int)sizeof(pkt));
    CHECK(memcmp(sitest_tun_writes[0].buf, pkt, sizeof(pkt)) == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    sitest_destroy_server(&srv);
}

static void test_broadcast_forwarding(void)
{
    server_t srv = {0};
    uint8_t pkt[20];
    uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);

    sitest_init_server(&srv);
    setup_server(&srv);
    sitest_build_ipv4_packet(pkt, srv.peers[0].virt_ip, broadcast_ip);
    sitest_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(sitest_send_count == 2);
    CHECK(sitest_tun_write_count == 1);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[0].bytes_out == 0);
    CHECK(srv.peers[1].bytes_out == sizeof(pkt));
    CHECK(srv.peers[2].bytes_out == sizeof(pkt));
    CHECK(memcmp(sitest_tun_writes[0].buf, pkt, sizeof(pkt)) == 0);
    check_forwarded_data_message(0, pkt);
    sitest_destroy_server(&srv);
}

static void test_unknown_destination_drops_after_accounting(void)
{
    server_t srv = {0};
    uint8_t pkt[20];

    sitest_init_server(&srv);
    setup_server(&srv);
    sitest_build_ipv4_packet(pkt, srv.peers[0].virt_ip, htonl(TUND_IP_START + 42));
    sitest_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &srv.peers[0].real_addr);

    CHECK(sitest_send_count == 0);
    CHECK(sitest_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == sizeof(pkt));
    CHECK(srv.peers[1].bytes_out == 0);
    sitest_destroy_server(&srv);
}

static void test_unregistered_and_short_packets_drop(void)
{
    server_t srv = {0};
    uint8_t pkt[20];
    struct sockaddr_in stranger = sitest_addr(0xC00002FE, 20000);

    sitest_init_server(&srv);
    setup_server(&srv);
    sitest_build_ipv4_packet(pkt, htonl(TUND_IP_START), htonl(TUND_IP_START + 1));
    sitest_reset_io();

    server_handle_data(&srv, pkt, sizeof(pkt), &stranger);
    CHECK(sitest_send_count == 0);
    CHECK(sitest_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);

    server_handle_data(&srv, pkt, 19, &srv.peers[0].real_addr);
    CHECK(sitest_send_count == 0);
    CHECK(sitest_tun_write_count == 0);
    CHECK(srv.peers[0].bytes_in == 0);
    sitest_destroy_server(&srv);
}

int main(void)
{
    test_unicast_forwarding();
    test_server_destination_writes_tun();
    test_broadcast_forwarding();
    test_unknown_destination_drops_after_accounting();
    test_unregistered_and_short_packets_drop();

    return sitest_finish("server data tests");
}
