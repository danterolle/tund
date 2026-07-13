#ifdef __APPLE__

#include "tun.h"
#include "tund.h"

#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/wait.h>
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
    struct in_addr ip_addr, mask_addr, dst_addr;
    ip_addr.s_addr = ip;
    mask_addr.s_addr = netmask;

    uint32_t net = ntohl(ip) & ntohl(netmask);
    dst_addr.s_addr = htonl(net | 1);
    if (ip == dst_addr.s_addr)
        dst_addr.s_addr = htonl(net | 2);

    char ip_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN], mask_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &dst_addr, dst_str, sizeof(dst_str));
    inet_ntop(AF_INET, &mask_addr, mask_str, sizeof(mask_str));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("socket() for IP config failed: %s", strerror(errno));
        return -1;
    }

    struct ifaliasreq ifra;
    struct sockaddr_in *sin;

    memset(&ifra, 0, sizeof(ifra));
    strlcpy(ifra.ifra_name, dev->ifname, sizeof(ifra.ifra_name));

    sin = (struct sockaddr_in *)&ifra.ifra_addr;
    sin->sin_family = AF_INET;
    sin->sin_len = sizeof(*sin);
    sin->sin_addr.s_addr = ip;

    sin = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    sin->sin_family = AF_INET;
    sin->sin_len = sizeof(*sin);
    sin->sin_addr.s_addr = dst_addr.s_addr;

    sin = (struct sockaddr_in *)&ifra.ifra_mask;
    sin->sin_family = AF_INET;
    sin->sin_len = sizeof(*sin);
    sin->sin_addr.s_addr = netmask;

    if (ioctl(sock, SIOCAIFADDR, &ifra) < 0) {
        LOG_ERROR("ioctl(SIOCAIFADDR) on %s failed: %s", dev->ifname, strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);

    struct in_addr net_addr;
    net_addr.s_addr = htonl(net);
    char net_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &net_addr, net_str, sizeof(net_str));

    pid_t pid = fork();
    if (pid == 0) {
        execl("/sbin/route", "route", "add", "-net", net_str,
              "-netmask", mask_str, "-interface", dev->ifname, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        LOG_WARN("fork() for route add failed: %s", strerror(errno));
    }

    LOG_INFO("Configured %s: %s (peer %s) netmask %s",
             dev->ifname, ip_str, dst_str, mask_str);
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu)
{
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("socket() for MTU failed: %s", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->ifname, IFNAMSIZ);
    ifr.ifr_mtu = mtu;

    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        LOG_ERROR("ioctl(SIOCSIFMTU) on %s failed: %s", dev->ifname, strerror(errno));
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
