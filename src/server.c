#include "server.h"
#include "network.h"
#include "tun.h"
#include "tui.h"

static uint32_t server_alloc_ip(const server_t *srv)
{
    for (uint32_t ip_h = TUND_IP_START; ip_h <= TUND_IP_END; ip_h++) {
        uint32_t ip_n = htonl(ip_h);
        bool taken = false;
        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active && srv->peers[i].virt_ip == ip_n) {
                taken = true;
                break;
            }
        }
        if (!taken)
            return ip_n;
    }
    return 0;
}

static int server_find_peer_by_vip(const server_t *srv, uint32_t virt_ip)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && srv->peers[i].virt_ip == virt_ip)
            return i;
    }
    return -1;
}

static int server_find_peer_by_addr(const server_t *srv, const struct sockaddr_in *addr)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active &&
            srv->peers[i].real_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            srv->peers[i].real_addr.sin_port == addr->sin_port)
            return i;
    }
    return -1;
}

static int server_find_free_slot(const server_t *srv)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (!srv->peers[i].active)
            return i;
    }
    return -1;
}

static void server_broadcast(server_t *srv, uint8_t *buf, int len, int exclude_idx)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != exclude_idx) {
            if (net_send(srv->sockfd, buf, len, &srv->peers[i].real_addr) < 0)
                LOG_WARN("Broadcast to %s (%s) failed",
                         srv->peers[i].name, ip_to_str(srv->peers[i].virt_ip));
        }
    }
}

static void server_send_peer_list(server_t *srv, int peer_idx)
{
    uint8_t buf[TUND_MAX_PKT];
    uint8_t *payload = buf + TUND_HDR_SIZE;
    int offset = 0;
    int count = 0;

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != peer_idx) {
            if (offset + (int)sizeof(msg_peer_entry_t) > TUND_MAX_PAYLOAD)
                break; /* Too many peers for one packet */
            msg_peer_entry_t *entry = (msg_peer_entry_t *)(payload + offset);
            entry->virt_ip = srv->peers[i].virt_ip;
            memset(entry->name, 0, TUND_NAME_LEN);
            strncpy(entry->name, srv->peers[i].name, TUND_NAME_LEN - 1);
            entry->status = 1;
            offset += (int)sizeof(msg_peer_entry_t);
            count++;
        }
    }

    proto_write_hdr(buf, MSG_PEER_LIST, (uint16_t)offset);
    if (net_send(srv->sockfd, buf, TUND_HDR_SIZE + offset,
                 &srv->peers[peer_idx].real_addr) < 0)
        LOG_WARN("Failed to send peer list to %s", srv->peers[peer_idx].name);
    else
        LOG_DEBUG("Sent peer list (%d peers) to %s",
                  count, srv->peers[peer_idx].name);
}

static void server_handle_register(server_t *srv, const uint8_t *payload,
                                   uint16_t plen, const struct sockaddr_in *from)
{
    pthread_mutex_lock(&srv->peers_lock);

    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        srv->peers[idx].last_seen = time(NULL);
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_INFO("Peer %s reconnected (refreshed)", srv->peers[idx].name);

        uint8_t buf[TUND_MAX_PKT];
        int len = proto_build_assign(buf,
                                     srv->peers[idx].virt_ip,
                                     htonl(TUND_NETMASK),
                                     TUND_TUN_MTU);
        if (net_send(srv->sockfd, buf, len, from) < 0)
            LOG_WARN("Failed to send ASSIGN to reconnecting peer %s",
                     srv->peers[idx].name);
        return;
    }

    idx = server_find_free_slot(srv);
    if (idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN("Peer table full, rejecting connection from %s:%u",
                 inet_ntoa(from->sin_addr), ntohs(from->sin_port));
        return;
    }

    uint32_t vip = server_alloc_ip(srv);
    if (vip == 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN("IP pool exhausted, rejecting connection");
        return;
    }

    peer_t *p = &srv->peers[idx];
    p->active = true;
    p->virt_ip = vip;
    memset(p->name, 0, TUND_NAME_LEN);
    if (plen > 0) {
        int namelen = (plen < TUND_NAME_LEN - 1) ? plen : TUND_NAME_LEN - 1;
        memcpy(p->name, payload, (size_t)namelen);
    } else {
        snprintf(p->name, TUND_NAME_LEN, "peer-%d", idx);
    }
    memcpy(&p->real_addr, from, sizeof(*from));
    p->last_seen = time(NULL);
    srv->peer_count++;

    pthread_mutex_unlock(&srv->peers_lock);

    LOG_INFO("★ New peer: %s → %s [%s:%u]",
             p->name, ip_to_str(vip),
             inet_ntoa(from->sin_addr), ntohs(from->sin_port));

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_assign(buf, vip, htonl(TUND_NETMASK), TUND_TUN_MTU);
    if (net_send(srv->sockfd, buf, len, from) < 0)
        LOG_WARN("Failed to send ASSIGN to new peer %s", p->name);

    server_send_peer_list(srv, idx);

    len = proto_build_peer_join(buf, vip, p->name);
    server_broadcast(srv, buf, len, idx);
}

static void server_handle_data(server_t *srv, const uint8_t *payload,
                               uint16_t plen, const struct sockaddr_in *from)
{
    if (plen < 20) {
        LOG_DEBUG("DATA packet too short (%u bytes)", plen);
        return;
    }

    uint32_t dst_ip = proto_get_dst_ip(payload, plen);
    if (dst_ip == 0)
        return;

    pthread_mutex_lock(&srv->peers_lock);
    int sender_idx = server_find_peer_by_addr(srv, from);
    if (sender_idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_WARN("Dropped DATA from an unregistered endpoint");
        return;
    }

    uint32_t server_vip = htonl(TUND_SERVER_IP);
    if (dst_ip == server_vip) {
        pthread_mutex_unlock(&srv->peers_lock);
        tun_write(&srv->tun, payload, plen);
        return;
    }

    uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);
    if (dst_ip == broadcast_ip) {
        uint8_t buf[TUND_MAX_PKT];
        int len = proto_build_data(buf, payload, plen);
        server_broadcast(srv, buf, len, sender_idx);
        pthread_mutex_unlock(&srv->peers_lock);
        tun_write(&srv->tun, payload, plen);
        return;
    }

    int dst_idx = server_find_peer_by_vip(srv, dst_ip);
    if (dst_idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        LOG_DEBUG("No peer for %s, dropping packet", ip_to_str(dst_ip));
        return;
    }

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_data(buf, payload, plen);
    if (net_send(srv->sockfd, buf, len, &srv->peers[dst_idx].real_addr) < 0)
        LOG_WARN("Failed to forward data to %s", srv->peers[dst_idx].name);
    pthread_mutex_unlock(&srv->peers_lock);
}

static void server_handle_keepalive(server_t *srv, const struct sockaddr_in *from)
{
    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx >= 0) {
        srv->peers[idx].last_seen = time(NULL);
    }
    pthread_mutex_unlock(&srv->peers_lock);
    if (idx < 0) return;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_keepalive(buf, now_ms());
    if (net_send(srv->sockfd, buf, len, from) < 0 && idx >= 0)
        LOG_WARN("Keepalive reply to %s failed", srv->peers[idx].name);
}

static void server_handle_disconnect(server_t *srv, const struct sockaddr_in *from)
{
    pthread_mutex_lock(&srv->peers_lock);
    int idx = server_find_peer_by_addr(srv, from);
    if (idx < 0) {
        pthread_mutex_unlock(&srv->peers_lock);
        return;
    }

    peer_t *p = &srv->peers[idx];
    LOG_INFO("✦ Peer disconnected: %s (%s)", p->name, ip_to_str(p->virt_ip));

    uint32_t vip = p->virt_ip;
    p->active = false;
    srv->peer_count--;

    uint8_t buf[TUND_MAX_PKT];
    int len = proto_build_peer_leave(buf, vip);
    server_broadcast(srv, buf, len, -1);

    pthread_mutex_unlock(&srv->peers_lock);
}

static void *server_timeout_thread(void *arg)
{
    server_t *srv = (server_t *)arg;

    while (g_running && !srv->timeout_quit) {
        platform_sleep(TUND_KEEPALIVE_INTERVAL);
        if (!g_running || srv->timeout_quit)
            break;

        time_t now = time(NULL);
        pthread_mutex_lock(&srv->peers_lock);

        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active &&
                (now - srv->peers[i].last_seen) > TUND_PEER_TIMEOUT) {
                LOG_WARN("✦ Peer timed out: %s (%s)",
                         srv->peers[i].name, ip_to_str(srv->peers[i].virt_ip));

                uint32_t vip = srv->peers[i].virt_ip;
                srv->peers[i].active = false;
                srv->peer_count--;

                uint8_t buf[TUND_MAX_PKT];
                int len = proto_build_peer_leave(buf, vip);
                server_broadcast(srv, buf, len, -1);
            }
        }

        pthread_mutex_unlock(&srv->peers_lock);
    }
    return NULL;
}

static void *server_tun_thread(void *arg)
{
    server_t *srv = (server_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PAYLOAD];
    uint8_t msg_buf[TUND_MAX_PKT];

    while (g_running && !srv->tun_quit) {
        int n = tun_read(&srv->tun, pkt_buf, sizeof(pkt_buf));
        if (n <= 0) {
            if (!g_running || srv->tun_quit) break;
            continue;
        }

        uint32_t dst_ip = proto_get_dst_ip(pkt_buf, (uint16_t)n);
        if (dst_ip == 0)
            continue;

        uint32_t broadcast_ip = htonl(TUND_SUBNET | ~TUND_NETMASK);
        int msg_len = proto_build_data(msg_buf, pkt_buf, (uint16_t)n);

        pthread_mutex_lock(&srv->peers_lock);
        if (dst_ip == broadcast_ip) {
            server_broadcast(srv, msg_buf, msg_len, -1);
        } else {
            int idx = server_find_peer_by_vip(srv, dst_ip);
            if (idx >= 0) {
                if (net_send(srv->sockfd, msg_buf, msg_len, &srv->peers[idx].real_addr) < 0)
                    LOG_WARN("Failed to forward TUN packet to %s",
                             srv->peers[idx].name);
            }
        }
        pthread_mutex_unlock(&srv->peers_lock);
    }
    return NULL;
}

int server_init(server_t *srv, const config_t *cfg)
{
    memset(srv, 0, sizeof(*srv));
    srv->port = cfg->port;
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
    tun_set_mtu(&srv->tun, TUND_TUN_MTU);

    return 0;
}

void server_run(server_t *srv)
{
    char version_str[64];
    snprintf(version_str, sizeof(version_str), "Tund Server v%s", TUND_VERSION);
    char port_str[64];
    snprintf(port_str, sizeof(port_str), "Listening on UDP port %u", srv->port);
    char ip_str_full[64];
    snprintf(ip_str_full, sizeof(ip_str_full), "Virtual IP: %s", ip_to_str(htonl(TUND_SERVER_IP)));
    char subnet_str[] = "Subnet: 10.9.0.0/24";
    char tun_str[64];
    snprintf(tun_str, sizeof(tun_str), "TUN: %s", srv->tun.ifname);

    int max_len = (int)strlen(version_str);
    if ((int)strlen(port_str) > max_len) max_len = (int)strlen(port_str);
    if ((int)strlen(ip_str_full) > max_len) max_len = (int)strlen(ip_str_full);
    if ((int)strlen(subnet_str) > max_len) max_len = (int)strlen(subnet_str);
    if ((int)strlen(tun_str) > max_len) max_len = (int)strlen(tun_str);

    int inner = max_len + 4;
    char line[128];
    memset(line, '=', (size_t)inner);
    line[inner] = '\0';

    LOG_INFO("╔%s╗", line);
    LOG_INFO("║  %-*s║", inner - 2, version_str);
    LOG_INFO("║  %-*s║", inner - 2, port_str);
    LOG_INFO("║  %-*s║", inner - 2, ip_str_full);
    LOG_INFO("║  %-*s║", inner - 2, subnet_str);
    LOG_INFO("║  %-*s║", inner - 2, tun_str);
    LOG_INFO("╚%s╝", line);

    srv->timeout_quit = false;
    srv->tun_quit = false;

    if (pthread_create(&srv->timeout_tid, NULL, server_timeout_thread, srv) != 0) {
        LOG_ERROR("Failed to create timeout thread");
        g_running = 0;
        return;
    }
    srv->timeout_started = true;

    if (pthread_create(&srv->tun_tid, NULL, server_tun_thread, srv) != 0) {
        LOG_ERROR("Failed to create TUN thread");
        srv->timeout_quit = true;
        pthread_join(srv->timeout_tid, NULL);
        srv->timeout_started = false;
        g_running = 0;
        return;
    }
    srv->tun_started = true;

    if (g_tui_active)
        tui_init();

    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    tui_peer_t tui_peers[TUND_MAX_PEERS];
    int npeers;

    while (g_running) {
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
        if (n <= 0)
            continue;

        uint8_t type;
        uint16_t payload_len;
        if (proto_read_hdr(buf, &type, &payload_len) < 0) {
            LOG_DEBUG("Invalid magic from %s:%u",
                      inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            continue;
        }

        if (TUND_HDR_SIZE + payload_len > n) {
            LOG_DEBUG("Truncated packet from %s:%u",
                      inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            continue;
        }

        uint8_t *payload = buf + TUND_HDR_SIZE;

        switch (type) {
        case MSG_REGISTER:
            server_handle_register(srv, payload, payload_len, &from);
            break;
        case MSG_DATA:
            server_handle_data(srv, payload, payload_len, &from);
            break;
        case MSG_KEEPALIVE:
            server_handle_keepalive(srv, &from);
            break;
        case MSG_DISCONNECT:
            server_handle_disconnect(srv, &from);
            break;
        default:
            LOG_DEBUG("Unknown message type 0x%02X from %s:%u",
                      type, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            break;
        }
    }
}

void server_shutdown(server_t *srv)
{
    LOG_INFO("Shutting down server...");

    srv->timeout_quit = true;
    srv->tun_quit = true;

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
