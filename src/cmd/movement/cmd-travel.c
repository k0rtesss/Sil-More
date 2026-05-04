/* File: cmd-travel.c */
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
#include "cmd/movement/cmd-depth-bonus.h"
#include "item_set.h"
#include "log/log.h"
#include "object/object-flags.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "player/killer.h"
#include "runtime/runtime-game.h"
#include "metarun.h"

#define MIN_DEPTH_COUNTER_STEP_BASE 150000
#define MIN_DEPTH_COUNTER_STEP_SETTING_DELTA 30000
#define MIN_DEPTH_BASE_INCREMENT_START 85
#define MIN_DEPTH_BASE_INCREMENT_DIVISOR 850
#define MIN_DEPTH_INCREMENT_PER_BONUS 5
#define MIN_DEPTH_KILL_BONUS_STEP 500
#define MIN_DEPTH_KILL_BONUS_AMOUNT 5

static byte mandos_second_state(void)
{
    return quest_get_state(QUEST_ID_MANDOS_TRAITOR);
}

static byte mandos_third_state(void)
{
    return quest_get_state(QUEST_ID_MANDOS_BETRAYER);
}

static bool mandos_any_active(void)
{
    byte second = mandos_second_state();
    byte third = mandos_third_state();
    return (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) ||
           (second >= QUEST_STATE_ACTIVE && second < QUEST_STATE_REWARDED) ||
           (third >= QUEST_STATE_ACTIVE && third < QUEST_STATE_REWARDED);
}

static bool mandos_any_giver_present(void)
{
    byte second = mandos_second_state();
    byte third = mandos_third_state();
    return p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT ||
           second == QUEST_STATE_GIVER_PRESENT ||
           third == QUEST_STATE_GIVER_PRESENT;
}

static void mandos_reset_all_states(void)
{
    p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    quest_set_state(QUEST_ID_MANDOS_TRAITOR, QUEST_STATE_NOT_STARTED);
    quest_set_state(QUEST_ID_MANDOS_BETRAYER, QUEST_STATE_NOT_STARTED);
    p_ptr->mandos_level = 0;
}

static bool min_depth_timer_bonus_slot_active(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return true;
}

static int min_depth_timer_item_bonus_units(void)
{
    int units = 0;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        u32b f1, f2, f3, f4;
        bool equipped = (i >= INVEN_WIELD);

        if (!o_ptr->k_idx)
            continue;

        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        (void)f1;
        (void)f2;
        if (!min_depth_timer_bonus_slot_active(o_ptr))
            continue;

        if (f4 & TR4_DEEP_CALL)
            units += equipped ? MIN_DEPTH_ITEM_BONUS_DEEP_CALL_EQUIPPED
                              : MIN_DEPTH_ITEM_BONUS_DEEP_CALL_INVENTORY;
        /* Count the item grant itself, even if the player disables the ability. */
        if (equipped && object_grants_ability(o_ptr, S_STL, STL_CRUEL_BLOW))
            units += MIN_DEPTH_ITEM_BONUS_CRUEL_BLOW_EQUIPPED;
        if (f3 & TR3_PERMA_CURSE)
            units += MIN_DEPTH_ITEM_BONUS_PERMA_CURSE;
    }

    return units;
}

static int min_depth_timer_kill_bonus(void)
{
    int total_kills = 0;

    if (!z_info || !l_list)
        return 0;

    for (int i = 1; i < z_info->r_max; i++)
        total_kills += l_list[i].pkills;

    return (total_kills / MIN_DEPTH_KILL_BONUS_STEP)
        * MIN_DEPTH_KILL_BONUS_AMOUNT;
}

static int min_depth_counter_step_adjustment(void)
{
    switch (op_ptr->min_depth_timer_mode) {
    case MIN_DEPTH_TIMER_MODE_RELAXED:
        return MIN_DEPTH_COUNTER_STEP_SETTING_DELTA;
    case MIN_DEPTH_TIMER_MODE_HARSH:
        return -MIN_DEPTH_COUNTER_STEP_SETTING_DELTA;
    default:
        return 0;
    }
}

static int min_depth_counter_step(void)
{
    int step = MIN_DEPTH_COUNTER_STEP_BASE + min_depth_counter_step_adjustment();

    if (step < MIN_DEPTH_COUNTER_STEP_SETTING_DELTA)
        step = MIN_DEPTH_COUNTER_STEP_SETTING_DELTA;

    return step;
}

static int min_depth_timer_base_increment(void)
{
    return MIN_DEPTH_BASE_INCREMENT_START - (playerturn / MIN_DEPTH_BASE_INCREMENT_DIVISOR);
}

static int min_depth_timer_additional_increment(void)
{
    int min_depth_value = min_depth();
    int current_depth = p_ptr ? p_ptr->depth : min_depth_value;
    int depth_bonus;
    int item_bonus_units = min_depth_timer_item_bonus_units();
    /* Use half-depth units so carried Deep Call items can be worth 1.5 depths. */
    int item_bonus = (MIN_DEPTH_INCREMENT_PER_BONUS * item_bonus_units
        + (MIN_DEPTH_BONUS_UNITS_PER_DEPTH / 2))
        / MIN_DEPTH_BONUS_UNITS_PER_DEPTH;
    int kill_bonus = min_depth_timer_kill_bonus();

    /* Character creation has not placed the player on depth 1 yet. */
    if ((playerturn == 0) && (current_depth <= 0))
        current_depth = min_depth_value;

    depth_bonus = MIN_DEPTH_INCREMENT_PER_BONUS
        * (current_depth - min_depth_value);

    return depth_bonus + item_bonus + kill_bonus;
}

void min_depth_timer_status(int* base_increment, int* additional_increment,
    int* total_increment, int* progress, int* threshold)
{
    int base = min_depth_timer_base_increment();
    int additional = min_depth_timer_additional_increment();
    int total = base + additional;
    int step = min_depth_counter_step();
    int current_progress = min_depth_counter % step;

    if (current_progress < 0)
        current_progress += step;

    if (base_increment)
        *base_increment = base;
    if (additional_increment)
        *additional_increment = additional;
    if (total_increment)
        *total_increment = total;
    if (progress)
        *progress = current_progress;
    if (threshold)
        *threshold = step;
}

/*
 * Determines the shallowest a player is allowed to go.
 * As time goes on, they are forced deeper and deeper.
 */
int min_depth(void)
{
    int min_depth_value = min_depth_counter / min_depth_counter_step() + 1;

    // bounds on the base
    if (min_depth_value < 1)
        min_depth_value = 1;
    if (min_depth_value > MORGOTH_DEPTH)
        min_depth_value = MORGOTH_DEPTH;

    // can't leave Morgoth's hall once entered
    if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->morgoth_hall_entered)
    {
        min_depth_value = MORGOTH_DEPTH;
    }

    // no limits in the endgame
    if (p_ptr->on_the_run)
    {
        min_depth_value = 0;
    }

    return (min_depth_value);
}

void note_lost_greater_vault(void)
{
    char note[120];
    char* fmt = "Left without entering %s";
    int y, x;
    bool discovered = false;

    /* Handle lost greater vaults */
    if (g_vault_name[0] != '\0')
    {
        /* Analyze the actual map */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_info[y][x] & (CAVE_G_VAULT))
                    && (cave_info[y][x] & (CAVE_MARK)))
                {
                    discovered = true;
                }
            }
        }

        if (discovered)
        {
            strnfmt(note, sizeof(note), fmt, g_vault_name);
            do_cmd_note(note, p_ptr->depth);
        }

        g_vault_name[0] = '\0';
    }
}

/*
 * Determines whether a staircase is 'trapped' like a false floor trap.
 * This means you fall a level below where you expected to end up (if you were
 * going upwards), take some minor damage, and have no stairs back.
 *
 * It gets more likely the more stairs you have recently taken.
 * It is designed to stop you stair-scumming.
 */
static bool trapped_stairs(void)
{
    int chance;

    chance = p_ptr->staircasiness / 100;
    chance = chance * chance * chance;
    chance = chance / 10000;

    if (p_ptr->on_the_run)
        chance = 0;

    // msg_debug("%d, %d", p_ptr->staircasiness, chance);

    if (percent_chance(chance))
        return (true);
    else
        return (false);
}

/*
 * Go up a staircase
 */
void do_cmd_go_up(void)
{
    int min;
    int new;

    /* Verify stairs */
    if (!cave_up_stairs_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You see no up staircase here.");
        return;
    }

    /* Ironman */
    if (birth_ironman && (silmarils_possessed() == 0))
    {
        msg_print("You have vowed to not to return until you hold a Silmaril.");
        return;
    }

    if (chosen_oath(OATH_IRON) && !oath_invalid(OATH_IRON) &&
       (silmarils_possessed() == 0))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_IRON);
        if (!prompt || !prompt[0]) prompt = "Are you certain you wish to break your Oath of Iron?";
        
        if (get_check_oath_multiline(prompt))
        {
            /* Curse message and selection handled by apply_oath_breaking_curse */
            do_cmd_note("Broke your oath", p_ptr->depth);
            apply_oath_breaking_curse(OATH_IRON);
            
            /* Only mark oath as broken if player actually has it */
            p_ptr->oaths_broken |= OATH_IRON_FLAG;
        }
        else
        {
            return;
        }
    }

    // warn player if they have an active Niena quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Niena's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check("Are you sure you wish to abandon the quest and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aule quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aulë's forge will mean failure of the quest.");
        if (!get_check("Are you sure you wish to abandon the forge and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (mandos_any_active())
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check("Are you sure you wish to abandon the tomb and ascend? "))
        {
            return;
        }
    }

    /* Hack -- take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Calculate the shallowest a player is allowed to go */
    min = min_depth();

    /* At 1000ft, once locked in (by time or by entering Morgoth's hall),
     * you cannot retreat without a Silmaril. */
    if ((p_ptr->depth == MORGOTH_DEPTH) && (min == MORGOTH_DEPTH)
        && (silmarils_possessed() == 0))
    {
        msg_print("You enter a maze of staircases, but cannot find your way.");

        return;
    }

    // Store information for the combat rolls window
    combat_roll_special_char
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
    combat_roll_special_attr
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;

    // calculate the new depth to arrive at
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_LESS_SHAFT)
        && (p_ptr->depth > 0))
    {
        /* Create a way back (usually) */
        p_ptr->create_stair = FEAT_MORE_SHAFT;

        new = p_ptr->depth - 2;
    }
    else
    {
        /* Create a way back */
        p_ptr->create_stair = FEAT_MORE;

        new = p_ptr->depth - 1;
    }

    // deal with most cases where you can't find your way
    if ((new < min)
        && !((p_ptr->depth == MORGOTH_DEPTH) && (silmarils_possessed() > 0)))
    {
        message(MSG_STAIRS, 0,
            "You enter a maze of up staircases, but cannot find your way.");

        // deal with trapped stairs when trying and failing to go upwards
        if (trapped_stairs())
        {
            msg_print("The stairs crumble beneath you!");
            message_flush();
            msg_print("You fall through...");
            message_flush();
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

            // take some damage
            falling_damage(false);

            // no stairs back
            p_ptr->create_stair = false;
        }

        else
        {
            if (p_ptr->depth == min)
            {
                message(MSG_STAIRS, 0, "You emerge near where you began.");
            }
            else
            {
                message(
                    MSG_STAIRS, 0, "You emerge even deeper in the dungeon.");
            }

            if (p_ptr->create_stair == FEAT_MORE)
            {
                /* Change the way back */
                p_ptr->create_stair = FEAT_LESS;
            }
            else
            {
                /* Change the way back */
                p_ptr->create_stair = FEAT_LESS_SHAFT;
            }
        }

        new = min;
    }

    // deal with cases where you can find your way
    else
    {
        message(MSG_STAIRS, 0, "You enter a maze of up staircases.");

        if (silmarils_possessed() > 0)
        {
            message(MSG_STAIRS, 0, "The divine light reveals the way.");
        }

        if ((p_ptr->depth == MORGOTH_DEPTH) && (silmarils_possessed() > 0))
        {
            if (!p_ptr->morgoth_slain)
            {
                msg_print("As you climb the stair, a great cry of rage and "
                          "anguish comes "
                          "from below.");
                msg_print("Make quick your escape: it shall be hard-won.");
            }

            // set the 'on the run' flag
            p_ptr->on_the_run = true;

            // remove the 'truce' flag if it hasn't been done already
            p_ptr->truce = false;
            
            /* Check for crown theft and silmarils */
            /* Priority: Crown theft is more serious than individual silmarils */
            int crown_art = has_iron_crown();
            int sils = silmarils_possessed();
            int target_state = 1;  // Default: just crown dropped
            
            log_debug("do_cmd_go_up: pursuit begins - has_crown=%d, silmarils=%d, current_state=%d",
                     crown_art, sils, p_ptr->morgoth_state);
            
            /* Determine target anger state based on what player has */
            if (crown_art > 0)
            {
                /* Player has the crown itself - this is a major theft! */
                /* Crown with 0 silmarils still means you stole his crown → State 3 */
                target_state = 3;
                log_debug("do_cmd_go_up: player has crown (art=%d), target_state=3", crown_art);
                
                /* If crown still has silmarils on it, that's even worse */
                if (crown_art == ART_MORGOTH_3)  // 3 silmarils on crown
                {
                    target_state = 4;
                    log_debug("do_cmd_go_up: crown has all 3 silmarils, target_state=4");
                }
            }
            else if (sils > 0)
            {
                /* Player has prised silmarils (not carrying crown) */
                target_state = 1 + sils;  /* 1 sil → state 2, 2 sils → state 3, 3 sils → state 4 */
                log_debug("do_cmd_go_up: player has %d prised silmarils, target_state=%d", sils, target_state);
            }
            
            /* Apply anger if target exceeds current state */
            if (target_state > p_ptr->morgoth_state)
            {
                if (crown_art > 0)
                {
                    /* Crown theft messages */
                    if (target_state >= 4)
                    {
                        msg_print("Morgoth's rage shakes the very foundations of Angband!");
                        msg_print("You have stolen his crown with all the Silmarils intact!");
                    }
                    else  // State 3
                    {
                        msg_print("Morgoth howls in rage - you have stolen his Iron Crown!");
                    }
                }
                else if (sils > 0)
                {
                    /* Silmaril theft messages (without crown) */
                    switch(sils)
                    {
                        case 1:
                            msg_print("Morgoth roars with rage as he realizes a Silmaril is missing!");
                            break;
                        case 2:
                            msg_print("Morgoth howls in fury - two Silmarils stolen!");
                            break;
                        case 3:
                            msg_print("Morgoth's wrath is terrible - all Silmarils are gone!");
                            break;
                    }
                }
                
                log_debug("do_cmd_go_up: calling anger_morgoth(%d)", target_state);
                anger_morgoth(target_state);
            }
        }

        // deal with trapped stairs when going upwards
        else if (trapped_stairs())
        {
            msg_print("The stairs crumble beneath you!");
            message_flush();
            msg_print("You fall through...");
            message_flush();
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

            // take some damage
            falling_damage(false);

            // no stairs back
            p_ptr->create_stair = false;

            // go to a lower floor
            new ++;
        }
    }

    // make a note if the player loses a greater vault
    note_lost_greater_vault();

    /* New depth */
    p_ptr->depth = new;

    /* Reset tulkas quest */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    }

    /* Reset niena quest */
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        msg_print("You have failed Niena's mercy quest by leaving the level.");
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aulë's forge. The quest is lost.");
    }
    else if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    }

    /* Reset mandos quests if active */
    if (mandos_any_active())
    {
        mandos_reset_all_states();
        msg_print("You have abandoned the tomb. Mandos' quest is lost.");
    }
    else if (mandos_any_giver_present())
    {
        mandos_reset_all_states();
    }

    /* Reset Varda quest if she was waiting on the previous level */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT)
    {
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
        /* Encountering a quest giver still consumes the run's single quest slot. */
    }

    {
        byte varda_shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);
        if (varda_shadow_state == QUEST_STATE_GIVER_PRESENT && p_ptr->varda_shadow_level == p_ptr->depth)
        {
            quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_NOT_STARTED);
            p_ptr->varda_shadow_level = 0;
            p_ptr->varda_shadow_ready = 0;
            p_ptr->varda_shadow_placed = 0;
            p_ptr->varda_shadow_restricted = 0;
        }
    }

    // another staircase has been used...
    p_ptr->stairs_taken++;
    p_ptr->staircasiness += 1000;

    /* Remember disconnected stairs */
    if (birth_discon_stair)
        p_ptr->create_stair = false;

    /* Leaving */
    p_ptr->leaving = true;
}

/*
 * Go down a staircase
 */
void do_cmd_go_down(void)
{
    int min;
    int new;

    /* Verify stairs */
    if (!cave_down_stairs_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You see no down staircase here.");
        return;
    }

    // special message for tutorial
    if (p_ptr->game_type == -1)
    {
        // display the tutorial leaving text
        if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE)
        {
            pause_with_text(tutorial_leave_text, 5, 10, NULL, 0);
        }
        else
        {
            pause_with_text(tutorial_win_text, 5, 10, NULL, 0);
        }

        p_ptr->is_dead = true;
        p_ptr->energy_use = 100;
        p_ptr->leaving = true;
        close_game();
        return;
    }

    // warn player if they have an active Niena quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Niena's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check("Are you sure you wish to abandon the quest and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aule quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aulë's forge will mean failure of the quest.");
        if (!get_check("Are you sure you wish to abandon the forge and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (mandos_any_active())
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check("Are you sure you wish to abandon the tomb and descend? "))
        {
            return;
        }
    }

    // Do not descend from the Gates
    if (p_ptr->depth == 0)
    {
        msg_print("You have made it to the very gates of Angband and can once "
                  "more taste "
                  "the freshness on the air.");
        msg_print("You will not re-enter that fell pit.");
        return;
    }

    // Store information for the combat rolls window
    combat_roll_special_char
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
    combat_roll_special_attr
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;

    min = min_depth();
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT)
        && (p_ptr->depth < MORGOTH_DEPTH - 1))
    {
        /* Create a way back (usually) */
        p_ptr->create_stair = FEAT_LESS_SHAFT;

        new = p_ptr->depth + 2;
    }
    else
    {
        /* Create a way back */
        p_ptr->create_stair = FEAT_LESS;

        new = p_ptr->depth + 1;
    }

    /* Hack -- take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    message(MSG_STAIRS, 0, "You enter a maze of down staircases.");

    // Can never return to the throne room...
    if ((p_ptr->on_the_run) && (new == MORGOTH_DEPTH))
    {
        message(MSG_STAIRS, 0,
            "Try though you might, you cannot find your way back to Morgoth's "
            "throne.");
        message(MSG_STAIRS, 0, "You emerge near where you began.");
        p_ptr->create_stair = FEAT_MORE;
        new = MORGOTH_DEPTH - 1;
    }

    // deal with trapped stairs
    else if (trapped_stairs())
    {
        msg_print("The stairs crumble beneath you!");
        message_flush();
        msg_print("You fall through...");
        message_flush();
        msg_print("...and land somewhere deeper in the Iron Hells.");
        message_flush();

        // add to the notes file
        do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

        // take some damage
        falling_damage(false);

        // no stairs back
        p_ptr->create_stair = false;
    }

    else if (new < min)
    {
        message(MSG_STAIRS, 0, "You emerge much deeper in the dungeon.");
        new = min;
    }

    // make a note if the player loses a greater vault
    note_lost_greater_vault();

    /* New depth */
    p_ptr->depth = new;

    /* Reset tulkas quest */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    }

    /* Reset Varda quest if she was waiting on the previous level */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT)
    {
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
        /* Encountering a quest giver still consumes the run's single quest slot. */
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aulë's forge. The quest is lost.");
    }
    else if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    }

    /* Reset mandos quests if active */
    if (mandos_any_active())
    {
        mandos_reset_all_states();
        msg_print("You have abandoned the tomb. Mandos' quest is lost.");
    }
    else if (mandos_any_giver_present())
    {
        mandos_reset_all_states();
    }

    {
        byte varda_shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);
        if (varda_shadow_state == QUEST_STATE_GIVER_PRESENT && p_ptr->varda_shadow_level == p_ptr->depth)
        {
            quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_NOT_STARTED);
            p_ptr->varda_shadow_level = 0;
            p_ptr->varda_shadow_ready = 0;
            p_ptr->varda_shadow_placed = 0;
            p_ptr->varda_shadow_restricted = 0;
        }
    }

    /* Reset niena quest if active */
    if (p_ptr->niena_quest >= NIENA_QUEST_ACTIVE && p_ptr->niena_quest < NIENA_QUEST_REWARDED)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        msg_print("You have abandoned Niena's mercy quest. The quest is lost.");
    }
    else if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    }

    // another staircase has been used...
    p_ptr->stairs_taken++;
    p_ptr->staircasiness += 1000;

    /* Remember disconnected stairs */
    if (birth_discon_stair)
        p_ptr->create_stair = false;

    /* Leaving */
    p_ptr->leaving = true;
}

/*
 * Simple command to "search" for one turn
 */
