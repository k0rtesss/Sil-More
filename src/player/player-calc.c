/* File: player/player-calc.c */

#include "player/player-calc.h"

#include "angband.h"
#include "externs.h"

static bool player_has_equipped_flag3(u32b flag3)
{
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            continue;

        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag3)
            return true;
    }

    return false;
}

static bool player_has_inventory_flag3(u32b flag3)
{
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            continue;

        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag3)
            return true;
    }

    return false;
}

static int oath_special_ability_from_oath_num(int oath_num)
{
    switch (oath_num)
    {
    case OATH_MERCY:
        return SPC_OATH_MERCY;
    case OATH_SILENCE:
        return SPC_OATH_SILENCE;
    case OATH_IRON:
        return SPC_OATH_IRON;
    case OATH_SMITH:
        return SPC_OATH_SMITH;
    case OATH_VALOROUS:
        return SPC_OATH_VALOROUS;
    case OATH_LIGHT:
        return SPC_OATH_LIGHT;
    default:
        return -1;
    }
}

#define XTRA1_SECTION_PLAYER_CALC_TOP
#include "xtra1-body.inc"
#undef XTRA1_SECTION_PLAYER_CALC_TOP

#define XTRA1_SECTION_PLAYER_CALC_BOTTOM
#include "xtra1-body.inc"
#undef XTRA1_SECTION_PLAYER_CALC_BOTTOM
