#ifndef SIL_QUEST_CHALLENGES_H
#define SIL_QUEST_CHALLENGES_H
#include "angband.h"
void quest_challenge_unlock_for_quest(int quest);
void quest_challenge_validate(void);
bool quest_challenge_object_allowed(const object_type *object);
bool quest_challenge_forbid_ranged(void);
bool quest_challenge_weapon_allowed(const object_type *weapon);
int quest_challenge_melee_damage(const object_type *weapon, int damage);
void quest_challenge_prune_stairs(void);
void quest_debug_choose_challenge(void);
#endif
