#include "internal.h"
#include "log.h"

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

static void log_registration_failure(bool auth, bool mismatch, bool bad, bool reply) {
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
                  TUND_REGISTER_RETRIES);
    else
        LOG_ERROR("Registration failed after %d attempts", TUND_REGISTER_RETRIES);
}

int client_register(client_t *cli) {
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    bool auth = false, mismatch = false, bad = false, reply = false;

    for (int attempt = 0; attempt < TUND_REGISTER_RETRIES; attempt++) {
        LOG_INFO("Registering with server (attempt %d/%d)...", attempt + 1, TUND_REGISTER_RETRIES);
        int len = proto_build_register(buf, cli->name);
        if (net_send(cli->sockfd, buf, len, &cli->server_addr) < 0) return -1;

        int ret = platform_poll_one(cli->sockfd, TUND_REGISTER_TIMEOUT * 1000);
        if (ret < 0) {
            LOG_ERROR("poll() failed: %s", sock_errstr(SOCK_Error()));
            return -1;
        }
        if (ret == 0) {
            LOG_WARN("No response from server; check the IP, port, firewall, and that the server "
                     "is running.");
            continue;
        }

        int n = net_recv(cli->sockfd, buf, sizeof(buf), &from);
        if (n == NET_RECV_AUTH_FAILED && net_addr_equal(&from, &cli->server_addr)) {
            auth = reply = true;
            LOG_WARN("Server replied, but authentication failed; check the shared key and protocol "
                     "version.");
            continue;
        }
        if (n <= 0 || !net_addr_equal(&from, &cli->server_addr)) continue;
        reply = true;

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
            continue;
        }
        if (TUND_HDR_SIZE + payload_len > n) {
            bad = true;
            LOG_WARN("Server sent a truncated handshake reply.");
            continue;
        }
        uint64_t sequence = 0;
        if (!proto_read_sequence(buf, &sequence) ||
            !proto_replay_accept(&cli->server_replay, sequence)) {
            bad = true;
            LOG_WARN("Server sent a replayed handshake reply.");
            continue;
        }
        if (type == MSG_ASSIGN && handle_assign(cli, buf + TUND_HDR_SIZE, payload_len)) return 0;
        bad = true;
        LOG_WARN("Server sent an unexpected handshake message type 0x%02X.", type);
    }

    log_registration_failure(auth, mismatch, bad, reply);
    return -1;
}
