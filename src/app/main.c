#include "cli.h"
#include "events.h"
#include "log.h"
#include "platform.h"
#include "server.h"
#include "client.h"

int g_log_level = LOG_LEVEL_INFO;
bool g_tui_active = true;
time_t g_start_time = 0;
tund_stop_flag_t g_running = ATOMIC_VAR_INIT(true);
uint8_t g_crypto_key[TUND_KEY_SIZE];

static server_t g_server;
static client_t g_client;

int main(int argc, char *argv[]) {
    config_t cfg;
    app_init_early();

    cli_result_t cli_result = cli_parse(argc, argv, &cfg);
    if (cli_result == CLI_EXIT_OK) return 0;
    if (cli_result == CLI_EXIT_ERROR) return 1;

    g_log_level = cfg.log_level;
    g_tui_active = cfg.tui_mode;
    g_json_events_active = cfg.json_events;
    g_start_time = time(NULL);
    proto_key_from_passphrase(cfg.access_key, g_crypto_key);
    tund_wipe_secret(cfg.access_key, sizeof(cfg.access_key));

    if (cfg.tui_mode && !app_stderr_is_tty()) {
        cfg.tui_mode = false;
        g_tui_active = false;
        LOG_INFO("Terminal UI disabled because stderr is not interactive.");
    }

    app_startup_result_t platform_result = app_prepare_runtime(argc, argv, &cfg);
    if (platform_result == APP_STARTUP_EXIT_OK) {
        tund_wipe_secret(g_crypto_key, sizeof(g_crypto_key));
        return 0;
    }
    if (platform_result == APP_STARTUP_EXIT_ERROR) {
        tund_wipe_secret(g_crypto_key, sizeof(g_crypto_key));
        return 1;
    }

    app_setup_signals();
    app_setup_stdin_control();

    if (cfg.mode == MODE_SERVER) {
        if (server_init(&g_server, &cfg) < 0) {
            LOG_ERROR("Failed to initialize server");
            tund_wipe_secret(g_crypto_key, sizeof(g_crypto_key));
            return 1;
        }
        server_run(&g_server);
        server_shutdown(&g_server);
    } else {
        if (client_init(&g_client, &cfg) < 0) {
            LOG_ERROR("Failed to initialize client");
            tund_wipe_secret(g_crypto_key, sizeof(g_crypto_key));
            return 1;
        }
        client_run(&g_client);
        client_shutdown(&g_client);
    }

    tund_wipe_secret(g_crypto_key, sizeof(g_crypto_key));
    LOG_INFO("TunD terminated.");
    return 0;
}
