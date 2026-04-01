/* File: level-generation-connectivity.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "level-generation/level-generation.h"
#include "externs.h"
#include "log/log.h"
#include "gen-log.h"
#include "metarun.h"
#include "level-generation/level-generation-internal.h"
#include <string.h>

/* Dungeon streamer generation values */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

bool feature_is_any_door(int feat)
{
    return (feat == FEAT_SECRET) || (feat == FEAT_OPEN) || (feat == FEAT_BROKEN)
        || ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL));
}

/* Collapse adjacent doors outside vaults to avoid double-door seams */
int squash_double_doors(void)
{
    int removed = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!feature_is_any_door(cave_feat[y][x])) continue;
            if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /* Only clear east/south neighbors to keep at most one door */
            int ny = y, nx = x + 1;
            if ((nx < p_ptr->cur_map_wid - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
            ny = y + 1; nx = x;
            if ((ny < p_ptr->cur_map_hgt - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
        }
    }
    log_trace("squash_double_doors: converted %d adjacent doors to floor", removed);
    return removed;
}

/* determines whether the player can pass through a given feature */
/* icky locations (inside vaults) are all considered passable.    */
bool player_passable(int y, int x, bool ignore_rubble_and_chasms)
{
    if (!in_bounds_fully(y, x)) return false;

    byte feature = cave_feat[y][x];
    bool icky_interior = (cave_info[y][x] & (CAVE_ICKY))
        && (cave_info[y][x - 1] & (CAVE_ICKY))
        && (cave_info[y][x + 1] & (CAVE_ICKY))
        && (cave_info[y - 1][x] & (CAVE_ICKY))
        && (cave_info[y + 1][x] & (CAVE_ICKY));

    if ((feature < FEAT_WALL_HEAD) || (feature > FEAT_WALL_TAIL))
    {
        return !((feature == FEAT_CHASM) && !ignore_rubble_and_chasms);
    }
    else
    {
        return (feature == FEAT_SECRET)
            || ((feature >= FEAT_DOOR_HEAD) && (feature <= FEAT_DOOR_TAIL))
            || ((feature == FEAT_RUBBLE) && ignore_rubble_and_chasms)
            || icky_interior;
    }
}

/* floodfills access through the dungeon, marking all accessible squares with
 * true */
void flood_access(int y, int x,
    int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    bool ignore_rubble_and_chasms)
{
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    if (!in_bounds_fully(y, x)) return;
    if (access_array[y][x]) return;
    if (!player_passable(y, x, ignore_rubble_and_chasms)) return;

    access_array[y][x] = true;
    queue[tail++] = y * MAX_DUNGEON_WID + x;

    while (head < tail)
    {
        int idx = queue[head++];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + ddy[d];
            int nx = cx + ddx[d];

            if (!in_bounds_fully(ny, nx)) continue;
            if (access_array[ny][nx]) continue;
            if (!player_passable(ny, nx, ignore_rubble_and_chasms)) continue;

            access_array[ny][nx] = true;
            if (tail < (int)N_ELEMENTS(queue))
            {
                queue[tail++] = ny * MAX_DUNGEON_WID + nx;
            }
        }
    }
}

void label_rooms(void)
{
    int i;

    for (i = 0; i < dun->cent_n; i++)
    {
        // cave_feat[dun->corner[i].y1][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y1][dun->corner[i].x2] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x2] = 5 + 1;

        cave_feat[dun->cent[i].y][dun->cent[i].x] = 5 + (i % 10);
        if (i > 9)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1] = 5 + ((i / 10) % 10);
        }
        if (i > 99)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1]
                = 5 + ((i / 100) % 10);
        }
    }
}

// floodfills access through the *graph* of the dungeon
void flood_piece(int n, int piece_num)
{
    int i;

    dun->piece[n] = piece_num;

    for (i = 0; i < dun->cent_n; i++)
    {
        if (dun->connection[n][i] && (dun->piece[i] == 0))
        {
            flood_piece(i, piece_num);
        }
    }
    return;
}

int dungeon_pieces(void)
{
    int piece_num;
    int i;

    // first reset the pieces
    for (i = 0; i < dun->cent_n; i++)
    {
        dun->piece[i] = 0;
    }

    for (piece_num = 1; piece_num <= dun->cent_n; piece_num++)
    {
        // find the next room that doesn't belong to a piece
        for (i = 0; (i < dun->cent_n) && (dun->piece[i] != 0); i++)
            ;

        if (i == dun->cent_n)
        {
            break;
        }
        else
        {
            flood_piece(i, piece_num);
        }
    }

    return (piece_num - 1);
}

/*
 * Convert existing terrain type to rubble
 */
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
static int choose_up_stairs(void)
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
static int choose_down_stairs(void)
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

/* Mode-specific floor-drop tuning for partition scatter */

typedef struct partition_count_rule {
    int divisor;
    int min_count;
    int max_count;
} partition_count_rule;

typedef struct partition_depth_rule {
    int divisor;
    int min_count;
    int max_count;
    int scale_pct_at_depth_20;
    int hard_cap_divisor;
} partition_depth_rule;

typedef struct partition_metal_rule {
    int divisor;
    int min_count;
    int max_count;
    int min_depth;
} partition_metal_rule;

typedef struct partition_rule_config {
    drop_profile profiles[PARTITION_DROP_SOURCE_MAX];
    bool allow_floor_drops;
    int base_monster_scale_num;
    int base_monster_scale_den;
    partition_count_rule direct_monsters;
    partition_depth_rule depth_monsters;
    int room_object_divisor;
    int corridor_object_divisor;
    partition_metal_rule metal_drops;
    char discovery_text[1024];
    char big_cave_discovery_text[BIG_CAVE_TYPE_MAX][1024];
} partition_rule_config;

static partition_rule_config g_partition_rules[LEVEL_PART_MAX];
static bool g_partition_rules_initialized = false;
static const partition_rule_config* partition_config_get(level_partition_kind kind);

static level_partition_kind partition_config_normalize_kind(level_partition_kind kind)
{
    if (kind <= LEVEL_PART_NONE || kind >= LEVEL_PART_MAX)
        return LEVEL_PART_ROOMY;
    return kind;
}

static void partition_config_profile_assign(drop_profile* profile,
    int weapon, int armor, int jewelry, int supply, int potion, int herb,
    int gem, int staff, int misc, int tunneling)
{
    if (!profile)
        return;

    profile->weight_weapon = weapon;
    profile->weight_armor = armor;
    profile->weight_jewelry = jewelry;
    profile->weight_supply = supply;
    profile->supply_potion = potion;
    profile->supply_herb = herb;
    profile->supply_gem = gem;
    profile->supply_staff = staff;
    profile->supply_misc = misc;
    profile->supply_tunneling = tunneling;
}

static void partition_config_set_defaults_for_kind(level_partition_kind kind)
{
    partition_rule_config* cfg;
    drop_profile base_profile;

    kind = partition_config_normalize_kind(kind);
    cfg = &g_partition_rules[kind];

    memset(cfg, 0, sizeof(*cfg));
    drop_profile_default(&base_profile);

    cfg->profiles[PARTITION_DROP_SOURCE_FLOOR] = base_profile;
    cfg->profiles[PARTITION_DROP_SOURCE_CHEST] = base_profile;
    cfg->profiles[PARTITION_DROP_SOURCE_MONSTER] = base_profile;
    cfg->allow_floor_drops = true;
    cfg->base_monster_scale_num = 1;
    cfg->base_monster_scale_den = 1;
    cfg->room_object_divisor = 8;
    cfg->corridor_object_divisor = 12;

    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 30, 10, 20, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 10, 0, 0, 0, 0, 0, 0, 0);
        break;

    case LEVEL_PART_CAVEY:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            0, 0, 0, 100, 0, 0, 12, 3, 6, 1);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            25, 25, 25, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 3;
        cfg->base_monster_scale_den = 1;
        cfg->depth_monsters.divisor = 120;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 18;
        cfg->depth_monsters.scale_pct_at_depth_20 = 140;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 8;
        cfg->corridor_object_divisor = 12;
        cfg->metal_drops.divisor = 300;
        cfg->metal_drops.max_count = 2;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_RUINED:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 35, 0, 25, 7, 2, 1, 3, 15, 2);
        cfg->profiles[PARTITION_DROP_SOURCE_FLOOR].allow_damaged = true;
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 35, 0, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 2;
        cfg->base_monster_scale_den = 1;
        cfg->depth_monsters.divisor = 180;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 14;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        break;

    case LEVEL_PART_LABYRINTH:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            0, 0, 35, 65, 15, 2, 2, 15, 5, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            0, 0, 35, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 2;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 7;
        cfg->direct_monsters.min_count = 8;
        cfg->direct_monsters.max_count = 45;
        cfg->depth_monsters.divisor = 80;
        cfg->depth_monsters.min_count = 0;
        cfg->depth_monsters.max_count = 25;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        break;

    case LEVEL_PART_CHASM:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            40, 30, 20, 10, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 20, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 4;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 38;
        cfg->direct_monsters.min_count = 10;
        cfg->direct_monsters.max_count = 42;
        cfg->depth_monsters.divisor = 38;
        cfg->depth_monsters.min_count = 10;
        cfg->depth_monsters.max_count = 40;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        cfg->metal_drops.divisor = 240;
        cfg->metal_drops.max_count = 4;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_BIG_CAVE:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            20, 20, 15, 45, 0, 2, 8, 2, 0, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            20, 20, 15, 0, 0, 0, 0, 0, 0, 0);
        cfg->base_monster_scale_num = 5;
        cfg->base_monster_scale_den = 1;
        cfg->direct_monsters.divisor = 55;
        cfg->direct_monsters.min_count = 10;
        cfg->direct_monsters.max_count = 36;
        cfg->depth_monsters.divisor = 45;
        cfg->depth_monsters.min_count = 12;
        cfg->depth_monsters.max_count = 40;
        cfg->depth_monsters.scale_pct_at_depth_20 = 133;
        cfg->depth_monsters.hard_cap_divisor = 20;
        cfg->room_object_divisor = 2;
        cfg->corridor_object_divisor = 4;
        cfg->metal_drops.divisor = 500;
        cfg->metal_drops.max_count = 2;
        cfg->metal_drops.min_depth = 8;
        break;

    case LEVEL_PART_NONE:
    case LEVEL_PART_MAX:
    default:
        break;
    }
}

static void partition_config_ensure_initialized(void)
{
    if (g_partition_rules_initialized)
        return;

    partition_config_reset();
}

void partition_config_reset(void)
{
    for (int kind = 0; kind < LEVEL_PART_MAX; ++kind)
        partition_config_set_defaults_for_kind((level_partition_kind)kind);

    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
    g_partition_rules_initialized = true;
}

void partition_config_set_drop_profile(level_partition_kind kind,
    partition_drop_source_t source, const drop_profile* profile)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    if (source < PARTITION_DROP_SOURCE_FLOOR
        || source >= PARTITION_DROP_SOURCE_MAX)
    {
        return;
    }

    if (profile)
        g_partition_rules[kind].profiles[source] = *profile;
    else
        drop_profile_default(&g_partition_rules[kind].profiles[source]);

    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_floor_rules(level_partition_kind kind,
    bool allow_floor_drops)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].allow_floor_drops = allow_floor_drops;
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_base_monster_scale(level_partition_kind kind,
    int numerator, int denominator)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].base_monster_scale_num = MAX(0, numerator);
    g_partition_rules[kind].base_monster_scale_den = MAX(1, denominator);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_direct_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].direct_monsters.divisor = MAX(0, divisor);
    g_partition_rules[kind].direct_monsters.min_count = MAX(0, min_count);
    g_partition_rules[kind].direct_monsters.max_count = MAX(0, max_count);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_depth_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int scale_pct_at_depth_20,
    int hard_cap_divisor)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].depth_monsters.divisor = MAX(0, divisor);
    g_partition_rules[kind].depth_monsters.min_count = MAX(0, min_count);
    g_partition_rules[kind].depth_monsters.max_count = MAX(0, max_count);
    g_partition_rules[kind].depth_monsters.scale_pct_at_depth_20 =
        MAX(0, scale_pct_at_depth_20);
    g_partition_rules[kind].depth_monsters.hard_cap_divisor =
        MAX(1, hard_cap_divisor);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_object_rules(level_partition_kind kind,
    int room_divisor, int corridor_divisor)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].room_object_divisor = MAX(0, room_divisor);
    g_partition_rules[kind].corridor_object_divisor = MAX(0, corridor_divisor);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_metal_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int min_depth)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    g_partition_rules[kind].metal_drops.divisor = MAX(0, divisor);
    g_partition_rules[kind].metal_drops.min_count = MAX(0, min_count);
    g_partition_rules[kind].metal_drops.max_count = MAX(0, max_count);
    g_partition_rules[kind].metal_drops.min_depth = MAX(0, min_depth);
    g_partition_rules[LEVEL_PART_NONE] = g_partition_rules[LEVEL_PART_ROOMY];
}

void partition_config_set_discovery_text(level_partition_kind kind, cptr text)
{
    partition_config_ensure_initialized();

    kind = partition_config_normalize_kind(kind);
    SDL_strlcpy(g_partition_rules[kind].discovery_text, text ? text : "",
        sizeof(g_partition_rules[kind].discovery_text));
}

void partition_config_set_big_cave_discovery_text(big_cave_type_t cave_type,
    cptr text)
{
    partition_config_ensure_initialized();

    if (cave_type <= BIG_CAVE_NONE || cave_type >= BIG_CAVE_TYPE_MAX)
        return;

    SDL_strlcpy(g_partition_rules[LEVEL_PART_BIG_CAVE]
                    .big_cave_discovery_text[cave_type],
        text ? text : "",
        sizeof(g_partition_rules[LEVEL_PART_BIG_CAVE]
                   .big_cave_discovery_text[cave_type]));
}

cptr partition_config_get_discovery_text(level_partition_kind kind,
    big_cave_type_t cave_type)
{
    const partition_rule_config* cfg = partition_config_get(kind);

    if (!cfg)
        return NULL;

    if (kind == LEVEL_PART_BIG_CAVE
        && cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX
        && cfg->big_cave_discovery_text[cave_type][0])
    {
        return cfg->big_cave_discovery_text[cave_type];
    }

    return cfg->discovery_text[0] ? cfg->discovery_text : NULL;
}

static const partition_rule_config* partition_config_get(level_partition_kind kind)
{
    partition_config_ensure_initialized();
    return &g_partition_rules[partition_config_normalize_kind(kind)];
}

quadrant_mode_t partition_mode_for_point(int y, int x)
{
    /* If we're on a loaded level (no generation metadata), infer a reasonable grid and
     * classify big partitions so runtime systems (drops, UI messages) can work. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
    {
        /* Candidate grids matching apply_quadrant_generation_modes() (including its random-orientation cases). */
        int blocks = (PANEL_HGT > 0) ? (p_ptr->cur_map_hgt / PANEL_HGT) : 0;
        int grids[2][2] = {{0, 0}, {0, 0}};
        int grid_count = 0;

        if (blocks > 0)
        {
            if (blocks <= 9)
            {
                grids[0][0] = 2; grids[0][1] = 2; grid_count = 1;
            }
            else if (blocks == 10)
            {
                grids[0][0] = 3; grids[0][1] = 2;
                grids[1][0] = 2; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 13)
            {
                grids[0][0] = 3; grids[0][1] = 3; grid_count = 1;
            }
            else if (blocks == 14)
            {
                grids[0][0] = 3; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 16)
            {
                grids[0][0] = 4; grids[0][1] = 4; grid_count = 1;
            }
            else if (blocks <= 20)
            {
                grids[0][0] = 5; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 5;
                grid_count = 2;
            }
            else
            {
                grids[0][0] = 5; grids[0][1] = 5; grid_count = 1;
            }
        }

        int best_rows = 0, best_cols = 0;
        int best_score = -1000000;
        quadrant_mode_t best_modes[25];
        memset(best_modes, 0, sizeof(best_modes));

        for (int gi = 0; gi < grid_count; ++gi)
        {
            int rows = grids[gi][0];
            int cols = grids[gi][1];
            if (rows <= 0 || cols <= 0)
                continue;

            int count = rows * cols;
            if (count <= 0 || count > 25)
                continue;

            quadrant_mode_t modes_tmp[25];
            for (int i = 0; i < 25; ++i)
                modes_tmp[i] = QUAD_MODE_ROOMY;

            int chasm_parts = 0, labyrinth_parts = 0, cave_parts = 0;
            int total_chasm_tiles = 0, max_chasm_tiles = 0;

            for (int pi = 0; pi < count; ++pi)
            {
                int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
                if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                    continue;

                int tiles = 0;
                int open_tiles = 0;
                int open_dead_ends = 0;
                int open_wide = 0;
                int open_corridor = 0;
                int chasm_tiles = 0;

                for (int yy = y1; yy <= y2; ++yy)
                {
                    for (int xx = x1; xx <= x2; ++xx)
                    {
                        if (!in_bounds_fully(yy, xx))
                            continue;
                        tiles++;

                        if ((cave_info[yy][xx] & CAVE_CHASM_AREA) || (cave_feat[yy][xx] == FEAT_CHASM))
                        {
                            chasm_tiles++;
                            continue;
                        }

                        if (!cave_floorlike_bold(yy, xx))
                            continue;

                        open_tiles++;

                        int n = 0;
                        if (in_bounds_fully(yy - 1, xx) && cave_floorlike_bold(yy - 1, xx) && cave_feat[yy - 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy + 1, xx) && cave_floorlike_bold(yy + 1, xx) && cave_feat[yy + 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx - 1) && cave_floorlike_bold(yy, xx - 1) && cave_feat[yy][xx - 1] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx + 1) && cave_floorlike_bold(yy, xx + 1) && cave_feat[yy][xx + 1] != FEAT_CHASM) n++;

                        if (n <= 1) open_dead_ends++;
                        if (n >= 3) open_wide++;
                        if (n == 2) open_corridor++;
                    }
                }

                total_chasm_tiles += chasm_tiles;
                if (chasm_tiles > max_chasm_tiles) max_chasm_tiles = chasm_tiles;

                quadrant_mode_t picked = QUAD_MODE_ROOMY;

                if (chasm_tiles > 0)
                {
                    picked = QUAD_MODE_CHASM;
                    chasm_parts++;
                }
                else if (tiles > 0 && open_tiles > 0)
                {
                    int open_pct = (open_tiles * 100) / tiles;
                    int wide_pct = (open_wide * 100) / open_tiles;
                    int dead_pct = (open_dead_ends * 100) / open_tiles;
                    int corridor_pct = (open_corridor * 100) / open_tiles;

                    /* BIG_CAVE: lots of open area, many wide tiles (3-4 neighbors). */
                    if (open_pct >= 38 && wide_pct >= 40)
                    {
                        picked = QUAD_MODE_BIG_CAVE;
                        cave_parts++;
                    }
                    /* LABYRINTH: corridor-dominated maze with relatively few open 'wide' tiles. */
                    else if (wide_pct <= 28 && corridor_pct >= 50 && dead_pct >= 8 && open_pct <= 55)
                    {
                        picked = QUAD_MODE_LABYRINTH;
                        labyrinth_parts++;
                    }
                }

                modes_tmp[pi] = picked;
            }

            /* Score grids that keep special features concentrated (avoid splitting a big area across partitions). */
            int score = 0;
            score -= (chasm_parts * 100);
            score -= ((labyrinth_parts + cave_parts) * 20);
            if (total_chasm_tiles > 0)
                score += (max_chasm_tiles * 500) / total_chasm_tiles;

            if (score > best_score)
            {
                best_score = score;
                best_rows = rows;
                best_cols = cols;
                memcpy(best_modes, modes_tmp, sizeof(best_modes));
            }
        }

        if (best_rows > 0 && best_cols > 0)
        {
            int count = best_rows * best_cols;
            remember_partition_grid(best_rows, best_cols, count);
            for (int i = 0; i < count; ++i)
                current_partition_modes[i] = best_modes[i];
            for (int i = count; i < 25; ++i)
                current_partition_modes[i] = QUAD_MODE_ROOMY;

            /* Densities are only used for generation decisions, so default to NORMAL. */
            for (int i = 0; i < 25; ++i)
                current_partition_densities[i] = DENSITY_NORMAL;

            log_trace("Inferred partition grid for runtime: blocks=%d grid=%dx%d score=%d",
                      blocks, best_rows, best_cols, best_score);
        }
    }

    int pi = partition_index_from_point(
        y, x, current_partition_rows, current_partition_cols);
    if (pi >= 0 && pi < current_partition_count)
        return current_partition_modes[pi];
    return QUAD_MODE_ROOMY;
}

/* Determine appropriate drop mode for a location based on partition type. */
quadrant_mode_t drop_mode_for_point(int y, int x)
{
    return partition_mode_for_point(y, x);
}

static level_partition_kind partition_kind_from_mode(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return LEVEL_PART_ROOMY;
    case QUAD_MODE_CAVEY:
        return LEVEL_PART_CAVEY;
    case QUAD_MODE_RUINED:
        return LEVEL_PART_RUINED;
    case QUAD_MODE_LABYRINTH:
        return LEVEL_PART_LABYRINTH;
    case QUAD_MODE_CHASM:
        return LEVEL_PART_CHASM;
    case QUAD_MODE_BIG_CAVE:
        return LEVEL_PART_BIG_CAVE;
    default:
        return LEVEL_PART_NONE;
    }
}

/* Native chasm walkable terrain is the platform/bridge floor carved by the
 * chasm generator, not later boundary openings or rescue corridors. */
static bool chasm_native_walkable_bold(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    return ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
        == (CAVE_ROOM | CAVE_CHASM_AREA));
}

static bool partition_population_floor_bold(quadrant_mode_t mode, int y, int x)
{
    if (generation_escape_tunnel_bold(y, x))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return chasm_native_walkable_bold(y, x);
    return cave_floor_bold(y, x);
}

static bool partition_population_naked_bold(quadrant_mode_t mode, int y, int x)
{
    if (generation_escape_tunnel_bold(y, x))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return chasm_native_walkable_bold(y, x) && cave_naked_bold(y, x);
    return cave_naked_bold(y, x);
}

static bool partition_mode_avoids_corridor_spawns(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_BIG_CAVE:
    case QUAD_MODE_CHASM:
        return true;

    default:
        return false;
    }
}

static const char* quadrant_mode_debug_name(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return "ROOMY";
    case QUAD_MODE_CAVEY:
        return "CAVEY";
    case QUAD_MODE_RUINED:
        return "RUINED";
    case QUAD_MODE_LABYRINTH:
        return "LABYRINTH";
    case QUAD_MODE_CHASM:
        return "CHASM";
    case QUAD_MODE_BIG_CAVE:
        return "BIG_CAVE";
    default:
        return "UNKNOWN";
    }
}

static const char* partition_kind_debug_name(level_partition_kind kind)
{
    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        return "ROOMY";
    case LEVEL_PART_CAVEY:
        return "CAVEY";
    case LEVEL_PART_RUINED:
        return "RUINED";
    case LEVEL_PART_LABYRINTH:
        return "LABYRINTH";
    case LEVEL_PART_CHASM:
        return "CHASM";
    case LEVEL_PART_BIG_CAVE:
        return "BIG_CAVE";
    default:
        return "NONE";
    }
}

static const char* big_cave_type_debug_name(big_cave_type_t cave_type)
{
    switch (cave_type)
    {
    case BIG_CAVE_ICE:
        return "ICE";
    case BIG_CAVE_FIRE:
        return "FIRE";
    case BIG_CAVE_POIS:
        return "POIS";
    default:
        return "NONE";
    }
}

static bool suppress_partition_effects_for_point(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    return (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
}

level_partition_kind level_partition_kind_for_point(int y, int x)
{
    /* Suppress partition effects (labyrinth memory loss, big-cave penalties, etc.)
     * inside greater vault regions and Morgoth's entry tunnels. */
    if (suppress_partition_effects_for_point(y, x))
        return LEVEL_PART_ROOMY;

    /* Chests should follow the partition they spawned in (not room overrides). */
    quadrant_mode_t mode = partition_mode_for_point(y, x);
    return partition_kind_from_mode(mode);
}

void level_partition_meta_get(partition_meta_save* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    /* Populate metadata if this is a loaded level. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(p_ptr->py, p_ptr->px);

    out->grid_rows = (s16b)current_partition_rows;
    out->grid_cols = (s16b)current_partition_cols;
    out->partition_count = (s16b)current_partition_count;

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->modes[i] = (byte)current_partition_modes[i];

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->big_cave_types[i] = (byte)current_partition_big_cave_types[i];
}

void level_partition_meta_set(const partition_meta_save* in)
{
    if (!in)
        return;

    int rows = in->grid_rows;
    int cols = in->grid_cols;
    int count = in->partition_count;

    if (rows <= 0 || cols <= 0 || count <= 0 || count > PARTITION_META_MAX || rows * cols != count)
    {
        current_partition_rows = 0;
        current_partition_cols = 0;
        current_partition_count = 0;
        reset_partition_population_metadata();
        for (int i = 0; i < PARTITION_META_MAX; ++i)
        {
            current_partition_modes[i] = QUAD_MODE_ROOMY;
            current_partition_densities[i] = DENSITY_NORMAL;
            current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        }
        return;
    }

    remember_partition_grid(rows, cols, count);
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        quadrant_mode_t mode = QUAD_MODE_ROOMY;
        big_cave_type_t cave_type = BIG_CAVE_NONE;
        if (i < count)
        {
            byte raw = in->modes[i];
            if (raw <= QUAD_MODE_BIG_CAVE)
                mode = (quadrant_mode_t)raw;
            if (mode == QUAD_MODE_BIG_CAVE)
            {
                byte raw_type = in->big_cave_types[i];
                if (raw_type > BIG_CAVE_NONE && raw_type < BIG_CAVE_TYPE_MAX)
                    cave_type = (big_cave_type_t)raw_type;
            }
        }
        current_partition_modes[i] = mode;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = cave_type;
    }
}

int level_partition_index_for_point(int y, int x)
{
    /* Ensure partition metadata exists even for loaded levels. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(y, x);

    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        return -1;

    int pi = partition_index_from_point(y, x, current_partition_rows, current_partition_cols);
    if (pi < 0 || pi >= current_partition_count)
        return -1;

    return pi;
}

big_cave_type_t level_partition_big_cave_type_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count)
        return BIG_CAVE_NONE;
    if (current_partition_modes[pi] != QUAD_MODE_BIG_CAVE)
        return BIG_CAVE_NONE;
    return current_partition_big_cave_types[pi];
}

big_cave_type_t level_partition_big_cave_type_for_point(int y, int x)
{
    if (suppress_partition_effects_for_point(y, x))
        return BIG_CAVE_NONE;

    int pi = level_partition_index_for_point(y, x);
    if (pi < 0)
        return BIG_CAVE_NONE;
    return level_partition_big_cave_type_for_index(pi);
}

void log_partition_debug_for_point(const char* tag, int y, int x)
{
    const char* label = tag ? tag : "partition_debug";
    const bool in_bounds = in_bounds_fully(y, x);
    const bool suppressed = in_bounds && suppress_partition_effects_for_point(y, x);
    int pi = -1;
    int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
    quadrant_mode_t raw_mode = QUAD_MODE_ROOMY;
    level_partition_kind eff_kind = LEVEL_PART_NONE;
    big_cave_type_t raw_big_cave = BIG_CAVE_NONE;
    big_cave_type_t eff_big_cave = BIG_CAVE_NONE;

    if (!in_bounds)
    {
        log_debug("%s: point=(%d,%d) out_of_bounds", label, y, x);
        return;
    }

    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        (void)partition_mode_for_point(y, x);
    }

    pi = level_partition_index_for_point(y, x);
    if (pi >= 0 && pi < current_partition_count)
    {
        raw_mode = current_partition_modes[pi];
        raw_big_cave = current_partition_big_cave_types[pi];
        (void)compute_partition_bounds(pi, current_partition_rows,
            current_partition_cols, &y1, &y2, &x1, &x2);
    }

    eff_kind = level_partition_kind_for_point(y, x);
    eff_big_cave = level_partition_big_cave_type_for_point(y, x);

    log_debug(
        "%s: point=(%d,%d) pi=%d grid=%dx%d/%d bounds=(%d,%d)-(%d,%d) raw_mode=%s raw_big_cave=%s effective_kind=%s effective_big_cave=%s suppressed=%d room=%d gvault=%d morgoth_tunnel=%d feat=%d cave_info=0x%08X",
        label, y, x, pi, current_partition_rows, current_partition_cols,
        current_partition_count, y1, x1, y2, x2,
        quadrant_mode_debug_name(raw_mode),
        big_cave_type_debug_name(raw_big_cave),
        partition_kind_debug_name(eff_kind),
        big_cave_type_debug_name(eff_big_cave), suppressed ? 1 : 0,
        (cave_info[y][x] & CAVE_ROOM) ? 1 : 0,
        (cave_info[y][x] & CAVE_G_VAULT) ? 1 : 0,
        (cave_info[y][x] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
        cave_feat[y][x], (unsigned int)cave_info[y][x]);
}

void level_layout_info_current(level_layout_info* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->map_wid = p_ptr->cur_map_wid;
    out->map_hgt = p_ptr->cur_map_hgt;
    out->partition_rows = current_partition_rows;
    out->partition_cols = current_partition_cols;
    out->partition_count = current_partition_count;

    int area_by_kind[LEVEL_PART_MAX] = {0};

    for (int i = 0; i < current_partition_count; ++i)
    {
        level_partition_kind kind = partition_kind_from_mode(current_partition_modes[i]);
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        int area = 0;

        if (compute_partition_bounds(
                i, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
        {
            area = (y2 - y1 + 1) * (x2 - x1 + 1);
        }

        if (kind == LEVEL_PART_LABYRINTH)
            out->labyrinth_parts++;
        else if (kind == LEVEL_PART_BIG_CAVE)
            out->big_cave_parts++;
        else if (kind == LEVEL_PART_CHASM)
            out->chasm_parts++;

        if (kind > LEVEL_PART_NONE && kind < LEVEL_PART_MAX)
            area_by_kind[kind] += area;
    }

    const level_partition_kind preference[] = {LEVEL_PART_LABYRINTH,
        LEVEL_PART_BIG_CAVE, LEVEL_PART_CHASM, LEVEL_PART_RUINED,
        LEVEL_PART_CAVEY, LEVEL_PART_ROOMY};

    int dominant_area = 0;
    level_partition_kind dominant_kind = LEVEL_PART_NONE;
    for (size_t i = 0; i < N_ELEMENTS(preference); ++i)
    {
        level_partition_kind kind = preference[i];
        int area = area_by_kind[kind];
        if (area > dominant_area)
        {
            dominant_area = area;
            dominant_kind = kind;
        }
    }

    out->dominant_kind = dominant_kind;
}

static partition_drop_profile partition_drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof;
    prof.allow_floor_drops = true;
    drop_profile_default(&prof.profile);

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        /* Default (ROOMY) aÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â 40:30:10:20 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 10;
        prof.profile.weight_supply = 20;
        prof.profile.supply_potion = 1;
        prof.profile.supply_herb = 1;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 1;
        prof.profile.supply_misc = 1;
        break;
    case QUAD_MODE_LABYRINTH:
        /* LABYRINTH - 0:0:35:65 */
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 35;
        prof.profile.weight_supply = 65;
        prof.profile.supply_potion = 15;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 15;
        prof.profile.supply_misc = 5;
        break;
    case QUAD_MODE_RUINED:
        /* RUINED 40:35:0:25 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 35;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 25;
        prof.profile.supply_potion = 7;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 3;
        prof.profile.supply_misc = 15; /* torches, horns, arrows */
        prof.profile.supply_tunneling = 2; /* small chance for shovels/mattocks */
        prof.profile.allow_damaged = true;
        break;
    case QUAD_MODE_CAVEY:
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 100;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 0;
        prof.profile.supply_gem = 12;
        prof.profile.supply_staff = 3;
        prof.profile.supply_misc = 6;
        prof.profile.supply_tunneling = 1;
        break;
    case QUAD_MODE_BIG_CAVE:
        /* BIG_CAVE 20:20:15:45 */
        prof.profile.weight_weapon = 20;
        prof.profile.weight_armor = 20;
        prof.profile.weight_jewelry = 15;
        prof.profile.weight_supply = 45;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 8;
        prof.profile.supply_staff = 2;
        prof.profile.supply_misc = 0;
        break;
    case QUAD_MODE_CHASM:
        /* CHASM 40:30:20:10 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 20;
        prof.profile.weight_supply = 10;
        prof.profile.supply_potion = 1;
        prof.profile.supply_herb = 1;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 1;
        prof.profile.supply_misc = 1;
        break;
    default:
        break;
    }

    return prof;
}

static drop_profile drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof = partition_drop_profile_for_mode(mode);
    return prof.profile;
}

static bool place_partition_chest_at(int pi, int y, int x,
    const partition_chest_recipe* recipe, quadrant_mode_t mode,
    bool require_room_tile)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    int depth = p_ptr->depth;
    drop_profile active_profile = drop_profile_for_mode(mode);
    int chest_mode = 0;

    if (!in_bounds_fully(y, x))
        return false;
    if (pi >= 0 && level_partition_index_for_point(y, x) != pi)
        return false;

    if (mode == QUAD_MODE_CHASM && !chasm_native_walkable_bold(y, x))
        return false;
    if (generation_escape_tunnel_bold(y, x))
        return false;

    /* Chests should land on an actual open floor tile, not on stairs or vault cells. */
    if (!cave_clean_bold(y, x) || cave_m_idx[y][x]
        || (cave_info[y][x] & CAVE_G_VAULT))
    {
        return false;
    }
    if (require_room_tile && !(cave_info[y][x] & CAVE_ROOM))
        return false;

    if (recipe)
        chest_mode = recipe->chest_mode;
    if (chest_mode < 0 || chest_mode > 2)
        chest_mode = 0;

    drop_set_chest_mode(chest_mode);
    drop_clear_chest_material_weights();
    if (recipe
        && recipe->material_wood_pct >= 0
        && recipe->material_steel_pct >= 0
        && recipe->material_jewel_pct >= 0)
    {
        drop_set_chest_material_weights(recipe->material_wood_pct,
            recipe->material_steel_pct, recipe->material_jewel_pct);
    }
    drop_set_chest_vault_type(0);

    object_wipe(i_ptr);

    if (!drop_generate_object_profiled(
            depth, DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, 0, false,
            &active_profile, i_ptr))
    {
        drop_clear_chest_material_weights();
        drop_set_chest_mode(0);
        return false;
    }

    if (i_ptr->tval == TV_CHEST)
        i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));

    if (!floor_carry(y, x, i_ptr))
    {
        genlog_anchor("Failed to carry chest in partition at (%d,%d)", y, x);
        return false;
    }

    return true;
}

void reset_partition_population_metadata(void)
{
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        current_partition_population_meta[i].chest_count = 0;
        for (int recipe_idx = 0; recipe_idx < PARTITION_CHEST_RECIPE_MAX; ++recipe_idx)
            init_partition_chest_recipe(&current_partition_population_meta[i].chest_recipes[recipe_idx]);
    }
}

static bool place_chest_in_bounds(
    int pi, int y1, int y2, int x1, int x2,
    const partition_chest_recipe* recipe, quadrant_mode_t mode,
    bool require_room_tile)
{
    int attempts = 0;
    int max_attempts = 100;

    if (y2 - y1 <= 1 || x2 - x1 <= 1)
        return false;

    while (attempts < max_attempts)
    {
        int cy = rand_range(y1 + 1, y2 - 1);
        int cx = rand_range(x1 + 1, x2 - 1);

        if (place_partition_chest_at(pi, cy, cx, recipe, mode,
                require_room_tile))
        {
            genlog_anchor("Placed chest in partition at (%d,%d)", cy, cx);
            return true;
        }

        attempts++;
    }

    for (int cy = y1 + 1; cy < y2; ++cy)
    {
        for (int cx = x1 + 1; cx < x2; ++cx)
        {
            if (!place_partition_chest_at(pi, cy, cx, recipe, mode,
                    require_room_tile))
                continue;

            genlog_anchor(
                "Placed chest in partition at (%d,%d) after fallback scan", cy, cx);
            return true;
        }
    }

    drop_clear_chest_material_weights();
    drop_set_chest_mode(0);
    genlog_anchor(
        "Failed to place chest in partition after %d attempts and fallback scan",
        max_attempts);

    return false;
}

static bool place_chest_in_partition(
    int pi, int y1, int y2, int x1, int x2,
    const partition_chest_recipe* recipe, quadrant_mode_t mode)
{
    bool require_room_tile = (mode == QUAD_MODE_BIG_CAVE);

    if (recipe && recipe->anchor_pref == PARTITION_CHEST_ANCHOR_BSP_SLICE)
    {
        int room_count = dun->cent_n;
        int start = (room_count > 0) ? rand_int(room_count) : 0;

        for (int offset = 0; offset < room_count; ++offset)
        {
            int room_idx = (start + offset) % room_count;
            rectangle bounds;

            if (room_anchor_kind[room_idx] != LAYOUT_ANCHOR_BSP_SLICE)
                continue;

            bounds = dun->corner[room_idx];
            if (bounds.y1 < y1 || bounds.y2 > y2 || bounds.x1 < x1 || bounds.x2 > x2)
                continue;

            if (place_chest_in_bounds(pi, bounds.y1, bounds.y2, bounds.x1,
                    bounds.x2, recipe, mode, require_room_tile))
            {
                return true;
            }
        }
    }

    if (place_chest_in_bounds(pi, y1, y2, x1, x2, recipe, mode,
            require_room_tile))
    {
        return true;
    }

    if (require_room_tile)
    {
        genlog_anchor(
            "Big cave chest fallback: retrying without room-tile restriction in partition %d",
            pi);
        return place_chest_in_bounds(
            pi, y1, y2, x1, x2, recipe, mode, false);
    }

    return false;
}

void drop_profile_for_partition_kind(level_partition_kind kind, drop_profile* out)
{
    drop_profile_for_partition_kind_source(
        kind, PARTITION_DROP_SOURCE_FLOOR, out);
}

static partition_drop_profile partition_drop_profile_for_kind_source_cfg(
    level_partition_kind kind, partition_drop_source_t source)
{
    const partition_rule_config* cfg = partition_config_get(kind);
    partition_drop_profile prof;

    drop_profile_default(&prof.profile);
    prof.allow_floor_drops = true;

    if (cfg && source >= PARTITION_DROP_SOURCE_FLOOR
        && source < PARTITION_DROP_SOURCE_MAX)
    {
        prof.profile = cfg->profiles[source];
        if (source == PARTITION_DROP_SOURCE_FLOOR)
        {
            prof.allow_floor_drops = cfg->allow_floor_drops;
        }
    }

    return prof;
}

partition_drop_profile partition_drop_profile_for_mode_source_cfg(
    quadrant_mode_t mode, partition_drop_source_t source)
{
    return partition_drop_profile_for_kind_source_cfg(
        partition_kind_from_mode(mode), source);
}

void drop_profile_for_partition_kind_source(level_partition_kind kind,
    partition_drop_source_t source, drop_profile* out)
{
    if (!out)
        return;

    *out = partition_drop_profile_for_kind_source_cfg(kind, source).profile;
}

void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof);

static void place_object_with_profile(
    int y, int x, const partition_drop_profile* prof)
{
    place_object_with_profile_params(
        y, x, object_level, object_level, DROP_QUALITY_NORMAL, DROP_TYPE_UNTHEMED,
        false, 1, 0, prof);
}

void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof)
{
    if (!in_bounds(y, x))
        return;
    if (!cave_clean_bold(y, x))
        return;

    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    object_wipe(i_ptr);

    int attempts = 0;
    const drop_profile* dp = (prof) ? &prof->profile : NULL;

    while (!drop_generate_object_profiled_depths_biased(base_depth,
               min_depth_penalty_depth, quality, droptype, 0, allow_artefacts,
               artefact_weight_multiplier, dp, i_ptr))
    {
        attempts++;
        if (attempts > 200)
            return;
    }

    if (i_ptr->tval == TV_CHEST)
        i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
    if (extra_ident)
        i_ptr->ident |= extra_ident;

    if (!floor_carry(y, x, i_ptr))
    {
        a_info[i_ptr->name1].cur_num = 0;
    }
}

static int partition_metal_drop_target(quadrant_mode_t mode, int floor_count,
    int depth)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_metal_rule* rule = cfg ? &cfg->metal_drops : NULL;
    int target = 0;

    if (floor_count <= 0)
        return 0;
    if (!rule || rule->divisor <= 0)
        return 0;
    if (depth < rule->min_depth)
        return 0;

    target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;

    return target;
}

static s16b partition_metal_kind_for_mode(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_BIG_CAVE:
        return lookup_kind(TV_METAL, SV_METAL_MITHRIL);
    case QUAD_MODE_CHASM:
        return lookup_kind(TV_METAL, SV_METAL_STAR_IRON);
    default:
        return 0;
    }
}

static bool partition_metal_tile_ok(const partition_population_plan* plan,
    int y, int x, bool require_chasm_tag)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (level_partition_index_for_point(y, x) != plan->pi)
        return false;
    if (cave_info[y][x] & CAVE_G_VAULT)
        return false;
    if (!partition_population_naked_bold(plan->mode, y, x))
        return false;
    if (plan->mode == QUAD_MODE_CHASM && require_chasm_tag
        && !(cave_info[y][x] & CAVE_CHASM_AREA))
    {
        return false;
    }

    return true;
}

static int place_partition_metal_drops(const partition_population_plan* plan)
{
    int target;
    int placed = 0;
    s16b k_idx;
    bool require_chasm_tag;

    if (!plan)
        return 0;

    target = partition_metal_drop_target(plan->mode,
        (plan->floor_count_non_vault > 0) ? plan->floor_count_non_vault
                                          : plan->floor_count,
        p_ptr->depth);
    if (target <= 0)
        return 0;

    k_idx = partition_metal_kind_for_mode(plan->mode);
    if (k_idx <= 0)
        return 0;

    require_chasm_tag = (plan->mode == QUAD_MODE_CHASM)
        && bounds_have_chasm_tag(plan->y1, plan->y2, plan->x1, plan->x2);

    for (int n = 0; n < target; ++n)
    {
        bool placed_this = false;

        for (int tries = 0; tries < 200; ++tries)
        {
            int y = rand_range(plan->y1, plan->y2);
            int x = rand_range(plan->x1, plan->x2);
            object_type object_type_body;
            object_type* i_ptr = &object_type_body;

            if (!partition_metal_tile_ok(plan, y, x, require_chasm_tag))
                continue;

            object_wipe(i_ptr);
            object_prep(i_ptr, k_idx);
            i_ptr->number = 1;

            if (floor_carry(y, x, i_ptr))
            {
                placed++;
                placed_this = true;
                break;
            }
        }

        if (!placed_this)
            break;
    }

    if (placed > 0)
    {
        log_trace("Partition metal drops: pi=%d mode=%d placed=%d depth=%d",
            plan->pi, plan->mode, placed, p_ptr ? p_ptr->depth : 0);
    }

    return placed;
}

/*
 * Allocates some objects (using "place" and "type") globally (not partition-specific)
 * Used for rubble and other non-object placement
 */

static int partition_base_monsters_for_mode(quadrant_mode_t mode, int room_count)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));

    if (room_count <= 0)
        return 0;

    int base = (room_count + dieroll(MAX(1, room_count))) / 2;
    if (!cfg || cfg->base_monster_scale_num <= 0)
        return 0;

    return (base * cfg->base_monster_scale_num)
        / MAX(1, cfg->base_monster_scale_den);
}

static int partition_apply_monster_curse_scale(int monster_count)
{
    int stacks = curse_flag_count_cur(CUR_MON_NUM);
    if (!stacks || monster_count <= 0)
        return monster_count;

    return monster_count * (100 + 30 * stacks) / 100;
}

static int partition_direct_floor_monsters(quadrant_mode_t mode, int floor_count)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_count_rule* rule = cfg ? &cfg->direct_monsters : NULL;

    if (floor_count <= 0)
        return 0;

    if (!rule || rule->divisor <= 0)
        return 0;

    int target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;
    return target;
}

static int partition_extra_monster_target_for_depth(
    quadrant_mode_t mode, int floor_count, int depth)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    const partition_depth_rule* rule = cfg ? &cfg->depth_monsters : NULL;
    int target = 0;

    if (floor_count <= 0)
        return 0;

    if (!rule || rule->divisor <= 0)
        return 0;

    target = floor_count / rule->divisor;
    if (target < rule->min_count)
        target = rule->min_count;
    if (rule->max_count > 0 && target > rule->max_count)
        target = rule->max_count;

    if (target <= 0)
        return 0;

    int scale_pct = 100;
    if (rule->scale_pct_at_depth_20 > 100 && depth > 0)
    {
        int extra_pct = rule->scale_pct_at_depth_20 - 100;
        scale_pct += (extra_pct * depth) / 20;
        if (scale_pct > rule->scale_pct_at_depth_20)
            scale_pct = rule->scale_pct_at_depth_20;
    }

    target = target * scale_pct / 100;

    {
        int hard_cap = floor_count / MAX(1, rule->hard_cap_divisor);
        if (hard_cap < 1) hard_cap = 1;
        if (target > hard_cap)
            target = hard_cap;
    }

    return target;
}

static int partition_depth_bonus_monsters(quadrant_mode_t mode, int floor_count, int depth)
{
    int target_at_20 = partition_extra_monster_target_for_depth(mode, floor_count, 20);

    if (depth <= 1 || target_at_20 <= 0)
        return 0;
    if (depth >= 20)
        return target_at_20;

    return (target_at_20 * (depth - 1) + 9) / 19;
}

static int partition_object_scale_pct(void)
{
    switch (op_ptr->vault_drop_frequency)
    {
    case VDF_PLENTIFUL: return 150;
    case VDF_NORMAL:    return 100;
    case VDF_MODEST:    return 67;
    case VDF_SCARCE:    return 33;
    case VDF_MEAGER:    return 10;
    default:            return 100;
    }
}

static void partition_object_counts_from_total_monsters(
    quadrant_mode_t mode, int total_monsters, int* room_objects, int* corr_objects)
{
    const partition_rule_config* cfg =
        partition_config_get(partition_kind_from_mode(mode));
    int room_count = 0;
    int corr_count = 0;
    int pct = partition_object_scale_pct();

    if (cfg)
    {
        if (cfg->room_object_divisor > 0)
            room_count = total_monsters / cfg->room_object_divisor;
        if (cfg->corridor_object_divisor > 0)
            corr_count = total_monsters / cfg->corridor_object_divisor;
    }

    if (pct != 100)
    {
        room_count = MAX(0, room_count * pct / 100);
        corr_count = MAX(0, corr_count * pct / 100);
    }

    if (room_objects)
        *room_objects = room_count;
    if (corr_objects)
        *corr_objects = corr_count;
}

static void rebalance_partition_corridor_objects(partition_population_plan* plan)
{
    int corridor_cap;
    int overflow;

    if (!plan)
        return;

    if (plan->corr_objects <= 0)
        return;

    switch (plan->mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_BIG_CAVE:
    case QUAD_MODE_CHASM:
        plan->room_objects += plan->corr_objects;
        plan->corr_objects = 0;
        return;

    case QUAD_MODE_ROOMY:
        return;

    default:
        break;
    }

    if (plan->corridor_floor_count <= 0)
    {
        plan->room_objects += plan->corr_objects;
        plan->corr_objects = 0;
        return;
    }

    corridor_cap = plan->corridor_floor_count / 8;
    if (corridor_cap <= 0)
        corridor_cap = 1;

    if (corridor_cap >= plan->corr_objects)
        return;

    overflow = plan->corr_objects - corridor_cap;
    plan->corr_objects = corridor_cap;
    plan->room_objects += overflow;
}

static void distribute_partition_base_monsters(
    partition_population_plan* plans, int plan_count)
{
    int rooms_by_mode[QUAD_MODE_BIG_CAVE + 1] = {0};

    for (int i = 0; i < plan_count; ++i)
    {
        if (plans[i].mode >= QUAD_MODE_ROOMY && plans[i].mode <= QUAD_MODE_BIG_CAVE)
            rooms_by_mode[plans[i].mode] += plans[i].room_centers;
        plans[i].monsters_base = 0;
    }

    for (int mode = QUAD_MODE_ROOMY; mode <= QUAD_MODE_BIG_CAVE; ++mode)
    {
        int total_rooms = rooms_by_mode[mode];
        int total_monsters;
        int assigned = 0;
        int remainders[PARTITION_META_MAX];

        if (total_rooms <= 0)
            continue;

        total_monsters = partition_base_monsters_for_mode((quadrant_mode_t)mode, total_rooms);
        if (total_monsters <= 0)
            continue;

        for (int i = 0; i < PARTITION_META_MAX; ++i)
            remainders[i] = -1;

        for (int i = 0; i < plan_count; ++i)
        {
            long weighted;

            if (plans[i].mode != (quadrant_mode_t)mode || plans[i].room_centers <= 0)
                continue;

            weighted = (long)total_monsters * (long)plans[i].room_centers;
            plans[i].monsters_base = (int)(weighted / total_rooms);
            remainders[i] = (int)(weighted % total_rooms);
            assigned += plans[i].monsters_base;
        }

        for (int left = total_monsters - assigned; left > 0; --left)
        {
            int best_i = -1;
            int best_rem = -1;

            for (int i = 0; i < plan_count; ++i)
            {
                if (remainders[i] > best_rem)
                {
                    best_i = i;
                    best_rem = remainders[i];
                }
            }

            if (best_i < 0)
                break;

            plans[best_i].monsters_base++;
            remainders[best_i] = -1;
        }
    }
}

static void apply_curse_scale_to_partition_totals(
    partition_population_plan* plans, int plan_count)
{
    int total_monsters = 0;
    int scaled_total;
    int assigned = 0;
    int remainders[PARTITION_META_MAX];

    for (int i = 0; i < plan_count; ++i)
    {
        plans[i].monsters_curse_bonus = 0;
        total_monsters += plans[i].monsters_precurse;
        remainders[i] = -1;
    }

    if (total_monsters <= 0)
    {
        for (int i = 0; i < plan_count; ++i)
            plans[i].monsters_total = plans[i].monsters_precurse;
        return;
    }

    scaled_total = partition_apply_monster_curse_scale(total_monsters);
    if (scaled_total <= total_monsters)
    {
        for (int i = 0; i < plan_count; ++i)
            plans[i].monsters_total = plans[i].monsters_precurse;
        return;
    }

    for (int i = 0; i < plan_count; ++i)
    {
        long weighted = (long)scaled_total * (long)plans[i].monsters_precurse;

        plans[i].monsters_total = (int)(weighted / total_monsters);
        remainders[i] = (int)(weighted % total_monsters);
        assigned += plans[i].monsters_total;
    }

    for (int left = scaled_total - assigned; left > 0; --left)
    {
        int best_i = -1;
        int best_rem = -1;

        for (int i = 0; i < plan_count; ++i)
        {
            if (remainders[i] > best_rem)
            {
                best_i = i;
                best_rem = remainders[i];
            }
        }

        if (best_i < 0)
            break;

        plans[best_i].monsters_total++;
        remainders[best_i] = -1;
    }

    for (int i = 0; i < plan_count; ++i)
        plans[i].monsters_curse_bonus =
            plans[i].monsters_total - plans[i].monsters_precurse;
}

int build_partition_population_plans(
    partition_population_plan* plans, int max_plans)
{
    int room_centers[PARTITION_META_MAX] = {0};
    int count = MIN(current_partition_count, max_plans);

    if (count <= 0 || current_partition_rows <= 0 || current_partition_cols <= 0)
        return 0;

    for (int i = 0; i < dun->cent_n; ++i)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        if (pi >= 0 && pi < count)
            room_centers[pi]++;
    }

    for (int pi = 0; pi < count; ++pi)
    {
        partition_population_plan* plan = &plans[pi];

        memset(plan, 0, sizeof(*plan));
        plan->pi = pi;
        plan->mode = current_partition_modes[pi];
        plan->cave_type = current_partition_big_cave_types[pi];
        plan->meta = current_partition_population_meta[pi];
        plan->room_centers = room_centers[pi];

        if (!compute_partition_bounds(
                pi, current_partition_rows, current_partition_cols,
                &plan->y1, &plan->y2, &plan->x1, &plan->x2))
        {
            continue;
        }

        for (int y = plan->y1; y <= plan->y2; ++y)
        {
            for (int x = plan->x1; x <= plan->x2; ++x)
            {
                bool is_room;

                if (!in_bounds_fully(y, x))
                    continue;
                if (!partition_population_floor_bold(plan->mode, y, x))
                    continue;

                is_room = (cave_info[y][x] & CAVE_ROOM) ? true : false;

                plan->floor_count++;
                if (is_room)
                    plan->room_floor_count++;
                else
                    plan->corridor_floor_count++;
                if (!(cave_info[y][x] & CAVE_ICKY))
                    plan->floor_count_non_icky++;
                if (!(cave_info[y][x] & CAVE_G_VAULT))
                    plan->floor_count_non_vault++;
            }
        }
    }

    distribute_partition_base_monsters(plans, count);

    for (int i = 0; i < count; ++i)
    {
        plans[i].monsters_floor =
            partition_direct_floor_monsters(plans[i].mode, plans[i].floor_count);
        plans[i].monsters_depth =
            partition_depth_bonus_monsters(
                plans[i].mode, plans[i].floor_count_non_icky, p_ptr->depth);
        plans[i].monsters_precurse =
            plans[i].monsters_base + plans[i].monsters_floor + plans[i].monsters_depth;
    }

    apply_curse_scale_to_partition_totals(plans, count);

    for (int i = 0; i < count; ++i)
    {
        partition_object_counts_from_total_monsters(
            plans[i].mode, plans[i].monsters_precurse,
            &plans[i].room_objects, &plans[i].corr_objects);
        rebalance_partition_corridor_objects(&plans[i]);
    }

    return count;
}

void partition_theme_depth_band(int depth, int* min_depth, int* max_depth)
{
    int min_level = MAX(1, depth - PARTITION_THEME_LEVEL_DELTA);
    int max_level = MIN(MORGOTH_DEPTH + 3, depth + PARTITION_THEME_LEVEL_DELTA);

    if (min_depth)
        *min_depth = min_level;
    if (max_depth)
        *max_depth = max_level;
}

static bool partition_mode_uses_monster_pools(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_CAVEY:
    case QUAD_MODE_LABYRINTH:
    case QUAD_MODE_CHASM:
    case QUAD_MODE_BIG_CAVE:
        return true;

    default:
        return false;
    }
}

static bool monster_name_contains_ci(const monster_race* r_ptr, cptr needle)
{
    size_t len;
    const char* name;

    if (!r_ptr || !needle || !needle[0] || !r_ptr->name)
        return false;

    len = strlen(needle);
    name = r_name + r_ptr->name;

    for (const char* p = name; *p; ++p)
    {
        if (SDL_strncasecmp(p, needle, len) == 0)
            return true;
    }

    return false;
}

static bool monster_is_bat(const monster_race* r_ptr)
{
    return r_ptr && (r_ptr->d_char == 'b')
        && monster_name_contains_ci(r_ptr, "bat");
}

static bool monster_has_blow_effect(const monster_race* r_ptr, byte effect)
{
    if (!r_ptr)
        return false;

    for (int i = 0; i < MONSTER_BLOW_MAX; ++i)
    {
        if (!r_ptr->blow[i].method)
            continue;
        if (r_ptr->blow[i].effect == effect)
            return true;
    }

    return false;
}

static bool monster_counts_toward_labyrinth_fixed_cap(const monster_race* r_ptr)
{
    return r_ptr
        && ((r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE)) != 0);
}

static bool monster_matches_partition_theme(
    const monster_race* r_ptr, quadrant_mode_t mode, big_cave_type_t cave_type)
{
    if (!r_ptr)
        return false;

    switch (mode)
    {
    case QUAD_MODE_CHASM:
        return (r_ptr->light < 0);

    case QUAD_MODE_BIG_CAVE:
        if (r_ptr->flags3 & (RF3_TROLL | RF3_GIANT))
            return true;

        switch (cave_type)
        {
        case BIG_CAVE_FIRE:
            return ((r_ptr->flags4 & RF4_BRTH_FIRE) != 0)
                || monster_has_blow_effect(r_ptr, RBE_FIRE);

        case BIG_CAVE_ICE:
            return ((r_ptr->flags4 & RF4_BRTH_COLD) != 0)
                || monster_has_blow_effect(r_ptr, RBE_COLD);

        case BIG_CAVE_POIS:
            return ((r_ptr->flags4 & RF4_BRTH_POIS) != 0)
                || monster_has_blow_effect(r_ptr, RBE_POISON);

        case BIG_CAVE_NONE:
        case BIG_CAVE_TYPE_MAX:
        default:
            return ((r_ptr->flags3
                        & (RF3_WOLF | RF3_SPIDER | RF3_VAMPIRE
                            | RF3_TROLL | RF3_GIANT))
                    != 0)
                || monster_is_bat(r_ptr);
        }

    case QUAD_MODE_CAVEY:
        return ((r_ptr->flags3
                    & (RF3_WOLF | RF3_SPIDER | RF3_VAMPIRE
                        | RF3_TROLL | RF3_GIANT))
                != 0)
            || monster_is_bat(r_ptr);

    case QUAD_MODE_LABYRINTH:
        return ((r_ptr->flags2 & RF2_INVISIBLE) != 0)
            || ((r_ptr->flags1 & (RF1_NEVER_MOVE | RF1_HIDDEN_MOVE)) != 0)
            || ((r_ptr->flags4 & RF4_DIM) != 0)
            || ((r_ptr->flags3 & RF3_VAMPIRE) != 0);

    default:
        return false;
    }
}

static bool partition_pool_monster_ok(
    const partition_population_plan* plan, int r_idx, int min_depth,
    int max_depth, bool themed, int labyrinth_fixed_remaining)
{
    monster_race* r_ptr = &r_info[r_idx];

    if (!plan)
        return false;
    if (!r_ptr->name || !r_ptr->rarity)
        return false;
    if (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY)
        return false;
    if (r_ptr->flags1 & RF1_SPECIAL_GEN)
        return false;
    if (r_ptr->level < min_depth || r_ptr->level > max_depth)
        return false;
    if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (r_ptr->level > p_ptr->depth))
        return false;
    if (r_ptr->cur_num >= r_ptr->max_num)
        return false;
    if (plan->mode == QUAD_MODE_LABYRINTH
        && labyrinth_fixed_remaining <= 0
        && monster_counts_toward_labyrinth_fixed_cap(r_ptr))
    {
        return false;
    }
    if (themed && !monster_matches_partition_theme(r_ptr, plan->mode, plan->cave_type))
        return false;

    return true;
}

static s16b choose_partition_pool_monster(
    const partition_population_plan* plan, bool themed, int min_depth,
    int max_depth, int labyrinth_fixed_remaining)
{
    alloc_entry* table = alloc_race_table;
    long total = 0L;

    if (!plan)
        return 0;
    if (min_depth < 1)
        min_depth = 1;
    if (max_depth > MORGOTH_DEPTH + 3)
        max_depth = MORGOTH_DEPTH + 3;
    if (min_depth > max_depth)
        return 0;

    for (int i = 0; i < alloc_race_size; ++i)
    {
        int r_idx = table[i].index;

        if (table[i].level > max_depth)
            break;
        if (!partition_pool_monster_ok(plan, r_idx, min_depth, max_depth,
                themed, labyrinth_fixed_remaining))
        {
            continue;
        }

        total += table[i].prob1;
    }

    if (total <= 0)
        return 0;

    {
        long value = rand_int(total);

        for (int i = 0; i < alloc_race_size; ++i)
        {
            int r_idx = table[i].index;

            if (table[i].level > max_depth)
                break;
            if (!partition_pool_monster_ok(plan, r_idx, min_depth, max_depth,
                    themed, labyrinth_fixed_remaining))
            {
                continue;
            }

            if (value < table[i].prob1)
                return r_idx;

            value -= table[i].prob1;
        }
    }

    return 0;
}

static bool place_partition_pool_monster(
    const partition_population_plan* plan, int y, int x, bool themed,
    int labyrinth_fixed_remaining)
{
    int min_depth;
    int max_depth;

    partition_theme_depth_band(p_ptr->depth, &min_depth, &max_depth);

    for (int tries = 0; tries < 24; ++tries)
    {
        s16b r_idx = choose_partition_pool_monster(plan, themed, min_depth,
            max_depth, labyrinth_fixed_remaining);

        if (!r_idx)
            return false;

        if (plan->mode == QUAD_MODE_CHASM && themed)
        {
            if (place_chasm_theme_monster_at(y, x, r_idx))
                return true;
        }
        else if (place_monster_one(y, x, r_idx, true, false, NULL))
        {
            return true;
        }
    }

    return false;
}

static bool choose_partition_monster_location(
    const partition_population_plan* plan, int* out_y, int* out_x)
{
    bool avoid_corridors = partition_mode_avoids_corridor_spawns(plan->mode);

    for (int tries = 0; tries < 250; ++tries)
    {
        int y = rand_range(plan->y1, plan->y2);
        int x = rand_range(plan->x1, plan->x2);

        if (!in_bounds_fully(y, x))
            continue;
        if (level_partition_index_for_point(y, x) != plan->pi)
            continue;
        if (cave_info[y][x] & CAVE_ICKY)
            continue;
        if (!partition_population_naked_bold(plan->mode, y, x))
            continue;
        if (avoid_corridors && !(cave_info[y][x] & CAVE_ROOM))
            continue;

        *out_y = y;
        *out_x = x;
        return true;
    }

    return false;
}

static bool place_partition_themed_monster(
    const partition_population_plan* plan, int y, int x)
{
    if (partition_mode_uses_monster_pools(plan->mode))
        return place_partition_pool_monster(plan, y, x, true, 5);

    return false;
}

bool partition_monster_pass_skips_plan(
    const partition_population_plan* plan)
{
    if (!plan)
        return false;

    return morgoth_region_active() && (plan->pi == morgoth_partition_index);
}

int run_partition_monster_pass(
    const partition_population_plan* plans, int plan_count)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        const partition_population_plan* plan = &plans[i];
        bool use_pool_theme = partition_mode_uses_monster_pools(plan->mode);
        int labyrinth_fixed_remaining =
            (plan->mode == QUAD_MODE_LABYRINTH) ? 5 : 0;
        int generic_remaining = 0;
        int themed_remaining = 0;
        int generic_target = 0;
        int themed_target = 0;
        int target_total = 0;
        int placed = 0;
        int attempts;

        if (use_pool_theme)
        {
            target_total = plan->monsters_total;
            themed_target =
                (target_total * PARTITION_THEME_MONSTER_PERCENT + 50) / 100;
            if (themed_target > target_total)
                themed_target = target_total;
            generic_target = target_total - themed_target;
            themed_remaining = themed_target;
            generic_remaining = generic_target;
        }
        else
        {
            int precurse_total;
            int curse_bonus;

            generic_remaining = plan->monsters_base;
            themed_remaining = plan->monsters_floor + plan->monsters_depth;
            precurse_total = generic_remaining + themed_remaining;
            curse_bonus = plan->monsters_curse_bonus;

            if (curse_bonus > 0)
            {
                int generic_bonus = 0;

                if (precurse_total > 0 && generic_remaining > 0)
                {
                    long weighted_generic =
                        (long)curse_bonus * (long)generic_remaining;

                    generic_bonus = (int)(weighted_generic / precurse_total);
                    if ((weighted_generic % precurse_total) * 2 >= precurse_total)
                        generic_bonus++;
                }

                if (generic_bonus > curse_bonus)
                    generic_bonus = curse_bonus;

                generic_remaining += generic_bonus;
                themed_remaining += curse_bonus - generic_bonus;
            }

            generic_target = generic_remaining;
            themed_target = themed_remaining;
            target_total = generic_remaining + themed_remaining;
        }

        attempts = MAX(1, target_total) * 250;

        if (partition_monster_pass_skips_plan(plan))
        {
            if (target_total > 0)
            {
                log_trace(
                    "Partition monsters: pi=%d mode=%d skipped for Morgoth throne partition",
                    plan->pi, plan->mode);
            }
            continue;
        }

        for (int tries = 0;
             tries < attempts && (generic_remaining > 0 || themed_remaining > 0);
             ++tries)
        {
            bool themed =
                (themed_remaining > 0)
                && ((generic_remaining == 0)
                    || (rand_int(generic_remaining + themed_remaining) < themed_remaining));
            int y, x;
            bool placed_mon = false;
            bool consume_themed_quota = themed;

            if (!choose_partition_monster_location(plan, &y, &x))
                continue;

            if (use_pool_theme)
            {
                placed_mon = place_partition_pool_monster(plan, y, x, themed,
                    labyrinth_fixed_remaining);
                if (!placed_mon && themed)
                {
                    placed_mon = place_partition_pool_monster(plan, y, x, false,
                        labyrinth_fixed_remaining);
                    if (placed_mon && generic_remaining > 0)
                        consume_themed_quota = false;
                }
            }
            else
            {
                if (themed)
                    placed_mon = place_partition_themed_monster(plan, y, x);

                if (!placed_mon)
                    placed_mon = place_monster(y, x, true,
                        (!themed && plan->mode == QUAD_MODE_ROOMY), false);
            }

            if (!placed_mon)
                continue;

            if (consume_themed_quota)
                themed_remaining--;
            else
                generic_remaining--;

            if (plan->mode == QUAD_MODE_LABYRINTH && cave_m_idx[y][x] > 0
                && labyrinth_fixed_remaining > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                monster_race* r_ptr = &r_info[m_ptr->r_idx];

                if (monster_counts_toward_labyrinth_fixed_cap(r_ptr))
                    labyrinth_fixed_remaining--;
            }

            placed++;
            total_placed++;
        }

        log_trace(
            "Partition monsters: pi=%d mode=%d rooms=%d floors=%d base=%d floor=%d depth=%d precurse=%d curse=%d total=%d theme_target=%d global_target=%d placed=%d",
            plan->pi, plan->mode, plan->room_centers, plan->floor_count,
            plan->monsters_base, plan->monsters_floor, plan->monsters_depth,
            plan->monsters_precurse, plan->monsters_curse_bonus,
            target_total, themed_target, generic_target, placed);
    }

    return total_placed;
}

static int alloc_objects_from_plan(
    const partition_population_plan* plan, int set, int num)
{
    int placed = 0;

    for (int k = 0; k < num; ++k)
    {
        partition_drop_profile active_profile =
            partition_drop_profile_for_mode_source_cfg(
                plan->mode, PARTITION_DROP_SOURCE_FLOOR);
        int y = 0;
        int x = 0;
        int i;

        for (i = 0; i < 10000; ++i)
        {
            bool is_room;

            y = rand_range(plan->y1, plan->y2);
            x = rand_range(plan->x1, plan->x2);

            if (!in_bounds_fully(y, x))
                continue;
            if (!partition_population_naked_bold(plan->mode, y, x))
                continue;
            if (level_partition_index_for_point(y, x) != plan->pi)
                continue;

            is_room = (cave_info[y][x] & CAVE_ROOM) ? true : false;

            if (plan->mode != QUAD_MODE_CHASM)
            {
                if ((set == ALLOC_SET_CORR) && is_room)
                    continue;
                if ((set == ALLOC_SET_ROOM) && !is_room)
                    continue;
            }

            active_profile = partition_drop_profile_for_mode_source_cfg(
                drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
            if (!active_profile.allow_floor_drops)
                continue;

            break;
        }

        if (i >= 10000)
            continue;

        place_object_with_profile(y, x, &active_profile);
        if (cave_o_idx[y][x] != 0)
            placed++;
    }

    return placed;
}

int run_partition_object_pass(
    const partition_population_plan* plans, int plan_count, bool rooms)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        int target = rooms ? plans[i].room_objects : plans[i].corr_objects;
        int placed;

        if (target <= 0)
            continue;

        placed = alloc_objects_from_plan(
            &plans[i], rooms ? ALLOC_SET_ROOM : ALLOC_SET_CORR, target);
        total_placed += placed;

        log_trace("Partition %s objects: pi=%d mode=%d target=%d placed=%d total_monsters=%d",
            rooms ? "room" : "corridor", plans[i].pi, plans[i].mode,
            target, placed, plans[i].monsters_total);
    }

    return total_placed;
}

static int place_partition_skeletons(
    const partition_population_plan* plan, int target,
    int human_pct, int elf_pct, bool avoid_rubble)
{
    int placed = 0;

    for (int sk = 0; sk < target; ++sk)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(plan->y1, plan->y2);
            int sx = rand_range(plan->x1, plan->x2);
            int roll;
            s16b k_idx;
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;

            if (!in_bounds_fully(sy, sx))
                continue;
            if (!cave_floor_bold(sy, sx))
                continue;
            if (generation_escape_tunnel_bold(sy, sx))
                continue;
            if ((cave_info[sy][sx] & CAVE_G_VAULT) != 0)
                continue;
            if (avoid_rubble && cave_feat[sy][sx] == FEAT_RUBBLE)
                continue;
            if (cave_o_idx[sy][sx] != 0)
                continue;

            object_wipe(i_ptr);

            roll = rand_int(100);
            if (roll < human_pct)
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
            else if (roll < human_pct + elf_pct)
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);
            else
                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);

            object_prep(i_ptr, k_idx);
            i_ptr->pval = 1;
            drop_near(i_ptr, -1, sy, sx);
            placed++;
            break;
        }
    }

    return placed;
}

static bool partition_exact_monster_tile_ok(
    const partition_population_plan* plan, int y, int x)
{
    if (!plan)
        return false;
    if (!in_bounds_fully(y, x))
        return false;
    if (level_partition_index_for_point(y, x) != plan->pi)
        return false;
    if (cave_info[y][x] & CAVE_G_VAULT)
        return false;
    if (!partition_population_naked_bold(plan->mode, y, x))
        return false;

    return true;
}

static int place_partition_exact_monster_tokens(
    const partition_population_plan* plan, char token, int target)
{
    int placed = 0;

    if (!plan || target <= 0)
        return 0;

    for (int n = 0; n < target; ++n)
    {
        bool placed_this = false;

        for (int tries = 0; tries < 500; ++tries)
        {
            int y = rand_range(plan->y1, plan->y2);
            int x = rand_range(plan->x1, plan->x2);

            if (!partition_exact_monster_tile_ok(plan, y, x))
                continue;
            if (!place_vault_monster_token(token, y, x))
                continue;

            placed++;
            placed_this = true;
            break;
        }

        if (!placed_this)
        {
            for (int y = plan->y1; y <= plan->y2 && !placed_this; ++y)
            {
                for (int x = plan->x1; x <= plan->x2; ++x)
                {
                    if (!partition_exact_monster_tile_ok(plan, y, x))
                        continue;
                    if (!place_vault_monster_token(token, y, x))
                        continue;

                    placed++;
                    placed_this = true;
                    break;
                }
            }
        }

        if (!placed_this)
            break;
    }

    return placed;
}

int run_partition_special_scatter_pass(
    const partition_population_plan* plans, int plan_count)
{
    int total_placed = 0;

    for (int i = 0; i < plan_count; ++i)
    {
        const partition_population_plan* plan = &plans[i];

        total_placed += place_partition_metal_drops(plan);

        if (plan->mode == QUAD_MODE_BIG_CAVE && plan->floor_count > 0)
        {
            int target = plan->floor_count / 40;
            if (target < 2) target = 2;
            if (target > 8) target = 8;
            total_placed += place_partition_skeletons(plan, target, 35, 35, false);
        }
        else if (plan->mode == QUAD_MODE_LABYRINTH && plan->floor_count > 0)
        {
            int target = plan->floor_count / 25;
            if (target < 2) target = 2;
            if (target > 6) target = 6;
            total_placed += place_partition_skeletons(plan, target, 30, 60, false);
        }
        else if (plan->mode == QUAD_MODE_RUINED && plan->floor_count_non_vault > 0)
        {
            int skeleton_target = plan->floor_count_non_vault / 15;

            if (skeleton_target < 3) skeleton_target = 3;
            if (skeleton_target > 10) skeleton_target = 10;
            total_placed += place_partition_skeletons(plan, skeleton_target, 20, 20, true);
        }
        else if (plan->mode == QUAD_MODE_CHASM && plan->floor_count > 0)
        {
            total_placed += place_partition_exact_monster_tokens(
                plan, 'q', CHASM_WHISPERING_SHADOW_TARGET);
        }

        for (int chest = 0; chest < plan->meta.chest_count; ++chest)
        {
            place_chest_in_partition(
                plan->pi, plan->y1, plan->y2, plan->x1, plan->x2,
                &plan->meta.chest_recipes[chest], plan->mode);
        }

        log_trace("Partition specials: pi=%d mode=%d chests=%d",
            plan->pi, plan->mode, plan->meta.chest_count);
    }

    return total_placed;
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
            place_object_with_profile(y, x, &active_profile);
            break;
        }
        }
    }
}

/*
 * Places "streamers" of quartz through dungeon
 */
static bool build_streamer(int feat)
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
static void build_chasms(void)
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

static bool h_tunnel_ok(
    int x1, int x2, int y, bool tentative, int desired_changes)
{
    int x, x_lo, x_hi, changes;

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y][x1] == FEAT_WALL_OUTER)
        || (cave_feat[y][x2] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y][x_lo] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y - 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y - 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y + 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y + 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y][x_hi] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y - 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y - 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y + 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y + 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
    }

    /* test each location in the corridor */
    for (x = x_lo; x <= x_hi; x++)
    {
        /* count the number of times it enters or leaves a room */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && // to outside
            (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER))) // from inside
        {
            changes++;
        }
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && // from outside
            (cave_floor_bold(y, x)
                || (cave_feat[y][x] == FEAT_WALL_INNER))) // to inside
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through or adjacent to an existing door
         * (except in vaults) */
        if (cave_known_closed_door_bold(y - 1, x)
            && !(cave_info[y - 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y + 1, x)
            && !(cave_info[y + 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y + 1][x] == FEAT_FLOOR && !(cave_info[y + 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y - 1][x] == FEAT_FLOOR && !(cave_info[y - 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

static bool v_tunnel_ok(
    int y1, int y2, int x, bool tentative, int desired_changes)
{
    int y, y_lo, y_hi, changes;

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y1][x] == FEAT_WALL_OUTER)
        || (cave_feat[y2][x] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y_lo][x] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y_lo - 1][x - 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y_lo - 1][x + 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x + 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y_hi][x] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y_hi + 1][x - 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x - 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y_hi + 1][x + 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
    }

    /* test each location in the corridor */
    for (y = y_lo; y <= y_hi; y++)
    {
        /* count the number of times it enters or leaves a room */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through, or adjacent to an existing
         * (non-vault) door */
        if (cave_known_closed_door_bold(y, x - 1)
            && !(cave_info[y][x - 1] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x + 1)
            && !(cave_info[y][x + 1] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y][x + 1] == FEAT_FLOOR && !(cave_info[y][x + 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y][x - 1] == FEAT_FLOOR && !(cave_info[y][x - 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

typedef enum {
    TUNNEL_TREAT_NONE = 0,
    TUNNEL_TREAT_NICHES,
    TUNNEL_TREAT_PILLARS
} tunnel_treatment;

typedef struct tunnel_profile {
    byte width;          /* 1 = normal, 2 = offset double, 3 = grand hall */
    int side_bias;       /* -1/0/1: which side to favour when width == 2 */
    tunnel_treatment treatment;
} tunnel_profile;

static const tunnel_profile TUNNEL_PROFILE_NORMAL = {1, 0, TUNNEL_TREAT_NONE};

static tunnel_profile choose_tunnel_profile(bool tentative)
{
    tunnel_profile profile = TUNNEL_PROFILE_NORMAL;

    /* On shallow branches, fall back to narrow connectors */
    if (tentative)
    {
        /* allow style variation even on tentative digs */
    }

    int depth = p_ptr->depth;
    int sidx = styles_get_level_primary_style();
    byte style_group = (sidx >= 0 && style_info) ? style_info[sidx].group : 0;
    bool style_grand = (style_group >= 4); /* warmer/darker palettes get a bump */

    /* Variable tunnel widths at any depth, probability scales with depth */
    /* Base rarity values (lower = more common) */
    int medium_rarity, grand_rarity;
    
    if (depth >= 20)
    {
        medium_rarity = style_grand ? 5 : 7;
        grand_rarity = style_grand ? 8 : 12;
    }
    else if (depth >= 12)
    {
        medium_rarity = style_grand ? 7 : 10;
        grand_rarity = style_grand ? 11 : 16;
    }
    else if (depth >= 7)
    {
        medium_rarity = style_grand ? 10 : 14;
        grand_rarity = style_grand ? 16 : 22;
    }
    else
    {
        /* Even early levels can have occasional wider corridors */
        medium_rarity = style_grand ? 16 : 20;
        grand_rarity = style_grand ? 25 : 30;
    }

    if (one_in_(grand_rarity))
    {
        profile.width = 3;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_PILLARS : TUNNEL_TREAT_NICHES;
    }
    else if (one_in_(medium_rarity))
    {
        profile.width = one_in_(4) ? 3 : 2;
        profile.side_bias = one_in_(2) ? 1 : -1;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_NICHES : TUNNEL_TREAT_NONE;
    }

    return profile;
}

static void apply_tunnel_niche_torch_glow(int niche_y, int niche_x, int front_dy, int front_dx)
{
    if (!in_bounds_fully(niche_y, niche_x))
        return;

    /* "Torch" effect (radius 1) biased into the corridor:
     * - light the niche floor itself
     * - light the two wall tiles flanking the niche (along the corridor axis)
     * - light the 3 corridor floor tiles directly in front of the niche
     */
    int axis_dy = (front_dx != 0) ? 1 : 0;
    int axis_dx = (front_dy != 0) ? 1 : 0;

    if (cave_floor_bold(niche_y, niche_x)
        && !(cave_info[niche_y][niche_x] & (CAVE_ROOM | CAVE_ICKY)))
    {
        cave_info[niche_y][niche_x] |= (CAVE_GLOW);
    }

    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; i += 2 * RADIUS_TORCH)
    {
        int wy = niche_y + axis_dy * i;
        int wx = niche_x + axis_dx * i;
        if (!in_bounds_fully(wy, wx))
            continue;
        if (cave_info[wy][wx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        if (cave_wall_bold(wy, wx))
            cave_info[wy][wx] |= (CAVE_GLOW);
    }

    int entry_y = niche_y + front_dy;
    int entry_x = niche_x + front_dx;
    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; ++i)
    {
        int fy = entry_y + axis_dy * i;
        int fx = entry_x + axis_dx * i;
        if (!in_bounds_fully(fy, fx))
            continue;
        if (!cave_floor_bold(fy, fx))
            continue;
        if (cave_info[fy][fx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        cave_info[fy][fx] |= (CAVE_GLOW);
    }
}

static void apply_v_tunnel_treatment(
    int r1, int r2, int y_lo, int y_hi, int x, bool widen_west, bool widen_east,
    const tunnel_profile* profile, bool mark_escape)
{
    if (!profile)
        return;

    /* Side niches sit just outside the carved width */
    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_west && widen_east)
            side = one_in_(2) ? -offset : offset;
        else if (widen_west)
            side = -offset;
        else if (widen_east)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int y = y_lo + 2 + rand_int(3);
        while (y < y_hi - 1)
        {
            int nx = x + side;
            if (in_bounds_fully(y, nx) && cave_feat[y][nx] == FEAT_WALL_EXTRA
                && !(cave_info[y][nx] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(y, nx, FEAT_FLOOR);
                cave_corridor1[y][nx] = r1;
                cave_corridor2[y][nx] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, nx);

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(y, nx, 0, -dir);
            }
            y += 3 + rand_int(3);
            side = -side; /* alternate sides */
        }
    }

    /* Pillar lines break up wide halls without blocking flow */
    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int y = y_lo + 2 + rand_int(2);
        while (y <= y_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            y += 3 + rand_int(2);
        }
    }
}

static void apply_h_tunnel_treatment(
    int r1, int r2, int x_lo, int x_hi, int y, bool widen_north, bool widen_south,
    const tunnel_profile* profile, bool mark_escape)
{
    if (!profile)
        return;

    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_north && widen_south)
            side = one_in_(2) ? -offset : offset;
        else if (widen_north)
            side = -offset;
        else if (widen_south)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int x = x_lo + 2 + rand_int(3);
        while (x < x_hi - 1)
        {
            int ny = y + side;
            if (in_bounds_fully(ny, x) && cave_feat[ny][x] == FEAT_WALL_EXTRA
                && !(cave_info[ny][x] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(ny, x, FEAT_FLOOR);
                cave_corridor1[ny][x] = r1;
                cave_corridor2[ny][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(ny, x);

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(ny, x, -dir, 0);
            }
            x += 3 + rand_int(3);
            side = -side;
        }
    }

    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int x = x_lo + 2 + rand_int(2);
        while (x <= x_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            x += 3 + rand_int(2);
        }
    }
}

static void build_v_tunnel(
    int r1, int r2, int y1, int y2, int x, const tunnel_profile* profile)
{
    int y, y_lo, y_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool mark_escape = tunnel_should_mark_escape(r1, r2);
    bool short_span = (ABS(y2 - y1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_west = (width >= 3) || (width == 2 && local.side_bias < 0);
    bool widen_east = (width >= 3) || (width == 2 && local.side_bias > 0);

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);

    for (y = y_lo; y <= y_hi; y++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            /* all doors get randomised later */
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            if (mark_escape)
                mark_generation_escape_tunnel(y, x);
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_east && x + 1 < MAX_DUNGEON_WID
                && cave_feat[y][x + 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x + 1)
                && !(cave_info[y][x + 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x + 1, FEAT_FLOOR);
                cave_corridor1[y][x + 1] = r1;
                cave_corridor2[y][x + 1] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, x + 1);
            }
            if (widen_west && x - 1 > 0 && cave_feat[y][x - 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x - 1)
                && !(cave_info[y][x - 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x - 1, FEAT_FLOOR);
                cave_corridor1[y][x - 1] = r1;
                cave_corridor2[y][x - 1] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, x - 1);
            }
        }
    }

    apply_v_tunnel_treatment(r1, r2, y_lo, y_hi, x, widen_west, widen_east,
        &local, mark_escape);
}

static void build_h_tunnel(
    int r1, int r2, int x1, int x2, int y, const tunnel_profile* profile)
{
    int x, x_lo, x_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool mark_escape = tunnel_should_mark_escape(r1, r2);
    bool short_span = (ABS(x2 - x1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_south = (width >= 3) || (width == 2 && local.side_bias > 0);
    bool widen_north = (width >= 3) || (width == 2 && local.side_bias < 0);

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);

    for (x = x_lo; x <= x_hi; x++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            /* all doors get randomised later */
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            if (mark_escape)
                mark_generation_escape_tunnel(y, x);
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_south && y + 1 < MAX_DUNGEON_HGT
                && cave_feat[y + 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y + 1, x)
                && !(cave_info[y + 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y + 1, x, FEAT_FLOOR);
                cave_corridor1[y + 1][x] = r1;
                cave_corridor2[y + 1][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y + 1, x);
            }
            if (widen_north && y - 1 > 0 && cave_feat[y - 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y - 1, x)
                && !(cave_info[y - 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y - 1, x, FEAT_FLOOR);
                cave_corridor1[y - 1][x] = r1;
                cave_corridor2[y - 1][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y - 1, x);
            }
        }
    }

    apply_h_tunnel_treatment(r1, r2, x_lo, x_hi, y, widen_north, widen_south,
        &local, mark_escape);
}

static bool build_tunnel(
    int r1, int r2, int y1, int x1, int y2, int x2, bool tentative)
{
    tunnel_profile profile = choose_tunnel_profile(tentative);

    /* build a vertical tunnel */
    if (x1 == x2)
    {
        if (!v_tunnel_ok(y1, y2, x1, tentative, 2))
        {
            return (false);
        }
        build_v_tunnel(r1, r2, y1, y2, x1, &profile);
    }

    /* build a horizontal tunnel */
    else if (y1 == y2)
    {
        if (!h_tunnel_ok(x1, x2, y1, tentative, 2))
        {
            return (false);
        }
        build_h_tunnel(r1, r2, x1, x2, y1, &profile);
    }

    /* build an L-shaped tunnel */
    else
    {
        /* build an h-v tunnel */
        if (one_in_(2))
        {
            if (!h_tunnel_ok(x1, x2, y1, tentative, 1)
                || !v_tunnel_ok(y1, y2, x2, tentative, 1))
            {
                return (false);
            }
            build_h_tunnel(r1, r2, x1, x2, y1, &profile);
            build_v_tunnel(r1, r2, y1, y2, x2, &profile);
        }

        /* build a v-h tunnel */
        else
        {
            if (!h_tunnel_ok(x1, x2, y2, tentative, 1)
                || !v_tunnel_ok(y1, y2, x1, tentative, 1))
            {
                return (false);
            }
            build_v_tunnel(r1, r2, y1, y2, x1, &profile);
            build_h_tunnel(r1, r2, x1, x2, y2, &profile);
        }
    }

    return (true);
}

bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate)
{
    int x, y;
    int r1y, r1x, r1y1, r1x1, r1y2, r1x2;
    int r2y, r2x, r2y1, r2x1, r2y2, r2x2;
    bool success;
    int morgoth_margin = 1;

    /* Allow long corridor spans across 3x3 partitions on 15x15 block maps */
    int base_limit_x = MAX(50, (p_ptr->cur_map_wid * 2) / 3); /* ~110 on 165x165 */
    int base_limit_y = MAX(35, (p_ptr->cur_map_hgt * 2) / 3); /* ~110 on 165x165 */
    int distance_limitx = desperate ? base_limit_x + base_limit_x / 2 : base_limit_x;
    int distance_limity = desperate ? base_limit_y + base_limit_y / 2 : base_limit_y;

    r1y = dun->cent[r1].y;
    r1x = dun->cent[r1].x;
    r1y1 = dun->corner[r1].y1;
    r1x1 = dun->corner[r1].x1;
    r1y2 = dun->corner[r1].y2;
    r1x2 = dun->corner[r1].x2;

    r2y = dun->cent[r2].y;
    r2x = dun->cent[r2].x;
    r2y1 = dun->corner[r2].y1;
    r2x1 = dun->corner[r2].x1;
    r2y2 = dun->corner[r2].y2;
    r2x2 = dun->corner[r2].x2;

    if (morgoth_region_active())
    {
        /* Skip any corridor that would cross the throne room partition */
        if (morgoth_segment_blocked(r1y, r1x, r2y, r2x, morgoth_margin))
            return false;
    }

    /* if the rooms are too far apart, then just give up immediately */
    // look at total distance of room centres
    if ((ABS(r1y - r2y) > distance_limity * 3)
        || (ABS(r1x - r2x) > distance_limitx * 3))
    {
        return (false);
    }
    // then look at distance of relevant room edges
    if ((r1x < r2x) && (r2x1 - r1x2 > distance_limitx))
    {
        return (false);
    }
    if ((r2x < r1x) && (r1x1 - r2x2 > distance_limitx))
    {
        return (false);
    }
    if ((r1y < r2y) && (r2y1 - r1y2 > distance_limity))
    {
        return (false);
    }
    if ((r2y < r1y) && (r1y1 - r2y2 > distance_limity))
    {
        return (false);
    }

    /* if we have vertical or horizontal overlap, connect a straight tunnel */
    /* at a random point where they overlap */

    /* if vertical overlap */
    if ((r1x1 <= r2x2) && (r2x1 <= r1x2))
    {
        /* unless careful, there will be too many vertical tunnels */
        /* since rooms are wider than they are tall                */
        if (tentative && one_in_(2))
        {
            return (false);
        }
        x = rand_range(MAX(r1x1, r2x1),
            MIN(r1x2,
                r2x2)); // Sil-x: one of these two lines has somehow caused a
                        // crash:
                        // http://angband.oook.cz/ladder-show.php?id=13070

        if (morgoth_segment_blocked(r1y, x, r2y, x, morgoth_margin))
            return false;
        success = build_tunnel(r1, r2, r1y, x, r2y, x, tentative);
    }
    /* if horizontal overlap */
    else if ((r1y1 <= r2y2) && (r2y1 <= r1y2))
    {
        y = rand_range(MAX(r1y1, r2y1),
            MIN(r1y2,
                r2y2)); // Sil-x: one of these two lines has somehow caused a
                        // crash

        if (morgoth_segment_blocked(y, r1x, y, r2x, morgoth_margin))
            return false;
        success = build_tunnel(r1, r2, y, r1x, y, r2x, tentative);
    }

    /* otherwise, make an L shaped corridor between their centres */
    else
    {
        // this must fail if any of the tunnels would be too long
        if (MIN(ABS(r2x - r1x1), ABS(r2x - r1x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r1x - r2x1), ABS(r1x - r2x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r2y - r1y1), ABS(r2y - r1y2)) > distance_limity - 2)
            return (false);
        if (MIN(ABS(r1y - r2y1), ABS(r1y - r2y2)) > distance_limity - 2)
            return (false);

        if (morgoth_segment_blocked(r1y, r1x, r1y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r2x, r2y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r1x, r2y, r1x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r2y, r1x, r2y, r2x, morgoth_margin))
            return false;

        success = build_tunnel(r1, r2, r1y, r1x, r2y, r2x, tentative);
    }

    if (success)
    {
        dun->connection[r1][r2] = true;
        dun->connection[r2][r1] = true;
    }

    return (success);
}

static bool connect_room_to_corridor(int r)
{
    int length = 10;
    int x;
    int y;
    int delta;
    int ry, rx, r1, r2;
    bool success = false;
    bool done = false;

    ry = dun->cent[r].y;
    rx = dun->cent[r].x;

    y = ry;
    x = rx;

    // go down/right half the time, up/left the other half
    if (one_in_(2))
        delta = 1;
    else
        delta = -1;

    // go horizontal half the time, vertical the other half
    if (one_in_(2))
    {
        while (!done)
        {
            y += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(y - ry) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }
            else if (coord_in_morgoth_region(y, x, 1))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (v_tunnel_ok(ry, y - (delta * 2), x, true, 1))
                    {
                        build_v_tunnel(r, r1, ry, y, x, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    // do the vertical case (very similar to the horizontal one!)
    else
    {
        while (!done)
        {
            x += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(x - rx) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }
            else if (coord_in_morgoth_region(y, x, 1))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (h_tunnel_ok(rx, x - (delta * 2), y, true, 1))
                    {
                        build_h_tunnel(r, r1, rx, x, y, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    return (success);
}

/*
 * Places some staircases near walls
 */
static bool alloc_stairs(int feat, int num)
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

bool feat_within_los(int y0, int x0, int feat)
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
bool stairs_within_los(int y, int x)
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

static bool connectivity_rescue_traversable(int ry, int rx)
{
    if (!in_bounds_fully(ry, rx))
        return false;

    if (cave_feat[ry][rx] == FEAT_WALL_PERM)
        return false;
    if (cave_feat[ry][rx] == FEAT_CHASM)
        return false;

    bool is_wall = (cave_feat[ry][rx] >= FEAT_WALL_HEAD)
        && (cave_feat[ry][rx] <= FEAT_WALL_TAIL)
        && (cave_feat[ry][rx] != FEAT_SECRET);

    /* Never carve through Morgoth's vault walls: require using the forced doors. */
    if (morgoth_level_active && (cave_info[ry][rx] & CAVE_G_VAULT) && is_wall)
        return false;

    /* Avoid carving new routes inside the sealed Morgoth region: only traverse
     * existing vault/tunnel squares there (and don't cross permanent walls). */
    if (coord_in_morgoth_region(ry, rx, 0)
        && !(cave_info[ry][rx] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
        && is_wall)
    {
        return false;
    }

    return true;
}

static int connectivity_unreachable_component(
    int start_y, int start_x,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID])
{
    static const int ddy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            component[y][x] = 0;
        }
    }

    if (!in_bounds_fully(start_y, start_x))
        return 0;
    if (cave_access[start_y][start_x])
        return 0;
    if (!player_passable(start_y, start_x, true))
        return 0;

    component[start_y][start_x] = 1;
    component_cells[tail++] = start_y * MAX_DUNGEON_WID + start_x;

    while (head < tail)
    {
        int cur = component_cells[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + ddy8[d];
            int nx = cx + ddx8[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (component[ny][nx])
                continue;
            if (cave_access[ny][nx])
                continue;
            if (!player_passable(ny, nx, true))
                continue;

            component[ny][nx] = 1;
            nidx = ny * MAX_DUNGEON_WID + nx;
            if (tail < MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
                component_cells[tail++] = nidx;
        }
    }

    return tail;
}

static bool connectivity_component_boundary_cell(
    int y, int x,
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int d = 0; d < 4; ++d)
    {
        int ny = y + ddy4[d];
        int nx = x + ddx4[d];

        if (!in_bounds_fully(ny, nx))
            continue;
        if (component[ny][nx])
            continue;
        if (!connectivity_rescue_traversable(ny, nx))
            continue;

        return true;
    }

    return false;
}

static bool connectivity_rescue_component(
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID],
    int component_count,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int *out_source_y, int *out_source_x,
    int *out_target_y, int *out_target_x,
    int *out_carve_count, int *out_boundary_sources)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;
    int source_y = -1;
    int source_x = -1;
    int carve_count = 0;
    int boundary_sources = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            prev[y][x] = -1;
        }
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;
        prev[cy][cx] = -2;
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] == FEAT_CHASM)
            continue;
        if (!connectivity_component_boundary_cell(cy, cx, component))
            continue;

        prev[cy][cx] = idx;
        if (tail < (int)N_ELEMENTS(queue))
            queue[tail++] = idx;
        boundary_sources++;
    }

    if (out_boundary_sources)
        *out_boundary_sources = boundary_sources;

    if (boundary_sources == 0)
        return false;

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!connectivity_rescue_traversable(ny, nx))
                continue;

            nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (cave_access[ny][nx]
                && player_passable(ny, nx, true)
                && !coord_in_morgoth_region(ny, nx, 1))
            {
                found_y = ny;
                found_x = nx;
                head = tail;
                break;
            }
        }
    }

    if (found_y < 0 || found_x < 0)
        return false;

    {
        int cur = found_y * MAX_DUNGEON_WID + found_x;
        int safety = 0;

        while (safety++ < (int)N_ELEMENTS(queue))
        {
            int cy = cur / MAX_DUNGEON_WID;
            int cx = cur % MAX_DUNGEON_WID;

            if (cave_feat[cy][cx] != FEAT_WALL_PERM)
            {
                bool in_morgoth = coord_in_morgoth_region(cy, cx, 0);
                bool allow_morgoth = (cave_info[cy][cx] & CAVE_MORGOTH_TUNNEL) != 0;

                if (!in_morgoth || allow_morgoth)
                {
                    if (!cave_floor_bold(cy, cx)
                        && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                            || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
                    {
                        cave_set_feat(cy, cx, FEAT_FLOOR);
                        carve_count++;
                    }

                    if (!(cave_info[cy][cx] & CAVE_ROOM))
                        mark_generation_escape_tunnel(cy, cx);
                }
            }

            if (prev[cy][cx] == cur)
            {
                source_y = cy;
                source_x = cx;
                break;
            }

            if (prev[cy][cx] < 0)
                break;
            cur = prev[cy][cx];
        }
    }

    if (out_source_y)
        *out_source_y = source_y;
    if (out_source_x)
        *out_source_x = source_x;
    if (out_target_y)
        *out_target_y = found_y;
    if (out_target_x)
        *out_target_x = found_x;
    if (out_carve_count)
        *out_carve_count = carve_count;

    return (source_y >= 0 && source_x >= 0);
}

/*
 *  Make sure that the level is sufficiently connected.
 */

bool check_connectivity(void)
{
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int y, x;

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Log which room centers are unreachable before rescue attempts */
    flood_access(p_ptr->py, p_ptr->px, cave_access, true);
    int unreachable_rooms = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        int ry = dun->cent[i].y;
        int rx = dun->cent[i].x;
        if (in_bounds_fully(ry, rx) && !cave_access[ry][rx])
        {
            unreachable_rooms++;
            genlog_connect("UNREACHABLE ROOM #%d at (%d,%d) bounds=(%d,%d)-(%d,%d)",
                           i, ry, rx, 
                           dun->corner[i].y1, dun->corner[i].x1,
                           dun->corner[i].y2, dun->corner[i].x2);
        }
    }
    if (unreachable_rooms > 0)
    {
        genlog_fail("PRE-RESCUE: %d/%d rooms unreachable from player at (%d,%d)",
                    unreachable_rooms, dun->cent_n, p_ptr->py, p_ptr->px);
    }
    
    /* Reset for rescue loop */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Attempt connectivity with iterative rescue tunnels for each disconnected component */
    int rescue_attempts = 0;
    while (true)
    {
        // Make sure entire dungeon is connected (ignoring rubble and chasms)
        flood_access(p_ptr->py, p_ptr->px, cave_access, true);
        int unreachable = 0;
        int sample_y = -1, sample_x = -1;
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (player_passable(y, x, true) && (cave_access[y][x] == false))
                {
                    unreachable++;
                    if (sample_y < 0)
                    {
                        sample_y = y;
                        sample_x = x;
                    }
                }

        if (unreachable == 0)
            break;

        /* Prefer sampling an unreachable room center to connect large components early. */
        if (dun)
        {
            for (int i = 0; i < dun->cent_n; ++i)
            {
                int ry = dun->cent[i].y;
                int rx = dun->cent[i].x;
                if (!in_bounds_fully(ry, rx)) continue;
                if (cave_access[ry][rx]) continue;
                if (!player_passable(ry, rx, true)) continue;
                sample_y = ry;
                sample_x = rx;
                break;
            }
        }

        /* Stop if we've tried too many rescues - scale with level size */
        /* Larger levels need more rescue attempts: base 20 + (blocks-8)*4 (and at least ~half room count). */
        int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
        int max_rescues = 20 + MAX(0, (blocks - 8) * 4);  /* 20 for 8 blocks, 72 for 21 blocks */
        if (dun) max_rescues = MAX(max_rescues, 20 + (dun->cent_n / 2));
        if (rescue_attempts++ >= max_rescues)
        {
            log_trace("check_connectivity: %d unreachable passable grids after %d rescues (first at %d,%d) -- FAILING",
                      unreachable, rescue_attempts, sample_y, sample_x);
            genlog_fail("CONNECTIVITY FAILED: %d unreachable passable grids after %d rescues (max=%d), first at (%d,%d)",
                        unreachable, rescue_attempts, max_rescues, sample_y, sample_x);
            return false;
        }

        {
            int component_count;
            int source_y = -1, source_x = -1;
            int found_y = -1, found_x = -1;
            int carve_count = 0;
            int boundary_sources = 0;

            component_count = connectivity_unreachable_component(
                sample_y, sample_x, cave_access, component, component_cells);

            if (component_count <= 0)
            {
                log_trace("check_connectivity: failed to flood unreachable component from (%d,%d)", sample_y, sample_x);
                genlog_fail("CONNECTIVITY FAILED: could not flood unreachable component from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            if (!connectivity_rescue_component(
                    component, component_cells, component_count, cave_access,
                    &source_y, &source_x, &found_y, &found_x,
                    &carve_count, &boundary_sources))
            {
                log_trace("check_connectivity: BFS rescue could not find a reachable target from component at (%d,%d) size=%d boundary=%d",
                    sample_y, sample_x, component_count, boundary_sources);
                genlog_fail("CONNECTIVITY FAILED: BFS rescue could not find reachable target from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            log_trace("check_connectivity: component rescue from (%d,%d) boundary=(%d,%d) to reachable (%d,%d), component=%d boundary=%d carved=%d (unreachable=%d, attempt=%d)",
                sample_y, sample_x, source_y, source_x, found_y, found_x,
                component_count, boundary_sources, carve_count, unreachable,
                rescue_attempts);
            genlog_connect("RESCUE TUNNEL: component=%d boundary=%d from (%d,%d) to (%d,%d), carved=%d",
                component_count, boundary_sources, source_y, source_x, found_y,
                found_x, carve_count);
        }

        /* Clear and loop to re-check connectivity */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
            for (x = 0; x < p_ptr->cur_map_wid; x++)
                cave_access[y][x] = false;
    }

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        return (true);
    }

    if (p_ptr->create_stair == FEAT_MORE
        || p_ptr->create_stair == FEAT_MORE_SHAFT)
    {
        return (true);
    }

    // Make sure player can reach down stairs without going through rubble and
    // chasms
    flood_access(p_ptr->py, p_ptr->px, cave_access, false);
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (((cave_feat[y][x] == FEAT_MORE) && (cave_access[y][x] == true))
                || ((cave_feat[y][x] == FEAT_MORE_SHAFT)
                    && (cave_access[y][x] == true)))
            {
                return (true);
            }
        }

    genlog_fail("CONNECTIVITY FAILED: player cannot reach down stairs without rubble/chasms");
    return (false);
}

/*
 *  Check if there are two adjacent doors on the level.
 */
bool doubled_doors(void)
{
    int y, x;

    // Check each grid within boundary
    for (y = 0; y < p_ptr->cur_map_hgt - 1; y++)
        for (x = 0; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_known_closed_door_bold(y, x))
            {
                if (cave_known_closed_door_bold(y + 1, x))
                    return (true);
                if (cave_known_closed_door_bold(y, x + 1))
                    return (true);
            }

    return (false);
}

bool connect_rooms_stairs(void)
{
    int i;
    int corridor_attempts;
    int r1, r2, r_closest, d_closest, d;
    int pieces = 0;
    int stairs = 0;
    int initial_up = FEAT_LESS;
    int initial_down = FEAT_MORE;

    bool joined;
    bool single_stair_mode = adult_single_stair;
    bool no_down_stairs = (p_ptr->depth >= MORGOTH_DEPTH);
    bool niena_level = (quest_lottery_winner == QUEST_ID_NIENA);

    /* Add backbone links across partition neighbors */
    connect_partition_hubs();

    // Phase 1:
    // connect each room to the closest room (if not already connected)
    // Try normal mode first, then desperate mode if that fails

    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        /* find closest room */
        r_closest = 0; /* default values that will get beaten trivially */
        d_closest = 1000;
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 != r1)
            {
                d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y,
                    dun->cent[r2].x);
                if (d < d_closest)
                {
                    d_closest = d;
                    r_closest = r2;
                }
            }
        }

        /* connect the rooms, if not already connected */
        if (!(dun->connection[r1][r_closest]))
        {
            /* Try normal mode first, then desperate mode */
            if (!connect_two_rooms(r1, r_closest, true, false))
            {
                (void)connect_two_rooms(r1, r_closest, true, true);
            }
        }
    }
    
    // Phase 1.5: Connect to second-closest room as well for redundancy
    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        int closest1 = -1, closest2 = -1;
        int dist1 = 99999, dist2 = 99999;
        
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 == r1) continue;
            d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
            if (d < dist1)
            {
                dist2 = dist1; closest2 = closest1;
                dist1 = d; closest1 = r2;
            }
            else if (d < dist2)
            {
                dist2 = d; closest2 = r2;
            }
        }
        
        /* Try to connect to second-closest if not already connected */
        if (closest2 >= 0 && !(dun->connection[r1][closest2]))
        {
            (void)connect_two_rooms(r1, closest2, true, false);
        }
    }

    // Phase 2:
    // make some random connections between rooms so long as they don't
    // intersect things

    switch (p_ptr->cur_map_hgt / PANEL_HGT)
    {
    case 3:
        corridor_attempts = dun->cent_n * dun->cent_n;
        break;
    case 4:
        corridor_attempts = dun->cent_n * dun->cent_n * 2;
        break;
    case 5:
    default:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
        break;
    }

    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        r2 = rand_int(dun->cent_n);
        if ((r1 != r2) && !(dun->connection[r1][r2]))
        {
            (void)connect_two_rooms(r1, r2, true, false);
        }
    }

    // add some T-intersections in the corridors
    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        (void)connect_room_to_corridor(r1);
    }

    // Phase 3:
    // cut the dungeon up into connected pieces and try hard to make corridors
    // that connect them

    pieces = dungeon_pieces();
    while (pieces > 1)
    {
        joined = false;

        for (r1 = 0; r1 < dun->cent_n; r1++)
        {
            for (r2 = 0; r2 < dun->cent_n; r2++)
            {
                if (!joined && (dun->piece[r1] != dun->piece[r2]))
                {
                    for (i = 0; i < 10; i++)
                    {
                        if (!(dun->connection[r1][r2]))
                        {
                            joined = connect_two_rooms(r1, r2, true, true);
                        }
                    }
                }
            }
        }

        if (!joined)
            break;

        // cut the dungeon up into connected pieces and stop if there is only
        // one
        pieces = dungeon_pieces();
    }

    /* Phase 3.5: L-shaped corridor fallback before force-connect.
     * Try carving clean L-shaped corridors between disconnected pieces.
     * This produces better-looking results than diagonal Bresenham carving. */
    if (pieces > 1)
    {
        int l_connects = 0;
        for (int attempt = 0; attempt < 100 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    if (dun->connection[ra][rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;
            
            int y0 = dun->cent[best_a].y, x0 = dun->cent[best_a].x;
            int y1 = dun->cent[best_b].y, x1 = dun->cent[best_b].x;
            
            /* Try L-shaped corridor (horizontal then vertical, or vice versa) */
            bool carved = false;
            for (int dir = 0; dir < 2 && !carved; ++dir)
            {
                bool valid = true;
                
                /* Check if the L-path is carveable (no permanent walls) */
                int min_x = MIN(x0, x1), max_x = MAX(x0, x1);
                int min_y = MIN(y0, y1), max_y = MAX(y0, y1);
                
                /* Check horizontal leg */
                int leg_y = (dir == 0) ? y0 : y1;
                for (int tx = min_x; tx <= max_x && valid; ++tx)
                {
                    if (!in_bounds_fully(leg_y, tx) || cave_feat[leg_y][tx] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(leg_y, tx, 1))
                        valid = false;
                }
                
                /* Check vertical leg */
                int leg_x = (dir == 0) ? x1 : x0;
                for (int ty = min_y; ty <= max_y && valid; ++ty)
                {
                    if (!in_bounds_fully(ty, leg_x) || cave_feat[ty][leg_x] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(ty, leg_x, 1))
                        valid = false;
                }
                
                if (valid)
                {
                    /* Carve horizontal leg */
                    for (int tx = min_x; tx <= max_x; ++tx)
                    {
                        if (coord_in_morgoth_region(leg_y, tx, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(leg_y, tx))
                            cave_set_feat(leg_y, tx, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    /* Carve vertical leg */
                    for (int ty = min_y; ty <= max_y; ++ty)
                    {
                        if (coord_in_morgoth_region(ty, leg_x, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(ty, leg_x))
                            cave_set_feat(ty, leg_x, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    
                    dun->connection[best_a][best_b] = true;
                    dun->connection[best_b][best_a] = true;
                    carved = true;
                    l_connects++;
                }
            }
            
            pieces = dungeon_pieces();
        }
        
        if (l_connects > 0)
            log_trace("connect_rooms_stairs: L-shaped fallback carved %d connections, pieces now %d", l_connects, pieces);
    }

    /* Last resort: forcibly connect distinct pieces by digging a straight corridor
     * ignoring tunnel safety checks (but respecting permanent walls). This handles
     * adjacent-but-unconnected rooms/vaults seen on dense maps.
     * IMPROVED: Instead of picking random pairs, find the NEAREST pair of rooms
     * from different pieces to minimize ugly cross-map tunnels. */
    if (pieces > 1)
    {
        for (int attempt = 0; attempt < 50 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;  /* No valid pair found */
            
            int a = best_a;
            int b = best_b;

            int y0 = dun->cent[a].y, x0 = dun->cent[a].x;
            int y1 = dun->cent[b].y, x1 = dun->cent[b].x;

            log_trace("force-connect: linking room %d (piece %d) to room %d (piece %d), dist=%d",
                      a, dun->piece[a], b, dun->piece[b], best_dist);

            /* Bresenham carve that ignores h/v tunnel constraints */
            int dy = ABS(y1 - y0), sx = (x0 < x1) ? 1 : -1;
            int dx = ABS(x1 - x0), sy = (y0 < y1) ? 1 : -1;
            int err = (dx > dy ? dx : -dy) / 2;
            int y = y0, x = x0;
            bool aborted = false;
            while (true)
            {
                if (coord_in_morgoth_region(y, x, 1))
                {
                    aborted = true;
                    break;
                }
                if (in_bounds_fully(y, x) && cave_feat[y][x] != FEAT_WALL_PERM)
                {
                    if (!cave_floor_bold(y, x))
                        cave_set_feat(y, x, FEAT_FLOOR);
                }
                if (y == y1 && x == x1) break;
                int e2 = err;
                if (e2 > -dx) { err -= dy; x += sx; }
                if (e2 < dy)  { err += dx; y += sy; }
            }

            if (!aborted)
            {
                dun->connection[a][b] = dun->connection[b][a] = true;
                pieces = dungeon_pieces();
            }
        }

        log_trace("connect_rooms_stairs: forced-connect phase reduced pieces to %d", pieces);
    }

    // label_rooms();

    if (single_stair_mode)
    {
        int down_feat = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;
        int up_feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
        bool down_ok = no_down_stairs;
        bool up_ok = false;

        for (int attempt = 0; attempt < 500 && !down_ok; ++attempt)
        {
            int yy = rand_range(1, p_ptr->cur_map_hgt - 2);
            int xx = rand_range(1, p_ptr->cur_map_wid - 2);

            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                cave_set_feat(yy, xx, down_feat);
                down_ok = true;
            }
        }

        for (int attempt = 0; attempt < 500 && !up_ok; ++attempt)
        {
            int yy = rand_range(1, p_ptr->cur_map_hgt - 2);
            int xx = rand_range(1, p_ptr->cur_map_wid - 2);

            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                cave_set_feat(yy, xx, up_feat);
                up_ok = true;
            }
        }

        if (!down_ok || !up_ok)
        {
            log_trace("connect_rooms_stairs failed: single stair placement failed (down_ok=%d, up_ok=%d)", down_ok, up_ok);
            return (false);
        }

        if (no_down_stairs)
        {
            log_trace("connect_rooms_stairs: single stair mode placed one up stair on the final level");
        }
        else
        {
            log_trace("connect_rooms_stairs: single stair mode placed one up and one down stair");
        }
    }
    else
    {
        /* Calculate number of stairs based on map size: 2 for 66x66, 8 for 165x165 */
        /* Linear interpolation: stairs = 2 + (size - 66) * (8 - 2) / (165 - 66) */
        int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;  /* Average dimension */
        int stairs_max_base = 8;
        int stairs_max_total = 12;
        if (more_stairs)
        {
            stairs_max_base *= 2;
            stairs_max_total *= 2;
        }
        stairs = 2 + ((map_size - 66) * 6) / 99;  /* 6 = (8-2), 99 = (165-66) */
        if (stairs < 2) stairs = 2;   /* Minimum 2 */
        if (stairs > stairs_max_base) stairs = stairs_max_base;  /* Maximum 8 (or doubled) */
        
        /* Labyrinth bonus: +1 stair per labyrinth partition (more escape routes in mazes) */
        if (current_labyrinth_partitions > 0)
        {
            int stair_bonus = current_labyrinth_partitions;
            stairs += stair_bonus;
            log_trace("Labyrinth stair bonus: +%d stairs from %d labyrinth partitions (total=%d)",
                      stair_bonus, current_labyrinth_partitions, stairs);
        }

        if (more_stairs)
        {
            stairs += (stairs + 1) / 2; /* +50% (rounded up) */
        }
        if (stairs > stairs_max_total) stairs = stairs_max_total;
        
        log_trace("Map size %d leads to %d stairs each direction", map_size, stairs);
        if (niena_level && !no_down_stairs)
        {
            log_trace("Niena level: limiting down stairs to a single target stair for the mercy quest");
        }

        /* Determine partition count for guaranteed stair placement */
        int partition_count = (map_size <= 80) ? 2 : 3;  /* Reduced from 4/9 to match lower stair count */
        int grid_rows = 1;
        int grid_cols = partition_count;
        if (partition_count == 4)
        {
            grid_rows = 2;
            grid_cols = 2;
        }
        else if (partition_count == 9)
        {
            grid_rows = 3;
            grid_cols = 3;
        }

        /* Place guaranteed stairs: at least one up and one down per partition */
        int down_placed = 0;
        int up_placed = 0;
        
        /* First pass: place one of each type per partition */
        for (int pi = 0; pi < partition_count; ++pi)
        {
            int row = pi / grid_cols;
            int col = pi % grid_cols;
            
            int y1 = 1 + (row * p_ptr->cur_map_hgt / grid_rows);
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 1;
            int x1 = 1 + (col * p_ptr->cur_map_wid / grid_cols);
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 1;
            
            /* Clamp boundaries */
            if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
            if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;
            
            /* Place one down stair in this partition (unless final level) */
            if (!no_down_stairs && !niena_level)
            {
                for (int attempt = 0; attempt < 100; ++attempt)
                {
                    int yy = rand_range(y1, y2);
                    int xx = rand_range(x1, x2);
                    
                    if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                        cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                        cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                        cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                        cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                    {
                        int feat = (p_ptr->on_the_run) ? FEAT_MORE_SHAFT : 
                                  (down_placed == 0 || p_ptr->depth >= MORGOTH_DEPTH) ? FEAT_MORE : 
                                  choose_down_stairs();
                        cave_set_feat(yy, xx, feat);
                        down_placed++;
                        break;
                    }
                }
            }
            
            /* Place one up stair in this partition */
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                int yy = rand_range(y1, y2);
                int xx = rand_range(x1, x2);
                
                if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                    cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                    cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                {
                    int feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT :
                              (up_placed == 0 || !p_ptr->depth) ? FEAT_LESS :
                              choose_up_stairs();
                    cave_set_feat(yy, xx, feat);
                    up_placed++;
                    break;
                }
            }
        }
        
        log_trace("Guaranteed partition stairs: %d down, %d up placed", down_placed, up_placed);

        /* Second pass: place remaining stairs randomly across the map */
        int down_remaining = 0;
        if (!no_down_stairs)
        {
            down_remaining = niena_level ? 1 : (stairs - down_placed);
        }
        int up_remaining = stairs - up_placed;
        
        /* Place remaining down stairs */
        int down_stairs = down_remaining;
        if (p_ptr->on_the_run)
            down_stairs *= 2;
        if ((p_ptr->create_stair == FEAT_MORE) || (p_ptr->create_stair == FEAT_MORE_SHAFT))
            down_stairs--;
        
        initial_down = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;
        
        if (no_down_stairs)
            down_stairs = 0;

        if (down_stairs > 0 && !(alloc_stairs(initial_down, down_stairs)))
        {
            if (cheat_room)
                msg_format("Failed to place remaining down stairs.");
            log_trace("connect_rooms_stairs failed: Could not place %d remaining down stairs", down_stairs);
            return (false);
        }

        /* Place remaining up stairs */
        int up_stairs = up_remaining;
        if (p_ptr->on_the_run && p_ptr->depth >= 2)
            up_stairs *= 2;
        if ((p_ptr->create_stair == FEAT_LESS) || (p_ptr->create_stair == FEAT_LESS_SHAFT))
            up_stairs--;
        
        initial_up = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
        
        if (up_stairs > 0 && !(alloc_stairs(initial_up, up_stairs)))
        {
            if (cheat_room)
                msg_format("Failed to place remaining up stairs.");
            log_trace("connect_rooms_stairs failed: Could not place %d remaining up stairs", up_stairs);
            return (false);
        }
        
        log_trace("Total stairs placed: %d down, %d up", down_placed + down_stairs, up_placed + up_stairs);
    }

    /* Hack -- Add some quartz streamers */
    for (i = 0; i < DUN_STR_QUA; i++)
    {
        /*if we can't build streamers, something is wrong with level*/
        if (!build_streamer(FEAT_QUARTZ))
        {
            log_trace("connect_rooms_stairs failed: Could not build quartz streamer %d", i);
            return (false);
        }
    }

    /* Do not mix the legacy random-chasm pass with partition chasm rooms. The
     * two systems use different styling/connectivity rules and produce broken
     * visuals/access when overlaid. */
    if (!level_has_chasm_partition())
    {
        build_chasms();
    }
    else
    {
        log_trace("connect_rooms_stairs: skipping legacy build_chasms() because the partition generator already placed chasm terrain");
    }

    return (true);
}
