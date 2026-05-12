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

/* File: cmd-ui-abilities-details.c */

/*
 * Ability description and prerequisite details for the semantic ability UI.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "cmd-ui-abilities-scenes.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"

int ability_purchase_exp_cost(int skilltype)
{
    int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
    int unit_cost = 500 - 200 * is_free;
    int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

    exp_cost -= unit_cost * affinity_level(skilltype);

    if (skilltype == S_SNG)
        exp_cost -= unit_cost * minstrel_level();

    exp_cost += 100 * curse_flag_delta_cur(CUR_ABILITY_COST);

    if (exp_cost < 0)
        exp_cost = 0;

    return exp_cost;
}

static int ability_knowledge_base_cost(int skilltype, int abilitynum)
{
    ability_type* b_ptr;

    if (skilltype < 0 || skilltype >= S_MAX)
        return 0;
    if (abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return 0;

    b_ptr = &b_info[ability_index(skilltype, abilitynum)];
    if (!b_ptr->name || b_ptr->skilltype != skilltype
        || b_ptr->abilitynum != abilitynum)
    {
        return 0;
    }

    return b_ptr->knowledge_cost;
}

bool ability_uses_knowledge_points(int skilltype, int abilitynum)
{
    return ability_knowledge_base_cost(skilltype, abilitynum) > 0;
}

int ability_purchase_knowledge_cost(int skilltype, int abilitynum)
{
    int cost = ability_knowledge_base_cost(skilltype, abilitynum);

    if (cost <= 0)
        return 0;

    cost -= p_ptr->lore;
    if (cost < 0)
        cost = 0;

    return cost;
}

int ability_semantic_oath_id_for_ability(int abilitynum)
{
    switch (abilitynum)
    {
    case SPC_OATH_MERCY:
        return OATH_MERCY;
    case SPC_OATH_SILENCE:
        return OATH_SILENCE;
    case SPC_OATH_IRON:
        return OATH_IRON;
    case SPC_OATH_SMITH:
        return OATH_SMITH;
    case SPC_OATH_VALOROUS:
        return OATH_VALOROUS;
    case SPC_OATH_LIGHT:
        return OATH_LIGHT;
    default:
        return 0;
    }
}

static cptr ability_semantic_stat_name(int stat, bool short_name)
{
    static cptr stat_names[A_MAX] = {
        "Strength", "Dexterity", "Constitution", "Grace"
    };
    static cptr stat_short_names[A_MAX] = { "Str", "Dex", "Con", "Gra" };

    if (stat < 0 || stat >= A_MAX)
        return "";

    return short_name ? stat_short_names[stat] : stat_names[stat];
}

static int ability_requirement_stat_value(int stat)
{
    if (stat < 0 || stat >= A_MAX)
        return 0;

    return p_ptr->stat_base[stat] + p_ptr->stat_drain[stat]
        + p_ptr->stat_equip_mod[stat];
}

static int ability_requirement_skill_value(int skill)
{
    if (skill < 0 || skill >= S_MAX)
        return 0;

    return p_ptr->skill_base[skill] + p_ptr->skill_equip_mod[skill];
}

static void ability_menu_format_amount_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int need, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s %d / %d", short_label, need, have);
    else
        strnfmt(buf, buflen, "%d %s (you have %d)", need, long_label, have);
}

static void ability_menu_format_less_than_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int limit, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s <%d / %d", short_label, limit, have);
    else
        strnfmt(buf, buflen, "%s below %d (you have %d)", long_label, limit,
            have);
}

bool ability_semantic_add_wrapped_detail_lines(app_ui_panel* panel,
    byte attr, cptr text, int wrap_chars)
{
    char line_buffer[APP_UI_TEXT_MAX];
    int line_pos = 0;
    const char* text_ptr = text;

    if (!panel || !text || !text[0])
        return true;
    if (wrap_chars < 8)
        wrap_chars = 8;

    while (*text_ptr)
    {
        while (*text_ptr == ' ' && line_pos == 0)
            text_ptr++;

        if (*text_ptr == '\n')
        {
            line_buffer[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                    return false;
            }
            else if (!app_ui_panel_add_detail_line(panel, attr, " "))
            {
                return false;
            }

            line_pos = 0;
            text_ptr++;
            continue;
        }

        if (line_pos >= wrap_chars)
        {
            int wrap_pos = line_pos - 1;

            while (wrap_pos > 0 && line_buffer[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;

                line_buffer[wrap_pos] = '\0';
                if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                    return false;

                memmove(line_buffer, line_buffer + wrap_pos + 1,
                    (size_t)remaining);
                line_pos = remaining;
            }
            else
            {
                line_buffer[line_pos] = '\0';
                if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                    return false;
                line_pos = 0;
            }

            continue;
        }

        line_buffer[line_pos++] = *text_ptr++;
    }

    if (line_pos > 0)
    {
        line_buffer[line_pos] = '\0';
        if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
            return false;
    }

    return true;
}

bool ability_semantic_add_detail_break(app_ui_panel* panel)
{
    if (!panel)
        return false;
    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return false;
    return app_ui_panel_add_detail_line(panel, TERM_SLATE, " ");
}

static void ability_semantic_add_prerequisite_lines(app_ui_panel* panel,
    int skilltype, const ability_type* b_ptr)
{
    char buf[APP_UI_TEXT_MAX];
    int j;
    bool have_requirement = false;

    if (!panel || !b_ptr)
        return;

    (void)app_ui_panel_add_detail_line(panel, TERM_YELLOW, "Prerequisites:");

    if (b_ptr->lore_req > 0)
    {
        have_requirement = true;
        ability_menu_format_amount_line(buf, sizeof(buf), "Lore", "Lore",
            b_ptr->lore_req, p_ptr->lore, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (b_ptr->lore_req <= p_ptr->lore) ? TERM_L_GREEN : TERM_L_DARK,
            buf);
    }

    if (b_ptr->lore_req_lt > 0)
    {
        have_requirement = true;
        ability_menu_format_less_than_line(buf, sizeof(buf), "Lore", "Lore",
            b_ptr->lore_req_lt, p_ptr->lore, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (p_ptr->lore < b_ptr->lore_req_lt) ? TERM_L_GREEN : TERM_L_DARK,
            buf);
    }

    for (j = 0; j < A_MAX; j++)
    {
        int need = b_ptr->stat_req[j];
        int have;

        if (need <= 0)
            continue;

        have = ability_requirement_stat_value(j);
        have_requirement = true;
        ability_menu_format_amount_line(buf, sizeof(buf),
            ability_semantic_stat_name(j, false),
            ability_semantic_stat_name(j, true), need, have, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (need <= have) ? TERM_L_GREEN : TERM_L_DARK, buf);
    }

    for (j = 0; j < A_MAX; j++)
    {
        int limit = b_ptr->stat_req_lt[j];
        int have;

        if (limit <= 0)
            continue;

        have = ability_requirement_stat_value(j);
        have_requirement = true;
        ability_menu_format_less_than_line(buf, sizeof(buf),
            ability_semantic_stat_name(j, false),
            ability_semantic_stat_name(j, true), limit, have, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (have < limit) ? TERM_L_GREEN : TERM_L_DARK, buf);
    }

    for (j = 0; j < S_MAX; j++)
    {
        int need = b_ptr->skill_req[j];
        int have;

        if (need <= 0)
            continue;

        have = ability_requirement_skill_value(j);
        have_requirement = true;
        ability_menu_format_amount_line(buf, sizeof(buf),
            skill_names_full[j], skill_names[j], need, have, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (need <= have) ? TERM_L_GREEN : TERM_L_DARK, buf);
    }

    for (j = 0; j < S_MAX; j++)
    {
        int limit = b_ptr->skill_req_lt[j];
        int have;

        if (limit <= 0)
            continue;

        have = ability_requirement_skill_value(j);
        have_requirement = true;
        ability_menu_format_less_than_line(buf, sizeof(buf),
            skill_names_full[j], skill_names[j], limit, have, 80);
        (void)app_ui_panel_add_detail_line(panel,
            (have < limit) ? TERM_L_GREEN : TERM_L_DARK, buf);
    }

    if (!have_requirement)
    {
        ability_menu_format_amount_line(buf, sizeof(buf), "skill points",
            "Skill", ability_requirement_level(b_ptr),
            p_ptr->skill_base[skilltype], 80);
        (void)app_ui_panel_add_detail_line(panel,
            (ability_requirement_level(b_ptr) <= p_ptr->skill_base[skilltype])
                ? TERM_L_GREEN
                : TERM_L_DARK,
            buf);
    }

    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
    {
        for (j = 0; j < b_ptr->prereqs; j++)
        {
            strnfmt(buf, sizeof(buf), "%s%s", (j == 0) ? "" : "or ",
                b_name
                    + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                           b_ptr->prereq_abilitynum[j])])
                          ->name);
            (void)app_ui_panel_add_detail_line(panel,
                p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                     [b_ptr->prereq_abilitynum[j]]
                    ? TERM_L_GREEN
                    : TERM_L_DARK,
                buf);
        }
    }
    else if (b_ptr->prereqs > 0)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_GREEN, "Quick Study");
    }

    if (skilltype != S_SPC && prereqs(skilltype, b_ptr->abilitynum))
    {
        int exp_cost = ability_purchase_exp_cost(skilltype);
        int knowledge_cost =
            ability_purchase_knowledge_cost(skilltype, b_ptr->abilitynum);
        char amount[APP_UI_TEXT_MAX];

        if (ability_uses_knowledge_points(skilltype, b_ptr->abilitynum))
        {
            ability_menu_format_amount_line(amount, sizeof(amount),
                "knowledge", "KP", knowledge_cost, p_ptr->knowledge_points,
                80);
        }
        else
        {
            ability_menu_format_amount_line(amount, sizeof(amount),
                "experience", "Exp", exp_cost, p_ptr->new_exp, 80);
        }
        strnfmt(buf, sizeof(buf), "Current price: %s", amount);
        (void)app_ui_panel_add_detail_line(panel,
            ((ability_uses_knowledge_points(skilltype, b_ptr->abilitynum)
                 ? knowledge_cost <= p_ptr->knowledge_points
                 : exp_cost <= p_ptr->new_exp))
                ? TERM_L_GREEN
                : TERM_L_DARK,
            buf);
    }
}

static void ability_semantic_add_niena_mercy_lines(app_ui_panel* panel)
{
    int i;
    int total_seen = 0;
    int total_killed = 0;
    char buf[APP_UI_TEXT_MAX];

    if (!panel)
        return;

    for (i = 1; i < z_info->r_max; i++)
    {
        monster_lore* l_ptr = &l_list[i];
        monster_race* r_ptr = &r_info[i];

        if (r_ptr->flags1 & RF1_UNIQUE)
            continue;

        total_seen += l_ptr->psights;
        total_killed += l_ptr->pkills;
    }

    if (total_seen > 0)
    {
        int mercy_ratio_times_10 = 10 * (total_seen - total_killed);
        int stealth_bonus =
            (mercy_ratio_times_10 + total_seen - 1) / total_seen;

        strnfmt(buf, sizeof(buf),
            "Current bonus: +%d stealth (%d seen, %d spared)",
            stealth_bonus, total_seen, total_seen - total_killed);
        (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, buf);
    }
    else
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Current bonus: +0 stealth (no monsters encountered yet)");
    }
}

static void ability_semantic_add_bane_status_lines(app_ui_panel* panel)
{
    int killed;
    int current_bonus;
    int next_threshold;
    int threshold;
    char buf[APP_UI_TEXT_MAX];

    if (!panel || p_ptr->bane_type <= 0)
        return;

    killed = bane_type_killed(p_ptr->bane_type);
    current_bonus = bane_bonus_for_type(p_ptr->bane_type);
    threshold = 2;

    while (threshold <= killed)
        threshold *= 2;
    next_threshold = threshold;

    strnfmt(buf, sizeof(buf), "%s-Bane:", bane_name[p_ptr->bane_type]);
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%d slain, giving a %+d bonus", killed,
        current_bonus);
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);

    if (current_bonus == 0 && killed < 2)
    {
        strnfmt(buf, sizeof(buf), "(next bonus at %d slain)", next_threshold);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
    else if (next_threshold <= 64)
    {
        strnfmt(buf, sizeof(buf), "(next bonus at %d slain)", next_threshold);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
}

static void ability_semantic_add_unique_bane_lines(app_ui_panel* panel)
{
    int uniques_killed;
    int current_bonus = 0;
    int next_threshold;
    int threshold = 2;
    char buf[APP_UI_TEXT_MAX];

    if (!panel)
        return;

    uniques_killed = unique_bane_type_killed();
    while (threshold <= uniques_killed)
    {
        threshold *= 2;
        current_bonus++;
    }

    next_threshold = (current_bonus == 0) ? 2 : threshold;

    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, "Unique Bane:");
    strnfmt(buf, sizeof(buf), "%d uniques slain, giving a %+d bonus",
        uniques_killed, current_bonus);
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);

    if (current_bonus == 0 && uniques_killed < 2)
    {
        strnfmt(buf, sizeof(buf), "(next bonus at %d uniques)",
            next_threshold);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
    else if (next_threshold <= 64)
    {
        strnfmt(buf, sizeof(buf), "(next bonus at %d uniques)",
            next_threshold);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
}

static void ability_semantic_add_oath_status_lines(app_ui_panel* panel)
{
    char buf[APP_UI_TEXT_MAX];

    if (!panel || p_ptr->oath_type <= 0)
        return;

    strnfmt(buf, sizeof(buf), "Oath: %s", oath_name_short(p_ptr->oath_type));
    (void)app_ui_panel_add_detail_line(panel, TERM_L_BLUE, buf);
    strnfmt(buf, sizeof(buf), "You have sworn not to %s.",
        oath_desc2_short(p_ptr->oath_type));
    (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_WHITE, buf, 58);

    if (oath_invalid(p_ptr->oath_type))
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_RED,
            "You are an oathbreaker.");
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Bonus: %s.",
            oath_reward_short(p_ptr->oath_type));
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
    }
}

static int ability_semantic_stepped_song_bonus(int skill, int first_threshold,
    int next_gap)
{
    int bonus = 1;
    int threshold = first_threshold;
    int gap = next_gap;

    if (skill < 0)
        skill = 0;

    while (skill > threshold)
    {
        bonus++;
        threshold += gap;
        gap++;
    }

    return bonus;
}

static int ability_semantic_current_song_score(void)
{
    return MAX(0, p_ptr->skill_use[S_SNG]);
}

static int ability_semantic_minor_song_score(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    if (c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER)
        return song_skill;

    return song_skill / 2;
}

static int ability_semantic_song_synergy_bonus(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    return (song_skill + 5) / 10;
}

static void ability_semantic_add_song_bonus_lines(app_ui_panel* panel,
    const ability_type* b_ptr)
{
    int song_skill;
    char buf[APP_UI_TEXT_MAX];

    if (!panel || !b_ptr)
        return;

    song_skill = ability_semantic_current_song_score();
    buf[0] = '\0';

    switch (b_ptr->abilitynum)
    {
    case SNG_ELBERETH:
    {
        int will_penalty = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: enemy Will -%d.", song_skill,
            will_penalty);
        break;
    }
    case SNG_CHALLENGE:
    {
        int debuff = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: enemy Will and Stealth -%d.",
            song_skill, debuff);
        break;
    }
    case SNG_DELVINGS:
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: delving range %d squares.",
            song_skill, song_skill + 8);
        break;
    case SNG_FREEDOM:
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: freedom checks use Song %d and grant +1 free action while singing.",
            song_skill, song_skill);
        break;
    case SNG_SILENCE:
    {
        int silence_bonus = song_skill / 2;
        int enemy_song_penalty = silence_bonus / 2;
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: +%d to hush/noise checks; enemy songs -%d.",
            song_skill, silence_bonus, enemy_song_penalty);
        break;
    }
    case SNG_STAUNCHING:
    {
        int base_heal = song_skill / 12;
        int extra_turns = song_skill % 12;

        if (extra_turns > 0)
        {
            strnfmt(buf, sizeof(buf),
                "Current effect at Song %d: stops bleeding and heals %d HP/turn, with +1 extra on %d turns in 12.",
                song_skill, base_heal, extra_turns);
        }
        else
        {
            strnfmt(buf, sizeof(buf),
                "Current effect at Song %d: stops bleeding and heals %d HP/turn.",
                song_skill, base_heal);
        }
        break;
    }
    case SNG_THRESHOLDS:
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: door-warding checks use Song %d.",
            song_skill, song_skill);
        break;
    case SNG_TREES:
    {
        int light_radius = ability_semantic_stepped_song_bonus(song_skill, 5, 6);
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: +%d light radius.", song_skill,
            light_radius);
        break;
    }
    case SNG_WOVEN_THEMES:
    {
        int minor_skill = ability_semantic_minor_song_score(song_skill);
        int synergy_bonus = ability_semantic_song_synergy_bonus(song_skill);
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: a minor theme uses Song %d; a valid synergy pair adds +%d Song.",
            song_skill, minor_skill, synergy_bonus);
        break;
    }
    case SNG_SLAYING:
    {
        int hp_threshold = song_skill * 2;
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN)
            hp_threshold *= 2;

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: criticals can slay foes at %d HP or less.",
            song_skill, hp_threshold);
        break;
    }
    case SNG_REVEALING:
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: revealing range %d squares.",
            song_skill, (song_skill / 2) + 8);
        break;
    case SNG_ELVENESS:
    {
        int evasion_bonus = ability_semantic_stepped_song_bonus(song_skill, 7, 8);
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: +1 Grace and +%d Evasion.",
            song_skill, evasion_bonus);
        break;
    }
    case SNG_STAYING:
    {
        int will_bonus = song_skill / 2;
        int protection_dice = 2;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN)
        {
            will_bonus = song_skill * 2;
            protection_dice = 4;
        }

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: +%d Will and [%dd2] protection.",
            song_skill, will_bonus, protection_dice);
        break;
    }
    case SNG_DISGUISE:
    {
        int disguise_bonus = song_skill + 5;
        const char* extra = (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
            ? " + Perception"
            : "";

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: disguise checks use %d + Will%s.",
            song_skill, disguise_bonus, extra);
        break;
    }
    case SNG_LORIEN:
    {
        int sleep_score = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT)
            sleep_score = (3 * song_skill) / 2;

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: sleep checks use %d.",
            song_skill, sleep_score);
        break;
    }
    case SNG_SHATTERING:
        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: shatter checks use Song %d; each success has a %d%% weaken chance.",
            song_skill, song_skill, song_skill / 3);
        break;
    case SNG_MASTERY:
    {
        int mastery_bonus = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL)
            mastery_bonus = (7 * song_skill) / 4;

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: mastery rolls are 2d8 + %d.",
            song_skill, mastery_bonus);
        break;
    }
    case SNG_GRA:
        SDL_strlcpy(buf, "Current effect: +1 Grace.", sizeof(buf));
        break;
    case SNG_CONTEST:
    {
        int will_penalty = MAX(1, song_skill / 3);
        int stealth_penalty = MAX(1, song_skill / 2);
        int evasion_penalty = MAX(1, song_skill / 5);
        int armour_penalty = MAX(1, song_skill / 12);

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: duel checks use Song + Will/2; victory inflicts -%d Will, -%d Stealth, -%d Evasion, -%d armour die.",
            song_skill, will_penalty, stealth_penalty, evasion_penalty,
            armour_penalty);
        break;
    }
    case SNG_LAMENT:
    {
        int will_penalty = MAX(1, song_skill / 2);
        int attrition_steps = MAX(1, song_skill / 12);

        strnfmt(buf, sizeof(buf),
            "Current effect at Song %d: duel checks use Song + Will/2; victory inflicts -%d Will and -%d health/damage steps.",
            song_skill, will_penalty, attrition_steps);
        break;
    }
    default:
        break;
    }

    if (buf[0])
        (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, buf);
}

void ability_semantic_add_description_lines(app_ui_panel* panel,
    int skilltype, const ability_type* b_ptr)
{
    const char* desc_text;
    const char* effect_text;
    const char* death_text = NULL;
    bool has_desc;
    bool has_effect;
    int oath_id;
    bool added_section = false;

    if (!panel || !b_ptr)
        return;

    desc_text = b_ptr->text ? (b_text + b_ptr->text) : NULL;
    effect_text = b_ptr->effect ? (b_text + b_ptr->effect) : NULL;
    has_desc = desc_text && desc_text[0];
    has_effect = effect_text && effect_text[0];

    oath_id = (skilltype == S_SPC)
        ? ability_semantic_oath_id_for_ability(b_ptr->abilitynum)
        : 0;
    if (oath_id > 0 && oath_invalid(oath_id))
        death_text = oath_death_message(oath_id);

    if (death_text && death_text[0])
    {
        (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_RED,
            death_text, 58);
        added_section = true;
    }
    else
    {
        switch (op_ptr->ability_desc_mode)
        {
        case 1:
            if (has_effect)
            {
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_L_WHITE, effect_text, 58);
                added_section = true;
            }
            if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (added_section)
                    (void)ability_semantic_add_detail_break(panel);
                ability_semantic_add_prerequisite_lines(panel, skilltype,
                    b_ptr);
                added_section = true;
            }
            if (has_desc)
            {
                if (added_section)
                    (void)ability_semantic_add_detail_break(panel);
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_SLATE, desc_text, 58);
                added_section = true;
            }
            break;

        case 2:
            if (has_effect)
            {
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_L_WHITE, effect_text, 58);
                added_section = true;
            }
            else if (has_desc)
            {
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_L_WHITE, desc_text, 58);
                added_section = true;
            }
            if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (added_section)
                    (void)ability_semantic_add_detail_break(panel);
                ability_semantic_add_prerequisite_lines(panel, skilltype,
                    b_ptr);
                added_section = true;
            }
            break;

        default:
            if (has_desc)
            {
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_SLATE, desc_text, 58);
                added_section = true;
            }
            if (has_effect)
            {
                if (added_section)
                    (void)ability_semantic_add_detail_break(panel);
                (void)ability_semantic_add_wrapped_detail_lines(panel,
                    TERM_L_WHITE, effect_text, 58);
                added_section = true;
            }
            if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (added_section)
                    (void)ability_semantic_add_detail_break(panel);
                ability_semantic_add_prerequisite_lines(panel, skilltype,
                    b_ptr);
                added_section = true;
            }
            break;
        }
    }

    if (skilltype == S_SPC && b_ptr->abilitynum == SPC_NIENA_MERCY
        && p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
    {
        if (added_section)
            (void)ability_semantic_add_detail_break(panel);
        ability_semantic_add_niena_mercy_lines(panel);
        added_section = true;
    }

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && skilltype == S_PER && b_ptr->abilitynum == PER_BANE
        && p_ptr->bane_type > 0)
    {
        if (added_section)
            (void)ability_semantic_add_detail_break(panel);
        ability_semantic_add_bane_status_lines(panel);
        added_section = true;
    }
    else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH
        && p_ptr->oath_type > 0)
    {
        if (added_section)
            (void)ability_semantic_add_detail_break(panel);
        ability_semantic_add_oath_status_lines(panel);
        added_section = true;
    }
    else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && skilltype == S_SPC && b_ptr->abilitynum == SPC_UNIQUE_BANE)
    {
        if (added_section)
            (void)ability_semantic_add_detail_break(panel);
        ability_semantic_add_unique_bane_lines(panel);
        added_section = true;
    }

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && skilltype == S_SNG)
    {
        if (added_section)
            (void)ability_semantic_add_detail_break(panel);
        ability_semantic_add_song_bonus_lines(panel, b_ptr);
    }
}
