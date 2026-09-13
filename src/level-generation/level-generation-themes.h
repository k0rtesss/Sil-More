#ifndef INCLUDED_LEVEL_GENERATION_THEMES_H
#define INCLUDED_LEVEL_GENERATION_THEMES_H

#include "h-basic.h"

enum terrain_theme_material
{
    TERRAIN_THEME_WATER,
    TERRAIN_THEME_CHASM,
    TERRAIN_THEME_LAVA,
    TERRAIN_THEME_POISON,
    TERRAIN_THEME_ICE,
    TERRAIN_THEME_MATERIAL_MAX
};

typedef struct terrain_theme_profile
{
    char name[48];
    int weights[TERRAIN_THEME_MATERIAL_MAX];
    int chance;
    int min_length, max_length;
    int max_width;
    int pool_chance;
} terrain_theme_profile;

/* Network chance/reach; material uses the matching D weights.
 * Compact chamber/pool families are exempt from the minimum map span. */
typedef struct terrain_landmark_profile
{
    int chance;
    int min_span_percent, max_span_percent;
    /* Ordinary non-narrow links draw from MAX(2,min_width)..max_width.
     * max_width=1 keeps all ordinary links narrow; broad river cores use 2..4. */
    int min_width, max_width;
    int lake_chance;   /* Broad river's chance of a second basin, within N max. */
    int branch_chance; /* Optional feeder for chain, chamber and broad river. */
} terrain_landmark_profile;

enum terrain_network_family
{
    TERRAIN_NETWORK_CHAIN,
    TERRAIN_NETWORK_TRIBUTARIES,
    TERRAIN_NETWORK_CHAMBER,
    TERRAIN_NETWORK_POOLS,
    TERRAIN_NETWORK_RIVER,
    TERRAIN_NETWORK_FAMILY_MAX
};

typedef struct terrain_network_profile
{
    int weights[TERRAIN_NETWORK_FAMILY_MAX];
    /* Count range for chain/pools; chamber has one, tributaries have up to
     * two within max, and broad rivers have one or two within max. */
    int min_basins, max_basins;
    int min_radius, max_radius; /* Bounds for each irregular basin's rx/ry. */
    int one_tile_percent; /* Literal one-cell roll for ordinary channel runs. */
} terrain_network_profile;

typedef struct terrain_history_profile
{
    int ancient, disaster, overflow;
    int second_system_chance;
} terrain_history_profile;
const terrain_history_profile* terrain_history_for_depth(int depth);

/* Called once during normal initialization. Invalid/missing files use defaults. */
bool terrain_themes_load(void);

/* Depth is the dungeon level (50 ft per level); outside 1..20 is disabled. */
const terrain_theme_profile* terrain_theme_for_depth(int depth);
const terrain_landmark_profile* terrain_landmark_for_depth(int depth);
const terrain_network_profile* terrain_network_for_depth(int depth);

#endif
