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

#include "sdl-scene-menu.h"

typedef struct sdl_menu_pointer_capture {
    bool active;
    u16b device;
    u16b button;
    SDL_FingerID finger_id;
    u16b target_kind;
    s16b target_id;
} sdl_menu_pointer_capture;

static sdl_menu_pointer_capture g_menu_pointer_capture;
static u64b g_structured_input_sequence;

static u16b sdl_menu_pointer_modifiers_from_keymod(SDL_Keymod mod)
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
    if (mod & SDL_KMOD_CAPS)
        modifiers |= APP_INPUT_MODIFIER_CAPS_LOCK;
    if (mod & SDL_KMOD_NUM)
        modifiers |= APP_INPUT_MODIFIER_NUM_LOCK;

    return modifiers;
}

static bool sdl_menu_pointer_submit_input(u16b device, u16b type, u16b flags,
    float x, float y, float dx, float dy, u16b button, u16b clicks)
{
    app_session* session = app_session_current();
    app_input input;

    if (!session)
        return false;

    memset(&input, 0, sizeof(input));
    input.layer = APP_INPUT_LAYER_INTENT;
    input.type = type;
    input.device = device;
    input.modifiers = sdl_menu_pointer_modifiers_from_keymod(
        SDL_GetModState());
    input.flags = flags;
    input.sequence = ++g_structured_input_sequence;
    input.timestamp_usec = SDL_GetTicksNS() / 1000ULL;

    if (type == APP_INPUT_TYPE_POINTER_WHEEL)
    {
        input.payload.wheel.x = (s32b)dx;
        input.payload.wheel.y = (s32b)dy;
    }
    else
    {
        input.payload.pointer.x = (s32b)x;
        input.payload.pointer.y = (s32b)y;
        input.payload.pointer.dx = (s32b)dx;
        input.payload.pointer.dy = (s32b)dy;
        input.payload.pointer.button = button;
        input.payload.pointer.clicks = clicks;
    }

    return app_session_submit_input(session, &input);
}

static void sdl_menu_pointer_capture_clear(void)
{
    memset(&g_menu_pointer_capture, 0, sizeof(g_menu_pointer_capture));
}

static bool sdl_menu_pointer_target_matches_capture(
    const sdl_menu_hit_target* target)
{
    return target && g_menu_pointer_capture.active
        && target->kind == g_menu_pointer_capture.target_kind
        && target->id == g_menu_pointer_capture.target_id;
}

static bool sdl_menu_pointer_activate_target(
    const sdl_menu_hit_target* target)
{
    if (!target || target->action_key == 0)
        return false;

    log_trace("ui pointer: activating target kind=%u id=%d key=%d label='%s'",
        (unsigned)target->kind, (int)target->id, (int)target->action_key,
        target->label);
    sdl_submit_legacy_input_byte(target->action_key);
    return true;
}

static bool sdl_menu_pointer_event_xy(const SDL_Event* ev, float* out_x,
    float* out_y)
{
    int window_w = 0;
    int window_h = 0;

    if (!ev || !out_x || !out_y)
        return false;

    if (ev->type == SDL_EVENT_MOUSE_MOTION)
    {
        *out_x = ev->motion.x;
        *out_y = ev->motion.y;
        return true;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN
        || ev->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        *out_x = ev->button.x;
        *out_y = ev->button.y;
        return true;
    }
    if (ev->type == SDL_EVENT_FINGER_DOWN
        || ev->type == SDL_EVENT_FINGER_MOTION
        || ev->type == SDL_EVENT_FINGER_UP
        || ev->type == SDL_EVENT_FINGER_CANCELED)
    {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return false;
        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        *out_x = ev->tfinger.x * (float)window_w;
        *out_y = ev->tfinger.y * (float)window_h;
        return true;
    }

    return false;
}

bool sdl_menu_pointer_handle_event(const SDL_Event* ev)
{
    const sdl_menu_hit_target* target;
    float x;
    float y;

    if (!ev)
        return false;

    if (ev->type == SDL_EVENT_MOUSE_MOTION)
    {
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_MOTION, 0, ev->motion.x, ev->motion.y,
            ev->motion.xrel, ev->motion.yrel, 0, 0);
        return false;
    }

    if (ev->type == SDL_EVENT_MOUSE_WHEEL)
    {
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_WHEEL, 0, 0.0f, 0.0f, (float)ev->wheel.x,
            (float)ev->wheel.y, 0, 0);
        return false;
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
            return false;
        target = sdl_menu_hit_test(x, y);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_BUTTON, APP_INPUT_FLAG_PRESS, x, y, 0.0f,
            0.0f, ev->button.button, ev->button.clicks);
        if (!target)
            return false;
        g_menu_pointer_capture.active = true;
        g_menu_pointer_capture.device = APP_INPUT_DEVICE_POINTER;
        g_menu_pointer_capture.button = ev->button.button;
        g_menu_pointer_capture.target_kind = target->kind;
        g_menu_pointer_capture.target_id = target->id;
        return true;
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        bool had_capture = g_menu_pointer_capture.active
            && g_menu_pointer_capture.device == APP_INPUT_DEVICE_POINTER
            && g_menu_pointer_capture.button == ev->button.button;

        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
            return false;
        target = sdl_menu_hit_test(x, y);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_BUTTON, APP_INPUT_FLAG_RELEASE, x, y, 0.0f,
            0.0f, ev->button.button, ev->button.clicks);
        if (!had_capture)
            return false;
        if (sdl_menu_pointer_target_matches_capture(target))
            (void)sdl_menu_pointer_activate_target(target);
        sdl_menu_pointer_capture_clear();
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_DOWN)
    {
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
            return false;
        target = sdl_menu_hit_test(x, y);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_TOUCH,
            APP_INPUT_TYPE_POINTER_BUTTON, APP_INPUT_FLAG_PRESS, x, y, 0.0f,
            0.0f, SDL_BUTTON_LEFT, 1);
        if (!target)
            return false;
        g_menu_pointer_capture.active = true;
        g_menu_pointer_capture.device = APP_INPUT_DEVICE_TOUCH;
        g_menu_pointer_capture.button = SDL_BUTTON_LEFT;
        g_menu_pointer_capture.finger_id = ev->tfinger.fingerID;
        g_menu_pointer_capture.target_kind = target->kind;
        g_menu_pointer_capture.target_id = target->id;
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_MOTION)
    {
        if (!g_menu_pointer_capture.active
            || g_menu_pointer_capture.device != APP_INPUT_DEVICE_TOUCH
            || g_menu_pointer_capture.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
            return true;
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_TOUCH,
            APP_INPUT_TYPE_POINTER_MOTION, 0, x, y, ev->tfinger.dx,
            ev->tfinger.dy, SDL_BUTTON_LEFT, 1);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_UP
        || ev->type == SDL_EVENT_FINGER_CANCELED)
    {
        bool activate = ev->type == SDL_EVENT_FINGER_UP;

        if (!g_menu_pointer_capture.active
            || g_menu_pointer_capture.device != APP_INPUT_DEVICE_TOUCH
            || g_menu_pointer_capture.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
        {
            sdl_menu_pointer_capture_clear();
            return true;
        }
        target = sdl_menu_hit_test(x, y);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_TOUCH,
            APP_INPUT_TYPE_POINTER_BUTTON, APP_INPUT_FLAG_RELEASE, x, y, 0.0f,
            0.0f, SDL_BUTTON_LEFT, 1);
        if (activate && sdl_menu_pointer_target_matches_capture(target))
            (void)sdl_menu_pointer_activate_target(target);
        sdl_menu_pointer_capture_clear();
        return true;
    }

    return false;
}
