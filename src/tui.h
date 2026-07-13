#ifndef TUND_TUI_H
#define TUND_TUI_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    uint32_t virt_ip;
    char     name[32];
    time_t   last_seen;
    bool     active;
} tui_peer_t;

void tui_init(void);
void tui_shutdown(void);
void tui_render_server(uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers);
void tui_render_client(const char *server_addr, uint16_t port,
                       const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       time_t start_time,
                       int peer_count, const tui_peer_t *peers, int npeers);

#endif
