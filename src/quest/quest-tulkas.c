/* File: quest-tulkas.c */
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

static bool tulkas_target_valid(int r_idx, const monster_race* r_ptr, int depth)
{
    if (!r_ptr) return false;

    if (!(r_ptr->flags1 & RF1_UNIQUE)) return false;
    if (r_ptr->max_num <= 0) return false;
    if (r_ptr->level < depth) return false;
    if (r_ptr->level > MORGOTH_DEPTH) return false;
    if (r_idx == R_IDX_TULKAS || r_idx == R_IDX_MORGOTH) return false;

    return true;
}

bool tulkas_has_valid_target(int depth)
{
    int i;

    if (!z_info) return false;

    for (i = 1; i < z_info->r_max; i++)
    {
        if (tulkas_target_valid(i, &r_info[i], depth)) return true;
    }

    return false;
}

static int select_tulkas_quest_target(void)
{
    int i;
    int valid_targets[50];
    int count = 0;
    
    log_trace("select_tulkas_quest_target: z_info=%p, r_max=%d", z_info, z_info ? z_info->r_max : -1);
    
    if (!z_info) 
    {
        log_trace("z_info is NULL!");
        return 0;
    }
    
    /* Look for unique monsters at current depth or deeper */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        
        if (!r_ptr) 
        {
            log_trace("r_info[%d] returned NULL", i);
            continue;
        }
        
        /* Must be unique, alive (max_num > 0), and at appropriate depth */
        /* Exclude Tulkas himself and Morgoth from being targets */
        if (tulkas_target_valid(i, r_ptr, p_ptr->depth))
        {
            valid_targets[count] = i;
            count++;
            if (count >= 50) break; /* Safety limit */
        }
    }
    
    if (count == 0) {
        log_trace("select_tulkas_quest_target: No valid unique targets found");
        return 0; /* No valid targets */
    }
    
    log_trace("select_tulkas_quest_target: Found %d valid unique targets", count);
    return valid_targets[rand_int(count)];
}

/*
 * Select a suitable artifact prize for the Tulkas quest
 */
static int select_tulkas_quest_prize(int target_level)
{
    int i;
    int valid_prizes[100];
    int count = 0;
    int max_artifact_level = target_level + 6; /* Not more than 6 levels deeper */
    
    log_trace("select_tulkas_quest_prize: target_level=%d, max_artifact_level=%d, z_info=%p, art_max=%d", 
              target_level, max_artifact_level, z_info, z_info ? z_info->art_max : -1);
    
    if (!z_info) 
    {
        log_trace("z_info is NULL!");
        return 0;
    }
    
    if (!valar_reserved_artifacts)
    {
        log_trace("valar_reserved_artifacts is NULL! Initializing...");
        
        /* Initialize the array if it doesn't exist */
        if (z_info && z_info->art_max > 0) {
            valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
            for (int j = 0; j < z_info->art_max; j++) {
                valar_reserved_artifacts[j] = false;
            }
            log_trace("Initialized valar_reserved_artifacts with %d entries", z_info->art_max);
        } else {
            log_error("Cannot initialize valar_reserved_artifacts: z_info=%p, art_max=%d", 
                     z_info, z_info ? z_info->art_max : -1);
            return 0;
        }
    }
    
    /* First pass: Look for artifacts with rarity >= 10 within depth constraint */
    for (i = 1; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        
        if (!a_ptr) 
        {
            log_trace("a_info[%d] returned NULL", i);
            continue;
        }
        
        /* Must be high rarity, within depth constraint, and not yet created */
        if ((a_ptr->rarity >= 10) &&
            (a_ptr->level >= target_level) &&
            (a_ptr->level <= max_artifact_level) &&
            (a_ptr->cur_num == 0) &&
            !valar_reserved_artifacts[i])
        {
            valid_prizes[count] = i;
            count++;
            if (count >= 100) break; /* Safety limit */
        }
    }
    
    /* If no suitable artifacts found with rarity >= 10, increase level requirement */
    if (count == 0)
    {
        log_trace("No artifacts found with rarity >= 10 within depth constraint, relaxing requirements");
        max_artifact_level = MORGOTH_DEPTH; /* Remove depth constraint */
        
        /* Second pass: Look for any artifacts with rarity >= 10 regardless of depth */
        for (i = 1; i < z_info->art_max; i++)
        {
            artefact_type* a_ptr = &a_info[i];
            
            if (!a_ptr) continue;
            
            /* Must be high rarity, appropriate level, and not yet created */
            if ((a_ptr->rarity >= 10) &&
                (a_ptr->level >= target_level) &&
                (a_ptr->cur_num == 0) &&
                !valar_reserved_artifacts[i])
            {
                valid_prizes[count] = i;
                count++;
                if (count >= 100) break; /* Safety limit */
            }
        }
        
        /* Third pass: If still no artifacts, take highest level artifact available */
        if (count == 0)
        {
            log_trace("No artifacts found with rarity >= 10, looking for highest level artifact");
            int best_artifact = 0;
            int best_level = 0;
            
            for (i = 1; i < z_info->art_max; i++)
            {
                artefact_type* a_ptr = &a_info[i];
                
                if (!a_ptr) continue;
                
                /* Must be high rarity and not yet created, ignore level constraint */
                if ((a_ptr->rarity >= 10) &&
                    (a_ptr->cur_num == 0) &&
                    !valar_reserved_artifacts[i] &&
                    (a_ptr->level > best_level))
                {
                    best_artifact = i;
                    best_level = a_ptr->level;
                }
            }
            
            if (best_artifact > 0)
            {
                valid_prizes[0] = best_artifact;
                count = 1;
                log_trace("Selected highest level artifact: %d (level %d)", best_artifact, best_level);
            }
        }
        
        if (count > 0)
        {
            log_trace("Found %d artifacts after relaxing depth constraint", count);
        }
    }
    else
    {
        log_trace("Found %d artifacts with rarity >= 10 within depth constraint", count);
    }
    
    if (count == 0) return 0; /* No valid prizes */
    
    return valid_prizes[rand_int(count)];
}

static const int tulkas_orc_targets[] = { 54, 76, 84, 85, 95, 105 };
static const size_t tulkas_orc_target_count = N_ELEMENTS(tulkas_orc_targets);

bool tulkas_orc_is_target(int r_idx)
{
    for (size_t i = 0; i < tulkas_orc_target_count; i++)
    {
        if (tulkas_orc_targets[i] == r_idx) return true;
    }
    return false;
}

bool tulkas_orc_targets_alive(bool require_unspawned)
{
    if (!z_info || !r_info || !l_list) return false;

    for (size_t i = 0; i < tulkas_orc_target_count; i++)
    {
        int r_idx = tulkas_orc_targets[i];
        if (r_idx <= 0 || r_idx >= z_info->r_max) return false;

        const monster_race* r_ptr = &r_info[r_idx];
        const monster_lore* l_ptr = &l_list[r_idx];

        if (!(r_ptr->flags1 & RF1_UNIQUE)) return false;
        if (r_ptr->max_num == 0) return false;

        if (require_unspawned)
        {
            if (l_ptr->psights > 0 || l_ptr->pkills > 0) return false;
        }
    }

    return true;
}

void ensure_tulkas_morgoth_active(void)
{
    byte state = quest_get_state(QUEST_ID_TULKAS_MORGOTH);
    if (state >= QUEST_STATE_REWARDED) return;

    bool oath_active = (p_ptr->oath_type == OATH_VALOROUS && !oath_invalid(OATH_VALOROUS));
    bool unlocked = metarun_quest_completion_count(METARUN_QUEST_TULKAS_ORCS) > 0 ||
        quest_get_state(QUEST_ID_TULKAS_ORCS) >= QUEST_STATE_REWARDED;

    if (!oath_active || !unlocked)
    {
        if (state != QUEST_STATE_NOT_STARTED) quest_set_state(QUEST_ID_TULKAS_MORGOTH, QUEST_STATE_NOT_STARTED);
        return;
    }

    if (state == QUEST_STATE_NOT_STARTED)
    {
        quest_set_state(QUEST_ID_TULKAS_MORGOTH, QUEST_STATE_ACTIVE);
        p_ptr->tulkas_morgoth_progress = 0;
    }
}

static void tulkas_quest_decline(cptr message)
{
    if (message) {
        msg_print(message);
    }

    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_target_r_idx = 0;
    p_ptr->tulkas_prize_a_idx = 0;
    p_ptr->tulkas_quest_complete = 0;

    remove_quest_giver_silent(R_IDX_TULKAS);
}

/*
 * Handle Tulkas interaction
 */
void tulkas_quest_interaction(void)
{
    int target_r_idx, prize_a_idx;
    monster_race* r_ptr;
    artefact_type* a_ptr;
    byte orc_state;
    
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    orc_state = quest_get_state(QUEST_ID_TULKAS_ORCS);
    if (orc_state == QUEST_STATE_GIVER_PRESENT)
    {
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_TULKAS_ORCS, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_TULKAS_ORCS, init_texts, &text_count, false);

        quest_set_state(QUEST_ID_TULKAS_ORCS, QUEST_STATE_ACTIVE);
        p_ptr->tulkas_orc_mask = 0;
        p_ptr->tulkas_orc_restricted = 1;
        remove_quest_giver(R_IDX_TULKAS);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Tulkas, Orc-Bane", init_texts, text_count, TERM_YELLOW, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Tulkas laughs like thunder in the stronghold's halls.",
                "'Break every captain in this den and leave not one of them standing.'"
            };
            quest_typewriter_menu("Tulkas, Orc-Bane", fallback, N_ELEMENTS(fallback), TERM_YELLOW, TERM_WHITE);
        }
        return;
    }
    if (orc_state == QUEST_STATE_SUCCESS)
    {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_TULKAS_ORCS, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_TULKAS_ORCS, completion_texts, &completion_count, true);

        quest_set_state(QUEST_ID_TULKAS_ORCS, QUEST_STATE_REWARDED);
        p_ptr->tulkas_orc_restricted = 0;
        remove_quest_giver(R_IDX_TULKAS);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Tulkas, Orc-Bane", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Tulkas strides from the dust of the broken stronghold.",
                "'You have crushed every captain and broken the orcs' pride.'"
            };
            quest_typewriter_menu("Tulkas, Orc-Bane", fallback, N_ELEMENTS(fallback), TERM_L_GREEN, TERM_WHITE);
        }

        grant_followup_quest_rewards(QUEST_ID_TULKAS_ORCS);
        return;
    }
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT && 
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE)
    {
        log_trace("tulkas_quest_interaction called with invalid quest state: %d", p_ptr->tulkas_quest);
        return;
    }
    
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Tulkas quest interaction - assigning target and prize");
        
        /* Assign quest target and prize */
        target_r_idx = select_tulkas_quest_target();
        if (target_r_idx == 0 || target_r_idx >= z_info->r_max)
        {
            log_trace("Invalid target_r_idx: %d", target_r_idx);
            tulkas_quest_decline("Tulkas nods thoughtfully and fades away, finding no worthy challenge for you at this time.");
            return;
        }
        
        r_ptr = &r_info[target_r_idx];
        if (target_r_idx <= 0 || target_r_idx >= z_info->r_max || r_ptr->name == 0)
        {
            log_trace("Invalid monster race data for r_idx: %d", target_r_idx);
            tulkas_quest_decline("Tulkas nods thoughtfully and fades away, finding no worthy challenge for you at this time.");
            return;
        }
        
        /* Validate that the target unique is still alive (double-check after selection) */
        if (r_ptr->max_num == 0)
        {
            log_trace("Target unique %d (%s) has already been killed (max_num=0)", target_r_idx, r_name + r_ptr->name);
            tulkas_quest_decline("Tulkas frowns. 'The foe I had in mind has already fallen. Impressive, but I have no new challenge for you now.'");
            return;
        }
        
        /* Select prize artifact that is 5 levels higher than the target monster */
        prize_a_idx = select_tulkas_quest_prize(r_ptr->level);
        if (prize_a_idx == 0 || prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid prize_a_idx: %d", prize_a_idx);
            tulkas_quest_decline("Tulkas frowns and fades away, having no suitable reward to offer.");
            return;
        }
        
        a_ptr = &a_info[prize_a_idx];
        if (prize_a_idx <= 0 || prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid artifact data for a_idx: %d", prize_a_idx);
            tulkas_quest_decline("Tulkas frowns and fades away, having no suitable reward to offer.");
            return;
        }
        
        /* Store quest data */
        p_ptr->tulkas_target_r_idx = target_r_idx;
        p_ptr->tulkas_prize_a_idx = prize_a_idx;
        p_ptr->tulkas_quest = TULKAS_QUEST_ACTIVE;
        
        /* Remove the quest giver now that quest is accepted */
        remove_quest_giver(R_IDX_TULKAS);
        
        /* Reserve the artifact */
        valar_reserved_artifacts[prize_a_idx] = true;
        
        log_trace("Quest assigned: target=%d (%s), prize=%d (%s)", 
                 target_r_idx, r_name + r_ptr->name, prize_a_idx, a_ptr->name);
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(1, &text_count); /* Tulkas is quest index 1 */
        init_texts = prepend_repeat_context(QUEST_ID_TULKAS, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            /* Substitute [monster name] and [artifact name] in the texts */
            cptr* processed_texts = mem_alloc_array(text_count, cptr);
            for (int i = 0; i < text_count; i++) {
                char temp_text[1024];
                SDL_strlcpy(temp_text, init_texts[i], sizeof(temp_text));
                
                /* Replace [monster name] with actual monster name */
                char* monster_pos = strstr(temp_text, "[monster name]");
                if (monster_pos) {
                    char before[512], after[512];
                    int before_len = monster_pos - temp_text;
                    SDL_strlcpy(before, temp_text, before_len + 1);
                    before[before_len] = '\0';
                    SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
                    strnfmt(temp_text, sizeof(temp_text), "%s%s%s", before, r_name + r_ptr->name, after);
                }
                
                /* Replace [artifact name] with actual artifact name */
                char* artifact_pos = strstr(temp_text, "[artifact name]");
                if (artifact_pos) {
                    char before[512], after[512];
                    int before_len = artifact_pos - temp_text;
                    SDL_strlcpy(before, temp_text, before_len + 1);
                    before[before_len] = '\0';
                    SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */
                    
                    /* Get proper artifact name using object_desc */
                    char artifact_name[120];
                    if (a_ptr->name[0] != '\0') {
                        /* Create a temporary object to get proper description */
                        object_type temp_obj;
                        object_wipe(&temp_obj);
                        
                        /* Set up the object as the artifact */
                        s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                        object_prep(&temp_obj, k_idx);
                        temp_obj.name1 = prize_a_idx;
                        temp_obj.ident |= IDENT_KNOWN;
                        
                        /* Get the full artifact description */
                        object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                    } else {
                        SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
                    }
                    
                    strnfmt(temp_text, sizeof(temp_text), "%s%s%s", before, artifact_name, after);
                }
                
                processed_texts[i] = str_dup(temp_text);
            }
            
            /* Display typewriter quest dialog */
            quest_typewriter_menu("Quest of Tulkas the Strong", processed_texts, text_count, TERM_YELLOW, TERM_WHITE);
            
            /* Clean up */
            for (int i = 0; i < text_count; i++) {
                if (processed_texts[i]) str_free((char*)processed_texts[i]);
            }
            mem_free_null(processed_texts);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            msg_print("Tulkas the Strong speaks of a great quest, but the words are lost in thunder.");
        }
    }
    else if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE)
    {
        log_trace("Completing Tulkas quest - giving reward artifact %d", p_ptr->tulkas_prize_a_idx);
        
        /* Safety check for valid artifact index */
        if (p_ptr->tulkas_prize_a_idx <= 0 || p_ptr->tulkas_prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid prize artifact index: %d", p_ptr->tulkas_prize_a_idx);
            msg_print("Tulkas appears but looks puzzled about your reward.");
            return;
        }
        
        log_trace("About to create artifact %d at position (%d,%d)", p_ptr->tulkas_prize_a_idx, p_ptr->py, p_ptr->px);
        
        /* Give the artifact reward */
        create_chosen_artefact(p_ptr->tulkas_prize_a_idx, p_ptr->py, p_ptr->px, true);
        
        log_trace("Successfully created artifact %d", p_ptr->tulkas_prize_a_idx);
        
        /* Clear quest state */
        if (valar_reserved_artifacts && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            valar_reserved_artifacts[p_ptr->tulkas_prize_a_idx] = false;
            log_trace("Cleared reservation for artifact %d", p_ptr->tulkas_prize_a_idx);
        } else {
            log_trace("Skipping reservation clear: valar_reserved_artifacts=%p, artifact_idx=%d, art_max=%d", 
                     valar_reserved_artifacts, p_ptr->tulkas_prize_a_idx, z_info->art_max);
        }
        p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
        p_ptr->tulkas_target_r_idx = 0;
        p_ptr->tulkas_prize_a_idx = 0;
        p_ptr->tulkas_quest_complete = 0;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(1); /* Tulkas is quest index 1 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(1); /* Tulkas is quest index 1 */
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(1, &completion_count); /* Tulkas is quest index 1 */
        completion_texts = prepend_repeat_context(QUEST_ID_TULKAS, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            /* Display typewriter completion dialog */
            quest_typewriter_menu("Quest Complete: Tulkas Rewards Your Valor", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Tulkas appears with a great laugh of triumph!",
                "'Well fought, warrior! You have proven your valor in battle.'"
            };
            quest_typewriter_menu("Quest Complete: Tulkas Rewards Your Valor", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }

        metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_TULKAS);
        
        log_trace("Tulkas quest completed and rewarded");
    }
}

/*
 * Check if player is adjacent to Tulkas and handle interaction
 */
void check_tulkas_quest_interaction(void)
{
    int i, y, x;
    byte orc_state = quest_get_state(QUEST_ID_TULKAS_ORCS);
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT && 
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE &&
        orc_state != QUEST_STATE_GIVER_PRESENT &&
        orc_state != QUEST_STATE_SUCCESS)
    {
        return;
    }
    
    log_trace("check_tulkas_quest_interaction: checking adjacency, quest state: %d", p_ptr->tulkas_quest);
    
    /* Check all adjacent squares for Tulkas */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];
        
        /* Check bounds */
        if (!in_bounds(y, x)) continue;
        
        /* Check for monster */
        if (cave_m_idx[y][x] > 0)
        {
            int m_idx = cave_m_idx[y][x];
            
            if (m_idx >= mon_max) continue;
            
            monster_type* m_ptr = &mon_list[m_idx];
            
            /* Check if it's Tulkas */
            if (m_ptr->r_idx == R_IDX_TULKAS)
            {
                log_trace("Found Tulkas adjacent, calling interaction");
                tulkas_quest_interaction();
                return;
            }
        }
    }

    if (orc_state == QUEST_STATE_SUCCESS && !is_quest_giver_present(R_IDX_TULKAS))
    {
        spawn_quest_giver_near_player(R_IDX_TULKAS);
    }
}

/*
 * Handle monster death for Tulkas quest
 */
void check_tulkas_quest_completion(int r_idx)
{
    if (p_ptr->tulkas_quest == TULKAS_QUEST_ACTIVE && 
        r_idx == p_ptr->tulkas_target_r_idx)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
        p_ptr->tulkas_quest_complete = 1;
        
        msg_print("The deed is done! Seek out Tulkas Unclad to claim your reward.");
        
        /* Spawn Tulkas in the same room */
        int y, x;
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    place_monster_one(y, x, R_IDX_TULKAS, true, true, NULL);
                    msg_print("Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!");
                    return;
                }
            }
        }
    }

    if (quest_get_state(QUEST_ID_TULKAS_ORCS) == QUEST_STATE_ACTIVE && tulkas_orc_is_target(r_idx))
    {
        for (size_t i = 0; i < tulkas_orc_target_count; i++)
        {
            if (tulkas_orc_targets[i] == r_idx) {
                p_ptr->tulkas_orc_mask |= (1 << i);
                break;
            }
        }

        if ((byte)p_ptr->tulkas_orc_mask == (byte)((1 << tulkas_orc_target_count) - 1))
        {
            quest_set_state(QUEST_ID_TULKAS_ORCS, QUEST_STATE_SUCCESS);
            p_ptr->tulkas_orc_restricted = 0;
            msg_print("The last orc captain falls. Seek out Tulkas to claim your reward.");
            spawn_quest_giver_near_player(R_IDX_TULKAS);
        }
    }
}

/*
 * Validate Tulkas quest target on game load
 * Auto-completes the quest if the assigned target is already dead
 * This fixes stuck saves where players have dead targets assigned
 */
void validate_tulkas_quest_on_load(void)
{
    monster_race* r_ptr;
    
    /* Only validate if quest is in ACTIVE state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_ACTIVE)
    {
        return;
    }
    
    /* Check if we have a valid target assigned */
    if (p_ptr->tulkas_target_r_idx <= 0 || p_ptr->tulkas_target_r_idx >= z_info->r_max)
    {
        log_trace("validate_tulkas_quest_on_load: Invalid target r_idx=%d, skipping", p_ptr->tulkas_target_r_idx);
        return;
    }
    
    r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
    
    /* Check if the target unique is already dead (max_num == 0) */
    if (r_ptr->max_num == 0)
    {
        log_trace("validate_tulkas_quest_on_load: Target unique %d (%s) is dead (max_num=0), auto-completing quest",
                 p_ptr->tulkas_target_r_idx, r_name + r_ptr->name);
        
        /* Validate we have a valid artifact prize */
        if (p_ptr->tulkas_prize_a_idx <= 0 || p_ptr->tulkas_prize_a_idx >= z_info->art_max)
        {
            log_trace("validate_tulkas_quest_on_load: Invalid prize artifact index: %d, clearing quest", p_ptr->tulkas_prize_a_idx);
            
            /* Clear quest state without reward */
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            p_ptr->tulkas_target_r_idx = 0;
            p_ptr->tulkas_prize_a_idx = 0;
            p_ptr->tulkas_quest_complete = 0;
            return;
        }
        
        /* Set quest to COMPLETE state - this will trigger normal completion flow */
        /* Tulkas will spawn near player on next level generation/turn */
        p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
        p_ptr->tulkas_quest_complete = 1;
        
        log_trace("validate_tulkas_quest_on_load: Quest set to COMPLETE state, Tulkas will spawn on next turn");
    }
    else
    {
        log_trace("validate_tulkas_quest_on_load: Target unique %d (%s) is still alive (max_num=%d)",
                 p_ptr->tulkas_target_r_idx, r_name + r_ptr->name, r_ptr->max_num);
    }
}
