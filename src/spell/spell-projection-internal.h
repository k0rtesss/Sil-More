/* File: spell/spell-projection-internal.h */

/*
 * Lane-local internals for the split projection engine.
 */

#ifndef INCLUDED_SPELL_PROJECTION_INTERNAL_H
#define INCLUDED_SPELL_PROJECTION_INTERNAL_H

#include "../h-basic.h"

typedef struct projection_monster_state
{
    int hit_count;
    int last_hit_x;
    int last_hit_y;
    int unseen_death_count;
} projection_monster_state;

bool projection_affect_feature(
    int who, int y, int x, int dist, int dd, int ds, int dif, int typ);
bool projection_affect_object(
    int who, int y, int x, int dd, int ds, int dif, int typ);
bool projection_affect_monster(int who, int y, int x, int dd, int ds, int dif,
    int typ, u32b flg, projection_monster_state* state);
bool projection_affect_player(
    int who, int y, int x, int dd, int ds, int dif, int typ);

#endif /* INCLUDED_SPELL_PROJECTION_INTERNAL_H */
