#ifndef INCLUDED_MONSTER_ROUTINE_H
#define INCLUDED_MONSTER_ROUTINE_H

enum monster_routine_style { MON_ROUTINE_NONE, MON_ROUTINE_PENDING, MON_ROUTINE_PATROL };
#define MON_ROUTINE_SAVE_MAGIC 0x5021
/* Shared local limit for territorial work and dragon scent excursions. */
#define MONSTER_LAIR_RADIUS 6

void monster_routine_spawn(monster_type* m);
void monster_routine_finish_level(void);
bool monster_routine_plan(monster_type* m);
bool monster_routine_allows(const monster_type* m, int y, int x);
bool monster_routine_move(monster_type* m, int* y, int* x);
bool monster_routine_valid(const monster_type* m);

#endif
