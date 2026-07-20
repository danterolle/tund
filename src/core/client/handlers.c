#include "internal.h"

static void client_handle_keepalive(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    uint64_t sent_at = 0;
    if (!proto_read_keepalive_timestamp(payload, plen, &sent_at))
        return;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_keepalive_ack(buf, sent_at);
    if (net_send(cli->sockfd, buf, len, &cli->server_addr) < 0)
        LOG_WARN("Keepalive reply to server failed");
}

static void client_handle_keepalive_ack(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    uint64_t sent_at = 0;
    if (!proto_read_keepalive_timestamp(payload, plen, &sent_at))
        return;
    uint64_t now = now_ms();
    if (now < sent_at)
        return;

    uint64_t sample = now - sent_at;
    cli->server_rtt_ms = smooth_rtt_ms(cli->server_rtt_ms, sample,
                                       cli->has_server_rtt);
    cli->has_server_rtt = true;
    LOG_DEBUG("Server keepalive RTT: %llums",
              (unsigned long long)cli->server_rtt_ms);
}

static void client_handle_peer_list(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    int entry_size = (int)sizeof(msg_peer_entry_t);
    int count = plen / entry_size;
    if (!g_tui_active) LOG_INFO("Received peer list: %d peer(s)", count);

    for (int i = 0; i < count; i++) {
        const msg_peer_entry_t *entry = (const msg_peer_entry_t *)(payload + i * entry_size);
        client_update_peer(cli, entry->virt_ip, entry->name, entry->status != 0);
    }
    if (!g_tui_active && count > 0) client_log_peers(cli);
}

static void client_handle_peer_join(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    if (plen < 4 + TUND_NAME_LEN) return;

    uint32_t vip;
    memcpy(&vip, payload, 4);
    char name[TUND_NAME_LEN];
    memset(name, 0, sizeof(name));
    memcpy(name, payload + 4, TUND_NAME_LEN);

    char vip_str[TUND_IP_STR_LEN];
    LOG_INFO("★ Peer joined: %s (%s)", name, ip_to_str_buf(vip, vip_str, sizeof(vip_str)));
    client_update_peer(cli, vip, name, true);
    if (!g_tui_active) client_log_peers(cli);
}

static void client_handle_peer_leave(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    if (plen < 4) return;

    uint32_t vip;
    memcpy(&vip, payload, 4);

    pthread_mutex_lock(&cli->peers_lock);
    char name[TUND_NAME_LEN] = "unknown";
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active && cli->peers[i].virt_ip == vip) {
            strncpy(name, cli->peers[i].name, TUND_NAME_LEN - 1);
            cli->peers[i].active = false;
            cli->peer_count--;
            break;
        }
    }
    int remaining = cli->peer_count;
    pthread_mutex_unlock(&cli->peers_lock);

    char vip_str[TUND_IP_STR_LEN];
    LOG_INFO("✦ Peer left: %s (%s)", name, ip_to_str_buf(vip, vip_str, sizeof(vip_str)));
    if (!g_tui_active && remaining > 0)
        client_log_peers(cli);
    else if (!g_tui_active)
        LOG_INFO("No peers connected.");
}

void client_handle_server_packet(client_t *cli, uint8_t *buf, int len)
{
    uint8_t type;
    uint16_t payload_len;
    int hdr = proto_read_hdr(buf, &type, &payload_len);
    if (hdr < 0) {
        if (hdr == TUND_HDR_BAD_VERSION)
            LOG_DEBUG("Ignored packet with unsupported protocol version %u", buf[1]);
        return;
    }
    if (TUND_HDR_SIZE + payload_len > len) return;

    client_mark_server_seen(cli);

    uint8_t *payload = buf + TUND_HDR_SIZE;
    switch (type) {
    case MSG_DATA:
        client_add_peer_traffic(cli, proto_get_src_ip(payload, payload_len), payload_len, 0);
        tun_write(&cli->tun, payload, payload_len);
        break;
    case MSG_PEER_LIST:     client_handle_peer_list(cli, payload, payload_len); break;
    case MSG_PEER_JOIN:     client_handle_peer_join(cli, payload, payload_len); break;
    case MSG_PEER_LEAVE:    client_handle_peer_leave(cli, payload, payload_len); break;
    case MSG_KEEPALIVE:     client_handle_keepalive(cli, payload, payload_len); break;
    case MSG_KEEPALIVE_ACK: client_handle_keepalive_ack(cli, payload, payload_len); break;
    case MSG_DISCONNECT:    LOG_WARN("Server disconnected!"); tund_request_stop(); break;
    default:                LOG_DEBUG("Unknown message type 0x%02X", type); break;
    }
}
