#include "internal.h"

void client_mark_server_seen(client_t *cli) {
    time_t now = time(NULL);
    if (now > 0)
        atomic_store_explicit(&cli->last_server_seen, (uint_fast64_t)now, memory_order_relaxed);
}

bool client_server_timed_out(const client_t *cli, time_t now) {
    uint_fast64_t last = atomic_load_explicit(&cli->last_server_seen, memory_order_relaxed);
    if (last == 0 || now <= 0) return false;

    uint_fast64_t current = (uint_fast64_t)now;
    return current > last && current - last > TUND_SERVER_TIMEOUT;
}

bool client_is_connected(client_t *cli) {
    return tund_stop_flag_load(&cli->connected);
}

void client_set_connected(client_t *cli, bool connected) {
    tund_stop_flag_store(&cli->connected, connected);
}
