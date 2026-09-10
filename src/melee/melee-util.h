#ifndef INCLUDED_MELEE_UTIL_H
#define INCLUDED_MELEE_UTIL_H

#include "../h-basic.h"

typedef struct monster_type monster_type;
typedef struct monster_race monster_race;

extern int get_scent(int y, int x);
extern bool cave_exist_mon(
    monster_race* r_ptr, int y, int x, bool occupied_ok, bool can_dig);
extern int cave_passable_mon(monster_type* m_ptr, int y, int x, bool* bash);
extern int monster_step_cost(monster_type* m_ptr,
    int from_y, int from_x, int to_y, int to_x);
extern int monster_terrain_penalty(monster_type* m_ptr, int y, int x);
extern int monster_poison_step_damage(monster_type* m_ptr,
    int from_y, int from_x, int to_y, int to_x);
extern bool attacker_at(int y, int x);
extern int adj_mon_count(int y, int x);
extern void tell_allies(int y, int x, u32b flag);

#endif /* INCLUDED_MELEE_UTIL_H */
