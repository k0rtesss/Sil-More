/* File: level-generation-rooms.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

/*
 * Generate helper -- test a rectangle to see if it is all rock (i.e. not floor
 * and not icky)
 */
static bool solid_rock(int y1, int x1, int y2, int x2)
{
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
static bool doubled_wall(int y1, int x1, int y2, int x2)
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
static void generate_room(int y1, int x1, int y2, int x2, int light)
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
static void generate_fill(int y1, int x1, int y2, int x2, int feat)
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
static void generate_draw(int y1, int x1, int y2, int x2, int feat)
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
static void generate_plus(int y1, int x1, int y2, int x2, int feat)
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

/*
 * Room building routines.
 *
 * Six basic room types:
 *   1 -- normal
 *   2 -- cross shaped
 *   3 -- (removed)
 *   4 -- large room with features (removed)
 *   5 -- monster nests (removed)
 *   6 -- least vaults (formerly: monster pits)
 *   7 -- lesser vaults
 *   8 -- greater vaults
 */

/*
 * Type 1 -- normal rectangular rooms
 */
bool build_type1(int y0, int x0)
{
    int y, x;

    int y1, x1, y2, x2;

    int light = false;

    // Occasional light - chance of darkness starts very small and
    // increases quadratically until always dark at 950 ft
    if ((p_ptr->depth < dieroll(MORGOTH_DEPTH - 1))
        || (p_ptr->depth < dieroll(MORGOTH_DEPTH - 1)))
    {
        light = true;
    }

    /* Pick a room size */
    y1 = y0 - dieroll(3);
    x1 = x0 - dieroll(5);
    y2 = y0 + dieroll(3);
    x2 = x0 + dieroll(4) + 1;

    /* Sil: bounds checking */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        return (false);
    }

    if (doubled_wall(y1, x1, y2, x2))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1;
    dun->corner[dun->cent_n].x1 = x1;
    dun->corner[dun->cent_n].y2 = y2;
    dun->corner[dun->cent_n].x2 = x2;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CLASSIC;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1 - 1, x1 - 1, y2 + 1, x2 + 1, light);

    /* Generate outer walls */
    generate_draw(y1 - 1, x1 - 1, y2 + 1, x2 + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1, x1, y2, x2, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */
    if (one_in_(20) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                cave_set_feat(y, x, FEAT_WALL_INNER);
            }
        }
    }

    /* Hack -- Occasional pillar-lined room */
    if (one_in_(10) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                if ((x == x1 + 1) || (x == x2 - 1) || (y == y1 + 1)
                    || (y == y2 - 1))
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
        }
    }

    return (true);
}

/*
 * Type 2 -- Cross shaped rooms
 */
bool build_type2(int y0, int x0)
{
    int y, x;

    int y1h, x1h, y2h, x2h;
    int y1v, x1v, y2v, x2v;

    int h_hgt, h_wid, v_hgt, v_wid;

    int light = false;

    /* Occasional light - always at level 1 through to never at Morgoth's level
     */
    if (p_ptr->depth < dieroll(MORGOTH_DEPTH))
        light = true;

    /* Pick a room size */

    h_hgt = 1; /* 3 */
    h_wid = rand_range(5, 7); /* 11, 13, 15 */

    y1h = y0 - h_hgt;
    x1h = x0 - h_wid;
    y2h = y0 + h_hgt;
    x2h = x0 + h_wid;

    v_hgt = rand_range(3, 6); /* 7, 9, 11, 13 */
    v_wid = rand_range(1, 2); /* 3, 5 */

    y1v = y0 - v_hgt;
    x1v = x0 - v_wid;
    y2v = y0 + v_hgt;
    x2v = x0 + v_wid;

    /* Sil: bounds checking */
    if ((y1v <= 3) || (x1h <= 3) || (y2v >= p_ptr->cur_map_hgt - 3)
        || (x2h >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1v - 1, x1h - 1, y2v + 1, x2h + 1))
    {
        return (false);
    }

    if (doubled_wall(y1v, x1h, y2v, x2h))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1v;
    dun->corner[dun->cent_n].x1 = x1h;
    dun->corner[dun->cent_n].y2 = y2v;
    dun->corner[dun->cent_n].x2 = x2h;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CROSS;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1h - 1, x1h - 1, y2h + 1, x2h + 1, light);
    generate_room(y1v - 1, x1v - 1, y2v + 1, x2v + 1, light);

    /* Generate outer walls */
    generate_draw(y1h - 1, x1h - 1, y2h + 1, x2h + 1, FEAT_WALL_OUTER);
    generate_draw(y1v - 1, x1v - 1, y2v + 1, x2v + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1h, x1h, y2h, x2h, FEAT_FLOOR);
    generate_fill(y1v, x1v, y2v, x2v, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */

    switch (dieroll(7))
    {
    case 1:
    {
        if ((v_wid == 2) && (v_hgt == 6))
        {
            for (y = y1v + 1; y <= y2v; y += 2)
            {
                for (x = x1v + 1; x <= x2v; x += 2)
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
            {
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y0, x0), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(y0, x0, object_level, object_level,
                    DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, false, 1, 0, &active_profile);
            }
        }
        break;
    }
    case 2:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            generate_plus(y0 - 1, x0 - 1, y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 3:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0 - 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 4:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0, FEAT_WALL_INNER);
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return (true);
}
