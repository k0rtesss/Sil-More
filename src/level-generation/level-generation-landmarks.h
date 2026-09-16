#ifndef INCLUDED_LEVEL_GENERATION_LANDMARKS_H
#define INCLUDED_LEVEL_GENERATION_LANDMARKS_H

#include "h-basic.h"

typedef struct terrain_landmark_stats
{
    int attempted, accepted, material, kind;
    int tiles, span, partitions, bridges, lakes, tributaries, vaults;
    int excavated, original_floor, relocated_monsters, relocated_objects;
    int y1, x1, y2, x2;
    int rejected_route, rejected_access, rejected_contents;
    int family, basin_count, basin_tiles, channel_cells, narrow_cells;
    int confluences, road_crossings, bridge_savings;
    int sources, outlets, edge_mouths, springs, sinks, terminal_basins, vents, fissure_tips;
    int flooded_structures, breached_structures, repaired_structures;
    int rubble_tiles, repair_tiles, scenario_retries, scenario;
    int overflow_tiles;
} terrain_landmark_stats;

enum { TERRAIN_LANDMARK_RIVER, TERRAIN_LANDMARK_RIFT, TERRAIN_LANDMARK_LAKE };
/* Liquid families use terrain_network_family; fractures have their own form. */
#define TERRAIN_LANDMARK_FRACTURE_FAMILY 5
enum { TERRAIN_LANDMARK_NONE, TERRAIN_LANDMARK_TERRAIN,
    TERRAIN_LANDMARK_BRIDGE, TERRAIN_LANDMARK_BANK,
    TERRAIN_LANDMARK_RUBBLE, TERRAIN_LANDMARK_REPAIR };
enum { TERRAIN_TERMINAL_NONE, TERRAIN_TERMINAL_EDGE, TERRAIN_TERMINAL_SPRING,
    TERRAIN_TERMINAL_SINK, TERRAIN_TERMINAL_BASIN, TERRAIN_TERMINAL_VENT, TERRAIN_TERMINAL_FISSURE };
enum { TERRAIN_FLOW_LAKE_CHAIN, TERRAIN_FLOW_TRAVERSE, TERRAIN_FLOW_SPRING,
    TERRAIN_FLOW_VOLCANIC, TERRAIN_FLOW_POOLS, TERRAIN_FLOW_WORKS,
    TERRAIN_FLOW_FRACTURE };

void terrain_landmark_reset(void);
bool place_terrain_landmark(void);
const terrain_landmark_stats* terrain_landmark_last_stats(void);
int terrain_landmark_cell(int y, int x);
int terrain_landmark_basin_cell(int y, int x);
bool terrain_landmark_channel_cell(int y, int x);
int terrain_landmark_terminal_cell(int y, int x);
/* Terminal role bits: bit 0 is a source, bit 1 is a receiving outlet. */
int terrain_landmark_terminal_role(int y, int x);
int terrain_landmark_structure_cell(int y, int x);
bool terrain_landmark_partition(int partition);

/* A system is planned before construction; its geometry survives the later
 * room, tunnel and incident passes. These records are generation-only. */
void terrain_landmark_plans_reset(void);
bool terrain_landmark_plan_system(int system, int material);
void terrain_landmark_foundation(int system);
bool terrain_landmark_realize(int system, int epoch);
int terrain_landmark_planned_feature(int system, int y, int x);
int terrain_landmark_planned_cap(int system, int y, int x);
int terrain_landmark_system_cell(int system, int y, int x, int field);
const terrain_landmark_stats* terrain_landmark_system_stats(int system);

#endif
