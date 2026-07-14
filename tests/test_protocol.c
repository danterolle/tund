#include "../src/protocol.h"

#include <stdio.h>

static int failures = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

static void test_siphash_vector(void)
{
    uint8_t empty[1] = {0};
    uint64_t tag = proto_siphash24(empty, 0,
                                   0x0706050403020100ULL,
                                   0x0f0e0d0c0b0a0908ULL);

    CHECK(tag == 0x726fdb47dd0e0e31ULL);
}

static void test_header_roundtrip(void)
{
    uint8_t buf[TUND_MAX_PKT];
    uint8_t type = 0;
    uint16_t payload_len = 0;

    memset(buf, 0xCC, sizeof(buf));
    CHECK(proto_write_hdr(buf, MSG_DATA, 0x1234) == TUND_HDR_SIZE);
    CHECK(buf[0] == TUND_MAGIC);
    CHECK(buf[1] == MSG_DATA);
    CHECK(buf[2] == 0x12);
    CHECK(buf[3] == 0x34);

    for (int i = 0; i < TUND_AUTH_TAG_SIZE; i++)
        CHECK(buf[4 + i] == 0);

    CHECK(proto_read_hdr(buf, &type, &payload_len) == 0);
    CHECK(type == MSG_DATA);
    CHECK(payload_len == 0x1234);

    buf[0] = 0;
    CHECK(proto_read_hdr(buf, &type, &payload_len) == -1);
}

static void test_authentication(void)
{
    uint8_t ip_pkt[20];
    uint8_t buf[TUND_MAX_PKT];
    uint64_t k0, k1, other_k0, other_k1;

    for (size_t i = 0; i < sizeof(ip_pkt); i++)
        ip_pkt[i] = (uint8_t)i;
    ip_pkt[0] = 0x45;

    proto_key_from_passphrase("a-long-random-key", &k0, &k1);
    proto_key_from_passphrase("another-random-key", &other_k0, &other_k1);

    int len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    proto_sign(buf, len, k0, k1);

    CHECK(proto_verify(buf, len, k0, k1));
    CHECK(!proto_verify(buf, TUND_HDR_SIZE - 1, k0, k1));
    CHECK(!proto_verify(buf, len, other_k0, other_k1));

    buf[1] ^= 0x01;
    CHECK(!proto_verify(buf, len, k0, k1));

    len = proto_build_data(buf, ip_pkt, (uint16_t)sizeof(ip_pkt));
    proto_sign(buf, len, k0, k1);
    buf[TUND_HDR_SIZE + 5] ^= 0xFF;
    CHECK(!proto_verify(buf, len, k0, k1));
}

static void test_builders(void)
{
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
    for (int i = 0; i < TUND_NAME_LEN; i++)
        CHECK(buf[TUND_HDR_SIZE + i] == 'A');

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
}

static void test_dst_ip(void)
{
    uint8_t ip_pkt[20];
    uint32_t dst = htonl(0x0A090003);

    memset(ip_pkt, 0, sizeof(ip_pkt));
    memcpy(ip_pkt + 16, &dst, sizeof(dst));

    CHECK(proto_get_dst_ip(ip_pkt, sizeof(ip_pkt)) == dst);
    CHECK(proto_get_dst_ip(ip_pkt, 19) == 0);
}

int main(void)
{
    test_siphash_vector();
    test_header_roundtrip();
    test_authentication();
    test_builders();
    test_dst_ip();

    if (failures != 0) {
        fprintf(stderr, "%d protocol test(s) failed\n", failures);
        return 1;
    }

    puts("protocol tests passed");
    return 0;
}
