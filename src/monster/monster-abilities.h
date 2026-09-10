#ifndef MONSTER_ABILITIES_H
#define MONSTER_ABILITIES_H

#include "angband.h"

/* These bracket every energy-consuming action, including sleep and recovery.
 * begin owns the history shift; callers must not shift it a second time. */
void monster_abilities_begin_action(monster_type* m_ptr);
void monster_abilities_end_action(monster_type* m_ptr, int old_y, int old_x,
    bool skipped);
void monster_abilities_mark_melee(monster_type* m_ptr, bool ordinary);
/* Call for displacement imposed by another actor, including knockback during
 * this creature's own action. Ordinary voluntary movement must not call it. */
void monster_abilities_forced_movement(monster_type* m_ptr);

bool monster_moved_last_action(const monster_type* m_ptr);
bool monster_sprinting(const monster_type* m_ptr);
bool monster_abilities_can_react(const monster_type* m_ptr);

int monster_concentration_bonus(const monster_type* m_ptr, bool ordinary);
int monster_dodging_bonus(const monster_type* m_ptr);
/* Extra dice only: ordinary protection already includes shield_dd once. */
int monster_blocking_bonus_dice(const monster_type* m_ptr);
int monster_vengeance_bonus_dice(const monster_type* m_ptr);
void monster_receive_melee_damage(monster_type* m_ptr, int damage);
/* Call after a landed melee hit, even if protection absorbs all damage. */
void monster_consume_vengeance(monster_type* m_ptr);
/* Call before the hit roll. A successful choice pays even on a miss. */
bool monster_try_smite(monster_type* m_ptr, bool ordinary);

#endif
