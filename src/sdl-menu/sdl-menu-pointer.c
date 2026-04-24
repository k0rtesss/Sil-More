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

#include "platform-config.h"
#include "sdl-scene-menu.h"

#define SDL_MENU_TOUCH_LONG_PRESS_MS 450
#define SDL_MENU_TOUCH_SCROLL_PX 18.0f
#define SDL_MENU_TOUCH_MOVE_CANCEL_PX 12.0f
#define SDL_MENU_TOOLTIP_DELAY_MS 550

typedef struct sdl_menu_pointer_capture {
    bool active;
    bool moved;
    bool long_press_possible;
    bool long_press_sent;
    u16b device;
    u16b button;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
    Uint64 start_time_ns;
    u16b scene_kind;
    u16b panel_index;
    u16b target_kind;
    s16b target_id;
} sdl_menu_pointer_capture;

typedef struct sdl_menu_tooltip_state {
    bool active;
    bool pressed;
    Uint64 hover_start_ns;
    sdl_menu_hit_target target;
} sdl_menu_tooltip_state;

typedef struct sdl_menu_panel_drag_capture {
    bool active;
    bool changed;
    u16b device;
    u16b button;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    int start_offset_x;
    int start_offset_y;
    int current_offset_x;
    int current_offset_y;
    char overlay_id[SDL_OVERLAY_PANEL_ID_LEN];
} sdl_menu_panel_drag_capture;

static sdl_menu_pointer_capture g_menu_pointer_capture;
static sdl_menu_tooltip_state g_menu_tooltip_state;
static sdl_menu_panel_drag_capture g_menu_panel_drag_capture;
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

static bool sdl_menu_pointer_same_tooltip_target(
    const sdl_menu_hit_target* target)
{
    return target && g_menu_tooltip_state.active
        && g_menu_tooltip_state.target.scene_kind == target->scene_kind
        && g_menu_tooltip_state.target.panel_index == target->panel_index
        && g_menu_tooltip_state.target.kind == target->kind
        && g_menu_tooltip_state.target.id == target->id;
}

static void sdl_menu_pointer_tooltip_clear(void)
{
    memset(&g_menu_tooltip_state, 0, sizeof(g_menu_tooltip_state));
}

static void sdl_menu_pointer_tooltip_note(
    const sdl_menu_hit_target* target, bool pressed)
{
    if (!target || pressed || !target->tooltip[0])
    {
        sdl_menu_pointer_tooltip_clear();
        return;
    }

    if (sdl_menu_pointer_same_tooltip_target(target))
    {
        g_menu_tooltip_state.pressed = false;
        return;
    }

    memset(&g_menu_tooltip_state, 0, sizeof(g_menu_tooltip_state));
    g_menu_tooltip_state.active = true;
    g_menu_tooltip_state.hover_start_ns = SDL_GetTicksNS();
    g_menu_tooltip_state.target = *target;
}

static void sdl_menu_pointer_target_ref(
    const sdl_menu_hit_target* target, app_ui_widget_ref* out_ref)
{
    if (!out_ref)
        return;

    app_ui_widget_ref_clear(out_ref);
    if (!target)
        return;

    out_ref->scene_kind = target->scene_kind;
    out_ref->panel_index = target->panel_index;
    out_ref->panel_layer = target->panel_layer;
    out_ref->panel_style = target->panel_style;
    out_ref->target_kind = target->kind;
    out_ref->widget_id = target->id;
    out_ref->action_key = target->action_key;
    out_ref->role = target->role;
    out_ref->action = target->action;
    out_ref->flags = target->flags;
    out_ref->focus_area = target->focus_area;
    out_ref->state_flags = target->state_flags;
    out_ref->focus_order = target->focus_order;
    out_ref->owner_id = target->owner_id;
    out_ref->payload0 = target->payload0;
    out_ref->payload1 = target->payload1;
    SDL_strlcpy(out_ref->label, target->label, sizeof(out_ref->label));
    SDL_strlcpy(out_ref->tooltip, target->tooltip,
        sizeof(out_ref->tooltip));
}

static void sdl_menu_pointer_note_focus(const sdl_menu_hit_target* target,
    u16b device, bool pressed)
{
    app_session* session = app_session_current();
    app_ui_widget_ref ref;
    u16b reason;

    if (!session || !target)
        return;

    sdl_menu_pointer_target_ref(target, &ref);
    if (device == APP_INPUT_DEVICE_TOUCH)
        reason = pressed ? APP_UI_FOCUS_REASON_TOUCH_PRESS
            : APP_UI_FOCUS_REASON_POINTER_HOVER;
    else
        reason = pressed ? APP_UI_FOCUS_REASON_POINTER_PRESS
            : APP_UI_FOCUS_REASON_POINTER_HOVER;

    app_session_note_ui_focus(session, &ref, reason, pressed);
    if (device == APP_INPUT_DEVICE_POINTER)
        sdl_menu_pointer_tooltip_note(target, pressed);
}

static bool sdl_menu_pointer_target_matches_capture(
    const sdl_menu_hit_target* target)
{
    return target && g_menu_pointer_capture.active
        && target->scene_kind == g_menu_pointer_capture.scene_kind
        && target->panel_index == g_menu_pointer_capture.panel_index
        && target->kind == g_menu_pointer_capture.target_kind
        && target->id == g_menu_pointer_capture.target_id;
}

static bool sdl_menu_pointer_submit_target_command(
    const sdl_menu_hit_target* target, u16b command_kind, u16b device,
    u16b input_type, u16b input_flags, float x, float y, float dx, float dy,
    s32b scroll_x, s32b scroll_y, u16b button, u16b clicks)
{
    app_session* session = app_session_current();
    app_ui_command command;

    if (!target)
        return false;

    app_ui_command_clear(&command);
    command.kind = command_kind;
    if (command.kind == APP_UI_COMMAND_KIND_NONE)
    {
        command.kind = app_ui_command_kind_from_widget_action(target->action);
        if (command.kind == APP_UI_COMMAND_KIND_NONE)
            command.kind = APP_UI_COMMAND_KIND_ACTIVATE;
    }
    command.device = device;
    command.input_type = input_type;
    command.input_flags = input_flags;
    command.modifiers = sdl_menu_pointer_modifiers_from_keymod(
        SDL_GetModState());
    command.x = (s32b)x;
    command.y = (s32b)y;
    command.dx = (s32b)dx;
    command.dy = (s32b)dy;
    command.scroll_x = scroll_x;
    command.scroll_y = scroll_y;
    command.button = button;
    command.clicks = clicks;
    command.sequence = ++g_structured_input_sequence;
    command.timestamp_usec = SDL_GetTicksNS() / 1000ULL;
    sdl_menu_pointer_target_ref(target, &command.target);

    log_trace("ui command: kind=%u device=%u target kind=%u id=%d action=%u key=%d label='%s'",
        (unsigned)command.kind, (unsigned)device,
        (unsigned)target->kind, (int)target->id,
        (unsigned)target->action, (int)target->action_key,
        target->label);
    if (session && app_session_submit_ui_command(session, &command))
        return true;

    if (target->action_key)
    {
        sdl_submit_legacy_input_byte(target->action_key);
        return true;
    }

    return true;
}

static bool sdl_menu_pointer_activate_target(
    const sdl_menu_hit_target* target, u16b device, float x, float y,
    u16b button, u16b clicks)
{
    return sdl_menu_pointer_submit_target_command(target,
        APP_UI_COMMAND_KIND_NONE, device, APP_INPUT_TYPE_POINTER_BUTTON,
        APP_INPUT_FLAG_RELEASE, x, y, 0.0f, 0.0f, 0, 0, button, clicks);
}

static bool sdl_menu_pointer_scroll_target(
    const sdl_menu_hit_target* target, u16b device, float x, float y,
    float dx, float dy, s32b scroll_x, s32b scroll_y)
{
    if (!target || (!scroll_x && !scroll_y))
        return false;

    return sdl_menu_pointer_submit_target_command(target,
        APP_UI_COMMAND_KIND_SCROLL, device,
        (device == APP_INPUT_DEVICE_TOUCH)
            ? APP_INPUT_TYPE_POINTER_MOTION
            : APP_INPUT_TYPE_POINTER_WHEEL,
        0,
        x, y, dx, dy, scroll_x, scroll_y, 0, 0);
}

static u16b sdl_menu_pointer_context_command_kind(
    const sdl_menu_hit_target* target)
{
    if (target && target->action == APP_UI_WIDGET_ACTION_INSPECT)
        return APP_UI_COMMAND_KIND_INSPECT;

    return APP_UI_COMMAND_KIND_CONTEXT;
}

static bool sdl_menu_pointer_submit_long_press(Uint64 now_ns)
{
    const sdl_menu_hit_target* target;

    if (!g_menu_pointer_capture.active
        || g_menu_pointer_capture.device != APP_INPUT_DEVICE_TOUCH
        || !g_menu_pointer_capture.long_press_possible
        || g_menu_pointer_capture.long_press_sent)
    {
        return false;
    }

    if (now_ns - g_menu_pointer_capture.start_time_ns
        < (Uint64)SDL_MENU_TOUCH_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    target = sdl_menu_hit_test(g_menu_pointer_capture.last_x,
        g_menu_pointer_capture.last_y);
    if (!sdl_menu_pointer_target_matches_capture(target))
        return false;

    g_menu_pointer_capture.long_press_sent = true;
    sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_TOUCH, true);
    return sdl_menu_pointer_submit_target_command(target,
        sdl_menu_pointer_context_command_kind(target), APP_INPUT_DEVICE_TOUCH,
        APP_INPUT_TYPE_POINTER_BUTTON,
        APP_INPUT_FLAG_PRESS | APP_INPUT_FLAG_LONG_PRESS,
        g_menu_pointer_capture.last_x, g_menu_pointer_capture.last_y, 0.0f,
        0.0f, 0, 0, SDL_BUTTON_LEFT, 1);
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

int sdl_menu_pointer_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_menu_pointer_capture.active
        || g_menu_pointer_capture.device != APP_INPUT_DEVICE_TOUCH
        || !g_menu_pointer_capture.long_press_possible
        || g_menu_pointer_capture.long_press_sent)
    {
        return -1;
    }

    elapsed = now_ns - g_menu_pointer_capture.start_time_ns;
    if (elapsed >= (Uint64)SDL_MENU_TOUCH_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return SDL_MENU_TOUCH_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_menu_pointer_flush_pending_long_press(Uint64 now_ns)
{
    return sdl_menu_pointer_submit_long_press(now_ns);
}

bool sdl_menu_gamepad_submit_focus_delta(int dy, int dx, u16b input_type,
    u16b control)
{
    const sdl_menu_hit_target* target = sdl_menu_hit_focus_delta(dy, dx);

    if (!target)
        target = sdl_menu_hit_current_focus();
    if (!target)
        return false;

    return sdl_menu_pointer_submit_target_command(target,
        APP_UI_COMMAND_KIND_FOCUS, APP_INPUT_DEVICE_GAMEPAD, input_type,
        APP_INPUT_FLAG_PRESS, 0.0f, 0.0f, (float)dx, (float)dy, 0, 0,
        control, 0);
}

bool sdl_menu_gamepad_submit_current(u16b command_kind, u16b input_type,
    u16b control)
{
    const sdl_menu_hit_target* target = sdl_menu_hit_current_focus();

    if (!target)
        return false;

    return sdl_menu_pointer_submit_target_command(target, command_kind,
        APP_INPUT_DEVICE_GAMEPAD, input_type, APP_INPUT_FLAG_PRESS, 0.0f,
        0.0f, 0.0f, 0.0f, 0, 0, control, 0);
}

void sdl_menu_pointer_render_tooltip(int canvas_w, int canvas_h)
{
    TTF_Font* font;
    const sdl_ui_style* style = sdl_ui_style_for_panel(
        g_menu_tooltip_state.target.panel_style);
    SDL_FRect anchor;
    SDL_FRect box;
    SDL_Color fill = style->panel_fill_alt;
    SDL_Color border = style->panel_border;
    SDL_Color text_color = style->text;
    int origin_x = 0;
    int origin_y = 0;
    int pad_x;
    int pad_y;
    int line_h;
    int text_w;

    if (!g_menu_tooltip_state.active || g_menu_tooltip_state.pressed)
        return;
    if (!g_menu_tooltip_state.target.tooltip[0])
        return;
    if (SDL_GetTicksNS() - g_menu_tooltip_state.hover_start_ns
        < (Uint64)SDL_MENU_TOOLTIP_DELAY_MS * 1000000ULL)
    {
        return;
    }

    line_h = sdl_menu_scale_px(14.0f);
    font = sdl_ui_font_for_height(line_h);
    if (!font)
        return;

    text_w = sdl_menu_measure_text(font, g_menu_tooltip_state.target.tooltip);
    pad_x = sdl_menu_scale_px(8.0f);
    pad_y = sdl_menu_scale_px(5.0f);
    sdl_menu_hit_origin(&origin_x, &origin_y);

    anchor = g_menu_tooltip_state.target.rect;
    anchor.x -= (float)origin_x;
    anchor.y -= (float)origin_y;

    box.w = (float)(text_w + pad_x * 2);
    box.h = (float)(line_h + pad_y * 2);
    if (box.w > (float)(canvas_w - pad_x * 2))
        box.w = (float)(canvas_w - pad_x * 2);
    box.x = anchor.x;
    box.y = anchor.y + anchor.h + sdl_menu_scale_px(6.0f);
    if (box.x + box.w > (float)(canvas_w - pad_x))
        box.x = (float)(canvas_w - pad_x) - box.w;
    if (box.x < (float)pad_x)
        box.x = (float)pad_x;
    if (box.y + box.h > (float)(canvas_h - pad_y))
        box.y = anchor.y - box.h - sdl_menu_scale_px(6.0f);
    if (box.y < (float)pad_y)
        box.y = (float)pad_y;

    fill.a = 242;
    border.a = 220;
    sdl_menu_fill_rect(&box, fill);
    sdl_menu_draw_rect(&box, border);
    {
        SDL_Rect clip = {
            (int)(box.x + (float)pad_x),
            (int)(box.y + (float)pad_y),
            MAX(0, (int)box.w - pad_x * 2),
            MAX(0, (int)box.h - pad_y * 2)
        };

        SDL_SetRenderClipRect(g_state.renderer, &clip);
        sdl_menu_render_text(font, box.x + (float)pad_x,
            box.y + (float)pad_y, line_h, text_color,
            g_menu_tooltip_state.target.tooltip);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }
}

static bool sdl_menu_panel_drag_target(const sdl_menu_hit_target* target)
{
    return target
        && target->role == APP_UI_WIDGET_ROLE_PANEL_DRAG_HANDLE
        && target->action == APP_UI_WIDGET_ACTION_DRAG;
}

static void sdl_menu_panel_overlay_ids_for_target(
    const sdl_menu_hit_target* target, char* overlay_id, size_t overlay_id_sz,
    char* legacy_id, size_t legacy_id_sz)
{
    if (overlay_id && overlay_id_sz)
        overlay_id[0] = '\0';
    if (legacy_id && legacy_id_sz)
        legacy_id[0] = '\0';
    if (!target)
        return;

    sdl_menu_overlay_panel_id(target->scene_kind, target->panel_index,
        target->panel_style, target->label, overlay_id, overlay_id_sz);
    sdl_menu_overlay_panel_id(target->scene_kind, target->panel_index,
        target->panel_style, NULL, legacy_id, legacy_id_sz);
}

static void sdl_menu_panel_reset_offset(const sdl_menu_hit_target* target)
{
    char overlay_id[SDL_OVERLAY_PANEL_ID_LEN];
    char legacy_overlay_id[SDL_OVERLAY_PANEL_ID_LEN];

    if (!sdl_menu_panel_drag_target(target))
        return;

    sdl_menu_panel_overlay_ids_for_target(target, overlay_id,
        sizeof(overlay_id), legacy_overlay_id, sizeof(legacy_overlay_id));
    if (overlay_id[0])
        sdl_menu_overlay_panel_set_offset(overlay_id, 0, 0, false);
    if (legacy_overlay_id[0] && !streq(legacy_overlay_id, overlay_id))
        sdl_menu_overlay_panel_set_offset(legacy_overlay_id, 0, 0, false);
    (void)save_pane_config_to_json();
    g_state.need_present = true;
}

static void sdl_menu_panel_drag_begin(const sdl_menu_hit_target* target,
    u16b device, u16b button, SDL_FingerID finger_id, float x, float y)
{
    int offset_x = 0;
    int offset_y = 0;
    bool pinned = false;
    char legacy_overlay_id[SDL_OVERLAY_PANEL_ID_LEN];

    if (!target)
        return;

    memset(&g_menu_panel_drag_capture, 0, sizeof(g_menu_panel_drag_capture));
    g_menu_panel_drag_capture.active = true;
    g_menu_panel_drag_capture.device = device;
    g_menu_panel_drag_capture.button = button;
    g_menu_panel_drag_capture.finger_id = finger_id;
    g_menu_panel_drag_capture.start_x = x;
    g_menu_panel_drag_capture.start_y = y;
    sdl_menu_panel_overlay_ids_for_target(target,
        g_menu_panel_drag_capture.overlay_id,
        sizeof(g_menu_panel_drag_capture.overlay_id),
        legacy_overlay_id, sizeof(legacy_overlay_id));
    (void)sdl_menu_overlay_panel_get_offset(
        g_menu_panel_drag_capture.overlay_id, &offset_x, &offset_y, &pinned);
    if (!pinned && !streq(g_menu_panel_drag_capture.overlay_id,
            legacy_overlay_id))
    {
        (void)sdl_menu_overlay_panel_get_offset(
            legacy_overlay_id, &offset_x, &offset_y, &pinned);
        if (pinned)
        {
            sdl_menu_overlay_panel_set_offset(
                g_menu_panel_drag_capture.overlay_id, offset_x, offset_y,
                true);
        }
    }
    if (!pinned)
    {
        offset_x = 0;
        offset_y = 0;
    }
    g_menu_panel_drag_capture.start_offset_x = offset_x;
    g_menu_panel_drag_capture.start_offset_y = offset_y;
    g_menu_panel_drag_capture.current_offset_x = offset_x;
    g_menu_panel_drag_capture.current_offset_y = offset_y;
}

static void sdl_menu_panel_drag_clear(void)
{
    memset(&g_menu_panel_drag_capture, 0, sizeof(g_menu_panel_drag_capture));
}

static bool sdl_menu_panel_drag_handle_event(const SDL_Event* ev)
{
    bool matching_mouse;
    bool matching_touch;
    float x;
    float y;

    if (!g_menu_panel_drag_capture.active || !ev)
        return false;

    matching_mouse = g_menu_panel_drag_capture.device == APP_INPUT_DEVICE_POINTER
        && (ev->type == SDL_EVENT_MOUSE_MOTION
            || ev->type == SDL_EVENT_MOUSE_BUTTON_UP);
    matching_touch = g_menu_panel_drag_capture.device == APP_INPUT_DEVICE_TOUCH
        && (ev->type == SDL_EVENT_FINGER_MOTION
            || ev->type == SDL_EVENT_FINGER_UP
            || ev->type == SDL_EVENT_FINGER_CANCELED);

    if (!matching_mouse && !matching_touch)
        return true;
    if (matching_mouse && ev->type == SDL_EVENT_MOUSE_BUTTON_UP
        && ev->button.button != g_menu_panel_drag_capture.button)
    {
        return true;
    }
    if (matching_touch
        && ev->tfinger.fingerID != g_menu_panel_drag_capture.finger_id)
    {
        return true;
    }

    if (!sdl_menu_pointer_event_xy(ev, &x, &y)) {
        sdl_menu_panel_drag_clear();
        return true;
    }

    if (ev->type == SDL_EVENT_MOUSE_MOTION
        || ev->type == SDL_EVENT_FINGER_MOTION)
    {
        int next_x = g_menu_panel_drag_capture.start_offset_x
            + (int)(x - g_menu_panel_drag_capture.start_x);
        int next_y = g_menu_panel_drag_capture.start_offset_y
            + (int)(y - g_menu_panel_drag_capture.start_y);

        sdl_menu_overlay_panel_set_offset(
            g_menu_panel_drag_capture.overlay_id, next_x, next_y, true);
        g_menu_panel_drag_capture.current_offset_x = next_x;
        g_menu_panel_drag_capture.current_offset_y = next_y;
        g_menu_panel_drag_capture.changed = true;
        g_state.need_present = true;
        return true;
    }

    if (g_menu_panel_drag_capture.changed)
    {
        sdl_menu_overlay_panel_set_offset(
            g_menu_panel_drag_capture.overlay_id,
            g_menu_panel_drag_capture.current_offset_x,
            g_menu_panel_drag_capture.current_offset_y, true);
        (void)save_pane_config_to_json();
    }

    sdl_menu_panel_drag_clear();
    g_state.need_present = true;
    return true;
}

bool sdl_menu_pointer_handle_event(const SDL_Event* ev)
{
    const sdl_menu_hit_target* target;
    float x;
    float y;

    if (!ev)
        return false;

    if (sdl_menu_panel_drag_handle_event(ev))
        return true;

    if (ev->type == SDL_EVENT_MOUSE_MOTION)
    {
        target = sdl_menu_hit_test(ev->motion.x, ev->motion.y);
        if (target)
            sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_POINTER,
                false);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_MOTION, 0, ev->motion.x, ev->motion.y,
            ev->motion.xrel, ev->motion.yrel, 0, 0);
        return false;
    }

    if (ev->type == SDL_EVENT_MOUSE_WHEEL)
    {
        float mx = 0.0f;
        float my = 0.0f;

        (void)SDL_GetMouseState(&mx, &my);
        target = sdl_menu_hit_test(mx, my);
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_POINTER,
            APP_INPUT_TYPE_POINTER_WHEEL, 0, 0.0f, 0.0f, (float)ev->wheel.x,
            (float)ev->wheel.y, 0, 0);
        if (target)
        {
            (void)sdl_menu_pointer_scroll_target(target,
                APP_INPUT_DEVICE_POINTER, mx, my, 0.0f, 0.0f,
                (s32b)ev->wheel.x, (s32b)ev->wheel.y);
            return true;
        }
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
        if (ev->button.button == SDL_BUTTON_RIGHT
            && sdl_menu_panel_drag_target(target))
        {
            sdl_menu_panel_reset_offset(target);
            sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_POINTER, true);
            return true;
        }
        if (ev->button.button == SDL_BUTTON_LEFT
            && sdl_menu_panel_drag_target(target))
        {
            sdl_menu_panel_drag_begin(target, APP_INPUT_DEVICE_POINTER,
                ev->button.button, 0, x, y);
            sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_POINTER, true);
            return true;
        }
        g_menu_pointer_capture.active = true;
        g_menu_pointer_capture.moved = false;
        g_menu_pointer_capture.long_press_possible = false;
        g_menu_pointer_capture.long_press_sent = false;
        g_menu_pointer_capture.device = APP_INPUT_DEVICE_POINTER;
        g_menu_pointer_capture.button = ev->button.button;
        g_menu_pointer_capture.start_x = x;
        g_menu_pointer_capture.start_y = y;
        g_menu_pointer_capture.last_x = x;
        g_menu_pointer_capture.last_y = y;
        g_menu_pointer_capture.start_time_ns = SDL_GetTicksNS();
        g_menu_pointer_capture.scene_kind = target->scene_kind;
        g_menu_pointer_capture.panel_index = target->panel_index;
        g_menu_pointer_capture.target_kind = target->kind;
        g_menu_pointer_capture.target_id = target->id;
        sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_POINTER, true);
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
        {
            if (ev->button.button == SDL_BUTTON_RIGHT)
            {
                (void)sdl_menu_pointer_submit_target_command(target,
                    sdl_menu_pointer_context_command_kind(target),
                    APP_INPUT_DEVICE_POINTER, APP_INPUT_TYPE_POINTER_BUTTON,
                    APP_INPUT_FLAG_RELEASE, x, y, 0.0f, 0.0f, 0, 0,
                    ev->button.button, ev->button.clicks);
            }
            else
            {
                (void)sdl_menu_pointer_activate_target(target,
                    APP_INPUT_DEVICE_POINTER, x, y, ev->button.button,
                    ev->button.clicks);
            }
        }
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
        if (sdl_menu_panel_drag_target(target))
        {
            sdl_menu_panel_drag_begin(target, APP_INPUT_DEVICE_TOUCH,
                SDL_BUTTON_LEFT, ev->tfinger.fingerID, x, y);
            sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_TOUCH, true);
            return true;
        }
        g_menu_pointer_capture.active = true;
        g_menu_pointer_capture.moved = false;
        g_menu_pointer_capture.long_press_possible = true;
        g_menu_pointer_capture.long_press_sent = false;
        g_menu_pointer_capture.device = APP_INPUT_DEVICE_TOUCH;
        g_menu_pointer_capture.button = SDL_BUTTON_LEFT;
        g_menu_pointer_capture.finger_id = ev->tfinger.fingerID;
        g_menu_pointer_capture.start_x = x;
        g_menu_pointer_capture.start_y = y;
        g_menu_pointer_capture.last_x = x;
        g_menu_pointer_capture.last_y = y;
        g_menu_pointer_capture.start_time_ns = SDL_GetTicksNS();
        g_menu_pointer_capture.scene_kind = target->scene_kind;
        g_menu_pointer_capture.panel_index = target->panel_index;
        g_menu_pointer_capture.target_kind = target->kind;
        g_menu_pointer_capture.target_id = target->id;
        sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_TOUCH, true);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_MOTION)
    {
        float dx;
        float dy;

        if (!g_menu_pointer_capture.active
            || g_menu_pointer_capture.device != APP_INPUT_DEVICE_TOUCH
            || g_menu_pointer_capture.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_menu_pointer_event_xy(ev, &x, &y))
            return true;
        dx = x - g_menu_pointer_capture.last_x;
        dy = y - g_menu_pointer_capture.last_y;
        (void)sdl_menu_pointer_submit_input(APP_INPUT_DEVICE_TOUCH,
            APP_INPUT_TYPE_POINTER_MOTION, 0, x, y, dx, dy,
            SDL_BUTTON_LEFT, 1);
        target = sdl_menu_hit_test(x, y);
        if (target)
            sdl_menu_pointer_note_focus(target, APP_INPUT_DEVICE_TOUCH, true);
        if (ABS((int)(x - g_menu_pointer_capture.start_x))
                > (int)SDL_MENU_TOUCH_MOVE_CANCEL_PX
            || ABS((int)(y - g_menu_pointer_capture.start_y))
                > (int)SDL_MENU_TOUCH_MOVE_CANCEL_PX)
        {
            g_menu_pointer_capture.moved = true;
            g_menu_pointer_capture.long_press_possible = false;
        }
        if (target && ABS((int)dy) >= (int)SDL_MENU_TOUCH_SCROLL_PX)
        {
            s32b scroll_y = (dy > 0.0f) ? 1 : -1;

            (void)sdl_menu_pointer_scroll_target(target,
                APP_INPUT_DEVICE_TOUCH, x, y, dx, dy, 0, scroll_y);
        }
        g_menu_pointer_capture.last_x = x;
        g_menu_pointer_capture.last_y = y;
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
        if (activate && !g_menu_pointer_capture.long_press_sent
            && !g_menu_pointer_capture.moved
            && sdl_menu_pointer_target_matches_capture(target))
        {
            (void)sdl_menu_pointer_activate_target(target,
                APP_INPUT_DEVICE_TOUCH, x, y, SDL_BUTTON_LEFT, 1);
        }
        sdl_menu_pointer_capture_clear();
        return true;
    }

    return false;
}
