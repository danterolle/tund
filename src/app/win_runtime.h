#ifndef TUND_WIN_RUNTIME_H
#define TUND_WIN_RUNTIME_H

#ifdef _WIN32

#include "tund.h"

void win_runtime_init_early(void);
bool win_runtime_stderr_is_tty(void);
bool win_runtime_is_admin(void);
bool win_runtime_relaunch_as_admin(int argc, char *argv[]);
void win_runtime_enable_tui_console(void);
void win_runtime_setup_signals(void);

#endif /* _WIN32 */
#endif /* TUND_WIN_RUNTIME_H */
