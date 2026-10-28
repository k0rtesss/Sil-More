#!/usr/bin/env python3
"""Check saved terminal-menu scales, footer/corner input, and native exclusions.

Reuses the existing SDL renderer/empty-inventory fixture and links production
objects. All JSON and screenshots stay in scripts/output; no user data is read.
"""
from pathlib import Path
import os
import shlex
import subprocess

import check_bigger_font_layout as layout

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/terminal-menu-scale" / ("run-" + str(os.getpid()))

CHECKS = r'''
/* BLITZ_SUMMARY_SOURCE */

static float scale_density = 1;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display)
{ return scale_density; }

static void check_scale_persistence(void)
{
    struct sdl_config loaded, original;
    sdl_config_set_defaults(&loaded);
    fixture_assert(!strcmp(sdl_menu_font_key(SDL_MENU_FONT_ABOUT), "about"));
    fixture_assert(!strcmp(sdl_menu_font_label(SDL_MENU_FONT_ABOUT), "About"));
    fixture_assert(loaded.terminal_menu_scale_offsets[SDL_MENU_FONT_ABOUT]
        == SDL_TERMINAL_MENU_SCALE_OFFSET_DEFAULT_FOR_MAIN_SCALE(
            loaded.main_view_scale));
    fixture_assert(loaded.terminal_menu_scale_offsets[SDL_MENU_FONT_HELP]
        == SDL_TERMINAL_MENU_SCALE_OFFSET_DEFAULT_FOR_MAIN_SCALE(
            loaded.main_view_scale));

    FILE *file = fopen("legacy-scales.json", "wb");
    fixture_assert(file);
    fputs("{\"sdl\":{\"mainViewScale\":4,\"terminalMenuScaleOffset\":-2}}", file);
    fclose(file);
    fixture_assert(sdl_config_load("legacy-scales.json", &loaded, NULL, 0, NULL)
        == SDL_CONFIG_LOAD_OK);
    for (int menu = 0; menu < SDL_MENU_FONT_COUNT; menu++)
        fixture_assert(loaded.terminal_menu_scale_offsets[menu] == -2);

    file = fopen("legacy-max-scale.json", "wb");
    fixture_assert(file);
    fputs("{\"sdl\":{\"mainViewScale\":2,\"terminalMenuScaleOffset\":0,"
        "\"terminalMenuScaleOffsets\":{\"help\":0}}}", file);
    fclose(file);
    fixture_assert(sdl_config_load("legacy-max-scale.json", &loaded, NULL, 0, NULL)
        == SDL_CONFIG_LOAD_OK);
    fixture_assert(loaded.terminal_menu_scale_offsets[SDL_MENU_FONT_HELP] == 0);
    fixture_assert(loaded.terminal_menu_scale_offsets[SDL_MENU_FONT_ABOUT] == 0);

    file = fopen("partial-scales.json", "wb");
    fixture_assert(file);
    fputs("{\"sdl\":{\"terminalMenuScaleOffset\":-1,\"terminalMenuScaleOffsets\":{"
        "\"inventory\":-2,\"abilities\":-3,\"help\":0,\"smithing\":-99,"
        "\"messages\":99,\"scores\":\"invalid\",\"about\":0}}}", file);
    fclose(file);
    fixture_assert(sdl_config_load("partial-scales.json", &original, NULL, 0, NULL)
        == SDL_CONFIG_LOAD_OK);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_INVENTORY] == -2);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_ABILITIES] == -3);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_HELP] == 0);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_SMITHING] == -3);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_MESSAGES] == 0);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_SCORES] == -1);
    fixture_assert(original.terminal_menu_scale_offsets[SDL_MENU_FONT_ABOUT] == 0);
    fixture_assert(sdl_config_save("roundtrip-scales.json", &original, NULL, 0));
    fixture_assert(sdl_config_load("roundtrip-scales.json", &loaded, NULL, 0, NULL)
        == SDL_CONFIG_LOAD_OK);
    fixture_assert(!memcmp(original.terminal_menu_scale_offsets,
        loaded.terminal_menu_scale_offsets, sizeof(original.terminal_menu_scale_offsets)));
    puts("Terminal scales: legacy migration, independent roundtrip, missing/invalid values: PASS");
}

static SDL_FRect scale_control(bool increase)
{
    SDL_FRect rail = sdl_menu_font_button_bounds();
    return sdl_menu_corner_control_rect(rail,
        sdl_menu_scale_control_first_index() + (increase ? 1 : 0));
}

static void scale_click(bool increase)
{
    SDL_FRect button = scale_control(increase);
    font_button_mouse(true, button.x + button.w / 2, button.y + button.h / 2);
    font_button_mouse(false, button.x + button.w / 2, button.y + button.h / 2);
}

static void scale_drain_wakes(bool expected)
{
    char key;
    int count = 0;
    bool never_bored = Term->never_bored;
    Term->never_bored = true; /* Inspect the queue without starting idle game work. */
    while (Term->key_head != Term->key_tail) {
        fixture_assert(Term_inkey(&key, false, true) == 0);
        fixture_assert(key == UI_MENU_CLICK_WAKE_KEY);
        count++;
    }
    Term->never_bored = never_bored;
    fixture_assert((count > 0) == expected);
    fixture_assert(!ui_menu_click_has_pending());
}

static void check_native_exclusion(void)
{
    /* Deliberately keep the inventory terminal scope pushed under each native
     * surface: scope identity alone must not enable these controls. */
    fixture_assert(sdl_terminal_menu_font() == SDL_MENU_FONT_INVENTORY);
    set_sdl_show_menu_font_button(false);
    SDL_FRect old = scale_control(true);
    bool *native[] = {&g_question_menu.active, &g_song_menu.active,
        &g_main_menu_overlay_active, &g_log_pane_menu.active,
        &g_side_pane_menu.active, &g_unified_look_prompt.active,
        &g_unified_look_sidebar.active, &g_touch_pane_yes_no_prompt_active,
        &g_touch_pane_reset_confirm_active, &g_description_overlay.active,
        &g_sdl_halls.active, &g_sdl_poetry_screen.active,
        &g_sdl_tale_screen.active, &g_sdl_pause_text_screen.active,
        &g_minimap.active};
    for (int i = 0; i < N_ELEMENTS(native); i++) {
        *native[i] = true;
        fixture_assert(!sdl_menu_scale_controls_active());
        fixture_assert(sdl_menu_font_button_bounds().w == 0);
        SDL_Event ev = {0};
        ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.x = old.x + old.w / 2;
        ev.button.y = old.y + old.h / 2;
        fixture_assert(!sdl_menu_font_button_handle_event(&ev));
        ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
        fixture_assert(!sdl_menu_font_button_handle_event(&ev));
        *native[i] = false;
        fixture_assert(sdl_menu_scale_controls_active());
    }
    for (int context = SDL_CHARACTER_SHEET_LIVE;
            context <= SDL_CHARACTER_SHEET_NARRATIVE; context++) {
        g_sdl_character_sheet_screen.context = context;
        fixture_assert(!sdl_menu_scale_controls_active());
        set_sdl_show_menu_font_button(true);
        fixture_assert(sdl_menu_corner_control_count() == 1); /* B only. */
        set_sdl_show_menu_font_button(false);
        fixture_assert(sdl_menu_font_button_bounds().w == 0);
    }
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_HIDDEN;
    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_INTRO;
    fixture_assert(!sdl_menu_scale_controls_active());
    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_HIDDEN;
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_QUESTS, "Fixture", "", true, false, 0);
    fixture_assert(!sdl_menu_scale_controls_active());
    sdl_hint_quest_menu_hide();
    fixture_assert(sdl_menu_scale_controls_active());
    scale_drain_wakes(false);
    puts("Terminal scales: native menus, sheets, stories, quests, maps and modal overlays excluded: PASS");
}

static void check_corner_input(void)
{
    config.main_view_scale = 2;
    set_sdl_terminal_menu_scale_offset(0);
    fixture_assert(get_sdl_terminal_menu_scale_offset() == 0);
    fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_HELP) == 0);
    fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_ABOUT) == 0);

    set_sdl_terminal_menu_scale_offset(-1);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
    Term_flush(); ui_menu_click_clear();
    fixture_assert(sdl_menu_scale_controls_active());
    check_native_exclusion();
    set_sdl_show_menu_font_button(true);
    /* The existing B shortcut still refreshes the underlying terminal when
     * an inventory preview is open, while -/+ remain hidden for the preview. */
    g_description_overlay.active = true;
    fixture_assert(!sdl_menu_scale_controls_active());
    SDL_FRect big_button = sdl_menu_font_button_rect();
    font_button_mouse(true, big_button.x + big_button.w / 2, big_button.y + big_button.h / 2);
    font_button_mouse(false, big_button.x + big_button.w / 2, big_button.y + big_button.h / 2);
    fixture_assert(get_sdl_menu_bigger_font_for(SDL_MENU_FONT_INVENTORY));
    scale_drain_wakes(true);
    g_description_overlay.active = false;
    set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY, false);
    SDL_Rect canvas = {0, 0, fixture_width, fixture_height};
    SDL_Rect after = sdl_menu_content_rect(canvas);
    fixture_assert(!memcmp(&canvas, &after, sizeof(canvas)));
    ui_menu_click_begin();
    ui_menu_click_set_touch_exit_button(true);
    for (int big = 0; big < 2; big++) {
        set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY, big);
        sdl_apply_config_no_redraw(); Term_flush();
        SDL_FRect minus = scale_control(false), plus = scale_control(true);
        fixture_assert(minus.x >= 0 && plus.x + plus.w <= fixture_width);
        fixture_assert(minus.y >= 0 && plus.y + plus.h <= fixture_height);
        SDL_FRect mh = sdl_menu_control_hit_rect(minus), ph = sdl_menu_control_hit_rect(plus);
        fixture_assert(mh.w >= sdl_ui_min_tap_px() && ph.w >= sdl_ui_min_tap_px());
        fixture_assert(mh.x + mh.w <= ph.x + .01f);
        sdl_touch_menu_button_layout_entry footer[SDL_TOUCH_MENU_BUTTON_MAX];
        fixture_assert(sdl_touch_menu_button_layout(footer, N_ELEMENTS(footer)) == 1);
        fixture_assert(minus.x < fixture_width * .5f && minus.y > fixture_height * .5f);
        fixture_assert(!SDL_HasRectIntersectionFloat(&mh, &footer[0].rect));
        fixture_assert(!SDL_HasRectIntersectionFloat(&ph, &footer[0].rect));
        sdl_view *view = &g_views[PANE_MAIN];
        float bottom = view->rect.y + view->margin_y
            + sdl_main_view_visual_rows(view) * view->cell_h;
        float text_bottom = bottom - sdl_touch_menu_button_reserved_rows() * view->cell_h;
        fixture_assert(mh.y + .01f >= text_bottom && ph.y + .01f >= text_bottom);
        fixture_assert(mh.y + mh.h <= bottom && ph.y + ph.h <= bottom);
        font_button_mouse(true, plus.x + plus.w / 2, plus.y + plus.h / 2);
        font_button_mouse(false, minus.x + minus.w / 2, minus.y + minus.h / 2);
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY) == -1);
        scale_drain_wakes(false); /* Releasing on another control cancels. */
        int previous_cell_h = g_views[PANE_MAIN].cell_h;
        scale_click(false);
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY) == -2);
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_HELP) == -1);
        if (big && sdl_handheld_ui_active())
            fixture_assert(g_views[PANE_MAIN].cell_h < previous_cell_h);
        scale_drain_wakes(true);
        scale_click(true);
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY) == -1);
        scale_drain_wakes(true);
        struct sdl_config saved;
        fixture_assert(sdl_config_load("button-menu-fonts.json", &saved, NULL, 0, NULL)
            == SDL_CONFIG_LOAD_OK);
        fixture_assert(saved.terminal_menu_scale_offsets[SDL_MENU_FONT_INVENTORY] == -1);
        fixture_assert(saved.terminal_menu_scale_offsets[SDL_MENU_FONT_HELP] == -1);
        fixture_assert(saved.menu_bigger_font[SDL_MENU_FONT_INVENTORY] == big);
        /* Touch cancellation and duplicate mouse events must not change size. */
        plus = scale_control(true);
        SDL_Event ev = {0};
        ev.type = SDL_EVENT_FINGER_DOWN;
        ev.tfinger.windowID = SDL_GetWindowID(g_state.window);
        ev.tfinger.touchID = 17; ev.tfinger.fingerID = 23;
        ev.tfinger.x = (plus.x + plus.w / 2) / fixture_width;
        ev.tfinger.y = (plus.y + plus.h / 2) / fixture_height;
        fixture_assert(sdl_menu_font_button_handle_event(&ev));
        ev.type = SDL_EVENT_FINGER_UP; ev.tfinger.fingerID = 24;
        fixture_assert(!sdl_menu_font_button_handle_event(&ev));
        ev.tfinger.fingerID = 23;
        fixture_assert(sdl_menu_font_button_handle_event(&ev));
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY) == 0);
        scale_drain_wakes(true);
        scale_click(true); scale_drain_wakes(false); /* At the upper limit. */
        ev.type = SDL_EVENT_FINGER_DOWN;
        fixture_assert(sdl_menu_font_button_handle_event(&ev));
        ev.type = SDL_EVENT_FINGER_CANCELED;
        fixture_assert(sdl_menu_font_button_handle_event(&ev));
        scale_drain_wakes(false);
        set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, -1);
    }
    /* A full action row must keep every command and move zoom back upstairs. */
    for (int i = 0; i < 8; i++)
        ui_menu_click_add_touch_button(100 + i, "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW", TERM_DARK);
    sdl_touch_menu_button_layout_entry crowded[SDL_TOUCH_MENU_BUTTON_MAX];
    fixture_assert(sdl_touch_menu_button_layout(crowded, N_ELEMENTS(crowded)) == 9);
    fixture_assert(scale_control(false).y < fixture_height * .5f);
    for (int i = 0; i < 9; i++)
        fixture_assert(crowded[i].rect.w > 0);
    ui_menu_click_begin();
    ui_menu_click_set_touch_exit_button(true);
    set_sdl_show_menu_font_button(false);
    fixture_assert(scale_control(false).y > fixture_height * .5f);
    set_sdl_show_menu_font_button(true);
    fixture_assert(sdl_touch_menu_button_layout(crowded, N_ELEMENTS(crowded)) == 1);
    fixture_assert(sdl_touch_exit_button_handle_pointer(
        crowded[0].rect.x + crowded[0].rect.w / 2,
        crowded[0].rect.y + crowded[0].rect.h / 2));
    char exit_key = 0;
    fixture_assert(Term_inkey(&exit_key, false, true) == 0 && exit_key == ESCAPE);
    ui_menu_click_clear();
    fixture_assert(scale_control(false).y < fixture_height * .5f);
    set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_HELP, -3);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
    set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_HELP, -2);
    sdl_apply_config_no_redraw();
    sdl_pop_terminal_menu_scale();
    fixture_assert(sdl_terminal_menu_font() == SDL_MENU_FONT_INVENTORY);
    fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY) == -1);
    fixture_assert(g_terminal_menu_scale_override == get_sdl_terminal_menu_scale());
    sdl_set_terminal_menu_font(SDL_MENU_FONT_HELP);
    fixture_assert(get_sdl_terminal_menu_scale_offset_for(sdl_terminal_menu_font()) == -2);
    fixture_assert(g_terminal_menu_scale_override == get_sdl_terminal_menu_scale());
    sdl_pop_terminal_menu_scale(); Term_flush(); ui_menu_click_clear();
    set_sdl_show_menu_font_button(false);
    fixture_assert(sdl_menu_font_button_bounds().w == 0);
    puts("Terminal scales: footer/corner layout, reserved touch targets, crowded actions, mouse/touch, limits, save, nested restore: PASS");
}

static bool actual_scale_active;
static int actual_scale_frames;
static bool blitz_scale_active;
static int blitz_scale_frames;
static int blitz_scale_inkey(void)
{
    fixture_assert(blitz_scale_frames < 2);
    if (blitz_scale_frames++ == 0) {
        fixture_assert(sdl_menu_scale_controls_active());
        scale_click(false);
        scale_drain_wakes(true);
        return UI_MENU_CLICK_WAKE_KEY;
    }
    char title[160] = "";
    for (int col = 0; col < MIN(Term->wid, (int)sizeof(title) - 1); col++) {
        byte attr; char ch;
        Term_what(col, 1, &attr, &ch); title[col] = ch ? ch : ' ';
    }
    fixture_assert(strstr(title, "Blitz Effects"));
    return '\r';
}

static void check_blitz_scale(void)
{
    set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_BIRTH, -1);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_BIRTH);
    Term_flush(); ui_menu_click_clear();
    blitz_scale_active = true; blitz_scale_frames = 0;
    blitz_show_effect_summary();
    blitz_scale_active = false;
    fixture_assert(blitz_scale_frames == 2); /* A scale tap cannot continue. */
    sdl_pop_terminal_menu_scale(); Term_flush();
    puts("Terminal scales: real Blitz summary redraws after tap and waits for continue: PASS");
}

static int actual_scale_inkey(void)
{
    int step = actual_scale_frames++;
    fixture_assert(step < 12);
    int phase = step % 3;
    fixture_assert(sdl_terminal_menu_font() == SDL_MENU_FONT_INVENTORY);
    fixture_assert(sdl_menu_scale_controls_active());
    if (step < 3) {
        /* Show the real terminal canvas plus its controls. Gameplay HUD
         * overlays require a loaded dungeon, which this empty fixture lacks. */
        sdl_view *view = &g_views[PANE_MAIN];
        /* Layout refreshes recreate this bare fixture's term without the real
         * window activation callback. Production terms are mapped here. */
        Term->mapped_flag = true;
        fixture_assert(Term_fresh() == 0); /* Normally performed inside inkey(). */
        SDL_SetRenderTarget(g_state.renderer, NULL);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        SDL_FRect dst = {view->rect.x + view->margin_x, view->rect.y + view->margin_y};
        fixture_assert(SDL_GetTextureSize(view->canvas, &dst.w, &dst.h));
        fixture_assert(SDL_RenderTexture(g_state.renderer, view->canvas, NULL, &dst));
        sdl_menu_font_button_render();
        sdl_touch_exit_button_render();
        capture(phase == 1 ? "inventory-smaller" : "inventory-original");
    }
    if (phase < 2) {
        int before = g_views[PANE_MAIN].cell_h;
        scale_click(phase == 1);
        fixture_assert(get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY)
            == (phase == 0 ? -2 : -1));
        if (fixture_width == 3840)
            fixture_assert(phase == 0 ? g_views[PANE_MAIN].cell_h < before
                : g_views[PANE_MAIN].cell_h > before);
        scale_drain_wakes(true);
        return UI_MENU_CLICK_WAKE_KEY;
    }
    return step == 11 ? ESCAPE : KTRL('N');
}

static void check_clamped_desktop_scale(int width, int height)
{
    SDL_Rect screen = {0, 0, width, height};
    struct sdl_config saved_config = config;
    int saved_max[SDL_MIN_TERMINAL_MODE_COUNT];
    sdl_view *view = &g_views[PANE_MAIN];

    if (sdl_handheld_ui_active()
        || sdl_max_scale_for_rect_mode(&screen, SDL_MIN_TERMINAL_NORMAL)
            <= SDL_MAIN_VIEW_MIN_SCALE)
        return;
    for (int i = 0; i < SDL_MIN_TERMINAL_MODE_COUNT; i++) {
        saved_max[i] = g_platform_max_main_view_scale[i];
        g_platform_max_main_view_scale[i] = SDL_MAIN_VIEW_MIN_SCALE;
    }
    fixture_set_all_fonts(false);
    config.min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
    screen_push_supporting_panes_hidden();
    set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, 0);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
    fixture_assert(g_terminal_menu_scale_override > SDL_MAIN_VIEW_MIN_SCALE);
    fixture_assert(sdl_current_main_view_scale() == SDL_MAIN_VIEW_MIN_SCALE);
    int automatic = sdl_terminal_menu_font_px();
    fixture_assert(automatic == sdl_menu_role_font_px(SDL_UI_FONT_TITLE));
    fixture_assert(view->cell_h == automatic);
    fixture_assert(view->cols >= sdl_min_terminal_cols_for_mode(SDL_MIN_TERMINAL_NORMAL));
    fixture_assert(view->rows >= sdl_min_terminal_rows_for_mode(SDL_MIN_TERMINAL_NORMAL));
    int previous = automatic;
    for (int offset = -1; offset >= SDL_TERMINAL_MENU_SCALE_OFFSET_MIN; offset--) {
        set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, offset);
        sdl_apply_config_no_redraw();
        fixture_assert(sdl_current_main_view_scale() == SDL_MAIN_VIEW_MIN_SCALE);
        fixture_assert(view->cell_h == sdl_terminal_menu_font_px());
        fixture_assert(view->cell_h < previous);
        previous = view->cell_h;
    }
    set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, 0);
    sdl_apply_config_no_redraw();
    fixture_assert(view->cell_h == automatic);
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    for (int i = 0; i < SDL_MIN_TERMINAL_MODE_COUNT; i++)
        g_platform_max_main_view_scale[i] = saved_max[i];

    /* When the larger scale is actually available, retain that choice. */
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
    int effective = sdl_current_main_view_scale();
    fixture_assert(effective > SDL_MAIN_VIEW_MIN_SCALE);
    fixture_assert(sdl_terminal_menu_font_px() == 0);
    fixture_assert(view->cell_h == effective * TILE_SIZE);
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    config = saved_config;
    printf("desktop %dx%d: clamped menu %d, offsets shrink and restore; native scale %d preserved: PASS\n",
        width, height, automatic, effective);
}

static void check(int width, int height)
{
    fixture_width = width; fixture_height = height;
    scale_density = (float)atof(getenv("SIL_SCALE_TEST_DENSITY"));
    g_state.system_scale = scale_density;
    if (getenv("SIL_SCALE_TEST_HANDHELD"))
        g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
    SDL_strlcpy(fixture_id, "terminal-scale", sizeof(fixture_id));
    g_state.window = SDL_CreateWindow("Terminal scales", width, height, SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    fixture_assert(g_state.renderer);
    g_state.safe_area = (SDL_Rect){0, 0, width, height};
    for (int mode = 0; mode < SDL_MIN_TERMINAL_MODE_COUNT; mode++)
        g_platform_max_main_view_scale[mode] =
            sdl_max_scale_for_rect_mode(&g_state.safe_area, mode);
    config.touch_thumb_enabled = false; /* The empty fixture has no dungeon actions. */
    sdl_view *view = &g_views[PANE_MAIN];
    term_init(&view->t, 80, 24, 256); Term_activate(&view->t); term_screen = &view->t;
    character_icky = 1;
    g_suppress_layout_refresh_present = true;
    check_clamped_desktop_scale(width, height);
    if (!sdl_handheld_ui_active()) {
        int saved_main = config.main_view_scale;
        int saved_offset = get_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY);
        config.main_view_scale = 1;
        fixture_set_all_fonts(false);
        set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, 0);
        screen_push_supporting_panes_hidden();
        sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
        if (g_terminal_menu_scale_override == SDL_MAIN_VIEW_MIN_SCALE) {
            int automatic = view->cell_h;
            int min_px = (int)SDL_ceilf(scale_density * SDL_AUX_VIEW_FONT_MIN);
            int cols = sdl_min_terminal_cols_for_mode(SDL_MIN_TERMINAL_NORMAL);
            int rows = sdl_min_terminal_rows_for_mode(SDL_MIN_TERMINAL_NORMAL);
            fixture_assert(automatic == sdl_terminal_menu_font_px());
            fixture_assert(automatic >= min_px);
            if (width / MAX(1,min_px/2) >= cols && height / min_px >= rows) {
                fixture_assert(view->cols >= cols && view->rows >= rows);
            }
            set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, -1);
            sdl_apply_config_no_redraw();
            fixture_assert(view->cell_h <= automatic);
            if (automatic > min_px)
                fixture_assert(view->cell_h < automatic);
            printf("desktop %dx%d @%.2f menu: auto=%d smaller=%d grid=%dx%d\n",
                width,height,scale_density,automatic,view->cell_h,view->cols,view->rows);
        }
        sdl_pop_terminal_menu_scale();
        screen_pop_supporting_panes_hidden();
        set_sdl_terminal_menu_scale_offset_for(SDL_MENU_FONT_INVENTORY, saved_offset);
        config.main_view_scale = saved_main;
    }
    check_scale_persistence(); check_corner_input(); check_blitz_scale();
    fixture_set_all_fonts(false);
    set_sdl_terminal_menu_scale_offset(-1);
    int saved_main_scale = config.main_view_scale;
    int saved_map_zoom = g_main_view_zoom_scale;
    actual_scale_active = true; actual_scale_frames = 0;
    supply_menu_request request = {.focus_page = true, .page = SUPPLY_MENU_PAGE_EQUIPPED};
    fixture_assert(!do_cmd_knowledge_supplies(&request));
    actual_scale_active = false;
    fixture_assert(actual_scale_frames == 12);
    fixture_assert(sdl_terminal_menu_font() == SDL_MENU_FONT_NONE);
    fixture_assert(g_terminal_menu_scale_override == 0);
    fixture_assert(config.main_view_scale == saved_main_scale);
    fixture_assert(g_main_view_zoom_scale == saved_map_zoom);
    Term_flush(); ui_menu_click_clear();
    printf("Terminal scales %dx%d @%.2f (%s): real Equipped/Inventory/Jewelry/Supplies redraws and exit: PASS\n",
        width, height, scale_density, sdl_handheld_ui_active() ? "handheld" : "desktop");
    sdl_view_destroy(view);
    term_screen = NULL; Term = NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
    SDL_DestroyWindow(g_state.window); g_state.window = NULL;
}
'''


def c_function(source, marker):
    start = source.rindex(marker)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def main():
    menu_source = (ROOT / "src/cmd/ui/cmd-ui-main-menu.c").read_text(encoding="utf-8")
    about_start = menu_source.index("static void main_menu_about(void)")
    about_end = menu_source.index("static void main_menu_blitz_text_line", about_start)
    assert "main_menu_about_semantic(about_lines)" in menu_source[about_start:about_end]
    OUT.mkdir(parents=True, exist_ok=True)
    print("Terminal scale artifacts:", OUT, flush=True)
    harness = layout.HARNESS
    # This fixture supplies its own density wrapper below.  The shared layout
    # harness also defines one for its main-menu density sweep.
    shared_density_wrapper = '''float __real_SDL_GetDisplayContentScale(SDL_DisplayID display);
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) {
    return fixture_main_menu_density>0 ? fixture_main_menu_density
        : __real_SDL_GetDisplayContentScale(display);
}
'''
    if shared_density_wrapper in harness:
        harness = harness.replace(shared_density_wrapper, "", 1)
    start = harness.index("static void check(int width,int height)")
    end = harness.index("int main(int argc,char **argv)", start)
    blitz = (ROOT / "src/birth/birth-blitz.c").read_text()
    blitz_summary = ('#include "blitz.h"\n#include "metarun.h"\n'
        'void birth_put_str_fit(byte attr, cptr text, int row, int col);\n'
        'void birth_prompt_label(int binding, const char *fallback, char *buf, size_t size);\n')
    for marker in ["static cptr blitz_curse_name_str(", "static cptr blitz_blessing_name_str(",
                   "static void blitz_show_effect_summary("]:
        blitz_summary += c_function(blitz, marker)
    checks = CHECKS.replace("/* BLITZ_SUMMARY_SOURCE */", blitz_summary)
    harness = harness[:start] + checks + "\n" + harness[end:]
    # Route the real inventory's waits through scale taps while retaining the
    # existing renderer and input setup used by the general layout fixture.
    declaration = ("static bool actual_scale_active, blitz_scale_active;\n"
                   "static int actual_scale_inkey(void), blitz_scale_inkey(void);\n")
    harness = harness.replace("char __real_inkey(void);", declaration + "char __real_inkey(void);")
    harness = harness.replace("char __wrap_inkey(void)\n{",
        "char __wrap_inkey(void)\n{\n    if (actual_scale_active) return actual_scale_inkey();\n"
        "    if (blitz_scale_active) return blitz_scale_inkey();")
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/ui/sdl-song-menu.c.obj",
                "/src/sdl/ui/sdl-question-menu.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj",
                "/src/sdl/ui/sdl-halls-screen.c.obj", "/src/sdl/ui/sdl-menu-font.c.obj",
                "/src/sdl/core/sdl-layout.c.obj", "/src/sdl/config/sdl-settings.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_get_layout_screen_rect",
             "sdl_character_sheet_screen_commit_select", "save_pane_config_to_json",
             "inkey", "SDL_GetDisplayContentScale", "sdl_touch_pane_point_to_slot"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/sdl/core/sdl-layout.c"),
                    str(ROOT / "src/sdl/config/sdl-settings.c"), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, density, handheld in [(360, 800, 1, False), (800, 360, 1, False),
                                   (640, 480, 1, False), (1024, 768, 1, False),
                                   (1280, 720, 1, False), (1280, 720, 2, False),
                                   (1920, 1080, 1, False), (1280, 800, 1, True),
                                   (580, 1280, 1.5, True), (1080, 2400, 2.75, True),
                                   (2400, 1080, 2.75, True), (3840, 2160, 1, False)]:
        env["SIL_SCALE_TEST_DENSITY"] = str(density)
        if handheld:
            env["SIL_SCALE_TEST_HANDHELD"] = "1"
        else:
            env.pop("SIL_SCALE_TEST_HANDHELD", None)
        subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(width), str(height), str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
