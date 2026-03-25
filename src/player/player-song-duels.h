/* File: player/player-song-duels.h */

/*
 * Song duel system: Contest and Lament songs.
 */

#ifndef INCLUDED_PLAYER_SONG_DUELS_H
#define INCLUDED_PLAYER_SONG_DUELS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

bool song_is_duel(int song);
void display_synergy_message(int song1, int song2);
void song_duel_clear_player_target(void);
void song_duel_reset_player_stack(void);
bool song_duel_select_target(int song);
bool song_duel_process_contest(int song_skill);
bool song_duel_process_lament(int song_skill);
void song_duels_new_player_turn(void);
void song_duels_handle_monster_removed(int m_idx);

#endif /* INCLUDED_PLAYER_SONG_DUELS_H */
