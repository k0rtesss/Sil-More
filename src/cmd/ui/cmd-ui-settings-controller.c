/* File: cmd-ui-settings-controller.c */

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

typedef enum controller_entry_type {
    CONTROLLER_ENTRY_TOGGLE = 0,
    CONTROLLER_ENTRY_ACTION,
} controller_entry_type;

typedef enum controller_toggle_id {
    CONTROLLER_TOGGLE_ENABLED = 0,
    CONTROLLER_TOGGLE_AUTO_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_MODE,
    CONTROLLER_TOGGLE_DPAD,
    CONTROLLER_TOGGLE_LEFT_STICK,
} controller_toggle_id;

typedef struct controller_entry {
    controller_entry_type type;
    int id;
    const char* label;
} controller_entry;

typedef struct controller_binding_spec {
    int slot_count;
    settings_indexed_int_getter_fn get;
    settings_indexed_int_setter_fn set;
    settings_indexed_int_getter_fn get_default;
} controller_binding_spec;

static int controller_get_shoulder_combo_binding(int id)
{
    (void)id;
    return SETTINGS_SDL_GET(gamepad_shoulder_combo_binding)();
}

static void controller_set_shoulder_combo_binding(int id, int binding)
{
    (void)id;
    SETTINGS_SDL_SET(gamepad_shoulder_combo_binding)(binding);
}

static int controller_get_default_button_binding(int id)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    if (id < 0 || id >= GAMEPAD_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    return defaults->gamepad_button_bindings[id];
}

static int controller_get_default_trigger_binding(int id)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    if (id < 0 || id >= GAMEPAD_TRIGGER_COUNT)
        return GAMEPAD_BIND_NONE;

    return defaults->gamepad_trigger_bindings[id];
}

static int controller_get_default_left_stick_binding(int id)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    if (id < 0 || id >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;

    return defaults->gamepad_left_stick_bindings[id];
}

static int controller_get_default_right_stick_binding(int id)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    if (id < 0 || id >= GAMEPAD_STICK_DIR_COUNT)
        return GAMEPAD_BIND_NONE;

    return defaults->gamepad_right_stick_bindings[id];
}

static int controller_get_default_shoulder_combo_binding(int id)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    (void)id;
    return defaults->gamepad_shoulder_combo_binding;
}

static const settings_bool_binding controller_toggle_bindings[] = {
    [CONTROLLER_TOGGLE_ENABLED]
        = { SETTINGS_SDL_GET(gamepad_enabled),
            SETTINGS_SDL_SET(gamepad_enabled) },
    [CONTROLLER_TOGGLE_AUTO_MODE]
        = { SETTINGS_SDL_GET(gamepad_auto_mode),
            SETTINGS_SDL_SET(gamepad_auto_mode) },
    [CONTROLLER_TOGGLE_STEAMDECK_MODE]
        = { SETTINGS_SDL_GET(steamdeck_mode),
            SETTINGS_SDL_SET(steamdeck_mode) },
    [CONTROLLER_TOGGLE_DPAD]
        = { SETTINGS_SDL_GET(gamepad_use_dpad),
            SETTINGS_SDL_SET(gamepad_use_dpad) },
    [CONTROLLER_TOGGLE_LEFT_STICK]
        = { SETTINGS_SDL_GET(gamepad_use_left_stick),
            SETTINGS_SDL_SET(gamepad_use_left_stick) },
};

static const controller_binding_spec controller_binding_specs[] = {
    [GAMEPAD_CAPTURE_BUTTON]
        = { GAMEPAD_BUTTON_COUNT, SETTINGS_SDL_GET(gamepad_button_binding),
            SETTINGS_SDL_SET(gamepad_button_binding),
            controller_get_default_button_binding },
    [GAMEPAD_CAPTURE_TRIGGER]
        = { GAMEPAD_TRIGGER_COUNT, SETTINGS_SDL_GET(gamepad_trigger_binding),
            SETTINGS_SDL_SET(gamepad_trigger_binding),
            controller_get_default_trigger_binding },
    [GAMEPAD_CAPTURE_LEFT_STICK]
        = { GAMEPAD_STICK_DIR_COUNT,
            SETTINGS_SDL_GET(gamepad_left_stick_binding),
            SETTINGS_SDL_SET(gamepad_left_stick_binding),
            controller_get_default_left_stick_binding },
    [GAMEPAD_CAPTURE_RIGHT_STICK]
        = { GAMEPAD_STICK_DIR_COUNT,
            SETTINGS_SDL_GET(gamepad_right_stick_binding),
            SETTINGS_SDL_SET(gamepad_right_stick_binding),
            controller_get_default_right_stick_binding },
    [GAMEPAD_CAPTURE_SHOULDER_COMBO]
        = { 1, controller_get_shoulder_combo_binding,
            controller_set_shoulder_combo_binding,
            controller_get_default_shoulder_combo_binding },
};

static bool controller_toggle_value(int toggle_id)
{
    if (toggle_id < 0
        || toggle_id >= (int)N_ELEMENTS(controller_toggle_bindings)
        || !controller_toggle_bindings[toggle_id].get)
    {
        return false;
    }

    return controller_toggle_bindings[toggle_id].get();
}

static int controller_binding_slot_count(int type)
{
    if (type < 0 || type >= (int)N_ELEMENTS(controller_binding_specs))
        return 0;

    return controller_binding_specs[type].slot_count;
}

static int controller_binding_value(int type, int id)
{
    if (type < 0 || type >= (int)N_ELEMENTS(controller_binding_specs)
        || !controller_binding_specs[type].get)
    {
        return GAMEPAD_BIND_NONE;
    }

    return controller_binding_specs[type].get(id);
}

static void controller_set_binding_value(int type, int id, int binding)
{
    if (type < 0 || type >= (int)N_ELEMENTS(controller_binding_specs)
        || !controller_binding_specs[type].set)
    {
        return;
    }

    controller_binding_specs[type].set(id, binding);
}

static int controller_default_binding_value(int type, int id)
{
    if (type < 0 || type >= (int)N_ELEMENTS(controller_binding_specs)
        || !controller_binding_specs[type].get_default)
    {
        return GAMEPAD_BIND_NONE;
    }

    return controller_binding_specs[type].get_default(id);
}

static const char* controller_gamepad_button_label(int button)
{
    switch (button) {
    case GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case GAMEPAD_BUTTON_EAST: return "B (East)";
    case GAMEPAD_BUTTON_WEST: return "X (West)";
    case GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case GAMEPAD_BUTTON_START: return "Start (Menu)";
    case GAMEPAD_BUTTON_BACK: return "Back (View)";
    case GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case GAMEPAD_BUTTON_MISC1: return "Misc1";
    case GAMEPAD_BUTTON_MISC2: return "Misc2";
    case GAMEPAD_BUTTON_MISC3: return "Misc3";
    case GAMEPAD_BUTTON_MISC4: return "Misc4";
    case GAMEPAD_BUTTON_MISC5: return "Misc5";
    case GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* controller_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* controller_gamepad_stick_dir_label(int type, int dir)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* dir_label = NULL;

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; break;
    default: dir_label = "Unknown"; break;
    }

    return format("%s %s", stick, dir_label);
}

static const char* controller_gamepad_combo_label(void)
{
    return "L1+R1 Combo";
}

static void controller_binding_label(int type, int id, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, controller_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, controller_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, controller_gamepad_stick_dir_label(type, id), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, controller_gamepad_combo_label(), buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static int controller_action_binding_count(int binding, int* out_type, int* out_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };
    int count = 0;

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_binding_value(type, i) != binding)
                continue;
            if (count == 0 && out_type && out_id) {
                *out_type = type;
                *out_id = i;
            }
            count++;
        }
    }

    return count;
}

static void controller_action_binding_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = controller_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        controller_binding_label(type, id, buf, buflen);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

static bool controller_binding_matches_action(int binding, int type, int id)
{
    return controller_binding_value(type, id) == binding;
}

void controller_prompt_label(int binding, const char* default_label, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    platform_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple")) {
        SDL_strlcpy(buf, default_label, buflen);
    }
}

static void controller_entry_value(const controller_entry* entry, char* buf, size_t buflen)
{
    if (!entry || !buf || !buflen)
        return;

    switch (entry->type) {
    case CONTROLLER_ENTRY_TOGGLE:
        if (entry->id < CONTROLLER_TOGGLE_ENABLED
            || entry->id > CONTROLLER_TOGGLE_LEFT_STICK)
        {
            SDL_strlcpy(buf, "(unknown)", buflen);
        }
        else
        {
            SDL_strlcpy(buf, controller_toggle_value(entry->id) ? "On" : "Off",
                buflen);
        }
        break;
    case CONTROLLER_ENTRY_ACTION:
        controller_action_binding_label(entry->id, buf, buflen);
        break;
    default:
        SDL_strlcpy(buf, "(unknown)", buflen);
        break;
    }
}

static void controller_set_toggle(int toggle_id, bool value)
{
    if (toggle_id < 0
        || toggle_id >= (int)N_ELEMENTS(controller_toggle_bindings)
        || !controller_toggle_bindings[toggle_id].set)
    {
        return;
    }

    controller_toggle_bindings[toggle_id].set(value);
}

static void controller_clear_action_bindings(int binding, int skip_type, int skip_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };

    if (binding == GAMEPAD_BIND_NONE)
        return;

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_binding_value(type, i) != binding)
                continue;
            if (skip_type == type && skip_id == i)
                continue;
            controller_set_binding_value(type, i, GAMEPAD_BIND_NONE);
        }
    }
}

static void controller_assign_action_binding(int binding, int type, int id)
{
    controller_clear_action_bindings(binding, type, id);
    controller_set_binding_value(type, id, binding);
}

static bool controller_action_default_binding(int binding, int* out_type, int* out_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_default_binding_value(type, i) != binding)
                continue;
            if (out_type)
                *out_type = type;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    return false;
}

void do_cmd_controller_settings(void)
{
    bool done = false;
    int highlight = 0;
    int top = 0;
    const int list_start_row = 5;

    static const controller_entry entries[] = {
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_ENABLED, "Controller Input" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_AUTO_MODE, "Auto Controller Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_MODE, "Steam Deck UI Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_DPAD, "D-pad Movement" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_LEFT_STICK, "Left Stick Movement" },
        { CONTROLLER_ENTRY_ACTION, ' ', "Confirm (Space)" },
        { CONTROLLER_ENTRY_ACTION, '\r', "Enter" },
        { CONTROLLER_ENTRY_ACTION, ESCAPE, "Escape" },
        { CONTROLLER_ENTRY_ACTION, '\t', "Abilities (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire (primary)" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire (secondary)" },
        { CONTROLLER_ENTRY_ACTION, 'l', "Look around" },
        { CONTROLLER_ENTRY_ACTION, 'T', "Tunnel / dig" },
        { CONTROLLER_ENTRY_ACTION, 'b', "Bash door" },
        { CONTROLLER_ENTRY_ACTION, 'z', "Wait" },
        { CONTROLLER_ENTRY_ACTION, 'j', "Supplies overview" },
        { CONTROLLER_ENTRY_ACTION, '.', "Run" },
        { CONTROLLER_ENTRY_ACTION, '/', "Alt action" },
        { CONTROLLER_ENTRY_ACTION, 'w', "Wear / wield" },
        { CONTROLLER_ENTRY_ACTION, 'r', "Remove equipment" },
        { CONTROLLER_ENTRY_ACTION, 'd', "Drop item" },
        { CONTROLLER_ENTRY_ACTION, 'k', "Destroy item" },
        { CONTROLLER_ENTRY_ACTION, 'g', "Pick up items" },
        { CONTROLLER_ENTRY_ACTION, 'Z', "Rest" },
        { CONTROLLER_ENTRY_ACTION, 'o', "Open door / chest" },
        { CONTROLLER_ENTRY_ACTION, 'c', "Close door" },
        { CONTROLLER_ENTRY_ACTION, 'D', "Disarm trap / chest" },
        { CONTROLLER_ENTRY_ACTION, 'X', "Exchange places" },
        { CONTROLLER_ENTRY_ACTION, '-', "Fletch arrows" },
        { CONTROLLER_ENTRY_ACTION, '{', "Inscribe item" },
        { CONTROLLER_ENTRY_ACTION, 'a', "Activate staff" },
        { CONTROLLER_ENTRY_ACTION, 'E', "Eat food" },
        { CONTROLLER_ENTRY_ACTION, 't', "Throw item" },
        { CONTROLLER_ENTRY_ACTION, 'p', "Blow horn" },
        { CONTROLLER_ENTRY_ACTION, 'q', "Quaff potion" },
        { CONTROLLER_ENTRY_ACTION, 'M', "View map" },
        { CONTROLLER_ENTRY_ACTION, 'L', "Pan view" },
        { CONTROLLER_ENTRY_ACTION, '0', "Smithing screen" },
        { CONTROLLER_ENTRY_ACTION, '<', "Go upstairs" },
        { CONTROLLER_ENTRY_ACTION, '>', "Go downstairs" },
        { CONTROLLER_ENTRY_ACTION, 'm', "Main menu" },
        { CONTROLLER_ENTRY_ACTION, '?', "Help" },
        { CONTROLLER_ENTRY_ACTION, 'O', "Options menu" },
        { CONTROLLER_ENTRY_ACTION, ':', "Take notes" },
        { CONTROLLER_ENTRY_ACTION, '~', "Knowledge browser" },
        { CONTROLLER_ENTRY_ACTION, '[', "Monster list" },
        { CONTROLLER_ENTRY_ACTION, ']', "Object list" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_SHIFT, "Shift modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_CTRL, "Ctrl modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_ALT, "Alt modifier" },
    };

    int entry_count = (int)N_ELEMENTS(entries);

    while (!done) {
        settings_ui_layout layout = settings_ui_read_layout();
        bool steamdeck = steamdeck_controls_active();
        bool compact_width;
        int row_width;

        row_width = layout.inset_prompt_line_chars;
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 5);
        compact_width = layout.compact;

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;
        if (entry_count > visible_rows) {
            int max_top = entry_count - visible_rows;
            if (top > max_top)
                top = max_top;
        } else {
            top = 0;
        }

        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            "Controller Settings", "", 1180, 2200);

        if (!panel) {
            done = true;
            continue;
        }

        if (steamdeck) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];

            controller_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            controller_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "D-pad %s bind  %s back"
                              : "D-pad navigate  %s bind  %s back",
                confirm_label, back_label);
            app_ui_panel_set_subtitle(panel, TERM_SLATE, prompt_buf);
        } else {
            app_ui_panel_set_subtitle(panel, TERM_SLATE,
                compact_width ? "8/2 move  Enter bind  Esc return"
                              : "Arrow to navigate, Enter to bind, Escape to return");
        }

        if (top > 0)
            app_ui_panel_set_row_offset(panel, (s16b)top);

        for (int i = 0; i < entry_count; i++) {
            char value_buf[64];
            byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            controller_entry_value(&entries[i], value_buf,
                sizeof(value_buf));
            if (!settings_browser_add_pair_row(panel, (s16b)i, attr,
                    TERM_SLATE, true, i == highlight, entries[i].label,
                    value_buf))
            {
                done = true;
                break;
            }
        }

        if (steamdeck) {
            char reset_label[16];
            char reset_all_label[16];
            char prompt_buf[80];

            controller_prompt_label(steamdeck_alt_action_key(), "X",
                reset_label, sizeof(reset_label));
            controller_prompt_label(steamdeck_secondary_key(), "Y",
                reset_all_label, sizeof(reset_all_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "[%s] reset  [%s] reset all"
                              : "Reset: [%s] selected, [%s] all",
                reset_label, reset_all_label);
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE, prompt_buf);
        } else {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                compact_width ? "r: reset selected  R: reset all"
                              : "Press 'r' to reset selected binding, 'R' to reset all bindings");
        }
        (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
            compact_width ? "Saves on exit."
                          : "Changes are saved on exit.");
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Enter", "Bind");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "r", "Reset");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "R", "Reset all");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "Esc", "Back");

        if (!done && !ui_information_scene_present_ui(&scene)) {
            done = true;
            continue;
        }

        char ch = settings_ui_read_key(false);

        if (ch == ESCAPE || ch == 'q' || ch == 'Q' || (steamdeck && ch == steamdeck_back_key())) {
            done = true;
        } else if (ch == '8') {
            highlight = (highlight + entry_count - 1) % entry_count;
        } else if (ch == '2') {
            highlight = (highlight + 1) % entry_count;
        } else if (ch == 'r' || (steamdeck && ch == steamdeck_alt_action_key())) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                int binding_type = 0;
                int binding_id = 0;
                if (controller_action_default_binding(entries[highlight].id, &binding_type, &binding_id)) {
                    controller_assign_action_binding(entries[highlight].id, binding_type, binding_id);
                    msg_print("Binding reset to default.");
                } else {
                    controller_clear_action_bindings(entries[highlight].id, -1, -1);
                    msg_print("No default binding for action.");
                }
                message_flush();
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            platform_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];

            if (entry->type == CONTROLLER_ENTRY_TOGGLE) {
                char cur[16];
                controller_entry_value(entry, cur, sizeof(cur));
                controller_set_toggle(entry->id, streq(cur, "Off"));
            } else {
                char prompt[80];
                char prompt_long[96];
                char prompt_medium[80];
                char prompt_short[64];
                int cap_type = 0;
                int cap_id = 0;
                if (steamdeck) {
                    char cancel_label[16];
                    controller_prompt_label(steamdeck_back_key(), "B", cancel_label, sizeof(cancel_label));
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Press button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Bind %s  (%s cancel)", entry->label, cancel_label);
                } else {
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s (Esc=cancel, Backspace=clear)",
                        entry->label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Bind %s (Esc=cancel, Bksp=clear)", entry->label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s (Esc cancel, Bksp clear)", entry->label);
                }
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                        prompt_short));
                {
                    app_ui_scene prompt_scene;
                    app_ui_panel* prompt_panel = settings_browser_scene_begin_ex(
                        &prompt_scene, "Controller Settings", prompt, 1100,
                        2200);

                    if (prompt_panel) {
                        char current_value[64];

                        controller_entry_value(entry, current_value,
                            sizeof(current_value));
                        (void)settings_browser_add_pair_row(prompt_panel, 0,
                            TERM_L_BLUE, TERM_SLATE, true, true, entry->label,
                            current_value);
                        (void)app_ui_panel_add_body_line(prompt_panel,
                            TERM_SLATE, steamdeck
                                ? "Press the controller input now."
                                : "Esc cancels. Backspace clears.");
                        (void)ui_information_scene_present_ui(&prompt_scene);
                    }
                }

                input_clear_pending();
                if (!platform_gamepad_capture_begin()) {
                    msg_print("No controller detected.");
                    message_flush();
                    continue;
                }

                bool waiting = true;
                while (waiting) {
                    if (platform_gamepad_capture_poll(&cap_type, &cap_id)) {
                        if (controller_binding_matches_action(ESCAPE, cap_type, cap_id)) {
                            platform_gamepad_capture_cancel();
                            waiting = false;
                            break;
                        }
                        controller_assign_action_binding(entry->id, cap_type, cap_id);
                        waiting = false;
                        break;
                    }

                    char choice = settings_ui_read_key(true);
                    if (choice == ESCAPE) {
                        platform_gamepad_capture_cancel();
                        waiting = false;
                    } else if (choice == '\b' || choice == 127) {
                        platform_gamepad_capture_cancel();
                        controller_clear_action_bindings(entry->id, -1, -1);
                        waiting = false;
                    } else if (choice == 0) {
                        platform_delay_ms(10);
                    }
                }
            }
        }
    }
}

