#ifdef _WIN32

#include "internal.h"
#include "log.h"

static HMODULE g_wintun_dll = NULL;

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

#define WINTUN_LOAD(name)                                                                          \
    do {                                                                                           \
        FARPROC proc = GetProcAddress(g_wintun_dll, #name);                                        \
        if (!proc) {                                                                               \
            LOG_ERROR("Missing symbol: %s", #name);                                                \
            return -1;                                                                             \
        }                                                                                          \
        memcpy(&p##name, &proc, sizeof(p##name));                                                  \
    } while (0)

int wintun_load(void) {
    if (g_wintun_dll) return 0;

    g_wintun_dll = LoadLibraryW(L"wintun.dll");
    if (!g_wintun_dll) {
        LOG_ERROR("Failed to load wintun.dll (error %lu); keep wintun.dll next to tund-cli.exe or "
                  "use the bundled Windows release.",
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

WINTUN_ADAPTER_HANDLE wintun_open_adapter(LPCWSTR name) {
    return pWintunOpenAdapter(name);
}

WINTUN_ADAPTER_HANDLE wintun_create_adapter(LPCWSTR name, LPCWSTR tunnel_type) {
    return pWintunCreateAdapter(name, tunnel_type, NULL);
}

void wintun_close_adapter(WINTUN_ADAPTER_HANDLE adapter) {
    pWintunCloseAdapter(adapter);
}

bool wintun_get_adapter_luid(WINTUN_ADAPTER_HANDLE adapter, NET_LUID *luid) {
    pWintunGetAdapterLUID(adapter, luid);
    return true;
}

WINTUN_SESSION_HANDLE wintun_start_session(WINTUN_ADAPTER_HANDLE adapter, DWORD capacity) {
    return pWintunStartSession(adapter, capacity);
}

void wintun_end_session(WINTUN_SESSION_HANDLE session) {
    pWintunEndSession(session);
}

HANDLE wintun_get_read_wait_event(WINTUN_SESSION_HANDLE session) {
    return pWintunGetReadWaitEvent(session);
}

BYTE *wintun_receive_packet(WINTUN_SESSION_HANDLE session, DWORD *size) {
    return pWintunReceivePacket(session, size);
}

void wintun_release_receive_packet(WINTUN_SESSION_HANDLE session, const BYTE *packet) {
    pWintunReleaseReceivePacket(session, packet);
}

BYTE *wintun_allocate_send_packet(WINTUN_SESSION_HANDLE session, DWORD len) {
    return pWintunAllocateSendPacket(session, len);
}

void wintun_send_packet(WINTUN_SESSION_HANDLE session, const BYTE *packet) {
    pWintunSendPacket(session, packet);
}

#endif /* _WIN32 */
