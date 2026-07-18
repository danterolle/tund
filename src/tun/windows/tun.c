#ifdef _WIN32

#include "internal.h"

int tun_open(tun_device_t *dev)
{
    dev->adapter = NULL;
    dev->session = NULL;
    dev->read_event = NULL;
    dev->has_luid = false;

    if (wintun_load() < 0)
        return -1;

    WINTUN_ADAPTER_HANDLE adapter = wintun_open_adapter(L"Tund");
    if (!adapter)
        adapter = wintun_create_adapter(L"Tund", L"Tund VPN");
    if (!adapter) {
        LOG_ERROR("Cannot create/open Wintun adapter (error %lu). Accept the UAC prompt and try again.",
                  GetLastError());
        return -1;
    }

    WINTUN_SESSION_HANDLE session = wintun_start_session(adapter, 0x400000);
    if (!session) {
        LOG_ERROR("Cannot start Wintun session (error %lu). Close other Tund instances and try again.",
                  GetLastError());
        wintun_close_adapter(adapter);
        return -1;
    }

    HANDLE read_event = wintun_get_read_wait_event(session);
    if (!read_event) {
        LOG_ERROR("WintunGetReadWaitEvent failed (error %lu)", GetLastError());
        wintun_end_session(session);
        wintun_close_adapter(adapter);
        return -1;
    }

    NET_LUID luid;
    WCHAR ifname_w[256] = {0};
    char ifname_a[256] = {0};
    if (wintun_get_adapter_luid(adapter, &luid)) {
        dev->luid = luid;
        dev->has_luid = true;
        if (ConvertInterfaceLuidToNameW(&luid, ifname_w, 256)) {
            WideCharToMultiByte(CP_UTF8, 0, ifname_w, -1, ifname_a, sizeof(ifname_a), NULL, NULL);
        }
    }
    if (ifname_a[0] == '\0')
        strncpy(ifname_a, "tund", sizeof(ifname_a) - 1);

    dev->adapter = adapter;
    dev->session = session;
    dev->read_event = read_event;
    snprintf(dev->ifname, TUN_IFNAME_MAX, "%s", ifname_a);
    dev->mtu = TUND_TUN_MTU;
    LOG_INFO("TUN interface created: %s", dev->ifname);
    return 0;
}

void tun_close(tun_device_t *dev)
{
    if (dev->session) wintun_end_session((WINTUN_SESSION_HANDLE)dev->session);
    if (dev->adapter) wintun_close_adapter((WINTUN_ADAPTER_HANDLE)dev->adapter);
    LOG_INFO("TUN interface closed: %s", dev->ifname);
}

int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize)
{
    DWORD size;
    WINTUN_SESSION_HANDLE session = (WINTUN_SESSION_HANDLE)dev->session;
    if (!session) return -1;

    HANDLE re = (HANDLE)dev->read_event;
    for (;;) {
        BYTE *packet = wintun_receive_packet(session, &size);
        if (packet) {
            int n = (size <= (DWORD)bufsize) ? (int)size : bufsize;
            memcpy(buf, packet, (size_t)n);
            wintun_release_receive_packet(session, packet);
            return n;
        }
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_ITEMS) {
            LOG_WARN("WintunReceivePacket error %lu", err);
            return -1;
        }
        WaitForSingleObject(re, 500);
        if (!tund_is_running()) return -1;
    }
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    WINTUN_SESSION_HANDLE session = (WINTUN_SESSION_HANDLE)dev->session;
    if (!session) return -1;

    BYTE *packet = wintun_allocate_send_packet(session, (DWORD)len);
    if (!packet) return -1;

    memcpy(packet, buf, (size_t)len);
    wintun_send_packet(session, packet);
    return len;
}

#endif /* _WIN32 */
