/* File: cmd-combat.h */

/*
 * Transitional public header for the combat command split.
 */

#ifndef INCLUDED_CMD_COMBAT_H
#define INCLUDED_CMD_COMBAT_H

#include "h-basic.h"

bool graphics_are_ascii(void);

bool check_hit(int power, bool display_roll);
int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll);
int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker,
    const object_type* o_ptr);
void hit_trap(int y, int x);
void py_attack(int y, int x, int attack_type);
void flanking_or_retreat(int y, int x);

int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2);

#endif /* INCLUDED_CMD_COMBAT_H */
