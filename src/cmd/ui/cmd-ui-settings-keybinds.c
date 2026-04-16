/* File: cmd-ui-settings-keybinds.c */

#include "angband.h"
#include "app/app-command.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "sdl-config.h"
#include "sdl-main-internal.h"
#include "sound-config.h"
#include "cmd-ui-settings-internal.h"
#include "cmd-ui.h"
#include "cmd-ui-settings.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

static int macros_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Legacy pref files removed", false },
#ifdef ALLOW_MACROS
        { 2, '2', "2) Append macros to a file", false },
        { 3, '3', "3) Query a macro", false },
        { 4, '4', "4) Create a macro", false },
        { 5, '5', "5) Remove a macro", false },
        { 6, '6', "6) Append keymaps to a file", false },
        { 7, '7', "7) Query a keymap", false },
        { 8, '8', "8) Create a keymap", false },
        { 9, '9', "9) Remove a keymap", false },
        { 10, '0', "0) Enter a new action", false },
#endif
    };

    return settings_choice_menu("Interact with Macros", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

typedef struct movement_setting_entry {
    u16b action;
    u16b direction;
    cptr label;
    bool essential;
} movement_setting_entry;

typedef struct movement_slot_state {
    bool in_use;
    app_movement_binding binding;
} movement_slot_state;

enum {
    MOVEMENT_SLOT_PRIMARY = 0,
    MOVEMENT_SLOT_SECONDARY = 1,
    MOVEMENT_SLOT_COUNT = 2
};

static const movement_setting_entry movement_settings[] = {
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_NORTHWEST, "Move NW", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_NORTH, "Move N", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_NORTHEAST, "Move NE", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_WEST, "Move W", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_EAST, "Move E", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_SOUTHWEST, "Move SW", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_SOUTH, "Move S", true },
    { APP_MOVEMENT_ACTION_MOVE_DIR, APP_MOVEMENT_DIRECTION_SOUTHEAST, "Move SE", true },
    { APP_MOVEMENT_ACTION_WAIT, APP_MOVEMENT_DIRECTION_NONE, "Wait", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_NORTHWEST, "Run NW", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_NORTH, "Run N", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_NORTHEAST, "Run NE", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_WEST, "Run W", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_EAST, "Run E", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_SOUTHWEST, "Run SW", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_SOUTH, "Run S", true },
    { APP_MOVEMENT_ACTION_RUN_DIR, APP_MOVEMENT_DIRECTION_SOUTHEAST, "Run SE", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_NORTHWEST, "Interact NW", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_NORTH, "Interact N", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_NORTHEAST, "Interact NE", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_WEST, "Interact W", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_EAST, "Interact E", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_SOUTHWEST, "Interact SW", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_SOUTH, "Interact S", true },
    { APP_MOVEMENT_ACTION_INTERACT_DIR, APP_MOVEMENT_DIRECTION_SOUTHEAST, "Interact SE", true },
    { APP_MOVEMENT_ACTION_REST, APP_MOVEMENT_DIRECTION_NONE, "Rest", false },
};

static cptr movement_preset_label(u16b preset_id)
{
    switch (preset_id)
    {
    case APP_MOVEMENT_PRESET_MODERN_ARROWS:
        return "Modern Arrows";
    case APP_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        return "Modern WASD+QEZC";
    case APP_MOVEMENT_PRESET_VI_KEYS:
        return "Vi Keys";
    case APP_MOVEMENT_PRESET_CLASSIC_SIL:
        return "Classic Sil";
    default:
        return "Custom";
    }
}

static void movement_adjust_view(int entry_count, int visible_rows, int* highlight,
    int* top)
{
    int max_top;

    if (!highlight || !top)
        return;

    if (entry_count <= 0)
    {
        *highlight = 0;
        *top = 0;
        return;
    }

    if (*highlight < 0)
        *highlight = 0;
    if (*highlight >= entry_count)
        *highlight = entry_count - 1;

    if (*top > *highlight)
        *top = *highlight;
    if ((*top + visible_rows) <= *highlight)
        *top = *highlight - visible_rows + 1;
    if (*top < 0)
        *top = 0;

    max_top = entry_count - visible_rows;
    if (max_top < 0)
        max_top = 0;
    if (*top > max_top)
        *top = max_top;
}

static bool movement_entry_matches_binding(const movement_setting_entry* entry,
    const app_movement_binding* binding)
{
    if (!entry || !binding || !app_movement_binding_is_valid(binding))
        return false;
    if (binding->device != APP_INPUT_DEVICE_KEYBOARD
        || binding->input_type != APP_INPUT_TYPE_KEY)
    {
        return false;
    }
    if (entry->action != binding->action)
        return false;
    if (app_movement_action_is_directional(entry->action))
        return entry->direction == binding->direction;

    return true;
}

static void movement_slot_states_clear(
    movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count)
{
    int i;

    for (i = 0; i < entry_count; i++)
    {
        int slot;

        for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
        {
            slots[i][slot].in_use = false;
            app_movement_binding_clear(&slots[i][slot].binding);
        }
    }
}

static void movement_slot_states_from_config(const struct sdl_config* source_config,
    movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count)
{
    int i;

    movement_slot_states_clear(slots, entry_count);
    if (!source_config)
        return;

    for (i = 0; i < source_config->movement_binding_count; i++)
    {
        const app_movement_binding* binding = &source_config->movement_bindings[i];
        int entry_index;

        for (entry_index = 0; entry_index < entry_count; entry_index++)
        {
            int slot;

            if (!movement_entry_matches_binding(&movement_settings[entry_index],
                    binding))
            {
                continue;
            }

            for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
            {
                if (!slots[entry_index][slot].in_use)
                {
                    slots[entry_index][slot].binding = *binding;
                    slots[entry_index][slot].in_use = true;
                    break;
                }
            }

            break;
        }
    }
}

static void movement_slot_states_to_config(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count,
    u16b preset_id, struct sdl_config* target_config)
{
    int i;

    if (!target_config)
        return;

    sdl_config_clear_movement_bindings(target_config);
    target_config->movement_keyboard_present = true;
    target_config->movement_keyboard_preset = preset_id;

    for (i = 0; i < entry_count; i++)
    {
        int slot;

        for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
        {
            if (!slots[i][slot].in_use)
                continue;
            if (target_config->movement_binding_count >= SDL_MOVEMENT_BINDING_MAX)
                return;

            target_config->movement_bindings[target_config->movement_binding_count++]
                = slots[i][slot].binding;
        }
    }
}

static bool movement_entry_has_any_binding(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_index)
{
    int slot;

    for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
    {
        if (slots[entry_index][slot].in_use)
            return true;
    }

    return false;
}

static bool movement_list_missing_essentials(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count,
    char* buf, size_t buflen)
{
    bool ok = true;
    size_t cursor = 0;
    int i;

    if (!buf || !buflen)
        return true;

    buf[0] = '\0';
    for (i = 0; i < entry_count; i++)
    {
        if (!movement_settings[i].essential)
            continue;
        if (movement_entry_has_any_binding(slots, i))
            continue;

        if (!ok)
            strnfcat(buf, buflen, &cursor, ", ");
        strnfcat(buf, buflen, &cursor, "%s", movement_settings[i].label);
        ok = false;
    }

    return ok;
}

static bool movement_find_conflict(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count,
    int skip_entry, int skip_slot, const app_movement_binding* candidate,
    int* out_entry, int* out_slot)
{
    int i;

    if (!candidate || !app_movement_binding_is_valid(candidate))
        return false;

    for (i = 0; i < entry_count; i++)
    {
        int slot;

        for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
        {
            if (!slots[i][slot].in_use)
                continue;
            if (i == skip_entry && slot == skip_slot)
                continue;
            if (!app_movement_bindings_conflict(candidate, &slots[i][slot].binding))
                continue;

            if (out_entry)
                *out_entry = i;
            if (out_slot)
                *out_slot = slot;
            return true;
        }
    }

    return false;
}

static bool movement_has_any_conflicts(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count)
{
    int i;

    for (i = 0; i < entry_count; i++)
    {
        int slot;

        for (slot = 0; slot < MOVEMENT_SLOT_COUNT; slot++)
        {
            if (!slots[i][slot].in_use)
                continue;
            if (movement_find_conflict(slots, entry_count, i, slot,
                    &slots[i][slot].binding, NULL, NULL))
            {
                return true;
            }
        }
    }

    return false;
}

static bool movement_capture_is_modifier_only(SDL_Scancode scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
    case SDL_SCANCODE_CAPSLOCK:
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return true;
    default:
        return false;
    }
}

static void movement_binding_key_label(SDL_Scancode scancode, char* buf,
    size_t buflen)
{
    const char* name;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    name = SDL_GetScancodeName(scancode);
    if (!name || !name[0])
    {
        SDL_strlcpy(buf, "(unknown)", buflen);
        return;
    }

    if (prefix(name, "Keypad "))
    {
        strnfmt(buf, buflen, "Numpad %s", name + strlen("Keypad "));
        return;
    }
    if (streq(name, "Return"))
    {
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    }
    if (streq(name, "Escape"))
    {
        SDL_strlcpy(buf, "Esc", buflen);
        return;
    }
    if (streq(name, "Page Up"))
    {
        SDL_strlcpy(buf, "PageUp", buflen);
        return;
    }
    if (streq(name, "Page Down"))
    {
        SDL_strlcpy(buf, "PageDown", buflen);
        return;
    }

    SDL_strlcpy(buf, name, buflen);
}

static void movement_binding_label(const movement_slot_state* slot_state,
    char* buf, size_t buflen)
{
    size_t cursor = 0;
    char key_buf[32];
    const app_movement_binding* binding;

    if (!buf || !buflen)
        return;

    if (!slot_state || !slot_state->in_use
        || !app_movement_binding_is_valid(&slot_state->binding))
    {
        SDL_strlcpy(buf, "(unbound)", buflen);
        return;
    }

    binding = &slot_state->binding;
    buf[0] = '\0';

    if (binding->required_modifiers & APP_INPUT_MODIFIER_CTRL)
        strnfcat(buf, buflen, &cursor, "Ctrl+");
    if (binding->required_modifiers & APP_INPUT_MODIFIER_SHIFT)
        strnfcat(buf, buflen, &cursor, "Shift+");
    if (binding->required_modifiers & APP_INPUT_MODIFIER_ALT)
        strnfcat(buf, buflen, &cursor, "Alt+");
    if (binding->required_modifiers & APP_INPUT_MODIFIER_META)
        strnfcat(buf, buflen, &cursor, "Meta+");

    movement_binding_key_label((SDL_Scancode)binding->trigger, key_buf,
        sizeof(key_buf));
    strnfcat(buf, buflen, &cursor, "%s", key_buf);
}

static void movement_build_binding_from_event(const movement_setting_entry* entry,
    const SDL_KeyboardEvent* key_event, app_movement_binding* out_binding)
{
    u16b required_modifiers = 0;
    u16b forbidden_modifiers = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT
        | APP_INPUT_MODIFIER_META;

    if (!entry || !key_event || !out_binding)
        return;

    if (key_event->mod & SDL_KMOD_SHIFT)
    {
        required_modifiers |= APP_INPUT_MODIFIER_SHIFT;
        forbidden_modifiers &= ~APP_INPUT_MODIFIER_SHIFT;
    }
    if (key_event->mod & SDL_KMOD_CTRL)
    {
        required_modifiers |= APP_INPUT_MODIFIER_CTRL;
        forbidden_modifiers &= ~APP_INPUT_MODIFIER_CTRL;
    }
    if (key_event->mod & SDL_KMOD_ALT)
    {
        required_modifiers |= APP_INPUT_MODIFIER_ALT;
        forbidden_modifiers &= ~APP_INPUT_MODIFIER_ALT;
    }
    if (key_event->mod & SDL_KMOD_GUI)
    {
        required_modifiers |= APP_INPUT_MODIFIER_META;
        forbidden_modifiers &= ~APP_INPUT_MODIFIER_META;
    }

    app_movement_binding_clear(out_binding);
    out_binding->context = APP_MOVEMENT_CONTEXT_ANY;
    out_binding->action = entry->action;
    out_binding->direction = entry->direction;
    out_binding->device = APP_INPUT_DEVICE_KEYBOARD;
    out_binding->input_type = APP_INPUT_TYPE_KEY;
    out_binding->required_modifiers = required_modifiers;
    out_binding->forbidden_modifiers = forbidden_modifiers;
    out_binding->trigger = (u32b)key_event->scancode;
}

typedef enum movement_capture_result {
    MOVEMENT_CAPTURE_CANCEL = 0,
    MOVEMENT_CAPTURE_CLEAR,
    MOVEMENT_CAPTURE_BIND
} movement_capture_result;

static movement_capture_result movement_capture_binding(
    const movement_setting_entry* entry, app_movement_binding* out_binding)
{
    SDL_Event event;

    app_command_clear_pending();
    platform_frame_flush_events();

    while (true)
    {
        if (!SDL_WaitEventTimeout(&event, 16))
        {
            platform_delay_ms(10);
            continue;
        }

        if (event.type != SDL_EVENT_KEY_DOWN)
            continue;
        if (event.key.repeat)
            continue;
        if (movement_capture_is_modifier_only(event.key.scancode))
            continue;
        if (event.key.key == SDLK_ESCAPE)
            return MOVEMENT_CAPTURE_CANCEL;
        if (event.key.key == SDLK_BACKSPACE || event.key.key == SDLK_DELETE)
            return MOVEMENT_CAPTURE_CLEAR;
        if (event.key.scancode == SDL_SCANCODE_UNKNOWN)
            continue;

        movement_build_binding_from_event(entry, &event.key, out_binding);
        if (!app_movement_binding_is_valid(out_binding))
            continue;

        return MOVEMENT_CAPTURE_BIND;
    }
}

#ifdef ALLOW_MACROS
/* Forward declaration */
static errr keymap_dump(cptr fname);
#endif

static bool movement_present_prompt_scene(bool showing_primary,
    const movement_setting_entry* entry, const movement_slot_state* current_slot,
    cptr prompt)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char binding_buf[64];

    if (!entry)
        return false;

    panel = settings_browser_scene_begin_ex(&scene, "Movement Settings",
        prompt ? prompt : "", 1100, 2200);
    if (!panel)
        return false;

    (void)app_ui_panel_add_tab(panel, 1,
        showing_primary ? TERM_L_BLUE : TERM_SLATE, showing_primary,
        "Primary");
    (void)app_ui_panel_add_tab(panel, 2,
        showing_primary ? TERM_SLATE : TERM_L_BLUE, !showing_primary,
        "Secondary");

    movement_binding_label(current_slot, binding_buf, sizeof(binding_buf));
    (void)settings_browser_add_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
        true, true, entry->label, binding_buf);
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Press the key chord now. Esc cancels. Backspace clears.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "Any key",
        "Bind");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Bksp",
        "Clear");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "Esc",
        "Cancel");

    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[panel->selected_row].id;
    }

    return ui_information_scene_present_ui(&scene);
}

static bool movement_present_ui_scene(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count,
    bool showing_primary, int highlight, int top, bool dirty, u16b preset_id,
    cptr config_label, cptr note)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    int i;
    char preset_buf[80];

    panel = settings_browser_scene_begin_ex(&scene, "Movement Settings",
        showing_primary
            ? "8/2 move  Enter bind  Tab secondary  p preset  Esc return"
            : "8/2 move  Enter bind  Tab primary  p preset  Esc return",
        1180, 2200);
    if (!panel)
        return false;

    (void)app_ui_panel_add_tab(panel, 1,
        showing_primary ? TERM_L_BLUE : TERM_SLATE, showing_primary,
        "Primary");
    (void)app_ui_panel_add_tab(panel, 2,
        showing_primary ? TERM_SLATE : TERM_L_BLUE, !showing_primary,
        "Secondary");

    if (top > 0)
        app_ui_panel_set_row_offset(panel, (s16b)top);

    for (i = 0; i < entry_count; i++)
    {
        char binding_buf[64];
        byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

        movement_binding_label(&slots[i][showing_primary
                ? MOVEMENT_SLOT_PRIMARY
                : MOVEMENT_SLOT_SECONDARY], binding_buf, sizeof(binding_buf));
        if (!settings_browser_add_pair_row(panel, (s16b)i, attr, TERM_SLATE,
                true, i == highlight, movement_settings[i].label, binding_buf))
        {
            return false;
        }
    }

    strnfmt(preset_buf, sizeof(preset_buf), "Preset: %s",
        movement_preset_label(preset_id));
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE, preset_buf);
    if (note && note[0])
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW, note);
    if (dirty)
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW, "Unsaved changes.");
    else
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
            config_label && config_label[0]
                ? format("Press 's' to save to %s.", config_label)
                : "Press 's' to save.");

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "8/2",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Tab",
        "Slot");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "Enter",
        "Bind");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "r",
        "Revert row");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "R",
        "Revert all");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true, "p",
        "Preset");
    (void)app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true, "s",
        "Save");
    (void)app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true, "Esc",
        "Back");

    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[panel->selected_row].id;
    }

    return ui_information_scene_present_ui(&scene);
}

static int movement_conflict_resolution_choice(cptr current_label,
    cptr conflict_label)
{
    settings_choice_entry entries[3];
    int highlight = 1;

    entries[0] = (settings_choice_entry){ 1, '1', "1) Swap bindings", false };
    entries[1] = (settings_choice_entry){ 2, '2', "2) Clear old binding", false };
    entries[2] = (settings_choice_entry){ 0, '3', "3) Cancel", false };

    msg_format("Binding conflict: %s already uses %s.", current_label,
        conflict_label);
    message_flush();
    return settings_choice_menu("Movement Binding Conflict", entries,
        (int)N_ELEMENTS(entries), &highlight, 0);
}

static int movement_preset_choice(u16b current_preset)
{
    settings_choice_entry entries[5];
    int highlight = 1;
    int idx;

    entries[0] = (settings_choice_entry){ APP_MOVEMENT_PRESET_MODERN_ARROWS,
        '1', "1) Modern Arrows", false };
    entries[1] = (settings_choice_entry){
        APP_MOVEMENT_PRESET_MODERN_WASD_QEZC, '2', "2) Modern WASD+QEZC",
        false };
    entries[2] = (settings_choice_entry){ APP_MOVEMENT_PRESET_VI_KEYS, '3',
        "3) Vi Keys", false };
    entries[3] = (settings_choice_entry){ APP_MOVEMENT_PRESET_CLASSIC_SIL, '4',
        "4) Classic Sil", false };
    entries[4] = (settings_choice_entry){ 0, '5', "5) Cancel", false };

    idx = settings_choice_find_index_by_id(entries, (int)N_ELEMENTS(entries),
        current_preset);
    if (idx >= 0)
        highlight = entries[idx].id;

    return settings_choice_menu("Movement Presets", entries,
        (int)N_ELEMENTS(entries), &highlight, 0);
}

static bool movement_save_to_config(
    const movement_slot_state slots[][MOVEMENT_SLOT_COUNT], int entry_count,
    u16b preset_id, cptr config_label)
{
    char missing[256];

    if (!movement_list_missing_essentials(slots, entry_count, missing,
            sizeof(missing)))
    {
        msg_format("Essential movement actions are unbound: %s", missing);
        message_flush();
        return false;
    }

    if (movement_has_any_conflicts(slots, entry_count))
    {
        msg_print("Movement bindings still conflict. Resolve conflicts before saving.");
        message_flush();
        return false;
    }

    movement_slot_states_to_config(slots, entry_count, preset_id, &config);
    if (!save_pane_config_to_json())
    {
        msg_print("Failed to save movement settings.");
        message_flush();
        return false;
    }

    msg_format("Movement settings saved to %s",
        (config_label && config_label[0]) ? config_label : "sil_sdl.json");
    message_flush();
    return true;
}

void do_cmd_keybinds(void)
{
    const int entry_count = (int)N_ELEMENTS(movement_settings);
    const int list_start_row = 5;
    movement_slot_state slots[N_ELEMENTS(movement_settings)][MOVEMENT_SLOT_COUNT];
    movement_slot_state baseline[N_ELEMENTS(movement_settings)][MOVEMENT_SLOT_COUNT];
    movement_slot_state preset_slots[N_ELEMENTS(movement_settings)][MOVEMENT_SLOT_COUNT];
    struct sdl_config working_config;
    bool dirty = false;
    bool done = false;
    bool showing_primary = true;
    int highlight = 0;
    int top = 0;
    u16b preset_id = config.movement_keyboard_preset;
    u16b baseline_preset = preset_id;
    char note[160];
    const char* config_label = settings_sdl_config_path();

    note[0] = '\0';
    memcpy(&working_config, &config, sizeof(working_config));

    if (!sdl_config_has_movement_bindings(&working_config))
    {
        sdl_config_set_default_movement_bindings(&working_config,
            APP_MOVEMENT_PRESET_CLASSIC_SIL);
        SDL_strlcpy(note,
            "Initialized Classic Sil bindings. Save to persist them.",
            sizeof(note));
        preset_id = working_config.movement_keyboard_preset;
        dirty = true;
    }

    movement_slot_states_from_config(&working_config, slots, entry_count);
    memcpy(baseline, slots, sizeof(baseline));
    baseline_preset = preset_id;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout, list_start_row,
            8, 5);
        char ch;

        movement_adjust_view(entry_count, visible_rows, &highlight, &top);
        if (!movement_present_ui_scene(slots, entry_count, showing_primary,
                highlight, top, dirty, preset_id, config_label, note))
        {
            done = true;
            continue;
        }

        ch = settings_ui_read_key(false);
        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            if (!dirty)
            {
                done = true;
                continue;
            }

            if (movement_save_to_config(slots, entry_count, preset_id,
                    config_label))
            {
                memcpy(baseline, slots, sizeof(baseline));
                baseline_preset = preset_id;
                dirty = false;
                done = true;
            }
            else if (get_check("Discard unsaved movement settings? "))
            {
                done = true;
            }
        }
        else if (ch == '\t')
        {
            showing_primary = !showing_primary;
        }
        else if (ch == '8')
        {
            highlight = (highlight + entry_count - 1) % entry_count;
        }
        else if (ch == '2')
        {
            highlight = (highlight + 1) % entry_count;
        }
        else if (ch == 'r')
        {
            slots[highlight][MOVEMENT_SLOT_PRIMARY]
                = baseline[highlight][MOVEMENT_SLOT_PRIMARY];
            slots[highlight][MOVEMENT_SLOT_SECONDARY]
                = baseline[highlight][MOVEMENT_SLOT_SECONDARY];
            dirty = true;
            note[0] = '\0';
        }
        else if (ch == 'R')
        {
            memcpy(slots, baseline, sizeof(slots));
            preset_id = baseline_preset;
            dirty = false;
            note[0] = '\0';
        }
        else if (ch == 'p' || ch == 'P')
        {
            int choice = movement_preset_choice(preset_id);

            if (choice != 0)
            {
                struct sdl_config preset_config;

                memset(&preset_config, 0, sizeof(preset_config));
                sdl_config_set_default_movement_bindings(&preset_config,
                    (u16b)choice);
                movement_slot_states_from_config(&preset_config, preset_slots,
                    entry_count);
                memcpy(slots, preset_slots, sizeof(slots));
                preset_id = (u16b)choice;
                dirty = true;
                note[0] = '\0';
            }
        }
        else if (ch == 's' || ch == 'S')
        {
            if (movement_save_to_config(slots, entry_count, preset_id,
                    config_label))
            {
                memcpy(baseline, slots, sizeof(baseline));
                baseline_preset = preset_id;
                dirty = false;
                note[0] = '\0';
            }
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            app_movement_binding candidate;
            movement_capture_result capture_result;
            int selected_slot = showing_primary ? MOVEMENT_SLOT_PRIMARY
                : MOVEMENT_SLOT_SECONDARY;
            int conflict_entry = -1;
            int conflict_slot = -1;
            int conflict_choice = 0;
            char prompt[96];
            char prompt_long[128];
            char prompt_medium[112];
            char prompt_short[96];

            strnfmt(prompt_long, sizeof(prompt_long),
                "Press key chord for %s (%s slot):",
                movement_settings[highlight].label,
                showing_primary ? "primary" : "secondary");
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "Bind %s (%s slot):", movement_settings[highlight].label,
                showing_primary ? "primary" : "secondary");
            strnfmt(prompt_short, sizeof(prompt_short), "%s (%s):",
                movement_settings[highlight].label,
                showing_primary ? "primary" : "secondary");
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(layout.inset_prompt_line_chars,
                    prompt_long, prompt_medium, prompt_short));

            (void)movement_present_prompt_scene(showing_primary,
                &movement_settings[highlight], &slots[highlight][selected_slot],
                prompt);

            capture_result = movement_capture_binding(&movement_settings[highlight],
                &candidate);
            if (capture_result == MOVEMENT_CAPTURE_CANCEL)
                continue;

            if (capture_result == MOVEMENT_CAPTURE_CLEAR)
            {
                slots[highlight][selected_slot].in_use = false;
                app_movement_binding_clear(
                    &slots[highlight][selected_slot].binding);
                dirty = true;
                preset_id = APP_MOVEMENT_PRESET_NONE;
                note[0] = '\0';
                continue;
            }

            if (movement_find_conflict(slots, entry_count, highlight,
                    selected_slot, &candidate, &conflict_entry, &conflict_slot))
            {
                conflict_choice = movement_conflict_resolution_choice(
                    movement_settings[highlight].label,
                    movement_settings[conflict_entry].label);
                if (conflict_choice == 0)
                    continue;
                if (conflict_choice == 1)
                {
                    movement_slot_state temp = slots[highlight][selected_slot];
                    slots[conflict_entry][conflict_slot] = temp;
                }
                else if (conflict_choice == 2)
                {
                    slots[conflict_entry][conflict_slot].in_use = false;
                    app_movement_binding_clear(
                        &slots[conflict_entry][conflict_slot].binding);
                }
            }

            slots[highlight][selected_slot].binding = candidate;
            slots[highlight][selected_slot].in_use = true;
            dirty = true;
            preset_id = APP_MOVEMENT_PRESET_NONE;
            note[0] = '\0';
        }
    }
}


#ifdef ALLOW_MACROS
/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
    static cptr mark = "Macro Dump";

    int i;

    ang_file* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("macro_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old macros */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = ang_file_open_compat(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    ang_file_printf_compat(fff, "\n\n");

    /* Start dumping */
    ang_file_printf_compat(fff, "# Automatic macro dump\n\n");

    /* Dump them */
    for (i = 0; i < macro__num; i++)
    {
        /* Start the macro */
        ang_file_printf_compat(fff, "# Macro '%d'\n\n", i);

        /* Extract the macro action */
        ascii_to_text(buf, sizeof(buf), macro__act[i]);

        /* Dump the macro action */
        ang_file_printf_compat(fff, "A:%s\n", buf);

        /* Extract the macro pattern */
        ascii_to_text(buf, sizeof(buf), macro__pat[i]);

        /* Dump the macro pattern */
        ang_file_printf_compat(fff, "P:%s\n", buf);

        /* End the macro */
        ang_file_printf_compat(fff, "\n\n");
    }

    /* Start dumping */
    ang_file_printf_compat(fff, "\n\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    (void)ang_file_close_compat(fff);

    /* Success */
    return (0);
}
/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Clear pending input before and after capture so the trigger buffer only
 * sees the keys entered for the current binding.
 */
void do_cmd_macro_aux(char* buf)
{
    char ch;

    int n = 0;

    input_clear_pending();

    /* First key */
    ch = settings_ui_read_key(false);

    /* Read the pattern */
    while (ch != '\0')
    {
        /* Save the key */
        buf[n++] = ch;

        /* Attempt to read a key */
        ch = settings_ui_read_key(true);
    }

    /* Terminate */
    buf[n] = '\0';

    input_clear_pending();
}
/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Clear pending input before and after capture so the keymap trigger reads
 * exactly one fresh keypress.
 */
void do_cmd_macro_aux_keymap(char* buf)
{
    input_clear_pending();

    /* Get a key */
    buf[0] = settings_ui_read_key(false);
    buf[1] = '\0';

    input_clear_pending();
}
/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
    static cptr mark = "Keymap Dump";

    int i;

    ang_file* fff;

    char buf[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("keymap_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old keymaps */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = ang_file_open_compat(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    ang_file_printf_compat(fff, "\n\n");

    /* Start dumping */
    ang_file_printf_compat(fff, "# Automatic keymap dump\n\n");

    /* Dump them */
    for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
    {
        char key[2] = "?";

        cptr act;

        /* Loop up the keymap */
        act = keymap_act[mode][i];

        /* Skip empty keymaps */
        if (!act)
            continue;

        /* Encode the action */
        ascii_to_text(buf, sizeof(buf), act);

        /* Dump the keymap action */
        ang_file_printf_compat(fff, "A:%s\n", buf);

        /* Convert the key into a string */
        key[0] = i;

        /* Encode the key */
        ascii_to_text(buf, sizeof(buf), key);

        /* Dump the keymap pattern */
        ang_file_printf_compat(fff, "C:%d:%s\n", mode, buf);

        /* Skip a line */
        ang_file_printf_compat(fff, "\n");
    }

    /* Skip some lines */
    ang_file_printf_compat(fff, "\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    (void)ang_file_close_compat(fff);

    /* Success */
    return (0);
}

#endif

/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
    int choice = 0;
    int highlight = 1;
    char tmp[1024];
    char pat[1024];
    char key_desc[128];
    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Process requests until done */
    while (1)
    {
        choice = macros_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(0);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (choice == 2)
        {
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!settings_ui_prompt_string("Interact with Macros",
                    "Enter the file name to append macros to.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)macro_dump(ftmp);

            /* Prompt */
            msg_print("Appended macros.");
        }

        /* Query a macro */
        else if (choice == 3)
        {
            int k;

            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to query.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Get the action */
            k = macro_find_exact(pat);

            /* Nothing found */
            if (k < 0)
            {
                /* Prompt */
                msg_print("Found no macro.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);
                settings_ui_show_text_value("Interact with Macros",
                    "Found a macro.", key_desc, "Action", tmp);
            }
        }

        /* Create a macro */
        else if (choice == 4)
        {
            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to create.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text for this macro.", key_desc,
                    tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Link the macro */
                macro_add(pat, macro_buffer);

                /* Prompt */
                msg_print("Added a macro.");
            }
        }

        /* Remove a macro */
        else if (choice == 5)
        {
            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to remove.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Link the macro */
            macro_add(pat, pat);

            /* Prompt */
            msg_print("Removed a macro.");
        }

        /* Save keymaps */
        else if (choice == 6)
        {
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!settings_ui_prompt_string("Interact with Macros",
                    "Enter the file name to append keymaps to.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)keymap_dump(ftmp);

            /* Prompt */
            msg_print("Appended keymaps.");
        }

        /* Query a keymap */
        else if (choice == 7)
        {
            cptr act;

            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to query.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Look up the keymap */
            act = keymap_act[mode][(byte)(pat[0])];

            /* Nothing found */
            if (!act)
            {
                /* Prompt */
                msg_print("Found no keymap.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, act, sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);
                settings_ui_show_text_value("Interact with Macros",
                    "Found a keymap.", key_desc, "Action", tmp);
            }
        }

        /* Create a keymap */
        else if (choice == 8)
        {
            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to create.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text for this keymap.", key_desc,
                    tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Free old keymap */
                str_free(keymap_act[mode][(byte)(pat[0])]);

                /* Make new keymap */
                keymap_act[mode][(byte)(pat[0])] = str_dup(macro_buffer);

                /* Prompt */
                msg_print("Added a keymap.");
            }
        }

        /* Remove a keymap */
        else if (choice == 9)
        {
            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to remove.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Free old keymap */
            str_free(keymap_act[mode][(byte)(pat[0])]);

            /* Make new keymap */
            keymap_act[mode][(byte)(pat[0])] = NULL;

            /* Prompt */
            msg_print("Removed a keymap.");
        }

        /* Enter a new action */
        else if (choice == 10)
        {
            /* Analyze the current action */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text.", "", tmp, 80))
            {
                /* Extract an action */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
            }
        }

#endif /* ALLOW_MACROS */

        /* Oops */
        else
        {
            /* Oops */
            bell("Illegal command for macros!");
        }

        /* Flush messages */
        message_flush();
    }
}
