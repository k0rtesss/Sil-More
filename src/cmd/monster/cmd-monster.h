/* File: cmd-monster.h */

/*
 * Transitional public header for the monster command split.
 */

#ifndef INCLUDED_CMD_MONSTER_H
#define INCLUDED_CMD_MONSTER_H

#include "h-basic.h"

void new_wandering_flow(monster_type* m_ptr, int y, int x);
void new_wandering_destination(
    monster_type* m_ptr, monster_type* leader_ptr);
void drop_iron_crown(monster_type* m_ptr, const char* msg);
int success_chance(int sides, int skill, int difficulty);

#endif /* INCLUDED_CMD_MONSTER_H */
