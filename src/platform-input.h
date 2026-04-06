#ifndef INCLUDED_PLATFORM_INPUT_H
#define INCLUDED_PLATFORM_INPUT_H

#include "h-basic.h"
#include "gamepad-config.h"

bool steamdeck_controls_active(void);
bool portable_controls_active(void);
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
void sdl_gamepad_action_binding_short_label(int binding, char* buf,
    size_t buflen);

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
void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf,
    size_t buflen);
void set_sdl_touch_pane_button_label_for_panel(int panel, int index,
    cptr label);
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

#endif /* INCLUDED_PLATFORM_INPUT_H */
