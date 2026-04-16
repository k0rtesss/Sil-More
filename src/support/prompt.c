#include "angband.h"
#include "app/app-session.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "ui/ui-information-scene.h"

static cptr g_prompt_interaction_label = NULL;

typedef struct prompt_menu_scene_scope {
    bool active;
    u16b previous_scene;
    app_wait_scope wait_scope;
    app_input_capture_scope input_capture_scope;
} prompt_menu_scene_scope;

static char prompt_inkey_with_wait_reason(u16b reason)
{
    return (char)ui_information_scene_wait_key_with_wait_reason(reason);
}

static bool prompt_menu_scene_supported(void)
{
    app_session* session = app_session_current();

    return session
        && app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT);
}

static bool prompt_menu_scene_enter(prompt_menu_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    if (!prompt_menu_scene_supported() || !session)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || (snapshot->scene != APP_SCENE_KIND_DUNGEON
            && snapshot->scene != APP_SCENE_KIND_MENU))
        return false;

    scope->previous_scene = snapshot->scene;
    app_session_push_wait_scope(session, &scope->wait_scope,
        APP_WAIT_REASON_CONFIRM, 0, 0);
    app_session_push_input_capture(session, &scope->input_capture_scope);
    app_session_clear_inputs(session);
    scope->active = true;
    return true;
}

static bool prompt_menu_scene_present(prompt_menu_scene_scope* scope,
    const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !scene || !session)
        return false;
    if (scope->previous_scene == APP_SCENE_KIND_DUNGEON)
    {
        if (!app_session_publish_dungeon_overlay_scene(session, scene))
            return false;
    }
    else if (!app_session_publish_menu_scene(session, scene))
    {
        return false;
    }

    platform_frame_present();
    return true;
}

static void prompt_menu_scene_leave(prompt_menu_scene_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_inputs(session);
    app_session_pop_input_capture(session, &scope->input_capture_scope);
    if (scope->previous_scene == APP_SCENE_KIND_DUNGEON)
        app_session_clear_dungeon_overlay_scene(session);
    app_session_pop_wait_scope(session, &scope->wait_scope);
    scope->active = false;
    platform_frame_present();
}

static void prompt_menu_scene_add_wrapped_text(app_ui_panel* panel,
    byte attr, cptr text, size_t wrap_chars)
{
    const char* cursor = text;

    if (!panel || !text || !text[0])
        return;
    if (wrap_chars < 8)
        wrap_chars = 8;

    while (*cursor && panel->body_line_count < APP_UI_BODY_LINE_MAX)
    {
        const char* line_start;
        const char* line_end;
        const char* last_space = NULL;
        char line[APP_UI_TEXT_MAX];
        size_t line_len;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n')
            cursor++;
        if (!*cursor)
            break;

        line_start = cursor;
        while (*cursor && *cursor != '\n'
            && (size_t)(cursor - line_start) < wrap_chars)
        {
            if (*cursor == ' ' || *cursor == '\t')
                last_space = cursor;
            cursor++;
        }

        if (*cursor == '\n')
        {
            line_end = cursor;
            cursor++;
        }
        else if (*cursor && last_space && last_space > line_start)
        {
            line_end = last_space;
            cursor = last_space + 1;
        }
        else
        {
            line_end = cursor;
        }

        while (line_end > line_start
            && (line_end[-1] == ' ' || line_end[-1] == '\t'))
        {
            line_end--;
        }

        line_len = (size_t)(line_end - line_start);
        if (line_len == 0)
            continue;
        if (line_len >= sizeof(line))
            line_len = sizeof(line) - 1;

        memcpy(line, line_start, line_len);
        line[line_len] = '\0';
        (void)app_ui_panel_add_body_line(panel, attr, line);
    }
}

static void prompt_menu_scene_format_value(char* buf, size_t buf_size,
    cptr value, size_t cursor_index)
{
    size_t value_len;

    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!value)
        value = "";

    value_len = strlen(value);
    if (cursor_index > value_len)
        cursor_index = value_len;

    if (value_len + 1 >= buf_size)
        value_len = buf_size - 2;
    if (cursor_index > value_len)
        cursor_index = value_len;

    memcpy(buf, value, cursor_index);
    buf[cursor_index] = '|';
    memcpy(buf + cursor_index + 1, value + cursor_index, value_len - cursor_index);
    buf[value_len + 1] = '\0';
}

static bool prompt_menu_scene_build_text_input_scene(app_ui_scene* scene,
    cptr prompt, cptr detail, cptr value, size_t cursor_index,
    bool allow_randomize)
{
    app_ui_panel* panel;
    char value_buf[APP_UI_TEXT_MAX];

    if (!scene)
        return false;

    prompt_menu_scene_format_value(value_buf, sizeof(value_buf), value,
        cursor_index);
    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 420, 760);
    app_ui_panel_set_title(panel, TERM_L_BLUE, prompt ? prompt : "Input");
    if (detail && detail[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, detail);
    if (!app_ui_panel_add_body_line(panel, TERM_YELLOW, value_buf))
        return false;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_GREEN, true,
        "Enter", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Cancel");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "Bksp", "Erase");
    if (allow_randomize)
    {
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "Tab", "Random");
    }

    return true;
}

static int prompt_menu_scene_run_text_input(cptr prompt, char* buf, size_t len,
    cptr detail, bool allow_randomize)
{
    prompt_menu_scene_scope menu_scope;
    char ch = ESCAPE;
    size_t k = 0;

    if (!buf || len < 1)
        return -1;
    if (!prompt_menu_scene_enter(&menu_scope))
        return -1;

    buf[len - 1] = '\0';
    k = strlen(buf);
    if (k >= len)
        k = len - 1;

    while (true)
    {
        app_ui_scene scene;

        if (!prompt_menu_scene_build_text_input_scene(&scene, prompt, detail,
                buf, k, allow_randomize)
            || !prompt_menu_scene_present(&menu_scope, &scene))
        {
            prompt_menu_scene_leave(&menu_scope);
            return -1;
        }

        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (ch == ESCAPE)
            break;
        if (ch == '\n' || ch == '\r')
        {
            k = strlen(buf);
            break;
        }
        if (ch == 0x7F || ch == '\010')
        {
            if (k > 0)
                k--;
            else
                bell("Nothing to erase.");
        }
        else if (allow_randomize && ch == '\t')
        {
            make_random_name(buf, len);
            k = strlen(buf);
        }
        else if ((k < len - 1) && isprint((unsigned char)ch))
        {
            buf[k++] = ch;
        }
        else
        {
            bell("Illegal edit key!");
        }

        buf[k] = '\0';
    }

    prompt_menu_scene_leave(&menu_scope);
    return (ch != ESCAPE) ? 1 : 0;
}

static bool prompt_menu_scene_build_confirm_scene(app_ui_scene* scene,
    cptr prompt, cptr detail, bool allow_space, char other)
{
    app_ui_panel* panel;
    char other_buf[32];

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 420, 760);
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Confirm");
    prompt_menu_scene_add_wrapped_text(panel, TERM_WHITE,
        prompt ? prompt : "", 60);
    if (detail && detail[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, detail);

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_GREEN, true,
        allow_space ? "Y/Enter/Space" : "Y", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "N/Esc", "Cancel");
    if (other)
    {
        strnfmt(other_buf, sizeof(other_buf), "%c", other);
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            other_buf, "Other");
    }

    return true;
}

static bool prompt_menu_scene_build_oath_confirm_scene(app_ui_scene* scene,
    cptr prompt)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 420, 760);
    app_ui_panel_set_title(panel, TERM_L_RED, "Breaking a Sacred Oath");
    prompt_menu_scene_add_wrapped_text(panel, TERM_WHITE, prompt ? prompt : "",
        62);
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_GREEN, true,
        "Y", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "N/Esc", "Cancel");
    return true;
}

static bool prompt_menu_scene_build_prompt_scene(app_ui_scene* scene,
    cptr title, cptr prompt, cptr detail, cptr primary_key,
    cptr primary_label, cptr secondary_key, cptr secondary_label)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 420, 760);
    app_ui_panel_set_title(panel, TERM_L_BLUE, title ? title : "Prompt");
    prompt_menu_scene_add_wrapped_text(panel, TERM_WHITE,
        prompt ? prompt : "", 60);
    if (detail && detail[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, detail);
    if (primary_key && primary_key[0] && primary_label && primary_label[0])
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_GREEN, true,
            primary_key, primary_label);
    }
    if (secondary_key && secondary_key[0] && secondary_label
        && secondary_label[0])
    {
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            secondary_key, secondary_label);
    }

    return true;
}

static bool prompt_menu_scene_build_quantity_scene(app_ui_scene* scene,
    cptr prompt, int current, int max)
{
    app_ui_panel* panel;
    char prompt_header[120];
    char value_buf[16];

    if (!scene)
        return false;

    strnfmt(prompt_header, sizeof(prompt_header), "%s%d/%d", prompt, current,
        max);
    strnfmt(value_buf, sizeof(value_buf), "%d", current);

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 420, 760);
    app_ui_panel_set_title(panel, TERM_L_BLUE, prompt_header);
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "8/+ increase, 2/- decrease, digits type, Enter accepts.");
    if (!app_ui_panel_add_body_line(panel, TERM_YELLOW, value_buf))
        return false;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_GREEN, true,
        "Enter", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Cancel");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/+", "Increase");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "2/-", "Decrease");
    return true;
}

static int prompt_menu_scene_run_oath_confirm(cptr prompt)
{
    prompt_menu_scene_scope menu_scope;
    char ch;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -1;

    while (true)
    {
        app_ui_scene scene;

        if (!prompt_menu_scene_build_oath_confirm_scene(&scene, prompt)
            || !prompt_menu_scene_present(&menu_scope, &scene))
        {
            prompt_menu_scene_leave(&menu_scope);
            return -1;
        }

        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (quick_messages || ch == ESCAPE || strchr("Nn", ch))
        {
            prompt_menu_scene_leave(&menu_scope);
            return 0;
        }
        if (strchr("Yy", ch))
        {
            prompt_menu_scene_leave(&menu_scope);
            return 1;
        }

        bell("Illegal response to a 'yes/no' question!");
    }
}

static int prompt_menu_scene_run_menu_choice(s16b max, cptr prompt)
{
    prompt_menu_scene_scope menu_scope;
    int choice = -1;
    char ch;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -2;

    while (true)
    {
        app_ui_scene scene;

        if (!prompt_menu_scene_build_prompt_scene(&scene, "Choose", prompt,
                "Press a menu letter or Esc to cancel.", "A-Z", "Select",
                "Esc", "Cancel")
            || !prompt_menu_scene_present(&menu_scope, &scene))
        {
            choice = -2;
            break;
        }

        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_LIST_SELECTION);
        if (isalpha((unsigned char)ch))
        {
            if (islower((unsigned char)ch))
                choice = A2I(ch);
            else
                choice = ch - 'A' + 26;

            if ((choice > -1) && (choice < max))
                break;

            bell("Illegal response to question!");
            choice = -1;
            continue;
        }

        if (ch == ESCAPE)
        {
            choice = -1;
            break;
        }

        bell("Illegal response to question!");
    }

    prompt_menu_scene_leave(&menu_scope);
    return choice;
}

static int prompt_menu_scene_run_command_prompt(cptr prompt)
{
    prompt_menu_scene_scope menu_scope;
    app_ui_scene scene;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -2;

    if (!prompt_menu_scene_build_prompt_scene(&scene, "Command", prompt,
            "Press a key or Esc to cancel.", "Key", "Input", "Esc", "Cancel")
        || !prompt_menu_scene_present(&menu_scope, &scene))
    {
        prompt_menu_scene_leave(&menu_scope);
        return -2;
    }

    {
        int ch = (unsigned char)prompt_inkey_with_wait_reason(
            APP_WAIT_REASON_CONFIRM);
        prompt_menu_scene_leave(&menu_scope);
        return ch;
    }
}

static int prompt_menu_scene_run_pause(cptr prompt)
{
    prompt_menu_scene_scope menu_scope;
    app_ui_scene scene;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -2;

    if (!prompt_menu_scene_build_prompt_scene(&scene, "Pause",
            prompt ? prompt : "(press any key)",
            "Press any key to continue.", "Any", "Continue", NULL, NULL)
        || !prompt_menu_scene_present(&menu_scope, &scene))
    {
        prompt_menu_scene_leave(&menu_scope);
        return -2;
    }

    {
        int ch = (unsigned char)prompt_inkey_with_wait_reason(
            APP_WAIT_REASON_INFORMATIONAL_PAUSE);
        prompt_menu_scene_leave(&menu_scope);
        return ch;
    }
}

static int prompt_menu_scene_run_quantity(cptr prompt, int max, int initial)
{
    prompt_menu_scene_scope menu_scope;
    char entry_buf[16] = "";
    int entry_len = 0;
    int current = initial;
    bool done = false;
    bool canceled = false;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -1;

    while (!done)
    {
        app_ui_scene scene;
        int ch;

        if (!prompt_menu_scene_build_quantity_scene(&scene, prompt, current,
                max)
            || !prompt_menu_scene_present(&menu_scope, &scene))
        {
            prompt_menu_scene_leave(&menu_scope);
            return -1;
        }

        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_LIST_SELECTION);
        switch (ch)
        {
        case ESCAPE:
            canceled = true;
            done = true;
            break;

        case '\r':
        case '\n':
        case ' ':
#ifdef KC_ENTER
        case KC_ENTER:
#endif
            done = true;
            break;

        case '+':
        case '=':
        case '8':
        case 'k':
        case 'K':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            if (max > 0)
            {
                if (current >= max)
                    current = 0;
                else
                    current++;
            }
            else
            {
                current = 0;
            }
            entry_len = 0;
            entry_buf[0] = '\0';
            break;

        case '-':
        case '_':
        case '2':
        case 'j':
        case 'J':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            if (max > 0)
            {
                if (current > 0)
                    current--;
                else
                    current = max;
            }
            else
            {
                current = 0;
            }
            entry_len = 0;
            entry_buf[0] = '\0';
            break;

#ifdef KC_PGUP
        case KC_PGUP:
            if (max > 0)
            {
                current += 10;
                if (current > max)
                    current = 0;
            }
            else
            {
                current = 0;
            }
            entry_len = 0;
            entry_buf[0] = '\0';
            break;
#endif

#ifdef KC_PGDOWN
        case KC_PGDOWN:
            if (max > 0)
            {
                current -= 10;
                if (current < 0)
                    current = max;
            }
            else
            {
                current = 0;
            }
            entry_len = 0;
            entry_buf[0] = '\0';
            break;
#endif

        case '\b':
        case 0x7F:
            if (entry_len > 0)
            {
                entry_buf[--entry_len] = '\0';
                current = entry_len ? MAX(0, MIN(atoi(entry_buf), max)) : 0;
            }
            else
            {
                bell("Nothing to erase.");
            }
            break;

        default:
            if (isdigit((unsigned char)ch))
            {
                if (entry_len < (int)sizeof(entry_buf) - 1)
                {
                    entry_buf[entry_len++] = (char)ch;
                    entry_buf[entry_len] = '\0';
                    current = MAX(0, MIN(atoi(entry_buf), max));
                }
                else
                {
                    bell("Quantity too large.");
                }
            }
            else
            {
                bell("Illegal response to quantity prompt!");
            }
            break;
        }
    }

    prompt_menu_scene_leave(&menu_scope);
    if (canceled)
        return 0;

    return current;
}

static int prompt_menu_scene_run_confirm(cptr prompt, cptr detail,
    bool allow_space, char other)
{
    prompt_menu_scene_scope menu_scope;
    char ch;
    int result = 0;

    if (!prompt_menu_scene_enter(&menu_scope))
        return -1;

    while (true)
    {
        app_ui_scene scene;

        if (!prompt_menu_scene_build_confirm_scene(&scene, prompt, detail,
                allow_space, other)
            || !prompt_menu_scene_present(&menu_scope, &scene))
        {
            result = -1;
            break;
        }

        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (strchr("Yy", ch)
            || (allow_space && (ch == ' ' || ch == '\r' || ch == '\n')))
        {
            result = 1;
            break;
        }
        if (other && (ch == toupper((unsigned char)other)
                || ch == tolower((unsigned char)other)))
        {
            result = 2;
            break;
        }
        if (quick_messages || ch == ESCAPE || strchr("Nn", ch))
            break;

        bell("Illegal response to question!");
    }

    prompt_menu_scene_leave(&menu_scope);
    return result;
}

bool prompt_text_input(cptr prompt, cptr detail, char* buf, size_t len,
    bool allow_randomize)
{
    bool prompt_scene_supported = prompt_menu_scene_supported();

    if (!buf || len < 1)
        return false;

    /* Paranoia XXX XXX XXX */
    message_flush();

    if (prompt_scene_supported)
    {
        int scene_result = prompt_menu_scene_run_text_input(prompt, buf, len,
            detail, allow_randomize);

        if (scene_result >= 0)
            return scene_result == 1;
    }

    return false;
}

bool askfor_aux(char* buf, size_t len)
{
    return prompt_text_input(
        g_prompt_interaction_label ? g_prompt_interaction_label : "Input:",
        "Enter accepts, Esc cancels, Backspace erases.", buf, len, false);
}

/*
 * A reimplementation of askfor_aux, but allows for random names
 *
 * Sil-y: this is poor style...
 */
bool askfor_name(char* buf, size_t len)
{
    return prompt_text_input("Name:",
        "Enter accepts, Esc cancels, Tab randomizes.", buf, len, true);
}

/*
 * Request a "quantity" from the user
 *
 * Allow "p_ptr->command_arg" to specify a quantity
 */
s16b get_quantity(cptr prompt, int max)
{
    int amt = (max > 0) ? max : 1;

    /* Use "command_arg" */
    if (p_ptr->command_arg)
    {
        amt = p_ptr->command_arg;
        p_ptr->command_arg = 0;
    }

#ifdef ALLOW_REPEAT

    else if ((max != 1) && repeat_pull(&amt))
    {
        /* use repeated value */
    }

#endif /* ALLOW_REPEAT */

    else if (max != 1)
    {
        char prompt_buf[80];
        int current = amt;
        bool prompt_scene_supported = prompt_menu_scene_supported();

        if (!prompt)
        {
            strnfmt(prompt_buf, sizeof(prompt_buf), "Quantity (0-%d): ", max);
            prompt = prompt_buf;
        }

        if (max < 0)
            max = 0;

        current = MAX(0, MIN(current, max));
        if (prompt_scene_supported)
        {
            int scene_value = prompt_menu_scene_run_quantity(prompt, max,
                current);

            if (scene_value >= 0)
                amt = scene_value;
            if (scene_value >= 0)
                goto quantity_done;
        }
        return 0;
    }

quantity_done:

    if (amt > max)
        amt = max;

    if (amt < 0)
        amt = 0;

#ifdef ALLOW_REPEAT

    if (amt)
        repeat_push(amt);

#endif /* ALLOW_REPEAT */

    return (amt);
}

/*
 * Hack - duplication of get_check prompt to give option of setting destroyed
 * option to squelch.
 */
int get_check_other(cptr prompt, char other)
{
    bool prompt_scene_supported = prompt_menu_scene_supported();

    /* Paranoia XXX XXX XXX */
    message_flush();

    if (prompt_scene_supported)
    {
        int scene_result = prompt_menu_scene_run_confirm(prompt,
            "Y confirms, N declines.", false, other);

        if (scene_result >= 0)
            return scene_result;
    }

    return 0;
}

/*
 * Verify something with the user
 */
bool get_check(cptr prompt)
{
    bool portable = portable_controls_active();
    bool prompt_scene_supported = prompt_menu_scene_supported();

    /* Paranoia XXX XXX XXX */
    message_flush();

    if (prompt_scene_supported)
    {
        int scene_result = prompt_menu_scene_run_confirm(prompt,
            portable ? "Y, Enter, or Space confirms, N declines."
                     : "Y confirms, N declines.",
            portable, '\0');

        if (scene_result >= 0)
            return scene_result == 1;
    }

    return false;
}

/*
 * Multiline version of get_check() for long oath confirmation prompts
 * Displays text with proper word wrapping and fade effects
 */
bool get_check_oath_multiline(cptr prompt)
{
    /* Paranoia */
    message_flush();
    if (prompt_menu_scene_supported())
    {
        int scene_result = prompt_menu_scene_run_oath_confirm(prompt);

        if (scene_result >= 0)
            return scene_result == 1;
    }
    return false;
}

/*
 * Give a prompt, then get a choice withing a certain range.
 */
int get_menu_choice(s16b max, char* prompt)
{
    bool prompt_scene_supported = prompt_menu_scene_supported();

    if (prompt_scene_supported)
    {
        int scene_choice = prompt_menu_scene_run_menu_choice(max, prompt);

        if (scene_choice != -2)
            return scene_choice;
    }
    return -1;
}

/*
 * Prompts for a keypress
 *
 * The "prompt" should take the form "Command: "
 *
 * Returns true unless the character is "Escape"
 */
bool get_com(cptr prompt, char* command)
{
    bool prompt_scene_supported = prompt_menu_scene_supported();

    /* Paranoia XXX XXX XXX */
    message_flush();

    if (prompt_scene_supported)
    {
        int scene_ch = prompt_menu_scene_run_command_prompt(prompt);

        if (scene_ch != -2)
        {
            *command = (char)scene_ch;
            return (scene_ch != ESCAPE);
        }
    }
    *command = ESCAPE;
    return false;
}

/*
 * Pause for user response
 *
 * This function is stupid.  XXX XXX XXX
 */
void pause_line(int row)
{
    bool prompt_scene_supported = prompt_menu_scene_supported();

    (void)row;

    if (prompt_scene_supported)
    {
        if (prompt_menu_scene_run_pause("(press any key)") != -2)
            return;
    }
}
