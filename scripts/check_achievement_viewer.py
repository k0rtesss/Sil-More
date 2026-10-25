#!/usr/bin/env python3
"""Render the production achievement book with real fonts in a hidden SDL fixture."""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STEAM = os.environ.get("ACHIEVEMENTS_STEAM") == "1"
BUILD = ROOT / ("build-steam" if STEAM else "build-standard")
OUT = ROOT / ("scripts/output/achievement-viewer-steam-check" if STEAM
              else "scripts/output/achievement-viewer-check")

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "achievements-ui.c"

/* HERO FIXTURE */
static float fixture_density = 1.0f;
static bool fixture_mobile, fixture_hero;
static int fixture_width, fixture_height;
static char startup_messages[SIL_ACH_COUNT][1024];
static int startup_message_count;
static int startup_panel_count;
static const char *fixture_screenshot;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display)
{ (void)display; return fixture_density; }
bool __wrap_sdl_mobile_portrait_layout_active(void)
{ return fixture_mobile && fixture_height > fixture_width; }
bool __wrap_sdl_touch_only_device_active(void)
{ return fixture_mobile; }
/* This renderer fixture has no dungeon/object templates for map redraws. */
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void) { return false; }
void __wrap_handle_stuff(void) {}
void __wrap_msg_format(cptr format, ...)
{
    if (startup_message_count >= SIL_ACH_COUNT) exit(3);
    va_list args; va_start(args, format);
    vsnprintf(startup_messages[startup_message_count++], 1024, format, args);
    va_end(args);
}
enum { SDL_CHAR_SHEET_TEXT_LEN = WRAP_TEXT_LEN, SDL_CHAR_SHEET_MAX_LINES = WRAP_MAX_LINES };
/* Fail headlessly: the Windows CRT assertion dialog would stall CI. */
#undef assert
#define assert(check) do { if (!(check)) { fprintf(stderr,"Fixture assertion: %s at line %d\n",#check,__LINE__); exit(3); } } while (0)
extern int sdl_char_sheet_wrap_text(TTF_Font*, cptr, float,
    char (*)[SDL_CHAR_SHEET_TEXT_LEN], int);

static bool feanor_deed(int id)
{
    return id == SIL_ACH_A_SINGLE_SPARK || id == SIL_ACH_THESE_THINGS_ARE_MINE
        || id == SIL_ACH_THE_OATH_HE_SHOULD_HAVE_SWORN || id == SIL_ACH_HIS_FATHERS_LIGHT;
}

static bool description_for_hero(cptr description, cptr name)
{
    char prefix[128];
    strnfmt(prefix, sizeof(prefix), "As %s,", name);
    return !strncmp(description, prefix, strlen(prefix))
        || (!strcmp(name, "Túrin Turambar")
            && !strncmp(description, "As Túrin,", strlen("As Túrin,")));
}

char __wrap_inkey(void)
{
    sdl_character_sheet_screen_state *book = &g_sdl_character_sheet_screen;
    assert(sdl_character_sheet_screen_active());
    assert(!strcmp(book->narrative_title, "Character achievements"));
    assert(book->narrative_close_enabled);
    assert(!strcmp(book->narrative_close_label, "Done"));
    ++startup_panel_count;
    int expected = 0, entries = 0;
    for (int id = 0; id < SIL_ACH_COUNT; ++id) {
        const sil_achievement_definition *entry = sil_achievement_get(id);
        if (!entry) continue;
        bool matches = description_for_hero(entry->description,
            fixture_heroes[p_ptr->pcharacter].name)
            && (!entry->hidden || sil_achievement_unlocked_at(id));
        expected += matches;
        int found = 0;
        for (int para = 0; para < book->narrative_para_count; ++para)
            if (strstr(book->narrative_paras[para], entry->description)) {
                assert(strstr(book->narrative_paras[para], entry->title));
                assert(!strncmp(book->narrative_paras[para],
                    sil_achievement_unlocked_at(id) ? "Unlocked: " : "Locked: ",
                    sil_achievement_unlocked_at(id) ? 10 : 8));
                assert(book->narrative_icon_row[para] == achievement_icons[id].row);
                ++found;
            }
        assert(found == matches);
    }
    for (int para = 0; para < book->narrative_para_count; ++para) {
        assert(book->narrative_para_choice[para] < 0);
        assert(!strstr(book->narrative_paras[para], "Undiscovered achievement"));
        entries += book->narrative_icon_row[para] >= 0;
    }
    assert(expected > 0 && entries == expected);
    assert(book->narrative_icon_row[0] >= 0); /* First page leads with a deed. */
    /* Fëanor has the longest list: inspect every rendered page, wrap and hitbox
     * at every desktop/mobile density and normal/Big Font fixture size. */
    if (p_ptr->pcharacter == 1) {
        sdl_character_sheet_screen_render();
        assert(book->narrative_page_count > 0);
        for (int page = 0; page < book->narrative_page_count; ++page) {
            sdl_character_sheet_screen_set_book_page(page);
            assert(SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255));
            assert(SDL_RenderClear(g_state.renderer));
            ui_menu_click_begin(); sdl_character_sheet_screen_render();
            assert(book->narrative_page_count > 0);
            bool close = false;
            for (int hit = 0; hit < book->hit_count; ++hit) {
                SDL_FRect rect = book->hits[hit].rect;
                assert(rect.x >= -1 && rect.y >= -1);
                assert(rect.x + rect.w <= fixture_width + 1);
                assert(rect.y + rect.h <= fixture_height + 1);
                close |= book->hits[hit].choice == SDL_SELECT_CLICK_CLOSE;
            }
            assert(close);
            TTF_Font *font = sdl_story_font_for_height_slot(book->narrative_body_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
            float line_h = sdl_char_sheet_line_h(font, book->narrative_body_px, 1.28f);
            float width = book->select_scroll_rect.w;
            float d = fixture_mobile ? fixture_density : 1;
            for (int para = 0; para < book->narrative_para_count; ++para) {
                char lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
                float gutter = book->narrative_icon_row[para] >= 0
                    ? MIN(line_h * 1.6f, width <= 680.0f*d
                        ? MIN(40.0f*d, width * 0.12f) : width * 0.18f) : 0;
                float text_w = width - gutter;
                int count = sdl_char_sheet_wrap_text(font, book->narrative_paras[para],
                    text_w, lines, SDL_CHAR_SHEET_MAX_LINES);
                assert(count == book->narrative_para_lines[para]);
                for (int line = 0; line < count; ++line)
                    assert(sdl_char_sheet_text_width(font, lines[line]) <= (int)text_w);
            }
            int steps = 0;
            while (sdl_character_sheet_screen_scroll_book(1)) assert(++steps < 256);
            assert(book->sheet_scroll == book->sheet_scroll_max);
            assert(SDL_RenderClear(g_state.renderer));
            ui_menu_click_begin(); sdl_character_sheet_screen_render();
            SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
            assert(pixels);
            char path[256];
            strnfmt(path, sizeof(path), "%s-startup-%dx%d-page%d.png",
                fixture_screenshot, fixture_width, fixture_height, page + 1);
            assert(IMG_SavePNG(pixels, path)); SDL_DestroySurface(pixels);
        }
        bool wake = false;
        assert(ui_menu_click_handle_choice_action(SDL_SELECT_CLICK_CLOSE,
            UI_MENU_CLICK_PRIMARY, &wake));
        return UI_MENU_CLICK_WAKE_KEY;
    }
    return ESCAPE;
}

static void check_character_start(void)
{
    int original = p_ptr->pcharacter;
    int hidden_locked = 0, hidden_earned = 0;
    for (int hero = 0; hero < z_info->c_max; ++hero) {
        p_ptr->pcharacter = hero;
        int expected = 0;
        for (int id = 0; id < SIL_ACH_COUNT; ++id) {
            const sil_achievement_definition *entry = sil_achievement_get(id);
            bool matches = entry && description_for_hero(entry->description, fixture_heroes[hero].name);
            if (sil_achievement_for_current_character(id) != matches)
                fprintf(stderr, "Character mapping mismatch hero=%s id=%d expected=%d actual=%d description=%s\n",
                    fixture_heroes[hero].name,id,matches,sil_achievement_for_current_character(id),entry ? entry->description : "disabled");
            assert(sil_achievement_for_current_character(id) == matches);
            bool visible = entry && (!entry->hidden || sil_achievement_unlocked_at(id));
            expected += matches && visible;
            hidden_locked += matches && entry->hidden && !visible;
            hidden_earned += matches && entry->hidden && visible;
        }
        startup_message_count = startup_panel_count = 0;
        int saved_icky = character_icky, saved_energy = p_ptr->energy_use;
        bool saved_cursor = hide_cursor;
        show_hidden_requirements = true; /* Prior viewer toggles cannot spoil startup. */
        sil_achievement_character_on_start(true);
        assert(startup_message_count == expected);
        assert(startup_panel_count == (expected > 0));
        assert(!sdl_character_sheet_screen_active());
        assert(character_icky == saved_icky && hide_cursor == saved_cursor);
        assert(playerturn == 0 && p_ptr->energy_use == saved_energy);
        for (int id = 0; id < SIL_ACH_COUNT; ++id) {
            const sil_achievement_definition *entry = sil_achievement_get(id);
            if (!entry || !description_for_hero(entry->description, fixture_heroes[hero].name)) continue;
            int found = 0;
            for (int n = 0; n < startup_message_count; ++n)
                if (strstr(startup_messages[n], entry->title)) {
                    assert(strstr(startup_messages[n], sil_achievement_unlocked_at(id)
                        ? "already unlocked" : "available"));
                    if (!sil_achievement_unlocked_at(id))
                        assert(strstr(startup_messages[n], entry->description));
                    ++found;
                }
            assert(found == (!entry->hidden || sil_achievement_unlocked_at(id)));
        }
    }
#ifdef FIXTURE_HIDDEN_CHARACTERS
    assert(hidden_locked + hidden_earned > 0);
    printf("Hidden character catalogue: %d undiscovered deeds omitted, %d earned hidden deeds shown PASS\n",
        hidden_locked, hidden_earned);
#endif
    show_hidden_requirements = false;
    p_ptr->pcharacter = 1; /* Fëanor has multiple deeds. */
    startup_message_count = startup_panel_count = 0;
    sil_achievement_character_on_start(false);
    p_ptr->restoring = true; sil_achievement_character_on_start(true); p_ptr->restoring = false;
    playerturn = 1; sil_achievement_character_on_start(true); playerturn = 0;
    p_ptr->wizard = true; sil_achievement_character_on_start(true); p_ptr->wizard = false;
    p_ptr->game_type = 1; sil_achievement_character_on_start(true); p_ptr->game_type = 0;
    p_ptr->is_dead = true; sil_achievement_character_on_start(true); p_ptr->is_dead = false;
    p_ptr->playing = false; sil_achievement_character_on_start(true); p_ptr->playing = true;
    assert(startup_message_count == 0);
    assert(startup_panel_count == 0);
    p_ptr->pcharacter = z_info->c_max;
    assert(!sil_achievement_for_current_character(SIL_ACH_A_SINGLE_SPARK));
    sil_achievement_character_on_start(true); assert(startup_message_count == 0);
    assert(startup_panel_count == 0);
    p_ptr->pcharacter = original;
}

static SDL_Rect green_ink(SDL_Surface* surface, SDL_Rect region)
{
    int left=surface->w, top=surface->h, right=-1, bottom=-1;
    for (int y=MAX(0,region.y); y<MIN(surface->h,region.y+region.h); ++y)
        for (int x=MAX(0,region.x); x<MIN(surface->w,region.x+region.w); ++x) {
            Uint8 r,g,b,a;
            assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
            if ((int)g*a/255>50 && ((g>r*2 && g>b*2)
                || (fixture_mobile && g>r*2 && b>r*2))) {
                left=MIN(left,x); right=MAX(right,x); top=MIN(top,y); bottom=MAX(bottom,y);
            }
        }
    assert(right>=left && bottom>=top);
    return (SDL_Rect){left,top,right-left+1,bottom-top+1};
}

static int gray_pixels(SDL_Surface* surface, SDL_Rect region)
{
    int pixels = 0;
    for (int y=MAX(0,region.y); y<MIN(surface->h,region.y+region.h); ++y)
        for (int x=MAX(0,region.x); x<MIN(surface->w,region.x+region.w); ++x) {
            Uint8 r,g,b,a;
            assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
            if ((int)r*a/255 > 50 && abs((int)r-g) <= 1 && abs((int)g-b) <= 1)
                ++pixels;
        }
    return pixels;
}

static void check_catalogue(sdl_character_sheet_screen_state *book, bool requirements)
{
    bool seen[SIL_ACH_COUNT] = {0};
    const char *category = NULL;
    int para = 2, headings = 0, hidden = 0, progress_count = 0;
    bool any_locked = false;
    int order[SIL_ACH_COUNT], count = 0;
    if (fixture_hero) {
        const int first[] = { SIL_ACH_A_SINGLE_SPARK, SIL_ACH_THESE_THINGS_ARE_MINE,
            SIL_ACH_THE_OATH_HE_SHOULD_HAVE_SWORN, SIL_ACH_HIS_FATHERS_LIGHT };
        for (unsigned n = 0; n < N_ELEMENTS(first); ++n)
            order[count++] = first[n];
    }
    for (int position = 0; position < SIL_ACH_COUNT; ++position)
        if (!fixture_hero || !feanor_deed(achievement_order[position].id))
            order[count++] = achievement_order[position].id;
    assert(count == SIL_ACH_COUNT);
    for (int position = 0; position < count; ++position) {
        int id = order[position];
        assert(id >= 0 && id < SIL_ACH_COUNT && !seen[id]);
        if (!sil_achievement_get(id)) continue;
        seen[id] = true;
        const char *section = fixture_hero && feanor_deed(id) ? "For this character" : NULL;
        for (int n = 0; !section && n < SIL_ACH_COUNT; ++n)
            if (achievement_order[n].id == id) section = achievement_order[n].category;
        assert(section);
        if (!category || strcmp(category, section)) {
            category = section;
            assert(!strcmp(book->narrative_paras[para], category));
            assert(book->narrative_icon_row[para] == -1);
            ++para; ++headings;
        }
        const sil_achievement_definition *entry = sil_achievement_get(id);
        bool visible = requirements || sil_achievement_revealed(id);
        any_locked |= sil_achievement_unlocked_at(id) == 0;
        const char *text = book->narrative_paras[para];
        if (visible) {
            assert(strstr(text, entry->title));
            assert(strstr(text, entry->description));
            int occurrences = 0;
            for (int j = 0; j < book->narrative_para_count; ++j) {
                const char *line = book->narrative_paras[j];
                if (!strncmp(line, "Locked: ", 8)) line += 8;
                else if (!strncmp(line, "Unlocked: ", 10)) line += 10;
                else continue;
                size_t length = strlen(entry->title);
                if (!strncmp(line, entry->title, length)
                    && (line[length] == '\n' || line[length] == '\0'
                        || !strncmp(line + length, " (", 2))) ++occurrences;
            }
            assert(occurrences == 1);
            const int expected_count = achievement_icons[id].row != achievement_icons[id].row2
                || achievement_icons[id].col != achievement_icons[id].col2 ? 2
                : achievement_icons[id].count;
            assert(book->narrative_icon_row[para] == achievement_icons[id].row);
            assert(book->narrative_icon_col[para] == achievement_icons[id].col);
            assert(book->narrative_icon_row2[para] == achievement_icons[id].row2);
            assert(book->narrative_icon_col2[para] == achievement_icons[id].col2);
            assert(book->narrative_icon_count[para] == expected_count);
            assert(book->narrative_icon_source_size[para] == achievement_icons[id].source_size);
            char progress[256];
            if (sil_achievement_progress_text(id, progress, sizeof(progress))) {
                if (!sil_achievement_unlocked_at(id)) {
                    assert(strstr(text, progress));
                    ++progress_count;
                    if (id == SIL_ACH_AT_MOST_SEVEN) {
                        assert(strstr(text, "2/7"));
                        assert(strstr(text, "[x] Duruin"));
                        assert(strstr(text, "[ ] Delthaur"));
                        assert(strstr(text, "[x] Gothmog"));
                    }
                } else assert(!strstr(text, progress));
            }
        } else {
            assert(!strcmp(text, "Undiscovered achievement\nIts name and deed will be revealed when earned."));
            assert(book->narrative_icon_row[para] == 32);
            assert(book->narrative_icon_col[para] == 0);
            assert(book->narrative_icon_count[para] == 1);
            ++hidden;
        }
        assert(book->narrative_para_lines[para] >= 2);
        ++para;
    }
    assert(headings == (fixture_hero ? 10 : 9));
    for (int id = 0; id < SIL_ACH_COUNT; ++id)
        assert(seen[id] == (sil_achievement_get(id) != NULL));
    char total[32];
    strnfmt(total, sizeof(total), "of %d unlocked", SIL_ACH_ACTIVE_COUNT);
    assert(strstr(book->narrative_paras[0], total));
    int toggles = 0;
    for (int j = 0; j < book->narrative_para_count; ++j)
        if (book->narrative_para_choice[j] == 2) {
            assert(!strcmp(book->narrative_paras[j], requirements
                ? "Hide hidden requirements [R]" : "Show hidden requirements [R]"));
            ++toggles;
        }
    assert(toggles == 1);
    if (requirements) assert(hidden == 0);
    if (requirements && any_locked) assert(progress_count > 0);
}

int main(int argc, char **argv)
{
    assert(argc == 7 || argc == 9);
    fixture_mobile = argc == 9;
    if (fixture_mobile) {
        fixture_density = (float)atof(argv[7]); fixture_hero = atoi(argv[8]) != 0;
        g_startup_device_class = SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    }
    int width = atoi(argv[3]), height = atoi(argv[4]), bigger = atoi(argv[5]);
    fixture_width = width; fixture_height = height;
    char startup_screenshot[256];
    strnfmt(startup_screenshot, sizeof(startup_screenshot), "%s-big%d", argv[6], bigger);
    fixture_screenshot = startup_screenshot;
    maxima limits = {0};
    player_type player = {0};
    player_other options = {0};
    character_profile heroes[N_ELEMENTS(fixture_heroes)] = {0};
    for (unsigned n = 0; n < N_ELEMENTS(fixture_heroes); ++n) {
        heroes[n].guid = fixture_heroes[n].guid; heroes[n].flags_u = fixture_heroes[n].flags;
    }
    c_info = heroes; limits.c_max = N_ELEMENTS(heroes);
    player.pcharacter = fixture_hero ? 1 : 0; op_ptr = &options;
    z_info = &limits; p_ptr = &player;
    player.playing = true; player.cur_map_hgt = 32; player.cur_map_wid = 64;
    player.achievement_flags = SIL_ACH_RUN_STARTED | SIL_ACH_RUN_CHASM_SHAFT_ROUTE;
    player.achievement_balrogs = (1u << 0) | (1u << 6);
    player.achievement_ringil_wounds = 3;
    player.achievement_artefact_rings[0] = 1;
    character_generated = character_dungeon = true;
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)); assert(TTF_Init());
    sdl_config_set_defaults(&config); sdl_sync_palette();
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[1], sizeof(config.story_font2));
    config.use_unsafe_area = true;
    config.aux_view_font_size = 16;
    set_sdl_menu_bigger_font(SDL_MENU_FONT_HELP, bigger != 0);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
    g_state.system_scale = fixture_density;
    g_state.window = SDL_CreateWindow("Achievement render fixture", width, height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software"); assert(g_state.renderer);
    g_state.safe_area = (SDL_Rect){0,0,width,height};
    sdl_view *view = &g_views[PANE_MAIN];
    term_init(&view->t, width/8, height/16, 256);
    Term_activate(&view->t); term_screen = &view->t; view->term_ready = true;
    view->rect = (SDL_Rect){0,0,width,height};
    view->cell_w = 8; view->cell_h = 16; view->cols = width/8; view->rows = height/16;
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, width, height); assert(view->canvas);
    ANGBAND_DIR_APEX = argv[2]; sil_achievements_init();
    assert(sil_achievements_saved());
    assert(!strcmp(main_menu_title(MAIN_MENU_ACHIEVEMENTS), "Achievements"));
    assert(main_menu_keyboard_key(MAIN_MENU_ACHIEVEMENTS) == 'v');
    uint64_t earned_before[SIL_ACH_WORD_COUNT];
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        earned_before[word] = sil_achievements_mask(word);
    check_character_start();
    printf("Character startup panel %dx%d big=%d: all %d heroes, undiscovered deeds omitted from panel/history, earned hidden deeds visible, acknowledgement and lifecycle PASS\n",
        width, height, bigger, limits.c_max);
    for (int requirements = 0; requirements < 2; ++requirements) {
    if (requirements) toggle_hidden_requirements();
    else build_achievement_book(0, false);
    ui_menu_click_begin();
    SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255); SDL_RenderClear(g_state.renderer);
    sdl_character_sheet_screen_render();
    sdl_character_sheet_screen_state *book = &g_sdl_character_sheet_screen;
    int pages = book->narrative_page_count;
    assert(pages >= 1 && pages < SDL_BOOK_MAX_PAGES);
    assert(book->narrative_page_start[0] == 0);
    assert(book->narrative_page_start[pages] == book->narrative_para_count);
    assert(book->narrative_para_count >= SIL_ACH_ACTIVE_COUNT + 2);
    assert(book->narrative_para_count <= SDL_BOOK_MAX_PARAS);
    assert(g_state.tileset); /* Icons must render even when the map is ASCII. */
    check_catalogue(book, requirements != 0);
    TTF_Font *body_font = sdl_story_font_for_height_slot(book->narrative_body_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    /* Pagination stores rounded cache keys; inspect the exact painted viewport
     * for line-fit assertions so subpixel panel widths cannot change one wrap. */
    float book_w = book->select_scroll_rect.w;
    assert(SDL_fabsf(book_w - sdl_char_sheet_book_width(book->narrative_body_px,
        book->narrative_paginated_for_w)) < 1.0f);
    float line_h = sdl_char_sheet_line_h(body_font, book->narrative_body_px, 1.28f);
    assert(book->narrative_icon_book);
    if (fixture_mobile) {
        assert(book->narrative_body_px >= fixture_density * (bigger ? 36 : 24));
        assert(book_w >= width * (height > width ? 0.80f : 0.45f));
    } else if (bigger) assert(book->narrative_body_px >= (height > width ? 48 : 26));
    if (!fixture_mobile && width <= 580 && height > width) {
        assert(book_w >= width - 55); /* Panel insets, not body-size padding. */
        float reading_w = book_w - MIN(40.0f, book_w*0.12f);
        assert(reading_w >= width*0.75f);
        if (width == 360) assert(book->narrative_body_px == 50);
        if (width == 580) assert(book->narrative_body_px == 69);
        const char* words[] = { "Undiscovered", "achievement" };
        for (int i=0; i<2; ++i) {
            char word_lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
            assert(sdl_char_sheet_text_width(body_font,words[i]) <= (int)reading_w);
            assert(sdl_char_sheet_wrap_text(body_font,words[i],reading_w,
                word_lines,SDL_CHAR_SHEET_MAX_LINES)==1);
            assert(!strcmp(word_lines[0],words[i]));
        }
    }
    for (int para = 0; para < book->narrative_para_count; ++para) {
        char lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
        float d = fixture_mobile ? fixture_density : 1;
        float gutter = book->narrative_icon_row[para] >= 0
            ? MIN(line_h * 1.6f, book_w <= 680.0f*d
                ? MIN(40.0f*d, book_w * 0.12f) : book_w * 0.18f) : 0;
        float text_w = book_w - gutter;
        int count = sdl_char_sheet_wrap_text(body_font, book->narrative_paras[para],
            text_w, lines, SDL_CHAR_SHEET_MAX_LINES);
        assert(count <= SDL_CHAR_SHEET_MAX_LINES);
        assert(count == book->narrative_para_lines[para]);
        for (int line = 0; line < count; ++line)
            assert(sdl_char_sheet_text_width(body_font, lines[line]) <= (int)text_w);
    }
    /* Compare the drawn Back ink with a full native-font raster. Hitbox bounds
     * alone cannot detect a glyph cropped by a shorter texture source rect. */
    int prompt_px = sdl_menu_font_px(sdl_char_sheet_clampi((int)(height*0.025f),13,30));
    if (fixture_mobile && bigger) prompt_px = sdl_menu_role_font_px(SDL_UI_FONT_CONTROL);
    TTF_Font* back_font = sdl_story_font_for_height_slot(prompt_px, SDL_STORY_FONT_SLOT_DEFAULT);
    if (!(fixture_mobile && bigger)
        && book->narrative_paginated_for_w >= 700 * (fixture_mobile ? fixture_density : 1)) {
        float prompt_h = sdl_char_sheet_line_h(back_font,prompt_px,1.02f);
        back_font = sdl_story_font_for_height_slot((int)(prompt_h*1.12f+0.5f),SDL_STORY_FONT_SLOT_DEFAULT);
    }
    SDL_Surface* expected_back = TTF_RenderText_Blended(back_font,"Back",0,(SDL_Color){0,255,0,255});
    assert(expected_back);
    SDL_Rect expected_ink = green_ink(expected_back,(SDL_Rect){0,0,expected_back->w,expected_back->h});
    SDL_DestroySurface(expected_back);
    int locked_rings_para = -1;
    if (!sil_achievement_unlocked_at(SIL_ACH_THREE_UNTOUCHED))
        for (int para = 0; para < book->narrative_para_count; ++para)
            if (strstr(book->narrative_paras[para],
                    sil_achievement_get(SIL_ACH_THREE_UNTOUCHED)->title))
                locked_rings_para = para;
    for (int page = 0; page < pages; ++page) {
        sdl_character_sheet_screen_set_book_page(page);
        SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255); SDL_RenderClear(g_state.renderer);
        ui_menu_click_begin(); sdl_character_sheet_screen_render();
        assert(book->select_page == page);
        assert(book->narrative_page_start[page+1] > book->narrative_page_start[page]);
        bool close = false;
        SDL_FRect close_rect = {0};
        for (int hit = 0; hit < book->hit_count; ++hit) {
            SDL_FRect rect = book->hits[hit].rect;
            assert(rect.x >= -1 && rect.y >= -1);
            assert(rect.x + rect.w <= width + 1 && rect.y + rect.h <= height + 1);
            if (book->hits[hit].choice == SDL_SELECT_CLICK_CLOSE) { close = true; close_rect = rect; }
        }
        if (!close) {
            fprintf(stderr,"Missing Back page=%d/%d hits=%d size=%dx%d mobile=%d big=%d\n",page,pages,book->hit_count,width,height,fixture_mobile,bigger);
            for (int hit = 0; hit < book->hit_count; ++hit)
                fprintf(stderr,"hit=%d choice=%d rect=%g,%g,%g,%g\n",hit,book->hits[hit].choice,
                    book->hits[hit].rect.x,book->hits[hit].rect.y,book->hits[hit].rect.w,book->hits[hit].rect.h);
        }
        assert(close);
        SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL); assert(pixels);
        /* Check a standalone Locked: line after earlier partial clips in this
         * long book. Inspect real pixels, not just paragraph/wrap buffers.
         * Do not clear the text cache between pages. */
        if (!fixture_mobile && width == 360 && book->narrative_page_start[page] == locked_rings_para) {
            SDL_Surface *expected = TTF_RenderText_Blended(body_font, "Locked:", 0,
                (SDL_Color){128,128,128,255});
            assert(expected && book->sheet_scroll == 0);
            int wanted = gray_pixels(expected, (SDL_Rect){0,0,expected->w,expected->h});
            assert(wanted > 0);
            float gutter = MIN(line_h * 1.6f, MIN(40.0f, book_w * 0.12f));
            SDL_Rect line = {(int)SDL_roundf(book->select_scroll_rect.x + gutter),
                (int)SDL_roundf(book->select_scroll_rect.y), expected->w, expected->h};
            int actual = gray_pixels(pixels, line);
            if (actual < wanted*9/10 || actual > wanted*11/10)
                fprintf(stderr,"Cached Locked: ink %d differs from expected %d\n",actual,wanted);
            assert(actual >= wanted*9/10 && actual <= wanted*11/10);
            SDL_DestroySurface(expected);
        }
        SDL_Rect back_ink = green_ink(pixels,(SDL_Rect){(int)close_rect.x,(int)close_rect.y,
            (int)SDL_ceilf(close_rect.w),(int)SDL_ceilf(close_rect.h)});
        if (abs(back_ink.w-expected_ink.w)>1 || abs(back_ink.h-expected_ink.h)>1)
            fprintf(stderr,"Back ink %dx%d differs from native %dx%d at %dx%d big=%d\n",
                back_ink.w,back_ink.h,expected_ink.w,expected_ink.h,width,height,bigger);
        assert(abs(back_ink.w-expected_ink.w)<=1 && abs(back_ink.h-expected_ink.h)<=1);
        assert(back_ink.y>=0 && back_ink.y+back_ink.h<=height-2);
        bool triple_icon = false;
        for (int para = book->narrative_page_start[page];
                para < book->narrative_page_start[page+1]; ++para) {
            triple_icon |= book->narrative_icon_count[para] == 3;
            triple_icon |= strstr(book->narrative_paras[para],
                sil_achievement_get(SIL_ACH_THREE_LIGHTS_IN_EXILE)->title) != NULL;
            triple_icon |= strstr(book->narrative_paras[para],
                sil_achievement_get(SIL_ACH_THREE_UNTOUCHED)->title) != NULL;
        }
        if (page == 0 || page == MIN(2, pages-1) || page == pages-1 || triple_icon) {
            char path[160];
            snprintf(path, sizeof(path), "%s-%dx%d-big%d-requirements%d-page%d.bmp", argv[6], width,height,bigger,requirements,page+1);
            if (fixture_mobile) {
                strnfmt(path, sizeof(path), "%s-%dx%d-big%d-requirements%d-page%d.png", argv[6],width,height,bigger,requirements,page+1);
                assert(IMG_SavePNG(pixels, path));
            } else assert(SDL_SaveBMP(pixels, path));
        }
        SDL_DestroySurface(pixels);
        bool progress_page = false;
        for (int para = book->narrative_page_start[page];
                para < book->narrative_page_start[page+1]; ++para)
            progress_page |= strstr(book->narrative_paras[para],
                sil_achievement_get(SIL_ACH_AT_MOST_SEVEN)->title) != NULL;
        if ((triple_icon || progress_page) && book->sheet_scroll_max > 0) {
            int steps = 0;
            while (sdl_character_sheet_screen_scroll_book(1)) assert(++steps < 256);
            assert(book->sheet_scroll == book->sheet_scroll_max);
            SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255);
            SDL_RenderClear(g_state.renderer);
            ui_menu_click_begin(); sdl_character_sheet_screen_render();
            SDL_Surface *bottom = SDL_RenderReadPixels(g_state.renderer, NULL);
            assert(bottom);
            char path[180];
            snprintf(path, sizeof(path), "%s-%dx%d-big%d-requirements%d-page%d-bottom.bmp",
                argv[6],width,height,bigger,requirements,page+1);
            assert(SDL_SaveBMP(bottom,path)); SDL_DestroySurface(bottom);
            steps = 0;
            while (sdl_character_sheet_screen_scroll_book(-1)) assert(++steps < 256);
            assert(book->sheet_scroll == 0);
        }
    }
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        assert(earned_before[word] == sil_achievements_mask(word));
    printf("Achievement viewer %s %dx%d big=%d requirements=%d: %d pages, %dpx body, ordered complete catalogue, composites, fitted lines and complete native Back ink PASS\n",
        argv[6], width,height,bigger,requirements,pages,book->narrative_body_px);
    }
    toggle_hidden_requirements();
    assert(!show_hidden_requirements);
    sdl_character_sheet_screen_hide();
    sdl_story_font_cache_clear(); SDL_DestroyTexture(view->canvas);
    term_nuke(&view->t); view->term_ready=false; term_screen=NULL; Term=NULL;
    SDL_DestroyTexture(g_state.tileset); g_state.tileset=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
    sdl_pop_terminal_menu_scale();
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    # externs.h is intentionally unguarded in this repository; the private
    # renderer header above already includes it. Keep production functions exact.
    implementation = (ROOT / "src/achievements-ui.c").read_text().replace(
        '#include "externs.h"', '')
    renderer = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")
    fixture = HARNESS.replace('#include "achievements-ui.c"', implementation)
    for placeholder, name in (("WRAP_TEXT_LEN", "SDL_CHAR_SHEET_TEXT_LEN"),
                              ("WRAP_MAX_LINES", "SDL_CHAR_SHEET_MAX_LINES")):
        fixture = fixture.replace(placeholder, re.search(name + r"\s*=\s*(\d+)", renderer)[1])
    heroes = []
    for line in (ROOT / "lib/edit/character.txt").read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line.startswith("N:"):
            _, index, name = line.split(":", 2)
            assert int(index) == len(heroes)
            heroes.append({"name": name, "guid": 0, "flags": []})
        elif line.startswith("Q:"):
            heroes[-1]["guid"] = int(line[2:], 16)
        elif line.startswith("U:"):
            heroes[-1]["flags"].extend("UNQ_" + flag.strip() for flag in line[2:].split("|"))
    records = []
    for hero in heroes:
        guid = hero["guid"]
        records.append('{%s, {0x%08x, 0x%08x}, %s}' % (
            json.dumps(hero["name"], ensure_ascii=False), guid >> 32, guid & 0xffffffff,
            " | ".join(hero["flags"]) or "0"))
    fixture = fixture.replace("/* HERO FIXTURE */", "static const struct { const char *name; guid64 guid; u32b flags; } fixture_heroes[] = {\n"
                              + ",\n".join(records) + "\n};")
    source.write_text(fixture, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(("/src/main.c.obj", "/src/achievements-ui.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")] +
        [str(BUILD), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    def compile_fixture(target, object_response, extra_sources=(), extra_flags=()):
        subprocess.run([
            "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
            *(["-DSIL_STEAMWORKS"] if STEAM else []), *extra_flags,
            "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *extra_sources,
            "@" + str(object_response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            *(["-lstdc++"] if STEAM else []),
            "-Wl,--wrap=SDL_GetDisplayContentScale,--wrap=msg_format,--wrap=inkey,--wrap=sdl_mobile_portrait_layout_active,--wrap=sdl_touch_only_device_active,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen,--wrap=handle_stuff",
            "-o", str(target)], cwd=BUILD, env=env, check=True)

    exe = OUT / "check.exe"
    compile_fixture(exe, response)
    # Current hero deeds are all public. Exercise the real visibility policy
    # with fixture-only hidden definitions, including a hero whose sole deed
    # is hidden. Production catalogue flags and provider data stay untouched.
    hidden_source = OUT / "hidden-definitions.c"
    hidden_definitions = (ROOT / "src/achievements.c").read_text(encoding="utf-8")
    for key in ("A_SINGLE_SPARK", "IRON_CLEAVER"):
        pattern = r'(\{\s*"' + key + r'",\s*"[^"]*",\s*"[^"]*",\s*)false'
        hidden_definitions, count = re.subn(pattern, r'\g<1>true', hidden_definitions)
        assert count == 1
    hidden_source.write_text(hidden_definitions, encoding="utf-8")
    hidden_response = OUT / "hidden-objects.rsp"
    hidden_response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith("/src/achievements.c.obj")), encoding="utf-8")
    hidden_exe = OUT / "check-hidden.exe"
    compile_fixture(hidden_exe, hidden_response, [str(hidden_source)],
                    ["-DFIXTURE_HIDDEN_CHARACTERS"])
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as directory:
        enum = (ROOT / "src/achievements.h").read_text().split("typedef enum sil_achievement_id {", 1)[1].split("} sil_achievement_id;", 1)[0]
        keys = re.findall(r"\bSIL_ACH_([A-Z][A-Z0-9_]*)(?:\s*=\s*\d+)?\s*,", enum)
        for earned in (False, True):
            profile = Path(directory) / str(earned)
            profile.mkdir()
            (profile / "achievements.json").write_text(json.dumps({
                "version": 1, "unlocked": dict.fromkeys(
                    keys, 1800000000) if earned else {}}))
            subprocess.run([
                str(hidden_exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(profile),
                "1280", "720", "0", str(OUT / ("hidden-unlocked" if earned else "hidden-locked"))],
                cwd=ROOT, env=env, check=True, timeout=30)
            for width, height, bigger in ((1280,720,0), (1280,800,1), (580,1280,1), (1280,580,1), (360,800,1)):
                subprocess.run([
                    str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(profile),
                    str(width), str(height), str(bigger), str(OUT / ("unlocked" if earned else "locked"))],
                    cwd=ROOT, env=env, check=True, timeout=30)
            for width, height, density in ((580,1280,1.5), (720,1600,2), (1080,2340,2.75), (1600,720,2)):
                for bigger in (0, 1):
                    subprocess.run([
                        str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(profile),
                        str(width), str(height), str(bigger), str(OUT / ("hero-unlocked" if earned else "hero-locked")),
                        str(density), "1"], cwd=ROOT, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
