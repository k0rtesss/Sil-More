#include "angband.h"

#include "app-session.h"
#include "externs.h"
#include "runtime-cli.h"

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
    app_interaction_state interaction;
    app_snapshot snapshot;
    app_dungeon_snapshot dungeon_snapshot;
    app_information_snapshot information_snapshot;
    u32b snapshot_dirty_mask;
    u64b next_snapshot_revision;
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

static void app_session_emit_snapshot_invalidation(app_session* session,
    u32b invalidation_mask)
{
    if (!session || !invalidation_mask)
        return;

    app_session_emit_internal_event(session, APP_EVENT_KIND_SNAPSHOT_INVALIDATED,
        APP_EVENT_SCOPE_SCENE, APP_SCENE_KIND_DUNGEON,
        (s32b)invalidation_mask, session->state, session->wait_state.reason);
}

static void app_interaction_state_init(app_interaction_state* interaction)
{
    if (!interaction)
        return;

    memset(interaction, 0, sizeof(*interaction));
    interaction->format_version = APP_INTERACTION_FORMAT_VERSION;
    interaction->selected_index = -1;
}

static void app_session_touch_interaction(app_session* session)
{
    if (!session)
        return;

    app_session_mark_snapshot_dirty(session, APP_SNAPSHOT_INVALIDATE_OVERLAY);
    if (app_session_interactions_enabled(session))
        (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
}

static void app_session_sync_information_blob(app_session* session)
{
    if (!session)
        return;

    session->information_snapshot.snapshot.scene = APP_SCENE_KIND_INFORMATION;
    session->information_snapshot.snapshot.blobs = session->information_snapshot.blobs;
    session->information_snapshot.snapshot.blob_count = N_ELEMENTS(session->information_snapshot.blobs);
    session->information_snapshot.blobs[0].kind = APP_SNAPSHOT_BLOB_INFORMATION;
    session->information_snapshot.blobs[0].format_version = APP_INFORMATION_FORMAT_VERSION;
    session->information_snapshot.blobs[0].data = (const byte*)&session->information_snapshot.scene;
    session->information_snapshot.blobs[0].size = sizeof(session->information_snapshot.scene);
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
    app_interaction_state_init(&session->interaction);
    session->snapshot.scene = APP_SCENE_KIND_NONE;
    app_dungeon_snapshot_init(&session->dungeon_snapshot);
    app_information_snapshot_init(&session->information_snapshot);
    session->next_snapshot_revision = 1u;
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

    app_dungeon_snapshot_destroy(&session->dungeon_snapshot);
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

    if (session->snapshot.scene == APP_SCENE_KIND_DUNGEON)
    {
        app_session_mark_snapshot_dirty(session,
            APP_SNAPSHOT_INVALIDATE_CURSOR | APP_SNAPSHOT_INVALIDATE_TARGET
            | APP_SNAPSHOT_INVALIDATE_PANES
            | APP_SNAPSHOT_INVALIDATE_OVERLAY);
        (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
    }
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

    if (session->snapshot.scene != APP_SCENE_KIND_DUNGEON)
        session->snapshot_dirty_mask = 0;
}

const app_dungeon_snapshot* app_session_dungeon_snapshot(
    const app_session* session)
{
    return session ? &session->dungeon_snapshot : NULL;
}

const app_information_snapshot* app_session_information_snapshot(
    const app_session* session)
{
    return session ? &session->information_snapshot : NULL;
}

void app_session_clear_information_snapshot(app_session* session)
{
    if (!session)
        return;

    app_information_scene_init(&session->information_snapshot.scene);
    app_session_sync_information_blob(session);
}

bool app_session_add_information_op(app_session* session, s16b row,
    s16b col, byte attr, cptr text)
{
    if (!session)
        return false;

    return app_information_scene_add_text(&session->information_snapshot.scene,
        row, col, attr, text);
}

bool app_session_publish_information_snapshot(app_session* session)
{
    if (!session)
        return false;

    app_session_sync_information_blob(session);
    session->information_snapshot.snapshot.flags
        = APP_SNAPSHOT_FLAG_PARTIAL | APP_SNAPSHOT_FLAG_WAITING;
    session->information_snapshot.snapshot.revision = session->next_snapshot_revision++;
    session->snapshot = session->information_snapshot.snapshot;
    session->snapshot_dirty_mask = 0;
    return true;
}

void app_session_mark_snapshot_dirty(app_session* session,
    u32b invalidation_mask)
{
    u32b new_bits;

    if (!session || !invalidation_mask)
        return;

    new_bits = invalidation_mask & ~session->snapshot_dirty_mask;
    session->snapshot_dirty_mask |= invalidation_mask;

    if (new_bits && (session->snapshot.scene == APP_SCENE_KIND_DUNGEON))
        app_session_emit_snapshot_invalidation(session, new_bits);
}

bool app_session_build_dungeon_snapshot(app_session* session,
    u32b update_mask, u32b redraw_mask, u32b window_mask)
{
    u32b mask;

    if (!session)
        return false;

    if (session->snapshot.scene != APP_SCENE_KIND_DUNGEON)
        return false;

    mask = app_snapshot_invalidation_from_masks(update_mask, redraw_mask,
        window_mask);
    if (mask)
        app_session_mark_snapshot_dirty(session, mask);

    if (!session->snapshot_dirty_mask
        && (session->snapshot.scene == APP_SCENE_KIND_DUNGEON)
        && session->dungeon_snapshot.snapshot.revision)
    {
        return true;
    }

    if (!app_build_dungeon_snapshot(&session->dungeon_snapshot,
            session->next_snapshot_revision, &session->wait_state,
            &session->interaction, update_mask, redraw_mask, window_mask))
    {
        return false;
    }

    session->next_snapshot_revision++;
    session->snapshot = session->dungeon_snapshot.snapshot;
    session->snapshot_dirty_mask = 0;
    return true;
}

void app_session_note_message(app_session* session, u16b message_type)
{
    if (!session)
        return;

    app_session_emit_internal_event(session, APP_EVENT_KIND_MESSAGE,
        APP_EVENT_SCOPE_SESSION, message_type, 0, message_num(), 0);
    app_session_mark_snapshot_dirty(session, APP_SNAPSHOT_INVALIDATE_MESSAGES);
}

void app_session_note_animation(app_session* session, u16b animation_kind,
    s32b subject, s32b arg0, s32b arg1, s32b arg2,
    u32b invalidation_mask)
{
    u16b event_kind = APP_EVENT_KIND_ANIMATION_HINT;

    if (!session)
        return;

    if (animation_kind == APP_ANIMATION_HINT_ACTOR_MOVED)
        event_kind = APP_EVENT_KIND_ACTOR_MOVED;
    else if (animation_kind == APP_ANIMATION_HINT_DAMAGE)
        event_kind = APP_EVENT_KIND_DAMAGE;
    else if (animation_kind == APP_ANIMATION_HINT_PROJECTILE)
        event_kind = APP_EVENT_KIND_PROJECTILE;
    else if (animation_kind == APP_ANIMATION_HINT_OBJECT_TRANSFER)
        event_kind = APP_EVENT_KIND_OBJECT_TRANSFER;

    app_session_emit_internal_event(session, event_kind, APP_EVENT_SCOPE_SCENE,
        subject, arg0, arg1, arg2);
    if (invalidation_mask)
        app_session_mark_snapshot_dirty(session, invalidation_mask);
}

void app_session_note_cursor_relative(app_session* session, s16b map_y,
    s16b map_x)
{
    if (!session)
        return;

    session->dungeon_snapshot.cursor_state.visible = inkey_cursor_hidden()
        ? 0 : 1;
    session->dungeon_snapshot.cursor_state.relative = 1;
    session->dungeon_snapshot.cursor_state.row = 0;
    session->dungeon_snapshot.cursor_state.col = 0;
    session->dungeon_snapshot.cursor_state.map_y = map_y;
    session->dungeon_snapshot.cursor_state.map_x = map_x;
    app_session_mark_snapshot_dirty(session,
        APP_SNAPSHOT_INVALIDATE_CURSOR | APP_SNAPSHOT_INVALIDATE_MAP);
}

void app_session_note_cursor_absolute(app_session* session, s16b row,
    s16b col, bool visible)
{
    if (!session)
        return;

    session->dungeon_snapshot.cursor_state.visible = visible ? 1 : 0;
    session->dungeon_snapshot.cursor_state.relative = 0;
    session->dungeon_snapshot.cursor_state.row = row;
    session->dungeon_snapshot.cursor_state.col = col;
    session->dungeon_snapshot.cursor_state.map_y = -1;
    session->dungeon_snapshot.cursor_state.map_x = -1;
    app_session_mark_snapshot_dirty(session, APP_SNAPSHOT_INVALIDATE_CURSOR);
}

void app_session_set_cursor_visible(app_session* session, bool visible)
{
    u32b invalidation_mask = APP_SNAPSHOT_INVALIDATE_CURSOR;

    if (!session)
        return;
    if (session->dungeon_snapshot.cursor_state.visible == (visible ? 1 : 0))
        return;

    session->dungeon_snapshot.cursor_state.visible = visible ? 1 : 0;
    if (session->dungeon_snapshot.cursor_state.relative)
        invalidation_mask |= APP_SNAPSHOT_INVALIDATE_MAP;

    app_session_mark_snapshot_dirty(session, invalidation_mask);
    if (session->snapshot.scene == APP_SCENE_KIND_DUNGEON)
        (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
}

bool app_session_interactions_enabled(const app_session* session)
{
    return session && runtime_cli_snapshot_renderer()
        && (session->snapshot.scene == APP_SCENE_KIND_DUNGEON);
}

const app_interaction_state* app_session_interaction(
    const app_session* session)
{
    return session ? &session->interaction : NULL;
}

void app_session_clear_interaction(app_session* session)
{
    app_interaction_state cleared;

    if (!session)
        return;

    app_interaction_state_init(&cleared);
    if (memcmp(&session->interaction, &cleared, sizeof(cleared)) == 0)
        return;

    session->interaction = cleared;
    app_session_touch_interaction(session);
}

void app_session_begin_interaction(app_session* session, u16b kind,
    u16b reason, u16b flags)
{
    if (!session)
        return;

    app_interaction_state_init(&session->interaction);
    session->interaction.kind = kind;
    session->interaction.reason = reason;
    session->interaction.flags = flags;
    app_session_touch_interaction(session);
}

void app_session_set_interaction_prompt(app_session* session, byte attr,
    cptr prompt)
{
    if (!session)
        return;

    session->interaction.prompt_attr = attr;
    SDL_strlcpy(session->interaction.prompt, prompt ? prompt : "",
        sizeof(session->interaction.prompt));
    app_session_touch_interaction(session);
}

void app_session_set_interaction_detail(app_session* session, byte attr,
    cptr detail)
{
    if (!session)
        return;

    session->interaction.detail_attr = attr;
    SDL_strlcpy(session->interaction.detail, detail ? detail : "",
        sizeof(session->interaction.detail));
    app_session_touch_interaction(session);
}

void app_session_set_interaction_value(app_session* session, byte attr,
    cptr value, s16b cursor_index)
{
    if (!session)
        return;

    session->interaction.value_attr = attr;
    session->interaction.cursor_index = cursor_index;
    SDL_strlcpy(session->interaction.value, value ? value : "",
        sizeof(session->interaction.value));
    app_session_touch_interaction(session);
}

void app_session_clear_interaction_options(app_session* session)
{
    if (!session)
        return;

    memset(session->interaction.options, 0, sizeof(session->interaction.options));
    session->interaction.option_count = 0;
    session->interaction.selected_index = -1;
    app_session_touch_interaction(session);
}

void app_session_set_interaction_selected(app_session* session,
    s16b selected_index)
{
    size_t i;

    if (!session)
        return;

    session->interaction.selected_index = selected_index;
    for (i = 0; i < session->interaction.option_count; i++)
    {
        session->interaction.options[i].selected
            = (selected_index >= 0 && (size_t)selected_index == i) ? 1 : 0;
        if (session->interaction.options[i].selected)
            session->interaction.options[i].flags
                |= APP_INTERACTION_ENTRY_FLAG_SELECTED;
        else
            session->interaction.options[i].flags
                &= (byte)~APP_INTERACTION_ENTRY_FLAG_SELECTED;
    }
    app_session_touch_interaction(session);
}

bool app_session_add_interaction_option(app_session* session, byte attr,
    char tag, bool enabled, bool selected, cptr label, cptr meta)
{
    app_interaction_option* option;
    size_t index;

    if (!session
        || session->interaction.option_count >= APP_INTERACTION_OPTION_MAX)
    {
        return false;
    }

    index = session->interaction.option_count;
    option = app_interaction_append_entry(&session->interaction);
    if (!option)
        return false;

    option->attr = attr;
    option->tag = tag ? (byte)tag : 0;
    option->enabled = enabled ? 1 : 0;
    option->selected = selected ? 1 : 0;
    option->flags = APP_INTERACTION_ENTRY_FLAG_NONE;
    if (!enabled)
        option->flags |= APP_INTERACTION_ENTRY_FLAG_DISABLED;
    if (selected)
        option->flags |= APP_INTERACTION_ENTRY_FLAG_SELECTED;
    if (tag)
        strnfmt(option->key, sizeof(option->key), "%c", tag);
    SDL_strlcpy(option->label, label ? label : "", sizeof(option->label));
    SDL_strlcpy(option->meta, meta ? meta : "", sizeof(option->meta));

    if (selected)
        session->interaction.selected_index = (s16b)index;
    else if (session->interaction.selected_index == (s16b)index)
    {
        option->selected = 1;
        option->flags |= APP_INTERACTION_ENTRY_FLAG_SELECTED;
    }

    app_session_touch_interaction(session);
    return true;
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
