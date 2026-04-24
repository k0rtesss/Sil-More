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

#ifndef INCLUDED_APP_UI_COMMAND_H
#define INCLUDED_APP_UI_COMMAND_H

#include "app-input.h"
#include "app-ui.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_UI_COMMAND_FORMAT_VERSION 1u

typedef enum app_ui_command_kind {
    APP_UI_COMMAND_KIND_NONE = 0,
    APP_UI_COMMAND_KIND_HOVER = 1,
    APP_UI_COMMAND_KIND_FOCUS = 2,
    APP_UI_COMMAND_KIND_PRESS = 3,
    APP_UI_COMMAND_KIND_RELEASE = 4,
    APP_UI_COMMAND_KIND_ACTIVATE = 5,
    APP_UI_COMMAND_KIND_SELECT = 6,
    APP_UI_COMMAND_KIND_CANCEL = 7,
    APP_UI_COMMAND_KIND_SCROLL = 8,
    APP_UI_COMMAND_KIND_INSPECT = 9,
    APP_UI_COMMAND_KIND_CONTEXT = 10,
    APP_UI_COMMAND_KIND_DRAG = 11,
    APP_UI_COMMAND_KIND_RESIZE = 12
} app_ui_command_kind;

typedef enum app_ui_focus_reason {
    APP_UI_FOCUS_REASON_NONE = 0,
    APP_UI_FOCUS_REASON_POINTER_HOVER = 1,
    APP_UI_FOCUS_REASON_POINTER_PRESS = 2,
    APP_UI_FOCUS_REASON_TOUCH_PRESS = 3,
    APP_UI_FOCUS_REASON_KEYBOARD = 4,
    APP_UI_FOCUS_REASON_GAMEPAD = 5,
    APP_UI_FOCUS_REASON_COMMAND = 6
} app_ui_focus_reason;

typedef struct app_ui_widget_ref {
    u16b scene_kind;
    u16b panel_index;
    u16b panel_layer;
    u16b panel_style;
    u16b target_kind;
    s16b widget_id;
    s16b action_key;
    u16b role;
    u16b action;
    u16b flags;
    u16b focus_area;
    u16b state_flags;
    s16b focus_order;
    s16b owner_id;
    s32b payload0;
    s32b payload1;
    char label[APP_UI_LABEL_MAX];
    char tooltip[APP_UI_TOOLTIP_MAX];
} app_ui_widget_ref;

typedef struct app_ui_command {
    u16b format_version;
    u16b kind;
    u16b device;
    u16b input_type;
    u16b input_flags;
    u16b modifiers;
    u16b source_id;
    u16b reserved;
    s32b x;
    s32b y;
    s32b dx;
    s32b dy;
    s32b scroll_x;
    s32b scroll_y;
    u16b button;
    u16b clicks;
    u64b sequence;
    u64b timestamp_usec;
    app_ui_widget_ref target;
} app_ui_command;

typedef struct app_ui_focus_state {
    u16b format_version;
    u16b active;
    u16b reason;
    u16b pressed;
    u64b sequence;
    u64b timestamp_usec;
    app_ui_widget_ref target;
} app_ui_focus_state;

void app_ui_widget_ref_clear(app_ui_widget_ref* ref);
void app_ui_command_clear(app_ui_command* command);
void app_ui_focus_state_clear(app_ui_focus_state* state);
bool app_ui_widget_ref_is_valid(const app_ui_widget_ref* ref);
bool app_ui_command_is_valid(const app_ui_command* command);
u16b app_ui_command_kind_from_widget_action(u16b action);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_UI_COMMAND_H */
