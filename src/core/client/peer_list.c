#include "internal.h"
#include "log.h"

void client_handle_peer_list(client_t *cli, const uint8_t *payload, uint16_t payload_len) {
    int count = proto_peer_entry_count(payload_len);
    if (!g_tui_active) LOG_INFO("Received peer list: %d peer(s)", count);

    for (int i = 0; i < count; i++) {
        uint32_t virt_ip = 0;
        char name[TUND_NAME_LEN];
        bool online = false;
        if (!proto_read_peer_entry(payload, payload_len, i, &virt_ip, name, &online)) break;
        client_update_peer(cli, virt_ip, name, online);
    }
    if (!g_tui_active && count > 0) client_log_peers(cli);
}
