#ifdef __APPLE__

#include "log.h"
#include "tun.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/wait.h>

int tun_set_ip(tun_device_t *dev, uint32_t ip, uint32_t netmask) {
    struct in_addr ip_addr = {.s_addr = ip};
    struct in_addr mask_addr = {.s_addr = netmask};
    uint32_t net = ntohl(ip) & ntohl(netmask);
    struct in_addr dst_addr = {.s_addr = htonl(net | 1)};
    if (ip == dst_addr.s_addr) dst_addr.s_addr = htonl(net | 2);

    char ip_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN], mask_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
    inet_ntop(AF_INET, &dst_addr, dst_str, sizeof(dst_str));
    inet_ntop(AF_INET, &mask_addr, mask_str, sizeof(mask_str));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("Cannot open socket for macOS interface configuration: %s.", strerror(errno));
        return -1;
    }

    struct ifaliasreq ifra;
    memset(&ifra, 0, sizeof(ifra));
    strlcpy(ifra.ifra_name, dev->ifname, sizeof(ifra.ifra_name));

    struct sockaddr_in *sin = (struct sockaddr_in *)&ifra.ifra_addr;
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
        LOG_ERROR("Cannot assign macOS TUN address on %s: %s.", dev->ifname, strerror(errno));
        close(sock);
        return -1;
    }
    close(sock);

    struct in_addr net_addr = {.s_addr = htonl(net)};
    char net_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &net_addr, net_str, sizeof(net_str));

    pid_t pid = fork();
    if (pid == 0) {
        execl("/sbin/route", "route", "add", "-net", net_str, "-netmask", mask_str, "-interface",
              dev->ifname, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        LOG_WARN("fork() for route add failed: %s", strerror(errno));
    }

    LOG_INFO("Configured %s: %s (peer %s) netmask %s", dev->ifname, ip_str, dst_str, mask_str);
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("Cannot open socket for macOS MTU configuration: %s.", strerror(errno));
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->ifname, IFNAMSIZ);
    ifr.ifr_mtu = mtu;

    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        LOG_ERROR("Cannot set macOS TUN MTU to %d on %s: %s.", mtu, dev->ifname, strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);
    dev->mtu = mtu;
    LOG_INFO("MTU set to %d on %s", mtu, dev->ifname);
    return 0;
}

#endif /* __APPLE__ */
