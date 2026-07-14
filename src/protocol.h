/*
 * Tund — Virtual LAN Tool
 * protocol.h — Wire protocol definitions
 *
 * Every UDP datagram exchanged between server and client starts with
 * a 13-byte header. The final 8 bytes are a keyed integrity tag.
 *
 *   +-------+---------+------+-----------------+----------------+
 *   | Magic | Version | Type | Length (16-bit) | Tag (64-bit)   |
 *   +-------+---------+------+-----------------+----------------+
 *   |                         Payload ...                       |
 *   +-----------------------------------------------------------+
 *
 * Magic  = 0xA9
 * Version = TUND_PROTOCOL_VERSION
 * Type   = one of MSG_* constants
 * Length = big-endian uint16 payload length (NOT including header)
 */

#ifndef TUND_PROTOCOL_H
#define TUND_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#define TUND_MAGIC        0xA9
#define TUND_PROTOCOL_VERSION 2
#define TUND_PORT         9909
#define TUND_AUTH_TAG_SIZE 8
#define TUND_AUTH_TAG_OFFSET 5
#define TUND_HDR_SIZE     (TUND_AUTH_TAG_OFFSET + TUND_AUTH_TAG_SIZE)
#define TUND_MAX_PAYLOAD  1600   /* MTU 1500 + some headroom */
#define TUND_MAX_PKT      (TUND_HDR_SIZE + TUND_MAX_PAYLOAD)
#define TUND_NAME_LEN     32
#define TUND_TUN_MTU      1400   /* Leave room for UDP encapsulation */

#define TUND_HDR_OK          0
#define TUND_HDR_BAD_MAGIC  -1
#define TUND_HDR_BAD_VERSION -2

/* Virtual subnet: 10.9.0.0/24 */
#define TUND_SUBNET       0x0A090000  /* 10.9.0.0 in host byte order */
#define TUND_NETMASK      0xFFFFFF00  /* /24 */
#define TUND_SERVER_IP    0x0A090001  /* 10.9.0.1 — server's virtual IP */
#define TUND_IP_START     0x0A090002  /* 10.9.0.2 — first client IP */
#define TUND_IP_END       0x0A0900FE  /* 10.9.0.254 — last client IP */

/* Timing */
#define TUND_KEEPALIVE_INTERVAL   10  /* seconds */
#define TUND_PEER_TIMEOUT         30  /* seconds */
#define TUND_REGISTER_TIMEOUT     5   /* seconds — wait for ASSIGN */
#define TUND_REGISTER_RETRIES     3

enum {
    MSG_REGISTER    = 0x01,   /* Client → Server : name (max 32B)        */
    MSG_ASSIGN      = 0x02,   /* Server → Client : ip(4) mask(4) mtu(2)  */
    MSG_PEER_LIST   = 0x03,   /* Server → Client : N × peer_info         */
    MSG_DATA        = 0x04,   /* Bidirectional   : raw IP packet          */
    MSG_KEEPALIVE   = 0x05,   /* Bidirectional   : timestamp (8B)         */
    MSG_DISCONNECT  = 0x06,   /* Client → Server : (empty)               */
    MSG_PEER_JOIN   = 0x07,   /* Server → Client : ip(4) + name(32)      */
    MSG_PEER_LEAVE  = 0x08,   /* Server → Client : ip(4)                 */
};

typedef struct {
    uint32_t virt_ip;       /* network byte order */
    char     name[TUND_NAME_LEN];
    uint8_t  status;        /* 1 = online, 0 = offline */
} __attribute__((packed)) msg_peer_entry_t;

static inline int proto_write_hdr(uint8_t *buf, uint8_t type, uint16_t payload_len)
{
    buf[0] = TUND_MAGIC;
    buf[1] = TUND_PROTOCOL_VERSION;
    buf[2] = type;
    buf[3] = (uint8_t)(payload_len >> 8);
    buf[4] = (uint8_t)(payload_len & 0xFF);
    memset(buf + TUND_AUTH_TAG_OFFSET, 0, TUND_AUTH_TAG_SIZE);
    return TUND_HDR_SIZE;
}

/* SipHash-2-4: keyed MAC for authenticated LAN membership.
 * Reference: https://ia.cr/2012/351 — Aumasson & Bernstein. */
static inline uint64_t proto_rotl64(uint64_t x, int b)
{
    return (x << b) | (x >> (64 - b));
}

static inline uint64_t proto_load64_le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

#define SIPROUND \
    do { \
        v0 += v1; v1 = proto_rotl64(v1, 13); v1 ^= v0; v0 = proto_rotl64(v0, 32); \
        v2 += v3; v3 = proto_rotl64(v3, 16); v3 ^= v2; \
        v0 += v3; v3 = proto_rotl64(v3, 21); v3 ^= v0; \
        v2 += v1; v1 = proto_rotl64(v1, 17); v1 ^= v2; v2 = proto_rotl64(v2, 32); \
    } while (0)

static inline uint64_t proto_siphash24(const uint8_t *in, size_t len,
                                       uint64_t k0, uint64_t k1)
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

static inline void proto_key_from_passphrase(const char *passphrase,
                                             uint64_t *k0, uint64_t *k1)
{
    *k0 = proto_siphash24((const uint8_t *)passphrase, strlen(passphrase),
                          0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL);
    *k1 = proto_siphash24((const uint8_t *)passphrase, strlen(passphrase),
                          0xfedcba9876543210ULL, 0x0123456789abcdefULL);
}

static inline void proto_sign(uint8_t *buf, int len, uint64_t k0, uint64_t k1)
{
    uint64_t tag = proto_siphash24(buf, TUND_AUTH_TAG_OFFSET, k0, k1)
                 ^ proto_siphash24(buf + TUND_HDR_SIZE,
                                   (size_t)len - TUND_HDR_SIZE, k0, k1);
    for (int i = 0; i < 8; i++)
        buf[TUND_AUTH_TAG_OFFSET + i] = (uint8_t)(tag >> (8 * i));
}

static inline bool proto_verify(const uint8_t *buf, int len, uint64_t k0, uint64_t k1)
{
    if (len < TUND_HDR_SIZE)
        return false;
    uint64_t got = proto_load64_le(buf + TUND_AUTH_TAG_OFFSET);
    uint64_t tag = proto_siphash24(buf, TUND_AUTH_TAG_OFFSET, k0, k1)
                 ^ proto_siphash24(buf + TUND_HDR_SIZE,
                                   (size_t)len - TUND_HDR_SIZE, k0, k1);
    return got == tag;
}

static inline int proto_read_hdr(const uint8_t *buf, uint8_t *type, uint16_t *payload_len)
{
    if (buf[0] != TUND_MAGIC)
        return TUND_HDR_BAD_MAGIC;
    if (buf[1] != TUND_PROTOCOL_VERSION)
        return TUND_HDR_BAD_VERSION;
    *type = buf[2];
    *payload_len = (uint16_t)((buf[3] << 8) | buf[4]);
    return 0;
}

static inline int proto_build_register(uint8_t *buf, const char *name)
{
    uint16_t nlen = (uint16_t)strlen(name);
    if (nlen > TUND_NAME_LEN)
        nlen = TUND_NAME_LEN;
    proto_write_hdr(buf, MSG_REGISTER, nlen);
    memcpy(buf + TUND_HDR_SIZE, name, nlen);
    return TUND_HDR_SIZE + nlen;
}

static inline int proto_build_assign(uint8_t *buf, uint32_t virt_ip,
                                     uint32_t netmask, uint16_t mtu)
{
    uint16_t plen = 10; /* 4 (ip) + 4 (mask) + 2 (mtu) */
    proto_write_hdr(buf, MSG_ASSIGN, plen);
    uint8_t *p = buf + TUND_HDR_SIZE;
    memcpy(p, &virt_ip, 4);  p += 4;
    memcpy(p, &netmask, 4);  p += 4;
    uint16_t nmtu = htons(mtu);
    memcpy(p, &nmtu, 2);
    return TUND_HDR_SIZE + plen;
}

static inline int proto_build_data(uint8_t *buf, const uint8_t *ip_pkt, uint16_t ip_len)
{
    proto_write_hdr(buf, MSG_DATA, ip_len);
    memcpy(buf + TUND_HDR_SIZE, ip_pkt, ip_len);
    return TUND_HDR_SIZE + ip_len;
}

static inline int proto_build_keepalive(uint8_t *buf, uint64_t timestamp)
{
    proto_write_hdr(buf, MSG_KEEPALIVE, 8);
    uint8_t *p = buf + TUND_HDR_SIZE;
    /* big-endian timestamp */
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(timestamp & 0xFF);
        timestamp >>= 8;
    }
    return TUND_HDR_SIZE + 8;
}

static inline int proto_build_disconnect(uint8_t *buf)
{
    proto_write_hdr(buf, MSG_DISCONNECT, 0);
    return TUND_HDR_SIZE;
}

static inline int proto_build_peer_join(uint8_t *buf, uint32_t virt_ip, const char *name)
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

static inline int proto_build_peer_leave(uint8_t *buf, uint32_t virt_ip)
{
    proto_write_hdr(buf, MSG_PEER_LEAVE, 4);
    memcpy(buf + TUND_HDR_SIZE, &virt_ip, 4);
    return TUND_HDR_SIZE + 4;
}

static inline uint32_t proto_get_dst_ip(const uint8_t *ip_pkt, uint16_t len)
{
    if (len < 20)
        return 0;
    /* IPv4 header: dst IP is at offset 16 */
    uint32_t dst;
    memcpy(&dst, ip_pkt + 16, 4);
    return dst;
}

static inline uint32_t proto_get_src_ip(const uint8_t *ip_pkt, uint16_t len)
{
    if (len < 20)
        return 0;
    uint32_t src;
    memcpy(&src, ip_pkt + 12, 4);
    return src;
}

#endif /* TUND_PROTOCOL_H */
