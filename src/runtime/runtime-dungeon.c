/* File: dungeon.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "app/app-command.h"
#include "app/app-scene-birth.h"
#include "app/app-session.h"
#include "app/app-ui.h"
#include "cmd/combat/cmd-ranged.h"
#include "cmd/debug/cmd-debug.h"
#include "cmd/world/cmd-world.h"
#include "fs/load.h"
#include "object/object-ui-select.h"
#include "blitz.h"
#include "log/log.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "platform-time.h"
#include "runtime/runtime-cli.h"
#include "runtime/runtime-game.h"
#include "runtime/runtime-dungeon-internal.h"
#include "player/killer.h"
#include "metarun.h"
#include "drop_system.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_runs.h"
#include "ui/ui-story.h"
#include "ui/ui-information-scene.h"
#include "ui/smithing/ui-smithing-screen.h"
#include "score/score_ui.h"
#include <time.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

static bool dungeon_poll_pending_key(void) { return inkey_can_consume_immediately(); }

/* Reset dungeon-only static state so a fresh run starts cleanly. */
void reset_dungeon_state(void)
{
    runtime_dungeon_reset_vault_transition_state();
    runtime_dungeon_reset_presentation_state();
}

/*
 * Parse and execute the current command
 * Give "Warning" on illegal commands.
 */
void process_command(void)
{
    log_trace("process_command: character_icky=%d, command='%c' (%d)",
              character_icky, p_ptr->command_cmd, (int)p_ptr->command_cmd);

    /* Debug: Log character_icky state but don't aggressively reset it during normal operation */
    if (character_icky > 0) {
        log_debug("process_command: character_icky is %d (may be normal during menu operations)", character_icky);
    }

#ifdef ALLOW_REPEAT

    /* Handle repeating the last command */
    repeat_check();

#endif /* ALLOW_REPEAT */

    /* Disallow actions that would advance time while viewing the final map. */
    if (death_spectator_active()
        && !runtime_dungeon_death_spectator_command_allowed(p_ptr->command_cmd))
    {
        if (p_ptr->command_cmd)
        {
            msg_print("You can no longer take that action.");
        }
        p_ptr->command_cmd = 0;
        return;
    }

    /* Parse the command */
    switch (p_ptr->command_cmd)
    {
    /* Ignore */
    case ' ':
    case '\n':
    case '\r':
    case '\a':
    {
        break;
    }

    /*** Cheating Commands ***/

    /* Toggle Wizard Mode */
    case KTRL('W'):
    {
        if (p_ptr->wizard)
        {
            p_ptr->wizard = false;
            msg_print("Wizard mode off.");
            p_ptr->update |= (PU_BONUS);
        }
        else if (enter_wizard_mode())
        {
            p_ptr->wizard = true;
            msg_print("Wizard mode on.");
            p_ptr->update |= (PU_BONUS);
        }

        /* Update monsters */
        p_ptr->update |= (PU_MONSTERS);

        break;
    }

#ifdef ALLOW_DEBUG

    /* Special "debug" commands */
    case KTRL('A'):
    {
        if (verify_debug_mode())
        {
            log_info("Ctrl-A debug menu opened (wizard=%d, noscore=0x%04X, savefile='%s')",
                     p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);
            do_cmd_debug();
        }
        break;
    }

#endif

    /*** Inventory Commands ***/

    /* Wear/wield equipment */
    case 'w':
    {
        do_cmd_wield_wrapper();
        break;
    }

    /* Remove equipment */
    case 'r':
    {
        do_cmd_takeoff(NULL, 0);
        break;
    }

    /* Drop an item */
    case 'd':
    {
        do_cmd_drop();
        break;
    }

    /* Destroy an item */
    case 'k':
    {
        do_cmd_destroy();
        break;
    }

    /* Equipment list */
    /* Equipment list */
    case 'e':
    {
        do_cmd_equip_direct();
        break;
    }

    /* Inventory list */
    case 'i':
    {
        do_cmd_inven_direct();
        break;
    }

    /* Sing */
    case 's':
    {
        do_cmd_change_song();
        break;
    }

    /* Ability screen */
    case '\t':
    {
        do_cmd_ability_screen();
        
        /* Force a full redraw after returning from the ability UI. */
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
        break;
    }

    /* Smithing screen */
    case '0':
    case KTRL('D'): // Hack: required to get Angband-like keyset to work
    {
        do_cmd_smithing_screen();
        break;
    }

    /*** Various commands ***/

    /* Examine an object */
    case 'x':
    {
        do_cmd_observe();
        break;
    }

    /* Hack -- toggle windows */
    case KTRL('E'):
    {
        toggle_inven_equip();
        break;
    }

    /*** Standard "Movement" Commands ***/

    /* Alternate action */
    case '/':
    {
        do_cmd_alter();
        break;
    }

    /* Dig a tunnel */
    case 'T':
    {
        do_cmd_tunnel();
        break;
    }

    /* Walk */
    case ';':
    {
        do_cmd_walk();
        break;
    }

    /*** Running, Resting, Searching, Staying */

    /* Begin Running -- Arg is Max Distance */
    case '.':
    {
        do_cmd_run();
        break;
    }

    /* Hold still */
    case 'z':
    {
        do_cmd_hold();
        break;
    }

    /* Rest */
    case '%':
    case 'Z':
    {
        do_cmd_rest();
        break;
    }

    /* Get */
    case 'g':
    {
        do_cmd_pickup();
        break;
    }

    /* Toggle stealth mode */
    case 'S':
    {
        do_cmd_toggle_stealth();
        break;
    }

    /*** Stairs and Doors and Chests and Traps ***/

    /* Go up staircase */
    case '<':
    {
        // Autosave
        save_game_quietly = true;
        do_cmd_save_game();

        do_cmd_go_up();
        break;
    }

    /* Go down staircase */
    case '>':
    {
        // Autosave
        save_game_quietly = true;
        do_cmd_save_game();

        do_cmd_go_down();
        break;
    }

    /* Open a door or chest */
    case 'o':
    {
        do_cmd_open();
        break;
    }

    /* Close a door */
    case 'c':
    {
        do_cmd_close();
        break;
    }

    /* Bash a door */
    case 'b':
    {
        do_cmd_bash();
        break;
    }

    /* Disarm a trap or chest */
    case 'D':
    {
        do_cmd_disarm();
        break;
    }

    /* Exchange places */
    case 'X':
    {
        do_cmd_exchange();
        break;
    }

    case '-':
    {
        do_cmd_fletchery();
        break;
    }

    /*** Use various objects ***/

    /* Inscribe an object */
    case '{':
    {
        do_cmd_inscribe();
        break;
    }

    /* Activate a staff */
    case 'a':
    {
        do_cmd_activate_staff(NULL, 0);
        break;
    }

    /* Eat some food */
    case 'E':
    {
        do_cmd_eat_food(NULL, 0);
        break;
    }

    /* Fire an arrow from the 1st quiver */
    case 'f':
    {
        do_cmd_fire(1);
        break;
    }

    /* Fire an arrow from the 2nd quiver */
    case 'F':
    {
        do_cmd_fire(2);
        break;
    }

    /* Throw an item */
    case 't':
    {
        do_cmd_throw(false);
        break;
    }

        /* Throw an automatically chosen item at nearest target */
    case KTRL('T'):
    {
        do_cmd_throw(true);
        break;
    }

    /* Play an instrument */
    case 'p':
    {
        do_cmd_play_instrument(NULL, 0);
        break;
    }

    /* Quaff a potion */
    case 'q':
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true, true);
        break;
    }

    /* Use an item */
    case 'u':
    {
        do_cmd_use_item();
        break;
    }

    /*** Looking at Things (nearby or on map) ***/

    /* Full dungeon map */
    case 'M':
    {
        do_cmd_view_map();
        break;
    }

    /* Locate player on map */
    case 'L':
    {
        do_cmd_locate();
        break;
    }

    /* Look around */
    case 'l':
    {
        do_cmd_look();
        break;
    }

    /* Target monster or location */
    // case '*':
    //{
    //	do_cmd_target();
    //	break;
    //}

    /*** Help and Such ***/

    /* Help */
    case '?':
    {
        do_cmd_help();
        break;
    }

    /* Character sheet (alternative key) */
    case 'h':
    {
        do_cmd_character_sheet();
        break;
    }
    
    /* Direct access to skill distribution */
    case 'H':
    {
        gain_skills();
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
        break;
    }

    /* Main menu */
    case 'm':
    {
        do_cmd_main_menu();
        break;
    }
    case ESCAPE:
    {
        log_debug("[metarun-esc-trace] process_command esc easy_main_menu=%d character_icky=%d",
            easy_main_menu ? 1 : 0, character_icky ? 1 : 0);
        if (easy_main_menu)
            do_cmd_main_menu();
        break;
    }

    /* Identify symbol */
    // case '/':
    //{
    //	do_cmd_query_symbol();
    //	break;
    //}

    /* Character sheet */
    case '@':
    {
        do_cmd_character_sheet();
        break;
    }

    /*** System Commands ***/

    /* Interact with visuals */
    // case '%':
    //{
    //	do_cmd_visuals();
    //	break;
    //}

    /* Interact with options */
    case 'O':
    {
        do_cmd_options();
        do_cmd_redraw();
        break;
    }

    /*** Misc Commands ***/

    /* Take notes */
    case ':':
    {
        do_cmd_note("", p_ptr->depth);
        break;
    }

    /* Show previous message */
    case KTRL('O'):
    {
        do_cmd_message_one();
        break;
    }

    /* Show previous messages */
    case KTRL('P'):
    {
        do_cmd_messages();
        break;
    }

    /* Redraw the screen */
    case KTRL('R'):
    {
        do_cmd_redraw();
        break;
    }

#ifndef VERIFY_SAVEFILE

    /* Hack -- Save and don't quit */
    case KTRL('S'):
    {
        do_cmd_save_game();
        break;
    }

#endif

    /* Save and quit */
    case KTRL('X'):
    case KTRL('C'):
    {
        /* Stop playing */
        p_ptr->playing = false;

        /* Leaving */
        p_ptr->leaving = true;
        break;
    }

    /* Supplies overview */
    case 'j':
    {
        do_cmd_knowledge_supplies(NULL);
        break;
    }

    /* Check knowledge */
    case '~':
    {
        do_cmd_knowledge();
        break;
    }

    case '[':
    {
        do_cmd_view_monsters();
        break;
    }

    case ']':
    {
        do_cmd_view_objects();
        break;
    }

    /* Hack -- Unknown command */
    default:
    {
        msg_print("Type '?' for help.");
        break;
    }
    }
}

static bool auto_pickup_okay(const object_type* o_ptr)
{
    int max_qty;
    // cptr s;

    /* It can't be carried */
    if (!inven_carry_okay(o_ptr))
        return (false);

    /*
     * Don't interrupt movement with a quantity prompt when a supply stack
     * only fits partially. The player can still pick it up manually.
     */
    if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
    {
        max_qty = supplies_max_absorbable_quantity(o_ptr);
        if ((max_qty > 0) && (max_qty < o_ptr->number))
            return (false);
    }

    /* object has pickup flag set */
    if (o_ptr->pickup)
        return (true);

    /* No inscription */
    if (!o_ptr->obj_note)
        return (false);

    /* Find a '=' */
    // s = strchr(quark_str(o_ptr->obj_note), '=');

    /* Process inscription */ // Sil-y: turned the =g inscriptions off for now
    // while (s)
    //{
    //	/* Auto-pickup on "=g" */
    //	if (s[1] == 'g') return (true);

    //	/* Find another '=' */
    //	s = strchr(s + 1, '=');
    //}

    /* Don't auto pickup */
    return (false);
}

/*
 * Finish your leap
 */
void land(void)
{
    // the player has landed
    p_ptr->leaping = false;

    // make some noise when landing
    stealth_score -= 5;

    /* Set off traps */
    if (cave_trap_bold(p_ptr->py, p_ptr->px)
        || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[p_ptr->py][p_ptr->px] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(p_ptr->py, p_ptr->px);
        }

        /* Hit the trap */
        hit_trap(p_ptr->py, p_ptr->px);
    }
}

/*
 * Continue your leap
 */
static void continue_leap(void)
{
    int dir;
    int y_end, x_end; // the desired endpoint of the leap

    dir = p_ptr->previous_action[1];

    /* Get location */
    y_end = p_ptr->py + ddy[dir];
    x_end = p_ptr->px + ddx[dir];

    // display a message until player input is received
    msg_print("You fly through the air.");
    message_flush();

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = dir;

    // solid objects end the leap
    if (cave_info[y_end][x_end] & (CAVE_WALL))
    {
        if (cave_feat[y_end][x_end] == FEAT_RUBBLE)
        {
            msg_print("You slam into a wall of rubble.");
        }
        if (cave_wall_bold(y_end, x_end))
        {
            msg_print("You slam into a wall.");
        }
        else if (cave_any_closed_door_bold(y_end, x_end))
        {
            msg_print("You slam into a door.");
        }
    }

    // monsters end the leap
    else if (cave_m_idx[y_end][x_end] > 0)
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y_end][x_end]];
        char m_name[80];

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        if (m_ptr->ml)
            msg_format("%^s blocks your landing.", m_name);
        else
            msg_format("Some unseen foe blocks your landing.", m_name);
    }

    // successful leap
    else
    {
        // we generously give you your free flanking attack...
        flanking_or_retreat(y_end, x_end);

        // move player to the new position
        monster_swap(p_ptr->py, p_ptr->px, y_end, x_end);
    }

    // land on the ground
    land();
}

/*
 * Hack -- helper function for "process_player()"
 *
 * Check for changes in the "monster memory"
 */
static void process_player_aux(void)
{
    int i;
    bool changed = false;

    static int old_monster_race_idx = 0;

    static u32b old_flags1 = 0L;
    static u32b old_flags2 = 0L;
    static u32b old_flags3 = 0L;
    static u32b old_flags4 = 0L;

    static byte old_blows[MONSTER_BLOW_MAX];

    static byte old_ranged = 0;

    /* Tracking a monster */
    if (p_ptr->monster_race_idx)
    {
        /* Get the monster lore */
        monster_lore* l_ptr = &l_list[p_ptr->monster_race_idx];

        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            if (old_blows[i] != l_ptr->blows[i])
            {
                changed = true;
                break;
            }
        }

        /* Check for change of any kind */
        if (changed || (old_monster_race_idx != p_ptr->monster_race_idx)
            || (old_flags1 != l_ptr->flags1) || (old_flags2 != l_ptr->flags2)
            || (old_flags3 != l_ptr->flags3) || (old_flags4 != l_ptr->flags4)
            || (old_ranged != l_ptr->ranged))

        {
            /* Memorize old race */
            old_monster_race_idx = p_ptr->monster_race_idx;

            /* Memorize flags */
            old_flags1 = l_ptr->flags1;
            old_flags2 = l_ptr->flags2;
            old_flags3 = l_ptr->flags3;
            old_flags4 = l_ptr->flags4;

            /* Memorize blows */
            for (i = 0; i < MONSTER_BLOW_MAX; i++)
                old_blows[i] = l_ptr->blows[i];

            /* Memorize castings */
            old_ranged = l_ptr->ranged;

            /* Window stuff */
            p_ptr->window |= (PW_MONSTER);

            /* Window stuff */
            window_stuff();
        }
    }
}

/*
 * Process the player
 *
 * Notice the annoying code to handle "pack overflow", which
 * must come first just in case somebody manages to corrupt
 * the savefiles by clever use of menu commands or something.
 *
 * Notice the annoying code to handle "monster memory" changes,
 * which allows us to avoid having to update the window flags
 * every time we change any internal monster memory field, and
 * also reduces the number of times that the recall window must
 * be redrawn.
 *
 * Note that the code to check for user abort during repeated commands
 * and running and resting can be disabled entirely with an option, and
 * even if not disabled, it will only check during every 128th game turn
 * while resting, for efficiency.
 */
static void process_player(void)
{
    int i;
    int amount;
    int regen_multiplier;
    int depth_counter_increment;

    // reset the number of times you have riposted since last turn
    p_ptr->ripostes = 0;

    // reset whether you have just woken up from entrancement
    p_ptr->was_entranced = false;

    // update the player's torch radius
    calc_torch();

    listen_hint_new_player_turn();
    song_disguise_new_player_turn();
    song_duels_new_player_turn();

    /*** Check certain things between player turns (don't need to do this when
     * restoring a game) ***/

    if (!p_ptr->restoring)
    {
        /*** Check for interrupts ***/

        /* Complete resting */
        if (p_ptr->resting < 0)
        {
            /* Basic resting */
            if (p_ptr->resting == -1)
            {
                /* Stop resting */
                if ((p_ptr->chp == p_ptr->mhp) && (p_ptr->csp == p_ptr->msp))
                {
                    disturb(0, 0);
                }
            }

            /* Complete resting */
            else if (p_ptr->resting == -2)
            {
                /* Stop resting */
                if ((p_ptr->chp == p_ptr->mhp)
                    && ((p_ptr->csp == p_ptr->msp) || !singing(SNG_NOTHING))
                    && !p_ptr->blind && !p_ptr->confused && !p_ptr->poisoned
                    && !p_ptr->afraid && !p_ptr->stun && !p_ptr->cut
                    && !p_ptr->slow && !p_ptr->entranced)
                {
                    disturb(0, 0);
                }
            }
        }

        /* Check for "player abort" */
        if (p_ptr->running || p_ptr->fletching || p_ptr->smithing
            || p_ptr->command_rep || (p_ptr->resting && !(turn & 0x7F)))
        {
            /* Check for a key */
            if (dungeon_poll_pending_key())
            {
                app_session* session = app_session_current();

                /* Flush input */
                app_command_clear_pending();
                platform_frame_flush_events();
                if (session)
                    app_session_clear_inputs(session);

                /* Disturb */
                disturb(0, 0);

                /* Hack -- Show a Message */
                msg_print("Cancelled.");
            }
        }

        /*** Other checks ***/

        do_betrayal_ring_amulet();

        // Make the stealth-modified noise (has to occur after monsters have had
        // a chance to move)
        monster_perception(true, true, stealth_score);

        // Stop stealth mode if something happened
        if (stop_stealth_mode)
        {
            /* Cancel */
            p_ptr->stealth_mode = false;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);

            // Reset the flag
            stop_stealth_mode = false;
        }

        // Morgoth will announce a challenge if adjacent
        if (p_ptr->truce && (p_ptr->depth == MORGOTH_DEPTH))
        {
            int d, yy, xx;

            /* Check around the character */
            for (d = 0; d < 8; d++)
            {
                monster_type* m_ptr;

                /* Extract adjacent (legal) location */
                yy = p_ptr->py + ddy_ddd[d];
                xx = p_ptr->px + ddx_ddd[d];

                // paranoia
                if (cave_m_idx[yy][xx] < 0)
                    continue;

                m_ptr = &mon_list[cave_m_idx[yy][xx]];

                if ((m_ptr->r_idx == R_IDX_MORGOTH)
                    && (m_ptr->alertness >= ALERTNESS_ALERT))
                {
                    msg_print("With a voice as of rolling thunder, Morgoth, "
                              "Lord of Darkness, "
                              "speaks:");
                    msg_print("'You dare challenge me in mine own hall? Now is "
                              "your death upon "
                              "you!'");

                    // Break the truce (always)
                    break_truce(true);
                }
            }
        }

        /* List all challenge options at the start of the game */
        if (playerturn == 1)
        {
            for (i = 0; i < OPT_PAGE_PER; i++)
            {
                int option_number = option_page[CHALLENGE_PAGE][i];

                /* Collect options on this "page" */
                if ((option_number != OPT_NONE) && (op_ptr->opt[option_number]))
                {
                    do_cmd_note(
                        format("Challenge: %s", option_desc[option_number]),
                        p_ptr->depth);
                }
            }
        }

        if (p_ptr->previous_action[0] != ACTION_ARCHERY)
        {
            p_ptr->killed_enemy_with_arrow = false;
            p_ptr->redraw |= PR_ARC;
        }

        // shuffle along the array of previous actions
        for (i = ACTION_MAX - 1; i > 0; i--)
        {
            p_ptr->previous_action[i] = p_ptr->previous_action[i - 1];
        }
        // put in a default for this turn
        // Sil-y: it is possible that this isn't always changed to something
        // else, but I think it is
        p_ptr->previous_action[0] = ACTION_NOTHING;

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        // Sil-y: have to update the player bonuses at every turn with
        // sprinting, dodging etc.
        //        this might cause annoying slowdowns, I'm not sure
        p_ptr->update |= (PU_BONUS);
    }

    /*** Handle actual user input ***/

    /* Repeat until energy is reduced */
    do
    {
        u32b update_mask = p_ptr->update;
        u32b redraw_mask = p_ptr->redraw;
        u32b window_mask = p_ptr->window;

        /* Notice stuff (if needed) */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff (if needed) */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            dungeon_note_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        if (cheat_noise)
            display_noise_map();
        else if (cheat_scent)
            display_scent_map();
        else if (cheat_light)
            display_light_map();

        runtime_dungeon_publish_runtime_snapshot(update_mask, redraw_mask,
            window_mask);

        /* Hack -- Pack Overflow if needed */
        check_pack_overflow();

        if (cave_o_idx[p_ptr->py][p_ptr->px] != 0)
        {
            (&o_list[cave_o_idx[p_ptr->py][p_ptr->px]])->marked = true;
        }

        /* Hack -- cancel "lurking browse mode" */
        if (!p_ptr->command_new)
            p_ptr->command_see = false;

        /* Assume free turn */
        p_ptr->energy_use = 0;

    // Reset number of attacks this turn happens at start of player energy loop

        // get base stealth score for the round
        // this will get modified by the type of action
        stealth_score = p_ptr->skill_use[S_STL];

        // display a note at the start of the game
        if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
        {
            object_type* o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            if ((o_ptr->tval == TV_NOTE) && (playerturn == 1))
            {
                note_info_screen(o_ptr);
            }
        }

        /* Leaping */
        if (p_ptr->leaping)
        {
            continue_leap();
        }

        /* Entranced or Knocked Out */
        else if ((p_ptr->entranced) || (p_ptr->stun > 100))
        {
            // stop singing
            change_song(SNG_NOTHING);

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;
        }

        /* Smithing */
        else if (p_ptr->smithing)
        {
            if (p_ptr->smithing == 1)
            {
                // Display a message
                msg_print("You complete your work.");

                create_smithing_item();

                /* Aule quest: check for success condition during forging */
                {
                    int diff = object_difficulty(smith_o_ptr);
                    p_ptr->aule_last_object_diff = diff;
                    if (diff > 20 && p_ptr->aule_quest == AULE_QUEST_ACTIVE) {
                        p_ptr->aule_quest = AULE_QUEST_SUCCESS;
                        log_trace("Aule quest: state -> SUCCESS (diff=%d)", diff);
                        msg_print("Your forging radiates unparalleled craft!");
                        msg_print("You sense that Aule would be pleased with this work...");
                        msg_print("Seek out Aule to receive his blessing.");
                    }
                }
            }

            /* Reduce smithing count */
            p_ptr->smithing--;

            /* Reduce smithing leftover counter */
            p_ptr->smithing_leftover--;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);
        }
    /* Aule quest: no longer requires standing at special forge; acceptance handled during forging */

        /* Fletching */
        else if (p_ptr->fletching)
        {
            if (p_ptr->fletching == 1)
            {
                // Display a message
                msg_print("You complete your work.");

                finish_fletching(0);
            }

            /* Reduce fletching count */
            p_ptr->fletching--;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);
        }

        /* Resting */
        else if (p_ptr->resting)
        {
            /* Timed rest */
            if (p_ptr->resting > 0)
            {
                /* Reduce rest count */
                p_ptr->resting--;

                /* Redraw the state */
                p_ptr->redraw |= (PR_STATE);
            }

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = 5;

            // store the 'focus' attribute
            p_ptr->focused = true;

            /* Searching */
            search();
        }

        /* Recovering footing */
        else if (p_ptr->skip_next_turn)
        {
            // let the player know
            if (p_ptr->knocked_back)
            {
                msg_print("You recover your footing.");

                // force a -more-
                message_flush();
                p_ptr->knocked_back = false;
            }

            // reset flag
            p_ptr->skip_next_turn = false;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            // Pause to show enemies moving.
            platform_delay_ms(500u);
        }

        /* Running */
        else if (p_ptr->running)
        {
            /* Take a step */
            run_step(0);

            // Pause for 17 miliseconds (minimum needed for mac OS X to pause)
            if (!instant_run)
            {
                platform_delay_ms(17u);
            }
        }

        /* Repeated command */
        else if (p_ptr->command_rep)
        {
            /* Hack -- Assume messages were seen */
            msg_flag = false;

            /* Clear the top line */
            message_topline_clear_override();

            /* Process the command */
            process_command();

            /* Count this execution */
            if (p_ptr->command_rep)
            {
                /* Count this execution */
                p_ptr->command_rep--;

                /* Redraw the state */
                p_ptr->redraw |= (PR_STATE);
            }
        }

        /* Normal command */
        else
        {
            char out_val[160];
            char o_name[80];
            object_type* o_ptr;

            // build an object description
            if (cave_o_idx[p_ptr->py][p_ptr->px])
            {
                o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                strnfmt(out_val, sizeof(out_val), "Pick up %s? ", o_name);
            }

            // always offer to pickup if the mode is on, there is an object
            // present, and you have just moved
            if (always_pickup && cave_o_idx[p_ptr->py][p_ptr->px]
                && (o_ptr->tval != TV_NOTE) && (p_ptr->previous_action[1] >= 1)
                && (p_ptr->previous_action[1] <= 9)
                && (p_ptr->previous_action[1] != 5))
            {
                // allow the player to decline to pick up the object
                if (get_check(out_val))
                {
                    /* Handle "objects" */
                    py_pickup();
                }
            }

            // if the player hasn't used their turn picking something up...
            if (p_ptr->energy_use < 100)
            {
                /* Check monster recall */
                process_player_aux();

                /* Place the cursor on the player or target */
                if (hilite_player)
                    dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
                if (hilite_target && target_sighted())
                    dungeon_note_cursor_relative(p_ptr->target_row,
                        p_ptr->target_col);

                /* We are certainly no longer in the process of restoring a game
                 */
                p_ptr->restoring = false;

                /* Get a command (normal) */
                app_request_player_command();

                /* Process the command */
                process_command();
            }

            // check the item under the player
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

            /* Test for auto-pickup for thrown/fired items */
            if (auto_pickup_okay(o_ptr))
            {
                /* Pick up the object */
                py_pickup_aux(cave_o_idx[p_ptr->py][p_ptr->px]);
            }
        }

        /*** Clean up ***/

        /* Update labyrinth map restriction and partition-entry messages/XP. */
        runtime_dungeon_update_labyrinth_view_state(true);
        runtime_dungeon_handle_partition_entry(false);
        runtime_dungeon_handle_vault_transition();

        /* Significant */
        if (p_ptr->energy_use)
        {
            /* Use some energy */
            p_ptr->energy -= p_ptr->energy_use;

            /* Hack -- constant hallucination */
            if (p_ptr->image)
                p_ptr->redraw |= (PR_MAP);

            /* Shimmer monsters if needed */
            if (shimmer_monsters)
            {
                /* Clear the flag */
                shimmer_monsters = false;

                /* Shimmer multi-hued monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;
                    monster_race* r_ptr;

                    /* Get the monster */
                    m_ptr = &mon_list[i];

                    /* Skip dead monsters */
                    if (!m_ptr->r_idx)
                        continue;

                    /* Get the monster race */
                    r_ptr = &r_info[m_ptr->r_idx];

                    /* Skip non-multi-hued monsters */
                    if (!(r_ptr->flags1 & (RF1_ATTR_MULTI)))
                        continue;

                    /* Reset the flag */
                    shimmer_monsters = true;

                    /* Redraw regardless */
                    dungeon_mark_map_for_redraw();
                }
            }

            /* Repair "mark" flags */
            if (repair_mflag_mark)
            {
                /* Reset the flag */
                repair_mflag_mark = false;

                /* Process the monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;

                    /* Get the monster */
                    m_ptr = &mon_list[i];

                    /* Skip dead monsters */
                    /* if (!m_ptr->r_idx) continue; */

                    /* Repair "mark" flag */
                    if (m_ptr->mflag & (MFLAG_MARK))
                    {
                        /* Skip "show" monsters */
                        if (m_ptr->mflag & (MFLAG_SHOW))
                        {
                            /* Repair "mark" flag */
                            repair_mflag_mark = true;

                            /* Skip */
                            continue;
                        }

                        /* Forget flag */
                        m_ptr->mflag &= ~(MFLAG_MARK);

                        /* Update the monster */
                        update_mon(i, false);
                    }
                }
            }
        }

        /* Repair "show" flags */
        if (repair_mflag_show)
        {
            /* Reset the flag */
            repair_mflag_show = false;

            /* Process the monsters */
            for (i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr;

                /* Get the monster */
                m_ptr = &mon_list[i];

                /* Skip dead monsters */
                /* if (!m_ptr->r_idx) continue; */

                /* Clear "show" flag */
                m_ptr->mflag &= ~(MFLAG_SHOW);
            }
        }
    } while (!p_ptr->energy_use && !p_ptr->leaving);

    // if the player is exiting the the game in some manner then stop processing
    // now
    if (p_ptr->leaving)
        return;

    /* Do song effects */
    sing();

    // make less noise if you did nothing at all
    // (+7 in total whether or not stealth mode is used)
    if (p_ptr->resting)
    {
        if (p_ptr->stealth_mode)
            stealth_score += 2;
        else
            stealth_score += 7;
    }

    // make much more noise when smithing
    if (p_ptr->smithing)
    {
        /* Make a lot of noise */
        monster_perception(true, false, -10);
    }

    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

    /* Update scent trail */
    update_smell();

    /* possibly identify passive abilities every so often*/
    if (one_in_(100))
    {
        ident_passive();
    }

    /*** Damage over Time ***/

    /* Take damage from poison */
    if (p_ptr->poisoned)
    {
        /* Take damage */

        // amount is one fifth of the poison, rounding up
        amount = (p_ptr->poisoned + 4) / 5;

        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "poison");
    }

    /* Take damage from cuts */
    if (p_ptr->cut)
    {
        amount = (p_ptr->cut + 4) / 5;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "a fatal wound");
    }

    /*** Check the Food, and Regenerate ***/

    /* Basic digestion rate */
    i = 1;

    // Note: speed and regeneration are taken into account already in the hunger
    // rate

    // Hack: slow hunger rates are done statistically
    if (p_ptr->hunger < 0)
    {
        if (!one_in_(int_exp(3, -(p_ptr->hunger))))
        {
            i = 0;
        }
    }
    else if (p_ptr->hunger > 0)
    {
        i *= int_exp(3, p_ptr->hunger);
    }

    /* Digest very quickly when gorged */
    if (p_ptr->food >= PY_FOOD_MAX)
        i *= 50;

    /* CUR_HUNGER increases p_ptr->hunger modifier (applied in calc_bonuses) */
    /* This is now handled via p_ptr->hunger in calc_bonuses() */
    /* Each stack adds +1 to hunger rate, giving 3x, 9x, 27x scaling */

    /* Digest some food */
    (void)set_food(p_ptr->food - i);

    /* Starve to death (slowly) */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        /* Calculate damage */
        i = 1; // old: (PY_FOOD_STARVE - p_ptr->food) / 10;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(i, "starvation");
    }

    /* Lower the staircasiness */
    if (p_ptr->staircasiness > 0)
    {
        // decreases much faster on the escape
        if (p_ptr->on_the_run)
        {
            // amount is one hundredth of the current value, rounding up
            amount = (p_ptr->staircasiness + 99) / 100;
        }

        else
        {
            // amount is one thousandth of the current value, rounding up
            amount = (p_ptr->staircasiness + 999) / 1000;
        }

        p_ptr->staircasiness -= amount;
    }

    /* Regeneration ability */
    regen_multiplier = p_ptr->regenerate + 1;

    /* Regenerate the mana */
    if (p_ptr->csp < p_ptr->msp)
    {
        runtime_dungeon_regenmana(regen_multiplier);
    }

    /* Various things interfere with healing */
    if (p_ptr->food < PY_FOOD_STARVE)
        regen_multiplier = 0;
    if (p_ptr->poisoned)
        regen_multiplier = 0;
    if (p_ptr->cut)
        regen_multiplier = 0;

    /* Regenerate Hit Points if needed */
    if (p_ptr->chp < p_ptr->mhp)
    {
        runtime_dungeon_regenhp(regen_multiplier);
    }

    /*** Timeout Various Things ***/

    amount = 1;

    /* Hack -- Hallucinating */
    if (p_ptr->image)
    {
        (void)set_image(p_ptr->image - amount);
    }

    /* Blindness */
    if (p_ptr->blind)
    {
        (void)set_blind(p_ptr->blind - amount);
    }

    /* Timed see-invisible */
    if (p_ptr->tim_invis)
    {
        (void)set_tim_invis(p_ptr->tim_invis - 1);
    }

    /* Entranced */
    if (p_ptr->entranced)
    {
        (void)set_entranced(p_ptr->entranced - amount);
    }

    /* Confusion */
    if (p_ptr->confused)
    {
        (void)set_confused(p_ptr->confused - amount);
    }

    /* Afraid */
    if (p_ptr->afraid)
    {
        (void)set_afraid(p_ptr->afraid - amount);
    }

    /* Darkened */
    if (p_ptr->darkened)
    {
        (void)set_darkened(p_ptr->darkened - amount);
    }

    /* Fast */
    if (p_ptr->fast)
    {
        (void)set_fast(p_ptr->fast - 1);
    }

    /* Slow */
    if (p_ptr->slow)
    {
        if (singing(SNG_FREEDOM))
            (void)set_slow(p_ptr->slow - ability_bonus(S_SNG, SNG_FREEDOM));
        else
            (void)set_slow(p_ptr->slow - 1);
    }

    /* Rage */
    if (p_ptr->rage)
    {
        (void)set_rage(p_ptr->rage - 1);
    }

    /* Temporary Strength */
    if (p_ptr->tmp_str)
    {
        (void)set_tmp_str(p_ptr->tmp_str - 1);
    }

    /* Temporary Dexterity */
    if (p_ptr->tmp_dex)
    {
        (void)set_tmp_dex(p_ptr->tmp_dex - 1);
    }

    /* Temporary Constitution */
    if (p_ptr->tmp_con)
    {
        (void)set_tmp_con(p_ptr->tmp_con - 1);
    }

    /* Temporary Grace */
    if (p_ptr->tmp_gra)
    {
        (void)set_tmp_gra(p_ptr->tmp_gra - 1);
    }

    /* Temporary Perception */
    if (p_ptr->tmp_per)
    {
        (void)set_tmp_per(p_ptr->tmp_per - 1);
    }

    /* Song of Challenge lingering effect */
    if (p_ptr->song_challenge_effect)
    {
        p_ptr->song_challenge_effect -= 1;
    }

    /* Song of Elbereth lingering effect */
    if (p_ptr->song_elbereth_effect)
    {
        p_ptr->song_elbereth_effect -= 1;
    }

    /* Oppose Fire */
    if (p_ptr->oppose_fire)
    {
        (void)set_oppose_fire(p_ptr->oppose_fire - 1);
    }

    /* Oppose Cold */
    if (p_ptr->oppose_cold)
    {
        (void)set_oppose_cold(p_ptr->oppose_cold - 1);
    }

    /* Oppose Poison */
    if (p_ptr->oppose_pois)
    {
        (void)set_oppose_pois(p_ptr->oppose_pois - 1);
    }

    /*** Poison and Stun and Cut ***/

    /* Poison */
    if (p_ptr->poisoned)
    {
        // adjust is one fifth of the poison, rounding up
        int adjust = (p_ptr->poisoned + 4) / 5;

        /* Apply some healing */
        (void)set_poisoned(p_ptr->poisoned - adjust * amount);
    }

    /* Stun */
    if (p_ptr->stun)
    {
        int adjust = 1;

        /* Apply some healing */
        (void)set_stun(p_ptr->stun - adjust * amount);
    }

    /* Cut */
    if (p_ptr->cut)
    {
        // adjust is one fifth of the wound, rounding up
        int adjust = (p_ptr->cut + 4) / 5;

        /* Apply some healing */
        (void)set_cut(p_ptr->cut - adjust * amount);
    }

    // reset the focus flag if the player didn't 'pass' this turn
    if (p_ptr->previous_action[0] != 5)
    {
        p_ptr->focused = false;
    }

    // if the player didn't attack or 'pass' then the consecutive attacks needs
    // to be reset
    if (!player_attacked && (p_ptr->previous_action[0] != 5))
    {
        p_ptr->consecutive_attacks = 0;
        p_ptr->last_attack_m_idx = 0;
    }

    // boots of radiance
    if (inventory[INVEN_FEET].k_idx)
    {
        u32b f1, f2, f3;
        object_type* o_ptr = &inventory[INVEN_FEET];

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (f2 & (TR2_RADIANCE))
        {
            if (!(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
            {
                if (!object_known_p(o_ptr) && one_in_(10))
                {
                    char o_short_name[80];
                    char o_full_name[80];

                    object_desc(
                        o_short_name, sizeof(o_short_name), o_ptr, false, 0);
                    object_aware(o_ptr);
                    object_known(o_ptr);
                    object_desc(
                        o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                    msg_print("Your footsteps leave a trail of light!");
                    msg_format("You recognize your %s to be %s", o_short_name,
                        o_full_name);
                }

                cave_info[p_ptr->py][p_ptr->px] |= CAVE_GLOW;
            }
        }
    }

    playerturn++;

    min_depth_timer_status(NULL, NULL, &depth_counter_increment, NULL, NULL);

    min_depth_counter += depth_counter_increment > 0 ?
        depth_counter_increment : 0;

    /* Window stuff */

    // Sil-y: note that these are now being set every single turn, somewhat
    // defeating their purpose
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
    
    /*
     * Do NOT refresh the main-terminal combat rolls here.
     * We refresh them after monster processing in the main loop so that
     * both sides of the current round (player and monsters) are included.
     */
}

/*
 * Interact with the current dungeon level.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 */
void runtime_dungeon_run_level(void)
{
    monster_type* m_ptr;
    int i;

    /* Hack -- enforce illegal panel */
    p_ptr->wy = p_ptr->cur_map_hgt;
    p_ptr->wx = p_ptr->cur_map_wid;

    /* Not leaving */
    p_ptr->leaving = false;

    /* Reset the "command" vars */
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;

    /* Cancel the target */
    target_set_monster(0);

    /* Cancel the health bar */
    health_track(0);

    /* Reset shimmer flags */
    shimmer_monsters = true;
    shimmer_objects = true;

    /* Reset repair flags */
    repair_mflag_show = true;
    repair_mflag_mark = true;

    /* Disturb */
    disturb(0, 0);

    /* Track maximum dungeon level */
    if (p_ptr->max_depth < p_ptr->depth)
    {
        log_info("Player reached new maximum depth: %d", p_ptr->depth);
        for (i = p_ptr->max_depth + 1; i <= p_ptr->depth; i++)
        {
            if (i > 1)
            {
                int new_exp = i * 50;
                gain_exp(new_exp);
                p_ptr->descent_exp += new_exp;

                log_debug("Depth %d reached, gained %d descent experience", i, new_exp);

                // Sil-x
                // do_cmd_note(format("exp:%d = s:5000 + e:%d + k:%d + d:%d +
                // i:%d",
                //		    p_ptr->exp, p_ptr->encounter_exp,
                // p_ptr->kill_exp, p_ptr->descent_exp, p_ptr->ident_exp), i);
            }
        }
        p_ptr->max_depth = p_ptr->depth;
    }

    /* No stairs from the surface */
    if (!p_ptr->depth)
    {
        p_ptr->create_stair = false;
    }

    /* Make a staircase */
    if (p_ptr->create_stair)
    {
        log_debug("Creating staircase at player position");
        /* Place a staircase */
        if (cave_valid_bold(p_ptr->py, p_ptr->px))
        {
            /* XXX XXX XXX */
            delete_object(p_ptr->py, p_ptr->px);

            cave_set_feat(p_ptr->py, p_ptr->px, p_ptr->create_stair);

            /* Mark the stairs as known */
            cave_info[p_ptr->py][p_ptr->px] |= (CAVE_MARK);

            log_trace("Staircase created and marked at (%d, %d)", p_ptr->py, p_ptr->px);
        }

        /* Cancel the stair request */
        p_ptr->create_stair = false;
    }

    /* Make rubble */
    if (p_ptr->create_rubble)
    {
        log_debug("Creating rubble via earthquake");
        earthquake(p_ptr->py, p_ptr->px, -1, -1, 5, 0);

        /* Cancel the rubble request */
        p_ptr->create_rubble = false;
    }

    if (!runtime_dungeon_prepare_level_presentation())
        return;

    /*** Process this dungeon level ***/

    /* Reset generation depth; the Gates use depth 20 tables while displayed as 0. */
    monster_level = player_generation_depth();
    object_level = player_generation_depth();

    runtime_dungeon_begin_level_vault_tracking();
    log_live_special_vault_only_monsters("dungeon loop start");

    log_info("Starting main dungeon loop for depth %d", p_ptr->depth);

    /* Main loop */
    while (true)
    {
        /* Hack -- Compact the monster list occasionally */
        if (mon_cnt + 10 > MAX_MONSTERS) {
            log_debug("Compacting monster list (count: %d)", mon_cnt);
            compact_monsters(20);
        }

        /* Hack -- Compress the monster list occasionally */
        if (mon_cnt + 32 < MAX_MONSTERS)
            compact_monsters(0);

        /* Hack -- Compact the object list occasionally */
        if (o_cnt + 32 > z_info->o_max) {
            log_debug("Compacting object list (count: %d)", o_cnt);
            compact_objects(64);
        }

        /* Hack -- Compress the object list occasionally */
        if (o_cnt + 32 < o_max)
            compact_objects(0);

        /*** Apply energy ***/

          /* Can the player move? */
        while ((p_ptr->energy >= 100) && (!p_ptr->leaving))
          {
            /* Start a new combat round BEFORE any actors move this turn.
                    This ensures monsters that act before the player (due to higher
                    energy) are recorded in the same current round as the player's
                    actions, avoiding a one-turn lag in the bottom log. */
            log_trace("[LOOP] Begin player-energy turn: energy=%d", p_ptr->energy);
                new_combat_round();
            log_trace("[LOOP] After new_combat_round: turns_since_combat=%d combat_number=%d old=%d", turns_since_combat, combat_number, combat_number_old);

                /* Process monster with even more energy first */
            log_trace("[LOOP] process_monsters pre-player: threshold=%d", p_ptr->energy + 1);
            process_monsters(p_ptr->energy + 1);
            log_trace("[LOOP] after process_monsters pre-player: combat_number=%d old=%d", combat_number, combat_number_old);

                /* Show newly added monster attacks immediately so they are not perceived as a turn late */
                if (op_ptr->main_combat_rolls > 0)
                {
                    log_trace("[LOOP] interim display_main_combat_rolls pre-player");
                    display_main_combat_rolls();
                }

            /* If still alive */
            if (!p_ptr->leaving)
            {
                /* Update stuff */
                if (p_ptr->update) {
                    update_stuff();
                }

                /* Redraw stuff */
                if (p_ptr->redraw) {
                    redraw_stuff();
                }

                /* Process the player */
                log_trace("[LOOP] process_player start");
                process_player();
                log_trace("[LOOP] process_player end: combat_number=%d old=%d", combat_number, combat_number_old);
                
                /* Scan for artifacts near player and mark as seen */
                runtime_dungeon_scan_artifacts_near_player();
                
            }
        }

        /* Notice stuff */
        if (p_ptr->notice) {
            notice_stuff();
        }

        /* Update stuff */
        if (p_ptr->update) {
            update_stuff();
        }

        /* Redraw stuff */
        if (p_ptr->redraw) {
            redraw_stuff();
        }

        /* Redraw stuff */
        if (p_ptr->window) {
            window_stuff();
        }

        /* Place the cursor on the player or target */
        if (hilite_player)
            dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            dungeon_note_cursor_relative(p_ptr->target_row,
                p_ptr->target_col);

        /* Handle "leaving" */
    if (p_ptr->leaving) {
            log_info("Player leaving dungeon level %d", p_ptr->depth);
            break;
        }

        /* Process monsters (any that haven't had a chance to move yet) */
    log_trace("[LOOP] process_monsters post-player: threshold=100");
    process_monsters(100);
    log_trace("[LOOP] after process_monsters post-player: combat_number=%d old=%d", combat_number, combat_number_old);
    
        /* Update main terminal combat rolls after monster processing */
        if (op_ptr->main_combat_rolls > 0)
        {
            log_trace("[LOOP] display_main_combat_rolls() now");
            display_main_combat_rolls();
        }

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Redraw stuff */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            dungeon_note_cursor_relative(p_ptr->target_row,
                p_ptr->target_col);

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Process the world */
        runtime_dungeon_process_world();

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            dungeon_note_cursor_relative(p_ptr->target_row,
                p_ptr->target_col);

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Give the player some energy */
        p_ptr->energy += extract_energy[p_ptr->pspeed];

        /* Give energy to all monsters */
        bool freeze_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && !p_ptr->morgoth_hall_entered && (silmarils_possessed() == 0);
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore "dead" monsters */
            if (!m_ptr->r_idx)
                continue;

            /* Keep Morgoth's hall frozen until the player enters it */
            if (freeze_morgoth_vault
                && (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_G_VAULT))
            {
                m_ptr->energy = 0;
                continue;
            }

            /* Give this monster some energy */
            m_ptr->energy += extract_energy[m_ptr->mspeed];
        }

        /* Count game turns */
        turn++;
    }
}

/* Tiny proxy for frontends to query current depth without including player headers */
int p_ptr_depth_proxy(void) { return p_ptr ? p_ptr->depth : 0; }
