#include "events.h"
#include "tund.h"

bool g_json_events_active = false;

static pthread_mutex_t g_event_lock = PTHREAD_MUTEX_INITIALIZER;

static void event_json_string(const char *value) {
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        switch (*p) {
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\b':
            fputs("\\b", stdout);
            break;
        case '\f':
            fputs("\\f", stdout);
            break;
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\r':
            fputs("\\r", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        default:
            if (*p < 0x20)
                fprintf(stdout, "\\u%04x", *p);
            else
                fputc(*p, stdout);
            break;
        }
    }
    fputc('"', stdout);
}

void app_event_peer_join(const char *name, uint32_t virt_ip, const struct sockaddr_in *endpoint) {
    if (!g_json_events_active) return;

    char vip[TUND_IP_STR_LEN];
    char endpoint_ip[TUND_IP_STR_LEN];

    pthread_mutex_lock(&g_event_lock);
    fputs("{\"event\":\"peer_join\",\"name\":", stdout);
    event_json_string(name);
    fputs(",\"virtual_ip\":", stdout);
    event_json_string(ip_to_str_buf(virt_ip, vip, sizeof(vip)));
    fputs(",\"endpoint\":", stdout);
    char endpoint_text[TUND_IP_STR_LEN + 8];
    snprintf(endpoint_text, sizeof(endpoint_text), "%s:%u",
             ip_to_str_buf(endpoint->sin_addr.s_addr, endpoint_ip, sizeof(endpoint_ip)),
             ntohs(endpoint->sin_port));
    event_json_string(endpoint_text);
    fputs("}\n", stdout);
    fflush(stdout);
    pthread_mutex_unlock(&g_event_lock);
}

void app_event_peer_leave(uint32_t virt_ip, const char *reason) {
    if (!g_json_events_active) return;

    char vip[TUND_IP_STR_LEN];

    pthread_mutex_lock(&g_event_lock);
    fputs("{\"event\":\"peer_leave\",\"virtual_ip\":", stdout);
    event_json_string(ip_to_str_buf(virt_ip, vip, sizeof(vip)));
    fputs(",\"reason\":", stdout);
    event_json_string(reason);
    fputs("}\n", stdout);
    fflush(stdout);
    pthread_mutex_unlock(&g_event_lock);
}
