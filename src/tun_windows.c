#ifdef _WIN32

#include "tun.h"
#include "tund.h"
#include "wintun.h"

static HMODULE g_wintun_dll = NULL;

static const char *win32_errstr(DWORD err)
{
    static char buf[256];
    buf[0] = '\0';
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, sizeof(buf), NULL);
    if (buf[0] == '\0')
        snprintf(buf, sizeof(buf), "Windows error %lu", err);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == '.'))
        buf[--n] = '\0';
    return buf;
}

static int run_cmd(const char *exe, const char *args)
{
    char full[4096];
    int written;
    if (strchr(exe, ' ') || strchr(exe, '\t'))
        written = snprintf(full, sizeof(full), "\"%s\" %s", exe, args);
    else
        written = snprintf(full, sizeof(full), "%s %s", exe, args);
    if (written < 0 || written >= (int)sizeof(full)) {
        LOG_ERROR("Command line too long for %s", exe);
        return -1;
    }

    LOG_DEBUG("Running command: %s", full);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, full, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        LOG_ERROR("Failed to start command '%s': %s (error %lu)",
                  full, win32_errstr(err), err);
        return -1;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, INFINITE);
    if (wait == WAIT_FAILED) {
        DWORD err = GetLastError();
        LOG_ERROR("Failed while waiting for command '%s': %s (error %lu)",
                  full, win32_errstr(err), err);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        DWORD err = GetLastError();
        LOG_ERROR("Could not read exit code for command '%s': %s (error %lu)",
                  full, win32_errstr(err), err);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (code != 0) {
        LOG_ERROR("Command failed with exit code %lu: %s", code, full);
        return -1;
    }

    LOG_DEBUG("Command completed successfully: %s", full);
    return 0;
}

static WINTUN_ADAPTER_HANDLE (*pWintunCreateAdapter)(LPCWSTR, LPCWSTR, const GUID *);
static void (*pWintunCloseAdapter)(WINTUN_ADAPTER_HANDLE);
static WINTUN_ADAPTER_HANDLE (*pWintunOpenAdapter)(LPCWSTR);
static void (*pWintunGetAdapterLUID)(WINTUN_ADAPTER_HANDLE, NET_LUID *);
static WINTUN_SESSION_HANDLE (*pWintunStartSession)(WINTUN_ADAPTER_HANDLE, DWORD);
static void (*pWintunEndSession)(WINTUN_SESSION_HANDLE);
static HANDLE (*pWintunGetReadWaitEvent)(WINTUN_SESSION_HANDLE);
static BYTE *(*pWintunReceivePacket)(WINTUN_SESSION_HANDLE, DWORD *);
static void (*pWintunReleaseReceivePacket)(WINTUN_SESSION_HANDLE, const BYTE *);
static BYTE *(*pWintunAllocateSendPacket)(WINTUN_SESSION_HANDLE, DWORD);
static void (*pWintunSendPacket)(WINTUN_SESSION_HANDLE, const BYTE *);

#define WINTUN_LOAD(name) do { \
    *(void **)(&p##name) = (void *)GetProcAddress(g_wintun_dll, #name); \
    if (!p##name) { LOG_ERROR("Missing symbol: %s", #name); return -1; } \
} while (0)

static int wintun_load(void)
{
    if (g_wintun_dll) return 0;

    g_wintun_dll = LoadLibraryW(L"wintun.dll");
    if (!g_wintun_dll) {
        LOG_ERROR("Failed to load wintun.dll (error %lu); keep wintun.dll next to tund.exe or use the bundled Windows release.",
                  GetLastError());
        return -1;
    }

    WINTUN_LOAD(WintunCreateAdapter);
    WINTUN_LOAD(WintunCloseAdapter);
    WINTUN_LOAD(WintunOpenAdapter);
    WINTUN_LOAD(WintunGetAdapterLUID);
    WINTUN_LOAD(WintunStartSession);
    WINTUN_LOAD(WintunEndSession);
    WINTUN_LOAD(WintunGetReadWaitEvent);
    WINTUN_LOAD(WintunReceivePacket);
    WINTUN_LOAD(WintunReleaseReceivePacket);
    WINTUN_LOAD(WintunAllocateSendPacket);
    WINTUN_LOAD(WintunSendPacket);

    LOG_INFO("Wintun loaded successfully");
    return 0;
}

int tun_open(tun_device_t *dev)
{
    dev->adapter = NULL;
    dev->session = NULL;
    dev->read_event = NULL;

    if (wintun_load() < 0)
        return -1;

    WINTUN_ADAPTER_HANDLE adapter = pWintunOpenAdapter(L"Tund");
    if (!adapter)
        adapter = pWintunCreateAdapter(L"Tund", L"Tund VPN", NULL);
    if (!adapter || !pWintunCloseAdapter || !pWintunStartSession
        || !pWintunGetReadWaitEvent || !pWintunEndSession
        || !pWintunGetAdapterLUID) {
        LOG_ERROR("Cannot create/open Wintun adapter (error %lu). Accept the UAC prompt, keep wintun.dll next to tund.exe, and try again.",
                  GetLastError());
        return -1;
    }

    WINTUN_SESSION_HANDLE session = pWintunStartSession(adapter, 0x400000);
    if (!session) {
        LOG_ERROR("Cannot start Wintun session (error %lu). Close other Tund instances and try again.",
                  GetLastError());
        pWintunCloseAdapter(adapter);
        return -1;
    }

    HANDLE read_event = pWintunGetReadWaitEvent(session);
    if (!read_event) {
        LOG_ERROR("WintunGetReadWaitEvent failed (error %lu)", GetLastError());
        pWintunEndSession(session);
        pWintunCloseAdapter(adapter);
        return -1;
    }

    NET_LUID luid;
    WCHAR ifname_w[256] = {0};
    char ifname_a[256] = {0};
    if (pWintunGetAdapterLUID) {
        pWintunGetAdapterLUID(adapter, &luid);
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
    if (dev->session) pWintunEndSession((WINTUN_SESSION_HANDLE)dev->session);
    if (dev->adapter) pWintunCloseAdapter((WINTUN_ADAPTER_HANDLE)dev->adapter);
    LOG_INFO("TUN interface closed: %s", dev->ifname);
}

int tun_set_ip(tun_device_t *dev, uint32_t ip, uint32_t netmask)
{
    char buf[2048];
    char ip_s[32], mask_s[32], net_s[32];
    struct in_addr ip_a, mask_a, net_a;

    ip_a.s_addr = ip;
    mask_a.s_addr = netmask;
    inet_ntop(AF_INET, &ip_a, ip_s, sizeof(ip_s));
    inet_ntop(AF_INET, &mask_a, mask_s, sizeof(mask_s));

    snprintf(buf, sizeof(buf),
             "interface ip set address name=\"%s\" static %s %s",
             dev->ifname, ip_s, mask_s);
    if (run_cmd("netsh", buf) < 0) {
        LOG_ERROR("Failed to configure the Wintun IP address with netsh. Run from an elevated prompt and check Windows network policy.");
        return -1;
    }

    uint32_t net = ntohl(ip) & ntohl(netmask);
    net_a.s_addr = htonl(net);
    inet_ntop(AF_INET, &net_a, net_s, sizeof(net_s));
    snprintf(buf, sizeof(buf),
             "add %s mask %s %s metric 2",
             net_s, mask_s, ip_s);
    if (run_cmd("route", buf) < 0) {
        LOG_ERROR("Failed to add the Windows route for 10.9.0.0/24. Check for an existing route/VPN conflict and run as Administrator.");
        return -1;
    }

    LOG_INFO("Configured %s: %s/%s", dev->ifname, ip_s, mask_s);
    return 0;
}

int tun_set_mtu(tun_device_t *dev, int mtu)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
             "interface ipv4 set subinterface \"%s\" mtu=%d store=active",
             dev->ifname, mtu);
    if (run_cmd("netsh", buf) < 0) {
        LOG_ERROR("Failed to set Wintun MTU with netsh. Run as Administrator and check Windows network policy.");
        return -1;
    }
    dev->mtu = mtu;
    LOG_INFO("MTU set to %d on %s", mtu, dev->ifname);
    return 0;
}

int tun_read(tun_device_t *dev, uint8_t *buf, int bufsize)
{
    DWORD size;
    WINTUN_SESSION_HANDLE session = (WINTUN_SESSION_HANDLE)dev->session;

    if (!session || !pWintunReceivePacket || !pWintunReleaseReceivePacket)
        return -1;

    HANDLE re = (HANDLE)dev->read_event;

    for (;;) {
        BYTE *packet = pWintunReceivePacket(session, &size);
        if (packet) {
            int n = (size <= (DWORD)bufsize) ? (int)size : bufsize;
            memcpy(buf, packet, (size_t)n);
            pWintunReleaseReceivePacket(session, packet);
            return n;
        }
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_ITEMS) {
            LOG_WARN("WintunReceivePacket error %lu", err);
            return -1;
        }
        WaitForSingleObject(re, 500);
        if (!g_running)
            return -1;
    }
}

int tun_write(tun_device_t *dev, const uint8_t *buf, int len)
{
    WINTUN_SESSION_HANDLE session = (WINTUN_SESSION_HANDLE)dev->session;

    if (!session) return -1;

    BYTE *packet = pWintunAllocateSendPacket(session, (DWORD)len);
    if (!packet)
        return -1;

    memcpy(packet, buf, (size_t)len);
    pWintunSendPacket(session, packet);
    return len;
}

#endif /* _WIN32 */
