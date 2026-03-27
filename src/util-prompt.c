#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "platform-ui.h"

static cptr g_prompt_interaction_label = NULL;

/*
 * Get some input at the cursor location.
 */
static int active_term_width(void)
{
    int wid = 80;
    int hgt = 24;

    if (Term)
        Term_get_size(&wid, &hgt);

    if (wid < 1)
        wid = 80;

    return wid;
}

static char prompt_inkey_with_wait_reason(u16b reason)
{
    app_wait_scope scope;
    app_session* session = app_session_current();
    char ch;

    app_session_push_wait_scope(session, &scope, reason, 0, 0);
    ch = inkey();
    app_session_pop_wait_scope(session, &scope);
    return ch;
}

static bool prompt_snapshot_interaction_active(void)
{
    return app_session_interactions_enabled(app_session_current());
}

static void prompt_snapshot_begin(u16b kind, u16b reason, u16b flags,
    byte prompt_attr, cptr prompt, byte detail_attr, cptr detail)
{
    app_session* session = app_session_current();

    if (!app_session_interactions_enabled(session))
        return;

    app_session_begin_interaction(session, kind, reason, flags);
    app_session_set_interaction_prompt(session, prompt_attr, prompt);
    if (detail && detail[0])
        app_session_set_interaction_detail(session, detail_attr, detail);
}

static void prompt_snapshot_set_value(byte value_attr, cptr value,
    s16b cursor_index)
{
    app_session* session = app_session_current();

    if (!app_session_interactions_enabled(session))
        return;

    app_session_set_interaction_value(session, value_attr, value,
        cursor_index);
}

static void prompt_snapshot_clear(void)
{
    app_session_clear_interaction(app_session_current());
}

static void prompt_snapshot_present(void)
{
    if (!prompt_snapshot_interaction_active())
        return;

    Term_fresh();
}

bool askfor_aux(char* buf, size_t len)
{
    int y, x;
    int term_wid = active_term_width();
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    size_t k = 0;

    char ch = '\0';

    bool done = false;

    /* Locate the cursor */
    Term_locate(&x, &y);

    /* Paranoia */
    if ((x < 0) || (x >= term_wid))
        x = 0;

    /* Restrict the length */
    if ((size_t)x + len > (size_t)term_wid)
        len = (size_t)(term_wid - x);
    if (len < 1)
        len = 1;

    /* Truncate the default entry */
    buf[len - 1] = '\0';

    /* Display the default answer */
    if (!snapshot_interaction)
    {
        Term_erase(x, y, (int)len);
        Term_putstr(x, y, -1, TERM_YELLOW, buf);
    }

    /* Process input */
    while (!done)
    {
        if (snapshot_interaction)
        {
            prompt_snapshot_begin(APP_INTERACTION_KIND_TEXT_INPUT,
                APP_WAIT_REASON_CONFIRM,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL
                    | APP_INTERACTION_FLAG_SHOW_VALUE
                    | APP_INTERACTION_FLAG_SHOW_CURSOR,
                TERM_WHITE,
                g_prompt_interaction_label ? g_prompt_interaction_label
                    : "Input:",
                TERM_SLATE, "Enter accepts, Esc cancels.");
            prompt_snapshot_set_value(TERM_YELLOW, buf, (s16b)k);
            prompt_snapshot_present();
        }
        else
        {
            /* Place cursor */
            Term_gotoxy(x + k, y);
        }

        /* Get a key */
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);

        /* Analyze the key */
        switch (ch)
        {
        case ESCAPE:
        {
            k = 0;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            k = strlen(buf);
            done = true;
            break;
        }

        case 0x7F:
        case '\010':
        {
            if (k > 0)
                k--;
            break;
        }

        default:
        {
            if ((k < len - 1) && (isprint((unsigned char)ch)))
            {
                buf[k++] = ch;
            }
            else
            {
                bell("Illegal edit key!");
            }
            break;
        }
        }

        /* Terminate */
        buf[k] = '\0';

        /* Update the entry */
        if (!snapshot_interaction)
        {
            Term_erase(x, y, (int)len);
            Term_putstr(x, y, -1, TERM_WHITE, buf);
        }
    }

    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }

    /* Done */
    return (ch != ESCAPE);
}

/*
 * A reimplementation of askfor_aux, but allows for random names
 *
 * Sil-y: this is poor style...
 */
bool askfor_name(char* buf, size_t len)
{
    int y, x;
    int term_wid = active_term_width();
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    size_t k = 0;

    char ch = '\0';

    bool done = false;
    bool new_default_name = false;

    /* Locate the cursor */
    Term_locate(&x, &y);

    /* Paranoia */
    if ((x < 0) || (x >= term_wid))
        x = 0;

    /* Restrict the length */
    if ((size_t)x + len > (size_t)term_wid)
        len = (size_t)(term_wid - x);
    if (len < 1)
        len = 1;

    /* Truncate the default entry */
    buf[len - 1] = '\0';

    /* Display the default answer */
    if (!snapshot_interaction)
    {
        Term_erase(x, y, (int)len);
        Term_putstr(x, y, -1, TERM_YELLOW, buf);
    }

    /* Process input */
    while (!done)
    {
        if (snapshot_interaction)
        {
            prompt_snapshot_begin(APP_INTERACTION_KIND_TEXT_INPUT,
                APP_WAIT_REASON_CONFIRM,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL
                    | APP_INTERACTION_FLAG_SHOW_VALUE
                    | APP_INTERACTION_FLAG_SHOW_CURSOR,
                TERM_WHITE,
                g_prompt_interaction_label ? g_prompt_interaction_label
                    : "Name:",
                TERM_SLATE,
                "Enter accepts, Esc cancels, Tab randomizes.");
            prompt_snapshot_set_value(TERM_YELLOW, buf, (s16b)k);
            prompt_snapshot_present();
        }
        else
        {
            /* Place cursor */
            Term_gotoxy(x + k, y);
        }

        /* Get a key */
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);

        /* Analyze the key */
        switch (ch)
        {
        case ESCAPE:
        {
            k = 0;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            k = strlen(buf);
            done = true;
            break;
        }

        case 0x7F:
        case '\010':
        {
            if (k > 0)
                k--;
            break;
        }

        case '\t':
        {
            /*get the random name, display for approval. */
            make_random_name(buf, len);

            new_default_name = true;
            k = 0;
            break;
        }

        default:
        {
            if ((k < len - 1) && (isprint((unsigned char)ch)))
            {
                buf[k++] = ch;
            }
            else
            {
                bell("Illegal edit key!");
            }
            break;
        }
        }

        if (new_default_name)
        {
            /* Display the random name */
            if (!snapshot_interaction)
            {
                Term_erase(x, y, (int)len);
                Term_putstr(x, y, -1, TERM_YELLOW, buf);
            }

            new_default_name = false;
        }
        else
        {
            /* Terminate */
            buf[k] = '\0';

            /* Update the entry */
            if (!snapshot_interaction)
            {
                Term_erase(x, y, (int)len);
                Term_putstr(x, y, -1, TERM_WHITE, buf);
            }
        }
    }

    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }

    /* Done */
    return (ch != ESCAPE);
}

/*
 * Prompt for a string from the user.
 *
 * The "prompt" should take the form "Prompt: ".
 */
bool term_get_string(cptr prompt, char* buf, size_t len)
{
    bool res;
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Display prompt */
    if (!snapshot_interaction)
        prt(prompt, 0, 0);

    /* Ask the user for a string */
    g_prompt_interaction_label = prompt;
    res = askfor_aux(buf, len);
    g_prompt_interaction_label = NULL;

    /* Clear prompt */
    if (!snapshot_interaction)
        prt("", 0, 0);

    /* Result */
    return (res);
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
        char entry_buf[16] = "";
        int entry_len = 0;
        int current = amt;
        bool done = false;
        bool canceled = false;
        int ch;
        bool snapshot_interaction = prompt_snapshot_interaction_active();

        if (!prompt)
        {
            strnfmt(prompt_buf, sizeof(prompt_buf), "Quantity (0-%d): ", max);
            prompt = prompt_buf;
        }

        if (max < 0)
            max = 0;

        current = MAX(0, MIN(current, max));

        while (!done)
        {
            char prompt_header[120];
            strnfmt(prompt_header, sizeof(prompt_header), "%s%d/%d", prompt,
                current, max);
            if (snapshot_interaction)
            {
                char value_buf[16];

                prompt_snapshot_begin(APP_INTERACTION_KIND_TEXT_INPUT,
                    APP_WAIT_REASON_LIST_SELECTION,
                    APP_INTERACTION_FLAG_CAN_CONFIRM
                        | APP_INTERACTION_FLAG_CAN_CANCEL
                        | APP_INTERACTION_FLAG_SHOW_VALUE
                        | APP_INTERACTION_FLAG_SHOW_CURSOR,
                    TERM_WHITE, prompt_header, TERM_SLATE,
                    "8/+ increase, 2/- decrease, digits type, Enter accepts.");
                strnfmt(value_buf, sizeof(value_buf), "%d", current);
                prompt_snapshot_set_value(TERM_YELLOW, value_buf,
                    (s16b)strlen(value_buf));
                prompt_snapshot_present();
            }
            else
            {
                prt(prompt_header, 0, 0);
                prt("Use arrows or +/- to adjust, digits type exact value, Enter=OK, Esc=cancel.",
                    1, 0);
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

        if (snapshot_interaction)
        {
            prompt_snapshot_clear();
            prompt_snapshot_present();
        }
        else
        {
            prt("", 0, 0);
            prt("", 1, 0);
        }

        if (canceled)
            return (0);

        amt = current;
    }

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
    char ch;
    char buf[160];
    int term_wid = active_term_width();
    int suffix_wid = 9;
    int prompt_wid = term_wid - suffix_wid;
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    /*default set to no*/
    int result = 0;

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Hack -- Build a "useful" prompt */
    if (prompt_wid < 8)
        prompt_wid = 8;
    strnfmt(buf, sizeof(buf), "%.*s[y/n/%c] ", prompt_wid, prompt, other);

    /* Prompt for it */
    if (!snapshot_interaction)
        prt(buf, 0, 0);

    /* Get an acceptable answer */
    while (true)
    {
        if (snapshot_interaction)
        {
            prompt_snapshot_begin(APP_INTERACTION_KIND_PROMPT,
                APP_WAIT_REASON_CONFIRM,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL,
                TERM_WHITE, buf, TERM_SLATE,
                "Y confirms, N declines.");
            prompt_snapshot_present();
        }
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (quick_messages)
            break;
        if (ch == ESCAPE)
            break;
        if (strchr("YyNn", ch))
            break;
        if (ch == toupper(other))
            break;
        if (ch == tolower(other))
            break;
        bell("Illegal response to question!");
    }

    /* Erase the prompt */
    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }
    else
        prt("", 0, 0);

    /* Normal negation */
    if ((ch == 'Y') || (ch == 'y'))
        result = 1;
    /*other option*/
    else if ((ch == toupper(other)) || (ch == tolower(other)))
        result = 2;
    /*all else default to no*/

    /* Success */
    return (result);
}

/*
 * Verify something with the user
 */
bool get_check(cptr prompt)
{
    char ch;

    char buf[160];
    bool steamdeck = steamdeck_controls_active();
    int term_wid = active_term_width();
    int suffix_wid = steamdeck ? 13 : 7;
    int prompt_wid = term_wid - suffix_wid;
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Hack -- Build a "useful" prompt */
    if (prompt_wid < 8)
        prompt_wid = 8;
    strnfmt(buf, sizeof(buf), "%.*s[y/n%s] ", prompt_wid, prompt,
        steamdeck ? "/space" : "");

    /* Prompt for it */
    if (!snapshot_interaction)
        prt(buf, 0, 0);

    /* Get an acceptable answer */
    while (true)
    {
        if (snapshot_interaction)
        {
            prompt_snapshot_begin(APP_INTERACTION_KIND_PROMPT,
                APP_WAIT_REASON_CONFIRM,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL,
                TERM_WHITE, buf, TERM_SLATE,
                steamdeck ? "Y or Space confirms, N declines."
                    : "Y confirms, N declines.");
            prompt_snapshot_present();
        }
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (quick_messages)
            break;
        if (ch == ESCAPE)
            break;
        if (strchr("YyNn", ch) || (steamdeck && ch == ' '))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    /* Erase the prompt */
    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }
    else
        prt("", 0, 0);

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !(steamdeck && ch == ' '))
        return (false);

    /* Success */
    return (true);
}

/*
 * Multiline version of get_check() for long oath confirmation prompts
 * Displays text with proper word wrapping and fade effects
 */
bool get_check_oath_multiline(cptr prompt)
{
    char ch;
    int wid, h;

    /* Paranoia */
    message_flush();

    /* Get terminal size */
    Term_get_size(&wid, &h);

    /* Save screen */
    screen_save();
    Term_clear();

    /* Title */
    Term_putstr((wid - 24) / 2, 2, -1, TERM_L_RED, "Breaking a Sacred Oath");

    /* Display the oath confirmation prompt with word wrapping */
    if (prompt && prompt[0])
    {
        char* desc_ptr = (char*)prompt;
        char line_buffer[80];
        int row = 5;
        int max_width = 70; /* Leave margins */

        while (*desc_ptr && row < h - 4)
        {
            int line_len = 0;
            char* line_start = desc_ptr;

            /* Find the longest line that fits */
            while (*desc_ptr && line_len < max_width)
            {
                if (*desc_ptr == ' ')
                {
                    /* Potential break point */
                    if (line_len > 0 && line_len + 1 < max_width)
                    {
                        memcpy(line_buffer, line_start, (size_t)line_len);
                        line_buffer[line_len] = '\0';
                    }
                }
                line_len++;
                desc_ptr++;
            }

            /* Back up to last space if we exceeded width */
            if (line_len >= max_width && *desc_ptr)
            {
                while (desc_ptr > line_start && *desc_ptr != ' ')
                {
                    desc_ptr--;
                    line_len--;
                }
                if (*desc_ptr == ' ')
                    desc_ptr++; /* Skip the space */
            }

            /* Copy the line */
            int actual_len = (int)(desc_ptr - line_start);
            if (actual_len > 79)
                actual_len = 79;
            memcpy(line_buffer, line_start, (size_t)actual_len);
            line_buffer[actual_len] = '\0';

            /* Remove trailing space */
            while (actual_len > 0 && line_buffer[actual_len - 1] == ' ')
            {
                actual_len--;
                line_buffer[actual_len] = '\0';
            }

            /* Display centered line */
            if (actual_len > 0)
            {
                int start_col = (wid - actual_len) / 2;
                if (start_col < 1)
                    start_col = 1;
                Term_putstr(start_col, row, -1, TERM_WHITE, line_buffer);
                row++;
            }

            /* Skip whitespace for next line */
            while (*desc_ptr && *desc_ptr == ' ')
                desc_ptr++;

            if (!*desc_ptr)
                break;
        }
    }

    /* Prompt at bottom */
    Term_putstr((wid - 20) / 2, h - 3, -1, TERM_YELLOW, "Are you certain? [y/n]");

    /* Get an acceptable answer */
    while (true)
    {
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);
        if (quick_messages)
            break;
        if (ch == ESCAPE)
            break;
        if (strchr("YyNn", ch))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    /* Restore screen */
    screen_load();

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y'))
        return (false);

    /* Success */
    return (true);
}

/*
 * Give a prompt, then get a choice withing a certain range.
 */
int get_menu_choice(s16b max, char* prompt)
{
    int choice = -1;

    char ch;

    bool done = false;
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    if (!snapshot_interaction)
        prt(prompt, 0, 0);

    while (!done)
    {
        if (snapshot_interaction)
        {
            prompt_snapshot_begin(APP_INTERACTION_KIND_PROMPT,
                APP_WAIT_REASON_LIST_SELECTION,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL,
                TERM_WHITE, prompt, TERM_SLATE,
                "Press a menu letter or Esc to cancel.");
            prompt_snapshot_present();
        }
        ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_LIST_SELECTION);

        /* Letters are used for selection */
        if (isalpha((unsigned char)ch))
        {
            if (islower((unsigned char)ch))
            {
                choice = A2I(ch);
            }
            else
            {
                choice = ch - 'A' + 26;
            }

            /* Validate input */
            if ((choice > -1) && (choice < max))
            {
                done = true;
            }

            else
            {
                bell("Illegal response to question!");
            }
        }

        /* Allow user to exit the fuction */
        else if (ch == ESCAPE)
        {
            /* Mark as no choice made */
            choice = -1;

            done = true;
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");
    }

    /* Clear the prompt */
    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }
    else
        prt("", 0, 0);

    /* Return */
    return (choice);
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
    char ch;
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Display a prompt */
    if (!snapshot_interaction)
        prt(prompt, 0, 0);

    if (snapshot_interaction)
    {
        prompt_snapshot_begin(APP_INTERACTION_KIND_PROMPT,
            APP_WAIT_REASON_CONFIRM,
            APP_INTERACTION_FLAG_CAN_CONFIRM
                | APP_INTERACTION_FLAG_CAN_CANCEL,
            TERM_WHITE, prompt, TERM_SLATE,
            "Press a key or Esc to cancel.");
        prompt_snapshot_present();
    }

    /* Get a key */
    ch = prompt_inkey_with_wait_reason(APP_WAIT_REASON_CONFIRM);

    /* Clear the prompt */
    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }
    else
        prt("", 0, 0);

    /* Save the command */
    *command = ch;

    /* Done */
    return (ch != ESCAPE);
}

/*
 * Pause for user response
 *
 * This function is stupid.  XXX XXX XXX
 */
void pause_line(int row)
{
    bool snapshot_interaction = prompt_snapshot_interaction_active();

    if (!snapshot_interaction)
    {
        prt("", row, 0);
        put_str("(press any key)", row, 23);
    }
    else
    {
        prompt_snapshot_begin(APP_INTERACTION_KIND_PROMPT,
            APP_WAIT_REASON_INFORMATIONAL_PAUSE,
            APP_INTERACTION_FLAG_CAN_CONFIRM,
            TERM_WHITE, "(press any key)", TERM_SLATE, "");
        prompt_snapshot_present();
    }

    (void)prompt_inkey_with_wait_reason(APP_WAIT_REASON_INFORMATIONAL_PAUSE);
    if (snapshot_interaction)
    {
        prompt_snapshot_clear();
        prompt_snapshot_present();
    }
    else
        prt("", row, 0);
}
