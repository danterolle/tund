#ifdef _WIN32

#include "win_runtime.h"

#include <shellapi.h>

static BOOL WINAPI console_handler(DWORD dwCtrlType)
{
    (void)dwCtrlType;
    g_running = 0;
    return TRUE;
}

bool win_runtime_is_admin(void)
{
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt_auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0,
                                 &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin != FALSE;
}

static bool append_text(char *buf, size_t bufsize, size_t *pos, const char *text)
{
    size_t len = strlen(text);
    if (*pos + len >= bufsize)
        return false;
    memcpy(buf + *pos, text, len);
    *pos += len;
    buf[*pos] = '\0';
    return true;
}

static bool append_quoted_arg(char *buf, size_t bufsize, size_t *pos, const char *arg)
{
    if (!append_text(buf, bufsize, pos, "\""))
        return false;

    size_t slashes = 0;
    for (const char *p = arg; ; p++) {
        if (*p == '\\') { slashes++; continue; }
        if (*p == '"' || *p == '\0') {
            for (size_t i = 0; i < slashes * 2; i++)
                if (!append_text(buf, bufsize, pos, "\\")) return false;
            slashes = 0;
            if (*p == '"') {
                if (!append_text(buf, bufsize, pos, "\\\"")) return false;
                continue;
            }
            break;
        }
        for (size_t i = 0; i < slashes; i++)
            if (!append_text(buf, bufsize, pos, "\\")) return false;
        slashes = 0;
        char ch[2] = {*p, '\0'};
        if (!append_text(buf, bufsize, pos, ch)) return false;
    }
    return append_text(buf, bufsize, pos, "\"");
}

static bool build_relaunch_params(int argc, char *argv[], char *buf, size_t bufsize)
{
    size_t pos = 0;
    buf[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1 && !append_text(buf, bufsize, &pos, " ")) return false;
        if (!append_quoted_arg(buf, bufsize, &pos, argv[i])) return false;
    }
    return true;
}

bool win_runtime_relaunch_as_admin(int argc, char *argv[])
{
    char exe_path[MAX_PATH], params[8192];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) {
        fprintf(stderr, "Error: cannot determine executable path for UAC relaunch.\n");
        return false;
    }
    if (!build_relaunch_params(argc, argv, params, sizeof(params))) {
        fprintf(stderr, "Error: command line is too long for UAC relaunch.\n");
        return false;
    }

    HINSTANCE result = ShellExecuteA(NULL, "runas", exe_path,
                                    params[0] ? params : NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        fprintf(stderr, "Error: UAC relaunch failed or was cancelled (code %ld).\n",
                (long)(INT_PTR)result);
        return false;
    }
    fprintf(stderr, "Tund is relaunching with Administrator privileges...\n");
    return true;
}

static void log_to_exe_dir(const char *filename, const char *msg)
{
    char logpath[MAX_PATH] = "";
    DWORD len = GetModuleFileNameA(NULL, logpath, sizeof(logpath));
    if (len > 0 && len < sizeof(logpath)) {
        char *p = strrchr(logpath, '\\');
        if (p) {
            p[1] = '\0';
            strncat(logpath, filename, sizeof(logpath) - strlen(logpath) - 1);
        }
    } else {
        strncpy(logpath, filename, sizeof(logpath) - 1);
    }
    FILE *f = fopen(logpath, "w");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

static LONG WINAPI crash_handler(EXCEPTION_POINTERS *ep)
{
    (void)ep;
    log_to_exe_dir("tund_crash.log", "Tund - Fatal error (access violation)");
    MessageBoxA(NULL,
        "Tund encountered a fatal error.\n\n"
        "Possible causes:\n"
        "  - wintun.dll missing, damaged, or incompatible\n"
        "  - Stack overflow in the network thread\n"
        "  - Wintun driver not installed correctly\n\n"
        "Make sure you are running as Administrator and that wintun.dll\n"
        "is in the same directory as tund.exe.\n\n"
        "Details saved to: tund_crash.log",
        "Tund - Fatal Error", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void win_runtime_init_early(void)
{
    SetUnhandledExceptionFilter(crash_handler);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

bool win_runtime_stderr_is_tty(void)
{
    DWORD mode = 0;
    HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
    return herr != INVALID_HANDLE_VALUE && GetConsoleMode(herr, &mode);
}

void win_runtime_enable_tui_console(void)
{
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
}

void win_runtime_setup_signals(void)
{
    SetConsoleCtrlHandler(console_handler, TRUE);
}

#endif /* _WIN32 */
