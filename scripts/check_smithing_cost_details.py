#!/usr/bin/env python3
"""Exercise the Smithing editor and its read-only cost report.

Uses initialized production templates, previews, pending Reforge state and the
real keyboard/click consumers. UI input and unrelated repaint are fixtures.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_reforge_work_transaction import CHECKS as WORK_CHECKS, SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/smithing-cost-details"
EXTRA = r'''
#include <assert.h>
void fixture_detail_bill(int* uses, int* turns)
{
    assert(smith_ui_detail_valid);
    *uses = smith_ui_detail_cost.uses;
    *turns = p_ptr->smithing_leftover ? p_ptr->smithing_leftover : smith_ui_detail_turns;
}

void fixture_details(int item, int prefix, int mode)
{
    object_type saved_object = *smith_o_ptr;
    if (mode < 2 || mode == 4)
    {
        reforge_preview_type preview;
        const object_type* source = player_inventory_object(item);
        assert(reforge_preview_build(source, prefix, &preview));
        smith_sheet_layout sheet = smith_sheet_geometry();
        prt_reforge_preview(&sheet, &preview, source);
        assert(smith_ui_detail_valid && !smith_ui_detail_paid);
        assert(!memcmp(&preview.cost, &smith_ui_detail_cost, sizeof(preview.cost)));
        assert(smith_ui_detail_turns == preview.turns);
    }
    else
    {
        if (mode == 2)
        {
            smith_sheet_layout sheet = smith_sheet_geometry();
            smith_sheet_order(&sheet, smith_o_ptr, smith_measure_design(), true, false,
                SMITH_NAME_READ_ONLY);
            assert(smith_ui_detail_valid && smith_ui_detail_paid);
            smithing_cost_type zero = { 0 };
            assert(!memcmp(&zero, &smith_ui_detail_cost, sizeof(zero)));
            assert(smith_ui_detail_turns == p_ptr->smithing_leftover);
        }
        else
        {
            object_wipe(smith_o_ptr);
            smith_ui_detail_valid = false;
        }
    }

    object_type object = *smith_o_ptr;
    smithing_cost_type cost = smithing_cost;
    smith_alloy_state alloy = smith_alloy;
    int work = p_ptr->smithing, remaining = p_ptr->smithing_leftover;
    if (mode == 1 || mode == 4)
    {
        smith_sheet_layout sheet = smith_sheet_geometry();
        smith_sheet_begin(&sheet, false);
        const smith_sheet_action costs[] = {
            { SMITH_CLICK_CALC, "Costs", TERM_WHITE },
        };
        smith_sheet_actions(&sheet, costs, N_ELEMENTS(costs), 0);
        int col = 0;
        while (col < sheet.width)
        {
            byte attr; char cell;
            assert(!Term_what(col, sheet.footer, &attr, &cell));
            if (cell == 'C') break;
            ++col;
        }
        assert(col < sheet.width);
        int choice, action;
        assert(ui_menu_click_get_cell_choice(col, sheet.footer, &choice));
        assert(choice == SMITH_CLICK_CALC);
        assert(ui_menu_click_handle_cell_action(col, sheet.footer,
            mode == 4 ? UI_MENU_CLICK_HOVER : UI_MENU_CLICK_PRIMARY));
        assert(smith_ui_take_click_action(&choice, &action));
        assert(choice == 0 && action == UI_MENU_CLICK_HOVER);
    }
    else smith_show_cost_details();
    assert(!memcmp(&object, smith_o_ptr, sizeof(object)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    assert(!memcmp(&alloy, &smith_alloy, sizeof(alloy)));
    assert(p_ptr->smithing == work && p_ptr->smithing_leftover == remaining);
    if (mode < 2 || mode == 4) *smith_o_ptr = saved_object;
}

void fixture_long_costs(void)
{
    create_base_object(TV_SWORD, SV_SHORT_SWORD);
    smith_sheet_layout sheet = smith_sheet_geometry();
    smith_sheet_order(&sheet, smith_o_ptr, smith_measure_design(), false, false,
        SMITH_NAME_READ_ONLY);
    smith_ui_detail_reforge = false;
    smith_ui_detail_paid = false;
    smith_ui_detail_cost = (smithing_cost_type){
        .uses = 2, .drain = 3, .mithril = 12, .star_iron = 21,
        .str = 1, .dex = 2, .con = 3, .gra = 4, .exp = 250,
        .weaponsmith = 1, .enchantment = 1, .artifice = 1,
    };
    smith_ui_detail_turns = 90;
    object_type design = *smith_o_ptr;
    smithing_cost_type current_cost = smithing_cost;
    smith_alloy_state alloy = smith_alloy;
    smith_show_cost_details();
    assert(!memcmp(&design, smith_o_ptr, sizeof(design)));
    assert(!memcmp(&current_cost, &smithing_cost, sizeof(current_cost)));
    assert(!memcmp(&alloy, &smith_alloy, sizeof(alloy)));
}

cptr fixture_effects(void)
{
    return smith_description.active && smith_description.effects
        ? smith_description.text : "";
}
'''
CHECKS = r'''
#include "sdl-config.h"
#include "ui/menu-click.h"
extern void fixture_details(int,int,int);
extern void fixture_detail_bill(int*,int*);
extern void fixture_long_costs(void);
extern cptr fixture_effects(void);
typedef enum { DRAFT_UNCHECKED, DRAFT_PLAIN, DRAFT_PREFIX, DRAFT_ARTEFACT } draft_check;
typedef enum { NEW_WORK, INTERRUPTED_WORK, PAID_REFORGE } work_check;
typedef struct editor_step {
    cptr label;
    char key;
    bool click;
    draft_check draft;
    int attack_gain;
    int strength;
    bool finesse;
    cptr also;
    cptr missing;
    int base_sval;
    cptr row_value;
    bool current_attack;
    bool check_target;
    int drag_rows;
    cptr missing_choice;
    bool reselected_base;
    bool report_bill;
    bool description_scroll;
    bool full_description;
    int skill, skill_bonus;
    cptr name_input;
    cptr effect;
    int tunneling, suffix;
    bool no_tunneling;
} editor_step;
static const editor_step* editor_steps;
static int editor_step_index;
static cptr editor_name_input;
static draft_check previous_draft;
static object_type editor_blueprint;
static bool editor_wide;
static bool editor_touch;
static int browser_footer_row, design_footer_row;
static int direct_report_mode = -1;
static int direct_report_frames;

bool __real_sdl_touch_only_device_active(void);
bool __wrap_sdl_touch_only_device_active(void)
{
    return editor_touch || __real_sdl_touch_only_device_active();
}

int __real_sdl_touch_menu_button_reserved_rows(void);
int __wrap_sdl_touch_menu_button_reserved_rows(void)
{
    return editor_touch ? 2 : __real_sdl_touch_menu_button_reserved_rows();
}

static void editor_frame_line(int row, char* line)
{
    byte attr;
    for (int x = 0; x < Term->wid; ++x)
        assert(!Term_what(x, row, &attr, &line[x]));
    line[Term->wid] = '\0';
}

static bool editor_frame_find(cptr text, int* col, int* row, bool clickable)
{
    for (int y = 0; y < Term->hgt; ++y)
    {
        char line[256];
        editor_frame_line(y, line);
        for (char* match = strstr(line, text); match; match = strstr(match + 1, text))
        {
            int choice;
            if (clickable && !ui_menu_click_get_cell_choice((int)(match - line), y, &choice))
                continue;
            if (col) *col = (int)(match - line);
            if (row) *row = y;
            return true;
        }
    }
    return false;
}

static int editor_require(cptr text, bool clickable, int* col)
{
    int row;
    if (!editor_frame_find(text, col, &row, clickable))
    {
        fprintf(stderr, "Missing %s at Smithing step %d%s\n", text,
            editor_step_index, clickable ? " (click target)" : "");
        for (int y = 0; y < Term->hgt; ++y)
        {
            char line[256];
            editor_frame_line(y, line);
            fprintf(stderr, "|%s|\n", line);
        }
        assert(false);
    }
    return row;
}

static bool editor_touch_button(cptr text, int* choice)
{
    if (!editor_touch) return false;
    bool found = false;
    for (int i = 0; i < ui_menu_click_touch_button_count(); i++)
    {
        int id;
        cptr label;
        assert(ui_menu_click_touch_button_get(i, &id, &label, NULL));
        if (strcmp(label, text)) continue;
        assert(!found);
        found = true;
        if (choice) *choice = id;
    }
    return found;
}

static int editor_require_action(cptr label, int* target)
{
    if (!editor_touch) return editor_require(label, true, target);
    assert(editor_touch_button(label, target));
    assert(!editor_frame_find(label, NULL, NULL, true));
    return Term->hgt - 1 - sdl_touch_menu_button_reserved_rows();
}

static int report_value(cptr label, int* row)
{
    int col, value;
    *row = editor_require(label, false, &col);
    char line[256];
    editor_frame_line(*row, line);
    assert(sscanf(line + col + strlen(label), "%d", &value) == 1);
    return value;
}

static void check_report_bill(void)
{
    int heading = editor_require("COMPLETE COSTS", false, NULL);
    int back = editor_require_action("Back", NULL);
    int paid, uses_row, turns_row, uses, turns;
    fixture_detail_bill(&uses, &turns);
    assert(report_value("Forge uses", &uses_row) == uses);
    assert(report_value("Turns", &turns_row) == turns);
    assert(heading < uses_row && uses_row < back);
    assert(heading < turns_row && turns_row < back);
    if (editor_frame_find("Costs already paid.", NULL, &paid, false))
        assert(uses == 0 && heading < paid && paid < back);
}

static void check_direct_report_frame(void)
{
    ++direct_report_frames;
    assert(direct_report_mode != 4); /* Hover must leave the sheet alone. */
    if (direct_report_mode == 3)
    {
        editor_require("COMPLETE COSTS", false, NULL);
        editor_require("Choose an item", false, NULL);
        assert(!editor_frame_find("Forge uses", NULL, NULL, false));
        return;
    }
    editor_require("Shortsword", false, NULL);
    check_report_bill();
    assert(editor_frame_find("Costs already paid.", NULL, NULL, false)
        == (direct_report_mode == 2));
}

static bool editor_cell_is_choice(int col, int row, int choice)
{
    int candidate;
    return ui_menu_click_get_cell_choice(col, row, &candidate) && candidate == choice;
}

static void editor_check_inline(int col, int row, cptr label, cptr value,
    int base_sval)
{
    int choice;
    char line[256];
    assert(ui_menu_click_get_cell_choice(col, row, &choice));
    editor_frame_line(row, line);
    int summary_col, summary_row;
    if (editor_wide)
    {
        editor_require("Work order", false, &summary_col);
        assert(col < summary_col && !editor_cell_is_choice(summary_col, row, choice));
    }
    else if (editor_frame_find("Work order", NULL, &summary_row, false))
        assert(row < summary_row);
    else
        assert(row + 1 < editor_require("Difficulty", false, NULL));
    if (base_sval)
    {
        object_type item;
        object_prep(&item, lookup_kind(TV_SWORD, base_sval));
        char glyph = object_char(&item);
        int x = col - 1;
        while (x >= 0 && line[x] == ' ') --x;
        assert(x >= col - 3 && x >= 0 && line[x] == glyph);
        assert(editor_cell_is_choice(x, row, choice));
    }
    if (value)
    {
        char* match = strstr(line + col + strlen(label), value);
        if (!match || !editor_cell_is_choice((int)(match - line), row, choice))
            fprintf(stderr, "Expected '%s' beside '%s', step %d: |%s|\n",
                value, label, editor_step_index, line);
        assert(match && editor_cell_is_choice((int)(match - line), row, choice));
    }
}

static void editor_check_geometry(void)
{
    int tab_col, tab_row, details_target, details_row, primary_target;
    bool browser = editor_frame_find("Weapons", &tab_col, &tab_row, true);
    bool editor = !browser &&
        editor_frame_find("Enhancements", &tab_col, &tab_row, true);
    if (!browser && !editor) return;

    int heading_row = editor_require("Smithing", false, NULL);
    assert(heading_row < tab_row);
    details_row = editor_require_action("Costs", &details_target);
    assert(tab_row < details_row && details_row < Term->hgt);

    int* footer = browser ? &browser_footer_row : &design_footer_row;
    if (*footer < 0) *footer = details_row;
    assert(details_row == *footer);
    if (browser)
    {
        bool replace = editor_frame_find("Change base item", NULL, NULL, false);
        if (editor_touch)
        {
            assert(ui_menu_click_touch_exit_button_active() == !replace);
            assert(editor_touch_button("Back", NULL) == replace);
            assert(!editor_touch_button("Exit", NULL));
        }
        if (!replace)
        {
            assert(editor_require_action("Melt", NULL) == details_row);
            assert(editor_require_action("Reforge", NULL) == details_row);
        }
        if (replace || Term->wid >= 48)
            assert(editor_require_action("Inspect", NULL) == details_row);
        assert(editor_require_action("Design", &primary_target) == details_row);
    }
    else
    {
        bool choosing = editor_touch_button("Apply", NULL)
            || editor_frame_find("Apply", NULL, NULL, true);
        cptr primary = choosing ? "Apply" : p_ptr->smithing_leftover ? "Resume" : "Craft";
        assert(editor_require_action(primary, &primary_target) == details_row);
        if (!choosing)
            assert(editor_require_action("Inspect", NULL) == details_row);
        if (editor_touch)
        {
            assert(!ui_menu_click_touch_exit_button_active());
            assert(editor_require_action("Back", NULL) == details_row);
        }
    }
    assert(primary_target != details_target);

    int difficulty_col, uses_col, turns_col, uses_row;
    int difficulty_row = editor_require("Difficulty", false, &difficulty_col);
    cptr uses_label = "Forge uses";
    if (!editor_frame_find(uses_label, &uses_col, &uses_row, false))
    {
        uses_label = "Uses";
        uses_row = editor_require(uses_label, false, &uses_col);
    }
    int turns_row = editor_require("Turns", false, &turns_col);
    assert(tab_row < difficulty_row && difficulty_row < details_row);
    assert(tab_row < uses_row && uses_row < details_row);
    assert(tab_row < turns_row && turns_row < details_row);
    char line[256];
    int difficulty, capacity, uses, available, turns, expected_uses, expected_turns;
    fixture_detail_bill(&expected_uses, &expected_turns);
    editor_frame_line(difficulty_row, line);
    assert(sscanf(line + difficulty_col + strlen("Difficulty"), "%d / %d",
        &difficulty, &capacity) == 2 && difficulty >= 0 && capacity >= 0);
    editor_frame_line(uses_row, line);
    assert(sscanf(line + uses_col + strlen(uses_label), "%d / %d",
        &uses, &available) == 2);
    assert(uses == expected_uses && available == forge_uses(5, 5));
    editor_frame_line(turns_row, line);
    assert(sscanf(line + turns_col + strlen("Turns"), "%d", &turns) == 1);
    assert(turns == expected_turns && turns > 0);
    if (editor_wide)
    {
        int summary_col;
        int summary_row = editor_require("Work order", false, &summary_col);
        assert(tab_row < summary_row && summary_row <= difficulty_row);
        assert(tab_col < summary_col && summary_col == difficulty_col);
    }
}

static char editor_step_key(void)
{
    if (editor_name_input && *editor_name_input) return *editor_name_input++;
    const editor_step* step = editor_steps++;
    int col = 0, row = 0, touch_choice;
    assert(step->label);
    if (step->name_input)
    {
        assert(sdl_question_menu_is_active());
        editor_name_input = step->name_input;
        ++editor_step_index;
        return *editor_name_input++;
    }
    bool touch_action = editor_touch_button(step->label, &touch_choice);
    if (touch_action)
        assert(!editor_frame_find(step->label, NULL, NULL, true));
    else
        row = editor_require(step->label,
            step->click || step->base_sval || step->row_value
                || step->current_attack || step->check_target, &col);
    editor_check_geometry();
    if (step->report_bill) check_report_bill();
    if (step->full_description)
    {
        int description_col;
        int first = editor_require("Description", true, &description_col);
        int end = editor_require_action("Costs", NULL);
        assert(!ui_scroll_area_has_cell(description_col, first));
        for (int y = first + 1; y < end; y++)
        {
            char line[256];
            editor_frame_line(y, line);
            assert(!strstr(line, "..."));
        }
    }
    if (step->base_sval || step->row_value || step->current_attack)
    {
        cptr value = step->row_value;
        char attack[16];
        if (step->current_attack)
        {
            strnfmt(attack, sizeof(attack), "%+d", smith_o_ptr->att);
            value = attack;
        }
        editor_check_inline(col, row, step->label, value, step->base_sval);
    }
    if (step->also && !editor_touch_button(step->also, NULL))
        editor_require(step->also, false, NULL);
    if (step->missing) assert(!editor_frame_find(step->missing, NULL, NULL, false));
    if (step->missing_choice)
        assert(!editor_frame_find(step->missing_choice, NULL, NULL, true));
    if (step->skill)
        assert(smith_o_ptr->skill_bonus[step->skill] == step->skill_bonus);
    if (step->effect)
    {
        editor_require("Effects", true, NULL);
        if (!strstr(fixture_effects(), step->effect))
            fprintf(stderr, "Expected effect '%s', got '%s'\n", step->effect, fixture_effects());
        assert(strstr(fixture_effects(), step->effect));
    }
    if (step->tunneling || step->no_tunneling)
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);
        assert(((f1 & TR1_TUNNEL) != 0) != step->no_tunneling);
        assert(smith_o_ptr->pval == step->tunneling);
    }
    if (step->suffix)
        assert(object_ego_suffix(smith_o_ptr) == step->suffix);
    ++editor_step_index;
    if (step->draft)
    {
        if (smith_o_ptr->tval != TV_SWORD || smith_o_ptr->sval != SV_SHORT_SWORD)
            fprintf(stderr, "%s: expected Shortsword, got tval %d, sval %d\n",
                step->label, smith_o_ptr->tval, smith_o_ptr->sval);
        assert(smith_o_ptr->tval == TV_SWORD && smith_o_ptr->sval == SV_SHORT_SWORD);
        assert((object_ego_prefix(smith_o_ptr) != 0) == (step->draft == DRAFT_PREFIX));
        assert(!object_ego_suffix(smith_o_ptr));
        assert((smith_o_ptr->name1 != 0) == (step->draft == DRAFT_ARTEFACT));
        if (step->draft == DRAFT_ARTEFACT)
        {
            u32b f1, f2, f3;
            object_flags(smith_o_ptr, &f1, &f2, &f3);
            assert(f1 & TR1_STR);
            assert(smith_o_ptr->stat_bonus[A_STR] == step->strength);
            assert(smith_o_ptr->abilities == (int)step->finesse);
            if (step->finesse)
                assert(smith_o_ptr->skilltype[0] == S_MEL
                    && smith_o_ptr->abilitynum[0] == MEL_FINESSE);
        }
        if (previous_draft != step->draft)
        {
            editor_blueprint = *smith_o_ptr;
            previous_draft = step->draft;
        }
        object_type expected = editor_blueprint;
        if (step->reselected_base)
        {
            /* Recreating a base rerolls its hallucination-only appearance. */
            expected.image_k_idx = smith_o_ptr->image_k_idx;
            editor_blueprint.image_k_idx = expected.image_k_idx;
        }
        expected.att += step->attack_gain;
        if (step->draft == DRAFT_ARTEFACT)
            expected.stat_bonus[A_STR] = step->strength;
        if (step->finesse)
        {
            expected.abilities = 1;
            expected.skilltype[0] = S_MEL;
            expected.abilitynum[0] = MEL_FINESSE;
        }
        if (memcmp(smith_o_ptr, &expected, sizeof(expected)))
            fprintf(stderr, "Smithing step %d (%s): expected attack %d, got %d\n",
                editor_step_index - 1, step->label, expected.att, smith_o_ptr->att);
        assert(!memcmp(smith_o_ptr, &expected, sizeof(expected)));
    }
    if (step->click)
    {
        if (touch_action)
        {
            assert(ui_menu_click_handle_choice_action(touch_choice,
                UI_MENU_CLICK_PRIMARY, NULL));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        assert(ui_menu_click_handle_cell(col, row));
        return '\r'; /* SDL primary clicks wake inkey() with Enter. */
    }
    if (step->drag_rows)
    {
        if (step->base_sval || step->description_scroll)
            assert(ui_scroll_area_has_cell(col, row));
        assert(ui_scroll_area_has_offset_target());
        assert(ui_scroll_area_offset_scroll(step->drag_rows));
        return 0;
    }
    return step->key;
}

static void run_editor_steps(cptr name, int width, int height,
    const editor_step* steps, work_check work_mode)
{
    scenario = name;
    if (Term->wid != width || Term->hgt != height)
        assert(!Term_resize(width, height));
    prepare(3);
    p_ptr->skill_use[S_SMT] = 30;
    if (work_mode == INTERRUPTED_WORK)
    {
        object_prep(smith_o_ptr, lookup_kind(TV_SOFT_ARMOR, SV_ROBE));
        smith_o_ptr->number = 1;
        p_ptr->smithing_leftover = 1;
    }
    if (work_mode == PAID_REFORGE)
    {
        int prefix, turns, dex;
        assert(fixture_reforge_plan(selected_item, &prefix, &turns, &dex) >= 0);
        assert(fixture_begin(selected_item, prefix));
        assert(smith_o_ptr->unused1 == 2 && p_ptr->smithing_leftover == turns);
        p_ptr->smithing = 0;
        p_ptr->energy_use = 0;
        cave_feat[5][5] = FEAT_FORGE_NORMAL_HEAD;
    }
    object_type work = *smith_o_ptr;
    int remaining = p_ptr->smithing_leftover;
    previous_draft = DRAFT_UNCHECKED;
    editor_steps = steps;
    editor_step_index = 0;
    editor_wide = width >= 72 && height > 18 && 2 * width >= 3 * height;
    browser_footer_row = design_footer_row = -1;
    long xp = p_ptr->new_exp, world = turn, actions = playerturn;
    int uses = forge_uses(5, 5), count = source_count();
    do_cmd_smithing_screen();
    assert(!editor_steps->label);
    editor_steps = NULL;
    assert(!p_ptr->smithing && p_ptr->smithing_leftover == remaining);
    if (work_mode != NEW_WORK)
        assert(!memcmp(&work, smith_o_ptr, sizeof(work)));
    else
        assert(!smith_o_ptr->k_idx);
    assert(!p_ptr->energy_use && forge_uses(5, 5) == uses && source_count() == count);
    assert(p_ptr->new_exp == xp && turn == world && playerturn == actions);
    printf("%s at %dx%d PASS\n", name, width, height);
}

static void editor_checks(void)
{
    const editor_step steps[] = {
        { "Choose base item", 'b', .also = "Dagger" },
        { "Attack", 'a', false, DRAFT_PLAIN, .current_attack = true },
        { "Choose Attack", '6', false, DRAFT_PLAIN, .also = "Costs" },
        { "Costs", 0, true, DRAFT_PLAIN, .also = "Choose Attack" },
        { "COMPLETE COSTS", ESCAPE, false, DRAFT_PLAIN, .report_bill = true },
        { "Choose Attack", '\r', false, DRAFT_PLAIN, .also = "Simple" },
        { "Attack", 'a', false, DRAFT_PLAIN, 1, .current_attack = true },
        { "Choose Attack", '8', false, DRAFT_PLAIN, 1 },
        { "Choose Attack", ESCAPE, false, DRAFT_PLAIN, 1 },
        { "Attack", 'a', false, DRAFT_PLAIN, 1 },
        { "Choose Attack", '8', false, DRAFT_PLAIN, 1 },
        { "Choose Attack", '\t', false, DRAFT_PLAIN, 1 },
        { "Prefix", '[', false, DRAFT_PLAIN, 1, .row_value = "None" },
        { "Attack", 'a', false, DRAFT_PLAIN, 1 },
        { "Choose Attack", '8', false, DRAFT_PLAIN, 1 },
        { "Enhancements", 0, true, DRAFT_PLAIN, 1, .also = "Choose Attack" },
        { "Prefix", 'a', false, DRAFT_PLAIN, 1 },
        { "Prefix", ESCAPE },
        { "Suffix", 'b', false, DRAFT_PLAIN, 1 },
        { "Suffix", ESCAPE },
        { "Prefix", ']', false, DRAFT_PLAIN, 1 },
        { "Stat bonuses", 'a', false, DRAFT_PLAIN, 1 },
        { "Property", ESCAPE },
        { "Stat bonuses", '[', false, DRAFT_PLAIN, 1 },
        { "Prefix", '\t', false, DRAFT_PLAIN, 1 },
        { "Simple", 0, true, DRAFT_PLAIN, 1 },
        { "Costs", 0, true, DRAFT_PLAIN, 1 },
        { "COMPLETE COSTS", ESCAPE, false, DRAFT_PLAIN, 1, .report_bill = true },
        { "Attack", ESCAPE, false, DRAFT_PLAIN, 1 },
        { "Jewellery", 0, true, .also = "Draft" },
        { "Armour", 0, true, .missing = "Dagger" },
        { "Weapons", 0, true, .also = "Robe" },
        { "Shortsword", 0, true, .base_sval = SV_SHORT_SWORD },
        { "Attack", ESCAPE, false, DRAFT_PLAIN, 1 },
        { "Dagger", 'a', .base_sval = SV_DAGGER },
        { "Simple", ESCAPE, .also = "Dagger" },
        { "Shortsword", 'b', .base_sval = SV_SHORT_SWORD },
        { "Attack", ESCAPE, false, DRAFT_PLAIN, .reselected_base = true },
        { "Shortsword", ESCAPE },
        { 0 },
    };
    const editor_step prefix_steps[] = {
        { "Shortsword", 0, true },
        { "Attack", ']' },
        { "Prefix", 'a' },
        { "Prefix", 'b' },
        { "Prefix", ']', false, DRAFT_PREFIX },
        { "Stat bonuses", 'a', false, DRAFT_PREFIX, .row_value = "0 / 0 properties",
            .missing_choice = "Shortsword" },
        { "Stat bonuses", ']', false, DRAFT_PREFIX },
        { "Attack", ESCAPE, false, DRAFT_PREFIX },
        { "Shortsword", ESCAPE },
        { 0 },
    };
    const editor_step property_steps[] = {
        { "Shortsword", 'b' },
        { "Attack", '[' },
        { "Stat bonuses", 'a', .row_value = "0 / 8 properties" },
        { "Str bonus", 'a' },
        { "Str bonus", 0, true, .row_value = "+0" },
        { "Choose Strength", '6', .also = "Artifice" },
        { "Choose Strength", ESCAPE },
        { "Str bonus", '\r', .row_value = "+0" },
        { "b) +1", 0, true, .also = "Choose Strength" },
        { "Str bonus", ESCAPE, .row_value = "+1" },
        { "h) Melee", 0, true, DRAFT_ARTEFACT, .strength = 1, .row_value = "0 / 13 abilities" },
        { "Finesse", 'b' },
        { "Ability", ESCAPE },
        { "h) Melee", 'a', false, DRAFT_ARTEFACT, 0, 1, true, .row_value = "1 / 13 abilities" },
        { "Edit", 0, true, .also = "Str bonus" },
        { "c) +2", 0, true, .also = "Choose Strength" },
        { "Str bonus", ESCAPE, .row_value = "+2" },
        { "1 / 13 abilities", ']', false, DRAFT_ARTEFACT, 0, 2, true },
        { "Attack", '[', false, DRAFT_ARTEFACT, 0, 2, true, .missing_choice = "Strength" },
        { "1 / 13 abilities", 'h', false, DRAFT_ARTEFACT, 0, 2, true },
        { "Finesse", ESCAPE },
        { "1 / 13 abilities", ']', false, DRAFT_ARTEFACT, 0, 2, true },
        { "Attack", ']', false, DRAFT_ARTEFACT, 0, 2, true, .missing_choice = "Strength" },
        { "Prefix", 'a', false, DRAFT_ARTEFACT, 0, 2, true },
        { "Prefix", '[', false, DRAFT_ARTEFACT, 0, 2, true, .missing_choice = "Strength" },
        { "Attack", '[', false, DRAFT_ARTEFACT, 0, 2, true },
        { "Stat bonuses", 'a', .row_value = "1 / 8 properties" },
        { "Remove", 0, true, .also = "Str bonus" },
        { "Str bonus", ESCAPE, .missing_choice = "Remove" },
        { "Stat bonuses", ']', .row_value = "0 / 8 properties" },
        { "Attack", ESCAPE, .missing_choice = "Strength" },
        { "Shortsword", ESCAPE },
        { 0 },
    };
    const editor_step description_reader_steps[] = {
        { "Description", 'D', .missing = "longer weapon." },
        { "A belt-knife", '3', .also = "Description" },
        { "weapon.", ESCAPE, .also = "Back" },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step affix_bonus_steps[] = {
        { "Armour", 0, true },
        { "Robe", 0, true },
        { "Weight", ']', .missing_choice = "Stealth " },
        { "Prefix", 0, true },
        { "Stealthy", 0, true },
        { "Stealthy", 0, true },
        { "Stealth ", 0, true, .row_value = "+1", .skill = S_STL, .skill_bonus = 1 },
        { "b) +2", 0, true, .also = "Choose Stealth" },
        { "Stealth ", '[', .row_value = "+2", .skill = S_STL, .skill_bonus = 2 },
        { "Weight", ESCAPE, .missing_choice = "Stealth ", .skill = S_STL, .skill_bonus = 2 },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step intrinsic_bonus_steps[] = {
        { "Jewellery", 0, true },
        { "Ring of the Laiquendi", 0, true },
        { "Weight", ']', .missing_choice = "Stealth" },
        { "Stealth", 0, true, .row_value = "+1" },
        { "b) +2", 0, true, .also = "Choose Stealth" },
        { "Archery", '[', .row_value = "+1", .skill = S_STL, .skill_bonus = 2 },
        { "Weight", ESCAPE, .missing_choice = "Stealth", .skill = S_STL, .skill_bonus = 2 },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step affix_effect_steps[] = {
        { "Dagger", 0, true },
        { "Enhancements", 0, true },
        { "Suffix", 0, true },
        { "of the Haladin", 0, true, .effect = "without a suffix" },
        { "of the Haladin", 0, true, .effect = "spiders", .also = "They slay" },
        { "Suffix", 0, true, .suffix = 186 },
        { "of the Haladin", '8', .row_value = "Current", .suffix = 186 },
        { "of the Gaurwaith", 0, .effect = "ability: Throwing", .suffix = 186,
            .also = "Volume per item" },
        { "of the Haladin", ESCAPE, .row_value = "Current", .effect = "ability: Throwing" },
        { "Suffix", ESCAPE, .row_value = "of the Haladin", .suffix = 186 },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step mattock_steps[] = {
        { "Mattock", 0, true },
        { "Tunneling", '[', .row_value = "+2 (Fixed)", .tunneling = 2 },
        { "Melee powers", 0, true },
        { "Tunneling Bonus", '-', .row_value = "+2", .effect = "your tunneling by 2",
            .missing = "Toggle" },
        { "Edit", 0, true, .also = "Tunneling Bonus" },
        { "+3", 0, true, .also = "Choose Tunneling", .tunneling = 2 },
        { "Tunneling Bonus", ESCAPE, .row_value = "+3", .effect = "your tunneling by 3",
            .tunneling = 3, .also = "Strength" },
        { "Melee powers", ']', .tunneling = 3 },
        { "Tunneling", 0, true, .row_value = "+3", .tunneling = 3 },
        { "Choose Tunneling", '6', .tunneling = 3 },
        { "Choose Tunneling", ESCAPE, .effect = "your tunneling by 4", .tunneling = 3 },
        { "Tunneling", ESCAPE, .row_value = "+3", .tunneling = 3 },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step added_tunneling_steps[] = {
        { "Dagger", 0, true },
        { "Simple", '[' },
        { "Melee powers", 0, true },
        { "Add", 0, true, .effect = "your tunneling by 1" },
        { "Tunneling Bonus", 0, .row_value = "+1", .tunneling = 1 },
        { "Remove", 0, true },
        { "Tunneling Bonus", ESCAPE, .effect = "your tunneling by 1", .no_tunneling = true },
        { "Melee powers", ESCAPE },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step stone_hewer_steps[] = {
        { "Mattock", 0, true },
        { "Tunneling", ']', .row_value = "+2 (Fixed)", .tunneling = 2 },
        { "Suffix", 0, true },
        { "of the Stone-hewers", 0, true },
        { "of the Stone-hewers", 0, true, .effect = "your tunneling by 3", .tunneling = 2 },
        { "Suffix", '[', .row_value = "of the Stone-hewers", .tunneling = 3, .suffix = 51 },
        { "Tunneling", ESCAPE, .row_value = "+3 (Fixed)", .tunneling = 3 },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step description_scroll_steps[] = {
        { "Description", 0, .also = "A belt-knife", .missing = "weapon.",
            .drag_rows = 100, .description_scroll = true },
        { "weapon.", '9', .also = "Choose base item" },
        { "Description", '9' },
        { "A belt-knife", '2', .missing = "weapon." },
        { "Description", 0, true },
        { "Back", ESCAPE, .also = "warrior." },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step portrait_description_steps[] = {
        { "Dagger", 0, true, .also = "weapon.", .full_description = true },
        { "Attack", ESCAPE, .also = "weapon.", .full_description = true },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step interrupted_steps[] = {
        { "Change base item", 0, true, .also = "Resume" },
        { "Change base item", 0, true, .also = "Edit item" },
        { "Change base item", ESCAPE, .also = "Robe", .missing = "Dagger" },
        { "Resume", ESCAPE, .check_target = true },
        { 0 },
    };
    const editor_step prepaid_steps[] = {
        { "Costs", 0, true, .also = "Resume", .missing = "Edit item" },
        { "Costs already paid.", ESCAPE, .report_bill = true },
        { "Resume", ESCAPE, .also = "Shortsword",
            .missing = "Change base item", .check_target = true },
        { 0 },
    };
    const editor_step scroll_steps[] = {
        { "Dagger", '8', .base_sval = SV_DAGGER },
        { "Arrow", 0, true, .missing = "Dagger" },
        { "Simple", ESCAPE },
        { "Arrow", ESCAPE, .missing = "Dagger" },
        { 0 },
    };
    const editor_step touch_scroll_steps[] = {
        { "Dagger", 0, .base_sval = SV_DAGGER, .drag_rows = 100 },
        { "Arrow", 0, true, .missing_choice = "Dagger" },
        { "Enhancements", 0, true, .also = "Simple" },
        { "Simple", 0, true, .also = "Prefix" },
        { "Costs", 0, true },
        { "Back", 0, true, .also = "Arrows", .report_bill = true },
        { "Simple", ESCAPE },
        { "Arrow", ESCAPE, .missing = "Dagger" },
        { 0 },
    };
    const editor_step touch_actions[] = {
        { "Design", 0, true, .also = "Choose base item" },
        { "Attack", 0, true, .row_value = "+0" },
        { "Choose Attack", '6' },
        { "Apply", 0, true, .also = "Choose Attack" },
        { "Attack", 0, true, .row_value = "+1" },
        { "Back", 0, true, .also = "Choose Attack" },
        { "Costs", 0, true, .also = "Craft" },
        { "Back", 0, true, .report_bill = true },
        { "Back", 0, true, .also = "Simple" },
        { "Choose base item", ESCAPE },
        { 0 },
    };
    const editor_step artifice_scroll_steps[] = {
        { "Shortsword", 'b' },
        { "Attack", '[' },
        { "Stat bonuses", '8', .row_value = "0 / 8", .missing = "Name artefact" },
        { "Craft", '8' },
        { "Shortsword", '8', .check_target = true, .also = "Rename" },
        { "Song", '2', .check_target = true, .missing_choice = "Stat bonuses" },
        { "Shortsword", '\r', .check_target = true, .also = "Rename" },
        { "Artifact name", .name_input = "\033" },
        { "Song", ESCAPE, .check_target = true, .missing_choice = "Stat bonuses" },
        { "Shortsword", ESCAPE },
        { 0 },
    };
    const editor_step artifice_name_steps[] = {
        { "Shortsword", 'b' },
        { "Attack", '[', .missing_choice = "Shortsword" },
        { "Shortsword", 0, true, DRAFT_PLAIN, .missing = "Name artefact" },
        { "Artifact name", .name_input = "Cancelled\033" },
        { "Shortsword", 0, true, DRAFT_PLAIN },
        { "Artifact name", .name_input = "of Dawn (Test)\r" },
        { "Shortsword of Dawn (Test)", ']', .check_target = true },
        { "Shortsword of Dawn (Test)", 0, true },
        { "Artifact name", .name_input = "of Dusk\r" },
        { "Shortsword of Dusk", ESCAPE, .check_target = true },
        { "Shortsword of Dusk", ESCAPE, .missing_choice = "Shortsword of Dusk" },
        { 0 },
    };
    const editor_step cost_scroll_steps[] = {
        { "COMPLETE COSTS", 0, .also = "Forge uses", .missing = "Experience", .drag_rows = 100 },
        { "Previous", 0, true, .also = "Experience", .missing = "Forge uses" },
        { "Next", 0, true, .also = "Mithril", .missing = "Experience" },
        { "Back", 0, true, .also = "Experience", .missing = "Forge uses" },
        { 0 },
    };
    run_editor_steps("Smithing browser, editor, Costs and draft recovery", 80, 24, steps, NEW_WORK);
    run_editor_steps("Smithing prefix commit survives page changes and blocks Artifice", 80, 24, prefix_steps, NEW_WORK);
    run_editor_steps("Smithing Artifice edits values in the property list and preserves the draft", 80, 24, property_steps, NEW_WORK);
    run_editor_steps("Smithing Work Order renames the artefact and preserves a cancelled draft", 80, 24, artifice_name_steps, NEW_WORK);
    run_editor_steps("Smithing affix values are editable beside their affix", 80, 24, affix_bonus_steps, NEW_WORK);
    run_editor_steps("Smithing intrinsic values have independent editors in Enhancements", 80, 70, intrinsic_bonus_steps, NEW_WORK);
    run_editor_steps("Smithing browser, editor, Costs and draft recovery", 42, 32, steps, NEW_WORK);
    run_editor_steps("Smithing browser and editor in portrait", 80, 70, steps, NEW_WORK);
    run_editor_steps("Smithing browser reaches items below the fold and preserves scroll", 30, 12, scroll_steps, NEW_WORK);
    run_editor_steps("Smithing browser reaches items below the fold and preserves scroll", 80, 10, scroll_steps, NEW_WORK);
    run_editor_steps("Smithing cancelled replacement preserves interrupted work", 80, 24, interrupted_steps, INTERRUPTED_WORK);
    run_editor_steps("Smithing prepaid reforge can resume at an exhausted forge", 30, 12, prepaid_steps, PAID_REFORGE);
    run_editor_steps("Smithing full description opens and pages on a short screen", 30, 12, description_reader_steps, NEW_WORK);
    editor_touch = true;
    run_editor_steps("Smithing touch suffix previews show real effects and preserve the applied affix", 44, 45, affix_effect_steps, NEW_WORK);
    run_editor_steps("Smithing touch Mattock edits inherent tunneling through Artifice and Simple", 44, 45, mattock_steps, NEW_WORK);
    run_editor_steps("Smithing added tunneling has an immediate effect and can be removed", 44, 45, added_tunneling_steps, NEW_WORK);
    run_editor_steps("Smithing Stone-hewers previews and applies an increased tunneling bonus", 44, 45, stone_hewer_steps, NEW_WORK);
    run_editor_steps("Smithing portrait touch renames the artefact from the Work Order", 44, 45, artifice_name_steps, NEW_WORK);
    run_editor_steps("Smithing native touch actions design, apply, costs and back", 30, 15, touch_actions, NEW_WORK);
    run_editor_steps("Smithing drag scroll and touch targets", 30, 15, touch_scroll_steps, NEW_WORK);
    run_editor_steps("Smithing description drags independently and opens by tapping", 44, 29, description_scroll_steps, NEW_WORK);
    run_editor_steps("Smithing portrait recovers unused catalogue rows", 44, 36, portrait_description_steps, NEW_WORK);
    run_editor_steps("Smithing portrait shows the full description without scrolling", 44, 45, portrait_description_steps, NEW_WORK);
    assert(!Term_resize(30, 15));
    scenario = "Smithing report drag scroll, paging and Back";
    prepare(3);
    editor_steps = cost_scroll_steps;
    editor_step_index = 0;
    fixture_long_costs();
    assert(!editor_steps->label);
    editor_steps = NULL;
    puts("Smithing report drag scroll, paging and Back at 30x15 PASS");
    editor_touch = false;
    run_editor_steps("Smithing Artifice reaches fields below the fold", 30, 12, artifice_scroll_steps, NEW_WORK);
    run_editor_steps("Smithing browser, editor, Costs and draft recovery", 30, 12, steps, NEW_WORK);
}

static void details_checks(void)
{
    scenario = "read-only Smithing Details";
    prepare(3);
    config.menu_bigger_font[SDL_MENU_FONT_SMITHING] = true;
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_SMITHING);
    int prefix, turns, dex;
    assert(fixture_reforge_plan(selected_item, &prefix, &turns, &dex) >= 0);
    for (int mode = 0; mode < 5; ++mode)
    {
        if (mode == 2) assert(fixture_begin(selected_item, prefix));
        long xp = p_ptr->new_exp, world = turn, actions = playerturn;
        int energy = p_ptr->energy_use, uses = forge_uses(5, 5), count = source_count();
        keys = mode == 4 ? "" : "\033";
        direct_report_mode = mode;
        direct_report_frames = 0;
        fixture_details(selected_item, prefix, mode);
        direct_report_mode = -1;
        assert(direct_report_frames == (mode == 4 ? 0 : 1));
        assert(!*keys);
        assert(p_ptr->new_exp == xp && turn == world && playerturn == actions);
        assert(p_ptr->energy_use == energy && forge_uses(5, 5) == uses);
        assert(source_count() == count);
        if (mode == 3)
        {
            prepare(3);
            assert(fixture_reforge_plan(selected_item, &prefix, &turns, &dex) >= 0);
        }
    }
    puts("Smithing Details: actual preview bill, keyboard/click/hover, paid remaining work, empty blueprint and unchanged resources/turns PASS");
    set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
    sdl_pop_terminal_menu_scale();
    editor_checks();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    work = WORK_CHECKS[:WORK_CHECKS.index("static void checks(void)")]
    work = work.replace("    assert(keys && *keys);return *keys++;",
        "    if (editor_steps) return editor_step_key();\n"
        "    check_direct_report_frame();\n"
        "    assert(keys && *keys);return *keys++;")
    observer = "struct editor_step;\nstatic const struct editor_step* editor_steps;\n"
    observer += "static char editor_step_key(void);\nstatic void check_direct_report_frame(void);\n"
    source.write_text(prefix + observer + work
                      + CHECKS + init + "    assert(!init_flavor_info()); flavor_init();\n"
                      + "    details_checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = OUT / "smith.c"
    smith.write_text(SMITH + EXTRA, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj")
    response.write_text("\n".join('"' + path + '"' for path in objects
                                 if not path.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / name) for name in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("inkey", "open_inventory_item_select_menu", "handle_stuff", "msg_print",
             "request_command", "process_command", "get_sdl_config_path",
             "tutorial_game_action_allowed", "sdl_touch_only_device_active",
             "sdl_touch_menu_button_reserved_rows")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(smith), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
