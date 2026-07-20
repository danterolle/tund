#ifndef TUND_CLIENT_H
#define TUND_CLIENT_H

#include "tund.h"
#include "tun.h"

typedef struct {
    socket_t sockfd;                /* UDP socket */
    tun_device_t tun;               /* Virtual interface */
    struct sockaddr_in server_addr; /* Server's real UDP address */
    uint32_t virt_ip;               /* Our assigned virtual IP (nbo) */
    uint32_t netmask;               /* Subnet mask (nbo) */
    uint64_t server_rtt_ms;
    bool has_server_rtt;
    atomic_uint_fast64_t last_server_seen;
    proto_replay_window_t server_replay;
    char name[TUND_NAME_LEN];     /* Display name */
    peer_t peers[TUND_MAX_PEERS]; /* Known peers */
    int peer_count;               /* Active peer count */
    pthread_mutex_t peers_lock;   /* Thread safety */
    pthread_t ka_tid;             /* Keepalive thread handle */
    pthread_t tun_tid;            /* TUN reader thread handle */
    bool ka_started;
    bool tun_started;
    tund_stop_flag_t ka_quit;  /* Signal keepalive thread to stop */
    tund_stop_flag_t tun_quit; /* Signal TUN reader thread to stop */
} client_t;

int client_init(client_t *cli, const config_t *cfg);
void client_run(client_t *cli);
void client_shutdown(client_t *cli);

#endif /* TUND_CLIENT_H */
