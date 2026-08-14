#include "internal.h"
#include "log.h"
#include "network.h"
#include "tun.h"

static void *client_tun_thread(void *arg) {
    client_t *cli = (client_t *)arg;
    uint8_t pkt_buf[TUND_MAX_PLAINTEXT];
    uint8_t msg_buf[TUND_MAX_PKT];

    while (tund_is_running() && !tund_stop_flag_load(&cli->tun_quit)) {
        int n = tun_read(&cli->tun, pkt_buf, sizeof(pkt_buf));
        if (n <= 0) {
            if (!tund_is_running() || tund_stop_flag_load(&cli->tun_quit)) break;
            continue;
        }
        if (!client_is_connected(cli)) continue;

        int validation = proto_validate_ipv4_packet(pkt_buf, (uint16_t)n);
        if (validation != TUND_IPV4_OK) {
            LOG_DEBUG("Dropped invalid TUN packet: %s", proto_ipv4_validation_error(validation));
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

int client_tunnel_open(client_t *cli) {
    memset(&cli->tun, 0, sizeof(cli->tun));
    cli->tun.fd = -1;
    if (tun_open(&cli->tun) < 0) return -1;
    if (tun_set_ip(&cli->tun, cli->virt_ip, cli->netmask) < 0 ||
        tun_set_mtu(&cli->tun, TUND_TUN_MTU) < 0) {
        tun_close(&cli->tun);
        return -1;
    }
    return 0;
}

int client_tunnel_start(client_t *cli) {
    tund_stop_flag_store(&cli->tun_quit, false);
    if (pthread_create(&cli->tun_tid, NULL, client_tun_thread, cli) != 0) {
        LOG_ERROR("Failed to create TUN thread");
        return -1;
    }
    cli->tun_started = true;
    return 0;
}

void client_tunnel_stop(client_t *cli) {
    tund_stop_flag_store(&cli->tun_quit, true);
    tun_close(&cli->tun);
    if (cli->tun_started) {
        pthread_join(cli->tun_tid, NULL);
        cli->tun_started = false;
    }
}
