#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "log/log.h"
#include "platform-audio.h"

/*
 * Flush the screen, make a noise
 */
void bell(cptr reason)
{
    /* Mega-Hack -- Flush the output */
    Term_fresh();

    if (character_generated && reason)
    {
        message_add(reason, MSG_BELL);

        /* Window stuff */
        p_ptr->window |= (PW_MESSAGE);

        /* Force window redraw */
        window_stuff();
    }

    /* Make a bell noise (if allowed) */
    if (system_beep)
        Term_xtra(TERM_XTRA_NOISE, 0);

    /* Flush the input (later!) */
    flush();
}

/*
 * Hack -- Make a (relevant?) sound
 */
void sound(int val)
{
    /* No sound */
    if (!use_sound)
        return;

    /* Route directly to SDL sound backend */
    sdl_sound_handle(val);
}

/*
 * The "quark" package
 */

/*
 * The number of quarks (first quark is NULL)
 */
static s16b quark__num = 1;

/*
 * The array[QUARK_MAX] of pointers to the quarks
 */
static cptr* quark__str;

/*
 * Add a new "quark" to the set of quarks.
 */
s16b quark_add(cptr str)
{
    int i;

    /* Look for an existing quark */
    for (i = 1; i < quark__num; i++)
    {
        /* Check for equality */
        if (streq(quark__str[i], str))
            return (i);
    }

    /* Hack -- Require room XXX XXX XXX */
    if (quark__num == QUARK_MAX)
        return (0);

    /* New quark */
    i = quark__num++;

    /* Add a new quark */
    quark__str[i] = str_dup(str);

    /* Return the index */
    return (i);
}

/*
 * This function looks up a quark
 */
cptr quark_str(s16b i)
{
    cptr q;

    /* Verify */
    if ((i < 0) || (i >= quark__num))
        i = 0;

    /* Get the quark */
    q = quark__str[i];

    /* Return the quark */
    return (q);
}

/*
 * Initialize the "quark" package
 */
errr quarks_init(void)
{
    /* Quark variables */
    quark__str = mem_alloc_array(QUARK_MAX, cptr);

    /* Success */
    return (0);
}

/*
 * Free the "quark" package
 */
errr quarks_free(void)
{
    int i;

    /* Free the "quarks" */
    for (i = 1; i < quark__num; i++)
    {
        str_free(quark__str[i]);
    }

    /* Free the list of "quarks" */
    mem_free_null(quark__str);

    /* Success */
    return (0);
}

/*
 * The "message memorization" package.
 */

/*
 * The next "free" index to use
 */
static u16b message__next;

/*
 * The index of the oldest message (none yet)
 */
static u16b message__last;

/*
 * The next "free" offset
 */
static u16b message__head;

/*
 * The offset to the oldest used char (none yet)
 */
static u16b message__tail;

/*
 * The array[MESSAGE_MAX] of offsets, by index
 */
static u16b* message__ptr;

/*
 * The array[MESSAGE_BUF] of chars, by offset
 */
static char* message__buf;

/*
 * The array[MESSAGE_MAX] of u16b for the types of messages
 */
static u16b* message__type;

/*
 * The array[MESSAGE_MAX] of u16b for the count of messages
 */
static u16b* message__count;

/*
 * Table of colors associated to message-types
 */
static byte message__color[MSG_MAX];

/*
 * Calculate the index of a message
 */
static s16b message_age2idx(int age)
{
    return ((message__next + MESSAGE_MAX - (age + 1)) % MESSAGE_MAX);
}

/*
 * How many messages are "available"?
 */
s16b message_num(void)
{
    /* Determine how many messages are "available" */
    return (message_age2idx(message__last - 1));
}

/*
 * Recall the "text" of a saved message
 */
cptr message_str(s16b age)
{
    static char buf[1024];
    s16b x;
    u16b o;
    cptr s;

    /* Forgotten messages have no text */
    if ((age < 0) || (age >= message_num()))
        return ("");

    /* Get the "logical" index */
    x = message_age2idx(age);

    /* Get the "offset" for the message */
    o = message__ptr[x];

    /* Get the message text */
    s = &message__buf[o];

    /* HACK - Handle repeated messages */
    if (message__count[x] > 1)
    {
        strnfmt(buf, sizeof(buf), "%s <%dx>", s, message__count[x]);
        s = buf;
    }

    /* Return the message text */
    return (s);
}

/*
 * Recall the "type" of a saved message
 */
u16b message_type(s16b age)
{
    s16b x;

    /* Paranoia */
    if (!message__type)
        return (MSG_GENERIC);

    /* Forgotten messages are generic */
    if ((age < 0) || (age >= message_num()))
        return (MSG_GENERIC);

    /* Get the "logical" index */
    x = message_age2idx(age);

    /* Return the message type */
    return (message__type[x]);
}

/*
 * Recall the "color" of a message type
 */
static byte message_type_color(u16b type)
{
    byte color = message__color[type];

    if (color == TERM_DARK)
        color = TERM_WHITE;

    return (color);
}

/*
 * Recall the "color" of a saved message
 */
byte message_color(s16b age) { return message_type_color(message_type(age)); }

errr message_color_define(u16b type, byte color)
{
    /* Ignore illegal types */
    if (type >= MSG_MAX)
        return (1);

    /* Store the color */
    message__color[type] = color;

    /* Success */
    return (0);
}

/*
 * Add a new message, with great efficiency
 */
void message_add(cptr str, u16b type)
{
    int k, i, x, o;
    size_t n;

    cptr s;

    cptr u;
    char* v;

    /*** Step 1 -- Analyze the message ***/

    /* Hack -- Ignore "non-messages" */
    if (!str)
        return;

    /* Message length */
    n = strlen(str);

    /* Hack -- Ignore "long" messages */
    if (n >= MESSAGE_BUF / 4)
        return;

    /*** Step 2 -- Attempt to optimize ***/

    /* Get the "logical" last index */
    x = message_age2idx(0);

    /* Get the "offset" for the last message */
    o = message__ptr[x];

    /* Get the message text */
    s = &message__buf[o];

    /* Last message repeated? */
    if (streq(str, s))
    {
        /* Increase the message count */
        message__count[x]++;
        app_session_note_message(app_session_current(), type);

        /* Success */
        return;
    }

    /*** Step 3 -- Attempt to optimize ***/

    /* Limit number of messages to check */
    k = message_num() / 4;

    /* Limit number of messages to check */
    if (k > 32)
        k = 32;

    /* Start just after the most recent message */
    i = message__next;

    /* Check the last few messages for duplication */
    for (; k; k--)
    {
        u16b q;

        cptr old;

        /* Back up, wrap if needed */
        if (i-- == 0)
            i = MESSAGE_MAX - 1;

        /* Stop before oldest message */
        if (i == message__last)
            break;

        /* Index */
        o = message__ptr[i];

        /* Extract "distance" from "head" */
        q = (message__head + MESSAGE_BUF - o) % MESSAGE_BUF;

        /* Do not optimize over large distances */
        if (q >= MESSAGE_BUF / 4)
            continue;

        /* Get the old string */
        old = &message__buf[o];

        /* Continue if not equal */
        if (!streq(str, old))
            continue;

        /* Get the next available message index */
        x = message__next;

        /* Advance 'message__next', wrap if needed */
        if (++message__next == MESSAGE_MAX)
            message__next = 0;

        /* Kill last message if needed */
        if (message__next == message__last)
        {
            /* Advance 'message__last', wrap if needed */
            if (++message__last == MESSAGE_MAX)
                message__last = 0;
        }

        /* Assign the starting address */
        message__ptr[x] = message__ptr[i];

        /* Store the message type */
        message__type[x] = type;

        /* Store the message count */
        message__count[x] = 1;
        app_session_note_message(app_session_current(), type);

        /* Success */
        return;
    }

    /*** Step 4 -- Ensure space before end of buffer ***/

    /* Kill messages, and wrap, if needed */
    if (message__head + (n + 1) >= MESSAGE_BUF)
    {
        /* Kill all "dead" messages */
        for (i = message__last; true; i++)
        {
            /* Wrap if needed */
            if (i == MESSAGE_MAX)
                i = 0;

            /* Stop before the new message */
            if (i == message__next)
                break;

            /* Get offset */
            o = message__ptr[i];

            /* Kill "dead" messages */
            if (o >= message__head)
            {
                /* Track oldest message */
                message__last = i + 1;
            }
        }

        /* Wrap "tail" if needed */
        if (message__tail >= message__head)
            message__tail = 0;

        /* Start over */
        message__head = 0;
    }

    /*** Step 5 -- Ensure space for actual characters ***/

    /* Kill messages, if needed */
    if (message__head + (n + 1) > message__tail)
    {
        /* Advance to new "tail" location */
        message__tail += (MESSAGE_BUF / 4);

        /* Kill all "dead" messages */
        for (i = message__last; true; i++)
        {
            /* Wrap if needed */
            if (i == MESSAGE_MAX)
                i = 0;

            /* Stop before the new message */
            if (i == message__next)
                break;

            /* Get offset */
            o = message__ptr[i];

            /* Kill "dead" messages */
            if ((o >= message__head) && (o < message__tail))
            {
                /* Track oldest message */
                message__last = i + 1;
            }
        }
    }

    /*** Step 6 -- Grab a new message index ***/

    /* Get the next available message index */
    x = message__next;

    /* Advance 'message__next', wrap if needed */
    if (++message__next == MESSAGE_MAX)
        message__next = 0;

    /* Kill last message if needed */
    if (message__next == message__last)
    {
        /* Advance 'message__last', wrap if needed */
        if (++message__last == MESSAGE_MAX)
            message__last = 0;
    }

    /*** Step 7 -- Insert the message text ***/

    /* Assign the starting address */
    message__ptr[x] = message__head;

    /* Inline 'strcpy(message__buf + message__head, str)' */
    v = message__buf + message__head;
    for (u = str; *u;)
        *v++ = *u++;
    *v = '\0';

    /* Advance the "head" pointer */
    message__head += (n + 1);

    /* Store the message type */
    message__type[x] = type;

    /* Store the message count */
    message__count[x] = 1;
    app_session_note_message(app_session_current(), type);
}

/*
 * Initialize the "message" package
 */
errr messages_init(void)
{
    /* Message variables */
    message__ptr = mem_alloc_array(MESSAGE_MAX, u16b);
    message__buf = mem_alloc_array(MESSAGE_BUF, char);
    message__type = mem_alloc_array(MESSAGE_MAX, u16b);
    message__count = mem_alloc_array(MESSAGE_MAX, u16b);

    /* Init the message colors to white */
    memset(message__color, TERM_WHITE, sizeof(byte) * MSG_MAX);

    /* Hack -- No messages yet */
    message__tail = MESSAGE_BUF;

    /* Success */
    return (0);
}

/*
 * Free the "message" package
 */
void messages_free(void)
{
    /* Free the messages */
    mem_free_null(message__ptr);
    mem_free_null(message__buf);
    mem_free_null(message__type);
    mem_free_null(message__count);
}

/*
 * Move the cursor
 */
static void message_topline_reset(void);
static void message_topline_append(cptr text, u16b type, byte color);

void move_cursor(int row, int col)
{
    Term_gotoxy(col, row);
    app_session_note_cursor_absolute(app_session_current(), row, col,
        !inkey_cursor_hidden());
}

/*
 * Hack -- flush
 */
static void msg_flush(int x)
{
    byte a = TERM_L_BLUE;
    app_wait_scope scope;
    app_session* session = app_session_current();

    /* Pause for response */
    Term_putstr(x, 0, -1, a, "-more-");

    /* Place the cursor on the player or target */
    if (hilite_player)
        move_cursor_relative(p_ptr->py, p_ptr->px);
    if (hilite_target && target_sighted())
        move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

    if (!auto_more)
    {
        if (app_session_interactions_enabled(session))
        {
            app_session_begin_interaction(session, APP_INTERACTION_KIND_PROMPT,
                APP_WAIT_REASON_INFORMATIONAL_PAUSE,
                APP_INTERACTION_FLAG_CAN_CONFIRM
                    | APP_INTERACTION_FLAG_CAN_CANCEL);
            app_session_set_interaction_prompt(session, a, "-more-");
            app_session_set_interaction_detail(session, TERM_SLATE,
                "Press Space, Enter, or Esc to continue.");
        }

        app_session_push_wait_scope(session, &scope,
            APP_WAIT_REASON_INFORMATIONAL_PAUSE, 0, 0);

        /* Get an acceptable keypress */
        while (1)
        {
            char ch;
            ch = inkey();
            if (quick_messages)
                break;
            if ((ch == ESCAPE) || (ch == ' '))
                break;
            if ((ch == '\n') || (ch == '\r'))
                break;
            bell("Illegal response to a 'more' prompt!");
        }

        app_session_pop_wait_scope(session, &scope);
        if (app_session_interactions_enabled(session))
            app_session_clear_interaction(session);
    }

    /* Clear the line */
    Term_erase(0, 0, 255);
    message_topline_reset();
}

static int message_column = 0;
static char message_topline[1024];
static u16b message_topline_type = MSG_GENERIC;
static byte message_topline_color = TERM_WHITE;
static bool message_topline_active = false;

static void message_topline_reset(void)
{
    message_topline[0] = '\0';
    message_topline_type = MSG_GENERIC;
    message_topline_color = TERM_WHITE;
    message_topline_active = false;
}

static void message_topline_append(cptr text, u16b type, byte color)
{
    if (!text || !text[0])
        return;

    if (!message_topline_active)
        message_topline[0] = '\0';

    if (message_topline_active && message_topline[0])
        SDL_strlcat(message_topline, " ", sizeof(message_topline));

    SDL_strlcat(message_topline, text, sizeof(message_topline));
    message_topline_type = type;
    message_topline_color = color;
    message_topline_active = true;
}

bool message_topline_snapshot(char* out_text, size_t out_text_size,
    byte* out_color, u16b* out_type, bool* out_more_pending)
{
    if (!message_topline_active || !message_topline[0])
        return false;

    if (out_text && out_text_size)
        SDL_strlcpy(out_text, message_topline, out_text_size);
    if (out_color)
        *out_color = message_topline_color;
    if (out_type)
        *out_type = message_topline_type;
    if (out_more_pending)
        *out_more_pending = false;

    return true;
}

/*
 * Output a message to the top line of the screen.
 */
static void msg_print_aux(u16b type, cptr msg)
{
    int n;
    char* t;
    char buf[1024];
    byte color;
    int w, h;

    /* Obtain the size */
    (void)Term_get_size(&w, &h);

    /* Hack -- Reset */
    if (!msg_flag)
    {
        message_column = 0;
        message_topline_reset();
    }

    /* Message Length */
    n = (msg ? strlen(msg) : 0);

    /* Hack -- flush when requested or needed */
    if (message_column && (!msg || ((message_column + n) > (w - 8))))
    {
        /* Flush */
        msg_flush(message_column);

        /* Forget it */
        msg_flag = false;

        /* Reset */
        message_column = 0;
    }

    /* No message */
    if (!msg)
        return;

    /* Paranoia */
    if (n > 1000)
        return;

    /* Memorize the message (if legal) */
    if (character_generated && !p_ptr->is_dead)
        message_add(msg, type);

    /* Window stuff */
    p_ptr->window |= (PW_MESSAGE);

    /* Copy it */
    SDL_strlcpy(buf, msg, sizeof(buf));

    /* Analyze the buffer */
    t = buf;

    /* Get the color of the message */
    color = message_type_color(type);

    /* Split message */
    while (n > (w - 8))
    {
        char oops;

        int check, split;

        /* Default split */
        split = (w - 8);

        /* Find the "best" split point */
        for (check = (w / 2); check < (w - 8); check++)
        {
            /* Found a valid split point */
            if (t[check] == ' ')
                split = check;
        }

        /* Save the split character */
        oops = t[split];

        /* Split the message */
        t[split] = '\0';

        /* Display part of the message */
        Term_putstr(0, 0, split, color, t);

        /* Flush it */
        msg_flush(split + 1);

        /* Restore the split character */
        t[split] = oops;

        /* Insert a space */
        t[--split] = ' ';

        /* Prepare to recurse on the rest of "buf" */
        t += split;
        n -= split;
    }

    /* Display the tail of the message */
    Term_putstr(message_column, 0, n, color, t);
    message_topline_append(t, type, color);

    /* Remember the message */
    msg_flag = true;

    /* Remember the position */
    message_column += n + 1;

    /* Optional refresh */
    if (fresh_after)
        Term_fresh();

    app_session_mark_snapshot_dirty(app_session_current(),
        APP_SNAPSHOT_INVALIDATE_MESSAGES);
}

/*
 * Print a message in the default color (white)
 */
void msg_print(cptr msg) { msg_print_aux(MSG_GENERIC, msg); }

/*
 * Display a formatted message, using "vstrnfmt()" and "msg_print()".
 */
void msg_format(cptr fmt, ...)
{
    va_list vp;

    char buf[1024];

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Display */
    msg_print_aux(MSG_GENERIC, buf);
}

/*
 * Display a message many times, using "vstrnfmt()" and "msg_print()".
 */
void msg_debug(cptr fmt, ...)
{
    va_list vp;

    char buf[1024];
    char buf2[1030]; /* Slightly larger to accommodate "<< >>" wrapper */

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    snprintf(buf2, sizeof(buf2), "<< %s >>", buf);

    /* Display */
    msg_print_aux(MSG_GENERIC, buf2);
    message_flush();
}

/*
 * Display a message and play the associated sound.
 *
 * The "extra" parameter is currently unused.
 */
void message(u16b message_type, s16b extra, cptr message)
{
    /* Unused parameter */
    (void)extra;

    sound(message_type);

    msg_print_aux(message_type, message);
}

/*
 * Display a formatted message and play the associated sound.
 *
 * The "extra" parameter is currently unused.
 */
void message_format(u16b message_type, s16b extra, cptr fmt, ...)
{
    va_list vp;

    char buf[1024];

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Display */
    message(message_type, extra, buf);
}

/*
 * Print the queued messages.
 */
void message_flush(void)
{
    /* Hack -- Reset */
    if (!msg_flag)
        message_column = 0;

    /* Flush when needed */
    if (message_column)
    {
        /* Print pending messages */
        msg_flush(message_column);

        /* Forget it */
        msg_flag = false;

        /* Reset */
        message_column = 0;
    }
}

/*
 * Hack -- prevent "accidents" in "screen_save()" or "screen_load()"
 */
static int screen_depth = 0;

/*
 * Save the screen, and increase the "icky" depth.
 *
 * This function must match exactly one call to "screen_load()".
 */
void screen_save(void)
{
    /* Hack -- Flush messages */
    message_flush();

    /* Log line 0 state before save */
    if (Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("screen_save: BEFORE save row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            buffer_content,
            scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
            scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9],
            scr_story[10]);
    }

    /* Save the screen (if legal) */
    if (screen_depth++ == 0)
        Term_save();

    /* Increase "icky" depth */
    character_icky++;
    log_debug("screen_save: character_icky incremented to %d, screen_depth=%d",
        character_icky, screen_depth);
}

/*
 * Load the screen, and decrease the "icky" depth.
 *
 * This function must match exactly one call to "screen_save()".
 */
void screen_load(void)
{
    /* Hack -- Flush messages */
    message_flush();

    /* Load the screen (if legal) */
    if (--screen_depth == 0)
        Term_load();

    /* Decrease "icky" depth */
    character_icky--;
    log_debug("screen_load: character_icky decremented to %d, screen_depth=%d",
        character_icky, screen_depth);

    /* Log line 0 state after load */
    if (Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("screen_load: AFTER load row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            buffer_content,
            scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
            scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9],
            scr_story[10]);
    }
}
