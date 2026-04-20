/* File: level-generation-rooms-vault-advanced.c */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool build_vault(int y0, int x0, vault_type* v_ptr, bool flip_d);
bool place_room(int y0, int x0, vault_type* v_ptr);
bool try_place_docked_vault(vault_type* v_ptr, int* placed_y, int* placed_x);

/*
 * Mark greater vault grids with the CAVE_G_VAULT flag.
 * Returns true if it succeds.
 */
static bool mark_g_vault(int y0, int x0, int ymax, int xmax)
{
    int y1, x1, y2, x2, y, x;

    /* Get the coordinates */
    y1 = y0 - ymax / 2;
    x1 = x0 - xmax / 2;
    y2 = y1 + ymax - 1;
    x2 = x1 + xmax - 1;

    /* Step 1 - Mark all grids inside that perimeter with the new flag */
    for (y = y1 + 1; y < y2; y++)
    {
        for (x = x1 + 1; x < x2; x++)
        {
            cave_info[y][x] |= (CAVE_G_VAULT);
        }
    }

    return true;
}

static bool vault_type8_is_repeated(s16b v_idx)
{
    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == v_idx)
            return true;
    }

    return false;
}

static bool vault_type8_is_eligible(s16b v_idx, bool test_only)
{
    vault_type* v_ptr = &v_info[v_idx];

    if (v_ptr->typ != 8)
        return false;
    if (v_ptr->flags & VLT_QUEST)
        return false;
    if (v_ptr->depth > p_ptr->depth)
        return false;
    if (v_ptr->max_depth != 0 && p_ptr->depth > v_ptr->max_depth)
        return false;
    if (vault_type8_is_repeated(v_idx))
        return false;
    if (test_only && !(v_ptr->flags & VLT_TEST))
        return false;

    return true;
}

static bool any_eligible_type8_test_vault(void)
{
    for (int i = 0; i < z_info->v_max; i++)
    {
        if (vault_type8_is_eligible(i, false) && (v_info[i].flags & VLT_TEST))
            return true;
    }

    return false;
}

static bool choose_reserved_type8(vault_type** out_v_ptr, s16b* out_v_idx)
{
    int tries = 0;
    bool test_only = any_eligible_type8_test_vault();

    while (tries++ < 2000)
    {
        s16b v_idx = rand_int(z_info->v_max);
        vault_type* v_ptr = &v_info[v_idx];

        if (!vault_type8_is_eligible(v_idx, test_only))
            continue;

        if (!one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth)))
            continue;

        *out_v_ptr = v_ptr;
        *out_v_idx = v_idx;
        return true;
    }

    return false;
}

static bool place_type8_vault(int y0, int x0, vault_type* v_ptr, s16b v_idx)
{
    bool placed = false;
    int placed_y = y0;
    int placed_x = x0;

    level_gen_debug_note_greater_vault_name(v_name + v_ptr->name);
    if (one_in_(2) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;

    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == 0)
        {
            p_ptr->greater_vaults[i] = v_idx;
            break;
        }
    }

    if (cheat_room)
        msg_format("GV (%s).", v_name + v_ptr->name);

    if (mark_g_vault(placed_y, placed_x, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return true;
}

bool build_reserved_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    s16b v_idx = 0;

    if (g_vault_name[0] != '\0')
        return false;

    if (!choose_reserved_type8(&v_ptr, &v_idx))
        return false;

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 8 -- greater vaults (see "vault.txt")
 */
bool build_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    int tries = 0;
    bool found = false;
    bool repeated = false;
    int i;
    s16b v_idx;
    bool prefer_test = any_eligible_type8_test_vault();

    /* Can only have one greater vault per level */
    if (g_vault_name[0] != '\0')
        return false;

    /* Pick a greater vault */
    while (!found)
    {
        tries++;

        /* Get a random vault record */
        v_idx = rand_int(z_info->v_max);
        v_ptr = &v_info[v_idx];

        /* Try additional times to place any vault marked TEST */
        if (prefer_test && (tries < 1000) && !(v_ptr->flags & VLT_TEST))
            continue;

        /* Accept the first greater vault (but not quest vaults) */
        if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth)
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
            && (one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth)))
            && !(v_ptr->flags & VLT_QUEST))
        {
            repeated = false;
            for (i = 0; i < MAX_GREATER_VAULTS; i++)
            {
                if (v_idx == p_ptr->greater_vaults[i])
                    repeated = true;
            }

            if (!repeated)
                found = true;
        }

        if (tries > 2000)
            return false;
    }

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 9 -- Morgoth's vault (see "vault.txt")
 */
bool build_type9(int y0, int x0, vault_type** used_vault)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a version of Morgoth's vault */
    while (true)
    {
        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        /* Accept the first morgoth vault */
        if (v_ptr->typ == 9)
            break;

        tries++;
        if (tries > 10000)
        {
            msg_format(
                "Could not find a record for Morgoth's Vault in vault.txt");
            return false;
        }
    }

    if (used_vault)
        *used_vault = v_ptr;

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
        return false;

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return true;
}

bool build_type10(int y0, int x0)
{
    vault_type* v_ptr;

    /* Get the first vault record */
    v_ptr = &v_info[1];

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
        return false;

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return true;
}

int vault_type8_generation_rarity(const vault_type* v_ptr, int depth)
{
    int rarity = v_ptr->rarity;

    if ((depth >= 6) && (v_ptr->flags & VLT_SURFACE))
        rarity += (1 << depth);

    return rarity;
}

bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth)
{
    if (v_ptr->typ == 6)
    {
        if (depth < 6)
        {
            if (!(v_ptr->flags & VLT_SURFACE) && !one_in_(4))
                return false;
        }
        else if (v_ptr->flags & VLT_SURFACE)
        {
            if (!one_in_(1 << depth))
                return false;
        }
    }
    else if ((depth >= 6) && (v_ptr->flags & VLT_SURFACE))
    {
        if (!one_in_(1 << depth))
            return false;
    }

    return true;
}
