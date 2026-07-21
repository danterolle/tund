#include "sitest.h"
#include "protocol.h"

#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

static void test_siphash_vector(void) {
    uint8_t empty[1] = {0};
    uint64_t tag = proto_siphash24(empty, 0, 0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL);

    CHECK(tag == 0x726fdb47dd0e0e31ULL);
}

static void test_header_roundtrip(void) {
    uint8_t buf[TUND_MAX_PKT];
    uint8_t type = 0;
    uint16_t payload_len = 0;

    memset(buf, 0xCC, sizeof(buf));
    CHECK(proto_write_hdr(buf, MSG_DATA, 0x0123) == TUND_HDR_SIZE);
    CHECK(buf[0] == TUND_MAGIC);
    CHECK(buf[1] == TUND_PROTOCOL_VERSION);
    CHECK(buf[2] == MSG_DATA);
    CHECK(buf[3] == 0x01);
    CHECK(buf[4] == 0x23);
    uint64_t sequence = 0;
    CHECK(proto_read_sequence(buf, &sequence));
    CHECK(sequence > 0);

    bool nonce_has_data = false;
    for (int i = 0; i < TUND_NONCE_SIZE; i++)
        nonce_has_data = nonce_has_data || buf[TUND_NONCE_OFFSET + i] != 0;
    CHECK(nonce_has_data);

    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == 0x0123);

    buf[0] = 0;
    CHECK(proto_read_hdr(buf, &type, &payload_len) == TUND_HDR_BAD_MAGIC);

    proto_write_hdr(buf, MSG_DATA, 0);
    buf[1] = TUND_PROTOCOL_VERSION + 1;
    CHECK(proto_read_hdr(buf, &type, &payload_len) == TUND_HDR_BAD_VERSION);
}

static void test_transport_encryption(void) {
    uint8_t ip_pkt[20];
    uint8_t buf[TUND_MAX_PKT];
    uint8_t original_payload[20];
    uint8_t key[TUND_KEY_SIZE], other_key[TUND_KEY_SIZE];

    for (size_t i = 0; i < sizeof(ip_pkt); i++) ip_pkt[i] = (uint8_t)i;
    ip_pkt[0] = 0x45;
    memcpy(original_payload, ip_pkt, sizeof(original_payload));

    proto_key_from_passphrase("a-long-random-key", key);
    proto_key_from_passphrase("another-random-key", other_key);

    int len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    int wire_len = proto_encrypt(buf, len, key);
    CHECK(wire_len == len + TUND_AUTH_TAG_SIZE);
    CHECK(memcmp(buf + TUND_HDR_SIZE, original_payload, sizeof(original_payload)) != 0);
    CHECK(proto_decrypt(buf, wire_len, other_key) < 0);
    CHECK(proto_decrypt(buf, wire_len, key) == len);
    CHECK(memcmp(buf + TUND_HDR_SIZE, original_payload, sizeof(original_payload)) == 0);

    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    wire_len = proto_encrypt(buf, len, key);
    buf[1] ^= 0x01;
    CHECK(proto_decrypt(buf, wire_len, key) < 0);

    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    wire_len = proto_encrypt(buf, len, key);
    buf[TUND_SEQUENCE_OFFSET + 7] ^= 0x01;
    CHECK(proto_decrypt(buf, wire_len, key) < 0);

    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    wire_len = proto_encrypt(buf, len, key);
    buf[TUND_HDR_SIZE + 5] ^= 0xFF;
    CHECK(proto_decrypt(buf, wire_len, key) < 0);

    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    wire_len = proto_encrypt(buf, len, key);
    buf[wire_len - 1] ^= 0xFF;
    CHECK(proto_decrypt(buf, wire_len, key) < 0);
}

static void test_transport_encryption_refreshes_header(void) {
    uint8_t ip_pkt[20];
    uint8_t plaintext[TUND_MAX_PKT];
    uint8_t first[TUND_MAX_PKT], second[TUND_MAX_PKT];
    uint8_t key[TUND_KEY_SIZE];
    uint64_t first_sequence = 0, second_sequence = 0;

    for (size_t i = 0; i < sizeof(ip_pkt); i++) ip_pkt[i] = (uint8_t)i;
    ip_pkt[0] = 0x45;
    proto_key_from_passphrase("a-long-random-key", key);

    int len = proto_build_data(plaintext, ip_pkt, (uint16_t)sizeof(ip_pkt));
    memcpy(first, plaintext, (size_t)len);
    memcpy(second, plaintext, (size_t)len);

    int first_wire_len = proto_encrypt(first, len, key);
    int second_wire_len = proto_encrypt(second, len, key);

    CHECK(first_wire_len == len + TUND_AUTH_TAG_SIZE);
    CHECK(second_wire_len == len + TUND_AUTH_TAG_SIZE);
    CHECK(proto_read_sequence(first, &first_sequence));
    CHECK(proto_read_sequence(second, &second_sequence));
    CHECK(first_sequence != second_sequence);
    CHECK(memcmp(first + TUND_NONCE_OFFSET, second + TUND_NONCE_OFFSET, TUND_NONCE_SIZE) != 0);
    CHECK(memcmp(first + TUND_HDR_SIZE, second + TUND_HDR_SIZE,
                 (size_t)(first_wire_len - TUND_HDR_SIZE)) != 0);

    CHECK(proto_decrypt(first, first_wire_len, key) == len);
    CHECK(proto_decrypt(second, second_wire_len, key) == len);
    CHECK(memcmp(first + TUND_HDR_SIZE, ip_pkt, sizeof(ip_pkt)) == 0);
    CHECK(memcmp(second + TUND_HDR_SIZE, ip_pkt, sizeof(ip_pkt)) == 0);
}

static void test_key_derivation(void) {
    uint8_t key[TUND_KEY_SIZE], same_key[TUND_KEY_SIZE], other_key[TUND_KEY_SIZE];
    proto_key_from_passphrase("a-long-random-key", key);
    proto_key_from_passphrase("a-long-random-key", same_key);
    proto_key_from_passphrase("another-random-key", other_key);

    CHECK(memcmp(key, same_key, sizeof(key)) == 0);
    CHECK(memcmp(key, other_key, sizeof(key)) != 0);

    bool key_has_data = false;
    for (int i = 0; i < TUND_KEY_SIZE; i++) key_has_data = key_has_data || key[i] != 0;
    CHECK(key_has_data);
}

static void test_siphash_tamper_vector(void) {
    uint8_t ip_pkt[20];
    uint8_t buf[TUND_MAX_PKT];
    uint64_t k0 = 0x0706050403020100ULL;
    uint64_t k1 = 0x0f0e0d0c0b0a0908ULL;

    for (size_t i = 0; i < sizeof(ip_pkt); i++) ip_pkt[i] = (uint8_t)i;
    ip_pkt[0] = 0x45;
    int len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    uint64_t tag = proto_siphash24(buf, (size_t)len, k0, k1);
    buf[1] ^= 0x01;
    CHECK(proto_siphash24(buf, (size_t)len, k0, k1) != tag);
}

static void test_builders(void) {
    uint8_t buf[TUND_MAX_PKT];
    uint8_t type = 0;
    uint16_t payload_len = 0;
    char long_name[64];

    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    int len = proto_build_register(buf, long_name);
    CHECK(len == TUND_HDR_SIZE + TUND_NAME_LEN);
    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_REGISTER);
    CHECK(payload_len == TUND_NAME_LEN);
    for (int i = 0; i < TUND_NAME_LEN; i++) CHECK(buf[TUND_HDR_SIZE + i] == 'A');

    uint8_t ip_pkt[20];
    memset(ip_pkt, 0xAB, sizeof(ip_pkt));
    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    CHECK(len == TUND_HDR_SIZE + (int)sizeof(ip_pkt));
    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == sizeof(ip_pkt));
    CHECK(memcmp(buf + TUND_HDR_SIZE, ip_pkt, sizeof(ip_pkt)) == 0);

    uint32_t ip = htonl(TUND_IP_START);
    uint32_t netmask = htonl(TUND_NETMASK);
    len = proto_build_assign(buf, ip, netmask, TUND_TUN_MTU);
    CHECK(len == TUND_HDR_SIZE + 10);
    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_ASSIGN);
    CHECK(payload_len == 10);

    uint32_t got_ip = 0;
    uint32_t got_netmask = 0;
    uint16_t got_mtu = 0;
    memcpy(&got_ip, buf + TUND_HDR_SIZE, sizeof(got_ip));
    memcpy(&got_netmask, buf + TUND_HDR_SIZE + 4, sizeof(got_netmask));
    memcpy(&got_mtu, buf + TUND_HDR_SIZE + 8, sizeof(got_mtu));
    CHECK(got_ip == ip);
    CHECK(got_netmask == netmask);
    CHECK(ntohs(got_mtu) == TUND_TUN_MTU);

    len = proto_build_keepalive(buf, 0x0102030405060708ULL);
    CHECK(len == TUND_HDR_SIZE + 8);
    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_KEEPALIVE);
    CHECK(payload_len == 8);
    const uint8_t expected_ts[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(memcmp(buf + TUND_HDR_SIZE, expected_ts, sizeof(expected_ts)) == 0);

    uint64_t got_ts = 0;
    CHECK(proto_read_keepalive_timestamp(buf + TUND_HDR_SIZE, payload_len, &got_ts));
    CHECK(got_ts == 0x0102030405060708ULL);
    CHECK(!proto_read_keepalive_timestamp(buf + TUND_HDR_SIZE, 7, &got_ts));

    len = proto_build_keepalive_ack(buf, 0x1112131415161718ULL);
    CHECK(len == TUND_HDR_SIZE + 8);
    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_KEEPALIVE_ACK);
    CHECK(payload_len == 8);
    CHECK(proto_read_keepalive_timestamp(buf + TUND_HDR_SIZE, payload_len, &got_ts));
    CHECK(got_ts == 0x1112131415161718ULL);
}

static void test_dst_ip(void) {
    uint8_t ip_pkt[20];
    uint32_t src = htonl(0x0A090002);
    uint32_t dst = htonl(0x0A090003);

    memset(ip_pkt, 0, sizeof(ip_pkt));
    memcpy(ip_pkt + 12, &src, sizeof(src));
    memcpy(ip_pkt + 16, &dst, sizeof(dst));

    CHECK(proto_get_src_ip(ip_pkt, sizeof(ip_pkt)) == src);
    CHECK(proto_get_src_ip(ip_pkt, 19) == 0);
    CHECK(proto_get_dst_ip(ip_pkt, sizeof(ip_pkt)) == dst);
    CHECK(proto_get_dst_ip(ip_pkt, 19) == 0);
}

static void test_ipv4_validation(void) {
    uint8_t ip_pkt[24];
    uint32_t src = htonl(0x0A090002);
    uint32_t dst = htonl(0x0A090003);
    uint16_t total = htons(20);

    memset(ip_pkt, 0, sizeof(ip_pkt));
    ip_pkt[0] = 0x45;
    memcpy(ip_pkt + 2, &total, sizeof(total));
    memcpy(ip_pkt + 12, &src, sizeof(src));
    memcpy(ip_pkt + 16, &dst, sizeof(dst));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_OK);

    ip_pkt[0] = 0x65;
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_BAD_VERSION);
    ip_pkt[0] = 0x44;
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_BAD_IHL);
    ip_pkt[0] = 0x45;

    total = htons(21);
    memcpy(ip_pkt + 2, &total, sizeof(total));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_TRUNCATED);

    total = htons(19);
    memcpy(ip_pkt + 2, &total, sizeof(total));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_LENGTH_MISMATCH);

    total = htons(20);
    memcpy(ip_pkt + 2, &total, sizeof(total));
    uint32_t outside = htonl(0xC0000201);
    memcpy(ip_pkt + 12, &outside, sizeof(outside));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_BAD_SRC);
    memcpy(ip_pkt + 12, &src, sizeof(src));

    memcpy(ip_pkt + 16, &outside, sizeof(outside));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_BAD_DST);

    uint32_t network = htonl(TUND_SUBNET);
    memcpy(ip_pkt + 16, &network, sizeof(network));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_BAD_DST);

    uint32_t broadcast = htonl(TUND_SUBNET | ~TUND_NETMASK);
    memcpy(ip_pkt + 16, &broadcast, sizeof(broadcast));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 20) == TUND_IPV4_OK);

    memset(ip_pkt, 0, sizeof(ip_pkt));
    ip_pkt[0] = 0x46;
    total = htons(24);
    memcpy(ip_pkt + 2, &total, sizeof(total));
    memcpy(ip_pkt + 12, &src, sizeof(src));
    memcpy(ip_pkt + 16, &dst, sizeof(dst));
    CHECK(proto_validate_ipv4_packet(ip_pkt, 24) == TUND_IPV4_OK);
}

static void test_replay_window(void) {
    proto_replay_window_t replay;
    proto_replay_reset(&replay);

    CHECK(proto_replay_accept(&replay, 100));
    CHECK(!proto_replay_accept(&replay, 100));
    CHECK(proto_replay_accept(&replay, 99));
    CHECK(!proto_replay_accept(&replay, 99));
    CHECK(proto_replay_accept(&replay, 120));
    CHECK(!proto_replay_accept(&replay, 55));
    CHECK(!proto_replay_accept(&replay, 0));
}

int main(void) {
    test_siphash_vector();
    test_header_roundtrip();
    test_transport_encryption();
    test_transport_encryption_refreshes_header();
    test_key_derivation();
    test_siphash_tamper_vector();
    test_builders();
    test_dst_ip();
    test_ipv4_validation();
    test_replay_window();

    return sitest_finish("protocol tests");
}
