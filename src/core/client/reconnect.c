#include "internal.h"
#include "log.h"
#include "network.h"

static void client_drain_server_packets(client_t *cli) {
    uint8_t buf[TUND_MAX_PKT];
    struct sockaddr_in from;
    for (int i = 0; i < 64 && platform_poll_one(cli->sockfd, 0) > 0; i++)
        (void)net_recv(cli->sockfd, buf, sizeof(buf), &from);
}

static void client_reset_remote_state(client_t *cli) {
    proto_replay_reset(&cli->server_replay);
    atomic_store_explicit(&cli->last_server_seen, 0, memory_order_relaxed);
    cli->server_rtt_ms = 0;
    cli->has_server_rtt = false;
    memset(&cli->peer_list, 0, sizeof(cli->peer_list));

    pthread_mutex_lock(&cli->peers_lock);
    memset(cli->peers, 0, sizeof(cli->peers));
    cli->peer_count = 0;
    pthread_mutex_unlock(&cli->peers_lock);
}

static bool client_wait_to_retry(unsigned int delay) {
    for (unsigned int elapsed = 0; elapsed < delay; elapsed++) {
        if (!tund_is_running()) return false;
        platform_sleep(1);
    }
    return tund_is_running();
}

int client_reconnect(client_t *cli) {
    unsigned int delay = TUND_RECONNECT_INITIAL_DELAY;
    client_set_connected(cli, false);
    client_tunnel_stop(cli);

    while (tund_is_running()) {
        client_drain_server_packets(cli);
        client_reset_remote_state(cli);
        LOG_INFO("Attempting to reconnect to the server...");

        client_registration_result_t result = client_register(cli, 1);
        if (result == CLIENT_REGISTER_OK && tund_is_running() && client_tunnel_open(cli) == 0 &&
            client_tunnel_start(cli) == 0) {
            client_mark_server_seen(cli);
            client_set_connected(cli, true);
            LOG_INFO("Server connection restored.");
            return 0;
        }
        client_tunnel_stop(cli);
        if (!tund_is_running()) break;

        LOG_WARN("Reconnect failed; retrying in %u second(s).", delay);
        if (!client_wait_to_retry(delay)) break;
        if (delay < TUND_RECONNECT_MAX_DELAY) {
            delay *= 2;
            if (delay > TUND_RECONNECT_MAX_DELAY) delay = TUND_RECONNECT_MAX_DELAY;
        }
    }
    return -1;
}
