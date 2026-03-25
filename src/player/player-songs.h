/* File: player/player-songs.h */

/*
 * Music/song ability helpers.
 */

#ifndef INCLUDED_PLAYER_SONGS_H
#define INCLUDED_PLAYER_SONGS_H

#include "../h-basic.h"

int affinity_level(int skilltype);
int minstrel_level(void);
int song_effective_skill(int abilitynum);
int ability_bonus(int skilltype, int abilitynum);

#endif /* INCLUDED_PLAYER_SONGS_H */
