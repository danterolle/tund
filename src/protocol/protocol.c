#include "protocol.h"
#include "monocypher.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
BOOLEAN NTAPI SystemFunction036(PVOID random_buffer, ULONG random_buffer_length);
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static uint64_t proto_next_sequence(void);
static bool proto_random_bytes(uint8_t *buf, size_t len);
static void proto_write_payload_len(uint8_t *buf, uint16_t payload_len);

int proto_write_hdr(uint8_t *buf, uint8_t type, uint16_t payload_len) {
    if (payload_len > TUND_MAX_PLAINTEXT) return -1;
    buf[0] = TUND_MAGIC;
    buf[1] = TUND_PROTOCOL_VERSION;
    buf[2] = type;
    proto_write_payload_len(buf, payload_len);
    uint64_t sequence = proto_next_sequence();
    for (int i = 7; i >= 0; i--) {
        buf[TUND_SEQUENCE_OFFSET + i] = (uint8_t)(sequence & 0xFF);
        sequence >>= 8;
    }
    if (!proto_random_bytes(buf + TUND_NONCE_OFFSET, TUND_NONCE_SIZE)) return -1;
    return TUND_HDR_SIZE;
}

static void proto_write_payload_len(uint8_t *buf, uint16_t payload_len) {
    buf[3] = (uint8_t)(payload_len >> 8);
    buf[4] = (uint8_t)(payload_len & 0xFF);
}

static atomic_uint_fast64_t g_proto_next_sequence = ATOMIC_VAR_INIT(0);

static uint64_t proto_initial_sequence(void) {
    uint64_t now = (uint64_t)time(NULL);
    uint64_t addr = (uint64_t)(uintptr_t)&g_proto_next_sequence;
    uint64_t seed = (now << 32) ^ (addr & 0xFFFFFFFFULL);
    return seed == 0 ? 1 : seed;
}

static uint64_t proto_next_sequence(void) {
    uint_fast64_t current = atomic_load_explicit(&g_proto_next_sequence, memory_order_relaxed);
    if (current == 0) {
        uint_fast64_t expected = 0;
        atomic_compare_exchange_strong_explicit(&g_proto_next_sequence, &expected,
                                                (uint_fast64_t)proto_initial_sequence(),
                                                memory_order_relaxed, memory_order_relaxed);
    }
    return (uint64_t)atomic_fetch_add_explicit(&g_proto_next_sequence, 1, memory_order_relaxed) + 1;
}

static bool proto_random_bytes(uint8_t *buf, size_t len) {
#ifdef _WIN32
    if (len > (size_t)ULONG_MAX) return false;
    return SystemFunction036(buf, (ULONG)len) != FALSE;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;

    size_t offset = 0;
    while (offset < len) {
        ssize_t got = read(fd, buf + offset, len - offset);
        if (got <= 0) {
            close(fd);
            return false;
        }
        offset += (size_t)got;
    }
    close(fd);
    return true;
#endif
}

static uint64_t proto_rotl64(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

static uint64_t proto_load64_le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static uint64_t proto_load64_be(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

#define SIPROUND                                                                                   \
    do {                                                                                           \
        v0 += v1;                                                                                  \
        v1 = proto_rotl64(v1, 13);                                                                 \
        v1 ^= v0;                                                                                  \
        v0 = proto_rotl64(v0, 32);                                                                 \
        v2 += v3;                                                                                  \
        v3 = proto_rotl64(v3, 16);                                                                 \
        v3 ^= v2;                                                                                  \
        v0 += v3;                                                                                  \
        v3 = proto_rotl64(v3, 21);                                                                 \
        v3 ^= v0;                                                                                  \
        v2 += v1;                                                                                  \
        v1 = proto_rotl64(v1, 17);                                                                 \
        v1 ^= v2;                                                                                  \
        v2 = proto_rotl64(v2, 32);                                                                 \
    } while (0)

uint64_t proto_siphash24(const uint8_t *in, size_t len, uint64_t k0, uint64_t k1) {
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;
    const uint8_t *end = in + (len & ~(size_t)7);
    uint64_t b = (uint64_t)len << 56;

    for (; in != end; in += 8) {
        uint64_t m = proto_load64_le(in);
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
    }
    for (size_t i = 0; i < (len & 7); i++) b |= (uint64_t)in[i] << (8 * i);

    v3 ^= b;
    SIPROUND;
    SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

void proto_key_from_passphrase(const char *passphrase, uint8_t key[TUND_KEY_SIZE]) {
    static const uint8_t context[] = "TunD transport key v1";
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, TUND_KEY_SIZE);
    crypto_blake2b_update(&ctx, context, sizeof(context) - 1);
    crypto_blake2b_update(&ctx, (const uint8_t *)passphrase, strlen(passphrase));
    crypto_blake2b_final(&ctx, key);
}

int proto_encrypt(uint8_t *buf, int len, const uint8_t key[TUND_KEY_SIZE]) {
    if (len < TUND_HDR_SIZE) return -1;
    int plaintext_len = len - TUND_HDR_SIZE;
    if (plaintext_len < 0 || plaintext_len > TUND_MAX_PLAINTEXT) return -1;

    uint8_t ciphertext[TUND_MAX_PLAINTEXT];
    uint8_t mac[TUND_AUTH_TAG_SIZE];
    uint16_t wire_payload_len = (uint16_t)(plaintext_len + TUND_AUTH_TAG_SIZE);
    proto_write_payload_len(buf, wire_payload_len);

    crypto_aead_lock(ciphertext, mac, key, buf + TUND_NONCE_OFFSET, buf, TUND_HDR_SIZE,
                     buf + TUND_HDR_SIZE, (size_t)plaintext_len);
    memcpy(buf + TUND_HDR_SIZE, ciphertext, (size_t)plaintext_len);
    memcpy(buf + TUND_HDR_SIZE + plaintext_len, mac, sizeof(mac));
    crypto_wipe(ciphertext, sizeof(ciphertext));
    crypto_wipe(mac, sizeof(mac));
    return TUND_HDR_SIZE + (int)wire_payload_len;
}

int proto_decrypt(uint8_t *buf, int len, const uint8_t key[TUND_KEY_SIZE]) {
    uint8_t type;
    uint16_t wire_payload_len;
    if (len < TUND_HDR_SIZE + TUND_AUTH_TAG_SIZE) return -1;
    if (proto_read_hdr(buf, &type, &wire_payload_len) < 0) return -1;
    (void)type;
    if (wire_payload_len < TUND_AUTH_TAG_SIZE || TUND_HDR_SIZE + wire_payload_len != len) return -1;

    int plaintext_len = (int)wire_payload_len - TUND_AUTH_TAG_SIZE;
    if (plaintext_len > TUND_MAX_PLAINTEXT) return -1;

    uint8_t plaintext[TUND_MAX_PLAINTEXT];
    const uint8_t *mac = buf + TUND_HDR_SIZE + plaintext_len;
    int failed = crypto_aead_unlock(plaintext, mac, key, buf + TUND_NONCE_OFFSET, buf,
                                    TUND_HDR_SIZE, buf + TUND_HDR_SIZE, (size_t)plaintext_len);
    if (failed) return -1;

    memcpy(buf + TUND_HDR_SIZE, plaintext, (size_t)plaintext_len);
    proto_write_payload_len(buf, (uint16_t)plaintext_len);
    crypto_wipe(plaintext, sizeof(plaintext));
    return TUND_HDR_SIZE + plaintext_len;
}

int proto_read_hdr(const uint8_t *buf, uint8_t *type, uint16_t *payload_len) {
    if (buf[0] != TUND_MAGIC) return TUND_HDR_BAD_MAGIC;
    if (buf[1] != TUND_PROTOCOL_VERSION) return TUND_HDR_BAD_VERSION;
    *type = buf[2];
    *payload_len = (uint16_t)((buf[3] << 8) | buf[4]);
    return 0;
}

bool proto_read_sequence(const uint8_t *buf, uint64_t *sequence) {
    uint64_t value = proto_load64_be(buf + TUND_SEQUENCE_OFFSET);
    if (value == 0) return false;
    *sequence = value;
    return true;
}

void proto_replay_reset(proto_replay_window_t *window) {
    window->highest = 0;
    window->seen = 0;
}

bool proto_replay_accept(proto_replay_window_t *window, uint64_t sequence) {
    if (sequence == 0) return false;

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
    if (age >= 64) return false;

    uint64_t bit = 1ULL << age;
    if ((window->seen & bit) != 0) return false;

    window->seen |= bit;
    return true;
}

int proto_build_register(uint8_t *buf, const char *name) {
    uint16_t nlen = (uint16_t)strlen(name);
    if (nlen > TUND_NAME_LEN) nlen = TUND_NAME_LEN;
    if (proto_write_hdr(buf, MSG_REGISTER, nlen) < 0) return -1;
    memcpy(buf + TUND_HDR_SIZE, name, nlen);
    return TUND_HDR_SIZE + nlen;
}

int proto_build_assign(uint8_t *buf, uint32_t virt_ip, uint32_t netmask, uint16_t mtu) {
    uint16_t plen = 10;
    if (proto_write_hdr(buf, MSG_ASSIGN, plen) < 0) return -1;
    uint8_t *p = buf + TUND_HDR_SIZE;
    memcpy(p, &virt_ip, 4);
    p += 4;
    memcpy(p, &netmask, 4);
    p += 4;
    uint16_t nmtu = htons(mtu);
    memcpy(p, &nmtu, 2);
    return TUND_HDR_SIZE + plen;
}

int proto_build_data(uint8_t *buf, const uint8_t *ip_pkt, uint16_t ip_len) {
    if (proto_write_hdr(buf, MSG_DATA, ip_len) < 0) return -1;
    memcpy(buf + TUND_HDR_SIZE, ip_pkt, ip_len);
    return TUND_HDR_SIZE + ip_len;
}

static int proto_build_timestamp_msg(uint8_t *buf, uint8_t type, uint64_t timestamp) {
    if (proto_write_hdr(buf, type, 8) < 0) return -1;
    uint8_t *p = buf + TUND_HDR_SIZE;
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(timestamp & 0xFF);
        timestamp >>= 8;
    }
    return TUND_HDR_SIZE + 8;
}

int proto_build_keepalive(uint8_t *buf, uint64_t timestamp) {
    return proto_build_timestamp_msg(buf, MSG_KEEPALIVE, timestamp);
}
int proto_build_keepalive_ack(uint8_t *buf, uint64_t timestamp) {
    return proto_build_timestamp_msg(buf, MSG_KEEPALIVE_ACK, timestamp);
}

bool proto_read_keepalive_timestamp(const uint8_t *payload, uint16_t payload_len,
                                    uint64_t *timestamp) {
    if (payload_len < 8) return false;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) value = (value << 8) | payload[i];
    *timestamp = value;
    return true;
}

int proto_build_disconnect(uint8_t *buf) {
    if (proto_write_hdr(buf, MSG_DISCONNECT, 0) < 0) return -1;
    return TUND_HDR_SIZE;
}

int proto_build_peer_join(uint8_t *buf, uint32_t virt_ip, const char *name) {
    uint16_t plen = 4 + TUND_NAME_LEN;
    if (proto_write_hdr(buf, MSG_PEER_JOIN, plen) < 0) return -1;
    uint8_t *p = buf + TUND_HDR_SIZE;
    memcpy(p, &virt_ip, 4);
    p += 4;
    memset(p, 0, TUND_NAME_LEN);
    strncpy((char *)p, name, TUND_NAME_LEN - 1);
    return TUND_HDR_SIZE + plen;
}

int proto_build_peer_leave(uint8_t *buf, uint32_t virt_ip) {
    if (proto_write_hdr(buf, MSG_PEER_LEAVE, 4) < 0) return -1;
    memcpy(buf + TUND_HDR_SIZE, &virt_ip, 4);
    return TUND_HDR_SIZE + 4;
}

uint32_t proto_get_dst_ip(const uint8_t *ip_pkt, uint16_t len) {
    if (len < 20) return 0;
    uint32_t dst;
    memcpy(&dst, ip_pkt + 16, 4);
    return dst;
}

uint32_t proto_get_src_ip(const uint8_t *ip_pkt, uint16_t len) {
    if (len < 20) return 0;
    uint32_t src;
    memcpy(&src, ip_pkt + 12, 4);
    return src;
}
