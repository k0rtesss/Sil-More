#ifndef INCLUDED_APP_SESSION_H
#define INCLUDED_APP_SESSION_H

#include "app-events.h"
#include "app-host.h"
#include "app-input.h"
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
    APP_SESSION_FLAG_ALLOW_INTENT_INPUT = 0x00000002u
} app_session_flag;

typedef struct app_wait_state {
    u16b reason;
    u16b flags;
    s32b detail0;
    s32b detail1;
} app_wait_state;

typedef struct app_session_config {
    u32b api_version;
    u32b flags;
    size_t initial_event_capacity;
    const app_host* host;
} app_session_config;

typedef struct app_session_counters {
    u64b submitted_inputs;
    u64b submitted_intents;
    u64b emitted_events;
    u64b dropped_events;
} app_session_counters;

typedef struct app_session app_session;

app_session* app_session_create(const app_session_config* config);
void app_session_destroy(app_session* session);
const app_host* app_session_host(const app_session* session);
u16b app_session_state_id(const app_session* session);
void app_session_set_state(app_session* session, u16b state);
const app_wait_state* app_session_wait_state(const app_session* session);
void app_session_set_wait_state(app_session* session,
    const app_wait_state* wait_state);
const app_snapshot* app_session_snapshot(const app_session* session);
void app_session_set_snapshot(app_session* session,
    const app_snapshot* snapshot);
const app_session_counters* app_session_get_counters(
    const app_session* session);
bool app_session_submit_input(app_session* session, const app_input* input);
bool app_session_submit_intent(app_session* session, const app_intent* intent);
bool app_session_emit_event(app_session* session,
    const app_event_record* record);
app_event_span app_session_view_events(const app_session* session);
app_event_span app_session_drain_events(app_session* session);
void app_session_clear_events(app_session* session);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SESSION_H */
