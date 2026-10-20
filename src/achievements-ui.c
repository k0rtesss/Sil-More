#include "angband.h"
#include "achievements.h"
#include "achievements-play-games.h"
#include "externs.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "sdl-config.h"
#include "steamworks.h"
#include <time.h>

/* The atlas coordinates are shared with the store export, never generated art. */
static const struct { int row, col; } achievement_icons[SIL_ACH_COUNT] = {
#define SIL_ACHIEVEMENT_ICON(key, row, col, row2, col2, count) [SIL_ACH_##key] = { row, col },
#include "achievement-icons.h"
#undef SIL_ACHIEVEMENT_ICON
};

static bool platform_achievements_available(void)
{
    return sil_steamworks_active() || sil_play_games_available();
}

static void show_platform_achievements(void)
{
    if (!sil_steamworks_show_achievements() && !sil_play_games_show())
        sdl_popup_notification_show("Platform achievements are currently unavailable.");
}

static void build_achievement_book(int page)
{
    char text[768];
    int earned = 0;
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
        if (sil_achievement_unlocked_at(i)) ++earned;
    sdl_character_sheet_screen_begin_book("Achievements");
    sdl_character_sheet_screen_set_book_close_button(true);
    sdl_character_sheet_screen_set_book_close_label("Back");
    strnfmt(text, sizeof(text), "%d of %d unlocked", earned, SIL_ACH_COUNT);
    sdl_character_sheet_screen_add_book_paragraph_colored(text, TERM_L_BLUE);
    sdl_character_sheet_screen_add_book_paragraph(
        "Across all characters and Tales.");
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
    {
        const sil_achievement_definition *entry = sil_achievement_get(i);
        uint64_t unlocked = sil_achievement_unlocked_at(i);
        int row = achievement_icons[i].row, col = achievement_icons[i].col;
        if (entry->hidden && !unlocked)
        {
            /* The shared description symbol conceals the deed's identifying art. */
            sdl_character_sheet_screen_add_book_icon_paragraph(
                "Undiscovered achievement\nIts name and deed will be revealed when earned.",
                TERM_SLATE, 32, 0);
            continue;
        }
        strnfmt(text, sizeof(text), "%s: %s", unlocked ? "Unlocked" : "Locked", entry->title);
        if (unlocked)
        {
            time_t when = (time_t)unlocked;
            struct tm *date = localtime(&when);
            char formatted[32];
            if (date && strftime(formatted, sizeof(formatted), "%Y-%m-%d", date))
                strnfmt(text, sizeof(text), "Unlocked: %s (%s)", entry->title, formatted);
        }
        /* One paragraph per entry fits the shared book paragraph budget;
         * normal pagination and scrolling preserve the chosen readable font. */
        size_t used = strlen(text);
        strnfmt(text + used, sizeof(text) - used, "\n%s", entry->description);
        sdl_character_sheet_screen_add_book_icon_paragraph(text,
            unlocked ? TERM_YELLOW : TERM_SLATE, row, col);
    }
    sdl_character_sheet_screen_add_book_paragraph_colored(
        "Debug, cheat and practice games do not count.",
        TERM_SLATE);
    if (!sil_achievements_saved())
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Some achievement progress could not be saved on this device.", TERM_L_RED);
    if (platform_achievements_available())
    {
        char key[24];
        if (steamdeck_controls_active())
            controller_prompt_label(steamdeck_secondary_key(), "Y", key, sizeof(key));
        else
            SDL_strlcpy(key, "P", sizeof(key));
        strnfmt(text, sizeof(text), "%s achievements [%s]",
            sil_steamworks_active() ? "Steam" : "Google Play", key);
        sdl_character_sheet_screen_add_book_action(text, 1);
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Offline unlocks sync when platform services are available.", TERM_SLATE);
    }
    sdl_character_sheet_screen_commit_book();
    sdl_character_sheet_screen_set_book_page(page);
}

void sil_achievements_show(void)
{
    sil_achievements_init();
    sil_achievements_update();
    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
    bool old_hide_cursor = hide_cursor;
    hide_cursor = true;
    unsigned int shown_revision = sil_achievements_revision();
    build_achievement_book(0);
    while (1)
    {
        int clicked = 0, action = UI_MENU_CLICK_PRIMARY;
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        int key = inkey();
        if (shown_revision != sil_achievements_revision())
        {
            int page = sdl_character_sheet_screen_select_page();
            shown_revision = sil_achievements_revision();
            build_achievement_book(page);
        }
        if (ui_menu_click_take_action(&clicked, &action))
        {
            ui_menu_click_clear();
            if (action == UI_MENU_CLICK_HOVER) continue;
            if (clicked == SDL_SELECT_CLICK_CLOSE) break;
            if (clicked == 1) { show_platform_achievements(); continue; }
            if (clicked == SDL_SELECT_CLICK_PAGE_PREV) key = '4';
            else if (clicked == SDL_SELECT_CLICK_PAGE_NEXT) key = '6';
            else continue;
        }
        else if (key == UI_MENU_CLICK_WAKE_KEY) continue;
        ui_menu_click_clear();
        if ((key == 'p' || key == 'P'
                || (steamdeck_controls_active() && key == steamdeck_secondary_key()))
            && platform_achievements_available())
        {
            show_platform_achievements();
            continue;
        }
        key = steamdeck_menu_key(key, '4', '6');
        if (sdl_character_sheet_screen_page_turning()) continue;
        if (key == ESCAPE || key == 'q' || key == 'Q') break;
        int page = sdl_character_sheet_screen_select_page();
        int count = sdl_character_sheet_screen_select_page_count();
        if (steamdeck_controls_active() && key == steamdeck_alt_action_key()
            && page < count - 1)
            sdl_character_sheet_screen_begin_page_turn_to(count - 1);
        else if (key == '4' && page > 0)
            sdl_character_sheet_screen_begin_page_turn(-1);
        else if (key == '8' || key == 'k')
            (void)sdl_character_sheet_screen_scroll_book(-1);
        else if (key == '2' || key == 'j')
            (void)sdl_character_sheet_screen_scroll_book(1);
        else if (key == '6' || key == ' ' || key == '\r' || key == '\n')
        {
            if (page < count - 1) sdl_character_sheet_screen_begin_page_turn(1);
            else break;
        }
    }
    hide_cursor = old_hide_cursor;
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr)
    {
        p_ptr->redraw |= PR_MAP | PR_BASIC | PR_EXTRA;
        p_ptr->window |= PW_INVEN | PW_EQUIP;
        handle_stuff();
    }
}
