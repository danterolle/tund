#ifndef TUND_TUN_H
#define TUND_TUN_H

#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <netioapi.h>
#endif

#define TUN_IFNAME_MAX  32

typedef struct {
    int     fd;
    char    ifname[TUN_IFNAME_MAX];
    int     mtu;
#ifdef _WIN32
    void    *adapter;
    void    *session;
    void    *read_event;
    NET_LUID luid;
    bool    has_luid;
#endif
} tun_device_t;

int tun_open(tun_device_t *dev);
void tun_close(tun_device_t *dev);
int tun_set_ip(tun_device_t *dev, uint32_t ip, uint32_t netmask);
int tun_set_mtu(tun_device_t *dev, int mtu);
int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize);
int tun_write(tun_device_t *dev, const uint8_t *buf, int len);

#endif /* TUND_TUN_H */
