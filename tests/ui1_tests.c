#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "angband.h"
#include "app/app-events.h"
#include "app/app-host.h"
#include "app/app-session.h"

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

int main(void)
{
    test_record_round_trip();
    test_buffer_growth_and_view();
    test_sequence_assignment();
    test_drain_semantics();
    test_host_wrappers();
    test_session_scaffolding();

    if (g_failures != 0)
    {
        fprintf(stderr, "ui1 tests failed: %d\n", g_failures);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
