#include "angband.h"
#include "app/app-movement.h"
#include "app/app-session.h"
#include "log/log.h"
#include "platform-frame.h"
#include "ui/ui-information-scene.h"

struct inkey_state {
    bool cursor_hidden;
};

static struct inkey_state g_inkey_state = { 0 };

void input_byte_queue_clear(void);
void input_clear_movement_commands(void);

bool inkey_cursor_hidden(void)
{
    return g_inkey_state.cursor_hidden;
}

void inkey_set_cursor_hidden(bool hidden)
{
    if (g_inkey_state.cursor_hidden == hidden)
        return;

    g_inkey_state.cursor_hidden = hidden;
    app_session_set_cursor_visible(app_session_current(), !hidden);
}

/*
 * Clear all pending input immediately across the semantic session bridge,
 * the legacy byte queue, and deferred movement input.
 */
void input_clear_pending(void)
{
    app_session* session = app_session_current();

    if (session)
        app_session_clear_inputs(session);

    input_byte_queue_clear();
    input_clear_movement_commands();
    platform_frame_flush_events();
    input_clear_movement_commands();
}

enum {
    INPUT_BYTE_QUEUE_SIZE = 256
};

struct input_byte_queue_state {
    char queue[INPUT_BYTE_QUEUE_SIZE];
    u16b head;
    u16b tail;
};

static struct input_byte_queue_state g_input_byte_queue = { 0 };

enum {
    INPUT_MOVEMENT_QUEUE_SIZE = 32
};

struct input_movement_command_state {
    app_movement_command queued[INPUT_MOVEMENT_QUEUE_SIZE];
    u16b count;
};

static struct input_movement_command_state g_input_movement_commands = { 0 };

static bool input_movement_context_matches(u16b command_context,
    u16b requested_context)
{
    return requested_context == APP_MOVEMENT_CONTEXT_ANY
        || command_context == APP_MOVEMENT_CONTEXT_ANY
        || command_context == requested_context;
}

static bool input_take_queued_movement_command(u16b context,
    app_movement_command* out_command)
{
    u16b i;

    for (i = 0; i < g_input_movement_commands.count; i++)
    {
        if (!input_movement_context_matches(
                g_input_movement_commands.queued[i].context, context))
        {
            continue;
        }

        if (out_command)
            *out_command = g_input_movement_commands.queued[i];

        if (i + 1 < g_input_movement_commands.count)
        {
            memmove(&g_input_movement_commands.queued[i],
                &g_input_movement_commands.queued[i + 1],
                (g_input_movement_commands.count - (i + 1))
                    * sizeof(g_input_movement_commands.queued[0]));
        }
        g_input_movement_commands.count--;
        return true;
    }

    return false;
}

bool input_submit_movement_command(const app_movement_command* command)
{
    if (!app_movement_command_is_valid(command))
        return false;
    if (g_input_movement_commands.count >= INPUT_MOVEMENT_QUEUE_SIZE)
        return false;

    g_input_movement_commands
        .queued[g_input_movement_commands.count++] = *command;
    return true;
}

void input_clear_movement_commands(void)
{
    g_input_movement_commands.count = 0;
}

errr input_byte_unshift(int key)
{
    if (!key)
        return -1;

    if (g_input_byte_queue.tail == 0)
        g_input_byte_queue.tail = INPUT_BYTE_QUEUE_SIZE;

    g_input_byte_queue.queue[--g_input_byte_queue.tail] = (char)key;
    if (g_input_byte_queue.head != g_input_byte_queue.tail)
        return 0;

    return 1;
}

errr input_byte_enqueue(int key)
{
    if (!key)
        return -1;

    g_input_byte_queue.queue[g_input_byte_queue.head++] = (char)key;
    if (g_input_byte_queue.head == INPUT_BYTE_QUEUE_SIZE)
        g_input_byte_queue.head = 0;
    if (g_input_byte_queue.head != g_input_byte_queue.tail)
        return 0;

    return 1;
}

void input_byte_queue_clear(void)
{
    g_input_byte_queue.head = 0;
    g_input_byte_queue.tail = 0;
}

bool input_byte_queue_pending(void)
{
    return g_input_byte_queue.head != g_input_byte_queue.tail;
}

static errr input_byte_read(char* ch, bool take)
{
    if (!ch)
        return -1;
    if (!input_byte_queue_pending())
        return 1;

    *ch = g_input_byte_queue.queue[g_input_byte_queue.tail];
    if (take && (++g_input_byte_queue.tail == INPUT_BYTE_QUEUE_SIZE))
        g_input_byte_queue.tail = 0;
    return 0;
}

static bool input_take_queued_legacy_key(char* out_ch)
{
    app_session* session = app_session_current();
    app_input input;
    char ch = '\0';

    if (session)
    {
        while (app_session_peek_input(session, &input))
        {
            if (input.layer != APP_INPUT_LAYER_LEGACY
                || input.type != APP_INPUT_TYPE_KEY)
            {
                app_input discarded;

                (void)app_session_pop_input(session, &discarded);
                continue;
            }

            ch = (char)(input.payload.key.logical_key & 0xFFu);
            (void)app_session_pop_input(session, &input);
            if (out_ch)
                *out_ch = ch;
            return true;
        }
    }

    if (input_byte_read(&ch, true) == 0)
    {
        if (out_ch)
            *out_ch = ch;
        return true;
    }

    return false;
}

static bool inkey_information_scene_candidate(const app_input* input)
{
    return input && input->layer == APP_INPUT_LAYER_LEGACY
        && input->type == APP_INPUT_TYPE_KEY;
}

static errr inkey_information_scene(char* ch, bool take)
{
    app_session* session = app_session_current();
    app_input input;

    if (!ch || !ui_information_scene_owns_input() || !session)
        return -1;

    while (app_session_peek_input(session, &input))
    {
        if (!inkey_information_scene_candidate(&input))
        {
            app_input discarded;

            (void)app_session_pop_input(session, &discarded);
            continue;
        }

        *ch = (char)(input.payload.key.logical_key & 0xFFu);
        if (*ch == ESCAPE) {
            log_debug("[metarun-esc-trace] inkey_information_scene esc take=%d flags=0x%04x",
                take ? 1 : 0, (unsigned)input.flags);
        }
        if (take)
        {
            app_input consumed;

            (void)app_session_pop_input(session, &consumed);
        }
        return 0;
    }

    return 1;
}

static errr input_read_legacy_key(char* ch, bool wait, bool take)
{
    while (true)
    {
        if (input_byte_read(ch, take) == 0)
            return 0;

        if (inkey_information_scene(ch, take) == 0)
            return 0;

        if (!wait)
            return 1;

        platform_frame_process_events(true);
    }
}

bool inkey_can_consume_immediately(void)
{
    char ch;

    return (input_read_legacy_key(&ch, false, false) == 0);
}

bool input_wait_for_movement_or_legacy(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch)
{
    app_wait_scope scope;
    bool scope_active = false;

    if (out_command)
        app_movement_command_clear(out_command);
    if (out_ch)
        *out_ch = '\0';

    if (wait_reason != APP_WAIT_REASON_NONE)
    {
        app_session_push_wait_scope(app_session_current(), &scope, wait_reason,
            0, 0);
        scope_active = scope.active;
    }

    while (true)
    {
        if (input_take_queued_movement_command(context, out_command))
        {
            if (scope_active)
                app_session_pop_wait_scope(app_session_current(), &scope);
            return true;
        }

        if (input_take_queued_legacy_key(out_ch))
        {
            if (out_command)
                app_movement_command_clear(out_command);
            if (scope_active)
                app_session_pop_wait_scope(app_session_current(), &scope);
            return true;
        }

        platform_frame_process_events(true);
    }
}
