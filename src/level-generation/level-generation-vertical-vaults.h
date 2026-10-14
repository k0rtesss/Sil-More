#ifndef INCLUDED_LEVEL_GENERATION_VERTICAL_VAULTS_H
#define INCLUDED_LEVEL_GENERATION_VERTICAL_VAULTS_H

#include "h-basic.h"

#define VERTICAL_VAULT_MAX 4

enum vertical_vault_role
{
    VERTICAL_VAULT_GATE = 1,
    VERTICAL_VAULT_UPPER_BASIN,
    VERTICAL_VAULT_LOWER_BASIN,
    VERTICAL_VAULT_SPILLWAY,
    VERTICAL_VAULT_OUTLET,
    VERTICAL_VAULT_OVERLOOK,
    VERTICAL_VAULT_STAIR,
    VERTICAL_VAULT_APPROACH,
    VERTICAL_VAULT_DRAIN_REWARD
};

typedef struct vertical_vault_placement
{
    u32b identity;
    u16b upper_template, lower_template;
    s16b upper_depth;
    s16b y, x; /* Shared transformed top-left origin. */
    byte transform; /* bit0 mirror rows, bit1 mirror cols, bit2 transpose */
    byte discovered_layers;
    byte reward_layers; /* A layer's authored reward attempt is consumed. */
} vertical_vault_placement;

typedef struct vertical_vault_state
{
    byte count;
    vertical_vault_placement placements[VERTICAL_VAULT_MAX];
} vertical_vault_state;

typedef struct vertical_vault_marker
{
    byte role;
    s16b y, x, target_depth, target_y, target_x;
    u32b identity;
} vertical_vault_marker;

void vertical_vault_reset(void);
void vertical_vault_begin(int depth, bool allow_down);
const vertical_vault_state *vertical_vault_get(void);
void vertical_vault_capture(vertical_vault_state *out);
void vertical_vault_restore(const vertical_vault_state *state);
void vertical_vault_inherit(const vertical_vault_state *neighbor, int target_depth);
bool vertical_vault_place(void);
bool vertical_vault_fits(int y1, int x1, int y2, int x2);
bool vertical_vault_marker_at(int index, vertical_vault_marker *out);
/* Authored liquid footprint: 3 deep, 2 shallow, 1 adjacent dry bank, 0
 * protected/unrelated. Explicit image state makes this safe for parked maps. */
int vertical_vault_liquid_profile(const vertical_vault_state *state, int depth,
    u32b identity, int y, int x);
bool vertical_vault_peer_marker_at(int index, vertical_vault_marker *out);
/* Return a template id only on the shared pair's first discovery. */
int vertical_vault_visit(int y, int x);
/* Called after active/worker basin painting. A submerged reward consumes its
 * existing layer bit only after reaching dry floor, never merely on build. */
void vertical_vault_release_rewards(void);

/* build_vault uses these only during the paired module's construction. */
bool vertical_vault_build_transform(bool *flip_v, bool *flip_h);
bool vertical_vault_suppress_rewards(void);

#endif
