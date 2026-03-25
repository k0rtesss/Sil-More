/* File: spell/spell-monster.h */

/*
 * Monster control spells and monster-facing song effects.
 */

#ifndef INCLUDED_SPELL_MONSTER_H
#define INCLUDED_SPELL_MONSTER_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

bool slow_monsters(int power);
bool sleep_monsters(int power);
bool destroy_traps(int power);
bool open_doors(int power);
bool lock_doors(int power);
void wake_all_monsters(int who);
bool make_aggressive(void);
bool banishment(void);
bool mass_banishment(void);

void song_of_binding(monster_type* m_ptr);
void song_of_piercing(monster_type* m_ptr);
void song_of_oaths(monster_type* m_ptr);
void hatch_spider(monster_type* m_ptr);

#endif /* INCLUDED_SPELL_MONSTER_H */
