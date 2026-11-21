#!/usr/bin/env python3
"""Render compact tutorial zones and check complete, unobscured badges.

This reuses the isolated SDL fixture and production tutorial source, with the
mobile compile-time branches enabled.  It captures PNGs under scripts/output
and reads build-standard objects without rebuilding or touching player data.
Use --baseline to demonstrate the old badge regression against HEAD.
"""
from pathlib import Path
import ast
import os
import re
import subprocess
import sys

import check_touch_tutorial_render as fixture

ROOT = Path(__file__).resolve().parents[1]
fixture.OUT = ROOT / "scripts/output/touch-zone-badge-check"


def replace_once(source, old, new):
    assert source.count(old) == 1, f"fixture changed: {old[:80]}"
    return source.replace(old, new, 1)


def check_startup_copy():
    source = (ROOT / "src/sdl/core/sdl-layout.c").read_text(encoding="utf-8")
    settings = (ROOT / "src/cmd/ui/cmd-ui-settings.c").read_text(encoding="utf-8")
    for function, route in (
        ("sdl_prompt_mobile_startup_portrait_mode",
         "Options > Display and Layout > Orientation"),
        ("sdl_prompt_mobile_startup_bigger_font",
         "Options > Text Options > Text Size and Fonts > Big Font"),
    ):
        body = source.split(f"bool {function}(void)", 1)[1].split(
            ".numbuttons", 1)[0].split(".message =", 1)[1]
        message = "".join(ast.literal_eval(s) for s in
                          re.findall(r'"(?:[^"\\]|\\.)*"', body))
        assert route in message, (function, message)
        assert "General Settings" not in message
    for label in ("Display and Layout", "Orientation", "Text Options",
                  "Text Size and Fonts", "Big Font", "Show B button"):
        assert f'"{label}"' in settings, label

    def extract(name):
        start = source.index(f"bool {name}(void)")
        opening = source.index("{", start)
        depth = 1
        end = opening + 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        return source[start:end]

    # Capture the message-box data from the production functions themselves;
    # only SDL, logging and the device classification are replaced with stubs.
    harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
typedef struct { int flags, buttonid; const char *text; } SDL_MessageBoxButtonData;
typedef struct { int flags; void *window; const char *title, *message;
    int numbuttons; const SDL_MessageBoxButtonData *buttons; void *colorScheme;
} SDL_MessageBoxData;
enum { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT = 1,
    SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT = 2,
    SDL_MESSAGEBOX_INFORMATION = 4, SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT = 8,
    SDL_STARTUP_DEVICE_MOBILE_TOUCH = 1 };
#define N_ELEMENTS(x) (sizeof(x) / sizeof((x)[0]))
#define log_warn(...) ((void)0)
#define log_info(...) ((void)0)
static struct { void *window; } g_state;
static int g_startup_device_class = SDL_STARTUP_DEVICE_MOBILE_TOUCH;
static bool tablet, show_ok = true;
static int answer, calls;
static const char *captured_message;
static bool sdl_mobile_device_is_tablet(void) { return tablet; }
static bool SDL_ShowMessageBox(const SDL_MessageBoxData *box, int *selected) {
    ++calls; captured_message = box->message; *selected = answer; return show_ok;
}
''' + extract("sdl_prompt_mobile_startup_portrait_mode") + "\n" + extract(
        "sdl_prompt_mobile_startup_bigger_font") + r'''
int main(void) {
    answer = 0; assert(!sdl_prompt_mobile_startup_portrait_mode());
    assert(strstr(captured_message, "Options > Display and Layout > Orientation"));
    assert(!strstr(captured_message, "General Settings"));
    answer = 1; assert(sdl_prompt_mobile_startup_portrait_mode());
    answer = 0; assert(!sdl_prompt_mobile_startup_bigger_font());
    assert(strstr(captured_message,
        "Options > Text Options > Text Size and Fonts > Big Font"));
    assert(!strstr(captured_message, "General Settings"));
    answer = 1; assert(sdl_prompt_mobile_startup_bigger_font());
    int previous_calls = calls;
    tablet = true; assert(!sdl_prompt_mobile_startup_bigger_font());
    assert(calls == previous_calls);
    tablet = false; show_ok = false;
    assert(!sdl_prompt_mobile_startup_bigger_font());
    assert(!sdl_prompt_mobile_startup_portrait_mode());
    return 0;
}
'''
    source_path = fixture.OUT / "startup-check.c"
    exe = fixture.OUT / "startup-check.exe"
    source_path.write_text(harness, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
                                   env.get("PATH", "")])
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    "-Werror", str(source_path), "-o", str(exe)],
                   env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=10)
    print("Startup message boxes and options paths: PASS", flush=True)


def instrument(source):
    source = replace_once(source,
        '#include "sdl/input/sdl-touch-tutorial.c"', r'''
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
bool sdl_mobile_lifecycle_handle_event(const SDL_Event *event)
{ (void)event; return false; }
#include "sdl/input/sdl-touch-tutorial.c"
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 0''')
    source = replace_once(source, 'static SDL_FRect cards[8], footer;', r'''
static SDL_FRect cards[8], footer;
static SDL_FRect badges[16];
static char badge_names[16][64], last_plain_text[64];
static SDL_Texture *last_plain_texture;
static int badge_count, active_badge = -1;
static bool fixture_big;''')
    source = replace_once(source,
        '    return __real_SDL_RenderRect(renderer, r);', r'''
    if (red == 255 && green == 255 && blue == 0 && alpha == 238) {
        check(badge_count < 16, "too many badges");
        badges[badge_count] = *r;
        SDL_strlcpy(badge_names[badge_count], last_plain_text,
            sizeof(badge_names[badge_count]));
        active_badge = badge_count++;
    }
    return __real_SDL_RenderRect(renderer, r);''')
    source = replace_once(source,
        '    texture_count++;\n    if (dst && active_card >= 0) {', r'''
    texture_count++;
    if (dst && active_badge >= 0) {
        float texture_w = 0, texture_h = 0;
        check(texture == last_plain_texture, "badge did not use unwrapped label");
        check(SDL_GetTextureSize(texture, &texture_w, &texture_h), "no texture size");
        check(src && src->x == 0 && src->y == 0
            && src->w == texture_w && src->h == texture_h,
            "badge label texture was cropped");
        check(contains(badges[active_badge], *dst), "badge text outside border");
        active_badge = -1;
    }
    if (dst && active_card >= 0) {''')
    source = replace_once(source,
        '    return __real_sdl_ui_text_texture(font, text, color, w, h);', r'''
    SDL_strlcpy(last_plain_text, text, sizeof(last_plain_text));
    last_plain_texture = __real_sdl_ui_text_texture(font, text, color, w, h);
    return last_plain_texture;''')
    source = replace_once(source,
        '    *r = (SDL_FRect){fixture_screen.x, fixture_screen.y, 60, 28}; return true;', r'''
    if (fixture_screen.w >= 2200 && fixture_screen.h < fixture_screen.w)
        *r = (SDL_FRect){fixture_screen.x + fixture_screen.w * 0.5f - 57,
            fixture_screen.y, 114, 126};
    else
        *r = (SDL_FRect){fixture_screen.x, fixture_screen.y, 60, 28};
    return true;''')
    source = replace_once(source,
        '    fixture_mobile = mobile; fixture_font = font;', r'''
    fixture_mobile = mobile; fixture_font = font;
    fixture_density = 2.625f; g_state.system_scale = fixture_density;
    config.bigger_font = fixture_big;''')
    source = replace_once(source,
        '"%dx%d-mobile%d-font%d-inset%d-profile%d-page%d-part%d.png",\n'
        '            width, height, mobile, font, inset, profile, page + 1, tutorial_panel_part + 1);',
        '"%dx%d-mobile%d-font%d-inset%d-profile%d-big%d-page%d-part%d.png",\n'
        '            width, height, mobile, font, inset, profile, fixture_big, page + 1, tutorial_panel_part + 1);')
    source = replace_once(source,
        "        fixture_page = page;\n        card_count = 0; active_card = -1; drawn_words[0] = '\\0';", r'''
        fixture_page = page;
        card_count = 0; active_card = -1; drawn_words[0] = '\0';
        badge_count = 0; active_badge = -1; last_plain_text[0] = '\0';''')
    source = replace_once(source,
        '        if (mobile && width < height && page == 1) {', r'''
        if (page < 2) {
            check(active_badge < 0, "badge text missing");
            for (int i = 0; i < badge_count; i++) {
                check(contains(screen, badges[i]), "badge outside safe viewport");
                check(!SDL_HasRectIntersectionFloat(&badges[i], &footer),
                    "badge overlaps footer");
                for (int j = 0; j < card_count; j++)
                    check(!SDL_HasRectIntersectionFloat(&badges[i], &cards[j]),
                        "badge overlaps heading or legend");
                for (int j = 0; j < i; j++)
                    check(!SDL_HasRectIntersectionFloat(&badges[i], &badges[j]),
                        "badges overlap one another");
            }
            const char *required0[] = { "Menu", "Depth", "Map / Player" };
            const char *required1[] = { "Combat", "Quick", "Status", "Rolls",
                "Inventory", "Equipment", "Messages" };
            const char **required = page == 0 ? required0 : required1;
            int required_count = page == 0 ? (int)N_ELEMENTS(required0)
                : (int)N_ELEMENTS(required1);
            for (int i = 0; i < required_count; i++) {
                bool found = false;
                for (int j = 0; j < badge_count; j++)
                    found |= strcmp(required[i], badge_names[j]) == 0;
                if (!found) fprintf(stderr, "missing badge: %s\n", required[i]);
                check(found, "complete badge missing");
            }
            if (page == 0) {
                SDL_FRect menu;
                __wrap_sdl_main_menu_pane_current_rect(&menu);
                for (int i = 0; i < badge_count; i++)
                    if (!strcmp(badge_names[i], "Menu"))
                        check(SDL_HasRectIntersectionFloat(&badges[i], &menu),
                            "Menu badge detached from its target");
            }
        }
        if (mobile && width < height && page == 1) {''')
    start = source.index('    for (int profile = 0; profile < SDL_TOUCH_PROFILE_COUNT; ++profile) {')
    end = source.index('    TTF_Quit(); SDL_Quit();', start)
    source = source[:start] + r'''
    fixture_big = false;
    paint(2400, 1080, true, 48, 0, SDL_TOUCH_PROFILE_ROUND_WHEEL);
    paint(2400, 1080, true, 48, 32, SDL_TOUCH_PROFILE_ROUND_WHEEL);
    paint(1080, 2400, true, 48, 32, SDL_TOUCH_PROFILE_ROUND_WHEEL);
    paint(800, 360, true, 24, 0, SDL_TOUCH_PROFILE_ROUND_WHEEL);
    fixture_big = true;
    paint(2400, 1080, true, 48, 0, SDL_TOUCH_PROFILE_ROUND_WHEEL);
    paint(1080, 2400, true, 48, 32, SDL_TOUCH_PROFILE_ROUND_WHEEL);
''' + source[end:]
    return source


def main():
    fixture.OUT.mkdir(parents=True, exist_ok=True)
    check_startup_copy()
    source = instrument(fixture.HARNESS)
    if "--baseline" in sys.argv:
        original = subprocess.run(
            ["git", "show", "HEAD:src/sdl/input/sdl-touch-tutorial.c"],
            cwd=ROOT, capture_output=True, text=True, check=True).stdout
        baseline = fixture.OUT / "baseline.c"
        baseline.write_text(original, encoding="utf-8")
        source = source.replace('#include "sdl/input/sdl-touch-tutorial.c"',
                                f'#include "{baseline.as_posix()}"')
    fixture.HARNESS = source
    fixture.main()


if __name__ == "__main__":
    main()
