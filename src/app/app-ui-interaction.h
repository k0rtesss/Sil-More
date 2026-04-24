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
#ifndef INCLUDED_APP_UI_INTERACTION_H
#define INCLUDED_APP_UI_INTERACTION_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_UI_TOOLTIP_MAX 160u

typedef enum app_ui_widget_role {
    APP_UI_WIDGET_ROLE_NONE = 0,
    APP_UI_WIDGET_ROLE_BUTTON = 1,
    APP_UI_WIDGET_ROLE_LIST_ITEM = 2,
    APP_UI_WIDGET_ROLE_TAB = 3,
    APP_UI_WIDGET_ROLE_SCROLL_REGION = 4,
    APP_UI_WIDGET_ROLE_PANEL_DRAG_HANDLE = 5,
    APP_UI_WIDGET_ROLE_PANEL_RESIZE_HANDLE = 6,
    APP_UI_WIDGET_ROLE_MAP_CELL = 7
} app_ui_widget_role;

typedef enum app_ui_widget_action {
    APP_UI_WIDGET_ACTION_NONE = 0,
    APP_UI_WIDGET_ACTION_ACTIVATE = 1,
    APP_UI_WIDGET_ACTION_SELECT = 2,
    APP_UI_WIDGET_ACTION_CANCEL = 3,
    APP_UI_WIDGET_ACTION_INSPECT = 4,
    APP_UI_WIDGET_ACTION_SCROLL = 5,
    APP_UI_WIDGET_ACTION_DRAG = 6,
    APP_UI_WIDGET_ACTION_RESIZE = 7
} app_ui_widget_action;

typedef enum app_ui_interaction_flag {
    APP_UI_INTERACTION_FLAG_NONE = 0x0000u,
    APP_UI_INTERACTION_FLAG_POINTER_ENABLED = 0x0001u,
    APP_UI_INTERACTION_FLAG_TOUCH_TARGET = 0x0002u,
    APP_UI_INTERACTION_FLAG_DRAGGABLE = 0x0004u,
    APP_UI_INTERACTION_FLAG_RESIZABLE = 0x0008u,
    APP_UI_INTERACTION_FLAG_TOOLTIP = 0x0010u
} app_ui_interaction_flag;

typedef struct app_ui_interaction {
    s16b action_key;
    u16b role;
    u16b action;
    u16b flags;
    char tooltip[APP_UI_TOOLTIP_MAX];
} app_ui_interaction;

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_UI_INTERACTION_H */
