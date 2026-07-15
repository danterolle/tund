#include "client_internal.h"

void client_update_peer(client_t *cli, uint32_t vip, const char *name, bool online)
{
    pthread_mutex_lock(&cli->peers_lock);
    int idx = -1, free_idx = -1;
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active && cli->peers[i].virt_ip == vip) { idx = i; break; }
        if (!cli->peers[i].active && free_idx < 0) free_idx = i;
    }

    if (online) {
        if (idx < 0) {
            if (free_idx < 0) { pthread_mutex_unlock(&cli->peers_lock); return; }
            idx = free_idx;
            cli->peers[idx].bytes_in = 0;
            cli->peers[idx].bytes_out = 0;
            cli->peer_count++;
        }
        cli->peers[idx].active = true;
        cli->peers[idx].virt_ip = vip;
        memset(cli->peers[idx].name, 0, TUND_NAME_LEN);
        if (name) strncpy(cli->peers[idx].name, name, TUND_NAME_LEN - 1);
        cli->peers[idx].last_seen = time(NULL);
    } else if (idx >= 0) {
        cli->peers[idx].active = false;
        cli->peer_count--;
    }
    pthread_mutex_unlock(&cli->peers_lock);
}

void client_add_peer_traffic(client_t *cli, uint32_t vip,
                             uint64_t bytes_in, uint64_t bytes_out)
{
    pthread_mutex_lock(&cli->peers_lock);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active && cli->peers[i].virt_ip == vip) {
            cli->peers[i].bytes_in += bytes_in;
            cli->peers[i].bytes_out += bytes_out;
            break;
        }
    }
    pthread_mutex_unlock(&cli->peers_lock);
}

void client_add_broadcast_traffic(client_t *cli, uint64_t bytes_out)
{
    pthread_mutex_lock(&cli->peers_lock);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active)
            cli->peers[i].bytes_out += bytes_out;
    }
    pthread_mutex_unlock(&cli->peers_lock);
}

void client_log_peers(client_t *cli)
{
    pthread_mutex_lock(&cli->peers_lock);
    LOG_INFO("┌─────────────────────────────────────────┐");
    LOG_INFO("│         Connected Peers (%d)              │", cli->peer_count);
    LOG_INFO("├──────────────┬──────────────────────────┤");

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (cli->peers[i].active) {
            char peer_ip[TUND_IP_STR_LEN];
            LOG_INFO("│ %-12s │ %-24s │",
                     ip_to_str_buf(cli->peers[i].virt_ip, peer_ip, sizeof(peer_ip)),
                     cli->peers[i].name);
        }
    }
    LOG_INFO("└──────────────┴──────────────────────────┘");
    pthread_mutex_unlock(&cli->peers_lock);
}
