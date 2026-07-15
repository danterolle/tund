#include "internal.h"

uint32_t server_alloc_ip(const server_t *srv)
{
    for (uint32_t ip_h = TUND_IP_START; ip_h <= TUND_IP_END; ip_h++) {
        uint32_t ip_n = htonl(ip_h);
        bool taken = false;
        for (int i = 0; i < TUND_MAX_PEERS; i++) {
            if (srv->peers[i].active && srv->peers[i].virt_ip == ip_n) {
                taken = true;
                break;
            }
        }
        if (!taken)
            return ip_n;
    }
    return 0;
}

int server_find_peer_by_vip(const server_t *srv, uint32_t virt_ip)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && srv->peers[i].virt_ip == virt_ip)
            return i;
    }
    return -1;
}

int server_find_peer_by_addr(const server_t *srv, const struct sockaddr_in *addr)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active &&
            srv->peers[i].real_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            srv->peers[i].real_addr.sin_port == addr->sin_port)
            return i;
    }
    return -1;
}

int server_find_free_slot(const server_t *srv)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (!srv->peers[i].active)
            return i;
    }
    return -1;
}

void server_broadcast(server_t *srv, uint8_t *buf, int len,
                      int exclude_idx, uint64_t bytes_out)
{
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != exclude_idx) {
            if (net_send(srv->sockfd, buf, len, &srv->peers[i].real_addr) < 0) {
                char peer_ip[TUND_IP_STR_LEN];
                LOG_WARN("Broadcast to %s (%s) failed",
                         srv->peers[i].name,
                         ip_to_str_buf(srv->peers[i].virt_ip, peer_ip, sizeof(peer_ip)));
            } else {
                srv->peers[i].bytes_out += bytes_out;
            }
        }
    }
}

void server_send_peer_list(server_t *srv, int peer_idx)
{
    uint8_t buf[TUND_MAX_PKT];
    uint8_t *payload = buf + TUND_HDR_SIZE;
    int offset = 0, count = 0;

    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        if (srv->peers[i].active && i != peer_idx) {
            if (offset + (int)sizeof(msg_peer_entry_t) > TUND_MAX_PAYLOAD) break;
            msg_peer_entry_t *entry = (msg_peer_entry_t *)(payload + offset);
            entry->virt_ip = srv->peers[i].virt_ip;
            memset(entry->name, 0, TUND_NAME_LEN);
            strncpy(entry->name, srv->peers[i].name, TUND_NAME_LEN - 1);
            entry->status = 1;
            offset += (int)sizeof(msg_peer_entry_t);
            count++;
        }
    }

    proto_write_hdr(buf, MSG_PEER_LIST, (uint16_t)offset);
    if (net_send(srv->sockfd, buf, TUND_HDR_SIZE + offset,
                 &srv->peers[peer_idx].real_addr) < 0)
        LOG_WARN("Failed to send peer list to %s", srv->peers[peer_idx].name);
    else
        LOG_DEBUG("Sent peer list (%d peers) to %s", count, srv->peers[peer_idx].name);
}
