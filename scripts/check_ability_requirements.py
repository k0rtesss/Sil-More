#!/usr/bin/env python3
"""Check parsed skill gates and atomic multi-skill ability purchases.

Run build-incremental.ps1 first. Uses real engine objects, a terminal fixture,
and temporary template caches. No player saves or visible game windows are used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/ability-requirements"

HARNESS = r'''
#include "angband.h"
#include "log/log.h"
#include "init/init2-internal.h"
#include "player/player-upkeep-internal.h"
#include <assert.h>

/* Capture the actual purchase question; only the user's answer is simulated. */
bool fixture_confirm(cptr prompt);
#define get_check fixture_confirm
#include "cmd/ui/cmd-ui-abilities.c"
#undef get_check

static term fixture_term;
static char confirmation[1024];
static int confirmation_count;
static bool accept_purchase;
static cptr input_sequence;
static cptr capture_path;
extern char g_touch_pane_yes_no_prompt_text[];

bool fixture_confirm(cptr prompt)
{
    SDL_strlcpy(confirmation, prompt, sizeof(confirmation));
    sdl_touch_pane_begin_yes_no_prompt(prompt);
    size_t length = strlen(prompt);
    while (length && isspace((unsigned char)prompt[length - 1])) length--;
    assert(strlen(g_touch_pane_yes_no_prompt_text) == length);
    assert(!strncmp(g_touch_pane_yes_no_prompt_text, prompt, length));
    sdl_touch_pane_end_yes_no_prompt();
    confirmation_count++;
    return accept_purchase;
}

static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT)
    {
        if (input_sequence)
        {
            assert(*input_sequence);
            if (*input_sequence == ESCAPE && capture_path)
            {
                FILE* out = fopen(capture_path, "w"); assert(out);
                for (int y = 0; y < Term->hgt; ++y)
                {
                    for (int x = 0; x < Term->wid; ++x)
                    {
                        byte attr; char ch; Term_what(x, y, &attr, &ch);
                        fputc(ch ? ch : ' ', out);
                    }
                    fputc('\n', out);
                }
                fclose(out);
            }
            Term_keypress(*input_sequence++);
        }
        else Term_keypress(' ');
    }
    return 0;
}

static void reset_player(void)
{
    memset(p_ptr->skill_base, 0, sizeof(p_ptr->skill_base));
    memset(p_ptr->skill_use, 0, sizeof(p_ptr->skill_use));
    memset(p_ptr->skill_equip_mod, 0, sizeof(p_ptr->skill_equip_mod));
    memset(p_ptr->skill_misc_mod, 0, sizeof(p_ptr->skill_misc_mod));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->insight_ability_upgraded, 0, sizeof(p_ptr->insight_ability_upgraded));
    p_ptr->insight_monster_types = 0;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    for (int stat = 0; stat < A_MAX; ++stat) p_ptr->stat_base[stat] = 3;
    p_ptr->new_exp = 100000;
    p_ptr->insight_points = 77;
    op_ptr->opt[OPT_insight_beta] = false;
    confirmation[0] = '\0'; confirmation_count = 0;
    accept_purchase = false;
}

static errr parse_line(header* h, cptr line)
{
    char text[256];
    SDL_strlcpy(text, line, sizeof(text));
    return parse_b_info(text, h);
}

static void check_parser(void)
{
    ability_type entries[1] = {0};
    header h = {0};
    h.info_num = 1; h.info_ptr = entries;
    h.name_ptr = calloc(z_info->fake_name_size, 1);
    h.text_ptr = calloc(z_info->fake_text_size, 1);
    error_idx = -1;
    assert(!parse_line(&h, "N:0:Fixture"));
    assert(!parse_line(&h, "I:1:4:6"));
    assert(!parse_line(&h, "R:DEX:2:STEALTH:3:WILL:5"));
    assert(entries[0].stat_req[A_DEX] == 2);
    assert(entries[0].skill_req[S_STL] == 3 && entries[0].skill_req[S_WIL] == 5);
    assert(!parse_line(&h, "R:skill_archery:4:S_PER:2"));
    assert(ability_required_skill(&entries[0], S_ARC) == 6);
    assert(!parse_line(&h, "R:ARCHERY:8:STEALTH:2"));
    assert(ability_required_skill(&entries[0], S_ARC) == 8);
    assert(entries[0].skill_req[S_STL] == 3);
    assert(parse_line(&h, "R:SPECIAL:1"));
    assert(parse_line(&h, "R:STEALTH:101"));
    assert(parse_line(&h, "R:STEALTH:-1"));
    assert(parse_line(&h, "R:UNKNOWN:3"));
    assert(parse_line(&h, "R:WILL"));
    assert(!parse_line(&h, "R:0:2")); /* Existing numeric stat syntax. */
    assert(entries[0].stat_req[A_STR] == 2);
    assert(!parse_line(&h, "L:1"));
    assert(!parse_line(&h, "K:3"));
    assert(entries[0].insight_branch && entries[0].insight_cost == 3);
    assert(parse_line(&h, "L:2"));
    assert(parse_line(&h, "K:-1"));
    assert(!parse_line(&h, "U:1"));
    assert(entries[0].insight_upgrade_cost == 1 && entries[0].insight_cost == 3);
    assert(parse_line(&h, "U:-1"));
    assert(parse_line(&h, "U:256"));
    assert(parse_line(&h, "U:1:2"));
    assert(parse_line(&h, "B:0/10:0:1"));
    assert(parse_line(&h, "B:0/10:1:-1"));
    assert(parse_line(&h, "B:0/10:1:256"));
    assert(parse_line(&h, "B:0/10:1:1:2"));
    assert(parse_line(&h, "B:8/0:1:0"));
    assert(parse_line(&h, "B:1/0:1:2:3"));
    assert(!parse_line(&h, "B:0/10:1:0"));
    assert(!parse_line(&h, "B:1/0:1:0"));
    assert(entries[0].stage_parent_count == 2
        && entries[0].stage_parent_skill[0] == 0
        && entries[0].stage_parent_ability[0] == 10
        && entries[0].stage_parent_skill[1] == 1
        && entries[0].stage_parent_ability[1] == 0);
    assert(entries[0].stage_cost == 1 && entries[0].stage_choice_group == 0);
    assert(parse_line(&h, "B:0/10:2:0")); /* Alternatives share their price. */
    assert(parse_line(&h, "B:0/10:1:1")); /* Alternatives share their choice group. */
    assert(parse_line(&h, "B:1/0:1:0")); /* Duplicate parent. */
    assert(parse_line(&h, "R:LORE:2")); /* Insight is no longer an attribute. */
    free(h.name_ptr); free(h.text_ptr);
    puts("Parser: mixed stats/Insight/skills, aliases, duplicate maxima and invalid ranks PASS.");
}

static void check_selected_requirements(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    ability_type* contest = &b_info[ability_index(S_SNG, SNG_CONTEST)];
    assert(ambush->skill_req[S_STL] == 3 && contest->skill_req[S_WIL] == 5);
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_use[S_STL] = p_ptr->skill_equip_mod[S_STL] = 30;
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = true;
    assert(!prereqs(S_ARC, ARC_AMBUSH));
    p_ptr->skill_base[S_STL] = 3;
    assert(prereqs(S_ARC, ARC_AMBUSH));
    p_ptr->skill_base[S_SNG] = 12;
    assert(!prereqs(S_SNG, SNG_CONTEST));
    p_ptr->skill_base[S_WIL] = 5;
    assert(prereqs(S_SNG, SNG_CONTEST));
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = false;
    assert(!prereqs(S_SNG, SNG_CONTEST));
    int parents[] = {SNG_STAYING, SNG_DISGUISE, SNG_MASTERY};
    for (int i = 0; i < 3; ++i)
    {
        p_ptr->innate_ability[S_SNG][parents[i]] = true;
        assert(prereqs(S_SNG, SNG_CONTEST));
        p_ptr->innate_ability[S_SNG][parents[i]] = false;
    }
    p_ptr->skill_base[S_EVN] = 8;
    assert(!prereqs(S_EVN, EVN_FLANKING));
    p_ptr->have_ability[S_EVN][EVN_DODGING] = true;
    assert(!prereqs(S_EVN, EVN_FLANKING)); /* Equipment cannot teach a prerequisite. */
    p_ptr->innate_ability[S_EVN][EVN_DODGING] = true;
    assert(prereqs(S_EVN, EVN_FLANKING));
    p_ptr->skill_base[S_WIL] = 9;
    p_ptr->innate_ability[S_WIL][WIL_CURSE_BREAKING] = true;
    assert(!prereqs(S_WIL, WIL_MAJESTY));
    p_ptr->innate_ability[S_WIL][WIL_FORMIDABLE] = true;
    assert(prereqs(S_WIL, WIL_MAJESTY));
    assert(prereqs(S_SNG, SNG_SLAYING));
    p_ptr->innate_ability[S_WIL][WIL_FORMIDABLE] = false;
    p_ptr->innate_ability[S_WIL][WIL_INNER_LIGHT] = true;
    assert(prereqs(S_WIL, WIL_MAJESTY));
    p_ptr->innate_ability[S_SNG][SNG_STAUNCHING] = true;
    assert(!prereqs(S_SNG, SNG_SLAYING));
    p_ptr->innate_ability[S_SNG][SNG_CHALLENGE] = true;
    assert(prereqs(S_SNG, SNG_SLAYING));
    puts("Five selected abilities: real template gates, OR routes and Quick Study/base-rank isolation PASS.");
}

static bool description_has(ability_browser_desc_line* lines, int count, cptr text)
{
    for (int i = 0; i < count; ++i)
        if (strstr(lines[i].text, text)) return true;
    return false;
}

static void check_purchase(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    p_ptr->skill_base[S_MEL] = 9; /* Unrelated investment must survive. */
    int price = ability_purchase_exp_cost(S_ARC);
    int total = 2100 + 600 + price;
    ability_skill_training training;
    assert(ability_browser_plan_training(ambush, &training));
    assert(training.count == 2 && training.total_cost == 2700);
    char training_text[768];
    ability_browser_format_training(&training, training_text, sizeof(training_text));
    assert(strstr(training_text, "Archery 0 to 6 (2100 XP)"));
    assert(strstr(training_text, "Stealth 0 to 3 (600 XP)"));
    p_ptr->new_exp = total - 1;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(confirmation_count == 0 && p_ptr->new_exp == total - 1);
    assert(!p_ptr->skill_base[S_ARC] && !p_ptr->skill_base[S_STL]);
    assert(!p_ptr->innate_ability[S_ARC][ARC_AMBUSH]);
    p_ptr->new_exp = total;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(confirmation_count == 1 && p_ptr->new_exp == total);
    assert(!p_ptr->skill_base[S_ARC] && !p_ptr->skill_base[S_STL]);
    assert(strstr(confirmation, training_text));
    char total_text[64]; strnfmt(total_text, sizeof(total_text), "%d XP total", total);
    assert(strstr(confirmation, total_text));
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_ARC] == 6 && p_ptr->skill_base[S_STL] == 3);
    assert(p_ptr->skill_base[S_MEL] == 9 && p_ptr->insight_points == 77);
    assert(!p_ptr->new_exp && p_ptr->innate_ability[S_ARC][ARC_AMBUSH]);

    reset_player();
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_base[S_STL] = 2;
    assert(ability_browser_plan_training(ambush, &training));
    assert(training.count == 1 && training.total_cost == 300);
    p_ptr->new_exp = 300 + ability_purchase_exp_cost(S_ARC);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_STL] == 3 && !p_ptr->new_exp);
    assert(!strstr(confirmation, "Raise Archery"));

    reset_player();
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_base[S_STL] = 5;
    assert(ability_browser_plan_training(ambush, &training));
    assert(!training.count && !training.total_cost);
    p_ptr->new_exp = ability_purchase_exp_cost(S_ARC);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_STL] == 5 && !strstr(confirmation, "Raise"));

    reset_player();
    p_ptr->skill_base[S_SNG] = 12;
    p_ptr->innate_ability[S_SNG][SNG_STAYING] = true;
    p_ptr->new_exp = 1500 + ability_purchase_exp_cost(S_SNG);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(p_ptr->skill_base[S_WIL] == 5 && !p_ptr->new_exp);

    /* Acquired and starting gifts remain usable below the new learning gates. */
    p_ptr->skill_base[S_WIL] = 0;
    int questions = confirmation_count;
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(!p_ptr->active_ability[S_SNG][SNG_CONTEST]);
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(p_ptr->active_ability[S_SNG][SNG_CONTEST]);
    assert(confirmation_count == questions && !p_ptr->skill_base[S_WIL]);
    puts("Real purchase: multi-skill total, cancel/insufficient atomicity, exact XP, partial/no training and acquired gifts PASS.");
}

static void check_display_and_existing_gates(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
    for (int width = 30; width <= 80; width += 50)
    {
        int count = 0;
        ability_browser_add_prerequisites(lines, &count, S_ARC, ambush, width);
        assert(description_has(lines, count, "6 Archery base"));
        assert(description_has(lines, count, "3 Stealth base"));
        assert(description_has(lines, count, "Total XP:"));
        for (int i = 0; i < count; ++i) assert(strlen(lines[i].text) <= (size_t)width);
    }
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = true;
    p_ptr->stat_base[A_DEX] = 0;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    ability_skill_training training;
    ability_type synthetic = *ambush;
    synthetic.skill_req[S_ARC] = 8;
    assert(ability_browser_plan_training(&synthetic, &training));
    assert(training.total_cost == 3600 + 600); /* Primary counted only once. */
    synthetic.skill_req[S_WIL] = BASE_SKILL_MAX + 1;
    assert(!ability_browser_plan_training(&synthetic, &training));

    /* A data-authored ability can require more than two trained skills. */
    reset_player();
    ability_type saved_ambush = *ambush;
    for (int skill = 0; skill < S_SPC; ++skill) ambush->skill_req[skill] = 3;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(strlen(confirmation) > 160 && strstr(confirmation, "Song 0 to 3"));
    assert(strstr(confirmation, "XP total"));
    *ambush = saved_ambush;

    reset_player();
    ability_type* study = &b_info[ability_index(S_PER, PER_QUICK_STUDY)];
    byte saved_requirement = study->skill_req[S_STL];
    study->skill_req[S_STL] = 3;
    op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->new_exp = 599;
    assert(!ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(p_ptr->new_exp == 599 && p_ptr->insight_points == 77);
    assert(!p_ptr->skill_base[S_PER] && !p_ptr->skill_base[S_STL]);
    p_ptr->new_exp = 600; accept_purchase = true;
    assert(ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(!p_ptr->new_exp && p_ptr->insight_points == 76);
    assert(p_ptr->skill_base[S_PER] == 0 && p_ptr->skill_base[S_STL] == 3);
    study->skill_req[S_STL] = saved_requirement;
    puts("Wrapped requirements, duplicate primary ranks, stat gates and Insight-currency multi-skill purchase PASS.");
}

static void check_insight_points(void)
{
    extern NavResult player_birth_aux_2(int stats[A_MAX]);
    extern void get_extra(void);
    reset_player();
    p_ptr->insight_points = 0;
    p_ptr->insight_milestones = 0;
    insight_artefact_milestones(65);
    gain_insight_points(1, NULL);
    assert(!p_ptr->insight_points && !p_ptr->insight_milestones);
    op_ptr->opt[OPT_insight_beta] = true;
    assert(insight_monster_types_possible() == 14);
    insight_award_monster_type(RF3_ORC);
    assert(p_ptr->insight_points == 1 && insight_monster_types_seen() == 1);
    insight_award_monster_type(RF3_ORC);
    assert(p_ptr->insight_points == 1 && insight_monster_types_seen() == 1);
    insight_award_monster_type(RF3_RAUKO | RF3_ELF);
    assert(p_ptr->insight_points == 3 && insight_monster_types_seen() == 3);
    insight_award_monster_type(RF3_RACE_MASK);
    assert(p_ptr->insight_points == 14 && insight_monster_types_seen() == 14);
    insight_award_monster_type(RF3_RACE_MASK);
    assert(p_ptr->insight_points == 14 && insight_monster_types_seen() == 14);
    p_ptr->insight_points = 0;
    p_ptr->insight_monster_types = 0;
    p_ptr->insight_milestones = 0;
    insight_artefact_milestones(14); assert(!p_ptr->insight_points);
    for (int i = 0; i < 6; ++i)
    {
        insight_artefact_milestones(15 + 10 * i);
        assert(p_ptr->insight_points == i + 1);
        insight_artefact_milestones(15 + 10 * i);
        assert(p_ptr->insight_points == i + 1);
    }
    object_type forged = {0}; forged.k_idx = 1; forged.name1 = 1;
    catastrophe_accept_craft(65); catastrophe_crafted(&forged);
    assert(p_ptr->insight_points == 6); /* Same thresholds as identification. */
    p_ptr->insight_points = 0; p_ptr->insight_milestones = 0;
    catastrophe_accept_craft(35); catastrophe_crafted(&forged);
    assert(p_ptr->insight_points == 3);
    insight_artefact_milestones(25); assert(p_ptr->insight_points == 3);
    insight_award_milestone(INSIGHT_MILESTONE_SONG, NULL);
    insight_award_milestone(INSIGHT_MILESTONE_SONG, NULL);
    insight_award_milestone(INSIGHT_MILESTONE_CATASTROPHE, NULL);
    insight_award_milestone(INSIGHT_MILESTONE_CATASTROPHE, NULL);
    assert(p_ptr->insight_points == 5);
    gain_insight_points(1, NULL); gain_insight_points(1, NULL); /* Each new vault. */
    assert(p_ptr->insight_points == 7);
    p_ptr->insight_points = PY_MAX_EXP - 1;
    gain_insight_points(10, NULL); assert(p_ptr->insight_points == PY_MAX_EXP);

    /* Purchased ranks, rather than racial/house bonuses, set stat prices. */
    p_ptr->insight_stat_invested[A_STR] = 2;
    p_ptr->stat_base[A_STR] = 5;
    p_ptr->insight_points = 2;
    assert(insight_stat_increase_cost(A_STR) == 3);
    assert(!insight_increase_stat(A_STR) && p_ptr->stat_base[A_STR] == 5);
    p_ptr->insight_points = 3;
    assert(insight_increase_stat(A_STR));
    assert(p_ptr->stat_base[A_STR] == 6 && !p_ptr->insight_points);
    assert(p_ptr->insight_stat_invested[A_STR] == 3);
    assert(insight_stat_increase_cost(A_STR) == 4);
    assert(!insight_increase_stat(-1) && !insight_increase_stat(A_MAX));
    p_ptr->insight_stat_invested[A_STR] = 6;
    assert(!insight_stat_increase_cost(A_STR));

    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->new_exp = 0; p_ptr->insight_points = 3;
    ability_browser_entry entries[ABILITIES_MAX];
    int count = ability_browser_collect_entries(ABILITY_BROWSER_INSIGHT, entries, ABILITIES_MAX);
    assert(count == 11);
    assert(ability_browser_tab_skill(ability_menu_skill_options()) == ABILITY_BROWSER_INSIGHT);
    char tokens[S_MAX + 1][40]; int widths[S_MAX + 1];
    ability_browser_build_skill_tokens(ability_menu_skill_options() + 1,
        ability_menu_skill_options(), true, tokens, widths);
    assert(strstr(tokens[ability_menu_skill_options()], "Insight"));

    /* Stat abilities are Insight-only, cost one IP, waive their own XP, and
     * retain the authored primary skill gate through explicit R: fields. */
    const int stat_skills[] = { S_MEL, S_ARC, S_EVN, S_STL,
        S_PER, S_WIL, S_SMT, S_SNG };
    const int stat_abilities[] = { MEL_STR, ARC_DEX, EVN_DEX, STL_DEX,
        PER_GRA, WIL_CON, SMT_GRA, SNG_GRA };
    for (int i = 0; i < 8; ++i)
    {
        ability_type* stat = &b_info[ability_index(stat_skills[i], stat_abilities[i])];
        assert(stat->insight_branch && stat->insight_cost == 1);
        assert(ability_purchase_xp(stat) == 0);
        assert(ability_required_skill(stat, stat_skills[i]) == stat->level);
    }

    int purchased_branch = 0;
    for (int i = 0; i < count; ++i)
    {
        ability_type* a = entries[i].b_ptr;
        bool is_stat = false;
        for (int stat = 0; stat < 8; ++stat)
            if (a->skilltype == stat_skills[stat]
                && a->abilitynum == stat_abilities[stat]) is_stat = true;
        if (is_stat) continue;

        assert(ability_purchase_xp(a) == 0);
        assert(ability_purchase_insight_cost(a->skilltype, a->abilitynum) == 1);
        ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
        int n = ability_browser_build_description(ABILITY_BROWSER_INSIGHT, &entries[i], lines, 60);
        assert(description_has(lines, n, "1 IP"));
        accept_purchase = false;
        assert(!ability_browser_activate_choice(a->skilltype, a->abilitynum));
        assert(p_ptr->insight_points == 3 - purchased_branch);
        accept_purchase = true;
        assert(ability_browser_activate_choice(a->skilltype, a->abilitynum));
        assert(!p_ptr->new_exp && p_ptr->insight_points == 2 - purchased_branch);
        purchased_branch++;
    }
    assert(purchased_branch == 3);

    /* A stat ability cannot bypass its retained skill requirement even with
     * no XP available; once the rank is already present, only the IP is spent. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->new_exp = 0; p_ptr->insight_points = 1;
    p_ptr->skill_base[S_MEL] = 19;
    accept_purchase = true;
    assert(!ability_browser_activate_choice(S_MEL, MEL_STR));
    assert(!p_ptr->have_ability[S_MEL][MEL_STR]
        && p_ptr->insight_points == 1 && p_ptr->new_exp == 0);
    p_ptr->skill_base[S_MEL] = 20;
    assert(ability_browser_activate_choice(S_MEL, MEL_STR));
    assert(p_ptr->have_ability[S_MEL][MEL_STR]
        && p_ptr->insight_points == 0 && p_ptr->new_exp == 0);
    assert(!abilities_in_skill(S_PER) && !abilities_in_skill(S_WIL));
    count = ability_browser_collect_entries(S_PER, entries, ABILITIES_MAX);
    for (int i = 0; i < count; ++i) assert(!entries[i].b_ptr->insight_branch);

    /* K: on a normal ability adds IP without waiving XP or rank requirements. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    ability_type* a = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    byte old_cost = a->insight_cost; a->insight_cost = 2;
    int total = ability_purchase_xp(a) + 2700;
    p_ptr->insight_points = 1; p_ptr->new_exp = total;
    accept_purchase = true;
    assert(!ability_browser_activate_choice(a->skilltype, a->abilitynum));
    assert(p_ptr->new_exp == total && p_ptr->insight_points == 1);
    p_ptr->insight_points = 2; p_ptr->new_exp = total - 1;
    assert(!ability_browser_activate_choice(a->skilltype, a->abilitynum));
    assert(!p_ptr->skill_base[S_ARC] && !p_ptr->skill_base[S_STL]);
    p_ptr->new_exp = total;
    assert(ability_browser_activate_choice(a->skilltype, a->abilitynum));
    assert(!p_ptr->new_exp && !p_ptr->insight_points);
    assert(strstr(confirmation, "XP + 2 IP total"));
    assert(abilities_in_skill(S_ARC) == 1);
    a->insight_cost = old_cost;

    /* Exercise the real late-allocation modal, including cancellation and
     * nested screen restoration, at desktop and narrow terminal widths. */
    for (int width = 40; width <= 80; width += 40)
    {
        Term_resize(width, 24);
        p_ptr->insight_stat_invested[A_STR] = 0;
        p_ptr->stat_base[A_STR] = 3; p_ptr->insight_points = 3;
        Term_putstr(1, 1, -1, TERM_WHITE, "outer ability screen");
        accept_purchase = false;
        Term_flush(); input_sequence = "\r\033";
        ability_browser_train_attributes();
        input_sequence = NULL;
        assert(p_ptr->insight_points == 3 && p_ptr->stat_base[A_STR] == 3);
        accept_purchase = true;
        Term_flush(); input_sequence = "\r\033";
        ability_browser_train_attributes();
        input_sequence = NULL;
        assert(p_ptr->insight_points == 2 && p_ptr->stat_base[A_STR] == 4);
        assert(p_ptr->insight_stat_invested[A_STR] == 1);
        assert(strstr(confirmation, "1 insight point"));
        byte attr; char ch; Term_what(1, 1, &attr, &ch); assert(ch == 'o');
    }
    /* Confirming birth with points left retains them, including after the
     * setup helper runs again for the next creation screen. */
    int birth_stats[A_MAX] = {1, 1, 1, 1};
    p_ptr->tutorial_deferred = true;
    Term_flush(); input_sequence = "\r";
    assert(player_birth_aux_2(birth_stats) == NAV_OK);
    input_sequence = NULL;
    assert(p_ptr->insight_points == 9);
    for (int i = 0; i < A_MAX; ++i) assert(p_ptr->insight_stat_invested[i] == 1);
    get_extra(); assert(p_ptr->insight_points == 9);
    sdl_character_sheet_screen_hide();
    op_ptr->opt[OPT_insight_beta] = false;
    assert(!ability_browser_collect_entries(ABILITY_BROWSER_INSIGHT, entries, ABILITIES_MAX));
    assert(!insight_stat_increase_cost(A_CON));
    puts("Insight: milestones, shared smithing thresholds, retained stat spending, branch and atomic XP+IP purchases PASS.");
}

static void check_smithing_insight_upgrades(void)
{
    const int abilities[] = { SMT_ENCHANTMENT, SMT_EXPERTISE, SMT_ARTEFACT };
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->stat_base[A_DEX] = 4; p_ptr->stat_base[A_GRA] = 5;
    p_ptr->skill_base[S_SMT] = 10;
    p_ptr->innate_ability[S_SMT][SMT_WEAPONSMITH] = true;
    p_ptr->have_ability[S_SMT][SMT_WEAPONSMITH] = true;
    p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH] = true;
    p_ptr->insight_points = 0;
    accept_purchase = true;
    object_type heavy, ring;
    object_prep(&heavy, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    object_prep(&ring, lookup_kind(TV_RING, 0));
    int heavy_base = smithing_effective_skill(&heavy);
    int ring_base = smithing_effective_skill(&ring);
    int bonus = 0;
    for (int i = 0; i < 3; ++i)
    {
        int ability = abilities[i];
        ability_type* entry = &b_info[ability_index(S_SMT, ability)];
        assert(insight_ability_upgrade_cost(S_SMT, ability) == 1);
        bool staged = ability == SMT_ARTEFACT;
        if (staged)
        {
            p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = true;
            p_ptr->have_ability[S_SMT][SMT_ENCHANTMENT] = true;
            p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
            p_ptr->insight_points = 1;
        }
        assert(ability_purchase_insight_cost(S_SMT, ability) == (staged ? 1 : 0));
        assert(!insight_upgrade_ability(S_SMT, ability));
        /* XP buys the original ability even with no IP. */
        assert(ability_browser_activate_choice(S_SMT, ability));
        assert(!p_ptr->insight_points && !p_ptr->insight_ability_upgraded[S_SMT][ability]);
        assert(!smithing_mastery_stat_bonus_scaled(ability));
        assert(smithing_effective_skill(&heavy) == heavy_base + bonus);
        s32b xp = p_ptr->new_exp;
        assert(!ability_browser_upgrade_choice(S_SMT, ability));
        p_ptr->insight_points = 1;
        accept_purchase = false;
        assert(!ability_browser_upgrade_choice(S_SMT, ability));
        assert(p_ptr->insight_points == 1 && p_ptr->new_exp == xp);
        assert(!p_ptr->insight_ability_upgraded[S_SMT][ability]);
        for (int width = 40; width <= 80; width += 40)
        {
            Term_resize(width, 24);
            ability_browser_layout layout;
            ability_browser_init_layout(&layout, 10, "Smithing");
            ui_menu_click_begin();
            ability_browser_draw_prompt(&layout, entry);
            char line[100] = {0};
            for (int col = 0; col < width; ++col)
            {
                byte attr; Term_what(col, layout.prompt_row, &attr, &line[col]);
            }
            assert(strstr(line, "* upgrade") && strstr(line, "toggle") && strstr(line, "train"));
            int col = (int)(strstr(line, "upgrade") - line);
            int choice, action;
            assert(ui_menu_click_handle_cell(col, layout.prompt_row));
            assert(ui_menu_click_take_action(&choice, &action));
            assert(choice == ABILITY_MENU_CLICK_UPGRADE);
        }
        Term_resize(80, 24);
        accept_purchase = true;
        assert(ability_browser_upgrade_choice(S_SMT, ability));
        assert(strstr(confirmation, "Smithing stat bonus for 1 IP"));
        assert(!p_ptr->insight_points && p_ptr->new_exp == xp);
        int stat = ability == SMT_EXPERTISE ? A_DEX : A_GRA;
        int contribution = smithing_effective_stat(stat);
        bonus += contribution;
        assert(smithing_mastery_stat_bonus_scaled(ability) == 100 * contribution);
        assert(smithing_effective_skill(&heavy) == heavy_base + bonus);
        assert(smithing_effective_skill(&ring) == ring_base + bonus);
        p_ptr->insight_points = 1;
        assert(!ability_browser_upgrade_choice(S_SMT, ability));
        assert(p_ptr->insight_points == 1);
        assert(ability_browser_activate_choice(S_SMT, ability));
        assert(!smithing_mastery_stat_bonus_scaled(ability));
        assert(ability_browser_activate_choice(S_SMT, ability));
        assert(smithing_mastery_stat_bonus_scaled(ability) == 100 * contribution);
        p_ptr->insight_points = 0;
    }
    /* Equipment grants do not qualify as a learned ability. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->have_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
    assert(!ability_browser_upgrade_choice(S_SMT, SMT_ENCHANTMENT));
    assert(!insight_upgrade_ability(S_SMT, SMT_ENCHANTMENT));
    assert(!smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT));
    op_ptr->opt[OPT_insight_beta] = false;
    assert(!insight_upgrade_ability(S_SMT, SMT_ENCHANTMENT));
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT) == 300);
    assert(!insight_upgrade_ability(-1, 0) && !insight_upgrade_ability(S_MAX, 0));
    /* Exercise the new keyboard action through the actual browser loop. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->have_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->insight_points = 1; accept_purchase = true;
    ability_browser_entry entries[ABILITIES_MAX];
    int count = ability_browser_collect_entries(S_SMT, entries, ABILITIES_MAX);
    int index = 0;
    while (index < count && entries[index].abilitynum != SMT_ENCHANTMENT) ++index;
    assert(index < count);
    char sequence[100];
    memset(sequence, ']', S_SMT);
    memset(sequence + S_SMT, '2', index);
    SDL_strlcpy(sequence + S_SMT + index, "*\033", sizeof(sequence) - S_SMT - index);
    Term_flush(); input_sequence = sequence;
    do_cmd_ability_screen();
    input_sequence = NULL;
    assert(!p_ptr->insight_points && p_ptr->insight_ability_upgraded[S_SMT][SMT_ENCHANTMENT]);
    assert(p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]);
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT) == 300);
    op_ptr->opt[OPT_insight_beta] = false;
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT) == 300);
    op_ptr->opt[OPT_insight_beta] = true;
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT) == 300);
    puts("Smithing IP upgrades: separate XP purchases, atomic 1 IP powers, toggle, footer click targets and beta-off behavior PASS.");
}

static void fixture_learn(int skill, int ability)
{
    p_ptr->innate_ability[skill][ability] = true;
    p_ptr->have_ability[skill][ability] = true;
    p_ptr->active_ability[skill][ability] = true;
}

static void check_ability_stages(void)
{
    const int pairs[][4] = {
        {S_MEL, MEL_CONTROL, S_MEL, MEL_FINESSE},
        {S_MEL, MEL_WHIRLWIND_ATTACK, S_MEL, MEL_FOLLOW_THROUGH},
        {S_MEL, MEL_POWER_THROW, S_MEL, MEL_THROWING},
        {S_STL, STL_CRUEL_BLOW, S_STL, STL_ASSASSINATION},
        {S_PER, PER_LISTEN, S_PER, PER_KEEN_SENSES},
        {S_WIL, WIL_MAJESTY, S_WIL, WIL_FORMIDABLE},
        {S_EVN, EVN_RIPOSTE, S_EVN, EVN_PARRY},
        {S_EVN, EVN_FLANKING, S_EVN, EVN_DODGING},
        {S_ARC, ARC_SKIRMISHING, S_EVN, EVN_DODGING},
        {S_STL, STL_VANISH, S_STL, STL_DISGUISE},
        {S_ARC, ARC_CRIPPLING, S_ARC, ARC_PUNCTURE},
        {S_PER, PER_CONCENTRATION, S_PER, PER_FOCUSED_ATTACK},
        {S_WIL, WIL_CHANNELING, S_PER, PER_ALCHEMY},
        {S_SMT, SMT_ARTEFACT, S_SMT, SMT_ENCHANTMENT},
        {S_SNG, SNG_REVEALING, S_SNG, SNG_DELVINGS},
        {S_SNG, SNG_DISGUISE, S_SNG, SNG_SILENCE},
        {S_SNG, SNG_SHATTERING, S_SNG, SNG_FREEDOM}
    };
    assert(!ability_stages_validate());
    for (size_t i = 0; i < N_ELEMENTS(pairs); ++i)
    {
        int skill = pairs[i][0], num = pairs[i][1];
        int parent_skill = pairs[i][2], parent = pairs[i][3];
        ability_type* stage = &b_info[ability_index(skill, num)];
        reset_player();
        assert(!ability_is_stage(stage) && !ability_purchase_insight_cost(skill, num));
        assert(ability_purchase_xp(stage) == ability_purchase_exp_cost(skill));
        op_ptr->opt[OPT_insight_beta] = true;
        assert(ability_is_stage(stage) && ability_stage_depth(stage) == 1);
        assert(ability_stage_parent(stage)->skilltype == parent_skill);
        assert(ability_stage_parent(stage)->abilitynum == parent);
        assert(ability_purchase_insight_cost(skill, num) == 1 && !ability_purchase_xp(stage));
        p_ptr->skill_base[skill] = stage->level;
        for (int stat = 0; stat < A_MAX; ++stat) p_ptr->stat_base[stat] = 5;
        fixture_learn(S_PER, PER_QUICK_STUDY);
        accept_purchase = true;
        assert(!prereqs(skill, num));
        assert(!ability_browser_activate_choice(skill, num));
        p_ptr->have_ability[parent_skill][parent] = p_ptr->active_ability[parent_skill][parent] = true;
        assert(!ability_browser_activate_choice(skill, num)); /* Equipment cannot skip learning. */
        fixture_learn(parent_skill, parent);
        assert(prereqs(skill, num));
        p_ptr->insight_points = 0;
        assert(!ability_browser_activate_choice(skill, num));
        p_ptr->insight_points = 1; accept_purchase = false;
        int base_count = abilities_in_skill(skill), xp_price = ability_purchase_exp_cost(skill);
        s32b xp = p_ptr->new_exp;
        assert(!ability_browser_activate_choice(skill, num));
        assert(p_ptr->insight_points == 1 && p_ptr->new_exp == xp);
        accept_purchase = true;
        assert(ability_browser_activate_choice(skill, num));
        assert(!p_ptr->insight_points && p_ptr->new_exp == xp);
        assert(p_ptr->innate_ability[skill][num]
            && p_ptr->active_ability[parent_skill][parent]);
        assert(abilities_in_skill(skill) == base_count && ability_purchase_exp_cost(skill) == xp_price);
        assert(strstr(confirmation, "Upgrade") && strstr(confirmation, "1 IP"));
        op_ptr->opt[OPT_insight_beta] = false;
        assert(abilities_in_skill(skill) == base_count + 1 + (skill == S_PER ? 1 : 0));
    }
    /* Smite accepts either of its two authored upgrade parents. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true; accept_purchase = true;
    ability_type* smite = &b_info[ability_index(S_MEL, MEL_SMITE)];
    assert(ability_stage_parent_count(smite) == 2);
    assert(ability_stage_parent_at(smite, 0)->abilitynum == MEL_POWER);
    assert(ability_stage_parent_at(smite, 1)->abilitynum == MEL_IMPALE);
    p_ptr->stat_base[A_STR] = 3; p_ptr->skill_base[S_MEL] = 11;
    p_ptr->insight_points = 1; p_ptr->new_exp = 100000;
    assert(!ability_browser_activate_choice(S_MEL, MEL_SMITE));
    fixture_learn(S_MEL, MEL_POWER);
    assert(ability_stage_parent_path_met(smite, 0));
    assert(ability_browser_activate_choice(S_MEL, MEL_SMITE));
    assert(!p_ptr->insight_points && p_ptr->innate_ability[S_MEL][MEL_SMITE]);
    reset_player(); op_ptr->opt[OPT_insight_beta] = true; accept_purchase = true;
    p_ptr->stat_base[A_STR] = 3; p_ptr->skill_base[S_MEL] = 11;
    p_ptr->insight_points = 1; p_ptr->new_exp = 100000;
    fixture_learn(S_MEL, MEL_IMPALE);
    assert(ability_stage_parent_path_met(smite, 1));
    assert(ability_browser_activate_choice(S_MEL, MEL_SMITE));
    assert(!p_ptr->insight_points && p_ptr->innate_ability[S_MEL][MEL_SMITE]);

    /* The menu repeats a multi-parent stage beneath each authored path, so
     * Impale visibly exposes Smite as well as Power. */
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    ability_browser_entry display_entries[ABILITY_BROWSER_ENTRIES_MAX];
    int display_count = ability_browser_collect_entries(S_MEL,
        display_entries, ABILITY_BROWSER_ENTRIES_MAX);
    int smite_display_count = 0;
    bool saw_power = false;
    bool saw_impale = false;
    for (int i = 0; i < display_count; ++i)
    {
        if (display_entries[i].b_ptr->abilitynum == MEL_POWER) saw_power = true;
        if (display_entries[i].b_ptr->abilitynum == MEL_IMPALE) saw_impale = true;
        if (display_entries[i].b_ptr->abilitynum == MEL_SMITE)
        {
            smite_display_count++;
            assert(smite_display_count == 1 ? saw_power : saw_impale);
        }
    }
    assert(smite_display_count == 2);

    /* A third stage inherits every earlier stage and can share an exclusive
     * fork with another child. */
    ability_type* third = &b_info[ability_index(S_MEL, MEL_WARDEN)];
    ability_type* choice = &b_info[ability_index(S_MEL, MEL_RAPID_ATTACK)];
    ability_type saved = *third;
    ability_type saved_choice = *choice;
    third->stage_parent_count = 1;
    third->stage_parent_skill[0] = S_MEL;
    third->stage_parent_ability[0] = MEL_WHIRLWIND_ATTACK;
    third->stage_cost = 1; third->stage_choice_group = 1;
    choice->stage_parent_count = 1;
    choice->stage_parent_skill[0] = S_MEL;
    choice->stage_parent_ability[0] = MEL_WHIRLWIND_ATTACK;
    choice->stage_cost = 1; choice->stage_choice_group = 1;
    assert(!ability_stages_validate());
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    assert(ability_stage_depth(third) == 2);
    fixture_learn(S_MEL, MEL_WHIRLWIND_ATTACK);
    assert(ability_stage_missing_parent(third)->abilitynum == MEL_FOLLOW_THROUGH);
    fixture_learn(S_MEL, MEL_FOLLOW_THROUGH);
    fixture_learn(S_MEL, MEL_RAPID_ATTACK);
    assert(ability_stage_conflict(third)->abilitynum == MEL_RAPID_ATTACK);
    p_ptr->innate_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->have_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->skill_base[S_MEL] = 13; p_ptr->insight_points = 1; accept_purchase = true;
    assert(ability_browser_activate_choice(S_MEL, MEL_WARDEN));
    assert(!p_ptr->insight_points);
    ability_browser_entry entries[ABILITIES_MAX];
    int count = ability_browser_collect_entries(S_MEL, entries, ABILITIES_MAX);
    int found = -1;
    for (int i = 0; i < count; ++i) if (entries[i].abilitynum == MEL_WARDEN) found = i;
    assert(found > 1 && entries[found].stage_depth == 2);
    assert(entries[found - 1].abilitynum == MEL_WHIRLWIND_ATTACK);
    assert(entries[found - 2].abilitynum == MEL_FOLLOW_THROUGH);
    ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
    int n = ability_browser_build_description(S_MEL, &entries[found], lines, 90);
    assert(description_has(lines, n, "Stage 3 upgrade"));
    assert(description_has(lines, n, "Follow-Through > Whirlwind Attack > Warden"));
    third->stage_parent_ability[0] = MEL_WARDEN;
    assert(ability_stages_validate()); /* Cycle, rather than unbounded recursion. */
    third->stage_parent_ability[0] = ABILITIES_MAX - 1;
    assert(ability_stages_validate()); /* Missing parent. */
    *third = saved;
    *choice = saved_choice;
    assert(!ability_stages_validate());
    reset_player();
    puts("Insight stages: authored mappings, learned ancestors, IP-only upgrades, retained gates, exclusive paths, equipment/legacy normalization and future chains PASS.");
}

static void check_stage_ui(cptr output)
{
    reset_player(); op_ptr->opt[OPT_insight_beta] = true;
    fixture_learn(S_MEL, MEL_FOLLOW_THROUGH);
    p_ptr->skill_base[S_MEL] = 13; p_ptr->insight_points = 2;
    ability_browser_entry entries[ABILITIES_MAX];
    int count = ability_browser_collect_entries(S_MEL, entries, ABILITIES_MAX);
    int follow = -1;
    for (int i = 0; i < count; ++i) if (entries[i].abilitynum == MEL_FOLLOW_THROUGH) follow = i;
    assert(follow >= 0 && entries[follow + 1].abilitynum == MEL_WHIRLWIND_ATTACK);
    for (int width = 40; width <= 100; width += 20)
    {
        Term_resize(width, 30);
        ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
        int n = ability_browser_build_description(S_MEL, &entries[follow], lines, width - 4);
        assert(description_has(lines, n, "Next upgrades"));
        assert(description_has(lines, n, "Whirlwind Attack - 1 IP"));
        n = ability_browser_build_description(S_MEL, &entries[follow + 1], lines, width - 4);
        assert(description_has(lines, n, "Stage 2 upgrade"));
        assert(description_has(lines, n, "Previous stage: Follow-Through"));
        for (int i = 0; i < n; ++i) assert(menu_text_display_width(lines[i].text) <= width - 4);
        char sequence[100], path[1024];
        memset(sequence, '2', follow + 1);
        SDL_strlcpy(sequence + follow + 1, "\033", sizeof(sequence) - follow - 1);
        strnfmt(path, sizeof(path), "%s/stages-%d.txt", output, width);
        capture_path = path; Term_flush(); input_sequence = sequence;
        do_cmd_ability_screen();
        capture_path = input_sequence = NULL;
    }
    op_ptr->opt[OPT_insight_beta] = false;
    count = ability_browser_collect_entries(S_MEL, entries, ABILITIES_MAX);
    for (int i = 0; i < count; ++i) assert(!entries[i].stage_depth);
    Term_resize(80, 24);
    puts("Stage UI: parent-first ordering, 40/60/80/100-column browser captures, prices, requirements and Insight-off layout PASS.");
}

int main(int argc, char** argv)
{
    assert(argc == 4); setbuf(stdout, NULL); log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    assert(term_init(&fixture_term, 80, 24, 256) == 0);
    fixture_term.xtra_hook = terminal_extra;
    angband_term[0] = &fixture_term; Term_activate(&fixture_term);
    assert(!init_z_info()); assert(!init_f_info()); assert(!init_k_info());
    assert(!init_b_info()); assert(!init_a_info()); assert(!init_e_info());
    assert(!init_r_info()); assert(!init_v_info()); assert(!init_p_info());
    assert(!init_c_info()); assert(!init_oath_info()); assert(!init_cu_info());
    assert(!init_mb_info()); assert(!init_style_info()); assert(!init_other());
    assert(!init_alloc());
    rp_ptr = &p_info[5]; current_character_profile = &c_info[30];
    p_ptr->prace = 5; p_ptr->pcharacter = 30;
    p_ptr->py = p_ptr->px = 2; cave_feat[2][2] = FEAT_FLOOR;
    check_parser(); check_selected_requirements(); check_purchase();
    check_display_and_existing_gates();
    check_smithing_insight_upgrades();
    check_ability_stages();
    check_stage_ui(argv[3]);
    check_insight_points();
    puts("Ability requirements integration: PASS.");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(
        ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects))
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data, str(OUT)],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
