#!/usr/bin/env python3
"""Render the production map UI with synthetic terrain and clue data.

Checks readable, unscaled text, unclipped wrapping, control hitboxes, zoom,
pan, hint visibility and close. No player saves or settings are opened.
Build the standard target first. PNGs go under scripts/output/bigger-font-map.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bigger-font-map"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>

/* Include the renderer to exercise its internal label path as well. */
#include "sdl/render/sdl-term-callbacks.c"

static float density;
static int input_mode, text_draws;
static bool record_text;
static const char *tip = "An enchanted forge lies to the northeast, beyond the ruined halls. Tap this clue to read the full message.";
static struct {SDL_Texture *texture; int w,h; char text[180];} texts[64];
static int text_count;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) {(void)display;return density;}
bool __wrap_steamdeck_controls_active(void) {return input_mode==2;}
bool __wrap_sdl_touch_only_device_active(void) {return input_mode==0;}
byte __wrap_hint_messages_count_for_save(void) {return 1;}
void __wrap_hint_messages_message_meta(int i,hint_message_meta *meta) {
    assert(i==0); memset(meta,0,sizeof(*meta));
    meta->source_y=meta->source_x=16;
    meta->destination_count=1;
    meta->destinations[0]=(hint_message_destination){
        HINT_DESTINATION_FIXED_FEATURE,8,28,FEAT_FORGE_GOOD_HEAD,4,18};
}
bool __wrap_hint_messages_short_tip_for_source(int y,int x,char *out,size_t size) {
    if(y!=16||x!=16)return false;
    SDL_strlcpy(out,tip,size);return true;
}
void __wrap_map_info(int y,int x,byte *a,char *c,byte *ta,char *tc) {
    *a=*ta=(y%8==0||x%10==0)?TERM_SLATE:TERM_L_DARK;
    *c=*tc='.';
}
SDL_Texture *__real_sdl_ui_wrapped_text_texture(TTF_Font*,cptr,int,SDL_Color,int*,int*);
SDL_Texture *__wrap_sdl_ui_wrapped_text_texture(TTF_Font *font,cptr text,int wrap,
    SDL_Color color,int *w,int *h) {
    SDL_Texture *texture=__real_sdl_ui_wrapped_text_texture(font,text,wrap,color,w,h);
    if(record_text&&texture) {
        assert(text_count<64);
        texts[text_count].texture=texture;texts[text_count].w=*w;texts[text_count].h=*h;
        SDL_strlcpy(texts[text_count++].text,text,180);
        assert(TTF_GetFontSize(font)>=density*15);
    }
    return texture;
}
static void contained(SDL_FRect rect,SDL_FRect area) {
    if(rect.x<area.x-0.1f||rect.y<area.y-0.1f
        ||rect.x+rect.w>area.x+area.w+0.1f||rect.y+rect.h>area.y+area.h+0.1f) {
        fprintf(stderr,"rect %.1f %.1f %.1f %.1f outside %.1f %.1f %.1f %.1f\n",
            rect.x,rect.y,rect.w,rect.h,area.x,area.y,area.w,area.h);abort();
    }
}
bool __real_SDL_RenderTexture(SDL_Renderer*,SDL_Texture*,const SDL_FRect*,const SDL_FRect*);
bool __wrap_SDL_RenderTexture(SDL_Renderer *renderer,SDL_Texture *texture,
    const SDL_FRect *src,const SDL_FRect *dst) {
    if(record_text) for(int i=text_count-1;i>=0;i--) if(texts[i].texture==texture) {
        assert(dst&&(!src||(src->w==texts[i].w&&src->h==texts[i].h)));
        assert(dst->w==texts[i].w&&dst->h==texts[i].h);
        contained(*dst,strstr(texts[i].text,"pan")?g_minimap.prompt_rect:g_minimap.viewport_rect);
        if(SDL_RenderClipEnabled(renderer)) {
            SDL_Rect clip;SDL_GetRenderClipRect(renderer,&clip);
            contained(*dst,(SDL_FRect){clip.x,clip.y,clip.w,clip.h});
        }
        text_draws++;break;
    }
    return __real_SDL_RenderTexture(renderer,texture,src,dst);
}
static void press(SDL_FRect rect) {
    assert(sdl_minimap_handle_control_point(rect.x+rect.w/2,rect.y+rect.h/2));
}
int main(int argc,char **argv) {
    assert(argc==7);setbuf(stdout,NULL);log_set_quiet(true);
    int width=atoi(argv[1]),height=atoi(argv[2]);density=atof(argv[3]);
    bool bigger=atoi(argv[4]);input_mode=atoi(argv[5]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    assert(TTF_Init());
    static player_type player;static player_other options;static maxima limits;
    static byte features[40][MAX_DUNGEON_WID];
    static s16b monsters[40][MAX_DUNGEON_WID],objects[40][MAX_DUNGEON_WID];
    static u16b info[40][256];
    p_ptr=&player;op_ptr=&options;z_info=&limits;
    cave_feat=features;cave_m_idx=monsters;cave_o_idx=objects;cave_info=info;
    mon_max=o_max=0;use_graphics=GRAPHICS_NONE;pixel_monster_status_icons=false;
    player.cur_map_hgt=40;player.cur_map_wid=48;player.py=player.px=16;player.depth=3;
    for(int y=0;y<40;y++)for(int x=0;x<48;x++) {
        features[y][x]=FEAT_FLOOR;info[y][x]=CAVE_MARK;
    }
    info[8][28]=0;
    sdl_config_set_defaults(&config);config.bigger_font=bigger;
    SDL_strlcpy(config.monospace_font,argv[6],sizeof(config.monospace_font));
    g_state.window=SDL_CreateWindow("Map fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer);g_state.system_scale=density;
    sdl_view *view=&g_views[PANE_MAIN];
    assert(sdl_view_create(view,(SDL_Rect){0,0,width,height},argv[6],0,1,0));
    assert(term_init(&view->t,view->cols,view->rows,32)==0);
    view->t.data=(void*)(uintptr_t)PANE_MAIN;view->term_ready=true;Term_activate(&view->t);
    sdl_minimap_begin();g_minimap.skeleton_hints_visible=true;
    g_minimap.focus_active=true;g_minimap.focus_y=g_minimap.focus_x=16;
    g_minimap.default_zoom_pending=false;
    record_text=bigger;
    assert(sdl_display_pixel_map(NULL,NULL));
    int canvas_w=view->cols*view->cell_w,canvas_h=view->rows*view->cell_h;
    SDL_FRect canvas={0,0,canvas_w,canvas_h};
    SDL_FRect buttons[]={g_minimap.close_rect,g_minimap.skeleton_hints_rect,
        g_minimap.zoom_in_rect,g_minimap.zoom_out_rect};
    for(int i=0;i<4;i++) {
        contained(buttons[i],canvas);
        if(bigger)assert(buttons[i].w>=sdl_ui_min_tap_px()&&buttons[i].h>=sdl_ui_min_tap_px());
        for(int j=0;j<i;j++)assert(!SDL_HasRectIntersectionFloat(&buttons[i],&buttons[j]));
        if(bigger)assert(!SDL_HasRectIntersectionFloat(&buttons[i],&g_minimap.viewport_rect));
    }
    if(bigger) {
        contained(g_minimap.prompt_rect,canvas);assert(text_draws>=2);
        assert(g_minimap.viewport_rect.h>height*0.35f);
        assert(g_minimap.viewport_rect.y+g_minimap.viewport_rect.h<=g_minimap.prompt_rect.y);
        assert(sdl_minimap_handle_control_point(1,1));
        assert(sdl_minimap_handle_control_point(1,canvas_h-1));
        /* Zoomed map coordinates beneath the footer must not accept a clue tap. */
        SDL_FRect saved_map=g_minimap.map_rect;
        g_minimap.map_rect=(SDL_FRect){20-16.5f*saved_map.w/48,
            canvas_h-2-16.5f*saved_map.h/40,saved_map.w,saved_map.h};
        int grid_y,grid_x;
        assert(!sdl_minimap_grid_at_canvas_point(20,canvas_h-2,&grid_y,&grid_x));
        assert(!sdl_minimap_hint_source_at_canvas_point(20,canvas_h-2,NULL,NULL,NULL));
        g_minimap.map_rect=saved_map;
    } else {
        assert(g_minimap.prompt_rect.h==0);
        assert(g_minimap.viewport_rect.y==0&&g_minimap.viewport_rect.h==canvas_h-view->cell_h);
    }
    char path[120];strnfmt(path,sizeof(path),"map-%dx%d-%.2f-%s-%d.png",
        width,height,density,bigger?"on":"off",input_mode);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
    assert(IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
    if(bigger) {
        /* A long tip in short landscape must gain width before losing text. */
        sdl_minimap_draw_large_text_box(tip,canvas_w/2,
            g_minimap.viewport_rect.y+g_minimap.viewport_rect.h/2,true,
            (SDL_Color){80,245,130,255},SDL_UI_FONT_BODY);
    }
    record_text=false;
    press(g_minimap.zoom_in_rect);assert(g_minimap.zoom_step==1);
    press(g_minimap.zoom_out_rect);assert(g_minimap.zoom_step==0);
    press(g_minimap.zoom_out_rect);assert(g_minimap.zoom_step==0);
    press(g_minimap.skeleton_hints_rect);assert(!g_minimap.skeleton_hints_visible);
    assert(sdl_minimap_pan(1,0));
    press(g_minimap.close_rect);char key=0;assert(Term_inkey(&key,false,true)==0&&key==ESCAPE);
    printf("Map %dx%d @%.2f font=%s input=%d: text, wrapping, bounds and controls PASS\n",
        width,height,density,bigger?"big":"normal",input_mode);
    sdl_minimap_end();return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/render/sdl-term-callbacks.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["SDL_GetDisplayContentScale", "steamdeck_controls_active",
             "sdl_touch_only_device_active", "hint_messages_count_for_save",
             "hint_messages_message_meta", "hint_messages_short_tip_for_source",
             "map_info", "sdl_ui_wrapped_text_texture", "SDL_RenderTexture"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, scale in [(360,800,1), (800,360,1), (580,1280,1.5),
                                 (720,1600,2), (1600,720,2), (1080,2400,2.75),
                                 (2400,1080,2.75), (1280,720,1), (240,800,1)]:
        for mode, bigger in [(0,1), (1,1), (2,1), (0,0)]:
            subprocess.run([str(exe), str(width), str(height), str(scale), str(bigger), str(mode),
                            str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf")],
                           cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
