#ifndef INCLUDED_GAME_PROGRESS_RESET_H
#define INCLUDED_GAME_PROGRESS_RESET_H

#include "h-basic.h"

/* Called before any progress is loaded or migrated. A failed reset must stop
 * startup; its durable request is retained so the next launch can retry. */
bool sil_progress_reset_startup(const char* install_root, const char* user_root);
bool sil_progress_reset_skip_legacy_import(void);

/* Consume the internal desktop relaunch argument before normal startup. */
bool sil_progress_reset_launch_init(int* argc, char*** argv);

/* Only call after all three UI confirmations. On success this exits the old
 * process; false leaves its progress untouched and supplies a user message. */
bool sil_progress_reset_request_restart(char* error, size_t error_size);
bool sil_progress_reset_restarting(void);

#endif
