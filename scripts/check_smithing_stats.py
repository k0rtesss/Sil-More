#!/usr/bin/env python3
"""Check smithing scaling, parsed ability gates and real forge cost paths.

Uses current Windows engine objects and temporary template caches only.
Run build-incremental.ps1 first. Does not open or modify player saves.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/smithing-stats"

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
extern void fixture_old_extra(void);
size_t fixture_write_player(byte* buffer, size_t capacity, bool old)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    if (old) fixture_old_extra(); else save_write_extra();
    save_wr_u32b(0xA12345FEU);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
int fixture_read_player(const byte* buffer, size_t length, int extra)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    /* FEATURE_FLAGS */
    savefile_has_varda_quest = true;
    int result = load_read_extra();
    u32b sentinel = 0;
    if (!result) load_rd_u32b(&sentinel);
    assert(!result && sentinel == 0xA12345FEU);
    assert((size_t)SDL_TellIO(fff) == length);
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
'''

SDL_UI = r'''
#include "sdl/ui/sdl-screens.c"
#include "player/player-upkeep-internal.h"
#include <assert.h>
void check_sdl_skill_display(cptr output, cptr fonts)
{
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    for (int stat = 0; stat < A_MAX; ++stat) p_ptr->stat_base[stat] = 20;
    p_ptr->skill_base[S_SMT] = 0;
    calc_bonuses();
    assert(p_ptr->skill_stat_mod[S_SMT] == 40);
    sdl_char_sheet_line lines[S_MAX];
    int count = sdl_char_sheet_collect_skills(lines, S_MAX, false);
    bool found = false;
    for (int i = 0; i < count; ++i)
        if (strstr(lines[i].text, "Smithing\t"))
        {
            assert(strstr(lines[i].text, "= 0 +40")); found = true;
        }
    assert(found);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO));
    assert(TTF_Init());
    SDL_Surface* canvas = SDL_CreateSurface(820, 570, SDL_PIXELFORMAT_RGBA8888);
    assert(canvas);
    g_state.renderer = SDL_CreateSoftwareRenderer(canvas); assert(g_state.renderer);
    for (size_t i = 0; i < N_ELEMENTS(g_state.palette); ++i)
        g_state.palette[i] = (SDL_Color){angband_color_table[i][1],
            angband_color_table[i][2], angband_color_table[i][3], 255};
    char path[1024]; strnfmt(path, sizeof(path), "%s/MarcellusSC-Regular.ttf", fonts);
    TTF_Font* font = TTF_OpenFont(path, 36); assert(font);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255); SDL_RenderClear(g_state.renderer);
    sdl_char_sheet_draw_lines(font, "Skills", lines, count, 24, 20, 770, 530, 55, 0.47f);
    SDL_RenderPresent(g_state.renderer);
    strnfmt(path, sizeof(path), "%s/character-skills.png", output);
    assert(IMG_SavePNG(canvas, path));
    sdl_ui_text_cache_clear();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
    SDL_DestroySurface(canvas); TTF_Quit();
    puts("Screenshot scenario: actual SDL skill collector and renderer show common +40 with zero ranks PASS.");
}
'''

BIRTH = r'''
#include "birth/birth-blitz.c"
#include <assert.h>
/* ORIGINAL_FOUR_STAT_ALLOCATOR */
void check_birth_allocation(void)
{
    bool saw_lore = false;
    for (int seed = 1; seed <= 100; ++seed)
    {
        int current[BIRTH_STAT_MAX], previous[A_MAX];
        op_ptr->opt[OPT_lore_beta] = false;
        Rand_state_import(seed);
        blitz_auto_assign_stats(current);
        Rand_state_import(seed);
        original_four_stat_allocator(previous);
        assert(!memcmp(current, previous, sizeof(previous)));
        assert(current[BIRTH_STAT_LORE] == 0);
        op_ptr->opt[OPT_lore_beta] = true;
        Rand_state_import(seed);
        blitz_auto_assign_stats(current);
        int cost = 0;
        for (int i = 0; i < BIRTH_STAT_MAX; ++i)
        {
            assert(current[i] >= 0 && current[i] <= 6);
            cost += birth_stat_current_cost(current[i]);
        }
        assert(cost <= MAX_COST);
        saw_lore |= current[BIRTH_STAT_LORE] > 0;
    }
    assert(saw_lore);
    op_ptr->opt[OPT_lore_beta] = false;
    puts("Birth allocation: 100 seeds identical to original with beta off; fifth stat charged within budget when on PASS.");
}
'''

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
#include <assert.h>
/* ORIGINAL_DIFFICULTY */
extern char fixture_input_key;

static bool report_has(const smith_calculation_report* report, cptr needle)
{
    for (int i = 0; i < report->count; ++i)
        if (strstr(report->text[i], needle)) return true;
    return false;
}

void check_calculation_display(cptr output)
{
    u64b saved_flags = c_info[p_ptr->pcharacter].flags_u;
    int checked = 0;
    for (int profile = 0; profile < 4; ++profile)
    {
        c_info[p_ptr->pcharacter].flags_u = profile == 1 ? UNQ_SMT_FEANOR
            : profile == 2 ? UNQ_SMT_TELCHAR : profile == 3 ? UNQ_SMT_CELEBRIMBOR : 0;
        for (int kind = 1; kind < z_info->k_max; ++kind)
        {
            if (!k_info[kind].name || !k_info[kind].weight) continue;
            object_type object;
            object_prep(&object, kind);
            bool craftable_type = false;
            for (int i = 0; i < MAX_SMITHING_TVALS; ++i)
                if (object.tval == smithing_tvals[i].tval) craftable_type = true;
            if (!craftable_type) continue;
            for (int variation = 0; variation < 3; ++variation)
            {
                *smith_o_ptr = object;
                smith_o_ptr->att += variation - 1;
                smith_o_ptr->evn += variation;
                smith_o_ptr->stat_bonus[A_DEX] = variation;
                smith_o_ptr->weight += variation * 3;
                smith_difficulty_breakdown breakdown;
                int actual = smith_object_difficulty(smith_o_ptr, &breakdown);
                assert(actual == original_object_difficulty(smith_o_ptr));
                int sum = 0;
                for (int i = 0; i < SMITH_DIF_PARTS; ++i) sum += breakdown.parts[i];
                assert(sum == breakdown.subtotal);
                assert(breakdown.multiplier == 100 + breakdown.character_percent
                    + breakdown.slot_percent + breakdown.enchantable_percent);
                assert(breakdown.scaled == breakdown.subtotal * breakdown.multiplier / 100);
                assert(breakdown.total == actual);
                ++checked;
            }
        }
    }
    c_info[p_ptr->pcharacter].flags_u = saved_flags;
    printf("Difficulty accounting: %d designs match the original engine calculation exactly PASS.\n", checked);
    object_prep(smith_o_ptr, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    smith_clear_alloy_state(&smith_alloy);
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    p_ptr->stat_base[A_STR] = p_ptr->stat_base[A_DEX] = p_ptr->stat_base[A_GRA] = 3;
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->skill_equip_mod[S_SMT] = p_ptr->skill_misc_mod[S_SMT] = 0;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_FLOOR;
    object_type saved = *smith_o_ptr;
    smithing_cost_type cost = smithing_cost;
    smith_calculation_report report;
    for (int width = 28; width <= 76; width += 24)
    {
        smith_build_calculation_report(&report, width, NULL);
        assert(report_has(&report, "Common foundation"));
        assert(report_has(&report, "ITEM DIFFICULTY"));
        assert(report_has(&report, "CAPACITY AND SACRIFICE"));
        assert(report.count < SMITH_REPORT_MAX_LINES);
        for (int i = 0; i < report.count; ++i)
            assert(menu_text_display_width(report.text[i]) <= width);
    }
    assert(!memcmp(&saved, smith_o_ptr, sizeof(saved)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    char path[1024]; strnfmt(path, sizeof(path), "%s/calculation-report.txt", output);
    FILE* file = fopen(path, "w"); assert(file);
    for (int i = 0; i < report.count; ++i) fprintf(file, "%s\n", report.text[i]);
    fclose(file);
    /* Real modal route: keyboard entry/exit restores the forge and its state. */
    Term_flush(); fixture_input_key = ESCAPE;
    assert(smithing_menu_key('?', NULL) == 0);
    fixture_input_key = ' ';
    assert(!memcmp(&saved, smith_o_ptr, sizeof(saved)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    /* Inspect the actual root/menu terminal output at desktop and compact widths. */
    for (int width = 64; width <= 80; width += 16)
    {
        Term_resize(width, 30);
        bool valid[SMT_MENU_MAX]; byte attrs[SMT_MENU_MAX]; char labels[SMT_MENU_MAX][32];
        ui_menu_click_begin(); smith_root_build_entries(valid, attrs, labels);
        smith_root_draw(1, valid, attrs, labels); prt_object_difficulty();
        char measure[180];
        for (int y = 2; y <= 3; ++y)
        {
            int n = 0;
            for (int x = smith_ui_cost_col(); x < width; ++x)
            { byte a; char c; Term_what(x,y,&a,&c); measure[n++] = c; }
            measure[n] = '\0';
            assert(strstr(measure, y == 2 ? "Difficulty 2" : "Capacity 21"));
        }
        strnfmt(path, sizeof(path), "%s/forge-%d.txt", output, width);
        file = fopen(path, "w"); assert(file);
        for (int y = 0; y < 30; ++y)
        {
            for (int x = 0; x < width; ++x) { byte a; char c; Term_what(x,y,&a,&c); fputc(c,file); }
            fputc('\n',file);
        }
        fclose(file);
    }
    Term_resize(80,24);
    puts("Calculation view: narrow wrapping, actual modal navigation, state preservation and forge layouts PASS.");
}

void check_forge_costs(void)
{
    int drain;
    object_prep(smith_o_ptr, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = true;
    p_ptr->active_ability[S_SMT][SMT_MASTERPIECE] = true;
    p_ptr->skill_base[S_SMT] = 4;
    /* Raise numeric quality until this real design exceeds normal capacity. */
    int diff = object_difficulty(smith_o_ptr);
    while (diff <= smithing_effective_skill(smith_o_ptr) + 1)
    {
        smith_o_ptr->att++;
        diff = object_difficulty(smith_o_ptr);
    }
    assert(smithing_cost.drain == diff - smithing_effective_skill(smith_o_ptr));
    assert(smithing_cost.str == 0 && smithing_cost.dex == 0
        && smithing_cost.gra == 0 && smithing_cost.exp == 0);
    p_ptr->have_ability[S_SPC][SPC_AULE] = true;
    diff = object_difficulty(smith_o_ptr);
    int excess = diff - smithing_effective_skill(smith_o_ptr);
    assert(excess > 0 && excess <= 2 * p_ptr->skill_base[S_SMT]);
    assert(smithing_cost.drain == (excess + 1) / 2);
    assert(!too_difficult(smith_o_ptr));
    puts("Real forge costs: Expertise retains Masterpiece/Aule rank sacrifice PASS.");

    p_ptr->have_ability[S_SPC][SPC_AULE] = false;
    p_ptr->active_ability[S_SMT][SMT_MASTERPIECE] = false;
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = false;
    p_ptr->stat_base[A_DEX] = 4; p_ptr->stat_base[A_GRA] = 2;
    object_type mail, ring;
    object_prep(&mail, lookup_kind(TV_MAIL, SV_MAIL_CORSLET));
    object_prep(&ring, lookup_kind(TV_RING, 0));
    assert(mail.k_idx && ring.k_idx);
    assert(smithing_effective_skill(&mail) == 14);
    assert(smithing_effective_skill(&ring) == 12);
    assert(smith_reforge_difficulty_affordable(&mail, 14, &drain) && drain == 0);
    assert(!smith_reforge_difficulty_affordable(&ring, 14, &drain));
    p_ptr->active_ability[S_SMT][SMT_MASTERPIECE] = true;
    assert(smith_reforge_difficulty_affordable(&ring, 14, &drain) && drain == 2);
    p_ptr->have_ability[S_SPC][SPC_AULE] = true;
    assert(smith_reforge_difficulty_affordable(&ring, 15, &drain) && drain == 2);
    assert(!smith_reforge_difficulty_affordable(&ring, 21, &drain));
    puts("Real reforging capacity: target category, overcap limits and round-up cost PASS.");

    p_ptr->active_ability[S_SMT][SMT_REPAIR] = true;
    p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH] = true;
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = true;
    p_ptr->skill_base[S_SMT] = 30;
    p_ptr->new_exp = 100000;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_FORGE_NORMAL_TAIL;
    int prefix = 0;
    for (int i = 1; i < z_info->e_max; ++i)
        if (ego_prefix_can_apply_to_object(&mail, i)) { prefix = i; break; }
    assert(prefix);
    reforge_preview_type preview;
    assert(reforge_preview_build(&mail, prefix, &preview));
    assert(!preview.affordable && preview.cost.enchantment);
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
    assert(reforge_preview_build(&mail, prefix, &preview));
    assert(preview.affordable && !preview.cost.enchantment);
    assert(preview.scaled_difficulty == (preview.raw_delta_difficulty * 3 + 1) / 2);
    smith_calculation_report reforge_report;
    object_type target_snapshot = *smith_o_ptr;
    smith_build_calculation_report(&reforge_report, 76, &mail);
    assert(report_has(&reforge_report, "ORIGINAL ITEM DIFFICULTY"));
    assert(report_has(&reforge_report, "REFORGED ITEM DIFFICULTY"));
    assert(report_has(&reforge_report, "Reforge: ceil(1.5 x max(0,"));
    assert(!memcmp(&target_snapshot, smith_o_ptr, sizeof(target_snapshot)));
    object_prep(&mail, lookup_kind(TV_MAIL, SV_MITHRIL_CORSLET));
    assert(reforge_preview_build(&mail, prefix, &preview));
    assert(!preview.affordable && preview.cost.alloy_mastery);
    puts("Real prefix preview: 1.5x difficulty, Enchantment and material permissions survive overcap PASS.");
}
'''

ABILITIES = r'''
#include "cmd/ui/cmd-ui-abilities.c"
#include <assert.h>
void check_learning(void)
{
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->stat_base[A_DEX] = 4; p_ptr->stat_base[A_GRA] = 4;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = true;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    p_ptr->innate_ability[S_SMT][SMT_WEAPONSMITH] = true;
    assert(prereqs(S_SMT, SMT_ARTEFACT));
    assert(!prereqs(S_SMT, SMT_MASTERPIECE));
    p_ptr->stat_base[A_GRA] = 5;
    assert(prereqs(S_SMT, SMT_MASTERPIECE));
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = true;
    p_ptr->stat_base[A_DEX] = 3;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    assert(!prereqs(S_SMT, SMT_MASTERPIECE));
    p_ptr->stat_use[A_DEX] = p_ptr->stat_use[A_GRA] = 20;
    p_ptr->stat_equip_mod[A_DEX] = p_ptr->stat_misc_mod[A_DEX] = 10;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    p_ptr->new_exp = 100000;
    p_ptr->skill_base[S_SMT] = 0;
    assert(!ability_browser_activate_choice(S_SMT, SMT_ARTEFACT));
    assert(p_ptr->new_exp == 100000 && p_ptr->skill_base[S_SMT] == 0);
    assert(!p_ptr->innate_ability[S_SMT][SMT_ARTEFACT]);
    p_ptr->stat_base[A_DEX] = 4; p_ptr->stat_base[A_GRA] = 3;
    p_ptr->skill_base[S_SMT] = 12;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    p_ptr->stat_base[A_GRA] = 5;
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    assert(prereqs(S_SMT, SMT_ARTEFACT)); /* Quick Study bypasses only tree. */
    p_ptr->skill_base[S_SMT] = 0;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    puts("Parsed AND/OR prerequisites, permanent stat gates, Quick Study and atomic failure PASS.");

    assert(!lore_system_enabled());
    p_ptr->lore = 0;
    p_ptr->skill_base[S_PER] = 10;
    p_ptr->innate_ability[S_PER][PER_QUICK_STUDY] = true;
    assert(!ability_uses_knowledge_points(S_PER, PER_QUICK_STUDY));
    assert(abilities_in_skill(S_PER) == 1);
    assert(prereqs(S_PER, PER_QUICK_STUDY));
    op_ptr->opt[OPT_lore_beta] = true;
    assert(ability_uses_knowledge_points(S_PER, PER_QUICK_STUDY));
    assert(!ability_uses_knowledge_points(S_SMT, SMT_EXPERTISE));
    assert(b_info[ability_index(S_SMT, SMT_EXPERTISE)].lore_req == 0);
    assert(abilities_in_skill(S_PER) == 0);
    assert(!prereqs(S_PER, PER_QUICK_STUDY));
    p_ptr->lore = 2;
    assert(prereqs(S_PER, PER_QUICK_STUDY));
    assert(ability_purchase_knowledge_cost(S_PER, PER_QUICK_STUDY) == 0);
    /* As on develop, Lore discounts K: costs; missing base skill still costs XP. */
    p_ptr->knowledge_points = 77; p_ptr->new_exp = 99;
    p_ptr->skill_base[S_PER] = 0;
    p_ptr->innate_ability[S_PER][PER_QUICK_STUDY] = false;
    p_ptr->have_ability[S_PER][PER_QUICK_STUDY] = false;
    assert(!ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(p_ptr->knowledge_points == 77 && p_ptr->new_exp == 99);
    assert(p_ptr->skill_base[S_PER] == 0);
    op_ptr->opt[OPT_lore_beta] = false;
    gain_knowledge_points(5, NULL);
    assert(p_ptr->knowledge_points == 77 && p_ptr->lore == 2);
    op_ptr->opt[OPT_lore_beta] = true;
    gain_knowledge_points(5, NULL);
    assert(p_ptr->knowledge_points == 82);
    p_ptr->knowledge_points = PY_MAX_EXP - 1;
    gain_knowledge_points(0x7fffffff, NULL);
    assert(p_ptr->knowledge_points == PY_MAX_EXP);
    op_ptr->opt[OPT_lore_beta] = false;
    puts("Lore beta: default off, gated requirements/currency, XP auto-training and dormant values PASS.");
}
'''

MAIN = r'''
#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "sdl-config.h"
#include "player/player-upkeep-internal.h"
#include "init/init2-internal.h"
#include <assert.h>
extern void check_forge_costs(void);
extern void check_learning(void);
extern void check_birth_allocation(void);
extern void check_calculation_display(cptr output);
extern void check_sdl_skill_display(cptr output, cptr fonts);
extern size_t fixture_write_player(byte* buffer, size_t capacity, bool old);
extern int fixture_read_player(const byte* buffer, size_t length, int extra);
static term test_term;
char fixture_input_key = ' ';
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) Term_keypress(fixture_input_key);
    return 0;
}
static errr parse_line(header* h, cptr line)
{
    char buf[256]; SDL_strlcpy(buf, line, sizeof(buf));
    return parse_b_info(buf, h);
}
static void check_coefficients(void)
{
    ability_type entries[2] = {0};
    header h = {0};
    h.info_num = 2; h.info_ptr = entries;
    h.name_ptr = calloc(z_info->fake_name_size, 1);
    assert(h.name_ptr);
    error_idx = -1;
    assert(parse_line(&h, "N:1:Coefficient test") == 0);
    assert(parse_line(&h, "I:6:4:6") == 0);
    assert(parse_line(&h, "R:DEX:2:GRA:4:LORE:5") == 0);
    assert(entries[1].stat_req[A_DEX] == 2 && entries[1].stat_req[A_GRA] == 4);
    assert(entries[1].lore_req == 5);
    assert(parse_line(&h, "S:STR:0.5:DEX:3/2:GRA:100%:SMITHING:2x") == 0);
    assert(entries[1].stat_score_weight[A_STR] == 50);
    assert(entries[1].stat_score_weight[A_DEX] == 150);
    assert(entries[1].stat_score_weight[A_GRA] == 100);
    assert(entries[1].skill_score_weight[S_SMT] == 200);
    assert(parse_line(&h, "S:DEX:999999999999999999999") != 0);
    assert(parse_line(&h, "S:DEX:1/0") != 0);
    assert(parse_line(&h, "R:DEX:-1") != 0);
    assert(parse_line(&h, "R:UNKNOWN:2") != 0);
    assert(parse_line(&h, "A:9/0") != 0);
    int index = ability_index(S_SMT, SMT_EXPERTISE);
    ability_type saved = b_info[index];
    b_info[index] = entries[1];
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->skill_equip_mod[S_SMT] = 1;
    p_ptr->skill_misc_mod[S_SMT] = 1;
    p_ptr->stat_base[A_STR] = p_ptr->stat_base[A_DEX] = p_ptr->stat_base[A_GRA] = 3;
    assert(ability_score(S_SMT, SMT_EXPERTISE) == 37);
    b_info[index] = saved;
    p_ptr->skill_equip_mod[S_SMT] = p_ptr->skill_misc_mod[S_SMT] = 0;
    free(h.name_ptr);
    puts("Develop coefficient syntax, combined stats/skills, malformed input and actual score PASS.");
}
static void check_player_save(void)
{
    byte* buffer = calloc(1024*1024, 1); assert(buffer);
    p_ptr->lore = 5; p_ptr->knowledge_points = 1234;
    p_ptr->diseased = 0;
    memset(p_ptr->stat_disease, 0, sizeof(p_ptr->stat_disease));
    p_ptr->disease_name = p_ptr->disease_cure = p_ptr->disease_knowledge = 0;
    p_ptr->lamp_oil = 137;
    p_ptr->morgoth_call_state = SAVEFILE_MORGOTH_CALL_SEEN;
    p_ptr->discovery_lore_flags = DISC_LORE_CHASM;
    op_ptr->opt[OPT_lore_beta] = false;
    size_t new_size = fixture_write_player(buffer, 1024*1024, false);
    p_ptr->lore = 0; p_ptr->knowledge_points = 0;
    assert(fixture_read_player(buffer, new_size, 24) == 0);
    assert(p_ptr->lore == 5 && p_ptr->knowledge_points == 1234);
    assert(!lore_system_enabled());
    assert(p_ptr->lamp_oil == 137 && p_ptr->morgoth_call_state == SAVEFILE_MORGOTH_CALL_SEEN);
    assert(p_ptr->discovery_lore_flags == DISC_LORE_CHASM);
    size_t old_size = fixture_write_player(buffer, 1024*1024, true);
    assert(old_size == new_size);
    p_ptr->lore = 19; p_ptr->knowledge_points = 9999;
    assert(fixture_read_player(buffer, old_size, 23) == 0);
    assert(p_ptr->lore == 0 && p_ptr->knowledge_points == 0);
    assert(p_ptr->lamp_oil == 137 && p_ptr->morgoth_call_state == SAVEFILE_MORGOTH_CALL_SEEN);
    assert(p_ptr->discovery_lore_flags == DISC_LORE_CHASM);
    free(buffer);
    puts("Real player save: v24 beta-off roundtrip, original v23 writer load, unchanged length/tail PASS.");
}
static void check_settings(cptr directory)
{
    char filename[1024];
    strnfmt(filename, sizeof(filename), "%s/lore-options.json", directory);
    assert(strcmp(option_text[OPT_lore_beta], "lore_beta") == 0);
    assert(!option_norm[OPT_lore_beta]);
    assert(strcmp(option_text[OPT_active_weapon_switch_confirm], "active_weapon_switch_confirm") == 0);
    assert(strcmp(option_text[OPT_environment_speed], "environment_speed") == 0);
    struct sdl_config local_config;
    sdl_config_set_defaults(&local_config);
    op_ptr->opt[OPT_lore_beta] = true;
    assert(sdl_config_save(filename, &local_config, NULL, 0));
    op_ptr->opt[OPT_lore_beta] = false;
    sdl_config_load_app_options(filename);
    assert(lore_system_enabled());
    FILE* file = fopen(filename, "w"); assert(file);
    fputs("{\"appOptions\":{\"gameplay\":{}}}", file); fclose(file);
    sdl_config_load_app_options(filename);
    assert(!lore_system_enabled());
    puts("Real SDL JSON settings: fixed option slots, beta false default, saved true and missing-key migration PASS.");
}
int main(int argc, char** argv)
{
    assert(argc == 5); setbuf(stdout, NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    assert(term_init(&test_term, 80, 24, 256) == 0);
    test_term.xtra_hook = terminal_extra;
    angband_term[0] = &test_term; Term_activate(&test_term);
    assert(init_z_info()==0); assert(init_f_info()==0);
    assert(init_k_info()==0); assert(init_b_info()==0);
    assert(init_a_info()==0); assert(init_e_info()==0);
    assert(init_r_info()==0); assert(init_v_info()==0);
    assert(init_p_info()==0); assert(init_c_info()==0);
    assert(init_oath_info()==0); assert(init_cu_info()==0); assert(init_mb_info()==0);
    assert(init_style_info()==0); assert(init_other()==0); assert(init_alloc()==0);
    rp_ptr = &p_info[0]; current_character_profile = &c_info[0];
    p_ptr->py = p_ptr->px = 2; cave_feat[2][2] = FEAT_FLOOR;
    check_settings(argv[2]);
    check_birth_allocation();
    check_coefficients();
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->stat_base[A_STR] = p_ptr->stat_base[A_DEX] = p_ptr->stat_base[A_GRA] = 3;
    object_type heavy, mail, ring, bow;
    object_prep(&heavy, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    object_prep(&mail, lookup_kind(TV_MAIL, SV_MAIL_CORSLET));
    object_prep(&ring, lookup_kind(TV_RING, 0));
    object_prep(&bow, lookup_kind(TV_BOW, SV_SHORT_BOW));
    assert(heavy.k_idx && mail.k_idx && ring.k_idx && bow.k_idx);
    assert(smithing_effective_skill(&heavy) == 21);
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = true;
    assert(smithing_effective_skill(&heavy) == 24);
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
    assert(smithing_effective_skill(&heavy) == 27);
    p_ptr->active_ability[S_SMT][SMT_ARTEFACT] = true;
    assert(smithing_effective_skill(&heavy) == 30);
    p_ptr->innate_ability[S_SMT][SMT_EXPERTISE] = true;
    p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->innate_ability[S_SMT][SMT_ARTEFACT] = true;
    p_ptr->innate_ability[S_WIL][WIL_STRENGTH_IN_ADVERSITY] = true;
    p_ptr->active_ability[S_WIL][WIL_STRENGTH_IN_ADVERSITY] = true;
    p_ptr->mhp = p_ptr->chp = 100;
    calc_bonuses();
    int normal_capacity = smithing_effective_skill(&heavy);
    int normal_dex = p_ptr->stat_use[A_DEX];
    p_ptr->chp = 1;
    p_ptr->tmp_str = p_ptr->tmp_dex = p_ptr->tmp_gra = 10;
    calc_bonuses();
    assert(p_ptr->stat_use[A_DEX] > normal_dex);
    assert(smithing_effective_skill(&heavy) == normal_capacity);
    assert(p_ptr->skill_stat_mod[S_SMT] == smithing_common_stat_bonus());
    p_ptr->tmp_str = p_ptr->tmp_dex = p_ptr->tmp_gra = 0;
    p_ptr->chp = p_ptr->mhp;
    p_ptr->skill_equip_mod[S_SMT] = p_ptr->skill_misc_mod[S_SMT] = 0;
    puts("Real calc_bonuses: Adversity/potions leave craft stats unchanged; sheet includes common contribution PASS.");
    for (int i=0; i<A_MAX; ++i) {
        p_ptr->stat_use[i] = 20; p_ptr->stat_equip_mod[i] = 3;
        p_ptr->stat_misc_mod[i] = 3; p_ptr->stat_drain[i] = -3;
    }
    assert(smithing_effective_skill(&heavy) == 30);
    p_ptr->active_ability[S_SMT][SMT_GRA] = true;
    p_ptr->innate_ability[S_SMT][SMT_GRA] = true;
    assert(smithing_effective_skill(&heavy) == 33);
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    p_ptr->stat_base[A_DEX] = 4; p_ptr->stat_base[A_GRA] = 2;
    assert(smithing_stat_bonus(&mail) == 10);
    assert(smithing_stat_bonus(&bow) == 10);
    p_ptr->stat_base[A_DEX] = 2; p_ptr->stat_base[A_GRA] = 4;
    assert(smithing_stat_bonus(&ring) == 10);
    p_ptr->stat_base[A_STR] = -1; p_ptr->stat_base[A_DEX] = 0;
    p_ptr->stat_base[A_GRA] = 0;
    assert(smithing_stat_bonus(&heavy) == -1); /* floor, not truncation */
    p_ptr->stat_base[A_STR] = 1; p_ptr->stat_base[A_DEX] = 1;
    assert(smithing_stat_bonus(&heavy) == 2); /* round once */
    heavy.weight *= 3; heavy.tval = TV_RING; heavy.sval = 0;
    assert(smithing_stat_bonus(&heavy) == 2); /* immutable base kind */
    p_ptr->skill_equip_mod[S_SMT] = 2; p_ptr->skill_misc_mod[S_SMT] = 3;
    assert(smithing_effective_skill(&heavy) == 19);
    p_ptr->skill_equip_mod[S_SMT] = p_ptr->skill_misc_mod[S_SMT] = 0;
    puts("Spec capacities 21/24/27/30, mail/jewellery, permanent Grace, flat bonuses and rounding PASS.");
    check_learning();
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    p_ptr->stat_base[A_STR] = p_ptr->stat_base[A_DEX] = p_ptr->stat_base[A_GRA] = 3;
    check_forge_costs();
    check_player_save();
    check_calculation_display(argv[3]);
    check_sdl_skill_display(argv[3], argv[4]);
    puts("Smithing stats integration: PASS.");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    omit = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj",
            "/src/cmd/ui/cmd-ui-abilities.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
            "/src/birth/birth-blitz.c.obj", "/src/sdl/ui/sdl-screens.c.obj")
    objects = [obj for obj in objects if not obj.endswith(omit)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    sources = []
    # Read the unmodified parent writer: this proves actual old-layout compatibility.
    old = subprocess.check_output(["git", "show", "5256a6f8:src/fs/save-player.c"],
                                  cwd=ROOT).decode("utf-8")
    old = old[:old.index("\n}", old.index("void wr_extra(void)")) + 2]
    old = old.replace("void wr_extra(void)", "void fixture_old_extra(void)")
    loader = (ROOT / "src/fs/load.c").read_text(encoding="utf-8")
    pos = loader.index("static errr rd_savefile_new_aux(void)")
    start = loader.index("    savefile_has_runtime_overrides =", pos)
    end = loader.index("    /* Reset load byte offset counter */", start)
    reader = READER.replace("    /* FEATURE_FLAGS */", loader[start:end])
    old_birth = subprocess.check_output(["git", "show", "5256a6f8:src/birth/birth-blitz.c"],
                                        cwd=ROOT).decode("utf-8")
    start = old_birth.index("static void blitz_auto_assign_stats(")
    old_birth = old_birth[start:old_birth.index("\n}", start) + 2]
    old_birth = old_birth.replace("blitz_auto_assign_stats", "original_four_stat_allocator")
    birth = BIRTH.replace("/* ORIGINAL_FOUR_STAT_ALLOCATOR */", old_birth)
    old_smith = subprocess.check_output(["git", "show", "5256a6f8:src/cmd/ui/cmd-ui-smithing.c"],
                                        cwd=ROOT).decode("utf-8")
    start = old_smith.index("int object_difficulty(object_type* o_ptr)")
    old_smith = old_smith[start:old_smith.index("\n}", start) + 2]
    old_smith = old_smith.replace("int object_difficulty(", "static int original_object_difficulty(")
    smith = SMITH.replace("/* ORIGINAL_DIFFICULTY */", old_smith)
    for name, code in (("smith.c", smith), ("abilities.c", ABILITIES), ("main.c", MAIN),
                       ("writer.c", WRITER), ("reader.c", reader), ("old-player.c", old),
                       ("birth.c", birth), ("sdl-ui.c", SDL_UI)):
        source = OUT / name
        source.write_text(code, encoding="utf-8")
        sources.append(str(source))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data, str(OUT), str(ROOT / "lib/xtra/font")],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
