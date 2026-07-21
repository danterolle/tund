#ifdef __APPLE__

#include "protocol.h"
#include "tun.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <net/if_utun.h>

int tun_open(tun_device_t *dev) {
    struct sockaddr_ctl sc;
    struct ctl_info ctlInfo;
    int fd;

    fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        LOG_ERROR("Cannot open macOS utun control socket: %s. Run with administrator privileges.",
                  strerror(errno));
        return -1;
    }

    memset(&ctlInfo, 0, sizeof(ctlInfo));
    strncpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &ctlInfo) < 0) {
        LOG_ERROR("Cannot locate macOS utun control: %s. Check that utun support is available.",
                  strerror(errno));
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
        LOG_ERROR("Cannot create macOS utun interface: %s. Run with administrator privileges and "
                  "allow network configuration if prompted.",
                  strerror(errno));
        close(fd);
        return -1;
    }

    socklen_t ifname_len = TUN_IFNAME_MAX;
    if (getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, dev->ifname, &ifname_len) < 0) {
        LOG_ERROR("getsockopt(UTUN_OPT_IFNAME) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    dev->fd = fd;
    dev->mtu = TUND_TUN_MTU;

    LOG_INFO("TUN interface created: %s (fd=%d)", dev->ifname, dev->fd);
    return 0;
}

void tun_close(tun_device_t *dev) {
    if (dev->fd >= 0) {
        LOG_INFO("Closing TUN interface: %s", dev->ifname);
        close(dev->fd);
        dev->fd = -1;
    }
}

int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize) {
    uint8_t tmp[TUND_MAX_PLAINTEXT + 4];
    int maxread = bufsize + 4;
    if (maxread > (int)sizeof(tmp)) maxread = (int)sizeof(tmp);

    struct pollfd pfd;
    pfd.fd = dev->fd;
    pfd.events = POLLIN;

    int ready = poll(&pfd, 1, 250);
    if (ready <= 0) return 0;

    int n = (int)read(dev->fd, tmp, (size_t)maxread);
    if (n <= 4) return -1;

    int payload = n - 4;
    if (payload > bufsize) payload = bufsize;
    memcpy(buf, tmp + 4, (size_t)payload);
    return payload;
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len) {
    uint8_t tmp[TUND_MAX_PLAINTEXT + 4];
    if (len + 4 > (int)sizeof(tmp)) return -1;

    uint32_t af = htonl(AF_INET);
    memcpy(tmp, &af, 4);
    memcpy(tmp + 4, buf, (size_t)len);

    int n = (int)write(dev->fd, tmp, (size_t)(len + 4));
    if (n <= 4) return -1;
    return n - 4;
}

#endif /* __APPLE__ */
