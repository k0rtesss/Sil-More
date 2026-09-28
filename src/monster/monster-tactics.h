#ifndef INCLUDED_MONSTER_TACTICS_H
#define INCLUDED_MONSTER_TACTICS_H

#include "../h-basic.h"
typedef struct monster_type monster_type;

enum monster_squad_role
{
    MON_SQUAD_NONE, MON_SQUAD_COMMANDER, MON_SQUAD_SOLDIER
};

enum monster_command_grade
{
    MON_COMMAND_NONE, MON_COMMAND_ORDINARY, MON_COMMAND_LEADER, MON_COMMAND_COMMANDER
};
enum monster_command_style { MON_COMMAND_TACTICAL, MON_COMMAND_PACK };
enum monster_command_kin
{
    MON_KIN_NONE, MON_KIN_ORC, MON_KIN_MAN, MON_KIN_ELF, MON_KIN_TROLL,
    MON_KIN_RAUKO, MON_KIN_WOLF, MON_KIN_CAT, MON_KIN_DRAGON, MON_KIN_SPIDER,
    MON_KIN_UNDEAD, MON_KIN_VAMPIRE, MON_KIN_GIANT, MON_KIN_HORROR, MON_KIN_MAX
};
enum monster_squad_job
{
    MON_JOB_NONE, MON_JOB_ADVANCE, MON_JOB_FLANK, MON_JOB_GUARD, MON_JOB_COVER,
    MON_JOB_HOLD, MON_JOB_REGROUP
};
enum monster_squad_plan { MON_PLAN_ADVANCE, MON_PLAN_HOLD, MON_PLAN_REGROUP };

void monster_squad_prepare(void);
bool monster_squad_order_valid(const monster_type* m_ptr);
bool monster_squad_hold_position(const monster_type* m_ptr);

/* All previews start after the actor's current action-start poison tick.
 * A move applies entry contact, then each future action starts with contact
 * before damage. These functions do not change any game state. */
int monster_ai_poison_preview(int pending, bool pool_contact, bool entry,
    int actions, int* remaining);
int monster_ai_poison_damage(const monster_type* m_ptr, int y, int x, int actions);
bool monster_ai_poison_safe(const monster_type* m_ptr, int y, int x, int actions);
int monster_tactical_move_utility(monster_type* m_ptr);
int monster_tactical_displacement_utility(monster_type* m_ptr, int y, int x,
    bool exchange);

#endif
