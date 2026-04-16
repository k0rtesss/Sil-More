#ifndef INCLUDED_RUNTIME_GAME_H
#define INCLUDED_RUNTIME_GAME_H

#include "h-basic.h"

extern bool save_game_quietly;

bool death_processing_in_progress(void);
bool death_spectator_active(void);
bool preconfirm_enter_morgoth_hall(void);
int generation_depth_for_level(int depth);
int player_generation_depth(void);
void reset_dungeon_state(void);
PlayResult play_game(void);

void do_cmd_save_game(void);
void do_cmd_escape(int silmarils);
void do_cmd_suicide(void);
void close_game(void);
void exit_game_panic(void);

bool autoload_alive_from_scores(void);
void metarun_finalize_scores_and_saves(void);
void backup_and_clear_saves(void);

#endif /* INCLUDED_RUNTIME_GAME_H */
