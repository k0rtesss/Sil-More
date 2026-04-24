/* File: quest-orome.c */
/*
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
#include "metarun.h"
#include "quest/quest.h"
#include "quest/quest-internal.h"
#include "log/log.h"

static cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";

    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }

    switch (oath_id) {
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

static const int orome_great_hunt_targets[] = {
    R_IDX_SCATHA,
    R_IDX_SMAUG,
    R_IDX_DRAUGLUIN,
    R_IDX_GOSTIR,
    R_IDX_SHELOB,
    R_IDX_THURINGWETHIL
};
static const size_t orome_great_hunt_target_count = N_ELEMENTS(orome_great_hunt_targets);

int orome_great_hunt_bit_for_target(int r_idx)
{
    for (size_t i = 0; i < orome_great_hunt_target_count; i++)
    {
        if (orome_great_hunt_targets[i] == r_idx) {
            return (1 << i);
        }
    }

    return 0;
}

void check_orome_quest_completion(int r_idx)
{
    if (p_ptr->orome_quest == OROME_QUEST_ACTIVE) {
        /* Check thresholds for each monster type */
        bool quest_complete = false;
        cptr monster_name = "";
        int kill_count = 0;
        
        if (p_ptr->orome_wolves_killed >= 100) {
            quest_complete = true;
            monster_name = "wolves";
            kill_count = p_ptr->orome_wolves_killed;
        }
        else if (p_ptr->orome_spiders_killed >= 80) {
            quest_complete = true;
            monster_name = "spiders";
            kill_count = p_ptr->orome_spiders_killed;
        }
        else if (p_ptr->orome_serpents_killed >= 60) {
            quest_complete = true;
            monster_name = "serpents";
            kill_count = p_ptr->orome_serpents_killed;
        }
        else if (p_ptr->orome_vampires_killed >= 30) {
            quest_complete = true;
            monster_name = "vampires";
            kill_count = p_ptr->orome_vampires_killed;
        }
        
        if (quest_complete) {
            p_ptr->orome_quest = OROME_QUEST_SUCCESS;
            
            msg_format("The hunt is complete! You have slain %d %s, proving your prowess!", 
                       kill_count, monster_name);
            msg_print("Oromë the Huntsman will be pleased with your mastery.");
            msg_print("Seek him out to claim your reward - the knowledge of Unique Bane!");
            
            log_trace("Orome quest completed - %d %s slain (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                     kill_count, monster_name,
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
    }

    if (quest_get_state(QUEST_ID_OROME_DRAGONS) == QUEST_STATE_ACTIVE &&
        p_ptr->orome_dragons_killed >= 10) {
        quest_set_state(QUEST_ID_OROME_DRAGONS, QUEST_STATE_SUCCESS);
        msg_format("The dragon hunt is complete! You have slain %d mighty dragons.", p_ptr->orome_dragons_killed);
        if (!is_quest_giver_present(R_IDX_OROME))
            spawn_quest_giver_near_player(R_IDX_OROME);
    }

    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) == QUEST_STATE_ACTIVE) {
        byte all_targets_mask = (byte)((1 << orome_great_hunt_target_count) - 1);
        int hunt_bit = orome_great_hunt_bit_for_target(r_idx);

        if (hunt_bit) {
            log_trace("Orome great hunt progress: mask=0x%02x after slaying target %d", p_ptr->orome_great_hunt_mask, r_idx);
        }

        if ((p_ptr->orome_great_hunt_mask & all_targets_mask) == all_targets_mask) {
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_OROME_GREAT_HUNT, &completion_count);
            completion_texts = prepend_repeat_context(QUEST_ID_OROME_GREAT_HUNT, completion_texts, &completion_count, true);

            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Oromë, Hunt of the Great", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                cptr fallback[] = {
                    "The last of Oromë's marked prey falls at last.",
                    "The Valaróma rings once more, and the great hunt is ended."
                };
                quest_typewriter_menu("Oromë, Hunt of the Great", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
            }

            quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_REWARDED);
            metarun_set_orome_great_hunt_active(false);
            grant_followup_quest_rewards(QUEST_ID_OROME_GREAT_HUNT);
        }
    }
}

/*
 * Handle interaction with Orome for the hunting quest
 */
void orome_quest_interaction(void)
{
    byte dragon_state;

    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    dragon_state = quest_get_state(QUEST_ID_OROME_DRAGONS);
    if (dragon_state == QUEST_STATE_GIVER_PRESENT)
    {
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_OROME_DRAGONS, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_OROME_DRAGONS, init_texts, &text_count, false);

        quest_set_state(QUEST_ID_OROME_DRAGONS, QUEST_STATE_ACTIVE);
        p_ptr->orome_dragons_killed = 0;
        remove_quest_giver(R_IDX_OROME);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Oromë, Warden of the Drakes", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Oromë's horn-call echoes like fire through the deep halls.",
                "'Slay ten mighty dragons and return to me, hunter.'"
            };
            quest_typewriter_menu("Oromë, Warden of the Drakes", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
        }
        return;
    }
    if (dragon_state == QUEST_STATE_SUCCESS)
    {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_OROME_DRAGONS, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_OROME_DRAGONS, completion_texts, &completion_count, true);

        quest_set_state(QUEST_ID_OROME_DRAGONS, QUEST_STATE_REWARDED);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Oromë, Warden of the Drakes", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Oromë appears amid the fading terror of the drakes.",
                "'The mightiest worms have learned to fear your name.'"
            };
            quest_typewriter_menu("Oromë, Warden of the Drakes", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
        }

        grant_followup_quest_rewards(QUEST_ID_OROME_DRAGONS);
        remove_quest_giver(R_IDX_OROME);
        return;
    }
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT && 
        p_ptr->orome_quest != OROME_QUEST_SUCCESS)
    {
        log_trace("orome_quest_interaction called with invalid quest state: %d", p_ptr->orome_quest);
        return;
    }
    
    if (p_ptr->orome_quest == OROME_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Orome quest interaction - offering hunting quest");
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(5, &text_count); /* Orome is quest index 5 */
        init_texts = prepend_repeat_context(QUEST_ID_OROME, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Oromë the Huntsman", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Oromë the Huntsman regards you with keen eyes:",
                "'Prove your skill as a hunter. The dark creatures multiply.'"
            };
            quest_typewriter_menu("Oromë the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }
        
        /* Determine hunt target based on dungeon depth */
        int depth = p_ptr->depth;
        cptr target_name;
        int target_count;
        
        if (depth <= 250) {
            p_ptr->orome_target_type = OROME_TARGET_WOLF;
            target_count = 100;
            target_name = "wolves";
        } else if (depth <= 500) {
            p_ptr->orome_target_type = OROME_TARGET_SPIDER;
            target_count = 80;
            target_name = "spiders";
        } else if (depth <= 750) {
            p_ptr->orome_target_type = OROME_TARGET_SERPENT;
            target_count = 60;
            target_name = "serpents";
        } else {
            p_ptr->orome_target_type = OROME_TARGET_VAMPIRE;
            target_count = 30;
            target_name = "vampires";
        }
        
        /* Accept the quest */
        p_ptr->orome_quest = OROME_QUEST_ACTIVE;
        p_ptr->orome_target_count = target_count;
        p_ptr->orome_killed_count = 0;
        
        /* Remove the quest giver now that quest is accepted */
        remove_quest_giver(R_IDX_OROME);
        
        msg_format("You must hunt and slay %d %s to prove your prowess.", target_count, target_name);
        msg_print("Return when the hunt is complete to claim your reward.");
        msg_print("Oromë fades into the wild, but his presence lingers in your soul.");
        
        log_trace("Orome quest started - hunt %d %s at depth %d", 
                 target_count, target_name, depth);
    }
    else if (p_ptr->orome_quest == OROME_QUEST_SUCCESS)
    {
        log_trace("Completing Orome quest - giving Unique Bane reward");
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(5, &completion_count); /* Orome is quest index 5 */
        completion_texts = prepend_repeat_context(QUEST_ID_OROME, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Oromë the Huntsman", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Oromë appears with a proud smile!",
                "'You have proven yourself a true hunter of the wild.'"
            };
            quest_typewriter_menu("Oromë the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }
        
        /* Show the specific numbers after the main dialogue */
        cptr monster_names[] = {"wolves", "spiders", "serpents", "vampires"};
        cptr monster_name = (p_ptr->orome_target_type >= 1 && p_ptr->orome_target_type <= 4) 
                           ? monster_names[p_ptr->orome_target_type - 1] : "creatures";
        
        msg_format("You have slain %d %s as commanded.", 
                  p_ptr->orome_target_count, monster_name);
        msg_print("You learn the secret of hunting unique creatures!");
        
        /* Clear quest state */
        p_ptr->orome_quest = OROME_QUEST_REWARDED;
        log_trace("Orome reward: Quest state set to REWARDED (%d)", OROME_QUEST_REWARDED);
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(5); /* Orome is quest index 5 */
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_OROME);
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(5); /* Orome is quest index 5 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
            msg_format("The %s is now available for future characters in this lineage!", 
                      get_oath_name_from_id(oath_id));
        }
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_OROME);
        
        log_trace("Orome quest completed - Unique Bane granted, oath unlocked");
    }
}

/*
 * Check if player should interact with Orome
 */
void check_orome_quest_interaction(void)
{
    byte dragon_state = quest_get_state(QUEST_ID_OROME_DRAGONS);

    /* Only check if quest can be started or completed */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT && 
        p_ptr->orome_quest != OROME_QUEST_SUCCESS &&
        dragon_state != QUEST_STATE_GIVER_PRESENT &&
        dragon_state != QUEST_STATE_SUCCESS) {
        return;
    }
    
    log_trace("check_orome_quest_interaction: checking adjacency, quest state: %d", p_ptr->orome_quest);
    
    /* Check for adjacent Orome */
    int y, x;
    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            if (in_bounds(y, x) && cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                if (m_ptr->r_idx == R_IDX_OROME)
                {
                    log_trace("Found Orome adjacent - triggering interaction");
                    orome_quest_interaction();
                    return;
                }
            }
        }
    }
    
    /* If quest is completed but Orome isn't adjacent, try to spawn him */
    if (p_ptr->orome_quest == OROME_QUEST_SUCCESS || dragon_state == QUEST_STATE_SUCCESS)
    {
        log_trace("Orome quest complete but no Orome found - trying to spawn");
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    if (place_monster_one(y, x, R_IDX_OROME, true, true, NULL))
                    {
                        msg_print("Oromë the Huntsman materializes nearby, ready to honor your success!");
                        log_trace("Successfully spawned Orome for quest completion");
                        return;
                    }
                }
            }
        }
        
        log_trace("Failed to spawn Orome for quest completion - will complete anyway");
        /* Complete the quest directly if spawning fails */
        orome_quest_interaction();
    }
}

/*
 * Grant the unique bane special ability to the player
 * This function can be called from quests, debug commands, or other rewards
 */
void grant_unique_bane_ability(void)
{
    log_trace("grant_unique_bane_ability: Function called, checking current state");
    log_trace("grant_unique_bane_ability: have_ability[S_SPC][SPC_UNIQUE_BANE] = %s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");
    
    if (p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE])
    {
        log_trace("grant_unique_bane_ability: Player already has ability, showing message and returning");
        msg_print("You already possess the power to hunt unique creatures effectively.");
        return;
    }
    
    log_trace("grant_unique_bane_ability: Setting ability flags to TRUE");
    /* Grant the ability */
    p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] = true;
    p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] = true;
    
    log_trace("grant_unique_bane_ability: After setting flags - have_ability=%s, active_ability=%s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE",
             p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");
    
    msg_print("You have learned the art of Unique Bane!");
    msg_print("You gain significant advantages when fighting unique creatures.");
    
    log_trace("Granted Unique Bane special ability to player");
    
    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);
    handle_stuff();
}
