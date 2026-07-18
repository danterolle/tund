#ifndef TUND_SERVER_H
#define TUND_SERVER_H

#include "tund.h"
#include "tun.h"

typedef struct {
    socket_t        sockfd;                     /* UDP socket */
    tun_device_t    tun;                        /* Server's TUN interface */
    peer_t          peers[TUND_MAX_PEERS];      /* Peer table */
    int             peer_count;                 /* Active peers */
    uint16_t        port;                       /* Listen port */
    pthread_mutex_t peers_lock;                 /* Thread safety for peer table */
    pthread_t       timeout_tid;                /* Timeout checker thread handle */
    pthread_t       tun_tid;                    /* TUN reader thread handle */
    bool            timeout_started;
    bool            tun_started;
    tund_stop_flag_t timeout_quit;              /* Signal timeout thread to stop */
    tund_stop_flag_t tun_quit;                  /* Signal TUN thread to stop */
} server_t;

int server_init(server_t *srv, const config_t *cfg);
void server_run(server_t *srv);
void server_shutdown(server_t *srv);

#endif /* TUND_SERVER_H */
