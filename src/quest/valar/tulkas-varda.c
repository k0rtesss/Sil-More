#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"
#include "ui/targeting/targeting-internal.h"

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

    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    /* Safety check - ensure valid quest state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT &&
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE)
    {
        log_trace("tulkas_quest_interaction called with invalid quest state: %d", p_ptr->tulkas_quest);
        return;
    }

    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Tulkas quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

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

        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_TULKAS);

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
                char* monster_pos = my_strstr(temp_text, "[monster name]");
                if (monster_pos) {
                    char before[512], after[512];
                    int before_len = monster_pos - temp_text;
                    SDL_strlcpy(before, temp_text, before_len + 1);
                    before[before_len] = '\0';
                    SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
                    strnfmt(temp_text, sizeof(temp_text), "%s%s%s", before, r_name + r_ptr->name, after);
                }

                /* Replace [artifact name] with actual artifact name */
                char* artifact_pos = my_strstr(temp_text, "[artifact name]");
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
        sdl_popup_notification_show("New quest added");
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
    /* Only check if quest is in appropriate state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT &&
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE)
    {
        return;
    }

    log_trace("check_tulkas_quest_interaction: checking adjacency, quest state: %d", p_ptr->tulkas_quest);

    if (trigger_adjacent_quest_giver_interaction(
        R_IDX_TULKAS, "Tulkas", tulkas_quest_interaction))
    {
        return;
    }

    if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE)
        ensure_reward_quest_giver_near_player(
            R_IDX_TULKAS, 3, "Tulkas",
            "Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!",
            NULL, NULL);
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

        ensure_reward_quest_giver_near_player(
            R_IDX_TULKAS, 3, "Tulkas",
            "Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!",
            NULL, NULL);
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

    /*
     * Repair saves created before target selection excluded peaceful,
     * non-attacking, and special-generation uniques.  Such a target cannot
     * satisfy the normal exact-death completion path.
     */
    if (!tulkas_target_race_valid(r_ptr))
    {
        int replacement_depth = MIN(p_ptr->depth, r_ptr->level);
        int replacement_r_idx =
            select_tulkas_quest_target_for_depth(replacement_depth);
        if (replacement_r_idx == 0 && replacement_depth > 0)
            replacement_r_idx = select_tulkas_quest_target_for_depth(0);

        log_warn("validate_tulkas_quest_on_load: found impossible target %d (%s)",
            p_ptr->tulkas_target_r_idx,
            r_ptr->name ? r_name + r_ptr->name : "(unnamed)");

        /*
         * The original encounter already consumed one of this run's quest
         * initiations.  Keep the accepted quest and its prize, replacing only
         * the impossible target so no second roulette encounter is required.
         */
        if (replacement_r_idx > 0)
        {
            log_warn("validate_tulkas_quest_on_load: replacing target %d with %d (%s)",
                p_ptr->tulkas_target_r_idx, replacement_r_idx,
                r_name + r_info[replacement_r_idx].name);
            p_ptr->tulkas_target_r_idx = replacement_r_idx;
            return;
        }

        /*
         * No eligible unique remains.  Release the abandoned quest and its
         * initiation slot rather than leaving the character permanently
         * blocked at the per-run cap.
         */
        if (valar_reserved_artifacts && p_ptr->tulkas_prize_a_idx > 0
            && p_ptr->tulkas_prize_a_idx < z_info->art_max)
        {
            valar_reserved_artifacts[p_ptr->tulkas_prize_a_idx] = false;
        }
        if (p_ptr->quest_reserved[0] > 0)
            p_ptr->quest_reserved[0]--;
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
        p_ptr->tulkas_target_r_idx = 0;
        p_ptr->tulkas_prize_a_idx = 0;
        p_ptr->tulkas_quest_complete = 0;
        return;
    }

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

/* -----------------------------
 * Varda quest helpers and flow
 * ----------------------------- */
static bool artefact_is_radiant_candidate(artefact_type* a_ptr)
{
    if (!a_ptr) return false;
    if (!(a_ptr->flags2 & (TR2_LIGHT | TR2_RADIANCE))) return false;
    if (a_ptr->flags2 & TR2_DARKNESS) return false;
    if (a_ptr->flags4 & TR4_UNLIGHT) return false;
    if (a_ptr->flags3 & TR3_LIGHT_CURSE) return false;
    return true;
}

static int varda_rarity_weight(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];
    int rarity = (a_ptr->rarity > 0) ? a_ptr->rarity : 1;

    return MAX(1, 10000 / rarity);
}

static int take_weighted_varda_candidate(int* candidates, int* candidate_count)
{
    int total_weight = 0;
    int pick;
    int selected = 0;

    if (!candidates || !candidate_count || *candidate_count <= 0) return 0;

    for (int i = 0; i < *candidate_count; i++) {
        total_weight += varda_rarity_weight(candidates[i]);
    }

    if (total_weight <= 0) return 0;

    pick = rand_int(total_weight);
    for (int i = 0; i < *candidate_count; i++) {
        int weight = varda_rarity_weight(candidates[i]);
        if (pick < weight) {
            selected = candidates[i];
            for (int j = i; j < *candidate_count - 1; j++) {
                candidates[j] = candidates[j + 1];
            }
            (*candidate_count)--;
            return selected;
        }
        pick -= weight;
    }

    selected = candidates[*candidate_count - 1];
    (*candidate_count)--;
    return selected;
}

static int build_varda_reward_options(int* choices, int max_choices)
{
    if (!choices || max_choices <= 0 || !z_info) return 0;

    /* Ensure reservation table exists so we can filter reserved artefacts */
    if (!valar_reserved_artifacts && z_info && z_info->art_max > 0) {
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
        for (int j = 0; j < z_info->art_max; j++) {
            valar_reserved_artifacts[j] = false;
        }
        log_trace("Varda reward: initialized valar_reserved_artifacts for %d artefacts", z_info->art_max);
    }

    int candidates[256];
    int candidate_count = 0;

    /* Early-light reward: bias toward items not far above current depth */
    int depth_cap = 25;
    if (p_ptr) {
        depth_cap = MIN(p_ptr->depth + 6, depth_cap);
    }
    depth_cap = MAX(depth_cap, 15); /* keep at least mid-depth options */

    for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
        artefact_type* a_ptr = &a_info[i];

        if (!a_ptr || a_ptr->tval == 0) continue;
        if (a_ptr->name[0] == '\0') continue;
        if (!artefact_is_radiant_candidate(a_ptr)) continue;
        if (a_ptr->cur_num != 0) continue; /* already created */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;
        if (a_ptr->level > depth_cap) continue;

        candidates[candidate_count++] = i;
    }

    /* Relax depth cap if we still need more options */
    if (candidate_count < max_choices) {
        for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
            artefact_type* a_ptr = &a_info[i];

            if (!a_ptr || a_ptr->tval == 0) continue;
            if (a_ptr->name[0] == '\0') continue;
            if (!artefact_is_radiant_candidate(a_ptr)) continue;
            if (a_ptr->cur_num != 0) continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

            candidates[candidate_count++] = i;
        }
    }

    if (candidate_count == 0) return 0;

    int final_count = MIN(max_choices, candidate_count);
    for (int i = 0; i < final_count; i++) {
        choices[i] = take_weighted_varda_candidate(candidates, &candidate_count);
    }
    return final_count;
}

static void describe_varda_choice(int a_idx, char* buf, size_t buf_len)
{
    artefact_type* a_ptr = &a_info[a_idx];
    object_type temp_obj;
    object_wipe(&temp_obj);

    s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (k_idx > 0) {
        object_prep(&temp_obj, k_idx);
        temp_obj.name1 = a_idx;
        temp_obj.ident |= IDENT_KNOWN;
        object_desc(buf, buf_len, &temp_obj, true, 0);
    } else if (a_ptr->name[0] != '\0') {
        SDL_strlcpy(buf, a_ptr->name, buf_len);
    } else {
        SDL_strlcpy(buf, "a radiant artefact", buf_len);
    }
}

static void inspect_varda_reward(int a_idx)
{
    desc_art_fake(a_idx);
}

/* Display Varda's completion and every radiant gift in the same parchment
 * book used by quest introductions.  Closing it leaves the reward pending. */
static int prompt_varda_reward_choice_menu(const int* choices,
    int choice_count, cptr* completion_texts, int text_count)
{
    cptr labels[3];
    char descriptions[3][120];

    for (int i = 0; i < choice_count; i++)
    {
        describe_varda_choice(choices[i], descriptions[i],
            sizeof(descriptions[i]));
        labels[i] = descriptions[i];
    }

    return quest_reward_book_choice("Starlight Triumph", completion_texts,
        text_count, "Choose your radiant gift:", choices, labels, NULL,
        choice_count, 0, inspect_varda_reward);
}

static bool grant_varda_reward(cptr* completion_texts, int completion_count)
{
    int choices[3] = {0};
    int available = build_varda_reward_options(choices, (int)N_ELEMENTS(choices));
    if (available <= 0) {
        msg_print("Varda has no radiant artefacts left to offer.");
        log_trace("Varda reward: no available radiant artefacts");
        return false;
    }

    /* Use the integrated scrollable menu with completion text */
    int selected = prompt_varda_reward_choice_menu(choices, available, completion_texts, completion_count);
    if (selected <= 0) {
        msg_print("The starlight gifts wait until you are ready to choose.");
        return false;
    }

    create_chosen_artefact(selected, p_ptr->py, p_ptr->px, true);
    msg_print("Starlight gathers at your feet, coalescing into a shining relic.");
    p_ptr->varda_quest = VARDA_QUEST_REWARDED;
    p_ptr->varda_vault_ready = 0;
    p_ptr->varda_vault_placed = 1;

    metarun_mark_quest_completed(METARUN_QUEST_VARDA);
    metarun_unlock_oath(OATH_LIGHT);
    do_cmd_note("Varda blessed me with a radiant artefact and the Oath of Light.", p_ptr->depth);

    return true;
}

static void varda_make_light_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++) {
        for (int nx = x - 1; nx <= x + 1; nx++) {
            if (!in_bounds(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

static void try_place_varda_near_player(void)
{
    int y = -1;
    int x = -1;

    if (ensure_reward_quest_giver_near_player(
        R_IDX_VARDA, 2, "Varda", NULL, &y, &x))
    {
        if (y >= 0 && x >= 0)
            varda_make_light_pool(y, x);
        p_ptr->varda_level = p_ptr->depth;
        return;
    }

    log_trace("Varda reward: failed to place quest giver near player; will retry while reward is pending");
}

static bool varda_quest_duruin_present(void)
{
    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_DURUIN) return true;
    }

    return false;
}

bool varda_quest_bastion_level_active(void)
{
    if (!p_ptr) return false;
    if (p_ptr->varda_quest != VARDA_QUEST_ACTIVE) return false;
    if (!p_ptr->varda_vault_placed) return false;
    if (p_ptr->varda_level != p_ptr->depth) return false;

    return varda_quest_duruin_present();
}

void varda_quest_note_duruin_ranged_attack(monster_type* m_ptr)
{
    if (!m_ptr || m_ptr->r_idx != R_IDX_DURUIN) return;
    if (p_ptr->varda_quest != VARDA_QUEST_ACTIVE) return;
    if (!p_ptr->varda_vault_placed) return;
    if (p_ptr->varda_level != p_ptr->depth) return;

    if (!(m_ptr->mflag & MFLAG_DURUIN_PROVOKED))
    {
        m_ptr->mflag |= MFLAG_DURUIN_PROVOKED;
        log_trace("Varda quest: Duruin provoked by a ranged attack");
    }
}

bool varda_quest_duruin_can_enter(
    const monster_type* m_ptr, int y, int x)
{
    int feat;

    if (!m_ptr || m_ptr->r_idx != R_IDX_DURUIN) return true;
    if (p_ptr->varda_quest != VARDA_QUEST_ACTIVE) return true;
    if (!p_ptr->varda_vault_placed) return true;
    if (p_ptr->varda_level != p_ptr->depth) return true;
    if (m_ptr->mflag & MFLAG_DURUIN_PROVOKED) return true;
    if (!in_bounds(y, x)) return false;

    /* The only exits from Duruin's inner enclosure are its two doors.  Keep
     * him from moving across either threshold until a bow or thrown attack
     * draws him out. */
    feat = cave_feat[y][x];
    return !cave_any_closed_door_bold(y, x)
        && feat != FEAT_OPEN && feat != FEAT_BROKEN;
}

void varda_quest_notice_bastion_level_entry(void)
{
    if (!varda_quest_bastion_level_active()) return;

    msg_print("Varda's quest presses upon you.");
    msg_print("This is the first level you have reached after 500 ft.");
    msg_print("Duruin, least of the Balrogs, waits here.");
    msg_print("His Bastion is on this level.");
    msg_print("Leave without slaying him and the quest is lost.");
}

bool varda_quest_confirm_leave_bastion(void)
{
    if (!varda_quest_bastion_level_active()) return true;

    msg_print("Duruin's Bastion lies on this level.");
    msg_print("It is the first level you reached after 500 ft.");
    msg_print("Leaving now will fail Varda's quest.");

    return get_check("Leave Duruin's Bastion and fail Varda's quest? ");
}

void varda_quest_fail_if_bastion_missed(void)
{
    if (!varda_quest_bastion_level_active()) return;

    p_ptr->varda_quest = VARDA_QUEST_FAILED;
    p_ptr->varda_vault_ready = 0;

    msg_print("You have left Duruin's Bastion behind.");
    msg_print("Varda's quest is lost.");
    do_cmd_note("Failed Varda's quest by leaving Duruin's Bastion behind.", p_ptr->depth);
    log_trace("Varda quest: FAILED - player left Duruin's Bastion at depth %d", p_ptr->depth);
}

void check_varda_quest_completion(int r_idx)
{
    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE && r_idx == R_IDX_DURUIN) {
        p_ptr->varda_quest = VARDA_QUEST_SUCCESS;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_level = p_ptr->depth;
        msg_print("Duruin falls. The Bastion's shadows unravel under starlight!");
        try_place_varda_near_player();
    }
}

void varda_quest_interaction(void)
{
    static s32b last_interaction_turn = -1;
    if (last_interaction_turn == turn) return;
    last_interaction_turn = turn;

    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT) {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Varda quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

        log_trace("Varda quest: accepting quest");
        p_ptr->varda_quest = VARDA_QUEST_ACTIVE;
        p_ptr->varda_level = p_ptr->depth;

        /* Remove quest giver for roulette quests without a generic
         * completion-style departure message during quest acceptance.
         */
        remove_quest_giver_silent(R_IDX_VARDA);

        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_VARDA, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_VARDA, init_texts, &text_count, false);
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Varda, Lady of the Stars", init_texts, text_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Varda's voice is clear as starlight:",
                "\"Seek Duruin's bastion of shadow and break it open to the Sun.\""
            };
            quest_typewriter_menu("Varda, Lady of the Stars", fallback, 2, TERM_WHITE, TERM_L_BLUE);
        }

        do_cmd_note("Varda sent me to destroy Duruin and cleanse his bastion.", p_ptr->depth);
        sdl_popup_notification_show("New quest added");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
        msg_print("Varda's whisper:");
        msg_print("\"Find Duruin's Bastion on the first level");
        msg_print("you reach after 500 ft.\"");
        msg_print("\"Leave it behind and the quest is lost.\"");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_SUCCESS) {
        log_trace("Varda quest: delivering reward");
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA, completion_texts, &completion_count, true);

        /* Fallback texts if quest data not available */
        cptr fallback[] = {
            "Varda inclines her head in silent approval:",
            "\"The stolen light is free. Choose your blessing.\""
        };
        cptr* texts_to_use = (completion_texts && completion_count > 0) ? completion_texts : fallback;
        int text_count = (completion_texts && completion_count > 0) ? completion_count : 2;

        /* Display quest completion and reward selection in one integrated menu */
        bool rewarded = grant_varda_reward(texts_to_use, text_count);

        if (completion_texts) {
            free_quest_texts(completion_texts, completion_count);
        }

        if (rewarded) {
            remove_quest_giver(R_IDX_VARDA);
        }
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED) {
        msg_print("Varda's blessing still follows the path you walk.");
    }
}

void check_varda_quest_interaction(void)
{
    if (p_ptr->varda_quest < VARDA_QUEST_GIVER_PRESENT || p_ptr->varda_quest > VARDA_QUEST_SUCCESS) return;

    if (trigger_adjacent_quest_giver_interaction(
        R_IDX_VARDA, "Varda", varda_quest_interaction))
    {
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_SUCCESS && !is_quest_giver_present(R_IDX_VARDA)) {
        log_trace("Varda quest: success state without nearby quest giver - attempting to place Varda");
        try_place_varda_near_player();
    }
}
