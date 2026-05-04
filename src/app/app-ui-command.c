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

static bool app_ui_command_kind_is_known(u16b kind)
{
    return kind <= APP_UI_COMMAND_KIND_RESIZE;
}

static u16b app_ui_widget_action_from_semantic_action(u32b action_id)
{
    switch (action_id)
    {
    case APP_UI_SEMANTIC_ACTION_CANCEL:
    case APP_UI_SEMANTIC_ACTION_CLOSE:
    case APP_UI_SEMANTIC_ACTION_TRAVEL_CANCEL:
        return APP_UI_WIDGET_ACTION_CANCEL;

    case APP_UI_SEMANTIC_ACTION_SELECT:
        return APP_UI_WIDGET_ACTION_SELECT;

    case APP_UI_SEMANTIC_ACTION_INSPECT:
    case APP_UI_SEMANTIC_ACTION_RECALL:
        return APP_UI_WIDGET_ACTION_INSPECT;

    default:
        return APP_UI_WIDGET_ACTION_ACTIVATE;
    }
}

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
    if (!app_ui_command_kind_is_known(command->kind)
        || command->kind == APP_UI_COMMAND_KIND_NONE)
        return false;
    if ((command->semantic_command_id || command->semantic_action_id)
        && (!app_ui_semantic_command_id_is_known(
                command->semantic_command_id)
            || !app_ui_semantic_action_id_is_known(
                command->semantic_action_id)))
    {
        return false;
    }

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

bool app_ui_semantic_command_id_is_known(u32b command_id)
{
    switch (command_id)
    {
    case APP_UI_SEMANTIC_COMMAND_NONE:
    case APP_UI_SEMANTIC_COMMAND_TOUCH_ZONE:
    case APP_UI_SEMANTIC_COMMAND_TOUCH_TOP_PANEL:
    case APP_UI_SEMANTIC_COMMAND_ROUND_MOVEMENT:
    case APP_UI_SEMANTIC_COMMAND_PLAYER_ACTION_MENU:
    case APP_UI_SEMANTIC_COMMAND_POINTER_ATTACK:
    case APP_UI_SEMANTIC_COMMAND_POINTER_RECALL:
    case APP_UI_SEMANTIC_COMMAND_POINTER_INSPECT:
    case APP_UI_SEMANTIC_COMMAND_POINTER_TRAVEL:
        return true;

    default:
        return false;
    }
}

bool app_ui_semantic_action_id_is_known(u32b action_id)
{
    switch (action_id)
    {
    case APP_UI_SEMANTIC_ACTION_NONE:
    case APP_UI_SEMANTIC_ACTION_PRESS:
    case APP_UI_SEMANTIC_ACTION_RELEASE:
    case APP_UI_SEMANTIC_ACTION_ACTIVATE:
    case APP_UI_SEMANTIC_ACTION_CANCEL:
    case APP_UI_SEMANTIC_ACTION_SHORT_BINDING:
    case APP_UI_SEMANTIC_ACTION_LONG_BINDING:
    case APP_UI_SEMANTIC_ACTION_OPEN:
    case APP_UI_SEMANTIC_ACTION_CLOSE:
    case APP_UI_SEMANTIC_ACTION_SELECT:
    case APP_UI_SEMANTIC_ACTION_MOVE:
    case APP_UI_SEMANTIC_ACTION_RUN:
    case APP_UI_SEMANTIC_ACTION_INTERACT:
    case APP_UI_SEMANTIC_ACTION_WAIT:
    case APP_UI_SEMANTIC_ACTION_REST:
    case APP_UI_SEMANTIC_ACTION_ATTACK_PRIMARY:
    case APP_UI_SEMANTIC_ACTION_ATTACK_SECONDARY:
    case APP_UI_SEMANTIC_ACTION_INSPECT:
    case APP_UI_SEMANTIC_ACTION_RECALL:
    case APP_UI_SEMANTIC_ACTION_TRAVEL_START:
    case APP_UI_SEMANTIC_ACTION_TRAVEL_STEP:
    case APP_UI_SEMANTIC_ACTION_TRAVEL_CANCEL:
        return true;

    default:
        return false;
    }
}

u16b app_ui_semantic_target_kind_for_command(u32b command_id)
{
    switch (command_id)
    {
    case APP_UI_SEMANTIC_COMMAND_TOUCH_ZONE:
        return APP_UI_SEMANTIC_TARGET_TOUCH_ZONE;

    case APP_UI_SEMANTIC_COMMAND_TOUCH_TOP_PANEL:
        return APP_UI_SEMANTIC_TARGET_TOUCH_TOP_PANEL;

    case APP_UI_SEMANTIC_COMMAND_ROUND_MOVEMENT:
        return APP_UI_SEMANTIC_TARGET_ROUND_MOVEMENT;

    case APP_UI_SEMANTIC_COMMAND_PLAYER_ACTION_MENU:
        return APP_UI_SEMANTIC_TARGET_PLAYER_ACTION_MENU;

    case APP_UI_SEMANTIC_COMMAND_POINTER_ATTACK:
        return APP_UI_SEMANTIC_TARGET_POINTER_ATTACK;

    case APP_UI_SEMANTIC_COMMAND_POINTER_RECALL:
        return APP_UI_SEMANTIC_TARGET_POINTER_RECALL;

    case APP_UI_SEMANTIC_COMMAND_POINTER_INSPECT:
        return APP_UI_SEMANTIC_TARGET_POINTER_INSPECT;

    case APP_UI_SEMANTIC_COMMAND_POINTER_TRAVEL:
        return APP_UI_SEMANTIC_TARGET_POINTER_TRAVEL;

    default:
        return APP_UI_SEMANTIC_TARGET_NONE;
    }
}

u16b app_ui_command_kind_from_semantic_action(u32b action_id)
{
    switch (action_id)
    {
    case APP_UI_SEMANTIC_ACTION_PRESS:
        return APP_UI_COMMAND_KIND_PRESS;

    case APP_UI_SEMANTIC_ACTION_RELEASE:
        return APP_UI_COMMAND_KIND_RELEASE;

    case APP_UI_SEMANTIC_ACTION_CANCEL:
    case APP_UI_SEMANTIC_ACTION_CLOSE:
    case APP_UI_SEMANTIC_ACTION_TRAVEL_CANCEL:
        return APP_UI_COMMAND_KIND_CANCEL;

    case APP_UI_SEMANTIC_ACTION_SELECT:
        return APP_UI_COMMAND_KIND_SELECT;

    case APP_UI_SEMANTIC_ACTION_INSPECT:
        return APP_UI_COMMAND_KIND_INSPECT;

    case APP_UI_SEMANTIC_ACTION_RECALL:
        return APP_UI_COMMAND_KIND_CONTEXT;

    default:
        return APP_UI_COMMAND_KIND_ACTIVATE;
    }
}

void app_ui_semantic_widget_ref_init(app_ui_widget_ref* ref,
    u32b command_id, u32b action_id, s16b widget_id, s32b payload0,
    s32b payload1, cptr label, cptr tooltip)
{
    if (!ref)
        return;

    app_ui_widget_ref_clear(ref);
    ref->target_kind = app_ui_semantic_target_kind_for_command(command_id);
    ref->widget_id = widget_id;
    ref->role = (command_id == APP_UI_SEMANTIC_COMMAND_POINTER_INSPECT
            || command_id == APP_UI_SEMANTIC_COMMAND_POINTER_RECALL
            || command_id == APP_UI_SEMANTIC_COMMAND_POINTER_TRAVEL
            || command_id == APP_UI_SEMANTIC_COMMAND_POINTER_ATTACK)
        ? APP_UI_WIDGET_ROLE_MAP_CELL
        : APP_UI_WIDGET_ROLE_BUTTON;
    ref->action = app_ui_widget_action_from_semantic_action(action_id);
    ref->flags = APP_UI_INTERACTION_FLAG_POINTER_ENABLED
        | APP_UI_INTERACTION_FLAG_TOUCH_TARGET;
    ref->payload0 = payload0;
    ref->payload1 = payload1;
    SDL_strlcpy(ref->label, label ? label : "", sizeof(ref->label));
    SDL_strlcpy(ref->tooltip, tooltip ? tooltip : "", sizeof(ref->tooltip));
}

bool app_ui_semantic_command_init(app_ui_command* command,
    u32b command_id, u32b action_id, u16b device, u16b input_type,
    u16b input_flags, u16b modifiers, u16b source_id, s32b x, s32b y,
    s32b dx, s32b dy, u16b button, u16b clicks, u64b sequence,
    u64b timestamp_usec, s16b widget_id, s32b payload0, s32b payload1,
    cptr label, cptr tooltip)
{
    if (!command)
        return false;
    if (!app_ui_semantic_command_id_is_known(command_id)
        || command_id == APP_UI_SEMANTIC_COMMAND_NONE)
    {
        return false;
    }
    if (!app_ui_semantic_action_id_is_known(action_id)
        || action_id == APP_UI_SEMANTIC_ACTION_NONE)
    {
        return false;
    }

    app_ui_command_clear(command);
    command->kind = app_ui_command_kind_from_semantic_action(action_id);
    command->device = device;
    command->input_type = input_type;
    command->input_flags = input_flags;
    command->modifiers = modifiers;
    command->source_id = source_id;
    command->semantic_command_id = command_id;
    command->semantic_action_id = action_id;
    command->x = x;
    command->y = y;
    command->dx = dx;
    command->dy = dy;
    command->button = button;
    command->clicks = clicks;
    command->sequence = sequence;
    command->timestamp_usec = timestamp_usec;
    app_ui_semantic_widget_ref_init(&command->target, command_id, action_id,
        widget_id, payload0, payload1, label, tooltip);

    return app_ui_command_is_valid(command);
}
