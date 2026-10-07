#!/usr/bin/env python3
"""Render the production Halls surface at real phone pixels and display density.

Uses the score caller's complete action sequence, including two page actions,
and verifies actual label pixels as well as geometry. No user data is loaded.
Requires a current standard build; output is scripts/output/phone-halls-check.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/phone-halls-check"
HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "sdl/main-sdl-private.h"
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include "sdl/render/sdl-fonts.c"
#include "sdl/ui/sdl-halls-screen.c"
static int width, height;
static float density;
static int tapped_choice=INT_MIN,tapped_action=INT_MIN,wake_count;
bool __wrap_ui_menu_click_handle_choice_action(int choice,int action,bool *wake)
{ tapped_choice=choice; tapped_action=action; if(wake) *wake=false; return true; }
errr __wrap_Term_keypress(int key) { assert(key==UI_MENU_CLICK_WAKE_KEY); wake_count++; return 0; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,width,height}; }
static void inside(SDL_FRect r)
{
    assert(r.w>0 && r.h>0 && r.x>=0 && r.y>=0);
    assert(r.x+r.w<=width+1 && r.y+r.h<=height+1);
}
static bool overlap(SDL_FRect a, SDL_FRect b)
{ return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y; }
static void snapshot(const char *kind,int actions)
{
    char path[160];
    strnfmt(path,sizeof(path),"%dx%d-%.3f-%s-%s-%d.png",width,height,density,
        config.bigger_font?"on":"off",kind,actions);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(pixels && IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
}
static void verify_actions(void)
{
    float dp=sdl_ui_density_scale();
    for(int i=0;i<g_sdl_halls.action_count;i++) {
        sdl_halls_action *a=&g_sdl_halls.actions[i];
        if(!a->enabled) { assert(a->hit_rect.w==0); continue; }
        SDL_FRect r=a->hit_rect; inside(r);
        assert(r.w>=48*dp && r.h>=48*dp);
        assert(r.y>=g_halls_mobile.body.y+g_halls_mobile.body.h);
        for(int j=0;j<i;j++) if(g_sdl_halls.actions[j].enabled)
            assert(!overlap(r,g_sdl_halls.actions[j].hit_rect));
        int tw=0,th=0;
        TTF_Font *font=sdl_halls_role_font(SDL_UI_FONT_CONTROL);
        assert(TTF_GetStringSizeWrapped(font,sdl_halls_mobile_action_label(a),0,
            (int)(r.w-12*dp),&tw,&th));
        assert(th<=r.h-12*dp+1 && tw<=r.w-12*dp+1);
        /* Check bright text ink inside the label box, excluding the border. */
        SDL_Rect area={(int)(r.x+6*dp),(int)(r.y+(r.h-th)*.5f),
            MAX(1,tw),MAX(1,th)};
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&area);
        assert(pixels); int ink=0,first=th,last=-1;
        for(int y=0;y<pixels->h;y++) for(int x=0;x<pixels->w;x++) {
            Uint8 red,green,blue,alpha;
            assert(SDL_ReadSurfacePixel(pixels,x,y,&red,&green,&blue,&alpha));
            if(red>110 && green>110 && blue>110) {
                ink++; first=MIN(first,y); last=MAX(last,y);
            }
        }
        SDL_DestroySurface(pixels);
        assert(ink>20*dp && last-first+1>=sdl_ui_role_font_px(SDL_UI_FONT_CONTROL)*.4f);
        assert(sdl_halls_hit_at(r.x+r.w*.5f,r.y+r.h*.5f)==a->choice);
        int before=wake_count;
        sdl_halls_touch_press_begin(r.x+r.w*.5f,r.y+r.h*.5f,9);
        sdl_halls_touch_press_finish(r.x+r.w*.5f,r.y+r.h*.5f,9);
        assert(tapped_choice==a->choice && tapped_action==UI_MENU_CLICK_PRIMARY);
        assert(wake_count==before+1);
    }
}
static void check(bool detailed,int action_count)
{
    sdl_halls_screen_begin("Here are remembered the fates of those who entered Angband.",
        "Score order  |  full memorials  |  page 2 of 3",detailed,-1);
    int capacity=sdl_halls_screen_page_capacity(detailed);
    if(!detailed && height>width) assert(capacity>1);
    for(int i=0;i<capacity;i++) {
        char name[80],rank[16]; strnfmt(name,sizeof(name),"Aglar the Bold %d",i);
        strnfmt(rank,sizeof(rank),"%d",i+1);
        sdl_halls_screen_add_entry(i,rank,name,"Score: 123,456",
            "Slain by an orc warrior in the depths of Angband.",
            "12,345 turns | deepest descent 950 ft | 2026-10-06",
            "Honourable",TERM_YELLOW,
            "Score increases: treasure, exploration, defeated enemies and valour",
            "Score decreases: none",TERM_L_WHITE,i==0);
    }
    /* Same order and labels as src/score/score_ui.c. Touch callers omit Open. */
    sdl_halls_screen_add_action(-1,"Back",TERM_WHITE,true);
    sdl_halls_screen_add_action(-2,"Run History",TERM_L_BLUE,true);
    sdl_halls_screen_add_action(-3,"Order: Score",TERM_WHITE,true);
    sdl_halls_screen_add_action(-4,detailed?"View: Full":"View: Brief",TERM_WHITE,true);
    if(action_count==7) sdl_halls_screen_add_action(-5,"Open Hero",TERM_L_BLUE,true);
    sdl_halls_screen_add_action(-6,"Previous",TERM_SLATE,action_count>=6);
    sdl_halls_screen_add_action(-7,"Next",TERM_SLATE,action_count>=5);
    sdl_halls_screen_render(); verify_actions(); snapshot(detailed?"full":"brief",action_count);
    int columns,rows; float row_h;
    sdl_halls_mobile_action_grid(g_halls_mobile.body.w,&columns,&rows,&row_h);
    if(action_count==4 && width>=1800 && width>height) {
        assert(columns==4 && rows==1);
        for(int i=1;i<4;i++) assert(g_sdl_halls.actions[i].hit_rect.y==g_sdl_halls.actions[0].hit_rect.y);
    }
    if((action_count==6 || action_count==7) && width>height && config.bigger_font) {
        /* If one complete row fits, use it. Otherwise require balanced3x2/4x2
         * instead of a mostly empty final row. */
        if(rows==1) {
            assert(columns==action_count);
            TTF_Font *font=sdl_halls_role_font(SDL_UI_FONT_CONTROL);
            for(int i=0;i<g_sdl_halls.action_count;i++) if(g_sdl_halls.actions[i].enabled) {
                int w=0,h=0;
                assert(TTF_GetStringSize(font,sdl_halls_mobile_action_label(&g_sdl_halls.actions[i]),0,&w,&h));
                assert(w<=g_sdl_halls.actions[i].hit_rect.w-12*density+1);
            }
        } else assert(rows==2 && columns==(action_count==6?3:4));
    }
    assert(g_halls_mobile.body.h>=48*density);
    if(g_halls_mobile.maximum>0) {
        SDL_Event wheel={0}; wheel.type=SDL_EVENT_MOUSE_WHEEL; wheel.wheel.y=-10000;
        assert(sdl_halls_screen_handle_pointer_event(&wheel));
        assert(g_halls_mobile.offset==g_halls_mobile.maximum);
        sdl_halls_screen_render(); verify_actions();
        sdl_halls_entry *last=&g_sdl_halls.entries[capacity-1];
        inside(last->hit_rect); assert(last->hit_rect.h>=48*density);
        snapshot(detailed?"full-end":"brief-end",action_count);
        /* A vertical finger drag scrolls without committing a hero. */
        float x=g_halls_mobile.body.x+20*density;
        float y=g_halls_mobile.body.y+g_halls_mobile.body.h*.3f;
        sdl_halls_touch_press_begin(x,y,7);
        sdl_halls_touch_press_motion(x,y+60*density,7);
        assert(g_halls_mobile.offset<g_halls_mobile.maximum);
        assert(g_sdl_halls.touch_press_dragged);
        sdl_halls_touch_press_finish(x,y+60*density,7);
    }
    sdl_halls_screen_hide();
}
int main(int argc,char **argv)
{
    assert(argc==6); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[4],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Halls phone fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window); g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer); g_state.system_scale=density;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_SLATE]=(SDL_Color){150,150,150,255};
    g_state.palette[TERM_BLUE]=(SDL_Color){30,60,90,255};
    int base[4];
    for(int big=0;big<2;big++) {
        config.bigger_font=big;
        for(int role=0;role<4;role++) {
            int px=sdl_ui_role_font_px(role);
            if(!big) base[role]=px;
            else assert(abs(px-(int)SDL_ceilf(base[role]*1.5f))<=1);
        }
        for(int actions=4;actions<=7;actions++) {check(true,actions);check(false,actions);}
    }
    printf("Halls %dx%d @%.3f: Off/On, Full/Brief, 4/5/6/7 actions, balanced grid/ink/taps/scroll PASS\n",width,height,density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-halls-screen.c.obj",
                "/src/sdl/render/sdl-fonts.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=SDL_GetDisplayContentScale,--wrap=sdl_get_layout_screen_rect,"
                    "--wrap=ui_menu_click_handle_choice_action,--wrap=Term_keypress",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, density in [(720,1600,2.0),(1080,2340,2.75),(1080,2400,2.625)]:
        for w, h in [(width,height),(height,width)]:
            subprocess.run([str(exe),str(w),str(h),str(density),
                            str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                            str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf")],
                           cwd=OUT,env=env,check=True,timeout=60)


if __name__ == "__main__":
    main()
