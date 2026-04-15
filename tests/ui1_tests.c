#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "angband.h"
#include "app/app-events.h"
#include "app/app-host.h"
#include "app/app-movement.h"
#include "app/app-scene-menu.h"
#include "app/app-session.h"
#include "app/app-ui.h"
#include "externs.h"
#include "sdl-config.h"
#include "ui/ui-information-scene.h"
#include <SDL3/SDL_scancode.h>

static int g_failures = 0;

#define CHECK(expr)                                                         \
    do                                                                      \
    {                                                                       \
        if (!(expr))                                                        \
        {                                                                   \
            fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__,  \
                __LINE__);                                                  \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

static app_event_record make_event(u16b kind, u16b scope, u32b sequence,
    s32b subject, s32b arg0, s32b arg1, s32b arg2)
{
    app_event_record record;

    memset(&record, 0, sizeof(record));
    record.kind = kind;
    record.scope = scope;
    record.sequence = sequence;
    record.subject = subject;
    record.arg0 = arg0;
    record.arg1 = arg1;
    record.arg2 = arg2;
    record.payload_size = sizeof(record);
    return record;
}

static bool build_test_menu_scene(app_ui_scene* scene, cptr title, cptr body,
    int row_id, cptr row_label)
{
    app_ui_panel* panel;

    if (!scene || !title || !body || !row_label)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_title(panel, TERM_WHITE, title);
    return app_ui_panel_add_body_line(panel, TERM_SLATE, body)
        && app_ui_panel_add_row(panel, (s16b)row_id, TERM_WHITE, true, true,
            "", row_label, "");
}

static bool publish_menu_scene_to_session(app_session* session,
    const app_ui_scene* scene)
{
    if (!session || !scene)
        return false;

    return app_session_publish_menu_scene(session, scene);
}

static void test_record_round_trip(void)
{
    app_event_record src;
    app_event_record dst;
    unsigned char bytes[sizeof(app_event_record)];

    src = make_event(APP_EVENT_KIND_MESSAGE, APP_EVENT_SCOPE_SESSION, 42,
        -7, 11, 22, 33);
    src.flags = APP_EVENT_FLAG_IMPORTANT | APP_EVENT_FLAG_TRANSIENT;
    src.timestamp_usec = 123456789u;

    memcpy(bytes, &src, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    memcpy(&dst, bytes, sizeof(dst));

    CHECK(memcmp(&src, &dst, sizeof(src)) == 0);
    CHECK(dst.kind == APP_EVENT_KIND_MESSAGE);
    CHECK(dst.scope == APP_EVENT_SCOPE_SESSION);
    CHECK(dst.flags == (APP_EVENT_FLAG_IMPORTANT | APP_EVENT_FLAG_TRANSIENT));
    CHECK(dst.sequence == 42);
    CHECK(dst.subject == -7);
    CHECK(dst.arg0 == 11);
    CHECK(dst.arg1 == 22);
    CHECK(dst.arg2 == 33);
}

static void test_buffer_growth_and_view(void)
{
    app_event_buffer* buffer;
    app_event_record record;
    app_event_span span;
    size_t capacity_before;

    buffer = app_event_buffer_create(1);
    CHECK(buffer != NULL);
    if (!buffer)
        return;

    capacity_before = app_event_buffer_capacity(buffer);
    CHECK(capacity_before >= 1);
    CHECK(app_event_buffer_count(buffer) == 0);
    CHECK(app_event_buffer_dropped_count(buffer) == 0);

    record = make_event(APP_EVENT_KIND_SESSION_LIFECYCLE,
        APP_EVENT_SCOPE_GLOBAL, 0, 1, 2, 3, 4);
    CHECK(app_event_buffer_push(buffer, &record));
    CHECK(app_event_buffer_count(buffer) == 1);
    CHECK(app_event_buffer_capacity(buffer) >= capacity_before);

    CHECK(app_event_buffer_reserve(buffer, 8));
    CHECK(app_event_buffer_capacity(buffer) >= 8);
    span = app_event_buffer_view(buffer);
    CHECK(span.count == 1);
    CHECK(span.records[0].kind == APP_EVENT_KIND_SESSION_LIFECYCLE);
    CHECK(span.records[0].sequence == 1);

    record = make_event(APP_EVENT_KIND_WAIT_STATE, APP_EVENT_SCOPE_SCENE, 0,
        5, 6, 7, 8);
    CHECK(app_event_buffer_push(buffer, &record));
    CHECK(app_event_buffer_count(buffer) == 2);
    CHECK(app_event_buffer_capacity(buffer) >= 2);

    span = app_event_buffer_view(buffer);
    CHECK(span.records != NULL);
    CHECK(span.count == 2);
    CHECK(span.records[0].kind == APP_EVENT_KIND_SESSION_LIFECYCLE);
    CHECK(span.records[1].kind == APP_EVENT_KIND_WAIT_STATE);
    CHECK(span.records[0].sequence == 1);
    CHECK(span.records[1].sequence == 2);

    app_event_buffer_destroy(buffer);
}

static void test_sequence_assignment(void)
{
    app_event_buffer* buffer;
    app_event_record record;
    app_event_span span;

    buffer = app_event_buffer_create(2);
    CHECK(buffer != NULL);
    if (!buffer)
        return;

    record = make_event(APP_EVENT_KIND_MESSAGE, APP_EVENT_SCOPE_VIEW, 0,
        10, 20, 30, 40);
    CHECK(app_event_buffer_push(buffer, &record));

    record = make_event(APP_EVENT_KIND_AUDIO_CUE, APP_EVENT_SCOPE_AUDIO, 17,
        50, 60, 70, 80);
    CHECK(app_event_buffer_push(buffer, &record));

    record = make_event(APP_EVENT_KIND_CUSTOM, APP_EVENT_SCOPE_ENTITY, 0,
        90, 91, 92, 93);
    CHECK(app_event_buffer_push(buffer, &record));

    span = app_event_buffer_view(buffer);
    CHECK(span.count == 3);
    CHECK(span.records[0].sequence == 1);
    CHECK(span.records[1].sequence == 17);
    CHECK(span.records[2].sequence == 18);

    app_event_buffer_destroy(buffer);
}

static void test_drain_semantics(void)
{
    app_event_buffer* buffer;
    app_event_record record;
    app_event_span before;
    app_event_span drained;

    buffer = app_event_buffer_create(1);
    CHECK(buffer != NULL);
    if (!buffer)
        return;

    record = make_event(APP_EVENT_KIND_RESOURCE_INVALIDATED,
        APP_EVENT_SCOPE_SESSION, 0, 101, 102, 103, 104);
    CHECK(app_event_buffer_push(buffer, &record));
    CHECK(app_event_buffer_count(buffer) == 1);

    before = app_event_buffer_view(buffer);
    CHECK(before.count == 1);

    drained = app_event_buffer_drain(buffer);
    CHECK(drained.count == 1);
    CHECK(app_event_buffer_count(buffer) == 0);
    CHECK(app_event_buffer_view(buffer).count == 0);
    CHECK(app_event_buffer_dropped_count(buffer) == 0);
    CHECK(drained.records == before.records);
    CHECK(drained.records[0].kind == APP_EVENT_KIND_RESOURCE_INVALIDATED);
    CHECK(drained.records[0].sequence == 1);

    app_event_buffer_destroy(buffer);
}

typedef struct test_host_state {
    u32b capabilities;
    u64b monotonic_usec;
    u64b wall_usec;
    int load_calls;
    int store_calls;
    int release_calls;
    int log_calls;
} test_host_state;

static u32b test_host_query_capabilities(void* user_data)
{
    test_host_state* state = user_data;

    return state->capabilities;
}

static u64b test_host_monotonic_usec(void* user_data)
{
    test_host_state* state = user_data;

    return state->monotonic_usec;
}

static u64b test_host_wall_usec(void* user_data)
{
    test_host_state* state = user_data;

    return state->wall_usec;
}

static bool test_host_resolve_path(void* user_data,
    const app_host_path_request* request)
{
    test_host_state* state = user_data;

    (void)state;

    if (!request || !request->buffer || request->buffer_size < 4)
        return false;

    memcpy(request->buffer, "cfg", 4);
    return true;
}

static bool test_host_load_blob(void* user_data, u16b kind, const char* slot,
    app_host_blob* out_blob)
{
    static const byte blob_data[] = { 0x11, 0x22, 0x33 };
    test_host_state* state = user_data;

    CHECK(kind == APP_HOST_PATH_RESOURCE);
    CHECK(strcmp(slot, "blob") == 0);
    CHECK(out_blob != NULL);

    state->load_calls++;
    out_blob->data = blob_data;
    out_blob->size = sizeof(blob_data);
    out_blob->version = 7;
    out_blob->flags = 9;
    return true;
}

static bool test_host_store_blob(void* user_data, u16b kind, const char* slot,
    const app_host_blob* blob)
{
    test_host_state* state = user_data;

    CHECK(kind == APP_HOST_PATH_SAVE);
    CHECK(strcmp(slot, "save") == 0);
    CHECK(blob != NULL);
    CHECK(blob->data != NULL);
    CHECK(blob->size == 2);
    CHECK(blob->version == 3);
    CHECK(blob->flags == 4);

    state->store_calls++;
    return true;
}

static void test_host_release_blob(void* user_data, app_host_blob* blob)
{
    test_host_state* state = user_data;

    CHECK(blob != NULL);
    state->release_calls++;
}

static void test_host_log_message(void* user_data, u16b level,
    const char* subsystem, const char* message)
{
    test_host_state* state = user_data;

    CHECK(level == APP_HOST_LOG_WARN);
    CHECK(strcmp(subsystem, "ui1") == 0);
    CHECK(strcmp(message, "hello") == 0);

    state->log_calls++;
}

static void test_host_wrappers(void)
{
    static const app_host_vtable vtable = {
        test_host_query_capabilities,
        test_host_monotonic_usec,
        test_host_wall_usec,
        test_host_resolve_path,
        test_host_load_blob,
        test_host_store_blob,
        test_host_release_blob,
        test_host_log_message
    };
    test_host_state state;
    app_host host;
    app_host_blob blob;
    char path[8];
    byte save_data[2] = { 0xAA, 0x55 };

    memset(&state, 0, sizeof(state));
    state.capabilities = APP_HOST_CAPABILITY_MONOTONIC_CLOCK
        | APP_HOST_CAPABILITY_RESOURCE_LOOKUP
        | APP_HOST_CAPABILITY_LOGGING;
    state.monotonic_usec = 111;
    state.wall_usec = 222;

    host.vtable = &vtable;
    host.user_data = &state;

    CHECK(app_host_query_capabilities(NULL) == 0);
    CHECK(!app_host_has_capability(NULL, APP_HOST_CAPABILITY_LOGGING));
    CHECK(app_host_query_capabilities(&host) == state.capabilities);
    CHECK(app_host_has_capability(&host, APP_HOST_CAPABILITY_LOGGING));
    CHECK(app_host_monotonic_usec(&host) == 111);
    CHECK(app_host_wall_usec(&host) == 222);
    CHECK(app_host_resolve_path(&host, APP_HOST_PATH_CONFIG, "cfg", path,
        sizeof(path)));
    CHECK(strcmp(path, "cfg") == 0);
    CHECK(app_host_load_blob(&host, APP_HOST_PATH_RESOURCE, "blob", &blob));
    CHECK(blob.size == 3);
    CHECK(blob.version == 7);
    CHECK(blob.flags == 9);
    CHECK(app_host_store_blob(&host, APP_HOST_PATH_SAVE, "save", save_data,
        sizeof(save_data), 3, 4));
    app_host_log(&host, APP_HOST_LOG_WARN, "ui1", "hello");
    app_host_release_blob(&host, &blob);
    CHECK(blob.data == NULL);
    CHECK(blob.size == 0);
    CHECK(state.load_calls == 1);
    CHECK(state.store_calls == 1);
    CHECK(state.release_calls == 1);
    CHECK(state.log_calls == 1);
}

typedef struct test_advance_state {
    int calls;
    int wait_after;
    bool idle_without_wait;
} test_advance_state;

static bool test_session_advance_callback(app_session* session,
    void* user_data)
{
    test_advance_state* state = user_data;

    CHECK(session != NULL);
    CHECK(state != NULL);
    if (!session || !state)
        return false;

    if (state->idle_without_wait)
        return false;

    state->calls++;
    if (state->wait_after > 0 && state->calls >= state->wait_after)
    {
        app_session_begin_wait(session, APP_WAIT_REASON_COMMAND_INPUT,
            state->calls, 0);
    }

    return true;
}

static void test_session_scaffolding(void)
{
    app_session_config config;
    app_session* session;
    app_wait_state wait_state;
    app_wait_scope wait_scope;
    app_input_capture_scope input_capture_scope;
    app_input_capture_scope nested_input_capture_scope;
    app_input input;
    app_input popped_input;
    app_intent intent;
    app_intent popped_intent;
    app_event_record event;
    app_snapshot snapshot;
    app_snapshot_blob blob;
    app_ui_scene menu_scene;
    const app_menu_snapshot* menu_snapshot;
    const app_interaction_state* interaction;
    app_event_span drained;
    const app_session_counters* counters;
    u64b emitted_baseline;
    static const byte snapshot_bytes[] = { 1, 2, 3, 4 };

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.initial_event_capacity = 1;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

    session = app_session_create(NULL);
    CHECK(session == NULL);
    config.api_version = APP_SESSION_API_VERSION + 1;
    session = app_session_create(&config);
    CHECK(session == NULL);
    config.api_version = APP_SESSION_API_VERSION;
    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    CHECK(app_session_current() == NULL);
    app_session_make_current(session);
    CHECK(app_session_current() == session);

    CHECK(app_session_host(session) == NULL);
    CHECK(app_session_flags(session) == config.flags);
    CHECK(app_session_has_flag(session, APP_SESSION_FLAG_ALLOW_LEGACY_INPUT));
    CHECK(app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT));
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_IDLE);
    CHECK(app_session_wait_state(session)->reason == APP_WAIT_REASON_NONE);
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_NONE);
    CHECK(app_session_pending_input_count(session) == 0);
    CHECK(app_session_pending_intent_count(session) == 0);
    CHECK(!app_session_interactions_enabled(session));

    emitted_baseline = app_session_get_counters(session)->emitted_events;

    memset(&wait_state, 0, sizeof(wait_state));
    wait_state.reason = APP_WAIT_REASON_COMMAND_INPUT;
    wait_state.detail0 = 17;
    app_session_set_wait_state(session, &wait_state);
    CHECK(app_session_wait_state(session)->reason
        == APP_WAIT_REASON_COMMAND_INPUT);
    CHECK(app_session_wait_state(session)->detail0 == 17);
    CHECK(app_session_view_events(session).count == 2);

    app_session_push_wait_scope(session, &wait_scope, APP_WAIT_REASON_TARGETING,
        3, 4);
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_WAITING);
    CHECK(app_session_wait_state(session)->reason == APP_WAIT_REASON_TARGETING);
    CHECK(app_session_wait_state(session)->detail0 == 3);
    CHECK(app_session_wait_state(session)->detail1 == 4);
    app_session_pop_wait_scope(session, &wait_scope);
    CHECK(app_session_wait_state(session)->reason
        == APP_WAIT_REASON_COMMAND_INPUT);
    CHECK(app_session_wait_state(session)->detail0 == 17);

    CHECK(!app_session_input_capture_active(session));
    app_session_push_input_capture(session, &input_capture_scope);
    CHECK(app_session_input_capture_active(session));
    app_session_push_input_capture(session, &nested_input_capture_scope);
    CHECK(app_session_input_capture_active(session));
    app_session_pop_input_capture(session, &nested_input_capture_scope);
    CHECK(app_session_input_capture_active(session));
    app_session_pop_input_capture(session, &input_capture_scope);
    CHECK(!app_session_input_capture_active(session));

    app_session_resume_running(session);
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_RUNNING);
    CHECK(app_session_wait_state(session)->reason == APP_WAIT_REASON_NONE);

    memset(&blob, 0, sizeof(blob));
    blob.kind = APP_SNAPSHOT_BLOB_HEADER;
    blob.format_version = 1;
    blob.data = snapshot_bytes;
    blob.size = sizeof(snapshot_bytes);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 9;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.blobs = &blob;
    snapshot.blob_count = 1;
    app_session_set_snapshot(session, &snapshot);
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_DUNGEON);
    CHECK(app_session_snapshot(session)->blob_count == 1);
    CHECK(app_session_interactions_enabled(session));

    CHECK(build_test_menu_scene(&menu_scene, "Info Header",
        "Some informational text.", 17, "Open details"));
    app_session_clear_menu_snapshot(session);
    CHECK(publish_menu_scene_to_session(session, &menu_scene));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_MENU);
    menu_snapshot = app_session_menu_snapshot(session);
    CHECK(menu_snapshot != NULL);
    CHECK(menu_snapshot->scene.panel_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].title, "Info Header"));
    CHECK(menu_snapshot->scene.panels[0].body_line_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].body_lines[0].text,
        "Some informational text."));
    CHECK(menu_snapshot->scene.panels[0].row_count == 1);
    CHECK(menu_snapshot->scene.panels[0].rows[0].id == 17);
    CHECK(streq(menu_snapshot->scene.panels[0].rows[0].label, "Open details"));
    CHECK(!app_session_interactions_enabled(session));

    app_session_set_snapshot(session, &snapshot);
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_DUNGEON);
    CHECK(app_session_interactions_enabled(session));

    app_session_note_cursor_relative(session, 7, 9);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.visible == 1);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.relative == 1);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.map_y == 7);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.map_x == 9);
    app_session_set_cursor_visible(session, false);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.visible == 0);
    app_session_set_cursor_visible(session, true);
    CHECK(app_session_dungeon_snapshot(session)->cursor_state.visible == 1);

    app_session_begin_interaction(session, APP_INTERACTION_KIND_LIST,
        APP_WAIT_REASON_LIST_SELECTION,
        APP_INTERACTION_FLAG_CAN_CONFIRM | APP_INTERACTION_FLAG_CAN_CANCEL
            | APP_INTERACTION_FLAG_SHOW_OPTIONS);
    app_session_set_interaction_prompt(session, TERM_WHITE, "Choose an item");
    app_session_set_interaction_detail(session, TERM_SLATE,
        "Press Enter to confirm.");
    app_session_set_interaction_value(session, TERM_YELLOW, "12", 2);
    CHECK(app_session_add_interaction_option(session, TERM_WHITE, 'a', true,
        true, "Arrow", "1.0 lb"));
    CHECK(app_session_add_interaction_option(session, TERM_WHITE, 'b', false,
        false, "Bow", ""));
    app_session_set_interaction_selected(session, 0);

    interaction = app_session_interaction(session);
    CHECK(interaction != NULL);
    CHECK(interaction->kind == APP_INTERACTION_KIND_LIST);
    CHECK(interaction->reason == APP_WAIT_REASON_LIST_SELECTION);
    CHECK(interaction->option_count == 2);
    CHECK(interaction->selected_index == 0);
    CHECK(interaction->cursor_index == 2);
    CHECK(streq(interaction->prompt, "Choose an item"));
    CHECK(streq(interaction->detail, "Press Enter to confirm."));
    CHECK(streq(interaction->value, "12"));
    CHECK(interaction->options[0].selected == 1);
    CHECK(interaction->options[1].enabled == 0);

    app_session_clear_interaction(session);
    interaction = app_session_interaction(session);
    CHECK(interaction->kind == APP_INTERACTION_KIND_NONE);
    CHECK(interaction->option_count == 0);
    CHECK(interaction->selected_index == -1);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.payload.key.logical_key = 'x';
    CHECK(app_session_submit_input(session, &input));
    CHECK(!app_session_submit_input(session, NULL));
    CHECK(app_session_pending_input_count(session) == 1);
    CHECK(app_session_peek_input(session, &popped_input));
    CHECK(popped_input.payload.key.logical_key == 'x');
    CHECK(app_session_pop_input(session, &popped_input));
    CHECK(popped_input.payload.key.logical_key == 'x');
    CHECK(app_session_pending_input_count(session) == 0);
    CHECK(!app_session_pop_input(session, NULL));

    memset(&intent, 0, sizeof(intent));
    intent.kind = APP_INTENT_KIND_CONFIRM;
    CHECK(app_session_submit_intent(session, &intent));
    CHECK(!app_session_submit_intent(session, NULL));
    CHECK(app_session_pending_intent_count(session) == 1);
    CHECK(app_session_peek_intent(session, &popped_intent));
    CHECK(popped_intent.kind == APP_INTENT_KIND_CONFIRM);
    CHECK(app_session_pop_intent(session, &popped_intent));
    CHECK(popped_intent.kind == APP_INTENT_KIND_CONFIRM);
    CHECK(app_session_pending_intent_count(session) == 0);

    event = make_event(APP_EVENT_KIND_WAIT_STATE, APP_EVENT_SCOPE_SESSION, 0,
        7, 8, 9, 10);
    CHECK(app_session_emit_event(session, &event));
    CHECK(app_session_view_events(session).count >= 1);

    drained = app_session_drain_events(session);
    CHECK(drained.count >= 1);
    CHECK(app_session_view_events(session).count == 0);

    counters = app_session_get_counters(session);
    CHECK(counters != NULL);
    CHECK(counters->submitted_inputs == 1);
    CHECK(counters->submitted_intents == 1);
    CHECK(counters->consumed_inputs == 1);
    CHECK(counters->consumed_intents == 1);
    CHECK(counters->emitted_events >= emitted_baseline + 1);
    CHECK(counters->dropped_events == 0);

    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
}

static void test_public_boundary_wrappers(void)
{
    static const byte snapshot_bytes[] = { 7, 6, 5, 4 };
    app_session_config config;
    app_session* session;
    app_input input;
    app_intent intent;
    app_input popped_input;
    app_intent popped_intent;
    app_snapshot_blob blob;
    app_snapshot snapshot;
    app_event_record event;
    app_event_span span;
    test_advance_state advance_state;

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    CHECK(!app_session_can_advance(NULL));
    CHECK(!app_session_can_advance(session));
    CHECK(app_get_snapshot(session)->scene == APP_SCENE_KIND_NONE);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'p';
    CHECK(app_submit_input(session, &input));
    CHECK(app_session_pop_input(session, &popped_input));
    CHECK(popped_input.payload.key.logical_key == 'p');

    memset(&intent, 0, sizeof(intent));
    intent.kind = APP_INTENT_KIND_CONFIRM;
    CHECK(app_submit_intent(session, &intent));
    CHECK(app_session_pop_intent(session, &popped_intent));
    CHECK(popped_intent.kind == APP_INTENT_KIND_CONFIRM);

    memset(&blob, 0, sizeof(blob));
    blob.kind = APP_SNAPSHOT_BLOB_HEADER;
    blob.format_version = 1;
    blob.data = snapshot_bytes;
    blob.size = sizeof(snapshot_bytes);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 33;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.blobs = &blob;
    snapshot.blob_count = 1;
    app_session_set_snapshot(session, &snapshot);
    CHECK(app_get_snapshot(session)->scene == APP_SCENE_KIND_DUNGEON);
    CHECK(app_get_snapshot(session)->revision == 33);

    app_session_clear_events(session);
    event = make_event(APP_EVENT_KIND_DAMAGE, APP_EVENT_SCOPE_SCENE, 0,
        1, 2, 3, 4);
    CHECK(app_session_emit_event(session, &event));
    span = app_view_events(session);
    CHECK(span.count == 1);
    CHECK(span.records[0].kind == APP_EVENT_KIND_DAMAGE);
    span = app_drain_events(session);
    CHECK(span.count == 1);
    CHECK(app_view_events(session).count == 0);

    memset(&advance_state, 0, sizeof(advance_state));
    advance_state.wait_after = 3;
    app_session_set_advance_callback(session, test_session_advance_callback,
        &advance_state);
    CHECK(app_session_can_advance(session));
    CHECK(app_advance_until_waiting(session) == APP_WAIT_REASON_COMMAND_INPUT);
    CHECK(advance_state.calls == 3);
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_WAITING);
    CHECK(app_session_wait_state(session)->detail0 == 3);

    app_session_resume_running(session);
    memset(&advance_state, 0, sizeof(advance_state));
    advance_state.idle_without_wait = true;
    app_session_set_advance_callback(session, test_session_advance_callback,
        &advance_state);
    CHECK(app_advance_until_waiting(session) == APP_WAIT_REASON_NONE);
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_IDLE);

    app_session_set_advance_callback(session, NULL, NULL);
    CHECK(!app_session_can_advance(session));
    CHECK(app_advance_until_waiting(session) == APP_WAIT_REASON_NONE);

    app_session_destroy(session);
}

static void test_information_scene_nested_restore(void)
{
    app_session_config config;
    app_session* session;
    app_snapshot snapshot;
    app_snapshot_blob blob;
    app_ui_scene outer_scene;
    app_ui_scene inner_scene;
    ui_information_scene_scope outer_scope;
    ui_information_scene_scope inner_scope;
    const app_menu_snapshot* menu_snapshot;
    bool refresh_enabled;
    static const byte snapshot_bytes[] = { 5, 4, 3, 2 };
    bool outer_entered = false;
    bool inner_entered = false;

    refresh_enabled = ui_information_scene_set_refresh_enabled(false);

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.initial_event_capacity = 1;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
    {
        ui_information_scene_set_refresh_enabled(refresh_enabled);
        return;
    }

    app_session_make_current(session);

    memset(&blob, 0, sizeof(blob));
    blob.kind = APP_SNAPSHOT_BLOB_HEADER;
    blob.format_version = 1;
    blob.data = snapshot_bytes;
    blob.size = sizeof(snapshot_bytes);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 3;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.blobs = &blob;
    snapshot.blob_count = 1;
    app_session_set_snapshot(session, &snapshot);

    CHECK(build_test_menu_scene(&outer_scene, "Outer title", "Outer body", 7,
        "Outer action"));
    CHECK(build_test_menu_scene(&inner_scene, "Inner title", "Inner body", 11,
        "Inner action"));

    outer_entered = ui_information_scene_enter(&outer_scope);
    CHECK(outer_entered);
    if (!outer_entered)
        goto cleanup;
    CHECK(app_session_input_capture_active(session));

    CHECK(ui_information_scene_present_ui(&outer_scene));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_MENU);
    menu_snapshot = app_session_menu_snapshot(session);
    CHECK(menu_snapshot != NULL);
    CHECK(menu_snapshot->scene.panel_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].title, "Outer title"));
    CHECK(menu_snapshot->scene.panels[0].body_line_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].body_lines[0].text,
        "Outer body"));
    CHECK(menu_snapshot->scene.panels[0].row_count == 1);
    CHECK(menu_snapshot->scene.panels[0].rows[0].id == 7);
    CHECK(streq(menu_snapshot->scene.panels[0].rows[0].label,
        "Outer action"));

    inner_entered = ui_information_scene_enter(&inner_scope);
    CHECK(inner_entered);
    if (!inner_entered)
        goto cleanup;
    CHECK(app_session_input_capture_active(session));

    CHECK(ui_information_scene_present_ui(&inner_scene));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_MENU);
    menu_snapshot = app_session_menu_snapshot(session);
    CHECK(menu_snapshot != NULL);
    CHECK(menu_snapshot->scene.panel_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].title, "Inner title"));
    CHECK(menu_snapshot->scene.panels[0].body_line_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].body_lines[0].text,
        "Inner body"));
    CHECK(menu_snapshot->scene.panels[0].row_count == 1);
    CHECK(menu_snapshot->scene.panels[0].rows[0].id == 11);
    CHECK(streq(menu_snapshot->scene.panels[0].rows[0].label,
        "Inner action"));

    ui_information_scene_leave(&inner_scope);
    inner_entered = false;
    CHECK(app_session_input_capture_active(session));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_MENU);
    menu_snapshot = app_session_menu_snapshot(session);
    CHECK(menu_snapshot != NULL);
    CHECK(menu_snapshot->scene.panel_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].title, "Outer title"));
    CHECK(menu_snapshot->scene.panels[0].body_line_count == 1);
    CHECK(streq(menu_snapshot->scene.panels[0].body_lines[0].text,
        "Outer body"));
    CHECK(menu_snapshot->scene.panels[0].row_count == 1);
    CHECK(menu_snapshot->scene.panels[0].rows[0].id == 7);
    CHECK(streq(menu_snapshot->scene.panels[0].rows[0].label,
        "Outer action"));

    ui_information_scene_leave(&outer_scope);
    outer_entered = false;
    CHECK(!app_session_input_capture_active(session));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_DUNGEON);

cleanup:
    if (inner_entered)
        ui_information_scene_leave(&inner_scope);
    if (outer_entered)
        ui_information_scene_leave(&outer_scope);
    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
    ui_information_scene_set_refresh_enabled(refresh_enabled);
}

static void test_information_scene_wait_key_nonrepeat(void)
{
    app_session_config config;
    app_session* session;
    ui_information_scene_scope scope;
    app_input input;
    bool entered = false;
    bool refresh_enabled;

    refresh_enabled = ui_information_scene_set_refresh_enabled(false);

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.initial_event_capacity = 1;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
    {
        ui_information_scene_set_refresh_enabled(refresh_enabled);
        return;
    }

    app_session_make_current(session);
    entered = ui_information_scene_enter(&scope);
    CHECK(entered);
    if (!entered)
        goto cleanup;
    CHECK(app_session_input_capture_active(session));

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS | APP_INPUT_FLAG_REPEAT;
    input.payload.key.logical_key = 'm';
    CHECK(app_session_submit_input(session, &input));

    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'x';
    CHECK(app_session_submit_input(session, &input));

    CHECK(ui_information_scene_wait_key_nonrepeat() == 'x');

cleanup:
    if (entered)
        ui_information_scene_leave(&scope);
    CHECK(!app_session_input_capture_active(session));
    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
    ui_information_scene_set_refresh_enabled(refresh_enabled);
}

static void test_information_scene_wait_key_with_wait_reason(void)
{
    app_session_config config;
    app_session* session;
    app_input input;
    bool refresh_enabled;

    refresh_enabled = ui_information_scene_set_refresh_enabled(false);

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.initial_event_capacity = 1;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
    {
        ui_information_scene_set_refresh_enabled(refresh_enabled);
        return;
    }

    app_session_make_current(session);
    app_session_set_state(session, APP_SESSION_STATE_RUNNING);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'k';
    CHECK(app_session_submit_input(session, &input));

    CHECK(ui_information_scene_wait_key_with_wait_reason(
        APP_WAIT_REASON_LIST_SELECTION) == 'k');
    CHECK(app_session_wait_state(session)->reason == APP_WAIT_REASON_NONE);
    CHECK(app_session_state_id(session) == APP_SESSION_STATE_RUNNING);
    CHECK(app_session_pending_input_count(session) == 0);

    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
    ui_information_scene_set_refresh_enabled(refresh_enabled);
}

static void test_ui_scene_direct_panel_payload(void)
{
    app_ui_scene ui_scene;
    app_ui_panel* panel;

    app_ui_scene_init(&ui_scene);
    ui_scene.flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;

    panel = app_ui_scene_append_panel(&ui_scene, APP_UI_LAYER_MODAL);
    CHECK(panel != NULL);
    if (!panel)
        return;

    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS
        | APP_UI_PANEL_FLAG_SHOW_DETAIL;
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Inventory");
    app_ui_panel_set_subtitle(panel, TERM_WHITE, "Choose one item");
    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Details");
    app_ui_panel_set_widths(panel, 320, 760);
    app_ui_panel_set_row_offset(panel, 2);
    CHECK(app_ui_panel_add_body_line_ex(panel, TERM_WHITE,
        STORY_FLAG_USE, "Body line"));
    CHECK(app_ui_panel_add_row_ex(panel, 7, TERM_WHITE, TERM_SLATE,
        TERM_L_RED, '!', true, true, "a", "Potion", "x2"));
    CHECK(app_ui_panel_add_detail_line(panel, TERM_L_WHITE, "Detail line"));
    CHECK(app_ui_panel_add_footer_action(panel, 11, TERM_L_BLUE, true,
        "Enter", "Choose"));
    CHECK(app_ui_panel_add_tab(panel, 3, TERM_WHITE, true, "Equipment"));

    CHECK(ui_scene.format_version == APP_UI_FORMAT_VERSION);
    CHECK(ui_scene.flags == APP_UI_SCENE_FLAG_DIM_BACKDROP);
    CHECK(ui_scene.panel_count == 1);

    CHECK(panel->layer == APP_UI_LAYER_MODAL);
    CHECK((panel->flags & APP_UI_PANEL_FLAG_ACTIVE) != 0);
    CHECK((panel->flags & APP_UI_PANEL_FLAG_TOP_ANCHORED) != 0);
    CHECK((panel->flags & APP_UI_PANEL_FLAG_SCROLL_ROWS) != 0);
    CHECK((panel->flags & APP_UI_PANEL_FLAG_SHOW_DETAIL) != 0);
    CHECK(panel->style == APP_UI_PANEL_STYLE_DEFAULT);
    CHECK(panel->focus_area == APP_UI_FOCUS_TABS);
    CHECK(panel->focus_id == 3);
    CHECK(panel->selected_row == 0);
    CHECK(panel->row_offset == 2);
    CHECK(panel->min_width_px == 320);
    CHECK(panel->width_cap_px == 760);
    CHECK(panel->title_attr == TERM_L_BLUE);
    CHECK(panel->subtitle_attr == TERM_WHITE);
    CHECK(panel->detail_title_attr == TERM_L_BLUE);
    CHECK(streq(panel->title, "Inventory"));
    CHECK(streq(panel->subtitle, "Choose one item"));
    CHECK(streq(panel->detail_title, "Details"));
    CHECK(panel->body_line_count == 1);
    CHECK(panel->body_lines[0].attr == TERM_WHITE);
    CHECK(panel->body_lines[0].story == STORY_FLAG_USE);
    CHECK(streq(panel->body_lines[0].text, "Body line"));
    CHECK(panel->row_count == 1);
    CHECK(panel->rows[0].id == 7);
    CHECK(panel->rows[0].attr == TERM_WHITE);
    CHECK(panel->rows[0].meta_attr == TERM_SLATE);
    CHECK(panel->rows[0].icon_attr == TERM_L_RED);
    CHECK(panel->rows[0].icon_char == '!');
    CHECK((panel->rows[0].flags & APP_UI_ITEM_FLAG_SELECTED) != 0);
    CHECK(streq(panel->rows[0].key, "a"));
    CHECK(streq(panel->rows[0].label, "Potion"));
    CHECK(streq(panel->rows[0].meta, "x2"));
    CHECK(panel->detail_line_count == 1);
    CHECK(streq(panel->detail_lines[0].text, "Detail line"));
    CHECK(panel->footer_action_count == 1);
    CHECK(streq(panel->footer_actions[0].key, "Enter"));
    CHECK(streq(panel->footer_actions[0].label, "Choose"));
    CHECK(panel->tab_count == 1);
    CHECK(streq(panel->tabs[0].label, "Equipment"));
    CHECK((panel->tabs[0].flags & APP_UI_ITEM_FLAG_ACTIVE) != 0);
}

static void test_movement_service(void)
{
    app_movement_direction_payload payload;
    u16b direction = APP_MOVEMENT_DIRECTION_NONE;
    app_movement_binding plain_move;
    app_movement_binding shift_run;
    app_movement_binding wait_binding;
    app_movement_binding prompt_move;
    app_movement_binding conflicting_plain_move;
    app_movement_binding wildcard_wait;
    app_movement_binding bindings[4];
    app_input input;
    app_movement_command command;

    CHECK(app_movement_action_is_directional(APP_MOVEMENT_ACTION_MOVE_DIR));
    CHECK(app_movement_action_is_directional(APP_MOVEMENT_ACTION_RUN_DIR));
    CHECK(!app_movement_action_is_directional(APP_MOVEMENT_ACTION_WAIT));

    CHECK(app_movement_direction_payload_from_direction(
        APP_MOVEMENT_DIRECTION_NORTHWEST, &payload));
    CHECK(payload.direction == APP_MOVEMENT_DIRECTION_NORTHWEST);
    CHECK(payload.dy == -1);
    CHECK(payload.dx == -1);
    CHECK(app_movement_direction_to_legacy_keypad(
        APP_MOVEMENT_DIRECTION_NORTHWEST) == 7);
    CHECK(app_movement_direction_from_legacy_keypad(3, &direction));
    CHECK(direction == APP_MOVEMENT_DIRECTION_SOUTHEAST);
    CHECK(!app_movement_direction_from_legacy_keypad(0, &direction));

    app_movement_binding_clear(&plain_move);
    plain_move.context = APP_MOVEMENT_CONTEXT_DUNGEON;
    plain_move.action = APP_MOVEMENT_ACTION_MOVE_DIR;
    plain_move.direction = APP_MOVEMENT_DIRECTION_NORTH;
    plain_move.device = APP_INPUT_DEVICE_KEYBOARD;
    plain_move.input_type = APP_INPUT_TYPE_KEY;
    plain_move.forbidden_modifiers = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT;
    plain_move.trigger = 0x52u;
    CHECK(app_movement_binding_is_valid(&plain_move));

    app_movement_binding_clear(&shift_run);
    shift_run.context = APP_MOVEMENT_CONTEXT_DUNGEON;
    shift_run.action = APP_MOVEMENT_ACTION_RUN_DIR;
    shift_run.direction = APP_MOVEMENT_DIRECTION_NORTH;
    shift_run.device = APP_INPUT_DEVICE_KEYBOARD;
    shift_run.input_type = APP_INPUT_TYPE_KEY;
    shift_run.required_modifiers = APP_INPUT_MODIFIER_SHIFT;
    shift_run.forbidden_modifiers = APP_INPUT_MODIFIER_CTRL
        | APP_INPUT_MODIFIER_ALT;
    shift_run.trigger = 0x52u;
    CHECK(app_movement_binding_is_valid(&shift_run));
    CHECK(!app_movement_bindings_conflict(&plain_move, &shift_run));

    conflicting_plain_move = plain_move;
    conflicting_plain_move.forbidden_modifiers = APP_INPUT_MODIFIER_CTRL
        | APP_INPUT_MODIFIER_ALT;
    CHECK(app_movement_bindings_conflict(&conflicting_plain_move, &shift_run));

    app_movement_binding_clear(&wait_binding);
    wait_binding.context = APP_MOVEMENT_CONTEXT_DUNGEON;
    wait_binding.action = APP_MOVEMENT_ACTION_WAIT;
    wait_binding.device = APP_INPUT_DEVICE_KEYBOARD;
    wait_binding.input_type = APP_INPUT_TYPE_KEY;
    wait_binding.forbidden_modifiers = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT;
    wait_binding.trigger = 0x35u;
    CHECK(app_movement_binding_is_valid(&wait_binding));

    app_movement_binding_clear(&prompt_move);
    prompt_move.context = APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT;
    prompt_move.action = APP_MOVEMENT_ACTION_MOVE_DIR;
    prompt_move.direction = APP_MOVEMENT_DIRECTION_WEST;
    prompt_move.device = APP_INPUT_DEVICE_KEYBOARD;
    prompt_move.input_type = APP_INPUT_TYPE_KEY;
    prompt_move.forbidden_modifiers = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT;
    prompt_move.trigger = 0x48u;
    CHECK(app_movement_binding_is_valid(&prompt_move));
    CHECK(!app_movement_bindings_conflict(&plain_move, &prompt_move));

    wildcard_wait = wait_binding;
    wildcard_wait.context = APP_MOVEMENT_CONTEXT_ANY;
    CHECK(app_movement_bindings_conflict(&wildcard_wait, &wait_binding));

    bindings[0] = plain_move;
    bindings[1] = shift_run;
    bindings[2] = prompt_move;
    bindings[3] = wildcard_wait;

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_INTENT;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.physical_key = 0x52u;
    input.payload.key.logical_key = 'W';
    CHECK(app_movement_binding_matches_input(&plain_move, &input,
        APP_MOVEMENT_CONTEXT_DUNGEON));
    CHECK(!app_movement_binding_matches_input(&plain_move, &input,
        APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT));

    input.modifiers = APP_INPUT_MODIFIER_SHIFT;
    input.flags = APP_INPUT_FLAG_PRESS | APP_INPUT_FLAG_REPEAT
        | APP_INPUT_FLAG_SYNTHETIC;
    input.source_id = 2u;
    input.sequence = 41u;
    input.timestamp_usec = 777u;
    CHECK(app_movement_resolve_input(bindings, N_ELEMENTS(bindings), &input,
        APP_MOVEMENT_CONTEXT_DUNGEON, &command));
    CHECK(app_movement_command_is_valid(&command));
    CHECK(command.context == APP_MOVEMENT_CONTEXT_DUNGEON);
    CHECK(command.action == APP_MOVEMENT_ACTION_RUN_DIR);
    CHECK(command.flags == (APP_MOVEMENT_COMMAND_FLAG_REPEAT
        | APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC));
    CHECK(command.modifiers == APP_INPUT_MODIFIER_SHIFT);
    CHECK(command.device == APP_INPUT_DEVICE_KEYBOARD);
    CHECK(command.input_type == APP_INPUT_TYPE_KEY);
    CHECK(command.source_id == 2u);
    CHECK(command.trigger == 0x52u);
    CHECK(command.trigger_aux == 'W');
    CHECK(command.sequence == 41u);
    CHECK(command.timestamp_usec == 777u);
    CHECK(command.direction.direction == APP_MOVEMENT_DIRECTION_NORTH);
    CHECK(command.direction.dy == -1);
    CHECK(command.direction.dx == 0);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_INTENT;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 0x48u;
    CHECK(app_movement_resolve_input(bindings, N_ELEMENTS(bindings), &input,
        APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT, &command));
    CHECK(command.action == APP_MOVEMENT_ACTION_MOVE_DIR);
    CHECK(command.direction.direction == APP_MOVEMENT_DIRECTION_WEST);
    CHECK(command.direction.dy == 0);
    CHECK(command.direction.dx == -1);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_INTENT;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 0x35u;
    CHECK(app_movement_resolve_input(bindings, N_ELEMENTS(bindings), &input,
        APP_MOVEMENT_CONTEXT_TARGETING, &command));
    CHECK(command.context == APP_MOVEMENT_CONTEXT_TARGETING);
    CHECK(command.action == APP_MOVEMENT_ACTION_WAIT);
    CHECK(command.direction.direction == APP_MOVEMENT_DIRECTION_CENTER);
    CHECK(command.direction.dy == 0);
    CHECK(command.direction.dx == 0);

    input.flags = APP_INPUT_FLAG_RELEASE;
    CHECK(!app_movement_resolve_input(bindings, N_ELEMENTS(bindings), &input,
        APP_MOVEMENT_CONTEXT_TARGETING, &command));
}

static bool test_find_movement_binding(const struct sdl_config* config,
    u16b action, u16b direction, u32b trigger, u16b required_modifiers)
{
    u16b i;

    if (!config)
        return false;

    for (i = 0; i < config->movement_binding_count; i++)
    {
        const app_movement_binding* binding = &config->movement_bindings[i];

        if (!app_movement_binding_is_valid(binding))
            continue;
        if (binding->action != action)
            continue;
        if (binding->direction != direction)
            continue;
        if (binding->trigger != trigger)
            continue;
        if (binding->required_modifiers != required_modifiers)
            continue;

        return true;
    }

    return false;
}

static void test_sdl_movement_presets(void)
{
    struct sdl_config config;
    u16b i;
    u16b j;

    memset(&config, 0, sizeof(config));
    sdl_config_set_defaults(&config);
    CHECK(!sdl_config_has_movement_bindings(&config));

    sdl_config_set_default_movement_bindings(&config,
        APP_MOVEMENT_PRESET_MODERN_ARROWS);
    CHECK(config.movement_keyboard_present);
    CHECK(config.movement_keyboard_preset == APP_MOVEMENT_PRESET_MODERN_ARROWS);
    CHECK(config.movement_binding_count > 0);
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_MOVE_DIR,
        APP_MOVEMENT_DIRECTION_NORTH, SDL_SCANCODE_UP, 0));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_RUN_DIR,
        APP_MOVEMENT_DIRECTION_NORTH, SDL_SCANCODE_UP,
        APP_INPUT_MODIFIER_SHIFT));
    CHECK(test_find_movement_binding(&config,
        APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_NORTH,
        SDL_SCANCODE_UP, APP_INPUT_MODIFIER_CTRL));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_WAIT,
        APP_MOVEMENT_DIRECTION_NONE, SDL_SCANCODE_PERIOD, 0));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_REST,
        APP_MOVEMENT_DIRECTION_NONE, SDL_SCANCODE_PERIOD,
        APP_INPUT_MODIFIER_SHIFT));

    sdl_config_set_default_movement_bindings(&config,
        APP_MOVEMENT_PRESET_CLASSIC_SIL);
    CHECK(config.movement_keyboard_present);
    CHECK(config.movement_keyboard_preset == APP_MOVEMENT_PRESET_CLASSIC_SIL);
    CHECK(config.movement_binding_count > 0);
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_MOVE_DIR,
        APP_MOVEMENT_DIRECTION_NORTH, SDL_SCANCODE_KP_8, 0));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_RUN_DIR,
        APP_MOVEMENT_DIRECTION_NORTH, SDL_SCANCODE_KP_8,
        APP_INPUT_MODIFIER_SHIFT));
    CHECK(test_find_movement_binding(&config,
        APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_NORTH,
        SDL_SCANCODE_KP_8, APP_INPUT_MODIFIER_CTRL));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_RUN_DIR,
        APP_MOVEMENT_DIRECTION_NORTH, SDL_SCANCODE_UP,
        APP_INPUT_MODIFIER_SHIFT));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_WAIT,
        APP_MOVEMENT_DIRECTION_NONE, SDL_SCANCODE_KP_5, 0));
    CHECK(test_find_movement_binding(&config, APP_MOVEMENT_ACTION_REST,
        APP_MOVEMENT_DIRECTION_NONE, SDL_SCANCODE_KP_5,
        APP_INPUT_MODIFIER_SHIFT));

    for (i = 0; i < config.movement_binding_count; i++)
    {
        CHECK(app_movement_binding_is_valid(&config.movement_bindings[i]));
        for (j = (u16b)(i + 1); j < config.movement_binding_count; j++)
        {
            CHECK(!app_movement_bindings_conflict(&config.movement_bindings[i],
                &config.movement_bindings[j]));
        }
    }
}

static void test_sdl_movement_config_round_trip(void)
{
    struct sdl_config saved;
    struct sdl_config loaded;
    struct pane_config pane_configs[1];
    int pane_count = 0;
    u16b i;
    const char* path = "ui1-movement-config-test.json";

    memset(&saved, 0, sizeof(saved));
    memset(&loaded, 0, sizeof(loaded));
    memset(pane_configs, 0, sizeof(pane_configs));

    sdl_config_set_defaults(&saved);
    sdl_config_set_default_movement_bindings(&saved,
        APP_MOVEMENT_PRESET_MODERN_WASD_QEZC);
    sdl_config_save(path, &saved, pane_configs, 0);

    sdl_config_set_defaults(&loaded);
    sdl_config_load(path, &loaded, pane_configs, &pane_count, 1);
    CHECK(loaded.movement_keyboard_present);
    CHECK(loaded.movement_keyboard_preset
        == APP_MOVEMENT_PRESET_MODERN_WASD_QEZC);
    CHECK(loaded.movement_binding_count == saved.movement_binding_count);
    for (i = 0; i < saved.movement_binding_count; i++)
    {
        CHECK(loaded.movement_bindings[i].context
            == saved.movement_bindings[i].context);
        CHECK(loaded.movement_bindings[i].action
            == saved.movement_bindings[i].action);
        CHECK(loaded.movement_bindings[i].direction
            == saved.movement_bindings[i].direction);
        CHECK(loaded.movement_bindings[i].trigger
            == saved.movement_bindings[i].trigger);
        CHECK(loaded.movement_bindings[i].required_modifiers
            == saved.movement_bindings[i].required_modifiers);
        CHECK(loaded.movement_bindings[i].forbidden_modifiers
            == saved.movement_bindings[i].forbidden_modifiers);
    }

    (void)remove(path);
}

static void test_movement_input_bridge(void)
{
    app_session_config config;
    app_session* session;
    app_movement_command dungeon_command;
    app_movement_command prompt_command;
    app_movement_command resolved;
    app_input input;
    char ch = '\0';

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.initial_event_capacity = 1;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    app_session_make_current(session);
    input_clear_movement_commands();

    app_movement_command_clear(&dungeon_command);
    dungeon_command.context = APP_MOVEMENT_CONTEXT_DUNGEON;
    dungeon_command.action = APP_MOVEMENT_ACTION_MOVE_DIR;
    dungeon_command.device = APP_INPUT_DEVICE_KEYBOARD;
    dungeon_command.input_type = APP_INPUT_TYPE_KEY;
    CHECK(app_movement_direction_payload_from_direction(
        APP_MOVEMENT_DIRECTION_NORTH, &dungeon_command.direction));
    CHECK(app_movement_command_is_valid(&dungeon_command));

    app_movement_command_clear(&prompt_command);
    prompt_command.context = APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT;
    prompt_command.action = APP_MOVEMENT_ACTION_INTERACT_DIR;
    prompt_command.device = APP_INPUT_DEVICE_KEYBOARD;
    prompt_command.input_type = APP_INPUT_TYPE_KEY;
    CHECK(app_movement_direction_payload_from_direction(
        APP_MOVEMENT_DIRECTION_WEST, &prompt_command.direction));
    CHECK(app_movement_command_is_valid(&prompt_command));

    CHECK(input_submit_movement_command(&dungeon_command));
    CHECK(input_submit_movement_command(&prompt_command));

    app_movement_command_clear(&resolved);
    CHECK(input_wait_for_movement_or_legacy(
        APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT, APP_WAIT_REASON_NONE, &resolved,
        &ch));
    CHECK(app_movement_command_is_valid(&resolved));
    CHECK(resolved.context == APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT);
    CHECK(resolved.action == APP_MOVEMENT_ACTION_INTERACT_DIR);
    CHECK(resolved.direction.direction == APP_MOVEMENT_DIRECTION_WEST);
    CHECK(ch == '\0');

    app_movement_command_clear(&resolved);
    CHECK(input_wait_for_movement_or_legacy(APP_MOVEMENT_CONTEXT_DUNGEON,
        APP_WAIT_REASON_NONE, &resolved, &ch));
    CHECK(app_movement_command_is_valid(&resolved));
    CHECK(resolved.context == APP_MOVEMENT_CONTEXT_DUNGEON);
    CHECK(resolved.action == APP_MOVEMENT_ACTION_MOVE_DIR);
    CHECK(resolved.direction.direction == APP_MOVEMENT_DIRECTION_NORTH);
    CHECK(ch == '\0');

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'm';
    CHECK(app_session_submit_input(session, &input));

    app_movement_command_clear(&resolved);
    ch = '\0';
    CHECK(input_wait_for_movement_or_legacy(APP_MOVEMENT_CONTEXT_DUNGEON,
        APP_WAIT_REASON_NONE, &resolved, &ch));
    CHECK(!app_movement_command_is_valid(&resolved));
    CHECK(ch == 'm');

    input_set_active_movement_command(&dungeon_command);
    CHECK(input_take_active_movement_command(&resolved));
    CHECK(resolved.context == APP_MOVEMENT_CONTEXT_DUNGEON);
    CHECK(!input_take_active_movement_command(&resolved));

    input_clear_movement_commands();
    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
}

int main(void)
{
    test_record_round_trip();
    test_buffer_growth_and_view();
    test_sequence_assignment();
    test_drain_semantics();
    test_host_wrappers();
    test_session_scaffolding();
    test_public_boundary_wrappers();
    test_information_scene_nested_restore();
    test_information_scene_wait_key_nonrepeat();
    test_information_scene_wait_key_with_wait_reason();
    test_ui_scene_direct_panel_payload();
    test_movement_service();
    test_sdl_movement_presets();
    test_sdl_movement_config_round_trip();
    test_movement_input_bridge();

    if (g_failures != 0)
    {
        fprintf(stderr, "ui1 tests failed: %d\n", g_failures);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
