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

#ifndef INCLUDED_SDL_MAIN_INTERNAL_H
#define INCLUDED_SDL_MAIN_INTERNAL_H

#include "app/app-session.h"
#include "app/app-ui.h"
#define ANGBAND_NO_IO_COMPAT
#include "fs/io_sdl.h"
#undef ANGBAND_NO_IO_COMPAT
#include "fs/path.h"
#include "log/log.h"
#include "main.h"
#include "main-sdl.h"
#include "pane.h"
#include "runtime/runtime-dungeon.h"
#include "runtime/runtime-cli.h"
#include "sdl-config.h"
#include "platform/sdl-ui-style-internal.h"
#include "sdl-sound.h"
#include "sound-config.h"
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

enum {
    TILE_SIZE = 16,
    MAX_TERM_DATA = ANGBAND_TERM_MAX,
    MAX_STORY_FONT_CACHE = 16,
    MAX_PANE_CONFIGS = 8,
    TOUCH_PANE_LONG_PRESS_MS = 350,
    MAX_GAMEPADS = 4,
    DPAD_DIAGONAL_WINDOW_MS = 100,
    SHOULDER_COMBO_WINDOW_MS = 150,
};

typedef struct story_font_entry {
    int pixel_height;
    TTF_Font* font;
} story_font_entry;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    SDL_Color palette[16];
    SDL_Rect safe_area;
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
    story_font_entry story_fonts[MAX_STORY_FONT_CACHE];
    int story_font_count;
    int story_font_depth;
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    int ttf_font_size;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    bool ready;
} sdl_view;

#include "platform/sdl-scene-internal.h"

extern struct sdl_config config;
extern bool g_hide_left_panel;
extern struct sound_config g_sound_config;
extern char config_file_path[1024];
extern struct pane_config pane_config[MAX_PANE_CONFIGS];
extern int pane_config_count;
extern sdl_state g_state;
extern sdl_view g_views[MAX_TERM_DATA];
extern SDL_Rect g_pane_rects[PANE_MAX];

void sdl_copy_default_pane_config(void);
#if defined(__ANDROID__) || defined(SIL_IOS)
void sdl_ensure_default_pane_configs_present(bool enable_new_panes);
#endif
void sdl_ensure_touch_pane_config_present(void);
bool sdl_touch_pane_is_config_enabled(void);
bool sdl_min_terminal_mode_is_valid(int mode);
int platform_current_min_terminal_cols(void);
int platform_current_min_terminal_rows(void);
const char* sdl_min_terminal_mode_name(int mode);
int sdl_auto_aux_view_font_size(void);
int sdl_resolve_aux_view_font_size(int requested_size);
int sdl_auto_menu_panel_font_size(void);
int sdl_resolve_menu_panel_font_size(int requested_size);
int sdl_effective_menu_font_size_for_panel_style(u16b panel_style);
int sdl_effective_pane_font_size_for_config(const struct pane_config* pc);
int sdl_effective_pane_font_size_for_type(enum pane_type type);
void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights);
void sdl_compute_split_panes(const SDL_Rect* screen, SDL_Rect* panes);
void sdl_refresh_safe_area(void);
SDL_Rect sdl_get_layout_screen_rect(void);
int sdl_max_scale_for_rect(const SDL_Rect* rect);
void sdl_update_cursor_visibility(void);
void resize(const SDL_Rect* screen);
bool sdl_pane_resize_handle_event(const SDL_Event* ev);
void sdl_pane_resize_render_handles(void);

void sdl_view_destroy(sdl_view* d);
int sdl_active_view_index(void);
void sdl_set_active_view_index(int view_index);
void sdl_redraw_all_views(void);
void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
void sdl_sync_palette(void);
void sdl_present_if_needed(sdl_view* d);
void sdl_handle_renderer_reset(void);
void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
void sdl_window_set_position(int x, int y);

void sdl_handle_event(sdl_state* st, const SDL_Event* ev);
void sdl_gamepad_init(void);
void sdl_gamepad_shutdown(void);
int sdl_gamepad_count(void);
void platform_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
void platform_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen);
bool steamdeck_controls_active(void);
int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
void sdl_gamepad_apply_modifier(int binding, bool down);
bool sdl_gamepad_shift_active(void);
bool sdl_gamepad_ctrl_active(void);
bool sdl_gamepad_alt_active(void);
void sdl_gamepad_send_key(int key, bool apply_modifiers);
u16b sdl_movement_input_modifiers_from_keyboard_event(
    const SDL_KeyboardEvent* key_event);
void sdl_submit_legacy_input_byte(int key);
void sdl_drain_legacy_input_queue(void);
void sdl_clear_legacy_input_queue(void);
void sdl_gamepad_load_default_bindings(void);

bool sdl_touch_pane_handle_event(const SDL_Event* ev);
void sdl_touch_pane_render(void);
void sdl_touch_dungeon_overlay_render(int canvas_w, int canvas_h);
void sdl_touch_pane_render_reset_prompt(void);
void sdl_touch_pane_cancel_press(void);
int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_pane_flush_pending_press(Uint64 now_ns);
void sdl_touch_pane_load_default_bindings(void);
bool sdl_touch_pane_panel_is_valid(int panel);
int sdl_touch_pane_raw_binding_for_panel(int panel, int index);
void sdl_touch_pane_reset_input_state(void);

int sdl_menu_pointer_pending_timeout_ms(Uint64 now_ns);
bool sdl_menu_pointer_flush_pending_long_press(Uint64 now_ns);

bool sdl_map_pointer_handle_event(const SDL_Event* ev);
int sdl_map_pointer_pending_timeout_ms(Uint64 now_ns);
bool sdl_map_pointer_flush_pending_long_press(Uint64 now_ns);
void sdl_map_pointer_reset_input_state(void);
bool sdl_map_pointer_preview_cell(s16b map_y, s16b map_x,
    bool* out_is_target);

bool sdl_round_movement_handle_event(const SDL_Event* ev);
void sdl_round_movement_render(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot);
void sdl_round_movement_reset_input_state(void);

bool sdl_player_action_menu_handle_event(const SDL_Event* ev);
int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns);
void sdl_player_action_menu_render(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot);
void sdl_player_action_menu_reset_input_state(void);

void sdl_load_story_fonts(void);
void sdl_story_font_cache_clear(void);
TTF_Font* sdl_story_font_for_height(int pixel_height);
TTF_Font* sdl_story_font_for_view(const sdl_view* d);
int sdl_ui_scale_px(float logical_value);
int sdl_ui_font_size_logical(const sdl_view* view);
TTF_Font* sdl_ui_font_for_height(int pixel_height);
void sdl_ui_font_cache_clear(void);
int sdl_ui_measure_text(TTF_Font* font, cptr text);
int sdl_ui_text_left_padding(TTF_Font* font, int target_h);
int sdl_ui_text_pair_left_padding(TTF_Font* primary, TTF_Font* secondary,
    int target_h);
void sdl_ui_render_text(TTF_Font* font, float x_px, float y_px,
    SDL_Color color, cptr text);
const sdl_ui_style* sdl_ui_style_for_panel(u16b panel_style);
SDL_Color sdl_ui_style_color_for_attr(const sdl_ui_style* style, byte attr);
SDL_Color sdl_ui_style_accent_for_attr(const sdl_ui_style* style, byte attr);
SDL_Color sdl_ui_style_with_alpha(SDL_Color color, byte alpha);
void sdl_ui_style_draw_canvas(const sdl_ui_style* style, int canvas_w,
    int canvas_h);
void sdl_ui_style_draw_panel_frame(const sdl_ui_style* style,
    const SDL_FRect* rect, bool border);
void sdl_ui_style_draw_control_frame(const sdl_ui_style* style,
    const SDL_FRect* rect, u16b state_flags, bool active, bool focused,
    bool pressed);
void sdl_ui_style_draw_rule(const sdl_ui_style* style, const SDL_FRect* rect);

#endif /* INCLUDED_SDL_MAIN_INTERNAL_H */
