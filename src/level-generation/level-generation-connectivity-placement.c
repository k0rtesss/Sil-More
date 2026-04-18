/* File: level-generation-connectivity-placement.c */

#include "angband.h"
#include "level-generation/level-generation.h"
#include "log/log.h"
#include "level-generation/gen-log.h"
#include "metarun.h"
#include "level-generation/level-generation-internal.h"
#include <string.h>

/* Dungeon streamer generation values */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

int choose_up_stairs(void);
int choose_down_stairs(void);
void place_item_randomly(int tval, int sval, bool close);
bool build_streamer(int feat);
void build_chasms(void);
bool alloc_stairs(int feat, int num);

static void place_rubble(int y, int x)
{
    /* Create rubble */
    if (p_ptr->depth >= 4 && cave_feat[y][x] != FEAT_MORE
        && cave_feat[y][x] != FEAT_LESS)
        cave_set_feat(y, x, FEAT_RUBBLE);
}

/*
 * Choose either an ordinary up staircase or an up shaft.
 */
int choose_up_stairs(void)
{
    if (p_ptr->depth >= 2)
    {
        if (one_in_(2) || p_ptr->on_the_run)
            return (FEAT_LESS_SHAFT);
    }

    return (FEAT_LESS);
}

/*
 * Choose either an ordinary down staircase or an down shaft.
 */
int choose_down_stairs(void)
{
    if (p_ptr->depth < MORGOTH_DEPTH - 2)
    {
        if (one_in_(2) || p_ptr->on_the_run)
            return (FEAT_MORE_SHAFT);
    }

    return (FEAT_MORE);
}

/*
 * Calculate the distance from a point to the nearest down stair on the level.
 * Returns -1 if no down stair is found.
 */
int calculate_nearest_down_stair_distance_from(int y0, int x0)
{
    int min_distance = 9999;
    bool found_down = false;

    if (!in_bounds_fully(y0, x0))
        return -1;

    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_feat[y][x] == FEAT_MORE || cave_feat[y][x] == FEAT_MORE_SHAFT)
            {
                int dist = distance(y0, x0, y, x);
                found_down = true;
                if (dist < min_distance)
                    min_distance = dist;
            }
        }
    }

    if (!found_down)
        return -1;

    return min_distance;
}

/*
 * Place an up/down staircase at given location
 */
void place_random_stairs(int y, int x)
{
    /* Paranoia */
    if (!cave_clean_bold(y, x))
        return;

    /* Create a staircase */
    if (!p_ptr->depth)
    {
        cave_set_feat(y, x, FEAT_MORE);
    }
    else if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        if (one_in_(2))
            cave_set_feat(y, x, FEAT_LESS);
        else
            cave_set_feat(y, x, FEAT_LESS_SHAFT);
    }
    else if (one_in_(2))
    {
        if (p_ptr->depth <= 1)
            cave_set_feat(y, x, FEAT_MORE);
        else if (one_in_(2))
            cave_set_feat(y, x, FEAT_MORE);
        else
            cave_set_feat(y, x, FEAT_MORE_SHAFT);
    }
    else
    {
        if ((one_in_(2)) || (p_ptr->depth == 1))
            cave_set_feat(y, x, FEAT_LESS);
        else
            cave_set_feat(y, x, FEAT_LESS_SHAFT);
    }
}

static bool wearable_p(const object_type *o_ptr)
{
    /* INVEN_WIELD is the first equipment slot (see defines.h)           */
    /* Anything that gets a slot number below that lives in inventory.    */
    return (wield_slot(o_ptr) >= INVEN_WIELD);
}

/*
 * Generate the chosen item at a random spot in the dungeon.
 * If 'close' is true, it must be nearby and in line-of-sight of the player.
 */
void place_item_randomly(int tval, int sval, bool close)
{
    object_type* i_ptr;
    object_type object_type_body;
    int y, x;
    int i;
    s16b k_idx;

    if (close)
    {
        for (i = 0; i < 1000; i++)
        {
            y = p_ptr->py + rand_range(-5, +5);
            x = p_ptr->px + rand_range(-5, +5);

            if (cave_naked_bold(y, x) && los(p_ptr->py, p_ptr->px, y, x)
                && (cave_info[y][x] & (CAVE_ROOM)))
            {
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < 1000; i++)
        {
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            if (cave_naked_bold(y, x))
            {
                break;
            }
        }
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Get the object_kind */
    k_idx = lookup_kind(tval, sval);

    /* Valid item? */
    if (!k_idx)
        return;

    /* Paranoia regarding having found a spot */
    if (i == 1000)
        return;

    /* Prepare the item */
    object_prep(i_ptr, k_idx);

    /* Escape-curse: higher chance of cursed finds */
    {
        int stacks = curse_flag_count_cur(CUR_FINDCURSE);
        if (stacks && wearable_p(i_ptr))
        {
            int chance = 20 >> stacks;         /* base 1-in-20 ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚ÂÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¥ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â  1-in-10 ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚ÂÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¥ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â  1-in-5 */
            if (!chance || one_in_(chance))
                add_random_curse(i_ptr);
        }
    }


    if (tval == TV_ARROW)
    {
        i_ptr->number = (byte)24;
    }
    else
    {
        i_ptr->number = (byte)1;
    }

    drop_near(i_ptr, 0, y, x);
}

static void alloc_object_global(int set, int typ, int num, bool out_of_sight)
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
            if (!cave_naked_bold(y, x))
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
            place_object_with_profile_params(
                y, x, object_level, object_level, DROP_QUALITY_NORMAL,
                DROP_TYPE_UNTHEMED, false, 1, 0, &active_profile);
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
 * Places a single chasm
 */
static bool build_chasm(void)
{
    int i;
    int y, x;
    int main_dir, new_dir;
    int length;
    int floor_to_chasm;

    bool chasm_ok = false;

    while (!chasm_ok)
    {
        // choose starting point
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);

        // choose a random cardinal direction for it to run in
        main_dir = ddd[rand_int(4)];

        // choose a random length for it
        length = damroll(4, 8);

        // determine its shape
        for (i = 0; i < length; i++)
        {
            // go in a random direction half the time
            if (one_in_(2))
            {
                // choose the random cardinal direction
                new_dir = ddd[rand_int(4)];
                y += ddy[new_dir];
                x += ddx[new_dir];
            }

            // go straight ahead the other half
            else
            {
                y += ddy[main_dir];
                x += ddx[main_dir];
            }

            // stop near dungeon edge
            if ((y < 3) || (y > p_ptr->cur_map_hgt - 3) || (x < 3)
                || (x > p_ptr->cur_map_wid - 3))
                break;

            // mark that we want to put a chasm here
            cave_info[y][x] |= (CAVE_TEMP);
        }

        // start by assuming it will be OK
        chasm_ok = true;

        // count floor squares that will be turned to chasm
        floor_to_chasm = 0;

        // check it doesn't wreck the dungeon
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        {
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
            {
                // only inspect squares that are currently destined to be chasms
                if (cave_info[y][x] & (CAVE_TEMP))
                {
                    // avoid chasms in interesting rooms / vaults
                    if (cave_info[y][x] & (CAVE_ICKY))
                    {
                        chasm_ok = false;
                    }

                    // avoid two chasm square in a row in corridors
                    if ((cave_info[y + 1][x] & (CAVE_TEMP))
                        && !(cave_info[y][x] & (CAVE_ROOM))
                        && !(cave_info[y + 1][x] & (CAVE_ROOM))
                        && cave_floorlike_bold(y, x)
                        && cave_floorlike_bold(y + 1, x))
                    {
                        chasm_ok = false;
                    }
                    if ((cave_info[y][x + 1] & (CAVE_TEMP))
                        && !(cave_info[y][x] & (CAVE_ROOM))
                        && !(cave_info[y][x + 1] & (CAVE_ROOM))
                        && cave_floorlike_bold(y, x)
                        && cave_floorlike_bold(y, x + 1))
                    {
                        chasm_ok = false;
                    }

                    // avoid a chasm taking out the rock next to a door
                    if (cave_any_closed_door_bold(y + 1, x)
                        || cave_any_closed_door_bold(y - 1, x)
                        || cave_any_closed_door_bold(y, x + 1)
                        || cave_any_closed_door_bold(y, x - 1))
                    {
                        chasm_ok = false;
                    }

                    // avoid a chasm just hitting the wall of a lit room (would
                    // look odd that the light doesn't hit the wall behind)
                    if (cave_wall_bold(y, x) && (cave_info[y][x] & (CAVE_GLOW)))
                    {
                        if ((cave_wall_bold(y + 1, x)
                                && !(cave_info[y + 1][x] & (CAVE_GLOW))
                                && !(cave_info[y + 1][x] & (CAVE_TEMP)))
                            || (cave_wall_bold(y - 1, x)
                                && !(cave_info[y - 1][x] & (CAVE_GLOW))
                                && !(cave_info[y - 1][x] & (CAVE_TEMP)))
                            || (cave_wall_bold(y, x + 1)
                                && !(cave_info[y][x + 1] & (CAVE_GLOW))
                                && !(cave_info[y][x + 1] & (CAVE_TEMP)))
                            || (cave_wall_bold(y, x - 1)
                                && !(cave_info[y][x - 1] & (CAVE_GLOW))
                                && !(cave_info[y][x - 1] & (CAVE_TEMP))))
                        {
                            chasm_ok = false;
                        }
                    }

                    // avoid a chasm having no squares in a room/corridor
                    if (cave_floor_bold(y, x))
                    {
                        floor_to_chasm++;
                    }
                }
            }
        }

        // the chasm must affect at least one floor square
        if (floor_to_chasm < 1)
            chasm_ok = false;

        // clear the flag for failed chasm placement
        if (!chasm_ok)
        {
            for (y = 0; y < p_ptr->cur_map_hgt; y++)
            {
                for (x = 0; x < p_ptr->cur_map_wid; x++)
                {
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        cave_info[y][x] &= ~(CAVE_TEMP);
                    }
                }
            }
        }
    }

    // actually place the chasm
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_info[y][x] & (CAVE_TEMP))
            {
                cave_set_feat(y, x, FEAT_CHASM);
            }
        }
    }

    // clear the temporary chasm marker
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            cave_info[y][x] &= ~(CAVE_TEMP);
        }
    }

    return (true);
}

/*
 * Places chasms through dungeon
 */
void build_chasms(void)
{
    int i;
    int chasms = 0;
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT)
        * (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    // determine whether to add chasms, and how many
    if ((p_ptr->depth > 2) && (p_ptr->depth < MORGOTH_DEPTH)
        && percent_chance(p_ptr->depth + 30))
    {
        // add some chasms
        chasms += damroll(1, panels / 3);

        // flip a coin, and if it is heads...
        while (one_in_(2))
        {
            // add some more chasms and flip again...
            chasms += damroll(1, panels / 3);
        }
    }

    if (chasms > 12)
        chasms = 12;

    // build them
    for (i = 0; i < chasms; i++)
    {
        build_chasm();
    }

    if (cheat_room && (chasms > 0))
        msg_format("%d chasms.", chasms);
}

bool alloc_stairs(int feat, int num)
{
    int x;

    /* Stairs can be placed anywhere on the map - rooms or corridors */
    
    /* Place "num" stairs */
    for (x = 0; x < num; x++)
    {
        int i;

        int yy, xx;

        for (i = 0; i < 1000; i++)
        {
            yy = rand_int(p_ptr->cur_map_hgt);
            xx = rand_int(p_ptr->cur_map_wid);

            /* make sure the square is empty (floor) and has no adjacent doors */
            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx))
                if ((cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD))
                {
                    break;
                }
        }
        
        /* Failed to find valid location after 1000 attempts */
        if (i == 1000)
        {
            log_trace("alloc_stairs: Failed to find valid location for stair %d/%d after 1000 attempts", x+1, num);
            return (false);
        }

        /* Surface -- must go down */
        if (!p_ptr->depth)
        {
            /* Clear previous contents, add down stairs */
            cave_set_feat(yy, xx, FEAT_MORE);
        }

        /* Bottom -- must go up */
        else if (p_ptr->depth >= MORGOTH_DEPTH)
        {
            /* Clear previous contents, add up stairs */
            if (x != 0)
                cave_set_feat(yy, xx, FEAT_LESS);
            else
                cave_set_feat(yy, xx, choose_up_stairs());
        }

        /* Requested type */
        else
        {
            /* Allow shafts, but guarantee the first one is an ordinary stair */
            if (x != 0)
            {
                if (feat == FEAT_LESS)
                    feat = choose_up_stairs();
                else if (feat == FEAT_MORE)
                    feat = choose_down_stairs();
            }

            /* Clear previous contents, add stairs */
            cave_set_feat(yy, xx, feat);
        }
    }

    return (true);
}

static bool feat_within_los(int y0, int x0, int feat)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = y0 - 15; y < y0 + 15; y++)
    {
        for (x = x0 - 15; x < x0 + 15; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!los(y0, x0, y, x))
                continue;

            /* Detect invisible traps */
            if (cave_feat[y][x] == feat)
            {
                detect = true;
            }
        }
    }

    /* Result */
    return (detect);
}

/*
 * Are there any stairs within line of sight?
 */
static bool stairs_within_los(int y, int x)
{
    if (feat_within_los(y, x, FEAT_LESS))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE))
        return (true);
    if (feat_within_los(y, x, FEAT_LESS_SHAFT))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE_SHAFT))
        return (true);

    // else:

    return (false);
}

/*
 * Determines the chance (out of 1000) that a trap will be placed in a given
 * square.
 */
int trap_placement_chance(int y, int x)
{
    int yy, xx;

    int chance = 0;
    /* extra traps from CUR_TRAPS */
    int bonus_traps = curse_flag_count_cur(CUR_TRAPS);
    if (bonus_traps)
        chance += 10 * bonus_traps;   /* +10/20/30 ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚ÂÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¡aÃƒâ€šÃ‚Âª on top of normal */

    // extra chance of having a trap for certain squares inside rooms
    if (cave_clean_bold(y, x) && (cave_info[y][x] & (CAVE_ROOM)))
    {
        chance = 1;

        // check the squares that neighbour (y,x)
        for (yy = y - 1; yy <= y + 1; yy++)
        {
            for (xx = x - 1; xx <= x + 1; xx++)
            {
                if (!((yy == y) && (xx == x)))
                {
                    // item
                    if (cave_o_idx[yy][xx] != 0)
                        chance += 10;

                    // stairs
                    if (cave_stair_bold(yy, xx))
                        chance += 10;

                    // closed doors (including secret)
                    if (cave_any_closed_door_bold(yy, xx))
                        chance += 10;
                }
            }
        }

        // opposing impassable squares (chasm or wall)
        if (cave_impassable_bold(y - 1, x) && cave_impassable_bold(y + 1, x))
            chance += 10;
        if (cave_impassable_bold(y, x - 1) && cave_impassable_bold(y, x + 1))
            chance += 10;
    }

    /* Small caves (CA-blob partitions): sprinkle a few extra traps on open cave floors. */
    if (p_ptr->depth >= 8 && cave_clean_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY)
        && (level_partition_kind_for_point(y, x) == LEVEL_PART_CAVEY))
    {
        chance = MAX(chance, 2);
    }

    return (chance);
}

/*
 * Place traps randomly on the level.
 * Biased towards certain sneaky locations.
 */
void place_traps(void)
{
    int y, x;

    // scan the map
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // randomly determine whether to place a trap based on the above
            if (dieroll(1000) <= trap_placement_chance(y, x))
            {
                place_trap(y, x);
            }
        }
    }
}

bool place_rubble_player(void)
{
    int r;
    int y, x;
    int i, panels;
    bool niena_level = (quest_lottery_winner == QUEST_ID_NIENA);

    /* Basic "amount" */

    panels = (p_ptr->cur_map_hgt / PANEL_HGT)
        * (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    r = dieroll(panels / 3);

    // occasionally produce much more rubble on deep levels
    if ((p_ptr->depth >= 10) && one_in_(10))
        r += panels * 2;

    /* Put some rubble in corridors */
    alloc_object_global(ALLOC_SET_BOTH, ALLOC_TYP_RUBBLE, r, false);

    /* simple way to place player */
    for (i = 0; i <= 100; i++)
    {
        y = rand_int(p_ptr->cur_map_hgt);
        x = rand_int(p_ptr->cur_map_wid);
        // require empty square that isn't in an interesting room or vault
        if (cave_naked_bold(y, x) && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            // require a room if it is the first level
            if ((playerturn > 0) || (cave_info[y][x] & (CAVE_ROOM)))
            {
                // don't generate stairs within line of sight if player arrived
                // using stairs
                if (!stairs_within_los(y, x) || (p_ptr->create_stair == false))
                {
                    if (niena_level)
                    {
                        int nearest_down = calculate_nearest_down_stair_distance_from(y, x);
                        if (nearest_down < 87)
                            continue;
                    }

                    player_place(y, x);
                    break;
                }
            }
        }
        if (i == 100)
        {
            log_trace("place_rubble_player failed: Could not find suitable player placement after 100 attempts");
            return (false);
        }
    }

    /* Niena levels need a long run to the only down stair; do an exhaustive
     * fallback scan before giving up on the level. */
    if (niena_level)
    {
        int nearest_down = calculate_nearest_down_stair_distance_from(p_ptr->py, p_ptr->px);
        if (nearest_down < 87)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                {
                    if (!cave_naked_bold(y, x) || (cave_info[y][x] & CAVE_ICKY))
                        continue;
                    if ((playerturn == 0) && !(cave_info[y][x] & CAVE_ROOM))
                        continue;

                    nearest_down = calculate_nearest_down_stair_distance_from(y, x);
                    if (nearest_down < 87)
                        continue;

                    player_place(y, x);
                    return true;
                }
            }

            log_trace("place_rubble_player failed: Niena level could not find player start at least 87 from the down stair");
            return false;
        }
    }

    return (true);
}
