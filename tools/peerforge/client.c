#include "client.h"

#include "options.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

void peerforge_client_init(peerforge_client_t *client) {
    client->sock = PEERFORGE_INVALID_SOCKET;
    client->virt_ip = 0;
    client->assigned = false;
}

int peerforge_client_open(peerforge_client_t *client) {
    client->sock = peerforge_socket_open();
    return client->sock == PEERFORGE_INVALID_SOCKET ? -1 : 0;
}

void peerforge_client_close(peerforge_client_t *client) {
    peerforge_socket_close(client->sock);
    peerforge_client_init(client);
}

static int send_encrypted(peerforge_socket_t sock, uint8_t *buf, int len,
                          const peerforge_session_t *session) {
    int wire_len = proto_encrypt(buf, len, session->key);
    if (wire_len < 0) return -1;
    int sent = (int)sendto(sock, (const char *)buf, (size_t)wire_len, 0,
                           (const struct sockaddr *)&session->server, sizeof(session->server));
    return sent == wire_len ? 0 : -1;
}

static int recv_message(peerforge_socket_t sock, const peerforge_session_t *session, uint8_t *buf,
                        int timeout_ms, uint8_t *type, uint16_t *payload_len) {
    int ready = peerforge_wait_readable(sock, timeout_ms);
    if (ready <= 0) return ready;

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int len =
        (int)recvfrom(sock, (char *)buf, TUND_MAX_PKT, 0, (struct sockaddr *)&from, &from_len);
    len = proto_decrypt(buf, len, session->key);
    if (len < TUND_HDR_SIZE) return -1;
    if (proto_read_hdr(buf, type, payload_len) < 0) return -1;
    if (TUND_HDR_SIZE + *payload_len > len) return -1;
    return len;
}

static void handle_server_keepalive(peerforge_socket_t sock, const peerforge_session_t *session,
                                    const uint8_t *payload, uint16_t payload_len) {
    uint64_t timestamp = 0;
    uint8_t buf[TUND_MAX_PKT];
    if (!proto_read_keepalive_timestamp(payload, payload_len, &timestamp)) return;
    int len = proto_build_keepalive_ack(buf, timestamp);
    send_encrypted(sock, buf, len, session);
}

void peerforge_client_drain(peerforge_client_t *client, const peerforge_session_t *session) {
    for (;;) {
        uint8_t buf[TUND_MAX_PKT];
        uint8_t type = 0;
        uint16_t payload_len = 0;
        int len = recv_message(client->sock, session, buf, 0, &type, &payload_len);
        if (len <= 0) return;
        if (type == MSG_KEEPALIVE)
            handle_server_keepalive(client->sock, session, buf + TUND_HDR_SIZE, payload_len);
    }
}

int peerforge_register_client(peerforge_client_t *client, int index,
                              const peerforge_session_t *session) {
    uint8_t buf[TUND_MAX_PKT];
    char name[TUND_NAME_LEN];
    snprintf(name, sizeof(name), "forge-%03d", index + 1);

    int len = proto_build_register(buf, name);
    if (send_encrypted(client->sock, buf, len, session) < 0) return -1;

    uint64_t deadline = peerforge_now_ms() + (uint64_t)session->timeout_ms;
    while (peerforge_now_ms() < deadline) {
        uint8_t type = 0;
        uint16_t payload_len = 0;
        int remaining = (int)(deadline - peerforge_now_ms());
        len = recv_message(client->sock, session, buf, remaining > 0 ? remaining : 1, &type,
                           &payload_len);
        if (len <= 0) continue;
        if (type == MSG_KEEPALIVE) {
            handle_server_keepalive(client->sock, session, buf + TUND_HDR_SIZE, payload_len);
        } else if (type == MSG_ASSIGN && payload_len >= 10) {
            memcpy(&client->virt_ip, buf + TUND_HDR_SIZE, sizeof(client->virt_ip));
            client->assigned = true;
            return 0;
        }
    }
    return -1;
}

int peerforge_keepalive_round(peerforge_client_t *clients, int count,
                              const peerforge_session_t *session) {
    int ok = 0;
    for (int i = 0; i < count; i++) {
        if (!clients[i].assigned) continue;
        uint8_t buf[TUND_MAX_PKT];
        uint64_t timestamp = peerforge_now_ms();
        int len = proto_build_keepalive(buf, timestamp);
        if (send_encrypted(clients[i].sock, buf, len, session) < 0) continue;

        uint64_t deadline = peerforge_now_ms() + (uint64_t)session->timeout_ms;
        while (peerforge_now_ms() < deadline) {
            uint8_t type = 0;
            uint16_t payload_len = 0;
            int remaining = (int)(deadline - peerforge_now_ms());
            len = recv_message(clients[i].sock, session, buf, remaining > 0 ? remaining : 1, &type,
                               &payload_len);
            if (len <= 0) continue;
            if (type == MSG_KEEPALIVE) {
                handle_server_keepalive(clients[i].sock, session, buf + TUND_HDR_SIZE, payload_len);
            } else if (type == MSG_KEEPALIVE_ACK) {
                uint64_t echoed = 0;
                if (proto_read_keepalive_timestamp(buf + TUND_HDR_SIZE, payload_len, &echoed) &&
                    echoed == timestamp) {
                    ok++;
                    break;
                }
            }
        }
    }
    return ok;
}

static void build_ipv4_probe(uint8_t *pkt, uint32_t src, uint32_t dst) {
    memset(pkt, 0, 20);
    pkt[0] = 0x45;
    pkt[8] = 64;
    pkt[9] = 253;
    uint16_t total = htons(20);
    memcpy(pkt + 2, &total, sizeof(total));
    memcpy(pkt + 12, &src, sizeof(src));
    memcpy(pkt + 16, &dst, sizeof(dst));
}

int peerforge_data_probe(peerforge_client_t *clients, int count, int pairs,
                         const peerforge_session_t *session) {
    int active_count = 0;
    int active[PEERFORGE_MAX_CLIENTS];
    for (int i = 0; i < count; i++) {
        if (clients[i].assigned) active[active_count++] = i;
    }
    if (active_count < 2) return 0;

    int ok = 0;
    for (int i = 0; i < pairs; i++) {
        int src_idx = active[i % active_count];
        int dst_idx = active[(i + 1) % active_count];
        uint8_t ip_pkt[20];
        uint8_t buf[TUND_MAX_PKT];
        build_ipv4_probe(ip_pkt, clients[src_idx].virt_ip, clients[dst_idx].virt_ip);
        int len = proto_build_data(buf, ip_pkt, sizeof(ip_pkt));
        if (send_encrypted(clients[src_idx].sock, buf, len, session) < 0) continue;

        uint64_t deadline = peerforge_now_ms() + (uint64_t)session->timeout_ms;
        while (peerforge_now_ms() < deadline) {
            uint8_t type = 0;
            uint16_t payload_len = 0;
            int remaining = (int)(deadline - peerforge_now_ms());
            len = recv_message(clients[dst_idx].sock, session, buf, remaining > 0 ? remaining : 1,
                               &type, &payload_len);
            if (len <= 0) continue;
            if (type == MSG_KEEPALIVE) {
                handle_server_keepalive(clients[dst_idx].sock, session, buf + TUND_HDR_SIZE,
                                        payload_len);
            } else if (type == MSG_DATA && payload_len >= sizeof(ip_pkt) &&
                       proto_get_src_ip(buf + TUND_HDR_SIZE, payload_len) ==
                           clients[src_idx].virt_ip &&
                       proto_get_dst_ip(buf + TUND_HDR_SIZE, payload_len) ==
                           clients[dst_idx].virt_ip) {
                ok++;
                break;
            }
        }
    }
    return ok;
}
