/* File: quest-rewards.c */

#include "angband.h"
#include "externs.h"
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

    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

static u32b get_metarun_quest_flag(int quest_idx)
{
    return quest_metarun_flag(quest_idx);
}

/*
 * Apply quest rewards (stats, skills, abilities) based on quest.txt data
 */
void apply_quest_rewards(int quest_idx)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (!p_ptr || quest_idx <= 0 || quest_idx >= z_info->quest_max) return;
    
    q_ptr = &quest_info[quest_idx];
    
    /* Apply stat bonuses */
    for (int i = 0; i < 4; i++) {
        if (q_ptr->stat_bonuses[i] > 0) {
            /* stat_bonuses array: [str, dex, con, gra] */
            int stat_idx = i; /* A_STR=0, A_DEX=1, A_CON=2, A_GRA=3 */
            
            for (int j = 0; j < q_ptr->stat_bonuses[i]; j++) {
                if (p_ptr->stat_base[stat_idx] < BASE_STAT_MAX) {
                    p_ptr->stat_base[stat_idx]++;
                }
            }
            
            log_trace("Applied %s bonus: +%d", 
                     (i == 0 ? "STR" : i == 1 ? "DEX" : i == 2 ? "CON" : "GRA"), 
                     q_ptr->stat_bonuses[i]);
        }
    }
    
    /* Apply skill bonus */
    if (q_ptr->skill_bonus > 0 && q_ptr->skill_type < S_MAX) {
        p_ptr->skill_base[q_ptr->skill_type] += q_ptr->skill_bonus;

        switch (q_ptr->skill_type) {
            case S_MEL:
                log_trace("Applied Melee bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_ARC:
                log_trace("Applied Archery bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_EVN:
                log_trace("Applied Evasion bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_STL:
                log_trace("Applied Stealth bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_PER:
                log_trace("Applied Perception bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_WIL:
                log_trace("Applied Will bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SMT:
                log_trace("Applied Smithing bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SNG:
                log_trace("Applied Song bonus: +%d", q_ptr->skill_bonus);
                break;
            default:
                log_trace("Applied skill bonus: skill=%d bonus=+%d",
                          q_ptr->skill_type, q_ptr->skill_bonus);
                break;
        }
    }
    
    /* Apply special ability */
    if (q_ptr->ability_type > 0 && q_ptr->ability_id < ABILITIES_MAX) {
        /* ability_type 8 is the Special skill type (S_SPC) based on quest.txt */
        if (q_ptr->ability_type == 8) {
            /* Grant the special ability using the ability_id from quest.txt */
            if (!p_ptr->have_ability[S_SPC][q_ptr->ability_id]) {
                p_ptr->have_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->innate_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->active_ability[S_SPC][q_ptr->ability_id] = true;
                ability_log_record_gain(S_SPC, q_ptr->ability_id);
                
                /* Get the ability name for the message */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You have learned %s!", b_name + b_ptr->name);
                    log_trace("Applied special ability: %s (skill=%d, ability=%d)", 
                             b_name + b_ptr->name, q_ptr->ability_type, q_ptr->ability_id);
                } else {
                    msg_print("You have gained a new special ability!");
                    log_trace("Applied special ability: Unknown name (skill=%d, ability=%d)", 
                             q_ptr->ability_type, q_ptr->ability_id);
                }
            } else {
                /* Already have this ability */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You already possess %s.", b_name + b_ptr->name);
                } else {
                    msg_print("You already possess this special ability.");
                }
                log_trace("Special ability already granted: skill=%d, ability=%d", 
                         q_ptr->ability_type, q_ptr->ability_id);
            }
        }
        else {
            log_trace("Unknown ability type: %d (not implemented)", q_ptr->ability_type);
        }
    }
    
    /* Recalculate bonuses and redraw */
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATS);
}

/*
 * Get oath ID from quest data
 */
int get_quest_oath_id(int quest_idx)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return 0; /* No oath */
    
    q_ptr = &quest_info[quest_idx];
    return q_ptr->oath_id;
}

/*
 * Check quest eligibility based on E: field data and standard quest requirements
 * Includes metarun completion checks and quest state checks
 */
bool check_quest_eligibility(int quest_idx, int depth)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return false;
    
    q_ptr = &quest_info[quest_idx];
    
    /* Debug quest 2 specifically - show what was actually loaded */
    if (quest_idx == 2) {
        log_trace("Quest %d (Aule) LOADED DATA: eligibility_type=%d, eligibility_skill=%d, eligibility_value=%d", 
                 quest_idx, q_ptr->eligibility_type, q_ptr->eligibility_skill, q_ptr->eligibility_value);
        log_trace("Quest %d (Aule) LOADED DATA: This data was loaded from save file, not parsed from quest.txt", quest_idx);
    }
    
    /* Check standard requirements that apply to all quests */
    /* 1. Quest not already completed in current metarun unless oath override applies */
    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int metarun_count = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    bool oath_override = false;
    if (q_ptr->oath_id > 0 && p_ptr && p_ptr->oath_type == q_ptr->oath_id && !oath_invalid(q_ptr->oath_id)) {
        oath_override = true;
    }
    
    if (metarun_flag) {
        int completion_cap = quest_completion_cap(quest_idx);
        if (metarun_count >= completion_cap) {
            log_trace("Quest %d eligibility: METARUN_CAP (%d/%d) = FAIL", quest_idx, metarun_count, completion_cap);
            return false;
        }
        if (metarun_count > 0 && !oath_override) {
            log_trace("Quest %d eligibility: METARUN_COMPLETED (count=%d) without oath override = FAIL", quest_idx, metarun_count);
            return false;
        }
        if (metarun_count > 0 && oath_override) {
            log_trace("Quest %d eligibility: metarun completion count=%d overridden by active oath %d", quest_idx, metarun_count, q_ptr->oath_id);
        }
    }
    
    /* 2. Check quest-specific state (must not be started for roulette quests) */
    switch (quest_idx) {
        case 1: /* Tulkas */
            if (p_ptr->tulkas_quest != TULKAS_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: TULKAS_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            if (!tulkas_has_valid_target(depth)) {
                log_trace("Quest %d eligibility: TULKAS_NO_TARGETS = FAIL (depth=%d)", quest_idx, depth);
                return false;
            }
            break;
        case 4: /* Niena */
            if (p_ptr->niena_quest != NIENA_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: NIENA_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
    }
    
    /* 3. Only one quest per run (for roulette quests) */
    if (q_ptr->quest_type == 1 && p_ptr->quest_reserved[0]) { /* Y:1 = roulette quest */
        log_trace("Quest %d eligibility: QUEST_RESERVED = FAIL", quest_idx);
        return false;
    }
    
    /* Check eligibility type from E: field */
    switch (q_ptr->eligibility_type) {
        case 0: /* No requirements */
            log_trace("Quest %d eligibility: NO_REQUIREMENTS = PASS", quest_idx);
            return true;
            
        case 1: /* SKILL_MIN - minimum skill requirement */
            if (q_ptr->eligibility_skill < S_MAX) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);
                
                /* Enhanced logging for debugging smith skill issues */
                if (q_ptr->eligibility_skill == S_SMT) {
                    log_trace("SMT SKILL DEBUG: Quest %d eligibility check:", quest_idx);
                    log_trace("  skill_base[S_SMT] = %d", p_ptr->skill_base[S_SMT]);
                    log_trace("  skill_use[S_SMT] = %d", p_ptr->skill_use[S_SMT]);
                    log_trace("  required value = %d", q_ptr->eligibility_value);
                    log_trace("  meets requirement = %s", meets_req ? "YES" : "NO");
                }
                
                log_trace("Quest %d eligibility: SKILL_MIN (skill=%d, player=%d, required=%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            return false;
            
        case 2: /* SKILL_RANGE - skill requirement within depth range */
            if (q_ptr->eligibility_skill < S_MAX && 
                depth >= q_ptr->eligibility_depth_min && 
                depth <= q_ptr->eligibility_depth_max) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);
                log_trace("Quest %d eligibility: SKILL_RANGE (skill=%d, player=%d, required=%d, depth=%d in %d-%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            log_trace("Quest %d eligibility: SKILL_RANGE (depth=%d NOT in %d-%d) = FAIL",
                     quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max);
            return false;
            
        case 3: /* DEPTH_RANGE - must be within depth range */
            {
                bool in_range = (depth >= q_ptr->eligibility_depth_min && depth <= q_ptr->eligibility_depth_max);
                log_trace("Quest %d eligibility: DEPTH_RANGE (depth=%d in %d-%d) = %s",
                         quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         in_range ? "PASS" : "FAIL");
                return in_range;
            }
            
        default:
            log_trace("Quest %d eligibility: Unknown type %d = FAIL", quest_idx, q_ptr->eligibility_type);
            return false;
    }
}

/*
 * Extract quest initialization texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_init_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;
    
    /* Initialize count */
    if (count) *count = 0;
    else return NULL;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;
    
    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->init_text) return NULL;
    
    /* Get the initialization text */
    full_text = q_text + q_ptr->init_text;
    if (!full_text || strlen(full_text) == 0) return NULL;
    
    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;
    
    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);
    
    /* Split text by single newlines (each I: line becomes an entry) */
    line_start = text_copy;
    while (line_start && *line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }
        
        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;
        
        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* No more lines to process */
            break;
        }
    }
    
    /* Clean up */
    mem_free_null(text_copy);
    
    *count = text_count;
    return texts;
}

/*
 * Extract quest completion texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_completion_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;
    
    /* Initialize count */
    if (count) *count = 0;
    else return NULL;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;
    
    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->completion_text) return NULL;
    
    /* Get the completion text */
    full_text = q_text + q_ptr->completion_text;
    if (!full_text || strlen(full_text) == 0) return NULL;
    
    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;
    
    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);
    
    /* Split text by single newlines (each W: line becomes an entry) */
    line_start = text_copy;
    while (line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }
        
        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;
        
        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* This was the last line, we're done */
            break;
        }
    }
    
    /* Clean up */
    mem_free_null(text_copy);
    
    *count = text_count;
    return texts;
}


cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion)
{
    if (!texts || !count || quest_idx <= 0 || quest_idx >= z_info->quest_max) return texts;

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr || q_ptr->oath_id <= 0) return texts;
    if (!p_ptr || p_ptr->oath_type != q_ptr->oath_id || oath_invalid(q_ptr->oath_id)) return texts;

    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int previous = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    if (previous <= 0) return texts;

    cptr quest_title = quest_display_title(quest_idx);
    cptr oath_name = get_oath_name_from_id(q_ptr->oath_id);

    char repeat_line[180];
    strnfmt(repeat_line, sizeof(repeat_line),
            is_completion
                ? "%s honors your %s oath after %d earlier success%s."
                : "%s returns under your %s oath; you have succeeded %d time%s before.",
            quest_title ? quest_title : "This quest",
            oath_name ? oath_name : "oath",
            previous,
            (previous == 1 ? "" : "s"));

    cptr* new_texts = mem_alloc_array(*count + 1, cptr);
    if (!new_texts) return texts;
    new_texts[0] = str_dup(repeat_line);
    for (int i = 0; i < *count; i++) new_texts[i + 1] = texts[i];
    mem_free_null(texts);
    (*count)++;
    return new_texts;
}

void free_quest_texts(cptr* texts, int count)
{
    if (!texts) return;

    if (count < 0) count = 0;
    if (count > 50) count = 50; /* hard cap safety */

    for (int i = 0; i < count; i++) {
        if (texts[i]) {
            str_free((char*)texts[i]);
        }
    }

    mem_free_null(texts);
}
