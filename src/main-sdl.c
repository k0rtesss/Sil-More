/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "app/app-movement.h"
#include "fs/resource.h"
#include "sdl-main-internal.h"
#include "ui/ui-information-scene.h"

struct sound_config g_sound_config;
char config_file_path[1024];

_Static_assert((int)GAMEPAD_BUTTON_COUNT == (int)SDL_GAMEPAD_BUTTON_COUNT,
    "gamepad button ids must stay aligned with SDL");
_Static_assert((int)GAMEPAD_BUTTON_SOUTH == (int)SDL_GAMEPAD_BUTTON_SOUTH,
    "south button id drifted from SDL");
_Static_assert((int)GAMEPAD_BUTTON_LEFT_PADDLE1 == (int)SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    "left paddle button id drifted from SDL");
_Static_assert((int)GAMEPAD_BUTTON_TOUCHPAD == (int)SDL_GAMEPAD_BUTTON_TOUCHPAD,
    "touchpad button id drifted from SDL");

typedef struct gamepad_entry {
    SDL_JoystickID id;
    SDL_Gamepad* pad;
} gamepad_entry;

typedef struct gamepad_input_state {
    gamepad_entry pads[MAX_GAMEPADS];
    int pad_count;
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    int dpad_dir;
    bool dpad_pending;
    int dpad_pending_dir;
    Uint64 dpad_pending_time;
    bool dpad_pending_shift;
    bool dpad_pending_ctrl;
    bool dpad_pending_alt;
    Sint16 left_x;
    Sint16 left_y;
    int left_dir;
    int left_bind_dir;
    bool left_pending;
    int left_pending_dir;
    Uint64 left_pending_time;
    bool left_pending_shift;
    bool left_pending_ctrl;
    bool left_pending_alt;
    Sint16 right_x;
    Sint16 right_y;
    int right_dir;
    bool left_shoulder_down;
    bool right_shoulder_down;
    bool shoulder_pending;
    int shoulder_pending_button;
    Uint64 shoulder_pending_time;
    bool left_trigger_down;
    bool right_trigger_down;
    int shift_held;
    int ctrl_held;
    int alt_held;
} gamepad_input_state;

static gamepad_input_state g_gamepad_state;
static bool g_gamepad_auto_ui = false;
static int g_default_gamepad_button_bindings[GAMEPAD_BUTTON_COUNT];
static int g_default_gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
static int g_default_gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_button_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_BUTTON_COUNT];
static int g_default_gamepad_trigger_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_TRIGGER_COUNT];
static int g_default_gamepad_left_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_right_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
static int g_default_gamepad_shoulder_combo_binding = GAMEPAD_BIND_NONE;
static bool g_default_gamepad_bindings_ready = false;
static bool g_gamepad_capture_active = false;
static bool g_gamepad_capture_ready = false;
static int g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
static int g_gamepad_capture_id = 0;
static u64b g_legacy_input_sequence = 0;

void sdl_handle_event(sdl_state* st, const SDL_Event* ev);
static void sdl_quit_hook(cptr str);
void sdl_gamepad_init(void);
void sdl_gamepad_shutdown(void);
static void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev);
static void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev);
static void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev);
static void sdl_gamepad_open(SDL_JoystickID id);
static void sdl_gamepad_close(SDL_JoystickID id);
static void sdl_gamepad_mark_auto_ui(void);
static int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone);
static int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone);
static void sdl_dispatch_gamepad_direction(int dir, bool shift, bool ctrl,
    bool alt, u16b input_type);
void sdl_gamepad_send_key(int key, bool apply_modifiers);
static u16b sdl_input_modifiers_from_keymod(SDL_Keymod mod);
static bool sdl_build_movement_command_from_input(u16b context, u16b action,
    u16b direction, const app_input* input,
    app_movement_command* out_command);
static u16b sdl_current_movement_input_context(void);
static void sdl_init_keyboard_input_event(const SDL_KeyboardEvent* key_event,
    app_input* out_input);
static bool sdl_submit_movement_command(
    const app_movement_command* command);
static int sdl_key_to_legacy_keypad_dir(int key);
static char sdl_legacy_key_char_from_keyboard_event(
    const SDL_KeyboardEvent* key_event);
static bool sdl_keyboard_event_reserved_for_movement(
    const SDL_KeyboardEvent* key_event);
static bool sdl_try_submit_keyboard_movement_event(
    const SDL_KeyboardEvent* key_event);
static bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event);
void sdl_gamepad_apply_modifier(int binding, bool down);
bool sdl_gamepad_shift_active(void);
bool sdl_gamepad_ctrl_active(void);
bool sdl_gamepad_alt_active(void);
static int sdl_gamepad_modifier_index(int binding);
static int sdl_gamepad_single_active_modifier(void);
static int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id);
static void sdl_gamepad_clear_pending_dpad(void);
static void sdl_gamepad_set_pending_dpad(int dir);
bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_left_stick(void);
static void sdl_gamepad_set_pending_left_stick(int dir);
bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
static void sdl_gamepad_clear_pending_shoulder(void);
static void sdl_gamepad_set_pending_shoulder(int button);
bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
void sdl_gamepad_load_default_bindings(void);

bool steamdeck_controls_active(void);

static bool sdl_legacy_input_bridge_active(void)
{
    app_session* session = app_session_current();

    if (!session)
        return false;

    return app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT);
}

static bool sdl_session_input_capture_active(void)
{
    app_session* session = app_session_current();

    return app_session_input_capture_active(session);
}

static bool sdl_queue_legacy_input_byte_ex(int key, bool repeat)
{
    app_session* session;
    app_input input;

    if (!key)
        return true;

    session = app_session_current();
    if (!session || !app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
        return false;

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_LEGACY;
    input.type = APP_INPUT_TYPE_KEY;
    input.device = APP_INPUT_DEVICE_KEYBOARD;
    input.flags = APP_INPUT_FLAG_PRESS | APP_INPUT_FLAG_SYNTHETIC;
    if (repeat)
        input.flags |= APP_INPUT_FLAG_REPEAT;
    input.sequence = ++g_legacy_input_sequence;
    input.timestamp_usec = SDL_GetTicksNS() / 1000ULL;
    input.payload.key.logical_key = (u32b)(byte)key;
    input.payload.key.physical_key = (u32b)(byte)key;
    input.payload.key.repeat_count = repeat ? 1 : 0;

    return app_session_submit_input(session, &input);
}

static bool sdl_queue_legacy_input_byte(int key)
{
    return sdl_queue_legacy_input_byte_ex(key, false);
}

void sdl_submit_legacy_input_byte(int key)
{
    if (sdl_queue_legacy_input_byte(key))
        return;

    (void)input_byte_enqueue(key);
}

void sdl_drain_legacy_input_queue(void)
{
    app_session* session;
    app_input input;

    if (!sdl_legacy_input_bridge_active())
        return;
    if (sdl_session_input_capture_active())
        return;

    session = app_session_current();
    if (!session)
        return;

    while (app_session_pop_input(session, &input)) {
        if (input.layer != APP_INPUT_LAYER_LEGACY || input.type != APP_INPUT_TYPE_KEY)
            continue;

        if ((int)(input.payload.key.logical_key & 0xFFu) == ESCAPE)
        {
            log_debug("[metarun-esc-trace] sdl_drain_legacy_input_queue esc flags=0x%04x seq=%u",
                (unsigned)input.flags, (unsigned)input.sequence);
        }

        (void)input_byte_enqueue((int)(input.payload.key.logical_key & 0xFFu));
    }
}

void sdl_clear_legacy_input_queue(void)
{
    app_session* session = app_session_current();

    if (session)
        app_session_clear_inputs(session);
    input_byte_queue_clear();
}

bool sdl_gamepad_shift_active(void)
{
    return g_gamepad_state.shift_held > 0;
}

bool sdl_gamepad_ctrl_active(void)
{
    return g_gamepad_state.ctrl_held > 0;
}

bool sdl_gamepad_alt_active(void)
{
    return g_gamepad_state.alt_held > 0;
}

static int sdl_gamepad_modifier_index(int binding)
{
    switch (binding) {
    case GAMEPAD_BIND_SHIFT:
        return GAMEPAD_MODIFIER_SHIFT;
    case GAMEPAD_BIND_CTRL:
        return GAMEPAD_MODIFIER_CTRL;
    case GAMEPAD_BIND_ALT:
        return GAMEPAD_MODIFIER_ALT;
    default:
        return -1;
    }
}

static int sdl_gamepad_single_active_modifier(void)
{
    int active = GAMEPAD_BIND_NONE;

    if (sdl_gamepad_shift_active())
        active = GAMEPAD_BIND_SHIFT;
    if (sdl_gamepad_ctrl_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_CTRL;
    }
    if (sdl_gamepad_alt_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_ALT;
    }

    return active;
}

static int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return GAMEPAD_BIND_NONE;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < GAMEPAD_BUTTON_COUNT)
            return config.gamepad_button_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return config.gamepad_trigger_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_left_stick_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_right_stick_combo_bindings[modifier_index][id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

static void sdl_gamepad_mark_auto_ui(void)
{
    if (config.gamepad_auto_mode)
        g_gamepad_auto_ui = true;
}

void sdl_gamepad_apply_modifier(int binding, bool down)
{
    int delta = down ? 1 : -1;

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_gamepad_state.shift_held += delta;
        if (g_gamepad_state.shift_held < 0)
            g_gamepad_state.shift_held = 0;
    } else if (binding == GAMEPAD_BIND_CTRL) {
        g_gamepad_state.ctrl_held += delta;
        if (g_gamepad_state.ctrl_held < 0)
            g_gamepad_state.ctrl_held = 0;
    } else if (binding == GAMEPAD_BIND_ALT) {
        g_gamepad_state.alt_held += delta;
        if (g_gamepad_state.alt_held < 0)
            g_gamepad_state.alt_held = 0;
    }
}

static u16b sdl_input_modifiers_from_keymod(SDL_Keymod mod)
{
    u16b modifiers = 0;

    if (mod & SDL_KMOD_SHIFT)
        modifiers |= APP_INPUT_MODIFIER_SHIFT;
    if (mod & SDL_KMOD_CTRL)
        modifiers |= APP_INPUT_MODIFIER_CTRL;
    if (mod & SDL_KMOD_ALT)
        modifiers |= APP_INPUT_MODIFIER_ALT;
    if (mod & SDL_KMOD_GUI)
        modifiers |= APP_INPUT_MODIFIER_META;
#ifdef SDL_KMOD_CAPS
    if (mod & SDL_KMOD_CAPS)
        modifiers |= APP_INPUT_MODIFIER_CAPS_LOCK;
#endif
#ifdef SDL_KMOD_NUM
    if (mod & SDL_KMOD_NUM)
        modifiers |= APP_INPUT_MODIFIER_NUM_LOCK;
#endif

    return modifiers;
}

static bool sdl_build_movement_command_from_input(u16b context, u16b action,
    u16b direction, const app_input* input,
    app_movement_command* out_command)
{
    if (!input || !out_command)
        return false;

    app_movement_command_clear(out_command);
    out_command->context = context;
    out_command->action = action;
    out_command->modifiers = input->modifiers;
    out_command->device = input->device;
    out_command->input_type = input->type;
    out_command->source_id = input->source_id;
    out_command->sequence = input->sequence;
    out_command->timestamp_usec = input->timestamp_usec;

    switch (input->type)
    {
    case APP_INPUT_TYPE_KEY:
        out_command->trigger = input->payload.key.physical_key
            ? input->payload.key.physical_key
            : input->payload.key.logical_key;
        out_command->trigger_aux = input->payload.key.logical_key;
        break;

    case APP_INPUT_TYPE_GAMEPAD_BUTTON:
    case APP_INPUT_TYPE_GAMEPAD_AXIS:
        out_command->trigger = input->payload.gamepad.control;
        out_command->trigger_aux = input->payload.gamepad.secondary_control;
        break;

    case APP_INPUT_TYPE_POINTER_BUTTON:
        out_command->trigger = input->payload.pointer.button;
        out_command->trigger_aux = input->payload.pointer.clicks;
        break;

    default:
        out_command->trigger = 0;
        out_command->trigger_aux = 0;
        break;
    }

    if (input->flags & APP_INPUT_FLAG_REPEAT)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_REPEAT;
    if (input->flags & APP_INPUT_FLAG_SYNTHETIC)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC;

    if (!app_movement_direction_payload_from_direction(direction,
            &out_command->direction))
    {
        return false;
    }

    return app_movement_command_is_valid(out_command);
}

static u16b sdl_current_movement_input_context(void)
{
    app_session* session = app_session_current();
    const app_wait_state* wait_state = session
        ? app_session_wait_state(session)
        : NULL;

    if (wait_state && wait_state->reason == APP_WAIT_REASON_TARGETING)
    {
        return APP_MOVEMENT_CONTEXT_ANY;
    }

    return APP_MOVEMENT_CONTEXT_DUNGEON;
}

u16b sdl_movement_input_modifiers_from_keyboard_event(
    const SDL_KeyboardEvent* key_event)
{
    SDL_Keymod mod = SDL_KMOD_NONE;

    if (key_event)
        mod = key_event->mod;

    mod |= SDL_GetModState() & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL
        | SDL_KMOD_ALT | SDL_KMOD_GUI);

    return sdl_input_modifiers_from_keymod(mod);
}

static void sdl_init_keyboard_input_event(const SDL_KeyboardEvent* key_event,
    app_input* out_input)
{
    if (!out_input)
        return;

    memset(out_input, 0, sizeof(*out_input));
    if (!key_event)
        return;

    out_input->layer = APP_INPUT_LAYER_INTENT;
    out_input->type = APP_INPUT_TYPE_KEY;
    out_input->device = APP_INPUT_DEVICE_KEYBOARD;
    out_input->modifiers
        = sdl_movement_input_modifiers_from_keyboard_event(key_event);
    out_input->flags = APP_INPUT_FLAG_PRESS;
    if (key_event->repeat)
        out_input->flags |= APP_INPUT_FLAG_REPEAT;
    out_input->sequence = ++g_legacy_input_sequence;
    out_input->timestamp_usec = SDL_GetTicksNS() / 1000ULL;
    out_input->payload.key.logical_key = (u32b)key_event->key;
    out_input->payload.key.physical_key = (u32b)key_event->scancode;
    out_input->payload.key.repeat_count = key_event->repeat ? 1 : 0;
}

static bool sdl_submit_movement_command(
    const app_movement_command* command)
{
    if (!command || !app_movement_command_is_valid(command))
        return false;

    return input_submit_movement_command(command);
}

bool platform_submit_directional_movement(int dir, bool shift, bool ctrl, bool alt,
    u16b device, u16b input_type, u16b source_id, u16b input_flags,
    u32b trigger, u32b trigger_aux)
{
    app_input input;
    app_movement_command command;
    u16b context;
    u16b action;
    u16b direction;
    u16b modifiers = 0;
    int modifier_count = (shift ? 1 : 0) + (ctrl ? 1 : 0) + (alt ? 1 : 0);

    if (dir < 1 || dir > 9)
        return false;
    if (modifier_count > 1)
        return false;

    if (shift)
        modifiers |= APP_INPUT_MODIFIER_SHIFT;
    if (ctrl)
        modifiers |= APP_INPUT_MODIFIER_CTRL;
    if (alt)
        modifiers |= APP_INPUT_MODIFIER_ALT;

    if (alt) {
        return false;
    }

    if (!app_movement_direction_from_legacy_keypad(dir, &direction))
        return false;

    if (shift)
        action = (dir == 5) ? APP_MOVEMENT_ACTION_REST
            : APP_MOVEMENT_ACTION_RUN_DIR;
    else if (ctrl)
        action = APP_MOVEMENT_ACTION_INTERACT_DIR;
    else
        action = (dir == 5) ? APP_MOVEMENT_ACTION_WAIT
            : APP_MOVEMENT_ACTION_MOVE_DIR;

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_INTENT;
    input.type = input_type;
    input.device = device;
    input.modifiers = modifiers;
    input.flags = input_flags;
    input.source_id = source_id;
    input.sequence = ++g_legacy_input_sequence;
    input.timestamp_usec = SDL_GetTicksNS() / 1000ULL;

    switch (input_type)
    {
    case APP_INPUT_TYPE_KEY:
        input.payload.key.logical_key = trigger_aux;
        input.payload.key.physical_key = trigger ? trigger : trigger_aux;
        input.payload.key.repeat_count
            = (input_flags & APP_INPUT_FLAG_REPEAT) ? 1 : 0;
        break;

    case APP_INPUT_TYPE_GAMEPAD_BUTTON:
    case APP_INPUT_TYPE_GAMEPAD_AXIS:
        input.payload.gamepad.control = (u16b)trigger;
        input.payload.gamepad.secondary_control = (u16b)trigger_aux;
        break;

    case APP_INPUT_TYPE_POINTER_BUTTON:
        input.payload.pointer.button = (u16b)trigger;
        input.payload.pointer.clicks = (u16b)trigger_aux;
        break;

    default:
        break;
    }

    context = sdl_current_movement_input_context();
    if (!sdl_build_movement_command_from_input(context, action, direction,
            &input, &command))
    {
        return false;
    }

    return sdl_submit_movement_command(&command);
}

static int sdl_key_to_legacy_keypad_dir(int key)
{
    switch (key) {
    case SDLK_UP:
    case SDLK_KP_8:
        return 8;

    case SDLK_DOWN:
    case SDLK_KP_2:
        return 2;

    case SDLK_LEFT:
    case SDLK_KP_4:
        return 4;

    case SDLK_RIGHT:
    case SDLK_KP_6:
        return 6;

    case SDLK_KP_1:
    case SDLK_END:
        return 1;

    case SDLK_KP_3:
    case SDLK_PAGEDOWN:
        return 3;

    case SDLK_KP_7:
    case SDLK_HOME:
        return 7;

    case SDLK_KP_9:
    case SDLK_PAGEUP:
        return 9;

    case SDLK_KP_5:
        return 5;

    default:
        return 0;
    }
}

static bool sdl_translate_legacy_input_key(int key, bool shift, bool ctrl,
    bool alt, bool gui, bool apply_modifiers, int* out_key)
{
    static const char shifted[256] = {
        ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$', ['5'] = '%',
        ['6'] = '^', ['7'] = '&', ['8'] = '*', ['9'] = '(', ['0'] = ')',
        ['-'] = '_', ['='] = '+',
        [','] = '<', ['.'] = '>', ['/'] = '?',
        ['['] = '{', [']'] = '}',
        [';'] = ':', ['\''] = '"', ['\\'] = '|',
        ['`'] = '~',
    };
    int dir = sdl_key_to_legacy_keypad_dir(key);
    int translated = dir ? ('0' + dir) : key;

    if (!translated)
        return false;

    if (!apply_modifiers)
    {
        if (out_key)
            *out_key = translated;
        return true;
    }

    if (dir && (shift || ctrl || alt || gui))
        return false;

    if (ctrl && !alt && !gui && SDL_isalpha(translated))
    {
        translated = KTRL(translated);
    }
    else if (ctrl || alt || gui)
    {
        return false;
    }
    else if (shift)
    {
        if (SDL_isalpha(translated))
        {
            translated = SDL_toupper(translated);
        }
        else if (translated > 0 && translated < 256 && shifted[translated])
        {
            translated = shifted[translated];
        }
    }

    if (out_key)
        *out_key = translated;
    return true;
}

static char sdl_legacy_key_char_from_keyboard_event(
    const SDL_KeyboardEvent* key_event)
{
    bool shift;
    bool ctrl;
    bool alt;
    bool gui;
    int translated = 0;

    if (!key_event)
        return 0;

    shift = (key_event->mod & SDL_KMOD_SHIFT) != 0;
    ctrl = (key_event->mod & SDL_KMOD_CTRL) != 0;
    alt = (key_event->mod & SDL_KMOD_ALT) != 0;
    gui = (key_event->mod & SDL_KMOD_GUI) != 0;
    if (!sdl_translate_legacy_input_key(key_event->key, shift, ctrl, alt, gui,
            true, &translated))
    {
        return 0;
    }

    if (translated <= 0 || translated >= 256)
        return 0;

    return (char)translated;
}

static bool sdl_keyboard_event_reserved_for_movement(
    const SDL_KeyboardEvent* key_event)
{
    char ch;

    if (!key_event)
        return false;

    if (sdl_key_to_legacy_keypad_dir(key_event->key))
        return true;

    ch = sdl_legacy_key_char_from_keyboard_event(key_event);
    return ch == ';' || ch == 'z' || ch == 'Z' || ch == '.'
        || ch == '/';
}

static bool sdl_try_submit_keyboard_movement_event(
    const SDL_KeyboardEvent* key_event)
{
    app_input input;
    app_movement_command command;
    u16b context;

    if (!key_event || !character_dungeon || sdl_session_input_capture_active())
        return false;

    if (!sdl_config_has_movement_bindings(&config))
        return false;

    context = sdl_current_movement_input_context();
    sdl_init_keyboard_input_event(key_event, &input);
    app_movement_command_clear(&command);
    if (app_movement_resolve_input(config.movement_bindings,
            config.movement_binding_count, &input, context, &command))
    {
        return sdl_submit_movement_command(&command);
    }

    /* Once movement bindings exist, movement keys should not fall through to
     * the legacy command-byte path and bypass the configured bindings.
     */
    return sdl_keyboard_event_reserved_for_movement(key_event);
}

static bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event)
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT))
        return false;

    key = key_event->key;

    if (key == '+' || key == '=' || key == SDLK_KP_PLUS) {
        int current_scale = platform_main_view_scale();
        int max_scale = platform_max_scale();

        if (current_scale < max_scale) {
            platform_set_main_view_scale(current_scale + 1);
            platform_apply_config();
            if (character_dungeon)
                sdl_submit_legacy_input_byte(KTRL('R'));
        }
        return true;
    }

    if (key == '-' || key == SDLK_KP_MINUS) {
        int current_scale = platform_main_view_scale();

        if (current_scale > 1) {
            platform_set_main_view_scale(current_scale - 1);
            platform_apply_config();
            if (character_dungeon)
                sdl_submit_legacy_input_byte(KTRL('R'));
        }
        return true;
    }

    if (key == 'i' || key == 'I') {
        bool enabled = platform_enable_right_panes();

        platform_set_enable_right_panes(!enabled);
        platform_apply_config();
        if (character_dungeon)
            sdl_submit_legacy_input_byte(KTRL('R'));
        return true;
    }

    if (key == 'l' || key == 'L') {
        bool enabled = platform_enable_bottom_panes();

        platform_set_enable_bottom_panes(!enabled);
        platform_apply_config();
        if (character_dungeon)
            sdl_submit_legacy_input_byte(KTRL('R'));
        return true;
    }

    if (key == 'p' || key == 'P') {
        bool hidden = platform_hide_left_panel();

        platform_set_hide_left_panel(!hidden);
        platform_apply_config();
        if (character_dungeon)
            sdl_submit_legacy_input_byte(KTRL('R'));
        return true;
    }

    return false;
}

void sdl_gamepad_send_key(int key, bool apply_modifiers)
{
    bool shift = sdl_gamepad_shift_active();
    bool ctrl = sdl_gamepad_ctrl_active();
    bool alt = sdl_gamepad_alt_active();
    int translated = 0;

    if (!sdl_translate_legacy_input_key(key, shift, ctrl, alt, false,
            apply_modifiers, &translated))
    {
        return;
    }

    sdl_submit_legacy_input_byte(translated);
}

static void sdl_gamepad_send_shoulder_combo(void)
{
    int binding = config.gamepad_shoulder_combo_binding;
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
        sdl_gamepad_apply_modifier(binding, false);
        return;
    }

    sdl_gamepad_send_key(binding, false);
}

static int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone)
{
    int dx = 0;
    int dy = 0;

    if (x > deadzone)
        dx = 1;
    else if (x < -deadzone)
        dx = -1;

    if (y > deadzone)
        dy = 1;
    else if (y < -deadzone)
        dy = -1;

    if (dx == 0 && dy == 0)
        return 0;

    if (dy < 0) {
        if (dx < 0) return 7;
        if (dx > 0) return 9;
        return 8;
    }
    if (dy > 0) {
        if (dx < 0) return 1;
        if (dx > 0) return 3;
        return 2;
    }
    if (dx < 0) return 4;
    if (dx > 0) return 6;
    return 0;
}

static int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone)
{
    int abs_x = abs(x);
    int abs_y = abs(y);

    if (abs_x < deadzone && abs_y < deadzone)
        return -1;

    if (abs_x >= abs_y) {
        return (x >= 0) ? GAMEPAD_STICK_DIR_RIGHT : GAMEPAD_STICK_DIR_LEFT;
    }

    return (y >= 0) ? GAMEPAD_STICK_DIR_DOWN : GAMEPAD_STICK_DIR_UP;
}

static void sdl_dispatch_gamepad_direction(int dir, bool shift, bool ctrl,
    bool alt, u16b input_type)
{
    (void)platform_submit_directional_movement(dir, shift, ctrl, alt,
        APP_INPUT_DEVICE_GAMEPAD, input_type, 0, APP_INPUT_FLAG_PRESS,
        (u32b)dir, 0);
}

static void sdl_gamepad_clear_pending_dpad(void)
{
    g_gamepad_state.dpad_pending = false;
    g_gamepad_state.dpad_pending_dir = 0;
    g_gamepad_state.dpad_pending_time = 0;
    g_gamepad_state.dpad_pending_shift = false;
    g_gamepad_state.dpad_pending_ctrl = false;
    g_gamepad_state.dpad_pending_alt = false;
}

static void sdl_gamepad_set_pending_dpad(int dir)
{
    g_gamepad_state.dpad_pending = true;
    g_gamepad_state.dpad_pending_dir = dir;
    g_gamepad_state.dpad_pending_time = SDL_GetTicksNS();
    g_gamepad_state.dpad_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.dpad_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.dpad_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.dpad_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_dpad) {
        sdl_gamepad_clear_pending_dpad();
        return false;
    }

    Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.dpad_pending_time < window_ns)
        return false;

    sdl_dispatch_gamepad_direction(g_gamepad_state.dpad_pending_dir,
        g_gamepad_state.dpad_pending_shift, g_gamepad_state.dpad_pending_ctrl,
        g_gamepad_state.dpad_pending_alt, APP_INPUT_TYPE_GAMEPAD_BUTTON);
    sdl_gamepad_clear_pending_dpad();
    return true;
}

static void sdl_gamepad_clear_pending_left_stick(void)
{
    g_gamepad_state.left_pending = false;
    g_gamepad_state.left_pending_dir = 0;
    g_gamepad_state.left_pending_time = 0;
    g_gamepad_state.left_pending_shift = false;
    g_gamepad_state.left_pending_ctrl = false;
    g_gamepad_state.left_pending_alt = false;
}

static void sdl_gamepad_set_pending_left_stick(int dir)
{
    g_gamepad_state.left_pending = true;
    g_gamepad_state.left_pending_dir = dir;
    g_gamepad_state.left_pending_time = SDL_GetTicksNS();
    g_gamepad_state.left_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.left_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.left_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.left_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_left_stick) {
        sdl_gamepad_clear_pending_left_stick();
        return false;
    }

    Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.left_pending_time < window_ns)
        return false;

    sdl_dispatch_gamepad_direction(g_gamepad_state.left_pending_dir,
        g_gamepad_state.left_pending_shift, g_gamepad_state.left_pending_ctrl,
        g_gamepad_state.left_pending_alt, APP_INPUT_TYPE_GAMEPAD_AXIS);
    sdl_gamepad_clear_pending_left_stick();
    return true;
}

static void sdl_gamepad_clear_pending_shoulder(void)
{
    g_gamepad_state.shoulder_pending = false;
    g_gamepad_state.shoulder_pending_button = 0;
    g_gamepad_state.shoulder_pending_time = 0;
}

static void sdl_gamepad_set_pending_shoulder(int button)
{
    g_gamepad_state.shoulder_pending = true;
    g_gamepad_state.shoulder_pending_button = button;
    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
}

bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.shoulder_pending)
        return false;
    if (!config.gamepad_enabled) {
        sdl_gamepad_clear_pending_shoulder();
        return false;
    }

    Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.shoulder_pending_time < window_ns)
        return false;

    int button = g_gamepad_state.shoulder_pending_button;
    sdl_gamepad_clear_pending_shoulder();

    if (button < 0 || button >= GAMEPAD_BUTTON_COUNT)
        return true;

    int binding = config.gamepad_button_bindings[button];
    if (binding == GAMEPAD_BIND_NONE)
        return true;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
    } else {
        sdl_gamepad_send_key(binding, false);
    }

    return true;
}

int sdl_gamepad_pending_timeout_ms(Uint64 now_ns)
{
    int dpad_timeout = -1;
    int left_timeout = -1;
    int shoulder_timeout = -1;

    if (g_gamepad_state.dpad_pending && config.gamepad_enabled && config.gamepad_use_dpad) {
        Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.dpad_pending_time;
        if (elapsed >= window_ns) {
            dpad_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            dpad_timeout = (int)(remaining_ns / 1000000ULL);
            if (dpad_timeout < 1)
                dpad_timeout = 1;
        }
    }

    if (g_gamepad_state.left_pending && config.gamepad_enabled && config.gamepad_use_left_stick) {
        Uint64 window_ns = (Uint64)DPAD_DIAGONAL_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.left_pending_time;
        if (elapsed >= window_ns) {
            left_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            left_timeout = (int)(remaining_ns / 1000000ULL);
            if (left_timeout < 1)
                left_timeout = 1;
        }
    }

    if (g_gamepad_state.shoulder_pending && config.gamepad_enabled && steamdeck_controls_active()) {
        Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.shoulder_pending_time;
        if (elapsed >= window_ns) {
            shoulder_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            shoulder_timeout = (int)(remaining_ns / 1000000ULL);
            if (shoulder_timeout < 1)
                shoulder_timeout = 1;
        }
    }

    if (dpad_timeout < 0 && left_timeout < 0)
        return shoulder_timeout;
    if (dpad_timeout < 0 && shoulder_timeout < 0)
        return left_timeout;
    if (left_timeout < 0 && shoulder_timeout < 0)
        return dpad_timeout;
    if (dpad_timeout < 0)
        return (left_timeout < shoulder_timeout) ? left_timeout : shoulder_timeout;
    if (left_timeout < 0)
        return (dpad_timeout < shoulder_timeout) ? dpad_timeout : shoulder_timeout;
    if (shoulder_timeout < 0)
        return (dpad_timeout < left_timeout) ? dpad_timeout : left_timeout;
    {
        int min = dpad_timeout;
        if (left_timeout < min)
            min = left_timeout;
        if (shoulder_timeout < min)
            min = shoulder_timeout;
        return min;
    }
}

static void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev)
{
    if (!ev)
        return;

    SDL_GamepadButton button = (SDL_GamepadButton)ev->button;
    bool down = ev->down;

    if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
        g_gamepad_state.left_shoulder_down = down;
    } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
        g_gamepad_state.right_shoulder_down = down;
    }

    if (g_gamepad_capture_active) {
        bool shoulder_button = (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
            || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if (shoulder_button) {
            if (down) {
                if (g_gamepad_state.shoulder_pending &&
                    g_gamepad_state.shoulder_pending_button != (int)button) {
                    sdl_gamepad_clear_pending_shoulder();
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
                    g_gamepad_capture_id = 0;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                } else {
                    g_gamepad_state.shoulder_pending = true;
                    g_gamepad_state.shoulder_pending_button = (int)button;
                    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
                }
            } else if (g_gamepad_state.shoulder_pending &&
                       g_gamepad_state.shoulder_pending_button == (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
                g_gamepad_capture_id = (int)button;
                g_gamepad_capture_ready = true;
                g_gamepad_capture_active = false;
            }
            return;
        }

        if (down) {
            bool dpad_button = (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
                || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            if (!dpad_button || !config.gamepad_use_dpad) {
                if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_BUTTON;
                    g_gamepad_capture_id = (int)button;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (config.gamepad_use_dpad &&
        (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
            button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
    {
        switch (button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: g_gamepad_state.dpad_up = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: g_gamepad_state.dpad_down = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: g_gamepad_state.dpad_left = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: g_gamepad_state.dpad_right = down; break;
            default: break;
        }

        int dx = 0;
        int dy = 0;
        if (g_gamepad_state.dpad_left) dx--;
        if (g_gamepad_state.dpad_right) dx++;
        if (g_gamepad_state.dpad_up) dy--;
        if (g_gamepad_state.dpad_down) dy++;

        int dir = 0;
        bool diagonal = false;
        if (dx || dy) {
            if (dy < 0) {
                dir = (dx < 0) ? 7 : (dx > 0) ? 9 : 8;
            } else if (dy > 0) {
                dir = (dx < 0) ? 1 : (dx > 0) ? 3 : 2;
            } else {
                dir = (dx < 0) ? 4 : (dx > 0) ? 6 : 0;
            }
            diagonal = (dx != 0 && dy != 0);
        }

        if (dir != g_gamepad_state.dpad_dir) {
            g_gamepad_state.dpad_dir = dir;

            if (!down)
                return;

            if (dir == 0) {
                /* Keep pending to allow quick taps to resolve. */
            } else if (diagonal) {
                sdl_gamepad_clear_pending_dpad();
                sdl_dispatch_gamepad_direction(dir,
                    sdl_gamepad_shift_active(), sdl_gamepad_ctrl_active(),
                    sdl_gamepad_alt_active(), APP_INPUT_TYPE_GAMEPAD_BUTTON);
            } else {
                if (g_gamepad_state.dpad_pending)
                    sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true);
                sdl_gamepad_set_pending_dpad(dir);
            }
        }
        return;
    }

    if (steamdeck_controls_active() &&
        (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
    {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (down && active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key(combo_binding, false);
                return;
            }
        }

        if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
            int binding = config.gamepad_button_bindings[button];
            if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                sdl_gamepad_apply_modifier(binding, down);
                return;
            }
        }

        if (down) {
            if (g_gamepad_state.shoulder_pending &&
                g_gamepad_state.shoulder_pending_button != (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                sdl_gamepad_send_shoulder_combo();
            } else {
                sdl_gamepad_set_pending_shoulder((int)button);
            }
        } else if (g_gamepad_state.shoulder_pending &&
                   g_gamepad_state.shoulder_pending_button == (int)button) {
            sdl_gamepad_flush_pending_shoulder(SDL_GetTicksNS(), true);
        }
        return;
    }

    if ((int)button < 0 || (int)button >= GAMEPAD_BUTTON_COUNT)
        return;

    if (down) {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key(combo_binding, false);
                return;
            }
        }
    }

    int binding = config.gamepad_button_bindings[button];
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, down);
        return;
    }

    if (down)
        sdl_gamepad_send_key(binding, false);
}

static void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev)
{
    if (!ev)
        return;

    if (g_gamepad_capture_active) {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
                g_gamepad_state.left_x = ev->value;
            else
                g_gamepad_state.left_y = ev->value;

            if (!config.gamepad_use_left_stick) {
                int deadzone = config.gamepad_deadzone;
                if (deadzone < 0)
                    deadzone = 0;
                int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
                if (dir >= 0) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_LEFT_STICK;
                    g_gamepad_capture_id = dir;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
                g_gamepad_state.right_x = ev->value;
            else
                g_gamepad_state.right_y = ev->value;

            int deadzone = config.gamepad_deadzone;
            if (deadzone < 0)
                deadzone = 0;
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
            if (dir >= 0) {
                g_gamepad_capture_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                g_gamepad_capture_id = dir;
                g_gamepad_capture_ready = true;
                g_gamepad_capture_active = false;
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            int threshold = config.gamepad_trigger_threshold;
            if (threshold < 0)
                threshold = 0;
            bool pressed = (ev->value >= threshold);

            if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                bool was_down = g_gamepad_state.left_trigger_down;
                g_gamepad_state.left_trigger_down = pressed;
                if (pressed && !was_down) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_TRIGGER;
                    g_gamepad_capture_id = 0;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            } else {
                bool was_down = g_gamepad_state.right_trigger_down;
                g_gamepad_state.right_trigger_down = pressed;
                if (pressed && !was_down) {
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_TRIGGER;
                    g_gamepad_capture_id = 1;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
            g_gamepad_state.left_x = ev->value;
        else
            g_gamepad_state.left_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;

        if (config.gamepad_use_left_stick) {
            int dir = sdl_gamepad_axis_to_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_dir;
            if (dir != prev_dir) {
                g_gamepad_state.left_dir = dir;
                if (dir == 0) {
                    /* Keep pending to allow quick taps to resolve. */
                } else if (dir == 1 || dir == 3 || dir == 7 || dir == 9) {
                    sdl_gamepad_clear_pending_left_stick();
                    sdl_dispatch_gamepad_direction(dir,
                        sdl_gamepad_shift_active(), sdl_gamepad_ctrl_active(),
                        sdl_gamepad_alt_active(), APP_INPUT_TYPE_GAMEPAD_AXIS);
                } else {
                    if (prev_dir == 1 || prev_dir == 3 || prev_dir == 7 || prev_dir == 9) {
                        sdl_gamepad_clear_pending_left_stick();
                        return;
                    }
                    if (g_gamepad_state.left_pending)
                        sdl_gamepad_flush_pending_left_stick(SDL_GetTicksNS(), true);
                    sdl_gamepad_set_pending_left_stick(dir);
                }
            }
        } else {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_bind_dir;
            if (dir != prev_dir) {
                if (prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                    int prev_binding = config.gamepad_left_stick_bindings[prev_dir];
                    if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(prev_binding, false);
                    }
                }

                g_gamepad_state.left_bind_dir = dir;

                if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    int binding = config.gamepad_left_stick_bindings[dir];
                    int combo_binding = GAMEPAD_BIND_NONE;

                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_LEFT_STICK, dir);
                    }

                    if (combo_binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key(combo_binding, false);
                    } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, true);
                    } else if (binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
            g_gamepad_state.right_x = ev->value;
        else
            g_gamepad_state.right_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;
        int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
        int prev_dir = g_gamepad_state.right_dir;
        if (dir != prev_dir) {
            if (prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                int prev_binding = config.gamepad_right_stick_bindings[prev_dir];
                if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                    sdl_gamepad_apply_modifier(prev_binding, false);
                }
            }

            g_gamepad_state.right_dir = dir;

            if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                int active_modifier = sdl_gamepad_single_active_modifier();
                int binding = config.gamepad_right_stick_bindings[dir];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (active_modifier != GAMEPAD_BIND_NONE) {
                    combo_binding = sdl_gamepad_combo_binding_for_input(
                        active_modifier, GAMEPAD_CAPTURE_RIGHT_STICK, dir);
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(combo_binding, false);
                } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                    sdl_gamepad_apply_modifier(binding, true);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(binding, false);
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        int threshold = config.gamepad_trigger_threshold;
        if (threshold < 0)
            threshold = 0;
        bool pressed = (ev->value >= threshold);

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            if (pressed != g_gamepad_state.left_trigger_down) {
                g_gamepad_state.left_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[0];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 0);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(combo_binding, false);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, pressed);
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        } else {
            if (pressed != g_gamepad_state.right_trigger_down) {
                g_gamepad_state.right_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[1];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 1);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(combo_binding, false);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, pressed);
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
    }
}

static void sdl_gamepad_open(SDL_JoystickID id)
{
    if (!SDL_IsGamepad(id)) {
        log_debug("Ignoring non-gamepad device id %d", (int)id);
        return;
    }

    if (SDL_GetGamepadFromID(id)) {
        return;
    }

    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) {
        log_warn("Failed to open gamepad id %d: %s", (int)id, SDL_GetError());
        return;
    }

    if (g_gamepad_state.pad_count >= MAX_GAMEPADS) {
        SDL_CloseGamepad(pad);
        log_warn("Gamepad list full, closing id %d", (int)id);
        return;
    }

    g_gamepad_state.pads[g_gamepad_state.pad_count].id = id;
    g_gamepad_state.pads[g_gamepad_state.pad_count].pad = pad;
    g_gamepad_state.pad_count++;

    log_info("Gamepad opened id %d (%s)", (int)id, SDL_GetGamepadName(pad));
    sdl_gamepad_mark_auto_ui();
}

static void sdl_gamepad_close(SDL_JoystickID id)
{
    for (int i = 0; i < g_gamepad_state.pad_count; i++) {
        if (g_gamepad_state.pads[i].id == id) {
            SDL_CloseGamepad(g_gamepad_state.pads[i].pad);
            g_gamepad_state.pads[i] = g_gamepad_state.pads[g_gamepad_state.pad_count - 1];
            g_gamepad_state.pad_count--;
            log_info("Gamepad closed id %d", (int)id);
            break;
        }
    }
}

static void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev)
{
    if (!ev)
        return;

    if (ev->type == SDL_EVENT_GAMEPAD_ADDED) {
        sdl_gamepad_open(ev->which);
    } else if (ev->type == SDL_EVENT_GAMEPAD_REMOVED) {
        sdl_gamepad_close(ev->which);
    }
}

void sdl_gamepad_init(void)
{
    SDL_SetGamepadEventsEnabled(true);
    g_gamepad_state.left_bind_dir = -1;
    g_gamepad_state.right_dir = -1;
    sdl_gamepad_clear_pending_shoulder();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        log_warn("SDL_GetGamepads failed: %s", SDL_GetError());
        return;
    }

    for (int i = 0; i < count; i++) {
        sdl_gamepad_open(ids[i]);
    }
    SDL_free(ids);
}

void sdl_gamepad_shutdown(void)
{
    while (g_gamepad_state.pad_count > 0) {
        SDL_JoystickID id = g_gamepad_state.pads[g_gamepad_state.pad_count - 1].id;
        sdl_gamepad_close(id);
    }
}
void sdl_handle_event(sdl_state* st, const SDL_Event* ev)
{
    (void)st;

    if (!ev)
        return;

    if (sdl_sound_try_handle_event(ev))
        return;

    if (ev->type == SDL_EVENT_QUIT) {
        sdl_submit_legacy_input_byte(27);
        return;
    }

    if (sdl_touch_pane_handle_event(ev))
        return;

    if (ev->type == SDL_EVENT_KEY_DOWN) {
        int key = ev->key.key;
        int translated = 0;

        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
            key == SDLK_LALT || key == SDLK_RALT ||
            key == SDLK_LCTRL || key == SDLK_RCTRL ||
            key == SDLK_LGUI || key == SDLK_RGUI) {
            return;
        }

        if (sdl_handle_global_layout_shortcut(&ev->key))
            return;

        bool alt = ev->key.mod & SDL_KMOD_ALT;
        if (alt && !character_dungeon)
            return;

        if (sdl_try_submit_keyboard_movement_event(&ev->key))
            return;

        if (!sdl_translate_legacy_input_key(key,
                (ev->key.mod & SDL_KMOD_SHIFT) != 0,
                (ev->key.mod & SDL_KMOD_CTRL) != 0,
                alt,
                (ev->key.mod & SDL_KMOD_GUI) != 0,
                true, &translated))
        {
            return;
        }

        if (sdl_queue_legacy_input_byte_ex(translated, ev->key.repeat))
            return;
        (void)input_byte_enqueue(translated);
        return;
    }

    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        sdl_gamepad_handle_button(&ev->gbutton);
        return;
    }

    if (ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        sdl_gamepad_handle_axis(&ev->gaxis);
        return;
    }

    if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED
        || ev->type == SDL_EVENT_GAMEPAD_REMAPPED) {
        sdl_gamepad_handle_device(&ev->gdevice);
        return;
    }

    if (ev->type == SDL_EVENT_WINDOW_RESIZED) {
        SDL_Rect window;
        sdl_refresh_safe_area();
        window = sdl_get_layout_screen_rect();
        resize(&window);
        return;
    }

    if (ev->type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED
        || ev->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
        || ev->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        float scale = SDL_GetWindowDisplayScale(g_state.window);
        SDL_Rect window = { 0 };
        bool scale_changed = (scale != g_state.system_scale);

        if (scale_changed) {
            g_state.system_scale = scale;
            sdl_load_story_fonts();
        }

        sdl_refresh_safe_area();
        window = sdl_get_layout_screen_rect();
        resize(&window);
        return;
    }

    if (ev->type == SDL_EVENT_RENDER_DEVICE_RESET || ev->type == SDL_EVENT_RENDER_TARGETS_RESET) {
        sdl_handle_renderer_reset();
        return;
    }

    if (ev->type == SDL_EVENT_WINDOW_RESTORED || ev->type == SDL_EVENT_WINDOW_EXPOSED) {
        g_state.need_present = true;
    }
}

static void sdl_quit_hook(cptr str)
{
    (void)str; // Unused parameter
    
    // Shut down audio before tearing down SDL
    platform_sound_shutdown();

    // Close any open gamepads
    sdl_gamepad_shutdown();
    
    // Clean up story fonts
    sdl_story_font_cache_clear();
    sdl_scene_stack_shutdown();
    
    // Only save if we have a valid window and config file path
    if (g_state.window && config_file_path[0] != '\0') {
        // Get current window position and size if not in fullscreen
        if (!config.fullscreen) {
            SDL_GetWindowPosition(g_state.window, &config.window_x, &config.window_y);
            SDL_GetWindowSize(g_state.window, &config.window_width, &config.window_height);
            log_debug("Saving window position (%d, %d) and size (%dx%d)",
                     config.window_x, config.window_y, config.window_width, config.window_height);
        }
        
        // Save configuration
        sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    }
}


errr init_sdl(int argc, char **argv)
{
    log_debug("init_sdl starting");
    
    // Initialize SDL first to get display information
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
    if (!TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }
    
    // Get primary display information
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (!primary) {
        log_error("SDL_GetPrimaryDisplay failed: %s", SDL_GetError());
        quit("could not get primary display ID");
    }
    
    // Get display bounds for window sizing (uses logical coordinates)
    SDL_Rect screen;
    if (!SDL_GetDisplayBounds(primary, &screen)) {
        log_error("SDL_GetDisplayBounds failed: %s", SDL_GetError());
        quit("could not get primary display bounds");
    }
    log_info("primary display bounds (logical): %dx%d at (%d,%d)",
             screen.w, screen.h, screen.x, screen.y);
    
    // Get the desktop display mode - this contains the pixel_density field we need
    const SDL_DisplayMode* desktop_mode = SDL_GetDesktopDisplayMode(primary);
    if (!desktop_mode) {
        log_error("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        quit("could not get desktop display mode");
    }
    
    // SDL_DisplayMode contains:
    // - w, h: logical resolution (points on macOS, pixels on Windows/Linux without scaling)
    // - pixel_density: scale factor (e.g., 2.0 on Retina displays, 1.0 otherwise)
    // Physical resolution = logical × pixel_density
    float pixel_density = desktop_mode->pixel_density;
    
    // Calculate physical pixel dimensions for resolution profile matching
    // On macOS Retina: 1440×900 logical × 2.0 density = 2560×1600 physical
    // On Windows/Linux (no scaling): 1920×1080 logical × 1.0 density = 1920×1080 physical
    int screen_pixels_w = (int)(desktop_mode->w * pixel_density + 0.5f);
    int screen_pixels_h = (int)(desktop_mode->h * pixel_density + 0.5f);
    
    log_info("primary display desktop mode: %dx%d @%.2fHz, pixel_density=%.2f",
             desktop_mode->w, desktop_mode->h, desktop_mode->refresh_rate, pixel_density);
    log_info("primary display physical resolution for defaults: %dx%d",
             screen_pixels_w, screen_pixels_h);
    
    // Save config file path for later use on exit
    char config_file[1024];
    if (!resource_build_ui_config_path(config_file, sizeof(config_file)))
        SDL_strlcpy(config_file, "sil_sdl.json", sizeof(config_file));
    SDL_strlcpy(config_file_path, config_file, sizeof(config_file_path));
    
    // Register quit hook to save configuration on exit
    log_register_quit_hook(sdl_quit_hook);
    
    // Check if config file exists
    bool config_exists = SDL_GetPathInfo(config_file_path, NULL);

    if (config_exists) {
        // Config file exists - use generic defaults first, then load from file
        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);
        
        // Copy default pane configuration
        sdl_copy_default_pane_config();
        
        sdl_config_load(config_file_path, &config, pane_config, &pane_config_count, MAX_PANE_CONFIGS);
        
        // Load the active sound configuration via the resource layer.
        char sound_config_path[1024];
        if (!resource_build_active_sound_config_path(sound_config_path,
                sizeof(sound_config_path)))
        {
            SDL_strlcpy(sound_config_path, "sound.json",
                sizeof(sound_config_path));
        }
        sound_config_load(sound_config_path, &g_sound_config);
        
        // Apply sound setting to global variable
        use_sound = g_sound_config.enabled;
        
        log_debug("After loading JSON: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d, sound=%d",
                  config.main_view_scale, config.aux_view_font_size,
                  config.menu_panel_font_size, config.margin,
                  config.fullscreen, config.tiles, g_sound_config.enabled);
    } else {
        // Config file doesn't exist - use resolution-based defaults
        log_debug("Config file not found, using resolution-based defaults");
        sdl_config_set_defaults_for_resolution(&config, pane_config, &pane_config_count,
                                               MAX_PANE_CONFIGS, screen_pixels_w, screen_pixels_h);
        
        // If no resolution-specific config was found, use default pane config
        if (pane_config_count == 0)
            sdl_copy_default_pane_config();
        
        log_debug("After resolution defaults: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d",
                  config.main_view_scale, config.aux_view_font_size,
                  config.menu_panel_font_size, config.margin,
                  config.fullscreen, config.tiles);
    }

#if defined(__ANDROID__) || defined(SIL_IOS)
    sdl_ensure_default_pane_configs_present(false);
    sdl_ensure_touch_pane_config_present();

    if (!config_exists) {
        for (int i = 0; i < pane_config_count; i++) {
            if (pane_config[i].pane == PANE_TOUCH) {
                pane_config[i].enabled = true;
                pane_config[i].where = PLACE_DOUBLE_RIGHT;
            } else {
                pane_config[i].enabled = false;
            }
        }

        config.enable_right_panes = true;
        config.enable_bottom_panes = false;
        log_info("Mobile default pane layout: touch only enabled; other panes available in settings");
    }
#endif

    sdl_ensure_touch_pane_config_present();

    g_hide_left_panel = config.hide_left_panel;
    
    // Apply command-line overrides
    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d",
              config.main_view_scale, config.aux_view_font_size,
              config.menu_panel_font_size, config.margin,
              config.fullscreen, config.tiles);

    if (!sdl_config_has_movement_bindings(&config)) {
        sdl_config_set_default_movement_bindings(&config,
            APP_MOVEMENT_PRESET_CLASSIC_SIL);
        log_info("Initialized default movement bindings: Classic Sil");
    }

#if defined(__ANDROID__) || defined(SIL_IOS)
    {
        int mobile_min_cols = platform_current_min_terminal_cols();
        int mobile_min_rows = platform_current_min_terminal_rows();
        int mobile_max_scale_w = (screen_pixels_w / mobile_min_cols) * 2 / TILE_SIZE;
        int mobile_max_scale_h = screen_pixels_h / mobile_min_rows / TILE_SIZE;
        int mobile_max_scale = mobile_max_scale_w;

        if (mobile_max_scale_h < mobile_max_scale)
            mobile_max_scale = mobile_max_scale_h;
        if (mobile_max_scale < 1)
            mobile_max_scale = 1;

        if (!config_exists) {
            if (config.main_view_scale != mobile_max_scale) {
                log_info("Mobile default main_view_scale set to %d for >=%dx%d (%s) at %dx%d",
                         mobile_max_scale,
                         mobile_min_cols, mobile_min_rows,
                         sdl_min_terminal_mode_name(config.min_terminal_mode),
                         screen_pixels_w, screen_pixels_h);
            }
            config.main_view_scale = mobile_max_scale;
        } else if (config.main_view_scale > mobile_max_scale) {
            log_info("Mobile main_view_scale clamped from %d to %d to keep >=%dx%d (%s)",
                     config.main_view_scale, mobile_max_scale,
                     mobile_min_cols, mobile_min_rows,
                     sdl_min_terminal_mode_name(config.min_terminal_mode));
            config.main_view_scale = mobile_max_scale;
        }
    }
#endif
    
    // Validate configuration
    if (config.main_view_scale <= 0) {
        log_warn("Invalid main_view_scale %d, using 1", config.main_view_scale);
        config.main_view_scale = 1;
    }
    if (config.aux_view_font_size < 0) {
        log_warn("Invalid aux_view_font_size %d, using auto", config.aux_view_font_size);
        config.aux_view_font_size = 0;
    } else if (config.aux_view_font_size > 48) {
        log_warn("Invalid aux_view_font_size %d, clamping to 48", config.aux_view_font_size);
        config.aux_view_font_size = 48;
    }
    if (config.menu_panel_font_size < 0) {
        log_warn("Invalid menu_panel_font_size %d, using auto",
            config.menu_panel_font_size);
        config.menu_panel_font_size = 0;
    } else if (config.menu_panel_font_size > 64) {
        log_warn("Invalid menu_panel_font_size %d, clamping to 64",
            config.menu_panel_font_size);
        config.menu_panel_font_size = 64;
    }
    if (config.plain_menu_font_size < 0) {
        log_warn("Invalid plain_menu_font_size %d, using auto",
            config.plain_menu_font_size);
        config.plain_menu_font_size = 0;
    } else if (config.plain_menu_font_size > 64) {
        log_warn("Invalid plain_menu_font_size %d, clamping to 64",
            config.plain_menu_font_size);
        config.plain_menu_font_size = 64;
    }
    if (config.browser_menu_font_size < 0) {
        log_warn("Invalid browser_menu_font_size %d, using auto",
            config.browser_menu_font_size);
        config.browser_menu_font_size = 0;
    } else if (config.browser_menu_font_size > 64) {
        log_warn("Invalid browser_menu_font_size %d, clamping to 64",
            config.browser_menu_font_size);
        config.browser_menu_font_size = 64;
    }
    if (config.character_sheet_font_size < 0) {
        log_warn("Invalid character_sheet_font_size %d, using auto",
            config.character_sheet_font_size);
        config.character_sheet_font_size = 0;
    } else if (config.character_sheet_font_size > 64) {
        log_warn("Invalid character_sheet_font_size %d, clamping to 64",
            config.character_sheet_font_size);
        config.character_sheet_font_size = 64;
    }
    if (config.margin < 0) {
        log_warn("Invalid margin %d, using 0", config.margin);
        config.margin = 0;
    }
    if (!sdl_min_terminal_mode_is_valid(config.min_terminal_mode)) {
#if defined(__ANDROID__) || defined(SIL_IOS)
        log_warn("Invalid min_terminal_mode %d, using compact", config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
        log_warn("Invalid min_terminal_mode %d, using normal", config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
    }
    if (config.gamepad_deadzone < 0) {
        log_warn("Invalid gamepad_deadzone %d, using 0", config.gamepad_deadzone);
        config.gamepad_deadzone = 0;
    } else if (config.gamepad_deadzone > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_deadzone %d, clamping to %d", config.gamepad_deadzone, SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_deadzone = SDL_JOYSTICK_AXIS_MAX;
    }
    if (config.gamepad_trigger_threshold < 0) {
        log_warn("Invalid gamepad_trigger_threshold %d, using 0", config.gamepad_trigger_threshold);
        config.gamepad_trigger_threshold = 0;
    } else if (config.gamepad_trigger_threshold > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_trigger_threshold %d, clamping to %d", config.gamepad_trigger_threshold,
            SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_trigger_threshold = SDL_JOYSTICK_AXIS_MAX;
    }

    sdl_gamepad_init();

#if defined(__ANDROID__) || defined(SIL_IOS)
    if (!config_exists) {
        config.steamdeck_mode = (g_gamepad_state.pad_count > 0);
        log_info("Mobile first-start Steam Deck UI mode set to %s (%d gamepad%s detected)",
            config.steamdeck_mode ? "on" : "off",
            g_gamepad_state.pad_count,
            (g_gamepad_state.pad_count == 1) ? "" : "s");
    }
#endif
    
    log_info("SDL Configuration:");
    log_info("  Main view scale: %d", config.main_view_scale);
    if (config.aux_view_font_size > 0)
        log_info("  Default aux view font size: %d", config.aux_view_font_size);
    else
        log_info("  Default aux view font size: auto (%d)", sdl_auto_aux_view_font_size());
    if (config.menu_panel_font_size > 0)
        log_info("  Default menu + left panel font size: %d",
            config.menu_panel_font_size);
    else
        log_info("  Default menu + left panel font size: auto (%d)",
            sdl_resolve_menu_panel_font_size(config.menu_panel_font_size));
    if (config.plain_menu_font_size > 0)
        log_info("  Plain menu font size: %d", config.plain_menu_font_size);
    else
        log_info("  Plain menu font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_PLAIN));
    if (config.browser_menu_font_size > 0)
        log_info("  Browser menu font size: %d",
            config.browser_menu_font_size);
    else
        log_info("  Browser menu font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_BROWSER));
    if (config.character_sheet_font_size > 0)
        log_info("  Character sheet font size: %d",
            config.character_sheet_font_size);
    else
        log_info("  Character sheet font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_CHARACTER_SHEET));
    log_info("  Margin: %d", config.margin);
    log_info("  Fullscreen: %s", config.fullscreen ? "true" : "false");
    log_info("  Tiles: %s", config.tiles ? "true" : "false");
    log_info("  Use unsafe area: %s", config.use_unsafe_area ? "true" : "false");
    log_info("  Minimum terminal size: %s (%dx%d)",
             sdl_min_terminal_mode_name(config.min_terminal_mode),
             platform_current_min_terminal_cols(), platform_current_min_terminal_rows());
    log_info("  Pane configurations: %d", pane_config_count);
    log_info("  Palette preset: %s",
        config.palette_preset[0] ? config.palette_preset : "classic");

    ui_colors_load_palette_presets();
    if (!ui_colors_apply_palette_preset(config.palette_preset))
        ui_colors_apply_palette_preset("classic");
    SDL_strlcpy(config.palette_preset, ui_colors_current_palette_preset(),
        sizeof(config.palette_preset));

    // Initialize palette from the selected preset
    sdl_sync_palette();

    // Prepare sound registry and audio playback
    platform_sound_reload();
    if (!platform_sound_initialize()) {
        log_info("Sound subsystem not initialized; continuing without audio output");
    }

    // Use full display size for fullscreen, reasonable default for windowed mode
    int window_width, window_height;
    if (config.fullscreen) {
        window_width = screen.w;
        window_height = screen.h;
    } else {
        // Use saved dimensions if valid, otherwise default to 3/4 of screen size
        if (config.window_width > 0 && config.window_height > 0) {
            window_width = config.window_width;
            window_height = config.window_height;
            log_debug("Using saved window size: %dx%d", window_width, window_height);
        } else {
            window_width = screen.w * 3 / 4;
            window_height = screen.h * 3 / 4;
            log_debug("Using default window size: %dx%d", window_width, window_height);
        }
    }
    
    sdl_window_create(window_width, window_height, config.fullscreen, config.tiles);
    
    // Set window position for windowed mode
    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0) {
        sdl_window_set_position(config.window_x, config.window_y);
    }
    
    // Load story and banner fonts
    sdl_load_story_fonts();

    ANGBAND_SYS = "sdl";
    if (config.tiles) {
        ANGBAND_GRAF = "new";
        runtime_cli_set_graphics_mode(GRAPHICS_MICROCHASM);
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        runtime_cli_set_graphics_mode(GRAPHICS_PSEUDO);
        use_graphics = GRAPHICS_PSEUDO;
        use_bigtile = false;
    }

    sdl_refresh_safe_area();
    SDL_Rect window = sdl_get_layout_screen_rect();
    log_debug("layout pixel rect (%d,%d %dx%d)", window.x, window.y,
        window.w, window.h);
    resize(&window);
    sdl_scene_stack_init();

    log_debug("init_sdl: SDL views initialized (tiles_mode=%d)",
        config.tiles);
    
    return 0;
}

/*
 * Get SDL configuration info as formatted string
 * Called from cmd4.c for the pane settings menu
 */
void sdl_gamepad_load_default_bindings(void)
{
    if (g_default_gamepad_bindings_ready)
        return;

    struct sdl_config defaults;
    sdl_config_set_defaults(&defaults);
    memcpy(g_default_gamepad_button_bindings, defaults.gamepad_button_bindings,
        sizeof(g_default_gamepad_button_bindings));
    memcpy(g_default_gamepad_trigger_bindings, defaults.gamepad_trigger_bindings,
        sizeof(g_default_gamepad_trigger_bindings));
    memcpy(g_default_gamepad_left_stick_bindings, defaults.gamepad_left_stick_bindings,
        sizeof(g_default_gamepad_left_stick_bindings));
    memcpy(g_default_gamepad_right_stick_bindings, defaults.gamepad_right_stick_bindings,
        sizeof(g_default_gamepad_right_stick_bindings));
    memcpy(g_default_gamepad_button_combo_bindings, defaults.gamepad_button_combo_bindings,
        sizeof(g_default_gamepad_button_combo_bindings));
    memcpy(g_default_gamepad_trigger_combo_bindings, defaults.gamepad_trigger_combo_bindings,
        sizeof(g_default_gamepad_trigger_combo_bindings));
    memcpy(g_default_gamepad_left_stick_combo_bindings,
        defaults.gamepad_left_stick_combo_bindings,
        sizeof(g_default_gamepad_left_stick_combo_bindings));
    memcpy(g_default_gamepad_right_stick_combo_bindings,
        defaults.gamepad_right_stick_combo_bindings,
        sizeof(g_default_gamepad_right_stick_combo_bindings));
    g_default_gamepad_shoulder_combo_binding = defaults.gamepad_shoulder_combo_binding;
    g_default_gamepad_bindings_ready = true;
}

bool steamdeck_controls_active(void)
{
    if (config.steamdeck_mode)
        return true;
    if (!config.gamepad_enabled)
        return false;
    return (config.gamepad_auto_mode && g_gamepad_auto_ui);
}

bool portable_controls_active(void)
{
#if defined(SIL_USE_LOCAL_DATA) || defined(__ANDROID__) || defined(SIL_IOS)
    /* Portable builds and mobile platforms use the controller-style menu shortcuts. */
    return true;
#else
    return steamdeck_controls_active();
#endif
}

bool platform_gamepad_enabled(void)
{
    return config.gamepad_enabled;
}

void platform_set_gamepad_enabled(bool value)
{
    config.gamepad_enabled = value;
    if (!value) {
        g_gamepad_state.dpad_up = false;
        g_gamepad_state.dpad_down = false;
        g_gamepad_state.dpad_left = false;
        g_gamepad_state.dpad_right = false;
        g_gamepad_state.dpad_dir = 0;
        sdl_gamepad_clear_pending_dpad();
        g_gamepad_state.left_x = 0;
        g_gamepad_state.left_y = 0;
        g_gamepad_state.left_dir = 0;
        g_gamepad_state.left_bind_dir = -1;
        sdl_gamepad_clear_pending_left_stick();
        g_gamepad_state.right_x = 0;
        g_gamepad_state.right_y = 0;
        g_gamepad_state.right_dir = -1;
        sdl_gamepad_clear_pending_shoulder();
        g_gamepad_state.left_trigger_down = false;
        g_gamepad_state.right_trigger_down = false;
        g_gamepad_state.shift_held = 0;
        g_gamepad_state.ctrl_held = 0;
        g_gamepad_state.alt_held = 0;
        sdl_touch_pane_reset_input_state();
    }
}

bool platform_gamepad_auto_mode(void)
{
    return config.gamepad_auto_mode;
}

void platform_set_gamepad_auto_mode(bool value)
{
    config.gamepad_auto_mode = value;
}

bool platform_steamdeck_mode(void)
{
    return config.steamdeck_mode;
}

void platform_set_steamdeck_mode(bool value)
{
    config.steamdeck_mode = value;
}

bool platform_gamepad_use_dpad(void)
{
    return config.gamepad_use_dpad;
}

void platform_set_gamepad_use_dpad(bool value)
{
    config.gamepad_use_dpad = value;
    if (value) {
        config.gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_UP] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_DOWN] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_LEFT] = GAMEPAD_BIND_NONE;
        config.gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_RIGHT] = GAMEPAD_BIND_NONE;
    } else {
        g_gamepad_state.dpad_up = false;
        g_gamepad_state.dpad_down = false;
        g_gamepad_state.dpad_left = false;
        g_gamepad_state.dpad_right = false;
        g_gamepad_state.dpad_dir = 0;
        sdl_gamepad_clear_pending_dpad();
    }
}

bool platform_gamepad_use_left_stick(void)
{
    return config.gamepad_use_left_stick;
}

void platform_set_gamepad_use_left_stick(bool value)
{
    config.gamepad_use_left_stick = value;
    if (value) {
        if (g_gamepad_state.left_bind_dir >= 0 && g_gamepad_state.left_bind_dir < GAMEPAD_STICK_DIR_COUNT) {
            int binding = config.gamepad_left_stick_bindings[g_gamepad_state.left_bind_dir];
            if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                sdl_gamepad_apply_modifier(binding, false);
            }
        }
        g_gamepad_state.left_bind_dir = -1;
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            config.gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
        }
    } else {
        g_gamepad_state.left_x = 0;
        g_gamepad_state.left_y = 0;
        g_gamepad_state.left_dir = 0;
        g_gamepad_state.left_bind_dir = -1;
        sdl_gamepad_clear_pending_left_stick();
    }
}

int platform_gamepad_button_binding(int button)
{
    if (button < 0 || button >= GAMEPAD_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_button_bindings[button];
}

void platform_set_gamepad_button_binding(int button, int binding)
{
    if (button < 0 || button >= GAMEPAD_BUTTON_COUNT)
        return;
    config.gamepad_button_bindings[button] = binding;
}

int platform_gamepad_trigger_binding(int index)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_trigger_bindings[index];
}

void platform_set_gamepad_trigger_binding(int index, int binding)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return;
    config.gamepad_trigger_bindings[index] = binding;
}

int platform_gamepad_left_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_left_stick_bindings[dir];
}

void platform_set_gamepad_left_stick_binding(int dir, int binding)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return;
    config.gamepad_left_stick_bindings[dir] = binding;
}

int platform_gamepad_right_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    return config.gamepad_right_stick_bindings[dir];
}

void platform_set_gamepad_right_stick_binding(int dir, int binding)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return;
    config.gamepad_right_stick_bindings[dir] = binding;
}

int platform_gamepad_combo_binding(int modifier, int type, int id)
{
    return sdl_gamepad_combo_binding_for_input(modifier, type, id);
}

void platform_set_gamepad_combo_binding(int modifier, int type, int id, int binding)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < GAMEPAD_BUTTON_COUNT)
            config.gamepad_button_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            config.gamepad_trigger_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            config.gamepad_left_stick_combo_bindings[modifier_index][id] = binding;
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            config.gamepad_right_stick_combo_bindings[modifier_index][id] = binding;
        break;
    default:
        break;
    }
}

int platform_gamepad_shoulder_combo_binding(void)
{
    return config.gamepad_shoulder_combo_binding;
}

void platform_set_gamepad_shoulder_combo_binding(int binding)
{
    config.gamepad_shoulder_combo_binding = binding;
}

int platform_gamepad_default_button_binding(int button)
{
    if (button < 0 || button >= GAMEPAD_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_button_bindings[button];
}

int platform_gamepad_default_trigger_binding(int index)
{
    if (index < 0 || index >= GAMEPAD_TRIGGER_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_trigger_bindings[index];
}

int platform_gamepad_default_left_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_left_stick_bindings[dir];
}

int platform_gamepad_default_right_stick_binding(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_right_stick_bindings[dir];
}

int platform_gamepad_default_combo_binding(int modifier, int type, int id)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return GAMEPAD_BIND_NONE;

    sdl_gamepad_load_default_bindings();

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < GAMEPAD_BUTTON_COUNT)
            return g_default_gamepad_button_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return g_default_gamepad_trigger_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return g_default_gamepad_left_stick_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return g_default_gamepad_right_stick_combo_bindings[modifier_index][id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

int platform_gamepad_default_shoulder_combo_binding(void)
{
    sdl_gamepad_load_default_bindings();
    return g_default_gamepad_shoulder_combo_binding;
}

void platform_gamepad_reset_bindings_to_default(void)
{
    sdl_config_set_default_gamepad_bindings(&config);
}

bool platform_gamepad_capture_begin(void)
{
    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = (g_gamepad_state.pad_count > 0);
    return g_gamepad_capture_active;
}

void platform_gamepad_capture_cancel(void)
{
    g_gamepad_capture_active = false;
    g_gamepad_capture_ready = false;
    sdl_gamepad_clear_pending_shoulder();
}

bool platform_gamepad_capture_poll(int* out_type, int* out_id)
{
    if (!g_gamepad_capture_ready)
        return false;

    if (out_type)
        *out_type = g_gamepad_capture_type;
    if (out_id)
        *out_id = g_gamepad_capture_id;

    g_gamepad_capture_ready = false;
    g_gamepad_capture_active = false;
    sdl_gamepad_clear_pending_shoulder();
    return true;
}

/*
 * Calculate the maximum scale for the current window.
 * This keeps at least the configured minimum terminal size visible in the
 * current window.
 */
