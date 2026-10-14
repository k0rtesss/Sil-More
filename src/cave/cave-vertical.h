#ifndef INCLUDED_CAVE_VERTICAL_H
#define INCLUDED_CAVE_VERTICAL_H

#include "h-basic.h"

/* Geometry belongs to the two resident neighbours, not to an archive of old
 * maps. A counterpart carries only the small piece which must be rebuilt. */
#define CAVE_VERTICAL_PORTALS 24
#define CAVE_VERTICAL_BASINS 8
#define CAVE_VERTICAL_PATCH 7

enum cave_vertical_kind {
    CAVE_VERTICAL_STREAM = 1, CAVE_VERTICAL_RAPIDS, CAVE_VERTICAL_FALL,
    CAVE_VERTICAL_SUMP, CAVE_VERTICAL_LAVA_FALL, CAVE_VERTICAL_LAKE,
    CAVE_VERTICAL_CONTROL, CAVE_VERTICAL_TUBE
};
enum cave_vertical_portal_flags {
    CAVE_VERTICAL_DOWN = 1, CAVE_VERTICAL_REVERSIBLE = 2,
    CAVE_VERTICAL_DISCOVERED = 4, CAVE_VERTICAL_GATE = 8,
    CAVE_VERTICAL_CLOSED = 16, CAVE_VERTICAL_BROKEN = 32
};
enum cave_vertical_view_flags {
    CAVE_VERTICAL_BELOW_VISIBLE = 1, CAVE_VERTICAL_OUTFLOW = 2,
    CAVE_VERTICAL_INFLOW = 4, CAVE_VERTICAL_CEILING = 8,
    CAVE_VERTICAL_PASSAGE = 16, CAVE_VERTICAL_DIVE = 32,
    CAVE_VERTICAL_DRY_TUBE = 64
};
typedef struct cave_vertical_view_info {
    byte flags, feature, attr, fluid_feature, direction;
    char chr;
    s16b relative_depth;
} cave_vertical_view_info;

enum cave_vertical_basin_flags {
    CAVE_VERTICAL_RISE_WARNED = 1, CAVE_VERTICAL_OVERFLOW_WARNED = 2,
    CAVE_VERTICAL_RESERVOIR = 8, CAVE_VERTICAL_SOURCE = 32,
    CAVE_VERTICAL_SINK = 64, CAVE_VERTICAL_CHANGED = 128
};

typedef struct cave_vertical_portal {
    u32b identity;
    s16b y, x, peer_depth, peer_y, peer_x;
    byte kind, material, strength, direction, flags, pending_material;
    byte local_style, peer_style;
    byte local_patch[CAVE_VERTICAL_PATCH][CAVE_VERTICAL_PATCH];
    byte peer_patch[CAVE_VERTICAL_PATCH][CAVE_VERTICAL_PATCH];
    u16b pending;
    s32b due;
} cave_vertical_portal;

typedef struct cave_vertical_basin {
    u32b identity;
    s16b depth, y, x;
    u16b volume, capacity, normal_volume;
    byte material, radius, hot, flags;
} cave_vertical_basin;

typedef struct cave_vertical_state {
    byte portal_count, basin_count, breath, submerged;
    s32b last_turn;
    cave_vertical_portal portals[CAVE_VERTICAL_PORTALS];
    cave_vertical_basin basins[CAVE_VERTICAL_BASINS];
} cave_vertical_state;

const cave_vertical_state* cave_vertical_get(void);
void cave_vertical_capture(cave_vertical_state* state);
bool cave_vertical_restore(const cave_vertical_state* state);
void cave_vertical_reset(void);
void cave_vertical_generation_begin(bool allow_down);
void cave_vertical_generation_inherit(const cave_vertical_state* source,
    int source_depth, int target_depth);
void cave_vertical_generation_reserve(void);
bool cave_vertical_generation_fits(int y1, int x1, int y2, int x2);
bool cave_vertical_generation_finish(void);
void cave_vertical_generation_vaults(void);
void cave_vertical_generation_extent(int* height, int* width);

bool cave_vertical_view(int y, int x, cave_vertical_view_info* view);
bool cave_vertical_describe(int y, int x, char* text, size_t size);
bool cave_vertical_travel(bool ascending);
bool cave_vertical_can_travel(bool ascending);
bool cave_vertical_forced_travel(int y, int x);
int cave_vertical_fall_damage(int levels, int landing_feature);
void cave_vertical_flow_build(void);
void cave_vertical_end_action(void);
void cave_vertical_process(void);
void cave_vertical_refresh(void);
bool cave_vertical_interact(int y, int x);
int cave_vertical_current(int y, int x, int* direction);
int cave_vertical_movement_energy(int energy, int from_y, int from_x,
    int to_y, int to_x);
void cave_vertical_terrain_changed(int y, int x, int old_feature);
struct level_image;
void cave_vertical_process_image(struct level_image* image);
bool cave_vertical_receive(cave_vertical_state* state, int depth,
    u32b identity, int material, int amount);

#endif
