#ifndef INCLUDED_PLAYER_ACTIVE_WEAPON_H
#define INCLUDED_PLAYER_ACTIVE_WEAPON_H

#include "../h-basic.h"

typedef struct active_weapon_preview
{
    int attack;
    int dd;
    int ds;
    bool throwing;
    bool has_offhand;
    int offhand_attack;
    int offhand_dd;
    int offhand_ds;
} active_weapon_preview;

bool player_equipped_weapon_preview(int mode, active_weapon_preview* preview);

#endif
