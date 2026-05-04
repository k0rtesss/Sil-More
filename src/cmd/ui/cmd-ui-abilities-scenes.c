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

/* File: cmd-ui-abilities-scenes.c */

/*
 * Lane-local semantic scene helpers split from cmd-ui-abilities.c.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "cmd-ui-abilities-scenes.h"
#include "ui/ui-browser-shell.h"
#include "ui/ui-information-scene.h"

static int ability_purchase_exp_cost(int skilltype)
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

static void ability_menu_format_amount_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int need, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s %d / %d", short_label, need, have);
    else
        strnfmt(buf, buflen, "%d %s (you have %d)", need, long_label, have);
}

/* Song selection and curse application helpers live in
 * cmd-ui-abilities-songs.c. */
#define OATH_TYPES 6

char* oath_desc1[] = {
    "Nothing",
    "to leave Angband without shedding blood of Man or Elf",
    "to leave Angband as you came, grim and silent",
    "that none will daunt you from facing Morgoth forthwith",
    "to craft all blades and armour by thine own hand",
    "to face your enemy while it has the heart to fight",
    "to bear the light of the stars and refuse all shadowed gear",
};

char* oath_desc2[] = {
    "Nothing",
    "attack Men or Elves",
    "sing",
    "go up stairs without a Silmaril",
    "pick up weapons or armour from the ground",
    "attack or deal damage to enemies that are fleeing in terror",
    "wear items that dim or shroud your light",
};

char* oath_reward[] = {
    "Nothing",
    "+1 Grace",
    "+1 Strength",
    "+2 Constitution",
    "+5 Smithing",
    "+1 Dexterity",
    "+1 Light Radius",
};

static const char* oath_name_short(int oath_id)
{
    if (oath_id < 0 || oath_id > OATH_TYPES) return "Unknown";
    return oath_name[oath_id];
}

static const char* oath_desc2_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_desc2)) return "";
    return oath_desc2[oath_id];
}

static const char* oath_reward_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_reward)) return "";
    return oath_reward[oath_id];
}

typedef enum ability_semantic_focus {
    ABILITY_SEMANTIC_FOCUS_SKILLS = 0,
    ABILITY_SEMANTIC_FOCUS_ABILITIES
} ability_semantic_focus;

typedef struct ability_ui_entry {
    const ability_type* ability;
    int abilitynum;
    byte attr;
} ability_ui_entry;

typedef struct ability_semantic_state {
    int skill_order[S_MAX];
    int skill_count;
    int current_skill_slot;
    int ability_highlight[S_MAX];
    int bane_highlight;
    int oath_highlight;
    ability_semantic_focus focus;
    byte status_attr;
    char status[APP_UI_TEXT_MAX];
} ability_semantic_state;

static bool ability_screen_pause_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope || !scope->active)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool ability_screen_resume_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

static void ability_semantic_set_status(ability_semantic_state* state,
    byte attr, cptr text)
{
    if (!state)
        return;

    state->status_attr = attr;
    SDL_strlcpy(state->status, text ? text : "", sizeof(state->status));
}

static void ability_semantic_bell_status(ability_semantic_state* state,
    byte attr, cptr text)
{
    if (text && text[0])
        bell(text);
    ability_semantic_set_status(state, attr, text);
}

static int ability_semantic_oath_id_for_ability(int abilitynum)
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

static bool ability_semantic_skill_visible(int skilltype)
{
    int i;

    if (skilltype < 0 || skilltype >= S_MAX)
        return false;
    if (skilltype != S_SPC)
        return true;

    for (i = 0; i < ABILITIES_MAX; i++)
    {
        if (p_ptr->have_ability[S_SPC][i])
            return true;
    }

    return false;
}

static int ability_semantic_collect_visible_skills(int out_skilltypes[S_MAX])
{
    int i;
    int count = 0;

    for (i = 0; i < S_MAX; i++)
    {
        if (!ability_semantic_skill_visible(i))
            continue;

        if (out_skilltypes)
            out_skilltypes[count] = i;
        count++;
    }

    return count;
}

static byte ability_semantic_entry_attr(int skilltype,
    const ability_type* b_ptr)
{
    if (!b_ptr)
        return TERM_L_DARK;

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            return p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                ? TERM_WHITE
                : TERM_RED;

        return p_ptr->active_ability[skilltype][b_ptr->abilitynum]
            ? TERM_L_GREEN
            : TERM_RED;
    }

    if (prereqs(skilltype, b_ptr->abilitynum))
        return TERM_SLATE;

    return TERM_L_DARK;
}

static int ability_semantic_collect_visible_abilities(int skilltype,
    ability_ui_entry out[ABILITIES_MAX])
{
    int i;
    int count = 0;

    for (i = 0; i < z_info->b_max && count < ABILITIES_MAX; i++)
    {
        const ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;
        if (b_ptr->skilltype != skilltype)
            continue;
        if (b_ptr->abilitynum >= ABILITIES_MAX)
            continue;
        if (skilltype == S_SPC
            && !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            continue;
        }
        if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH)
            continue;

        if (out)
        {
            out[count].ability = b_ptr;
            out[count].abilitynum = b_ptr->abilitynum;
            out[count].attr = ability_semantic_entry_attr(skilltype, b_ptr);
        }
        count++;
    }

    return count;
}

static int ability_semantic_find_entry_index(const ability_ui_entry* entries,
    int count, int abilitynum)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (entries[i].abilitynum == abilitynum)
            return i;
    }

    return -1;
}

static int ability_semantic_current_skill(const ability_semantic_state* state)
{
    if (!state || state->skill_count <= 0)
        return 0;
    if (state->current_skill_slot < 0)
        return state->skill_order[0];
    if (state->current_skill_slot >= state->skill_count)
        return state->skill_order[state->skill_count - 1];
    return state->skill_order[state->current_skill_slot];
}

static void ability_semantic_sync_state(ability_semantic_state* state)
{
    int new_order[S_MAX];
    int count;
    int current_skill;
    int i;

    if (!state)
        return;

    count = ability_semantic_collect_visible_skills(new_order);
    if (count <= 0)
    {
        new_order[0] = 0;
        count = 1;
    }

    current_skill = (state->skill_count > 0)
        ? ability_semantic_current_skill(state)
        : new_order[0];

    memcpy(state->skill_order, new_order, sizeof(state->skill_order));
    state->skill_count = count;
    state->current_skill_slot = 0;

    for (i = 0; i < count; i++)
    {
        if (state->skill_order[i] == current_skill)
        {
            state->current_skill_slot = i;
            break;
        }
    }

    for (i = 0; i < count; i++)
    {
        ability_ui_entry entries[ABILITIES_MAX];
        int skilltype = state->skill_order[i];
        int entry_count = ability_semantic_collect_visible_abilities(skilltype,
            entries);
        int entry_index;

        if (entry_count <= 0)
        {
            state->ability_highlight[skilltype] = 0;
            continue;
        }

        entry_index = ability_semantic_find_entry_index(entries, entry_count,
            state->ability_highlight[skilltype] - 1);
        if (entry_index < 0)
            state->ability_highlight[skilltype] = entries[0].abilitynum + 1;
    }

    if (state->bane_highlight < 1)
        state->bane_highlight = 1;
    if (state->oath_highlight < 1)
        state->oath_highlight = 1;

    if (state->focus == ABILITY_SEMANTIC_FOCUS_ABILITIES)
    {
        int skilltype = ability_semantic_current_skill(state);

        if (state->ability_highlight[skilltype] <= 0)
            state->focus = ABILITY_SEMANTIC_FOCUS_SKILLS;
    }
}

static bool ability_semantic_add_wrapped_detail_lines(app_ui_panel* panel,
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

static bool ability_semantic_add_detail_break(app_ui_panel* panel)
{
    if (!panel)
        return false;
    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return false;
    return app_ui_panel_add_detail_line(panel, TERM_SLATE, " ");
}

static void ability_semantic_format_row_label(int skilltype,
    const ability_type* b_ptr, char* out, size_t outsz)
{
    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (!b_ptr)
        return;

    if (skilltype == S_PER && b_ptr->abilitynum == PER_BANE
        && p_ptr->bane_type > 0)
    {
        strnfmt(out, outsz, "%s-%s", bane_name[p_ptr->bane_type],
            b_name + b_ptr->name);
        return;
    }

    if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH
        && p_ptr->oath_type > 0)
    {
        strnfmt(out, outsz, "%s: %s", b_name + b_ptr->name,
            oath_name_short(p_ptr->oath_type));
        return;
    }

    SDL_strlcpy(out, b_name + b_ptr->name, outsz);
}

static void ability_semantic_format_row_meta(int skilltype,
    const ability_type* b_ptr, char* out, size_t outsz)
{
    int oath_id;

    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (!b_ptr)
        return;

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        oath_id = (skilltype == S_SPC)
            ? ability_semantic_oath_id_for_ability(b_ptr->abilitynum)
            : 0;

        if (oath_id > 0 && oath_invalid(oath_id))
        {
            SDL_strlcpy(out, "Broken", outsz);
            return;
        }

        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
        {
            SDL_strlcpy(out,
                p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                    ? "Innate"
                    : "Innate off",
                outsz);
            return;
        }

        SDL_strlcpy(out,
            p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                ? "On"
                : "Off",
            outsz);
        return;
    }

    if (prereqs(skilltype, b_ptr->abilitynum))
    {
        strnfmt(out, outsz, "%d xp", ability_purchase_exp_cost(skilltype));
        return;
    }

    if (p_ptr->skill_base[skilltype] < b_ptr->level)
    {
        strnfmt(out, outsz, "Need %d skill", b_ptr->level);
        return;
    }

    SDL_strlcpy(out, "Need prereq", outsz);
}

static void ability_semantic_add_prerequisite_lines(app_ui_panel* panel,
    int skilltype, const ability_type* b_ptr)
{
    char buf[APP_UI_TEXT_MAX];
    int j;

    if (!panel || !b_ptr)
        return;

    (void)app_ui_panel_add_detail_line(panel, TERM_YELLOW, "Prerequisites:");

    ability_menu_format_amount_line(buf, sizeof(buf), "skill points", "Skill",
        b_ptr->level, p_ptr->skill_base[skilltype], 80);
    (void)app_ui_panel_add_detail_line(panel,
        (b_ptr->level <= p_ptr->skill_base[skilltype]) ? TERM_L_GREEN
                                                       : TERM_L_DARK,
        buf);

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
        char amount[APP_UI_TEXT_MAX];

        ability_menu_format_amount_line(amount, sizeof(amount), "experience",
            "Exp",
            exp_cost, p_ptr->new_exp, 80);
        strnfmt(buf, sizeof(buf), "Current price: %s", amount);
        (void)app_ui_panel_add_detail_line(panel,
            (exp_cost <= p_ptr->new_exp) ? TERM_L_GREEN : TERM_L_DARK,
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

static void ability_semantic_add_footer_actions(app_ui_panel* panel)
{
    bool steamdeck = steamdeck_controls_active();

    if (!panel)
        return;

    if (steamdeck)
    {
        char confirm_label[APP_UI_KEY_MAX];
        char increase_label[APP_UI_KEY_MAX];
        char back_label[APP_UI_KEY_MAX];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X",
            increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
            confirm_label, "Select");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            increase_label, "Increase");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            back_label, "Back");
        return;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "4/6", "Back/Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_L_BLUE, true,
        "Enter", "Use");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "i", "Increase");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Tab", "Exit");
}

static bool ability_semantic_build_scene(app_ui_scene* scene,
    ability_semantic_state* state)
{
    app_ui_panel* panel;
    ability_ui_entry entries[ABILITIES_MAX];
    char subtitle[APP_UI_TEXT_MAX];
    char body[APP_UI_TEXT_MAX];
    int current_skill;
    int entry_count;
    int selected_index = 0;
    int i;

    if (!scene || !state)
        return false;

    ability_semantic_sync_state(state);
    current_skill = ability_semantic_current_skill(state);
    entry_count = ability_semantic_collect_visible_abilities(current_skill,
        entries);
    if (entry_count > 0)
    {
        selected_index = ability_semantic_find_entry_index(entries, entry_count,
            state->ability_highlight[current_skill] - 1);
        if (selected_index < 0)
            selected_index = 0;
        state->ability_highlight[current_skill] =
            entries[selected_index].abilitynum + 1;
    }

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 1180, 2200);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Abilities");

    strnfmt(subtitle, sizeof(subtitle), "%s %d  |  %d visible  |  %d exp",
        skill_names_full[current_skill], p_ptr->skill_base[current_skill],
        entry_count, p_ptr->new_exp);
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    for (i = 0; i < state->skill_count; i++)
    {
        int skilltype = state->skill_order[i];
        byte attr = TERM_SLATE;

        if (i == state->current_skill_slot)
        {
            attr = (state->focus == ABILITY_SEMANTIC_FOCUS_SKILLS)
                ? TERM_YELLOW
                : TERM_L_BLUE;
        }

        (void)app_ui_panel_add_tab(panel, (s16b)skilltype, attr,
            i == state->current_skill_slot, skill_names_full[skilltype]);
    }

    if (state->status[0])
    {
        (void)app_ui_panel_add_body_line(panel, state->status_attr,
            state->status);
    }
    else
    {
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
            (state->focus == ABILITY_SEMANTIC_FOCUS_SKILLS)
                ? "Choose a skill, then press Enter to browse abilities."
                : "Press Enter to purchase or toggle the selected ability.");
    }

    strnfmt(body, sizeof(body), "Affinity %d  |  Desc mode %d",
        affinity_level(current_skill), op_ptr->ability_desc_mode);
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE, body);

    for (i = 0; i < entry_count; i++)
    {
        char keybuf[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        keybuf[0] = '\0';
        if (i < 26)
            strnfmt(keybuf, sizeof(keybuf), "%c", (char)('a' + i));
        ability_semantic_format_row_label(current_skill, entries[i].ability,
            label, sizeof(label));
        ability_semantic_format_row_meta(current_skill, entries[i].ability,
            meta, sizeof(meta));
        (void)app_ui_panel_add_row_ex(panel, (s16b)entries[i].abilitynum,
            entries[i].attr, TERM_SLATE, 0, '\0', true,
            i == selected_index, keybuf, label, meta);
    }

    if (entry_count > 0)
    {
        char detail_title[APP_UI_TITLE_MAX];

        ability_semantic_format_row_label(current_skill,
            entries[selected_index].ability, detail_title,
            sizeof(detail_title));
        app_ui_panel_set_detail_title(panel, TERM_YELLOW, detail_title);
        ability_semantic_add_description_lines(panel, current_skill,
            entries[selected_index].ability);
    }
    else
    {
        app_ui_panel_set_detail_title(panel, TERM_L_DARK, "No abilities");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_DARK,
            "No abilities available for this skill.");
    }

    ability_semantic_add_footer_actions(panel);

    if (state->focus == ABILITY_SEMANTIC_FOCUS_SKILLS && panel->tab_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_TABS;
        panel->focus_id = panel->tabs[state->current_skill_slot].id;
    }
    else if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[panel->selected_row].id;
    }

    return true;
}

static bool ability_semantic_build_bane_scene(app_ui_scene* scene,
    int highlight)
{
    app_ui_panel* panel;
    int i;
    int selected = highlight - 1;

    if (!scene)
        return false;

    if (selected < 0)
        selected = 0;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 1040, 1700);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Enemy types");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Choose a bane from foes you have slain often enough.");

    for (i = 1; i < PLAYER_BANE_TYPES; i++)
    {
        char keybuf[APP_UI_KEY_MAX];
        char meta[APP_UI_META_MAX];
        int killed = bane_type_killed(i);
        byte attr = (killed >= 4) ? TERM_SLATE : TERM_L_DARK;

        strnfmt(keybuf, sizeof(keybuf), "%c", (char)('a' + i - 1));
        strnfmt(meta, sizeof(meta), "%d slain", killed);
        (void)app_ui_panel_add_row(panel, (s16b)i, attr, true,
            (i - 1) == selected, keybuf, bane_name[i], meta);
    }

    if (selected >= 0 && selected < panel->row_count)
    {
        int bane_type = selected + 1;
        int killed = bane_type_killed(bane_type);
        char buf[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_YELLOW, bane_name[bane_type]);
        if (killed >= 4)
        {
            strnfmt(buf, sizeof(buf), "You have slain %d of these foes.",
                killed);
            (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        }
        else
        {
            strnfmt(buf, sizeof(buf),
                "You have slain %d of these foes and need %d more.",
                killed, 4 - killed);
            (void)app_ui_panel_add_detail_line(panel, TERM_L_DARK, buf);
        }
    }

    ability_semantic_add_footer_actions(panel);
    panel->focus_area = APP_UI_FOCUS_ROWS;
    return true;
}

static char ability_semantic_choice_scroll_command_key(
    const app_ui_command* command)
{
    if (!command)
        return '\0';
    if (ABS(command->scroll_y) >= ABS(command->scroll_x)
        && command->scroll_y != 0)
    {
        return (command->scroll_y > 0) ? '8' : '2';
    }
    if (command->scroll_x != 0)
        return (command->scroll_x < 0) ? '4' : '6';

    return '\0';
}

static bool ability_semantic_choice_command_to_key(
    const app_ui_command* command, int max_choice, int* highlight,
    char* out_key)
{
    const app_ui_widget_ref* target;
    bool steamdeck;

    if (out_key)
        *out_key = '\0';
    if (!command || !highlight || !out_key)
        return false;

    target = &command->target;
    steamdeck = steamdeck_controls_active();

    if (command->kind == APP_UI_COMMAND_KIND_CANCEL
        || target->action == APP_UI_WIDGET_ACTION_CANCEL)
    {
        *out_key = ESCAPE;
        return true;
    }

    if (command->kind == APP_UI_COMMAND_KIND_SCROLL
        || target->role == APP_UI_WIDGET_ROLE_SCROLL_REGION)
    {
        *out_key = ability_semantic_choice_scroll_command_key(command);
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        if (target->widget_id >= 1 && target->widget_id <= max_choice)
            *highlight = target->widget_id;
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
            return true;
        *out_key = '\r';
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_BUTTON)
    {
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
            return true;

        if (steamdeck)
        {
            switch (target->widget_id)
            {
            case 2:
                *out_key = '\r';
                return true;
            case 4:
                *out_key = ESCAPE;
                return true;
            default:
                return true;
            }
        }

        switch (target->widget_id)
        {
        case 2:
        case 3:
            *out_key = '\r';
            return true;
        case 5:
        case 6:
            *out_key = ESCAPE;
            return true;
        default:
            return true;
        }
    }

    return false;
}

static int ability_semantic_run_bane_menu(int* highlight)
{
    int visible_count = PLAYER_BANE_TYPES - 1;

    if (!highlight)
        return PLAYER_BANE_TYPES + 1;

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > visible_count)
        *highlight = visible_count;

    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (!ability_semantic_build_bane_scene(&scene, *highlight)
            || !ui_information_scene_present_ui(&scene))
        {
            return PLAYER_BANE_TYPES + 1;
        }

        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = 0;
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && ability_semantic_choice_command_to_key(&event.command,
                    visible_count, highlight, &command_key))
            {
                ch = command_key;
                if (!ch)
                    continue;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
            {
                continue;
            }
        }
        if (steamdeck_controls_active())
        {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
        }

        if ((ch >= 'a') && (ch < 'a' + visible_count))
        {
            *highlight = (int)ch - 'a' + 1;
            continue;
        }
        if ((ch >= 'A') && (ch < 'A' + visible_count))
        {
            *highlight = (int)ch - 'A' + 1;
            return *highlight;
        }
        if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
            return PLAYER_BANE_TYPES + 1;
        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
            return *highlight;
        if (ch == '8')
        {
            (*highlight)--;
            if (*highlight < 1)
                *highlight = visible_count;
            continue;
        }
        if (ch == '2')
        {
            (*highlight)++;
            if (*highlight > visible_count)
                *highlight = 1;
            continue;
        }

        bell("Unknown command.");
    }
}

static bool ability_semantic_build_oath_scene(app_ui_scene* scene,
    int highlight)
{
    app_ui_panel* panel;
    int selected = highlight - 1;
    int i;
    static const char* oath_tolkien_desc[] = {
        "",
        "\"Let no blood of the Children stain thy blade in these halls of sorrow\"",
        "\"In silence came I, and in silence shall I depart, as befits the wise\"",
        "\"Though darkness gather and Balrogs rise, I shall not yield nor turn aside\"",
        "\"By mine own hand shall all blades be wrought, and no other's craft shall I bear\"",
        "\"Valor guards the fallen foe; the honorable blade stays when terror takes them\"",
        "\"I will carry unsullied starlight, shunning the shadowed tools that would dim it\""
    };

    if (!scene)
        return false;

    if (selected < 0)
        selected = 0;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 1100, 1900);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Oaths");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Choose the vow you wish to swear.");

    for (i = 1; i <= OATH_TYPES; i++)
    {
        char keybuf[APP_UI_KEY_MAX];
        byte attr = oath_invalid(i) ? TERM_L_RED : TERM_WHITE;

        strnfmt(keybuf, sizeof(keybuf), "%c", (char)('a' + i - 1));
        (void)app_ui_panel_add_row(panel, (s16b)i, attr, true,
            (i - 1) == selected, keybuf, oath_name_short(i),
            oath_reward_short(i));
    }

    if (selected >= 0 && selected < panel->row_count)
    {
        int oath_idx = selected + 1;

        app_ui_panel_set_detail_title(panel,
            oath_invalid(oath_idx) ? TERM_L_RED : TERM_YELLOW,
            oath_name_short(oath_idx));
        if (oath_invalid(oath_idx))
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_L_RED,
                "OATH BROKEN");
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_RED,
                "\"Thy oath lies shattered, thy word worthless as dust.\"",
                58);
            (void)ability_semantic_add_detail_break(panel);
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_L_RED,
                "\"No Valar shall hear thy voice, no light shall guide thy path.\"",
                58);
            (void)ability_semantic_add_detail_break(panel);
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_RED,
                "Forever marked as oathbreaker in this age.", 58);
        }
        else
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_YELLOW, "Quote:");
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_SLATE,
                oath_tolkien_desc[oath_idx], 58);
            (void)ability_semantic_add_detail_break(panel);
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, "Vow:");
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_SLATE,
                oath_desc1[oath_idx], 58);
            (void)ability_semantic_add_detail_break(panel);
            (void)app_ui_panel_add_detail_line(panel, TERM_L_RED,
                "Restriction:");
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_L_RED,
                oath_desc2_short(oath_idx), 58);
            (void)ability_semantic_add_detail_break(panel);
            (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                "Reward:");
            (void)ability_semantic_add_wrapped_detail_lines(panel, TERM_L_GREEN,
                oath_reward_short(oath_idx), 58);
        }
    }

    ability_semantic_add_footer_actions(panel);
    panel->focus_area = APP_UI_FOCUS_ROWS;
    return true;
}

static int ability_semantic_run_oath_menu(int* highlight)
{
    if (!highlight)
        return OATH_TYPES + 1;

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > OATH_TYPES)
        *highlight = OATH_TYPES;

    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (!ability_semantic_build_oath_scene(&scene, *highlight)
            || !ui_information_scene_present_ui(&scene))
        {
            return OATH_TYPES + 1;
        }

        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = 0;
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && ability_semantic_choice_command_to_key(&event.command,
                    OATH_TYPES, highlight, &command_key))
            {
                ch = command_key;
                if (!ch)
                    continue;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
            {
                continue;
            }
        }
        if (steamdeck_controls_active())
        {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
        }

        if ((ch >= 'a') && (ch < 'a' + OATH_TYPES))
        {
            *highlight = (int)ch - 'a' + 1;
            continue;
        }
        if ((ch >= 'A') && (ch < 'A' + OATH_TYPES))
        {
            *highlight = (int)ch - 'A' + 1;
            return *highlight;
        }
        if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
            return OATH_TYPES + 1;
        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
            return *highlight;
        if (ch == '8')
        {
            (*highlight)--;
            if (*highlight < 1)
                *highlight = OATH_TYPES;
            continue;
        }
        if (ch == '2')
        {
            (*highlight)++;
            if (*highlight > OATH_TYPES)
                *highlight = 1;
            continue;
        }

        bell("Unknown command.");
    }
}

static void ability_semantic_grant_ability(int skilltype, int abilitynum,
    int banechoice, int oathchoice, int exp_cost)
{
    p_ptr->innate_ability[skilltype][abilitynum] = true;
    p_ptr->have_ability[skilltype][abilitynum] = true;
    p_ptr->active_ability[skilltype][abilitynum] = true;
    ability_log_record_gain(skilltype, abilitynum);
    p_ptr->new_exp -= exp_cost;

    if (banechoice <= 0 && oathchoice <= 0)
    {
        do_cmd_note(format("(%s)",
                        b_name
                            + (&b_info[ability_index(skilltype, abilitynum)])
                                  ->name),
            p_ptr->depth);
    }
    else if (oathchoice <= 0)
    {
        p_ptr->bane_type = banechoice;
        do_cmd_note(format("(%s-%s)", bane_name[banechoice],
                        b_name
                            + (&b_info[ability_index(skilltype, abilitynum)])
                                  ->name),
            p_ptr->depth);
    }
    else
    {
        int oath_special = -1;

        p_ptr->oath_type = oathchoice;

        switch (oathchoice)
        {
        case OATH_MERCY:
            oath_special = SPC_OATH_MERCY;
            break;
        case OATH_SILENCE:
            oath_special = SPC_OATH_SILENCE;
            break;
        case OATH_IRON:
            oath_special = SPC_OATH_IRON;
            break;
        case OATH_SMITH:
            oath_special = SPC_OATH_SMITH;
            break;
        case OATH_VALOROUS:
            oath_special = SPC_OATH_VALOROUS;
            break;
        case OATH_LIGHT:
            oath_special = SPC_OATH_LIGHT;
            break;
        }

        if (oath_special >= 0)
        {
            p_ptr->have_ability[S_SPC][oath_special] = true;
            p_ptr->innate_ability[S_SPC][oath_special] = true;
            p_ptr->active_ability[S_SPC][oath_special] = true;
            ability_log_record_gain(S_SPC, oath_special);
        }

        do_cmd_note(format("(%s: %s)",
                        b_name
                            + (&b_info[ability_index(skilltype, abilitynum)])
                                  ->name,
                        oath_name_short(oathchoice)),
            p_ptr->depth);
    }

    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->update |= (PU_BONUS | PU_MANA);
    handle_stuff();
}

static void ability_semantic_toggle_existing_ability(int skilltype,
    int abilitynum, ability_semantic_state* state)
{
    bool changed = false;

    if (skilltype == S_SPC
        && (abilitynum == SPC_OATH_MERCY
            || abilitynum == SPC_OATH_SILENCE
            || abilitynum == SPC_OATH_IRON
            || abilitynum == SPC_OATH_SMITH
            || abilitynum == SPC_OATH_VALOROUS
            || abilitynum == SPC_OATH_LIGHT))
    {
        int oath_id = ability_semantic_oath_id_for_ability(abilitynum);

        if (p_ptr->active_ability[skilltype][abilitynum])
        {
            ability_semantic_set_status(state, TERM_WHITE,
                "Sacred oaths cannot be deactivated once sworn.");
        }
        else if (oath_id > 0 && oath_invalid(oath_id))
        {
            ability_semantic_set_status(state, TERM_RED,
                "Broken oaths cannot be reactivated. They are lost forever.");
        }
        else
        {
            p_ptr->active_ability[skilltype][abilitynum] = true;
            ability_semantic_set_status(state, TERM_WHITE,
                "Oath ability reactivated.");
            changed = true;
        }
    }
    else if (p_ptr->active_ability[skilltype][abilitynum])
    {
        p_ptr->active_ability[skilltype][abilitynum] = false;
        ability_semantic_set_status(state, TERM_WHITE,
            "Ability now switched off.");
        changed = true;

        if (skilltype == S_SNG && abilitynum == SNG_WOVEN_THEMES)
            p_ptr->song2 = SNG_NOTHING;
    }
    else
    {
        p_ptr->active_ability[skilltype][abilitynum] = true;
        ability_semantic_set_status(state, TERM_WHITE,
            "Ability now switched on.");
        changed = true;
    }

    if (changed)
    {
        p_ptr->redraw |= (PR_EXP | PR_BASIC);
        p_ptr->update |= (PU_BONUS | PU_MANA);
        handle_stuff();
    }
}

static bool ability_semantic_activate_selected(int skilltype, int abilitynum,
    ability_semantic_state* state)
{
    ability_type* b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    if (!p_ptr->have_ability[skilltype][abilitynum])
    {
        int banechoice = -1;
        int oathchoice = -1;
        int exp_cost;
        bool has_skill_prereq;
        bool has_ability_prereq;

        if (skilltype == S_SPC)
        {
            ability_semantic_bell_status(state, TERM_WHITE,
                "This special ability cannot be purchased.");
            return false;
        }

        has_skill_prereq = (p_ptr->skill_base[skilltype] >= b_ptr->level);
        has_ability_prereq = ability_prereqs_met(skilltype, abilitynum);

        if (!has_skill_prereq || !has_ability_prereq)
        {
            ability_semantic_bell_status(state,
                has_skill_prereq ? TERM_RED : TERM_L_DARK,
                has_skill_prereq
                    ? "Insufficient prerequisite abilities for ability!"
                    : "Insufficient skill points for ability!");
            return false;
        }

        exp_cost = ability_purchase_exp_cost(skilltype);
        if (exp_cost > p_ptr->new_exp)
        {
            ability_semantic_bell_status(state, TERM_L_DARK,
                "You do not have enough experience to acquire this ability.");
            return false;
        }

        if (skilltype == S_PER && abilitynum == PER_BANE)
        {
            banechoice = ability_semantic_run_bane_menu(&state->bane_highlight);
            if (banechoice == PLAYER_BANE_TYPES + 1)
                return true;

            if (bane_type_killed(banechoice) < 4)
            {
                ability_semantic_bell_status(state, TERM_L_DARK,
                    "Insufficient kills to become a bane.");
                return false;
            }
        }

        if (skilltype == S_WIL && abilitynum == WIL_OATH)
        {
            oathchoice = ability_semantic_run_oath_menu(&state->oath_highlight);
            if (oathchoice == OATH_TYPES + 1)
                return true;

            if (oath_invalid(oathchoice))
            {
                ability_semantic_bell_status(state, TERM_RED,
                    "This oath was broken before it was made.");
                return false;
            }
        }

        if (skilltype == S_SMT && abilitynum == SMT_MASTERPIECE
            && p_ptr->have_ability[S_SPC][SPC_AULE])
        {
            ability_semantic_bell_status(state, TERM_RED,
                "Aulë's Forge supersedes Masterpiece; you cannot purchase it.");
            return false;
        }

        if (get_check("Are you sure you wish to gain this ability? "))
        {
            ability_semantic_grant_ability(skilltype, abilitynum, banechoice,
                oathchoice, exp_cost);
            ability_semantic_set_status(state, TERM_WHITE, "Ability gained.");
        }
        else
        {
            ability_semantic_set_status(state, TERM_SLATE,
                "Ability gain canceled.");
        }

        return false;
    }

    ability_semantic_toggle_existing_ability(skilltype, abilitynum, state);
    return false;
}

static bool ability_semantic_select_skill_by_type(
    ability_semantic_state* state, int skilltype)
{
    int i;

    if (!state)
        return false;

    for (i = 0; i < state->skill_count; i++)
    {
        if (state->skill_order[i] != skilltype)
            continue;
        state->current_skill_slot = i;
        state->focus = ABILITY_SEMANTIC_FOCUS_SKILLS;
        ability_semantic_set_status(state, TERM_SLATE, "");
        return true;
    }

    return false;
}

static bool ability_semantic_select_ability_by_id(
    ability_semantic_state* state, int current_skill,
    const ability_ui_entry* entries, int entry_count, int abilitynum)
{
    int index;

    if (!state || !entries || entry_count <= 0)
        return false;

    index = ability_semantic_find_entry_index(entries, entry_count, abilitynum);
    if (index < 0)
        return false;

    state->ability_highlight[current_skill] = entries[index].abilitynum + 1;
    state->focus = ABILITY_SEMANTIC_FOCUS_ABILITIES;
    ability_semantic_set_status(state, TERM_SLATE, "");
    return true;
}

static char ability_semantic_scroll_command_key(
    const app_ui_command* command)
{
    if (!command)
        return '\0';
    if (ABS(command->scroll_y) >= ABS(command->scroll_x)
        && command->scroll_y != 0)
    {
        return (command->scroll_y > 0) ? '8' : '2';
    }
    if (command->scroll_x != 0)
        return (command->scroll_x < 0) ? '4' : '6';

    return '\0';
}

static bool ability_semantic_footer_command_to_key(
    ability_semantic_state* state, const app_ui_command* command,
    char* out_key)
{
    const app_ui_widget_ref* target;
    bool steamdeck;

    if (!state || !command || !out_key)
        return false;

    target = &command->target;
    steamdeck = steamdeck_controls_active();

    if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
        return true;

    if (steamdeck)
    {
        switch (target->widget_id)
        {
        case 2:
            *out_key = '\r';
            return true;
        case 3:
            *out_key = 'i';
            return true;
        case 4:
            *out_key = ESCAPE;
            return true;
        default:
            return true;
        }
    }

    switch (target->widget_id)
    {
    case 2:
        *out_key = (state->focus == ABILITY_SEMANTIC_FOCUS_SKILLS)
            ? '\r'
            : '4';
        return true;
    case 3:
        *out_key = '\r';
        return true;
    case 4:
        *out_key = 'i';
        return true;
    case 5:
        *out_key = ESCAPE;
        return true;
    case 6:
        *out_key = '\t';
        return true;
    default:
        return true;
    }
}

static bool ability_semantic_command_to_key(ability_semantic_state* state,
    const ability_ui_entry* entries, int entry_count, int current_skill,
    const app_ui_command* command, char* out_key)
{
    const app_ui_widget_ref* target;

    if (out_key)
        *out_key = '\0';
    if (!state || !command || !out_key)
        return false;

    target = &command->target;

    if (command->kind == APP_UI_COMMAND_KIND_CANCEL
        || target->action == APP_UI_WIDGET_ACTION_CANCEL)
    {
        *out_key = ESCAPE;
        return true;
    }

    if (command->kind == APP_UI_COMMAND_KIND_SCROLL
        || target->role == APP_UI_WIDGET_ROLE_SCROLL_REGION)
    {
        *out_key = ability_semantic_scroll_command_key(command);
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_TAB)
    {
        if (!ability_semantic_select_skill_by_type(state, target->widget_id))
            bell("Cannot switch ability tab!");
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        (void)ability_semantic_select_ability_by_id(state, current_skill,
            entries, entry_count, target->widget_id);
        if (ui_browser_shell_list_item_should_focus_only(command, false))
            return true;
        *out_key = '\r';
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_BUTTON)
        return ability_semantic_footer_command_to_key(state, command, out_key);

    return false;
}

static void do_cmd_ability_screen_semantic(void)
{
    ability_semantic_state state;
    ui_information_scene_scope scope;
    bool done = false;

    memset(&state, 0, sizeof(state));
    state.focus = ABILITY_SEMANTIC_FOCUS_SKILLS;
    state.bane_highlight = 1;
    state.oath_highlight = 1;
    ability_semantic_sync_state(&state);

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("ability screen: information-scene scope unavailable");
        msg_print("Ability screen unavailable.");
        return;
    }

    while (!done)
    {
        app_ui_scene scene;
        ability_ui_entry entries[ABILITIES_MAX];
        int current_skill;
        int entry_count;
        int ch;
        bool exit_after_activate;

        ability_semantic_sync_state(&state);
        current_skill = ability_semantic_current_skill(&state);
        entry_count = ability_semantic_collect_visible_abilities(current_skill,
            entries);

        if (!ability_semantic_build_scene(&scene, &state)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            log_warn("ability screen: semantic scene presentation failed");
            msg_print("Ability screen unavailable.");
            return;
        }

        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = 0;
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && ability_semantic_command_to_key(&state, entries,
                    entry_count, current_skill, &event.command, &command_key))
            {
                ch = command_key;
                if (!ch)
                    continue;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
            {
                continue;
            }
        }
        if (steamdeck_controls_active())
        {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
            else if (ch == steamdeck_alt_action_key())
                ch = 'i';
        }

        if (ch == '\t')
        {
            done = true;
            continue;
        }

        if (ch == 'i')
        {
            if (!ability_screen_pause_information_scene(&scope))
                break;
            gain_skills();
            if (!ability_screen_resume_information_scene(&scope))
                return;
            p_ptr->redraw |= (PR_EXP | PR_BASIC);
            p_ptr->update |= (PU_BONUS | PU_MANA);
            handle_stuff();
            ability_semantic_set_status(&state, TERM_SLATE, "");
            continue;
        }

        if (state.focus == ABILITY_SEMANTIC_FOCUS_SKILLS)
        {
            if ((ch == ESCAPE) || (ch == 'q') || (ch == 'Q'))
            {
                done = true;
                continue;
            }

            if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
            {
                if (entry_count > 0)
                {
                    state.focus = ABILITY_SEMANTIC_FOCUS_ABILITIES;
                }
                else
                {
                    ability_semantic_bell_status(&state, TERM_L_DARK,
                        "No abilities available for this skill.");
                }
                continue;
            }

            if ((ch >= 'a') && (ch < 'a' + state.skill_count))
            {
                state.current_skill_slot = (int)ch - 'a';
                if (state.ability_highlight[
                        state.skill_order[state.current_skill_slot]] > 0)
                {
                    state.focus = ABILITY_SEMANTIC_FOCUS_ABILITIES;
                }
                else
                {
                    ability_semantic_bell_status(&state, TERM_L_DARK,
                        "No abilities available for this skill.");
                }
                continue;
            }
            if ((ch >= 'A') && (ch < 'A' + state.skill_count))
            {
                state.current_skill_slot = (int)ch - 'A';
                if (state.ability_highlight[
                        state.skill_order[state.current_skill_slot]] > 0)
                {
                    state.focus = ABILITY_SEMANTIC_FOCUS_ABILITIES;
                }
                else
                {
                    ability_semantic_bell_status(&state, TERM_L_DARK,
                        "No abilities available for this skill.");
                }
                continue;
            }

            if (ch == '8'
#ifdef ARROW_UP
                || ch == ARROW_UP
#endif
            )
            {
                state.current_skill_slot =
                    (state.current_skill_slot + state.skill_count - 1)
                    % state.skill_count;
                continue;
            }
            if (ch == '2'
#ifdef ARROW_DOWN
                || ch == ARROW_DOWN
#endif
            )
            {
                state.current_skill_slot =
                    (state.current_skill_slot + 1) % state.skill_count;
                continue;
            }

            ability_semantic_bell_status(&state, TERM_RED,
                "Illegal command for ability screen!");
            continue;
        }

        if ((ch == ESCAPE) || (ch == 'q') || (ch == 'Q') || (ch == '4'))
        {
            state.focus = ABILITY_SEMANTIC_FOCUS_SKILLS;
            continue;
        }

        if (entry_count <= 0)
        {
            state.focus = ABILITY_SEMANTIC_FOCUS_SKILLS;
            continue;
        }

        if ((ch >= 'a') && (ch < 'a' + entry_count))
        {
            state.ability_highlight[current_skill] =
                entries[(int)ch - 'a'].abilitynum + 1;
            continue;
        }
        if ((ch >= 'A') && (ch < 'A' + entry_count))
        {
            state.ability_highlight[current_skill] =
                entries[(int)ch - 'A'].abilitynum + 1;
            continue;
        }

        if (ch == '8'
#ifdef ARROW_UP
            || ch == ARROW_UP
#endif
        )
        {
            int selected = ability_semantic_find_entry_index(entries, entry_count,
                state.ability_highlight[current_skill] - 1);

            if (selected < 0)
                selected = 0;
            selected = (selected + entry_count - 1) % entry_count;
            state.ability_highlight[current_skill] =
                entries[selected].abilitynum + 1;
            continue;
        }

        if (ch == '2'
#ifdef ARROW_DOWN
            || ch == ARROW_DOWN
#endif
        )
        {
            int selected = ability_semantic_find_entry_index(entries, entry_count,
                state.ability_highlight[current_skill] - 1);

            if (selected < 0)
                selected = 0;
            selected = (selected + 1) % entry_count;
            state.ability_highlight[current_skill] =
                entries[selected].abilitynum + 1;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            int selected = ability_semantic_find_entry_index(entries, entry_count,
                state.ability_highlight[current_skill] - 1);

            if (selected < 0)
                selected = 0;
            exit_after_activate = ability_semantic_activate_selected(
                current_skill, entries[selected].abilitynum, &state);
            if (exit_after_activate)
                done = true;
            continue;
        }

        ability_semantic_bell_status(&state, TERM_RED,
            "Illegal command for ability screen!");
    }

    ui_information_scene_leave(&scope);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    handle_stuff();
    inven_enforce_current_pack_limits();
}

void do_cmd_ability_screen(void)
{
    do_cmd_ability_screen_semantic();
}
