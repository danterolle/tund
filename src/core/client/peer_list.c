#include "internal.h"
#include "log.h"

_Static_assert(TUND_PEER_LIST_MAX_PAGES <= 64, "peer list page bitmap is too small");

static bool peer_list_metadata_matches(const client_peer_list_state_t *state,
                                       const proto_peer_list_page_t *page) {
    return state->snapshot_id == page->snapshot_id && state->page_count == page->page_count &&
           state->total_entries == page->total_entries;
}

static bool snapshot_is_newer(uint32_t incoming, uint32_t current) {
    return incoming != current && incoming - current <= UINT32_MAX / 2;
}

static void start_peer_list_snapshot(client_t *cli, const proto_peer_list_page_t *page) {
    cli->peer_list.initialized = true;
    cli->peer_list.snapshot_id = page->snapshot_id;
    cli->peer_list.page_count = page->page_count;
    cli->peer_list.total_entries = page->total_entries;
    cli->peer_list.received_entries = 0;
    cli->peer_list.received_pages = 0;
}

void client_handle_peer_list_page(client_t *cli, const uint8_t *payload, uint16_t payload_len) {
    proto_peer_list_page_t page;
    if (!proto_read_peer_list_page(payload, payload_len, &page)) {
        LOG_DEBUG("Dropped invalid peer list page");
        return;
    }

    client_peer_list_state_t *state = &cli->peer_list;
    if (!state->initialized || snapshot_is_newer(page.snapshot_id, state->snapshot_id)) {
        start_peer_list_snapshot(cli, &page);
    } else if (!peer_list_metadata_matches(state, &page)) {
        LOG_DEBUG("Dropped stale or inconsistent peer list page");
        return;
    }

    uint64_t page_bit = 1ULL << page.page_index;
    if ((state->received_pages & page_bit) != 0) return;

    uint16_t entries_len = (uint16_t)(page.entry_count * TUND_PEER_ENTRY_SIZE);
    for (uint16_t i = 0; i < page.entry_count; i++) {
        uint32_t virt_ip = 0;
        char name[TUND_NAME_LEN];
        bool online = false;
        if (!proto_read_peer_entry(page.entries, entries_len, i, &virt_ip, name, &online)) return;
        client_update_peer(cli, virt_ip, name, online);
    }

    state->received_pages |= page_bit;
    state->received_entries = (uint16_t)(state->received_entries + page.entry_count);

    uint64_t complete_mask = (1ULL << state->page_count) - 1;
    if (state->received_pages == complete_mask && state->received_entries == state->total_entries) {
        LOG_INFO("Received peer list: %u peer(s) in %u page(s)", state->total_entries,
                 state->page_count);
        if (!g_tui_active && state->total_entries > 0) client_log_peers(cli);
    }
}
