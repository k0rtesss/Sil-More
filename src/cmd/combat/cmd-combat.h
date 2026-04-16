/* File: cmd-combat.h */

/*
 * Transitional public header for the combat command split.
 */

#ifndef INCLUDED_CMD_COMBAT_H
#define INCLUDED_CMD_COMBAT_H

#include "h-basic.h"

bool graphics_are_ascii(void);

int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2);

#endif /* INCLUDED_CMD_COMBAT_H */
