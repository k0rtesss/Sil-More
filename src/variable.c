/* File: variable.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "h-basic.h"
#include "init.h"

/*
 * Remaining transitional globals that do not yet have a better ownership
 * module in this pass.
 */
bool use_sound;
s16b image_count;
s16b object_level;
bool shimmer_objects;

/*
 * Standard sound (and message) names
 */
const cptr angband_sound_name[MSG_MAX] = {
    "",
    "hit",
    "miss",
    "flee",
    "drop",
    "kill",
    "level",
    "death",
    "study",
    "teleport",
    "shoot",
    "quaff",
    "zap",
    "walk",
    "tpother",
    "hitwall",
    "eat",
    "store1",
    "store2",
    "store3",
    "store4",
    "dig",
    "opendoor",
    "shutdoor",
    "bashdoor",
    "pick",
    "tplevel",
    "bell",
    "nothing_to_open",
    "lockpick_fail",
    "stairs",
    "hitpoint_warn",
    "weapon_slash_light",
    "weapon_slash_heavy",
    "weapon_thrust",
    "weapon_blunt",
    "weapon_unarmed",
    "armor",
    "weapon_slash_medium",
    "equip_sword",
    "equip_bow",
    "equip_weapon",
    "equip_mail",
    "equip_leather",
    "equip_armor",
    "equip_jewelry",
    "unequip_sword",
    "unequip_bow",
    "unequip_weapon",
    "unequip_mail",
    "unequip_leather",
    "unequip_armor",
    "unequip_jewelry",
    "drop_glass",
    "drop_small_metal",
    "drop_cloth",
    "drop_leather",
    "drop_big_metal",
    "drop_metal_medium",
    "drop_wood",
    "drop_generic",
    "use_gem",
    "activate",
    "monster_attack",
    "monster_attack_ranged",
    "monster_attack_breath",
};

bool (*ang_sort_comp)(const void* u, const void* v, int a, int b);
void (*ang_sort_swap)(void* u, void* v, int a, int b);
bool (*get_mon_num_hook)(int r_idx);
bool (*get_obj_num_hook)(int k_idx);
void (*object_info_out_flags)(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
