#!/usr/bin/env python3
"""Check independent font/orientation HUD profiles and render their defaults.

Uses production mobile layout/settings code with an offscreen SDL renderer.
Character/dungeon status content and OS orientation requests are fixtures; no
player data is loaded. Output lives in scripts/output/bigger-font-profiles.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bigger-font-profiles"
HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>

static int width, height;
static float density;
static bool large_values;
void sdl_ios_request_orientation(bool portrait) {}
void sdl_ios_install_orientation_observer(SDL_Window *window) {}
bool sdl_ios_get_safe_area_insets(SDL_Window *window,
    int *left, int *right, int *top, int *bottom) { return false; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
bool __wrap_sdl_layout_matches_supporting_pane_visibility(void) { return true; }
void __wrap_sdl_depth_menu_pane_label(char *out, size_t size) {
    SDL_strlcpy(out,"50 ft",size);
}
int __wrap_player_current_movement_energy(void) { return 100; }
int __wrap_player_current_movement_speed(void) { return 2; }
void __wrap_prt_frame_basic(void) {
    Term_putstr(0, ROW_HP, -1, TERM_WHITE,
        large_values ? "Hth  999/999" : "Health 41/41");
    Term_putstr(0, ROW_SP, -1, TERM_L_GREEN,
        large_values ? "Vce  999:999" : "Voice    7:7");
    Term_putstr(0, ROW_LIGHT, -1, TERM_YELLOW, "oo    2999");
    Term_putstr(0, ROW_MEL, -1, TERM_WHITE, "(+7,2d6)");
    Term_putstr(0, ROW_ARC, -1, TERM_WHITE, "(+3,1d7)");
}

static struct pane_config *find(struct sdl_pane_profile *p, enum pane_type type) {
    for(int i=0;i<p->pane_count;i++) if(p->pane_configs[i].pane==type)
        return &p->pane_configs[i];
    assert(0); return NULL;
}
static void defaults(void) {
    sdl_config_set_defaults(&config);
    config.main_view_scale=2;
    sdl_copy_pane_configs(pane_config,&pane_config_count,
        default_pane_config,default_pane_config_count);
    sdl_seed_all_pane_profiles_from_active();
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
}
static void check_default(struct sdl_pane_profile *p) {
    assert(p->left_panel_compact_mode==SDL_LEFT_PANEL_COMPACT_ROW);
    assert(!p->left_panel_expanded_on_launch && !p->show_main_menu_button);
    assert(p->touch_top_panel_cell_count==6 && p->touch_top_panel_rows==1);
    assert(p->touch_top_panel_bindings[5]=='m');
    assert(!p->touch_top_panel_arrows_visible && p->touch_top_panel_default_open);
    struct pane_config *left=find(p,PANE_LEFT_PANEL),*log=find(p,PANE_ROLLS);
    assert(left->enabled && left->where==PLACE_TOP_CENTER);
    assert(log->enabled && log->where==PLACE_TOP_CENTER && log->rect.rows==4);
    assert(left<log);
    assert(find(p,PANE_STATUS_DEPTH)->enabled);
    assert(find(p,PANE_STATUS_DEPTH)<find(p,PANE_OVERLAY_MENU));
    assert(!find(p,PANE_LOG)->enabled);
}
static void check_persistence(void) {
    defaults();
    for(int i=SDL_PANE_ORIENTATION_PROFILE_COUNT;i<SDL_PANE_PROFILE_COUNT;i++)
        check_default(&g_pane_profiles[i]);
    /* Each terminal size contains four independent font/orientation layouts. */
    for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
        set_sdl_min_terminal_mode(mode);
        for(int big=0;big<2;big++) {
            set_sdl_bigger_font(big);
            for(int portrait=0;portrait<2;portrait++) {
                set_sdl_mobile_portrait_mode(portrait);
                int index=SDL_PANE_FONT_PROFILE_INDEX(big,portrait,mode);
                config.touch_top_panel_cell_count=7+index;
                config.touch_top_panel_bindings[0]='a'+index;
                config.touch_top_panel_long_bindings[0]='A'+index;
                config.left_panel_compact_mode=index%2;
                pane_config[0].font_size=11+index;
                g_main_view_zoom_scale=7;
                sdl_store_active_pane_profile(mode);
                set_sdl_bigger_font(big); /* no-op must not discard edits */
                assert(config.touch_top_panel_cell_count==7+index);
                assert(g_main_view_zoom_scale==7);
            }
        }
    }
    assert(sdl_config_save("profiles.json",&config,g_pane_profiles,SDL_PANE_PROFILE_COUNT));
    memset(g_pane_profiles,0,sizeof(g_pane_profiles));
    assert(sdl_config_load("profiles.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
        set_sdl_min_terminal_mode(mode);
        for(int big=0;big<2;big++) {
            set_sdl_bigger_font(big);
            for(int portrait=0;portrait<2;portrait++) {
                set_sdl_mobile_portrait_mode(portrait);
                int index=SDL_PANE_FONT_PROFILE_INDEX(big,portrait,mode);
                assert(config.touch_top_panel_cell_count==7+index);
                assert(config.touch_top_panel_bindings[0]=='a'+index);
                assert(config.touch_top_panel_long_bindings[0]=='A'+index);
                assert(config.left_panel_compact_mode==index%2);
                assert(pane_config[0].font_size==11+index);
            }
        }
    }
    FILE *old=fopen("legacy.json","wb"); assert(old);
    fputs("{\"sdl\":{\"biggerFont\":true,\"leftPanelExpandedOnLaunch\":false},"
        "\"paneProfiles\":{\"landscape\":{\"NORMAL\":{\"touchTopPanelCellCount\":11,"
        "\"panes\":[{\"type\":\"LEFT_PANEL\",\"where\":\"TOP_RIGHT\",\"enabled\":true}]}},"
        "\"portrait\":{\"NORMAL\":{\"touchTopPanelCellCount\":10}}}}",old);
    fclose(old);
    defaults();
    assert(sdl_config_load("legacy.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    assert(g_pane_profiles[0].touch_top_panel_cell_count==11);
    assert(find(&g_pane_profiles[0],PANE_LEFT_PANEL)->where==PLACE_TOP_RIGHT);
    assert(g_pane_profiles[2].touch_top_panel_cell_count==10);
    sdl_normalize_unified_log_pane_profiles(true);
    sdl_ensure_default_pane_profiles_present(false);
    for(int i=SDL_PANE_ORIENTATION_PROFILE_COUNT;i<SDL_PANE_PROFILE_COUNT;i++)
        check_default(&g_pane_profiles[i]);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
    assert(config.touch_top_panel_cell_count==6);
    set_sdl_bigger_font(false); assert(config.touch_top_panel_cell_count==11);
    FILE *partial=fopen("partial.json","wb"); assert(partial);
    fputs("{\"paneProfiles\":{\"biggerFont\":{\"portrait\":{\"NORMAL\":{"
        "\"touchTopPanelCellCount\":13}}}}}",partial); fclose(partial);
    defaults();
    assert(sdl_config_load("partial.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,1,0)].touch_top_panel_cell_count==13);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,1,1)].touch_top_panel_cell_count==13);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,0,0)].touch_top_panel_cell_count==6);
    puts("Profiles: eight independent layouts, live toggle/rotation/terminal size, restart, legacy preservation PASS");
}

static void inside(SDL_FRect r) {
    assert(r.w>0 && r.h>0 && r.x>=-1 && r.y>=-1);
    assert(r.x+r.w<=width+1 && r.y+r.h<=height+1);
}
static void check_layout(char **argv) {
    defaults(); set_sdl_bigger_font(true);
    set_sdl_mobile_portrait_mode(height>width);
    config.use_unsafe_area=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.system_scale=density;
    g_state.window=SDL_CreateWindow("Big-font HUD fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); assert(g_state.renderer);
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    sdl_view *view=&g_views[PANE_MAIN];
    assert(term_init(&view->t,80,24,16)==0); view->t.data=(void *)(uintptr_t)PANE_MAIN;
    Term_activate(&view->t); term_screen=&view->t;
    view->term_ready=true; view->rect=(SDL_Rect){0,0,width,height};
    view->cell_w=8; view->cell_h=16; view->cols=width/8; view->rows=height/16;
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height); assert(view->canvas);
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){90,235,140,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){245,210,70,255};
    for(int values=0;values<3;values++) {
        large_values=values!=0;
        int custom_font=width>height && height/density<500?18:32;
        for(int i=0;i<pane_config_count;i++) if(pane_config[i].pane==PANE_LEFT_PANEL)
            pane_config[i].font_size=values==2?custom_font:0;
        sdl_left_panel_source_invalidate();
        g_sdl_present_generation++;
        SDL_Rect screen={0,0,width,height};
        sdl_place_active_panes(&screen,g_pane_rects,false,false,true);
        sdl_left_panel_metrics metrics;
        assert(sdl_left_panel_metrics_for_view(view,&metrics));
        SDL_FRect panel;
        assert(sdl_left_panel_pane_rect_for_metrics(view,&metrics,&panel)); inside(panel);
        assert(metrics.compact_row && metrics.compact_segment_count==3);
        assert(metrics.cell_h==sdl_effective_pane_cell_height_for_type(PANE_LEFT_PANEL));
        assert(values==2 ? metrics.panel_rows>=2 : metrics.panel_rows==2);
        if(values==2 && height>width) assert(metrics.panel_rows>2);
        g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){panel.x,panel.y,panel.w,panel.h};
        sdl_view *log=&g_views[PANE_ROLLS];
        assert(sdl_view_create(log,g_pane_rects[PANE_ROLLS],config.monospace_font,
            sdl_effective_pane_font_size_for_type(PANE_ROLLS),0,0));
        assert(log->rows==4);
        assert(term_init(&log->t,log->cols,log->rows,16)==0);
        log->t.data=(void *)(uintptr_t)PANE_ROLLS; log->term_ready=true;
        Term_activate(&log->t);
        const char *message="A cold wind blows through the corridor. You hear footsteps in the distance.";
        int offsets[16],lengths[16];
        int lines=sdl_overlay_log_wrap(message,16,offsets,lengths); assert(lines>0);
        TTF_Font *font=sdl_story_font_for_height_slot(log->cell_h,SDL_STORY_FONT_SLOT_LOG);
        assert(font);
        for(int i=0;i<MIN(4,lines);i++)
            sdl_render_story_text_free_px(log,font,4,i,message+offsets[i],lengths[i],
                g_state.palette[TERM_WHITE],log->cols*log->cell_w-8);
        Term_activate(&view->t);
        SDL_SetRenderTarget(g_state.renderer,NULL);
        sdl_apply_top_right_overlay_offset();
        SDL_Rect band; assert(sdl_overlay_log_pane_current_rect(&band));
        assert(sdl_overlay_log_left_margin(log->cols)==0);
        config.bigger_font=false;
        assert(sdl_overlay_log_left_margin(log->cols)==pane_log_overlay_left_margin(log->cols));
        config.bigger_font=true;
        assert(band.w>=width-log->cell_w);
        assert(band.y>=panel.y+panel.h);
        assert(log->rows==4 && log->rect.h>=4*log->cell_h);
        SDL_FRect log_rect={band.x,band.y,band.w,band.h}; inside(log_rect);
        SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,210);
        SDL_RenderFillRect(g_state.renderer,&log_rect);
        SDL_FRect log_content={band.x,band.y,log->cols*log->cell_w,4*log->cell_h};
        SDL_RenderTexture(g_state.renderer,log->canvas,NULL,&log_content);
        assert(sdl_render_left_panel_pane_from_cells(view,&panel));
        for(int i=0;i<3;i++) {
            SDL_FRect cell;
            assert(sdl_left_panel_source_cell_rect(0,metrics.compact_source_rows[i],
                LEFT_PANEL_CONTENT_WID,1,&cell));
            assert(cell.h==2*metrics.cell_h);
            for(int line=0;line<2;line++) {
                int col,row;
                assert(sdl_main_view_point_to_cell(cell.x+2,
                    cell.y+(line+.5f)*metrics.cell_h,&col,&row));
                assert(g_last_main_cell_hit_left_panel && row==metrics.compact_source_rows[i]);
            }
        }
        SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT],quick;
        SDL_Rect anchor;
        enum pane_placement quick_where;
        assert(sdl_touch_top_panel_current_anchor(&screen,&anchor,&quick_where));
        assert(sdl_touch_top_panel_compute_layout(buttons,&quick)); inside(quick);
        for(int i=0;i<6;i++) {
            inside(buttons[i]);
            assert(buttons[i].w>=sdl_ui_min_tap_px() && buttons[i].h>=sdl_ui_min_tap_px());
            for(int j=0;j<i;j++) assert(!SDL_HasRectIntersectionFloat(&buttons[i],&buttons[j]));
        }
        sdl_touch_top_panel_render_buttons(buttons);
        sdl_status_depth_pane_render();
        sdl_combat_overlay_pane_render();
        status_depth_pane_layout status;
        assert(sdl_status_depth_pane_layout(&status)); inside(status.panel);
        SDL_FRect contact;
        /* Integer anchors can touch the fractional final button edge. */
        assert(!SDL_GetRectIntersectionFloat(&quick,&status.panel,&contact)
            || contact.h<=1 || contact.w<=1);
        assert(!SDL_GetRectIntersectionFloat(&quick,&log_rect,&contact)
            || contact.h<=1 || contact.w<=1);
        SDL_Rect combat;
        assert(sdl_combat_overlay_pane_current_rect(&combat));
        SDL_FRect combat_rect={combat.x,combat.y,combat.w,combat.h};
        inside(combat_rect);
        assert(!SDL_GetRectIntersectionFloat(&status.panel,&combat_rect,&contact)
            || contact.h<=1 || contact.w<=1);
        char path[160]; strnfmt(path,sizeof(path),"hud-%dx%d-%.3f-%d.png",width,height,density,values);
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL); assert(pixels);
        assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
        SDL_DestroyTexture(log->canvas); log->canvas=NULL;
        term_nuke(&log->t); log->term_ready=false;
    }
    printf("HUD %dx%d @%.3f: readable two-line character row, label/value taps, full-width four-row log, six reachable cells PASS\n",width,height,density);
}
int main(int argc,char **argv) {
    assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"); assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    assert(TTF_Init());
    static maxima limits; static player_type player; static player_other options;
    static object_type items[INVEN_TOTAL];
    static byte features[32][MAX_DUNGEON_WID];
    cave_feat=features;
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    player.playing=true; player.song1=player.song2=SNG_NOTHING;
    player.chp=player.mhp=41; player.csp=player.msp=7;
    player.cur_map_hgt=32; player.cur_map_wid=64;
    items[INVEN_LITE].k_idx=1; character_generated=character_dungeon=true;
    check_persistence(); check_layout(argv); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    mobile_sources = ["src/sdl/core/sdl-state.c", "src/sdl/core/sdl-layout.c",
                      "src/sdl/config/sdl-settings.c", "src/sdl/render/sdl-fonts.c",
                      "src/sdl-config.c", "src/sdl/input/sdl-touch-controls.c",
                      "src/sdl/ui/sdl-panes.c"]
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = tuple("/" + p + ".obj" for p in mobile_sources) + ("/src/main.c.obj",)
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / d) for d in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ["SDL_GetDisplayContentScale", "sdl_touch_only_device_active",
             "sdl_layout_matches_supporting_pane_visibility", "prt_frame_basic",
             "sdl_depth_menu_pane_label", "player_current_movement_energy",
             "player_current_movement_speed"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source)]
                   + [str(ROOT / p) for p in mobile_sources]
                   + ["@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                      "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    for w, h, scale in [(360,800,1), (800,360,1), (720,1600,2), (1600,720,2),
                         (1080,2400,2.75), (2400,1080,2.75), (1280,720,1)]:
        subprocess.run([str(exe), str(w), str(h), str(scale),
                        str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
