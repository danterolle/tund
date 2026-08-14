#include "protocol.h"

static void write_u16_be(uint8_t *buf, uint16_t value) {
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);
}

static uint16_t read_u16_be(const uint8_t *buf) {
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

static void write_u32_be(uint8_t *buf, uint32_t value) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value & 0xFF);
}

static uint32_t read_u32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

uint16_t proto_peer_list_page_count(uint16_t total_entries) {
    if (total_entries > TUND_MAX_PEERS) return 0;
    if (total_entries == 0) return 1;
    return (uint16_t)((total_entries + TUND_PEER_LIST_PAGE_ENTRIES - 1) /
                      TUND_PEER_LIST_PAGE_ENTRIES);
}

uint16_t proto_peer_list_page_entry_count(uint16_t total_entries, uint16_t page_index) {
    uint16_t page_count = proto_peer_list_page_count(total_entries);
    if (page_count == 0 || page_index >= page_count) return 0;

    uint16_t first_entry = (uint16_t)(page_index * TUND_PEER_LIST_PAGE_ENTRIES);
    uint16_t remaining = (uint16_t)(total_entries - first_entry);
    return remaining < TUND_PEER_LIST_PAGE_ENTRIES ? remaining : TUND_PEER_LIST_PAGE_ENTRIES;
}

int proto_build_peer_list_page(uint8_t *buf, uint32_t snapshot_id, uint16_t page_index,
                               uint16_t total_entries) {
    uint16_t page_count = proto_peer_list_page_count(total_entries);
    if (snapshot_id == 0 || page_count == 0 || page_index >= page_count) return -1;

    uint16_t entry_count = proto_peer_list_page_entry_count(total_entries, page_index);
    uint16_t payload_len =
        (uint16_t)(TUND_PEER_LIST_PAGE_HEADER_SIZE + entry_count * TUND_PEER_ENTRY_SIZE);
    if (proto_write_hdr(buf, MSG_PEER_LIST, payload_len) < 0) return -1;

    uint8_t *payload = buf + TUND_HDR_SIZE;
    write_u32_be(payload, snapshot_id);
    write_u16_be(payload + 4, page_index);
    write_u16_be(payload + 6, page_count);
    write_u16_be(payload + 8, total_entries);
    return TUND_HDR_SIZE + payload_len;
}

bool proto_read_peer_list_page(const uint8_t *payload, uint16_t payload_len,
                               proto_peer_list_page_t *page) {
    if (payload_len < TUND_PEER_LIST_PAGE_HEADER_SIZE) return false;

    uint32_t snapshot_id = read_u32_be(payload);
    uint16_t page_index = read_u16_be(payload + 4);
    uint16_t page_count = read_u16_be(payload + 6);
    uint16_t total_entries = read_u16_be(payload + 8);
    uint16_t expected_pages = proto_peer_list_page_count(total_entries);
    if (snapshot_id == 0 || expected_pages == 0 || page_count != expected_pages ||
        page_index >= page_count)
        return false;

    uint16_t entry_count = proto_peer_list_page_entry_count(total_entries, page_index);
    uint16_t expected_len =
        (uint16_t)(TUND_PEER_LIST_PAGE_HEADER_SIZE + entry_count * TUND_PEER_ENTRY_SIZE);
    if (payload_len != expected_len) return false;

    page->snapshot_id = snapshot_id;
    page->page_index = page_index;
    page->page_count = page_count;
    page->total_entries = total_entries;
    page->entry_count = entry_count;
    page->entries = payload + TUND_PEER_LIST_PAGE_HEADER_SIZE;
    return true;
}
