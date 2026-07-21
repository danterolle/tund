#include "cli.h"

#include "log.h"

#include <getopt.h>
#include <stdlib.h>

static void print_usage(const char *prog, bool show_desc) {
    if (show_desc)
        fprintf(stderr, "TunD - Virtual LAN over UDP. Creates an encrypted\n"
                        "IPv4 tunnel for LAN gaming with friends.\n"
                        "\n");
    fprintf(stderr,
            "Usage:\n"
            "  %s server -k <key> [-p port] [-v]\n"
            "  %s client -s <server_ip> -k <key> [-p port] [-n name] [-v]\n"
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
            "  sudo %s server -k \"a-long-random-key\"\n"
            "  sudo %s client -s 1.2.3.4 -n MyPC -k \"a-long-random-key\"\n"
            "  sudo %s server -p 12345 -k \"a-long-random-key\"\n"
            "\n"
            "Note: Requires root/sudo for TUN interface creation.\n"
            "\n",
            prog, prog, TUND_PORT, prog, prog, prog);
}

static cli_result_t parse_mode(const char *prog, const char *mode, config_t *cfg) {
    if (strcmp(mode, "server") == 0) {
        cfg->mode = MODE_SERVER;
    } else if (strcmp(mode, "client") == 0) {
        cfg->mode = MODE_CLIENT;
    } else if (strcmp(mode, "-h") == 0 || strcmp(mode, "--help") == 0) {
        print_usage(prog, true);
        return CLI_EXIT_OK;
    } else {
        fprintf(stderr, "Error: Unknown mode '%s'. Use 'server' or 'client'.\n\n", mode);
        print_usage(prog, false);
        return CLI_EXIT_ERROR;
    }
    return CLI_OK;
}

static cli_result_t parse_port(const char *value, config_t *cfg) {
    char *end = NULL;
    long port = strtol(value, &end, 10);
    if (end == value || *end != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "Error: invalid port '%s' (use 1-65535)\n\n", value);
        return CLI_EXIT_ERROR;
    }
    cfg->port = (uint16_t)port;
    return CLI_OK;
}

static cli_result_t validate_config(const char *prog, config_t *cfg) {
    if (cfg->mode == MODE_CLIENT && cfg->server_ip[0] == '\0') {
        fprintf(stderr, "Error: Client mode requires -s <server_ip>\n\n");
        print_usage(prog, false);
#ifdef _WIN32
        MessageBoxA(NULL, "Client mode requires a server IP address.\nFill in the Server IP field.",
                    "TunD - Error", MB_OK | MB_ICONERROR);
#endif
        return CLI_EXIT_ERROR;
    }

    if (cfg->mode == MODE_CLIENT && cfg->client_name[0] == '\0') {
        if (gethostname(cfg->client_name, TUND_NAME_LEN - 1) != 0)
            strncpy(cfg->client_name, "tund-client", TUND_NAME_LEN - 1);
    }
    if (strlen(cfg->access_key) < 12) {
        fprintf(stderr, "Error: a shared access key of at least 12 characters is required (-k).\n");
#ifdef _WIN32
        MessageBoxA(NULL,
                    "A shared network key of at least 12 characters is required.\nEnter it in the "
                    "Network key field.",
                    "TunD - Error", MB_OK | MB_ICONERROR);
#endif
        return CLI_EXIT_ERROR;
    }
    return CLI_OK;
}

cli_result_t cli_parse(int argc, char *argv[], config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = TUND_PORT;
    cfg->log_level = LOG_LEVEL_INFO;
    cfg->tui_mode = true;

    if (argc < 2) {
        print_usage(argv[0], false);
        return CLI_EXIT_ERROR;
    }

    cli_result_t result = parse_mode(argv[0], argv[1], cfg);
    if (result != CLI_OK) return result;

    static struct option long_opts[] = {
        {"server", required_argument, 0, 's'}, {"port", required_argument, 0, 'p'},
        {"name", required_argument, 0, 'n'},   {"key", required_argument, 0, 'k'},
        {"no-tui", no_argument, 0, 't'},       {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},         {0, 0, 0, 0}};

    optind = 2;
    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:n:k:tvh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            strncpy(cfg->server_ip, optarg, sizeof(cfg->server_ip) - 1);
            break;
        case 'p':
            result = parse_port(optarg, cfg);
            if (result != CLI_OK) return result;
            break;
        case 'n':
            strncpy(cfg->client_name, optarg, TUND_NAME_LEN - 1);
            break;
        case 'k':
            snprintf(cfg->access_key, sizeof(cfg->access_key), "%s", optarg);
            break;
        case 't':
            cfg->tui_mode = false;
            break;
        case 'v':
            cfg->log_level = LOG_LEVEL_DEBUG;
            break;
        case 'h':
            print_usage(argv[0], true);
            return CLI_EXIT_OK;
        default:
            print_usage(argv[0], false);
            return CLI_EXIT_ERROR;
        }
    }

    return validate_config(argv[0], cfg);
}
