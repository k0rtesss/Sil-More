/* File: spell/spell-teleport.h */

/*
 * Teleportation spells and effects.
 */

#ifndef INCLUDED_SPELL_TELEPORT_H
#define INCLUDED_SPELL_TELEPORT_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

void teleport_away(int m_idx, int dis);
void teleport_player(int dis);
void teleport_player_to(int ny, int nx);
void teleport_towards(int oy, int ox, int ny, int nx);
void teleport_player_level(void);
void stun_monster(monster_type* m_ptr, int stun);

#endif /* INCLUDED_SPELL_TELEPORT_H */
