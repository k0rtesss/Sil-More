#ifndef INCLUDED_CAVE_ENVIRONMENT_H
#define INCLUDED_CAVE_ENVIRONMENT_H

#include "h-basic.h"

enum environment_bridge_material {
    ENV_BRIDGE_NONE, ENV_BRIDGE_WOOD, ENV_BRIDGE_STONE
};

enum environment_cell_flags {
    ENV_PROTECTED = 1, ENV_FLOOR_ICE = 2, ENV_ADDED_LIQUID = 4,
    ENV_MINERAL_SPENT = 8, ENV_BRIDGE = 16, ENV_NATURAL = 32,
    ENV_DEPOSIT = 64
};
#define ENV_SOURCES_MAX 32
typedef struct environment_cell {
    byte flags, base_feat, known_feat, bridge_feat, underlay, material;
    byte integrity, work, pending_feat, owner;
    byte known_underlay, known_material;
    s16b heat;
    s32b due;
} environment_cell;
typedef struct environment_source {
    byte y, x, feature, capacity, used, phase;
    s32b next_turn;
} environment_source;
typedef struct environment_state {
    u32b random;
    s32b last_turn;
    byte source_count, mineral_budget;
    bool ready;
} environment_state;

typedef struct environment_bridge_job {
    byte y, x, feature, material, integrity;
    bool repair;
} environment_bridge_job;

void cave_environment_reset(void);
void cave_environment_seed(void);
void cave_environment_process(void);
void cave_environment_set_speed(byte speed);
void cave_environment_changed(int y, int x, int old_feat, int new_feat);
void cave_environment_flood_bridge(int y, int x, int liquid, int force);
int cave_environment_pending_hazard(int y, int x);
bool cave_environment_bridge_job_at(int y, int x, environment_bridge_job* job);
/* One unit of work is one worker action. True means the crossing completed. */
bool cave_environment_bridge_work(int y, int x, int material);
int cave_environment_bridge_progress(int y, int x);
bool cave_environment_job_safe(int y, int x);
bool cave_environment_describe(int y, int x, char* text, size_t size);
int cave_environment_thaw_feature(int y, int x, int fallback);
int cave_environment_known_feature(int y, int x);
int cave_environment_display_underlay(int y, int x);
void cave_environment_observe(int y, int x);
const environment_cell* cave_environment_cell_at(int y, int x);
const environment_source* cave_environment_source_at(int index);
environment_state cave_environment_get_state(void);
bool cave_environment_restore_state(environment_state state);
bool cave_environment_restore_cell(int y, int x, environment_cell cell);
bool cave_environment_restore_source(int index, environment_source source);

#endif
