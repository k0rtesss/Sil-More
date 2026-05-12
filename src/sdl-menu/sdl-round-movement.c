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
#include "sdl-round-movement.h"

#include <math.h>

typedef struct sdl_round_movement_press {
    bool active;
    SDL_FingerID finger_id;
    float center_x;
    float center_y;
    float current_x;
    float current_y;
    int selected_dir;
    bool selected_ctrl;
    Uint64 start_time_ns;
} sdl_round_movement_press;

enum {
    SDL_ROUND_MOVEMENT_CIRCLE_SEGMENTS = 32
};

static sdl_round_movement_press g_round_movement_press;
static int g_round_movement_last_dir = 0;

static bool sdl_round_movement_scene_ready(u16b* out_reason)
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

static bool sdl_round_movement_commands_allowed(void)
{
    u16b reason;

    if (!platform_touch_round_movement_enabled())
        return false;
    if (!sdl_round_movement_scene_ready(&reason))
        return false;

    return reason == APP_WAIT_REASON_NONE
        || reason == APP_WAIT_REASON_COMMAND_INPUT;
}

static void sdl_round_movement_window_point_to_pixels(float* x, float* y)
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

static bool sdl_round_movement_event_xy(const SDL_Event* ev, float* out_x,
    float* out_y)
{
    int window_w = 0;
    int window_h = 0;

    if (!ev || !out_x || !out_y)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        *out_x = ev->button.x;
        *out_y = ev->button.y;
        sdl_round_movement_window_point_to_pixels(out_x, out_y);
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

static bool sdl_round_movement_point_in_touch_pane(float window_x,
    float window_y)
{
    const SDL_Rect* pane = &g_pane_rects[PANE_TOUCH];

    return pane->w > 0 && pane->h > 0
        && window_x >= (float)pane->x
        && window_x < (float)(pane->x + pane->w)
        && window_y >= (float)pane->y
        && window_y < (float)(pane->y + pane->h);
}

static bool sdl_round_movement_point_excluded(float window_x, float window_y)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot = session
        ? app_session_dungeon_snapshot(session)
        : NULL;
    s16b map_y = -1;
    s16b map_x = -1;
    int dy;
    int dx;

    if (!sdl_round_movement_commands_allowed())
        return false;
    if (sdl_round_movement_point_in_touch_pane(window_x, window_y))
        return true;
    if (!sdl_scene_dungeon_hit_test_map_cell(&g_views[0], snapshot,
            window_x, window_y, &map_y, &map_x))
    {
        return true;
    }
    if (!p_ptr)
        return false;

    dy = ABS(map_y - p_ptr->py);
    dx = ABS(map_x - p_ptr->px);
    return dy <= 1 && dx <= 1;
}

static float sdl_round_movement_radius_px(const sdl_view* main_view)
{
    int min_dim;
    int cell_px;
    float radius;

    if (!main_view || main_view->cell_w <= 0 || main_view->cell_h <= 0)
        return 72.0f;

    min_dim = MIN(main_view->cols * main_view->cell_w,
        main_view->rows * main_view->cell_h);
    cell_px = MAX(main_view->cell_w, main_view->cell_h);
    radius = (float)cell_px * 4.5f;

    if (radius < 72.0f)
        radius = 72.0f;
    if (radius > 150.0f)
        radius = 150.0f;
    if (min_dim > 0 && radius > (float)min_dim * 0.42f)
        radius = (float)min_dim * 0.42f;
    if (radius < 32.0f)
        radius = 32.0f;

    return radius;
}

static float sdl_round_movement_ctrl_radius_px(float radius)
{
    return radius * 3.0f;
}

static int sdl_round_movement_dir_for_delta(float dx, float dy)
{
    float abs_x = fabsf(dx);
    float abs_y = fabsf(dy);

    if (abs_x <= 0.0f && abs_y <= 0.0f)
        return 0;

    if (abs_x > abs_y * 2.41421356f)
        return (dx >= 0.0f) ? 6 : 4;
    if (abs_y > abs_x * 2.41421356f)
        return (dy >= 0.0f) ? 2 : 8;
    if (dy < 0.0f)
        return (dx < 0.0f) ? 7 : 9;

    return (dx < 0.0f) ? 1 : 3;
}

static void sdl_round_movement_send_dir(int dir, bool ctrl)
{
    if (dir < 1 || dir > 9 || dir == 5)
        return;

    if (platform_submit_directional_movement(dir, false, ctrl, false,
            APP_INPUT_DEVICE_TOUCH, APP_INPUT_TYPE_POINTER_BUTTON, 0,
            APP_INPUT_FLAG_RELEASE, APP_MOVEMENT_SEMANTIC_TRIGGER_ROUND_WHEEL,
            0))
    {
        g_round_movement_last_dir = dir;
    }
}

static void sdl_round_movement_draw_circle(float cx, float cy, float radius,
    SDL_Color color)
{
    int i;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    for (i = 0; i < SDL_ROUND_MOVEMENT_CIRCLE_SEGMENTS; i++)
    {
        float angle0 = (float)i * (6.28318531f
            / (float)SDL_ROUND_MOVEMENT_CIRCLE_SEGMENTS);
        float angle1 = (float)(i + 1) * (6.28318531f
            / (float)SDL_ROUND_MOVEMENT_CIRCLE_SEGMENTS);

        SDL_RenderLine(g_state.renderer,
            cx + cosf(angle0) * radius,
            cy + sinf(angle0) * radius,
            cx + cosf(angle1) * radius,
            cy + sinf(angle1) * radius);
    }
}

static void sdl_round_movement_draw_sector_lines(float cx, float cy,
    float inner_radius, float outer_radius, SDL_Color color)
{
    static const float points[8][2] = {
        { 0.923880f, 0.382683f },
        { 0.382683f, 0.923880f },
        { -0.382683f, 0.923880f },
        { -0.923880f, 0.382683f },
        { -0.923880f, -0.382683f },
        { -0.382683f, -0.923880f },
        { 0.382683f, -0.923880f },
        { 0.923880f, -0.382683f }
    };
    int i;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    for (i = 0; i < 8; i++)
    {
        SDL_RenderLine(g_state.renderer,
            cx + points[i][0] * inner_radius,
            cy + points[i][1] * inner_radius,
            cx + points[i][0] * outer_radius,
            cy + points[i][1] * outer_radius);
    }
}

static void sdl_round_movement_render_text(float center_x, float center_y,
    cptr text, SDL_Color color, int pixel_height)
{
    TTF_Font* font;
    int text_w;
    int text_h;

    if (!text || !text[0] || pixel_height <= 0)
        return;

    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return;

    text_w = sdl_ui_measure_text(font, text);
    text_h = MAX(pixel_height, TTF_GetFontHeight(font));
    sdl_ui_render_text(font, center_x - (float)text_w * 0.5f,
        center_y - (float)text_h * 0.5f, color, text);
}

static cptr sdl_round_movement_dir_label(int dir)
{
    switch (dir)
    {
    case 1: return "SW";
    case 2: return "S";
    case 3: return "SE";
    case 4: return "W";
    case 6: return "E";
    case 7: return "NW";
    case 8: return "N";
    case 9: return "NE";
    default: return "";
    }
}

void sdl_round_movement_reset_input_state(void)
{
    memset(&g_round_movement_press, 0, sizeof(g_round_movement_press));
}

bool sdl_round_movement_handle_event(const SDL_Event* ev)
{
    float x;
    float y;
    float dx;
    float dy;
    float dist;
    float radius;
    Uint64 press_time_ns;
    int dir;
    bool ctrl;

    if (!ev)
        return false;

    if (g_round_movement_press.active && !sdl_round_movement_commands_allowed())
    {
        sdl_round_movement_reset_input_state();
        g_state.need_present = true;
    }

    switch (ev->type)
    {
    case SDL_EVENT_FINGER_DOWN:
        if (!sdl_round_movement_commands_allowed())
            return false;
        if (!sdl_round_movement_event_xy(ev, &x, &y))
            return false;
        if (sdl_round_movement_point_excluded(x, y))
            return false;

        sdl_round_movement_reset_input_state();
        g_round_movement_press.active = true;
        g_round_movement_press.finger_id = ev->tfinger.fingerID;
        g_round_movement_press.center_x = x;
        g_round_movement_press.center_y = y;
        g_round_movement_press.current_x = x;
        g_round_movement_press.current_y = y;
        g_round_movement_press.start_time_ns = SDL_GetTicksNS();
        g_state.need_present = true;
        return true;

    case SDL_EVENT_FINGER_MOTION:
        if (!g_round_movement_press.active
            || g_round_movement_press.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_round_movement_event_xy(ev, &x, &y))
            return true;

        dx = x - g_round_movement_press.center_x;
        dy = y - g_round_movement_press.center_y;
        dist = SDL_sqrtf(dx * dx + dy * dy);
        radius = sdl_round_movement_radius_px(&g_views[0]);
        g_round_movement_press.current_x = x;
        g_round_movement_press.current_y = y;
        if (dist >= radius * 0.78f)
        {
            g_round_movement_press.selected_dir
                = sdl_round_movement_dir_for_delta(dx, dy);
            g_round_movement_press.selected_ctrl
                = dist >= sdl_round_movement_ctrl_radius_px(radius);
        }
        else
        {
            g_round_movement_press.selected_dir = 0;
            g_round_movement_press.selected_ctrl = false;
        }
        g_state.need_present = true;
        return true;

    case SDL_EVENT_FINGER_UP:
        if (!g_round_movement_press.active
            || g_round_movement_press.finger_id != ev->tfinger.fingerID)
        {
            return false;
        }
        if (!sdl_round_movement_event_xy(ev, &x, &y))
        {
            sdl_round_movement_reset_input_state();
            g_state.need_present = true;
            return true;
        }

        dx = x - g_round_movement_press.center_x;
        dy = y - g_round_movement_press.center_y;
        dist = SDL_sqrtf(dx * dx + dy * dy);
        radius = sdl_round_movement_radius_px(&g_views[0]);
        press_time_ns = SDL_GetTicksNS() - g_round_movement_press.start_time_ns;
        dir = 0;
        ctrl = false;

        if (dist <= radius * 0.34f)
        {
            if (press_time_ns < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
                dir = g_round_movement_last_dir;
        }
        else if (dist >= radius * 0.78f)
        {
            dir = sdl_round_movement_dir_for_delta(dx, dy);
            ctrl = dist >= sdl_round_movement_ctrl_radius_px(radius);
        }
        else if (g_round_movement_press.selected_dir)
        {
            dir = g_round_movement_press.selected_dir;
            ctrl = g_round_movement_press.selected_ctrl;
        }

        sdl_round_movement_reset_input_state();
        g_state.need_present = true;
        if (dir)
            sdl_round_movement_send_dir(dir, ctrl);
        return true;

    case SDL_EVENT_FINGER_CANCELED:
        if (g_round_movement_press.active
            && g_round_movement_press.finger_id == ev->tfinger.fingerID)
        {
            sdl_round_movement_reset_input_state();
            g_state.need_present = true;
            return true;
        }
        return false;

    default:
        return false;
    }
}

void sdl_round_movement_render(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot)
{
    SDL_Color frame = { 230, 232, 238, 150 };
    SDL_Color accent = { 92, 158, 255, 190 };
    SDL_Color selected = { 255, 212, 92, 232 };
    SDL_Color ctrl_selected = { 255, 104, 104, 236 };
    SDL_Color line_color;
    SDL_FRect target_rect;
    float radius;
    float ctrl_radius;
    float inner_radius;
    float dx;
    float dy;
    float dist;
    float end_x;
    float end_y;
    float center_x;
    float center_y;
    float current_x;
    float current_y;
    Uint64 press_time_ns;
    bool center_repeat;
    int target_dir;
    bool target_ctrl;

    if (!main_view || !snapshot || !g_round_movement_press.active)
        return;

    radius = sdl_round_movement_radius_px(main_view);
    ctrl_radius = sdl_round_movement_ctrl_radius_px(radius);
    inner_radius = radius * 0.34f;
    center_x = g_round_movement_press.center_x
        - (float)(main_view->rect.x + main_view->margin_x);
    center_y = g_round_movement_press.center_y
        - (float)(main_view->rect.y + main_view->margin_y);
    current_x = g_round_movement_press.current_x
        - (float)(main_view->rect.x + main_view->margin_x);
    current_y = g_round_movement_press.current_y
        - (float)(main_view->rect.y + main_view->margin_y);
    dx = current_x - center_x;
    dy = current_y - center_y;
    dist = SDL_sqrtf(dx * dx + dy * dy);
    press_time_ns = SDL_GetTicksNS() - g_round_movement_press.start_time_ns;
    center_repeat = dist <= inner_radius
        && press_time_ns < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL
        && g_round_movement_last_dir != 0;
    target_dir = g_round_movement_press.selected_dir
        ? g_round_movement_press.selected_dir
        : (center_repeat ? g_round_movement_last_dir : 0);
    target_ctrl = g_round_movement_press.selected_dir
        && g_round_movement_press.selected_ctrl;

    if (target_dir && p_ptr
        && sdl_scene_dungeon_map_cell_rect(main_view, snapshot,
            (s16b)(p_ptr->py + ddy[target_dir]),
            (s16b)(p_ptr->px + ddx[target_dir]), &target_rect))
    {
        SDL_Color highlight = target_ctrl ? ctrl_selected : selected;

        highlight.a = target_ctrl ? 96 : 72;
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_state.renderer, highlight.r, highlight.g,
            highlight.b, highlight.a);
        SDL_RenderFillRect(g_state.renderer, &target_rect);
        highlight.a = 200;
        SDL_SetRenderDrawColor(g_state.renderer, highlight.r, highlight.g,
            highlight.b, highlight.a);
        SDL_RenderRect(g_state.renderer, &target_rect);
    }

    sdl_round_movement_draw_circle(center_x, center_y, radius, frame);
    sdl_round_movement_draw_circle(center_x, center_y, radius - 2.0f, frame);
    sdl_round_movement_draw_circle(center_x, center_y, inner_radius, accent);
    sdl_round_movement_draw_sector_lines(center_x, center_y, inner_radius,
        radius, frame);
    frame.a = target_ctrl ? 230 : 118;
    if (target_ctrl)
    {
        frame.r = ctrl_selected.r;
        frame.g = ctrl_selected.g;
        frame.b = ctrl_selected.b;
    }
    sdl_round_movement_draw_circle(center_x, center_y, ctrl_radius, frame);
    sdl_round_movement_draw_sector_lines(center_x, center_y, radius,
        ctrl_radius, frame);

    end_x = current_x;
    end_y = current_y;
    if (dist > ctrl_radius && dist > 0.0f)
    {
        end_x = center_x + dx * ctrl_radius / dist;
        end_y = center_y + dy * ctrl_radius / dist;
    }

    line_color = target_ctrl ? ctrl_selected : accent;
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, line_color.r, line_color.g,
        line_color.b, line_color.a);
    SDL_RenderLine(g_state.renderer, center_x, center_y, end_x, end_y);

    if (target_dir)
    {
        sdl_round_movement_render_text(center_x, center_y,
            sdl_round_movement_dir_label(target_dir), selected,
            sdl_ui_scale_px(18.0f));
    }
    if (target_ctrl)
    {
        sdl_round_movement_render_text(center_x,
            center_y - radius - sdl_ui_scale_px(18.0f),
            "Ctrl", ctrl_selected, sdl_ui_scale_px(16.0f));
    }
}
