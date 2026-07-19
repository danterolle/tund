#include "tund.h"
#include "cli.h"
#include "platform.h"
#include "server.h"
#include "client.h"

int g_log_level = LOG_LEVEL_INFO;
bool g_tui_active = true;
time_t g_start_time = 0;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);
uint64_t g_auth_key0 = 0;
uint64_t g_auth_key1 = 0;

static server_t g_server;
static client_t g_client;

int main(int argc, char *argv[])
{
    config_t cfg;
    app_init_early();

    cli_result_t cli_result = cli_parse(argc, argv, &cfg);
    if (cli_result == CLI_EXIT_OK)
        return 0;
    if (cli_result == CLI_EXIT_ERROR)
        return 1;

    g_log_level = cfg.log_level;
    g_tui_active = cfg.tui_mode;
    g_start_time = time(NULL);
    proto_key_from_passphrase(cfg.access_key, &g_auth_key0, &g_auth_key1);

    if (cfg.tui_mode && !app_stderr_is_tty()) {
        cfg.tui_mode = false;
        g_tui_active = false;
        LOG_INFO("Terminal UI disabled because stderr is not interactive.");
    }

    app_startup_result_t platform_result = app_prepare_runtime(argc, argv, &cfg);
    if (platform_result == APP_STARTUP_EXIT_OK)
        return 0;
    if (platform_result == APP_STARTUP_EXIT_ERROR)
        return 1;

    app_setup_signals();

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

    LOG_INFO("TunD terminated.");
    return 0;
}
