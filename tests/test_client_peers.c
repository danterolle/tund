#include "../src/core/client/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

void log_msg(enum log_level level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

static void init_client(client_t *cli)
{
    pthread_mutex_init(&cli->peers_lock, NULL);
}

static void destroy_client(client_t *cli)
{
    pthread_mutex_destroy(&cli->peers_lock);
}

static void test_update_peer_lifecycle(void)
{
    client_t cli = {0};
    uint32_t vip = htonl(TUND_IP_START);

    init_client(&cli);
    client_update_peer(&cli, vip, "alpha", true);
    CHECK(cli.peer_count == 1);
    CHECK(cli.peers[0].active);
    CHECK(cli.peers[0].virt_ip == vip);
    CHECK(strcmp(cli.peers[0].name, "alpha") == 0);
    CHECK(cli.peers[0].last_seen != 0);

    cli.peers[0].bytes_in = 10;
    cli.peers[0].bytes_out = 20;
    client_update_peer(&cli, vip, "beta", true);
    CHECK(cli.peer_count == 1);
    CHECK(strcmp(cli.peers[0].name, "beta") == 0);
    CHECK(cli.peers[0].bytes_in == 10);
    CHECK(cli.peers[0].bytes_out == 20);

    client_update_peer(&cli, vip, "ignored", false);
    CHECK(cli.peer_count == 0);
    CHECK(!cli.peers[0].active);

    client_update_peer(&cli, htonl(TUND_IP_START + 1), "missing", false);
    CHECK(cli.peer_count == 0);
    destroy_client(&cli);
}

static void test_update_peer_truncates_names(void)
{
    client_t cli = {0};
    char long_name[64];

    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    init_client(&cli);
    client_update_peer(&cli, htonl(TUND_IP_START), long_name, true);
    CHECK(cli.peer_count == 1);
    CHECK(strlen(cli.peers[0].name) == TUND_NAME_LEN - 1);
    CHECK(cli.peers[0].name[TUND_NAME_LEN - 1] == '\0');
    destroy_client(&cli);
}

static void test_update_peer_table_full(void)
{
    client_t cli = {0};

    init_client(&cli);
    for (int i = 0; i < TUND_MAX_PEERS; i++) {
        cli.peers[i].active = true;
        cli.peers[i].virt_ip = htonl(TUND_IP_START + (uint32_t)i);
    }
    cli.peer_count = TUND_MAX_PEERS;

    client_update_peer(&cli, htonl(TUND_IP_END + 1), "overflow", true);
    CHECK(cli.peer_count == TUND_MAX_PEERS);
    for (int i = 0; i < TUND_MAX_PEERS; i++)
        CHECK(cli.peers[i].virt_ip == htonl(TUND_IP_START + (uint32_t)i));
    destroy_client(&cli);
}

static void test_traffic_accounting(void)
{
    client_t cli = {0};
    uint32_t first = htonl(TUND_IP_START);
    uint32_t second = htonl(TUND_IP_START + 1);

    init_client(&cli);
    client_update_peer(&cli, first, "alpha", true);
    client_update_peer(&cli, second, "beta", true);

    client_add_peer_traffic(&cli, first, 12, 34);
    CHECK(cli.peers[0].bytes_in == 12);
    CHECK(cli.peers[0].bytes_out == 34);
    CHECK(cli.peers[1].bytes_in == 0);
    CHECK(cli.peers[1].bytes_out == 0);

    client_add_peer_traffic(&cli, htonl(TUND_IP_START + 10), 50, 60);
    CHECK(cli.peers[0].bytes_in == 12);
    CHECK(cli.peers[0].bytes_out == 34);

    client_add_broadcast_traffic(&cli, 100);
    CHECK(cli.peers[0].bytes_out == 134);
    CHECK(cli.peers[1].bytes_out == 100);

    cli.peers[1].active = false;
    client_add_broadcast_traffic(&cli, 100);
    CHECK(cli.peers[0].bytes_out == 234);
    CHECK(cli.peers[1].bytes_out == 100);
    destroy_client(&cli);
}

int main(void)
{
    test_update_peer_lifecycle();
    test_update_peer_truncates_names();
    test_update_peer_table_full();
    test_traffic_accounting();

    if (failures != 0) {
        fprintf(stderr, "%d client peer test(s) failed\n", failures);
        return 1;
    }

    puts("client peer tests passed");
    return 0;
}
