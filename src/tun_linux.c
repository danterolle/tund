#ifdef __linux__

#include "tun.h"
#include "tund.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

int tun_open(tun_device_t *dev)
{
    struct ifreq ifr;
    int fd;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        LOG_ERROR("Cannot open /dev/net/tun: %s", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tund%d", IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        LOG_ERROR("ioctl(TUNSETIFF) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    dev->fd = fd;
    strncpy(dev->ifname, ifr.ifr_name, TUN_IFNAME_MAX - 1);
    dev->ifname[TUN_IFNAME_MAX - 1] = '\0';
    dev->mtu = TUND_TUN_MTU;

    LOG_INFO("TUN interface created: %s (fd=%d)", dev->ifname, dev->fd);
    return 0;
}

void tun_close(tun_device_t *dev)
{
    if (dev->fd >= 0) {
        LOG_INFO("Closing TUN interface: %s", dev->ifname);
        close(dev->fd);
        dev->fd = -1;
    }
}

int tun_set_ip(tun_device_t *dev, uint32_t ip, uint32_t netmask)
{
    struct ifreq ifr;
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("socket() for ioctl failed: %s", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->ifname, IFNAMSIZ);

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = ip;
    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCSIFADDR) failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = netmask;
    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCSIFNETMASK) failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCGIFFLAGS) failed: %s", strerror(errno));
        close(sock);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCSIFFLAGS) failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);

    char ip_str[TUND_IP_STR_LEN];
    char mask_str[TUND_IP_STR_LEN];
    LOG_INFO("Configured %s: %s/%s", dev->ifname,
             ip_to_str_buf(ip, ip_str, sizeof(ip_str)),
             ip_to_str_buf(netmask, mask_str, sizeof(mask_str)));
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu)
{
    struct ifreq ifr;
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->ifname, IFNAMSIZ);
    ifr.ifr_mtu = mtu;

    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCSIFMTU) failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);
    dev->mtu = mtu;
    LOG_INFO("MTU set to %d on %s", mtu, dev->ifname);
    return 0;
}

int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize)
{
    return (int)read(dev->fd, buf, (size_t)bufsize);
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    return (int)write(dev->fd, buf, (size_t)len);
}

#endif /* __linux__ */
