#ifndef TUND_WINTUN_H
#define TUND_WINTUN_H
#ifdef _WIN32
#include <windows.h>
#include <netioapi.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WINTUN_ADAPTER_ *WINTUN_ADAPTER_HANDLE;
typedef struct WINTUN_SESSION_ *WINTUN_SESSION_HANDLE;

#ifdef __cplusplus
}
#endif

#endif /* TUND_WINTUN_H */
