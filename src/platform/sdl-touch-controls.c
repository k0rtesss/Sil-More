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
#include "app/app-scene-dungeon.h"
#include "app/app-ui-command.h"
#include "sdl-menu/sdl-scene-menu.h"
#include "sdl-main-internal.h"
#include "sdl-touch-controls.h"

typedef struct touch_zone_press_state {
    bool active;
    SDL_FingerID finger_id;
    int zone;
    float start_x;
    float start_y;
    Uint64 start_time;
} touch_zone_press_state;

typedef struct touch_top_panel_press_state {
    bool active;
    SDL_FingerID finger_id;
    int slot;
    float start_x;
    float start_y;
    Uint64 start_time;
} touch_top_panel_press_state;

typedef enum sdl_touch_zone {
    TOUCH_ZONE_LEFT_NW = 0,
    TOUCH_ZONE_LEFT_N,
    TOUCH_ZONE_LEFT_W,
    TOUCH_ZONE_LEFT_CENTER,
    TOUCH_ZONE_LEFT_SW,
    TOUCH_ZONE_LEFT_S,
    TOUCH_ZONE_RIGHT_N,
    TOUCH_ZONE_RIGHT_NE,
    TOUCH_ZONE_RIGHT_CENTER,
    TOUCH_ZONE_RIGHT_E,
    TOUCH_ZONE_RIGHT_S,
    TOUCH_ZONE_RIGHT_SE,
    TOUCH_ZONE_COUNT
} sdl_touch_zone;

enum {
    SDL_TOUCH_TOP_PANEL_FLASH_NS = 150000000ULL
};

static bool g_touch_runtime_ready = false;
static bool g_touch_top_panel_open = false;
static int g_touch_top_panel_pressed_slot = -1;
static int g_touch_top_panel_flash_slot = -1;
static Uint64 g_touch_top_panel_flash_until = 0;
static touch_zone_press_state g_touch_zone_press;
static touch_top_panel_press_state g_touch_top_panel_press;

static int sdl_touch_profile_normalized(int profile);
static int sdl_touch_zone_overlay_mode_normalized(int mode);
static int sdl_touch_corner_up_down_side_normalized(int side);
static int sdl_touch_top_panel_mode_normalized(int mode);
static float sdl_touch_clampf(float value, float min_value, float max_value);
static void sdl_touch_runtime_ensure_state(void);
static const app_dungeon_snapshot* sdl_touch_current_dungeon_snapshot(void);
static const app_dungeon_overlay_snapshot* sdl_touch_overlay_snapshot(
    const app_dungeon_snapshot* snapshot);
static bool sdl_touch_semantic_overlay_active(void);
static bool sdl_touch_main_view_screen_rect(SDL_Rect* out_rect);
static bool sdl_touch_zone_overlay_visible(void);
static bool sdl_touch_zone_layout_visible(void);
static bool sdl_touch_zone_controls_active(void);
static bool sdl_touch_zone_compute_layout_for_screen(const SDL_Rect* screen,
    SDL_FRect* zone_rects);
static bool sdl_touch_zone_compute_layout(SDL_FRect* zone_rects);
static bool sdl_touch_zone_point_to_zone(float x, float y, int* out_zone);
static bool sdl_touch_corner_up_down_on_left(void);
static int sdl_touch_zone_center_binding_index(int zone, bool long_press);
static int sdl_touch_zone_binding_for_center(int zone, bool long_press);
static int sdl_touch_zone_corner_action_binding_index(int zone,
    bool long_press);
static int sdl_touch_zone_binding_for_corner_action(int zone,
    bool long_press);
static void sdl_touch_corner_action_binding_label(int binding, char* buf,
    size_t buflen);
static bool sdl_touch_zone_corner_action_label(int zone, char* name,
    size_t name_len, char* symbol, size_t symbol_len);
static void sdl_touch_zone_button_label(int zone, char* name, size_t name_len,
    char* symbol, size_t symbol_len);
static bool sdl_touch_zone_is_arrow(int zone);
static int sdl_touch_zone_arrow_dir(int zone);
static void sdl_touch_zone_send(int zone, bool long_press);
static void sdl_touch_zone_cancel_press(void);
static bool sdl_touch_zone_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
static bool sdl_touch_zone_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
static bool sdl_touch_zone_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
static int sdl_touch_zone_pending_timeout_ms(Uint64 now_ns);
static bool sdl_touch_zone_flush_pending_press(Uint64 now_ns);
static void sdl_touch_zone_render_markers_for_screen(const SDL_Rect* screen);
static bool sdl_touch_top_panel_layout_visible(void);
static void sdl_touch_top_panel_set_open(bool open);
static bool sdl_touch_top_panel_compute_layout_for_screen(
    const SDL_Rect* screen, SDL_FRect* button_rects, SDL_FRect* out_panel);
static bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects,
    SDL_FRect* out_panel);
static bool sdl_touch_top_panel_compute_reopen_rect(SDL_FRect* out_rect);
static bool sdl_touch_top_panel_point_to_reopen(float x, float y);
static int sdl_touch_top_panel_first_visible_slot(void);
static int sdl_touch_top_panel_visible_button_count(void);
static bool sdl_touch_top_panel_point_to_slot(float x, float y, int* out_slot);
static int sdl_touch_top_panel_binding_for_slot(int slot, bool long_press);
static void sdl_touch_top_panel_label_for_slot(int slot, bool long_press,
    char* buf, size_t buflen);
static void sdl_touch_top_panel_render_buttons(const SDL_FRect* button_rects);
static void sdl_touch_top_panel_render_for_screen(const SDL_Rect* screen);
static void sdl_touch_top_panel_send_slot(int slot, bool long_press);
static void sdl_touch_top_panel_cancel_press(void);
static bool sdl_touch_top_panel_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
static bool sdl_touch_top_panel_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
static bool sdl_touch_top_panel_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
static int sdl_touch_top_panel_pending_timeout_ms(Uint64 now_ns);
static bool sdl_touch_top_panel_flush_pending_press(Uint64 now_ns);
static int sdl_touch_profile_normalized(int profile)
{
    if (profile >= SDL_TOUCH_PROFILE_TOUCH_PANE
        && profile < SDL_TOUCH_PROFILE_COUNT)
    {
        return profile;
    }
    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}
static int sdl_touch_zone_overlay_mode_normalized(int mode)
{
    if (mode >= SDL_TOUCH_ZONE_OVERLAY_OFF
        && mode < SDL_TOUCH_ZONE_OVERLAY_COUNT)
    {
        return mode;
    }
    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}
static int sdl_touch_corner_up_down_side_normalized(int side)
{
    if (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
        || side == SDL_TOUCH_CORNER_UP_DOWN_RIGHT)
    {
        return side;
    }
    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}
static int sdl_touch_top_panel_mode_normalized(int mode)
{
    if (mode == SDL_TOUCH_TOP_PANEL_MODE_SHORT
        || mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
    {
        return mode;
    }
    return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
}
static float sdl_touch_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void sdl_touch_runtime_ensure_state(void)
{
    if (g_touch_runtime_ready)
        return;

    g_touch_top_panel_open = config.touch_top_panel_default_open;
    g_touch_runtime_ready = true;
}

static const app_dungeon_snapshot* sdl_touch_current_dungeon_snapshot(void)
{
    app_session* session;
    const app_snapshot* snapshot;

    session = app_session_current();
    if (!session)
        return NULL;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return NULL;

    return app_session_dungeon_snapshot(session);
}

static const app_dungeon_overlay_snapshot* sdl_touch_overlay_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    if (!snapshot || snapshot->overlay_size < sizeof(app_dungeon_overlay_snapshot))
        return NULL;

    return (const app_dungeon_overlay_snapshot*)snapshot->overlay_data;
}

static bool sdl_touch_semantic_overlay_active(void)
{
    const app_dungeon_overlay_snapshot* overlay =
        sdl_touch_overlay_snapshot(sdl_touch_current_dungeon_snapshot());

    if (!overlay)
        return false;

    return overlay->interaction.kind != APP_INTERACTION_KIND_NONE
        || (overlay->flags & APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_TRANSIENT_MENU)
        || overlay->transient_scene.panel_count > 0;
}

static bool sdl_touch_main_view_screen_rect(SDL_Rect* out_rect)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int w;
    int h;

    if (out_rect)
        memset(out_rect, 0, sizeof(*out_rect));
    if (!view->ready)
        return false;

    w = view->cols * view->cell_w;
    h = view->rows * view->cell_h;
    if (w <= 0 || h <= 0)
        return false;

    if (out_rect) {
        out_rect->x = view->rect.x + view->margin_x;
        out_rect->y = view->rect.y + view->margin_y;
        out_rect->w = w;
        out_rect->h = h;
    }

    return true;
}

static bool sdl_touch_zone_overlay_visible(void)
{
    return sdl_touch_profile_normalized(config.touch_profile)
            == SDL_TOUCH_PROFILE_CORNERS
        && !config.touch_round_movement_enabled
        && sdl_touch_current_dungeon_snapshot() != NULL
        && !sdl_touch_semantic_overlay_active();
}

static bool sdl_touch_zone_layout_visible(void)
{
    return sdl_touch_zone_overlay_visible();
}

static bool sdl_touch_zone_controls_active(void)
{
    return sdl_touch_zone_layout_visible();
}

static bool sdl_touch_zone_compute_layout_for_screen(const SDL_Rect* screen,
    SDL_FRect* zone_rects)
{
    int size_px;
    int max_size_px;
    int start_y_px;
    float size;
    float left_x;
    float right_x;
    float start_y;

    if (!screen || !zone_rects || screen->w <= 0 || screen->h <= 0)
        return false;

    size_px = screen->h / 3;
    max_size_px = screen->w / 4;
    if (size_px > max_size_px)
        size_px = max_size_px;
    if (size_px <= 0)
        return false;

    start_y_px = screen->y + (screen->h - size_px * 3) / 2;
    size = (float)size_px;
    left_x = (float)screen->x;
    right_x = (float)(screen->x + screen->w - size_px * 2);
    start_y = (float)start_y_px;

    zone_rects[TOUCH_ZONE_LEFT_NW] = (SDL_FRect){
        .x = left_x, .y = start_y, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_LEFT_N] = (SDL_FRect){
        .x = left_x + size, .y = start_y, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_LEFT_W] = (SDL_FRect){
        .x = left_x, .y = start_y + size, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_LEFT_CENTER] = (SDL_FRect){
        .x = left_x + size, .y = start_y + size, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_LEFT_SW] = (SDL_FRect){
        .x = left_x, .y = start_y + size * 2.0f, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_LEFT_S] = (SDL_FRect){
        .x = left_x + size, .y = start_y + size * 2.0f, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_N] = (SDL_FRect){
        .x = right_x, .y = start_y, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_NE] = (SDL_FRect){
        .x = right_x + size, .y = start_y, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_CENTER] = (SDL_FRect){
        .x = right_x, .y = start_y + size, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_E] = (SDL_FRect){
        .x = right_x + size, .y = start_y + size, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_S] = (SDL_FRect){
        .x = right_x, .y = start_y + size * 2.0f, .w = size, .h = size
    };
    zone_rects[TOUCH_ZONE_RIGHT_SE] = (SDL_FRect){
        .x = right_x + size, .y = start_y + size * 2.0f, .w = size, .h = size
    };

    return true;
}

static bool sdl_touch_zone_compute_layout(SDL_FRect* zone_rects)
{
    SDL_Rect screen;

    if (!zone_rects)
        return false;
    if (!sdl_touch_zone_layout_visible())
        return false;
    if (!sdl_touch_main_view_screen_rect(&screen))
        return false;

    return sdl_touch_zone_compute_layout_for_screen(&screen, zone_rects);
}

static bool sdl_touch_zone_point_to_zone(float x, float y, int* out_zone)
{
    SDL_FRect zone_rects[TOUCH_ZONE_COUNT];

    if (out_zone)
        *out_zone = -1;
    if (!sdl_touch_zone_compute_layout(zone_rects))
        return false;

    for (int i = 0; i < TOUCH_ZONE_COUNT; i++) {
        const SDL_FRect* rect = &zone_rects[i];

        if (x >= rect->x && x < rect->x + rect->w
            && y >= rect->y && y < rect->y + rect->h)
        {
            if (out_zone)
                *out_zone = i;
            return true;
        }
    }

    return false;
}

static bool sdl_touch_corner_up_down_on_left(void)
{
    return sdl_touch_corner_up_down_side_normalized(
        config.touch_corner_up_down_side) == SDL_TOUCH_CORNER_UP_DOWN_LEFT;
}

static int sdl_touch_zone_center_binding_index(int zone, bool long_press)
{
    switch (zone) {
    case TOUCH_ZONE_LEFT_CENTER:
        return long_press ? SDL_TOUCH_ZONE_CENTER_LEFT_LONG_TAP
                          : SDL_TOUCH_ZONE_CENTER_LEFT_TAP;
    case TOUCH_ZONE_RIGHT_CENTER:
        return long_press ? SDL_TOUCH_ZONE_CENTER_RIGHT_LONG_TAP
                          : SDL_TOUCH_ZONE_CENTER_RIGHT_TAP;
    default:
        return -1;
    }
}

static int sdl_touch_zone_binding_for_center(int zone, bool long_press)
{
    int index = sdl_touch_zone_center_binding_index(zone, long_press);

    if (index < 0 || index >= SDL_TOUCH_ZONE_CENTER_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;

    return config.touch_zone_center_bindings[index];
}

static int sdl_touch_zone_corner_action_binding_index(int zone,
    bool long_press)
{
    bool up_down_left = sdl_touch_corner_up_down_on_left();

    switch (zone) {
    case TOUCH_ZONE_LEFT_N:
        if (up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_TOP_TAP;
    case TOUCH_ZONE_RIGHT_N:
        if (!up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_TOP_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_TOP_TAP;
    case TOUCH_ZONE_LEFT_S:
        if (up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP;
    case TOUCH_ZONE_RIGHT_S:
        if (!up_down_left)
            return -1;
        return long_press ? SDL_TOUCH_CORNER_ACTION_BOTTOM_LONG_TAP
                          : SDL_TOUCH_CORNER_ACTION_BOTTOM_TAP;
    default:
        return -1;
    }
}

static int sdl_touch_zone_binding_for_corner_action(int zone,
    bool long_press)
{
    int index = sdl_touch_zone_corner_action_binding_index(zone, long_press);

    if (index < 0 || index >= SDL_TOUCH_CORNER_ACTION_BINDING_COUNT)
        return GAMEPAD_BIND_NONE;

    return config.touch_corner_action_bindings[index];
}

static void sdl_touch_corner_action_binding_label(int binding, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case 'f':
        SDL_strlcpy(buf, "Shoot", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Shoot 2", buflen);
        return;
    default:
        binding_action_short(binding, buf, buflen);
        return;
    }
}

static bool sdl_touch_zone_corner_action_label(int zone, char* name,
    size_t name_len, char* symbol, size_t symbol_len)
{
    int tap_binding;
    int long_binding;

    if (sdl_touch_zone_corner_action_binding_index(zone, false) < 0)
        return false;

    tap_binding = sdl_touch_zone_binding_for_corner_action(zone, false);
    long_binding = sdl_touch_zone_binding_for_corner_action(zone, true);
    sdl_touch_corner_action_binding_label(tap_binding, name, name_len);
    if (symbol && symbol_len) {
        if (long_binding == GAMEPAD_BIND_NONE) {
            symbol[0] = '\0';
        } else {
            sdl_touch_corner_action_binding_label(long_binding, symbol,
                symbol_len);
        }
    }

    return true;
}

static void sdl_touch_zone_button_label(int zone, char* name, size_t name_len,
    char* symbol, size_t symbol_len)
{
    if (name && name_len)
        name[0] = '\0';
    if (symbol && symbol_len)
        symbol[0] = '\0';
    if (!name || !name_len)
        return;

    switch (zone) {
    case TOUCH_ZONE_LEFT_NW:
        SDL_strlcpy(name, "NW", name_len);
        return;
    case TOUCH_ZONE_LEFT_N:
        if (sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "N", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_N:
        if (!sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "N", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_NE:
        SDL_strlcpy(name, "NE", name_len);
        return;
    case TOUCH_ZONE_LEFT_W:
        SDL_strlcpy(name, "W", name_len);
        return;
    case TOUCH_ZONE_RIGHT_E:
        SDL_strlcpy(name, "E", name_len);
        return;
    case TOUCH_ZONE_LEFT_SW:
        SDL_strlcpy(name, "SW", name_len);
        return;
    case TOUCH_ZONE_LEFT_S:
        if (sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "S", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_S:
        if (!sdl_touch_corner_up_down_on_left()) {
            SDL_strlcpy(name, "S", name_len);
            return;
        }
        (void)sdl_touch_zone_corner_action_label(zone, name, name_len,
            symbol, symbol_len);
        return;
    case TOUCH_ZONE_RIGHT_SE:
        SDL_strlcpy(name, "SE", name_len);
        return;
    case TOUCH_ZONE_LEFT_CENTER:
    case TOUCH_ZONE_RIGHT_CENTER:
        binding_action_short(sdl_touch_zone_binding_for_center(zone, false),
            name, name_len);
        if (symbol && symbol_len)
            binding_action_short(sdl_touch_zone_binding_for_center(zone, true),
                symbol, symbol_len);
        return;
    default:
        return;
    }
}

static bool sdl_touch_zone_is_arrow(int zone)
{
    bool up_down_left = sdl_touch_corner_up_down_on_left();

    return zone == TOUCH_ZONE_LEFT_NW
        || zone == TOUCH_ZONE_LEFT_W
        || zone == TOUCH_ZONE_LEFT_SW
        || zone == TOUCH_ZONE_RIGHT_NE
        || zone == TOUCH_ZONE_RIGHT_E
        || zone == TOUCH_ZONE_RIGHT_SE
        || (up_down_left
            && (zone == TOUCH_ZONE_LEFT_N || zone == TOUCH_ZONE_LEFT_S))
        || (!up_down_left
            && (zone == TOUCH_ZONE_RIGHT_N || zone == TOUCH_ZONE_RIGHT_S));
}

static int sdl_touch_zone_arrow_dir(int zone)
{
    switch (zone) {
    case TOUCH_ZONE_LEFT_NW:
        return 7;
    case TOUCH_ZONE_LEFT_N:
    case TOUCH_ZONE_RIGHT_N:
        return 8;
    case TOUCH_ZONE_RIGHT_NE:
        return 9;
    case TOUCH_ZONE_LEFT_W:
        return 4;
    case TOUCH_ZONE_RIGHT_E:
        return 6;
    case TOUCH_ZONE_LEFT_SW:
        return 1;
    case TOUCH_ZONE_LEFT_S:
    case TOUCH_ZONE_RIGHT_S:
        return 2;
    case TOUCH_ZONE_RIGHT_SE:
        return 3;
    default:
        return 0;
    }
}

static void sdl_touch_zone_send(int zone, bool long_press)
{
    if (!sdl_touch_zone_controls_active())
        return;

    if (sdl_touch_zone_is_arrow(zone)) {
        int dir = sdl_touch_zone_arrow_dir(zone);

        if (dir != 0) {
            (void)platform_submit_directional_movement(dir, false, long_press,
                sdl_gamepad_alt_active(), APP_INPUT_DEVICE_TOUCH,
                APP_INPUT_TYPE_POINTER_BUTTON, 0, APP_INPUT_FLAG_PRESS,
                APP_UI_SEMANTIC_COMMAND_TOUCH_ZONE,
                APP_MOVEMENT_SEMANTIC_TRIGGER_TOUCH_ZONE);
        }
        return;
    }

    if (sdl_touch_zone_center_binding_index(zone, long_press) >= 0) {
        sdl_touch_pane_send_binding(sdl_touch_zone_binding_for_center(zone,
            long_press), false, false);
        return;
    }

    if (sdl_touch_zone_corner_action_binding_index(zone, long_press) >= 0) {
        sdl_touch_pane_send_binding(
            sdl_touch_zone_binding_for_corner_action(zone, long_press),
            false, false);
    }
}

static void sdl_touch_zone_cancel_press(void)
{
    g_touch_zone_press.active = false;
    g_touch_zone_press.finger_id = 0;
    g_touch_zone_press.zone = -1;
    g_touch_zone_press.start_x = 0.0f;
    g_touch_zone_press.start_y = 0.0f;
    g_touch_zone_press.start_time = 0;
}

static bool sdl_touch_zone_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int zone = -1;

    if (!sdl_touch_zone_controls_active())
        return false;
    if (sdl_menu_hit_test(x, y))
        return false;
    if (!sdl_touch_zone_point_to_zone(x, y, &zone) || zone < 0)
        return false;

    sdl_touch_zone_cancel_press();
    g_touch_zone_press.active = true;
    g_touch_zone_press.finger_id = finger_id;
    g_touch_zone_press.zone = zone;
    g_touch_zone_press.start_x = x;
    g_touch_zone_press.start_y = y;
    g_touch_zone_press.start_time = SDL_GetTicksNS();
    return true;
}

static bool sdl_touch_zone_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;
    float start_x;
    float start_y;
    int zone = -1;

    if (!g_touch_zone_press.active
        || g_touch_zone_press.finger_id != finger_id)
    {
        return false;
    }

    if (!sdl_touch_zone_point_to_zone(x, y, &zone)
        || zone != g_touch_zone_press.zone)
    {
        sdl_touch_zone_cancel_press();
        return true;
    }

    dx = x - g_touch_zone_press.start_x;
    dy = y - g_touch_zone_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        start_x = g_touch_zone_press.start_x;
        start_y = g_touch_zone_press.start_y;
        sdl_touch_zone_cancel_press();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    return true;
}

static bool sdl_touch_zone_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool long_press;
    int zone;
    int release_zone = -1;

    if (!g_touch_zone_press.active
        || g_touch_zone_press.finger_id != finger_id)
    {
        return false;
    }

    zone = g_touch_zone_press.zone;
    press_time = SDL_GetTicksNS() - g_touch_zone_press.start_time;
    long_press = press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL;
    (void)sdl_touch_zone_point_to_zone(x, y, &release_zone);
    sdl_touch_zone_cancel_press();
    if (release_zone != zone)
        return true;

    sdl_touch_zone_send(zone, long_press);
    return true;
}

static int sdl_touch_zone_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_zone_press.active)
        return -1;

    elapsed = now_ns - g_touch_zone_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

static bool sdl_touch_zone_flush_pending_press(Uint64 now_ns)
{
    int zone;

    if (!g_touch_zone_press.active)
        return false;
    if (!sdl_touch_zone_controls_active()) {
        sdl_touch_zone_cancel_press();
        return false;
    }
    if (now_ns - g_touch_zone_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    zone = g_touch_zone_press.zone;
    sdl_touch_zone_cancel_press();
    sdl_touch_zone_send(zone, true);
    return true;
}

static void sdl_touch_zone_render_markers_for_screen(const SDL_Rect* screen)
{
    SDL_FRect zone_rects[TOUCH_ZONE_COUNT];
    SDL_Color marker_color = g_state.palette[TERM_L_BLUE];
    int overlay_mode =
        sdl_touch_zone_overlay_mode_normalized(config.touch_zone_overlay_mode);
    bool draw_markers = overlay_mode == SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    bool draw_borders = overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS
        || overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS;
    bool draw_labels = overlay_mode == SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS;
    float size;
    float start_y;
    float bottom_y;
    float left_split_x;
    float left_inner_x;
    float right_inner_x;
    float right_split_x;
    float marker_len;
    float marker_thickness;
    float top_bottom_markers[4];

    if (overlay_mode == SDL_TOUCH_ZONE_OVERLAY_OFF)
        return;
    if (!screen || !sdl_touch_zone_compute_layout_for_screen(screen, zone_rects))
        return;

    if (draw_borders) {
        SDL_SetRenderDrawColor(g_state.renderer, marker_color.r,
            marker_color.g, marker_color.b, 36);
        for (int i = 0; i < TOUCH_ZONE_COUNT; i++)
            SDL_RenderRect(g_state.renderer, &zone_rects[i]);
    }

    if (draw_labels) {
        SDL_Color label_color = marker_color;

        label_color.a = 92;
        for (int i = 0; i < TOUCH_ZONE_COUNT; i++) {
            char name[32];
            char symbol[32];

            sdl_touch_zone_button_label(i, name, sizeof(name), symbol,
                sizeof(symbol));
            sdl_touch_pane_draw_button_text(&zone_rects[i], name, symbol,
                label_color);
        }
    }

    if (!draw_markers)
        return;

    size = zone_rects[TOUCH_ZONE_LEFT_NW].h;
    start_y = zone_rects[TOUCH_ZONE_LEFT_NW].y;
    bottom_y = start_y + size * 3.0f;
    left_split_x = zone_rects[TOUCH_ZONE_LEFT_N].x;
    left_inner_x = zone_rects[TOUCH_ZONE_LEFT_N].x + size;
    right_inner_x = zone_rects[TOUCH_ZONE_RIGHT_N].x;
    right_split_x = zone_rects[TOUCH_ZONE_RIGHT_NE].x;
    marker_len = sdl_touch_clampf(size * 0.20f, 18.0f, 44.0f);
    marker_thickness = sdl_touch_clampf(size * 0.018f, 2.0f, 4.0f);
    top_bottom_markers[0] = left_split_x;
    top_bottom_markers[1] = left_inner_x;
    top_bottom_markers[2] = right_inner_x;
    top_bottom_markers[3] = right_split_x;

    SDL_SetRenderDrawColor(g_state.renderer, marker_color.r, marker_color.g,
        marker_color.b, 150);

    for (int i = 1; i <= 2; i++) {
        float y = start_y + size * (float)i - marker_thickness * 0.5f;

        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)screen->x,
            .y = y,
            .w = marker_len,
            .h = marker_thickness,
        });
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = (float)(screen->x + screen->w) - marker_len,
            .y = y,
            .w = marker_len,
            .h = marker_thickness,
        });
    }

    for (int i = 0; i < (int)N_ELEMENTS(top_bottom_markers); i++) {
        float x = top_bottom_markers[i] - marker_thickness * 0.5f;

        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = x,
            .y = start_y,
            .w = marker_thickness,
            .h = marker_len,
        });
        SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
            .x = x,
            .y = bottom_y - marker_len,
            .w = marker_thickness,
            .h = marker_len,
        });
    }
}

static bool sdl_touch_top_panel_layout_visible(void)
{
    sdl_touch_runtime_ensure_state();
    return sdl_touch_current_dungeon_snapshot() != NULL
        && !sdl_touch_semantic_overlay_active();
}

static void sdl_touch_top_panel_set_open(bool open)
{
    sdl_touch_runtime_ensure_state();

    if (g_touch_top_panel_open == open)
        return;

    g_touch_top_panel_open = open;
    sdl_touch_controls_cancel_top_panel_press();
    g_state.need_present = true;
}

static bool sdl_touch_top_panel_compute_layout_for_screen(
    const SDL_Rect* screen, SDL_FRect* button_rects, SDL_FRect* out_panel)
{
    const sdl_view* main_view = &g_views[PANE_MAIN];
    float screen_w;
    float screen_h;
    float corner_size;
    float short_panel_w;
    float panel_x;
    float panel_y;
    float margin_top;
    float gap;
    float panel_w;
    float panel_h;
    float button_w;
    int active_count;
    int first_slot;

    if (!screen || screen->w <= 0 || screen->h <= 0)
        return false;

    screen_w = (float)screen->w;
    screen_h = (float)screen->h;
    corner_size = screen_h / 3.0f;
    if (corner_size > screen_w / 4.0f)
        corner_size = screen_w / 4.0f;

    short_panel_w = screen_w - corner_size * 4.0f;
    panel_x = (float)screen->x + corner_size * 2.0f;
    panel_w = short_panel_w;
    margin_top = screen_h * 0.018f;
    panel_y = (float)screen->y + margin_top;
    if (main_view->ready && main_view->cell_h > 0) {
        float row_zero_bottom = (float)screen->y + (float)main_view->cell_h;
        float row_gap = screen_h * 0.006f;

        if (panel_y < row_zero_bottom + row_gap)
            panel_y = row_zero_bottom + row_gap;
    }
    gap = short_panel_w * 0.018f;
    panel_h = screen_h * 0.12f;
    if (panel_h > corner_size * 0.48f)
        panel_h = corner_size * 0.48f;

    active_count = sdl_touch_top_panel_visible_button_count();
    first_slot = sdl_touch_top_panel_first_visible_slot();
    if (short_panel_w <= gap * (float)(SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT - 1))
        return false;

    button_w = (short_panel_w
        - gap * (float)(SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT - 1))
        / (float)SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT;
    if (button_w <= 0.0f)
        return false;
    if (active_count > SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT) {
        panel_x -= button_w + gap;
        panel_w += (button_w + gap) * 2.0f;
        if (panel_x < (float)screen->x) {
            float adjust = (float)screen->x - panel_x;

            panel_x += adjust;
            panel_w -= adjust;
        }
        if (panel_x + panel_w > (float)(screen->x + screen->w))
            panel_w = (float)(screen->x + screen->w) - panel_x;
        if (panel_w <= gap * (float)(active_count - 1))
            return false;
        button_w = (panel_w - gap * (float)(active_count - 1))
            / (float)active_count;
        if (button_w <= 0.0f)
            return false;
    }

    if (out_panel) {
        *out_panel = (SDL_FRect){
            .x = panel_x, .y = panel_y, .w = panel_w, .h = panel_h
        };
    }

    if (button_rects) {
        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
            button_rects[i] = (SDL_FRect){ 0 };

        for (int i = 0; i < active_count; i++) {
            int slot = first_slot + i;

            button_rects[slot] = (SDL_FRect){
                .x = panel_x + (button_w + gap) * (float)i,
                .y = panel_y,
                .w = button_w,
                .h = panel_h,
            };
        }
    }

    return true;
}

static int sdl_touch_top_panel_first_visible_slot(void)
{
    return sdl_touch_top_panel_mode_normalized(config.touch_top_panel_mode)
            == SDL_TOUCH_TOP_PANEL_MODE_LONG
        ? 0
        : 1;
}

static int sdl_touch_top_panel_visible_button_count(void)
{
    return sdl_touch_top_panel_mode_normalized(config.touch_top_panel_mode)
            == SDL_TOUCH_TOP_PANEL_MODE_LONG
        ? SDL_TOUCH_TOP_PANEL_BUTTON_COUNT
        : SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT;
}

static bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects,
    SDL_FRect* out_panel)
{
    SDL_Rect screen;

    if (!g_touch_top_panel_open)
        return false;
    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (!sdl_touch_main_view_screen_rect(&screen))
        return false;

    return sdl_touch_top_panel_compute_layout_for_screen(&screen, button_rects,
        out_panel);
}

static bool sdl_touch_top_panel_compute_reopen_rect(SDL_FRect* out_rect)
{
    SDL_Rect screen;
    SDL_FRect panel;
    float reopen_w;
    float reopen_h;

    if (!out_rect)
        return false;

    *out_rect = (SDL_FRect){ 0 };
    if (g_touch_top_panel_open)
        return false;
    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (!sdl_touch_main_view_screen_rect(&screen))
        return false;
    if (!sdl_touch_top_panel_compute_layout_for_screen(&screen, NULL, &panel))
        return false;

    reopen_w = sdl_touch_clampf(panel.w * 0.20f, 72.0f, 150.0f);
    reopen_h = sdl_touch_clampf(panel.h * 0.58f, 24.0f, 42.0f);
    *out_rect = (SDL_FRect){
        .x = panel.x + (panel.w - reopen_w) * 0.5f,
        .y = panel.y,
        .w = reopen_w,
        .h = reopen_h,
    };
    return true;
}

static bool sdl_touch_top_panel_point_to_reopen(float x, float y)
{
    SDL_FRect rect;

    if (!sdl_touch_top_panel_compute_reopen_rect(&rect))
        return false;

    return x >= rect.x && x < rect.x + rect.w
        && y >= rect.y && y < rect.y + rect.h;
}

static bool sdl_touch_top_panel_point_to_slot(float x, float y, int* out_slot)
{
    SDL_FRect button_rects[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int first_slot = sdl_touch_top_panel_first_visible_slot();
    int active_count = sdl_touch_top_panel_visible_button_count();

    if (out_slot)
        *out_slot = -1;
    if (!sdl_touch_top_panel_compute_layout(button_rects, NULL))
        return false;

    for (int i = 0; i < active_count; i++) {
        int slot = first_slot + i;
        const SDL_FRect* rect = &button_rects[slot];

        if (x >= rect->x && x < rect->x + rect->w
            && y >= rect->y && y < rect->y + rect->h)
        {
            if (out_slot)
                *out_slot = slot;
            return true;
        }
    }

    return false;
}

static int sdl_touch_top_panel_binding_for_slot(int slot, bool long_press)
{
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    return long_press ? config.touch_top_panel_long_bindings[slot]
                      : config.touch_top_panel_bindings[slot];
}

static void sdl_touch_top_panel_label_for_slot(int slot, bool long_press,
    char* buf, size_t buflen)
{
    int binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (slot < 0 || slot >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return;

    binding = sdl_touch_top_panel_binding_for_slot(slot, long_press);
    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }

    if (!long_press) {
        switch (binding) {
        case 'z': SDL_strlcpy(buf, "Wait", buflen); return;
        case 'h': SDL_strlcpy(buf, "Character", buflen); return;
        case 'i': SDL_strlcpy(buf, "Inventory", buflen); return;
        case 'j': SDL_strlcpy(buf, "Supply", buflen); return;
        case 'a': SDL_strlcpy(buf, "Staff", buflen); return;
        case 'l': SDL_strlcpy(buf, "View", buflen); return;
        case 'f': SDL_strlcpy(buf, "Shoot", buflen); return;
        default:
            break;
        }
    } else {
        switch (binding) {
        case 'Z': SDL_strlcpy(buf, "Rest", buflen); return;
        case '\t': SDL_strlcpy(buf, "Abilities", buflen); return;
        case 'e': SDL_strlcpy(buf, "Equipped", buflen); return;
        case 'j': SDL_strlcpy(buf, "Supply", buflen); return;
        case 'F': SDL_strlcpy(buf, "Shoot 2", buflen); return;
        default:
            break;
        }
    }

    if (binding == INPUT_BIND_CONFIRM) {
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    }

    binding_action_short(binding, buf, buflen);
}

static void sdl_touch_top_panel_render_buttons(const SDL_FRect* button_rects)
{
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    int first_slot = sdl_touch_top_panel_first_visible_slot();
    int active_count = sdl_touch_top_panel_visible_button_count();

    if (!button_rects)
        return;

    for (int i = 0; i < active_count; i++) {
        int slot = first_slot + i;
        SDL_Color text_color;
        SDL_Color border_color;
        SDL_FRect shadow = button_rects[slot];
        char tap_label[32];
        char long_label[32];
        int binding = sdl_touch_top_panel_binding_for_slot(slot, false);
        bool flashed = slot == g_touch_top_panel_flash_slot;
        bool pressed = slot == g_touch_top_panel_pressed_slot;

        shadow.x += 2.0f;
        shadow.y += 2.0f;
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        if (binding == GAMEPAD_BIND_NONE) {
            SDL_SetRenderDrawColor(g_state.renderer, 26, 26, 26, 250);
            text_color = muted;
            border_color = muted;
        } else if (pressed || flashed) {
            SDL_SetRenderDrawColor(g_state.renderer, 54, 66, 86, 250);
            text_color = accent;
            border_color = accent;
        } else {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 250);
            text_color = frame;
            border_color = frame;
        }

        SDL_RenderFillRect(g_state.renderer, &button_rects[slot]);
        SDL_SetRenderDrawColor(g_state.renderer, border_color.r,
            border_color.g, border_color.b, 220);
        SDL_RenderRect(g_state.renderer, &button_rects[slot]);

        sdl_touch_top_panel_label_for_slot(slot, false, tap_label,
            sizeof(tap_label));
        sdl_touch_top_panel_label_for_slot(slot, true, long_label,
            sizeof(long_label));
        if (sdl_touch_top_panel_binding_for_slot(slot, true) == GAMEPAD_BIND_NONE)
            long_label[0] = '\0';

        sdl_touch_pane_draw_button_text(&button_rects[slot], long_label,
            tap_label, text_color);
    }
}

static void sdl_touch_top_panel_render_for_screen(const SDL_Rect* screen)
{
    SDL_FRect button_rects[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    SDL_FRect reopen_rect;

    if (!screen)
        return;
    if (!sdl_touch_top_panel_layout_visible()) {
        sdl_touch_controls_cancel_top_panel_press();
        return;
    }
    if (!g_touch_top_panel_open) {
        if (sdl_touch_top_panel_compute_reopen_rect(&reopen_rect)) {
            SDL_Color frame = g_state.palette[TERM_L_BLUE];
            SDL_Color label = frame;

            SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g,
                frame.b, 48);
            SDL_RenderFillRect(g_state.renderer, &reopen_rect);
            SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g,
                frame.b, 140);
            SDL_RenderRect(g_state.renderer, &reopen_rect);
            label.a = 180;
            sdl_touch_pane_draw_button_text(&reopen_rect, "Top", "^",
                label);
        }
        return;
    }
    if (!sdl_touch_top_panel_compute_layout_for_screen(screen, button_rects,
            NULL))
    {
        return;
    }

    sdl_touch_top_panel_render_buttons(button_rects);
    if (g_touch_top_panel_flash_slot >= 0
        && SDL_GetTicksNS() >= g_touch_top_panel_flash_until)
    {
        g_touch_top_panel_flash_slot = -1;
        g_touch_top_panel_flash_until = 0;
    }
}

static void sdl_touch_top_panel_send_slot(int slot, bool long_press)
{
    sdl_touch_pane_send_binding(sdl_touch_top_panel_binding_for_slot(slot,
        long_press), false, false);
}

static void sdl_touch_top_panel_cancel_press(void)
{
    if (!g_touch_top_panel_press.active && g_touch_top_panel_pressed_slot < 0)
        return;

    g_touch_top_panel_press.active = false;
    g_touch_top_panel_press.finger_id = 0;
    g_touch_top_panel_press.slot = -1;
    g_touch_top_panel_press.start_x = 0.0f;
    g_touch_top_panel_press.start_y = 0.0f;
    g_touch_top_panel_press.start_time = 0;
    g_touch_top_panel_pressed_slot = -1;
    g_state.need_present = true;
}

static bool sdl_touch_top_panel_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    int slot = -1;

    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (!g_touch_top_panel_open) {
        if (!sdl_touch_top_panel_point_to_reopen(x, y))
            return false;
        sdl_touch_top_panel_set_open(true);
        return true;
    }
    if (sdl_menu_hit_test(x, y))
        return false;
    if (!sdl_touch_top_panel_point_to_slot(x, y, &slot) || slot < 0)
        return false;

    sdl_touch_controls_cancel_top_panel_press();
    g_touch_top_panel_press.active = true;
    g_touch_top_panel_press.finger_id = finger_id;
    g_touch_top_panel_press.slot = slot;
    g_touch_top_panel_press.start_x = x;
    g_touch_top_panel_press.start_y = y;
    g_touch_top_panel_press.start_time = SDL_GetTicksNS();
    g_touch_top_panel_pressed_slot = slot;
    g_state.need_present = true;
    return true;
}

static bool sdl_touch_top_panel_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;
    float start_x;
    float start_y;
    int slot = -1;

    if (!g_touch_top_panel_press.active
        || g_touch_top_panel_press.finger_id != finger_id)
    {
        return false;
    }

    dx = x - g_touch_top_panel_press.start_x;
    dy = y - g_touch_top_panel_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        start_x = g_touch_top_panel_press.start_x;
        start_y = g_touch_top_panel_press.start_y;
        sdl_touch_controls_cancel_top_panel_press();
        if (sdl_touch_swipe_handle_pointer_down(start_x, start_y, finger_id))
            (void)sdl_touch_swipe_handle_pointer_motion(x, y, finger_id);
        return true;
    }

    if (!sdl_touch_top_panel_point_to_slot(x, y, &slot)
        || slot != g_touch_top_panel_press.slot)
    {
        sdl_touch_controls_cancel_top_panel_press();
        return true;
    }

    return true;
}

static bool sdl_touch_top_panel_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool long_press;
    int slot;
    int release_slot = -1;

    if (!g_touch_top_panel_press.active
        || g_touch_top_panel_press.finger_id != finger_id)
    {
        return false;
    }

    slot = g_touch_top_panel_press.slot;
    press_time = SDL_GetTicksNS() - g_touch_top_panel_press.start_time;
    long_press = press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL;
    (void)sdl_touch_top_panel_point_to_slot(x, y, &release_slot);
    sdl_touch_controls_cancel_top_panel_press();
    if (release_slot != slot)
        return true;

    sdl_touch_top_panel_send_slot(slot, long_press);
    g_touch_top_panel_flash_slot = slot;
    g_touch_top_panel_flash_until = SDL_GetTicksNS()
        + SDL_TOUCH_TOP_PANEL_FLASH_NS;
    g_state.need_present = true;
    return true;
}

static int sdl_touch_top_panel_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_top_panel_press.active)
        return -1;
    if (!sdl_touch_top_panel_layout_visible() || !g_touch_top_panel_open)
        return 0;

    elapsed = now_ns - g_touch_top_panel_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

static bool sdl_touch_top_panel_flush_pending_press(Uint64 now_ns)
{
    int slot;

    if (!g_touch_top_panel_press.active)
        return false;
    if (!sdl_touch_top_panel_layout_visible() || !g_touch_top_panel_open) {
        sdl_touch_controls_cancel_top_panel_press();
        return false;
    }
    if (now_ns - g_touch_top_panel_press.start_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    slot = g_touch_top_panel_press.slot;
    sdl_touch_controls_cancel_top_panel_press();
    sdl_touch_top_panel_send_slot(slot, true);
    g_touch_top_panel_flash_slot = slot;
    g_touch_top_panel_flash_until = SDL_GetTicksNS()
        + SDL_TOUCH_TOP_PANEL_FLASH_NS;
    g_state.need_present = true;
    return true;
}

void sdl_touch_controls_reset_input_state(void)
{
    sdl_touch_runtime_ensure_state();
    sdl_touch_zone_cancel_press();
    sdl_touch_top_panel_cancel_press();
    g_touch_top_panel_open = config.touch_top_panel_default_open;
    g_touch_top_panel_flash_slot = -1;
    g_touch_top_panel_flash_until = 0;
    g_touch_top_panel_pressed_slot = -1;
}
int sdl_touch_controls_pending_timeout_ms(Uint64 now_ns)
{
    int timeout_ms = -1;
    int zone_timeout_ms = sdl_touch_zone_pending_timeout_ms(now_ns);
    int top_panel_timeout_ms = sdl_touch_top_panel_pending_timeout_ms(now_ns);
    if (zone_timeout_ms >= 0)
        timeout_ms = zone_timeout_ms;
    if (timeout_ms < 0
        || (top_panel_timeout_ms >= 0 && top_panel_timeout_ms < timeout_ms))
    {
        timeout_ms = top_panel_timeout_ms;
    }
    return timeout_ms;
}
bool sdl_touch_controls_flush_pending_press(Uint64 now_ns)
{
    if (sdl_touch_zone_flush_pending_press(now_ns))
        return true;
    if (sdl_touch_top_panel_flush_pending_press(now_ns))
        return true;
    return false;
}
bool sdl_touch_controls_top_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_top_panel_handle_pointer_down(x, y, finger_id);
}
bool sdl_touch_controls_top_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_top_panel_handle_pointer_motion(x, y, finger_id);
}
bool sdl_touch_controls_top_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_top_panel_handle_pointer_up(x, y, finger_id);
}
bool sdl_touch_controls_zone_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_zone_handle_pointer_down(x, y, finger_id);
}
bool sdl_touch_controls_zone_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_zone_handle_pointer_motion(x, y, finger_id);
}
bool sdl_touch_controls_zone_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id)
{
    return sdl_touch_zone_handle_pointer_up(x, y, finger_id);
}

bool sdl_touch_controls_point_blocks_map(float x, float y)
{
    int slot = -1;
    int zone = -1;

    if (sdl_touch_top_panel_layout_visible()) {
        if (g_touch_top_panel_open) {
            if (sdl_touch_top_panel_point_to_slot(x, y, &slot) && slot >= 0)
                return true;
        } else if (sdl_touch_top_panel_point_to_reopen(x, y)) {
            return true;
        }
    }

    return sdl_touch_zone_controls_active()
        && sdl_touch_zone_point_to_zone(x, y, &zone)
        && zone >= 0;
}

void sdl_touch_controls_handle_pointer_canceled(SDL_FingerID finger_id)
{
    if (g_touch_zone_press.active && g_touch_zone_press.finger_id == finger_id)
        sdl_touch_zone_cancel_press();
    if (g_touch_top_panel_press.active
        && g_touch_top_panel_press.finger_id == finger_id)
    {
        sdl_touch_top_panel_cancel_press();
    }
}
void sdl_touch_controls_render(int canvas_w, int canvas_h)
{
    SDL_Rect screen;
    sdl_touch_runtime_ensure_state();
    if (canvas_w <= 0 || canvas_h <= 0)
        return;
    screen = (SDL_Rect){ 0, 0, canvas_w, canvas_h };
    if (sdl_touch_zone_layout_visible()) {
        sdl_touch_zone_render_markers_for_screen(&screen);
    } else {
        sdl_touch_zone_cancel_press();
    }
    if (sdl_touch_top_panel_layout_visible()) {
        sdl_touch_top_panel_render_for_screen(&screen);
    } else {
        sdl_touch_top_panel_cancel_press();
    }
}
void sdl_touch_controls_set_top_panel_open(bool open)
{
    sdl_touch_top_panel_set_open(open);
}
void sdl_touch_controls_cancel_top_panel_press(void)
{
    sdl_touch_top_panel_cancel_press();
}

void sdl_touch_controls_cancel_zone_press(void)
{
    sdl_touch_zone_cancel_press();
}
