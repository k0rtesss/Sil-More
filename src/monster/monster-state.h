#ifndef INCLUDED_MONSTER_MONSTER_STATE_H
#define INCLUDED_MONSTER_MONSTER_STATE_H

#include "h-basic.h"

extern s16b num_repro;
extern s16b monster_level;
extern char summon_kin_type;

extern bool shimmer_monsters;
extern bool repair_mflag_mark;
extern bool repair_mflag_show;

extern s16b mon_max;
extern s16b mon_cnt;
extern monster_type* mon_list;
extern monster_lore* l_list;
extern u32b mon_power_ave[MAX_DEPTH][CREATURE_TYPE_MAX];

#endif /* INCLUDED_MONSTER_MONSTER_STATE_H */
