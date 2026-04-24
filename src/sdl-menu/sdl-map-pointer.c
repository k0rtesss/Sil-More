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

#include "../platform-input.h"
#include "../sdl-main-internal.h"

typedef struct sdl_map_pointer_press {
    bool active;
    u16b device;
    u16b button;
    SDL_FingerID finger_id;
    s16b start_map_y;
    s16b start_map_x;
    s16b last_map_y;
    s16b last_map_x;
    float start_x;
    float start_y;
    Uint64 start_time_ns;
    bool long_press_eligible;
} sdl_map_pointer_press;

typedef struct sdl_map_pointer_travel {
    bool active;
    s16b target_y;
    s16b target_x;
    u16b device;
    u16b button;
    u16b clicks;
} sdl_map_pointer_travel;

enum {
    SDL_MAP_POINTER_TOUCH_DRIFT_CANCEL_PX = 24,
    SDL_MAP_POINTER_PATH_QUEUE_MAX = MAX_DUNGEON_HGT * MAX_DUNGEON_WID
};

static sdl_map_pointer_press g_map_pointer_press;
static sdl_map_pointer_travel g_map_pointer_travel;
static bool g_map_pointer_hover_active = false;
static s16b g_map_pointer_hover_y = -1;
static s16b g_map_pointer_hover_x = -1;
static bool g_map_pointer_consumed_touch = false;
static SDL_FingerID g_map_pointer_consumed_finger_id = 0;
static byte g_map_pointer_path_seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte g_map_pointer_path_prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte g_map_pointer_path_preview[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b g_map_pointer_path_queue_y[SDL_MAP_POINTER_PATH_QUEUE_MAX];
static s16b g_map_pointer_path_queue_x[SDL_MAP_POINTER_PATH_QUEUE_MAX];
static bool g_map_pointer_path_preview_active = false;
static s16b g_map_pointer_path_preview_target_y = -1;
static s16b g_map_pointer_path_preview_target_x = -1;

static void sdl_map_pointer_clear_path_preview(void);
static void sdl_map_pointer_update_path_preview(s16b map_y, s16b map_x);

static bool sdl_map_pointer_dungeon_scene_ready(u16b* out_reason)
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

static bool sdl_map_pointer_wait_allows_commands(void)
{
    u16b reason;

    if (!sdl_map_pointer_dungeon_scene_ready(&reason))
        return false;
    return reason == APP_WAIT_REASON_NONE
        || reason == APP_WAIT_REASON_COMMAND_INPUT;
}

static bool sdl_map_pointer_command_wait_active(void)
{
    u16b reason;

    return sdl_map_pointer_dungeon_scene_ready(&reason)
        && reason == APP_WAIT_REASON_COMMAND_INPUT;
}

static bool sdl_map_pointer_event_xy(const SDL_Event* ev, float* out_x,
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
    }

    return false;
}

static bool sdl_map_pointer_hit_test(float window_x, float window_y,
    s16b* out_map_y, s16b* out_map_x)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot = session
        ? app_session_dungeon_snapshot(session)
        : NULL;

    return sdl_scene_dungeon_hit_test_map_cell(&g_views[0], snapshot,
        window_x, window_y, out_map_y, out_map_x);
}

static void sdl_map_pointer_widget_ref(s16b map_y, s16b map_x, u16b action,
    app_ui_widget_ref* out_ref)
{
    int local_y = p_ptr ? map_y - p_ptr->wy : 0;
    int local_x = p_ptr ? map_x - p_ptr->wx : 0;

    if (!out_ref)
        return;

    app_ui_widget_ref_clear(out_ref);
    out_ref->scene_kind = APP_SCENE_KIND_DUNGEON;
    out_ref->target_kind = APP_UI_WIDGET_ROLE_MAP_CELL;
    out_ref->widget_id = (s16b)(local_y * SCREEN_WID + local_x);
    out_ref->role = APP_UI_WIDGET_ROLE_MAP_CELL;
    out_ref->action = action;
    out_ref->flags = APP_UI_INTERACTION_FLAG_POINTER_ENABLED
        | APP_UI_INTERACTION_FLAG_TOUCH_TARGET
        | APP_UI_INTERACTION_FLAG_TOOLTIP;
    out_ref->payload0 = map_y;
    out_ref->payload1 = map_x;
    SDL_strlcpy(out_ref->label, "Dungeon map cell", sizeof(out_ref->label));
    SDL_strlcpy(out_ref->tooltip, "Inspect dungeon map cell",
        sizeof(out_ref->tooltip));
}

static void sdl_map_pointer_note_focus(s16b map_y, s16b map_x, u16b device,
    bool pressed, u16b action)
{
    app_session* session = app_session_current();
    app_ui_widget_ref ref;
    u16b reason;

    if (!session)
        return;

    sdl_map_pointer_widget_ref(map_y, map_x, action, &ref);
    if (device == APP_INPUT_DEVICE_TOUCH)
        reason = pressed ? APP_UI_FOCUS_REASON_TOUCH_PRESS
            : APP_UI_FOCUS_REASON_POINTER_HOVER;
    else
        reason = pressed ? APP_UI_FOCUS_REASON_POINTER_PRESS
            : APP_UI_FOCUS_REASON_POINTER_HOVER;

    app_session_note_ui_focus(session, &ref, reason, pressed);
}

static void sdl_map_pointer_note_hover(s16b map_y, s16b map_x, u16b device)
{
    if (g_map_pointer_hover_active
        && g_map_pointer_hover_y == map_y
        && g_map_pointer_hover_x == map_x)
    {
        return;
    }

    g_map_pointer_hover_active = true;
    g_map_pointer_hover_y = map_y;
    g_map_pointer_hover_x = map_x;

    dungeon_note_cursor_relative(map_y, map_x);
    sdl_map_pointer_note_focus(map_y, map_x, device, false,
        APP_UI_WIDGET_ACTION_INSPECT);
    sdl_map_pointer_update_path_preview(map_y, map_x);
    g_state.need_present = true;
}

static void sdl_map_pointer_clear_hover(void)
{
    app_session* session = app_session_current();

    if (!g_map_pointer_hover_active)
        return;

    g_map_pointer_hover_active = false;
    g_map_pointer_hover_y = -1;
    g_map_pointer_hover_x = -1;
    if (!g_map_pointer_travel.active)
        sdl_map_pointer_clear_path_preview();
    dungeon_sync_cursor_state();
    if (session)
        app_session_clear_ui_focus(session);
    g_state.need_present = true;
}

static void sdl_map_pointer_cancel_press(void)
{
    memset(&g_map_pointer_press, 0, sizeof(g_map_pointer_press));
}

static void sdl_map_pointer_cancel_travel(void)
{
    if (!g_map_pointer_travel.active)
        return;

    memset(&g_map_pointer_travel, 0, sizeof(g_map_pointer_travel));
    sdl_map_pointer_clear_path_preview();
    dungeon_sync_cursor_state();
    g_state.need_present = true;
}

void sdl_map_pointer_reset_input_state(void)
{
    sdl_map_pointer_cancel_press();
    sdl_map_pointer_cancel_travel();
    g_map_pointer_consumed_touch = false;
    g_map_pointer_consumed_finger_id = 0;
    sdl_map_pointer_clear_path_preview();
    sdl_map_pointer_clear_hover();
}

static void sdl_map_pointer_clear_path_preview(void)
{
    if (!g_map_pointer_path_preview_active)
        return;

    g_map_pointer_path_preview_active = false;
    g_map_pointer_path_preview_target_y = -1;
    g_map_pointer_path_preview_target_x = -1;
    memset(g_map_pointer_path_preview, 0, sizeof(g_map_pointer_path_preview));
    g_state.need_present = true;
}

static bool sdl_map_pointer_store_path_preview(s16b target_y, s16b target_x)
{
    int y = target_y;
    int x = target_x;

    if (!p_ptr)
        return false;

    memset(g_map_pointer_path_preview, 0, sizeof(g_map_pointer_path_preview));

    while (y != p_ptr->py || x != p_ptr->px)
    {
        int dir;

        if (!in_bounds_fully(y, x))
        {
            memset(g_map_pointer_path_preview, 0,
                sizeof(g_map_pointer_path_preview));
            return false;
        }

        dir = g_map_pointer_path_prev[y][x];
        if (dir <= 0 || dir == 5)
        {
            memset(g_map_pointer_path_preview, 0,
                sizeof(g_map_pointer_path_preview));
            return false;
        }

        g_map_pointer_path_preview[y][x] = 1;
        y -= ddy[dir];
        x -= ddx[dir];
    }

    g_map_pointer_path_preview_active = true;
    g_map_pointer_path_preview_target_y = target_y;
    g_map_pointer_path_preview_target_x = target_x;
    return true;
}

bool sdl_map_pointer_preview_cell(s16b map_y, s16b map_x,
    bool* out_is_target)
{
    if (out_is_target)
        *out_is_target = false;
    if (!g_map_pointer_path_preview_active)
        return false;
    if (map_y < 0 || map_y >= MAX_DUNGEON_HGT
        || map_x < 0 || map_x >= MAX_DUNGEON_WID)
    {
        return false;
    }
    if (!g_map_pointer_path_preview[map_y][map_x])
        return false;

    if (out_is_target)
    {
        *out_is_target = map_y == g_map_pointer_path_preview_target_y
            && map_x == g_map_pointer_path_preview_target_x;
    }
    return true;
}

static bool sdl_map_pointer_path_cell_known(int y, int x)
{
    return in_bounds_fully(y, x)
        && (cave_info[y][x] & CAVE_MARK)
        && grid_info_is_available(y, x);
}

static bool sdl_map_pointer_path_cell_enterable(int y, int x, bool target)
{
    int m_idx;

    if (!sdl_map_pointer_path_cell_known(y, x))
        return false;
    if (y == p_ptr->py && x == p_ptr->px)
        return true;

    m_idx = cave_m_idx[y][x];
    if (m_idx > 0 && mon_list[m_idx].ml)
        return target;
    if (cave_known_closed_door_bold(y, x))
        return target;
    if (!cave_floor_bold(y, x) || cave_feat[y][x] == FEAT_CHASM)
        return false;
    if (cave_trap_bold(y, x))
        return target;
    return true;
}

static bool sdl_map_pointer_path_diagonal_ok(int y, int x, int dir,
    s16b target_y, s16b target_x)
{
    int side_y;
    int side_x;

    if (ddy[dir] == 0 || ddx[dir] == 0)
        return true;

    side_y = y + ddy[dir];
    side_x = x + ddx[dir];
    if (!sdl_map_pointer_path_cell_enterable(side_y, x,
            side_y == target_y && x == target_x))
    {
        return false;
    }
    if (!sdl_map_pointer_path_cell_enterable(y, side_x,
            y == target_y && side_x == target_x))
    {
        return false;
    }
    return true;
}

static void sdl_map_pointer_path_clear_seen(int hgt, int wid)
{
    for (int y = 0; y < hgt; y++)
    {
        memset(g_map_pointer_path_seen[y], 0, (size_t)wid);
        memset(g_map_pointer_path_prev[y], 0, (size_t)wid);
    }
}

static bool sdl_map_pointer_path_first_step(s16b target_y, s16b target_x,
    int* out_dir, bool record_preview)
{
    static const byte dirs[8] = { 8, 6, 2, 4, 9, 3, 1, 7 };
    int hgt;
    int wid;
    int head = 0;
    int tail = 0;

    if (out_dir)
        *out_dir = 0;
    if (!p_ptr || !in_bounds_fully(target_y, target_x))
        return false;
    if (p_ptr->py == target_y && p_ptr->px == target_x)
        return false;
    if (!sdl_map_pointer_path_cell_enterable(target_y, target_x, true))
        return false;

    hgt = p_ptr->cur_map_hgt;
    wid = p_ptr->cur_map_wid;
    sdl_map_pointer_path_clear_seen(hgt, wid);
    g_map_pointer_path_seen[p_ptr->py][p_ptr->px] = 1;
    g_map_pointer_path_queue_y[tail] = p_ptr->py;
    g_map_pointer_path_queue_x[tail++] = p_ptr->px;

    while (head < tail)
    {
        int y = g_map_pointer_path_queue_y[head];
        int x = g_map_pointer_path_queue_x[head++];

        for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
        {
            int dir = dirs[i];
            int ny = y + ddy[dir];
            int nx = x + ddx[dir];
            bool is_target = ny == target_y && nx == target_x;

            if (!in_bounds_fully(ny, nx)
                || g_map_pointer_path_seen[ny][nx])
                continue;
            if (!sdl_map_pointer_path_cell_enterable(ny, nx, is_target))
                continue;
            if (!sdl_map_pointer_path_diagonal_ok(y, x, dir,
                    target_y, target_x))
                continue;

            g_map_pointer_path_seen[ny][nx] = 1;
            g_map_pointer_path_prev[ny][nx] = (byte)dir;
            if (is_target)
            {
                int step_dir = dir;

                if (record_preview
                    && !sdl_map_pointer_store_path_preview(target_y, target_x))
                {
                    return false;
                }

                while (y != p_ptr->py || x != p_ptr->px)
                {
                    step_dir = g_map_pointer_path_prev[y][x];
                    y -= ddy[step_dir];
                    x -= ddx[step_dir];
                }
                if (out_dir)
                    *out_dir = step_dir;
                return step_dir > 0 && step_dir != 5;
            }

            if (tail >= SDL_MAP_POINTER_PATH_QUEUE_MAX)
                return false;
            g_map_pointer_path_queue_y[tail] = (s16b)ny;
            g_map_pointer_path_queue_x[tail++] = (s16b)nx;
        }
    }

    return false;
}

static bool sdl_map_pointer_is_adjacent_to_player(s16b map_y, s16b map_x,
    int* out_dir)
{
    int dy;
    int dx;
    int dir;

    if (out_dir)
        *out_dir = 0;
    if (!p_ptr)
        return false;

    dy = map_y - p_ptr->py;
    dx = map_x - p_ptr->px;
    if (dy == 0 && dx == 0)
        return false;
    if (dy < -1 || dy > 1 || dx < -1 || dx > 1)
        return false;

    dir = dir_from_delta(dy, dx);
    if (dir <= 0 || dir == 5)
        return false;

    if (out_dir)
        *out_dir = dir;
    return true;
}

static void sdl_map_pointer_update_path_preview(s16b map_y, s16b map_x)
{
    int dir = 0;

    if (!p_ptr || (p_ptr->py == map_y && p_ptr->px == map_x)
        || sdl_map_pointer_is_adjacent_to_player(map_y, map_x, NULL))
    {
        sdl_map_pointer_clear_path_preview();
        return;
    }

    if (!sdl_map_pointer_path_first_step(map_y, map_x, &dir, true))
    {
        sdl_map_pointer_clear_path_preview();
        return;
    }

    g_state.need_present = true;
}

static bool sdl_map_pointer_submit_adjacent_movement(s16b map_y, s16b map_x,
    u16b device, u16b button, u16b clicks)
{
    SDL_Keymod mod;
    int dir = 0;
    bool shift;
    bool ctrl;
    bool alt;

    if (!sdl_map_pointer_is_adjacent_to_player(map_y, map_x, &dir))
        return false;

    mod = SDL_GetModState();
    shift = ((mod & SDL_KMOD_SHIFT) != 0) || sdl_gamepad_shift_active();
    ctrl = ((mod & SDL_KMOD_CTRL) != 0) || sdl_gamepad_ctrl_active();
    alt = ((mod & SDL_KMOD_ALT) != 0) || sdl_gamepad_alt_active();
    return platform_submit_directional_movement(dir, shift, ctrl, alt,
        device, APP_INPUT_TYPE_POINTER_BUTTON, 0, APP_INPUT_FLAG_RELEASE,
        button, clicks);
}

static bool sdl_map_pointer_submit_travel_step(void)
{
    int dir = 0;
    s16b target_y;
    s16b target_x;
    u16b device;
    u16b button;
    u16b clicks;

    if (!g_map_pointer_travel.active || !p_ptr)
        return false;
    if (p_ptr->py == g_map_pointer_travel.target_y
        && p_ptr->px == g_map_pointer_travel.target_x)
    {
        sdl_map_pointer_cancel_travel();
        return false;
    }

    target_y = g_map_pointer_travel.target_y;
    target_x = g_map_pointer_travel.target_x;
    device = g_map_pointer_travel.device;
    button = g_map_pointer_travel.button;
    clicks = g_map_pointer_travel.clicks;

    if (sdl_map_pointer_is_adjacent_to_player(target_y, target_x, &dir))
    {
        bool final_target_blocks = cave_known_closed_door_bold(target_y, target_x)
            || (cave_m_idx[target_y][target_x] > 0
                && mon_list[cave_m_idx[target_y][target_x]].ml);

        if (sdl_map_pointer_submit_adjacent_movement(target_y, target_x,
                device, button, clicks))
        {
            if (final_target_blocks)
                sdl_map_pointer_cancel_travel();
            return true;
        }
        sdl_map_pointer_cancel_travel();
        return false;
    }

    if (!sdl_map_pointer_path_first_step(target_y, target_x, &dir, true))
    {
        msg_print("You do not know a safe path there.");
        sdl_map_pointer_cancel_travel();
        return false;
    }

    return platform_submit_directional_movement(dir, false, false, false,
        device, APP_INPUT_TYPE_POINTER_BUTTON, 0, APP_INPUT_FLAG_RELEASE,
        button, clicks);
}

static bool sdl_map_pointer_start_travel(s16b map_y, s16b map_x, u16b device,
    u16b button, u16b clicks)
{
    int dir = 0;

    sdl_map_pointer_cancel_press();
    sdl_map_pointer_cancel_travel();

    if (!p_ptr || (p_ptr->py == map_y && p_ptr->px == map_x))
        return false;
    if (!sdl_map_pointer_path_first_step(map_y, map_x, &dir, true))
    {
        msg_print("You do not know a safe path there.");
        sdl_map_pointer_clear_path_preview();
        return false;
    }

    g_map_pointer_travel.active = true;
    g_map_pointer_travel.target_y = map_y;
    g_map_pointer_travel.target_x = map_x;
    g_map_pointer_travel.device = device;
    g_map_pointer_travel.button = button;
    g_map_pointer_travel.clicks = clicks;
    dungeon_note_cursor_relative(map_y, map_x);
    sdl_map_pointer_note_focus(map_y, map_x, device, false,
        APP_UI_WIDGET_ACTION_ACTIVATE);
    g_state.need_present = true;
    return true;
}

static void sdl_map_pointer_open_preview(s16b map_y, s16b map_x, bool detail)
{
    sdl_map_pointer_cancel_press();
    sdl_map_pointer_cancel_travel();
    sdl_map_pointer_clear_path_preview();
    sdl_map_pointer_note_hover(map_y, map_x, APP_INPUT_DEVICE_POINTER);

    if (detail)
        (void)do_cmd_look_recall_at(map_y, map_x);
    else
        do_cmd_look_at(map_y, map_x);

    sdl_map_pointer_clear_hover();
}

static void sdl_map_pointer_handle_cell_release(s16b map_y, s16b map_x,
    u16b device, u16b button, u16b clicks)
{
    if (button == SDL_BUTTON_RIGHT)
    {
        sdl_map_pointer_open_preview(map_y, map_x, true);
        return;
    }

    if (button != SDL_BUTTON_LEFT)
        return;

    if (sdl_map_pointer_submit_adjacent_movement(map_y, map_x, device,
            button, clicks))
    {
        sdl_map_pointer_cancel_press();
        sdl_map_pointer_cancel_travel();
        sdl_map_pointer_clear_path_preview();
        return;
    }

    (void)sdl_map_pointer_start_travel(map_y, map_x, device, button, clicks);
}

static bool sdl_map_pointer_handle_consumed_touch_release(
    const SDL_Event* ev)
{
    if (!g_map_pointer_consumed_touch)
        return false;
    if (ev->type != SDL_EVENT_FINGER_UP
        && ev->type != SDL_EVENT_FINGER_CANCELED)
    {
        return false;
    }
    if (ev->tfinger.fingerID != g_map_pointer_consumed_finger_id)
        return false;

    g_map_pointer_consumed_touch = false;
    g_map_pointer_consumed_finger_id = 0;
    return true;
}

static void sdl_map_pointer_cancel_travel_for_input_event(const SDL_Event* ev)
{
    if (!ev)
        return;

    switch (ev->type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_FINGER_DOWN:
        if (g_map_pointer_travel.active)
            sdl_map_pointer_cancel_travel();
        else
            sdl_map_pointer_clear_path_preview();
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which != SDL_TOUCH_MOUSEID)
        {
            if (g_map_pointer_travel.active)
                sdl_map_pointer_cancel_travel();
            else
                sdl_map_pointer_clear_path_preview();
        }
        break;

    default:
        break;
    }
}

static void sdl_map_pointer_update_touch_motion(float x, float y,
    s16b map_y, s16b map_x)
{
    float dx;
    float dy;
    float threshold;

    if (!g_map_pointer_press.active)
        return;

    g_map_pointer_press.last_map_y = map_y;
    g_map_pointer_press.last_map_x = map_x;
    dx = x - g_map_pointer_press.start_x;
    dy = y - g_map_pointer_press.start_y;
    threshold = (float)SDL_MAP_POINTER_TOUCH_DRIFT_CANCEL_PX;
    if (dx * dx + dy * dy > threshold * threshold)
        g_map_pointer_press.long_press_eligible = false;
}

static bool sdl_map_pointer_begin_press(u16b device, u16b button,
    SDL_FingerID finger_id, float x, float y, s16b map_y, s16b map_x)
{
    sdl_map_pointer_cancel_press();
    sdl_map_pointer_cancel_travel();
    g_map_pointer_press.active = true;
    g_map_pointer_press.device = device;
    g_map_pointer_press.button = button;
    g_map_pointer_press.finger_id = finger_id;
    g_map_pointer_press.start_map_y = map_y;
    g_map_pointer_press.start_map_x = map_x;
    g_map_pointer_press.last_map_y = map_y;
    g_map_pointer_press.last_map_x = map_x;
    g_map_pointer_press.start_x = x;
    g_map_pointer_press.start_y = y;
    g_map_pointer_press.start_time_ns = SDL_GetTicksNS();
    g_map_pointer_press.long_press_eligible
        = (device == APP_INPUT_DEVICE_TOUCH);

    sdl_map_pointer_note_hover(map_y, map_x, device);
    sdl_map_pointer_note_focus(map_y, map_x, device, true,
        APP_UI_WIDGET_ACTION_ACTIVATE);
    return true;
}

static int sdl_map_pointer_pending_long_press_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!sdl_map_pointer_wait_allows_commands())
    {
        sdl_map_pointer_cancel_press();
        return -1;
    }

    if (!g_map_pointer_press.active
        || g_map_pointer_press.device != APP_INPUT_DEVICE_TOUCH
        || !g_map_pointer_press.long_press_eligible)
    {
        return -1;
    }

    elapsed = now_ns - g_map_pointer_press.start_time_ns;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

int sdl_map_pointer_pending_timeout_ms(Uint64 now_ns)
{
    int long_press_timeout_ms;

    if (g_map_pointer_travel.active)
        return sdl_map_pointer_command_wait_active() ? 0 : -1;

    long_press_timeout_ms
        = sdl_map_pointer_pending_long_press_timeout_ms(now_ns);
    return long_press_timeout_ms;
}

bool sdl_map_pointer_flush_pending_long_press(Uint64 now_ns)
{
    s16b map_y;
    s16b map_x;
    SDL_FingerID finger_id;

    if (g_map_pointer_travel.active)
        return sdl_map_pointer_command_wait_active()
            && sdl_map_pointer_submit_travel_step();

    if (sdl_map_pointer_pending_long_press_timeout_ms(now_ns) != 0)
        return false;

    map_y = g_map_pointer_press.start_map_y;
    map_x = g_map_pointer_press.start_map_x;
    finger_id = g_map_pointer_press.finger_id;
    sdl_map_pointer_cancel_press();

    g_map_pointer_consumed_touch = true;
    g_map_pointer_consumed_finger_id = finger_id;
    sdl_map_pointer_open_preview(map_y, map_x, true);
    return true;
}

bool sdl_map_pointer_handle_event(const SDL_Event* ev)
{
    float x;
    float y;
    s16b map_y;
    s16b map_x;

    if (!ev)
        return false;

    if (sdl_map_pointer_handle_consumed_touch_release(ev))
        return true;

    sdl_map_pointer_cancel_travel_for_input_event(ev);

    if (!sdl_map_pointer_wait_allows_commands())
    {
        if (g_map_pointer_press.active
            && (ev->type == SDL_EVENT_MOUSE_BUTTON_UP
                || ev->type == SDL_EVENT_FINGER_UP
                || ev->type == SDL_EVENT_FINGER_CANCELED))
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
        return false;
    }

    if (ev->type == SDL_EVENT_MOUSE_MOTION)
    {
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return false;
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
            return false;
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
        {
            sdl_map_pointer_clear_hover();
            return false;
        }
        sdl_map_pointer_note_hover(map_y, map_x, APP_INPUT_DEVICE_POINTER);
        return true;
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (ev->button.button != SDL_BUTTON_LEFT
            && ev->button.button != SDL_BUTTON_RIGHT)
        {
            return false;
        }
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
            return false;
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
            return false;
        return sdl_map_pointer_begin_press(APP_INPUT_DEVICE_POINTER,
            ev->button.button, 0, x, y, map_y, map_x);
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        bool had_capture = g_map_pointer_press.active
            && g_map_pointer_press.device == APP_INPUT_DEVICE_POINTER
            && g_map_pointer_press.button == ev->button.button;

        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        if (!had_capture)
            return false;
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
        sdl_map_pointer_handle_cell_release(map_y, map_x,
            APP_INPUT_DEVICE_POINTER, ev->button.button, ev->button.clicks);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_DOWN)
    {
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
            return false;
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
            return false;
        return sdl_map_pointer_begin_press(APP_INPUT_DEVICE_TOUCH,
            SDL_BUTTON_LEFT, ev->tfinger.fingerID, x, y, map_y, map_x);
    }

    if (ev->type == SDL_EVENT_FINGER_MOTION)
    {
        if (!g_map_pointer_press.active
            || g_map_pointer_press.device != APP_INPUT_DEVICE_TOUCH
            || g_map_pointer_press.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
            return true;
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
        {
            g_map_pointer_press.long_press_eligible = false;
            return true;
        }
        sdl_map_pointer_update_touch_motion(x, y, map_y, map_x);
        sdl_map_pointer_note_hover(map_y, map_x, APP_INPUT_DEVICE_TOUCH);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_UP)
    {
        bool had_capture = g_map_pointer_press.active
            && g_map_pointer_press.device == APP_INPUT_DEVICE_TOUCH
            && g_map_pointer_press.finger_id == ev->tfinger.fingerID;
        bool long_press = false;

        if (!had_capture)
            return false;
        long_press = g_map_pointer_press.long_press_eligible
            && (SDL_GetTicksNS() - g_map_pointer_press.start_time_ns
                >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
        if (!sdl_map_pointer_event_xy(ev, &x, &y))
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
        if (long_press)
        {
            map_y = g_map_pointer_press.start_map_y;
            map_x = g_map_pointer_press.start_map_x;
            sdl_map_pointer_open_preview(map_y, map_x, true);
            return true;
        }
        if (!sdl_map_pointer_hit_test(x, y, &map_y, &map_x))
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
        sdl_map_pointer_handle_cell_release(map_y, map_x,
            APP_INPUT_DEVICE_TOUCH, SDL_BUTTON_LEFT, 1);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_CANCELED)
    {
        if (g_map_pointer_press.active
            && g_map_pointer_press.device == APP_INPUT_DEVICE_TOUCH
            && g_map_pointer_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_map_pointer_cancel_press();
            return true;
        }
    }

    return false;
}
