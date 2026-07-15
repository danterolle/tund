#ifndef TUND_WINDOWS_INTERNAL_H
#define TUND_WINDOWS_INTERNAL_H

#ifdef _WIN32

#include "tun.h"
#include "tund.h"
#include "wintun.h"

const char *win32_errstr(DWORD err);
int run_cmd(const char *exe, const char *args);

int wintun_load(void);
WINTUN_ADAPTER_HANDLE wintun_open_adapter(LPCWSTR name);
WINTUN_ADAPTER_HANDLE wintun_create_adapter(LPCWSTR name, LPCWSTR tunnel_type);
void wintun_close_adapter(WINTUN_ADAPTER_HANDLE adapter);
bool wintun_get_adapter_luid(WINTUN_ADAPTER_HANDLE adapter, NET_LUID *luid);
WINTUN_SESSION_HANDLE wintun_start_session(WINTUN_ADAPTER_HANDLE adapter, DWORD capacity);
void wintun_end_session(WINTUN_SESSION_HANDLE session);
HANDLE wintun_get_read_wait_event(WINTUN_SESSION_HANDLE session);
BYTE *wintun_receive_packet(WINTUN_SESSION_HANDLE session, DWORD *size);
void wintun_release_receive_packet(WINTUN_SESSION_HANDLE session, const BYTE *packet);
BYTE *wintun_allocate_send_packet(WINTUN_SESSION_HANDLE session, DWORD len);
void wintun_send_packet(WINTUN_SESSION_HANDLE session, const BYTE *packet);

#endif /* _WIN32 */
#endif /* TUND_WINDOWS_INTERNAL_H */
