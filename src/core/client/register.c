#include "internal.h"
#include "log.h"
#include "network.h"

static int handle_assign(client_t *cli, const uint8_t *payload, uint16_t len) {
    if (len < 10) return 0;
    const uint8_t *p = payload;
    memcpy(&cli->virt_ip, p, 4);
    p += 4;
    memcpy(&cli->netmask, p, 4);
    p += 4;
    uint16_t mtu_n;
    memcpy(&mtu_n, p, 2);

    char virt_ip[TUND_IP_STR_LEN];
    LOG_INFO("★ Registered! Virtual IP: %s (MTU: %d)",
             ip_to_str_buf(cli->virt_ip, virt_ip, sizeof(virt_ip)), ntohs(mtu_n));
    return 1;
}

static void log_registration_failure(bool auth, bool mismatch, bool bad, bool reply, int attempts) {
    if (mismatch)
        LOG_ERROR("Registration failed: server protocol is not compatible with this client.");
    else if (auth)
        LOG_ERROR("Registration failed: server replied but authentication failed; check the shared "
                  "key and protocol compatibility.");
    else if (bad)
        LOG_ERROR("Registration failed: server replied with an invalid handshake.");
    else if (!reply)
        LOG_ERROR("Registration timed out after %d attempts; check server address, UDP port, and "
                  "firewall.",
                  attempts);
    else
        LOG_ERROR("Registration failed after %d attempts", attempts);
}

static bool is_runtime_message(uint8_t type) {
    return type == MSG_DATA || type == MSG_KEEPALIVE || type == MSG_KEEPALIVE_ACK ||
           type == MSG_PEER_JOIN || type == MSG_PEER_LEAVE || type == MSG_DISCONNECT;
}

client_registration_result_t client_register(client_t *cli, int attempts) {
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    bool auth = false, mismatch = false, bad = false, reply = false;

    for (int attempt = 0; attempt < attempts; attempt++) {
        LOG_INFO("Registering with server (attempt %d/%d)...", attempt + 1, attempts);
        int len = proto_build_register(buf, cli->name);
        if (net_send(cli->sockfd, buf, len, &cli->server_addr) < 0) return CLIENT_REGISTER_RETRY;

        bool attempt_reply = false;
        uint64_t deadline = now_ms() + TUND_REGISTER_TIMEOUT * 1000;
        while (true) {
            uint64_t now = now_ms();
            if (now >= deadline) break;
            int timeout_ms = (int)(deadline - now);
            int ret = platform_poll_one(cli->sockfd, timeout_ms);
            if (ret < 0) {
                LOG_ERROR("poll() failed: %s", sock_errstr(SOCK_Error()));
                return CLIENT_REGISTER_RETRY;
            }
            if (ret == 0) break;

            int n = net_recv(cli->sockfd, buf, sizeof(buf), &from);
            if (n == NET_RECV_AUTH_FAILED && net_addr_equal(&from, &cli->server_addr)) {
                auth = reply = attempt_reply = true;
                LOG_WARN("Server replied, but authentication failed; check the shared key and "
                         "protocol version.");
                break;
            }
            if (n <= 0 || !net_addr_equal(&from, &cli->server_addr)) continue;
            reply = attempt_reply = true;

            uint8_t type;
            uint16_t payload_len;
            int hdr = proto_read_hdr(buf, &type, &payload_len);
            if (hdr < 0) {
                bad = true;
                if (hdr == TUND_HDR_BAD_VERSION) {
                    mismatch = true;
                    LOG_WARN("Server uses unsupported protocol version %u (expected %u).", buf[1],
                             TUND_PROTOCOL_VERSION);
                } else {
                    LOG_WARN("Server sent an invalid handshake reply.");
                }
                break;
            }
            if (TUND_HDR_SIZE + payload_len != n) {
                bad = true;
                LOG_WARN("Server sent a truncated handshake reply.");
                break;
            }

            if (type == MSG_PEER_LIST || is_runtime_message(type)) {
                LOG_DEBUG("Ignored message type 0x%02X while registering", type);
                continue;
            }
            if (type != MSG_ASSIGN) {
                bad = true;
                LOG_WARN("Server sent an unexpected handshake message type 0x%02X.", type);
                break;
            }

            uint64_t session_id = 0, sequence = 0;
            if (!proto_read_session(buf, &session_id) || !proto_read_sequence(buf, &sequence) ||
                !proto_replay_accept(&cli->server_replay, session_id, sequence)) {
                LOG_DEBUG("Dropped replayed handshake reply");
                continue;
            }
            if (handle_assign(cli, buf + TUND_HDR_SIZE, payload_len)) return CLIENT_REGISTER_OK;
            bad = true;
            LOG_WARN("Server sent an invalid assignment.");
            break;
        }

        if (!attempt_reply) {
            LOG_WARN("No response from server; check the IP, port, firewall, and that the server "
                     "is running.");
        }
    }

    log_registration_failure(auth, mismatch, bad, reply, attempts);
    return CLIENT_REGISTER_RETRY;
}
