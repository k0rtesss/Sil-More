#!/usr/bin/env python3
"""Exercise native settings, birth, stories, character pages and menus at phone density."""
from pathlib import Path
import os
import shlex
import subprocess
from check_bigger_font_layout import HARNESS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/phone-screens-check"

HARNESS = BASE.replace(
    '#include "sdl/ui/sdl-screens.c"',
    'static float fixture_density;\n'
    'float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id) { return fixture_density; }\n'
    '#include "sdl/ui/sdl-screens.c"', 1
).replace('fixture_assert(argc==6)', 'fixture_assert(argc==7)').replace(
    'g_state.system_scale=1;', 'fixture_density=atof(argv[6]); g_state.system_scale=fixture_density;'
).replace('    check_actual_cell_metrics();', '').replace('    check_auto_font_metrics();', '').replace(
    'check_songs(); check_questions();', ''
).replace('check_halls();', '').replace('check_large_tutorial();', '').replace(
    '    fixture_assert(px>0);',
    '    fixture_assert(px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));\n'
    '    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {\n'
    '        const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];\n'
    '        if(hit->choice>=1001 || hit->choice==-1 || hit->choice==-2) {\n'
    '            inside(hit->rect);\n'
    '            fixture_assert(hit->rect.w+1>=sdl_ui_min_tap_px());\n'
    '            SDL_FRect list=g_sdl_character_sheet_screen.select_scroll_rect;\n'
    '            if(hit->choice<0 || (hit->rect.y>list.y+2.f && '
    'hit->rect.y+hit->rect.h<list.y+list.h-2.f))\n'
    '                { if(hit->rect.h+1<sdl_ui_min_tap_px()) fprintf(stderr,"min target choice%d rect %f,%f,%f,%f list %f,%f,%f,%f\\n", hit->choice,hit->rect.x,hit->rect.y,hit->rect.w,hit->rect.h,list.x,list.y,list.w,list.h); fixture_assert(hit->rect.h+1>=sdl_ui_min_tap_px()); }\n'
    '        }\n'
    '    }'
)


FOOTER = r'''
static void check_terminal_footer(void)
{
    sdl_view saved = g_views[PANE_MAIN];
    bool saved_big = config.bigger_font;
    SDL_strlcpy(fixture_id,"terminal-footer",sizeof(fixture_id));
    for(int big=0;big<2;big++) {
        config.bigger_font=big;
        sdl_view *view=&g_views[PANE_MAIN];
        view->rect=(SDL_Rect){0,0,fixture_width,fixture_height};
        view->cell_h=sdl_ui_role_font_px(SDL_UI_FONT_BODY);
        view->cell_w=view->cell_h/2;
        view->cols=fixture_width/view->cell_w;
        view->rows=fixture_height/view->cell_h;
        view->margin_x=view->margin_y=0; view->term_ready=true;
        ui_menu_click_begin();
        ui_menu_click_add_touch_button(501,"Preview",TERM_DARK);
        ui_menu_click_add_touch_button(502,"Drop On",TERM_YELLOW);
        ui_menu_click_add_touch_button(503,"Delete",TERM_DARK);
        ui_menu_click_set_touch_exit_button(true);
        sdl_touch_menu_button_layout_entry buttons[SDL_TOUCH_MENU_BUTTON_MAX];
        int count=sdl_touch_menu_button_layout(buttons,SDL_TOUCH_MENU_BUTTON_MAX);
        fixture_assert(count==4);
        int px=sdl_ui_role_font_px(SDL_UI_FONT_CONTROL);
        TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_MENU);
        int reserved=sdl_touch_menu_button_reserved_rows();
        float bottom=view->rows*view->cell_h;
        for(int i=0;i<count;i++) {
            inside(buttons[i].rect);
            fixture_assert(buttons[i].rect.w+1>=sdl_ui_min_tap_px());
            fixture_assert(buttons[i].rect.h+1>=sdl_ui_min_tap_px());
            int w=0,h=0;
            fixture_assert(TTF_GetStringSizeWrapped(font,buttons[i].label,0,
                (int)buttons[i].rect.w,&w,&h));
            fixture_assert(w<=buttons[i].rect.w+1 && h<=buttons[i].rect.h+1);
            fixture_assert(bottom-buttons[i].rect.y<=reserved*view->cell_h+1);
            if(i) fixture_assert(buttons[i-1].rect.x+buttons[i-1].rect.w<=buttons[i].rect.x+1);
        }
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
        sdl_touch_exit_button_render(); capture("terminal-footer");
        for(int i=0;i<count;i++) {
            ui_menu_click_clear_pending_hover();
            fixture_assert(sdl_touch_exit_button_handle_pointer(buttons[i].rect.x+buttons[i].rect.w/2,
                buttons[i].rect.y+buttons[i].rect.h/2));
            if(i<3) {
                int choice=0,action=0;
                fixture_assert(ui_menu_click_take_action(&choice,&action));
                fixture_assert(choice==501+i && action==UI_MENU_CLICK_PRIMARY);
            }
            char key=0; fixture_assert(Term_inkey(&key,false,true)==0);
            fixture_assert(key==(i==3?ESCAPE:UI_MENU_CLICK_WAKE_KEY));
        }
        ui_menu_click_clear();
    }
    g_views[PANE_MAIN]=saved; config.bigger_font=saved_big;
}
'''
HARNESS = HARNESS.replace("static void check(int width,int height)",FOOTER+"\nstatic void check(int width,int height)")
HARNESS = HARNESS.replace("    check_welcome_cache();", "    check_welcome_cache(); check_terminal_footer();")
# The imported fixture calls these helpers on one line.
HARNESS = HARNESS.replace("check_welcome_cache();\n    printf", "check_welcome_cache(); check_terminal_footer();\n    printf")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/ui/sdl-song-menu.c.obj",
                "/src/sdl/ui/sdl-question-menu.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj",
                "/src/sdl/ui/sdl-halls-screen.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_get_layout_screen_rect",
             "sdl_character_sheet_screen_commit_select", "SDL_GetDisplayContentScale"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, density in [(720, 1600, 2), (1080, 2340, 2.75), (1080, 2400, 2.625)]:
        for w, h in [(width, height), (height, width)]:
            subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                            str(w), str(h), str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                            str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"), str(density)],
                           cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
