/* File: monster/monster-death.h */

/*
 * Transitional public header for monster damage/death helpers used outside the
 * monster core implementation.
 */

#ifndef INCLUDED_MONSTER_MONSTER_DEATH_H
#define INCLUDED_MONSTER_MONSTER_DEATH_H

#include "h-basic.h"

void maybe_update_morgoth_state_from_hp(monster_type* m_ptr);
void monster_death(int m_idx);
bool mon_take_hit(int m_idx, int dam, cptr note, int who);

#endif /* INCLUDED_MONSTER_MONSTER_DEATH_H */
