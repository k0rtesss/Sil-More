#ifndef INCLUDED_APP_MOVEMENT_H
#define INCLUDED_APP_MOVEMENT_H

#include "app-input.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MOVEMENT_FORMAT_VERSION 1u

typedef enum app_movement_context {
    APP_MOVEMENT_CONTEXT_ANY = 0,
    APP_MOVEMENT_CONTEXT_DUNGEON = 1,
    APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT = 2,
    APP_MOVEMENT_CONTEXT_TARGETING = 3
} app_movement_context;

typedef enum app_movement_action {
    APP_MOVEMENT_ACTION_NONE = 0,
    APP_MOVEMENT_ACTION_MOVE_DIR = 1,
    APP_MOVEMENT_ACTION_RUN_DIR = 2,
    APP_MOVEMENT_ACTION_INTERACT_DIR = 3,
    APP_MOVEMENT_ACTION_WAIT = 4,
    APP_MOVEMENT_ACTION_REST = 5
} app_movement_action;

typedef enum app_movement_direction {
    APP_MOVEMENT_DIRECTION_NONE = 0,
    APP_MOVEMENT_DIRECTION_CENTER = 1,
    APP_MOVEMENT_DIRECTION_NORTH = 2,
    APP_MOVEMENT_DIRECTION_NORTHEAST = 3,
    APP_MOVEMENT_DIRECTION_EAST = 4,
    APP_MOVEMENT_DIRECTION_SOUTHEAST = 5,
    APP_MOVEMENT_DIRECTION_SOUTH = 6,
    APP_MOVEMENT_DIRECTION_SOUTHWEST = 7,
    APP_MOVEMENT_DIRECTION_WEST = 8,
    APP_MOVEMENT_DIRECTION_NORTHWEST = 9
} app_movement_direction;

typedef enum app_movement_preset_id {
    APP_MOVEMENT_PRESET_NONE = 0,
    APP_MOVEMENT_PRESET_MODERN_ARROWS = 1,
    APP_MOVEMENT_PRESET_MODERN_WASD_QEZC = 2,
    APP_MOVEMENT_PRESET_VI_KEYS = 3,
    APP_MOVEMENT_PRESET_CLASSIC_SIL = 4
} app_movement_preset_id;

typedef enum app_movement_command_flag {
    APP_MOVEMENT_COMMAND_FLAG_REPEAT = 0x0001u,
    APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC = 0x0002u
} app_movement_command_flag;

typedef struct app_movement_direction_payload {
    u16b direction;
    s16b dy;
    s16b dx;
} app_movement_direction_payload;

typedef struct app_movement_binding {
    u16b context;
    u16b action;
    u16b direction;
    u16b device;
    u16b input_type;
    u16b required_modifiers;
    u16b forbidden_modifiers;
    u16b reserved;
    u32b trigger;
    u32b trigger_aux;
} app_movement_binding;

typedef struct app_movement_command {
    u16b format_version;
    u16b context;
    u16b action;
    u16b flags;
    u16b modifiers;
    u16b device;
    u16b input_type;
    u16b source_id;
    u32b trigger;
    u32b trigger_aux;
    u64b sequence;
    u64b timestamp_usec;
    app_movement_direction_payload direction;
} app_movement_command;

void app_movement_binding_clear(app_movement_binding* binding);
void app_movement_command_clear(app_movement_command* command);
bool app_movement_action_is_directional(u16b action);
bool app_movement_direction_payload_from_direction(u16b direction,
    app_movement_direction_payload* out_payload);
bool app_movement_direction_from_legacy_keypad(int keypad_dir,
    u16b* out_direction);
int app_movement_direction_to_legacy_keypad(u16b direction);
bool app_movement_binding_is_valid(const app_movement_binding* binding);
bool app_movement_command_is_valid(const app_movement_command* command);
bool app_movement_binding_matches_input(const app_movement_binding* binding,
    const app_input* input, u16b context);
bool app_movement_bindings_conflict(const app_movement_binding* left,
    const app_movement_binding* right);
bool app_movement_command_from_binding(const app_movement_binding* binding,
    const app_input* input, u16b context,
    app_movement_command* out_command);
bool app_movement_resolve_input(const app_movement_binding* bindings,
    size_t binding_count, const app_input* input, u16b context,
    app_movement_command* out_command);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_MOVEMENT_H */
