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
#define ANGBAND_NO_IO_COMPAT
#include "fs/io_sdl.h"
#undef ANGBAND_NO_IO_COMPAT
#include "fs/resource.h"
#include "sdl-main-internal.h"

struct sdl_config config;
bool g_hide_left_panel = false;
struct pane_config pane_config[MAX_PANE_CONFIGS];
int pane_config_count = 0;
SDL_Rect g_pane_rects[PANE_MAX];

static int g_auto_aux_main_cell_h_override = 0;

static bool sdl_rect_has_area(const SDL_Rect* rect)
{
    return rect && rect->w > 0 && rect->h > 0;
}

static SDL_Rect sdl_get_window_pixel_rect(void)
{
    SDL_Rect rect = { 0, 0, 0, 0 };

    if (g_state.window)
        SDL_GetWindowSizeInPixels(g_state.window, &rect.w, &rect.h);

    return rect;
}

static SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect)
{
    SDL_Rect pixel_rect = { 0, 0, 0, 0 };
    SDL_Rect window_units = { 0, 0, 0, 0 };
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();
    double scale_x;
    double scale_y;

    if (!rect || !g_state.window || !sdl_rect_has_area(&window_pixels))
        return pixel_rect;

    SDL_GetWindowSize(g_state.window, &window_units.w, &window_units.h);
    if (window_units.w <= 0 || window_units.h <= 0)
        return window_pixels;

    scale_x = (double)window_pixels.w / (double)window_units.w;
    scale_y = (double)window_pixels.h / (double)window_units.h;
    pixel_rect.x = (int)((double)rect->x * scale_x + 0.5);
    pixel_rect.y = (int)((double)rect->y * scale_y + 0.5);
    pixel_rect.w = (int)((double)rect->w * scale_x + 0.5);
    pixel_rect.h = (int)((double)rect->h * scale_y + 0.5);
    return pixel_rect;
}

void sdl_refresh_safe_area(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();
    SDL_Rect safe_area = window_pixels;

    if (!g_state.window || !sdl_rect_has_area(&window_pixels)) {
        g_state.safe_area = window_pixels;
        return;
    }

    {
        SDL_Rect window_units = { 0, 0, 0, 0 };
        SDL_Rect safe_units = { 0, 0, 0, 0 };

        SDL_GetWindowSize(g_state.window, &window_units.w, &window_units.h);
        if (window_units.w > 0 && window_units.h > 0
            && SDL_GetWindowSafeArea(g_state.window, &safe_units)
            && safe_units.x >= 0 && safe_units.y >= 0
            && safe_units.w > 0 && safe_units.h > 0
            && safe_units.x + safe_units.w <= window_units.w
            && safe_units.y + safe_units.h <= window_units.h)
        {
            safe_area = sdl_window_rect_to_pixel_rect(&safe_units);
            if (!sdl_rect_has_area(&safe_area))
                safe_area = window_pixels;
        }
    }

    if (SDL_memcmp(&g_state.safe_area, &safe_area, sizeof(safe_area)) != 0) {
        log_info("SDL layout safe area updated to (%d,%d %dx%d)",
            safe_area.x, safe_area.y, safe_area.w, safe_area.h);
    }

    g_state.safe_area = safe_area;
}

SDL_Rect sdl_get_layout_screen_rect(void)
{
    SDL_Rect window_pixels = sdl_get_window_pixel_rect();

    if (config.use_unsafe_area)
        return window_pixels;

    if (!sdl_rect_has_area(&g_state.safe_area))
        sdl_refresh_safe_area();

    if (sdl_rect_has_area(&g_state.safe_area)) {
        SDL_Rect safe = g_state.safe_area;

        if (safe.x < 0)
            safe.x = 0;
        if (safe.y < 0)
            safe.y = 0;
        if (safe.x > window_pixels.w)
            safe.x = window_pixels.w;
        if (safe.y > window_pixels.h)
            safe.y = window_pixels.h;
        if (safe.x + safe.w > window_pixels.w)
            safe.w = window_pixels.w - safe.x;
        if (safe.y + safe.h > window_pixels.h)
            safe.h = window_pixels.h - safe.y;

        if (sdl_rect_has_area(&safe))
            return safe;
    }

    return window_pixels;
}

static const struct pane_config default_pane_config[] = {
    { .pane = PANE_INVENTORY, .where = PLACE_RIGHT, .enabled = true },
    { .pane = PANE_WORN, .where = PLACE_RIGHT, .enabled = true },
    { .pane = PANE_INFO, .where = PLACE_RIGHT, .enabled = true, .rect.rows = 8 },
    { .pane = PANE_TOUCH, .where = PLACE_DOUBLE_RIGHT, .enabled = true },
    { .pane = PANE_ROLLS, .where = PLACE_BOTTOM, .enabled = true, .rect.rows = 4 },
    { .pane = PANE_LOG, .where = PLACE_BOTTOM, .enabled = true },
};

static const int default_pane_config_count = sizeof(default_pane_config) / sizeof(struct pane_config);

static int sdl_min_terminal_cols_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 50 : 80;
}

static int sdl_min_terminal_rows_for_mode(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? 18 : 24;
}

static int sdl_build_active_pane_config(struct pane_config* active, bool include_side,
    bool include_bottom)
{
    int active_count = 0;

    for (int i = 0; i < pane_config_count && active_count < MAX_PANE_CONFIGS; i++) {
        enum pane_placement where = pane_config[i].where;

        if (pane_placement_is_side(where) && !include_side)
            continue;
        if (where == PLACE_BOTTOM && !include_bottom)
            continue;

        active[active_count++] = pane_config[i];
    }

    return active_count;
}

static int sdl_touch_pane_target_width_px(int pane_height_px)
{
    const int numerator = 40 * SDL_TOUCH_PANE_BUTTON_COLS;
    const int denominator = 39 * SDL_TOUCH_PANE_BUTTON_ROWS
        + SDL_TOUCH_PANE_BUTTON_COLS;

    if (pane_height_px <= 0)
        return 0;

    return (pane_height_px * numerator + denominator - 1) / denominator;
}

static void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active,
    int active_count, const SDL_Rect* screen, const int* cell_widths,
    const int* cell_heights, int margin_px)
{
    SDL_Rect temp_panes[PANE_MAX] = { 0 };
    int touch_idx = -1;
    int min_touch_cols;
    int min_touch_width_px;
    int desired_touch_px;
    int desired_touch_cols;
    int max_touch_px;
    int max_touch_cols;
    int min_main_width_px;

    if (!active || active_count <= 0 || !screen || !cell_widths || !cell_heights)
        return;

    for (int i = 0; i < active_count; i++) {
        if (!active[i].enabled)
            continue;
        if (active[i].pane != PANE_TOUCH)
            continue;
        if (!pane_placement_is_side(active[i].where))
            continue;
        if (active[i].rect.cols > 0)
            continue;

        touch_idx = i;
        break;
    }

    if (touch_idx < 0)
        return;

    place_panes(active, active_count, temp_panes, screen, cell_widths,
        cell_heights, margin_px);

    if (temp_panes[PANE_TOUCH].w <= 0 || temp_panes[PANE_TOUCH].h <= 0)
        return;

    min_touch_cols = pane_primary_min_cells(PANE_TOUCH, active[touch_idx].where);
    min_touch_width_px = min_touch_cols * cell_widths[PANE_TOUCH] + margin_px;
    desired_touch_px = sdl_touch_pane_target_width_px(temp_panes[PANE_TOUCH].h);
    if (desired_touch_px < min_touch_width_px)
        desired_touch_px = min_touch_width_px;

    desired_touch_cols = (desired_touch_px > margin_px)
        ? ((desired_touch_px - margin_px + cell_widths[PANE_TOUCH] - 1)
            / cell_widths[PANE_TOUCH])
        : min_touch_cols;
    if (desired_touch_cols < min_touch_cols)
        desired_touch_cols = min_touch_cols;

    min_main_width_px = platform_current_min_terminal_cols() * cell_widths[PANE_MAIN];
    max_touch_px = temp_panes[PANE_MAIN].w + temp_panes[PANE_TOUCH].w
        - min_main_width_px;

    if (max_touch_px >= min_touch_width_px) {
        max_touch_cols = (max_touch_px > margin_px)
            ? ((max_touch_px - margin_px) / cell_widths[PANE_TOUCH])
            : 0;
        if (max_touch_cols < min_touch_cols)
            max_touch_cols = min_touch_cols;
        if (desired_touch_cols > max_touch_cols)
            desired_touch_cols = max_touch_cols;
    }

    active[touch_idx].rect.cols = desired_touch_cols;
}

static void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes,
    bool include_side, bool include_bottom)
{
    struct pane_config active[MAX_PANE_CONFIGS] = { 0 };
    int active_count;
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };
    int margin_px;

    if (!screen || !panes)
        return;

    memset(panes, 0, sizeof(SDL_Rect) * PANE_MAX);

    margin_px = (int)(g_state.system_scale * config.margin);
    active_count = sdl_build_active_pane_config(active, include_side, include_bottom);
    sdl_build_supporting_pane_metrics(active, active_count, cell_widths, cell_heights);
    sdl_apply_dynamic_auto_pane_sizes(active, active_count, screen, cell_widths,
        cell_heights, margin_px);

    place_panes(active, active_count, panes, screen, cell_widths, cell_heights,
        margin_px);
}

static int sdl_pane_current_size(int index, bool want_rows)
{
    enum pane_type type;
    enum pane_placement where;
    int configured;

    if (index < 0 || index >= pane_config_count)
        return 0;

    type = pane_config[index].pane;
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return 0;

    if (g_views[type].ready && g_pane_rects[type].w > 0
        && g_pane_rects[type].h > 0) {
        int live = want_rows ? g_views[type].rows : g_views[type].cols;
        if (live > 0)
            return live;
    }

    configured = want_rows ? pane_config[index].rect.rows : pane_config[index].rect.cols;
    if (configured > 0)
        return configured;

    where = pane_config[index].where;
    if (want_rows) {
        return pane_placement_is_side(where)
            ? pane_secondary_min_cells(type, where)
            : pane_primary_min_cells(type, where);
    }

    return pane_placement_is_side(where)
        ? pane_primary_min_cells(type, where)
        : pane_secondary_min_cells(type, where);
}

void sdl_copy_default_pane_config(void)
{
    pane_config_count = default_pane_config_count;
    for (int i = 0; i < default_pane_config_count && i < MAX_PANE_CONFIGS; i++)
        pane_config[i] = default_pane_config[i];
}

bool sdl_min_terminal_mode_is_valid(int mode)
{
    return (mode == SDL_MIN_TERMINAL_NORMAL || mode == SDL_MIN_TERMINAL_COMPACT);
}

int platform_current_min_terminal_cols(void)
{
    return sdl_min_terminal_cols_for_mode(config.min_terminal_mode);
}

int platform_current_min_terminal_rows(void)
{
    return sdl_min_terminal_rows_for_mode(config.min_terminal_mode);
}

const char* sdl_min_terminal_mode_name(int mode)
{
    return (mode == SDL_MIN_TERMINAL_COMPACT) ? "compact" : "normal";
}

#if defined(__ANDROID__) || defined(SIL_IOS)
void sdl_ensure_default_pane_configs_present(bool enable_new_panes)
{
    for (int i = 0; i < default_pane_config_count; i++) {
        bool found = false;

        if (default_pane_config[i].pane == PANE_TOUCH)
            continue;

        for (int j = 0; j < pane_config_count; j++) {
            if (pane_config[j].pane == default_pane_config[i].pane) {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        if (pane_config_count >= MAX_PANE_CONFIGS) {
            log_warn("Could not append pane %d; max pane count reached",
                default_pane_config[i].pane);
            return;
        }

        pane_config[pane_config_count] = default_pane_config[i];
        pane_config[pane_config_count].enabled = enable_new_panes;
        pane_config_count++;
    }
}
#endif

void sdl_ensure_touch_pane_config_present(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return;
    }

    if (pane_config_count >= MAX_PANE_CONFIGS) {
        log_warn("Could not append touch pane config; max pane count reached");
        return;
    }

    pane_config[pane_config_count++] = (struct pane_config){
        .pane = PANE_TOUCH,
        .where = PLACE_DOUBLE_RIGHT,
        .enabled = true,
        .rect = { .rows = 0, .cols = 0 },
        .ratio = 0.0f,
    };
}

bool sdl_touch_pane_is_config_enabled(void)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_TOUCH)
            return pane_config[i].enabled;
    }
    return false;
}

void sdl_update_cursor_visibility(void)
{
    bool show_cursor = true;

    if (config.fullscreen)
        show_cursor = (g_pane_rects[PANE_TOUCH].w > 0 && sdl_touch_pane_is_config_enabled());

    if (show_cursor)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

int sdl_auto_aux_view_font_size(void)
{
    float system_scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;
    int main_cell_h_px = config.main_view_scale * TILE_SIZE;
    int main_font_size;
    int size;

    if (g_auto_aux_main_cell_h_override > 0)
        main_cell_h_px = g_auto_aux_main_cell_h_override;
    else if (g_views[0].ready && g_views[0].cell_h > 0)
        main_cell_h_px = g_views[0].cell_h;

    main_font_size = (int)((float)main_cell_h_px / system_scale + 0.5f);
    size = (main_font_size * 3 + 3) / 4;

    if (size >= main_font_size && main_font_size > 8)
        size = main_font_size - 1;
    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

int sdl_resolve_aux_view_font_size(int requested_size)
{
    int size = requested_size;

    if (size <= 0)
        size = sdl_auto_aux_view_font_size();
    if (size < 8)
        size = 8;
    if (size > 48)
        size = 48;

    return size;
}

int sdl_auto_menu_panel_font_size(void)
{
    float system_scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;
    int canvas_h = 0;
    int size = 34;

    if (g_views[0].ready && g_views[0].rows > 0 && g_views[0].cell_h > 0)
        canvas_h = g_views[0].rows * g_views[0].cell_h;

    if (canvas_h > 0 && canvas_h < (int)(420.0f * system_scale + 0.5f))
        size = 24;
    else if (canvas_h > 0 && canvas_h < (int)(540.0f * system_scale + 0.5f))
        size = 26;
    else if (canvas_h > 0 && canvas_h < (int)(700.0f * system_scale + 0.5f))
        size = 30;

    if (size < 8)
        size = 8;
    if (size > 64)
        size = 64;

    return size;
}

int sdl_resolve_menu_panel_font_size(int requested_size)
{
    int size = requested_size;

    if (size <= 0 && config.aux_view_font_size > 0)
        size = config.aux_view_font_size;
    if (size <= 0)
        size = sdl_auto_menu_panel_font_size();
    if (size < 8)
        size = 8;
    if (size > 64)
        size = 64;

    return size;
}

static int sdl_resolve_menu_style_font_size(int requested_size)
{
    int size = requested_size;

    if (size <= 0)
        size = sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
    if (size < 8)
        size = 8;
    if (size > 64)
        size = 64;

    return size;
}

int sdl_effective_menu_font_size_for_panel_style(u16b panel_style)
{
    switch (panel_style)
    {
    case APP_UI_PANEL_STYLE_PLAIN:
        return sdl_resolve_menu_style_font_size(config.plain_menu_font_size);

    case APP_UI_PANEL_STYLE_BROWSER:
        return sdl_resolve_menu_style_font_size(config.browser_menu_font_size);

    case APP_UI_PANEL_STYLE_CHARACTER_SHEET:
        return sdl_resolve_menu_style_font_size(
            config.character_sheet_font_size);

    default:
        return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
    }
}

int sdl_effective_pane_font_size_for_config(const struct pane_config* pc)
{
    if (pc && pc->font_size > 0)
        return sdl_resolve_aux_view_font_size(pc->font_size);

    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

int sdl_effective_pane_font_size_for_type(enum pane_type type)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == type)
            return sdl_effective_pane_font_size_for_config(&pane_config[i]);
    }

    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights)
{
    int default_font_size = sdl_resolve_aux_view_font_size(config.aux_view_font_size);
    int default_cell_h = (int)(g_state.system_scale * default_font_size);
    int default_cell_w;

    if (!cell_widths || !cell_heights)
        return;

    if (default_cell_h < 1)
        default_cell_h = 1;
    default_cell_w = default_cell_h / 2;
    if (default_cell_w < 1)
        default_cell_w = 1;

    for (int i = 0; i < PANE_MAX; i++) {
        cell_widths[i] = default_cell_w;
        cell_heights[i] = default_cell_h;
    }

    cell_widths[PANE_MAIN] = config.main_view_scale * TILE_SIZE / 2;
    cell_heights[PANE_MAIN] = config.main_view_scale * TILE_SIZE;

    for (int i = 0; i < count; i++) {
        enum pane_type type = configs[i].pane;
        int font_size;
        int cell_h;

        if (type <= PANE_MAIN || type >= PANE_MAX)
            continue;

        font_size = sdl_effective_pane_font_size_for_config(&configs[i]);
        cell_h = (int)(g_state.system_scale * font_size);
        if (cell_h < 1)
            cell_h = 1;
        cell_heights[type] = cell_h;
        cell_widths[type] = cell_h / 2;
        if (cell_widths[type] < 1)
            cell_widths[type] = 1;
    }
}

void sdl_compute_split_panes(const SDL_Rect* screen, SDL_Rect* panes)
{
    sdl_place_active_panes(screen, panes, config.enable_right_panes,
        config.enable_bottom_panes);
}

int sdl_max_scale_for_rect(const SDL_Rect* rect)
{
    int min_cols;
    int min_rows;
    int max_scale_w;
    int max_scale_h;
    int max_scale;

    if (!rect)
        return 1;

    min_cols = platform_current_min_terminal_cols();
    min_rows = platform_current_min_terminal_rows();
    max_scale_w = (rect->w / min_cols) * 2 / TILE_SIZE;
    max_scale_h = rect->h / min_rows / TILE_SIZE;
    max_scale = (max_scale_w < max_scale_h) ? max_scale_w : max_scale_h;

    if (max_scale < 1)
        max_scale = 1;
    if (max_scale > 20)
        max_scale = 20;

    return max_scale;
}

void resize(const SDL_Rect* screen)
{
    SDL_Rect panes[PANE_MAX] = { 0 };
    bool include_side = config.enable_right_panes;
    bool include_bottom = config.enable_bottom_panes;

    {
        int cell_w = config.main_view_scale * TILE_SIZE / 2;
        int cell_h = config.main_view_scale * TILE_SIZE;
        int min_main_cols = platform_current_min_terminal_cols();
        int min_main_rows = platform_current_min_terminal_rows();
        int cols;
        int rows;

        if (!include_side)
            log_info("side panes disabled by user setting");
        if (!include_bottom)
            log_info("bottom panes disabled by user setting");

        sdl_place_active_panes(screen, panes, include_side, include_bottom);

        for (;;) {
            cols = panes[PANE_MAIN].w / cell_w;
            rows = panes[PANE_MAIN].h / cell_h;
            log_debug("Main view: %dx%d pixels at (%d,%d) = %dx%d cells (minimum required: %dx%d %s)",
                panes[PANE_MAIN].w, panes[PANE_MAIN].h,
                panes[PANE_MAIN].x, panes[PANE_MAIN].y,
                cols, rows,
                min_main_cols, min_main_rows,
                sdl_min_terminal_mode_name(config.min_terminal_mode));

            if (include_side && cols < min_main_cols) {
                log_warn("main view too small, %d cols < %d; removing side panes",
                    cols, min_main_cols);
                include_side = false;
                sdl_place_active_panes(screen, panes, include_side, include_bottom);
                continue;
            }

            if (include_bottom && rows < min_main_rows) {
                log_warn("main view too small, %d rows < %d; removing bottom panes",
                    rows, min_main_rows);
                include_bottom = false;
                sdl_place_active_panes(screen, panes, include_side, include_bottom);
                continue;
            }

            break;
        }
    }

    for (int i = 0; i < PANE_MAX; i++) {
        const SDL_Rect* r = &panes[i];
        log_debug("pane %d is at (%d, %d) size %dx%d", i, r->x, r->y, r->w, r->h);
    }

    memcpy(g_pane_rects, panes, sizeof(g_pane_rects));

    char font_path[1024];

    if (!resource_resolve_xtra_path(font_path, sizeof(font_path),
            config.monospace_font[0] ? config.monospace_font : NULL,
            "font/VictorMono-Medium.ttf"))
    {
        quit("could not resolve monospace font path");
    }

    for (int i = 1; i < MAX_TERM_DATA; i++) {
        sdl_view_destroy(&g_views[i]);
        if (panes[i].w) {
            sdl_view_create(&g_views[i], panes[i], font_path,
                sdl_effective_pane_font_size_for_type((enum pane_type)i), 0,
                config.margin);
        }
    }

    sdl_view_destroy(&g_views[0]);
    sdl_view_create(&g_views[0], panes[PANE_MAIN], font_path, 0,
        config.main_view_scale, config.margin);

    sdl_set_active_view_index(0);
    sdl_scene_stack_on_layout_changed();

    if (character_dungeon && p_ptr) {
        p_ptr->update |= PU_PANEL;
        p_ptr->redraw |= PR_MAP;
    }

    g_state.need_present = true;
    sdl_update_cursor_visibility();
}

void platform_config_info(char* buf, size_t size)
{
    size_t offset = 0;
    int support_count = 0;

    offset += (size_t)strnfmt(buf + offset, size - offset, "=== SDL Settings ===\n");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Main View Scale: %d\n", config.main_view_scale);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Minimum Terminal Size: %s (%dx%d)\n",
        sdl_min_terminal_mode_name(config.min_terminal_mode),
        platform_current_min_terminal_cols(), platform_current_min_terminal_rows());
    if (config.aux_view_font_size > 0) {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Default Aux View Font Size: %d\n", config.aux_view_font_size);
    } else {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Default Aux View Font Size: auto (%d)\n", sdl_auto_aux_view_font_size());
    }
    if (config.menu_panel_font_size > 0) {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Menu + Left Panel Font Size: %d\n", config.menu_panel_font_size);
    } else {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Menu + Left Panel Font Size: auto (%d)\n",
            sdl_resolve_menu_panel_font_size(config.menu_panel_font_size));
    }
    if (config.plain_menu_font_size > 0) {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Plain Menu Font Size: %d\n", config.plain_menu_font_size);
    } else {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Plain Menu Font Size: auto (%d)\n",
            sdl_resolve_menu_style_font_size(config.plain_menu_font_size));
    }
    if (config.browser_menu_font_size > 0) {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Browser Menu Font Size: %d\n", config.browser_menu_font_size);
    } else {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Browser Menu Font Size: auto (%d)\n",
            sdl_resolve_menu_style_font_size(config.browser_menu_font_size));
    }
    if (config.character_sheet_font_size > 0) {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Character Sheet Font Size: %d\n",
            config.character_sheet_font_size);
    } else {
        offset += (size_t)strnfmt(buf + offset, size - offset,
            "Character Sheet Font Size: auto (%d)\n",
            sdl_resolve_menu_style_font_size(
                config.character_sheet_font_size));
    }
    offset += (size_t)strnfmt(buf + offset, size - offset, "Margin: %d\n", config.margin);
    offset += (size_t)strnfmt(buf + offset, size - offset, "Fullscreen: %s\n", config.fullscreen ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Tiles: %s\n", config.tiles ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset,
        "Use Unsafe Area: %s\n", config.use_unsafe_area ? "Yes" : "No");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Pane Borders: %s\n",
        config.show_pane_borders ? "White" : "Black");
    offset += (size_t)strnfmt(buf + offset, size - offset, "Hide Left Panel: %s\n\n",
        config.hide_left_panel ? "Yes" : "No");

    offset += (size_t)strnfmt(buf + offset, size - offset, "=== Pane Configuration (Supporting Panes) ===\n");
    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        if (pane_config[i].pane != PANE_MAIN)
            support_count++;
    }
    offset += (size_t)strnfmt(buf + offset, size - offset, "Supporting Panes: %d\n\n", support_count);

    for (int i = 0; i < pane_config_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pc = &pane_config[i];
        const char* type_str = "UNKNOWN";
        const char* where_str;

        if (pc->pane == PANE_MAIN)
            continue;

        where_str = pane_placement_name(pc->where);
        switch (pc->pane) {
        case PANE_MAIN: type_str = "MAIN"; break;
        case PANE_INVENTORY: type_str = "INVENTORY"; break;
        case PANE_WORN: type_str = "WORN"; break;
        case PANE_ROLLS: type_str = "ROLLS"; break;
        case PANE_INFO: type_str = "INFO"; break;
        case PANE_CHARACTER: type_str = "CHARACTER"; break;
        case PANE_LOG: type_str = "LOG"; break;
        case PANE_MONSTERS: type_str = "MONSTERS"; break;
        case PANE_TOUCH: type_str = "TOUCH"; break;
        default: break;
        }

        offset += (size_t)strnfmt(buf + offset, size - offset, "Pane %d: %s\n", i + 1, type_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Placement: %s\n", where_str);
        offset += (size_t)strnfmt(buf + offset, size - offset, "  Enabled: %s\n", pc->enabled ? "yes" : "no");
        if (pc->rect.rows > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Rows: %d\n", pc->rect.rows);
        if (pc->rect.cols > 0)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Cols: %d\n", pc->rect.cols);
        if (pc->font_size > 0) {
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Font Size: %d\n", pc->font_size);
        } else {
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Font Size: auto (%d)\n",
                sdl_effective_pane_font_size_for_config(pc));
        }
        if (pc->ratio > 0.0f)
            offset += (size_t)strnfmt(buf + offset, size - offset, "  Ratio: %.2f\n", pc->ratio);
        offset += (size_t)strnfmt(buf + offset, size - offset, "\n");
    }

    offset += (size_t)strnfmt(buf + offset, size - offset, "\nConfiguration file: %s\n", config_file_path);
}

bool save_pane_config_to_json(void)
{
    sdl_config_save(config_file_path, &config, pane_config, pane_config_count);
    log_info("Pane configuration saved to: %s", config_file_path);
    return true;
}

cptr platform_config_path(void)
{
    return config_file_path;
}

void platform_load_app_options(void)
{
    sdl_config_load_app_options(platform_config_path());
}

int platform_main_view_scale(void)
{
    return config.main_view_scale;
}

int platform_min_terminal_mode(void)
{
    return config.min_terminal_mode;
}

void platform_set_min_terminal_mode(int value)
{
    if (!sdl_min_terminal_mode_is_valid(value))
        return;

    config.min_terminal_mode = value;

    if (config.main_view_scale > platform_max_scale())
        config.main_view_scale = platform_max_scale();
}

void platform_set_main_view_scale(int value)
{
    int max_scale = platform_max_scale();
    if (value > 0 && value <= max_scale)
        config.main_view_scale = value;
}

int platform_aux_view_font_size(void)
{
    return config.aux_view_font_size;
}

int platform_effective_aux_view_font_size(void)
{
    return sdl_resolve_aux_view_font_size(config.aux_view_font_size);
}

void platform_set_aux_view_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 48))
        config.aux_view_font_size = value;
}

int platform_menu_panel_font_size(void)
{
    return config.menu_panel_font_size;
}

int platform_effective_menu_panel_font_size(void)
{
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

void platform_set_menu_panel_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 64))
        config.menu_panel_font_size = value;
}

int platform_plain_menu_font_size(void)
{
    return config.plain_menu_font_size;
}

int platform_effective_plain_menu_font_size(void)
{
    return sdl_resolve_menu_style_font_size(config.plain_menu_font_size);
}

void platform_set_plain_menu_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 64))
        config.plain_menu_font_size = value;
}

int platform_browser_menu_font_size(void)
{
    return config.browser_menu_font_size;
}

int platform_effective_browser_menu_font_size(void)
{
    return sdl_resolve_menu_style_font_size(config.browser_menu_font_size);
}

void platform_set_browser_menu_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 64))
        config.browser_menu_font_size = value;
}

int platform_character_sheet_font_size(void)
{
    return config.character_sheet_font_size;
}

int platform_effective_character_sheet_font_size(void)
{
    return sdl_resolve_menu_style_font_size(config.character_sheet_font_size);
}

void platform_set_character_sheet_font_size(int value)
{
    if (value == 0 || (value >= 8 && value <= 64))
        config.character_sheet_font_size = value;
}

int platform_margin(void)
{
    return config.margin;
}

void platform_set_margin(int value)
{
    if (value >= 0 && value <= 20)
        config.margin = value;
}

bool platform_fullscreen(void)
{
    return config.fullscreen;
}

void platform_set_fullscreen(bool value)
{
    if (config.fullscreen == value)
        return;

    config.fullscreen = value;

    if (g_state.window) {
        if (value) {
            SDL_GetWindowPosition(g_state.window, &config.window_x, &config.window_y);
            SDL_GetWindowSize(g_state.window, &config.window_width, &config.window_height);
            log_debug("Saving windowed position (%d, %d) and size (%dx%d) before fullscreen",
                config.window_x, config.window_y, config.window_width, config.window_height);

            if (!SDL_SetWindowFullscreen(g_state.window, true)) {
                log_error("Failed to enter fullscreen: %s", SDL_GetError());
                config.fullscreen = false;
                return;
            }
            log_info("Entered fullscreen mode");
        } else {
            if (!SDL_SetWindowFullscreen(g_state.window, false)) {
                log_error("Failed to exit fullscreen: %s", SDL_GetError());
                config.fullscreen = true;
                return;
            }

            if (config.window_width > 0 && config.window_height > 0) {
                SDL_SetWindowSize(g_state.window, config.window_width, config.window_height);
                if (config.window_x >= 0 && config.window_y >= 0)
                    SDL_SetWindowPosition(g_state.window, config.window_x, config.window_y);
                log_debug("Restored windowed position (%d, %d) and size (%dx%d)",
                    config.window_x, config.window_y, config.window_width, config.window_height);
            }
            log_info("Exited fullscreen mode");
        }

        SDL_Rect window;
        sdl_refresh_safe_area();
        window = sdl_get_layout_screen_rect();
        sdl_load_story_fonts();
        resize(&window);
        sdl_update_cursor_visibility();
        g_state.need_present = true;
        sdl_redraw_all_views();
    }
}

bool platform_tiles(void)
{
    return config.tiles;
}

bool platform_use_unsafe_area(void)
{
    return config.use_unsafe_area;
}

void platform_set_use_unsafe_area(bool value)
{
    if (config.use_unsafe_area == value)
        return;

    config.use_unsafe_area = value;
    platform_apply_config();
}

static void sdl_request_tiles_mode_refresh(void)
{
    if (!p_ptr)
        return;

    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->window |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT
        | PW_MONLIST);
}

static bool sdl_apply_live_tiles_mode(bool value)
{
    if (!g_state.renderer)
        return true;

    if (g_state.tileset) {
        SDL_DestroyTexture(g_state.tileset);
        g_state.tileset = NULL;
    }

    g_state.use_tiles = value;
    g_state.tileset_cols = 0;

    if (value) {
        char tileset_path[1024];
        SDL_Surface* ts;

        if (!resource_build_default_tileset_path(tileset_path,
                sizeof(tileset_path)))
        {
            log_error("Failed to resolve tileset path while enabling tiles");
            g_state.use_tiles = false;
            return false;
        }

        {
            ang_file* stream = sdl_fopen(tileset_path, "rb");
            if (stream)
                ts = IMG_Load_IO(stream, true);
            else
                ts = NULL;
        }
        int tileset_width;

        if (!ts) {
            log_error("Failed to load tileset PNG while enabling tiles: %s",
                SDL_GetError());
            g_state.use_tiles = false;
            return false;
        }

        tileset_width = ts->w;
        g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
        SDL_DestroySurface(ts);
        if (!g_state.tileset) {
            log_error("Failed to create tileset texture while enabling tiles: %s",
                SDL_GetError());
            g_state.use_tiles = false;
            return false;
        }

        SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
        g_state.tileset_cols = tileset_width / TILE_SIZE;
    }

    ANGBAND_GRAF = value ? "new" : "old";
    runtime_cli_set_graphics_mode(value ? GRAPHICS_MICROCHASM
        : GRAPHICS_PSEUDO);
    use_graphics = value ? GRAPHICS_MICROCHASM : GRAPHICS_PSEUDO;
    use_bigtile = value;
    sdl_sync_palette();
    reset_visuals(true);
    sdl_request_tiles_mode_refresh();

    return true;
}

void platform_set_tiles(bool value)
{
    if (config.tiles == value)
        return;

    config.tiles = value;

    if (!sdl_apply_live_tiles_mode(value)) {
        config.tiles = false;
        (void)sdl_apply_live_tiles_mode(false);
        log_warn("Falling back to ASCII mode because the tileset could not be loaded");
        return;
    }

    log_info("Tiles mode %s", value ? "enabled" : "disabled");
}

int get_pane_config_count(void)
{
    return pane_config_count;
}

int platform_pane_type(int index)
{
    if (index < 0 || index >= pane_config_count)
        return -1;
    return (int)pane_config[index].pane;
}

int platform_pane_where(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return (int)pane_config[index].where;
}

void platform_set_pane_where(int index, int where)
{
    enum pane_placement placement = (enum pane_placement)where;

    if (index < 0 || index >= pane_config_count)
        return;
    if (!pane_type_allows_placement(pane_config[index].pane, placement))
        placement = pane_first_allowed_placement(pane_config[index].pane);

    pane_config[index].where = placement;
}

bool platform_pane_enabled(int index)
{
    if (index < 0 || index >= pane_config_count)
        return false;
    return pane_config[index].enabled;
}

int platform_pane_rows(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].rect.rows;
}

int platform_pane_cols(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].rect.cols;
}

int platform_pane_font_size(int index)
{
    if (index < 0 || index >= pane_config_count)
        return 0;
    return pane_config[index].font_size;
}

int platform_pane_effective_font_size(int index)
{
    if (index < 0 || index >= pane_config_count)
        return sdl_resolve_aux_view_font_size(config.aux_view_font_size);

    return sdl_effective_pane_font_size_for_config(&pane_config[index]);
}

int platform_pane_current_rows(int index)
{
    return sdl_pane_current_size(index, true);
}

int platform_pane_current_cols(int index)
{
    return sdl_pane_current_size(index, false);
}

void platform_set_pane_rows(int index, int rows)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (rows < 0)
        rows = 0;
    if (rows > 200)
        rows = 200;
    pane_config[index].rect.rows = rows;
}

void platform_set_pane_cols(int index, int cols)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (cols < 0)
        cols = 0;
    if (cols > 200)
        cols = 200;
    pane_config[index].rect.cols = cols;
}

void platform_set_pane_font_size(int index, int font_size)
{
    if (index < 0 || index >= pane_config_count)
        return;
    if (font_size < 0)
        font_size = 0;
    if (font_size > 0 && font_size < 8)
        font_size = 8;
    if (font_size > 48)
        font_size = 48;
    pane_config[index].font_size = font_size;
}

void platform_set_pane_enabled(int index, bool enabled)
{
    if (index < 0 || index >= pane_config_count)
        return;
    pane_config[index].enabled = enabled;
}

bool platform_enable_right_panes(void)
{
    return config.enable_right_panes;
}

void platform_set_enable_right_panes(bool value)
{
    config.enable_right_panes = value;

    if (value)
    {
        for (int i = 0; i < pane_config_count; i++)
        {
            if (pane_placement_is_side(pane_config[i].where))
                pane_config[i].enabled = true;
        }
    }
}

bool platform_enable_bottom_panes(void)
{
    return config.enable_bottom_panes;
}

void platform_set_enable_bottom_panes(bool value)
{
    config.enable_bottom_panes = value;

    if (value)
    {
        for (int i = 0; i < pane_config_count; i++)
        {
            if (pane_config[i].where == PLACE_BOTTOM)
                pane_config[i].enabled = true;
        }
    }
}

bool platform_show_pane_borders(void)
{
    return config.show_pane_borders;
}

void platform_set_show_pane_borders(bool value)
{
    config.show_pane_borders = value;
}

bool platform_hide_left_panel(void)
{
    return g_hide_left_panel;
}

bool ui_left_panel_hidden(void)
{
    return g_hide_left_panel;
}

void platform_set_hide_left_panel(bool value)
{
    g_hide_left_panel = value;
    config.hide_left_panel = value;
}

int platform_intro_style(void)
{
    if (!op_ptr)
        return 0;
    return (op_ptr->intro_style == INTRO_STYLE_RANDOM)
        ? -1
        : (int)op_ptr->intro_style;
}

void platform_set_intro_style(int style)
{
    if (!op_ptr)
        return;
    op_ptr->intro_style = (style == -1)
        ? INTRO_STYLE_RANDOM
        : (byte)(style < 0 ? 0 : style > (INTRO_STYLE_RANDOM - 1) ? (INTRO_STYLE_RANDOM - 1) : style);
}

int platform_max_scale(void)
{
    int w;
    int h;
    SDL_Rect screen;
    SDL_Rect panes[PANE_MAX];
    int max_scale;

    if (!g_state.window)
        return 10;

    SDL_GetWindowSizeInPixels(g_state.window, &w, &h);
    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    sdl_compute_split_panes(&screen, panes);
    max_scale = sdl_max_scale_for_rect(&panes[PANE_MAIN]);

    log_debug("platform_max_scale: window=%dx%d main=%dx%d min=%dx%d (%s) max_scale=%d",
        w, h, panes[PANE_MAIN].w, panes[PANE_MAIN].h,
        platform_current_min_terminal_cols(), platform_current_min_terminal_rows(),
        sdl_min_terminal_mode_name(config.min_terminal_mode), max_scale);

    return max_scale;
}

void platform_apply_config(void)
{
    if (!g_state.window) {
        log_warn("platform_apply_config: no window, skipping");
        return;
    }

    {
        int max_scale = platform_max_scale();
        if (config.main_view_scale > max_scale) {
            log_info("Clamping main_view_scale from %d to %d for current pane layout",
                config.main_view_scale, max_scale);
            config.main_view_scale = max_scale;
        }
    }

    sdl_refresh_safe_area();
    SDL_Rect screen = sdl_get_layout_screen_rect();
    g_auto_aux_main_cell_h_override = config.main_view_scale * TILE_SIZE;
    sdl_load_story_fonts();
    resize(&screen);
    g_auto_aux_main_cell_h_override = 0;
    sdl_redraw_all_views();
}
