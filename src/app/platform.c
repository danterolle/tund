#include "platform.h"

#ifdef _WIN32
#include "win_runtime.h"

void app_init_early(void) {
    win_runtime_init_early();
}

bool app_stderr_is_tty(void) {
    return win_runtime_stderr_is_tty();
}

bool app_stdin_is_tty(void) {
    return win_runtime_stdin_is_tty();
}

app_startup_result_t app_prepare_runtime(int argc, char *argv[], const config_t *cfg) {
    if (!win_runtime_is_admin()) {
        fprintf(stderr, "TunD requires Administrator privileges for the TUN interface.\n");
        if (win_runtime_relaunch_as_admin(argc, argv)) return APP_STARTUP_EXIT_OK;
        fprintf(stderr, "Run the same command from an Administrator terminal.\n\n");
        return APP_STARTUP_EXIT_ERROR;
    }
    if (cfg->tui_mode) win_runtime_enable_tui_console();
    return APP_STARTUP_OK;
}

void app_setup_signals(void) {
    win_runtime_setup_signals();
}

#else

#include <signal.h>

void app_init_early(void) {}

bool app_stderr_is_tty(void) {
    return isatty(STDERR_FILENO);
}

bool app_stdin_is_tty(void) {
    return isatty(STDIN_FILENO);
}

app_startup_result_t app_prepare_runtime(int argc, char *argv[], const config_t *cfg) {
    (void)cfg;
    if (geteuid() == 0) return APP_STARTUP_OK;

    const char *mode = (argc > 1) ? argv[1] : "";
    if (app_stderr_is_tty()) {
        fprintf(stderr,
                "\033[33mWarning: TunD requires root privileges for TUN interface.\n"
                "Run with: sudo %s %s ...\033[0m\n\n",
                argv[0], mode);
    } else {
        fprintf(stderr,
                "Warning: TunD requires root privileges for TUN interface.\n"
                "Run with: sudo %s %s ...\n\n",
                argv[0], mode);
    }
    return APP_STARTUP_EXIT_ERROR;
}

static void signal_handler(int sig) {
    (void)sig;
    tund_request_stop();
}

void app_setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

#endif

static void *stdin_control_thread(void *arg) {
    (void)arg;
    char line[32];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "stop") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            tund_request_stop();
            break;
        }
    }
    return NULL;
}

void app_setup_stdin_control(void) {
    if (app_stdin_is_tty()) return;

    pthread_t tid;
    if (pthread_create(&tid, NULL, stdin_control_thread, NULL) == 0) {
        pthread_detach(tid);
    }
}
