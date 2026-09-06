#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "support/input.h"
#include "ui/menu-click.h"

static SDL_JoystickID g_active_gamepad_id;
static int g_gamepad_context_focus_kind = SDL_CONTROLLER_FOCUS_NONE;
static int g_gamepad_context_focus_id = -1;
static bool g_gamepad_focus_chord_pending;
static bool g_gamepad_focus_chord_used;
static int g_gamepad_button_modifiers[SDL_GAMEPAD_BUTTON_COUNT];
static int g_gamepad_trigger_modifiers[GAMEPAD_TRIGGER_COUNT];
static bool g_gamepad_right_binding_modifier_active;

void sdl_gamepad_release_button_modifier(int button)
{
    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT
        && g_gamepad_button_modifiers[button])
    {
        sdl_gamepad_apply_modifier(g_gamepad_button_modifiers[button], false);
        g_gamepad_button_modifiers[button] = 0;
    }
}

void sdl_gamepad_reset_modifiers(void)
{
    memset(g_gamepad_button_modifiers, 0, sizeof(g_gamepad_button_modifiers));
    memset(g_gamepad_trigger_modifiers, 0, sizeof(g_gamepad_trigger_modifiers));
    g_gamepad_right_binding_modifier_active = false;
    g_gamepad_state.shift_held = 0;
    g_gamepad_state.ctrl_held = 0;
    g_gamepad_state.alt_held = 0;
    g_gamepad_focus_chord_pending = false;
    g_gamepad_focus_chord_used = false;
}

bool sdl_gamepad_shift_active(void)
{
    return g_gamepad_state.shift_held > 0;
}

bool sdl_gamepad_ctrl_active(void)
{
    return g_gamepad_state.ctrl_held > 0;
}

bool sdl_gamepad_alt_active(void)
{
    return g_gamepad_state.alt_held > 0;
}

int sdl_gamepad_modifier_index(int binding)
{
    switch (binding) {
    case GAMEPAD_BIND_SHIFT:
        return GAMEPAD_MODIFIER_SHIFT;
    case GAMEPAD_BIND_CTRL:
        return GAMEPAD_MODIFIER_CTRL;
    case GAMEPAD_BIND_ALT:
        return GAMEPAD_MODIFIER_ALT;
    default:
        return -1;
    }
}

int sdl_gamepad_single_active_modifier(void)
{
    int active = GAMEPAD_BIND_NONE;

    if (sdl_gamepad_shift_active())
        active = GAMEPAD_BIND_SHIFT;
    if (sdl_gamepad_ctrl_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_CTRL;
    }
    if (sdl_gamepad_alt_active()) {
        if (active != GAMEPAD_BIND_NONE)
            return GAMEPAD_BIND_NONE;
        active = GAMEPAD_BIND_ALT;
    }

    return active;
}

int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id)
{
    int modifier_index = sdl_gamepad_modifier_index(modifier);

    if (modifier_index < 0)
        return GAMEPAD_BIND_NONE;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            return config.gamepad_button_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return config.gamepad_trigger_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_left_stick_combo_bindings[modifier_index][id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_right_stick_combo_bindings[modifier_index][id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

void sdl_gamepad_mark_auto_ui(void)
{
    if (get_sdl_input_ui_mode() != SDL_INPUT_UI_MODE_AUTO
        || g_gamepad_auto_ui)
    {
        return;
    }

#if defined(SDL_PLATFORM_ANDROID)
    /* SDL treats some Android keyboard covers with navigation keys as
     * gamepads.  Only a non-keyboard Android controller may enable the
     * controller presentation automatically. */
    if (!sdl_android_has_controller_device())
        return;
#endif

    g_gamepad_auto_ui = true;
}

void sdl_gamepad_apply_modifier(int binding, bool down)
{
    int delta = down ? 1 : -1;

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_gamepad_state.shift_held += delta;
        if (g_gamepad_state.shift_held < 0)
            g_gamepad_state.shift_held = 0;
    } else if (binding == GAMEPAD_BIND_CTRL) {
        g_gamepad_state.ctrl_held += delta;
        if (g_gamepad_state.ctrl_held < 0)
            g_gamepad_state.ctrl_held = 0;
        sdl_pointer_attack_clear_hover();
        sdl_pointer_attack_clear_touch_selection();
        sdl_pointer_attack_cancel_touch_press();
        sdl_mouse_path_cancel();
        g_state.need_present = true;
    } else if (binding == GAMEPAD_BIND_ALT) {
        g_gamepad_state.alt_held += delta;
        if (g_gamepad_state.alt_held < 0)
            g_gamepad_state.alt_held = 0;
    }

    if (down)
        (void)sdl_gamepad_resolve_pending_shoulder_with_modifier(binding);
}

void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt)
{
    if (alt)
        return;

    if (ctrl)
    {
        if (SDL_isalpha(key))
            Term_keypress(KTRL(key));
        return;
    }

    if (shift)
    {
        int shifted = sdl_shifted_ascii_for_key(key);

        if (SDL_isalpha(key))
            key = SDL_toupper(key);
        else if (shifted)
            key = shifted;
    }

    if (key > 0 && key < 256)
        Term_keypress(key);
}

int sdl_keymap_mode(void)
{
    if (!hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL;
    if (hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL_HJKL;
    if (!hjkl_movement && angband_keyset)
        return KEYMAP_MODE_ANGBAND;
    return KEYMAP_MODE_ANGBAND_HJKL;
}

int sdl_shifted_ascii_for_key(int key)
{
    if (key < 0 || key >= 256)
        return 0;

    switch (key) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '[': return '{';
    case ']': return '}';
    case ';': return ':';
    case '\'': return '"';
    case '\\': return '|';
    case '`': return '~';
    default: return 0;
    }
}

char sdl_direction_char_for_key(int key)
{
    switch (key) {
        case SDLK_UP:
        case SDLK_KP_8:
            return '8';
        case SDLK_DOWN:
        case SDLK_KP_2:
            return '2';
        case SDLK_LEFT:
        case SDLK_KP_4:
            return '4';
        case SDLK_RIGHT:
        case SDLK_KP_6:
            return '6';
        case SDLK_KP_1:
        case SDLK_END:
            return '1';
        case SDLK_KP_3:
        case SDLK_PAGEDOWN:
            return '3';
        case SDLK_KP_7:
        case SDLK_HOME:
            return '7';
        case SDLK_KP_9:
        case SDLK_PAGEUP:
            return '9';
        case SDLK_KP_5:
            return '5';
        default:
            break;
    }

    if (SDL_isprint(key) && key > 0 && key < 256)
        return (char)key;

    return 0;
}

int sdl_direction_for_key_char(char ch)
{
    int dir;
    int mode;
    cptr act;

    if (!ch)
        return 0;

    dir = target_dir(ch);
    if (dir)
        return dir;

    mode = sdl_keymap_mode();
    act = keymap_act[mode][(byte)ch];
    if (act && streq(act, "z"))
        return 5;

    return 0;
}

static u16b sdl_movement_modifiers_from_sdl(SDL_Keymod mod)
{
    u16b modifiers = 0;

    if (mod & SDL_KMOD_SHIFT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_SHIFT;
    if (mod & SDL_KMOD_CTRL)
        modifiers |= MOVEMENT_INPUT_MODIFIER_CTRL;
    if (mod & SDL_KMOD_ALT)
        modifiers |= MOVEMENT_INPUT_MODIFIER_ALT;
    if (mod & SDL_KMOD_GUI)
        modifiers |= MOVEMENT_INPUT_MODIFIER_META;

    return modifiers;
}

/*
 * Prompt input must reach the prompt before any movement-preset translation.
 * This covers both SDL-owned overlays and the older terminal prompts whose
 * input is synchronous inside inkey().  A separate predicate intentionally
 * omits character_icky: global layout shortcuts remain useful on saved
 * screens, while gameplay movement and command aliases must be disabled
 * there.
 */
static bool sdl_prompt_input_is_active(void)
{
    return g_touch_pane_yes_no_prompt_active
        || sdl_question_menu_captures_pointer()
        || inkey_prompt_input_active();
}

static bool sdl_movement_input_is_modal(void)
{
    return character_icky || sdl_prompt_input_is_active();
}

/*
 * The terminal menus normally translate the B binding after inkey() returns,
 * but a few native overlays own the event before it reaches that loop.  Keep
 * the physical B button on the same Back/Escape path for every modal menu.
 */
static bool sdl_gamepad_back_button_is_modal(void)
{
    return sdl_movement_input_is_modal()
        || sdl_hint_quest_menu_active();
}

static bool sdl_movement_command_input_is_live(void)
{
    return movement_input_active_context() == MOVEMENT_INPUT_CONTEXT_DUNGEON
        && !sdl_movement_input_is_modal();
}

/*
 * Movement is only submitted during live gameplay input: a real command
 * request, a direction prompt, or targeting. A non-NONE active context is the
 * primary signal, but that context can survive into a modal screen that was
 * not entered through a fresh command request. character_icky is set whenever
 * a screen is saved (options, inventory, knowledge, the movement-binding menu
 * itself, ...), so it reliably means "a modal is up, do not steal keys for the
 * player." Without this guard, presets that bind arrow/letter keys would eat
 * those keys inside menus instead of letting them navigate.
 */
static bool sdl_movement_input_is_live(void)
{
    return !sdl_movement_input_is_modal()
        && movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_NONE;
}

static bool sdl_submit_movement_command(
    const movement_input_command* command)
{
    if (!command || !sdl_movement_input_is_live())
        return false;
    if (!movement_input_submit_command(command))
        return false;

    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

static bool sdl_submit_legacy_keypad_movement(u16b action, int dir)
{
    movement_input_command command;
    u16b direction = MOVEMENT_INPUT_DIRECTION_NONE;
    u16b context = movement_input_active_context();

    if (!sdl_movement_input_is_live())
        return false;
    if (movement_input_action_is_directional(action))
    {
        if (!movement_input_direction_from_legacy_keypad(dir, &direction))
            return false;
    }

    movement_input_command_clear(&command);
    command.context = context;
    command.action = action;
    command.direction = direction;

    return sdl_submit_movement_command(&command);
}

bool sdl_try_send_movement_event(const SDL_KeyboardEvent* key_event)
{
    movement_input_command command;
    u16b context;
    u16b modifiers;
    u32b trigger;
    u32b trigger_aux;

    if (!key_event)
        return false;

    if (!sdl_movement_input_is_live())
        return false;
    context = movement_input_active_context();

    modifiers = sdl_movement_modifiers_from_sdl(key_event->mod);
    trigger = (u32b)key_event->scancode;
    trigger_aux = (u32b)SDL_GetKeyFromScancode(key_event->scancode,
        SDL_KMOD_NONE, false);

    if (!sdl_config_resolve_movement_binding(&config, context, trigger,
            trigger_aux, modifiers, &command))
    {
        return false;
    }

    return sdl_submit_movement_command(&command);
}

/*
 * Letter-based movement presets take over some letters' normal commands while
 * in the dungeon. That shadows both lowercase commands (w = wield, s = sing)
 * and Shift/capital commands (S = stealth, D = disarm, ...). Alt is the one
 * free modifier, so:
 *   Alt+<letter>       -> the lowercase command (Alt+w = wield, Alt+s = sing)
 *   Alt+Shift+<letter> -> the capital command  (Alt+Shift+s = stealth)
 * Only fires when that letter has a plain movement binding, so
 * Classic/Arrows presets are unaffected and the Alt layout shortcuts
 * (Alt+a/i/l) keep working for unshadowed letters.
 */
bool sdl_try_send_shadowed_command_event(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode base;
    char command;

    if (!key_event || !character_dungeon
        || !sdl_movement_command_input_is_live())
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT)
        || (key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        return false;
    }

    if (!sdl_config_scancode_is_plain_movement_letter(&config,
            (u32b)key_event->scancode))
    {
        return false;
    }

    base = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);
    if (base < 'a' || base > 'z')
        return false;

    /* Shift selects the capital command for that key. */
    command = (key_event->mod & SDL_KMOD_SHIFT)
        ? (char)SDL_toupper(base)
        : (char)base;

    /* Issue the underlying letter command (movement only steals the bare key,
     * so feeding the letter through the Term queue runs its real command). */
    Term_keypress(command);
    return true;
}

/*
 * WASD-grid-only extra command keys. The grid shadows several command letters,
 * so the free letters n/v/k are offered as command keys for that preset only:
 *   n = sing, v = examine, k = activate staff, Shift+N = toggle stealth.
 * (Other presets leave n/v/k alone; in particular Vi uses n/k for movement and
 * keeps the normal Shift+S for stealth.)
 */
bool sdl_try_send_preset_command_alias(const SDL_KeyboardEvent* key_event)
{
    char command;

    if (!key_event || !character_dungeon
        || !sdl_movement_command_input_is_live())
        return false;
    if (key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))
        return false;
    if (config.movement_keyboard_preset != SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC)
        return false;

    if (key_event->mod & SDL_KMOD_SHIFT)
    {
        /* Capital alias for the command shadowed by the WASD-grid bindings. */
        if (key_event->scancode != SDL_SCANCODE_N)
            return false;
        command = 'S'; /* toggle stealth */
    }
    else
    {
        switch (key_event->scancode)
        {
        case SDL_SCANCODE_N:
            command = 's'; /* sing */
            break;
        case SDL_SCANCODE_V:
            command = 'x'; /* examine */
            break;
        case SDL_SCANCODE_K:
            command = 'a'; /* activate staff */
            break;
        default:
            return false;
        }
    }

    Term_keypress(command);
    return true;
}

bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt,
    bool gui)
{
    bool control = ctrl || gui;
    int mod_count = (shift ? 1 : 0) + (control ? 1 : 0) + (alt ? 1 : 0);
    char action_key;
    char follow_key;

    if (dir < 1 || dir > 9 || mod_count != 1
        || sdl_movement_input_is_modal())
        return false;

    if (alt) {
        action_key = 'f';
        follow_key = (dir == 5) ? 'f' : dir_ch;
    } else if (control) {
        if (sdl_submit_legacy_keypad_movement(
                MOVEMENT_INPUT_ACTION_INTERACT_DIR, dir))
        {
            return true;
        }
        action_key = '/';
        follow_key = (dir == 5) ? '5' : dir_ch;
    } else {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_REST
                           : MOVEMENT_INPUT_ACTION_RUN_DIR,
                dir))
        {
            return true;
        }
        action_key = '.';
        follow_key = (dir == 5) ? '5' : dir_ch;
    }

    if (!follow_key)
        follow_key = (char)('0' + dir);

    /* Bypass keymaps for the action key itself, but keep the bound direction key. */
    Term_keypress('\\');
    Term_keypress(action_key);
    Term_keypress(follow_key);
    return true;
}

bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui)
{
    char dir_ch = sdl_direction_char_for_key(key);
    int dir = sdl_direction_for_key_char(dir_ch);

    if (!dir)
        return false;

    return sdl_send_modified_direction_action(dir, dir_ch, shift, ctrl, alt, gui);
}

bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event)
{
    bool shift;
    bool alt;
    bool ctrl;
    bool gui;
    SDL_Keycode base_key;
    int shifted_ascii;

    if (!key_event || !sdl_movement_input_is_live())
        return false;

    shift = key_event->mod & SDL_KMOD_SHIFT;
    alt = key_event->mod & SDL_KMOD_ALT;
    ctrl = key_event->mod & SDL_KMOD_CTRL;
    gui = key_event->mod & SDL_KMOD_GUI;

    base_key = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);

    /* Shifted punctuation is normally a distinct command (<, >, *, ?).
     * Do not reinterpret it as Shift plus the unshifted key's keymap action. */
    shifted_ascii = sdl_shifted_ascii_for_key(key_event->key);
    if (!shifted_ascii && base_key != key_event->key)
        shifted_ascii = sdl_shifted_ascii_for_key(base_key);
    if (shift && !ctrl && !alt && !gui && shifted_ascii)
        return false;

    if (sdl_try_send_modified_direction_key(key_event->key, shift, ctrl, alt, gui))
        return true;

    if (base_key != key_event->key
        && sdl_try_send_modified_direction_key(base_key, shift, ctrl, alt, gui))
    {
        return true;
    }

    return false;
}

bool sdl_handle_jewelry_preset_shortcut(
    const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event || !character_dungeon || sdl_movement_input_is_modal())
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT)
        || (key_event->mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        return false;
    }

    key = SDL_GetKeyFromScancode(key_event->scancode, SDL_KMOD_NONE, false);
    if (key < '1' || key > ('0' + JEWELRY_PRESET_MAX))
        return false;

    Term_keypress('\\');
    Term_keypress('J');
    Term_keypress(key);
    return true;
}

bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event)
{
    SDL_Keycode key;

    if (!key_event || sdl_prompt_input_is_active())
        return false;

    if (!(key_event->mod & SDL_KMOD_ALT))
        return false;

    key = key_event->key;

    if (key == '+' || key == '=' || key == SDLK_KP_PLUS) {
        (void)sdl_main_screen_adjust_main_view_scale(1);
        return true;
    }

    if (key == '-' || key == SDLK_KP_MINUS) {
        (void)sdl_main_screen_adjust_main_view_scale(-1);
        return true;
    }

    if (key == 'i' || key == 'I') {
        bool enabled = get_sdl_enable_right_panes();

        if (key_event->repeat)
            return true;

        set_sdl_enable_right_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if (key == 'l' || key == 'L') {
        bool enabled = get_sdl_enable_bottom_panes();

        if (key_event->repeat)
            return true;

        set_sdl_enable_bottom_panes(!enabled);
        sdl_apply_config();
        if (character_dungeon)
            Term_keypress(KTRL('R'));
        return true;
    }

    if ((key == 'a' || key == 'A')
        && !(key_event->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
    {
        bool old_tiles = get_sdl_tiles();

        if (key_event->repeat)
            return true;

        set_sdl_tiles(!old_tiles);
        return true;
    }

    return false;
}

/*
 * Give every unmodified controller control the same situational command
 * resolution as the touch controls.  Keep that resolution out of ordinary
 * modal menus: there, A/B/X/Y and the shoulders are translated by the menu's
 * controller contract and must not turn into dungeon-floor commands.
 */
static int sdl_gamepad_context_key(int key)
{
    bool description_open = g_description_overlay.active
        && g_description_overlay.interactive;
    int context_key = (key == INPUT_BIND_CONFIRM) ? ' ' : key;
    int resolved_key = context_key;

    if (description_open)
    {
        if (sdl_description_overlay_has_footer_action(context_key))
            return context_key;

        if (context_key == 'g'
            && sdl_description_overlay_has_footer_action(' '))
        {
            return ' ';
        }

        if (touch_shortcut_context_action(context_key, true, &resolved_key,
                NULL, 0)
            && sdl_description_overlay_has_footer_action(resolved_key))
        {
            return resolved_key;
        }

        return context_key;
    }

    if (!sdl_main_screen_click_shortcuts_active())
        return context_key;

    (void)touch_shortcut_context_action(context_key, false, &resolved_key,
        NULL, 0);
    return resolved_key;
}

void sdl_gamepad_send_key(int key, bool use_macro_mods)
{
    bool shift = sdl_gamepad_shift_active();
    bool ctrl = sdl_gamepad_ctrl_active();
    bool alt = sdl_gamepad_alt_active();

    if (use_macro_mods && (shift || ctrl || alt)) {
        sdl_send_macro_key(key, shift, ctrl, alt);
        return;
    }

    if (!shift && !ctrl && !alt)
        key = sdl_gamepad_context_key(key);

    if (SDL_isprint(key)) {
        if (ctrl && !alt && SDL_isalpha(key)) {
            Term_keypress(KTRL(key));
            return;
        }

        if (ctrl || alt) {
            sdl_send_macro_key(key, shift, ctrl, alt);
            return;
        }

        if (shift) {
            if (SDL_isalpha(key)) {
                key = SDL_toupper(key);
            } else {
                const char shifted[256] = {
                    ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$', ['5'] = '%',
                    ['6'] = '^', ['7'] = '&', ['8'] = '*', ['9'] = '(', ['0'] = ')',
                    ['-'] = '_', ['='] = '+',
                    [','] = '<', ['.'] = '>', ['/'] = '?',
                    ['['] = '{', [']'] = '}',
                    [';'] = ':', ['\''] = '"', ['\\'] = '|',
                    ['`'] = '~',
                };
                if (shifted[key])
                    key = shifted[key];
            }
        }

        Term_keypress(key);
        return;
    }

    if (shift || ctrl || alt) {
        sdl_send_macro_key(key, shift, ctrl, alt);
    } else {
        Term_keypress(key);
    }
}

void sdl_gamepad_send_key_raw(int key)
{
    Term_keypress(sdl_gamepad_context_key(key));
}

void sdl_gamepad_send_shoulder_combo(void)
{
    int binding = config.gamepad_shoulder_combo_binding;
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
        sdl_gamepad_apply_modifier(binding, false);
        return;
    }

    sdl_gamepad_send_key(binding, false);
}

void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt)
{
    log_debug("controller movement: dir=%d shift=%d ctrl=%d alt=%d ctx=%u icky=%d prompt=%d",
        dir, shift, ctrl, alt, movement_input_active_context(), character_icky,
        inkey_prompt_input_active());
    if (dir < 1 || dir > 9)
        return;

    if (!shift && !ctrl && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_WAIT
                           : MOVEMENT_INPUT_ACTION_MOVE_DIR,
                dir))
        {
            return;
        }
    }

    if (shift && !ctrl && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                (dir == 5) ? MOVEMENT_INPUT_ACTION_REST
                           : MOVEMENT_INPUT_ACTION_RUN_DIR,
                dir))
        {
            return;
        }
    }

    if (ctrl && !shift && !alt)
    {
        if (sdl_submit_legacy_keypad_movement(
                MOVEMENT_INPUT_ACTION_INTERACT_DIR, dir))
        {
            return;
        }
    }

    if (sdl_send_modified_direction_action(dir, (char)('0' + dir), shift, ctrl, alt, false))
        return;

    if (shift || ctrl || alt) {
        sdl_send_macro_key('0' + dir, shift, ctrl, alt);
    } else {
        Term_keypress('0' + dir);
    }
}

int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone)
{
    int dx = 0;
    int dy = 0;

    if (x > deadzone)
        dx = 1;
    else if (x < -deadzone)
        dx = -1;

    if (y > deadzone)
        dy = 1;
    else if (y < -deadzone)
        dy = -1;

    if (dx == 0 && dy == 0)
        return 0;

    if (dy < 0) {
        if (dx < 0) return 7;
        if (dx > 0) return 9;
        return 8;
    }
    if (dy > 0) {
        if (dx < 0) return 1;
        if (dx > 0) return 3;
        return 2;
    }
    if (dx < 0) return 4;
    if (dx > 0) return 6;
    return 0;
}

int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone)
{
    int abs_x = abs(x);
    int abs_y = abs(y);

    if (abs_x < deadzone && abs_y < deadzone)
        return -1;

    if (abs_x >= abs_y) {
        return (x >= 0) ? GAMEPAD_STICK_DIR_RIGHT : GAMEPAD_STICK_DIR_LEFT;
    }

    return (y >= 0) ? GAMEPAD_STICK_DIR_DOWN : GAMEPAD_STICK_DIR_UP;
}

static int sdl_gamepad_button_ui_direction(SDL_GamepadButton button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return GAMEPAD_STICK_DIR_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return GAMEPAD_STICK_DIR_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return GAMEPAD_STICK_DIR_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return GAMEPAD_STICK_DIR_RIGHT;
    default: return -1;
    }
}

static bool sdl_gamepad_send_ui_direction(int dir)
{
    int key = 0;

    switch (dir)
    {
    case GAMEPAD_STICK_DIR_UP: key = '8'; break;
    case GAMEPAD_STICK_DIR_DOWN: key = '2'; break;
    case GAMEPAD_STICK_DIR_LEFT: key = '4'; break;
    case GAMEPAD_STICK_DIR_RIGHT: key = '6'; break;
    default: break;
    }
    if (!key)
        return false;

    Term_keypress(key);
    return true;
}

void sdl_gamepad_send_direction(int dir)
{
    sdl_gamepad_send_direction_mods(dir, sdl_gamepad_shift_active(),
        sdl_gamepad_ctrl_active(), sdl_gamepad_alt_active());
}

static int sdl_gamepad_dpad_diagonal_delay_ms(void)
{
    int delay = config.gamepad_dpad_diagonal_delay_ms;

    if (delay < SDL_GAMEPAD_DPAD_DIAGONAL_DELAY_MIN_MS)
        return SDL_GAMEPAD_DPAD_DIAGONAL_DELAY_MIN_MS;
    if (delay > SDL_GAMEPAD_DPAD_DIAGONAL_DELAY_MAX_MS)
        return SDL_GAMEPAD_DPAD_DIAGONAL_DELAY_MAX_MS;
    return delay;
}

void sdl_gamepad_clear_pending_dpad(void)
{
    g_gamepad_state.dpad_pending = false;
    g_gamepad_state.dpad_pending_dir = 0;
    g_gamepad_state.dpad_pending_time = 0;
    g_gamepad_state.dpad_pending_shift = false;
    g_gamepad_state.dpad_pending_ctrl = false;
    g_gamepad_state.dpad_pending_alt = false;
}

void sdl_gamepad_set_pending_dpad(int dir, Uint64 press_time_ns)
{
    g_gamepad_state.dpad_pending = true;
    g_gamepad_state.dpad_pending_dir = dir;
    g_gamepad_state.dpad_pending_time = press_time_ns;
    g_gamepad_state.dpad_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.dpad_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.dpad_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.dpad_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_dpad) {
        sdl_gamepad_clear_pending_dpad();
        return false;
    }

    Uint64 window_ns = (Uint64)sdl_gamepad_dpad_diagonal_delay_ms()
        * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.dpad_pending_time < window_ns)
        return false;

    /* SDL_PollEvent can stop at a poll-cycle sentinel with newer events still
     * queued.  Resolve those button transitions before expiring the chord;
     * their timestamps may still fall inside its window. */
    if (!force && SDL_HasEvents(SDL_EVENT_GAMEPAD_BUTTON_DOWN,
            SDL_EVENT_GAMEPAD_BUTTON_UP))
        return false;

    log_debug("controller dpad timeout: dir=%d age_ms=%llu force=%d",
        g_gamepad_state.dpad_pending_dir,
        (unsigned long long)((now_ns - g_gamepad_state.dpad_pending_time) / 1000000ULL), force);
    sdl_gamepad_send_direction_mods(g_gamepad_state.dpad_pending_dir,
        g_gamepad_state.dpad_pending_shift, g_gamepad_state.dpad_pending_ctrl,
        g_gamepad_state.dpad_pending_alt);
    sdl_gamepad_clear_pending_dpad();
    return true;
}

void sdl_gamepad_clear_pending_left_stick(void)
{
    g_gamepad_state.left_pending = false;
    g_gamepad_state.left_pending_dir = 0;
    g_gamepad_state.left_pending_time = 0;
    g_gamepad_state.left_pending_shift = false;
    g_gamepad_state.left_pending_ctrl = false;
    g_gamepad_state.left_pending_alt = false;
}

void sdl_gamepad_set_pending_left_stick(int dir)
{
    g_gamepad_state.left_pending = true;
    g_gamepad_state.left_pending_dir = dir;
    g_gamepad_state.left_pending_time = SDL_GetTicksNS();
    g_gamepad_state.left_pending_shift = sdl_gamepad_shift_active();
    g_gamepad_state.left_pending_ctrl = sdl_gamepad_ctrl_active();
    g_gamepad_state.left_pending_alt = sdl_gamepad_alt_active();
}

bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.left_pending)
        return false;
    if (!config.gamepad_enabled || !config.gamepad_use_left_stick) {
        sdl_gamepad_clear_pending_left_stick();
        return false;
    }

    Uint64 window_ns = (Uint64)GAMEPAD_STICK_DIAGONAL_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.left_pending_time < window_ns)
        return false;

    sdl_gamepad_send_direction_mods(g_gamepad_state.left_pending_dir,
        g_gamepad_state.left_pending_shift, g_gamepad_state.left_pending_ctrl,
        g_gamepad_state.left_pending_alt);
    sdl_gamepad_clear_pending_left_stick();
    return true;
}

void sdl_gamepad_clear_pending_confirm(void)
{
    g_gamepad_state.confirm_pending = false;
    g_gamepad_state.confirm_pending_button = 0;
    g_gamepad_state.confirm_pending_binding = GAMEPAD_BIND_NONE;
    g_gamepad_state.confirm_pending_time = 0;
    g_gamepad_state.confirm_long_triggered = false;
}

bool sdl_gamepad_confirm_long_press_available(int binding)
{
    if (!config.gamepad_enabled)
        return false;
    if (!sdl_gamepad_action_is_confirm(binding))
        return false;
    if (g_player_action_menu.active || g_player_exchange_target.active)
        return false;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (sdl_gamepad_single_active_modifier() != GAMEPAD_BIND_NONE)
        return false;

    return true;
}

bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects,
    SDL_FRect* out_panel)
{
    SDL_Rect screen;
    SDL_Rect anchor;
    enum pane_placement where;

    if (!sdl_touch_top_panel_layout_visible())
        return false;
    if (config.touch_top_panel_arrows_visible && !g_touch_top_panel_open)
        return false;

    if (!sdl_touch_top_panel_current_anchor(&screen, &anchor, &where))
        return false;
    return sdl_touch_top_panel_compute_layout_for_anchor(&screen, &anchor,
        where, button_rects, out_panel);
}

/* Geometry-only layout for the tutorial: the quick access panel is taught even
 * when it is collapsed (g_touch_top_panel_open == false) or gameplay shortcuts
 * are inactive, so the runtime open/visible gates are skipped on purpose. */
bool sdl_touch_top_panel_compute_layout_for_display(SDL_FRect* button_rects,
    SDL_FRect* out_panel)
{
    SDL_Rect screen;
    SDL_Rect anchor;
    enum pane_placement where;

    if (!sdl_touch_top_panel_current_anchor(&screen, &anchor, &where))
        return false;
    return sdl_touch_top_panel_compute_layout_for_anchor(&screen, &anchor,
        where, button_rects, out_panel);
}

bool sdl_gamepad_handle_confirm_long_press_button(
    int button, int binding, bool down)
{
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    if (g_gamepad_state.confirm_pending
        && g_gamepad_state.confirm_pending_button == button)
    {
        if (down)
            return true;

        if (!g_gamepad_state.confirm_long_triggered)
            sdl_gamepad_send_key_raw(
                g_gamepad_state.confirm_pending_binding);

        sdl_gamepad_clear_pending_confirm();
        return true;
    }

    if (!down)
        return false;

    if (!sdl_gamepad_confirm_long_press_available(binding))
        return false;

    g_gamepad_state.confirm_pending = true;
    g_gamepad_state.confirm_pending_button = button;
    g_gamepad_state.confirm_pending_binding = binding;
    g_gamepad_state.confirm_pending_time = SDL_GetTicksNS();
    g_gamepad_state.confirm_long_triggered = false;
    return true;
}

int sdl_gamepad_pending_confirm_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_gamepad_state.confirm_pending)
        return -1;
    if (g_gamepad_state.confirm_long_triggered)
        return -1;
    if (!config.gamepad_enabled) {
        sdl_gamepad_clear_pending_confirm();
        return -1;
    }

    elapsed = now_ns - g_gamepad_state.confirm_pending_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_gamepad_flush_pending_confirm(Uint64 now_ns)
{
    if (!g_gamepad_state.confirm_pending)
        return false;
    if (g_gamepad_state.confirm_long_triggered)
        return false;
    if (!sdl_gamepad_confirm_long_press_available(
            g_gamepad_state.confirm_pending_binding))
    {
        return false;
    }
    if (now_ns - g_gamepad_state.confirm_pending_time
        < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    if (!sdl_player_action_menu_open())
        return false;

    g_gamepad_state.confirm_long_triggered = true;
    sdl_player_action_menu_select_default();
    return true;
}

void sdl_gamepad_clear_pending_shoulder(void)
{
    g_gamepad_state.shoulder_pending = false;
    g_gamepad_state.shoulder_pending_button = 0;
    g_gamepad_state.shoulder_pending_time = 0;
}

void sdl_gamepad_set_pending_shoulder(int button)
{
    g_gamepad_state.shoulder_pending = true;
    g_gamepad_state.shoulder_pending_button = button;
    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
}

bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force)
{
    if (!g_gamepad_state.shoulder_pending)
        return false;
    if (!config.gamepad_enabled) {
        sdl_gamepad_clear_pending_shoulder();
        return false;
    }

    Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
    if (!force && now_ns - g_gamepad_state.shoulder_pending_time < window_ns)
        return false;

    int button = g_gamepad_state.shoulder_pending_button;
    sdl_gamepad_clear_pending_shoulder();

    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return true;

    int binding = config.gamepad_button_bindings[button];
    if (binding == GAMEPAD_BIND_NONE)
        return true;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
    } else {
        sdl_gamepad_send_key(binding, false);
    }

    return true;
}

bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding)
{
    int button;
    int combo_binding;

    if (!g_gamepad_state.shoulder_pending)
        return false;
    if (!config.gamepad_enabled || !steamdeck_controls_active())
        return false;
    if (g_gamepad_capture_active)
        return false;
    if (binding != GAMEPAD_BIND_SHIFT && binding != GAMEPAD_BIND_CTRL
        && binding != GAMEPAD_BIND_ALT)
        return false;

    button = g_gamepad_state.shoulder_pending_button;
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return false;

    combo_binding = sdl_gamepad_combo_binding_for_input(binding,
        GAMEPAD_CAPTURE_BUTTON, button);
    if (combo_binding == GAMEPAD_BIND_NONE)
        return false;

    sdl_gamepad_clear_pending_shoulder();
    sdl_gamepad_send_key_raw(combo_binding);
    return true;
}

int sdl_gamepad_pending_timeout_ms(Uint64 now_ns)
{
    int dpad_timeout = -1;
    int left_timeout = -1;
    int shoulder_timeout = -1;
    int confirm_timeout = sdl_gamepad_pending_confirm_timeout_ms(now_ns);
    int best = -1;

    if (g_gamepad_state.dpad_pending && config.gamepad_enabled && config.gamepad_use_dpad) {
        Uint64 window_ns = (Uint64)sdl_gamepad_dpad_diagonal_delay_ms()
            * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.dpad_pending_time;
        if (elapsed >= window_ns) {
            dpad_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            dpad_timeout = (int)(remaining_ns / 1000000ULL);
            if (dpad_timeout < 1)
                dpad_timeout = 1;
        }
    }

    if (g_gamepad_state.left_pending && config.gamepad_enabled && config.gamepad_use_left_stick) {
        Uint64 window_ns = (Uint64)GAMEPAD_STICK_DIAGONAL_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.left_pending_time;
        if (elapsed >= window_ns) {
            left_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            left_timeout = (int)(remaining_ns / 1000000ULL);
            if (left_timeout < 1)
                left_timeout = 1;
        }
    }

    if (g_gamepad_state.shoulder_pending && config.gamepad_enabled && steamdeck_controls_active()) {
        Uint64 window_ns = (Uint64)SHOULDER_COMBO_WINDOW_MS * 1000000ULL;
        Uint64 elapsed = now_ns - g_gamepad_state.shoulder_pending_time;
        if (elapsed >= window_ns) {
            shoulder_timeout = 0;
        } else {
            Uint64 remaining_ns = window_ns - elapsed;
            shoulder_timeout = (int)(remaining_ns / 1000000ULL);
            if (shoulder_timeout < 1)
                shoulder_timeout = 1;
        }
    }

    if (dpad_timeout >= 0)
        best = dpad_timeout;
    if (left_timeout >= 0 && (best < 0 || left_timeout < best))
        best = left_timeout;
    if (shoulder_timeout >= 0 && (best < 0 || shoulder_timeout < best))
        best = shoulder_timeout;
    if (confirm_timeout >= 0 && (best < 0 || confirm_timeout < best))
        best = confirm_timeout;

    return best;
}

const char* sdl_gamepad_button_label(int button)
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
    case SDL_GAMEPAD_BUTTON_BACK: return "View / Select";
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

const char* sdl_gamepad_button_short_label(int button)
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
    case SDL_GAMEPAD_BUTTON_BACK: return "View";
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

const char* sdl_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

const char* sdl_gamepad_trigger_short_label(int index)
{
    if (index == 0)
        return "L2";
    if (index == 1)
        return "R2";
    return "?";
}

const char* sdl_gamepad_stick_dir_label(int type, int dir, bool short_label)
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

void sdl_gamepad_binding_label_ex(int type, int id, char* buf, size_t buflen, bool short_label)
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

bool sdl_gamepad_action_is_confirm(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

bool sdl_gamepad_button_is_ui_confirm(SDL_GamepadButton button)
{
    return button == SDL_GAMEPAD_BUTTON_SOUTH;
}

bool sdl_gamepad_button_is_ui_back(SDL_GamepadButton button)
{
    return button == SDL_GAMEPAD_BUTTON_EAST;
}

bool sdl_gamepad_action_binding_equals(int lhs, int rhs)
{
    if (sdl_gamepad_action_is_confirm(lhs) && sdl_gamepad_action_is_confirm(rhs))
        return true;

    return lhs == rhs;
}

static SDL_Gamepad* sdl_gamepad_active_pad(void)
{
    for (int i = 0; i < g_gamepad_state.pad_count; i++) {
        if (g_gamepad_state.pads[i].id == g_active_gamepad_id)
            return g_gamepad_state.pads[i].pad;
    }

    if (g_gamepad_state.pad_count > 0)
        return g_gamepad_state.pads[0].pad;

    return NULL;
}

bool sdl_gamepad_control_available(int type, int id)
{
    SDL_Gamepad* pad = sdl_gamepad_active_pad();
    SDL_GamepadAxis axis = SDL_GAMEPAD_AXIS_INVALID;

    if (!pad)
        return false;

    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id < 0 || id >= SDL_GAMEPAD_BUTTON_COUNT)
            return false;
        return SDL_GamepadHasButton(pad, (SDL_GamepadButton)id);

    case GAMEPAD_CAPTURE_TRIGGER:
        if (id == 0)
            axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        else if (id == 1)
            axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        break;

    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id == GAMEPAD_STICK_DIR_LEFT || id == GAMEPAD_STICK_DIR_RIGHT)
            axis = SDL_GAMEPAD_AXIS_LEFTX;
        else if (id == GAMEPAD_STICK_DIR_UP || id == GAMEPAD_STICK_DIR_DOWN)
            axis = SDL_GAMEPAD_AXIS_LEFTY;
        break;

    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id == GAMEPAD_STICK_DIR_LEFT || id == GAMEPAD_STICK_DIR_RIGHT)
            axis = SDL_GAMEPAD_AXIS_RIGHTX;
        else if (id == GAMEPAD_STICK_DIR_UP || id == GAMEPAD_STICK_DIR_DOWN)
            axis = SDL_GAMEPAD_AXIS_RIGHTY;
        break;

    case GAMEPAD_CAPTURE_SHOULDER_COMBO:
        return SDL_GamepadHasButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
            && SDL_GamepadHasButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    default:
        return false;
    }

    return axis != SDL_GAMEPAD_AXIS_INVALID && SDL_GamepadHasAxis(pad, axis);
}

int sdl_gamepad_direct_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_button_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_trigger_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_left_stick_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_right_stick_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (sdl_gamepad_action_binding_equals(config.gamepad_shoulder_combo_binding, binding)) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

int sdl_gamepad_physical_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_button_bindings[i], binding)) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (sdl_gamepad_action_binding_equals(config.gamepad_trigger_bindings[i], binding)) {
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
        if (config.gamepad_right_stick_bindings[i] == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    return count;
}

int sdl_gamepad_combo_action_binding_count(int binding, int* out_modifier_type,
    int* out_modifier_id, int* out_type, int* out_id)
{
    static const int modifiers[] = {
        GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,
    };
    static const int types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
    };
    int total = 0;

    for (int mi = 0; mi < (int)N_ELEMENTS(modifiers); mi++) {
        int mod_type = 0;
        int mod_id = 0;
        int mod_count = sdl_gamepad_physical_binding_count(modifiers[mi], &mod_type,
            &mod_id);

        if (mod_count <= 0)
            continue;

        for (int ti = 0; ti < (int)N_ELEMENTS(types); ti++) {
            int count = 0;

            if (types[ti] == GAMEPAD_CAPTURE_BUTTON)
                count = SDL_GAMEPAD_BUTTON_COUNT;
            else if (types[ti] == GAMEPAD_CAPTURE_TRIGGER)
                count = GAMEPAD_TRIGGER_COUNT;
            else
                count = GAMEPAD_STICK_DIR_COUNT;

            for (int id = 0; id < count; id++) {
                if (!sdl_gamepad_action_binding_equals(
                        sdl_gamepad_combo_binding_for_input(modifiers[mi], types[ti], id),
                        binding))
                    continue;

                if (total == 0) {
                    if (out_modifier_type)
                        *out_modifier_type = mod_type;
                    if (out_modifier_id)
                        *out_modifier_id = mod_id;
                    if (out_type)
                        *out_type = types[ti];
                    if (out_id)
                        *out_id = id;
                }

                total += mod_count;
            }
        }
    }

    return total;
}

void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int mod_type = 0;
    int mod_id = 0;
    int direct_count = sdl_gamepad_direct_binding_count(binding, &type, &id);
    int combo_count = sdl_gamepad_combo_action_binding_count(binding, &mod_type,
        &mod_id, &type, &id);
    int count = direct_count + combo_count;

    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1 && direct_count == 1) {
        sdl_gamepad_binding_label_ex(type, id, buf, buflen, short_label);
    } else if (count == 1) {
        char mod_buf[32];
        char base_buf[32];
        sdl_gamepad_binding_label_ex(mod_type, mod_id, mod_buf, sizeof(mod_buf),
            short_label);
        sdl_gamepad_binding_label_ex(type, id, base_buf, sizeof(base_buf),
            short_label);
        strnfmt(buf, buflen, "%s+%s", mod_buf, base_buf);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, false);
}

void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen)
{
    sdl_gamepad_action_binding_label_ex(binding, buf, buflen, true);
}

void sdl_gamepad_ui_prompt_label(int binding, cptr fallback, char* buf,
    size_t buflen)
{
    /* UI prompts name physical controls.  Looking up a dungeon binding here
     * would advertise the wrong button after a remap (or a binding collision). */
    if (fallback && (streq(fallback, "A") || streq(fallback, "B")
            || streq(fallback, "X") || streq(fallback, "Y")
            || streq(fallback, "L1") || streq(fallback, "R1")
            || streq(fallback, "View") || streq(fallback, "View/Select")))
    {
        SDL_strlcpy(buf, fallback, buflen);
        return;
    }
    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

int sdl_gamepad_capture_binding_for_input(int type, int id)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        if (id >= 0 && id < SDL_GAMEPAD_BUTTON_COUNT)
            return config.gamepad_button_bindings[id];
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        if (id >= 0 && id < GAMEPAD_TRIGGER_COUNT)
            return config.gamepad_trigger_bindings[id];
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_left_stick_bindings[id];
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        if (id >= 0 && id < GAMEPAD_STICK_DIR_COUNT)
            return config.gamepad_right_stick_bindings[id];
        break;
    default:
        break;
    }

    return GAMEPAD_BIND_NONE;
}

bool sdl_gamepad_capture_queue_input(int type, int id)
{
    int binding = sdl_gamepad_capture_binding_for_input(type, id);

    if (g_gamepad_capture_allow_modifier_combo
        && (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
            || binding == GAMEPAD_BIND_ALT)) {
        if (g_gamepad_capture_modifier != GAMEPAD_BIND_NONE)
            return false;
        g_gamepad_capture_modifier = binding;
        return false;
    }

    g_gamepad_capture_type = type;
    g_gamepad_capture_id = id;
    g_gamepad_capture_ready = true;
    g_gamepad_capture_active = false;
    return true;
}

/* Stable adapters for terminal menus.  These values describe UI actions,
 * independent of the player's configurable dungeon bindings. */
int steamdeck_back_key(void)
{
    return ESCAPE;
}

int steamdeck_confirm_key(void)
{
    return '\r';
}

int steamdeck_prev_page_key(void)
{
    return '[';
}

int steamdeck_next_page_key(void)
{
    return ']';
}

int steamdeck_menu_key(int key, int prev_page_key, int next_page_key)
{
    if (!steamdeck_controls_active())
        return key;

    if (key == steamdeck_back_key())
        return ESCAPE;
    if (key == steamdeck_confirm_key())
        return '\r';
    if (prev_page_key && key == steamdeck_prev_page_key())
        return prev_page_key;
    if (next_page_key && key == steamdeck_next_page_key())
        return next_page_key;

    return key;
}

int steamdeck_info_key(void)
{
    /*
     * View/Select is present on conventional and stickless handheld
     * controllers.  Keep optional stick directions available for gameplay,
     * but never require one for menu information or recall.
     */
    return 'h';
}

int steamdeck_alt_action_key(void)
{
    /* X button (WEST) - for alternate action in menus */
    return 'x';
}

int steamdeck_secondary_key(void)
{
    /* Y button (NORTH) - for secondary action in menus */
    return 's';
}

#define SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS 64

static int sdl_gamepad_context_focus_collect(
    sdl_controller_focus_target* targets, int max_targets)
{
    int count = 0;

    if (!targets || max_targets <= 0)
        return 0;

    if (sdl_movement_input_is_modal() || g_main_menu_overlay_active
        || g_player_action_menu.active || g_player_exchange_target.active)
        return 0;

    /* A movement-triggered action popup owns controller focus while present;
     * never let a stick direction jump through it to a surface behind it. */
    if (sdl_question_menu_context_hint_active())
    {
        return sdl_question_menu_collect_controller_focus_targets(targets,
            max_targets);
    }

    /* Quick Access and the left pane are command-surface shortcuts.  They
     * remain visible during targeting and direction prompts, but must not
     * steal either stick from aim/cursor movement in those contexts. */
    if (movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_DUNGEON)
        return 0;

    count += sdl_status_line_collect_controller_focus_targets(
        targets + count, max_targets - count);
    if (count < max_targets)
    {
        count += sdl_touch_top_panel_collect_controller_focus_targets(
            targets + count, max_targets - count);
    }
    if (count < max_targets)
    {
        count += sdl_character_panel_collect_controller_focus_targets(
            targets + count, max_targets - count);
    }
    if (count < max_targets)
    {
        count += sdl_combat_overlay_collect_controller_focus_targets(
            targets + count, max_targets - count);
    }

    return count;
}

static int sdl_gamepad_context_focus_find(
    const sdl_controller_focus_target* targets, int count, int kind, int id)
{
    for (int i = 0; i < count; i++)
    {
        if (targets[i].kind == kind && targets[i].id == id)
            return i;
    }

    return -1;
}

static void sdl_gamepad_context_focus_apply(int kind, int id)
{
    /* Clear the previous surface before setting the new one: the setters
     * share tooltip and hover state.  Clearing afterwards erases new focus. */
    if (sdl_question_menu_context_hint_active())
        sdl_question_menu_set_controller_focus(-1);
    sdl_touch_top_panel_set_controller_focus(-1);
    sdl_character_panel_set_controller_focus(SDL_PANEL_CLICK_NONE);
    sdl_character_panel_set_controller_attack_focus(-1);
    sdl_status_line_set_controller_focus(-1);

    switch (kind)
    {
    case SDL_CONTROLLER_FOCUS_QUESTION_MENU:
        sdl_question_menu_set_controller_focus(id);
        break;
    case SDL_CONTROLLER_FOCUS_QUICK_ACCESS:
        sdl_touch_top_panel_set_controller_focus(id);
        break;
    case SDL_CONTROLLER_FOCUS_LEFT_PANEL:
        sdl_character_panel_set_controller_focus(id);
        break;
    case SDL_CONTROLLER_FOCUS_LEFT_PANEL_ATTACK:
        sdl_character_panel_set_controller_attack_focus(id);
        break;
    case SDL_CONTROLLER_FOCUS_COMBAT_JEWELRY:
        sdl_combat_overlay_set_controller_jewelry_focus(true);
        break;
    case SDL_CONTROLLER_FOCUS_STATUS_LINE:
        sdl_status_line_set_controller_focus(id);
        break;
    }

    g_gamepad_context_focus_kind = kind;
    g_gamepad_context_focus_id = id;
    g_state.need_present = true;
}

void sdl_gamepad_context_focus_clear(void)
{
    if (g_gamepad_context_focus_kind == SDL_CONTROLLER_FOCUS_NONE)
        return;

    sdl_gamepad_context_focus_apply(SDL_CONTROLLER_FOCUS_NONE, -1);
}

void sdl_gamepad_context_focus_render(void)
{
    sdl_controller_focus_target targets[SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS];
    int count;
    int current;
    SDL_FRect rect;
    SDL_Color colour;

    if (g_gamepad_context_focus_kind == SDL_CONTROLLER_FOCUS_NONE)
        return;
    count = sdl_gamepad_context_focus_collect(targets,
        SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS);
    current = sdl_gamepad_context_focus_find(targets, count,
        g_gamepad_context_focus_kind, g_gamepad_context_focus_id);
    if (current < 0)
    {
        sdl_gamepad_context_focus_clear();
        return;
    }

    rect = targets[current].rect;
    colour = g_state.palette[TERM_L_BLUE];
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, colour.r, colour.g, colour.b, 255);
    SDL_RenderRect(g_state.renderer, &rect);
    if (rect.w > 4.0f && rect.h > 4.0f)
    {
        rect.x += 1.0f;
        rect.y += 1.0f;
        rect.w -= 2.0f;
        rect.h -= 2.0f;
        SDL_RenderRect(g_state.renderer, &rect);
    }
}

void sdl_gamepad_prepare_ui_navigation(void)
{
    sdl_gamepad_context_focus_clear();
    sdl_gamepad_clear_pending_shoulder();
    if (g_gamepad_state.left_bind_dir >= 0
        && g_gamepad_state.left_bind_dir < GAMEPAD_STICK_DIR_COUNT)
    {
        int binding = config.gamepad_left_stick_bindings[
            g_gamepad_state.left_bind_dir];

        if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
            || binding == GAMEPAD_BIND_ALT)
        {
            sdl_gamepad_apply_modifier(binding, false);
        }
    }
    if (g_gamepad_right_binding_modifier_active
        && g_gamepad_state.right_dir >= 0
        && g_gamepad_state.right_dir < GAMEPAD_STICK_DIR_COUNT)
    {
        int binding = config.gamepad_right_stick_bindings[
            g_gamepad_state.right_dir];

        if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
            || binding == GAMEPAD_BIND_ALT)
        {
            sdl_gamepad_apply_modifier(binding, false);
        }
    }

    g_gamepad_right_binding_modifier_active = false;

    g_gamepad_state.left_dir = 0;
    g_gamepad_state.left_bind_dir = -1;
    g_gamepad_state.left_ui_dir = sdl_gamepad_axis_to_cardinal_dir(
        g_gamepad_state.left_x, g_gamepad_state.left_y,
        MAX(config.gamepad_deadzone, 0));
    g_gamepad_state.right_dir = -1;
    g_gamepad_state.right_ui_dir = sdl_gamepad_axis_to_cardinal_dir(
        g_gamepad_state.right_x, g_gamepad_state.right_y,
        MAX(config.gamepad_deadzone, 0));
    sdl_gamepad_clear_pending_left_stick();
}

static bool sdl_gamepad_native_overlay_move(int dir)
{
    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return false;

    if (g_player_action_menu.active)
    {
        if (dir == GAMEPAD_STICK_DIR_LEFT)
            sdl_player_action_menu_move_hover(-1);
        else if (dir == GAMEPAD_STICK_DIR_RIGHT)
            sdl_player_action_menu_move_hover(1);
        else if (dir == GAMEPAD_STICK_DIR_UP)
            sdl_player_action_menu_move_hover_vertical(-1);
        else
            sdl_player_action_menu_move_hover_vertical(1);
        return true;
    }

    if (g_player_exchange_target.active)
    {
        sdl_player_exchange_move_hover(
            (dir == GAMEPAD_STICK_DIR_LEFT || dir == GAMEPAD_STICK_DIR_UP)
                ? -1 : 1);
        return true;
    }

    return false;
}

static float sdl_gamepad_context_focus_center_x(
    const sdl_controller_focus_target* target)
{
    return target->rect.x + target->rect.w * 0.5f;
}

static float sdl_gamepad_context_focus_center_y(
    const sdl_controller_focus_target* target)
{
    return target->rect.y + target->rect.h * 0.5f;
}

static bool sdl_gamepad_context_focus_move(int dir)
{
    sdl_controller_focus_target targets[
        SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS];
    SDL_Rect screen;
    int count;
    int current;
    int best = -1;
    float origin_x;
    float origin_y;
    float best_score = 0.0f;

    if (dir < 0 || dir >= GAMEPAD_STICK_DIR_COUNT)
        return false;

    count = sdl_gamepad_context_focus_collect(targets,
        SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS);
    current = sdl_gamepad_context_focus_find(targets, count,
        g_gamepad_context_focus_kind, g_gamepad_context_focus_id);
    if (count <= 0)
    {
        sdl_gamepad_context_focus_clear();
        return false;
    }

    if (current >= 0)
    {
        origin_x = sdl_gamepad_context_focus_center_x(&targets[current]);
        origin_y = sdl_gamepad_context_focus_center_y(&targets[current]);
    }
    else
    {
        screen = sdl_get_layout_screen_rect();
        origin_x = (float)screen.x + (float)screen.w * 0.5f;
        origin_y = (float)screen.y + (float)screen.h * 0.5f;
    }

    /* Prefer the nearest target in the requested half-plane, strongly
     * weighting alignment so rows and columns feel stable. */
    for (int i = 0; i < count; i++)
    {
        float dx;
        float dy;
        float primary;
        float perpendicular;
        float score;

        if (i == current)
            continue;
        dx = sdl_gamepad_context_focus_center_x(&targets[i]) - origin_x;
        dy = sdl_gamepad_context_focus_center_y(&targets[i]) - origin_y;

        if (dir == GAMEPAD_STICK_DIR_LEFT
            || dir == GAMEPAD_STICK_DIR_RIGHT)
        {
            primary = (dir == GAMEPAD_STICK_DIR_RIGHT) ? dx : -dx;
            perpendicular = (dy < 0.0f) ? -dy : dy;
        }
        else
        {
            primary = (dir == GAMEPAD_STICK_DIR_DOWN) ? dy : -dy;
            perpendicular = (dx < 0.0f) ? -dx : dx;
        }
        if (primary <= 1.0f)
            continue;

        score = primary + perpendicular * 2.5f;
        if (best < 0 || score < best_score)
        {
            best = i;
            best_score = score;
        }
    }

    /* Wrap at an edge so every visible action remains reachable with a
     * single stick.  Stay near the current perpendicular row/column. */
    if (best < 0)
    {
        for (int i = 0; i < count; i++)
        {
            float x;
            float y;
            float edge;
            float perpendicular;
            float score;

            if (i == current)
                continue;
            x = sdl_gamepad_context_focus_center_x(&targets[i]);
            y = sdl_gamepad_context_focus_center_y(&targets[i]);
            if (dir == GAMEPAD_STICK_DIR_RIGHT)
            {
                edge = x;
                perpendicular = y - origin_y;
            }
            else if (dir == GAMEPAD_STICK_DIR_LEFT)
            {
                edge = -x;
                perpendicular = y - origin_y;
            }
            else if (dir == GAMEPAD_STICK_DIR_DOWN)
            {
                edge = y;
                perpendicular = x - origin_x;
            }
            else
            {
                edge = -y;
                perpendicular = x - origin_x;
            }
            if (perpendicular < 0.0f)
                perpendicular = -perpendicular;
            score = edge + perpendicular * 0.25f;
            if (best < 0 || score < best_score)
            {
                best = i;
                best_score = score;
            }
        }
    }

    if (best < 0)
        best = (current >= 0) ? current : 0;

    sdl_gamepad_context_focus_apply(targets[best].kind, targets[best].id);
    return true;
}

static bool sdl_gamepad_combine_pending_dpad(int dir, Uint64 press_time_ns)
{
    int first = g_gamepad_state.dpad_pending_dir;
    int combined;
    Uint64 window_ns = (Uint64)sdl_gamepad_dpad_diagonal_delay_ms()
        * 1000000ULL;

    if (!g_gamepad_state.dpad_pending || !window_ns
        || press_time_ns < g_gamepad_state.dpad_pending_time
        || press_time_ns - g_gamepad_state.dpad_pending_time >= window_ns)
        return false;

    /* Keep the first press for the entire chord window, including a brief
     * release while the player rolls across the D-pad.  Same-axis taps and
     * opposite directions remain separate actions. */
    if (!(((first == 8 || first == 2) && (dir == 4 || dir == 6))
            || ((first == 4 || first == 6) && (dir == 8 || dir == 2))))
        return false;
    if (g_gamepad_state.dpad_pending_shift != sdl_gamepad_shift_active()
        || g_gamepad_state.dpad_pending_ctrl != sdl_gamepad_ctrl_active()
        || g_gamepad_state.dpad_pending_alt != sdl_gamepad_alt_active())
        return false;

    /* Keypad cardinal offsets compose directly: 8 + 6 - 5 = 9 (NE). */
    combined = first + dir - 5;
    sdl_gamepad_clear_pending_dpad();
    sdl_gamepad_send_direction(combined);
    return true;
}

static bool sdl_gamepad_context_focus_activate(void)
{
    sdl_controller_focus_target targets[
        SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS];
    int count = sdl_gamepad_context_focus_collect(targets,
        SDL_GAMEPAD_CONTEXT_FOCUS_MAX_TARGETS);
    int current = sdl_gamepad_context_focus_find(targets, count,
        g_gamepad_context_focus_kind, g_gamepad_context_focus_id);
    sdl_controller_focus_target target;
    bool activated = false;

    /* A transient item popup has a meaningful primary action even before the
     * player moves the right stick.  Other gameplay surfaces require an
     * explicit focus so South retains its normal gameplay meaning. */
    if (current < 0 && sdl_question_menu_context_hint_active() && count > 0)
    {
        current = 0;
        /* Description is deliberately the first rendered item button, but
         * South should choose the popup's primary gameplay action. */
        for (int i = 0; i < count; i++)
        {
            if (targets[i].id != 'x')
            {
                current = i;
                break;
            }
        }
    }
    if (current < 0)
    {
        sdl_gamepad_context_focus_clear();
        return false;
    }

    target = targets[current];
    if (target.kind == SDL_CONTROLLER_FOCUS_QUESTION_MENU)
    {
        activated = sdl_question_menu_activate_context_choice(target.id);
    }
    else if (target.kind == SDL_CONTROLLER_FOCUS_QUICK_ACCESS)
    {
        if (target.id == SDL_CONTROLLER_QUICK_ACCESS_TOGGLE)
            sdl_touch_top_panel_set_open(!g_touch_top_panel_open);
        else
            sdl_touch_top_panel_send_slot(target.id, false);
        activated = true;
    }
    else if (target.kind == SDL_CONTROLLER_FOCUS_STATUS_LINE)
    {
        activated = sdl_handle_status_line_click_action(
            target.id & SDL_CONTROLLER_STATUS_ACTION_MASK);
    }
    else if (target.kind == SDL_CONTROLLER_FOCUS_LEFT_PANEL)
    {
        activated = sdl_handle_character_panel_click_action(target.id);
    }
    else if (target.kind == SDL_CONTROLLER_FOCUS_LEFT_PANEL_ATTACK)
    {
        int mode = target.id & SDL_CONTROLLER_ATTACK_MODE_MASK;
        bool quiver = (target.id & SDL_CONTROLLER_ATTACK_QUIVER_FLAG) != 0;

        if (sdl_pointer_attack_input_context_active())
        {
            sdl_pointer_attack_activate_panel_choice(mode, quiver);
            activated = true;
        }
    }
    else if (target.kind == SDL_CONTROLLER_FOCUS_COMBAT_JEWELRY)
    {
        sdl_enqueue_bypassed_command('J');
        activated = true;
    }

    if (activated)
        sdl_gamepad_context_focus_clear();
    return activated;
}

static bool sdl_gamepad_context_focus_handle_button(
    SDL_GamepadButton button, bool down)
{
    int dir = sdl_gamepad_button_ui_direction(button);

    if (sdl_movement_input_is_modal()
        || movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_DUNGEON)
    {
        sdl_gamepad_context_focus_clear();
        return false;
    }

    /* Hold View/Select and use the D-pad to enter spatial focus on a pad
     * without sticks.  A plain tap retains the configured gameplay action. */
    if (button == SDL_GAMEPAD_BUTTON_BACK && down
        && sdl_gamepad_single_active_modifier() == GAMEPAD_BIND_NONE
        && config.gamepad_button_bindings[button] != GAMEPAD_BIND_SHIFT
        && config.gamepad_button_bindings[button] != GAMEPAD_BIND_CTRL
        && config.gamepad_button_bindings[button] != GAMEPAD_BIND_ALT)
    {
        g_gamepad_focus_chord_pending = true;
        g_gamepad_focus_chord_used = false;
        return true;
    }
    if (dir >= 0 && g_gamepad_focus_chord_pending)
        g_gamepad_focus_chord_used = true;

    /* A nonblocking square-action popup still owns its own choices.  D-pad is
     * the stickless fallback and must not move the player out from under it. */
    if (dir >= 0 && (g_gamepad_focus_chord_pending
            || sdl_question_menu_context_hint_active()
            || g_gamepad_context_focus_kind != SDL_CONTROLLER_FOCUS_NONE))
    {
        bool popup_active = sdl_question_menu_context_hint_active();

        g_gamepad_state.dpad_up = false;
        g_gamepad_state.dpad_down = false;
        g_gamepad_state.dpad_left = false;
        g_gamepad_state.dpad_right = false;
        g_gamepad_state.dpad_dir = 0;
        sdl_gamepad_clear_pending_dpad();
        if (!down)
            return true;
        if (sdl_gamepad_context_focus_move(dir))
            return true;

        /* A stale gameplay-surface focus was invalidated by a new targeting
         * or modal context.  Let this same press reach that new owner. */
        return popup_active || g_gamepad_focus_chord_pending;
    }

    if (!down)
        return false;

    /* Start and other gameplay shortcuts leave spatial focus.  Otherwise a
     * later South press could activate a stale row after that command ends. */
    if (button != SDL_GAMEPAD_BUTTON_SOUTH
        && button != SDL_GAMEPAD_BUTTON_EAST)
        sdl_gamepad_context_focus_clear();

    if (sdl_gamepad_button_is_ui_confirm(button))
        return sdl_gamepad_context_focus_activate();

    if (sdl_gamepad_button_is_ui_back(button))
    {
        if (sdl_question_menu_context_hint_active())
        {
            sdl_question_menu_clear_context_hint();
            sdl_gamepad_context_focus_clear();
            return true;
        }
        if (g_gamepad_context_focus_kind != SDL_CONTROLLER_FOCUS_NONE)
        {
            sdl_gamepad_context_focus_clear();
            return true;
        }
    }

    return false;
}

void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev)
{
    if (!ev)
        return;

    SDL_GamepadButton button = (SDL_GamepadButton)ev->button;
    bool down = ev->down;

    if (down)
        g_active_gamepad_id = ev->which;

    if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
        g_gamepad_state.left_shoulder_down = down;
    } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
        g_gamepad_state.right_shoulder_down = down;
    }

    if (g_gamepad_capture_active) {
        bool capture_armed = (SDL_GetTicksNS() >= g_gamepad_capture_arm_time);
        bool shoulder_button = (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
            || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if (shoulder_button) {
            if (!capture_armed)
                return;

            if (g_gamepad_capture_allow_modifier_combo && down) {
                int binding = sdl_gamepad_capture_binding_for_input(
                    GAMEPAD_CAPTURE_BUTTON, (int)button);
                if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL
                    || binding == GAMEPAD_BIND_ALT) {
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                    return;
                }
            }

            if (g_gamepad_capture_modifier != GAMEPAD_BIND_NONE) {
                if (down)
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                return;
            }

            if (down) {
                if (g_gamepad_state.shoulder_pending &&
                    g_gamepad_state.shoulder_pending_button != (int)button) {
                    sdl_gamepad_clear_pending_shoulder();
                    g_gamepad_capture_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
                    g_gamepad_capture_id = 0;
                    g_gamepad_capture_ready = true;
                    g_gamepad_capture_active = false;
                } else {
                    g_gamepad_state.shoulder_pending = true;
                    g_gamepad_state.shoulder_pending_button = (int)button;
                    g_gamepad_state.shoulder_pending_time = SDL_GetTicksNS();
                }
            } else if (g_gamepad_state.shoulder_pending &&
                       g_gamepad_state.shoulder_pending_button == (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                    (int)button);
            }
            return;
        }

        if (!capture_armed)
            return;

        if (down) {
            bool dpad_button = (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
                || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            if (!dpad_button || !config.gamepad_use_dpad) {
                if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
                    (void)sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON,
                        (int)button);
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (!down && button == SDL_GAMEPAD_BUTTON_BACK
        && g_gamepad_focus_chord_pending)
    {
        bool send_tap = !g_gamepad_focus_chord_used
            && sdl_movement_command_input_is_live();
        g_gamepad_focus_chord_pending = false;
        g_gamepad_focus_chord_used = false;
        if (send_tap && config.gamepad_button_bindings[button] != GAMEPAD_BIND_NONE)
            sdl_gamepad_send_key(config.gamepad_button_bindings[button], false);
        return;
    }

    /* A release must retire a held modifier even if a new UI owner now
     * consumes that physical button. */
    if (!down && button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT)
    {
        sdl_gamepad_release_button_modifier(button);
    }

    if (sdl_minimap_handle_gamepad_button(button, down))
        return;

    if (sdl_welcome_screen_handle_gamepad_button(button, down))
        return;

    if (!down && g_gamepad_state.confirm_pending
        && g_gamepad_state.confirm_pending_button == (int)button
        && sdl_gamepad_handle_confirm_long_press_button(
            (int)button, GAMEPAD_BIND_NONE, down))
    {
        return;
    }

    if (sdl_player_exchange_handle_gamepad_button(button, down))
        return;

    if (sdl_player_action_menu_handle_gamepad_button(button, down))
        return;

    if (sdl_gamepad_context_focus_handle_button(button, down))
        return;

    if (down && sdl_gamepad_button_is_ui_confirm(button)
        && sdl_movement_input_is_modal())
    {
        /* Terminal-backed prompts must receive the semantic controller action,
         * not whichever gameplay key happens to be bound to South.  Native and
         * contextual overlays have already had first refusal above. */
        Term_keypress('\r');
        return;
    }

    if (down && sdl_gamepad_button_is_ui_back(button)
        && sdl_gamepad_back_button_is_modal())
    {
        Term_keypress(ESCAPE);
        return;
    }

    if (sdl_gamepad_back_button_is_modal()
        || (movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_NONE
            && movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_DUNGEON))
    {
        int ui_key = 0;
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_SOUTH: ui_key = steamdeck_confirm_key(); break;
        case SDL_GAMEPAD_BUTTON_EAST:
        case SDL_GAMEPAD_BUTTON_START: ui_key = ESCAPE; break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: ui_key = steamdeck_prev_page_key(); break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: ui_key = steamdeck_next_page_key(); break;
        case SDL_GAMEPAD_BUTTON_BACK: ui_key = steamdeck_info_key(); break;
        case SDL_GAMEPAD_BUTTON_WEST: ui_key = steamdeck_alt_action_key(); break;
        case SDL_GAMEPAD_BUTTON_NORTH: ui_key = steamdeck_secondary_key(); break;
        default: break;
        }
        if (ui_key)
        {
            sdl_gamepad_clear_pending_shoulder();
            if (down)
                Term_keypress(ui_key);
            return;
        }
    }

    if (button == SDL_GAMEPAD_BUTTON_DPAD_UP
        || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
        || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
        || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
    {
        /* An enabled movement D-pad must resolve its chord before choosing
         * the movement or terminal queue, as it did before UI navigation was
         * added.  Clearing held/pending state for each modal event splits
         * Up+Right into '8' and '6' without ever using the diagonal delay.
         * sdl_gamepad_send_direction_mods() already routes a resolved direction
         * to the terminal when a saved screen or prompt owns the input. */
        if (!config.gamepad_use_dpad && sdl_movement_input_is_modal())
        {
            g_gamepad_state.dpad_up = false;
            g_gamepad_state.dpad_down = false;
            g_gamepad_state.dpad_left = false;
            g_gamepad_state.dpad_right = false;
            g_gamepad_state.dpad_dir = 0;
            sdl_gamepad_clear_pending_dpad();
            if (down)
            {
                (void)sdl_gamepad_send_ui_direction(
                    sdl_gamepad_button_ui_direction(button));
            }
            return;
        }

        /* The setting controls dungeon movement, not whether a physical
         * D-pad may be bound as four ordinary gameplay buttons. */
        if (!config.gamepad_use_dpad)
            goto handle_bound_button;

        switch (button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: g_gamepad_state.dpad_up = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: g_gamepad_state.dpad_down = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: g_gamepad_state.dpad_left = down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: g_gamepad_state.dpad_right = down; break;
            default: break;
        }

        int dx = 0;
        int dy = 0;
        if (g_gamepad_state.dpad_left) dx--;
        if (g_gamepad_state.dpad_right) dx++;
        if (g_gamepad_state.dpad_up) dy--;
        if (g_gamepad_state.dpad_down) dy++;

        int dir = 0;
        bool diagonal = false;
        if (dx || dy) {
            if (dy < 0) {
                dir = (dx < 0) ? 7 : (dx > 0) ? 9 : 8;
            } else if (dy > 0) {
                dir = (dx < 0) ? 1 : (dx > 0) ? 3 : 2;
            } else {
                dir = (dx < 0) ? 4 : (dx > 0) ? 6 : 0;
            }
            diagonal = (dx != 0 && dy != 0);
        }

        if (dir != g_gamepad_state.dpad_dir) {
            g_gamepad_state.dpad_dir = dir;

            if (!down)
                return;

            if (dir == 0) {
                /* Keep pending to allow quick taps to resolve. */
            } else if (diagonal) {
                sdl_gamepad_clear_pending_dpad();
                sdl_gamepad_send_direction(dir);
            } else {
                if (sdl_gamepad_dpad_diagonal_delay_ms() == 0) {
                    sdl_gamepad_clear_pending_dpad();
                    sdl_gamepad_send_direction(dir);
                } else {
                    Uint64 now_ns = SDL_GetTicksNS();
                    /* Measure the physical presses, not time spent rendering
                     * or handling other events between their dispatches.
                     * Manually supplied events may omit the SDL timestamp. */
                    Uint64 press_time_ns = ev->timestamp ? ev->timestamp : now_ns;
                    if (sdl_gamepad_combine_pending_dpad(dir, press_time_ns))
                        return;
                    if (g_gamepad_state.dpad_pending)
                        sdl_gamepad_flush_pending_dpad(now_ns, true);
                    sdl_gamepad_set_pending_dpad(dir, press_time_ns);
                }
            }
        }
        return;
    }

    if (steamdeck_controls_active() &&
        (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
    {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (down && active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key_raw(combo_binding);
                return;
            }
        }

        if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
            int binding = config.gamepad_button_bindings[button];
            if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                if (down && !g_gamepad_button_modifiers[button]) {
                    g_gamepad_button_modifiers[button] = binding;
                    sdl_gamepad_apply_modifier(binding, true);
                }
                return;
            }
        }

        if (down) {
            if (g_gamepad_state.shoulder_pending &&
                g_gamepad_state.shoulder_pending_button != (int)button) {
                sdl_gamepad_clear_pending_shoulder();
                sdl_gamepad_send_shoulder_combo();
            } else {
                sdl_gamepad_set_pending_shoulder((int)button);
            }
        } else if (g_gamepad_state.shoulder_pending &&
                   g_gamepad_state.shoulder_pending_button == (int)button) {
            sdl_gamepad_flush_pending_shoulder(SDL_GetTicksNS(), true);
        }
        return;
    }

handle_bound_button:
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return;

    if (down) {
        int active_modifier = sdl_gamepad_single_active_modifier();
        int combo_binding = GAMEPAD_BIND_NONE;

        if (active_modifier != GAMEPAD_BIND_NONE) {
            combo_binding = sdl_gamepad_combo_binding_for_input(active_modifier,
                GAMEPAD_CAPTURE_BUTTON, (int)button);
            if (combo_binding != GAMEPAD_BIND_NONE) {
                sdl_gamepad_send_key_raw(combo_binding);
                return;
            }
        }
    }

    int binding = config.gamepad_button_bindings[button];
    if (sdl_gamepad_handle_confirm_long_press_button((int)button, binding, down))
        return;

    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
        if (down && !g_gamepad_button_modifiers[button]) {
            g_gamepad_button_modifiers[button] = binding;
            sdl_gamepad_apply_modifier(binding, true);
        }
        return;
    }

    if (down)
        sdl_gamepad_send_key(binding, false);
}

void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev)
{
    if (!ev)
        return;

    if (abs((int)ev->value) > MAX(config.gamepad_deadzone, 0))
        g_active_gamepad_id = ev->which;

    if (g_gamepad_capture_active) {
        bool capture_armed = (SDL_GetTicksNS() >= g_gamepad_capture_arm_time);
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
                g_gamepad_state.left_x = ev->value;
            else
                g_gamepad_state.left_y = ev->value;

            if (!capture_armed)
                return;

            if (!config.gamepad_use_left_stick) {
                int deadzone = config.gamepad_deadzone;
                if (deadzone < 0)
                    deadzone = 0;
                int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
                if (dir >= 0) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_LEFT_STICK, dir);
                }
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
                g_gamepad_state.right_x = ev->value;
            else
                g_gamepad_state.right_y = ev->value;

            if (!capture_armed)
                return;

            int deadzone = config.gamepad_deadzone;
            if (deadzone < 0)
                deadzone = 0;
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
            if (dir >= 0) {
                (void)sdl_gamepad_capture_queue_input(
                    GAMEPAD_CAPTURE_RIGHT_STICK, dir);
            }
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            int threshold = config.gamepad_trigger_threshold;
            if (threshold < 0)
                threshold = 0;
            bool pressed = (ev->value >= threshold);

            if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                bool was_down = g_gamepad_state.left_trigger_down;
                g_gamepad_state.left_trigger_down = pressed;
                if (capture_armed && pressed && !was_down) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_TRIGGER, 0);
                }
            } else {
                bool was_down = g_gamepad_state.right_trigger_down;
                g_gamepad_state.right_trigger_down = pressed;
                if (capture_armed && pressed && !was_down) {
                    (void)sdl_gamepad_capture_queue_input(
                        GAMEPAD_CAPTURE_TRIGGER, 1);
                }
            }
        }
        return;
    }

    if (!config.gamepad_enabled)
        return;

    sdl_gamepad_mark_auto_ui();

    if (sdl_minimap_handle_gamepad_axis(ev))
        return;

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX || ev->axis == SDL_GAMEPAD_AXIS_LEFTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
            g_gamepad_state.left_x = ev->value;
        else
            g_gamepad_state.left_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;

        if (g_player_action_menu.active || g_player_exchange_target.active)
        {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x,
                g_gamepad_state.left_y, deadzone);
            int prev_ui_dir = g_gamepad_state.left_ui_dir;

            g_gamepad_state.left_dir = 0;
            g_gamepad_state.left_bind_dir = -1;
            sdl_gamepad_clear_pending_left_stick();
            if (dir != prev_ui_dir)
            {
                g_gamepad_state.left_ui_dir = dir;
                if (dir >= 0)
                    (void)sdl_gamepad_native_overlay_move(dir);
            }
            return;
        }

        if (sdl_movement_input_is_modal())
        {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x,
                g_gamepad_state.left_y, deadzone);
            int prev_ui_dir = g_gamepad_state.left_ui_dir;

            /* Release a gameplay modifier owned by a custom stick binding
             * before the modal surface takes ownership of the stick. */
            if (!config.gamepad_use_left_stick
                && g_gamepad_state.left_bind_dir >= 0
                && g_gamepad_state.left_bind_dir < GAMEPAD_STICK_DIR_COUNT)
            {
                int binding = config.gamepad_left_stick_bindings[
                    g_gamepad_state.left_bind_dir];

                if (binding == GAMEPAD_BIND_SHIFT
                    || binding == GAMEPAD_BIND_CTRL
                    || binding == GAMEPAD_BIND_ALT)
                {
                    sdl_gamepad_apply_modifier(binding, false);
                }
            }
            g_gamepad_state.left_dir = 0;
            g_gamepad_state.left_bind_dir = -1;
            sdl_gamepad_clear_pending_left_stick();

            if (dir != prev_ui_dir)
            {
                g_gamepad_state.left_ui_dir = dir;
                if (dir >= 0)
                    (void)sdl_gamepad_send_ui_direction(dir);
            }
            return;
        }

        /* Do not leak a stick held by the departing overlay into gameplay.
         * Re-centering explicitly hands ownership back to the dungeon. */
        if (g_gamepad_state.left_ui_dir >= 0)
        {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x,
                g_gamepad_state.left_y, deadzone);

            if (dir < 0)
                g_gamepad_state.left_ui_dir = -1;
            return;
        }

        if (sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x,
                g_gamepad_state.left_y, deadzone) >= 0)
            sdl_gamepad_context_focus_clear();

        if (config.gamepad_use_left_stick) {
            int dir = sdl_gamepad_axis_to_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_dir;
            if (dir != prev_dir) {
                g_gamepad_state.left_dir = dir;
                if (dir == 0) {
                    /* Keep pending to allow quick taps to resolve. */
                } else if (dir == 1 || dir == 3 || dir == 7 || dir == 9) {
                    sdl_gamepad_clear_pending_left_stick();
                    sdl_gamepad_send_direction(dir);
                } else {
                    if (prev_dir == 1 || prev_dir == 3 || prev_dir == 7 || prev_dir == 9) {
                        sdl_gamepad_clear_pending_left_stick();
                        return;
                    }
                    if (g_gamepad_state.left_pending)
                        sdl_gamepad_flush_pending_left_stick(SDL_GetTicksNS(), true);
                    sdl_gamepad_set_pending_left_stick(dir);
                }
            }
        } else {
            int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.left_x, g_gamepad_state.left_y, deadzone);
            int prev_dir = g_gamepad_state.left_bind_dir;
            if (dir != prev_dir) {
                if (prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                    int prev_binding = config.gamepad_left_stick_bindings[prev_dir];
                    if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(prev_binding, false);
                    }
                }

                g_gamepad_state.left_bind_dir = dir;

                if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    int binding = config.gamepad_left_stick_bindings[dir];
                    int combo_binding = GAMEPAD_BIND_NONE;

                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_LEFT_STICK, dir);
                    }

                    if (combo_binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key_raw(combo_binding);
                    } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        sdl_gamepad_apply_modifier(binding, true);
                    } else if (binding != GAMEPAD_BIND_NONE) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY) {
        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
            g_gamepad_state.right_x = ev->value;
        else
            g_gamepad_state.right_y = ev->value;

        int deadzone = config.gamepad_deadzone;
        if (deadzone < 0)
            deadzone = 0;
        int dir = sdl_gamepad_axis_to_cardinal_dir(g_gamepad_state.right_x, g_gamepad_state.right_y, deadzone);
        int prev_dir = g_gamepad_state.right_dir;

        if (g_player_action_menu.active || g_player_exchange_target.active)
        {
            int prev_ui_dir = g_gamepad_state.right_ui_dir;

            g_gamepad_state.right_dir = -1;
            if (dir != prev_ui_dir)
            {
                g_gamepad_state.right_ui_dir = dir;
                if (dir >= 0)
                    (void)sdl_gamepad_native_overlay_move(dir);
            }
            return;
        }

        if (sdl_movement_input_is_modal()
            || (movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_NONE
                && movement_input_active_context() != MOVEMENT_INPUT_CONTEXT_DUNGEON))
        {
            int prev_ui_dir = g_gamepad_state.right_ui_dir;

            if (g_gamepad_right_binding_modifier_active
                && prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT)
            {
                int binding = config.gamepad_right_stick_bindings[prev_dir];

                if (binding == GAMEPAD_BIND_SHIFT
                    || binding == GAMEPAD_BIND_CTRL
                    || binding == GAMEPAD_BIND_ALT)
                {
                    sdl_gamepad_apply_modifier(binding, false);
                }
            }
            g_gamepad_right_binding_modifier_active = false;
            g_gamepad_state.right_dir = -1;

            if (dir != prev_ui_dir)
            {
                g_gamepad_state.right_ui_dir = dir;
                if (dir >= 0)
                    (void)sdl_gamepad_send_ui_direction(dir);
            }
            return;
        }

        if (g_gamepad_state.right_ui_dir >= 0)
        {
            if (dir < 0)
                g_gamepad_state.right_ui_dir = -1;
            return;
        }

        if (dir != prev_dir) {
            if (g_gamepad_right_binding_modifier_active
                && prev_dir >= 0 && prev_dir < GAMEPAD_STICK_DIR_COUNT) {
                int prev_binding = config.gamepad_right_stick_bindings[prev_dir];
                if (prev_binding == GAMEPAD_BIND_SHIFT || prev_binding == GAMEPAD_BIND_CTRL || prev_binding == GAMEPAD_BIND_ALT) {
                    sdl_gamepad_apply_modifier(prev_binding, false);
                }
            }
            g_gamepad_right_binding_modifier_active = false;

            g_gamepad_state.right_dir = dir;

            if (dir >= 0
                && sdl_gamepad_single_active_modifier() == GAMEPAD_BIND_NONE
                && sdl_gamepad_context_focus_move(dir))
            {
                return;
            }

            if (dir >= 0 && dir < GAMEPAD_STICK_DIR_COUNT) {
                int active_modifier = sdl_gamepad_single_active_modifier();
                int binding = config.gamepad_right_stick_bindings[dir];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (active_modifier != GAMEPAD_BIND_NONE) {
                    combo_binding = sdl_gamepad_combo_binding_for_input(
                        active_modifier, GAMEPAD_CAPTURE_RIGHT_STICK, dir);
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                    g_gamepad_right_binding_modifier_active = true;
                    sdl_gamepad_apply_modifier(binding, true);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key(binding, false);
                }
            }
        }
        return;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        int threshold = config.gamepad_trigger_threshold;
        if (threshold < 0)
            threshold = 0;
        bool pressed = (ev->value >= threshold);

        int trigger_index = ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
        if (!pressed && g_gamepad_trigger_modifiers[trigger_index]) {
            sdl_gamepad_apply_modifier(g_gamepad_trigger_modifiers[trigger_index], false);
            g_gamepad_trigger_modifiers[trigger_index] = 0;
        }

        if (sdl_movement_input_is_modal() || g_main_menu_overlay_active
            || g_player_action_menu.active || g_player_exchange_target.active)
        {
            int index = ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
            int held = g_gamepad_trigger_modifiers[index];
            if (held) {
                sdl_gamepad_apply_modifier(held, false);
                g_gamepad_trigger_modifiers[index] = 0;
            }
            if (index == 0)
                g_gamepad_state.left_trigger_down = pressed;
            else
                g_gamepad_state.right_trigger_down = pressed;
            return;
        }

        if (ev->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            if (pressed != g_gamepad_state.left_trigger_down) {
                g_gamepad_state.left_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[0];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 0);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        if (pressed) {
                            g_gamepad_trigger_modifiers[0] = binding;
                            sdl_gamepad_apply_modifier(binding, true);
                        } else if (g_gamepad_trigger_modifiers[0]) {
                            sdl_gamepad_apply_modifier(g_gamepad_trigger_modifiers[0], false);
                            g_gamepad_trigger_modifiers[0] = 0;
                        }
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        } else {
            if (pressed != g_gamepad_state.right_trigger_down) {
                g_gamepad_state.right_trigger_down = pressed;
                int binding = config.gamepad_trigger_bindings[1];
                int combo_binding = GAMEPAD_BIND_NONE;

                if (pressed) {
                    int active_modifier = sdl_gamepad_single_active_modifier();
                    if (active_modifier != GAMEPAD_BIND_NONE) {
                        combo_binding = sdl_gamepad_combo_binding_for_input(
                            active_modifier, GAMEPAD_CAPTURE_TRIGGER, 1);
                    }
                }

                if (combo_binding != GAMEPAD_BIND_NONE) {
                    sdl_gamepad_send_key_raw(combo_binding);
                } else if (binding != GAMEPAD_BIND_NONE) {
                    if (binding == GAMEPAD_BIND_SHIFT || binding == GAMEPAD_BIND_CTRL || binding == GAMEPAD_BIND_ALT) {
                        if (pressed) {
                            g_gamepad_trigger_modifiers[1] = binding;
                            sdl_gamepad_apply_modifier(binding, true);
                        } else if (g_gamepad_trigger_modifiers[1]) {
                            sdl_gamepad_apply_modifier(g_gamepad_trigger_modifiers[1], false);
                            g_gamepad_trigger_modifiers[1] = 0;
                        }
                    } else if (pressed) {
                        sdl_gamepad_send_key(binding, false);
                    }
                }
            }
        }
    }
}

void sdl_gamepad_open(SDL_JoystickID id)
{
    if (!SDL_IsGamepad(id)) {
        log_debug("Ignoring non-gamepad device id %d", (int)id);
        return;
    }

    if (SDL_GetGamepadFromID(id)) {
        return;
    }

    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) {
        log_warn("Failed to open gamepad id %d: %s", (int)id, SDL_GetError());
        return;
    }

    if (g_gamepad_state.pad_count >= MAX_GAMEPADS) {
        SDL_CloseGamepad(pad);
        log_warn("Gamepad list full, closing id %d", (int)id);
        return;
    }

    g_gamepad_state.pads[g_gamepad_state.pad_count].id = id;
    g_gamepad_state.pads[g_gamepad_state.pad_count].pad = pad;
    g_gamepad_state.pad_count++;
    if (!g_active_gamepad_id)
        g_active_gamepad_id = id;

    log_info("Gamepad opened id %d (%s)", (int)id, SDL_GetGamepadName(pad));
    sdl_gamepad_mark_auto_ui();
}

void sdl_gamepad_close(SDL_JoystickID id)
{
    sdl_gamepad_context_focus_clear();
    sdl_gamepad_reset_modifiers();
    for (int i = 0; i < g_gamepad_state.pad_count; i++) {
        if (g_gamepad_state.pads[i].id == id) {
            SDL_CloseGamepad(g_gamepad_state.pads[i].pad);
            g_gamepad_state.pads[i] = g_gamepad_state.pads[g_gamepad_state.pad_count - 1];
            g_gamepad_state.pad_count--;
            if (g_active_gamepad_id == id) {
                g_active_gamepad_id = (g_gamepad_state.pad_count > 0)
                    ? g_gamepad_state.pads[0].id
                    : 0;
            }
            log_info("Gamepad closed id %d", (int)id);
            break;
        }
    }
}

void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev)
{
    if (!ev)
        return;

    if (ev->type == SDL_EVENT_GAMEPAD_ADDED) {
        sdl_gamepad_open(ev->which);
    } else if (ev->type == SDL_EVENT_GAMEPAD_REMOVED) {
        sdl_gamepad_close(ev->which);
    }
#if defined(SDL_PLATFORM_ANDROID)
    g_android_controller_present = sdl_android_has_controller_device();
    if (!g_android_controller_present) {
        g_gamepad_auto_ui = false;
        if (g_direct_touch_present)
            g_startup_device_class = SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    } else if (g_direct_touch_present) {
        g_startup_device_class = SDL_STARTUP_DEVICE_ANDROID_HANDHELD;
    }
#endif
}

void sdl_gamepad_init(void)
{
#if defined(SDL_PLATFORM_ANDROID) && !defined(NDEBUG)
    log_set_level(LOG_DEBUG);
#endif
    SDL_SetGamepadEventsEnabled(true);
    sdl_gamepad_reset_modifiers();
    g_gamepad_context_focus_kind = SDL_CONTROLLER_FOCUS_NONE;
    g_gamepad_context_focus_id = -1;
    g_gamepad_state.left_bind_dir = -1;
    g_gamepad_state.right_dir = -1;
    g_gamepad_state.left_ui_dir = -1;
    g_gamepad_state.right_ui_dir = -1;
    sdl_gamepad_clear_pending_shoulder();
    SDL_UpdateGamepads();
    SDL_PumpEvents();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        log_warn("SDL_GetGamepads failed: %s", SDL_GetError());
        return;
    }

    log_info("SDL_GetGamepads returned %d gamepad%s",
        count, (count == 1) ? "" : "s");
#if defined(SDL_PLATFORM_ANDROID)
    if (count == 0 && sdl_android_has_controller_device()) {
        log_warn("Android InputDevice reports a controller, but SDL_GetGamepads returned none at startup");
    }
#endif

    for (int i = 0; i < count; i++) {
        sdl_gamepad_open(ids[i]);
    }
    SDL_free(ids);
}

void sdl_gamepad_shutdown(void)
{
    while (g_gamepad_state.pad_count > 0) {
        SDL_JoystickID id = g_gamepad_state.pads[g_gamepad_state.pad_count - 1].id;
        sdl_gamepad_close(id);
    }
}

