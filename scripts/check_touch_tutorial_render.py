#!/usr/bin/env python3
"""Render the real touch guide with phone/desktop layout fixtures and fonts.

Checks card separation, text containment, gesture copy, and stale tooltips.
Uses the configured Windows build; no player files or settings are loaded.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/touch-tutorial-render-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>
#include "sdl/input/sdl-touch-tutorial.c"
#define sdl_touch_round_compute_layout fixture_control_compute_layout
#define sdl_touch_top_panel_compute_layout_for_display fixture_control_compute_display
#include "sdl/input/sdl-touch-controls.c"
#undef sdl_touch_round_compute_layout
#undef sdl_touch_top_panel_compute_layout_for_display

static SDL_Rect fixture_screen;
static SDL_FRect fixture_rolls;
static bool fixture_mobile;
static bool fixture_crowded;
static int fixture_font, fixture_page;
static SDL_FRect cards[8], footer;
static int card_count, active_card = -1, texture_count;
static char drawn_words[8192];
static bool fixture_render_ok = true;
static int direction_count, direction_sent;
static bool direction_ctrl, direction_run;

void __wrap_sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt)
{
    direction_count++; direction_sent = dir;
    direction_ctrl = ctrl; direction_run = shift;
    assert(!alt);
}

static void check(bool ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "%dx%d mobile %d font %d page %d: %s\n",
            fixture_screen.w, fixture_screen.h, fixture_mobile, fixture_font,
            fixture_page, what);
        abort();
    }
}
static bool contains(SDL_FRect box, SDL_FRect r)
{
    return r.x >= box.x - 0.1f && r.y >= box.y - 0.1f
        && r.x + r.w <= box.x + box.w + 0.1f
        && r.y + r.h <= box.y + box.h + 0.1f;
}
bool __real_SDL_RenderRect(SDL_Renderer *, const SDL_FRect *);
bool __wrap_SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *r)
{
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
    active_card = -1;
    if (red == 255 && green == 255 && blue == 0
        && (alpha == 230 || alpha == 235 || alpha == 236)) {
        check(card_count < 8, "too many cards");
        active_card = card_count;
        cards[card_count++] = *r;
    }
    return __real_SDL_RenderRect(renderer, r);
}
bool __real_SDL_RenderFillRect(SDL_Renderer *, const SDL_FRect *);
bool __wrap_SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *r)
{
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
    if (alpha == SDL_TOUCH_TUTORIAL_FOOTER_ALPHA && !red && !green && !blue) {
        footer = *r;
        active_card = -2;
    }
    return __real_SDL_RenderFillRect(renderer, r);
}
bool __real_SDL_RenderTexture(SDL_Renderer *, SDL_Texture *,
    const SDL_FRect *, const SDL_FRect *);
bool __wrap_SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_FRect *src, const SDL_FRect *dst)
{
    texture_count++;
    if (dst && active_card >= 0) {
        if (!contains(cards[active_card], *dst))
            fprintf(stderr, "card %.1f %.1f %.1f %.1f glyph %.1f %.1f %.1f %.1f\n",
                cards[active_card].x, cards[active_card].y,
                cards[active_card].w, cards[active_card].h,
                dst->x, dst->y, dst->w, dst->h);
        check(contains(cards[active_card], *dst), "text outside explanation card");
    }
    if (dst && active_card == -2)
        check(contains(footer, *dst), "footer text outside reserved area");
    return __real_SDL_RenderTexture(renderer, texture, src, dst);
}
SDL_Texture *__real_sdl_ui_text_texture(TTF_Font *, cptr, SDL_Color, int *, int *);
SDL_Texture *__wrap_sdl_ui_text_texture(TTF_Font *font, cptr text,
    SDL_Color color, int *w, int *h)
{
    SDL_strlcat(drawn_words, text, sizeof(drawn_words));
    SDL_strlcat(drawn_words, " ", sizeof(drawn_words));
    return __real_sdl_ui_text_texture(font, text, color, w, h);
}
bool __wrap_sdl_touch_only_mobile_device_active(void) { return fixture_mobile; }
bool __wrap_sdl_touch_tutorial_device_available(void) { return fixture_mobile; }
bool __wrap_touch_shortcut_context_action(int binding, bool description,
    int *key, char *label, size_t size)
{ (void)binding; (void)description; (void)key; (void)label; (void)size; return false; }
bool __wrap_sdl_touch_pane_current_rect(SDL_Rect *r)
{
    if (config.touch_profile != SDL_TOUCH_PROFILE_TOUCH_PANE) return false;
    *r = (SDL_Rect){fixture_screen.x, fixture_screen.y + fixture_screen.h - 240,
        fixture_screen.w / 2, 170};
    return true;
}
bool __wrap_steamdeck_controls_active(void) { return false; }
int __wrap_sdl_main_menu_pane_font_px(void) { return fixture_font; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void) { return fixture_screen; }
bool __wrap_sdl_main_menu_pane_current_rect(SDL_FRect *r)
{
    *r = (SDL_FRect){fixture_screen.x, fixture_screen.y, 60, 28}; return true;
}
bool __wrap_sdl_depth_menu_pane_current_rect(SDL_FRect *r)
{
    *r = (SDL_FRect){fixture_screen.x + fixture_screen.w - 60,
        fixture_screen.y, 60, 28}; return true;
}
bool __wrap_sdl_status_depth_pane_current_rect(SDL_FRect *r) { return false; }
bool __wrap_sdl_status_pane_layout(status_pane_layout *layout)
{
    layout->panel = (SDL_FRect){fixture_screen.x + fixture_screen.w - 90,
        fixture_screen.y + 140, 80, 60};
    return fixture_crowded;
}
bool __wrap_sdl_status_pane_current_rect(SDL_Rect *r, enum pane_placement *p)
{ return false; }
bool __wrap_sdl_should_show_supporting_panes(void) { return fixture_mobile || fixture_crowded; }
bool __wrap_sdl_view_is_overlay_log_pane(const sdl_view *view) { return false; }
bool __wrap_sdl_combat_overlay_pane_current_rect(SDL_Rect *r)
{
    *r = (SDL_Rect){fixture_screen.x + 8, fixture_screen.y + 55,
        fixture_screen.w - 16, 80}; return true;
}
bool __wrap_sdl_main_cell_rect(int col, int row, int cols, int rows, SDL_FRect *r)
{
    *r = (SDL_FRect){fixture_screen.x + col * 8, fixture_screen.y + row * 16,
        cols * 8, rows * 16}; return true;
}
bool __wrap_sdl_touch_top_panel_compute_layout_for_display(SDL_FRect *r,
    SDL_FRect *bounds)
{
    SDL_FRect box = {fixture_screen.x + 8,
        fixture_screen.y + fixture_screen.h - 68, fixture_screen.w - 16, 48};
    for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
        r[i] = (SDL_FRect){box.x + box.w * i / SDL_TOUCH_TOP_PANEL_BUTTON_COUNT,
            box.y, box.w / SDL_TOUCH_TOP_PANEL_BUTTON_COUNT - 2, box.h};
    if (bounds) *bounds = box;
    return true;
}
bool __wrap_sdl_touch_round_compute_layout(float *cx, float *cy, float *radius,
    float *inner, SDL_Rect *clip)
{
    float r = MIN(fixture_screen.w * 0.27f, fixture_screen.h * 0.16f);
    if (cx) *cx = fixture_screen.x + fixture_screen.w * 0.30f;
    if (cy) *cy = fixture_screen.y + fixture_screen.h - r - 100;
    if (radius) *radius = r;
    if (inner) *inner = r * 0.45f;
    if (clip) *clip = fixture_screen;
    return true;
}
bool __wrap_sdl_render_current_window_frame(void)
{
    check(g_touch_tutorial_suppress_runtime_top_panel,
        "tutorial snapshot did not suppress live tooltips");
    active_card = -1;
    SDL_SetRenderDrawColor(g_state.renderer, 12, 18, 22, 255);
    SDL_RenderClear(g_state.renderer);
    if (fixture_mobile && fixture_screen.w < fixture_screen.h) {
        SDL_SetRenderDrawColor(g_state.renderer, 18, 68, 106, 255);
        SDL_RenderFillRect(g_state.renderer, &fixture_rolls);
        sdl_touch_tutorial_draw_text_line("Combat roll description",
            fixture_rolls.x + 8, fixture_rolls.y + fixture_rolls.h - 65,
            fixture_rolls.w - 16, 20,
            g_state.palette[TERM_L_WHITE], false);
        sdl_touch_tutorial_draw_text_line("Tap to open combat history",
            fixture_rolls.x + 8, fixture_rolls.y + fixture_rolls.h - 35,
            fixture_rolls.w - 16, 20,
            g_state.palette[TERM_L_WHITE], false);
    }
    int before = texture_count;
    sdl_object_tooltip_render();
    check(before == texture_count, "stale live hover tooltip rendered");
    return fixture_render_ok;
}
static void paint(int width, int height, bool mobile, int font, int inset, int profile)
{
    fixture_screen = (SDL_Rect){inset, inset, width - inset * 2, height - inset * 2};
    fixture_mobile = mobile; fixture_font = font;
    fixture_crowded = font >= 48 || height < width;
    fixture_rolls = (SDL_FRect){fixture_screen.x + fixture_screen.w * 0.38f,
        fixture_screen.y + 55, fixture_screen.w * 0.60f,
        MIN(fixture_screen.h * 0.18f, 260)};
    g_state.window = SDL_CreateWindow("Touch guide fixture", width, height, SDL_WINDOW_HIDDEN);
    check(g_state.window != NULL, "window creation failed");
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    check(g_state.renderer != NULL, "software renderer failed");
    sdl_view *view = &g_views[PANE_MAIN];
    term_init(&view->t, width / 8, height / 16, 256);
    Term_activate(&view->t); term_screen = &view->t;
    view->term_ready = true; view->rect = fixture_screen;
    view->cell_w = 8; view->cell_h = 16; view->cols = width / 8; view->rows = height / 16;
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, width, height);
    check(view->canvas != NULL, "canvas creation failed");
    enum pane_type supporting[] = {PANE_ROLLS, PANE_INVENTORY, PANE_WORN, PANE_LOG};
    for (int i = 0; i < (int)N_ELEMENTS(supporting); i++) {
        sdl_view *pane = &g_views[supporting[i]];
        pane->term_ready = i == 0 || fixture_crowded;
        pane->canvas = view->canvas;
        pane->rect = (SDL_Rect){fixture_screen.x + fixture_screen.w - 100,
            fixture_screen.y + 200 + i * 65, 90, 60};
    }
    g_views[PANE_ROLLS].rect = (SDL_Rect){(int)fixture_rolls.x,
        (int)fixture_rolls.y, (int)fixture_rolls.w, (int)fixture_rolls.h};
    g_object_tooltip.active = true; g_object_tooltip.screen_rect = true;
    g_object_tooltip.rect = (SDL_FRect){10, 80, 200, 50};
    SDL_strlcpy(g_object_tooltip.text, "STALE HOVER DESCRIPTION", sizeof(g_object_tooltip.text));
    g_touch_top_panel_description_slot = 0;
    config.touch_profile = profile;
    int pages = mobile ? 3 : 2;
    for (int page = 0; page < pages; page++) {
        char page_words[16384] = "";
        tutorial_panel_part = 0;
        do {
        fixture_page = page;
        card_count = 0; active_card = -1; drawn_words[0] = '\0';
        footer = (SDL_FRect){0}; texture_count = 0;
        sdl_touch_tutorial_draw_page(page, true, pages, false);
        SDL_strlcat(page_words, drawn_words, sizeof(page_words));
        check(tutorial_panel_parts < 64, "excessive continuation pages");
        check(!g_touch_tutorial_suppress_runtime_top_panel, "suppression state leaked");
        check(g_object_tooltip.active, "tutorial discarded the live tooltip state");
        check(g_touch_top_panel_description_slot == 0, "tutorial discarded quick-access description");
        check(card_count == 2, "heading or explanation disappeared");
        SDL_FRect screen = {fixture_screen.x, fixture_screen.y,
            fixture_screen.w, fixture_screen.h};
        for (int i = 0; i < card_count; i++) {
            check(contains(screen, cards[i]), "card outside safe area");
            if (cards[i].y + cards[i].h > footer.y)
                fprintf(stderr,"profile %d part %d/%d card %.1f+%.1f footer %.1f\n",
                    profile,tutorial_panel_part+1,tutorial_panel_parts,cards[i].y,cards[i].h,footer.y);
            check(cards[i].y + cards[i].h <= footer.y, "card overlaps footer");
            for (int j = 0; j < i; j++)
                check(!SDL_HasRectIntersectionFloat(&cards[i], &cards[j]), "explanation cards overlap");
        }
        if (mobile && width < height && page == 1) {
            SDL_Rect combat;
            __wrap_sdl_combat_overlay_pane_current_rect(&combat);
            check(cards[0].y >= combat.y + combat.h + 8,
                "shortcut explanation covers the combat panel");
            check(cards[0].y >= fixture_rolls.y + fixture_rolls.h + 7,
                "shortcut explanation covers the rolls description");
        }
        bool last_part = tutorial_panel_part + 1 == tutorial_panel_parts;
        if (page == pages - 1 && last_part) {
            if (profile == SDL_TOUCH_PROFILE_ROUND_WHEEL) {
            check(strstr(page_words, "Long press an arrow:") != NULL,
                "long-press instructions missing");
            check(strstr(page_words, "tunnel,") != NULL, "directional action missing");
            check(strstr(page_words, "outer rim") != NULL, "run rim instructions missing");
            check(strstr(page_words, "Wait / Rest:") != NULL, "last instructions missing");
            } else if (profile == SDL_TOUCH_PROFILE_CORNERS) {
                check(strstr(page_words, "configurable commands") != NULL,
                    "corners instructions missing");
                check(strstr(page_words, "Touch Settings") != NULL,
                    "last corners instructions missing");
            } else {
                check(strstr(page_words, "Touch") != NULL && strstr(page_words, "Settings") != NULL,
                    "touch pane instructions missing");
            }
            check(strstr(drawn_words, "close") != NULL, "last page does not say close");
        } else if (page == pages - 1) {
            check(strstr(drawn_words, "next") != NULL, "continuation must not close the guide");
        }
        if (page == pages - 1 && profile == SDL_TOUCH_PROFILE_ROUND_WHEEL) {
            if (mobile && width < height) {
                float cx, cy, radius;
                __wrap_sdl_touch_round_compute_layout(&cx, &cy, &radius, NULL, NULL);
                check(cards[0].y + cards[0].h <= cy - radius,
                    "portrait explanation covers the wheel illustration");
            }
        }
        char name[128];
        snprintf(name, sizeof(name), "%dx%d-mobile%d-font%d-inset%d-profile%d-page%d-part%d.png",
            width, height, mobile, font, inset, profile, page + 1, tutorial_panel_part + 1);
        SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
        check(pixels && IMG_SavePNG(pixels, name), "capture failed");
        SDL_DestroySurface(pixels);
        /* Match the live guide's frame boundary before the next redraw. */
        SDL_RenderPresent(g_state.renderer);
        if (last_part) break;
        int old_part = tutorial_panel_part, same_page = page;
        SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);
        SDL_Event tap={0}; tap.type=SDL_EVENT_FINGER_DOWN;
        tap.tfinger.touchID=1; tap.tfinger.fingerID=7;
        check(SDL_PushEvent(&tap), "could not queue touch down");
        tap.type=SDL_EVENT_FINGER_UP;
        check(SDL_PushEvent(&tap), "could not queue touch up");
        int action=sdl_touch_tutorial_wait_action(0);
        check(action==1, "tap did not advance continuation");
        check(!sdl_touch_tutorial_navigate(&same_page,pages,action), "closed before reading all text");
        check(same_page == page && tutorial_panel_part == old_part + 1, "did not advance text");
        check(!sdl_touch_tutorial_navigate(&same_page,pages,-1), "back closed guide");
        check(tutorial_panel_part == old_part, "back skipped text");
        check(!sdl_touch_tutorial_navigate(&same_page,pages,1), "next closed guide");
        } while (true);
    }
    int final_page = pages - 1;
    check(sdl_touch_tutorial_navigate(&final_page,pages,1), "last tap did not close");
    fixture_render_ok = false;
    sdl_touch_tutorial_draw_page(0, true, pages, false);
    check(!g_touch_tutorial_suppress_runtime_top_panel, "failed snapshot leaked suppression");
    fixture_render_ok = true;
    sdl_story_font_cache_clear();
    sdl_ui_text_cache_clear();
    SDL_DestroyTexture(view->canvas); view->canvas = NULL; view->term_ready = false;
    for (int i = 0; i < (int)N_ELEMENTS(supporting); i++) {
        g_views[supporting[i]].canvas = NULL;
        g_views[supporting[i]].term_ready = false;
    }
    term_nuke(&view->t); Term = NULL; term_screen = NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
    SDL_DestroyWindow(g_state.window); g_state.window = NULL;
    printf("Touch guide %dx%d mobile %d font %d inset %d profile %d: PASS\n",
        width, height, mobile, font, inset, profile);
}
static void wheel_gestures(void)
{
    struct {
        bool button; float distance; int ms, dir; bool ctrl, run;
    } cases[] = {
        {true, 75, 1, 6, false, false},
        {true, 75, 400, 6, true, false},
        {false, 60, 400, 6, false, false},
        {false, 98, 400, 6, false, true},
        {false, 0, 1, 8, false, false},
    };
    for (int i = 0; i < (int)N_ELEMENTS(cases); i++) {
        g_touch_round_last_dir = 8;
        g_touch_round_press = (touch_round_press_state){
            .active = true, .finger_id = 1, .center_x = 100, .center_y = 100,
            .radius = 100, .inner_radius = 45, .button_press = cases[i].button,
            .button_dir = cases[i].button ? 6 : 0,
            .start_time = SDL_GetTicksNS() - (Uint64)cases[i].ms * 1000000ULL,
        };
        direction_count = 0;
        assert(sdl_touch_round_handle_pointer_up(100 + cases[i].distance, 100, 1));
        assert(direction_count == 1 && direction_sent == cases[i].dir);
        assert(direction_ctrl == cases[i].ctrl && direction_run == cases[i].run);
        assert(!g_touch_round_press.active);
    }
    puts("Wheel step, long press, inner drag, outer-rim run, and center repeat: PASS");
}
int main(int argc, char **argv)
{
    maxima limits = {0}; player_type player = {0};
    assert(argc == 2);
    setbuf(stdout, NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)); assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[1], sizeof(config.story_font2));
    g_state.system_scale = 1; z_info = &limits; p_ptr = &player;
    player.playing = true; character_generated = character_dungeon = true;
    g_state.palette[TERM_YELLOW] = (SDL_Color){255, 255, 0, 255};
    g_state.palette[TERM_L_WHITE] = (SDL_Color){210, 210, 210, 255};
    g_state.palette[TERM_L_BLUE] = (SDL_Color){0, 210, 220, 255};
    g_state.palette[TERM_L_GREEN] = (SDL_Color){0, 255, 0, 255};
    g_state.palette[TERM_GREEN] = (SDL_Color){0, 180, 0, 255};
    g_state.palette[TERM_WHITE] = (SDL_Color){200, 200, 200, 255};
    g_state.palette[TERM_ORANGE] = (SDL_Color){255, 150, 60, 255};
    g_state.palette[TERM_L_RED] = (SDL_Color){255, 100, 100, 255};
    g_state.palette[TERM_VIOLET] = (SDL_Color){200, 110, 240, 255};
    g_state.palette[TERM_UMBER] = (SDL_Color){170, 120, 80, 255};
    for (int profile = 0; profile < SDL_TOUCH_PROFILE_COUNT; ++profile) {
        paint(588, 1254, true, 32, 0, profile);
        paint(360, 800, true, 32, 0, profile);
        paint(360, 640, true, 32, 0, profile);
        paint(800, 360, true, 24, 0, profile);
        paint(1254, 588, true, 32, 0, profile);
        paint(588, 1254, true, 48, 20, profile);
        paint(1080, 2400, true, 48, 32, profile);
        paint(1280, 720, false, 24, 0, profile);
    }
    wheel_gestures();
    TTF_Quit(); SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith((
        "/src/main.c.obj", "/src/sdl/input/sdl-touch-tutorial.c.obj",
        "/src/sdl/input/sdl-touch-controls.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin"] +
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] + [env["PATH"]])
    wraps = ["SDL_RenderRect", "SDL_RenderFillRect", "SDL_RenderTexture", "sdl_ui_text_texture",
             "sdl_touch_only_mobile_device_active", "sdl_touch_tutorial_device_available",
             "sdl_touch_pane_current_rect",
             "touch_shortcut_context_action",
             "steamdeck_controls_active", "sdl_main_menu_pane_font_px", "sdl_get_layout_screen_rect",
             "sdl_main_menu_pane_current_rect", "sdl_depth_menu_pane_current_rect",
             "sdl_status_depth_pane_current_rect", "sdl_status_pane_layout", "sdl_status_pane_current_rect",
             "sdl_should_show_supporting_panes", "sdl_combat_overlay_pane_current_rect",
             "sdl_view_is_overlay_log_pane",
             "sdl_main_cell_rect", "sdl_touch_top_panel_compute_layout_for_display",
             "sdl_touch_round_compute_layout", "sdl_render_current_window_frame"]
    wraps.append("sdl_gamepad_send_direction_mods")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                   cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
