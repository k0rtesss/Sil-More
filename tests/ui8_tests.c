#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "angband.h"
#include "app/app-host-bridge.h"
#include "app/app-session.h"
#include "app/app-wire.h"

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
    record.timestamp_usec = 777;
    return record;
}

typedef struct host_log_capture {
    u32b calls;
    u16b level;
    char subsystem[APP_HOST_BRIDGE_LOG_SUBSYSTEM_MAX];
    char message[APP_HOST_BRIDGE_LOG_TEXT_MAX];
} host_log_capture;

static void capture_host_log(void* user_data, u16b level,
    const char* subsystem, const char* message)
{
    host_log_capture* capture = user_data;

    if (!capture)
        return;

    capture->calls++;
    capture->level = level;
    SDL_strlcpy(capture->subsystem, subsystem ? subsystem : "",
        sizeof(capture->subsystem));
    SDL_strlcpy(capture->message, message ? message : "",
        sizeof(capture->message));
}

static void test_snapshot_packet_round_trip(void)
{
    static const byte blob0[] = { 0x10, 0x20, 0x30, 0x40 };
    static const byte blob1[] = { 0xAA, 0xBB, 0xCC };
    app_snapshot_blob blobs[2];
    app_snapshot snapshot;
    app_snapshot decoded;
    app_snapshot_blob decoded_blobs[2];
    size_t packet_size;
    size_t written_size = 0;
    byte* packet;
    byte* packet_copy;

    memset(blobs, 0, sizeof(blobs));
    blobs[0].kind = APP_SNAPSHOT_BLOB_HEADER;
    blobs[0].format_version = 3;
    blobs[0].data = blob0;
    blobs[0].size = sizeof(blob0);
    blobs[1].kind = APP_SNAPSHOT_BLOB_OVERLAY;
    blobs[1].format_version = 7;
    blobs[1].data = blob1;
    blobs[1].size = sizeof(blob1);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 42;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.flags = APP_SNAPSHOT_FLAG_PARTIAL | APP_SNAPSHOT_FLAG_WAITING;
    snapshot.blobs = blobs;
    snapshot.blob_count = N_ELEMENTS(blobs);

    packet_size = app_wire_snapshot_packet_size(&snapshot);
    CHECK(packet_size > 0);

    packet = calloc(packet_size, 1);
    packet_copy = calloc(packet_size, 1);
    CHECK(packet != NULL);
    CHECK(packet_copy != NULL);
    if (!packet || !packet_copy)
    {
        free(packet);
        free(packet_copy);
        return;
    }

    CHECK(app_wire_serialize_snapshot_packet(&snapshot, packet, packet_size,
        &written_size));
    CHECK(written_size == packet_size);
    CHECK(app_wire_serialize_snapshot_packet(&snapshot, packet_copy, packet_size,
        NULL));
    CHECK(memcmp(packet, packet_copy, packet_size) == 0);

    memset(&decoded, 0, sizeof(decoded));
    memset(decoded_blobs, 0, sizeof(decoded_blobs));
    CHECK(app_wire_deserialize_snapshot_packet(packet, packet_size, &decoded,
        decoded_blobs, N_ELEMENTS(decoded_blobs)));
    CHECK(decoded.revision == snapshot.revision);
    CHECK(decoded.scene == snapshot.scene);
    CHECK(decoded.flags == snapshot.flags);
    CHECK(decoded.blob_count == snapshot.blob_count);
    CHECK(decoded.blobs[0].kind == blobs[0].kind);
    CHECK(decoded.blobs[0].format_version == blobs[0].format_version);
    CHECK(decoded.blobs[0].size == blobs[0].size);
    CHECK(memcmp(decoded.blobs[0].data, blob0, sizeof(blob0)) == 0);
    CHECK(decoded.blobs[1].kind == blobs[1].kind);
    CHECK(decoded.blobs[1].format_version == blobs[1].format_version);
    CHECK(decoded.blobs[1].size == blobs[1].size);
    CHECK(memcmp(decoded.blobs[1].data, blob1, sizeof(blob1)) == 0);

    packet[4] = 0xFF;
    CHECK(!app_wire_deserialize_snapshot_packet(packet, packet_size, &decoded,
        decoded_blobs, N_ELEMENTS(decoded_blobs)));

    free(packet);
    free(packet_copy);
}

static void test_event_packet_round_trip(void)
{
    app_event_record records[2];
    app_event_record decoded_records[2];
    app_event_span span;
    app_event_span decoded;
    size_t packet_size;
    byte* packet;

    records[0] = make_event(APP_EVENT_KIND_WAIT_STATE,
        APP_EVENT_SCOPE_SESSION, 12, 3, 4, 5, 6);
    records[1] = make_event(APP_EVENT_KIND_PROJECTILE,
        APP_EVENT_SCOPE_SCENE, 13, 9, 8, 7, 6);
    records[1].flags = APP_EVENT_FLAG_TRANSIENT;

    span.records = records;
    span.count = N_ELEMENTS(records);
    span.dropped_count = 5;

    packet_size = app_wire_event_packet_size(&span);
    CHECK(packet_size > 0);

    packet = calloc(packet_size, 1);
    CHECK(packet != NULL);
    if (!packet)
        return;

    CHECK(app_wire_serialize_event_packet(&span, packet, packet_size, NULL));
    CHECK(app_wire_deserialize_event_packet(packet, packet_size, decoded_records,
        N_ELEMENTS(decoded_records), &decoded));
    CHECK(decoded.count == span.count);
    CHECK(decoded.dropped_count == span.dropped_count);
    CHECK(decoded.records[0].kind == records[0].kind);
    CHECK(decoded.records[0].sequence == records[0].sequence);
    CHECK(decoded.records[1].kind == records[1].kind);
    CHECK(decoded.records[1].flags == records[1].flags);
    CHECK(decoded.records[1].arg2 == records[1].arg2);

    packet[6] = 0;
    CHECK(!app_wire_deserialize_event_packet(packet, packet_size,
        decoded_records, N_ELEMENTS(decoded_records), &decoded));

    free(packet);
}

static void test_session_export_and_state_packet(void)
{
    static const byte blob_data[] = { 1, 2, 3 };
    app_session_config config;
    app_session* session;
    app_wait_state wait_state;
    app_snapshot_blob blob;
    app_snapshot snapshot;
    app_input input;
    app_intent intent;
    app_session_export state;
    app_session_export decoded;
    app_event_record emitted;
    size_t packet_size;
    byte* packet;

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    memset(&wait_state, 0, sizeof(wait_state));
    wait_state.reason = APP_WAIT_REASON_COMMAND_INPUT;
    wait_state.flags = 9;
    wait_state.detail0 = 17;
    wait_state.detail1 = 29;
    app_session_set_wait_state(session, &wait_state);

    memset(&blob, 0, sizeof(blob));
    blob.kind = APP_SNAPSHOT_BLOB_HEADER;
    blob.format_version = 2;
    blob.data = blob_data;
    blob.size = sizeof(blob_data);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 91;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.flags = APP_SNAPSHOT_FLAG_DIRTY;
    snapshot.blobs = &blob;
    snapshot.blob_count = 1;
    app_session_set_snapshot(session, &snapshot);

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'k';
    CHECK(app_session_submit_input(session, &input));

    memset(&intent, 0, sizeof(intent));
    intent.kind = APP_INTENT_KIND_CONFIRM;
    CHECK(app_session_submit_intent(session, &intent));

    emitted = make_event(APP_EVENT_KIND_DAMAGE, APP_EVENT_SCOPE_SCENE, 0,
        7, 8, 9, 10);
    CHECK(app_session_emit_event(session, &emitted));

    memset(&state, 0, sizeof(state));
    app_session_export_state(session, &state);
    CHECK(state.api_version == APP_SESSION_API_VERSION);
    CHECK(state.flags == config.flags);
    CHECK(state.state == APP_SESSION_STATE_IDLE);
    CHECK(state.wait_reason == APP_WAIT_REASON_COMMAND_INPUT);
    CHECK(state.wait_flags == 9);
    CHECK(state.wait_detail0 == 17);
    CHECK(state.wait_detail1 == 29);
    CHECK(state.snapshot_revision == 91);
    CHECK(state.snapshot_scene == APP_SCENE_KIND_DUNGEON);
    CHECK(state.snapshot_flags == APP_SNAPSHOT_FLAG_DIRTY);
    CHECK(state.snapshot_blob_count == 1);
    CHECK(state.pending_input_count == 1);
    CHECK(state.pending_intent_count == 1);
    CHECK(state.pending_event_count >= 3);
    CHECK(state.counters.submitted_inputs == 1);
    CHECK(state.counters.submitted_intents == 1);
    CHECK(state.counters.emitted_events >= 3);

    packet_size = app_wire_session_packet_size();
    packet = calloc(packet_size, 1);
    CHECK(packet != NULL);
    if (!packet)
    {
        app_session_destroy(session);
        return;
    }

    CHECK(app_wire_serialize_session_packet(&state, packet, packet_size, NULL));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(app_wire_deserialize_session_packet(packet, packet_size, &decoded));
    CHECK(decoded.api_version == state.api_version);
    CHECK(decoded.wait_reason == state.wait_reason);
    CHECK(decoded.snapshot_revision == state.snapshot_revision);
    CHECK(decoded.pending_event_count == state.pending_event_count);
    CHECK(decoded.counters.emitted_events == state.counters.emitted_events);

    free(packet);
    app_session_destroy(session);
}

static void test_input_and_intent_packets_submit(void)
{
    app_session_config config;
    app_session* session;
    app_input input;
    app_input queued_input;
    app_intent intent;
    app_intent queued_intent;
    size_t input_packet_size;
    size_t intent_packet_size;
    byte* input_packet;
    byte* intent_packet;
    app_input invalid_input;

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS;
    input.payload.key.logical_key = 'x';
    input.payload.key.physical_key = 'X';
    input.payload.key.repeat_count = 2;

    input_packet_size = app_wire_input_packet_size();
    input_packet = calloc(input_packet_size, 1);
    intent_packet_size = app_wire_intent_packet_size();
    intent_packet = calloc(intent_packet_size, 1);
    CHECK(input_packet != NULL);
    CHECK(intent_packet != NULL);
    if (!input_packet || !intent_packet)
    {
        free(input_packet);
        free(intent_packet);
        app_session_destroy(session);
        return;
    }

    CHECK(app_wire_serialize_input_packet(&input, input_packet,
        input_packet_size, NULL));
    CHECK(app_wire_submit_input_packet(session, input_packet,
        input_packet_size));
    CHECK(app_session_peek_input(session, &queued_input));
    CHECK(queued_input.payload.key.logical_key == 'x');
    CHECK(queued_input.payload.key.repeat_count == 2);

    memset(&intent, 0, sizeof(intent));
    intent.kind = APP_INTENT_KIND_COMMAND;
    intent.payload.command.command_id = 99;
    memcpy(intent.payload.command.token, "open-door", sizeof("open-door"));

    CHECK(app_wire_serialize_intent_packet(&intent, intent_packet,
        intent_packet_size, NULL));
    CHECK(app_wire_submit_intent_packet(session, intent_packet,
        intent_packet_size));
    CHECK(app_session_peek_intent(session, &queued_intent));
    CHECK(queued_intent.kind == APP_INTENT_KIND_COMMAND);
    CHECK(queued_intent.payload.command.command_id == 99);
    CHECK(strcmp(queued_intent.payload.command.token, "open-door") == 0);

    memset(&invalid_input, 0, sizeof(invalid_input));
    invalid_input.layer = APP_INPUT_LAYER_LEGACY;
    invalid_input.type = APP_INPUT_TYPE_NONE;
    CHECK(!app_wire_serialize_input_packet(&invalid_input, input_packet,
        input_packet_size, NULL));

    input_packet[6] = 0;
    CHECK(!app_wire_submit_input_packet(session, input_packet,
        input_packet_size));

    free(input_packet);
    free(intent_packet);
    app_session_destroy(session);
}

static void test_session_wrappers(void)
{
    static const byte blob_data[] = { 9, 8, 7, 6 };
    app_session_config config;
    app_session* session;
    app_snapshot_blob blob;
    app_snapshot snapshot;
    app_event_record emitted;
    app_snapshot decoded_snapshot;
    app_snapshot_blob decoded_blobs[1];
    app_event_record decoded_records[1];
    app_event_span decoded_span;
    size_t snapshot_packet_size;
    size_t event_packet_size;
    byte* snapshot_packet;
    byte* event_packet;

    memset(&config, 0, sizeof(config));
    config.api_version = APP_SESSION_API_VERSION;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT;

    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
        return;

    app_session_clear_events(session);

    memset(&blob, 0, sizeof(blob));
    blob.kind = APP_SNAPSHOT_BLOB_MESSAGES;
    blob.format_version = 4;
    blob.data = blob_data;
    blob.size = sizeof(blob_data);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.revision = 1234;
    snapshot.scene = APP_SCENE_KIND_MENU;
    snapshot.flags = APP_SNAPSHOT_FLAG_PARTIAL;
    snapshot.blobs = &blob;
    snapshot.blob_count = 1;
    app_session_set_snapshot(session, &snapshot);

    snapshot_packet_size = app_wire_session_snapshot_packet_size(session);
    CHECK(snapshot_packet_size > 0);
    snapshot_packet = calloc(snapshot_packet_size, 1);
    CHECK(snapshot_packet != NULL);
    if (!snapshot_packet)
    {
        app_session_destroy(session);
        return;
    }

    CHECK(app_wire_serialize_session_snapshot_packet(session, snapshot_packet,
        snapshot_packet_size, NULL));
    CHECK(app_wire_deserialize_snapshot_packet(snapshot_packet,
        snapshot_packet_size, &decoded_snapshot, decoded_blobs,
        N_ELEMENTS(decoded_blobs)));
    CHECK(decoded_snapshot.scene == APP_SCENE_KIND_MENU);
    CHECK(decoded_snapshot.revision == 1234);
    CHECK(decoded_snapshot.blobs[0].kind == APP_SNAPSHOT_BLOB_MESSAGES);
    CHECK(memcmp(decoded_snapshot.blobs[0].data, blob_data,
        sizeof(blob_data)) == 0);

    emitted = make_event(APP_EVENT_KIND_OBJECT_TRANSFER,
        APP_EVENT_SCOPE_ENTITY, 0, 4, 5, 6, 7);
    CHECK(app_session_emit_event(session, &emitted));

    event_packet_size = app_wire_session_event_packet_size(session);
    CHECK(event_packet_size > 0);
    event_packet = calloc(event_packet_size, 1);
    CHECK(event_packet != NULL);
    if (!event_packet)
    {
        free(snapshot_packet);
        app_session_destroy(session);
        return;
    }

    CHECK(app_wire_serialize_session_event_packet(session, false, event_packet,
        event_packet_size, NULL));
    CHECK(app_session_view_events(session).count == 1);
    CHECK(app_wire_deserialize_event_packet(event_packet, event_packet_size,
        decoded_records, N_ELEMENTS(decoded_records), &decoded_span));
    CHECK(decoded_span.count == 1);
    CHECK(decoded_span.records[0].kind == APP_EVENT_KIND_OBJECT_TRANSFER);

    CHECK(app_wire_serialize_session_event_packet(session, true, event_packet,
        event_packet_size, NULL));
    CHECK(app_session_view_events(session).count == 0);

    free(event_packet);
    free(snapshot_packet);
    app_session_destroy(session);
}

static void test_host_bridge_round_trip(void)
{
    static const byte stored_blob[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    app_host_bridge bridge;
    const app_host* host;
    app_host_blob blob;
    host_log_capture capture;
    app_session_config config;
    app_session* session;
    app_event_span events;
    char path[APP_HOST_BRIDGE_PATH_MAX];

    memset(&bridge, 0, sizeof(bridge));
    memset(&blob, 0, sizeof(blob));
    memset(&capture, 0, sizeof(capture));
    memset(&config, 0, sizeof(config));
    memset(path, 0, sizeof(path));

    app_host_bridge_init(&bridge);
    host = app_host_bridge_host(&bridge);
    CHECK(host != NULL);
    CHECK(app_host_has_capability(host, APP_HOST_CAPABILITY_MONOTONIC_CLOCK));
    CHECK(app_host_has_capability(host, APP_HOST_CAPABILITY_WALL_CLOCK));
    CHECK(app_host_has_capability(host,
        APP_HOST_CAPABILITY_PERSISTENT_STORAGE));
    CHECK(app_host_has_capability(host, APP_HOST_CAPABILITY_RESOURCE_LOOKUP));
    CHECK(app_host_has_capability(host, APP_HOST_CAPABILITY_LOGGING));

    app_host_bridge_set_time(&bridge, 123456u, 654321u);
    CHECK(app_host_monotonic_usec(host) == 123456u);
    CHECK(app_host_wall_usec(host) == 654321u);

    CHECK(app_host_bridge_set_path(&bridge, APP_HOST_PATH_SAVE, "demo/save"));
    CHECK(app_host_resolve_path(host, APP_HOST_PATH_SAVE, "slot-a.bin", path,
        sizeof(path)));
    CHECK(strcmp(path, "demo/save/slot-a.bin") == 0);

    CHECK(app_host_store_blob(host, APP_HOST_PATH_SAVE, "slot-a.bin",
        stored_blob, sizeof(stored_blob), 7, 11));
    CHECK(app_host_load_blob(host, APP_HOST_PATH_SAVE, "slot-a.bin", &blob));
    CHECK(blob.size == sizeof(stored_blob));
    CHECK(blob.version == 7);
    CHECK(blob.flags == 11);
    CHECK(memcmp(blob.data, stored_blob, sizeof(stored_blob)) == 0);
    app_host_release_blob(host, &blob);
    CHECK(blob.data == NULL);
    CHECK(blob.size == 0);
    CHECK(app_host_bridge_remove(&bridge, APP_HOST_PATH_SAVE, "slot-a.bin"));
    CHECK(!app_host_load_blob(host, APP_HOST_PATH_SAVE, "slot-a.bin", &blob));

    app_host_bridge_set_log_callback(&bridge, capture_host_log, &capture);
    app_host_log(host, APP_HOST_LOG_WARN, "ui8", "bridge online");
    CHECK(app_host_bridge_log_count(&bridge) == 1);
    CHECK(app_host_bridge_last_log_level(&bridge) == APP_HOST_LOG_WARN);
    CHECK(strcmp(app_host_bridge_last_log_subsystem(&bridge), "ui8") == 0);
    CHECK(strcmp(app_host_bridge_last_log_message(&bridge),
        "bridge online") == 0);
    CHECK(capture.calls == 1);
    CHECK(capture.level == APP_HOST_LOG_WARN);
    CHECK(strcmp(capture.subsystem, "ui8") == 0);
    CHECK(strcmp(capture.message, "bridge online") == 0);

    config.api_version = APP_SESSION_API_VERSION;
    config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT;
    config.host = host;
    session = app_session_create(&config);
    CHECK(session != NULL);
    if (!session)
    {
        app_host_bridge_destroy(&bridge);
        return;
    }

    app_session_clear_events(session);
    app_session_begin_wait(session, APP_WAIT_REASON_CONFIRM, 3, 4);
    events = app_session_view_events(session);
    CHECK(events.count >= 1);
    CHECK(events.records[0].kind == APP_EVENT_KIND_WAIT_STATE);
    CHECK(events.records[0].timestamp_usec == 123456u);

    app_session_destroy(session);
    app_host_bridge_destroy(&bridge);
}

int main(void)
{
    test_snapshot_packet_round_trip();
    test_event_packet_round_trip();
    test_session_export_and_state_packet();
    test_input_and_intent_packets_submit();
    test_session_wrappers();
    test_host_bridge_round_trip();

    if (g_failures != 0)
    {
        fprintf(stderr, "ui8 tests failed: %d\n", g_failures);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
