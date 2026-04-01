#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "ui/ui-information-scene.h"

int macro_find_check(cptr pat);
int macro_find_maybe(cptr pat);
int macro_find_ready(cptr pat);

struct inkey_state {
    bool base;
    bool xtra;
    bool scan;
    bool flag;
    bool cursor_hidden;
};

static struct inkey_state g_inkey_state = { 0 };

static void inkey_clear_transient_flags(void)
{
    g_inkey_state.base = false;
    g_inkey_state.xtra = false;
    g_inkey_state.scan = false;
    g_inkey_state.flag = false;
}

static void inkey_set_flag(bool enabled)
{
    g_inkey_state.flag = enabled;
}

void inkey_set_base(bool enabled)
{
    g_inkey_state.base = enabled;
}

void inkey_set_scan(bool enabled)
{
    g_inkey_state.scan = enabled;
}

bool inkey_cursor_hidden(void)
{
    return g_inkey_state.cursor_hidden;
}

void inkey_set_cursor_hidden(bool hidden)
{
    if (g_inkey_state.cursor_hidden == hidden)
        return;

    g_inkey_state.cursor_hidden = hidden;
    app_session_set_cursor_visible(app_session_current(), !hidden);
}

/*
 * Flush all pending input.
 *
 * Actually, remember the flush, using the "inkey_xtra" flag, and in the
 * next call to "inkey()", perform the actual flushing, for efficiency,
 * and correctness of the "inkey()" function.
 */
void flush(void)
{
    /* Do it later */
    g_inkey_state.xtra = true;
}

/*
 * Flush all pending input if the flush_failure option is set.
 */
void flush_fail(void) { flush(); }

/*
 * Local variable -- we are inside a "macro action"
 *
 * Do not match any macros until "ascii 30" is found.
 */
static bool parse_macro = false;

/*
 * Local variable -- we are inside a "macro trigger"
 *
 * Strip all keypresses until a low ascii value is found.
 */
static bool parse_under = false;

static bool inkey_information_scene_candidate(const app_input* input)
{
    return input && input->layer == APP_INPUT_LAYER_LEGACY
        && input->type == APP_INPUT_TYPE_KEY;
}

static errr inkey_information_scene(char* ch, bool wait, bool take)
{
    app_session* session = app_session_current();
    app_input input;

    if (!ch || !ui_information_scene_owns_input() || !session)
        return -1;

    while (true)
    {
        while (app_session_peek_input(session, &input))
        {
            if (!inkey_information_scene_candidate(&input))
            {
                app_input discarded;

                (void)app_session_pop_input(session, &discarded);
                continue;
            }

            *ch = (char)(input.payload.key.logical_key & 0xFFu);
            if (take)
            {
                app_input consumed;

                (void)app_session_pop_input(session, &consumed);
            }
            return 0;
        }

        if (!wait)
            return 1;

        Term_xtra(TERM_XTRA_EVENT, true);
    }
}

static errr inkey_read(char* ch, bool wait, bool take)
{
    errr err;

    err = Term_inkey(ch, false, take);
    if (err == 0)
        return 0;

    err = inkey_information_scene(ch, wait, take);
    if (err >= 0)
        return err;

    return Term_inkey(ch, wait, take);
}

/*
 * Helper function called only from "inkey()"
 *
 * This function does almost all of the "macro" processing.
 */
static char inkey_aux(void)
{
    int k, n;
    int p = 0, w = 0;

    char ch;

    cptr pat, act;

    char buf[1024];

    /* Wait for a keypress */
    (void)(inkey_read(&ch, true, true));

    /* End "macro action" */
    if (ch == 30)
        parse_macro = false;

    /* Inside "macro action" */
    if (ch == 30)
        return (ch);

    /* Inside "macro action" */
    if (parse_macro)
        return (ch);

    /* Inside "macro trigger" */
    if (parse_under)
        return (ch);

    /* Save the first key, advance */
    buf[p++] = ch;
    buf[p] = '\0';

    /* Check for possible macro */
    k = macro_find_check(buf);

    /* No macro pending */
    if (k < 0)
        return (ch);

    /* Wait for a macro, or a timeout */
    while (true)
    {
        /* Check for pending macro */
        k = macro_find_maybe(buf);

        /* No macro pending */
        if (k < 0)
            break;

        /* Check for (and remove) a pending key */
        if (0 == inkey_read(&ch, false, true))
        {
            /* Append the key */
            buf[p++] = ch;
            buf[p] = '\0';

            /* Restart wait */
            w = 0;
        }

        /* No key ready */
        else
        {
            /* Increase "wait" */
            w += 10;

            /* Excessive delay */
            if (w >= 100)
                break;

            /* Delay */
            Term_xtra(TERM_XTRA_DELAY, w);
        }
    }

    /* Check for available macro */
    k = macro_find_ready(buf);

    /* No macro available */
    if (k < 0)
    {
        /* Push all the keys back on the queue */
        while (p > 0)
        {
            /* Push the key, notice over-flow */
            if (Term_key_push(buf[--p]))
                return (0);
        }

        /* Wait for (and remove) a pending key */
        (void)inkey_read(&ch, true, true);

        /* Return the key */
        return (ch);
    }

    /* Get the pattern */
    pat = macro__pat[k];

    /* Get the length of the pattern */
    n = strlen(pat);

    /* Push the "extra" keys back on the queue */
    while (p > n)
    {
        /* Push the key, notice over-flow */
        if (Term_key_push(buf[--p]))
            return (0);
    }

    /* Begin "macro action" */
    parse_macro = true;

    /* Push the "end of macro action" key */
    if (Term_key_push(30))
        return (0);

    /* Get the macro action */
    act = macro__act[k];

    /* Get the length of the action */
    n = strlen(act);

    /* Push the macro "action" onto the key queue */
    while (n > 0)
    {
        /* Push the key, notice over-flow */
        if (Term_key_push(act[--n]))
            return (0);
    }

    /* Hack -- Force "inkey()" to call us again */
    return (0);
}

/*
 * Mega-Hack -- special "inkey_next" pointer.  XXX XXX XXX
 *
 * This special pointer allows a sequence of keys to be "inserted" into
 * the stream of keys returned by "inkey()".  This key sequence will not
 * trigger any macros.  It is used in Angband to handle "keymaps".
 */
static cptr inkey_next = NULL;

bool inkey_can_consume_immediately(void)
{
    char ch;

    if (inkey_next && *inkey_next && !g_inkey_state.xtra)
        return true;
    if (!Term)
        return false;

    return (inkey_read(&ch, false, false) == 0);
}

static char inkey_with_wait_reason(u16b reason)
{
    app_wait_scope scope;
    app_session* session = app_session_current();
    char ch;

    app_session_push_wait_scope(session, &scope, reason, 0, 0);
    ch = inkey();
    app_session_pop_wait_scope(session, &scope);
    return ch;
}

/*
 * Get a keypress from the user.
 */
char inkey(void)
{
    bool cursor_state;

    char kk;

    char ch = 0;

    bool done = false;

    term* old = Term;

    /* Hack -- Use the "inkey_next" pointer */
    if (inkey_next && *inkey_next && !g_inkey_state.xtra)
    {
        /* Get next character, and advance */
        ch = *inkey_next++;

        /* Cancel the various "global parameters" */
        inkey_clear_transient_flags();

        /* Accept result */
        return (ch);
    }

    /* Forget pointer */
    inkey_next = NULL;

    /* Hack -- handle delayed "flush()" */
    if (g_inkey_state.xtra)
    {
        /* End "macro action" */
        parse_macro = false;

        /* End "macro trigger" */
        parse_under = false;

        /* Forget old keypresses */
        Term_flush();
    }

    /* Get the cursor state */
    (void)Term_get_cursor(&cursor_state);

    /* Show the cursor if waiting, except sometimes in "command" mode */
    if (!g_inkey_state.scan
        && (!g_inkey_state.flag || hilite_player
            || (hilite_target && target_sighted())
            || character_icky)
        && !g_inkey_state.cursor_hidden)
    {
        /* Show the cursor */
        (void)Term_set_cursor(true);
    }

    /* Hack -- Activate main screen */
    Term_activate(term_screen);

    /* Get a key */
    while (!ch)
    {
        /* Hack -- Handle "inkey_scan" */
        if (!g_inkey_state.base && g_inkey_state.scan
            && (0 != inkey_read(&kk, false, false)))
        {
            break;
        }

        /* Hack -- Flush output once when no key ready */
        if (!done && (0 != inkey_read(&kk, false, false)))
        {
            /* Hack -- activate proper term */
            Term_activate(old);

            /* Flush output */
            Term_fresh();

            /* Hack -- activate main screen */
            Term_activate(term_screen);

            /* Mega-Hack -- reset saved flag */
            character_saved = false;

            /* Mega-Hack -- reset signal counter */
            signal_count = 0;

            /* Only once */
            done = true;
        }

        /* Hack -- Handle "inkey_base" */
        if (g_inkey_state.base)
        {
            int w = 0;

            /* Wait forever */
            if (!g_inkey_state.scan)
            {
                /* Wait for (and remove) a pending key */
                if (0 == inkey_read(&ch, true, true))
                {
                    /* Done */
                    break;
                }

                /* Oops */
                break;
            }

            /* Wait */
            while (true)
            {
                /* Check for (and remove) a pending key */
                if (0 == inkey_read(&ch, false, true))
                {
                    /* Done */
                    break;
                }

                /* No key ready */
                else
                {
                    /* Increase "wait" */
                    w += 10;

                    /* Excessive delay */
                    if (w >= 100)
                        break;

                    /* Delay */
                    Term_xtra(TERM_XTRA_DELAY, w);
                }
            }

            /* Done */
            break;
        }

        /* Get a key (see above) */
        ch = inkey_aux();

        /* Handle "control-right-bracket" */
        if (ch == 29)
        {
            /* Strip this key */
            ch = 0;

            /* Continue */
            continue;
        }

        /* Treat back-quote as escape */
        if (ch == '`')
            ch = ESCAPE;

        /* End "macro trigger" */
        if (parse_under && (ch <= 32))
        {
            /* Strip this key */
            ch = 0;

            /* End "macro trigger" */
            parse_under = false;
        }

        /* Handle "control-caret" */
        if (ch == 30)
        {
            /* Strip this key */
            ch = 0;
        }

        /* Handle "control-underscore" */
        else if (ch == 31)
        {
            /* Strip this key */
            ch = 0;

            /* Begin "macro trigger" */
            parse_under = true;
        }

        /* Inside "macro trigger" */
        else if (parse_under)
        {
            /* Strip this key */
            ch = 0;
        }
    }

    /* Hack -- restore the term */
    Term_activate(old);

    /* Restore the cursor */
    Term_set_cursor(cursor_state);

    /* Cancel the various "global parameters" */
    inkey_clear_transient_flags();

    /* Return the keypress */
    return (ch);
}

/*
 * Hack -- special buffer to hold the action of the current keymap
 */
static char request_command_buffer[256];

/*
 * Request a command from the user.
 *
 * Sets p_ptr->command_cmd, p_ptr->command_dir, p_ptr->command_rep,
 * p_ptr->command_arg.  May modify p_ptr->command_new.
 */
void request_command(void)
{
    int i;

    char ch;

    int mode;

    cptr act;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* No command yet */
    p_ptr->command_cmd = 0;

    /* No "argument" yet */
    p_ptr->command_arg = 0;

    /* No "direction" yet */
    p_ptr->command_dir = 0;

    /* Get command */
    while (1)
    {
        /* Hack -- auto-commands */
        if (p_ptr->command_new)
        {
            /* Flush messages */
            message_flush();

            /* Use auto-command */
            ch = (char)p_ptr->command_new;

            /* Forget it */
            p_ptr->command_new = 0;
        }

        /* Get a keypress in "command" mode */
        else
        {
            /* Hack -- no flush needed */
            msg_flag = false;

            /* Activate "command mode" */
            inkey_set_flag(true);

            /* Get a command */
            ch = inkey_with_wait_reason(APP_WAIT_REASON_COMMAND_INPUT);
        }

        /* Clear top line */
        prt("", 0, 0);

        /* Command Count */
        if (((ch == 'R') && !angband_keyset) || ((ch == '0') && angband_keyset))
        {
            int old_arg = p_ptr->command_arg;

            /* Reset */
            p_ptr->command_arg = 0;

            /* Begin the input */
            prt("Repeat how many times: ", 0, 0);

            /* Get a command count */
            while (1)
            {
                /* Get a new keypress */
                ch = inkey_with_wait_reason(APP_WAIT_REASON_COMMAND_INPUT);

                /* Simple editing (delete or backspace) */
                if ((ch == 0x7F) || (ch == KTRL('H')))
                {
                    /* Delete a digit */
                    p_ptr->command_arg = p_ptr->command_arg / 10;

                    /* Show current count */
                    prt(format("Repeat how many times: %d", p_ptr->command_arg),
                        0, 0);
                }

                /* Actual numeric data */
                else if (isdigit((unsigned char)ch))
                {
                    /* Stop count at 9999 */
                    if (p_ptr->command_arg >= 1000)
                    {
                        /* Warn */
                        bell("Invalid repeat count!");

                        /* Limit */
                        p_ptr->command_arg = 9999;
                    }

                    /* Increase count */
                    else
                    {
                        /* Incorporate that digit */
                        p_ptr->command_arg = p_ptr->command_arg * 10 + D2I(ch);
                    }

                    /* Show current count */
                    prt(format("Repeat how many times: %d", p_ptr->command_arg),
                        0, 0);
                }

                /* Exit on "unusable" input */
                else
                {
                    break;
                }
            }

            /* Hack -- Handle "zero" */
            if (p_ptr->command_arg == 0)
            {
                /* Default to 99 */
                p_ptr->command_arg = 99;

                /* Show current count */
                prt(format("Repeat how many times: %d", p_ptr->command_arg), 0,
                    0);
            }

            /* Hack -- Handle "old_arg" */
            if (old_arg != 0)
            {
                /* Restore old_arg */
                p_ptr->command_arg = old_arg;

                /* Show current count */
                prt(format("Repeat how many times: %d", p_ptr->command_arg), 0,
                    0);
            }

            /* Hack -- white-space means "enter command now" */
            if ((ch == ' ') || (ch == '\n') || (ch == '\r'))
            {
                /* Get a real command */
                if (!get_com("Command: ", &ch))
                {
                    /* Clear count */
                    p_ptr->command_arg = 0;

                    /* Continue */
                    continue;
                }
            }
        }

        /* Allow "keymaps" to be bypassed */
        if (ch == '\\')
        {
            /* Get a real command */
            (void)get_com("Command: ", &ch);

            /* Hack -- bypass keymaps */
            if (!inkey_next)
                inkey_next = "";
        }

        /* Allow "control chars" to be entered */
        if (ch == '^')
        {
            /* Get a new command and controlify it */
            if (get_com("Control: ", &ch))
                ch = KTRL(ch);
        }

        /* Look up applicable keymap */
        act = keymap_act[mode][(byte)(ch)];

        /* Apply keymap if not inside a keymap already */
        /* Skip Space keymap if space_acts_as_comma option is disabled */
        if (act && !inkey_next && !((ch == ' ') && !space_acts_as_comma))
        {
            /* Install the keymap */
            SDL_strlcpy(
                request_command_buffer, act, sizeof(request_command_buffer));

            /* Start using the buffer */
            inkey_next = request_command_buffer;

            /* Continue */
            continue;
        }

        /* Paranoia */
        if (ch == '\0')
            continue;

        /* Use command */
        p_ptr->command_cmd = ch;

        /* Done */
        break;
    }

    /* Hack -- Scan equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        cptr s;

        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* No inscription */
        if (!o_ptr->obj_note)
            continue;

        /* Find a '^' */
        s = strchr(quark_str(o_ptr->obj_note), '^');

        /* Process preventions */
        while (s)
        {
            /* Check the "restriction" character */
            if ((s[1] == p_ptr->command_cmd) || (s[1] == '*'))
            {
                /* Hack -- Verify command */
                if (!get_check("Are you sure? "))
                {
                    /* Hack -- Use "newline" */
                    p_ptr->command_cmd = '\n';
                }
            }

            /* Find another '^' */
            s = strchr(s + 1, '^');
        }
    }

    /* Hack -- erase the message line. */
    prt("", 0, 0);
}
