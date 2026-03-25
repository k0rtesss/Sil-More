/* File: player/player-song-effects.h */

/*
 * Individual song effects, song control, and the main song processor.
 */

#ifndef INCLUDED_PLAYER_SONG_EFFECTS_H
#define INCLUDED_PLAYER_SONG_EFFECTS_H

#include "../h-basic.h"

void change_song(int song);
bool singing(int song);
void sing(void);

void sing_song_of_freedom(int score);
bool known_to_delvings(int y, int x);
void sing_song_of_challenge(int score);
void sing_song_of_delvings(int score);
void sing_song_of_elbereth(int score);
void sing_song_of_trees(int score);
void sing_song_of_lorien(int score);
void sing_song_of_shattering(int score);
void shatter_in_arc(int dir, int score);
void sing_song_of_revealing(int score, bool primary_song);

#endif /* INCLUDED_PLAYER_SONG_EFFECTS_H */
