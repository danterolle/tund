/*
 * TunD — Virtual LAN Tool
 * protocol.h — Wire protocol definitions
 *
 * Every UDP datagram exchanged between server and client starts with
 * a 21-byte header. The final 8 bytes are a keyed integrity tag.
 *
 *   +-------+---------+------+--------+----------------+----------------+
 *   | Magic | Version | Type | Length | Seq (64-bit)   | Tag (64-bit)   |
 *   +-------+---------+------+--------+----------------+----------------+
 *   |                           Payload ...                             |
 *   +-------------------------------------------------------------------+
 *
 * Magic   = 0xA9
 * Version = TUND_PROTOCOL_VERSION
 * Type    = one of MSG_* constants
 * Length  = big-endian uint16 payload length (NOT including header)
 * Seq     = sender sequence number for replay protection
 */

#ifndef TUND_PROTOCOL_H
#define TUND_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TUND_MAGIC            0xA9
#define TUND_PROTOCOL_VERSION 3
#define TUND_PORT             9909
#define TUND_SEQUENCE_SIZE    8
#define TUND_AUTH_TAG_SIZE    8
#define TUND_SEQUENCE_OFFSET  5
#define TUND_AUTH_TAG_OFFSET  (TUND_SEQUENCE_OFFSET + TUND_SEQUENCE_SIZE)
#define TUND_HDR_SIZE         (TUND_AUTH_TAG_OFFSET + TUND_AUTH_TAG_SIZE)
#define TUND_MAX_PAYLOAD      1600 /* MTU 1500 + some headroom */
#define TUND_MAX_PKT          (TUND_HDR_SIZE + TUND_MAX_PAYLOAD)
#define TUND_NAME_LEN         32
#define TUND_TUN_MTU          1400 /* Leave room for UDP encapsulation */

#define TUND_HDR_OK          0
#define TUND_HDR_BAD_MAGIC   -1
#define TUND_HDR_BAD_VERSION -2

/* Virtual subnet: 10.9.0.0/24 */
#define TUND_SUBNET    0x0A090000 /* 10.9.0.0 in host byte order */
#define TUND_NETMASK   0xFFFFFF00 /* /24 */
#define TUND_SERVER_IP 0x0A090001 /* 10.9.0.1 — server's virtual IP */
#define TUND_IP_START  0x0A090002 /* 10.9.0.2 — first client IP */
#define TUND_IP_END    0x0A0900FE /* 10.9.0.254 — last client IP */

/* Timing */
#define TUND_KEEPALIVE_INTERVAL 10 /* seconds */
#define TUND_PEER_TIMEOUT       30 /* seconds */
#define TUND_SERVER_TIMEOUT     30 /* seconds */
#define TUND_REGISTER_TIMEOUT   5  /* seconds — wait for ASSIGN */
#define TUND_REGISTER_RETRIES   3

enum {
    MSG_REGISTER = 0x01,      /* Client → Server : name (max 32B)        */
    MSG_ASSIGN = 0x02,        /* Server → Client : ip(4) mask(4) mtu(2)  */
    MSG_PEER_LIST = 0x03,     /* Server → Client : N × peer_info         */
    MSG_DATA = 0x04,          /* Bidirectional   : raw IP packet          */
    MSG_KEEPALIVE = 0x05,     /* Bidirectional   : timestamp (8B)         */
    MSG_DISCONNECT = 0x06,    /* Client → Server : (empty)               */
    MSG_PEER_JOIN = 0x07,     /* Server → Client : ip(4) + name(32)      */
    MSG_PEER_LEAVE = 0x08,    /* Server → Client : ip(4)                 */
    MSG_KEEPALIVE_ACK = 0x09, /* Bidirectional   : echoed timestamp (8B) */
};

typedef struct {
    uint32_t virt_ip; /* network byte order */
    char name[TUND_NAME_LEN];
    uint8_t status; /* 1 = online, 0 = offline */
} __attribute__((packed)) msg_peer_entry_t;

typedef struct {
    uint64_t highest;
    uint64_t seen;
} proto_replay_window_t;

int proto_write_hdr(uint8_t *buf, uint8_t type, uint16_t payload_len);
int proto_read_hdr(const uint8_t *buf, uint8_t *type, uint16_t *payload_len);
bool proto_read_sequence(const uint8_t *buf, uint64_t *sequence);

uint64_t proto_siphash24(const uint8_t *in, size_t len, uint64_t k0, uint64_t k1);
void proto_key_from_passphrase(const char *passphrase, uint64_t *k0, uint64_t *k1);
void proto_sign(uint8_t *buf, int len, uint64_t k0, uint64_t k1);
bool proto_verify(const uint8_t *buf, int len, uint64_t k0, uint64_t k1);
void proto_replay_reset(proto_replay_window_t *window);
bool proto_replay_accept(proto_replay_window_t *window, uint64_t sequence);

int proto_build_register(uint8_t *buf, const char *name);
int proto_build_assign(uint8_t *buf, uint32_t virt_ip, uint32_t netmask, uint16_t mtu);
int proto_build_data(uint8_t *buf, const uint8_t *ip_pkt, uint16_t ip_len);
int proto_build_keepalive(uint8_t *buf, uint64_t timestamp);
int proto_build_keepalive_ack(uint8_t *buf, uint64_t timestamp);
bool proto_read_keepalive_timestamp(const uint8_t *payload, uint16_t payload_len,
                                    uint64_t *timestamp);
int proto_build_disconnect(uint8_t *buf);
int proto_build_peer_join(uint8_t *buf, uint32_t virt_ip, const char *name);
int proto_build_peer_leave(uint8_t *buf, uint32_t virt_ip);

uint32_t proto_get_dst_ip(const uint8_t *ip_pkt, uint16_t len);
uint32_t proto_get_src_ip(const uint8_t *ip_pkt, uint16_t len);

#endif /* TUND_PROTOCOL_H */
