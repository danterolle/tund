#include "../src/core/client/internal.h"
#include "sitest.h"

#include <string.h>

static void build_peer_list(uint8_t *buf)
{
    msg_peer_entry_t entries[2];
    memset(entries, 0, sizeof(entries));
    entries[0].virt_ip = htonl(TUND_IP_START + 1);
    snprintf(entries[0].name, sizeof(entries[0].name), "alpha");
    entries[0].status = 1;
    entries[1].virt_ip = htonl(TUND_IP_START + 2);
    snprintf(entries[1].name, sizeof(entries[1].name), "beta");
    entries[1].status = 1;

    proto_write_hdr(buf, MSG_PEER_LIST, sizeof(entries));
    memcpy(buf + TUND_HDR_SIZE, entries, sizeof(entries));
}

static void test_peer_list_updates_table(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    sitest_init_client(&cli);
    build_peer_list(buf);
    client_handle_server_packet(&cli, buf,
                                TUND_HDR_SIZE + 2 * (int)sizeof(msg_peer_entry_t));

    CHECK(cli.peer_count == 2);
    CHECK(cli.peers[0].active);
    CHECK(cli.peers[0].virt_ip == htonl(TUND_IP_START + 1));
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);
    CHECK(cli.peers[1].active);
    CHECK(cli.peers[1].virt_ip == htonl(TUND_IP_START + 2));
    CHECK(strcmp(cli.peers[1].name, "beta") == 0);
    sitest_destroy_client(&cli);
}

static void test_peer_join_and_leave(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint32_t peer_ip = htonl(TUND_IP_START + 1);

    sitest_init_client(&cli);
    int len = proto_build_peer_join(buf, peer_ip, "alpha");
    client_handle_server_packet(&cli, buf, len);
    CHECK(cli.peer_count == 1);
    CHECK(cli.peers[0].active);
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);

    len = proto_build_peer_leave(buf, peer_ip);
    client_handle_server_packet(&cli, buf, len);
    CHECK(cli.peer_count == 0);
    CHECK(!cli.peers[0].active);
    sitest_destroy_client(&cli);
}

static void test_data_writes_tun_and_accounts_peer(void)
{
    client_t cli = {0};
    uint8_t ip_pkt[20];
    uint8_t buf[TUND_MAX_PKT];
    uint32_t peer_ip = htonl(TUND_IP_START + 1);

    sitest_init_client(&cli);
    client_update_peer(&cli, peer_ip, "alpha", true);
    sitest_build_ipv4_packet(ip_pkt, peer_ip, cli.virt_ip);
    int len = proto_build_data(buf, ip_pkt, sizeof(ip_pkt));
    sitest_reset_io();

    client_handle_server_packet(&cli, buf, len);

    CHECK(sitest_tun_write_count == 1);
    CHECK(sitest_tun_writes[0].len == (int)sizeof(ip_pkt));
    CHECK(memcmp(sitest_tun_writes[0].buf, ip_pkt, sizeof(ip_pkt)) == 0);
    CHECK(cli.peers[0].bytes_in == sizeof(ip_pkt));
    CHECK(cli.peers[0].bytes_out == 0);
    sitest_destroy_client(&cli);
}

static void test_keepalive_replies_with_ack(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint8_t type = 0;
    uint16_t payload_len = 0;
    uint64_t timestamp = 0;

    sitest_init_client(&cli);
    int len = proto_build_keepalive(buf, 0x0102030405060708ULL);
    sitest_reset_io();

    client_handle_server_packet(&cli, buf, len);

    CHECK(sitest_send_count == 1);
    CHECK(sitest_sends[0].dest.sin_addr.s_addr == cli.server_addr.sin_addr.s_addr);
    CHECK(sitest_sends[0].dest.sin_port == cli.server_addr.sin_port);
    CHECK(proto_read_hdr(sitest_sends[0].buf, &type, &payload_len) == 0);
    CHECK(type == MSG_KEEPALIVE_ACK);
    CHECK(payload_len == 8);
    CHECK(proto_read_keepalive_timestamp(sitest_sends[0].buf + TUND_HDR_SIZE,
                                         payload_len, &timestamp));
    CHECK(timestamp == 0x0102030405060708ULL);
    CHECK(sitest_sends[0].len == TUND_HDR_SIZE + 8);
    sitest_destroy_client(&cli);
}

static void test_keepalive_ack_updates_rtt(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];
    uint64_t sent_at = now_ms() - 10;

    sitest_init_client(&cli);
    int len = proto_build_keepalive_ack(buf, sent_at);
    client_handle_server_packet(&cli, buf, len);

    CHECK(cli.has_server_rtt);
    CHECK(cli.server_rtt_ms <= 1000);
    sitest_destroy_client(&cli);
}

static void test_disconnect_requests_stop(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    sitest_init_client(&cli);
    tund_stop_flag_store(&g_running, true);
    int len = proto_build_disconnect(buf);

    client_handle_server_packet(&cli, buf, len);

    CHECK(!tund_is_running());
    sitest_destroy_client(&cli);
}

static void test_truncated_packet_is_ignored(void)
{
    client_t cli = {0};
    uint8_t buf[TUND_MAX_PKT];

    sitest_init_client(&cli);
    build_peer_list(buf);
    sitest_reset_io();
    client_handle_server_packet(&cli, buf, TUND_HDR_SIZE + 1);

    CHECK(cli.peer_count == 0);
    CHECK(sitest_tun_write_count == 0);
    CHECK(sitest_send_count == 0);
    sitest_destroy_client(&cli);
}

int main(void)
{
    test_peer_list_updates_table();
    test_peer_join_and_leave();
    test_data_writes_tun_and_accounts_peer();
    test_keepalive_replies_with_ack();
    test_keepalive_ack_updates_rtt();
    test_disconnect_requests_stop();
    test_truncated_packet_is_ignored();

    return sitest_finish("client handler tests");
}
