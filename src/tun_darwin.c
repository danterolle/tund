#ifdef __APPLE__

#include "tun.h"
#include "tund.h"

#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/ioctl.h>
#include <net/if_utun.h>
#include <net/if.h>

int tun_open(tun_device_t *dev)
{
    struct sockaddr_ctl sc;
    struct ctl_info ctlInfo;
    int fd;

    fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        LOG_ERROR("socket(PF_SYSTEM) failed: %s", strerror(errno));
        return -1;
    }

    memset(&ctlInfo, 0, sizeof(ctlInfo));
    strncpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &ctlInfo) < 0) {
        LOG_ERROR("ioctl(CTLIOCGINFO) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memset(&sc, 0, sizeof(sc));
    sc.sc_id = ctlInfo.ctl_id;
    sc.sc_len = sizeof(sc);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = 0;

    if (connect(fd, (struct sockaddr *)&sc, sizeof(sc)) < 0) {
        LOG_ERROR("connect(utun) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    socklen_t ifname_len = TUN_IFNAME_MAX;
    if (getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME,
                   dev->ifname, &ifname_len) < 0) {
        LOG_ERROR("getsockopt(UTUN_OPT_IFNAME) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    dev->fd = fd;
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
    char cmd[256];
    struct in_addr ip_addr, mask_addr, dst_addr;
    ip_addr.s_addr = ip;
    mask_addr.s_addr = netmask;

    uint32_t net = ntohl(ip) & ntohl(netmask);
    dst_addr.s_addr = htonl(net | 1);
    if (ip == dst_addr.s_addr) {
        dst_addr.s_addr = htonl(net | 2);
    }

    char ip_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN], mask_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &dst_addr, dst_str, sizeof(dst_str));
    inet_ntop(AF_INET, &mask_addr, mask_str, sizeof(mask_str));

    snprintf(cmd, sizeof(cmd),
             "ifconfig %s inet %s %s netmask %s up",
             dev->ifname, ip_str, dst_str, mask_str);

    LOG_DEBUG("Running: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        LOG_ERROR("ifconfig failed (exit %d)", ret);
        return -1;
    }

    snprintf(cmd, sizeof(cmd),
             "route add -net %s -netmask %s -interface %s 2>/dev/null",
             "10.9.0.0", mask_str, dev->ifname);
    LOG_DEBUG("Running: %s", cmd);
    system(cmd);

    LOG_INFO("Configured %s: %s (peer %s) netmask %s",
             dev->ifname, ip_str, dst_str, mask_str);
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ifconfig %s mtu %d", dev->ifname, mtu);
    LOG_DEBUG("Running: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        LOG_ERROR("ifconfig mtu failed (exit %d)", ret);
        return -1;
    }
    dev->mtu = mtu;
    LOG_INFO("MTU set to %d on %s", mtu, dev->ifname);
    return 0;
}

int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize)
{
    uint8_t tmp[TUND_MAX_PAYLOAD + 4];
    int maxread = bufsize + 4;
    if (maxread > (int)sizeof(tmp))
        maxread = (int)sizeof(tmp);

    int n = (int)read(dev->fd, tmp, (size_t)maxread);
    if (n <= 4)
        return -1;

    int payload = n - 4;
    if (payload > bufsize)
        payload = bufsize;
    memcpy(buf, tmp + 4, (size_t)payload);
    return payload;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    uint8_t tmp[TUND_MAX_PAYLOAD + 4];
    if (len + 4 > (int)sizeof(tmp))
        return -1;

    uint32_t af = htonl(AF_INET);
    memcpy(tmp, &af, 4);
    memcpy(tmp + 4, buf, (size_t)len);

    int n = (int)write(dev->fd, tmp, (size_t)(len + 4));
    if (n <= 4)
        return -1;
    return n - 4;
}

#endif /* __APPLE__ */
