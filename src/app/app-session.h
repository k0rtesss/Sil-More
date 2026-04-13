#ifndef INCLUDED_APP_SESSION_H
#define INCLUDED_APP_SESSION_H

#include "app-events.h"
#include "app-host.h"
#include "app-interaction.h"
#include "app-input.h"
#include "app-scene-bootstrap.h"
#include "app-scene-dungeon.h"
#include "app-scene-information.h"
#include "app-scene-menu.h"
#include "app-snapshot.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SESSION_API_VERSION 0x00010000u
#define APP_SESSION_DEFAULT_EVENT_CAPACITY APP_EVENT_BUFFER_DEFAULT_CAPACITY

typedef enum app_session_state {
    APP_SESSION_STATE_UNINITIALIZED = 0,
    APP_SESSION_STATE_IDLE = 1,
    APP_SESSION_STATE_RUNNING = 2,
    APP_SESSION_STATE_WAITING = 3,
    APP_SESSION_STATE_STOPPING = 4,
    APP_SESSION_STATE_STOPPED = 5,
    APP_SESSION_STATE_FAULTED = 6
} app_session_state;

typedef enum app_wait_reason {
    APP_WAIT_REASON_NONE = 0,
    APP_WAIT_REASON_BOOTSTRAP = 1,
    APP_WAIT_REASON_COMMAND_INPUT = 2,
    APP_WAIT_REASON_CONFIRM = 3,
    APP_WAIT_REASON_LIST_SELECTION = 4,
    APP_WAIT_REASON_TARGETING = 5,
    APP_WAIT_REASON_INFORMATIONAL_PAUSE = 6,
    APP_WAIT_REASON_SCENE_TRANSITION = 7,
    APP_WAIT_REASON_SHUTDOWN = 8,
    APP_WAIT_REASON_FAULT = 9
} app_wait_reason;

typedef enum app_session_flag {
    APP_SESSION_FLAG_ALLOW_LEGACY_INPUT = 0x00000001u,
    APP_SESSION_FLAG_ALLOW_INTENT_INPUT = 0x00000002u,
    APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT = 0x00000004u,
    APP_SESSION_FLAG_EXTERNAL_DRIVE = 0x00000008u
} app_session_flag;

typedef struct app_wait_state {
    u16b reason;
    u16b flags;
    s32b detail0;
    s32b detail1;
} app_wait_state;

typedef struct app_wait_scope {
    bool active;
    u16b state;
    app_wait_state wait_state;
} app_wait_scope;

typedef struct app_session_config {
    u32b api_version;
    u32b flags;
    size_t initial_event_capacity;
    const app_host* host;
} app_session_config;

typedef struct app_session_counters {
    u64b submitted_inputs;
    u64b submitted_intents;
    u64b consumed_inputs;
    u64b consumed_intents;
    u64b emitted_events;
    u64b dropped_events;
} app_session_counters;

typedef struct app_session_export {
    u32b api_version;
    u32b flags;
    u16b state;
    u16b wait_reason;
    u16b wait_flags;
    u16b snapshot_scene;
    s32b wait_detail0;
    s32b wait_detail1;
    u16b snapshot_flags;
    u16b reserved0;
    u32b snapshot_blob_count;
    u32b pending_input_count;
    u32b pending_intent_count;
    u32b pending_event_count;
    u32b pending_event_dropped_count;
    u32b reserved1;
    u64b snapshot_revision;
    app_session_counters counters;
} app_session_export;

typedef struct app_session app_session;
typedef bool (*app_session_advance_callback)(app_session* session,
    void* user_data);

app_session* app_session_current(void);
void app_session_make_current(app_session* session);
void app_session_set_advance_callback(app_session* session,
    app_session_advance_callback callback, void* user_data);
bool app_session_can_advance(const app_session* session);
u16b app_session_advance_until_waiting(app_session* session);
app_session* app_session_create(const app_session_config* config);
void app_session_destroy(app_session* session);
const app_host* app_session_host(const app_session* session);
u32b app_session_flags(const app_session* session);
void app_session_set_flags(app_session* session, u32b flags);
bool app_session_has_flag(const app_session* session, u32b flag_mask);
u16b app_session_state_id(const app_session* session);
void app_session_set_state(app_session* session, u16b state);
const app_wait_state* app_session_wait_state(const app_session* session);
void app_session_set_wait_state(app_session* session,
    const app_wait_state* wait_state);
void app_session_begin_wait(app_session* session, u16b reason, s32b detail0,
    s32b detail1);
void app_session_resume_running(app_session* session);
void app_session_push_wait_scope(app_session* session, app_wait_scope* scope,
    u16b reason, s32b detail0, s32b detail1);
void app_session_pop_wait_scope(app_session* session,
    const app_wait_scope* scope);
const app_snapshot* app_session_snapshot(const app_session* session);
void app_session_set_snapshot(app_session* session,
    const app_snapshot* snapshot);
const app_bootstrap_snapshot* app_session_bootstrap_snapshot(
    const app_session* session);
const app_dungeon_snapshot* app_session_dungeon_snapshot(
    const app_session* session);
const app_information_snapshot* app_session_information_snapshot(
    const app_session* session);
const app_menu_snapshot* app_session_menu_snapshot(
    const app_session* session);
void app_session_clear_bootstrap_snapshot(app_session* session);
bool app_session_publish_bootstrap_scene(app_session* session,
    const app_bootstrap_scene* scene);
void app_session_clear_information_snapshot(app_session* session);
bool app_session_publish_information_scene(app_session* session,
    const app_information_scene* scene);
bool app_session_add_information_op(app_session* session, s16b row,
    s16b col, byte attr, cptr text);
bool app_session_add_information_op_ex(app_session* session, s16b row,
    s16b col, byte attr, byte story, cptr text);
bool app_session_add_information_cell_ex(app_session* session, s16b row,
    s16b col, byte attr, char ch, byte terrain_attr, char terrain_char,
    byte story, byte width);
bool app_session_add_information_cursor(app_session* session, s16b row,
    s16b col, byte attr, byte width);
bool app_session_publish_information_snapshot(app_session* session);
void app_session_clear_menu_snapshot(app_session* session);
bool app_session_publish_menu_scene(app_session* session,
    const app_ui_scene* scene);
void app_session_clear_dungeon_overlay_scene(app_session* session);
bool app_session_publish_dungeon_overlay_scene(app_session* session,
    const app_ui_scene* scene);
void app_session_mark_snapshot_dirty(app_session* session,
    u32b invalidation_mask);
bool app_session_build_dungeon_snapshot(app_session* session,
    u32b update_mask, u32b redraw_mask, u32b window_mask);
void app_session_note_message(app_session* session, u16b message_type);
void app_session_note_animation(app_session* session, u16b animation_kind,
    s32b subject, s32b arg0, s32b arg1, s32b arg2,
    u32b invalidation_mask);
void app_session_note_cursor_relative(app_session* session, s16b map_y,
    s16b map_x);
void app_session_note_cursor_absolute(app_session* session, s16b row,
    s16b col, bool visible);
void app_session_set_cursor_visible(app_session* session, bool visible);
bool app_session_interactions_enabled(const app_session* session);
const app_interaction_state* app_session_interaction(
    const app_session* session);
void app_session_clear_interaction(app_session* session);
void app_session_begin_interaction(app_session* session, u16b kind,
    u16b reason, u16b flags);
void app_session_set_interaction_prompt(app_session* session, byte attr,
    cptr prompt);
void app_session_set_interaction_detail(app_session* session, byte attr,
    cptr detail);
void app_session_set_interaction_value(app_session* session, byte attr,
    cptr value, s16b cursor_index);
void app_session_clear_interaction_options(app_session* session);
void app_session_set_interaction_selected(app_session* session,
    s16b selected_index);
bool app_session_add_interaction_option(app_session* session, byte attr,
    char tag, bool enabled, bool selected, cptr label, cptr meta);
const app_session_counters* app_session_get_counters(
    const app_session* session);
void app_session_export_state(const app_session* session,
    app_session_export* out_state);
bool app_session_submit_input(app_session* session, const app_input* input);
size_t app_session_pending_input_count(const app_session* session);
bool app_session_peek_input(const app_session* session, app_input* out_input);
bool app_session_pop_input(app_session* session, app_input* out_input);
void app_session_clear_inputs(app_session* session);
bool app_session_submit_intent(app_session* session, const app_intent* intent);
size_t app_session_pending_intent_count(const app_session* session);
bool app_session_peek_intent(const app_session* session,
    app_intent* out_intent);
bool app_session_pop_intent(app_session* session, app_intent* out_intent);
void app_session_clear_intents(app_session* session);
bool app_session_emit_event(app_session* session,
    const app_event_record* record);
app_event_span app_session_view_events(const app_session* session);
app_event_span app_session_drain_events(app_session* session);
void app_session_clear_events(app_session* session);

/*
 * Canonical boundary entry points promised by the UI architecture ADR.
 * These wrap the session-scoped helpers above so later drivers and hosts can
 * target one stable surface while the runtime extraction continues.
 */
bool app_submit_input(app_session* session, const app_input* input);
bool app_submit_intent(app_session* session, const app_intent* intent);
u16b app_advance_until_waiting(app_session* session);
const app_snapshot* app_get_snapshot(const app_session* session);
app_event_span app_view_events(const app_session* session);
app_event_span app_drain_events(app_session* session);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SESSION_H */
