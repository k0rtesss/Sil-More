#!/usr/bin/env python3
"""Check smithing scaling, parsed ability gates and real forge cost paths.

Uses current Windows engine objects and temporary template caches only.
Run build-incremental.ps1 first. Does not open or modify player saves.
"""
from pathlib import Path
import os
import re
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
extern void fixture_v28_extra(void);
size_t fixture_write_player(byte* buffer, size_t capacity, int old)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    if (old == 2) fixture_v28_extra();
    else if (old) fixture_old_extra(); else save_write_extra();
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

CHARACTER = r'''
#include "cmd/ui/cmd-ui-character.c"
#include <assert.h>
void fixture_smithing_description(char* desc, size_t size)
{
    character_sheet_item item = {0};
    item.kind = CHARACTER_SHEET_ITEM_SKILL; item.skill = S_SMT;
    character_sheet_format_item_description(&item, desc, size);
    assert(strlen(desc) < 256);
    assert(strstr(desc, "Next point:") && strstr(desc, "XP."));
}
'''

# Exercise the real mobile capability predicate without needing an Android driver.
MOBILE_LAYOUT = r'''
#include "sdl/main-sdl-private.h"
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include "sdl/core/sdl-layout.c"
'''

SDL_UI = r'''
#include "sdl/ui/sdl-screens.c"
#include "player/player-upkeep-internal.h"
#include <assert.h>
extern void fixture_smithing_description(char* desc, size_t size);
void fixture_input_mode(int mode)
{
    config.gamepad_enabled = true;
    config.input_ui_mode = mode == 1 ? SDL_INPUT_UI_MODE_CONTROLLER : SDL_INPUT_UI_MODE_PLATFORM;
    g_direct_touch_present = (mode == 2);
    assert(steamdeck_controls_active() == (mode == 1));
    assert(sdl_touch_only_device_active() == (mode == 2));
}
char fixture_controller_button(int button)
{
    SDL_GamepadButtonEvent event = {0};
    event.button = button; event.down = true;
    sdl_gamepad_handle_button(&event);
    event.down = false; sdl_gamepad_handle_button(&event);
    char ch;
    assert(Term_inkey(&ch, false, true) == 0);
    return ch;
}

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
    SDL_DestroySurface(canvas);
    /* Run the actual 640-byte formatter -> 256-byte live item -> hover renderer. */
    char desc[640]; fixture_smithing_description(desc, sizeof(desc));
    assert(strstr(desc, "= 0 ranks +40 stat"));
    strnfmt(config.story_font, sizeof(config.story_font), "%s/MarcellusSC-Regular.ttf", fonts);
    SDL_strlcpy(config.story_font2, config.story_font, sizeof(config.story_font2));
    int sizes[][2] = {{1280,800}, {640,360}, {360,640}};
    for (int mode = 0; mode < 3; ++mode)
    for (int s = 0; s < 3; ++s)
    {
        fixture_input_mode(mode);
        int w = sizes[s][0], h = sizes[s][1];
        canvas = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888); assert(canvas);
        g_state.window = SDL_CreateWindow("Smithing fixture", w, h, SDL_WINDOW_HIDDEN);
        assert(g_state.window);
        g_state.renderer = SDL_CreateSoftwareRenderer(canvas); assert(g_state.renderer);
        g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_LIVE;
        g_sdl_character_sheet_screen.live_item_count = 0;
        g_sdl_character_sheet_screen.hit_count = 0;
        g_sdl_character_sheet_screen.last_desc_px = 40;
        sdl_character_sheet_screen_add_live_item(42, 0 /* skill */,
            S_SMT, 0, "Smithing", desc);
        sdl_char_sheet_add_hit((SDL_FRect){30, h - 65, w - 60, 40}, 42, desc, TERM_L_BLUE);
        g_sdl_character_sheet_screen.hover_choice = 42;
        assert(!strcmp(sdl_char_sheet_hover_desc(NULL, NULL), desc));
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        sdl_char_sheet_render_hover_tooltip();
        SDL_FRect box = g_sdl_char_sheet_hover_tooltip_rect;
        assert(box.w > 0 && box.h > 0);
        assert(box.x >= 0 && box.y >= 0 && box.x + box.w <= w && box.y + box.h <= h);
        SDL_RenderPresent(g_state.renderer);
        strnfmt(path, sizeof(path), "%s/tooltip-%d-%dx%d.png", output, mode, w, h);
        assert(IMG_SavePNG(canvas, path));
        sdl_ui_text_cache_clear();
        SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
        SDL_DestroyWindow(g_state.window); g_state.window = NULL;
        SDL_DestroySurface(canvas);
    }
    fixture_input_mode(0);
    for (int i = 0; i < g_state.story_font_count; ++i) TTF_CloseFont(g_state.story_fonts[i].font);
    g_state.story_font_count = 0;
    TTF_Quit();
    puts("Screenshot scenario: actual SDL skill collector and renderer show common +40 with zero ranks PASS.");
    puts("Complete live tooltip and viewport bounds: desktop/controller/touch at landscape/portrait sizes PASS.");
}
'''

BIRTH = r'''
#include "birth/birth-blitz.c"
#include <assert.h>
/* ORIGINAL_FOUR_STAT_ALLOCATOR */
void check_birth_allocation(void)
{
    for (int seed = 1; seed <= 100; ++seed)
    {
        int current[BIRTH_STAT_MAX], previous[A_MAX];
        op_ptr->opt[OPT_insight_beta] = false;
        Rand_state_import(seed);
        blitz_auto_assign_stats(current);
        Rand_state_import(seed);
        original_four_stat_allocator(previous);
        assert(!memcmp(current, previous, sizeof(previous)));
        assert(BIRTH_STAT_MAX == A_MAX);
        op_ptr->opt[OPT_insight_beta] = true;
        Rand_state_import(seed);
        blitz_auto_assign_stats(current);
        int cost = 0;
        for (int i = 0; i < BIRTH_STAT_MAX; ++i)
        {
            assert(current[i] >= 0 && current[i] <= 6);
            cost += birth_stat_current_cost(current[i]);
        }
        assert(cost <= MAX_COST);
        assert(!memcmp(current, previous, sizeof(previous)));
    }
    op_ptr->opt[OPT_insight_beta] = false;
    puts("Birth allocation: 100 seeds identical to original with beta off; four stats remain identical with beta on PASS.");
}
'''

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
#include <assert.h>
/* ORIGINAL_DIFFICULTY */
extern char fixture_input_key;
extern char (*fixture_input_action)(void);
extern void fixture_input_mode(int mode);
extern char fixture_controller_button(int button);

static byte parent_planes[6][80 * 48];
static int input_step, input_mode;
static int previous_top;
static void check_parent_planes(bool save)
{
    void* planes[] = {Term->scr->va, Term->scr->vc, Term->scr->vta,
        Term->scr->vtc, Term->scr->vstory, Term->scr->vhealth};
    for (int i = 0; i < 6; ++i)
        if (save) memcpy(parent_planes[i], planes[i], Term->wid * Term->hgt);
        else assert(!memcmp(parent_planes[i], planes[i], Term->wid * Term->hgt));
}
static void fixture_click(int choice)
{
    bool wake = false;
    assert(ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_PRIMARY, &wake));
}
static char report_navigation(void)
{
    int step = input_step++;
    assert(step < 6);
    if (step == 0)
    {
        check_parent_planes(true);
        if (input_mode == 2)
        {
            bool found = false;
            for (int i = 0; i < ui_menu_click_touch_button_count(); ++i)
            {
                int choice; cptr label; byte attr;
                assert(ui_menu_click_touch_button_get(i, &choice, &label, &attr));
                if (choice == SMITH_CLICK_CALC && !strcmp(label, "How calculated")) found = true;
            }
            assert(found); fixture_click(SMITH_CLICK_CALC);
            return UI_MENU_CLICK_WAKE_KEY;
        }
        return input_mode == 1 ? fixture_controller_button(SDL_GAMEPAD_BUTTON_BACK) : '?';
    }
    char row[128];
    memcpy(row, Term->scr->c[1], Term->wid); row[Term->wid] = 0;
    int top; assert(sscanf(row, " Lines %d", &top) == 1);
    if (step == 1 || step == 3) assert(top == 1);
    if (step == 2 || step == 4) assert(top > previous_top);
    previous_top = top;
    assert(ui_scroll_area_has_offset_target());
    if (input_mode == 2)
    {
        assert(ui_scroll_area_is_horizontal_page_mode());
        if (step == 1) return ui_scroll_area_get_horizontal_key(-1);
        if (step == 2) return ui_scroll_area_get_horizontal_key(1);
        if (step == 3) assert(ui_scroll_area_offset_scroll(5));
        if (step == 4) fixture_click(SMITH_CLICK_BACK);
        return UI_MENU_CLICK_WAKE_KEY;
    }
    if (step == 1) return input_mode == 1 ? fixture_controller_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) : ' ';
    if (step == 2) return input_mode == 1 ? fixture_controller_button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) : '9';
    if (step == 3) return '2';
    return input_mode == 1 ? fixture_controller_button(SDL_GAMEPAD_BUTTON_EAST) : ESCAPE;
}

static void check_nested_report(void)
{
    for (input_mode = 0; input_mode < 3; ++input_mode)
    for (int shape = 0; shape < 2; ++shape)
    {
        fixture_input_mode(input_mode);
        Term_resize(shape ? 32 : 80, shape ? 40 : 24);
        Term_putstr(0, 0, -1, TERM_RED, "Original dungeon");
        screen_save();
        for (int y = 0; y < Term->hgt; ++y) Term_erase(0, y, Term->wid);
        Term_putstr(0, 0, -1, TERM_GREEN, "Forge parent");
        Term->scr->ta[0][0] = 12; Term->scr->tc[0][0] = 't';
        Term->scr->story[0][0] = STORY_FLAG_USE;
        Term->scr->health[0][0] = 128;
        screen_save();
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            int highlight = 1;
            input_step = 0; fixture_input_action = report_navigation;
            Term_flush();
            assert(create_sval_menu_aux(TV_SWORD, &highlight) == 0);
            fixture_input_action = NULL;
            assert(input_step == 5 && highlight == 1);
            assert(!ui_scroll_area_has_offset_target());
            check_parent_planes(false);
        }
        screen_load(); screen_load();
        assert(!memcmp(Term->scr->c[0], "Original dungeon", 16));
    }
    fixture_input_mode(0); Term_resize(80,24);
    puts("Actual subtype menu: nested restoration of all six planes, repeated keyboard/controller/touch paging and Back PASS.");
    /* Rotation/resize while in a report crops safely and erases uncovered cells,
     * while retaining the outer saved screen and invalidating every old plane. */
    int resized[][2] = {{40,18}, {100,30}};
    for (int shape = 0; shape < 2; ++shape)
    {
        Term_putstr(0, 0, -1, TERM_RED, "Outer screen");
        Term_save();
        Term_putstr(0, 0, -1, TERM_GREEN, "Parent screen");
        Term_gotoxy(79,23);
        term_snapshot* snapshot = Term_snapshot_save();
        Term_resize(resized[shape][0], resized[shape][1]);
        for (int y = 0; y < Term->hgt; ++y)
            for (int x = 0; x < Term->wid; ++x)
                Term_putch(x,y,TERM_BLUE,'X');
        assert(Term_snapshot_load(snapshot) == 0);
        assert(!memcmp(Term->scr->c[0], "Parent screen", 13));
        assert(Term->scr->cu == (shape == 0));
        for (int y = 0; y < Term->hgt; ++y)
            for (int x = 0; x < Term->wid; ++x)
            {
                assert(Term->old->a[y][x] == 255 && Term->old->c[y][x] == 0);
                assert(Term->old->ta[y][x] == 255 && Term->old->tc[y][x] == 0);
                assert(Term->old->story[y][x] == 255 && Term->old->health[y][x] == 255);
                if (x >= 80 || y >= 24)
                {
                    assert(Term->scr->c[y][x] == Term->char_blank);
                    assert(!Term->scr->ta[y][x] && !Term->scr->tc[y][x]);
                    assert(!Term->scr->story[y][x] && !Term->scr->health[y][x]);
                }
            }
        Term_load(); assert(!memcmp(Term->scr->c[0], "Outer screen", 12));
        Term_resize(80,24);
    }
    puts("Nested snapshots after shrink/grow: clear margins, cursor bounds and outer save preserved PASS.");
}

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
    op_ptr->opt[OPT_insight_beta] = true;
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = true;
    memset(p_ptr->insight_ability_upgraded, 0, sizeof(p_ptr->insight_ability_upgraded));
    smith_build_calculation_report(&report, 76, NULL);
    assert(report_has(&report, "Enchantment: 0.00 (stat bonus upgrade: 1 IP)"));
    p_ptr->insight_points = 1;
    assert(insight_upgrade_ability(S_SMT, SMT_ENCHANTMENT));
    smith_build_calculation_report(&report, 76, NULL);
    assert(report_has(&report, "Enchantment: 3.00"));
    op_ptr->opt[OPT_insight_beta] = false;
    p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] = false;
    p_ptr->innate_ability[S_SMT][SMT_ENCHANTMENT] = false;
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
    check_nested_report();
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

    assert(!insight_system_enabled());
    p_ptr->insight_points = 0;
    p_ptr->skill_base[S_PER] = 10;
    p_ptr->innate_ability[S_PER][PER_QUICK_STUDY] = true;
    assert(!ability_uses_insight_points(S_PER, PER_QUICK_STUDY));
    assert(abilities_in_skill(S_PER) == 1);
    assert(prereqs(S_PER, PER_QUICK_STUDY));
    op_ptr->opt[OPT_insight_beta] = true;
    ability_type* quick = &b_info[ability_index(S_PER, PER_QUICK_STUDY)];
    assert(ability_uses_insight_points(S_PER, PER_QUICK_STUDY));
    assert(ability_in_insight_branch(quick));
    assert(!ability_uses_insight_points(S_SMT, SMT_EXPERTISE));
    assert(!b_info[ability_index(S_SMT, SMT_EXPERTISE)].insight_branch);
    assert(abilities_in_skill(S_PER) == 0);
    assert(prereqs(S_PER, PER_QUICK_STUDY));
    assert(ability_purchase_insight_cost(S_PER, PER_QUICK_STUDY) == 1);
    assert(ability_purchase_xp(quick) == 0);
    p_ptr->new_exp = 99;
    p_ptr->skill_base[S_PER] = 0;
    p_ptr->innate_ability[S_PER][PER_QUICK_STUDY] = false;
    p_ptr->have_ability[S_PER][PER_QUICK_STUDY] = false;
    ability_skill_training training;
    assert(ability_browser_plan_training(quick, &training));
    assert(training.total_cost == 0 && training.count == 0);
    assert(!ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(p_ptr->insight_points == 0 && p_ptr->new_exp == 99);
    assert(p_ptr->skill_base[S_PER] == 0);
    p_ptr->insight_points = 77;
    op_ptr->opt[OPT_insight_beta] = false;
    gain_insight_points(5, NULL);
    assert(p_ptr->insight_points == 77);
    op_ptr->opt[OPT_insight_beta] = true;
    gain_insight_points(5, NULL);
    assert(p_ptr->insight_points == 82);
    p_ptr->insight_points = PY_MAX_EXP - 1;
    gain_insight_points(0x7fffffff, NULL);
    assert(p_ptr->insight_points == PY_MAX_EXP);
    op_ptr->opt[OPT_insight_beta] = false;
    puts("Insight beta: default off, separate branch, one-point cost, zero XP/training and dormant currency PASS.");
}
'''

MAIN = r'''
#include "angband.h"
#include "externs.h"
#include "metarun.h"
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
extern size_t fixture_write_player(byte* buffer, size_t capacity, int old);
extern int fixture_read_player(const byte* buffer, size_t length, int extra);
static term test_term;
char fixture_input_key = ' ';
char (*fixture_input_action)(void);
static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT)
        Term_keypress(fixture_input_action ? fixture_input_action() : fixture_input_key);
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
    assert(parse_line(&h, "R:DEX:2:GRA:4") == 0);
    assert(entries[1].stat_req[A_DEX] == 2 && entries[1].stat_req[A_GRA] == 4);
    assert(parse_line(&h, "R:LORE:5") != 0);
    assert(parse_line(&h, "K:2") == 0);
    assert(entries[1].insight_cost == 2);
    assert(parse_line(&h, "L:1") == 0);
    assert(entries[1].insight_branch);
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
    p_ptr->insight_points = 1234;
    p_ptr->insight_milestones = INSIGHT_MILESTONE_SONG;
    p_ptr->insight_monster_types = RF3_ORC | RF3_RAUKO;
    for (int i = 0; i < A_MAX; ++i) p_ptr->insight_stat_invested[i] = i + 1;
    memset(p_ptr->insight_ability_upgraded, 0, sizeof(p_ptr->insight_ability_upgraded));
    p_ptr->insight_ability_upgraded[S_SMT][SMT_ENCHANTMENT] = true;
    p_ptr->insight_ability_upgraded[S_SMT][SMT_ARTEFACT] = true;
    p_ptr->innate_ability[S_MEL][MEL_ZONE_OF_CONTROL] = true;
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON] = true;
    p_ptr->innate_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->diseased = 0;
    memset(p_ptr->stat_disease, 0, sizeof(p_ptr->stat_disease));
    p_ptr->disease_name = p_ptr->disease_cure = p_ptr->disease_knowledge = 0;
    p_ptr->lamp_oil = 137;
    p_ptr->morgoth_call_state = SAVEFILE_MORGOTH_CALL_SEEN;
    p_ptr->discovery_lore_flags = DISC_LORE_CHASM;
    op_ptr->opt[OPT_insight_beta] = false;
    size_t new_size = fixture_write_player(buffer, 1024*1024, false);
    p_ptr->insight_points = 0; p_ptr->insight_milestones = 0;
    p_ptr->insight_monster_types = 0;
    memset(p_ptr->insight_stat_invested, 0, sizeof(p_ptr->insight_stat_invested));
    memset(p_ptr->insight_ability_upgraded, 0xFF, sizeof(p_ptr->insight_ability_upgraded));
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON] = false;
    assert(fixture_read_player(buffer, new_size, VERSION_EXTRA) == 0);
    assert(p_ptr->insight_points == 1234 && p_ptr->insight_milestones == INSIGHT_MILESTONE_SONG);
    assert(p_ptr->insight_monster_types == (RF3_ORC | RF3_RAUKO));
    for (int i = 0; i < A_MAX; ++i) assert(p_ptr->insight_stat_invested[i] == i + 1);
    for (int skill = 0; skill < S_MAX; ++skill)
        for (int ability = 0; ability < ABILITIES_MAX; ++ability)
            assert(p_ptr->insight_ability_upgraded[skill][ability]
                == (skill == S_SMT && (ability == SMT_ENCHANTMENT || ability == SMT_ARTEFACT)));
    assert(!insight_system_enabled());
    assert(p_ptr->innate_ability[S_MEL][MEL_ZONE_OF_CONTROL]);
    assert(p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]);
    assert(!p_ptr->innate_ability[S_MEL][MEL_RAPID_ATTACK]);
    op_ptr->opt[OPT_insight_beta] = true;
    assert(!ability_stage_conflict(&b_info[ability_index(S_MEL, MEL_RAPID_ATTACK)]));
    op_ptr->opt[OPT_insight_beta] = false;
    assert(p_ptr->lamp_oil == 137 && p_ptr->morgoth_call_state == SAVEFILE_MORGOTH_CALL_SEEN);
    assert(p_ptr->discovery_lore_flags == DISC_LORE_CHASM);
    size_t v28_size = fixture_write_player(buffer, 1024*1024, 2);
    assert(new_size - v28_size == sizeof(u32b) + S_MAX * ABILITIES_MAX);
    assert(fixture_read_player(buffer, v28_size, 28) == 0);
    assert(p_ptr->insight_points == 1234 && p_ptr->insight_milestones == INSIGHT_MILESTONE_SONG);
    assert(p_ptr->insight_monster_types == 0);
    for (int i = 0; i < A_MAX; ++i) assert(p_ptr->insight_stat_invested[i] == i + 1);
    for (int skill = 0; skill < S_MAX; ++skill)
        for (int ability = 0; ability < ABILITIES_MAX; ++ability)
            assert(!p_ptr->insight_ability_upgraded[skill][ability]);
    for (int i = 0; i < A_MAX; ++i)
    {
        p_ptr->stat_base[i] = p_info[p_ptr->prace].r_adj[i]
            + c_info[p_ptr->pcharacter].h_adj[i] + i;
        for (int curse = 0; curse < z_info->cu_max; ++curse)
            p_ptr->stat_base[i] += CURSE_GET(curse) * cu_info[curse].cu_adj[i];
    }
    size_t old_size = fixture_write_player(buffer, 1024*1024, true);
    p_ptr->insight_points = 9999;
    assert(fixture_read_player(buffer, old_size, LEGACY_WRITER_EXTRA) == 0);
    assert(p_ptr->insight_points == 0);
    assert(p_ptr->insight_milestones == 0);
    for (int i = 0; i < A_MAX; ++i) assert(p_ptr->insight_stat_invested[i] == i);
    assert(p_ptr->lamp_oil == 137 && p_ptr->morgoth_call_state == SAVEFILE_MORGOTH_CALL_SEEN);
    assert(p_ptr->discovery_lore_flags == DISC_LORE_CHASM);
    free(buffer);
    puts("Real player save: current currency/ranks/upgrades/monster types roundtrip, v28 defaults, legacy points and complete tail alignment PASS.");
}
static void check_settings(cptr directory)
{
    char filename[1024];
    strnfmt(filename, sizeof(filename), "%s/lore-options.json", directory);
    assert(strcmp(option_text[OPT_insight_beta], "insight_beta") == 0);
    assert(!option_norm[OPT_insight_beta]);
    assert(strcmp(option_text[OPT_active_weapon_switch_confirm], "active_weapon_switch_confirm") == 0);
    assert(strcmp(option_text[OPT_environment_speed], "environment_speed") == 0);
    struct sdl_config local_config;
    sdl_config_set_defaults(&local_config);
    op_ptr->opt[OPT_insight_beta] = true;
    assert(sdl_config_save(filename, &local_config, NULL, 0));
    op_ptr->opt[OPT_insight_beta] = false;
    sdl_config_load_app_options(filename);
    assert(insight_system_enabled());
    FILE* file = fopen(filename, "w"); assert(file);
    fputs("{\"appOptions\":{\"gameplay\":{}}}", file); fclose(file);
    sdl_config_load_app_options(filename);
    assert(!insight_system_enabled());
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
    const player_race* saved_race = rp_ptr;
    character_profile* saved_profile = current_character_profile;
    rp_ptr = &p_info[4]; current_character_profile = &c_info[20];
    assert(rp_ptr->flags & RHF_DWARVEN_SMITHING);
    assert(affinity_level(S_SMT) == 1);
    assert(smithing_affinity_stat_bonus() == 1);
    assert(smithing_effective_stat(A_STR) == 4);
    assert(smithing_stat_bonus(&heavy) == 12);
    assert(smithing_stat_bonus(&mail) == 12);
    assert(smithing_stat_bonus(&ring) == 12);
    assert(!ability_stat_requirements_met(
        &b_info[ability_index(S_SMT, SMT_MASTERPIECE)]));
    current_character_profile = &c_info[18];
    assert(affinity_level(S_SMT) == 2);
    assert(smithing_affinity_stat_bonus() == 2);
    assert(smithing_effective_stat(A_GRA) == 5);
    assert(smithing_stat_bonus(&heavy) == 15);
    assert(smithing_stat_bonus(&mail) == 15);
    assert(smithing_stat_bonus(&ring) == 15);
    assert(ability_stat_requirements_met(
        &b_info[ability_index(S_SMT, SMT_MASTERPIECE)]));
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = true;
    assert(smithing_mastery_stat_bonus_scaled(SMT_EXPERTISE) == 500);
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE] = false;
    ability_type non_smith = {0};
    non_smith.skilltype = S_MEL; non_smith.stat_req[A_STR] = 4;
    assert(!ability_stat_requirements_met(&non_smith));
    current_character_profile = &c_info[22];
    assert(affinity_level(S_SMT) == 0);
    assert(smithing_affinity_stat_bonus() == 0);
    assert(smithing_effective_stat(A_DEX) == 3);
    assert(smithing_stat_bonus(&heavy) == 9);
    rp_ptr = saved_race; current_character_profile = saved_profile;
    puts("Dwarven Smithing: affinity +1, mastery +2, and a cancelled affinity +0 affect only Smithing stats PASS.");
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
            "/src/birth/birth-blitz.c.obj", "/src/sdl/ui/sdl-screens.c.obj",
            "/src/cmd/ui/cmd-ui-character.c.obj", "/src/sdl/core/sdl-layout.c.obj")
    objects = [obj for obj in objects if not obj.endswith(omit)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    sources = []
    # Retain the legacy writer layout, adapting only removed in-memory fields.
    legacy_ref = "ed5e9b322f550f423b14fb97fe8b92afa1d1a604"
    legacy_defines = subprocess.check_output(["git", "show", f"{legacy_ref}:src/defines.h"], cwd=ROOT, text=True)
    legacy_extra = int(re.search(r"^#define VERSION_EXTRA (\d+)", legacy_defines, re.M)[1])
    assert legacy_extra in (24, 25), "Pin legacy_ref to the pre-Lore-currency writer commit"
    old = subprocess.check_output(["git", "show", f"{legacy_ref}:src/fs/save-player.c"],
                                  cwd=ROOT).decode("utf-8")
    old = old[:old.index("\n}", old.index("void wr_extra(void)")) + 2]
    old = old.replace("void wr_extra(void)", "void fixture_old_extra(void)")
    old = old.replace("p_ptr->knowledge_points", "0 /* unused beta currency */")
    old = re.sub(r"p_ptr->lore\b", "0 /* unused beta stat */", old)
    v28 = subprocess.check_output(["git", "show", "837b5ace:src/fs/save-player.c"],
                                  cwd=ROOT).decode("utf-8")
    v28 = v28[:v28.index("\n}", v28.index("void wr_extra(void)")) + 2]
    v28 = v28.replace("void wr_extra(void)", "void fixture_v28_extra(void)")
    for old_name, new_name in (
        ("lore_milestones", "insight_milestones"),
        ("lore_points", "insight_points"),
        ("lore_stat_invested", "insight_stat_invested"),
    ):
        v28 = v28.replace(old_name, new_name)
    main_source = MAIN.replace("LEGACY_WRITER_EXTRA", str(legacy_extra))
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
    for name, code in (("smith.c", smith), ("abilities.c", ABILITIES), ("main.c", main_source),
                       ("writer.c", WRITER), ("reader.c", reader), ("old-player.c", old),
                       ("v28-player.c", v28),
                       ("birth.c", birth), ("sdl-ui.c", SDL_UI),
                       ("character.c", CHARACTER), ("mobile-layout.c", MOBILE_LAYOUT)):
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
