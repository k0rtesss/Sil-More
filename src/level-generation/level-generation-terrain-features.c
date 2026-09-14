/* File: level-generation-terrain-features.c */

#include "angband.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-internal.h"

void alloc_object_global(int set, int typ, int num, bool out_of_sight)
{
    int y, x, k, i;

    /* Place some objects */
    for (k = 0; k < num; k++)
    {
        partition_drop_profile active_profile =
            partition_drop_profile_for_mode_source_cfg(
                QUAD_MODE_ROOMY, PARTITION_DROP_SOURCE_FLOOR);
        /* Pick a "legal" spot */
        for (i = 0; i < 10000; i++)
        {
            bool is_room;

            /* Location */
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            /* Require "naked" floor grid */
            if (!cave_naked_bold(y, x)
                && !(typ == ALLOC_TYP_OBJECT
                    && (cave_feat[y][x] == FEAT_WATER
                        || cave_feat[y][x] == FEAT_DEEP_WATER)
                    && !cave_o_idx[y][x] && !cave_m_idx[y][x]))
                continue;
            if (typ == ALLOC_TYP_RUBBLE && terrain_generation_reserved(y, x))
                continue;

            /* Check for "room" */
            is_room = (cave_info[y][x] & (CAVE_ROOM)) ? true : false;

            /* Require corridor? */
            if ((set == ALLOC_SET_CORR) && is_room)
                continue;

            /* Require room? */
            if ((set == ALLOC_SET_ROOM) && !is_room)
                continue;

            /* Require out_of_sight -- actually more than MAX_SIGHT squares away
             */
            if (out_of_sight
                && (distance(p_ptr->py, p_ptr->px, y, x) <= MAX_SIGHT))
                continue;

            /* Enforce room-type and partition-specific drop behaviour */
            quadrant_mode_t mode = drop_mode_for_point(y, x);
            active_profile = partition_drop_profile_for_mode_source_cfg(
                mode, PARTITION_DROP_SOURCE_FLOOR);
            if (typ == ALLOC_TYP_OBJECT)
            {
                if (!active_profile.allow_floor_drops)
                    continue;
            }

            /* Accept it */
            break;
        }

        /* No point found */
        if (i == 10000)
            return;

        /* Place something */
        switch (typ)
        {
        case ALLOC_TYP_RUBBLE:
        {
            place_rubble(y, x);
            break;
        }

        case ALLOC_TYP_OBJECT:
        {
            place_object_with_profile(y, x, &active_profile);
            break;
        }
        }
    }
}

/*
 * Places "streamers" of quartz through dungeon
 */
bool build_streamer(int feat)
{
    int i, tx, ty;
    int y, x, dir;
    int tries1 = 0;
    int tries2 = 0;

    /* Hack -- Choose starting point */
    y = rand_spread(p_ptr->cur_map_hgt / 2, 10);
    x = rand_spread(p_ptr->cur_map_wid / 2, 15);

    /* Choose a random compass direction */
    dir = ddd[rand_int(8)];

    /* Place streamer into dungeon */
    while (true)
    {
        tries1++;

        if (tries1 > 2500)
            return (false);

        /* One grid per density */
        for (i = 0; i < DUN_STR_DEN; i++)
        {
            int d = DUN_STR_RNG;

            /* Pick a nearby grid */
            while (true)
            {
                tries2++;
                if (tries2 > 2500)
                    return (false);
                ty = rand_spread(y, d);
                tx = rand_spread(x, d);
                if (!in_bounds(ty, tx))
                    continue;
                break;
            }

            /* Only convert "granite" walls */
            if (cave_feat[ty][tx] < FEAT_WALL_EXTRA)
                continue;
            if (cave_feat[ty][tx] > FEAT_WALL_SOLID)
                continue;

            /* Clear previous contents, add proper vein type */
            cave_set_feat(ty, tx, feat);
        }

        /* Advance the streamer */
        y += ddy[dir];
        x += ddx[dir];

        /* Stop at dungeon edge */
        if (!in_bounds(y, x))
            break;
    }

    return (true);
}

/*
 * Generate helper -- test a rectangle to see if it is all rock (i.e. not floor
 * and not icky)
 */
bool solid_rock(int y1, int x1, int y2, int x2)
{
    if (!terrain_history_vault_fits(y1, x1, y2, x2)) return false;
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Sil
 * Generate helper -- test around a rectangle to see if there would be a doubled
 * wall
 *
 * eg:
 *       ######
 * #######....#
 * #....##....#
 * #....#######
 * ######
 */
bool doubled_wall(int y1, int x1, int y2, int x2)
{
    int y, x;

    /* check top wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y1 - 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y1 - 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check bottom wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y2 + 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y2 + 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check left wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x1 - 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x1 - 2] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check right wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x2 + 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x2 + 2] == FEAT_WALL_OUTER))
            return (true);
    }

    return (false);
}

/*
 * Generate helper -- create a new room with optional light
 */
void generate_room(int y1, int x1, int y2, int x2, int light)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_info[y][x] |= (CAVE_ROOM);
            if (light)
                cave_info[y][x] |= (CAVE_GLOW);
        }
    }
}

/*
 * Generate helper -- fill a rectangle with a feature
 */
void generate_fill(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_set_feat(y, x, feat);
        }
    }
}

/*
 * Generate helper -- draw a rectangle with a feature
 */
void generate_draw(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x1, feat);
        cave_set_feat(y, x2, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y1, x, feat);
        cave_set_feat(y2, x, feat);
    }
}

/*
 * Generate helper -- split a rectangle with a feature
 */
void generate_plus(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;
    int y0, x0;

    /* Center */
    y0 = (y1 + y2) / 2;
    x0 = (x1 + x2) / 2;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x0, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y0, x, feat);
    }
}
