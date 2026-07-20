#ifdef _WIN32

#include "internal.h"
#include "log.h"

const char *win32_errstr(DWORD err) {
    static char buf[256];
    buf[0] = '\0';
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);
    if (buf[0] == '\0') snprintf(buf, sizeof(buf), "Windows error %lu", err);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == '.'))
        buf[--n] = '\0';
    return buf;
}

int run_cmd(const char *exe, const char *args) {
    char full[4096];
    int written = strchr(exe, ' ') || strchr(exe, '\t')
                      ? snprintf(full, sizeof(full), "\"%s\" %s", exe, args)
                      : snprintf(full, sizeof(full), "%s %s", exe, args);
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
        LOG_ERROR("Failed to start command '%s': %s (error %lu)", full, win32_errstr(err), err);
        return -1;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, INFINITE);
    if (wait == WAIT_FAILED) {
        DWORD err = GetLastError();
        LOG_ERROR("Failed while waiting for command '%s': %s (error %lu)", full, win32_errstr(err),
                  err);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        DWORD err = GetLastError();
        LOG_ERROR("Could not read exit code for command '%s': %s (error %lu)", full,
                  win32_errstr(err), err);
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

#endif /* _WIN32 */
