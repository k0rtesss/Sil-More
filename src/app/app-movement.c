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

#include "app/app-movement.h"

static u16b app_movement_allowed_modifiers(void)
{
    return APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL
        | APP_INPUT_MODIFIER_ALT
        | APP_INPUT_MODIFIER_META
        | APP_INPUT_MODIFIER_CAPS_LOCK
        | APP_INPUT_MODIFIER_NUM_LOCK;
}

static bool app_movement_context_known(u16b context)
{
    return context <= APP_MOVEMENT_CONTEXT_TARGETING;
}

static bool app_movement_action_known(u16b action)
{
    return action <= APP_MOVEMENT_ACTION_REST;
}

static bool app_movement_direction_known(u16b direction)
{
    return direction <= APP_MOVEMENT_DIRECTION_NORTHWEST;
}

static bool app_movement_binding_context_matches(u16b binding_context,
    u16b requested_context)
{
    return binding_context == APP_MOVEMENT_CONTEXT_ANY
        || requested_context == APP_MOVEMENT_CONTEXT_ANY
        || binding_context == requested_context;
}

static bool app_movement_binding_device_matches(u16b binding_device,
    u16b input_device)
{
    return binding_device == APP_INPUT_DEVICE_NONE
        || binding_device == input_device;
}

static bool app_movement_binding_type_matches(u16b binding_type,
    u16b input_type)
{
    return binding_type == APP_INPUT_TYPE_NONE
        || binding_type == input_type;
}

static bool app_movement_modifiers_match(const app_movement_binding* binding,
    u16b modifiers)
{
    if (!binding)
        return false;

    if ((modifiers & binding->required_modifiers) != binding->required_modifiers)
        return false;
    if (modifiers & binding->forbidden_modifiers)
        return false;

    return true;
}

static bool app_movement_input_extract_trigger(const app_input* input,
    u32b* out_trigger, u32b* out_trigger_aux)
{
    u32b trigger = 0;
    u32b trigger_aux = 0;

    if (!input)
        return false;

    switch (input->type)
    {
    case APP_INPUT_TYPE_KEY:
        trigger = input->payload.key.physical_key;
        if (!trigger)
            trigger = input->payload.key.logical_key;
        trigger_aux = input->payload.key.logical_key;
        break;

    case APP_INPUT_TYPE_GAMEPAD_BUTTON:
    case APP_INPUT_TYPE_GAMEPAD_AXIS:
        trigger = input->payload.gamepad.control;
        trigger_aux = input->payload.gamepad.secondary_control;
        break;

    case APP_INPUT_TYPE_POINTER_BUTTON:
        trigger = input->payload.pointer.button;
        trigger_aux = input->payload.pointer.clicks;
        break;

    case APP_INPUT_TYPE_TEXT:
        trigger = input->payload.text.codepoint;
        break;

    case APP_INPUT_TYPE_SYSTEM:
        trigger = input->payload.system.code;
        trigger_aux = input->payload.system.value;
        break;

    default:
        return false;
    }

    if (out_trigger)
        *out_trigger = trigger;
    if (out_trigger_aux)
        *out_trigger_aux = trigger_aux;

    return true;
}

static bool app_movement_input_is_press_like(const app_input* input)
{
    if (!input)
        return false;

    if ((input->flags & (APP_INPUT_FLAG_PRESS | APP_INPUT_FLAG_REPEAT)) == 0)
        return false;
    if ((input->flags & APP_INPUT_FLAG_RELEASE)
        && (input->flags & APP_INPUT_FLAG_PRESS) == 0)
    {
        return false;
    }

    return true;
}

static bool app_movement_direction_payload_matches(
    const app_movement_direction_payload* payload)
{
    app_movement_direction_payload expected;

    if (!payload)
        return false;

    if (payload->direction == APP_MOVEMENT_DIRECTION_NONE)
        return payload->dy == 0 && payload->dx == 0;

    if (!app_movement_direction_payload_from_direction(payload->direction,
            &expected))
    {
        return false;
    }

    return expected.dy == payload->dy && expected.dx == payload->dx;
}

static int app_movement_binding_specificity(const app_movement_binding* binding,
    u16b requested_context)
{
    int score = 0;

    if (!binding)
        return -1;

    if (requested_context != APP_MOVEMENT_CONTEXT_ANY
        && binding->context == requested_context)
    {
        score += 8;
    }
    if (binding->device != APP_INPUT_DEVICE_NONE)
        score += 4;
    if (binding->input_type != APP_INPUT_TYPE_NONE)
        score += 2;
    if (binding->trigger_aux != 0)
        score += 1;

    return score;
}

void app_movement_binding_clear(app_movement_binding* binding)
{
    if (!binding)
        return;

    memset(binding, 0, sizeof(*binding));
}

void app_movement_command_clear(app_movement_command* command)
{
    if (!command)
        return;

    memset(command, 0, sizeof(*command));
    command->format_version = APP_MOVEMENT_FORMAT_VERSION;
}

bool app_movement_action_is_directional(u16b action)
{
    return action == APP_MOVEMENT_ACTION_MOVE_DIR
        || action == APP_MOVEMENT_ACTION_RUN_DIR
        || action == APP_MOVEMENT_ACTION_INTERACT_DIR;
}

bool app_movement_direction_payload_from_direction(u16b direction,
    app_movement_direction_payload* out_payload)
{
    app_movement_direction_payload payload;

    if (!out_payload)
        return false;

    memset(&payload, 0, sizeof(payload));
    payload.direction = direction;

    switch (direction)
    {
    case APP_MOVEMENT_DIRECTION_CENTER:
        break;

    case APP_MOVEMENT_DIRECTION_NORTH:
        payload.dy = -1;
        break;

    case APP_MOVEMENT_DIRECTION_NORTHEAST:
        payload.dy = -1;
        payload.dx = 1;
        break;

    case APP_MOVEMENT_DIRECTION_EAST:
        payload.dx = 1;
        break;

    case APP_MOVEMENT_DIRECTION_SOUTHEAST:
        payload.dy = 1;
        payload.dx = 1;
        break;

    case APP_MOVEMENT_DIRECTION_SOUTH:
        payload.dy = 1;
        break;

    case APP_MOVEMENT_DIRECTION_SOUTHWEST:
        payload.dy = 1;
        payload.dx = -1;
        break;

    case APP_MOVEMENT_DIRECTION_WEST:
        payload.dx = -1;
        break;

    case APP_MOVEMENT_DIRECTION_NORTHWEST:
        payload.dy = -1;
        payload.dx = -1;
        break;

    default:
        return false;
    }

    *out_payload = payload;
    return true;
}

bool app_movement_direction_from_legacy_keypad(int keypad_dir,
    u16b* out_direction)
{
    u16b direction = APP_MOVEMENT_DIRECTION_NONE;

    switch (keypad_dir)
    {
    case 1:
        direction = APP_MOVEMENT_DIRECTION_SOUTHWEST;
        break;
    case 2:
        direction = APP_MOVEMENT_DIRECTION_SOUTH;
        break;
    case 3:
        direction = APP_MOVEMENT_DIRECTION_SOUTHEAST;
        break;
    case 4:
        direction = APP_MOVEMENT_DIRECTION_WEST;
        break;
    case 5:
        direction = APP_MOVEMENT_DIRECTION_CENTER;
        break;
    case 6:
        direction = APP_MOVEMENT_DIRECTION_EAST;
        break;
    case 7:
        direction = APP_MOVEMENT_DIRECTION_NORTHWEST;
        break;
    case 8:
        direction = APP_MOVEMENT_DIRECTION_NORTH;
        break;
    case 9:
        direction = APP_MOVEMENT_DIRECTION_NORTHEAST;
        break;
    default:
        return false;
    }

    if (out_direction)
        *out_direction = direction;

    return true;
}

int app_movement_direction_to_legacy_keypad(u16b direction)
{
    switch (direction)
    {
    case APP_MOVEMENT_DIRECTION_SOUTHWEST:
        return 1;
    case APP_MOVEMENT_DIRECTION_SOUTH:
        return 2;
    case APP_MOVEMENT_DIRECTION_SOUTHEAST:
        return 3;
    case APP_MOVEMENT_DIRECTION_WEST:
        return 4;
    case APP_MOVEMENT_DIRECTION_CENTER:
        return 5;
    case APP_MOVEMENT_DIRECTION_EAST:
        return 6;
    case APP_MOVEMENT_DIRECTION_NORTHWEST:
        return 7;
    case APP_MOVEMENT_DIRECTION_NORTH:
        return 8;
    case APP_MOVEMENT_DIRECTION_NORTHEAST:
        return 9;
    default:
        return 0;
    }
}

bool app_movement_binding_is_valid(const app_movement_binding* binding)
{
    if (!binding)
        return false;
    if (!app_movement_context_known(binding->context))
        return false;
    if (!app_movement_action_known(binding->action)
        || binding->action == APP_MOVEMENT_ACTION_NONE)
    {
        return false;
    }
    if (binding->device > APP_INPUT_DEVICE_SYSTEM)
        return false;
    if (binding->input_type > APP_INPUT_TYPE_SYSTEM)
        return false;
    if ((binding->required_modifiers | binding->forbidden_modifiers)
        & ~app_movement_allowed_modifiers())
    {
        return false;
    }
    if (binding->required_modifiers & binding->forbidden_modifiers)
        return false;
    if (binding->trigger == 0)
        return false;

    if (app_movement_action_is_directional(binding->action))
    {
        if (!app_movement_direction_known(binding->direction)
            || binding->direction == APP_MOVEMENT_DIRECTION_NONE)
        {
            return false;
        }
    }
    else if (binding->direction != APP_MOVEMENT_DIRECTION_NONE
        && binding->direction != APP_MOVEMENT_DIRECTION_CENTER)
    {
        return false;
    }

    return true;
}

bool app_movement_command_is_valid(const app_movement_command* command)
{
    if (!command)
        return false;
    if (command->format_version != APP_MOVEMENT_FORMAT_VERSION)
        return false;
    if (!app_movement_context_known(command->context))
        return false;
    if (!app_movement_action_known(command->action)
        || command->action == APP_MOVEMENT_ACTION_NONE)
    {
        return false;
    }
    if (command->device > APP_INPUT_DEVICE_SYSTEM)
        return false;
    if (command->input_type > APP_INPUT_TYPE_SYSTEM)
        return false;
    if (command->modifiers & ~app_movement_allowed_modifiers())
        return false;
    if (command->flags & ~(APP_MOVEMENT_COMMAND_FLAG_REPEAT
            | APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC
            | APP_MOVEMENT_COMMAND_FLAG_TOUCH
            | APP_MOVEMENT_COMMAND_FLAG_TRAVEL))
    {
        return false;
    }
    if (!app_movement_direction_payload_matches(&command->direction))
        return false;

    if (app_movement_action_is_directional(command->action))
    {
        if (command->direction.direction == APP_MOVEMENT_DIRECTION_NONE)
            return false;
    }
    else if (command->direction.direction != APP_MOVEMENT_DIRECTION_NONE
        && command->direction.direction != APP_MOVEMENT_DIRECTION_CENTER)
    {
        return false;
    }

    return true;
}

bool app_movement_command_from_input(u16b context, u16b action,
    u16b direction, const app_input* input,
    app_movement_command* out_command)
{
    u32b trigger = 0;
    u32b trigger_aux = 0;

    if (!input || !out_command)
        return false;
    if (!app_movement_input_extract_trigger(input, &trigger, &trigger_aux))
        return false;

    return app_movement_command_from_semantic(context, action, direction,
        input->device, input->type, input->source_id, input->modifiers,
        input->flags, trigger, trigger_aux, input->sequence,
        input->timestamp_usec, out_command);
}

bool app_movement_command_from_semantic(u16b context, u16b action,
    u16b direction, u16b device, u16b input_type, u16b source_id,
    u16b modifiers, u16b flags, u32b trigger, u32b trigger_aux,
    u64b sequence, u64b timestamp_usec, app_movement_command* out_command)
{
    if (!out_command)
        return false;

    app_movement_command_clear(out_command);
    out_command->context = context;
    out_command->action = action;
    out_command->modifiers = modifiers;
    out_command->device = device;
    out_command->input_type = input_type;
    out_command->source_id = source_id;
    out_command->trigger = trigger;
    out_command->trigger_aux = trigger_aux;
    out_command->sequence = sequence;
    out_command->timestamp_usec = timestamp_usec;

    if (flags & APP_INPUT_FLAG_REPEAT)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_REPEAT;
    if (flags & APP_INPUT_FLAG_SYNTHETIC)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC;
    if (device == APP_INPUT_DEVICE_TOUCH)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_TOUCH;
    if (trigger == APP_MOVEMENT_SEMANTIC_TRIGGER_POINTER_TRAVEL)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_TRAVEL;

    if (!app_movement_direction_payload_from_direction(direction,
            &out_command->direction))
    {
        return false;
    }

    return app_movement_command_is_valid(out_command);
}

bool app_movement_binding_matches_input(const app_movement_binding* binding,
    const app_input* input, u16b context)
{
    u32b trigger = 0;
    u32b trigger_aux = 0;

    if (!app_movement_binding_is_valid(binding) || !input)
        return false;
    if (!app_movement_input_is_press_like(input))
        return false;
    if (!app_movement_binding_context_matches(binding->context, context))
        return false;
    if (!app_movement_binding_device_matches(binding->device, input->device))
        return false;
    if (!app_movement_binding_type_matches(binding->input_type, input->type))
        return false;
    if (!app_movement_modifiers_match(binding, input->modifiers))
        return false;
    if (!app_movement_input_extract_trigger(input, &trigger, &trigger_aux))
        return false;
    if (binding->trigger != trigger)
        return false;
    if (binding->trigger_aux != 0 && binding->trigger_aux != trigger_aux)
        return false;

    return true;
}

bool app_movement_bindings_conflict(const app_movement_binding* left,
    const app_movement_binding* right)
{
    u16b supported = app_movement_allowed_modifiers();
    u32b max_combo = (u32b)supported + 1u;
    u32b combo;

    if (!app_movement_binding_is_valid(left)
        || !app_movement_binding_is_valid(right))
    {
        return false;
    }
    if (!app_movement_binding_context_matches(left->context, right->context))
        return false;
    if (!app_movement_binding_device_matches(left->device, right->device))
        return false;
    if (!app_movement_binding_type_matches(left->input_type, right->input_type))
        return false;
    if (left->trigger != right->trigger)
        return false;
    if (left->trigger_aux != 0 && right->trigger_aux != 0
        && left->trigger_aux != right->trigger_aux)
    {
        return false;
    }

    for (combo = 0; combo < max_combo; combo++)
    {
        u16b modifiers = (u16b)combo;

        if (modifiers & ~supported)
            continue;
        if (app_movement_modifiers_match(left, modifiers)
            && app_movement_modifiers_match(right, modifiers))
        {
            return true;
        }
    }

    return false;
}

bool app_movement_command_from_binding(const app_movement_binding* binding,
    const app_input* input, u16b context,
    app_movement_command* out_command)
{
    u32b trigger = 0;
    u32b trigger_aux = 0;

    if (!out_command)
        return false;
    if (!app_movement_binding_matches_input(binding, input, context))
        return false;
    if (!app_movement_input_extract_trigger(input, &trigger, &trigger_aux))
        return false;

    app_movement_command_clear(out_command);
    out_command->context = (binding->context == APP_MOVEMENT_CONTEXT_ANY)
        ? context
        : binding->context;
    out_command->action = binding->action;
    out_command->modifiers = input->modifiers;
    out_command->device = input->device;
    out_command->input_type = input->type;
    out_command->source_id = input->source_id;
    out_command->trigger = trigger;
    out_command->trigger_aux = trigger_aux;
    out_command->sequence = input->sequence;
    out_command->timestamp_usec = input->timestamp_usec;

    if (input->flags & APP_INPUT_FLAG_REPEAT)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_REPEAT;
    if (input->flags & APP_INPUT_FLAG_SYNTHETIC)
        out_command->flags |= APP_MOVEMENT_COMMAND_FLAG_SYNTHETIC;

    if (binding->direction != APP_MOVEMENT_DIRECTION_NONE)
    {
        if (!app_movement_direction_payload_from_direction(binding->direction,
                &out_command->direction))
        {
            return false;
        }
    }
    else if (!app_movement_direction_payload_from_direction(
            APP_MOVEMENT_DIRECTION_CENTER, &out_command->direction))
    {
        return false;
    }

    return app_movement_command_is_valid(out_command);
}

bool app_movement_resolve_input(const app_movement_binding* bindings,
    size_t binding_count, const app_input* input, u16b context,
    app_movement_command* out_command)
{
    const app_movement_binding* selected = NULL;
    int best_score = -1;
    size_t i;

    if (!bindings || !binding_count || !input || !out_command)
        return false;

    /* Prefer the most specific binding so global fallbacks do not shadow
     * context- or device-specific movement chords.
     */
    for (i = 0; i < binding_count; i++)
    {
        const app_movement_binding* binding = &bindings[i];
        int score;

        if (!app_movement_binding_matches_input(binding, input, context))
            continue;

        score = app_movement_binding_specificity(binding, context);
        if (score > best_score)
        {
            best_score = score;
            selected = binding;
        }
    }

    if (!selected)
        return false;

    return app_movement_command_from_binding(selected, input, context,
        out_command);
}
