#ifndef TUND_PLATFORM_H
#define TUND_PLATFORM_H

#include "tund.h"

typedef enum {
    APP_STARTUP_OK,
    APP_STARTUP_EXIT_OK,
    APP_STARTUP_EXIT_ERROR,
} app_startup_result_t;

void app_init_early(void);
bool app_stderr_is_tty(void);
app_startup_result_t app_prepare_runtime(int argc, char *argv[], const config_t *cfg);
void app_setup_signals(void);

#endif /* TUND_PLATFORM_H */
