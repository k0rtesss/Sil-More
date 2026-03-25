#ifndef INCLUDED_RUNTIME_GAME_H
#define INCLUDED_RUNTIME_GAME_H

#include "h-basic.h"

bool death_processing_in_progress(void);

void do_cmd_save_game(void);
void close_game(void);
void exit_game_panic(void);

bool autoload_alive_from_scores(void);
void metarun_finalize_scores_and_saves(void);
void backup_and_clear_saves(void);

#endif /* INCLUDED_RUNTIME_GAME_H */
