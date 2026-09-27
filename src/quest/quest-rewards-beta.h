#ifndef SIL_QUEST_REWARDS_BETA_H
#define SIL_QUEST_REWARDS_BETA_H
#include "angband.h"
void quest_beta_apply_reward(int quest_id);
void quest_beta_birth(void);
bool quest_beta_resurrect(void);
bool quest_beta_cleanse_curse(void);
void quest_beta_bow_hit(int damage);
void quest_beta_bow_miss(void);
int quest_beta_melee_damage(const object_type *weapon, int damage);
void quest_beta_melee_miss(void);
#endif
