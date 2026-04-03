#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "angband.h"
#include "app/app-events.h"
#include "app/app-host.h"
#include "app/app-scene-information.h"
#include "app/app-scene-menu.h"
#include "app/app-session.h"
#include "app/app-ui.h"
#include "runtime-cli.h"
#include "ui/ui-information-scene.h"

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

static bool publish_information_scene_to_session(app_session* session,
    const app_information_scene* scene)
{
    if (!session || !scene)
        return false;

    return app_session_publish_information_scene(session, scene);
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
    app_input input;
    app_input popped_input;
    app_intent intent;
    app_intent popped_intent;
    app_event_record event;
    app_snapshot snapshot;
    app_snapshot_blob blob;
    app_information_scene info_scene;
    const app_information_snapshot* info_snapshot;
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

    app_information_scene_init(&info_scene);
    CHECK(app_information_scene_add_text(&info_scene, 0, 1, TERM_WHITE,
        "Info Header"));
    CHECK(app_information_scene_add_cell_ex(&info_scene, 1, 4,
        (byte)(TILE_FLAG | 3), (char)(TILE_FLAG | 5),
        (byte)(TILE_FLAG | 1), (char)(TILE_FLAG | 2), 0, 2));
    CHECK(app_information_scene_add_cursor(&info_scene, 1, 4, TERM_L_BLUE, 2));
    CHECK(app_information_scene_add_text(&info_scene, 2, 0, TERM_SLATE,
        "Some informational text."));
    app_session_clear_information_snapshot(session);
    CHECK(app_session_add_information_op(session, 0, 1, TERM_WHITE,
        "Info Header"));
    CHECK(app_session_add_information_cell_ex(session, 1, 4,
        (byte)(TILE_FLAG | 3), (char)(TILE_FLAG | 5),
        (byte)(TILE_FLAG | 1), (char)(TILE_FLAG | 2), 0, 2));
    CHECK(app_session_add_information_cursor(session, 1, 4, TERM_L_BLUE, 2));
    CHECK(app_session_add_information_op(session, 2, 0, TERM_SLATE,
        "Some informational text."));
    CHECK(app_session_publish_information_snapshot(session));
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_INFORMATION);
    info_snapshot = app_session_information_snapshot(session);
    CHECK(info_snapshot != NULL);
    CHECK(info_snapshot->scene.op_count == 4);
    CHECK(info_snapshot->scene.ops[0].kind == APP_INFORMATION_OP_KIND_TEXT);
    CHECK(streq(info_snapshot->scene.ops[0].text, "Info Header"));
    CHECK(info_snapshot->scene.ops[1].kind == APP_INFORMATION_OP_KIND_CELL);
    CHECK(info_snapshot->scene.ops[1].attr == (byte)(TILE_FLAG | 3));
    CHECK((byte)info_snapshot->scene.ops[1].ch == (byte)(TILE_FLAG | 5));
    CHECK(info_snapshot->scene.ops[1].terrain_attr == (byte)(TILE_FLAG | 1));
    CHECK((byte)info_snapshot->scene.ops[1].terrain_char
        == (byte)(TILE_FLAG | 2));
    CHECK(info_snapshot->scene.ops[1].width == 2);
    CHECK(info_snapshot->scene.ops[2].kind == APP_INFORMATION_OP_KIND_CURSOR);
    CHECK(info_snapshot->scene.ops[2].attr == TERM_L_BLUE);
    CHECK(info_snapshot->scene.ops[2].width == 2);
    CHECK(info_snapshot->scene.ops[3].kind == APP_INFORMATION_OP_KIND_TEXT);
    CHECK(streq(info_snapshot->scene.ops[3].text,
        "Some informational text."));
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
    app_information_scene outer_scene;
    app_information_scene inner_scene;
    ui_information_scene_scope outer_scope;
    ui_information_scene_scope inner_scope;
    const app_information_snapshot* info_snapshot;
    bool snapshot_renderer_enabled;
    bool refresh_enabled;
    static const byte snapshot_bytes[] = { 5, 4, 3, 2 };
    bool outer_entered = false;
    bool inner_entered = false;

    snapshot_renderer_enabled = runtime_cli_snapshot_renderer();
    runtime_cli_set_snapshot_renderer(true);
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
        runtime_cli_set_snapshot_renderer(snapshot_renderer_enabled);
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

    app_information_scene_init(&outer_scene);
    CHECK(app_information_scene_add_text_ex(&outer_scene, 0, 0, TERM_WHITE,
        STORY_FLAG_USE | STORY_FLAG_CELL_ALIGN, "Outer title"));
    CHECK(app_information_scene_add_text(&outer_scene, 2, 3, TERM_SLATE,
        "Outer body"));
    CHECK(app_information_scene_add_cell_ex(&outer_scene, 4, 5,
        (byte)(TILE_FLAG | 6), (char)(TILE_FLAG | 7),
        (byte)(TILE_FLAG | 8), (char)(TILE_FLAG | 9), 0, 2));
    CHECK(app_information_scene_add_cursor(&outer_scene, 4, 5, TERM_L_BLUE, 2));

    app_information_scene_init(&inner_scene);
    CHECK(app_information_scene_add_text_ex(&inner_scene, 1, 1, TERM_L_RED,
        STORY_FLAG_USE, "Inner title"));
    CHECK(app_information_scene_add_text(&inner_scene, 3, 2, TERM_WHITE,
        "Inner body"));
    CHECK(app_information_scene_add_cell_ex(&inner_scene, 5, 6,
        (byte)(TILE_FLAG | 10), (char)(TILE_FLAG | 11),
        (byte)(TILE_FLAG | 12), (char)(TILE_FLAG | 13), 0, 2));
    CHECK(app_information_scene_add_cursor(&inner_scene, 5, 6, TERM_L_BLUE, 2));

    outer_entered = ui_information_scene_enter(&outer_scope);
    CHECK(outer_entered);
    if (!outer_entered)
        goto cleanup;

    CHECK(publish_information_scene_to_session(session, &outer_scene));
    info_snapshot = app_session_information_snapshot(session);
    CHECK(info_snapshot->scene.op_count == 4);
    CHECK(streq(info_snapshot->scene.ops[0].text, "Outer title"));
    CHECK(info_snapshot->scene.ops[0].story
        == (STORY_FLAG_USE | STORY_FLAG_CELL_ALIGN));
    CHECK(streq(info_snapshot->scene.ops[1].text, "Outer body"));
    CHECK(info_snapshot->scene.ops[1].story == 0);
    CHECK(info_snapshot->scene.ops[2].kind == APP_INFORMATION_OP_KIND_CELL);
    CHECK(info_snapshot->scene.ops[2].width == 2);
    CHECK((byte)info_snapshot->scene.ops[2].ch == (byte)(TILE_FLAG | 7));
    CHECK(info_snapshot->scene.ops[3].kind == APP_INFORMATION_OP_KIND_CURSOR);
    CHECK(info_snapshot->scene.ops[3].width == 2);

    inner_entered = ui_information_scene_enter(&inner_scope);
    CHECK(inner_entered);
    if (!inner_entered)
        goto cleanup;

    CHECK(publish_information_scene_to_session(session, &inner_scene));
    info_snapshot = app_session_information_snapshot(session);
    CHECK(info_snapshot->scene.op_count == 4);
    CHECK(streq(info_snapshot->scene.ops[0].text, "Inner title"));
    CHECK(info_snapshot->scene.ops[0].story == STORY_FLAG_USE);
    CHECK(streq(info_snapshot->scene.ops[1].text, "Inner body"));
    CHECK(info_snapshot->scene.ops[1].story == 0);
    CHECK(info_snapshot->scene.ops[2].kind == APP_INFORMATION_OP_KIND_CELL);
    CHECK(info_snapshot->scene.ops[2].width == 2);
    CHECK((byte)info_snapshot->scene.ops[2].ch == (byte)(TILE_FLAG | 11));
    CHECK(info_snapshot->scene.ops[3].kind == APP_INFORMATION_OP_KIND_CURSOR);
    CHECK(info_snapshot->scene.ops[3].width == 2);

    ui_information_scene_leave(&inner_scope);
    inner_entered = false;
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_INFORMATION);
    info_snapshot = app_session_information_snapshot(session);
    CHECK(info_snapshot->scene.op_count == 4);
    CHECK(streq(info_snapshot->scene.ops[0].text, "Outer title"));
    CHECK(info_snapshot->scene.ops[0].story
        == (STORY_FLAG_USE | STORY_FLAG_CELL_ALIGN));
    CHECK(streq(info_snapshot->scene.ops[1].text, "Outer body"));
    CHECK(info_snapshot->scene.ops[1].story == 0);
    CHECK(info_snapshot->scene.ops[2].kind == APP_INFORMATION_OP_KIND_CELL);
    CHECK(info_snapshot->scene.ops[2].width == 2);
    CHECK((byte)info_snapshot->scene.ops[2].ch == (byte)(TILE_FLAG | 7));
    CHECK(info_snapshot->scene.ops[3].kind == APP_INFORMATION_OP_KIND_CURSOR);
    CHECK(info_snapshot->scene.ops[3].width == 2);

    ui_information_scene_leave(&outer_scope);
    outer_entered = false;
    CHECK(app_session_snapshot(session)->scene == APP_SCENE_KIND_DUNGEON);

cleanup:
    if (inner_entered)
        ui_information_scene_leave(&inner_scope);
    if (outer_entered)
        ui_information_scene_leave(&outer_scope);
    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
    ui_information_scene_set_refresh_enabled(refresh_enabled);
    runtime_cli_set_snapshot_renderer(snapshot_renderer_enabled);
}

static void test_information_scene_wait_key_nonrepeat(void)
{
    app_session_config config;
    app_session* session;
    ui_information_scene_scope scope;
    app_input input;
    bool entered = false;
    bool snapshot_renderer_enabled;
    bool refresh_enabled;

    snapshot_renderer_enabled = runtime_cli_snapshot_renderer();
    runtime_cli_set_snapshot_renderer(true);
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
        runtime_cli_set_snapshot_renderer(snapshot_renderer_enabled);
        return;
    }

    app_session_make_current(session);
    entered = ui_information_scene_enter(&scope);
    CHECK(entered);
    if (!entered)
        goto cleanup;

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
    app_session_destroy(session);
    CHECK(app_session_current() == NULL);
    ui_information_scene_set_refresh_enabled(refresh_enabled);
    runtime_cli_set_snapshot_renderer(snapshot_renderer_enabled);
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
    test_ui_scene_direct_panel_payload();

    if (g_failures != 0)
    {
        fprintf(stderr, "ui1 tests failed: %d\n", g_failures);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
