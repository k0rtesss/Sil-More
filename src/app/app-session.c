#include "angband.h"

#include "app-session.h"

struct app_session {
    const app_host* host;
    u16b state;
    app_wait_state wait_state;
    app_snapshot snapshot;
    app_session_counters counters;
    app_event_buffer* events;
};

static size_t app_session_initial_event_capacity(size_t requested)
{
    if (requested == 0)
        return APP_SESSION_DEFAULT_EVENT_CAPACITY;

    return requested;
}

app_session* app_session_create(const app_session_config* config)
{
    app_session* session;
    size_t initial_event_capacity;

    if (!config || config->api_version != APP_SESSION_API_VERSION)
        return NULL;

    session = calloc(1, sizeof(*session));
    if (!session)
        return NULL;

    initial_event_capacity = app_session_initial_event_capacity(
        config->initial_event_capacity);

    session->host = config->host;
    session->state = APP_SESSION_STATE_IDLE;
    session->wait_state.reason = APP_WAIT_REASON_NONE;
    session->snapshot.scene = APP_SCENE_KIND_NONE;
    session->events = app_event_buffer_create(initial_event_capacity);

    if (!session->events)
    {
        app_session_destroy(session);
        return NULL;
    }

    return session;
}

void app_session_destroy(app_session* session)
{
    if (!session)
        return;

    app_event_buffer_destroy(session->events);
    free(session);
}

const app_host* app_session_host(const app_session* session)
{
    return session ? session->host : NULL;
}

u16b app_session_state_id(const app_session* session)
{
    return session ? session->state : APP_SESSION_STATE_UNINITIALIZED;
}

void app_session_set_state(app_session* session, u16b state)
{
    if (!session)
        return;

    session->state = state;
}

const app_wait_state* app_session_wait_state(const app_session* session)
{
    return session ? &session->wait_state : NULL;
}

void app_session_set_wait_state(app_session* session,
    const app_wait_state* wait_state)
{
    if (!session)
        return;

    if (wait_state)
        session->wait_state = *wait_state;
    else
        memset(&session->wait_state, 0, sizeof(session->wait_state));
    session->wait_state.reason = wait_state ? wait_state->reason
        : APP_WAIT_REASON_NONE;
}

const app_snapshot* app_session_snapshot(const app_session* session)
{
    return session ? &session->snapshot : NULL;
}

void app_session_set_snapshot(app_session* session,
    const app_snapshot* snapshot)
{
    if (!session)
        return;

    if (snapshot)
        session->snapshot = *snapshot;
    else
        memset(&session->snapshot, 0, sizeof(session->snapshot));

    if (!snapshot)
    {
        session->snapshot.scene = APP_SCENE_KIND_NONE;
        session->snapshot.flags = 0;
    }
}

const app_session_counters* app_session_get_counters(
    const app_session* session)
{
    return session ? &session->counters : NULL;
}

bool app_session_submit_input(app_session* session, const app_input* input)
{
    if (!session || !input)
        return false;

    session->counters.submitted_inputs++;
    return true;
}

bool app_session_submit_intent(app_session* session, const app_intent* intent)
{
    if (!session || !intent)
        return false;

    session->counters.submitted_intents++;
    return true;
}

bool app_session_emit_event(app_session* session,
    const app_event_record* record)
{
    if (!session || !record)
        return false;

    if (!app_event_buffer_push(session->events, record))
    {
        session->counters.dropped_events++;
        return false;
    }

    session->counters.emitted_events++;
    return true;
}

app_event_span app_session_view_events(const app_session* session)
{
    return session ? app_event_buffer_view(session->events)
        : app_event_buffer_view(NULL);
}

app_event_span app_session_drain_events(app_session* session)
{
    return session ? app_event_buffer_drain(session->events)
        : app_event_buffer_drain(NULL);
}

void app_session_clear_events(app_session* session)
{
    if (!session)
        return;

    app_event_buffer_clear(session->events);
}
