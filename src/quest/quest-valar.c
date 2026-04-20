/* File: quest-valar.c */
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

void remove_quest_giver_silent(int quest_giver_r_idx)
{
    int i;

    log_trace("Attempting to remove quest giver silently with R_IDX: %d", quest_giver_r_idx);

    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];

        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;

        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing silently", m_ptr->fy, m_ptr->fx);
            delete_monster_idx(i);
            return;
        }
    }

    log_trace("Quest giver with R_IDX %d not found on current level (silent remove)", quest_giver_r_idx);
}

/*
 * Remove quest giver monster by R_IDX
 */
void remove_quest_giver(int quest_giver_r_idx)
{
    int i;
    
    log_trace("Attempting to remove quest giver with R_IDX: %d", quest_giver_r_idx);
    
    /* Find and remove the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        
        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;
        
        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing", m_ptr->fy, m_ptr->fx);
            
            /* Add a message about the quest giver departing */
            msg_print("The quest giver nods approvingly and fades away, their task complete.");
            
            /* Remove the monster */
            delete_monster_idx(i);
            
            log_trace("Quest giver successfully removed");
            return;
        }
    }
    
    log_trace("Quest giver with R_IDX %d not found on current level", quest_giver_r_idx);
}

/*
 * Check if a quest giver is present on the current level
 */
bool is_quest_giver_present(int quest_giver_r_idx)
{
    int i;
    
    /* Find the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        
        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;
        
        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            return true;
        }
    }
    
    return false;
}

/*
 * Spawn a quest giver near the player (for debug completion)
 */
bool spawn_quest_giver_near_player(int quest_giver_r_idx)
{
    int y, x;
    
    log_trace("Attempting to spawn quest giver R_IDX %d near player", quest_giver_r_idx);
    
    /* Try to find a suitable spot near the player */
    for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
    {
        for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
        {
            if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
            {
                if (place_monster_one(y, x, quest_giver_r_idx, true, true, NULL))
                {
                    msg_print("A quest giver materializes nearby!");
                    log_trace("Successfully spawned quest giver at (%d, %d)", y, x);
                    return true;
                }
            }
        }
    }
    
    log_trace("Failed to spawn quest giver near player");
    return false;
}

static void unlock_quest_completion_challenge(int quest_id)
{
    int challenge_id = 0;

    if (quest_id > 0 && quest_id < z_info->quest_max) {
        challenge_id = quest_info[quest_id].challenge_unlock;
    }

    /* Varda's shadow quest unlocks the torchlight challenge in practice. */
    if (quest_id == QUEST_ID_VARDA_SHADOW && challenge_id == 0) {
        challenge_id = CHALLENGE_TORCHLIGHT;
    }

    switch (challenge_id)
    {
    case CHALLENGE_DISCONNECTED:
        if (!metarun_challenge_disconnected_unlocked()) {
            metarun_unlock_challenge_disconnected();
            msg_print("The disconnected-stair challenge is now unlocked.");
        }
        break;
    case CHALLENGE_SINGLE_STAIR:
        if (!metarun_challenge_single_stair_unlocked()) {
            metarun_unlock_challenge_single_stair();
            msg_print("The single-stair challenge is now unlocked.");
        }
        break;
    case CHALLENGE_FIXED_50K_XP:
        if (!metarun_challenge_fixed_exp_unlocked()) {
            metarun_unlock_challenge_fixed_exp();
            msg_print("The fixed 50k XP challenge is now unlocked.");
        }
        break;
    case CHALLENGE_TULKAS_BLUNT:
        if (!metarun_challenge_tulkas_blunt_unlocked()) {
            metarun_unlock_challenge_tulkas_blunt();
            msg_print("The blunt-arms challenge is now unlocked.");
        }
        break;
    case CHALLENGE_TORCHLIGHT:
        if (!metarun_challenge_torchlight_unlocked()) {
            metarun_unlock_challenge_torchlight();
            msg_print("The torches-only challenge is now unlocked.");
        }
        break;
    default:
        break;
    }
}

void grant_followup_quest_rewards(int quest_id)
{
    u32b quest_flag = quest_metarun_flag(quest_id);
    int oath_id = get_quest_oath_id(quest_id);

    if (quest_flag) {
        metarun_mark_quest_completed(quest_flag);
    }
    if (oath_id > 0) {
        metarun_unlock_oath(oath_id);
    }

    apply_quest_rewards(quest_id);
    unlock_quest_completion_challenge(quest_id);
}

static bool is_brodda_dead(void)
{
    int i;
    
    /* Check if Brodda is still alive on the level */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_ALDOR) /* Brodda uses the same monster index as Aldor */
        {
            log_trace("Brodda is still alive at (%d, %d)", m_ptr->fy, m_ptr->fx);
            return false;
        }
    }
    
    log_trace("Brodda has been slain");
    return true;
}

/*
 * Check if player is adjacent to Aule
 */
void check_aule_quest_interaction(void)
{
    int i, y, x;
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->aule_quest != AULE_QUEST_NOT_STARTED &&
        p_ptr->aule_quest != AULE_QUEST_FORGE_PRESENT && 
        p_ptr->aule_quest != AULE_QUEST_SUCCESS)
    {
        return;
    }
    
    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        return;
    }
    
    log_trace("check_aule_quest_interaction: checking adjacency, quest state: %d", p_ptr->aule_quest);
    
    /* Check all adjacent squares for Aule */
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
            
            /* Check if it's Aule */
            if (m_ptr->r_idx == R_IDX_AULE)
            {
                log_trace("Found Aule adjacent, calling interaction");
                aule_quest_interaction();
                return;
            }
        }
    }
}

/*
 * Handle Aule interaction
 */
void aule_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        log_trace("Quest already rewarded, no interaction");
        return;
    }
    
    /* Handle first encounter - initialize quest */
    if (p_ptr->aule_quest == AULE_QUEST_NOT_STARTED)
    {
        log_trace("First encounter with Aule - setting to FORGE_PRESENT");
        p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
        p_ptr->aule_level = p_ptr->depth;
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Aule the Smith, Maker of Mountains.");
        msg_print("'Speak with me again to learn of the challenges that await.'");
        return;
    }
    
    /* Handle quest explanation */
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        log_trace("Aule quest explanation - setting to ACTIVE");
        p_ptr->aule_quest = AULE_QUEST_ACTIVE;
        
        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[2]; /* Aule is quest index 2 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver(R_IDX_AULE);
            log_trace("Aule quest giver removed (roulette-based quest)");
        } else {
            log_trace("Aule quest giver NOT removed (vault-based quest)");
        }
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(2, &text_count); /* Aule is quest index 2 */
        init_texts = prepend_repeat_context(QUEST_ID_AULE, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Aule the Smith", init_texts, text_count, TERM_YELLOW, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aule speaks in a voice like hammer on anvil:",
                "'Find my forge and create something worthy of my attention.'"
            };
            quest_typewriter_menu("Aule the Smith", fallback_texts, 2, TERM_YELLOW, TERM_WHITE);
        }
        
        /* Mark in the notes */
        do_cmd_note("Aule has challenged me to use his forge to create an item.", p_ptr->depth);
        return;
    }
    
    /* Handle quest completion */
    if (p_ptr->aule_quest == AULE_QUEST_SUCCESS)
    {
        log_trace("Aule quest completed - giving special ability reward");
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(2, &completion_count); /* Aule is quest index 2 */
        completion_texts = prepend_repeat_context(QUEST_ID_AULE, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Complete!", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aule nods with satisfaction:",
                "'Well done! Your skill at the forge shows promise.'"
            };
            quest_typewriter_menu("Quest Complete!", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_AULE);
        
        /* Change quest state to prevent repeated interactions */
        p_ptr->aule_quest = AULE_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(2); /* Aule is quest index 2 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(2); /* Aule is quest index 2 */
        
        msg_print("Aule smiles with approval and returns to his eternal labors.");
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_AULE);
        
        return;
    }
    
    /* Handle other quest states */
    if (p_ptr->aule_quest == AULE_QUEST_ACTIVE)
    {
        msg_print("Aule watches you with eyes like glowing coals:");
        msg_print("'The forge awaits your skill. Show me what you can create.'");
        return;
    }
    
    /* Default message */
    msg_print("Aule the Smith regards you with interest.");
}

/*
 * Handle Mandos interaction
 */
void mandos_quest_interaction(void)
{
    byte second_state;
    byte third_state;

    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    second_state = quest_get_state(QUEST_ID_MANDOS_TRAITOR);
    third_state = quest_get_state(QUEST_ID_MANDOS_BETRAYER);

    if (second_state == QUEST_STATE_GIVER_PRESENT || third_state == QUEST_STATE_GIVER_PRESENT)
    {
        int quest_id = (third_state == QUEST_STATE_GIVER_PRESENT) ? QUEST_ID_MANDOS_BETRAYER : QUEST_ID_MANDOS_TRAITOR;
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(quest_id, &text_count);
        init_texts = prepend_repeat_context(quest_id, init_texts, &text_count, false);

        quest_set_state(quest_id, QUEST_STATE_ACTIVE);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Mandos the Doomsman", init_texts, text_count, TERM_L_DARK, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Mandos speaks with the chill certainty of doom.",
                "'Fulfil the judgment I have laid upon these traitors.'"
            };
            quest_typewriter_menu("Mandos the Doomsman", fallback, N_ELEMENTS(fallback), TERM_L_DARK, TERM_WHITE);
        }
        return;
    }

    if (second_state == QUEST_STATE_ACTIVE) {
        msg_print("Mandos intones: \"Ulfang and Uldor still await their doom in the fortress below.\"");
        return;
    }
    if (third_state == QUEST_STATE_ACTIVE) {
        msg_print("Mandos intones: \"Maeglin still hides from his judgment in the deep places.\"");
        return;
    }
    if (second_state == QUEST_STATE_SUCCESS || third_state == QUEST_STATE_SUCCESS)
    {
        int quest_id = (third_state == QUEST_STATE_SUCCESS) ? QUEST_ID_MANDOS_BETRAYER : QUEST_ID_MANDOS_TRAITOR;
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(quest_id, &completion_count);
        completion_texts = prepend_repeat_context(quest_id, completion_texts, &completion_count, true);

        quest_set_state(quest_id, QUEST_STATE_REWARDED);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Mandos the Doomsman", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Mandos's judgment is fulfilled, and the chamber falls still.",
                "'The doom appointed has been carried out.'"
            };
            quest_typewriter_menu("Mandos the Doomsman", fallback, N_ELEMENTS(fallback), TERM_L_GREEN, TERM_WHITE);
        }

        grant_followup_quest_rewards(quest_id);
        remove_quest_giver(R_IDX_MANDOS);
        return;
    }
    if (second_state == QUEST_STATE_REWARDED || third_state == QUEST_STATE_REWARDED) {
        msg_print("Mandos regards you in silence, as one whose doom has already been spoken.");
        return;
    }
    
    /* Handle first encounter - initialize quest */
    if (p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED)
    {
        log_trace("First encounter with Mandos - setting to GIVER_PRESENT");
        p_ptr->mandos_quest = MANDOS_QUEST_GIVER_PRESENT;
        p_ptr->mandos_level = p_ptr->depth;
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Mandos, the Doomsman of the Valar.");
        msg_print("His stern gaze weighs upon your soul, as if judging your worth.");
        return;
    }
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT && 
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        log_trace("mandos_quest_interaction called with invalid quest state: %d", p_ptr->mandos_quest);
        return;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Mandos quest interaction - assigning Brodda quest");
        
        /* Set quest state */
        p_ptr->mandos_quest = MANDOS_QUEST_ACTIVE;
        p_ptr->mandos_level = p_ptr->depth;
        
        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[3]; /* Mandos is quest index 3 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver(R_IDX_MANDOS);
            log_trace("Mandos quest giver removed (roulette-based quest)");
        } else {
            log_trace("Mandos quest giver NOT removed (vault-based quest)");
        }
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(3, &text_count); /* Mandos is quest index 3 */
        init_texts = prepend_repeat_context(QUEST_ID_MANDOS, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Mandos the Doomsman", init_texts, text_count, TERM_L_DARK, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos speaks with the authority of the Valar:",
                "'Slay Brodda the Easterling and prove your worth.'"
            };
            quest_typewriter_menu("Mandos the Doomsman", fallback_texts, 2, TERM_L_DARK, TERM_WHITE);
        }
        
        log_trace("Mandos quest activated - player must slay Brodda");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        /* Check if Brodda is dead */
        if (is_brodda_dead())
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
            
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
            completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Justice Served", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Mandos nods with solemn approval:",
                    "'Justice has been served. The path forward opens.'"
                };
                quest_typewriter_menu("Justice Served", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
            }
            
            log_trace("Mandos quest completed successfully");
        }
        else
        {
            msg_print("Mandos gazes at you with penetrating eyes:");
            msg_print("'Brodda the Easterling still draws breath within these halls.");
            msg_print("Until his tyranny is ended, you may not pass beyond.'");
            msg_print("");
            msg_print("'Remember - he who ruled Dor-lomin with an iron fist");
            msg_print("must face the justice he denied to others.'");
        }
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_SUCCESS)
    {
        log_trace("Mandos quest already completed - giving special ability reward");
        
        /* We'll show the same completion texts again since this is the reward phase */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
        completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Reward", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos acknowledges you with respect:",
                "'Accept the gift of my protection from mortal fears.'"
            };
            quest_typewriter_menu("Quest Reward", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
        log_trace("Mandos quest: marked as completed in metarun");
        
        /* Change quest state to prevent repeated interactions */
        p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(3); /* Mandos is quest index 3 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(3); /* Mandos is quest index 3 */
        
        msg_print("Mandos bows deeply and fades into shadow, his task complete.");
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_MANDOS);
        
        log_trace("Mandos quest reward given");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED)
    {
        log_trace("Mandos quest already rewarded - giving acknowledgment");
        msg_print("Mandos nods with solemn respect:");
        msg_print("'The task is done, and the doom has been fulfilled.'");
        msg_print("'Your path continues ever deeper into the halls of Mandos.'");
    }
}

/*
 * Check if player is adjacent to Mandos and handle interaction
 */
void check_mandos_quest_interaction(void)
{
    int i, y, x;
    byte second_state = quest_get_state(QUEST_ID_MANDOS_TRAITOR);
    byte third_state = quest_get_state(QUEST_ID_MANDOS_BETRAYER);
    static s32b last_interaction_turn = -1;
    
    log_trace("check_mandos_quest_interaction called, quest state: %d, turn: %d", p_ptr->mandos_quest, turn);
    
    /* Prevent multiple interactions in the same turn */
    if (last_interaction_turn == turn)
    {
        log_trace("Already interacted this turn, skipping");
        return;
    }
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED &&
        p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT && 
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED &&
        second_state != QUEST_STATE_GIVER_PRESENT &&
        second_state != QUEST_STATE_ACTIVE &&
        second_state != QUEST_STATE_SUCCESS &&
        second_state != QUEST_STATE_REWARDED &&
        third_state != QUEST_STATE_GIVER_PRESENT &&
        third_state != QUEST_STATE_ACTIVE &&
        third_state != QUEST_STATE_SUCCESS &&
        third_state != QUEST_STATE_REWARDED)
    {
        log_trace("Quest not in correct state (%d), returning", p_ptr->mandos_quest);
        return;
    }
    
    /* Check all adjacent squares for Mandos */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];
        
        if (in_bounds(y, x))
        {
            s16b m_idx = cave_m_idx[y][x];

            if (m_idx <= 0 || m_idx >= mon_max)
                continue;

            monster_type* m_ptr = &mon_list[m_idx];
            if (!m_ptr)
                continue;

            if (m_ptr->r_idx <= 0 || m_ptr->r_idx >= z_info->r_max)
                continue;

            if (m_ptr->r_idx == R_IDX_MANDOS)
            {
                log_trace("Found Mandos, calling interaction (turn %d)", turn);
                last_interaction_turn = turn;
                mandos_quest_interaction();
                return;
            }
        }
    }
    
    log_trace("No Mandos found adjacent");
}

/*
 * Handle monster death for Mandos quest
 */
void check_mandos_quest_completion(int r_idx)
{
    if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        log_trace("Mandos quest: Checking completion after death of r_idx %d", r_idx);
        
        /* Check if Brodda was killed */
        if (r_idx == R_IDX_ALDOR)  /* Brodda (formerly Aldor) */
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
            
            msg_print("Brodda the Easterling falls! His tyranny is ended at last.");
            msg_print("The spirits of Dor-lomin can finally know peace.");
            msg_print("Return to Mandos the Doomsman to claim your reward.");
            
            log_trace("Mandos quest completed - Brodda slain");
        }
    }

    if (quest_get_state(QUEST_ID_MANDOS_TRAITOR) == QUEST_STATE_ACTIVE)
    {
        bool last_traitor = false;

        if (r_idx == R_IDX_ULFANG && r_info[R_IDX_ULDOR].max_num == 0)
            last_traitor = true;
        if (r_idx == R_IDX_ULDOR && r_info[R_IDX_ULFANG].max_num == 0)
            last_traitor = true;

        if (last_traitor)
        {
            quest_set_state(QUEST_ID_MANDOS_TRAITOR, QUEST_STATE_SUCCESS);
            msg_print("The Easterling lords are fallen. Mandos waits to pronounce their doom complete.");
            if (!is_quest_giver_present(R_IDX_MANDOS))
                spawn_quest_giver_near_player(R_IDX_MANDOS);
        }
    }

    if (quest_get_state(QUEST_ID_MANDOS_BETRAYER) == QUEST_STATE_ACTIVE && r_idx == R_IDX_MAEGLIN)
    {
        quest_set_state(QUEST_ID_MANDOS_BETRAYER, QUEST_STATE_SUCCESS);
        msg_print("Maeglin the betrayer is cast down. Seek out Mandos for the final judgment.");
        if (!is_quest_giver_present(R_IDX_MANDOS))
            spawn_quest_giver_near_player(R_IDX_MANDOS);
    }
}
