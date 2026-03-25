/* File: player/player-song-disguise.h */

/*
 * Song of Disguise and Song of Revealing overlay state.
 */

#ifndef INCLUDED_PLAYER_SONG_DISGUISE_H
#define INCLUDED_PLAYER_SONG_DISGUISE_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

#define SONG_REVEALING_FULL_VISIBILITY 10

extern bool song_disguise_active;
extern byte song_revealing_hint[];
extern bool song_revealing_has_data;

void song_disguise_on_start(void);
void song_disguise_on_stop(void);
bool any_monster_observes_player(void);
void song_disguise_new_player_turn(void);
void song_disguise_handle_monster_removed(int m_idx);
void song_disguise_note_monster_attack(int m_idx);
void song_disguise_note_player_attack(int m_idx);
void song_revealing_decay(void);
bool song_revealing_overlay(int m_idx, byte* a, char* c);
bool song_disguise_monster_is_fooled(const monster_type* m_ptr);
void sing_song_of_disguise(int score);

#endif /* INCLUDED_PLAYER_SONG_DISGUISE_H */
