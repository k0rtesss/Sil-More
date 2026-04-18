/* File: level-generation-connectivity-partitions.c */

#include "angband.h"
#include "level-generation/level-generation.h"
#include "log/log.h"
#include "level-generation/gen-log.h"
#include "metarun.h"
#include "level-generation/level-generation-internal.h"
#include <string.h>

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
    int gem, int staff, int light, int arrows, int tunneling)
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
    profile->supply_light = light;
    profile->supply_arrows = arrows;
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
            40, 30, 10, 20, 1, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 10, 0, 0, 0, 0, 0, 0, 0, 0);
        break;

    case LEVEL_PART_CAVEY:
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_FLOOR],
            0, 0, 0, 100, 0, 0, 12, 3, 6, 6, 1);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            25, 25, 25, 0, 0, 0, 0, 0, 0, 0, 0);
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
            40, 35, 0, 25, 7, 2, 1, 3, 15, 15, 2);
        cfg->profiles[PARTITION_DROP_SOURCE_FLOOR].allow_damaged = true;
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
            0, 0, 35, 65, 15, 2, 2, 15, 5, 5, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0);
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
            40, 30, 20, 10, 1, 1, 1, 1, 1, 1, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            40, 30, 20, 0, 0, 0, 0, 0, 0, 0, 0);
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
            20, 20, 15, 45, 0, 2, 8, 2, 0, 0, 0);
        partition_config_profile_assign(&cfg->profiles[PARTITION_DROP_SOURCE_CHEST],
            20, 20, 15, 0, 0, 0, 0, 0, 0, 0, 0);
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

level_partition_kind partition_kind_from_mode(quadrant_mode_t mode)
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

static partition_drop_profile partition_drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof;
    prof.allow_floor_drops = true;
    drop_profile_default(&prof.profile);

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        /* Default (ROOMY) aÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã
Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â 40:30:10:20 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 10;
        prof.profile.weight_supply = 20;
        prof.profile.supply_potion = 2;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 2;
        prof.profile.supply_light = 1;
        prof.profile.supply_arrows = 1;
        break;
    case QUAD_MODE_LABYRINTH:
        /* LABYRINTH - 0:0:35:65 */
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 35;
        prof.profile.weight_supply = 65;
        prof.profile.supply_potion = 30;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 4;
        prof.profile.supply_staff = 30;
        prof.profile.supply_light = 5;
        prof.profile.supply_arrows = 5;
        break;
    case QUAD_MODE_RUINED:
        /* RUINED 40:35:0:25 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 35;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 25;
        prof.profile.supply_potion = 14;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 6;
        prof.profile.supply_light = 15;
        prof.profile.supply_arrows = 15; /* arrows plus misc leftovers */
        prof.profile.supply_tunneling = 4; /* small chance for shovels/mattocks */
        prof.profile.allow_damaged = true;
        break;
    case QUAD_MODE_CAVEY:
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 100;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 0;
        prof.profile.supply_gem = 24;
        prof.profile.supply_staff = 6;
        prof.profile.supply_light = 6;
        prof.profile.supply_arrows = 6;
        prof.profile.supply_tunneling = 2;
        break;
    case QUAD_MODE_BIG_CAVE:
        /* BIG_CAVE 20:20:15:45 */
        prof.profile.weight_weapon = 20;
        prof.profile.weight_armor = 20;
        prof.profile.weight_jewelry = 15;
        prof.profile.weight_supply = 45;
        prof.profile.supply_potion = 0;
        prof.profile.supply_herb = 4;
        prof.profile.supply_gem = 16;
        prof.profile.supply_staff = 4;
        prof.profile.supply_light = 0;
        prof.profile.supply_arrows = 0;
        break;
    case QUAD_MODE_CHASM:
        /* CHASM 40:30:20:10 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 20;
        prof.profile.weight_supply = 10;
        prof.profile.supply_potion = 2;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 2;
        prof.profile.supply_light = 1;
        prof.profile.supply_arrows = 1;
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
    bool require_room_tile = partition_mode_avoids_corridor_spawns(mode);

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
    /* CAVEY partitions should populate across their full floor footprint.
     * Reusing the chest-style room-only filter dumps the whole monster quota
     * into whichever plain room happens to dominate the partition. */
    bool avoid_corridors = partition_mode_avoids_corridor_spawns(plan->mode)
        && (plan->mode != QUAD_MODE_CAVEY);

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
