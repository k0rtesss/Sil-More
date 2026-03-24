/* File: xtra2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion);
void grant_followup_quest_rewards(int quest_id);
int orome_great_hunt_bit_for_target(int r_idx);

/*
 * xtra2.c now contains the quest system and the remaining shared
 * presentation helpers that have not been split out yet.
 */

/*
 *  Choose the location of a random staircase on the level
 */
bool random_stair_location(int* sy, int* sx)
{
    int stair_y[100];
    int stair_x[100];
    int stair_num = 0;
    int y, x;

    // Note all the stairs
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_stair_bold(y, x))
            {
                stair_y[stair_num] = y;
                stair_x[stair_num] = x;
                if (stair_num < 99)
                    stair_num++;
            }
        }
    }

    // If no valid stairs are found, then bail out (paranoia)
    if (stair_num == 0)
    {
        return (false);
    }

    // Choose a random stair
    stair_num = rand_int(stair_num);
    *sy = stair_y[stair_num];
    *sx = stair_x[stair_num];

    return (true);
}

/*
 * Break the truce in Morgoth's throne room
 */
extern void break_truce(bool obvious)
{
    int i;

    monster_type* m_ptr = NULL; // default to soothe compiler warnings

    char m_name[80];

    if (p_ptr->truce)
    {
        /* Scan all other monsters */
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            // Ignore monsters out of line of sight
            if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
                continue;

            // Ignore unalert monsters
            if (m_ptr->alertness < ALERTNESS_ALERT)
                continue;

            /* Get the monster name (using 'something' for hidden creatures) */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

            p_ptr->truce = false;
        }

        if (obvious)
            p_ptr->truce = false;

        if (!p_ptr->truce)
        {
            if (!obvious)
            {
                msg_format(
                    "%^s lets out a cry! The tension is broken.", m_name);

                /* Make a lot of noise */
                update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
                monster_perception(false, false, -10);
            }
            else
            {
                msg_print("The tension is broken.");
            }

            /* Scan all other monsters */
            for (i = mon_max - 1; i >= 1; i--)
            {
                /* Access the monster */
                m_ptr = &mon_list[i];

                /* Ignore dead monsters */
                if (!m_ptr->r_idx)
                    continue;

                /* Mark minimum desired range for recalculation */
                m_ptr->min_range = 0;
            }
        }
    }
}

const char entry_poetry[][100] = { { "Into the vast and echoing gloom," },
    { "more dread than many-tunnelled tomb" },
    //	{ "in labyrinthine pyramid" },
    //	{ "where everlasting death is hid," },
    { "  down awful corridors that wind" },
    { "    down to a menace dark enshrined;" },
    { "      down to the mountain's roots profound," },
    { "devoured, tormented, bored and ground" },
    { "by seething vermin spawned of stone;" },
    { "  down to the depths they went alone..." },

    { "" } };

const char tutorial_leave_text[][100] = {
    { "You have finished the first half of the tutorial and are ready" },
    { "to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "Congratulations. You have survived a fire-drake (usually found" },
    { "at 900 ft!), and have finished the tutorial in fine form." },
    { "You are more than ready to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "A key feature of Sil (and all Roguelike games) is that you cannot" },
    { "use savepoints: if you die, that's it!" },
    { "It is thus a challenging game where you need to really *think*." },
    { " " },
    { "However, it is a bit frustrating to die before the end of the" },
    { "tutorial, so we evidentally made it a bit too deadly." }, { " " },
    { "Just restart the tutorial and you should be back to where you" },
    { "were in a couple of minutes. Remember that if combat is not going" },
    { "your way, you can try to escape and heal, then either come back" },
    { "and again to defeat your adversary, or simply ignore it." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "Congratulations: you have finished the tutorial." }, { " " },
    { "You have also just been through a rite of passage: dying." },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },
    { " " },
    { "You are now more than ready to create a character and start playing." },
    { " " }, { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." },

    { "" }
};

const char throne_poetry[][100] = { { "Loud rose a din of laughter hoarse," },
    { "  self-loathing yet without remorse;" },
    { "    loud came a singing harsh and fierce" },
    { "      like swords of terror souls to pierce." },
    { "Red was the glare through open doors" },
    { "  of firelight mirrored on brazen floors," },
    { "    and up the arches towering clomb" },
    { "      to glooms unguessed, to vaulted dome" },
    { "        swathed in wavering smokes and steams" },
    { "          stabbed with flickering lightning-gleams." },

    { "" } };

/*
const char throne_poetry2[][100] =
{
        { "To Morgoth's hall, where dreadful feast" },
        { "he held, and drank the blood of beast" },
        { "and lives of Men, she stumbling came:" },
        { "her eyes were dazed with smoke and flame." },
        { "The pillars, reared like monstrous shores" },
        { "to bear earth's overwhelming floors," },
        { "were devil-carven, shaped with skill" },
        { "such as unholy dreams doth fill:" },
        { "they towered like trees into the air," },
        { "whose trunks are rooted in despair," },
        { "whose shade is death, whose fruit is bane," },
        { "whose boughs like serpents writhe in pain." },
        { "Beneath them ranged with spear and sword" },
        { "stood Morgoth's sable-armoured horde:" },
        { "the fire on blade and boss of shield" },
        { "was red as blood on stricken field." },
        { "Beneath a monstrous column loomed" },
        { "the throne of Morgoth, and the doomed" },
        { "and dying gasped upon the floor:" },
        { "his hideous footstool, rape of war." },

        { "" }
};
*/

const char ultimate_bug_text[][100]
    = { { "Against all hope, you defeated the Dark Enemy," },
          { "  and destroyed his physical form." },
          { "    For the rest of this age at least," },
          { "      Arda shall be free from the tyrant's shadow." },
          { "But there will be time later for reflection" },
          { "  on this great change to Arda's fate." },
          { "    You are buried still in Angband's vaults" },
          { "      -- make quick your bold escape!" },

          { "" } };

static int pause_with_text_print_wrapped_segment(int row, int col, byte attr,
                                                 cptr text, int delay_msec)
{
    int term_wid = 80;
    int term_hgt = 24;
    int max_cols;
    int wrap_col;
    int rows_used = 1;

    if (!text)
        text = "";

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    if (row < 0 || row >= term_hgt)
        return 0;

    if (col < 0)
        col = 0;
    if (col >= term_wid)
        col = term_wid - 1;

    max_cols = term_wid - col - 2;
    if (max_cols < 1)
        max_cols = 1;

    wrap_col = col + max_cols;

    if (*text)
    {
        if (sdl_is_story_font_enabled())
            rows_used = count_wrapped_lines_story(text, wrap_col, col);
        else
            rows_used = count_wrapped_lines(text, wrap_col, col);

        if (rows_used < 1)
            rows_used = 1;
    }

    story_print_text(row, col, max_cols, attr, text);
    Term_fresh();

    if (delay_msec > 0)
        Term_xtra(TERM_XTRA_DELAY, delay_msec);

    return rows_used;
}

/* pause_with_text: prints name+alt, explicit blank line, then wrapped start splits */
void pause_with_text(const char desc[][100], int row, int col,
                     const char extra[][100], byte extra_attr)
{
    int i_main = 0, msec = 50;
    int banner_lines = 0;
    int main_rows = 0;
    int term_wid = 80;
    int term_hgt = 24;

    /* 0. save & clear screen */
    screen_save();
    Term_clear();
    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;
    (void)term_wid;

    sdl_story_font_enable();
    log_debug("Banner: story font enabled");

    /* 1. optional banner */
    if (extra) {
        /* Line 1: name+alt */
        banner_lines += pause_with_text_print_wrapped_segment(
            row + banner_lines, col - 5, extra_attr, extra[0], msec);

        /* Line 2: blank line */
        banner_lines += pause_with_text_print_wrapped_segment(
            row + banner_lines, col - 5, extra_attr, "", msec);

        /* Determine how many extra entries */
        int n_extra = 0;
        while (extra[n_extra][0]) n_extra++;

        /* Lines 3+: start splits, last one shifted further right */
        for (int i = 1; i < n_extra; ++i) {
            int shift = col - 5;
            if (i == n_extra - 1) shift += 4;
            banner_lines += pause_with_text_print_wrapped_segment(
                row + banner_lines, shift, extra_attr, extra[i], msec);
        }

        /* separator before stanza */
        banner_lines++;
    }

    /* 2. main stanza */
    while (desc && desc[i_main][0]) {
        main_rows += pause_with_text_print_wrapped_segment(
            row + banner_lines + main_rows, col, TERM_WHITE, desc[i_main], msec);
        ++i_main;
    }

    log_debug("Banner: story font disabled");
    sdl_story_font_disable();

    /* 3. wait for key */
    hide_cursor = true;
    (void)inkey();
    hide_cursor = false;

    /* 4. wipe the area used */
    int total = banner_lines + main_rows;
    int max_row = MIN(row + total, term_hgt);
    for (int j = row; j < max_row; ++j) {
        Term_erase(0, j, 255);
    }

    screen_load();
}

/*
 * Select a suitable unique monster for the Tulkas quest
 */
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

static bool tulkas_has_valid_target(int depth)
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

/*
 * Get metarun quest flag from quest index by looking up the M: field in quest.txt
 * Returns 0 if quest has no metarun tracking or quest_idx is invalid
 */
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

/* Forward declarations for quest text helpers used before their definitions */
static cptr get_quest_title(int quest_idx);
static cptr get_oath_name_from_id(byte oath_id);

/* Prepend a repeat-attempt context line when returning to a Valar quest under an oath */
cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion)
{
    if (!texts || !count || quest_idx <= 0 || quest_idx >= z_info->quest_max) return texts;

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr || q_ptr->oath_id <= 0) return texts;
    if (!p_ptr || p_ptr->oath_type != q_ptr->oath_id || oath_invalid(q_ptr->oath_id)) return texts;

    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int previous = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    if (previous <= 0) return texts; /* First attempt - no alternate text */

    cptr quest_title = get_quest_title(quest_idx);
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
    mem_free_null(texts); /* free only the array container; strings move to new_texts */
    (*count)++;
    return new_texts;
}

/*
 * Get quest title from quest data
 */
static cptr get_quest_title(int quest_idx)
{
    log_trace("QUEST TITLE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST TITLE: Invalid bounds check, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST TITLE: q_ptr is NULL, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    if (q_ptr->title_text && q_text) {
        log_trace("QUEST TITLE: Using title_text");
        return q_text + q_ptr->title_text;
    }
    
    /* Fallback to quest name */
    if (q_ptr->name && quest_name_text) {
        log_trace("QUEST TITLE: Using quest name fallback");
        return quest_name_text + q_ptr->name;
    }
    
    log_trace("QUEST TITLE: No valid text found, returning Unknown Quest");
    return "Unknown Quest";
}

/*
 * Get quest challenge description from quest data
 */
static cptr get_quest_challenge(int quest_idx)
{
    log_trace("QUEST CHALLENGE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST CHALLENGE: Invalid bounds check, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST CHALLENGE: q_ptr is NULL, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    if (q_ptr->challenge_text && q_text) {
        log_trace("QUEST CHALLENGE: Using challenge_text");
        return q_text + q_ptr->challenge_text;
    }
    
    log_trace("QUEST CHALLENGE: No valid text found, returning default");
    return "Face the unknown challenge";
}

/*
 * Get oath name from oath ID using oath_info data
 */
static cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";
    
    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }
    
    /* Fallback to hardcoded names if oath_info not loaded */
    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";  
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

/*
 * Display wrapped text for quest status - simple word wrapping
 */
static void display_wrapped_text(int col, int *row, cptr text, byte color, int max_width)
{
    char line_buf[256];
    int line_pos = 0;
    int effective_width = max_width - col - 4; /* Leave margin for indentation */
    int text_len = strlen(text);
    int word_start = 0;
    int i = 0;
    int loop_count = 0; /* Safety counter for this function call */
    
    if (effective_width < 20) effective_width = 20; /* Minimum width */
    
    line_buf[0] = '\0';
    
    while (i <= text_len) {
        /* Safety check to prevent infinite loop */
        loop_count++;
        if (loop_count > 1000) {
            log_warn("display_wrapped_text: safety break, possible infinite loop (text_len=%d, i=%d)", text_len, i);
            break;
        }
        
        /* End of string or found a space */
        if (i == text_len || text[i] == ' ') {
            /* Extract the current word */
            int word_len = i - word_start;
            char word[128];
            
            if (word_len > 0 && word_len < (int)sizeof(word)) {
                /* Copy the word manually to avoid buffer issues */
                int copy_len = word_len;
                if (copy_len >= (int)sizeof(word)) copy_len = (int)sizeof(word) - 1;
                
                /* Manual copy to avoid strncpy issues */
                int j;
                for (j = 0; j < copy_len; j++) {
                    word[j] = text[word_start + j];
                }
                word[copy_len] = '\0';
                
                /* Check if adding this word would exceed the line width */
                int new_line_len = line_pos + (line_pos > 0 ? 1 : 0) + copy_len;
                
                if (new_line_len > effective_width && line_pos > 0) {
                    /* Output current line and start new line with this word */
                    Term_putstr(col + 2, (*row)++, -1, color, line_buf);
                    
                    /* Check if the word itself is too long for a line */
                    if (copy_len > effective_width) {
                        /* Break the word across multiple lines */
                        int word_pos = 0;
                        while (word_pos < copy_len) {
                            int chunk_len = effective_width;
                            if (word_pos + chunk_len > copy_len) {
                                chunk_len = copy_len - word_pos;
                            }
                            
                            /* Extract chunk of the word */
                            char chunk[256];
                            int k;
                            for (k = 0; k < chunk_len && word_pos + k < copy_len; k++) {
                                chunk[k] = word[word_pos + k];
                            }
                            chunk[k] = '\0';
                            
                            /* Output this chunk */
                            Term_putstr(col + 2, (*row)++, -1, color, chunk);
                            word_pos += chunk_len;
                        }
                        
                        /* Reset line buffer */
                        line_buf[0] = '\0';
                        line_pos = 0;
                    } else {
                        /* Word fits on a new line */
                        SDL_strlcpy(line_buf, word, sizeof(line_buf));
                        line_pos = copy_len;
                    }
                } else {
                    /* Add word to current line */
                    if (line_pos > 0) {
                        SDL_strlcat(line_buf, " ", sizeof(line_buf));
                        line_pos++;
                    }
                    SDL_strlcat(line_buf, word, sizeof(line_buf));
                    line_pos += copy_len;
                }
            }
            
            /* Skip spaces and move to next word */
            while (i < text_len && text[i] == ' ') {
                i++;
            }
            word_start = i;
        } else {
            i++;
        }
    }
    
    /* Output any remaining text in the buffer */
    if (line_pos > 0) {
        Term_putstr(col + 2, (*row)++, -1, color, line_buf);
    }
}

/*
 * Simple string search function - finds needle in haystack
 * Returns pointer to first occurrence, or NULL if not found
 */
static char* my_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    
    int needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char* p = haystack; *p; p++) {
        int i;
        for (i = 0; i < needle_len && p[i] && p[i] == needle[i]; i++);
        if (i == needle_len) {
            return (char*)p;
        }
    }
    return NULL;
}

/*
 * Process placeholders in quest text (challenge, etc.) with actual values
 */
static cptr process_quest_placeholders(cptr text, int quest_idx)
{
    static char processed_buf[256];

    if (!text) {
        return "";
    }

    SDL_strlcpy(processed_buf, text, sizeof(processed_buf));
    
    if (quest_idx == QUEST_ID_TULKAS) {
        /* Replace [monster name] with actual monster name */
        char* monster_pos = my_strstr(processed_buf, "[monster name]");
        if (monster_pos && p_ptr->tulkas_target_r_idx > 0 && p_ptr->tulkas_target_r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
            char before[128], after[128];
            int before_len = monster_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, r_name + r_ptr->name, after);
        }
        
        /* Replace [artifact name] with actual artifact name */
        char* artifact_pos = my_strstr(processed_buf, "[artifact name]");
        if (artifact_pos && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
            char before[128], after[128];
            int before_len = artifact_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
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
                if (k_idx > 0) {
                    object_prep(&temp_obj, k_idx);
                    temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                    temp_obj.ident |= IDENT_KNOWN;
                    
                    /* Get the full artifact description */
                    object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                } else {
                    SDL_strlcpy(artifact_name, a_ptr->name, sizeof(artifact_name));
                }
            } else {
                SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
            }
            
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, artifact_name, after);
        }
    }
    
    return processed_buf;
}

/*
 * Get quest reward description for status display using actual quest data
 */
static cptr get_quest_reward_text(int quest_idx)
{
    static char reward_buf[200];
    char temp_buf[100];
    
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return "Unknown reward";
    
    quest_type* q_ptr = &quest_info[quest_idx];
    reward_buf[0] = '\0';
    
    /* Handle special Tulkas artifact reward */
    if (quest_idx == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
        artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
        if (a_ptr->name[0] != '\0') {
            /* Create a temporary object to get proper description */
            object_type temp_obj;
            object_wipe(&temp_obj);
            
            /* Set up the object as the artifact */
            s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx > 0) {
                object_prep(&temp_obj, k_idx);
                temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                temp_obj.ident |= IDENT_KNOWN;
                
                /* Get the full artifact description */
                object_desc(reward_buf, sizeof(reward_buf), &temp_obj, true, 0);
                return reward_buf;
            } else {
                SDL_strlcpy(reward_buf, a_ptr->name, sizeof(reward_buf));
                return reward_buf;
            }
        }
    }
    
    /* Varda reward description */
    if (quest_idx == QUEST_ID_VARDA) {
        SDL_strlcpy(reward_buf, "Choose one radiant artefact and unlock the Oath of Light (+1 light radius)", sizeof(reward_buf));
        return reward_buf;
    }
    
    /* Build reward description from quest data */
    bool has_rewards = false;
    
    /* Check stat bonuses */
    if (q_ptr->stat_bonuses[0] || q_ptr->stat_bonuses[1] || q_ptr->stat_bonuses[2] || q_ptr->stat_bonuses[3]) {
        has_rewards = true;
        SDL_strlcat(reward_buf, "Stats: ", sizeof(reward_buf));
        
        if (q_ptr->stat_bonuses[0]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Str ", q_ptr->stat_bonuses[0]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[1]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Dex ", q_ptr->stat_bonuses[1]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[2]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Con ", q_ptr->stat_bonuses[2]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[3]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Gra ", q_ptr->stat_bonuses[3]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
    }
    
    /* Check skill bonuses */
    if (q_ptr->skill_type && q_ptr->skill_bonus) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Convert skill type to name */
        cptr skill_name = "Unknown";
        switch (q_ptr->skill_type) {
            case 0: skill_name = "Melee"; break;
            case 1: skill_name = "Archery"; break;
            case 2: skill_name = "Evasion"; break;
            case 3: skill_name = "Stealth"; break;
            case 4: skill_name = "Perception"; break;
            case 5: skill_name = "Will"; break;
            case 6: skill_name = "Smithing"; break;
            case 7: skill_name = "Song"; break;
        }
        strnfmt(temp_buf, sizeof(temp_buf), "+%d %s ", q_ptr->skill_bonus, skill_name);
        SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
    }
    
    /* Check special abilities */
    if (q_ptr->ability_type && q_ptr->ability_id < ABILITIES_MAX) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Get ability name from ability database */
        cptr ability_name = "Special ability";
        if (q_ptr->ability_type == S_SPC) { /* Special abilities type */
            /* Use ability_index to find the ability and get its name */
            int idx = ability_index(S_SPC, q_ptr->ability_id);
            if (idx >= 0 && idx < z_info->b_max) {
                ability_type* b_ptr = &b_info[idx];
                if (b_ptr->name) {
                    ability_name = b_name + b_ptr->name;
                }
            }
        }
        
        SDL_strlcat(reward_buf, ability_name, sizeof(reward_buf));
    }
    
    /* Check oath association */
    if (q_ptr->oath_id) {
        if (has_rewards) SDL_strlcat(reward_buf, " | ", sizeof(reward_buf));
        has_rewards = true;
        SDL_strlcat(reward_buf, get_oath_name_from_id(q_ptr->oath_id), sizeof(reward_buf));
    }
    
    if (!has_rewards) {
        SDL_strlcpy(reward_buf, "Unknown reward", sizeof(reward_buf));
    }
    
    return reward_buf;
}

/*
 * Free quest text array returned by extract functions
 */
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

/*
 * Show quest status for current metarun - only active and completed quests
 * Now uses quest.txt data instead of hardcoded values
 */
void do_cmd_quest_status(void)
{
    char buf[128];
    int row = 1;
    int col = 2;
    bool any_quests = false;
    int wid, hgt;

    log_trace("QUEST STATUS: do_cmd_quest_status() called");

    /* Safety check: ensure we have a valid player and metarun */
    if (!p_ptr) {
        log_trace("QUEST STATUS: No player data available");
        msg_print("No character data available.");
        return;
    }

    log_trace("QUEST STATUS: Player exists, quest states - Tulkas: %d, Aule: %d, Mandos: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);

    /* Get terminal size for wrapping */
    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();
    Term_clear();

    /* Title */
    Term_putstr(col, row++, -1, TERM_YELLOW, "=== Quest Status ===");
    row++;

    /* Check Tulkas quest */
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr tulkas_status;
        byte color;
        
        log_trace("QUEST STATUS: Getting title and challenge for Tulkas quest");
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_TULKAS);
        log_trace("QUEST STATUS: Got title='%s', challenge='%s'", quest_title ? quest_title : "NULL", quest_challenge ? quest_challenge : "NULL");
        
        if (!quest_title) quest_title = "Tulkas Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->tulkas_quest) {
            case TULKAS_QUEST_GIVER_PRESENT:
                log_trace("QUEST STATUS: Tulkas GIVER_PRESENT case");
                tulkas_status = "Available - Tulkas awaits";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                {
                    log_trace("QUEST STATUS: About to call process_quest_placeholders");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    display_wrapped_text(col, &row, processed_challenge, TERM_SLATE, wid);
                }
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (GIVER_PRESENT)");
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case TULKAS_QUEST_ACTIVE:
                log_trace("QUEST STATUS: Tulkas ACTIVE case");
                {
                    /* Use processed challenge text instead of hardcoded status */
                    log_trace("QUEST STATUS: About to call process_quest_placeholders for ACTIVE");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    display_wrapped_text(col, &row, processed_challenge, TERM_WHITE, wid);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                    display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                    break;
                }
            case TULKAS_QUEST_COMPLETE:
                log_trace("QUEST STATUS: Tulkas COMPLETE case");
                tulkas_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                break;
            case TULKAS_QUEST_REWARDED:
                /* For Tulkas quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                tulkas_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (REWARDED)");
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                tulkas_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
        }
        row++;
    }

    /* Check Aule quest */
    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr aule_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_AULE);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->aule_quest) {
            case AULE_QUEST_FORGE_PRESENT:
                aule_status = "Available - Enter the forge";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_ACTIVE:
                /* Use challenge text instead of hardcoded status */
                display_wrapped_text(col, &row, quest_challenge, TERM_WHITE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_SUCCESS:
                aule_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                aule_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                aule_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, aule_status);
        }
        row++;
    }

    /* Check Mandos quest */
    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr mandos_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_MANDOS);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->mandos_quest) {
            case MANDOS_QUEST_GIVER_PRESENT:
                mandos_status = "Available - Enter the tomb";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                Term_putstr(col + 2, row++, -1, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_ACTIVE:
                mandos_status = "Active";
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case MANDOS_QUEST_SUCCESS:
                mandos_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                mandos_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_MANDOS));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                mandos_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
        }
        row++;
    }

    /* Check Niena quest */
    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr niena_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_NIENA);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->niena_quest) {
            case NIENA_QUEST_GIVER_PRESENT:
                niena_status = "Available - Niena offers mercy";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                Term_putstr(col + 2, row++, -1, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_ACTIVE:
                strnfmt(buf, sizeof(buf), "Active: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                niena_status = buf;
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case NIENA_QUEST_SUCCESS:
                niena_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                niena_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_FAILED:
                strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                color = TERM_RED;
                Term_putstr(col + 2, row++, -1, color, buf);
                Term_putstr(col + 2, row++, -1, TERM_SLATE,
                            "You took a life and lost Niena's mercy.");
                break;
            default:
                niena_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
        }
        row++;
    }

    /* Check Orome quest */
    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr orome_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_OROME);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->orome_quest) {
            case OROME_QUEST_GIVER_PRESENT:
                orome_status = "Available - Orome awaits";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case OROME_QUEST_ACTIVE:
                {
                    strnfmt(buf, sizeof(buf), "Active: Hunt the fell kindreds");
                    orome_status = buf;
                    color = TERM_WHITE;
                    Term_putstr(col + 2, row++, -1, color, orome_status);
                    
                    /* Show current kill counts for all monster types */
                    strnfmt(buf, sizeof(buf), "Wolves killed: %d/100", p_ptr->orome_wolves_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Spiders killed: %d/80", p_ptr->orome_spiders_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Serpents killed: %d/60", p_ptr->orome_serpents_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Vampires killed: %d/30", p_ptr->orome_vampires_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN : TERM_SLATE, wid);
                    
                    display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                    display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                }
                break;
            case OROME_QUEST_SUCCESS:
                orome_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case OROME_QUEST_REWARDED:
                /* For Orome quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                orome_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                orome_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, orome_status);
        }
        row++;
    }

    /* Check Varda quest */
    if (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr varda_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_VARDA);
        if (!quest_title) quest_title = "Varda Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";

        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);

        switch (p_ptr->varda_quest) {
            case VARDA_QUEST_GIVER_PRESENT:
                varda_status = "Available - Varda waits in sunlight";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_ACTIVE:
                varda_status = "Active - Seek Duruin's bastion";
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_SUCCESS:
                varda_status = "Complete - Claim Varda's blessing";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_REWARDED:
                varda_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                varda_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
        }
        row++;
    }

    /* Show previous metarun completions */
    bool has_previous_completions = false;
    int tulkas_completed = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    if (tulkas_completed > 0 && p_ptr->tulkas_quest != TULKAS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr oath_name = get_oath_name_from_id(quest_info[1].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, tulkas_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int aule_completed = metarun_quest_completion_count(METARUN_QUEST_AULE);
    if (aule_completed > 0 && p_ptr->aule_quest != AULE_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr oath_name = get_oath_name_from_id(quest_info[2].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, aule_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int mandos_completed = metarun_quest_completion_count(METARUN_QUEST_MANDOS);
    if (mandos_completed > 0 && p_ptr->mandos_quest != MANDOS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr oath_name = get_oath_name_from_id(quest_info[3].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, mandos_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int niena_completed = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    if (niena_completed > 0 && p_ptr->niena_quest != NIENA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr oath_name = get_oath_name_from_id(quest_info[4].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, niena_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int orome_completed = metarun_quest_completion_count(METARUN_QUEST_OROME);
    if (orome_completed > 0 && p_ptr->orome_quest != OROME_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr oath_name = get_oath_name_from_id(quest_info[5].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, orome_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int varda_completed = metarun_quest_completion_count(METARUN_QUEST_VARDA);
    if (varda_completed > 0 && p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr oath_name = get_oath_name_from_id(quest_info[6].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, varda_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    
    if (has_previous_completions) {
        row++;
    }

    /* If no quests are active or completed */
    if (!any_quests) {
        Term_putstr(col, row++, -1, TERM_SLATE, "No active or completed quests this run.");
        row++;
        Term_putstr(col, row++, -1, TERM_L_DARK, "Quest vaults may appear as you delve deeper...");
    }

    row++;
    Term_putstr(col, row, -1, TERM_L_WHITE, "Press any key to return.");
    inkey();
    
    screen_load();
}

/*
 * Quest typewriter menu function - displays quest dialog with typewriter effect
 * Based on print_story_intro() style
 */
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color)
{
    int wid, h;
    const int indent = 2;
    bool skipped = false;
    
    /* Get terminal size */
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent * 2;
    
    /* Save screen and start fresh */
    screen_save();
    Term_clear();
    
    /* Display title */
    int title_y = 1;
    Term_putstr((wid - strlen(title)) / 2, title_y, -1, title_color, title);
    
    int row = 3, col = 0;
    
    for (int idx = 0; idx < total_texts; idx++) {
        const char *s = texts[idx];
        
        /* Handle empty lines as paragraph breaks */
        if (!s || strlen(s) == 0) {
            /* Empty line - just advance row for paragraph break */
            row++;
            col = 0;
            /* Short pause for empty line */
            Term_xtra(TERM_XTRA_DELAY, 200);
            continue;
        }
        
        /* Count lines needed for this paragraph */
        int lines_needed = 0;
        int temp_col = col;
        for (int i = 0; s[i]; i++) {
            if (s[i] == '\n' || temp_col >= wrap_width) {
                lines_needed++;
                temp_col = 0;
                if (s[i] == '\n') continue;
            }
            temp_col++;
        }
        lines_needed++; /* Add one for the text itself */
        
        /* Check if we have enough space for the whole paragraph */
        if (row + lines_needed >= h - 2) {
            Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to continue)");
            {
                char k = inkey();
                if (k == 'Q' || k == 'q') { /* Q/q skips remaining dialog */
                    Term_clear();
                    screen_load();
                    return;
                }
            }
            Term_clear();
            /* Redisplay title */
            Term_putstr((wid - strlen(title)) / 2, title_y, -1, title_color, title);
            row = 3;
        }
        
        col = 0;
        
        /* Print this string with proper word wrapping and typewriter effect */
        int i = 0;
        while (s[i]) {
            /* Handle explicit newlines */
            if (s[i] == '\n') {
                row++;
                col = 0;
                i++;
                continue;
            }
            
            /* Find the end of the current word (or until we hit wrap width) */
            int word_start = i;
            int word_len = 0;
            bool has_space_after = false;
            
            /* Build the current word/phrase until we hit whitespace, newline, or exceed reasonable length */
            while (s[i] && s[i] != '\n' && word_len < wrap_width) {
                if (s[i] == ' ' || s[i] == '\t') {
                    has_space_after = true;
                    break;
                }
                word_len++;
                i++;
            }
            
            log_trace("WRAP DEBUG: word='%.*s', word_len=%d, col=%d, wrap_width=%d", word_len, &s[word_start], word_len, col, wrap_width);
            
            /* Check if this word fits on the current line */
            if (col + word_len > wrap_width && col > 0) {
                /* Word doesn't fit, wrap to next line */
                log_trace("WRAP DEBUG: Wrapping word to next line (col=%d + word_len=%d > wrap_width=%d)", col, word_len, wrap_width);
                row++;
                col = 0;
            }
            
            /* Print the word character by character with typewriter effect */
            if (skipped) {
                /* Skip mode: print entire word instantly */
                for (int j = word_start; j < word_start + word_len; j++) {
                    Term_putch(indent + col, row, text_color, s[j]);
                    col++;
                }
            }
            else {
                /* Normal mode: typewriter effect with character-by-character */
                for (int j = word_start; j < word_start + word_len; j++) {
                    /* Check for ESC or Enter key press to skip typewriter effect */
                    char check_key;
                    if (Term_inkey(&check_key, false, false) == 0) {
                        /* Only respond to ESC or Enter - consume and check */
                        Term_inkey(&check_key, false, true);
                        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r') {
                            skipped = true;
                            /* Print rest of current word instantly */
                            for (int k = j; k < word_start + word_len; k++) {
                                Term_putch(indent + col, row, text_color, s[k]);
                                col++;
                            }
                            break; /* Exit to continue with rest of text in skip mode */
                        }
                        /* Other keys are ignored (already consumed) */
                    }
                    
                    /* Print character with typewriter effect */
                    Term_putch(indent + col, row, text_color, s[j]);
                    Term_fresh();
                    col++;
                    
                    /* Delay 25 ms after each character for typewriter effect */
                    Term_xtra(TERM_XTRA_DELAY, 25);
                }
            }
            
            /* Handle the space/whitespace after the word */
            if (has_space_after) {
                if (s[i] == ' ') {
                    /* Only print space if we're not at the end of a line */
                    if (col < wrap_width) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for space too (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the space */
                }
                else if (s[i] == '\t') {
                    /* Handle tab - convert to spaces but respect wrap width */
                    int tab_spaces = 4 - (col % 4);
                    for (int t = 0; t < tab_spaces && col < wrap_width; t++) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for tab spaces (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the tab */
                }
            }
        }
        
        /* Move to next line after text */
        row++;
        col = 0;
        
        /* 400ms pause after each line of text (unless skipped) */
        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 400);
    }
    
    /* Refresh screen to show all text if skipped */
    if (skipped) Term_fresh();
    
    /* Final prompt */
    Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to continue)");
    inkey();
    
    /* Flush any queued keypresses that accumulated during the typewriter effect */
    Term_flush();
    
    Term_clear();
    screen_load();
}

/*
 * Remove quest giver monster by R_IDX without messaging
 */
static void remove_quest_giver_silent(int quest_giver_r_idx)
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

static const int tulkas_orc_targets[] = { 54, 76, 84, 85, 95, 105 };
static const size_t tulkas_orc_target_count = N_ELEMENTS(tulkas_orc_targets);

static const int orome_great_hunt_targets[] = {
    R_IDX_SCATHA,
    R_IDX_SMAUG,
    R_IDX_DRAUGLUIN,
    R_IDX_GOSTIR,
    R_IDX_SHELOB,
    R_IDX_THURINGWETHIL
};
static const size_t orome_great_hunt_target_count = N_ELEMENTS(orome_great_hunt_targets);

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

void ensure_varda_ungoliant_active(void)
{
    byte state = quest_get_state(QUEST_ID_VARDA_UNGOLIANT);
    if (state >= QUEST_STATE_REWARDED) return;

    bool oath_active = (p_ptr->oath_type == OATH_LIGHT && !oath_invalid(OATH_LIGHT));
    bool unlocked = metarun_quest_completion_count(METARUN_QUEST_VARDA_SHADOW) > 0 ||
        quest_get_state(QUEST_ID_VARDA_SHADOW) >= QUEST_STATE_REWARDED ||
        metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0 ||
        p_ptr->varda_quest >= VARDA_QUEST_REWARDED;

    if (!oath_active || !unlocked)
    {
        if (state != QUEST_STATE_NOT_STARTED) quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_NOT_STARTED);
        return;
    }

    if (state == QUEST_STATE_NOT_STARTED)
    {
        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_ACTIVE);
    }

    if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) == QUEST_STATE_ACTIVE &&
        r_info[R_IDX_UNGOLIANT].max_num == 0)
    {
        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_REWARDED);
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

    /* Shuffle candidates */
    for (int i = candidate_count - 1; i > 0; i--) {
        int swap_idx = rand_int(i + 1);
        int tmp = candidates[i];
        candidates[i] = candidates[swap_idx];
        candidates[swap_idx] = tmp;
    }

    int final_count = MIN(max_choices, candidate_count);
    for (int i = 0; i < final_count; i++) {
        choices[i] = candidates[i];
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

/*
 * Display Varda's reward selection in a scrollable menu integrated with quest completion text.
 * Returns the selected artefact index, or 0 if cancelled.
 */
static int prompt_varda_reward_choice_menu(const int* choices, int choice_count, cptr* completion_texts, int text_count)
{
    int wid, hgt;
    Term_get_size(&wid, &hgt);
    
    int selection = 0;
    bool done = false;
    int selected_artifact = 0;
    
    /* Save screen once */
    screen_save();

    while (!done) {
        /* Clear screen */
        Term_clear();
        
        /* Display title */
        int row = 1;
        cptr title = "Starlight Triumph";
        Term_putstr((wid - strlen(title)) / 2, row, -1, TERM_L_GREEN, title);
        row += 2;
        
        /* Display completion text */
        for (int i = 0; i < text_count && row < hgt - 10; i++) {
            if (completion_texts[i] && completion_texts[i][0] != '\0') {
                display_wrapped_text(2, &row, completion_texts[i], TERM_WHITE, wid);
            } else {
                row++; /* Empty line for paragraph break */
            }
        }
        
        row++;
        Term_putstr(2, row++, -1, TERM_L_BLUE, "Choose your radiant gift:");
        row++;
        
        /* Display reward choices with highlighting */
        char desc[120];
        for (int i = 0; i < choice_count && row < hgt - 3; i++) {
            describe_varda_choice(choices[i], desc, sizeof(desc));
            
            byte attr = (i == selection) ? TERM_YELLOW : TERM_L_WHITE;
            char marker = (i == selection) ? '>' : ' ';
            
            char line_buf[140];
            strnfmt(line_buf, sizeof(line_buf), "%c %c) %s", marker, 'a' + i, desc);
            Term_putstr(2, row++, -1, attr, line_buf);
        }
        
        /* Display controls */
        row = hgt - 2;
        Term_putstr(2, row, -1, TERM_L_DARK, "Arrows navigate   'x' Inspect   Space/Enter accept   Letter select");
        
        /* Position cursor at selection */
        Term_gotoxy(2, 6 + text_count + 2 + selection);
        Term_fresh();
        
        /* Get input */
        char key = inkey();
        
        /* Handle input */
        if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            /* Accept current selection */
            selected_artifact = choices[selection];
            done = true;
        } else if (key == 'x' || key == 'X' || key == '?') {
            /* Inspect selection */
            Term_clear();
            desc_art_fake(choices[selection]);
        } else if (key == '8' || key == 'k' || key == '-') {
            /* Move up */
            selection = (selection + choice_count - 1) % choice_count;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Move down */
            selection = (selection + 1) % choice_count;
        } else if (key >= 'a' && key < 'a' + choice_count) {
            /* Letter selection */
            selected_artifact = choices[key - 'a'];
            done = true;
        } else if (key >= 'A' && key < 'A' + choice_count) {
            /* Capital letter selection */
            selected_artifact = choices[key - 'A'];
            done = true;
        }
    }
    
    screen_load();
    return selected_artifact;
}

static bool grant_varda_reward(int quest_id, cptr* completion_texts, int completion_count)
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
    p_ptr->quest_reserved[0] = 1;
    if (quest_id == QUEST_ID_VARDA_SHADOW) {
        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_REWARDED);
        p_ptr->varda_shadow_ready = 0;
        p_ptr->varda_shadow_placed = 1;
        p_ptr->varda_shadow_restricted = 0;
        do_cmd_note("Varda blessed me with a radiant artefact after Belegwath's fall.", p_ptr->depth);
    } else {
        p_ptr->varda_quest = VARDA_QUEST_REWARDED;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_vault_placed = 1;
        do_cmd_note("Varda blessed me with a radiant artefact and the Oath of Light.", p_ptr->depth);
    }

    grant_followup_quest_rewards(quest_id);

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
    /* Avoid double-spawning */
    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            log_trace("Varda reward: quest giver already exists on level");
            return;
        }
    }

    for (int y = p_ptr->py - 2; y <= p_ptr->py + 2; y++) {
        for (int x = p_ptr->px - 2; x <= p_ptr->px + 2; x++) {
            if (!in_bounds(y, x)) continue;
            if (y == p_ptr->py && x == p_ptr->px) continue;
            if (distance(p_ptr->py, p_ptr->px, y, x) < 2) continue;
            if (!cave_floor_bold(y, x)) continue;
            if (cave_m_idx[y][x] != 0) continue;

            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                varda_make_light_pool(y, x);
                p_ptr->varda_level = p_ptr->depth;
                log_trace("Varda reward: placed quest giver at (%d,%d)", y, x);
                return;
            }
        }
    }

    /* Mark this depth as attempted even if placement failed, to avoid infinite spawn loops */
    p_ptr->varda_level = p_ptr->depth;
    log_trace("Varda reward: failed to place quest giver near player (no valid space), will retry on new depth");
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

    if (quest_get_state(QUEST_ID_VARDA_SHADOW) == QUEST_STATE_ACTIVE && r_idx == R_IDX_BELEGWATH) {
        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_SUCCESS);
        p_ptr->varda_shadow_ready = 0;
        p_ptr->varda_shadow_restricted = 0;
        p_ptr->varda_level = p_ptr->depth;
        msg_print("Belegwath falls. The Shadow Bastion breaks beneath returning starlight!");
        try_place_varda_near_player();
    }

    if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) == QUEST_STATE_ACTIVE && r_idx == R_IDX_UNGOLIANT) {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA_UNGOLIANT, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA_UNGOLIANT, completion_texts, &completion_count, true);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Varda, Gloomweaver's Doom", completion_texts, completion_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Ungoliant's unlight gutters and dies before Varda's stars.",
                "The Queen of the Stars marks the doom complete."
            };
            quest_typewriter_menu("Varda, Gloomweaver's Doom", fallback, N_ELEMENTS(fallback), TERM_WHITE, TERM_L_BLUE);
        }

        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_REWARDED);
        grant_followup_quest_rewards(QUEST_ID_VARDA_UNGOLIANT);
    }
}

void varda_quest_interaction(void)
{
    static s32b last_interaction_turn = -1;
    byte shadow_state;
    if (last_interaction_turn == turn) return;
    last_interaction_turn = turn;

    shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);

    if (shadow_state == QUEST_STATE_GIVER_PRESENT) {
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_VARDA_SHADOW, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_VARDA_SHADOW, init_texts, &text_count, false);

        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_ACTIVE);
        p_ptr->quest_reserved[0] = 1;
        p_ptr->varda_shadow_restricted = 1;
        remove_quest_giver(R_IDX_VARDA);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Varda, Shadow's Bastion", init_texts, text_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Varda's gaze is clear even in the deep dark.",
                "\"Go down and break Belegwath's Shadow Bastion beneath the stars.\""
            };
            quest_typewriter_menu("Varda, Shadow's Bastion", fallback, N_ELEMENTS(fallback), TERM_WHITE, TERM_L_BLUE);
        }

        do_cmd_note("Varda sent me to destroy Belegwath in his Shadow Bastion.", p_ptr->depth);
        return;
    }

    if (shadow_state == QUEST_STATE_ACTIVE) {
        msg_print("Varda's whisper: \"Belegwath's bastion waits beyond the fifteenth delving.\"");
        return;
    }

    if (shadow_state == QUEST_STATE_SUCCESS) {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA_SHADOW, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA_SHADOW, completion_texts, &completion_count, true);
        cptr fallback[] = {
            "Varda's voice rings clear through the shattered shadow:",
            "\"Choose your radiant gift anew, and carry the stars deeper still.\""
        };
        cptr* texts_to_use = (completion_texts && completion_count > 0) ? completion_texts : fallback;
        int text_count = (completion_texts && completion_count > 0) ? completion_count : 2;
        bool rewarded = grant_varda_reward(QUEST_ID_VARDA_SHADOW, texts_to_use, text_count);

        if (completion_texts) {
            free_quest_texts(completion_texts, completion_count);
        }
        if (rewarded) {
            remove_quest_giver(R_IDX_VARDA);
        }
        return;
    }

    if (shadow_state == QUEST_STATE_REWARDED) {
        msg_print("Varda's deeper blessing still shines in your wake.");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT) {
        log_trace("Varda quest: accepting quest");
        p_ptr->varda_quest = VARDA_QUEST_ACTIVE;
        p_ptr->quest_reserved[0] = 1;
        p_ptr->varda_level = p_ptr->depth;

        /* Remove quest giver for roulette quests */
        remove_quest_giver(R_IDX_VARDA);

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
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
        msg_print("Varda's whisper: \"Find Duruin's bastion beyond five hundred feet.\"");
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
        bool rewarded = grant_varda_reward(QUEST_ID_VARDA, texts_to_use, text_count);
        
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
    int i, y, x;
    byte shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);

    if ((p_ptr->varda_quest < VARDA_QUEST_GIVER_PRESENT || p_ptr->varda_quest > VARDA_QUEST_SUCCESS) &&
        shadow_state != QUEST_STATE_GIVER_PRESENT &&
        shadow_state != QUEST_STATE_ACTIVE &&
        shadow_state != QUEST_STATE_SUCCESS) return;

    for (i = 1; i < 9; i++) {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];

        if (!in_bounds(y, x)) continue;
        if (cave_m_idx[y][x] <= 0) continue;

        int m_idx = cave_m_idx[y][x];
        if (m_idx >= mon_max) continue;

        monster_type* m_ptr = &mon_list[m_idx];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            varda_quest_interaction();
            return;
        }
    }

    /* If Varda should be present for a reward but isn't adjacent, try to place her */
    /* Only attempt placement once per depth to avoid infinite spawn loops */
    if ((p_ptr->varda_quest == VARDA_QUEST_SUCCESS || shadow_state == QUEST_STATE_SUCCESS) &&
        p_ptr->varda_level != p_ptr->depth) {
        log_trace("Varda quest: success state without nearby quest giver - attempting to place Varda");
        try_place_varda_near_player();
    }
}

/*
 * Count hostile monsters in Mandos vault area using proper vault boundaries
 */
/*
 * Check if Brodda (formerly Aldor) has been killed for Mandos quest
 */
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

/*
 * Handle quest completion checking for Orome hunting quest
 */
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
            msg_print("Orome the Huntsman will be pleased with your mastery.");
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
                quest_typewriter_menu("Orome, Hunt of the Great", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                cptr fallback[] = {
                    "The last of Orome's marked prey falls at last.",
                    "The Valaroma rings once more, and the great hunt is ended."
                };
                quest_typewriter_menu("Orome, Hunt of the Great", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
            }

            quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_REWARDED);
            metarun_set_orome_great_hunt_active(false);
            grant_followup_quest_rewards(QUEST_ID_OROME_GREAT_HUNT);
        }
    }
}

/*
 * Handle interaction with Niena for the mercy quest
 */
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
            quest_typewriter_menu("Orome, Warden of the Drakes", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Orome's horn-call echoes like fire through the deep halls.",
                "'Slay ten mighty dragons and return to me, hunter.'"
            };
            quest_typewriter_menu("Orome, Warden of the Drakes", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
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
            quest_typewriter_menu("Orome, Warden of the Drakes", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Orome appears amid the fading terror of the drakes.",
                "'The mightiest worms have learned to fear your name.'"
            };
            quest_typewriter_menu("Orome, Warden of the Drakes", fallback, N_ELEMENTS(fallback), TERM_GREEN, TERM_WHITE);
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
            quest_typewriter_menu("Orome the Huntsman", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Orome the Huntsman regards you with keen eyes:",
                "'Prove your skill as a hunter. The dark creatures multiply.'"
            };
            quest_typewriter_menu("Orome the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
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
        msg_print("Orome fades into the wild, but his presence lingers in your soul.");
        
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
            quest_typewriter_menu("Orome the Huntsman", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Orome appears with a proud smile!",
                "'You have proven yourself a true hunter of the wild.'"
            };
            quest_typewriter_menu("Orome the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
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
                        msg_print("Orome the Huntsman materializes nearby, ready to honor your success!");
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

