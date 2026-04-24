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

#include "angband.h"

#include "app/app-ui-command.h"

void app_ui_widget_ref_clear(app_ui_widget_ref* ref)
{
    if (!ref)
        return;

    memset(ref, 0, sizeof(*ref));
}

void app_ui_command_clear(app_ui_command* command)
{
    if (!command)
        return;

    memset(command, 0, sizeof(*command));
    command->format_version = APP_UI_COMMAND_FORMAT_VERSION;
}

void app_ui_focus_state_clear(app_ui_focus_state* state)
{
    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->format_version = APP_UI_COMMAND_FORMAT_VERSION;
}

bool app_ui_widget_ref_is_valid(const app_ui_widget_ref* ref)
{
    if (!ref)
        return false;

    return ref->role != APP_UI_WIDGET_ROLE_NONE
        && ref->action != APP_UI_WIDGET_ACTION_NONE;
}

bool app_ui_command_is_valid(const app_ui_command* command)
{
    if (!command)
        return false;
    if (command->format_version != APP_UI_COMMAND_FORMAT_VERSION)
        return false;
    if (command->kind == APP_UI_COMMAND_KIND_NONE)
        return false;

    return app_ui_widget_ref_is_valid(&command->target);
}

u16b app_ui_command_kind_from_widget_action(u16b action)
{
    switch (action)
    {
    case APP_UI_WIDGET_ACTION_ACTIVATE:
        return APP_UI_COMMAND_KIND_ACTIVATE;

    case APP_UI_WIDGET_ACTION_SELECT:
        return APP_UI_COMMAND_KIND_SELECT;

    case APP_UI_WIDGET_ACTION_CANCEL:
        return APP_UI_COMMAND_KIND_CANCEL;

    case APP_UI_WIDGET_ACTION_INSPECT:
        return APP_UI_COMMAND_KIND_INSPECT;

    case APP_UI_WIDGET_ACTION_SCROLL:
        return APP_UI_COMMAND_KIND_SCROLL;

    case APP_UI_WIDGET_ACTION_DRAG:
        return APP_UI_COMMAND_KIND_DRAG;

    case APP_UI_WIDGET_ACTION_RESIZE:
        return APP_UI_COMMAND_KIND_RESIZE;

    default:
        return APP_UI_COMMAND_KIND_NONE;
    }
}
