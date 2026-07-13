#include "client.h"
#include "network.h"
#include "tun.h"
#include "tui.h"

static void client_update_peer(client_t *cli, uint32_t vip, const char *name, bool online)
{
    pthread_mutex_lock(&cli->peers_lock);

    int idx = -1;
    int free_idx = -1;
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active && cli->peers[i].virt_ip == vip) {
            idx = i;
            break;
        }
        if (!cli->peers[i].active && free_idx < 0)
            free_idx = i;
    }

    if (online) {
        if (idx < 0) {
            if (free_idx < 0) {
                pthread_mutex_unlock(&cli->peers_lock);
                return;
            }
            idx = free_idx;
            cli->peer_count++;
        }
        cli->peers[idx].active = true;
        cli->peers[idx].virt_ip = vip;
        memset(cli->peers[idx].name, 0, TUND_NAME_LEN);
        if (name) strncpy(cli->peers[idx].name, name, TUND_NAME_LEN - 1);
        cli->peers[idx].last_seen = time(NULL);
    } else {
        if (idx >= 0) {
            cli->peers[idx].active = false;
            cli->peer_count--;
        }
    }

    pthread_mutex_unlock(&cli->peers_lock);
}

static void client_print_peers(client_t *cli)
{
    pthread_mutex_lock(&cli->peers_lock);

    LOG_INFO("┌─────────────────────────────────────────┐");
    LOG_INFO("│         Connected Peers (%d)              │", cli->peer_count);
    LOG_INFO("├──────────────┬──────────────────────────┤");

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active) {
            LOG_INFO("│ %-12s │ %-24s │",
                     ip_to_str(cli->peers[i].virt_ip),
                     cli->peers[i].name);
        }
    }

    LOG_INFO("└──────────────┴──────────────────────────┘");
    pthread_mutex_unlock(&cli->peers_lock);
}

static int client_register(client_t *cli)
{
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;

    for (int attempt = 0; attempt < TUND_REGISTER_RETRIES; attempt++) {
        LOG_INFO("Registering with server (attempt %d/%d)...",
                 attempt + 1, TUND_REGISTER_RETRIES);

        int len = proto_build_register(buf, cli->name);
        if (net_send(cli->sockfd, buf, len, &cli->server_addr) < 0)
            return -1;

        int ret = platform_poll_one(cli->sockfd, TUND_REGISTER_TIMEOUT * 1000);
        if (ret < 0) {
            LOG_ERROR("poll() failed: %s", strerror(errno));
            return -1;
        }
        if (ret == 0) {
            LOG_WARN("No response from server, retrying...");
            continue;
        }

        int n = net_recv(cli->sockfd, buf, sizeof(buf), &from);
        if (n <= 0)
            continue;
        if (!net_addr_equal(&from, &cli->server_addr))
            continue;

        uint8_t type;
        uint16_t payload_len;
        if (proto_read_hdr(buf, &type, &payload_len) < 0)
            continue;

        if (type == MSG_ASSIGN && payload_len >= 10) {
            uint8_t *p = buf + TUND_HDR_SIZE;
            memcpy(&cli->virt_ip, p, 4);    p += 4;
            memcpy(&cli->netmask, p, 4);    p += 4;
            uint16_t mtu_n;
            memcpy(&mtu_n, p, 2);
            int mtu = ntohs(mtu_n);

            LOG_INFO("★ Registered! Virtual IP: %s (MTU: %d)",
                     ip_to_str(cli->virt_ip), mtu);
            return 0;
        }
    }

    LOG_ERROR("Registration failed after %d attempts", TUND_REGISTER_RETRIES);
    return -1;
}

static void *client_keepalive_thread(void *arg)
{
    client_t *cli = (client_t *)arg;
    uint8_t buf[TUND_MAX_PKT];
    unsigned int slept = 0;

    while (g_running && !cli->ka_quit) {
        platform_sleep(1);
        slept++;
        if (slept < TUND_KEEPALIVE_INTERVAL)
            continue;
        slept = 0;
        if (!g_running || cli->ka_quit) break;

        int len = proto_build_keepalive(buf, now_ms());
        net_send(cli->sockfd, buf, len, &cli->server_addr);
    }
    return NULL;
}

static void *client_tun_thread(void *arg)
{
    client_t *cli = (client_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PAYLOAD];
    uint8_t msg_buf[TUND_MAX_PKT];

    while (g_running && !cli->tun_quit) {
        int n = tun_read(&cli->tun, pkt_buf, sizeof(pkt_buf));
        if (n <= 0) {
            if (!g_running || cli->tun_quit) break;
            continue;
        }
        int msg_len = proto_build_data(msg_buf, pkt_buf, (uint16_t)n);
        net_send(cli->sockfd, msg_buf, msg_len, &cli->server_addr);
    }
    return NULL;
}

static void client_handle_peer_list(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    int entry_size = (int)sizeof(msg_peer_entry_t);
    int count = plen / entry_size;

    LOG_INFO("Received peer list: %d peer(s)", count);

    for (int i = 0; i < count; i++) {
        const msg_peer_entry_t *entry =
            (const msg_peer_entry_t *)(payload + i * entry_size);
        client_update_peer(cli, entry->virt_ip, entry->name, entry->status != 0);
    }

    if (count > 0)
        client_print_peers(cli);
}

static void client_handle_peer_join(client_t *cli, const uint8_t *payload, uint16_t plen)
{
    if (plen < 4 + TUND_NAME_LEN) return;

    uint32_t vip;
    memcpy(&vip, payload, 4);
    char name[TUND_NAME_LEN];
    memset(name, 0, sizeof(name));
    memcpy(name, payload + 4, TUND_NAME_LEN);

    LOG_INFO("★ Peer joined: %s (%s)", name, ip_to_str(vip));
    client_update_peer(cli, vip, name, true);
    client_print_peers(cli);
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

    LOG_INFO("✦ Peer left: %s (%s)", name, ip_to_str(vip));

    if (remaining > 0)
        client_print_peers(cli);
    else
        LOG_INFO("No peers connected.");
}

int client_init(client_t *cli, const config_t *cfg)
{
    memset(cli, 0, sizeof(*cli));
    cli->tun.fd = -1;
    snprintf(cli->name, TUND_NAME_LEN, "%s", cfg->client_name);
    pthread_mutex_init(&cli->peers_lock, NULL);

    if (net_resolve(cfg->server_ip, cfg->port, &cli->server_addr) < 0)
        return -1;

    cli->sockfd = net_create_socket(0);
    if (!sock_valid(cli->sockfd))
        return -1;

    if (client_register(cli) < 0) {
        sock_close(cli->sockfd);
        return -1;
    }

    if (tun_open(&cli->tun) < 0) {
        sock_close(cli->sockfd);
        return -1;
    }

    if (tun_set_ip(&cli->tun, cli->virt_ip, cli->netmask) < 0) {
        tun_close(&cli->tun);
        sock_close(cli->sockfd);
        return -1;
    }
    tun_set_mtu(&cli->tun, TUND_TUN_MTU);

    return 0;
}

void client_run(client_t *cli)
{
    char version_str[64];
    snprintf(version_str, sizeof(version_str), "Tund Client v%s", TUND_VERSION);
    char ip_str[64];
    snprintf(ip_str, sizeof(ip_str), "Virtual IP: %s", ip_to_str(cli->virt_ip));
    char server_str[64];
    snprintf(server_str, sizeof(server_str), "Server: %s:%u",
             inet_ntoa(cli->server_addr.sin_addr),
             ntohs(cli->server_addr.sin_port));
    char tun_str[64];
    snprintf(tun_str, sizeof(tun_str), "TUN: %s", cli->tun.ifname);
    char name_str[64];
    snprintf(name_str, sizeof(name_str), "Name: %s", cli->name);

    int max_len = (int)strlen(version_str);
    if ((int)strlen(ip_str) > max_len) max_len = (int)strlen(ip_str);
    if ((int)strlen(server_str) > max_len) max_len = (int)strlen(server_str);
    if ((int)strlen(tun_str) > max_len) max_len = (int)strlen(tun_str);
    if ((int)strlen(name_str) > max_len) max_len = (int)strlen(name_str);

    int inner = max_len + 4;
    char line[128];
    memset(line, '=', (size_t)inner);
    line[inner] = '\0';

    LOG_INFO("╔%s╗", line);
    LOG_INFO("║  %-*s║", inner - 2, version_str);
    LOG_INFO("║  %-*s║", inner - 2, ip_str);
    LOG_INFO("║  %-*s║", inner - 2, server_str);
    LOG_INFO("║  %-*s║", inner - 2, tun_str);
    LOG_INFO("║  %-*s║", inner - 2, name_str);
    LOG_INFO("╚%s╝", line);
    LOG_INFO("Press Ctrl+C to disconnect.");

    cli->ka_quit = false;
    cli->tun_quit = false;

    if (pthread_create(&cli->ka_tid, NULL, client_keepalive_thread, cli) != 0) {
        LOG_ERROR("Failed to create keepalive thread");
        g_running = 0;
        return;
    }
    cli->ka_started = true;

    if (pthread_create(&cli->tun_tid, NULL, client_tun_thread, cli) != 0) {
        LOG_ERROR("Failed to create TUN thread");
        cli->ka_quit = true;
        pthread_join(cli->ka_tid, NULL);
        cli->ka_started = false;
        g_running = 0;
        return;
    }
    cli->tun_started = true;

    tui_peer_t tui_peers[TUND_MAX_PEERS];
    if (g_tui_active)
        tui_init();

    uint8_t buf[TUND_MAX_PKT];
    char server_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli->server_addr.sin_addr, server_ip_str, sizeof(server_ip_str));
    int npeers;

    while (g_running) {
        int ret = net_poll_peers(cli->sockfd, cli->peers, TUND_MAX_PEERS,
                                 &cli->peers_lock, tui_peers, &npeers);
        if (ret < 0)
            break;
        if (ret == 0) {
            if (g_tui_active)
                tui_render_client(server_ip_str, ntohs(cli->server_addr.sin_port),
                                  cli->tun.ifname, cli->virt_ip, cli->netmask,
                                  g_start_time, npeers, tui_peers, npeers);
            continue;
        }

        struct sockaddr_in from;
        int n = net_recv(cli->sockfd, buf, sizeof(buf), &from);
        if (n <= 0)
            continue;
        if (!net_addr_equal(&from, &cli->server_addr))
            continue;

        uint8_t type;
        uint16_t payload_len;
        if (proto_read_hdr(buf, &type, &payload_len) < 0)
            continue;

        if (TUND_HDR_SIZE + payload_len > n)
            continue;

        uint8_t *payload = buf + TUND_HDR_SIZE;

        switch (type) {
        case MSG_DATA:
            tun_write(&cli->tun, payload, payload_len);
            break;
        case MSG_PEER_LIST:
            client_handle_peer_list(cli, payload, payload_len);
            break;
        case MSG_PEER_JOIN:
            client_handle_peer_join(cli, payload, payload_len);
            break;
        case MSG_PEER_LEAVE:
            client_handle_peer_leave(cli, payload, payload_len);
            break;
        case MSG_KEEPALIVE:
            break;
        case MSG_DISCONNECT:
            LOG_WARN("Server disconnected!");
            g_running = 0;
            break;
        default:
            LOG_DEBUG("Unknown message type 0x%02X", type);
            break;
        }
    }
}

void client_shutdown(client_t *cli)
{
    LOG_INFO("Disconnecting...");

    cli->ka_quit = true;
    cli->tun_quit = true;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_disconnect(buf);
    net_send(cli->sockfd, buf, len, &cli->server_addr);

    tun_close(&cli->tun);

    if (cli->ka_started) pthread_join(cli->ka_tid, NULL);
    if (cli->tun_started) pthread_join(cli->tun_tid, NULL);

    if (g_tui_active)
        tui_shutdown();
    if (sock_valid(cli->sockfd)) {
        sock_close(cli->sockfd);
        cli->sockfd = SOCK_INVALID;
    }
    pthread_mutex_destroy(&cli->peers_lock);

    LOG_INFO("Disconnected cleanly.");
}
