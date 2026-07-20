#include "internal.h"

static void *client_keepalive_thread(void *arg)
{
    client_t *cli = (client_t *)arg;
    uint8_t buf[TUND_MAX_PKT];
    unsigned int slept = 0;

    while (tund_is_running() && !tund_stop_flag_load(&cli->ka_quit)) {
        platform_sleep(1);
        slept++;
        if (slept < TUND_KEEPALIVE_INTERVAL)
            continue;
        slept = 0;
        if (!tund_is_running() || tund_stop_flag_load(&cli->ka_quit)) break;

        if (client_server_timed_out(cli, time(NULL))) {
            LOG_WARN("Server timed out after %d seconds without a response.",
                     TUND_SERVER_TIMEOUT);
            tund_request_stop();
            break;
        }

        int len = proto_build_keepalive(buf, now_ms());
        if (net_send(cli->sockfd, buf, len, &cli->server_addr) < 0)
            LOG_WARN("Keepalive send failed");
    }
    return NULL;
}

static void *client_tun_thread(void *arg)
{
    client_t *cli = (client_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PAYLOAD];
    uint8_t msg_buf[TUND_MAX_PKT];

    while (tund_is_running() && !tund_stop_flag_load(&cli->tun_quit)) {
        int n = tun_read(&cli->tun, pkt_buf, sizeof(pkt_buf));
        if (n <= 0) {
            if (!tund_is_running() || tund_stop_flag_load(&cli->tun_quit)) break;
            continue;
        }
        int msg_len = proto_build_data(msg_buf, pkt_buf, (uint16_t)n);
        if (net_send(cli->sockfd, msg_buf, msg_len, &cli->server_addr) < 0) {
            LOG_WARN("Failed to send TUN data to server");
        } else {
            uint32_t dst_ip = proto_get_dst_ip(pkt_buf, (uint16_t)n);
            uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);
            if (dst_ip == broadcast_ip)
                client_add_broadcast_traffic(cli, (uint64_t)n);
            else
                client_add_peer_traffic(cli, dst_ip, 0, (uint64_t)n);
        }
    }
    return NULL;
}

int client_init(client_t *cli, const config_t *cfg)
{
    cli->sockfd = SOCK_INVALID;
    memset(&cli->tun, 0, sizeof(cli->tun));
    memset(&cli->server_addr, 0, sizeof(cli->server_addr));
    cli->virt_ip = 0;
    cli->netmask = 0;
    cli->server_rtt_ms = 0;
    cli->has_server_rtt = false;
    atomic_init(&cli->last_server_seen, 0);
    memset(cli->name, 0, sizeof(cli->name));
    memset(cli->peers, 0, sizeof(cli->peers));
    cli->peer_count = 0;
    cli->ka_started = false;
    cli->tun_started = false;
    tund_stop_flag_init(&cli->ka_quit, false);
    tund_stop_flag_init(&cli->tun_quit, false);
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
    client_mark_server_seen(cli);

    if (tun_open(&cli->tun) < 0) {
        sock_close(cli->sockfd);
        return -1;
    }

    if (tun_set_ip(&cli->tun, cli->virt_ip, cli->netmask) < 0) {
        tun_close(&cli->tun);
        sock_close(cli->sockfd);
        return -1;
    }
    if (tun_set_mtu(&cli->tun, TUND_TUN_MTU) < 0) {
        tun_close(&cli->tun);
        sock_close(cli->sockfd);
        return -1;
    }

    return 0;
}

void client_run(client_t *cli)
{
    client_log_banner(cli);
    tund_stop_flag_store(&cli->ka_quit, false);
    tund_stop_flag_store(&cli->tun_quit, false);

    if (pthread_create(&cli->ka_tid, NULL, client_keepalive_thread, cli) != 0) {
        LOG_ERROR("Failed to create keepalive thread");
        tund_request_stop();
        return;
    }
    cli->ka_started = true;

    if (pthread_create(&cli->tun_tid, NULL, client_tun_thread, cli) != 0) {
        LOG_ERROR("Failed to create TUN thread");
        tund_stop_flag_store(&cli->ka_quit, true);
        pthread_join(cli->ka_tid, NULL);
        cli->ka_started = false;
        tund_request_stop();
        return;
    }
    cli->tun_started = true;

    tui_peer_t tui_peers[TUND_MAX_PEERS];
    if (g_tui_active)
        tui_init();

    client_log_startup_checklist(cli);

    uint8_t buf[TUND_MAX_PKT];
    char server_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli->server_addr.sin_addr, server_ip_str, sizeof(server_ip_str));
    int npeers;

    while (tund_is_running()) {
        int ret = net_poll_peers(cli->sockfd, cli->peers, TUND_MAX_PEERS,
                                 &cli->peers_lock, tui_peers, &npeers);
        if (ret < 0)
            break;
        if (ret == 0) {
            if (g_tui_active)
                tui_render_client(server_ip_str, ntohs(cli->server_addr.sin_port),
                                  cli->tun.ifname, cli->virt_ip, cli->netmask,
                                  cli->has_server_rtt, cli->server_rtt_ms,
                                  g_start_time, npeers, tui_peers, npeers);
            continue;
        }

        struct sockaddr_in from;
        int n = net_recv(cli->sockfd, buf, sizeof(buf), &from);
        if (n <= 0 || !net_addr_equal(&from, &cli->server_addr))
            continue;
        client_handle_server_packet(cli, buf, n);
    }
}

void client_shutdown(client_t *cli)
{
    LOG_INFO("Disconnecting...");
    tund_stop_flag_store(&cli->ka_quit, true);
    tund_stop_flag_store(&cli->tun_quit, true);

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
