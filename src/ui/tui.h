#ifndef TUND_TUI_H
#define TUND_TUI_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    uint32_t virt_ip;
    char name[32];
    time_t last_seen;
    uint64_t bytes_in;
    uint64_t bytes_out;
    uint64_t rtt_ms;
    bool has_rtt;
    bool active;
} tui_peer_t;

void tui_init(void);
void tui_shutdown(void);
bool tui_events_active(void);
void tui_add_event(int level, const char *message);
void tui_render_server(uint16_t port, const char *tun_name, uint32_t virt_ip, uint32_t netmask,
                       time_t start_time, int peer_count, const tui_peer_t *peers, int npeers);
void tui_render_client(const char *server_addr, uint16_t port, const char *tun_name,
                       uint32_t virt_ip, uint32_t netmask, bool has_server_rtt,
                       uint64_t server_rtt_ms, time_t start_time, int peer_count,
                       const tui_peer_t *peers, int npeers);

#endif
