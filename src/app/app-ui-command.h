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

#define APP_UI_COMMAND_FORMAT_VERSION 2u

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

typedef enum app_ui_semantic_command_id {
    APP_UI_SEMANTIC_COMMAND_NONE = 0,
    APP_UI_SEMANTIC_COMMAND_TOUCH_ZONE = 0x00010001u,
    APP_UI_SEMANTIC_COMMAND_TOUCH_TOP_PANEL = 0x00010002u,
    APP_UI_SEMANTIC_COMMAND_ROUND_MOVEMENT = 0x00010003u,
    APP_UI_SEMANTIC_COMMAND_PLAYER_ACTION_MENU = 0x00010004u,
    APP_UI_SEMANTIC_COMMAND_POINTER_ATTACK = 0x00010005u,
    APP_UI_SEMANTIC_COMMAND_POINTER_RECALL = 0x00010006u,
    APP_UI_SEMANTIC_COMMAND_POINTER_INSPECT = 0x00010007u,
    APP_UI_SEMANTIC_COMMAND_POINTER_TRAVEL = 0x00010008u
} app_ui_semantic_command_id;

typedef enum app_ui_semantic_action_id {
    APP_UI_SEMANTIC_ACTION_NONE = 0,
    APP_UI_SEMANTIC_ACTION_PRESS = 0x00020001u,
    APP_UI_SEMANTIC_ACTION_RELEASE = 0x00020002u,
    APP_UI_SEMANTIC_ACTION_ACTIVATE = 0x00020003u,
    APP_UI_SEMANTIC_ACTION_CANCEL = 0x00020004u,
    APP_UI_SEMANTIC_ACTION_SHORT_BINDING = 0x00020005u,
    APP_UI_SEMANTIC_ACTION_LONG_BINDING = 0x00020006u,
    APP_UI_SEMANTIC_ACTION_OPEN = 0x00020007u,
    APP_UI_SEMANTIC_ACTION_CLOSE = 0x00020008u,
    APP_UI_SEMANTIC_ACTION_SELECT = 0x00020009u,
    APP_UI_SEMANTIC_ACTION_MOVE = 0x0002000Au,
    APP_UI_SEMANTIC_ACTION_RUN = 0x0002000Bu,
    APP_UI_SEMANTIC_ACTION_INTERACT = 0x0002000Cu,
    APP_UI_SEMANTIC_ACTION_WAIT = 0x0002000Du,
    APP_UI_SEMANTIC_ACTION_REST = 0x0002000Eu,
    APP_UI_SEMANTIC_ACTION_ATTACK_PRIMARY = 0x0002000Fu,
    APP_UI_SEMANTIC_ACTION_ATTACK_SECONDARY = 0x00020010u,
    APP_UI_SEMANTIC_ACTION_INSPECT = 0x00020011u,
    APP_UI_SEMANTIC_ACTION_RECALL = 0x00020012u,
    APP_UI_SEMANTIC_ACTION_TRAVEL_START = 0x00020013u,
    APP_UI_SEMANTIC_ACTION_TRAVEL_STEP = 0x00020014u,
    APP_UI_SEMANTIC_ACTION_TRAVEL_CANCEL = 0x00020015u
} app_ui_semantic_action_id;

typedef enum app_ui_semantic_target_kind {
    APP_UI_SEMANTIC_TARGET_NONE = 0,
    APP_UI_SEMANTIC_TARGET_TOUCH_ZONE = 0x0100u,
    APP_UI_SEMANTIC_TARGET_TOUCH_TOP_PANEL = 0x0101u,
    APP_UI_SEMANTIC_TARGET_ROUND_MOVEMENT = 0x0102u,
    APP_UI_SEMANTIC_TARGET_PLAYER_ACTION_MENU = 0x0103u,
    APP_UI_SEMANTIC_TARGET_POINTER_ATTACK = 0x0104u,
    APP_UI_SEMANTIC_TARGET_POINTER_RECALL = 0x0105u,
    APP_UI_SEMANTIC_TARGET_POINTER_INSPECT = 0x0106u,
    APP_UI_SEMANTIC_TARGET_POINTER_TRAVEL = 0x0107u
} app_ui_semantic_target_kind;

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
    u32b semantic_command_id;
    u32b semantic_action_id;
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
bool app_ui_semantic_command_id_is_known(u32b command_id);
bool app_ui_semantic_action_id_is_known(u32b action_id);
u16b app_ui_semantic_target_kind_for_command(u32b command_id);
u16b app_ui_command_kind_from_semantic_action(u32b action_id);
void app_ui_semantic_widget_ref_init(app_ui_widget_ref* ref,
    u32b command_id, u32b action_id, s16b widget_id, s32b payload0,
    s32b payload1, cptr label, cptr tooltip);
bool app_ui_semantic_command_init(app_ui_command* command,
    u32b command_id, u32b action_id, u16b device, u16b input_type,
    u16b input_flags, u16b modifiers, u16b source_id, s32b x, s32b y,
    s32b dx, s32b dy, u16b button, u16b clicks, u64b sequence,
    u64b timestamp_usec, s16b widget_id, s32b payload0, s32b payload1,
    cptr label, cptr tooltip);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_UI_COMMAND_H */
