#ifndef INCLUDED_PLAYER_XP_H
#define INCLUDED_PLAYER_XP_H

#include "h-basic.h"

typedef struct monster_race monster_race;
typedef struct monster_type monster_type;

void check_experience(void);
s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill);
void gain_exp(s32b amount);
void lose_exp(s32b amount);
void scare_onlooking_friends(const monster_type* m_ptr, int amount);

#endif /* INCLUDED_PLAYER_XP_H */
