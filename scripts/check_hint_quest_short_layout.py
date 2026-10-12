#!/usr/bin/env python3
"""Render production Hints/Quests chrome at supported short window sizes.

Private fixture only: real fonts, book renderer, scroll state and hit regions.
--baseline removes only the short-page body budget to reproduce the blank body.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/hint-quest-short-layout"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#if FIXTURE_MOBILE
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#endif
static int width,height;
static float density;
static cptr output;
SDL_Rect __wrap_sdl_get_layout_screen_rect(void) {return (SDL_Rect){0,0,width,height};}
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id) {(void)id;return density;}
/* PRODUCTION */
static bool intersects(SDL_FRect a,SDL_FRect b)
{ return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h; }
static void inside(SDL_FRect r)
{ assert(r.x>=0 && r.y>=0 && r.x+r.w<=width+1 && r.y+r.h<=height+1); }
static void capture(cptr suffix,bool big,bool tabs)
{
    char path[1024];
    strnfmt(path,sizeof(path),"%s/%s-%dx%d-%s-%s-%s.png",output,
        FIXTURE_MOBILE?"phone":"desktop",width,height,big?"big":"normal",
        tabs?"tabs":"essential",suffix);
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(pixels && IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
}
static void body_ink(const sdl_hint_quest_layout* layout)
{
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
    int ink=0;
    /* Exclude the scrollbar and frame: require actual text inside the body. */
    for(int y=(int)layout->body.y;y<(int)(layout->body.y+layout->body.h);y++)
        for(int x=(int)layout->body.x+2;x<(int)(layout->body.x+layout->body.w)-12;x++) {
            Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
            if(a && (r>40 || g>40 || b>40))ink++;
        }
    SDL_DestroySurface(pixels);assert(ink>80);
}
static void populate(bool tabs)
{
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_QUESTS,
        tabs?"Hints & Quests":"Aule the Smith",
        tabs?"Quest Status":"An essential introduction",tabs,false,0);
    sdl_hint_quest_menu_add_block("Main Quest: The Silmaril",TERM_ORANGE,0,0);
    sdl_hint_quest_menu_add_block(
        "Current objective: Reach the depths of Angband and recover a Silmaril from Morgoth's crown.",
        TERM_L_WHITE,0,0);
    for(int i=0;i<10;i++)sdl_hint_quest_menu_add_block(
        "The long quest description must remain readable through scrolling without entering the footer or navigation controls.",
        TERM_L_WHITE,0,0);
    sdl_hint_quest_menu_add_block("Quest tail",TERM_L_BLUE,0,0);
    sdl_hint_quest_menu_add_button(HINT_QUEST_CLICK_RETURN,"Back",TERM_L_WHITE);
    sdl_hint_quest_menu_finish();
}
static void run(int w,int h,float scale,bool big,bool tabs)
{
    width=w;height=h;density=scale;
    printf("%s %dx%d big=%d tabs=%d\n",FIXTURE_MOBILE?"phone":"desktop",w,h,big,tabs);
    g_state.window=SDL_CreateWindow("Book layout fixture",w,h,SDL_WINDOW_HIDDEN);
    assert(g_state.window);g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer);g_state.safe_area=(SDL_Rect){0,0,w,h};
    config.menu_bigger_font[SDL_MENU_FONT_QUESTS]=big;
    populate(tabs);
    sdl_hint_quest_layout layout;assert(sdl_hint_quest_layout_compute(&layout));
    sdl_hint_quest_menu_render();capture("top",big,tabs);
    if(tabs) {
        if(layout.body.h+1<layout.line_h*2)
            fprintf(stderr,"Body %.2f smaller than two lines %.2f at %dx%d big=%d\n",
                layout.body.h,layout.line_h*2,w,h,big);
        assert(layout.body.h+1>=layout.line_h*2);
        const int ids[]={HINT_QUEST_CLICK_HINTS_TAB,HINT_QUEST_CLICK_QUESTS_TAB,HINT_QUEST_CLICK_THRALLS_TAB};
        for(int i=0;i<3;i++) {
            inside(layout.tabs[i]);assert(!intersects(layout.tabs[i],layout.body));
            assert(sdl_hint_quest_choice_at(layout.tabs[i].x+layout.tabs[i].w/2,
                layout.tabs[i].y+layout.tabs[i].h/2)==ids[i]);
        }
    } else {
        /* Non-tab introductions retain their meaningful title and section. */
        assert(layout.title.h>0 && layout.section.h>0);
    }
    inside(layout.body);inside(layout.footer);assert(!intersects(layout.body,layout.footer));
    if(layout.title.h>0)assert(!intersects(layout.title,layout.body));
    if(layout.section.h>0)assert(!intersects(layout.section,layout.body));
    SDL_FRect back={0};
    for(int i=0;i<g_hint_quest.hit_count;i++)
        if(g_hint_quest.hits[i].choice==HINT_QUEST_CLICK_RETURN)back=g_hint_quest.hits[i].rect;
    assert(back.w>0 && back.h>0);inside(back);assert(!intersects(back,layout.body));
    assert(sdl_hint_quest_choice_at(back.x+back.w/2,back.y+back.h/2)==HINT_QUEST_CLICK_RETURN);
    if(!FIXTURE_MOBILE) {
        int normal=MAX(44,MIN(72,(int)(MIN(w,h)*(h>w?.108f:.09f)+.5f)));
        assert(layout.body_px==(big?normal+(normal+1)/2:normal));
        if(w==768 && h==576 && tabs && !big) {
            assert(layout.title.h>0 && layout.section.h>0);
            assert(layout.body.h>141 && layout.body.h<143);
        }
        if(w==1920 && h==1080 && tabs)assert(layout.title.h>0 && layout.section.h>0);
    } else if(big)assert(layout.body_px==sdl_menu_role_font_px(SDL_UI_FONT_BODY));
    body_ink(&layout);
    assert(g_hint_quest.max_scroll_y>0);
    int steps=0;
    while(sdl_hint_quest_menu_scroll(1,true)) {
        sdl_hint_quest_menu_render();assert(++steps<500);
    }
    sdl_hint_quest_menu_render();capture("tail",big,tabs);
    assert(g_hint_quest.scroll_y==g_hint_quest.max_scroll_y);
    TTF_Font* font=sdl_story_font_for_height_slot(layout.body_px,SDL_STORY_FONT_SLOT_NARRATIVE);
    int last=g_hint_quest.block_count-1;
    float tail_h=sdl_hint_quest_block_height(font,&layout,&g_hint_quest.blocks[last],NULL);
    assert(tail_h<=layout.body.h);
    body_ink(&layout);
    float tail_scroll=g_hint_quest.scroll_y;
    for(int i=0;i<3;i++)sdl_hint_quest_menu_render();
    assert(g_hint_quest.scroll_y==tail_scroll);
    sdl_hint_quest_menu_hide();sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer);g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window);g_state.window=NULL;
}
int main(int argc,char** argv)
{
    assert(argc==3);setbuf(stdout,NULL);output=argv[2];
    player_type player={0};player_other options={0};maxima limits={0};
    p_ptr=&player;op_ptr=&options;z_info=&limits;
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));assert(TTF_Init());
    sdl_config_set_defaults(&config);sdl_sync_palette();config.use_unsafe_area=true;g_state.system_scale=1;
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    if(FIXTURE_MOBILE) {
        for(int big=0;big<2;big++) {run(720,1600,2,big,true);run(1600,720,2,big,true);}
    } else {
        const int sizes[][2]={{768,576},{640,384},{1280,900},{1920,1080},{580,900}};
        for(unsigned i=0;i<N_ELEMENTS(sizes);i++)for(int big=0;big<2;big++)
            run(sizes[i][0],sizes[i][1],1,big,true);
        for(int big=0;big<2;big++)run(768,576,1,big,false);
    }
    TTF_Quit();SDL_Quit();puts("Book body budget, font preservation, tab/Back hits, body ink and repeated tail scrolling PASS");return 0;
}
'''


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    if args.baseline:
        OUT = OUT / "baseline"
    OUT.mkdir(parents=True, exist_ok=True)
    production = (ROOT / "src/sdl/ui/sdl-hint-quest-menu.c").read_text()
    if args.baseline:
        start = production.index("    /* On short tabbed pages, navigation")
        end = production.index("    cursor_y = out->panel.y + pad_y;", start)
        production = production[:start] + production[end:]
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("/* PRODUCTION */", production), encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/sdl/ui/sdl-hint-quest-menu.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    for mobile in ([0] if args.baseline else [0, 1]):
        exe = OUT / ("phone.exe" if mobile else "desktop.exe")
        subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", f"-DFIXTURE_MOBILE={mobile}",
            "-std=c17", "-O0", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
            "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            "-Wl,--wrap=sdl_get_layout_screen_rect,--wrap=SDL_GetDisplayContentScale", "-o", str(exe)],
            cwd=BUILD, env=env, check=True)
        subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(OUT)],
            cwd=ROOT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
