#include "angband.h"
#include "sdl-main-internal.h"

static const char* sdl_gamepad_button_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case SDL_GAMEPAD_BUTTON_EAST: return "B (East)";
    case SDL_GAMEPAD_BUTTON_WEST: return "X (West)";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case SDL_GAMEPAD_BUTTON_START: return "Start (Menu)";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back (View)";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* sdl_gamepad_button_short_label(int button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "A";
    case SDL_GAMEPAD_BUTTON_EAST: return "B";
    case SDL_GAMEPAD_BUTTON_WEST: return "X";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Y";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5";
    case SDL_GAMEPAD_BUTTON_START: return "Start";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L3";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R3";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Misc1";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Misc2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Misc3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Misc4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Misc5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "?";
    }
}

static const char* sdl_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* sdl_gamepad_trigger_short_label(int index)
{
    if (index == 0)
        return "L2";
    if (index == 1)
        return "R2";
    return "?";
}

static const char* sdl_gamepad_stick_dir_label(int type, int dir, bool short_label)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* stick_short = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "RS" : "LS";
    const char* dir_label = "";
    const char* dir_short = "";

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; dir_short = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; dir_short = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; dir_short = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; dir_short = "Right"; break;
    default: return short_label ? "?" : "Unknown Stick";
    }

    if (short_label)
        return format("%s %s", stick_short, dir_short);
    return format("%s %s", stick, dir_label);
}

static void sdl_gamepad_binding_label_ex(int type, int id, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, short_label ? sdl_gamepad_button_short_label(id)
                                     : sdl_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, short_label ? sdl_gamepad_trigger_short_label(id)
                                     : sdl_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, sdl_gamepad_stick_dir_label(type, id, short_label), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, short_label ? "L1+R1" : "L1+R1 Combo", buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static int sdl_gamepad_action_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (config.gamepad_button_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (config.gamepad_trigger_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (config.gamepad_left_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (config.gamepad_right_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (config.gamepad_shoulder_combo_binding == binding) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

static void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = sdl_gamepad_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        sdl_gamepad_binding_label_ex(type, id, buf, buflen, short_label);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

void platform_gamepad_action_binding_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, false);
}

void platform_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, true);
}

static void sdl_input_key_prompt_label(int action_key, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (!action_key)
        return;

    switch (action_key)
    {
    case ESCAPE:
        SDL_strlcpy(buf, "Esc", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case ' ':
        SDL_strlcpy(buf, "Space", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Tab", buflen);
        return;
    default:
        break;
    }

    if (action_key >= 32 && action_key <= 126)
    {
        if (buflen < 2)
            return;
        buf[0] = (char)action_key;
        buf[1] = '\0';
    }
}

static void sdl_gamepad_ui_action_prompt_label(u16b action, int action_key,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (action == APP_UI_WIDGET_ACTION_CANCEL || action_key == ESCAPE)
    {
        SDL_strlcpy(buf, "B", buflen);
        return;
    }
    if (action == APP_UI_WIDGET_ACTION_INSPECT || action_key == 'x'
        || action_key == 'X' || action_key == '?')
    {
        SDL_strlcpy(buf, "X", buflen);
        return;
    }
    if (action == APP_UI_WIDGET_ACTION_SCROLL || action_key == '2'
        || action_key == '4' || action_key == '6' || action_key == '8')
    {
        SDL_strlcpy(buf, "D-pad", buflen);
        return;
    }
    if (action == APP_UI_WIDGET_ACTION_SELECT
        || action == APP_UI_WIDGET_ACTION_ACTIVATE
        || action_key == '\r' || action_key == ' ')
    {
        SDL_strlcpy(buf, "A", buflen);
        return;
    }
    if (action == APP_UI_WIDGET_ACTION_DRAG
        || action == APP_UI_WIDGET_ACTION_RESIZE)
    {
        SDL_strlcpy(buf, "Hold", buflen);
        return;
    }

    sdl_input_key_prompt_label(action_key, buf, buflen);
}

void platform_input_prompt_for_ui_action(u16b device, u16b action,
    int action_key, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    switch (device)
    {
    case APP_INPUT_DEVICE_GAMEPAD:
        sdl_gamepad_ui_action_prompt_label(action, action_key, buf, buflen);
        return;

    case APP_INPUT_DEVICE_POINTER:
        if (action == APP_UI_WIDGET_ACTION_SCROLL)
            SDL_strlcpy(buf, "Wheel", buflen);
        else if (action == APP_UI_WIDGET_ACTION_DRAG
            || action == APP_UI_WIDGET_ACTION_RESIZE)
            SDL_strlcpy(buf, "Drag", buflen);
        else
            SDL_strlcpy(buf, "Click", buflen);
        return;

    case APP_INPUT_DEVICE_TOUCH:
        if (action == APP_UI_WIDGET_ACTION_INSPECT)
            SDL_strlcpy(buf, "Hold", buflen);
        else if (action == APP_UI_WIDGET_ACTION_SCROLL)
            SDL_strlcpy(buf, "Swipe", buflen);
        else
            SDL_strlcpy(buf, "Tap", buflen);
        return;

    default:
        sdl_input_key_prompt_label(action_key, buf, buflen);
        return;
    }
}

int steamdeck_back_key(void)
{
    return platform_gamepad_button_binding(SDL_GAMEPAD_BUTTON_EAST);
}

int steamdeck_confirm_key(void)
{
    return platform_gamepad_button_binding(SDL_GAMEPAD_BUTTON_SOUTH);
}

int steamdeck_info_key(void)
{
    return platform_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_RIGHT);
}

int steamdeck_alt_action_key(void)
{
    return platform_gamepad_button_binding(SDL_GAMEPAD_BUTTON_WEST);
}

int steamdeck_secondary_key(void)
{
    return platform_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH);
}
