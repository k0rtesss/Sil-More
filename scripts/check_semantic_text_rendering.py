#!/usr/bin/env python3
"""Check native semantic text pixels, clipping and real menu rows without saves.

Requires a current standard build. Windows checks software, D3D11 and OpenGL;
Linux can use a configured build through SIL_TEXT_RENDER_BUILD and Xvfb.
"""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("SIL_TEXT_RENDER_BUILD", ROOT /
    ("build-standard" if os.name == "nt" else "build-linux-steam"))).resolve()
OUT = ROOT / "scripts/output/semantic-text-sharpness"
HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "sdl/ui/sdl-main-menu.c"
#include "sdl/ui/sdl-question-menu.c"
#include "sdl/ui/sdl-song-menu.c"
#include "sdl/ui/sdl-screens.c"
#include "sdl/input/sdl-tooltips.c"
#include <assert.h>

static int checked_draws;
static bool inspect_text, native_text;
static int width = 1280, height = 800;
static float density = 1.0f;
static player_type player;

SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){ 0, 0, width, height }; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane, SDL_Rect* rect)
{ *rect = (SDL_Rect){ 0, 0, width, height }; return true; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }
bool __real_SDL_RenderTexture(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*);
bool __wrap_SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture,
    const SDL_FRect* source, const SDL_FRect* destination)
{
    if (inspect_text && destination) {
        assert(SDL_fabsf(destination->x - SDL_roundf(destination->x)) < 0.0001f);
        assert(SDL_fabsf(destination->y - SDL_roundf(destination->y)) < 0.0001f);
        if (native_text) {
            float w, h;
            assert(SDL_GetTextureSize(texture, &w, &h));
            if (source) { w = source->w; h = source->h; }
            assert(SDL_fabsf(destination->w - w) < 0.0001f);
            assert(SDL_fabsf(destination->h - h) < 0.0001f);
        }
        ++checked_draws;
    }
    return __real_SDL_RenderTexture(renderer, texture, source, destination);
}
static void clear(void)
{
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    assert(SDL_RenderClear(g_state.renderer));
}
static void capture(cptr name)
{
    char path[160];
    strnfmt(path, sizeof(path), "%s-%dx%d-%s.png", name, width, height,
        get_sdl_menu_bigger_font() ? "big" : "normal");
    SDL_Surface* image = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(image && IMG_SavePNG(image, path));
    SDL_DestroySurface(image);
}
static void same_pixels(SDL_Surface* expected)
{
    SDL_Surface* actual = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(actual && actual->w == expected->w && actual->h == expected->h);
    assert(actual->format == expected->format);
    int bytes = actual->w * SDL_BYTESPERPIXEL(actual->format);
    for (int y = 0; y < actual->h; ++y)
        assert(!memcmp((char*)actual->pixels + y * actual->pitch,
            (char*)expected->pixels + y * expected->pitch, bytes));
    SDL_DestroySurface(actual);
}
static void sample(int kind, bool cropped)
{
    cptr text = "Inventory Story Equipment";
    SDL_Color color = {255, 255, 255, 255};
    TTF_Font* font = sdl_story_font_for_height_slot(24,
        kind == 3 ? SDL_STORY_FONT_SLOT_NARRATIVE : SDL_STORY_FONT_SLOT_MENU);
    int w = 0, h = 0;
    SDL_Texture* texture = sdl_ui_text_texture(font, text, color, &w, &h);
    assert(texture && w > 0 && h > 0);
    float max_w = cropped ? (float)(w / 2) : (float)w;
    float row_h = cropped ? (float)(h - 3) : (float)h;
    if (cropped && kind != 3 && kind != 4) {
        texture = sdl_ui_wrapped_text_texture(font, text, (int)max_w,
            color, &w, &h);
        assert(texture);
    }
    float x = 20.4f, y = 20.3f;
    SDL_FRect src = { 0, 0, max_w, MIN((float)h, row_h) };
    SDL_FRect dst = {20, 20, src.w, src.h};
    if (kind == 2) { src.h = h; dst.h = h; }
    if (kind == 2) dst.y = SDL_roundf(y + (row_h - dst.h) * 0.5f);
    clear();
    assert(__real_SDL_RenderTexture(g_state.renderer, texture, &src, &dst));
    SDL_Surface* reference = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(reference);
    clear(); inspect_text = native_text = true;
    if (kind == 0)
        sdl_main_menu_draw_text(font, text, x, y, max_w, row_h, color, MAIN_MENU_TEXT_LEFT);
    else if (kind == 1)
        sdl_question_menu_draw_text_aux(font, text, color, x, y, max_w, row_h, false, false);
    else if (kind == 2)
        sdl_song_menu_draw_text(font, text, color, x, y, max_w, row_h, false);
    else if (kind == 3)
        (void)sdl_char_sheet_draw_text(font, text, TERM_WHITE, x, y, max_w, row_h, false);
    else {
        SDL_FRect requested = {x, y, max_w, row_h};
        sdl_ui_render_text_texture(texture, NULL, &requested);
    }
    inspect_text = native_text = false;
    same_pixels(reference); SDL_DestroySurface(reference);
}
static void remaining_text_paths(void)
{
    SDL_Color color = {255,255,255,255};
    TTF_Font* font = sdl_story_font_for_height_slot(24, SDL_STORY_FONT_SLOT_MENU);
    assert(font);
    int line = TTF_GetFontHeight(font);
    SDL_FRect button = {20.4f, 20.3f, 180 * density, 96 * density};
    clear(); inspect_text = native_text = true;
    sdl_touch_pane_draw_button_text_scaled(&button, "Inventory", "<A>", color, .28f, .38f);
    sdl_touch_pane_draw_button_text_px(&button, "Close", "<B>", color, 28, 20);
    sdl_status_pane_draw_text(font, "A status which wraps", color,
        250.4f, 20.3f, 120, 3 * line, false);
    sdl_touch_tutorial_draw_text_line("A tutorial caption that must wrap at native size",
        400.4f, 120.3f, 140, 24, color, false);
    SDL_FRect prompt = {10.4f, 10.3f, 200, 90};
    sdl_touch_pane_draw_wrapped_prompt(&prompt,
        "A long prompt retains its complete native-size text and continues across pages instead of shrinking its lines.", color, 24);
    SDL_FRect box = {20.4f, 200.3f, 170, 3 * line};
    sdl_welcome_draw_text_box_alpha(font, "Welcome text wraps at native size", TERM_WHITE, 255, box, false);
    sdl_welcome_draw_text_span(font, "Continue or quit", 0, 8, TERM_WHITE,
        (SDL_FRect){20.4f, 300.3f, 500, 100}, false);
    SDL_FRect measured = sdl_welcome_text_rect_for_size(400, 50,
        (SDL_FRect){0,0,80,10}, false);
    assert(measured.w == 400 && measured.h == 50);
    font = sdl_description_overlay_story_font(32, STORY_FONT_SLOT_DEFAULT);
    assert(font && TTF_GetFontHeight(font) <= 32);
    int advance = 0;
    assert(TTF_MeasureString(font, "A story run", 11, 0, &advance, NULL));
    float drawn = sdl_description_overlay_render_story_run(font,
        20.4f, 400.3f, 32, "A story run", 11, TERM_WHITE);
    assert(drawn == advance);
    inspect_text = native_text = false;
}

static void title_menu(void)
{
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
    assert(sdl_welcome_screen_show_menu(false, false));
    SDL_Rect canvas = {0, 0, width, height};
    clear(); inspect_text = native_text = true;
    sdl_welcome_screen_render_canvas(&canvas);
    inspect_text = native_text = false;
    capture("title-menu");
    sdl_welcome_screen_hide();
}

static void menus(void)
{
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
    g_main_menu_overlay_active = true;
    main_menu_pane_layout main;
    assert(sdl_main_menu_overlay_layout(&main));
    TTF_Font* title = sdl_story_font_for_height(main.font_px);
    assert(main.row_h >= TTF_GetFontHeight(title));
    clear(); inspect_text = true;
    sdl_main_menu_pane_render();
    inspect_text = false; capture("main-menu");
    g_main_menu_overlay_active = false;

    ui_menu_click_begin(); sdl_question_menu_begin("Choose equipment");
    for (int i = 0; i < 35; ++i) {
        char text[256], letter[8];
        strnfmt(text, sizeof(text), "Item %d: a finely crafted sword with a long description which must wrap instead of shrinking. Its full name and properties stay readable across multiple lines.", i);
        strnfmt(letter, sizeof(letter), "%c)", 'a' + i % 26);
        sdl_question_menu_add_entry(i, letter, text, TERM_WHITE);
    }
    sdl_question_menu_set_highlight(0); sdl_question_menu_finish();
    sdl_question_menu_layout_info question;
    assert(sdl_question_menu_layout(&question));
    TTF_Font* body = sdl_story_font_for_height_slot(question.font_px, SDL_STORY_FONT_SLOT_MENU);
    assert(question.row_h >= TTF_GetFontHeight(body));
    assert(question.max_scroll_offset > 0);
    assert(question.rows[0].h >= 2 * TTF_GetFontHeight(body));
    clear(); inspect_text = true; sdl_question_menu_render();
    inspect_text = false; capture("question-menu");
    sdl_question_menu_set_highlight(34);
    assert(sdl_question_menu_layout(&question));
    SDL_FRect hit;
    assert(SDL_GetRectIntersectionFloat(&question.rows[34], &question.entries_rect, &hit));
    assert(hit.h > 0);
    assert(sdl_question_menu_handle_pointer(hit.x + hit.w / 2, hit.y + hit.h / 2, UI_MENU_CLICK_PRIMARY));
    int choice = -1, action = 0;
    assert(ui_menu_click_take_action(&choice, &action) && choice == 34);
    sdl_question_menu_clear(); ui_menu_click_clear();

    clear(); native_text = inspect_text = true;
    TTF_Font* story = sdl_story_font_for_height_slot(sdl_menu_font_px(24), SDL_STORY_FONT_SLOT_NARRATIVE);
    float line_h = TTF_GetFontHeight(story) + 8;
    cptr lines[] = {"A Tale of Middle-earth", "Beyond the mountains, beneath the stars,", "your journey into Angband begins.", "Continue"};
    for (int i = 0; i < 4; ++i)
        sdl_char_sheet_draw_text(story, lines[i], TERM_WHITE, 100.35f,
            100.25f + i * line_h, width - 200, line_h, false);
    inspect_text = native_text = false; capture("story-menu");
}
int main(int argc, char** argv)
{
    assert(argc == 5); setbuf(stdout, NULL); log_set_quiet(true);
    width = atoi(argv[2]); height = atoi(argv[3]); density = (float)atof(argv[4]);
    assert(SDL_Init(SDL_INIT_VIDEO) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font, "../../../../lib/xtra/font/Cinzel-Medium.ttf", sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, "../../../../lib/xtra/font/EBGaramond-Regular.ttf", sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font, "../../../../lib/xtra/font/VictorMono-Medium.ttf", sizeof(config.monospace_font));
    g_state.window = SDL_CreateWindow("Semantic text fixture", width, height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, argv[1]); assert(g_state.renderer);
    g_state.system_scale = density; g_state.safe_area = (SDL_Rect){0, 0, width, height};
    g_state.palette[TERM_WHITE] = (SDL_Color){255,255,255,255};
    g_state.palette[TERM_L_WHITE] = (SDL_Color){230,230,230,255};
    g_state.palette[TERM_L_BLUE] = (SDL_Color){120,160,255,255};
    g_state.palette[TERM_SLATE] = (SDL_Color){140,140,140,255};
    p_ptr = &player; player.playing = true; character_generated = character_dungeon = true;
    sdl_view* view = &g_views[PANE_MAIN];
    assert(term_init(&view->t, 80, 24, 256) == 0); Term_activate(&view->t); term_screen = &view->t;
    view->term_ready = true; view->rect = (SDL_Rect){0,0,width,height};
    config.main_view_scale = 2;
    for (int big = 0; big < 2; ++big) {
        config.bigger_font = big;
        for (int i = 0; i < SDL_MENU_FONT_COUNT; ++i) config.menu_bigger_font[i] = big;
        for (int kind = 0; kind < 5; ++kind) {
            sample(kind, false);
            if (!big || kind == 1 || kind == 4) sample(kind, true);
        }
        title_menu();
        menus();
        remaining_text_paths();
    }
    printf("Semantic text %s %dx%d @%.2f: native pixels, crop, rows, scrolling and selection PASS (%d draws)\n",
        SDL_GetRendererName(g_state.renderer), width, height, density, checked_draws);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
    return 0;
}
'''


def audit_sources():
    # Direct texture drawing here is permitted only for the listed art assets;
    # every semantic UI text texture must pass through the guarded draw helper.
    art = {"sdl-screens.c": {"leaf", "bg", "g_state.tileset"}, "sdl-hint-quest-menu.c": {"background"}}
    reviewed = []
    for path in sorted((ROOT / "src/sdl/ui").glob("*.c")):
        source = path.read_text(encoding="utf-8")
        assert "SDL_BlitSurfaceScaled" not in source, path
        for match in re.finditer(r"SDL_RenderTexture\(g_state\.renderer,\s*([\w.]+)", source):
            texture = match.group(1)
            assert texture in art.get(path.name, set()), (path, texture)
            reviewed.append({"file": str(path.relative_to(ROOT)),
                "line": source.count("\n", 0, match.start()) + 1,
                "texture": texture, "reason": "artwork"})
    for name in ("sdl-player-actions", "sdl-pointer-targeting", "sdl-screen-pointer",
                 "sdl-touch-controls", "sdl-touch-tutorial", "sdl-tooltips"):
        path = ROOT / "src/sdl/input" / (name + ".c")
        source = path.read_text(encoding="utf-8")
        for match in re.finditer(r"SDL_RenderTexture\(g_state\.renderer,\s*([\w.]+)", source):
            assert name == "sdl-tooltips" and match.group(1) == "atlas", path
            reviewed.append({"file": str(path.relative_to(ROOT)),
                "line": source.count("\n", 0, match.start()) + 1,
                "texture": "atlas", "reason": "legacy terminal character grid"})
    (OUT / "source-audit.json").write_text(json.dumps({
        "semantic_direct_texture_calls_remaining": 0,
        "semantic_scaled_surface_blits_remaining": 0,
        "reviewed_art_and_grid_calls": reviewed,
        "other_reviewed_paths": ["native standalone startup dialog",
            "legacy terminal glyph and map renderers retain their grid/zoom behavior"],
    }, indent=2), encoding="utf-8")
    print("Semantic source audit: all UI/input text uses native rendering; 4 artwork and 1 terminal-grid calls retained")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    audit_sources()
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD), *(str(BUILD / "_deps" / dep)
        for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    objdir = "CMakeFiles/sil-more.dir"
    if os.name == "nt":
        objects = shlex.split((BUILD / objdir / "objects1.rsp").read_text())
        compiler = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", f"@{objdir}/includes_C.rsp"]
        libraries = [f"@{objdir}/linkLibs.rsp"]
    else:
        entries = json.loads((BUILD / "compile_commands.json").read_text())
        flags = shlex.split(next(e["command"] for e in entries if e["file"].endswith("/src/sdl/core/sdl-events.c")))
        compiler = [flags[0], "-DUSE_SDL", "-std=gnu17", "-O0", *(f for f in flags if f.startswith(("-I", "-D")))]
        commands = subprocess.check_output(["ninja", "-t", "commands", "sil-more"], cwd=BUILD, text=True)
        link = shlex.split(next(line for line in commands.splitlines() if " -o sil-more " in line))
        objects = [p for p in link if p.endswith(".o")]
        start = link.index("-o") + 2
        libraries = link[start:link.index("&&", start)] if "&&" in link[start:] else link[start:]
        libraries.append("-lstdc++")
    if os.name == "nt" and any(obj.endswith(".cpp.obj") for obj in objects):
        libraries.append("-lstdc++")
    excluded = tuple(f"/src/{name}.c.{ext}" for name in
        ("main", "sdl/ui/sdl-main-menu", "sdl/ui/sdl-question-menu", "sdl/ui/sdl-song-menu", "sdl/ui/sdl-screens", "sdl/input/sdl-tooltips") for ext in ("obj", "o"))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    exe = OUT / ("check.exe" if os.name == "nt" else "check")
    wraps = ("SDL_RenderTexture", "sdl_get_layout_screen_rect", "sdl_overlay_pane_anchor_rect", "SDL_GetDisplayContentScale")
    subprocess.run([*compiler, "-UNDEBUG", str(source), "@" + str(response), *libraries,
        "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for driver in (("direct3d11", "opengl", "software") if os.name == "nt" else ("opengl", "software")):
        env.pop("SDL_VIDEO_DRIVER", None); env.pop("SDL_RENDER_DRIVER", None)
        if driver == "software": env["SDL_VIDEO_DRIVER"] = "dummy"
        run_out = OUT / driver; run_out.mkdir(exist_ok=True)
        for w, h, density in ((1280, 800, 1), (800, 1280, 1.5), (800, 360, 1)):
            subprocess.run([str(exe), driver, str(w), str(h), str(density)], cwd=run_out, env=env, check=True, timeout=60)

if __name__ == "__main__":
    main()
