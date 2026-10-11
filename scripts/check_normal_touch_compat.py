#!/usr/bin/env python3
"""Compare normal touch controls with the actual pre-big-text implementation.

Reads the pinned parent of the first big-font commit into ignored output files;
never checks out or replaces workspace files. Both executables use the same
isolated player fixtures and current SDL libraries. Compare geometry, hit tests
and rendered pixel hashes, including fixed/Stretch bars and Quick Touch labels.
Requires a current standard build. No user configuration or save is opened.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

from check_bigger_font_profiles import HARNESS as PROFILE_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/normal-touch-compat"
BEFORE = "87e2675d"
MODULES = ["src/pane.c", "src/sdl/core/sdl-layout.c", "src/sdl/render/sdl-fonts.c",
           "src/sdl/render/sdl-present.c", "src/sdl/input/sdl-map-geometry.c",
           "src/sdl/ui/sdl-panes.c"]
INCLUDED = ["src/sdl/input/sdl-touch-controls.c", "src/sdl/ui/sdl-menus.c"]
PREFIX = PROFILE_HARNESS[:PROFILE_HARNESS.index("static struct pane_config *find(")]
HARNESS = PREFIX + r'''
#ifdef ORIGINAL
#include "old-sdl-touch-controls.c"
#include "old-sdl-menus.c"
int sdl_ui_font_px(int px) { return px; }
float sdl_ui_density_scale(void) { return density; }
int sdl_ui_role_font_px(enum sdl_ui_font_role role) {
    return (int)SDL_ceilf(density*(role==SDL_UI_FONT_TITLE?20:role==SDL_UI_FONT_META?14:16));
}
int sdl_ui_min_tap_px(void) { return (int)SDL_ceilf(density*48); }
int sdl_overlay_log_left_margin(int cols) { return pane_log_overlay_left_margin(cols); }
bool sdl_touch_top_panel_layout_excludes_pane(enum pane_type pane) { return false; }
#else
#include "sdl/input/sdl-touch-controls.c"
#include "sdl/ui/sdl-menus.c"
#endif

bool __wrap_player_active_weapon_is_ranged(void) { return false; }
bool __wrap_player_quick_throw_available(void) { return false; }
int __wrap_floor_context_collect_square_actions(bool description,
    floor_context_action *actions,int capacity) { return 0; }
bool __wrap_touch_shortcut_context_action(int binding,bool description,
    int *key,char *label,size_t size) {
    const char *text=binding==' '?"Confirm":binding=='x'?"Description":NULL;
    if(description && binding=='g') text="Pick Up (Arrows, 10)";
    if(!text) return false;
    if(key) *key=binding;
    if(label) SDL_strlcpy(label,text,size);
    return true;
}
static const char *kind;
static void rect(SDL_FRect r) { printf("[%.9g,%.9g,%.9g,%.9g]",r.x,r.y,r.w,r.h); }
static void clear(void) {
    SDL_SetRenderTarget(g_state.renderer,NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_BLEND);
}
static unsigned long long capture(const char *name) {
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL); assert(pixels);
    SDL_Surface *rgba=SDL_ConvertSurface(pixels,SDL_PIXELFORMAT_RGBA32); assert(rgba);
    unsigned long long hash=1469598103934665603ULL;
    for(int y=0;y<rgba->h;y++) {
        const unsigned char *row=(const unsigned char *)rgba->pixels+y*rgba->pitch;
        for(int x=0;x<rgba->w*4;x++) { hash^=row[x]; hash*=1099511628211ULL; }
    }
    char path[192]; strnfmt(path,sizeof(path),"%s-%dx%d-%s.png",kind,width,height,name);
    assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(rgba); SDL_DestroySurface(pixels);
    return hash;
}
static void grid(void) {
    SDL_Rect screen={0,0,width,height};
    const int counts[]={6,8,9,16}; const float sizes[]={0,1.5f,3,6};
    const enum pane_placement placements[]={PLACE_BOTTOM_CENTER,PLACE_BOTTOM_RIGHT,PLACE_TOP_CENTER};
    const int bindings[]={'j','i','y','h',TOUCH_BIND_TOGGLE_TILES,'S','l','M','m','o','c','a','b','u','@','p'};
    pane_config_count=0; g_direct_touch_present=false;
    config.mobile_portrait_mode=height>width;
    config.touch_top_panel_arrows_visible=false; g_touch_top_panel_open=true;
    for(int i=0;i<16;i++) {
        config.touch_top_panel_bindings[i]=bindings[i];
        config.touch_top_panel_long_bindings[i]=GAMEPAD_BIND_NONE;
    }
    for(int c=0;c<4;c++) for(int rows=1;rows<=2;rows++)
    for(int s=0;s<4;s++) for(int p=0;p<3;p++) {
        config.touch_top_panel_cell_count=counts[c]; config.touch_top_panel_rows=rows;
        config.touch_top_panel_size=sizes[s];
        SDL_FRect buttons[16]={{0}},panel={0};
        bool valid=sdl_touch_top_panel_compute_layout_for_anchor(&screen,&screen,
            placements[p],buttons,&panel);
        bool hits=valid;
        g_sdl_present_generation++;
        memcpy(g_touch_top_panel_cached_buttons,buttons,sizeof(buttons));
        g_touch_top_panel_cached_layout_valid=valid;
        g_touch_top_panel_cached_generation=g_sdl_present_generation;
        if(valid) for(int i=0;i<counts[c];i++) {
            int slot=-1;
            hits=hits && sdl_touch_top_panel_point_to_slot(buttons[i].x+buttons[i].w/2,
                buttons[i].y+buttons[i].h/2,&slot) && slot==i;
        }
        unsigned long long pixels=0;
        if(valid && counts[c]==9 && rows==1 && p==0 && (s==0 || s==2)) {
            clear(); sdl_touch_top_panel_render_buttons(buttons);
            pixels=capture(s==0?"quick-access-stretch":"quick-access-fixed");
        }
        printf("{\"kind\":\"grid\",\"id\":\"%d-%d-%d-%d\",\"valid\":%d,\"hits\":%d,\"reserve\":%d,\"panel\":",
            counts[c],rows,s,p,valid,hits,sdl_touch_top_panel_reserved_stack_height(&screen));
        rect(panel); printf(",\"buttons\":[");
        for(int i=0;i<counts[c];i++) { if(i)printf(","); rect(buttons[i]); }
        printf("],\"pixels\":\"%016llx\"}\n",pixels);
    }
}
static void hud(void) {
    const int scales[]={2,6};
    g_direct_touch_present=true; config.touch_round_movement_enabled=true;
    config.touch_profile=SDL_TOUCH_PROFILE_ROUND_WHEEL;
    config.touch_top_panel_cell_count=8; config.touch_top_panel_rows=1;
    config.touch_top_panel_size=SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
    config.touch_thumb_enabled=true;
    config.left_panel_expanded_on_launch=false; g_left_panel_pane_expanded=false;
    for(int scale=0;scale<2;scale++) for(int row=0;row<2;row++)
    for(int description=0;description<2;description++) {
        config.main_view_scale=scales[scale]; config.aux_view_font_size=0;
        config.left_panel_compact_mode=row;
        pane_config_count=6;
        pane_config[0]=(struct pane_config){.pane=PANE_LEFT_PANEL,.where=PLACE_TOP_LEFT,.enabled=true};
        pane_config[1]=(struct pane_config){.pane=PANE_COMBAT,.where=PLACE_BOTTOM_LEFT,.enabled=true,
            .rect.rows=2,.rect.cols=12};
        pane_config[2]=(struct pane_config){.pane=PANE_ROLLS,.where=PLACE_TOP_RIGHT,.enabled=true,.rect.rows=8};
        pane_config[3]=(struct pane_config){.pane=PANE_STATUS_DEPTH,.where=PLACE_BOTTOM_RIGHT,.enabled=true,
            .rect.rows=1,.rect.cols=24};
        pane_config[4]=(struct pane_config){.pane=PANE_OVERLAY_MENU,.where=PLACE_BOTTOM_CENTER,.enabled=true,
            .rect.rows=1,.rect.cols=4};
        pane_config[5]=(struct pane_config){.pane=PANE_DESCRIPTION,.where=PLACE_BOTTOM_CENTER,.enabled=true,
            .rect.rows=80,.rect.cols=160};
        sdl_view *view=&g_views[PANE_MAIN];
        if(view->canvas) SDL_DestroyTexture(view->canvas);
        assert(sdl_view_create(view,(SDL_Rect){0,0,width,height},config.monospace_font,0,2,0));
        SDL_SetRenderTarget(g_state.renderer,NULL);
        g_sdl_present_generation++; sdl_left_panel_source_invalidate();
        g_description_overlay.active=false; g_description_overlay.interactive=false;
        sdl_reset_top_right_overlay_offset();
        g_touch_pane_hidden_layout_active=sdl_touch_pane_hidden_mode_active();
        g_touch_pane_proto_layout_active=sdl_touch_pane_proto_mode_active();
        sdl_place_active_panes(&(SDL_Rect){0,0,width,height},g_pane_rects,false,false,true);
        SDL_Rect fitted[PANE_MAX];
        sdl_place_active_panes_fitting_main(&(SDL_Rect){0,0,width,height},fitted,
            false,false,true,NULL,NULL);
        sdl_left_panel_metrics metrics; SDL_FRect left={0};
        bool have_left=sdl_left_panel_metrics_for_view(view,&metrics)
            && sdl_left_panel_pane_rect_for_metrics(view,&metrics,&left);
        if(have_left) g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){left.x,left.y,left.w,left.h};
        sdl_apply_top_right_overlay_offset();
        g_description_overlay.active=description; g_description_overlay.interactive=description;
        g_description_overlay.footer_action_count=4;
        const int actions[]={'g','d','x','i'};
        for(int i=0;i<4;i++) g_description_overlay.footer_actions[i].key=actions[i];
        SDL_FRect thumb[SDL_TOUCH_THUMB_RUNTIME_CAPACITY]={{0}};
        bool valid=sdl_touch_thumb_compute_runtime_rects(thumb,SDL_TOUCH_THUMB_RUNTIME_CAPACITY);
        int count=sdl_touch_thumb_button_count(); bool hits=valid;
        if(valid) for(int i=0;i<count;i++) {
            int index=-1;
            hits=hits && sdl_touch_thumb_point_to_button(thumb[i].x+thumb[i].w/2,
                thumb[i].y+thumb[i].h/2,&index) && index==i;
        }
        clear();
        if(have_left) assert(sdl_render_left_panel_pane_from_cells(view,&left));
        if(valid) for(int i=0;i<count;i++) sdl_touch_thumb_render_button(&thumb[i],i,false,false);
        char name[80]; strnfmt(name,sizeof(name),"quick-touch-%d-%d-%d",scales[scale],row,description);
        unsigned long long pixels=capture(name);
        printf("{\"kind\":\"hud\",\"id\":\"%s\",\"valid\":%d,\"hits\":%d,\"fonts\":[%d,%d,%d,%d],\"left\":",
            name,valid,hits,sdl_auto_aux_view_font_size(),sdl_auto_pane_font_size(PANE_LEFT_PANEL),
            sdl_auto_pane_font_size(PANE_COMBAT),sdl_auto_pane_font_size(PANE_STATUS_DEPTH));
        rect(left); printf(",\"thumb\":[");
        for(int i=0;i<count;i++) { if(i)printf(","); rect(thumb[i]); }
        printf("],\"fitted\":[");
        for(int i=0;i<PANE_MAX;i++) {
            if(i) printf(",");
            rect((SDL_FRect){fitted[i].x,fitted[i].y,fitted[i].w,fitted[i].h});
        }
        printf("],\"pixels\":\"%016llx\"}\n",pixels);
    }
}
int main(int argc,char **argv) {
    assert(argc==8); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]); kind=argv[4];
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"); assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); assert(TTF_Init());
    static maxima limits; static player_type player; static player_other options;
    static object_type items[INVEN_TOTAL]; static byte features[32][MAX_DUNGEON_WID];
    static s16b floor_objects[32][MAX_DUNGEON_WID],monsters[32][MAX_DUNGEON_WID];
    static u16b info[32][256];
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    cave_feat=features; cave_o_idx=floor_objects; cave_m_idx=monsters; cave_info=info;
    player.playing=true; player.song1=player.song2=SNG_NOTHING; player.food=PY_FOOD_FULL-1;
    player.chp=49; player.mhp=61; player.csp=player.msp=49;
    player.cur_map_hgt=32; player.cur_map_wid=64; items[INVEN_LITE].k_idx=1;
    character_generated=character_dungeon=true; use_bigtile=0;
    sdl_config_set_defaults(&config); assert(!config.bigger_font);
    config.use_unsafe_area=true; config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    config.mobile_portrait_mode=height>width; config.show_main_menu_button=false;
    config.enable_right_panes=config.enable_bottom_panes=false;
    SDL_strlcpy(config.monospace_font,argv[5],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[6],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[7],sizeof(config.story_font2));
    g_state.system_scale=density; g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    g_state.window=SDL_CreateWindow("Normal touch compatibility",width,height,SDL_WINDOW_HIDDEN); assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); assert(g_state.renderer);
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){240,210,60,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){90,235,140,255};
    sdl_view *view=&g_views[PANE_MAIN]; assert(term_init(&view->t,80,24,16)==0);
    view->t.data=(void *)(uintptr_t)PANE_MAIN; view->term_ready=true;
    Term_activate(&view->t); term_screen=&view->t;
    #ifndef ORIGINAL
    /* Normal rendering must also match after the enlarged caption font has
     * populated the shared texture/font caches. */
    config.bigger_font=true;
    clear();
    sdl_touch_pane_draw_button_text_scaled(&(SDL_FRect){0,0,96*density,96*density},
        "Confirm\nRepeat",NULL,g_state.palette[TERM_WHITE],0,0);
    config.bigger_font=false;
    #endif
    grid(); hud(); return 0;
}
'''


def compare(before, after, where=""):
    if isinstance(before, dict):
        assert before.keys() == after.keys(), where
        for key in before:
            compare(before[key], after[key], f"{where}.{key}")
    elif isinstance(before, list):
        assert len(before) == len(after), where
        for i, (a, b) in enumerate(zip(before, after)):
            compare(a, b, f"{where}[{i}]")
    elif isinstance(before, (int, float)):
        assert abs(before-after) <= .001, f"{where}: before={before}, now={after}"
    else:
        assert before == after, f"{where}: before={before}, now={after}"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    originals = {}
    for path in MODULES + INCLUDED:
        dest = OUT / ("old-" + Path(path).name)
        dest.write_bytes(subprocess.check_output(["git", "show", f"{BEFORE}:{path}"], cwd=ROOT))
        originals[path] = dest
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = tuple("/" + p + ".obj" for p in MODULES + INCLUDED) + ("/src/main.c.obj",)
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / d) for d in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["SDL_GetDisplayContentScale", "sdl_touch_only_device_active", "prt_frame_basic",
             "sdl_layout_matches_supporting_pane_visibility", "sdl_depth_menu_pane_label",
             "player_current_movement_energy", "player_current_movement_speed",
             "floor_context_collect_square_actions", "touch_shortcut_context_action",
             "player_active_weapon_is_ranged", "player_quick_throw_available"]
    for old in (True, False):
        name = "before" if old else "current"
        command = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS", "-std=c17", "-O0", "-g"]
        if old:
            command.append("-DORIGINAL")
        command += ["@CMakeFiles/sil-more.dir/includes_C.rsp", str(source)]
        command += [str(originals[p] if old else ROOT / p) for p in MODULES]
        command += ["@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(OUT / (name + ".exe"))]
        subprocess.run(command, cwd=BUILD, env=env, check=True)
    for w, h, scale in [(580,1280,1.5), (1280,580,1.5), (720,1600,2),
                         (1600,720,2), (1080,2400,2.75), (2400,1080,2.75)]:
        records = []
        for name in ("before", "current"):
            result = subprocess.run([str(OUT / (name + ".exe")), str(w), str(h), str(scale), name,
                                     str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                                     str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                                     str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                                    cwd=OUT, env=env, check=True, capture_output=True, text=True, timeout=60)
            (OUT / f"{name}-{w}x{h}.jsonl").write_text(result.stdout, encoding="utf-8")
            records.append([json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")])
        assert len(records[0]) == len(records[1]) == 104
        for old, new in zip(*records):
            compare(old, new, f"{w}x{h}:{old['kind']}:{old['id']}")
        print(f"Normal {w}x{h} @{scale}: 96 Quick Access grids + 8 Quick Touch/character renders match {BEFORE}; pixels and hit tests PASS", flush=True)


if __name__ == "__main__":
    main()
