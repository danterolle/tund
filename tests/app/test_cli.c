#include "cli.h"
#include "log.h"
#include "sitest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#define ARG_COUNT(args) ((int)(sizeof(args) / sizeof((args)[0])))

bool app_stdin_is_tty(void) {
    return false;
}

static int stderr_fd(void) {
#ifdef _WIN32
    return _fileno(stderr);
#else
    return fileno(stderr);
#endif
}

static int open_null_device(void) {
#ifdef _WIN32
    return _open("NUL", _O_WRONLY);
#else
    return open("/dev/null", O_WRONLY);
#endif
}

static int dup_fd(int fd) {
#ifdef _WIN32
    return _dup(fd);
#else
    return dup(fd);
#endif
}

static void close_fd(int fd) {
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

static void dup2_fd(int src, int dst) {
#ifdef _WIN32
    _dup2(src, dst);
#else
    dup2(src, dst);
#endif
}

static cli_result_t parse_quiet(int argc, char *argv[], config_t *cfg) {
    fflush(stderr);
    int err_fd = stderr_fd();
    int saved = dup_fd(err_fd);
    int null_fd = open_null_device();
    if (saved >= 0 && null_fd >= 0) dup2_fd(null_fd, err_fd);

    cli_result_t result = cli_parse(argc, argv, cfg);

    fflush(stderr);
    if (saved >= 0) {
        dup2_fd(saved, err_fd);
        close_fd(saved);
    }
    if (null_fd >= 0) close_fd(null_fd);
    return result;
}

static bool write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    fputs(contents, file);
    return fclose(file) == 0;
}

static void test_server_arguments(void) {
    config_t cfg;
    char *argv[] = {"tund-cli",          "server", "-p", "12345",        "-k",
                    "a-long-random-key", "-t",     "-v", "--json-events"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_OK);
    CHECK(cfg.mode == MODE_SERVER);
    CHECK(cfg.port == 12345);
    CHECK(strcmp(cfg.access_key, "a-long-random-key") == 0);
    CHECK(cfg.tui_mode == false);
    CHECK(cfg.log_level == LOG_LEVEL_DEBUG);
    CHECK(cfg.json_events == true);
}

static void test_client_arguments_with_default_name(void) {
    config_t cfg;
    char *argv[] = {"tund-cli", "client", "-s", "203.0.113.10", "-k", "a-long-random-key"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_OK);
    CHECK(cfg.mode == MODE_CLIENT);
    CHECK(cfg.port == TUND_PORT);
    CHECK(strcmp(cfg.server_ip, "203.0.113.10") == 0);
    CHECK(strcmp(cfg.access_key, "a-long-random-key") == 0);
    CHECK(cfg.client_name[0] != '\0');
    CHECK(strlen(cfg.client_name) < TUND_NAME_LEN);
}

static void test_key_file_trims_line_end(void) {
    config_t cfg;
    const char *path = "dist/test-cli-key.txt";
    char *argv[] = {"tund-cli", "server", "--key-file", (char *)path};

    CHECK(write_file(path, "file-random-key\n"));
    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_OK);
    CHECK(strcmp(cfg.access_key, "file-random-key") == 0);
    remove(path);
}

static void test_rejects_invalid_port(void) {
    config_t cfg;
    char *argv[] = {"tund-cli", "server", "-p", "0", "-k", "a-long-random-key"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_ERROR);
}

static void test_rejects_duplicate_key_sources(void) {
    config_t cfg;
    char *argv[] = {"tund-cli",          "server",     "-k",
                    "a-long-random-key", "--key-file", "dist/does-not-need-to-exist"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_ERROR);
}

static void test_rejects_too_long_key(void) {
    config_t cfg;
    char too_long_key[sizeof(cfg.access_key) + 1];
    memset(too_long_key, 'A', sizeof(too_long_key) - 1);
    too_long_key[sizeof(too_long_key) - 1] = '\0';
    char *argv[] = {"tund-cli", "server", "-k", too_long_key};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_ERROR);
}

static void test_rejects_short_key(void) {
    config_t cfg;
    char *argv[] = {"tund-cli", "server", "-k", "short"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_ERROR);
}

static void test_client_requires_server(void) {
    config_t cfg;
    char *argv[] = {"tund-cli", "client", "-k", "a-long-random-key"};
    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_ERROR);
}

static void test_help_exits_ok(void) {
    config_t cfg;
    char *argv[] = {"tund-cli", "--help"};

    CHECK(parse_quiet(ARG_COUNT(argv), argv, &cfg) == CLI_EXIT_OK);
}

int main(void) {
    test_server_arguments();
    test_client_arguments_with_default_name();
    test_key_file_trims_line_end();
    test_rejects_invalid_port();
    test_rejects_duplicate_key_sources();
    test_rejects_too_long_key();
    test_rejects_short_key();
    test_client_requires_server();
    test_help_exits_ok();

    return sitest_finish("CLI parsing tests");
}
