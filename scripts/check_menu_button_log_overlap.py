#!/usr/bin/env python3
"""Check independent menu-font geometry against the live overlay log.

Compiles production mobile layout and menu code into an offscreen SDL fixture;
no player configuration or save is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess

import check_bigger_font_profiles as base

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/menu-button-log-overlap"
EXTRA_MOBILE_SOURCES = []

CHECK = r'''
static void check_menu_and_log(void)
{
    config.use_unsafe_area=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    config.show_main_menu_button=true;
    config.aux_view_font_size=0;
    config.main_view_scale=3;
    g_main_view_zoom_scale=5;
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    g_direct_touch_present=true;
    g_state.system_scale=density;
    character_generated=character_dungeon=true;
    character_icky=0; inkey_flag=true; p_ptr->playing=true;

    sdl_view *main=&g_views[PANE_MAIN], *log=&g_views[PANE_ROLLS];
    assert(term_init(&main->t,80,24,32)==0);
    main->t.data=(void *)(uintptr_t)PANE_MAIN;
    main->term_ready=true; main->rect=(SDL_Rect){0,0,width,height};
    main->cell_w=40; main->cell_h=80;
    main->cols=width/main->cell_w; main->rows=height/main->cell_h;
    main->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height); assert(main->canvas);
    Term_activate(&main->t); term_screen=&main->t;

    pane_config_count=1;
    pane_config[0]=(struct pane_config){.pane=PANE_ROLLS,
        .where=PLACE_TOP_RIGHT,.enabled=true,.rect.rows=8};
    log->rect=(SDL_Rect){0,0,width,250};
    log->cell_w=20; log->cell_h=30; log->cols=width/20; log->rows=8;
    log->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,250); assert(log->canvas);
    g_pane_rects[PANE_ROLLS]=log->rect;

    for(int ui_big=0;ui_big<2;ui_big++) for(int menu_big=0;menu_big<2;menu_big++) {
        config.bigger_font=ui_big;
        config.menu_bigger_font[SDL_MENU_FONT_MAIN]=menu_big;
        config.menu_bigger_font[SDL_MENU_FONT_INVENTORY]=!menu_big;
        g_main_menu_overlay_active=false;
        sdl_reset_top_right_overlay_offset();
        log->rect=(SDL_Rect){0,0,width,250}; g_pane_rects[PANE_ROLLS]=log->rect;

        /* Call from the inventory context to prove the function restores its
         * caller and measures the same box used by the Main Menu renderer. */
        int saved=sdl_menu_font_set_render_context(SDL_MENU_FONT_INVENTORY);
        SDL_FRect measured,painted,hit;
        assert(sdl_main_menu_pane_button_rect(&measured));
        assert(sdl_menu_font_current()==SDL_MENU_FONT_INVENTORY);
        sdl_menu_font_set_render_context(SDL_MENU_FONT_MAIN);
        assert(sdl_main_menu_pane_button_rect(&painted));
        assert(sdl_menu_font_current()==SDL_MENU_FONT_MAIN);
        sdl_menu_font_set_render_context(SDL_MENU_FONT_INVENTORY);
        assert(SDL_fabsf(measured.x-painted.x)<.01f);
        assert(SDL_fabsf(measured.y-painted.y)<.01f);
        assert(SDL_fabsf(measured.w-painted.w)<.01f);
        assert(SDL_fabsf(measured.h-painted.h)<.01f);
        assert(sdl_main_menu_pane_hit(painted.x+painted.w/2,
            painted.y+painted.h/2,&hit));
        assert(SDL_fabsf(hit.x-painted.x)<.01f && SDL_fabsf(hit.w-painted.w)<.01f);
        sdl_menu_font_set_render_context(saved);

        SDL_Rect first={0};
        for(int frame=0;frame<12;frame++) {
            sdl_apply_top_right_overlay_offset();
            SDL_Rect band; assert(sdl_overlay_log_pane_current_rect(&band));
            SDL_FRect log_band={band.x,band.y,band.w,band.h};
            SDL_FRect contact={0};
            bool overlap=SDL_GetRectIntersectionFloat(&painted,&log_band,&contact)
                && contact.w>1.0f && contact.h>1.0f;
            if(overlap)
                fprintf(stderr,"ui=%d menu=%d frame=%d button=(%.1f,%.1f %.1fx%.1f) log=(%d,%d %dx%d)\n",
                    ui_big,menu_big,frame,painted.x,painted.y,painted.w,painted.h,
                    band.x,band.y,band.w,band.h);
            assert(!overlap);
            if(frame) assert(!memcmp(&first,&band,sizeof(band)));
            else first=band;
        }
    }
    printf("%dx%d @%.3f: independent gameplay/menu fonts, drawn and hit bounds, log avoidance and stable reflow PASS\n",
        width,height,density);
    SDL_DestroyTexture(log->canvas); log->canvas=NULL;
    SDL_DestroyTexture(main->canvas); main->canvas=NULL;
    term_nuke(&main->t); term_screen=NULL; Term=NULL;
}

int main(int argc,char **argv)
{
    assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); assert(TTF_Init());
    static maxima limits; static player_type player; static player_other options;
    static object_type items[INVEN_TOTAL];
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    player.song1=player.song2=SNG_NOTHING;
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Menu overlap fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window); g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer); g_state.safe_area=(SDL_Rect){0,0,width,height};
    check_menu_and_log();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(base.HARNESS[:base.HARNESS.index("static struct pane_config *find")]
                      + CHECK, encoding="utf-8")
    mobile_sources = ["src/sdl/core/sdl-state.c", "src/sdl/core/sdl-layout.c",
        "src/sdl/config/sdl-settings.c", "src/sdl/render/sdl-fonts.c",
        "src/sdl-config.c", "src/sdl/input/sdl-touch-controls.c",
        "src/sdl/ui/sdl-panes.c", "src/sdl/ui/sdl-main-menu.c"] + EXTRA_MOBILE_SOURCES
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = tuple("/" + p + ".obj" for p in mobile_sources) + ("/src/main.c.obj",)
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / d) for d in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["SDL_GetDisplayContentScale", "sdl_touch_only_device_active",
        "sdl_layout_matches_supporting_pane_visibility", "prt_frame_basic",
        "sdl_depth_menu_pane_label", "player_current_movement_energy",
        "player_current_movement_speed"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS",
        "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source)]
        + [str(ROOT / p) for p in mobile_sources]
        + ["@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
           "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    for w, h, scale in [(2268, 1080, 2.625), (1920, 1200, 2), (1280, 720, 1)]:
        subprocess.run([str(exe), str(w), str(h), str(scale)]
            + [str(ROOT / "lib/xtra/font" / f) for f in
               ("VictorMono-Medium.ttf", "Cinzel-Medium.ttf", "EBGaramond-Regular.ttf")],
            cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
