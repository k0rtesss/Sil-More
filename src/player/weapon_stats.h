#ifndef INCLUDED_PLAYER_WEAPON_STATS_H
#define INCLUDED_PLAYER_WEAPON_STATS_H

#include "h-basic.h"

typedef struct object_type object_type;

byte total_mdd(const object_type* o_ptr);
byte strength_modified_ds(const object_type* o_ptr, int str_adjustment);
byte total_mds(const object_type* o_ptr, int str_adjustment);
bool two_handed_melee(void);
int hand_and_a_half_bonus(const object_type* o_ptr);
int bow_bonus(const object_type* o_ptr);
int axe_bonus(const object_type* o_ptr);
int polearm_bonus(const object_type* o_ptr);
byte total_ads(const object_type* j_ptr);

#endif /* INCLUDED_PLAYER_WEAPON_STATS_H */
