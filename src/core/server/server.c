#include "internal.h"

int server_init(server_t *srv, const config_t *cfg)
{
    srv->sockfd = SOCK_INVALID;
    memset(&srv->tun, 0, sizeof(srv->tun));
    memset(srv->peers, 0, sizeof(srv->peers));
    srv->peer_count = 0;
    srv->port = cfg->port;
    srv->timeout_started = false;
    srv->tun_started = false;
    tund_stop_flag_init(&srv->timeout_quit, false);
    tund_stop_flag_init(&srv->tun_quit, false);
    srv->tun.fd = -1;
    pthread_mutex_init(&srv->peers_lock, NULL);

    srv->sockfd = net_create_socket(cfg->port);
    if (!sock_valid(srv->sockfd))
        return -1;

    if (tun_open(&srv->tun) < 0) {
        sock_close(srv->sockfd);
        return -1;
    }

    uint32_t server_ip = htonl(TUND_SERVER_IP);
    uint32_t netmask = htonl(TUND_NETMASK);
    if (tun_set_ip(&srv->tun, server_ip, netmask) < 0) {
        tun_close(&srv->tun);
        sock_close(srv->sockfd);
        return -1;
    }
    if (tun_set_mtu(&srv->tun, TUND_TUN_MTU) < 0) {
        tun_close(&srv->tun);
        sock_close(srv->sockfd);
        return -1;
    }

    return 0;
}

void server_run(server_t *srv)
{
    server_log_banner(srv);
    tund_stop_flag_store(&srv->timeout_quit, false);
    tund_stop_flag_store(&srv->tun_quit, false);

    if (pthread_create(&srv->timeout_tid, NULL, server_timeout_thread, srv) != 0) {
        LOG_ERROR("Failed to create timeout thread");
        tund_request_stop();
        return;
    }
    srv->timeout_started = true;

    if (pthread_create(&srv->tun_tid, NULL, server_tun_thread, srv) != 0) {
        LOG_ERROR("Failed to create TUN thread");
        tund_stop_flag_store(&srv->timeout_quit, true);
        pthread_join(srv->timeout_tid, NULL);
        srv->timeout_started = false;
        tund_request_stop();
        return;
    }
    srv->tun_started = true;

    if (g_tui_active)
        tui_init();

    server_log_startup_checklist(srv);

    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    tui_peer_t tui_peers[TUND_MAX_PEERS];
    int npeers;

    while (tund_is_running()) {
        int ret = net_poll_peers(srv->sockfd, srv->peers, TUND_MAX_PEERS,
                                 &srv->peers_lock, tui_peers, &npeers);
        if (ret < 0)
            break;
        if (ret == 0) {
            if (g_tui_active)
                tui_render_server(srv->port, srv->tun.ifname,
                                  htonl(TUND_SERVER_IP), htonl(TUND_NETMASK),
                                  g_start_time, npeers, tui_peers, npeers);
            continue;
        }

        int n = net_recv(srv->sockfd, buf, sizeof(buf), &from);
        if (n > 0)
            server_handle_packet(srv, buf, n, &from);
    }
}

void server_shutdown(server_t *srv)
{
    LOG_INFO("Shutting down server...");
    tund_stop_flag_store(&srv->timeout_quit, true);
    tund_stop_flag_store(&srv->tun_quit, true);

    pthread_mutex_lock(&srv->peers_lock);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active) {
            uint8_t buf[TUND_MAX_PKT];
            int len = proto_build_disconnect(buf);
            net_send(srv->sockfd, buf, len, &srv->peers[i].real_addr);
        }
    }
    pthread_mutex_unlock(&srv->peers_lock);

    tun_close(&srv->tun);

    if (srv->timeout_started) pthread_join(srv->timeout_tid, NULL);
    if (srv->tun_started) pthread_join(srv->tun_tid, NULL);

    if (g_tui_active)
        tui_shutdown();
    if (sock_valid(srv->sockfd)) {
        sock_close(srv->sockfd);
        srv->sockfd = SOCK_INVALID;
    }
    pthread_mutex_destroy(&srv->peers_lock);
    LOG_INFO("Server shut down cleanly.");
}
