#ifndef INCLUDED_MONSTER_WORLD_H
#define INCLUDED_MONSTER_WORLD_H

enum monster_world_task {
    MON_WORLD_NONE, MON_WORLD_INVESTIGATE, MON_WORLD_BRIDGE
};

void monster_world_observe(monster_type* m_ptr);
bool monster_world_turn(monster_type* m_ptr);

#endif
