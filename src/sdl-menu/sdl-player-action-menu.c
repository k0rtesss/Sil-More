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

#include "../sdl-main-internal.h"
#include "sdl-player-action-menu.h"

#include <math.h>

typedef enum sdl_player_action_kind {
    SDL_PLAYER_ACTION_NONE = 0,
    SDL_PLAYER_ACTION_WAIT = 1,
    SDL_PLAYER_ACTION_USE = 2,
    SDL_PLAYER_ACTION_STEALTH = 3,
    SDL_PLAYER_ACTION_SING = 4,
    SDL_PLAYER_ACTION_EXCHANGE = 5,
    SDL_PLAYER_ACTION_FLETCH = 6,
    SDL_PLAYER_ACTION_EXAMINE = 7,
    SDL_PLAYER_ACTION_ACTIVATE = 8,
    SDL_PLAYER_ACTION_HORN = 9
} sdl_player_action_kind;

typedef struct sdl_player_action_entry {
    int kind;
    int command;
    cptr label;
    SDL_FRect rect;
} sdl_player_action_entry;

typedef struct sdl_player_action_menu_state {
    bool active;
    int hover_kind;
    bool press_active;
    bool press_mouse;
    bool press_gamepad;
    bool press_secondary;
    SDL_FingerID press_finger_id;
    int press_button;
    int press_kind;
    float press_start_x;
    float press_start_y;
    Uint64 press_start_time_ns;
} sdl_player_action_menu_state;

enum {
    SDL_PLAYER_ACTION_MAX = 9
};

static sdl_player_action_menu_state g_player_action_menu;

static bool sdl_player_action_menu_scene_ready(u16b* out_reason)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;
    const app_wait_state* wait_state;
    u16b reason;

    if (!character_dungeon || !session || !g_views[0].ready)
        return false;
    if (app_session_input_capture_active(session))
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;
    if (!app_session_dungeon_snapshot(session))
        return false;

    wait_state = app_session_wait_state(session);
    reason = wait_state ? wait_state->reason : APP_WAIT_REASON_NONE;
    if (out_reason)
        *out_reason = reason;
    return true;
}

static bool sdl_player_action_menu_commands_allowed(void)
{
    u16b reason;

    if (!sdl_player_action_menu_scene_ready(&reason))
        return false;

    return reason == APP_WAIT_REASON_NONE
        || reason == APP_WAIT_REASON_COMMAND_INPUT;
}

static void sdl_player_action_menu_window_point_to_pixels(float* x, float* y)
{
    int window_w = 0;
    int window_h = 0;
    int pixel_w = 0;
    int pixel_h = 0;

    if (!g_state.window || !x || !y)
        return;

    SDL_GetWindowSize(g_state.window, &window_w, &window_h);
    SDL_GetWindowSizeInPixels(g_state.window, &pixel_w, &pixel_h);
    if (window_w <= 0 || window_h <= 0 || pixel_w <= 0 || pixel_h <= 0)
        return;

    if (window_w != pixel_w)
        *x = *x * (float)pixel_w / (float)window_w;
    if (window_h != pixel_h)
        *y = *y * (float)pixel_h / (float)window_h;
}

static bool sdl_player_action_menu_event_xy(const SDL_Event* ev, float* out_x,
    float* out_y)
{
    int window_w = 0;
    int window_h = 0;

    if (!ev || !out_x || !out_y)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        *out_x = ev->motion.x;
        *out_y = ev->motion.y;
        sdl_player_action_menu_window_point_to_pixels(out_x, out_y);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        *out_x = ev->button.x;
        *out_y = ev->button.y;
        sdl_player_action_menu_window_point_to_pixels(out_x, out_y);
        return true;

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
        if (!g_state.window
            || ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
        {
            return false;
        }
        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        if (window_w <= 0 || window_h <= 0)
            return false;
        *out_x = ev->tfinger.x * (float)window_w;
        *out_y = ev->tfinger.y * (float)window_h;
        return true;

    default:
        return false;
    }
}

static float sdl_player_action_menu_clampf(float value, float min_value,
    float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static bool sdl_player_action_menu_has_equipped_staff(void)
{
    return inventory[INVEN_STAFF].k_idx
        && inventory[INVEN_STAFF].tval == TV_STAFF;
}

static bool sdl_player_action_menu_has_equipped_horn(void)
{
    return inventory[INVEN_HORN].k_idx
        && inventory[INVEN_HORN].tval == TV_HORN;
}

static bool sdl_player_action_menu_has_singable_song(void)
{
    int i;

    if (!p_ptr)
        return false;

    for (i = 0; i < SNG_MAX; i++)
    {
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;
        if (p_ptr->active_ability[S_SNG][i])
            return true;
    }

    return false;
}

static bool sdl_player_action_menu_has_floor_item_underfoot(void)
{
    int floor_list[MAX_FLOOR_STACK];

    if (!p_ptr)
        return false;

    return scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00) > 0;
}

static void sdl_player_action_menu_add_entry(sdl_player_action_entry* entries,
    int* count, int kind, int command, cptr label)
{
    if (!entries || !count || *count >= SDL_PLAYER_ACTION_MAX)
        return;

    entries[*count].kind = kind;
    entries[*count].command = command;
    entries[*count].label = label;
    entries[*count].rect = (SDL_FRect){ 0.0f, 0.0f, 0.0f, 0.0f };
    (*count)++;
}

static int sdl_player_action_menu_collect(sdl_player_action_entry* entries)
{
    int count = 0;

    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_WAIT,
        'z', "Wait");
    sdl_player_action_menu_add_entry(entries, &count, SDL_PLAYER_ACTION_USE,
        'u', "Use");
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_STEALTH, 'S', "Stealth");
    if (sdl_player_action_menu_has_singable_song())
    {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_SING, 's', "Sing");
    }
    if (p_ptr && p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
    {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_EXCHANGE, 'X', "Xchg");
    }
    if (p_ptr && p_ptr->active_ability[S_ARC][ARC_FLETCHERY])
    {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_FLETCH, '-', "Fletch");
    }
    sdl_player_action_menu_add_entry(entries, &count,
        SDL_PLAYER_ACTION_EXAMINE, 'x', "Desc");
    if (sdl_player_action_menu_has_equipped_staff())
    {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_ACTIVATE, 'a', "Staff");
    }
    if (sdl_player_action_menu_has_equipped_horn())
    {
        sdl_player_action_menu_add_entry(entries, &count,
            SDL_PLAYER_ACTION_HORN, 'p', "Horn");
    }

    return count;
}

static bool sdl_player_action_menu_kind_supports_secondary(int kind)
{
    return kind == SDL_PLAYER_ACTION_WAIT
        || kind == SDL_PLAYER_ACTION_USE
        || kind == SDL_PLAYER_ACTION_EXAMINE;
}

static void sdl_player_action_menu_slot_offset(int slot, int count,
    float* out_x, float* out_y)
{
    static const float offsets[SDL_PLAYER_ACTION_MAX + 1]
        [SDL_PLAYER_ACTION_MAX][2] = {
            { { 0.0f, 0.0f } },
            { { 0.0f, -1.0f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f } },
            { { -0.78f, -0.78f }, { 0.78f, -0.78f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, 0.0f },
              { 0.78f, 0.78f }, { 0.0f, 1.0f },
              { -0.78f, 0.78f }, { -1.0f, 0.0f } },
            { { -0.78f, -0.78f }, { 0.0f, -1.0f },
              { 0.78f, -0.78f }, { 1.0f, -0.34f },
              { 1.0f, 0.34f }, { 0.78f, 0.78f },
              { 0.0f, 1.0f }, { -0.78f, 0.78f },
              { -1.0f, 0.0f } }
        };
    float x = 0.0f;
    float y = -1.0f;

    if (count < 1)
        count = 1;
    if (count > SDL_PLAYER_ACTION_MAX)
        count = SDL_PLAYER_ACTION_MAX;
    if (slot >= 0 && slot < count)
    {
        x = offsets[count][slot][0];
        y = offsets[count][slot][1];
    }

    if (out_x)
        *out_x = x;
    if (out_y)
        *out_y = y;
}

static bool sdl_player_action_menu_layout_for_origin(
    const sdl_view* main_view, const app_dungeon_snapshot* snapshot,
    int origin_x, int origin_y, sdl_player_action_entry* entries,
    int* out_count)
{
    SDL_FRect player_rect;
    float bounds_w;
    float bounds_h;
    float button_w;
    float button_h;
    float larger;
    float radius;
    float center_x;
    float center_y;
    int count;
    int i;

    if (!main_view || !snapshot || !entries || !out_count || !p_ptr)
        return false;
    if (!sdl_scene_dungeon_map_cell_rect(main_view, snapshot, p_ptr->py,
            p_ptr->px, &player_rect))
    {
        return false;
    }

    player_rect.x += (float)origin_x;
    player_rect.y += (float)origin_y;
    bounds_w = (float)(main_view->cols * main_view->cell_w);
    bounds_h = (float)(main_view->rows * main_view->cell_h);
    if (bounds_w <= 44.0f || bounds_h <= 44.0f)
        return false;

    count = sdl_player_action_menu_collect(entries);
    if (count <= 0)
        return false;

    button_w = sdl_player_action_menu_clampf((float)main_view->cell_w * 6.8f,
        74.0f, 108.0f);
    button_h = sdl_player_action_menu_clampf((float)main_view->cell_h * 2.65f,
        42.0f, 62.0f);
    if (button_w > bounds_w - 6.0f)
        button_w = bounds_w - 6.0f;
    if (button_h > bounds_h - 6.0f)
        button_h = bounds_h - 6.0f;
    if (button_w <= 0.0f || button_h <= 0.0f)
        return false;

    larger = (button_w > button_h) ? button_w : button_h;
    radius = sdl_player_action_menu_clampf(larger * 1.34f, 84.0f, 138.0f);
    center_x = player_rect.x + player_rect.w * 0.5f;
    center_y = player_rect.y + player_rect.h * 0.5f;

    for (i = 0; i < count; i++)
    {
        float dx;
        float dy;
        SDL_FRect rect;

        sdl_player_action_menu_slot_offset(i, count, &dx, &dy);
        rect.w = button_w;
        rect.h = button_h;
        rect.x = center_x + dx * radius - rect.w * 0.5f;
        rect.y = center_y + dy * radius - rect.h * 0.5f;
        rect.x = sdl_player_action_menu_clampf(rect.x, (float)origin_x,
            (float)origin_x + bounds_w - rect.w);
        rect.y = sdl_player_action_menu_clampf(rect.y, (float)origin_y,
            (float)origin_y + bounds_h - rect.h);
        entries[i].rect = rect;
    }

    *out_count = count;
    return true;
}

static bool sdl_player_action_menu_layout_for_window(
    sdl_player_action_entry* entries, int* out_count)
{
    const sdl_view* main_view = &g_views[PANE_MAIN];

    return sdl_player_action_menu_layout_for_origin(main_view,
        app_session_dungeon_snapshot(app_session_current()),
        main_view->rect.x + main_view->margin_x,
        main_view->rect.y + main_view->margin_y, entries, out_count);
}

static bool sdl_player_action_menu_layout_for_canvas(
    const sdl_view* main_view, const app_dungeon_snapshot* snapshot,
    sdl_player_action_entry* entries, int* out_count)
{
    return sdl_player_action_menu_layout_for_origin(main_view, snapshot,
        0, 0, entries, out_count);
}

static bool sdl_player_action_menu_point_in_rect(const SDL_FRect* rect,
    float x, float y)
{
    return rect && x >= rect->x && y >= rect->y
        && x < rect->x + rect->w && y < rect->y + rect->h;
}

static int sdl_player_action_menu_kind_at(float window_x, float window_y)
{
    sdl_player_action_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;
    int i;

    if (!sdl_player_action_menu_layout_for_window(entries, &count))
        return SDL_PLAYER_ACTION_NONE;

    for (i = count - 1; i >= 0; i--)
    {
        if (sdl_player_action_menu_point_in_rect(&entries[i].rect,
                window_x, window_y))
        {
            return entries[i].kind;
        }
    }

    return SDL_PLAYER_ACTION_NONE;
}

static void sdl_player_action_menu_cancel_press(void)
{
    g_player_action_menu.press_active = false;
    g_player_action_menu.press_mouse = false;
    g_player_action_menu.press_gamepad = false;
    g_player_action_menu.press_secondary = false;
    g_player_action_menu.press_finger_id = 0;
    g_player_action_menu.press_button = -1;
    g_player_action_menu.press_kind = SDL_PLAYER_ACTION_NONE;
    g_player_action_menu.press_start_x = 0.0f;
    g_player_action_menu.press_start_y = 0.0f;
    g_player_action_menu.press_start_time_ns = 0;
}

static void sdl_player_action_menu_cancel(void)
{
    bool was_active = g_player_action_menu.active;

    g_player_action_menu.active = false;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    sdl_player_action_menu_cancel_press();
    if (was_active)
        g_state.need_present = true;
}

static void sdl_player_action_menu_queue_command(int command)
{
    if (command)
        sdl_gamepad_send_key(command, false);
}

static void sdl_player_action_menu_activate_kind(int kind, bool secondary)
{
    int command = 0;
    bool select_floor = false;

    switch (kind)
    {
    case SDL_PLAYER_ACTION_WAIT:
        command = secondary ? 'Z' : 'z';
        break;

    case SDL_PLAYER_ACTION_USE:
        command = 'u';
        select_floor = !secondary
            && sdl_player_action_menu_has_floor_item_underfoot();
        break;

    case SDL_PLAYER_ACTION_STEALTH:
        command = 'S';
        break;

    case SDL_PLAYER_ACTION_SING:
        command = 's';
        break;

    case SDL_PLAYER_ACTION_EXCHANGE:
        command = 'X';
        break;

    case SDL_PLAYER_ACTION_FLETCH:
        command = '-';
        break;

    case SDL_PLAYER_ACTION_EXAMINE:
        command = 'x';
        select_floor = !secondary
            && sdl_player_action_menu_has_floor_item_underfoot();
        break;

    case SDL_PLAYER_ACTION_ACTIVATE:
        command = 'a';
        break;

    case SDL_PLAYER_ACTION_HORN:
        command = 'p';
        break;

    default:
        return;
    }

    sdl_player_action_menu_cancel();
    sdl_player_action_menu_queue_command(command);
    if (select_floor)
        sdl_player_action_menu_queue_command('-');
}

static bool sdl_player_action_menu_open(void)
{
    if (!sdl_player_action_menu_commands_allowed())
        return false;

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.active = true;
    g_player_action_menu.hover_kind = SDL_PLAYER_ACTION_NONE;
    g_state.need_present = true;
    return true;
}

static int sdl_player_action_menu_default_kind(void)
{
    sdl_player_action_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = sdl_player_action_menu_collect(entries);
    int i;

    if (count <= 0)
        return SDL_PLAYER_ACTION_NONE;

    for (i = 0; i < count; i++)
    {
        if (entries[i].kind == SDL_PLAYER_ACTION_USE)
            return entries[i].kind;
    }

    return entries[0].kind;
}

static void sdl_player_action_menu_select_default(void)
{
    int kind = sdl_player_action_menu_default_kind();

    if (kind == SDL_PLAYER_ACTION_NONE)
        return;
    if (g_player_action_menu.hover_kind != kind)
    {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }
}

static int sdl_player_action_menu_hover_index(
    const sdl_player_action_entry* entries, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (entries[i].kind == g_player_action_menu.hover_kind)
            return i;
    }

    return -1;
}

static void sdl_player_action_menu_move_hover(int delta)
{
    sdl_player_action_entry entries[SDL_PLAYER_ACTION_MAX];
    int count = 0;
    int index;

    if (!sdl_player_action_menu_layout_for_window(entries, &count) || count <= 0)
        return;

    index = sdl_player_action_menu_hover_index(entries, count);
    if (index < 0)
        index = (delta < 0) ? count - 1 : 0;
    else
        index = (index + delta + count) % count;

    if (g_player_action_menu.hover_kind != entries[index].kind)
    {
        g_player_action_menu.hover_kind = entries[index].kind;
        g_state.need_present = true;
    }
}

static void sdl_player_action_menu_activate_hover(void)
{
    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();
    if (g_player_action_menu.hover_kind != SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_activate_kind(g_player_action_menu.hover_kind,
            false);
}

static bool sdl_player_action_menu_event_on_player_cell(const SDL_Event* ev)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot = session
        ? app_session_dungeon_snapshot(session)
        : NULL;
    float x;
    float y;
    s16b map_y = -1;
    s16b map_x = -1;

    if (!p_ptr || !sdl_player_action_menu_event_xy(ev, &x, &y))
        return false;
    if (!sdl_scene_dungeon_hit_test_map_cell(&g_views[PANE_MAIN], snapshot,
            x, y, &map_y, &map_x))
    {
        return false;
    }

    return map_y == p_ptr->py && map_x == p_ptr->px;
}

static bool sdl_player_action_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_player_action_menu_commands_allowed())
    {
        sdl_player_action_menu_cancel();
        return false;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == SDL_PLAYER_ACTION_NONE)
    {
        sdl_player_action_menu_cancel();
        return true;
    }
    if (secondary && !sdl_player_action_menu_kind_supports_secondary(kind))
    {
        sdl_player_action_menu_cancel();
        return true;
    }

    sdl_player_action_menu_cancel_press();
    g_player_action_menu.press_active = true;
    g_player_action_menu.press_mouse = mouse;
    g_player_action_menu.press_secondary = secondary;
    g_player_action_menu.press_finger_id = finger_id;
    g_player_action_menu.press_kind = kind;
    g_player_action_menu.press_start_x = x;
    g_player_action_menu.press_start_y = y;
    g_player_action_menu.press_start_time_ns = SDL_GetTicksNS();
    g_player_action_menu.hover_kind = kind;
    g_state.need_present = true;
    return true;
}

static bool sdl_player_action_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse)
{
    int kind;
    float dx;
    float dy;
    float threshold;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_player_action_menu_commands_allowed())
    {
        sdl_player_action_menu_cancel();
        return false;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (g_player_action_menu.hover_kind != kind)
    {
        g_player_action_menu.hover_kind = kind;
        g_state.need_present = true;
    }

    if (!g_player_action_menu.press_active || g_player_action_menu.press_gamepad)
        return true;
    if (g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    dx = fabsf(x - g_player_action_menu.press_start_x);
    dy = fabsf(y - g_player_action_menu.press_start_y);
    threshold = sdl_player_action_menu_clampf((float)MAX(g_views[PANE_MAIN].cell_w,
        g_views[PANE_MAIN].cell_h) * 0.75f, 24.0f, 80.0f);
    if (kind != g_player_action_menu.press_kind
        || dx > threshold || dy > threshold)
    {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

static bool sdl_player_action_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary)
{
    int kind;
    bool activate_secondary;

    if (!g_player_action_menu.active)
        return false;
    if (!sdl_player_action_menu_commands_allowed())
    {
        sdl_player_action_menu_cancel();
        return false;
    }
    if (!g_player_action_menu.press_active)
        return true;
    if (g_player_action_menu.press_gamepad
        || g_player_action_menu.press_mouse != mouse
        || g_player_action_menu.press_secondary != secondary
        || g_player_action_menu.press_finger_id != finger_id)
    {
        return true;
    }

    kind = sdl_player_action_menu_kind_at(x, y);
    if (kind == g_player_action_menu.press_kind)
    {
        activate_secondary = g_player_action_menu.press_secondary;
        if (!mouse && sdl_player_action_menu_kind_supports_secondary(kind)
            && g_player_action_menu.press_start_time_ns
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time_ns
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        {
            activate_secondary = true;
        }
        sdl_player_action_menu_activate_kind(kind, activate_secondary);
    }
    else
    {
        sdl_player_action_menu_cancel_press();
    }

    return true;
}

static bool sdl_player_action_menu_handle_gamepad_confirm(SDL_GamepadButton button,
    bool down)
{
    int kind;

    if (button != SDL_GAMEPAD_BUTTON_SOUTH)
        return false;

    if (g_player_action_menu.press_active && g_player_action_menu.press_gamepad)
    {
        bool activate_secondary = false;

        if (down)
            return true;
        if (g_player_action_menu.press_start_time_ns
            && SDL_GetTicksNS() - g_player_action_menu.press_start_time_ns
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL
            && sdl_player_action_menu_kind_supports_secondary(
                g_player_action_menu.press_kind))
        {
            activate_secondary = true;
        }
        kind = g_player_action_menu.press_kind;
        sdl_player_action_menu_cancel_press();
        if (kind != SDL_PLAYER_ACTION_NONE)
            sdl_player_action_menu_activate_kind(kind, activate_secondary);
        return true;
    }

    if (!down)
        return true;
    if (g_player_action_menu.hover_kind == SDL_PLAYER_ACTION_NONE)
        sdl_player_action_menu_select_default();

    kind = g_player_action_menu.hover_kind;
    if (sdl_player_action_menu_kind_supports_secondary(kind))
    {
        sdl_player_action_menu_cancel_press();
        g_player_action_menu.press_active = true;
        g_player_action_menu.press_gamepad = true;
        g_player_action_menu.press_button = (int)button;
        g_player_action_menu.press_kind = kind;
        g_player_action_menu.press_start_time_ns = SDL_GetTicksNS();
        g_state.need_present = true;
    }
    else
    {
        sdl_player_action_menu_activate_hover();
    }

    return true;
}

int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_player_action_menu.active)
        return -1;
    if (!g_player_action_menu.press_active
        || g_player_action_menu.press_mouse
        || g_player_action_menu.press_secondary
        || !sdl_player_action_menu_kind_supports_secondary(
            g_player_action_menu.press_kind)
        || !g_player_action_menu.press_start_time_ns)
    {
        return -1;
    }

    elapsed = now_ns - g_player_action_menu.press_start_time_ns;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns)
{
    int kind;

    if (sdl_player_action_menu_pending_timeout_ms(now_ns) != 0)
        return false;

    kind = g_player_action_menu.press_kind;
    sdl_player_action_menu_activate_kind(kind, true);
    return true;
}

void sdl_player_action_menu_reset_input_state(void)
{
    sdl_player_action_menu_cancel();
}

bool sdl_player_action_menu_handle_event(const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev)
        return false;

    if (g_player_action_menu.active && !sdl_player_action_menu_commands_allowed())
    {
        sdl_player_action_menu_cancel();
    }

    switch (ev->type)
    {
    case SDL_EVENT_KEY_DOWN:
        if (!g_player_action_menu.active)
            return false;
        if (ev->key.key == SDLK_ESCAPE)
        {
            sdl_player_action_menu_cancel();
            return true;
        }
        sdl_player_action_menu_cancel();
        return false;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (!g_player_action_menu.active)
            return false;
        switch (ev->gbutton.button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                sdl_player_action_menu_move_hover(-1);
            return true;

        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                sdl_player_action_menu_move_hover(1);
            return true;

        case SDL_GAMEPAD_BUTTON_EAST:
        case SDL_GAMEPAD_BUTTON_BACK:
        case SDL_GAMEPAD_BUTTON_START:
            if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                sdl_player_action_menu_cancel();
            return true;

        default:
            break;
        }
        return sdl_player_action_menu_handle_gamepad_confirm(
            ev->gbutton.button, ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (ev->button.button != SDL_BUTTON_LEFT
            && ev->button.button != SDL_BUTTON_RIGHT)
        {
            return false;
        }
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return false;
        if (!g_player_action_menu.active)
        {
            if (ev->button.button == SDL_BUTTON_LEFT
                && sdl_player_action_menu_event_on_player_cell(ev))
            {
                return sdl_player_action_menu_open();
            }
            return false;
        }
        return sdl_player_action_menu_handle_pointer_down(x, y, 0, true,
            ev->button.button == SDL_BUTTON_RIGHT);

    case SDL_EVENT_MOUSE_MOTION:
        if (!g_player_action_menu.active || ev->motion.which == SDL_TOUCH_MOUSEID)
            return false;
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return false;
        return sdl_player_action_menu_handle_pointer_motion(x, y, 0, true);

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (ev->button.button != SDL_BUTTON_LEFT
            && ev->button.button != SDL_BUTTON_RIGHT)
        {
            return false;
        }
        if (!g_player_action_menu.active)
            return false;
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return true;
        return sdl_player_action_menu_handle_pointer_up(x, y, 0, true,
            ev->button.button == SDL_BUTTON_RIGHT);

    case SDL_EVENT_FINGER_DOWN:
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return false;
        if (!g_player_action_menu.active)
        {
            if (sdl_player_action_menu_event_on_player_cell(ev))
                return sdl_player_action_menu_open();
            return false;
        }
        return sdl_player_action_menu_handle_pointer_down(x, y,
            ev->tfinger.fingerID, false, false);

    case SDL_EVENT_FINGER_MOTION:
        if (!g_player_action_menu.active)
            return false;
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return true;
        return sdl_player_action_menu_handle_pointer_motion(x, y,
            ev->tfinger.fingerID, false);

    case SDL_EVENT_FINGER_UP:
        if (!g_player_action_menu.active)
            return false;
        if (!sdl_player_action_menu_event_xy(ev, &x, &y))
            return true;
        return sdl_player_action_menu_handle_pointer_up(x, y,
            ev->tfinger.fingerID, false, false);

    case SDL_EVENT_FINGER_CANCELED:
        if (g_player_action_menu.active && g_player_action_menu.press_active
            && !g_player_action_menu.press_mouse
            && !g_player_action_menu.press_gamepad
            && g_player_action_menu.press_finger_id == ev->tfinger.fingerID)
        {
            sdl_player_action_menu_cancel_press();
            g_state.need_present = true;
            return true;
        }
        return g_player_action_menu.active;

    default:
        return false;
    }
}

static void sdl_player_action_menu_render_label(const SDL_FRect* rect,
    cptr text, SDL_Color color)
{
    TTF_Font* font;
    int font_px;
    int text_w;
    int text_h;

    if (!rect || !text || !text[0])
        return;

    font_px = sdl_ui_scale_px(16.0f);
    if (font_px < 12)
        font_px = 12;
    font = sdl_ui_font_for_height(font_px);
    if (!font)
        return;

    text_w = sdl_ui_measure_text(font, text);
    text_h = MAX(font_px, TTF_GetFontHeight(font));
    sdl_ui_render_text(font, rect->x + (rect->w - (float)text_w) * 0.5f,
        rect->y + (rect->h - (float)text_h) * 0.5f, color, text);
}

void sdl_player_action_menu_render(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot)
{
    sdl_player_action_entry entries[SDL_PLAYER_ACTION_MAX];
    SDL_Color bg = { 16, 20, 22, 226 };
    SDL_Color hover_bg = { 48, 54, 58, 236 };
    SDL_Color border = { 92, 158, 255, 176 };
    SDL_Color hover_border = { 255, 212, 92, 238 };
    SDL_Color text = { 244, 244, 246, 255 };
    int count = 0;
    int i;

    if (!g_player_action_menu.active || !main_view || !snapshot)
        return;
    if (!sdl_player_action_menu_commands_allowed())
        return;
    if (!sdl_player_action_menu_layout_for_canvas(main_view, snapshot,
            entries, &count))
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    for (i = 0; i < count; i++)
    {
        SDL_FRect shadow = entries[i].rect;
        bool hover = entries[i].kind == g_player_action_menu.hover_kind;
        SDL_Color fill = hover ? hover_bg : bg;
        SDL_Color line = hover ? hover_border : border;

        shadow.x += 2.0f;
        shadow.y += 2.0f;
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 112);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
            fill.a);
        SDL_RenderFillRect(g_state.renderer, &entries[i].rect);
        SDL_SetRenderDrawColor(g_state.renderer, line.r, line.g, line.b,
            line.a);
        SDL_RenderRect(g_state.renderer, &entries[i].rect);
        sdl_player_action_menu_render_label(&entries[i].rect, entries[i].label,
            text);
    }
}
