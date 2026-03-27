#ifndef INCLUDED_PLATFORM_CONFIG_H
#define INCLUDED_PLATFORM_CONFIG_H

#include "h-basic.h"
#include "pane-config.h"

void get_sdl_config_info(char* buf, size_t size);
bool save_pane_config_to_json(void);
cptr get_sdl_config_path(void);
void platform_load_app_options(void);
bool sdl_config_should_force_intro_flame(void);
void sdl_config_mark_intro_seen(void);
bool option_is_app_persistent(int opt);
int get_sdl_main_view_scale(void);
void set_sdl_main_view_scale(int value);
int get_sdl_min_terminal_mode(void);
void set_sdl_min_terminal_mode(int value);
int get_sdl_aux_view_font_size(void);
int get_sdl_effective_aux_view_font_size(void);
void set_sdl_aux_view_font_size(int value);
int get_sdl_margin(void);
void set_sdl_margin(int value);
bool get_sdl_fullscreen(void);
void set_sdl_fullscreen(bool value);
bool get_sdl_tiles(void);
void set_sdl_tiles(bool value);
int get_pane_config_count(void);
bool get_sdl_enable_right_panes(void);
void set_sdl_enable_right_panes(bool value);
bool get_sdl_enable_bottom_panes(void);
void set_sdl_enable_bottom_panes(bool value);
bool get_sdl_hide_left_panel(void);
void set_sdl_hide_left_panel(bool value);
int get_sdl_pane_type(int index);
int get_sdl_pane_where(int index);
void set_sdl_pane_where(int index, int where);
bool get_sdl_pane_enabled(int index);
int get_sdl_pane_rows(int index);
int get_sdl_pane_cols(int index);
int get_sdl_pane_font_size(int index);
int get_sdl_pane_effective_font_size(int index);
int get_sdl_pane_current_rows(int index);
int get_sdl_pane_current_cols(int index);
void set_sdl_pane_rows(int index, int rows);
void set_sdl_pane_cols(int index, int cols);
void set_sdl_pane_font_size(int index, int font_size);
void set_sdl_pane_enabled(int index, bool enabled);
int get_sdl_intro_style(void);
void set_sdl_intro_style(int style);
int get_sdl_max_scale(void);
void sdl_apply_config(void);

#endif /* INCLUDED_PLATFORM_CONFIG_H */
