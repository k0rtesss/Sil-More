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

#ifndef INCLUDED_PLATFORM_INPUT_H
#define INCLUDED_PLATFORM_INPUT_H

#include "h-basic.h"
#include "gamepad-config.h"

bool platform_submit_directional_movement(int keypad_dir, bool shift, bool ctrl,
    bool alt, u16b device, u16b input_type, u16b source_id,
    u16b input_flags, u32b trigger, u32b trigger_aux);

bool steamdeck_controls_active(void);
bool portable_controls_active(void);
int steamdeck_back_key(void);
int steamdeck_confirm_key(void);
int steamdeck_info_key(void);
int steamdeck_alt_action_key(void);
int steamdeck_secondary_key(void);

bool platform_gamepad_enabled(void);
void platform_set_gamepad_enabled(bool value);
bool platform_gamepad_auto_mode(void);
void platform_set_gamepad_auto_mode(bool value);
bool platform_steamdeck_mode(void);
void platform_set_steamdeck_mode(bool value);
bool platform_gamepad_use_dpad(void);
void platform_set_gamepad_use_dpad(bool value);
bool platform_gamepad_use_left_stick(void);
void platform_set_gamepad_use_left_stick(bool value);
int platform_gamepad_button_binding(int button);
void platform_set_gamepad_button_binding(int button, int binding);
int platform_gamepad_trigger_binding(int index);
void platform_set_gamepad_trigger_binding(int index, int binding);
int platform_gamepad_left_stick_binding(int dir);
void platform_set_gamepad_left_stick_binding(int dir, int binding);
int platform_gamepad_right_stick_binding(int dir);
void platform_set_gamepad_right_stick_binding(int dir, int binding);
int platform_gamepad_combo_binding(int modifier, int type, int id);
void platform_set_gamepad_combo_binding(int modifier, int type, int id, int binding);
int platform_gamepad_shoulder_combo_binding(void);
void platform_set_gamepad_shoulder_combo_binding(int binding);
int platform_gamepad_default_button_binding(int button);
int platform_gamepad_default_trigger_binding(int index);
int platform_gamepad_default_left_stick_binding(int dir);
int platform_gamepad_default_right_stick_binding(int dir);
int platform_gamepad_default_combo_binding(int modifier, int type, int id);
int platform_gamepad_default_shoulder_combo_binding(void);
void platform_gamepad_reset_bindings_to_default(void);
void platform_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
void platform_gamepad_action_binding_short_label(int binding, char* buf,
    size_t buflen);
void platform_input_prompt_for_ui_action(u16b device, u16b action,
    int action_key, char* buf, size_t buflen);

bool platform_touch_menu_commands_enabled(int category);
void platform_set_touch_menu_commands_enabled(int category, bool value);
bool platform_touch_menu_commands_default_enabled(int category);
int platform_touch_profile(void);
void platform_set_touch_profile(int profile);
int platform_touch_default_profile(void);
bool platform_touch_pane_default_open(void);
void platform_set_touch_pane_default_open(bool value);
bool platform_touch_pane_default_open_default(void);
int platform_touch_movement_mode(void);
void platform_set_touch_movement_mode(int mode);
int platform_touch_movement_default_mode(void);
bool platform_touch_round_movement_enabled(void);
void platform_set_touch_round_movement_enabled(bool value);
bool platform_touch_round_movement_default_enabled(void);
int platform_touch_zone_overlay_mode(void);
void platform_set_touch_zone_overlay_mode(int mode);
int platform_touch_zone_overlay_default_mode(void);
int platform_touch_zone_center_binding(int index);
void platform_set_touch_zone_center_binding(int index, int binding);
int platform_touch_zone_center_default_binding(int index);
int platform_touch_corner_up_down_side(void);
void platform_set_touch_corner_up_down_side(int side);
int platform_touch_corner_up_down_default_side(void);
int platform_touch_corner_action_binding(int index);
void platform_set_touch_corner_action_binding(int index, int binding);
int platform_touch_corner_action_default_binding(int index);
int platform_touch_top_panel_mode(void);
void platform_set_touch_top_panel_mode(int mode);
int platform_touch_top_panel_default_mode(void);
bool platform_touch_top_panel_default_open(void);
void platform_set_touch_top_panel_default_open(bool value);
bool platform_touch_top_panel_default_open_default(void);
int platform_touch_top_panel_binding(int index, bool long_press);
void platform_set_touch_top_panel_binding(int index, bool long_press,
    int binding);
int platform_touch_top_panel_default_binding(int index, bool long_press);
int platform_touch_pane_binding(int index);
void platform_set_touch_pane_binding(int index, int binding);
int platform_touch_pane_default_binding(int index);
int platform_touch_pane_binding_for_panel(int panel, int index);
void platform_set_touch_pane_binding_for_panel(int panel, int index, int binding);
int platform_touch_pane_default_binding_for_panel(int panel, int index);
void platform_touch_pane_reset_bindings_to_default(void);
cptr platform_touch_pane_slot_name(int index);
void platform_touch_pane_button_label(int index, char* buf, size_t buflen);
void platform_set_touch_pane_button_label(int index, cptr label);
void platform_clear_touch_pane_button_label(int index);
void platform_touch_pane_button_label_for_panel(int panel, int index, char* buf,
    size_t buflen);
void platform_set_touch_pane_button_label_for_panel(int panel, int index,
    cptr label);
void platform_clear_touch_pane_button_label_for_panel(int panel, int index);
void platform_touch_pane_panel_name(int panel, char* buf, size_t buflen);
void platform_set_touch_pane_panel_name(int panel, cptr name);
bool platform_touch_pane_key_labels_visible(void);
void platform_set_touch_pane_key_labels_visible(bool value);
bool platform_touch_pane_key_labels_default_visible(void);
bool platform_touch_pane_inventory_equipment_cycle(void);
void platform_set_touch_pane_inventory_equipment_cycle(bool value);
bool platform_touch_pane_inventory_equipment_default_cycle(void);
bool platform_touch_swipe_enabled(void);
void platform_set_touch_swipe_enabled(bool value);
int platform_touch_swipe_binding(int dir);
void platform_set_touch_swipe_binding(int dir, int binding);
bool platform_touch_swipe_default_enabled(void);
int platform_touch_swipe_default_binding(int dir);

#define GAMEPAD_CAPTURE_BUTTON 0
#define GAMEPAD_CAPTURE_TRIGGER 1
#define GAMEPAD_CAPTURE_LEFT_STICK 2
#define GAMEPAD_CAPTURE_RIGHT_STICK 3
#define GAMEPAD_CAPTURE_SHOULDER_COMBO 4
bool platform_gamepad_capture_begin(void);
void platform_gamepad_capture_cancel(void);
bool platform_gamepad_capture_poll(int* out_type, int* out_id);

#endif /* INCLUDED_PLATFORM_INPUT_H */
