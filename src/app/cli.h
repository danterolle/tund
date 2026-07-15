#ifndef TUND_CLI_H
#define TUND_CLI_H

#include "tund.h"

typedef enum {
    CLI_OK,
    CLI_EXIT_OK,
    CLI_EXIT_ERROR,
} cli_result_t;

cli_result_t cli_parse(int argc, char *argv[], config_t *cfg);

#endif /* TUND_CLI_H */
