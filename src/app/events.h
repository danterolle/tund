#ifndef TUND_EVENTS_H
#define TUND_EVENTS_H

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

void app_event_peer_join(const char *name, uint32_t virt_ip, const struct sockaddr_in *endpoint);
void app_event_peer_leave(uint32_t virt_ip, const char *reason);

#endif /* TUND_EVENTS_H */
