#!/usr/bin/env python3
"""Render the real mobile settings rows and reject clipping or overlapping hits.

The test uses the production SDL renderer and isolated fixture state.  It does
not open an Android emulator, game save, or the user's settings.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess

from check_phone_screens_layout import HARNESS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/mobile-settings-layout"

TRACE = r'''
static bool fixture_inside_settings_row;
static bool fixture_inside_selected_row;
static int fixture_recorded_resets;
static char fixture_recorded_text[8192];
static void fixture_record_settings_text(cptr text, int tw, int th,
    const SDL_FRect *src, const SDL_FRect *dst)
{
    if (!fixture_inside_settings_row) return;
    if (src->w + 1 < tw || src->h + 1 < th) {
        fprintf(stderr,"Cropped settings text '%s': %dx%d -> %.1fx%.1f\n",
            text,tw,th,src->w,src->h);
        fixture_assert(false);
    }
    if (SDL_RenderClipEnabled(g_state.renderer)) {
        SDL_Rect clip; SDL_GetRenderClipRect(g_state.renderer,&clip);
        if (dst->x < clip.x-1 || dst->y < clip.y-1
            || dst->x+dst->w > clip.x+clip.w+1
            || dst->y+dst->h > clip.y+clip.h+1) {
            fprintf(stderr,"Settings text crosses scroll viewport: '%s'\n",text);
            fixture_assert(false);
        }
    }
    if (fixture_inside_selected_row) {
        if (!strcmp(text,"Reset")) fixture_recorded_resets++;
        else SDL_strlcat(fixture_recorded_text,text,sizeof(fixture_recorded_text));
    }
}
'''

CHECK = r'''
static void fixture_normalize(cptr src, char *dst, size_t size)
{
    size_t j=0;
    for(size_t i=0;src[i] && j+1<size;i++)
        if(isalnum((unsigned char)src[i]))
            dst[j++]=(char)tolower((unsigned char)src[i]);
    dst[j]='\0';
}
static void check_mobile_settings_no_crop(void)
{
    static cptr rows[]={
        "Minimum Terminal Size\tcompact (50x16)", "Map Zoom\t4",
        "Starting Zoom Offset\t+1", "Camera Vertical Recenter Distance\t3",
        "Camera Horizontal Recenter Distance\t3", "Orientation\tportrait",
        "Portrait Button Wheel Position\tside lane", "Tiles [Alt+A]\tyes",
        "Use Unsafe Area (notch/cutout area)\tno"};
    float normal_single_line_row_h=0.0f;
    for(int big=0;big<2;big++) {
        fixture_set_all_fonts(big);
        set_sdl_show_menu_font_button(true);
        ui_menu_click_begin();
        sdl_character_sheet_screen_begin_select(1,"Display and Layout");
        sdl_character_sheet_screen_set_font_menu(SDL_MENU_FONT_SETTINGS);
        sdl_character_sheet_screen_set_select_menu_style(true);
        for(int i=0;i<(int)N_ELEMENTS(rows);i++) {
            sdl_character_sheet_screen_add_select_row(i+1,rows[i],TERM_WHITE,"");
            sdl_character_sheet_screen_set_last_select_row_reset(1001+i);
        }
        sdl_character_sheet_screen_set_select_description(
            "Choose a setting and change it or reset its value.");
        for(int i=0;i<(int)N_ELEMENTS(rows);i++) {
            char expected[256], actual[8192];
            fixture_recorded_text[0]='\0'; fixture_recorded_resets=0;
            fixture_assert(sdl_character_sheet_screen_commit_select(i+1));
            fixture_assert(sdl_render_current_window_frame());
            fixture_normalize(rows[i],expected,sizeof(expected));
            fixture_normalize(fixture_recorded_text,actual,sizeof(actual));
            if(!strstr(actual,expected))
                fprintf(stderr,"Missing settings row '%s'; rendered '%s'\n",rows[i],fixture_recorded_text);
            fixture_assert(strstr(actual,expected)!=NULL);
            fixture_assert(fixture_recorded_resets>0);
            const sdl_character_sheet_hit *row=NULL,*reset=NULL;
            for(int j=0;j<g_sdl_character_sheet_screen.hit_count;j++) {
                const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[j];
                if(hit->choice==i+1) row=hit;
                if(hit->choice==1001+i) reset=hit;
            }
            fixture_assert(row && reset);
            fixture_assert(row->rect.w>0 && row->rect.h>0);
            fixture_assert(reset->rect.w>0 && reset->rect.h>0);
            if(!big && i==1) normal_single_line_row_h=row->rect.h;
            fixture_assert(!SDL_HasRectIntersectionFloat(&row->rect,&reset->rect));
            SDL_FRect font_button=sdl_menu_font_button_bounds();
            SDL_FRect font_hit=sdl_menu_control_hit_rect(font_button);
            fixture_assert(font_button.w>0 && font_button.h>0);
            fixture_assert(!SDL_HasRectIntersectionFloat(&row->rect,&font_hit));
            fixture_assert(!SDL_HasRectIntersectionFloat(&reset->rect,&font_hit));
        }
        capture(big?"settings-large":"settings-normal");
        sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    }
    if(fixture_width==2400 && fixture_height==1080)
        fixture_assert(normal_single_line_row_h<sdl_ui_min_tap_px());
    printf("Mobile settings %dx%d @%.3f: full labels, values and Reset text in both font modes; compact normal rows: PASS\n",
        fixture_width,fixture_height,fixture_density);
}
'''


def source() -> str:
    screens = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")
    start = screens.index("static SDL_FRect sdl_char_sheet_draw_text_aligned(")
    end = screens.index("static SDL_FRect sdl_char_sheet_draw_text_alpha(", start)
    draw = screens[start:end]
    call = "sdl_ui_render_text_texture(texture, &src, &dst);"
    assert draw.count(call) == 1
    draw = draw.replace(call,
        "fixture_record_settings_text(text,text_w,text_h,&src,&dst);\n    " + call)
    screens = screens[:start] + draw + screens[end:]
    start = screens.index("void sdl_char_sheet_draw_menu_row(")
    end = screens.index("static void sdl_char_sheet_dynamic_menu_headings(", start)
    row = screens[start:end]
    guard = "if (!font || !text || !text[0] || w <= 0.0f || line_h <= 0.0f)\n        return;"
    assert row.count(guard) == 1
    row = row.replace(guard, guard + "\n    fixture_inside_settings_row = true;\n"
        "    fixture_inside_selected_row = "
        "(choice == g_sdl_character_sheet_screen.selected_index);", 1)
    closing = row.rfind("\n}")
    assert closing >= 0
    row = (row[:closing] + "\n    fixture_inside_settings_row = false;\n"
        "    fixture_inside_selected_row = false;" + row[closing:])
    screens = screens[:start] + row + screens[end:]
    harness = BASE.replace('#include "sdl/ui/sdl-screens.c"', TRACE + screens, 1)
    harness = harness.replace("static void check(int width,int height)",
        CHECK + "\nstatic void check(int width,int height)", 1)
    marker = '    if(SDL_getenv("SIL_MAIN_MENU_TEST_ONLY")) {'
    assert marker in harness
    harness = harness.replace(marker,
        '    if(SDL_getenv("SIL_SETTINGS_TEST_ONLY")) {\n'
        '        check_mobile_settings_no_crop(); goto cleanup;\n    }\n' + marker, 1)
    return harness


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    harness = source()
    c_file = OUT / "check.c"
    c_file.write_text(harness, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
        "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/ui/sdl-song-menu.c.obj",
        "/src/sdl/ui/sdl-question-menu.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj",
        "/src/sdl/ui/sdl-halls-screen.c.obj", "/src/sdl/ui/sdl-menu-font.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env["SIL_SETTINGS_TEST_ONLY"] = "1"
    wraps = list(dict.fromkeys(re.findall(r"\b__wrap_(\w+)\s*\(", harness)))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(c_file), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width,height,density in [(1080,2400,2.625),(2400,1080,2.625),
                                  (1200,1920,2),(1920,1200,2),
                                  (1600,2560,2),(2560,1600,2)]:
        subprocess.run([str(exe),str(ROOT/"lib/xtra/font/EBGaramond-Regular.ttf"),
            str(width),str(height),str(ROOT/"lib/xtra/font/VictorMono-Medium.ttf"),
            str(ROOT/"lib/xtra/font/Cinzel-Medium.ttf"),str(density)],
            cwd=OUT,env=env,check=True,timeout=60)


if __name__ == "__main__":
    main()
