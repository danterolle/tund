#include "tund.h"
#include "server.h"
#include "client.h"
#include <getopt.h>

int g_log_level = LOG_LEVEL_INFO;
volatile bool g_tui_active = true;
time_t g_start_time = 0;
volatile int g_running = 1;
uint64_t g_auth_key0 = 0;
uint64_t g_auth_key1 = 0;

static server_t g_server;
static client_t g_client;

#ifdef _WIN32
#include <shellapi.h>

static BOOL WINAPI console_handler(DWORD dwCtrlType)
{
    (void)dwCtrlType;
    g_running = 0;
    return TRUE;
}

static void setup_signals(void)
{
    SetConsoleCtrlHandler(console_handler, TRUE);
}

static bool check_admin(void)
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

static bool stderr_is_tty(void)
{
    DWORD mode = 0;
    HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
    return herr != INVALID_HANDLE_VALUE && GetConsoleMode(herr, &mode);
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
        if (*p == '\\') {
            slashes++;
            continue;
        }

        if (*p == '"' || *p == '\0') {
            for (size_t i = 0; i < slashes * 2; i++) {
                if (!append_text(buf, bufsize, pos, "\\"))
                    return false;
            }
            slashes = 0;
            if (*p == '"') {
                if (!append_text(buf, bufsize, pos, "\\\""))
                    return false;
                continue;
            }
            break;
        }

        for (size_t i = 0; i < slashes; i++) {
            if (!append_text(buf, bufsize, pos, "\\"))
                return false;
        }
        slashes = 0;

        char ch[2] = {*p, '\0'};
        if (!append_text(buf, bufsize, pos, ch))
            return false;
    }

    return append_text(buf, bufsize, pos, "\"");
}

static bool build_relaunch_params(int argc, char *argv[], char *buf, size_t bufsize)
{
    size_t pos = 0;
    buf[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (i > 1 && !append_text(buf, bufsize, &pos, " "))
            return false;
        if (!append_quoted_arg(buf, bufsize, &pos, argv[i]))
            return false;
    }

    return true;
}

static bool relaunch_as_admin(int argc, char *argv[])
{
    char exe_path[MAX_PATH];
    char params[8192];

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
                                    params[0] ? params : NULL,
                                    NULL, SW_SHOWNORMAL);
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
        "Tund \u2014 Fatal Error",
        MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
static bool stderr_is_tty(void)
{
    return isatty(STDERR_FILENO);
}

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}
#endif

static void print_usage(const char *prog, bool show_desc)
{
    if (show_desc) fprintf(stderr,
        "Tund - Virtual LAN over UDP. Creates an authenticated\n"
        "IPv4 tunnel for LAN gaming with friends.\n"
        "\n");
    fprintf(stderr,
        "Usage:\n"
        "  %s server [-p port] [-v]\n"
        "  %s client -s <server_ip> [-p port] [-n name] [-v]\n"
        "\n"
        "Modes:\n"
        "  server   Run as hub server (requires public IP)\n"
        "  client   Connect to a server\n"
        "\n"
        "Options:\n"
        "  -s, --server <ip>    Server IP/hostname (client mode, required)\n"
        "  -p, --port <port>    UDP port (default: %u)\n"
        "  -n, --name <name>    Client display name (default: hostname)\n"
        "  -k, --key <key>      Shared network passphrase (required)\n"
         "  -t, --no-tui        Disable terminal UI (live peer dashboard)\n"
        "  -v, --verbose        Enable debug logging\n"
        "  -h, --help           Show this help\n"
        "\n"
        "Examples:\n"
        "  sudo %s server                        # Start server on port %u\n"
        "  sudo %s client -s 1.2.3.4 -n MyPC     # Connect to server\n"
        "  sudo %s server -p 12345               # Custom port\n"
        "\n"
        "Note: Requires root/sudo for TUN interface creation.\n"
        "\n",
        prog, prog,
        TUND_PORT,
        prog, TUND_PORT,
        prog, prog);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_handler);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port = TUND_PORT;
    cfg.log_level = LOG_LEVEL_INFO;
    cfg.tui_mode = true;

    if (argc < 2) {
        print_usage(argv[0], false);
        return 1;
    }

    const char *mode_str = argv[1];
    if (strcmp(mode_str, "server") == 0) {
        cfg.mode = MODE_SERVER;
    } else if (strcmp(mode_str, "client") == 0) {
        cfg.mode = MODE_CLIENT;
    } else if (strcmp(mode_str, "-h") == 0 || strcmp(mode_str, "--help") == 0) {
        print_usage(argv[0], true);
        return 0;
    } else {
        fprintf(stderr, "Error: Unknown mode '%s'. Use 'server' or 'client'.\n\n", mode_str);
        print_usage(argv[0], false);
        return 1;
    }

    static struct option long_opts[] = {
        {"server",  required_argument, 0, 's'},
        {"port",    required_argument, 0, 'p'},
        {"name",    required_argument, 0, 'n'},
        {"key",     required_argument, 0, 'k'},
        {"no-tui",  no_argument,       0, 't'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 2;
    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:n:k:tvh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            strncpy(cfg.server_ip, optarg, sizeof(cfg.server_ip) - 1);
            break;
        case 'p':
        {
            char *end = NULL;
            long p = strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || p < 1 || p > 65535) {
                fprintf(stderr, "Error: invalid port '%s' (use 1-65535)\n\n", optarg);
                return 1;
            }
            cfg.port = (uint16_t)p;
            break;
        }
        case 'n':
            strncpy(cfg.client_name, optarg, TUND_NAME_LEN - 1);
            break;
        case 'k':
            snprintf(cfg.access_key, sizeof(cfg.access_key), "%s", optarg);
            break;
        case 't':
            cfg.tui_mode = false;
            break;
        case 'v':
            cfg.log_level = LOG_LEVEL_DEBUG;
            break;
        case 'h':
            print_usage(argv[0], true);
            return 0;
        default:
            print_usage(argv[0], false);
            return 1;
        }
    }

    g_log_level = cfg.log_level;
    g_tui_active = cfg.tui_mode;
    g_start_time = time(NULL);

    if (cfg.mode == MODE_CLIENT && cfg.server_ip[0] == '\0') {
        fprintf(stderr, "Error: Client mode requires -s <server_ip>\n\n");
        print_usage(argv[0], false);
#ifdef _WIN32
        MessageBoxA(NULL, "Client mode requires a server IP address.\nFill in the Server IP field.", "Tund - Error", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }

    if (cfg.mode == MODE_CLIENT && cfg.client_name[0] == '\0') {
        if (gethostname(cfg.client_name, TUND_NAME_LEN - 1) != 0)
            strncpy(cfg.client_name, "tund-client", TUND_NAME_LEN - 1);
    }
    if (strlen(cfg.access_key) < 12) {
        fprintf(stderr, "Error: a shared access key of at least 12 characters is required (-k).\n");
#ifdef _WIN32
        MessageBoxA(NULL, "A shared network key of at least 12 characters is required.\nEnter it in the Network key field.", "Tund - Error", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
    proto_key_from_passphrase(cfg.access_key, &g_auth_key0, &g_auth_key1);

    if (cfg.tui_mode && !stderr_is_tty()) {
        cfg.tui_mode = false;
        g_tui_active = false;
        LOG_INFO("Terminal UI disabled because stderr is not interactive.");
    }

#ifdef _WIN32
    if (!check_admin()) {
        fprintf(stderr, "Tund requires Administrator privileges for the TUN interface.\n");
        if (relaunch_as_admin(argc, argv))
            return 0;
        fprintf(stderr, "Run the same command from an Administrator terminal.\n\n");
        return 1;
    }
    if (cfg.tui_mode) {
        HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
#else
    if (geteuid() != 0) {
        const char *mode_str = (argc > 1) ? argv[1] : "";
        fprintf(stderr,
            "\033[33mWarning: Tund requires root privileges for TUN interface.\n"
            "Run with: sudo %s %s ...\033[0m\n\n",
            argv[0], mode_str);
        return 1;
    }
#endif

    setup_signals();

    if (cfg.mode == MODE_SERVER) {
        if (server_init(&g_server, &cfg) < 0) {
            LOG_ERROR("Failed to initialize server");
            return 1;
        }
        server_run(&g_server);
        server_shutdown(&g_server);
    } else {
        if (client_init(&g_client, &cfg) < 0) {
            LOG_ERROR("Failed to initialize client");
            return 1;
        }
        client_run(&g_client);
        client_shutdown(&g_client);
    }

    LOG_INFO("Tund terminated.");
    return 0;
}
