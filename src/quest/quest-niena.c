/* File: quest-niena.c */
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

static int total_player_kills_this_run_local(void)
{
    if (!z_info || !l_list) return 0;

    int total = 0;
    for (int i = 0; i < z_info->r_max; i++)
    {
        int kills = l_list[i].pkills;
        if (kills > 0) total += kills;
    }

    return total;
}

void niena_mark_morgoth_attack(void)
{
    if (quest_get_state(QUEST_ID_NIENA_MORGOTH) != QUEST_STATE_ACTIVE) return;
    if (p_ptr->niena_reserved & NIENA_FLAG_MORGOTH_ATTACKED) return;

    p_ptr->niena_reserved |= NIENA_FLAG_MORGOTH_ATTACKED;
    msg_print("Nienna's mercy recoils: you have struck Morgoth.");
}

void niena_revoke_temp_mercy_gift(bool silent)
{
    if (!(p_ptr->niena_reserved & NIENA_FLAG_MERCY_GIFT_TEMP)) return;

    p_ptr->niena_reserved &= ~(NIENA_FLAG_MERCY_GIFT_TEMP);
    p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY] = false;
    p_ptr->active_ability[S_SPC][SPC_NIENA_MERCY] = false;

    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_BASIC);

    if (!silent)
    {
        msg_print("Nienna's borrowed mercy fades.");
    }
}

void check_niena_morgoth_interaction(void)
{
    int y, x;
    monster_type* m_ptr;

    byte state = quest_get_state(QUEST_ID_NIENA_MORGOTH);
    if (state != QUEST_STATE_GIVER_PRESENT) return;

    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            if (y == p_ptr->py && x == p_ptr->px) continue;
            if (!in_bounds(y, x)) continue;
            if (cave_m_idx[y][x] <= 0) continue;

            m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->r_idx != R_IDX_NIENA) continue;

            int text_count = 0;
            cptr* init_texts = extract_quest_init_texts(QUEST_ID_NIENA_MORGOTH, &text_count);
            init_texts = prepend_repeat_context(QUEST_ID_NIENA_MORGOTH, init_texts, &text_count, false);

            if (init_texts && text_count > 0)
            {
                quest_typewriter_menu("Nienna's Mercy", init_texts, text_count, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(init_texts, text_count);
            }
            else
            {
                const char* fallback[] = {
                    "In the shadow of the throne, Nienna waits with eyes full of sorrow and resolve.",
                    "'Take a Silmaril from Morgoth's crown, yet lay no blow upon him.'"
                };
                quest_typewriter_menu("Nienna's Mercy", fallback, N_ELEMENTS(fallback), TERM_L_BLUE, TERM_WHITE);
            }

            if (!p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
            {
                p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY] = true;
                p_ptr->active_ability[S_SPC][SPC_NIENA_MERCY] = true;
                p_ptr->niena_reserved |= NIENA_FLAG_MERCY_GIFT_TEMP;
                p_ptr->update |= (PU_BONUS);
                handle_stuff();
            }

            quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_ACTIVE);
            remove_quest_giver(R_IDX_NIENA);
            return;
        }
    }
}

void ensure_niena_pacifist_active(void)
{
    byte state = quest_get_state(QUEST_ID_NIENA_PACIFIST);
    if (state >= QUEST_STATE_REWARDED) return;

    bool unlocked = metarun_quest_completion_count(METARUN_QUEST_NIENA_MORGOTH) > 0 ||
        quest_get_state(QUEST_ID_NIENA_MORGOTH) >= QUEST_STATE_REWARDED;

    if (!unlocked)
    {
        p_ptr->niena_reserved |= NIENA_FLAG_PACIFIST_FAILED;
        if (state != QUEST_STATE_NOT_STARTED) quest_set_state(QUEST_ID_NIENA_PACIFIST, QUEST_STATE_NOT_STARTED);
        return;
    }

    if (state == QUEST_STATE_NOT_STARTED)
    {
        quest_set_state(QUEST_ID_NIENA_PACIFIST, QUEST_STATE_ACTIVE);
        p_ptr->niena_reserved &= ~NIENA_FLAG_PACIFIST_FAILED;
    }

    if (quest_get_state(QUEST_ID_NIENA_PACIFIST) == QUEST_STATE_ACTIVE &&
        total_player_kills_this_run_local() > 0)
    {
        p_ptr->niena_reserved |= NIENA_FLAG_PACIFIST_FAILED;
    }
}

void niena_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT && 
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        log_trace("niena_quest_interaction called with invalid quest state: %d", p_ptr->niena_quest);
        return;
    }
    
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Niena quest interaction - offering mercy quest");
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(4, &text_count); /* Nienna is quest index 4 */
        init_texts = prepend_repeat_context(QUEST_ID_NIENA, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Niena, Lady of Pity", init_texts, text_count, TERM_L_BLUE, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Niena, Lady of Pity, speaks with a voice full of sorrow and hope:",
                "'Show mercy to the creatures here and find the downward path.'"
            };
            quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
        }
        
        /* Accept the quest */
        p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
        p_ptr->niena_monsters_seen = 0;
        p_ptr->niena_monsters_killed = 0;
        p_ptr->niena_level = p_ptr->depth; /* Track where quest was started */
        
        /* Remove the quest giver now that quest is accepted */
        remove_quest_giver(R_IDX_NIENA);
        
        /* Make all stairs visible */
        int y, x;
        for (y = 0; y < p_ptr->cur_map_hgt; y++) {
            for (x = 0; x < p_ptr->cur_map_wid; x++) {
                if (cave_feat[y][x] == FEAT_MORE || cave_feat[y][x] == FEAT_MORE_SHAFT ||
                    cave_feat[y][x] == FEAT_LESS || cave_feat[y][x] == FEAT_LESS_SHAFT) {
                    cave_info[y][x] |= CAVE_MARK;
                    cave_info[y][x] |= CAVE_SEEN;
                }
            }
        }
        
        msg_print("The stairs throughout the level become clearly visible to you.");
        msg_print("Niena fades away, but her presence lingers in your heart.");
        
        /* Update display */
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
        p_ptr->redraw |= (PR_MAP);
        handle_stuff();
        
        log_trace("Niena quest started - all stairs revealed, monsters_seen=%d, monsters_killed=%d", 
                 p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
    }
    else if (p_ptr->niena_quest == NIENA_QUEST_SUCCESS)
    {
        log_trace("Completing Niena quest - giving enhanced stealth reward");
        
        /* Calculate the stealth bonus: 10 * (seen - killed) / seen, rounded up */
        int stealth_bonus = 0;
        if (p_ptr->niena_monsters_seen > 0) {
            /* Using ceiling division: (a + b - 1) / b */
            int mercy_ratio_times_10 = (10 * (p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed));
            stealth_bonus = (mercy_ratio_times_10 + p_ptr->niena_monsters_seen - 1) / p_ptr->niena_monsters_seen;
        }
        
        if (stealth_bonus > 0) {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Niena, Lady of Pity", completion_texts, completion_count, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Niena appears with tears of joy in her eyes!",
                    "'You have shown that true strength lies in restraint.'"
                };
                quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }
            
            /* Show the specific numbers and bonus after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.", 
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
            msg_format("You gain +%d effective stealth from your mercy.", stealth_bonus);
        } else {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 1) {
                /* Use alternate completion text if available */
                cptr alt_texts[] = {completion_texts[1]};
                quest_typewriter_menu("Niena, Lady of Pity", alt_texts, 1, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Niena appears, her expression neutral.",
                    "'You have completed the task, though perhaps not as I hoped.'"
                };
                quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }
            
            /* Show the specific numbers after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.", 
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
        }
        
        /* Clear quest state */
        p_ptr->niena_quest = NIENA_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(4); /* Nienna is quest index 4 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(4); /* Nienna is quest index 4 */
        
        /* Mark quest completion in metarun for score/persistence */
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
        log_trace("Niena quest marked complete in metarun and Mercy oath unlocked");
        
        msg_print("Niena smiles sadly and fades away, leaving you with her blessing.");
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_NIENA);
        
        /* Recalculate bonuses to apply the new stealth bonus */
        p_ptr->update |= (PU_BONUS);
        handle_stuff();
        
        log_trace("Niena quest completed and rewarded - stealth bonus: %d", stealth_bonus);
    }
}

/*
 * Check if player is adjacent to Niena and handle interaction
 */
void check_niena_quest_interaction(void)
{
    int y, x;
    monster_type* m_ptr;
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT && 
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        return;
    }
    
    log_trace("check_niena_quest_interaction: checking adjacency, quest state: %d", p_ptr->niena_quest);
    
    /* Look in adjacent squares for Niena */
    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            /* Skip the player's own square */
            if (y == p_ptr->py && x == p_ptr->px) continue;
            
            /* Skip invalid coordinates */
            if (!in_bounds(y, x)) continue;
            
            /* Check if there's a monster here */
            if (cave_m_idx[y][x] > 0)
            {
                m_ptr = &mon_list[cave_m_idx[y][x]];
                
                /* Is it Niena? */
                if (m_ptr->r_idx == R_IDX_NIENA)
                {
                    log_trace("Found Niena adjacent at (%d, %d), triggering interaction", y, x);
                    niena_quest_interaction();
                    return;
                }
            }
        }
    }
}

/*
 * Check if the player has completed the Niena mercy quest by reaching down stairs
 */
void check_niena_quest_completion(void)
{
    /* Only check if quest is active */
    if (p_ptr->niena_quest != NIENA_QUEST_ACTIVE) {
        return;
    }
    
    /* Check if player is on down stairs */
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE || 
        cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT) {
        
        log_trace("Player reached down stairs during Niena quest - quest completed!");
        log_trace("Final counts: seen=%d, killed=%d", p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
        
        p_ptr->niena_quest = NIENA_QUEST_SUCCESS;
        
        msg_print("As you step onto the stairs, you feel Niena's presence return.");
        msg_print("'You have done well, showing mercy where others would show only violence.'");
        msg_print("Wait here a moment - she wishes to speak with you.");
        
        /* Make Niena appear near the player for the reward interaction */
        int attempts;
        bool niena_spawned = false;
        
        for (attempts = 0; attempts < 20 && !niena_spawned; attempts++)
        {
            int try_y = p_ptr->py + rand_range(-3, 3);
            int try_x = p_ptr->px + rand_range(-3, 3);
            
            /* Must be valid coordinates and floor */
            if (in_bounds(try_y, try_x) && cave_floor_bold(try_y, try_x) && 
                cave_m_idx[try_y][try_x] == 0 &&
                distance(p_ptr->py, p_ptr->px, try_y, try_x) >= 2)
            {
                if (place_monster_one(try_y, try_x, R_IDX_NIENA, true, true, NULL))
                {
                    niena_spawned = true;
                    log_trace("Niena spawned near stairs at (%d, %d) for quest completion", try_y, try_x);
                }
            }
        }
        
        if (!niena_spawned) {
            log_trace("Failed to spawn Niena for quest completion - will complete anyway");
            /* Complete the quest directly if spawning fails */
            niena_quest_interaction();
        }
    }
}
