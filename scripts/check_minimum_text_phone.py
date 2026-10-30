#!/usr/bin/env python3
"""Render short race/hero choices at phone sizes with production SDL layout."""
import check_phone_books_layout as base

CHECK = r'''
static char minimum_test_inkey(void);
/* MINIMUM BIRTH CHOICE SOURCE */
static int actual_race_stage;
static char minimum_test_inkey(void)
{
    const char* expected = actual_race_stage == 0 ? "Feanorians" : "Fingolfinrim";
    reached[0] = 0;
    frame();
    expect(strstr(reached, "STR") && strstr(reached, "GRA"), "race stats missing after carousel navigation");
    expect(!strcmp(g_sdl_character_sheet_screen.select_focus_title, expected),
        "focused race title does not follow carousel");
    expect(strstr(reached, expected) != NULL, "focused race name not rendered");
    capture(actual_race_stage == 0 ? "actual-race-first" : "actual-race-next");
    return actual_race_stage++ == 0 ? '6' : '\r';
}
static void actual_race_detail(birth_menu choice)
{
    (void)choice;
    sdl_character_sheet_screen_add_select_detail("STR\t+2", TERM_L_BLUE, "Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("DEX\t+3", TERM_L_BLUE, "Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("CON\t+3", TERM_L_BLUE, "Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("GRA\t+4", TERM_L_BLUE, "Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("Melee Affinity", TERM_GREEN, "Trait bonus");
}
static void check_actual_race_choice(void)
{
    birth_menu choices[] = {{false, "Feanorians", ""}, {false, "Fingolfinrim", ""}};
    cptr headings[] = {"Noldor", NULL};
    birth_select_page page = {"Choose Race", NULL, NULL, NULL, headings, 4, 0, 1, false};
    SDL_strlcpy(fixture_id, "minimum-actual-race", sizeof(fixture_id));
    reached[0] = 0;
    op_ptr->opt[OPT_minimum_text_mode] = true;
    actual_race_stage = 0;
    fixture_assert(minimum_test_get_player_choice(choices, 2, 0, actual_race_detail, &page) == 1);
    fixture_assert(actual_race_stage == 2);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}

static void check_minimum_choice(bool hero_mode)
{
    static cptr races[] = {"Feanorians", "Fingolfinrim", "Finarfinrim", "Sindar", "Naugrim", "Edain"};
    static cptr heroes[] = {"Feanor", "Maedhros", "Celebrimbor"};
    SDL_strlcpy(fixture_id, hero_mode ? "minimum-hero" : "minimum-race", sizeof(fixture_id));
    reached[0] = 0;
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_select(0, hero_mode ? "Choose Hero" : "Choose Race");
    cptr* names = hero_mode ? heroes : races;
    int count = hero_mode ? 3 : 6;
    for (int i = 0; i < count; ++i)
        sdl_character_sheet_screen_add_select_row(i, names[i], TERM_WHITE, "");
    if (hero_mode)
        sdl_character_sheet_screen_set_select_title_detail("Feanor, Spirit of Fire", " *** mighty", TERM_GREEN);
    else
        sdl_character_sheet_screen_set_select_title_detail(races[0], "", TERM_WHITE);
    sdl_character_sheet_screen_set_select_detail_size_hint(4, hero_mode ? 2 : 0, 6);
    static cptr stats[] = {"STR\t+2", "DEX\t+3", "CON\t+3", "GRA\t+4"};
    for (int i = 0; i < 4; ++i)
        sdl_character_sheet_screen_add_select_detail(stats[i], TERM_L_BLUE, "Attribute bonus");
    if (hero_mode) {
        sdl_character_sheet_screen_set_select_ability_rows(2);
        sdl_character_sheet_screen_add_select_detail("Artifice", TERM_RED, "Starting ability");
        sdl_character_sheet_screen_add_select_detail("Jeweler", TERM_RED, "Starting ability");
    }
    for (int i = 0; i < 6; ++i) {
        char line[40]; strnfmt(line, sizeof(line), "Trait %d Affinity", i + 1);
        sdl_character_sheet_screen_add_select_detail(line, TERM_GREEN, "Trait bonus");
    }
    fixture_assert(sdl_character_sheet_screen_commit_select(0));
    scroll_to_end(hero_mode ? "minimum-hero" : "minimum-race");
    expect(strstr(reached, hero_mode ? "Feanor, Spirit of Fire" : "Feanorians") != NULL,
        "selected name missing from carousel");
    for (int i = 0; i < 4; ++i)
        expect(strstr(reached, ((cptr[]){"STR", "DEX", "CON", "GRA"})[i]) != NULL,
            "minimum-text stat unreachable");
    expect(strstr(reached, "Trait 6 Affinity") != NULL, "minimum-text trait unreachable");
    if (hero_mode)
        expect(strstr(reached, "Artifice") && strstr(reached, "Jeweler"), "minimum-text abilities unreachable");
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}
'''


def main():
    harness = base.HARNESS.replace('if(sdl_character_sheet_screen_mobile_carousel_active()) {',
        'if(sdl_character_sheet_screen_mobile_carousel_active() && !strcmp(fixture_id, "hero-carousel")) {', 1)
    harness = harness.replace('static void check_hero(void)', CHECK + '\nstatic void check_hero(void)', 1)
    birth_choice = (base.ROOT / "src/birth/birth-choice-ui.c").read_text(encoding="utf-8")
    birth_header = (base.ROOT / "src/birth/birth-internal.h").read_text(encoding="utf-8")
    birth_header = birth_header.replace('#include "externs.h"\n', '')
    birth_choice = birth_choice.replace('#include "birth/birth-internal.h"', birth_header)
    birth_choice = birth_choice.replace("int get_player_choice(", "int minimum_test_get_player_choice(")
    birth_choice = birth_choice.replace("c = inkey();", "c = minimum_test_inkey();", 1)
    harness = harness.replace("/* MINIMUM BIRTH CHOICE SOURCE */", birth_choice, 1)
    harness = harness.replace("books/hero/allocation %s", "minimum-text choices %s")
    harness = harness.replace('    check_books(false);check_books(true);check_hero();check_allocation();',
        '    check_actual_race_choice();check_minimum_choice(false);check_minimum_choice(true);', 1)
    base.HARNESS = harness
    base.OUT = base.ROOT / "scripts/output/minimum-text-phone-layout"
    base.main()


if __name__ == "__main__":
    main()
