#include "protocol.h"

#include <stdatomic.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

static uint64_t proto_next_sequence(void);

int proto_write_hdr(uint8_t *buf, uint8_t type, uint16_t payload_len)
{
    buf[0] = TUND_MAGIC;
    buf[1] = TUND_PROTOCOL_VERSION;
    buf[2] = type;
    buf[3] = (uint8_t)(payload_len >> 8);
    buf[4] = (uint8_t)(payload_len & 0xFF);
    uint64_t sequence = proto_next_sequence();
    for (int i = 7; i >= 0; i--) {
        buf[TUND_SEQUENCE_OFFSET + i] = (uint8_t)(sequence & 0xFF);
        sequence >>= 8;
    }
    memset(buf + TUND_AUTH_TAG_OFFSET, 0, TUND_AUTH_TAG_SIZE);
    return TUND_HDR_SIZE;
}

static atomic_uint_fast64_t g_proto_next_sequence = ATOMIC_VAR_INIT(0);

static uint64_t proto_initial_sequence(void)
{
    uint64_t now = (uint64_t)time(NULL);
    uint64_t addr = (uint64_t)(uintptr_t)&g_proto_next_sequence;
    uint64_t seed = (now << 32) ^ (addr & 0xFFFFFFFFULL);
    return seed == 0 ? 1 : seed;
}

static uint64_t proto_next_sequence(void)
{
    uint_fast64_t current =
        atomic_load_explicit(&g_proto_next_sequence, memory_order_relaxed);
    if (current == 0) {
        uint_fast64_t expected = 0;
        atomic_compare_exchange_strong_explicit(&g_proto_next_sequence,
                                                &expected,
                                                (uint_fast64_t)proto_initial_sequence(),
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    }
    return (uint64_t)atomic_fetch_add_explicit(&g_proto_next_sequence, 1,
                                               memory_order_relaxed) + 1;
}

static uint64_t proto_rotl64(uint64_t x, int b)
{
    return (x << b) | (x >> (64 - b));
}

static uint64_t proto_load64_le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static uint64_t proto_load64_be(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v = (v << 8) | p[i];
    return v;
}

#define SIPROUND \
    do { \
        v0 += v1; v1 = proto_rotl64(v1, 13); v1 ^= v0; v0 = proto_rotl64(v0, 32); \
        v2 += v3; v3 = proto_rotl64(v3, 16); v3 ^= v2; \
        v0 += v3; v3 = proto_rotl64(v3, 21); v3 ^= v0; \
        v2 += v1; v1 = proto_rotl64(v1, 17); v1 ^= v2; v2 = proto_rotl64(v2, 32); \
    } while (0)

uint64_t proto_siphash24(const uint8_t *in, size_t len, uint64_t k0, uint64_t k1)
{
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;
    const uint8_t *end = in + (len & ~(size_t)7);
    uint64_t b = (uint64_t)len << 56;

    for (; in != end; in += 8) {
        uint64_t m = proto_load64_le(in);
        v3 ^= m; SIPROUND; SIPROUND; v0 ^= m;
    }
    for (size_t i = 0; i < (len & 7); i++)
        b |= (uint64_t)in[i] << (8 * i);

    v3 ^= b; SIPROUND; SIPROUND; v0 ^= b;
    v2 ^= 0xff; SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

void proto_key_from_passphrase(const char *passphrase, uint64_t *k0, uint64_t *k1)
{
    *k0 = proto_siphash24((const uint8_t *)passphrase, strlen(passphrase),
                          0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL);
    *k1 = proto_siphash24((const uint8_t *)passphrase, strlen(passphrase),
                          0xfedcba9876543210ULL, 0x0123456789abcdefULL);
}

void proto_sign(uint8_t *buf, int len, uint64_t k0, uint64_t k1)
{
    uint64_t tag = proto_siphash24(buf, TUND_AUTH_TAG_OFFSET, k0, k1) ^
                   proto_siphash24(buf + TUND_HDR_SIZE, (size_t)len - TUND_HDR_SIZE, k0, k1);
    for (int i = 0; i < 8; i++)
        buf[TUND_AUTH_TAG_OFFSET + i] = (uint8_t)(tag >> (8 * i));
}

bool proto_verify(const uint8_t *buf, int len, uint64_t k0, uint64_t k1)
{
    if (len < TUND_HDR_SIZE)
        return false;
    uint64_t got = proto_load64_le(buf + TUND_AUTH_TAG_OFFSET);
    uint64_t tag = proto_siphash24(buf, TUND_AUTH_TAG_OFFSET, k0, k1) ^
                   proto_siphash24(buf + TUND_HDR_SIZE, (size_t)len - TUND_HDR_SIZE, k0, k1);
    return got == tag;
}

int proto_read_hdr(const uint8_t *buf, uint8_t *type, uint16_t *payload_len)
{
    if (buf[0] != TUND_MAGIC)
        return TUND_HDR_BAD_MAGIC;
    if (buf[1] != TUND_PROTOCOL_VERSION)
        return TUND_HDR_BAD_VERSION;
    *type = buf[2];
    *payload_len = (uint16_t)((buf[3] << 8) | buf[4]);
    return 0;
}

bool proto_read_sequence(const uint8_t *buf, uint64_t *sequence)
{
    uint64_t value = proto_load64_be(buf + TUND_SEQUENCE_OFFSET);
    if (value == 0)
        return false;
    *sequence = value;
    return true;
}

void proto_replay_reset(proto_replay_window_t *window)
{
    window->highest = 0;
    window->seen = 0;
}

bool proto_replay_accept(proto_replay_window_t *window, uint64_t sequence)
{
    if (sequence == 0)
        return false;

    if (window->highest == 0) {
        window->highest = sequence;
        window->seen = 1;
        return true;
    }

    if (sequence > window->highest) {
        uint64_t shift = sequence - window->highest;
        window->seen = shift >= 64 ? 1 : (window->seen << shift) | 1;
        window->highest = sequence;
        return true;
    }

    uint64_t age = window->highest - sequence;
    if (age >= 64)
        return false;

    uint64_t bit = 1ULL << age;
    if ((window->seen & bit) != 0)
        return false;

    window->seen |= bit;
    return true;
}

int proto_build_register(uint8_t *buf, const char *name)
{
    uint16_t nlen = (uint16_t)strlen(name);
    if (nlen > TUND_NAME_LEN)
        nlen = TUND_NAME_LEN;
    proto_write_hdr(buf, MSG_REGISTER, nlen);
    memcpy(buf + TUND_HDR_SIZE, name, nlen);
    return TUND_HDR_SIZE + nlen;
}

int proto_build_assign(uint8_t *buf, uint32_t virt_ip, uint32_t netmask, uint16_t mtu)
{
    uint16_t plen = 10;
    proto_write_hdr(buf, MSG_ASSIGN, plen);
    uint8_t *p = buf + TUND_HDR_SIZE;
    memcpy(p, &virt_ip, 4);  p += 4;
    memcpy(p, &netmask, 4);  p += 4;
    uint16_t nmtu = htons(mtu);
    memcpy(p, &nmtu, 2);
    return TUND_HDR_SIZE + plen;
}

int proto_build_data(uint8_t *buf, const uint8_t *ip_pkt, uint16_t ip_len)
{
    proto_write_hdr(buf, MSG_DATA, ip_len);
    memcpy(buf + TUND_HDR_SIZE, ip_pkt, ip_len);
    return TUND_HDR_SIZE + ip_len;
}

static int proto_build_timestamp_msg(uint8_t *buf, uint8_t type, uint64_t timestamp)
{
    proto_write_hdr(buf, type, 8);
    uint8_t *p = buf + TUND_HDR_SIZE;
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(timestamp & 0xFF);
        timestamp >>= 8;
    }
    return TUND_HDR_SIZE + 8;
}

int proto_build_keepalive(uint8_t *buf, uint64_t timestamp)
{
    return proto_build_timestamp_msg(buf, MSG_KEEPALIVE, timestamp);
}
int proto_build_keepalive_ack(uint8_t *buf, uint64_t timestamp)
{
    return proto_build_timestamp_msg(buf, MSG_KEEPALIVE_ACK, timestamp);
}

bool proto_read_keepalive_timestamp(const uint8_t *payload, uint16_t payload_len,
                                    uint64_t *timestamp)
{
    if (payload_len < 8) return false;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++)
        value = (value << 8) | payload[i];
    *timestamp = value;
    return true;
}

int proto_build_disconnect(uint8_t *buf)
{
    proto_write_hdr(buf, MSG_DISCONNECT, 0);
    return TUND_HDR_SIZE;
}

int proto_build_peer_join(uint8_t *buf, uint32_t virt_ip, const char *name)
{
    uint16_t plen = 4 + TUND_NAME_LEN;
    proto_write_hdr(buf, MSG_PEER_JOIN, plen);
    uint8_t *p = buf + TUND_HDR_SIZE;
    memcpy(p, &virt_ip, 4);
    p += 4;
    memset(p, 0, TUND_NAME_LEN);
    strncpy((char *)p, name, TUND_NAME_LEN - 1);
    return TUND_HDR_SIZE + plen;
}

int proto_build_peer_leave(uint8_t *buf, uint32_t virt_ip)
{
    proto_write_hdr(buf, MSG_PEER_LEAVE, 4);
    memcpy(buf + TUND_HDR_SIZE, &virt_ip, 4);
    return TUND_HDR_SIZE + 4;
}

uint32_t proto_get_dst_ip(const uint8_t *ip_pkt, uint16_t len)
{
    if (len < 20) return 0;
    uint32_t dst;
    memcpy(&dst, ip_pkt + 16, 4);
    return dst;
}

uint32_t proto_get_src_ip(const uint8_t *ip_pkt, uint16_t len)
{
    if (len < 20) return 0;
    uint32_t src;
    memcpy(&src, ip_pkt + 12, 4);
    return src;
}
