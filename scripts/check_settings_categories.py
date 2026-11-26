#!/usr/bin/env python3
"""Exercise production settings categories, navigation, resets and persistence.

Build the real menu for desktop and mobile platform policy. Input is supplied
at the terminal/click boundary; no synthetic SDL events, personal config, save,
browser, or visible game window are used.
"""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/settings-categories"

CHECKS = r'''
#include "cmd/ui/cmd-ui-settings.c"

static char title[128], labels[96][256];
static int choices[96], resets[96], row_count, selected;
static const unsigned char* inputs;
static int input_index, pending_choice = -9999, next_click;
static int app_saves, meta_saves, url_opens, lore_changes;
static int language_picker_calls;
static bool lore_failure;
static bool force_semantic_active;
static bool force_handheld;
static const char* config_file;
static int reset_answers[3], reset_questions, reset_requests, reset_errors;

int __real_ui_question_ask_overlay(cptr, cptr, const ui_question_option*,
    int, int, int, int);
int __real_ui_question_ask_language(cptr, cptr, const ui_question_option*,
    int, int);
int __wrap_ui_question_ask_language(cptr heading, cptr description,
    const ui_question_option* answers, int count, int initial)
{
    ++language_picker_calls;
    size_t locale_count;
    const tutorial_locale *locales = tutorial_available_locales(&locale_count);
    assert(streq(heading, ui_text("Interface language")));
    assert(streq(description,
        ui_text("Choose the language for menus, controls, status text, and messages.")));
    assert((size_t)count == locale_count && initial >= 0 && initial < count);
    assert(streq(locales[initial].code, ui_language()));
    for (int i = 0; i < count; ++i) {
        assert(streq(answers[i].label, locales[i].name));
        assert(!answers[i].disabled);
    }
    return __real_ui_question_ask_language(heading, description, answers, count, initial);
}

int __wrap_ui_question_ask_overlay(cptr heading, cptr description,
    const ui_question_option* answers, int count, int y, int x, int initial)
{
    if (streq(heading, "Progress reset failed")) {
        assert(reset_questions == 3 && reset_requests == 1);
        assert(count == 1 && initial == 0 && streq(answers[0].label, "OK"));
        assert(strstr(description, "Fixture restart failure"));
        assert(y == UI_QUESTION_GLOBAL && x == UI_QUESTION_GLOBAL);
        ++reset_errors;
        return 0;
    }
    if (!prefix(heading, "Clear all progress"))
        return __real_ui_question_ask_overlay(heading, description, answers,
            count, y, x, initial);
    assert(reset_questions < 3 && count == 2 && initial == 0);
    assert(strstr(answers[0].label, "Cancel") && answers[0].key == 'c');
    assert(answers[1].key == (reset_questions == 2 ? 'e' : 'w'));
    assert(y == UI_QUESTION_GLOBAL && x == UI_QUESTION_GLOBAL);
    assert(description && *description);
    return reset_answers[reset_questions++];
}
bool __wrap_sil_progress_reset_request_restart(char* error, size_t size)
{
    assert(reset_questions == 3);
    ++reset_requests;
    SDL_strlcpy(error, "Fixture restart failure; no personal data was touched.", size);
    return false;
}

void __wrap_sdl_character_sheet_screen_begin_select(int focus, cptr text)
{
    SDL_strlcpy(title, text, sizeof(title)); row_count = 0; selected = focus;
}
void __wrap_sdl_character_sheet_screen_add_select_row(int choice, cptr label,
    int attr, cptr desc)
{
    assert(row_count < (int)N_ELEMENTS(labels));
    choices[row_count] = choice; resets[row_count] = -1;
    SDL_strlcpy(labels[row_count++], label, sizeof(labels[0]));
}
void __wrap_sdl_character_sheet_screen_set_last_select_row_reset(int choice)
{ assert(row_count); resets[row_count - 1] = choice; }
bool __wrap_sdl_character_sheet_screen_commit_select(int focus)
{ selected = focus; return true; }
bool __wrap_ui_menu_click_take_action(int* choice, int* action)
{
    if (pending_choice == -9999) return false;
    *choice = pending_choice; *action = UI_MENU_CLICK_PRIMARY;
    pending_choice = -9999; return true;
}
char __wrap_inkey(void)
{
    assert(inputs && inputs[input_index]);
    unsigned char key = inputs[input_index++];
    if (key == 127) pending_choice = next_click;
    return (char)key;
}
bool __wrap_save_pane_config_to_json(void) { ++app_saves; return true; }
void __wrap_metarun_save_persistent_settings(void) { ++meta_saves; }
bool __wrap_SDL_OpenURL(const char* url) { assert(url && *url); ++url_opens; return true; }
const char* __wrap_get_sdl_config_path(void) { return config_file; }
void __wrap_tutorial_game_menu(const char* id, const char* description) {}
bool __wrap_sdl_menu_letters_enabled(void) { return true; }
bool __wrap_sdl_touch_only_device_active(void) { return false; }
bool __real_sdl_character_sheet_screen_active(void);
bool __wrap_sdl_character_sheet_screen_active(void)
{ return force_semantic_active || __real_sdl_character_sheet_screen_active(); }
bool __real_sdl_handheld_ui_active(void);
bool __wrap_sdl_handheld_ui_active(void)
{ return force_handheld || __real_sdl_handheld_ui_active(); }
bool __wrap_lore_set_generic(bool enabled)
{
    if (lore_failure) return false;
    op_ptr->opt[OPT_generic] = enabled; return true;
}
void __wrap_sdl_lore_changed(void) { ++lore_changes; }

static void keys(const char* text)
{ inputs = (const unsigned char*)text; input_index = 0; pending_choice = -9999; }
static void consumed(void) { assert(!inputs[input_index]); assert(!hide_cursor); }
static int find_row(const char* label)
{
    for (int i = 0; i < row_count; i++)
        if (strstr(labels[i], label)) return i;
    return -1;
}
static void page(int id, const char* text)
{ keys(text); do_cmd_options_aux(id, "Fixture Options"); consumed(); }
static void inspect(int id) { page(id, "\033"); }

static void check_categories(void)
{
    const int diagnostics[] = {OPT_show_level_generation_debug,
        OPT_show_elemental_item_rolls, OPT_show_dungeon_events,
        OPT_show_smithing_difficulty, OPT_show_smithing_difficulty_look};
    int seen[OPT_MAX] = {0};
    for (int p = 0; p < OPT_PAGE_MAX; p++)
        for (int i = 0; i < OPT_PAGE_PER; i++) {
            int opt = option_page[p][i];
            if (opt == OPT_NONE) continue;
            assert(!seen[opt]++);
            assert(get_option_groups_for_page(p));
            assert(option_group_for_setting(get_option_groups_for_page(p), opt));
            if (opt >= OPT_CHEAT && opt < OPT_ADULT) assert(p == DEBUG_PAGE);
        }
    for (int i = 0; i < (int)N_ELEMENTS(diagnostics); i++) {
        assert(option_page[DEVELOPER_PAGE][i] == diagnostics[i]);
        assert(!option_norm[diagnostics[i]]);
        assert(option_is_app_persistent(diagnostics[i]));
    }
    assert(!seen[OPT_story_character_sheet] && !seen[OPT_story_lists]);
    assert(!seen[OPT_story_lists_inven] && !seen[OPT_story_lists_equip]);
    assert(!seen[OPT_unlock_blitz_mode]);
    inspect(INTERFACE_PAGE);
    assert(find_row("Debug") < 0 && find_row("debug") < 0);
    assert(find_row("Compact Inventory Menus") >= 0);
    inspect(VISUAL_PAGE);
    assert(find_row("Color Palette") >= 0 && find_row("narrative") < 0);
    assert(find_row("Debug") < 0 && find_row("debug") < 0);
    inspect(TEXT_PAGE);
    assert(find_row("Generic Fantasy") >= 0 && find_row("Level entry narrative") >= 0);
    assert(find_row("Text Size and Fonts") >= 0);
    keys("\033"); do_cmd_pane_settings(); consumed();
    assert(streq(title, "Display and Layout"));
    assert(find_row("Big Font") < 0 && find_row("Open SDL Config") < 0);
    assert(find_row("Character Sheet") < 0 && find_row("Compact") < 0);
#if !SIL_SDL_MOBILE_BUILD
    assert(find_row("Starting Zoom") < 0 && find_row("Unsafe") < 0);
#endif
    keys("\033"); do_cmd_text_size_settings(); consumed();
    assert(streq(title, "Text Size and Fonts") && find_row("Big Font") >= 0);
    assert(find_row("Map Zoom") < 0 && find_row("Reset All") < 0);
    puts("All option rows have one category and heading; obsolete selectors and inapplicable controls stay hidden: PASS");
}

static void check_navigation(void)
{
    int focus = 1;
    p_ptr->noscore = 0; keys("p"); assert(options_menu(&focus) == 9); consumed();
    assert(find_row("Developer Settings") >= 0 && find_row("Debugging Options") < 0);
    keys("\033"); assert(options_menu(&focus) == OPTIONS_RETURN); consumed();
    keys("\177"); next_click = SETTINGS_CLICK_RETURN;
    assert(options_menu(&focus) == OPTIONS_RETURN); consumed();
    const int pages[] = {3,4,5,6,7,9};
    for (int i = 0; i < (int)N_ELEMENTS(pages); i++) {
        assert(options_menu_page_choice_turn(pages[i],1) == pages[(i+1)%6]);
        assert(options_menu_page_choice_turn(pages[i],-1) == pages[(i+5)%6]);
    }
    keys("\033"); assert(about_actions_menu(&focus) == ABOUT_ACTION_RETURN); consumed();
    assert(find_row("Palette") < 0 && find_row("Generic") < 0);
    puts("Developer is reachable in scored games; keyboard and touch Back return to play; page cycling and action grouping: PASS");
}

static void check_languages(void)
{
    int focus = 1;
    app_saves = 0;
    language_picker_calls = 0;
    tutorial_set_language("fr");
    ui_set_language("en");
    keys("l\033o"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "en") && !app_saves);
    keys("l\ro"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "en") && !app_saves);
    keys("ldo"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "de") && streq(tutorial_language(), "fr"));
    assert(streq(title, "Optionen") && app_saves == 1);
    assert(find_row("Français") >= 0 && find_row("Deutsch") >= 0);
    next_click = OPTIONS_TUTORIAL_LANGUAGE;
    keys("\177"); assert(options_menu(&focus) == OPTIONS_TUTORIAL_LANGUAGE); consumed();
    /* Select English (index 0) in the real tutorial language picker. */
    next_click = 0;
    keys("t\177o"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "de") && streq(tutorial_language(), "en"));
    assert(app_saves == 2);
    keys("leo"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "en") && app_saves == 3);

    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    tutorial_set_language("fr");
    keys("l2\ro"); do_cmd_options(); consumed();
    assert(streq(ui_language(), locales[1].code) && app_saves == 4);
    for (size_t i = 2; i < count; ++i) {
        next_click = (int)i;
        keys("l\177o"); do_cmd_options(); consumed();
        assert(streq(ui_language(), locales[i].code));
        assert(streq(tutorial_language(), "fr") && app_saves == 3 + (int)i);
        assert(find_row(locales[i].name) >= 0);
    }
    next_click = 0;
    keys("l\177o"); do_cmd_options(); consumed();
    assert(streq(ui_language(), "en") && app_saves == 3 + (int)count);
    assert(language_picker_calls == 4 + (int)count);
    tutorial_set_language("en");
    printf("All %zu interface locales are selectable through Options; keyboard, click, cancel and separate tutorial preference: PASS\n", count);
}

static void check_handheld_settings_labels(void)
{
    bool old_big=config.bigger_font;
    bool old_menus[SDL_MENU_FONT_COUNT];
    memcpy(old_menus,config.menu_bigger_font,sizeof(old_menus));
    force_handheld=true;
    config.bigger_font=false;
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++) config.menu_bigger_font[i]=false;
    force_semantic_active=true;
    keys("\033"); do_cmd_pane_settings(); consumed();
    assert(streq(title,"Display and Layout"));
    assert(find_row("Camera Vertical Recenter Distance")>=0);
    assert(find_row("Camera Horizontal Recenter Distance")>=0);
#if SIL_SDL_MOBILE_BUILD
    assert(find_row("Portrait Button Wheel Position")>=0);
    assert(find_row("Use Unsafe Area (notch/cutout area)")>=0);
#endif
    force_semantic_active=false;
    force_handheld=false;
    config.bigger_font=old_big;
    memcpy(config.menu_bigger_font,old_menus,sizeof(old_menus));
    puts("Handheld normal-font settings preserve complete semantic labels: PASS");
}

static void check_diagnostics(void)
{
    p_ptr->noscore = 0; playerturn = 10;
    inspect(DEVELOPER_PAGE);
    assert(row_count == 7 && find_row("Cheat Options") < 0);
    assert(find_row("Open SDL Config File") >= 0);
    assert(find_row("Clear all progress") >= 0);
    app_saves = meta_saves = 0;
    page(DEVELOPER_PAGE, "y2y2y2y2y\033");
    assert(app_saves == 1 && !meta_saves && !p_ptr->noscore);
    for (int i = 0; i < 5; i++) assert(op_ptr->opt[option_page[DEVELOPER_PAGE][i]]);
    for (int i = OPT_CHEAT; i < OPT_MAX; i++)
        if (i < OPT_ADULT || i >= OPT_SCORE) assert(!op_ptr->opt[i]);
    app_saves = 0; next_click = SETTINGS_CLICK_RESET_ROW_BASE;
    page(DEVELOPER_PAGE, "\177\033");
    assert(!op_ptr->opt[OPT_show_level_generation_debug] && app_saves == 1);
    url_opens = 0;
    page(DEVELOPER_PAGE, "22222r0y\033");
    assert(url_opens == 1 && !p_ptr->noscore);
    keys("\033"); do_cmd_options_aux(DEBUG_PAGE, "Cheat Options");
    assert(input_index == 0); /* Direct callers cannot bypass the score guard. */
    p_ptr->noscore = 1;
    inspect(DEVELOPER_PAGE);
    assert((find_row("Cheat Options") >= 0) == options_debug_page_available());
    puts("Diagnostics toggle and row-reset save app preferences without cheating; raw-config action is safe; cheat guard retained: PASS");
}

static void check_progress_reset_warnings(void)
{
    reset_errors = 0;
    for (int stop = 0; stop < 3; ++stop) {
        reset_questions = reset_requests = 0;
        for (int i = 0; i < 3; ++i) reset_answers[i] = i == stop ? -1 : 1;
        settings_clear_all_progress();
        assert(reset_questions == stop + 1 && !reset_requests && !reset_errors);
        reset_questions = 0;
        reset_answers[stop] = 0; /* Cancel button is equally safe. */
        settings_clear_all_progress();
        assert(reset_questions == stop + 1 && !reset_requests && !reset_errors);
    }
    reset_questions = reset_requests = 0;
    for (int i = 0; i < 3; ++i) reset_answers[i] = 1;
    settings_clear_all_progress();
    assert(reset_questions == 3 && reset_requests == 1 && reset_errors == 1);
    reset_questions = reset_requests = 0;
    settings_extra_row_handle(SETTINGS_EXTRA_CLEAR_PROGRESS, 'r');
    settings_extra_row_handle(SETTINGS_EXTRA_CLEAR_PROGRESS, 'n');
    assert(!reset_questions && !reset_requests);
    p_ptr->noscore = 0;
    inspect(DEVELOPER_PAGE);
    int row = find_row("Clear all progress"); assert(row >= 0);
    next_click = choices[row]; reset_answers[0] = 0;
    page(DEVELOPER_PAGE, "\177\177\033");
    assert(reset_questions == 1 && !reset_requests && !p_ptr->noscore);
    puts("Clear all progress: three cancel-default warnings, Escape/cancel at every step, final erase key, touch action and visible restart failure: PASS");
}

static void check_extra_controls(void)
{
    set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY, false);
    set_sdl_compact_inventory_menus(false);
    inspect(INTERFACE_PAGE);
    int row = find_row("Compact Inventory Menus"); assert(row >= 0);
    next_click = choices[row];
    page(INTERFACE_PAGE, "\177\177\033");
    assert(get_sdl_compact_inventory_menus());
    next_click = SETTINGS_CLICK_RESET_ROW_BASE + choices[row];
    page(INTERFACE_PAGE, "\177\033");
    assert(get_sdl_compact_inventory_menus() == false);
    set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY, true);
    inspect(INTERFACE_PAGE); assert(find_row("Compact Inventory Menus") < 0);
    set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY, false);
    assert(!settings_extra_row_handle(SETTINGS_EXTRA_PALETTE, 'r'));
    keys("\033"); settings_extra_row_handle(SETTINGS_EXTRA_PALETTE, ' '); consumed();
    op_ptr->opt[OPT_generic] = false; lore_changes = 0;
    page(TEXT_PAGE, "y\033"); assert(op_ptr->opt[OPT_generic] && lore_changes == 1);
    page(TEXT_PAGE, "n\033"); assert(!op_ptr->opt[OPT_generic] && lore_changes == 2);
    lore_failure = true;
    page(TEXT_PAGE, "y\033"); assert(!op_ptr->opt[OPT_generic]);
    lore_failure = false;
    puts("Inventory layout toggles/reset/hiding, palette action, and immediate lore updates with failed-load recovery: PASS");
}

static void check_persistence(void)
{
    struct sdl_pane_profile profiles[SDL_PANE_PROFILE_COUNT] = {0};
    op_ptr->opt[OPT_show_smithing_difficulty] = true;
    op_ptr->opt[OPT_show_dungeon_events] = true;
    op_ptr->opt[OPT_generic] = true;
    op_ptr->opt[OPT_ignore_monster_alarms] = true;
    op_ptr->opt[OPT_minimum_text_mode] = true;
    op_ptr->intro_style = INTRO_STYLE_HURIN;
    op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_OFF;
    ui_set_language("de");
    tutorial_set_language("fr");
    assert(sdl_config_save(config_file, &config, profiles, SDL_PANE_PROFILE_COUNT));
    ui_set_language("en");
    tutorial_set_language("en");
    op_ptr->opt[OPT_show_smithing_difficulty] = false;
    op_ptr->opt[OPT_show_dungeon_events] = false;
    op_ptr->opt[OPT_generic] = false;
    op_ptr->opt[OPT_ignore_monster_alarms] = false;
    op_ptr->opt[OPT_minimum_text_mode] = false;
    op_ptr->intro_style = 0;
    op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER;
    sdl_config_load_app_options(config_file);
    assert(streq(ui_language(), "de") && streq(tutorial_language(), "fr"));
    assert(op_ptr->opt[OPT_show_smithing_difficulty]);
    assert(op_ptr->opt[OPT_show_dungeon_events] && op_ptr->opt[OPT_generic]);
    assert(op_ptr->opt[OPT_ignore_monster_alarms]);
    assert(op_ptr->opt[OPT_minimum_text_mode]);
    assert(op_ptr->intro_style == INTRO_STYLE_HURIN);
    assert(op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_OFF);
    puts("Moved diagnostics, lore and narrative options round-trip through their existing JSON groups: PASS");

    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    for (size_t i = 0; i < count; ++i) {
        cptr interface_code = locales[i].code;
        cptr tutorial_code = locales[(i + 1) % count].code;
        ui_set_language(interface_code);
        tutorial_set_language(tutorial_code);
        assert(sdl_config_save(config_file, &config, profiles, SDL_PANE_PROFILE_COUNT));
        ui_set_language(tutorial_code);
        tutorial_set_language(interface_code);
        sdl_config_load_app_options(config_file);
        assert(streq(ui_language(), interface_code));
        assert(streq(tutorial_language(), tutorial_code));
    }

    const char bad_config[] = "{\"appOptions\":{\"uiLanguage\":\"invalid\",\"tutorialLanguage\":\"fr\"}}";
    assert(SDL_SaveFile(config_file, bad_config, sizeof(bad_config) - 1));
    sdl_config_load_app_options(config_file);
    assert(streq(ui_language(), "en") && streq(tutorial_language(), "fr"));
    ui_set_language("de");
    const char old_config[] = "{\"appOptions\":{\"tutorialLanguage\":\"fr\"}}";
    assert(SDL_SaveFile(config_file, old_config, sizeof(old_config) - 1));
    sdl_config_load_app_options(config_file);
    assert(streq(ui_language(), "en") && streq(tutorial_language(), "fr"));
    printf("All %zu interface locales round-trip separately; legacy and invalid settings select English: PASS\n", count);
}

static void check_minimum_text_option(void)
{
    assert(!option_norm[OPT_minimum_text_mode]);
    assert(option_is_app_persistent(OPT_minimum_text_mode));
    op_ptr->opt[OPT_minimum_text_mode] = true;
    sdl_config_load_app_options(config_file);
    assert(!op_ptr->opt[OPT_minimum_text_mode]);
    inspect(GAMEPLAY_PAGE);
    int row = find_row("Minimum text mode");
    assert(row >= 0);
    next_click = choices[row];
    page(GAMEPLAY_PAGE, "\177\033");
    assert(op_ptr->opt[OPT_minimum_text_mode]);
    next_click = SETTINGS_CLICK_RESET_ROW_BASE + choices[row];
    page(GAMEPLAY_PAGE, "\177\033");
    assert(!op_ptr->opt[OPT_minimum_text_mode]);
    puts("Minimum text: Gameplay row, default, legacy config, touch toggle and reset PASS");
}

static void check_alarm_option(void)
{
    assert(!option_norm[OPT_ignore_monster_alarms]);
    assert(option_is_app_persistent(OPT_ignore_monster_alarms));
    /* Older configurations have no alarm preference; defaults must win over
     * any value inherited from a save or a previous character. */
    op_ptr->opt[OPT_ignore_monster_alarms] = true;
    sdl_config_load_app_options(config_file);
    assert(!op_ptr->opt[OPT_ignore_monster_alarms]);
    inspect(GAMEPLAY_PAGE);
    int row = find_row("Ignore monster alarms");
    if (row < 0) row = find_row("Ignore alarms");
    assert(row >= 0);
    next_click = choices[row];
    page(GAMEPLAY_PAGE, "\177\177\033");
    assert(op_ptr->opt[OPT_ignore_monster_alarms]);
    next_click = SETTINGS_CLICK_RESET_ROW_BASE + choices[row];
    page(GAMEPLAY_PAGE, "\177\033");
    assert(!op_ptr->opt[OPT_ignore_monster_alarms]);
    puts("Ignore monster alarms: Gameplay row, off default, legacy-config default, touch toggle and reset: PASS");
}
static void check_ui_messages(void)
{
    const char *story = "With a voice as of rolling thunder, Morgoth, Lord of Darkness, speaks:";
    bool was_generated = character_generated;
    character_generated = true;
    ui_set_language("de");
    const char *german_menu = ui_text("main menu");
    assert(streq(german_menu, "Hauptmenü"));
    assert(streq(ui_message_text("Options"), "Options"));
    msg_print("You are blind!");
    assert(streq(message_str(0), "Du bist blind!"));
    msg_format("%^s misses you.", "testork");
    assert(streq(message_str(0), "Testork verfehlt dich."));
    assert(message_color(0) == TERM_L_GREEN);
    msg_print(story);
    assert(streq(message_str(0), story));

    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    cptr label = "main menu";
    cptr message = "You are blind!";
    for (size_t i = 1; i < count; ++i) {
        ui_set_language(locales[i].code);
        assert(ui_text(label) != label);
        assert(ui_message_text(message) != message);
        assert(streq(german_menu, "Hauptmenü"));
    }
    ui_set_language("de");
    assert(ui_text("main menu") == german_menu);
    ui_set_language("en");
    msg_print("You are blind!");
    assert(streq(message_str(0), "You are blind!"));
    assert(streq(message_str(3), "Du bist blind!"));
    character_generated = was_generated;
    puts("Gameplay message language, formatted values, history, and story fallback: PASS");
}

static void save_ui_catalogue_fixture(cptr directory, cptr code, cptr contents)
{
    char filename[32], path[2048];
    strnfmt(filename, sizeof(filename), "ui.%s.json", code);
    assert(path_build(path, sizeof(path), directory, filename));
    assert(SDL_SaveFile(path, contents, strlen(contents)));
}

static void check_catalogue_fallbacks(cptr directory)
{
    cptr original_help = ANGBAND_DIR_HELP;
    cptr missing_entry = "Fixture missing entry";
    cptr invalid_format = "Fixture %d";
    cptr positional_format = "Fixture %s %d";
    cptr unsafe_format = "Fixture %n";
    cptr empty_value = "Fixture empty";
    cptr valid_format = "Fixture %s: %02d%%";

    ui_language_cleanup();
    ui_set_language("cs");
    ANGBAND_DIR_HELP = NULL;
    assert(ui_text(valid_format) == valid_format);

    save_ui_catalogue_fixture(directory, "cs",
        "{\"version\":1,\"language\":\"cs\",\"strings\":{\"Fixture %d\":\"Local %s\","
        "\"Fixture %s %d\":\"Local %2$d %1$s\",\"Fixture %n\":\"Local %n\","
        "\"Fixture empty\":\"\",\"Fixture %s: %02d%%\":\"Local %s: %02d%%\"},"
        "\"messages\":{\"Fixture message\":\"Local message\"}}");
    save_ui_catalogue_fixture(directory, "da",
        "{\"version\":1,\"strings\":{\"Fixture %s: %02d%%\":\"Local %s: %02d%%\","
        "\"Fixture %s: %02d%%\":false},\"messages\":{}}");
    save_ui_catalogue_fixture(directory, "es", "{\"version\":1,\"strings\":{},");
    save_ui_catalogue_fixture(directory, "fr",
        "{\"version\":2,\"strings\":{\"Fixture %s: %02d%%\":\"Local %s: %02d%%\"},"
        "\"messages\":{}}");
    save_ui_catalogue_fixture(directory, "it",
        "{\"version\":1,\"language\":\"es\",\"strings\":{\"Fixture %s: %02d%%\":\"Local %s: %02d%%\"},"
        "\"messages\":{}}");

    ANGBAND_DIR_HELP = directory;
    assert(streq(ui_text(valid_format), "Local %s: %02d%%"));
    assert(streq(ui_message_text("Fixture message"), "Local message"));
    assert(ui_text(missing_entry) == missing_entry);
    assert(ui_text(invalid_format) == invalid_format);
    assert(ui_text(positional_format) == positional_format);
    assert(ui_text(unsafe_format) == unsafe_format);
    assert(ui_text(empty_value) == empty_value);
    assert(ui_text(NULL) == NULL && ui_message_text(NULL) == NULL);

    const char *fallback_codes[] = {"bg", "da", "es", "fr", "it"};
    for (size_t i = 0; i < N_ELEMENTS(fallback_codes); ++i) {
        ui_set_language(fallback_codes[i]);
        assert(streq(ui_language(), fallback_codes[i]));
        assert(ui_text(valid_format) == valid_format);
    }

    ui_set_language("unknown");
    assert(streq(ui_language(), "en") && ui_text(valid_format) == valid_format);
    ui_set_language("cs");
    ui_set_language(NULL);
    assert(streq(ui_language(), "en"));
    ui_language_cleanup();
    ANGBAND_DIR_HELP = original_help;
    puts("Missing, malformed, mislabelled, duplicate and invalid-format UI translations fall back safely: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"\n', '')
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    body = prefix + fixture_function("terminal_extra") + "\n" + CHECKS + init
    body += '''    sdl_config_set_defaults(&config);
    char filename[2048]; strnfmt(filename, sizeof(filename), "%s/settings.json", argv[2]);
    config_file = filename;
    ANGBAND_DIR_HELP = argv[3];
    check_categories(); check_navigation(); check_languages(); check_handheld_settings_labels(); check_diagnostics();
    check_progress_reset_warnings(); check_extra_controls();
    check_alarm_option(); check_minimum_text_option(); check_persistence(); check_ui_messages();
    check_catalogue_fallbacks(argv[2]);
    SDL_Quit(); return 0;
}
'''
    harness = OUT / "check.c"
    harness.write_text(body.replace("assert(argc==3);", "assert(argc==4);"), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wrappers = ["inkey", "save_pane_config_to_json", "metarun_save_persistent_settings",
        "sdl_character_sheet_screen_begin_select", "sdl_character_sheet_screen_add_select_row",
        "sdl_character_sheet_screen_set_last_select_row_reset", "sdl_character_sheet_screen_commit_select",
        "ui_menu_click_take_action", "SDL_OpenURL", "get_sdl_config_path",
        "tutorial_game_menu", "sdl_menu_letters_enabled", "sdl_touch_only_device_active",
        "sdl_character_sheet_screen_active",
        "sdl_handheld_ui_active",
        "lore_set_generic", "sdl_lore_changed", "ui_question_ask_overlay", "ui_question_ask_language",
        "sil_progress_reset_request_restart"]
    for name, defines in (("desktop", []), ("mobile-policy", ["-DSIL_IOS"])):
        exe = OUT / f"check-{name}.exe"
        subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
            *defines, "@CMakeFiles/sil-more.dir/includes_C.rsp", str(harness),
            "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            *("-Wl,--wrap=" + symbol for symbol in wrappers), "-o", str(exe)],
            cwd=BUILD, env=env, check=True)
        with tempfile.TemporaryDirectory(prefix=name + "-", dir=OUT) as state:
            for catalog in (ROOT / "lib/pref").glob("generic_lore*.json"):
                shutil.copy2(catalog, state)
            print(name, flush=True)
            subprocess.run([str(exe), str(ROOT / "lib/edit"), state, str(ROOT / "lib/help")],
                cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
