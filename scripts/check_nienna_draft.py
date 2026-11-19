#!/usr/bin/env python3
"""Replay Nienna's draft and the curse picker with the real menus, templates and SDL input."""

import json

import check_chronicle_scroll as base
from check_phone_welcome_poetry import INIT


CHECKS = r'''
errr init_z_info(void);
errr init_cu_info(void);
cptr blessing_display_name(int id);
u64b metarun_choose_run_blessings(void);

enum replay { TOUCH_DRAFT, CANCEL_DRAFT, KEY_DRAFT, CURSE_PICK };
static enum replay replay;
static int stage;
static int offers[RUN_BLESSING_OFFER_COUNT];
static u64b expected;
static bool known_curses;

/* This fixture owns one offscreen term. Drive its events explicitly so
 * background pane reconstruction and real-time fade delays stay out of it. */
void __wrap_screen_push_supporting_panes_hidden(void) {}
void __wrap_screen_pop_supporting_panes_hidden(void) {}
static errr fixture_term_extra(int action, int value) { (void)action; (void)value; return 0; }

static void render_picker(void)
{
    SDL_Rect canvas = {0, 0, fixture_width, fixture_height};
    sdl_poetry_screen_render_canvas(&canvas);
}

static void finger(Uint32 type, float x, float y)
{
    SDL_Event event = {0};
    event.type = type;
    event.tfinger.windowID = SDL_GetWindowID(g_state.window);
    event.tfinger.fingerID = 3;
    event.tfinger.touchID = 1;
    event.tfinger.x = x / fixture_width;
    event.tfinger.y = y / fixture_height;
    sdl_handle_event(&g_state, &event);
}

static void tap_rect(SDL_FRect rect)
{
    float x = rect.x + rect.w * 0.5f, y = rect.y + rect.h * 0.5f;
    finger(SDL_EVENT_FINGER_DOWN, x, y);
    finger(SDL_EVENT_FINGER_UP, x, y);
}

static SDL_FRect scroll_to_row(int row)
{
    for (int attempt = 0; attempt < 160; attempt++) {
        SDL_FRect hit = g_sdl_poetry_screen.choices[row].hit_rect;
        SDL_FRect view = g_sdl_standalone_pager.viewport;
        if (hit.w > 0 && hit.h >= MIN(sdl_ui_min_tap_px() * 0.5f, view.h * 0.4f))
            return hit;
        fixture_assert(view.h > 0 && g_sdl_standalone_pager.maximum > 0);
        int first = 0;
        while (first < g_sdl_poetry_screen.choice_count
            && g_sdl_poetry_screen.choices[first].hit_rect.h == 0)
            first++;
        bool up = row <= first && g_sdl_standalone_pager.offset > 0;
        float x = view.x + view.w * 0.5f;
        float start = view.y + view.h * (up ? 0.22f : 0.78f);
        float end = view.y + view.h * (up ? 0.78f : 0.22f);
        int old = (int)g_sdl_standalone_pager.offset;
        finger(SDL_EVENT_FINGER_DOWN, x, start);
        finger(SDL_EVENT_FINGER_MOTION, x, end);
        finger(SDL_EVENT_FINGER_UP, x, end);
        fixture_assert(!ui_menu_click_has_pending());
        char key;
        fixture_assert(Term_inkey(&key, false, true) != 0);
        fixture_assert((int)g_sdl_standalone_pager.offset != old);
        render_picker();
    }
    fixture_assert(false);
    return (SDL_FRect){0};
}

static void read_offers(void)
{
    u64b unique = 0;
    unsigned colors = 0;
    for (int row = 0; row < RUN_BLESSING_OFFER_COUNT; row++) {
        const sdl_poetry_choice_state *item = &g_sdl_poetry_screen.choices[row];
        int id;
        for (id = 0; id < MIN(z_info->cu_max, METAR_CURSE_SLOTS); id++) {
            if (cu_info[id].blessing_name && strstr(item->label, blessing_display_name(id)))
                break;
        }
        fixture_assert(id < MIN(z_info->cu_max, METAR_CURSE_SLOTS));
        fixture_assert(!(unique & (1ULL << id)));
        fixture_assert(streq(item->effect, cu_text + cu_info[id].blessing_power));
        fixture_assert(streq(item->body, cu_text + cu_info[id].blessing_text));
        unique |= 1ULL << id;
        colors |= 1U << item->label_attr;
        offers[row] = id;
    }
    fixture_assert(colors & (colors - 1));
}

char __wrap_metarun_inkey_hidden(void)
{
    sdl_poetry_screen_state *screen = &g_sdl_poetry_screen;
    render_picker();
    if (replay == CURSE_PICK) {
        fixture_assert(screen->choice_count == 3 && screen->selection_limit == 0);
        for (int i = 0; i < 3; i++) {
            fixture_assert(screen->choices[i].body[0]);
            fixture_assert((screen->choices[i].effect[0] != 0) == known_curses);
        }
        if (!stage && fixture_width == 720) capture("curse-picker");
        return stage++ ? '\r' : '2';
    }

    fixture_assert(screen->choice_count == RUN_BLESSING_OFFER_COUNT);
    fixture_assert(screen->selection_limit == RUN_BLESSING_COUNT);
    if (stage == 0) {
        fixture_assert(screen->selected_count == 0 && screen->marked_rows == 0);
        read_offers();
        inside(screen->cancel_rect);
        inside(screen->confirm_rect);
        fixture_assert(screen->confirm_rect.h >= sdl_ui_min_tap_px());
        fixture_assert(g_sdl_standalone_pager.viewport.y + g_sdl_standalone_pager.viewport.h
            <= screen->confirm_rect.y);
        tap_rect(screen->confirm_rect);
        fixture_assert(!ui_menu_click_has_pending());
        char key;
        fixture_assert(Term_inkey(&key, false, true) != 0);
    }

    char key = 0;
    int step = stage++;
    if (replay == KEY_DRAFT) {
        /* Early Enter cannot commit. Cursor movement must reveal the focused row. */
        if (step == 0) return '\r';
        fixture_assert(screen->choices[screen->highlight].hit_rect.h > 0);
        if (step <= 8) return '2';
        if (step == 9 || step == 11 || step == 13 || step == 15) return ' ';
        if (step < 15) return '2';
        fixture_assert(step == 16 && screen->selected_count == 4);
        expected = (1ULL << offers[8]) | (1ULL << offers[9])
            | (1ULL << offers[10]) | (1ULL << offers[11]);
        return '\r';
    }
    if (replay == CANCEL_DRAFT) {
        fixture_assert(step <= 1);
        tap_rect(step == 0 ? scroll_to_row(5) : screen->cancel_rect);
    } else {
        static const int taps[] = {0, 3, 7, 11, 9, 3, 2};
        static const int counts[] = {0, 1, 2, 3, 4, 4, 3, 4};
        fixture_assert(step < (int)N_ELEMENTS(counts));
        fixture_assert(screen->selected_count == counts[step]);
        if (step == 0 && fixture_width == 720) capture("blessing-picker");
        if (step < (int)N_ELEMENTS(taps)) {
            SDL_FRect row = scroll_to_row(taps[step]);
            fixture_assert(row.y + row.h <= screen->confirm_rect.y);
            tap_rect(row);
        } else {
            /* The fifth tap was refused and a previous pick was replaced. */
            fixture_assert(screen->marked_rows == ((1U<<0) | (1U<<2) | (1U<<7) | (1U<<11)));
            expected = (1ULL << offers[0]) | (1ULL << offers[2])
                | (1ULL << offers[7]) | (1ULL << offers[11]);
            tap_rect(screen->confirm_rect);
        }
    }
    fixture_assert(Term_inkey(&key, false, true) == 0);
    return key;
}

static void check_draft(void)
{
    int first[RUN_BLESSING_OFFER_COUNT];
    metar.id = 7901;
    metar.next_blessing_seed = 55211;
    metarun original = metar;
    Rand_state_init(112233);
    u64b rng = Rand_state_export();
    replay = TOUCH_DRAFT; stage = 0;
    u64b result = metarun_choose_run_blessings();
    fixture_assert(result == expected && stage == 8 && Rand_state_export() == rng);
    memcpy(first, offers, sizeof(first));

    Rand_state_init(987654);
    rng = Rand_state_export();
    replay = CANCEL_DRAFT; stage = 0;
    fixture_assert(metarun_choose_run_blessings() == 0 && stage == 2);
    fixture_assert(!memcmp(first, offers, sizeof(first)) && Rand_state_export() == rng);
    fixture_assert(!memcmp(&original, &metar, sizeof(metar)) && !p_ptr->run_blessings);

    fixture_touch_only = false;
    replay = KEY_DRAFT; stage = 0;
    result = metarun_choose_run_blessings();
    fixture_assert(stage == 17 && result == expected && !memcmp(first, offers, sizeof(first)));
    fixture_touch_only = true;

    ++metar.next_blessing_seed;
    replay = CANCEL_DRAFT; stage = 0;
    fixture_assert(metarun_choose_run_blessings() == 0 && memcmp(first, offers, sizeof(first)));
    replay = CURSE_PICK;
    for (int known = 0; known < 2; ++known) {
        known_curses = known;
        metar.curses_seen = known ? ~(u64b)0 : 0;
        stage = 0;
        fixture_assert(metarun_debug_preview_curse_menu() >= 0 && stage == 2);
    }
}
'''

base.OUT = base.ROOT / "scripts/output/nienna-draft-check"
base.EXTRA_WRAPS = ("metarun_inkey_hidden", "screen_push_supporting_panes_hidden",
                    "screen_pop_supporting_panes_hidden")
base.HARNESS = base.PREFIX + CHECKS + INIT + r'''
    static player_race race;
    static character_profile hero;
    p_info = &race; c_info = &hero;
    ANGBAND_DIR_EDIT = DRAFT_EDIT_DIR;
    ANGBAND_DIR_DATA = ".";
    fixture_assert(init_z_info() == 0 && init_cu_info() == 0);
    g_sdl_blocking_key_wait = true;
    view->t.xtra_hook = fixture_term_extra;
    fixture.active = false;
    for (int i = 0; i < 16; i++) {
        byte *rgb = angband_color_table[i];
        g_state.palette[i] = (SDL_Color){ rgb[1], rgb[2], rgb[3], 255 };
    }
    for (int big = 0; big < 2; big++) {
        config.bigger_font = big;
        for (int menu = 0; menu < SDL_MENU_FONT_COUNT; menu++) config.menu_bigger_font[menu] = big;
        check_draft();
    }
    printf("%dx%d @%.3f: twelve fixed offers, four-pick limit, replace/confirm/cancel, touch scrolling, keyboard and curse choices PASS\n",
        fixture_width, fixture_height, fixture_density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''
base.HARNESS = "#define DRAFT_EDIT_DIR " + json.dumps((base.ROOT / "lib/edit").as_posix()) + "\n" + base.HARNESS

if __name__ == "__main__":
    base.main()
