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

#ifndef INCLUDED_UI_INFORMATION_SCENE_H
#define INCLUDED_UI_INFORMATION_SCENE_H

#include "app/app-session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_information_scene_scope {
    bool active;
    bool previous_active;
    bool published_overlay;
    bool restore_snapshot;
    app_snapshot previous_snapshot;
    app_menu_snapshot* previous_menu_snapshot;
    app_wait_scope wait_scope;
    app_input_capture_scope input_capture_scope;
} ui_information_scene_scope;

typedef enum ui_information_scene_event_kind {
    UI_INFORMATION_SCENE_EVENT_NONE = 0,
    UI_INFORMATION_SCENE_EVENT_KEY = 1,
    UI_INFORMATION_SCENE_EVENT_COMMAND = 2
} ui_information_scene_event_kind;

typedef struct ui_information_scene_event {
    u16b kind;
    int key;
    app_ui_command command;
} ui_information_scene_event;

bool ui_information_scene_supported(void);
bool ui_information_scene_set_refresh_enabled(bool enabled);
bool ui_information_scene_acquire(ui_information_scene_scope* scope);
bool ui_information_scene_claim_input(ui_information_scene_scope* scope,
    u16b reason);
bool ui_information_scene_enter(ui_information_scene_scope* scope);
bool ui_information_scene_present_ui(const app_ui_scene* scene);
bool ui_information_scene_present_overlay(ui_information_scene_scope* scope,
    const app_ui_scene* scene);
bool ui_information_scene_show_monster_recall(int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon,
    int* out_key);
bool ui_information_scene_wait_event(ui_information_scene_event* out_event,
    u16b ignored_flags);
bool ui_information_scene_wait_event_with_wait_reason(
    ui_information_scene_event* out_event, u16b ignored_flags, u16b reason,
    bool hidden_cursor);
bool ui_information_scene_wait_dismissal(u16b ignored_flags);
bool ui_information_scene_wait_dismissal_with_wait_reason(u16b ignored_flags,
    u16b reason, bool hidden_cursor);
int ui_information_scene_choice_from_event(
    const ui_information_scene_event* event);
int ui_information_scene_wait_choice(u16b ignored_flags);
int ui_information_scene_wait_choice_with_wait_reason(u16b ignored_flags,
    u16b reason, bool hidden_cursor);
int ui_information_scene_wait_key(void);
int ui_information_scene_wait_key_nonrepeat(void);
int ui_information_scene_wait_key_with_wait_reason(u16b reason);
int ui_information_scene_wait_key_hidden_with_wait_reason(u16b reason);
bool ui_information_scene_is_active(void);
bool ui_information_scene_owns_input(void);
void ui_information_scene_leave_without_restore(
    ui_information_scene_scope* scope);
void ui_information_scene_leave(ui_information_scene_scope* scope);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_UI_INFORMATION_SCENE_H */
