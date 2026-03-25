#ifndef INCLUDED_PLATFORM_UI_H
#define INCLUDED_PLATFORM_UI_H

#include "h-basic.h"
#include "gamepad-config.h"
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

bool steamdeck_controls_active(void);
int steamdeck_back_key(void);
int steamdeck_confirm_key(void);
int steamdeck_info_key(void);
int steamdeck_alt_action_key(void);
int steamdeck_secondary_key(void);

bool get_sdl_gamepad_enabled(void);
void set_sdl_gamepad_enabled(bool value);
bool get_sdl_gamepad_auto_mode(void);
void set_sdl_gamepad_auto_mode(bool value);
bool get_sdl_steamdeck_mode(void);
void set_sdl_steamdeck_mode(bool value);
bool get_sdl_gamepad_use_dpad(void);
void set_sdl_gamepad_use_dpad(bool value);
bool get_sdl_gamepad_use_left_stick(void);
void set_sdl_gamepad_use_left_stick(bool value);
int get_sdl_gamepad_button_binding(int button);
void set_sdl_gamepad_button_binding(int button, int binding);
int get_sdl_gamepad_trigger_binding(int index);
void set_sdl_gamepad_trigger_binding(int index, int binding);
int get_sdl_gamepad_left_stick_binding(int dir);
void set_sdl_gamepad_left_stick_binding(int dir, int binding);
int get_sdl_gamepad_right_stick_binding(int dir);
void set_sdl_gamepad_right_stick_binding(int dir, int binding);
int get_sdl_gamepad_shoulder_combo_binding(void);
void set_sdl_gamepad_shoulder_combo_binding(int binding);
int get_sdl_gamepad_default_button_binding(int button);
int get_sdl_gamepad_default_trigger_binding(int index);
int get_sdl_gamepad_default_left_stick_binding(int dir);
int get_sdl_gamepad_default_right_stick_binding(int dir);
int get_sdl_gamepad_default_shoulder_combo_binding(void);
void sdl_gamepad_reset_bindings_to_default(void);
void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen);

int get_sdl_touch_pane_binding(int index);
void set_sdl_touch_pane_binding(int index, int binding);
int get_sdl_touch_pane_default_binding(int index);
int get_sdl_touch_pane_binding_for_panel(int panel, int index);
void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding);
int get_sdl_touch_pane_default_binding_for_panel(int panel, int index);
void sdl_touch_pane_reset_bindings_to_default(void);
cptr get_sdl_touch_pane_slot_name(int index);
void get_sdl_touch_pane_button_label(int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label(int index, cptr label);
void clear_sdl_touch_pane_button_label(int index);
void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label);
void clear_sdl_touch_pane_button_label_for_panel(int panel, int index);
void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen);
void set_sdl_touch_pane_panel_name(int panel, cptr name);

#define GAMEPAD_CAPTURE_BUTTON 0
#define GAMEPAD_CAPTURE_TRIGGER 1
#define GAMEPAD_CAPTURE_LEFT_STICK 2
#define GAMEPAD_CAPTURE_RIGHT_STICK 3
#define GAMEPAD_CAPTURE_SHOULDER_COMBO 4
bool sdl_gamepad_capture_begin(void);
void sdl_gamepad_capture_cancel(void);
bool sdl_gamepad_capture_poll(int* out_type, int* out_id);

void sdl_story_font_enable(void);
void sdl_story_font_disable(void);
void sdl_story_font_reset(void);
bool sdl_is_story_font_enabled(void);
void sdl_story_font_set_grid(bool grid);
bool sdl_is_story_font_grid(void);
int sdl_story_font_text_width(cptr text, int len);
int sdl_get_cell_width(void);

#endif /* INCLUDED_PLATFORM_UI_H */
