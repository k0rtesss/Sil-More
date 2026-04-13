#ifndef INCLUDED_SDL_MAIN_INTERNAL_H
#define INCLUDED_SDL_MAIN_INTERNAL_H

#include "externs.h"
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
#include "runtime-cli.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "sound-config.h"
#include "z-term.h"
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
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
    story_font_entry story_fonts[MAX_STORY_FONT_CACHE];
    int story_font_count;
    int story_font_depth;
    bool story_font_grid;
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
    term t;
    bool term_ready;
} sdl_view;

typedef enum sdl_scene_animation_kind {
    SDL_SCENE_ANIMATION_NONE = 0,
    SDL_SCENE_ANIMATION_ACTOR_MOVED = 1,
    SDL_SCENE_ANIMATION_DAMAGE = 2,
    SDL_SCENE_ANIMATION_PROJECTILE = 3,
    SDL_SCENE_ANIMATION_OBJECT_TRANSFER = 4
} sdl_scene_animation_kind;

typedef struct sdl_scene_animation {
    bool active;
    u16b kind;
    Uint64 started_ns;
    Uint64 duration_ns;
    s32b subject;
    s16b from_y;
    s16b from_x;
    s16b to_y;
    s16b to_x;
    s32b arg0;
    s32b arg1;
    s32b arg2;
} sdl_scene_animation;

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
#ifdef __ANDROID__
void sdl_ensure_default_pane_configs_present(bool enable_new_panes);
#endif
void sdl_ensure_touch_pane_config_present(void);
bool sdl_touch_pane_is_config_enabled(void);
bool sdl_min_terminal_mode_is_valid(int mode);
int sdl_current_min_terminal_cols(void);
int sdl_current_min_terminal_rows(void);
const char* sdl_min_terminal_mode_name(int mode);
int sdl_auto_aux_view_font_size(void);
int sdl_resolve_aux_view_font_size(int requested_size);
int sdl_auto_menu_panel_font_size(void);
int sdl_resolve_menu_panel_font_size(int requested_size);
int sdl_effective_pane_font_size_for_config(const struct pane_config* pc);
int sdl_effective_pane_font_size_for_type(enum pane_type type);
void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights);
void sdl_compute_split_panes(const SDL_Rect* screen, SDL_Rect* panes);
int sdl_max_scale_for_rect(const SDL_Rect* rect);
void sdl_update_cursor_visibility(void);
void resize(const SDL_Rect* screen);

sdl_view* sdl_view_from_term(term* t);
void sdl_view_destroy(sdl_view* d);
errr sdl_view_link_term(sdl_view* d, int term_index);
void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
void sdl_sync_palette(void);
void sdl_present_if_needed(sdl_view* d);
void sdl_handle_renderer_reset(void);
void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
void sdl_window_set_position(int x, int y);

void sdl_handle_event(sdl_state* st, const SDL_Event* ev);
void sdl_gamepad_init(void);
void sdl_gamepad_shutdown(void);
void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen);
bool steamdeck_controls_active(void);
int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
void sdl_gamepad_apply_modifier(int binding, bool down);
bool sdl_gamepad_shift_active(void);
bool sdl_gamepad_ctrl_active(void);
bool sdl_gamepad_alt_active(void);
void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt);
void sdl_gamepad_send_key(int key, bool use_macro_mods);
void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt);
void sdl_submit_legacy_input_byte(int key);
void sdl_drain_legacy_input_queue(void);
void sdl_clear_legacy_input_queue(void);
void sdl_gamepad_load_default_bindings(void);

bool sdl_touch_pane_handle_event(const SDL_Event* ev);
void sdl_touch_pane_render(void);
void sdl_touch_pane_render_reset_prompt(void);
void sdl_touch_pane_cancel_press(void);
int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_pane_flush_pending_press(Uint64 now_ns);
void sdl_touch_pane_load_default_bindings(void);
bool sdl_touch_pane_panel_is_valid(int panel);
int sdl_touch_pane_raw_binding_for_panel(int panel, int index);
void sdl_touch_pane_reset_input_state(void);

void sdl_load_story_fonts(void);
void sdl_story_font_cache_clear(void);
void sdl_apply_story_font_state(bool active);
void sdl_apply_story_grid_state(bool grid);
void sdl_story_font_reset_state(void);
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
void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col, float max_w_px);
void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr);

void sdl_scene_stack_init(void);
void sdl_scene_stack_shutdown(void);
void sdl_scene_stack_set_enabled(bool enabled);
bool sdl_scene_stack_is_enabled(void);
void sdl_scene_stack_on_layout_changed(void);
void sdl_scene_stack_on_renderer_reset(void);
void sdl_scene_stack_prepare_frame(Uint64 now_ns);
int sdl_scene_stack_pending_timeout_ms(Uint64 now_ns);
bool sdl_scene_stack_handles_main_view(void);
bool sdl_scene_stack_render_main_layer(void);
void sdl_scene_stack_render_overlay_layer(void);
void sdl_scene_stack_clear(void);
bool sdl_scene_dungeon_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns);
bool sdl_scene_information_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_information_snapshot* snapshot);
bool sdl_scene_information_render_overlay(SDL_Texture* canvas,
    const sdl_view* main_view, const app_information_snapshot* snapshot);
bool sdl_scene_bootstrap_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_bootstrap_snapshot* snapshot);
bool sdl_scene_ui_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene);
bool sdl_scene_ui_render_overlay(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene);
bool sdl_scene_menu_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_menu_snapshot* snapshot);

#endif /* INCLUDED_SDL_MAIN_INTERNAL_H */
