/* File: cmd4.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "melee/melee-combat-display.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "externs.h"

#include <ctype.h>

/* max length of note output */
#define LINEWRAP 75

/*
 * This command performs various low level updates, clears all the "extra"
 * windows, does a total redraw of the main window, and requests all of the
 * interesting updates and redraws that I can think of.
 *
 * This command is also used to "instantiate" the results of the user
 * selecting various things, such as graphics mode, so it must call
 * the "TERM_XTRA_REACT" hook before redrawing the windows.
 */
void do_cmd_redraw(void)
{
    input_clear_pending();

    if (g_banner_force_redraw_remaining <= 0)
        clear_active_narrative_banner();

    /* Hack -- React to changes */
    platform_frame_react();

    /* Combine and Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update stuff */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw everything */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window
        |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT | PW_MONLIST);

    /* Hack -- update */
    handle_stuff();

    /* Rebuild the semantic combat overlay/panes after a full redraw. */
    refresh_main_combat_overlay();
}

/*
 * Take notes.  There are two ways this can happen, either in the message recall
 * or a file.  The command can also be passed a string, which will automatically
 * be written. -CK-
 */
void do_cmd_note(char* note, int what_depth)
{
    char buf[120];
    char turn_string[16];

    int length, length_info;
    char info_note[40];
    char depths[10];

    /* Default */
    SDL_strlcpy(buf, "", sizeof(buf));

    /* If a note is passed, use that, otherwise accept user input. */
    if (streq(note, ""))
    {
        if (!prompt_text_input("Note:",
                "Enter accepts, Esc cancels, Backspace erases.", buf, 57,
                false))
        {
            return;
        }
    }
    else
    {
        SDL_strlcpy(buf, note, sizeof(buf));
    }

    /* Ignore empty notes */
    if (!buf[0] || (buf[0] == ' '))
        return;

    /* Artefacts use depth artefact created.  All others use player depth. */
    if (what_depth == 0)
    {
        SDL_strlcpy(depths, "   Gates", sizeof(depths));
    }
    else if (what_depth == CHEST_LEVEL)
    {
        SDL_strlcpy(depths, "   Chest", sizeof(depths));
    }
    else if (what_depth == SKELETON_LEVEL)
    {
        SDL_strlcpy(depths, "   Skeleton", sizeof(depths));
    }
    else
    {
        comma_number(depths, what_depth * 50);
        strnfmt(depths, sizeof(depths), "%5s ft", depths);
    }

    comma_number(turn_string, playerturn);

    /* Make preliminary part of note */
    strnfmt(info_note, sizeof(info_note), "%7s  %s   ", turn_string, depths);

    /* Write the info note */
    SDL_strlcat(notes_buffer, info_note, sizeof(notes_buffer));

    /* Get the length of the notes */
    length_info = strlen(info_note);
    length = strlen(buf);

    /* Break up long notes */
    if ((length + length_info) > LINEWRAP)
    {
        bool keep_going = true;
        int startpoint = 0;
        int endpoint, n;

        while (keep_going)
        {
            endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;

            while (true)
            {
                if (endpoint >= length)
                {
                    endpoint = length;
                    keep_going = false;
                    break;
                }
                else if ((buf[endpoint] == ' ') || (buf[endpoint] == '-'))
                {
                    break;
                }
                else if (endpoint == startpoint)
                {
                    endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;
                    break;
                }

                endpoint--;
            }

            if (startpoint)
                SDL_strlcat(notes_buffer, "                    ",
                    sizeof(notes_buffer));

            for (n = startpoint; n <= endpoint; n++)
            {
                char ch = (isprint(buf[n]) ? buf[n] : ' ');
                SDL_strlcat(notes_buffer, format("%c", ch),
                    sizeof(notes_buffer));
            }

            SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));
            startpoint = endpoint + 1;
        }
    }
    else
    {
        SDL_strlcat(notes_buffer, format("%s\n", buf), sizeof(notes_buffer));
    }
}

/*
 * Mention the current version
 */
void do_cmd_version(void)
{
    char verbuf[128];

    strnfmt(verbuf, sizeof(verbuf),
        "You are playing %s %s.  Type '?' for more info.", VERSION_NAME,
        VERSION_STRING);
    msg_print(verbuf);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_feeling_text[LEV_THEME_HEAD]
    = { "Looks like any other level.",
          "You feel there is something special about this level.",
          "You have a superb feeling about this level.",
          "You have an excellent feeling...", "You have a very good feeling...",
          "You have a good feeling...", "You feel strangely lucky...",
          "You feel your luck is turning...",
          "You like the look of this place...",
          "This level can't be all bad...", "What a boring place..." };

/*
 * Note that "feeling" is set to zero unless some time has passed.
 * Note that this is done when the level is GENERATED, not entered.
 */
void do_cmd_feeling(void)
{
    if (!p_ptr->depth)
    {
        msg_print("You stand once again upon the surface. Freedom awaits.");
        return;
    }

    if (!do_feeling)
    {
        msg_print("You are still uncertain about this level...");
        return;
    }

    msg_print(do_cmd_feeling_text[feeling]);
}

/*
 * Array of challenge strings
 */
static cptr do_cmd_challenge_text[14]
    = { "challenges you from beyond the grave!",
          "thunders 'Prove worthy of your traditions - or die ashamed!'.",
          "desires to test your mettle!",
          "has risen from the dead to test you!",
          "roars 'Fight, or know yourself for a coward!'.",
          "summons you to a duel of life and death!",
          "desires you to know that you face a mighty champion of yore!",
          "demands that you prove your worthiness in combat!",
          "calls you unworthy of your ancestors!",
          "challenges you to a deathmatch!", "walks Middle-Earth once more!",
          "challenges you to demonstrate your prowess!",
          "demands you prove yourself here and now!",
          "asks 'Can ye face the best of those who came before?'." };

/*
 * Personalize, randomize, and announce the challenge of a player ghost. -LM-
 */
void ghost_challenge(void)
{
    monster_race* r_ptr = &r_info[r_ghost];

    if (ghost_name[0] == '\0')
    {
        bones_selector = 0;
        return;
    }

    msg_format("%^s, the %^s %s", ghost_name, r_name + r_ptr->name,
        do_cmd_challenge_text[rand_int(14)]);

    message_flush();
}
