#include "angband.h"

#include "app-session.h"

typedef struct app_input_queue {
    app_input* items;
    size_t capacity;
    size_t head;
    size_t count;
} app_input_queue;

typedef struct app_intent_queue {
    app_intent* items;
    size_t capacity;
    size_t head;
    size_t count;
} app_intent_queue;

struct app_session {
    const app_host* host;
    u32b flags;
    u16b state;
    app_wait_state wait_state;
    app_snapshot snapshot;
    app_session_counters counters;
    app_event_buffer* events;
    app_input_queue inputs;
    app_intent_queue intents;
};

static app_session* g_current_session;

static size_t app_session_initial_event_capacity(size_t requested)
{
    if (requested == 0)
        return APP_SESSION_DEFAULT_EVENT_CAPACITY;

    return requested;
}

static size_t app_session_initial_input_capacity(void)
{
    return 64u;
}

static size_t app_session_initial_intent_capacity(void)
{
    return 16u;
}

static u64b app_session_event_timestamp(const app_session* session)
{
    if (!session)
        return 0;

    if (app_host_has_capability(session->host,
            APP_HOST_CAPABILITY_MONOTONIC_CLOCK))
    {
        return app_host_monotonic_usec(session->host);
    }

    if (app_host_has_capability(session->host, APP_HOST_CAPABILITY_WALL_CLOCK))
        return app_host_wall_usec(session->host);

    return 0;
}

static void app_session_emit_internal_event(app_session* session, u16b kind,
    u16b scope, s32b subject, s32b arg0, s32b arg1, s32b arg2)
{
    app_event_record record;

    if (!session)
        return;

    memset(&record, 0, sizeof(record));
    record.kind = kind;
    record.scope = scope;
    record.payload_size = sizeof(record);
    record.timestamp_usec = app_session_event_timestamp(session);
    record.subject = subject;
    record.arg0 = arg0;
    record.arg1 = arg1;
    record.arg2 = arg2;

    (void)app_session_emit_event(session, &record);
}

static bool app_input_queue_init(app_input_queue* queue, size_t capacity)
{
    if (!queue || capacity == 0)
        return false;

    memset(queue, 0, sizeof(*queue));
    queue->items = calloc(capacity, sizeof(*queue->items));
    if (!queue->items)
        return false;

    queue->capacity = capacity;
    return true;
}

static void app_input_queue_destroy(app_input_queue* queue)
{
    if (!queue)
        return;

    free(queue->items);
    memset(queue, 0, sizeof(*queue));
}

static bool app_input_queue_reserve(app_input_queue* queue, size_t capacity)
{
    app_input* items;
    size_t i;

    if (!queue || capacity <= queue->capacity)
        return true;

    items = calloc(capacity, sizeof(*items));
    if (!items)
        return false;

    for (i = 0; i < queue->count; i++)
    {
        size_t index = (queue->head + i) % queue->capacity;
        items[i] = queue->items[index];
    }

    free(queue->items);
    queue->items = items;
    queue->capacity = capacity;
    queue->head = 0;
    return true;
}

static bool app_input_queue_push(app_input_queue* queue, const app_input* input)
{
    size_t index;

    if (!queue || !input)
        return false;

    if (queue->count == queue->capacity)
    {
        size_t next_capacity = queue->capacity ? queue->capacity * 2u : 1u;
        if (!app_input_queue_reserve(queue, next_capacity))
            return false;
    }

    index = (queue->head + queue->count) % queue->capacity;
    queue->items[index] = *input;
    queue->count++;
    return true;
}

static bool app_input_queue_copy_front(const app_input_queue* queue,
    app_input* out_input)
{
    if (!queue || !queue->count)
        return false;

    if (out_input)
        *out_input = queue->items[queue->head];

    return true;
}

static bool app_input_queue_pop(app_input_queue* queue, app_input* out_input)
{
    if (!app_input_queue_copy_front(queue, out_input))
        return false;

    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return true;
}

static void app_input_queue_clear(app_input_queue* queue)
{
    if (!queue)
        return;

    queue->head = 0;
    queue->count = 0;
}

static bool app_intent_queue_init(app_intent_queue* queue, size_t capacity)
{
    if (!queue || capacity == 0)
        return false;

    memset(queue, 0, sizeof(*queue));
    queue->items = calloc(capacity, sizeof(*queue->items));
    if (!queue->items)
        return false;

    queue->capacity = capacity;
    return true;
}

static void app_intent_queue_destroy(app_intent_queue* queue)
{
    if (!queue)
        return;

    free(queue->items);
    memset(queue, 0, sizeof(*queue));
}

static bool app_intent_queue_reserve(app_intent_queue* queue, size_t capacity)
{
    app_intent* items;
    size_t i;

    if (!queue || capacity <= queue->capacity)
        return true;

    items = calloc(capacity, sizeof(*items));
    if (!items)
        return false;

    for (i = 0; i < queue->count; i++)
    {
        size_t index = (queue->head + i) % queue->capacity;
        items[i] = queue->items[index];
    }

    free(queue->items);
    queue->items = items;
    queue->capacity = capacity;
    queue->head = 0;
    return true;
}

static bool app_intent_queue_push(app_intent_queue* queue,
    const app_intent* intent)
{
    size_t index;

    if (!queue || !intent)
        return false;

    if (queue->count == queue->capacity)
    {
        size_t next_capacity = queue->capacity ? queue->capacity * 2u : 1u;
        if (!app_intent_queue_reserve(queue, next_capacity))
            return false;
    }

    index = (queue->head + queue->count) % queue->capacity;
    queue->items[index] = *intent;
    queue->count++;
    return true;
}

static bool app_intent_queue_copy_front(const app_intent_queue* queue,
    app_intent* out_intent)
{
    if (!queue || !queue->count)
        return false;

    if (out_intent)
        *out_intent = queue->items[queue->head];

    return true;
}

static bool app_intent_queue_pop(app_intent_queue* queue,
    app_intent* out_intent)
{
    if (!app_intent_queue_copy_front(queue, out_intent))
        return false;

    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return true;
}

static void app_intent_queue_clear(app_intent_queue* queue)
{
    if (!queue)
        return;

    queue->head = 0;
    queue->count = 0;
}

app_session* app_session_current(void)
{
    return g_current_session;
}

void app_session_make_current(app_session* session)
{
    g_current_session = session;
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
    session->flags = config->flags;
    session->state = APP_SESSION_STATE_IDLE;
    session->wait_state.reason = APP_WAIT_REASON_NONE;
    session->snapshot.scene = APP_SCENE_KIND_NONE;
    session->events = app_event_buffer_create(initial_event_capacity);

    if (!session->events
        || !app_input_queue_init(&session->inputs,
            app_session_initial_input_capacity())
        || !app_intent_queue_init(&session->intents,
            app_session_initial_intent_capacity()))
    {
        app_session_destroy(session);
        return NULL;
    }

    app_session_emit_internal_event(session, APP_EVENT_KIND_SESSION_LIFECYCLE,
        APP_EVENT_SCOPE_SESSION, APP_SESSION_STATE_IDLE,
        APP_SESSION_STATE_UNINITIALIZED, 0, 0);
    return session;
}

void app_session_destroy(app_session* session)
{
    if (!session)
        return;

    if (g_current_session == session)
        g_current_session = NULL;

    app_input_queue_destroy(&session->inputs);
    app_intent_queue_destroy(&session->intents);
    app_event_buffer_destroy(session->events);
    free(session);
}

const app_host* app_session_host(const app_session* session)
{
    return session ? session->host : NULL;
}

u32b app_session_flags(const app_session* session)
{
    return session ? session->flags : 0;
}

void app_session_set_flags(app_session* session, u32b flags)
{
    if (!session)
        return;

    session->flags = flags;
}

bool app_session_has_flag(const app_session* session, u32b flag_mask)
{
    return session && ((session->flags & flag_mask) == flag_mask);
}

u16b app_session_state_id(const app_session* session)
{
    return session ? session->state : APP_SESSION_STATE_UNINITIALIZED;
}

void app_session_set_state(app_session* session, u16b state)
{
    u16b previous_state;

    if (!session)
        return;

    previous_state = session->state;
    if (previous_state == state)
        return;

    session->state = state;
    app_session_emit_internal_event(session, APP_EVENT_KIND_SESSION_LIFECYCLE,
        APP_EVENT_SCOPE_SESSION, state, previous_state,
        session->wait_state.reason, 0);
}

const app_wait_state* app_session_wait_state(const app_session* session)
{
    return session ? &session->wait_state : NULL;
}

void app_session_set_wait_state(app_session* session,
    const app_wait_state* wait_state)
{
    app_wait_state next_wait_state;

    if (!session)
        return;

    memset(&next_wait_state, 0, sizeof(next_wait_state));
    if (wait_state)
        next_wait_state = *wait_state;
    next_wait_state.reason = wait_state ? wait_state->reason
        : APP_WAIT_REASON_NONE;

    if (memcmp(&session->wait_state, &next_wait_state,
            sizeof(next_wait_state)) == 0)
        return;

    session->wait_state = next_wait_state;
    app_session_emit_internal_event(session, APP_EVENT_KIND_WAIT_STATE,
        APP_EVENT_SCOPE_SESSION, session->wait_state.reason,
        session->wait_state.detail0, session->wait_state.detail1,
        session->state);
}

void app_session_begin_wait(app_session* session, u16b reason, s32b detail0,
    s32b detail1)
{
    app_wait_state wait_state;

    if (!session)
        return;

    memset(&wait_state, 0, sizeof(wait_state));
    wait_state.reason = reason;
    wait_state.detail0 = detail0;
    wait_state.detail1 = detail1;

    app_session_set_wait_state(session, &wait_state);
    app_session_set_state(session, APP_SESSION_STATE_WAITING);
}

void app_session_resume_running(app_session* session)
{
    if (!session)
        return;

    app_session_set_wait_state(session, NULL);
    app_session_set_state(session, APP_SESSION_STATE_RUNNING);
}

void app_session_push_wait_scope(app_session* session, app_wait_scope* scope,
    u16b reason, s32b detail0, s32b detail1)
{
    if (!scope)
        return;

    memset(scope, 0, sizeof(*scope));
    if (!session)
        return;

    scope->active = true;
    scope->state = app_session_state_id(session);
    scope->wait_state = *app_session_wait_state(session);
    app_session_begin_wait(session, reason, detail0, detail1);
}

void app_session_pop_wait_scope(app_session* session,
    const app_wait_scope* scope)
{
    if (!session || !scope || !scope->active)
        return;

    app_session_set_wait_state(session, &scope->wait_state);
    app_session_set_state(session, scope->state);
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

    if ((input->layer == APP_INPUT_LAYER_LEGACY)
        && !app_session_has_flag(session, APP_SESSION_FLAG_ALLOW_LEGACY_INPUT))
    {
        return false;
    }

    if ((input->layer == APP_INPUT_LAYER_INTENT)
        && !app_session_has_flag(session, APP_SESSION_FLAG_ALLOW_INTENT_INPUT))
    {
        return false;
    }

    session->counters.submitted_inputs++;
    return app_input_queue_push(&session->inputs, input);
}

size_t app_session_pending_input_count(const app_session* session)
{
    return session ? session->inputs.count : 0u;
}

bool app_session_peek_input(const app_session* session, app_input* out_input)
{
    return session ? app_input_queue_copy_front(&session->inputs, out_input)
        : false;
}

bool app_session_pop_input(app_session* session, app_input* out_input)
{
    if (!session || !app_input_queue_pop(&session->inputs, out_input))
        return false;

    session->counters.consumed_inputs++;
    return true;
}

void app_session_clear_inputs(app_session* session)
{
    if (!session)
        return;

    app_input_queue_clear(&session->inputs);
}

bool app_session_submit_intent(app_session* session, const app_intent* intent)
{
    if (!session || !intent)
        return false;

    if (!app_session_has_flag(session, APP_SESSION_FLAG_ALLOW_INTENT_INPUT))
        return false;

    session->counters.submitted_intents++;
    return app_intent_queue_push(&session->intents, intent);
}

size_t app_session_pending_intent_count(const app_session* session)
{
    return session ? session->intents.count : 0u;
}

bool app_session_peek_intent(const app_session* session,
    app_intent* out_intent)
{
    return session ? app_intent_queue_copy_front(&session->intents, out_intent)
        : false;
}

bool app_session_pop_intent(app_session* session, app_intent* out_intent)
{
    if (!session || !app_intent_queue_pop(&session->intents, out_intent))
        return false;

    session->counters.consumed_intents++;
    return true;
}

void app_session_clear_intents(app_session* session)
{
    if (!session)
        return;

    app_intent_queue_clear(&session->intents);
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
