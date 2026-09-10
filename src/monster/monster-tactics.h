#ifndef INCLUDED_MONSTER_TACTICS_H
#define INCLUDED_MONSTER_TACTICS_H

#include "../h-basic.h"
typedef struct monster_type monster_type;

/* All previews start after the actor's current action-start poison tick.
 * A move applies entry contact, then each future action starts with contact
 * before damage. These functions do not change any game state. */
int monster_ai_poison_preview(int pending, bool pool_contact, bool entry,
    int actions, int* remaining);
int monster_ai_poison_damage(const monster_type* m_ptr, int y, int x, int actions);
bool monster_ai_poison_safe(const monster_type* m_ptr, int y, int x, int actions);
int monster_tactical_move_utility(monster_type* m_ptr);
int monster_tactical_displacement_utility(monster_type* m_ptr, int y, int x,
    bool exchange);

#endif
