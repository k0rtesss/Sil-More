#!/usr/bin/env python3
"""Render real full-log combat rows with SDL, without opening player data."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/combat-log-tiles"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
/* FULL LOG SOURCE */
#include <assert.h>

static errr fixture_xtra(int n,int v) { return 0; }
static errr fixture_cursor(int x,int y) { return 0; }

static int find_icon(byte attr,char chr,int *x,int *y)
{
    int count=0;
    for(int row=0;row<Term->hgt;row++)for(int col=0;col<Term->wid;col++)
        if(Term->scr->a[row][col]==attr && Term->scr->c[row][col]==chr) {
            *x=col;*y=row;count++;
        }
    return count;
}

static void icon_pixels(sdl_view *view,int x,int y)
{
    SDL_Rect area={x*view->cell_w,y*view->cell_h,
        view->cell_w*(use_bigtile+1),view->cell_h};
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&area);
    assert(pixels);
    int ink=0;
    for(int py=0;py<pixels->h;py++)for(int px=0;px<pixels->w;px++) {
        Uint8 r,g,b,a;
        assert(SDL_ReadSurfacePixel(pixels,px,py,&r,&g,&b,&a));
        if(a && (r || g || b))ink++;
    }
    SDL_DestroySurface(pixels);
    assert(ink>8);
}

static void check(int width,bool bigger,bool bigtile,bool ascii,bool automatic)
{
    sdl_view *view=&g_views[PANE_MAIN];
    config.bigger_font=bigger;g_state.use_tiles=!ascii;use_bigtile=bigtile;
    use_graphics=ascii?GRAPHICS_NONE:GRAPHICS_MICROCHASM;
    view->cell_h=bigger?30:20;view->cell_w=view->cell_h/2;
    view->cols=width;view->rows=24;view->term_ready=true;
    view->rect=(SDL_Rect){0,0,width*view->cell_w,24*view->cell_h};
    assert(Term_resize(width,24)==0);
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,view->rect.w,view->rect.h);
    assert(view->canvas);
    view->font_atlas=sdl_load_ttf_font_cells(config.monospace_font,
        view->cell_w,view->cell_h,NULL);
    assert(view->font_atlas);
    view->font_atlas_cell_w=view->cell_w;view->font_atlas_cell_h=view->cell_h;
    Term->higher_pict=!ascii;
    combat_roll roll={.att_type=automatic?COMBAT_ROLL_AUTO:COMBAT_ROLL_ROLL,
        .att=14,.att_roll=17,.evn=8,.evn_roll=11,.dd=2,.ds=7,
        .dd2=1,.ds2=5,.dam=15,.prot=3,.prt_percent=100,
        /* The current player and Orc scout tiles from monster.txt. */
        .attacker_attr=ascii?TERM_L_BLUE:(0x80|13),
        .attacker_char=ascii?'@':(char)0x80,
        .defender_attr=ascii?TERM_GREEN:(0x80|7),
        .defender_char=ascii?'o':(char)(0x80|25)};
    log_history_entry entry={.kind=LOG_HISTORY_ENTRY_COMBAT,.roll=&roll};
    log_history_entries[0]=entry;
    char text[256];log_history_entry_search_text(&entry,text,sizeof(text));
    for(cptr p=text;*p;p++)assert((byte)*p<0x80);
    int rows=log_history_wrapped_entry_rows(LOG_HISTORY_FILTER_COMBAT,0,width);
    assert(rows>0 && rows<20);
    byte full_attrs[24][120];char full_chars[24][120];
    for(int first=0;first<rows;first++) {
        assert(Term_clear()==0);
        if(rows==1)log_history_draw_combat_entry(&entry,2,0);
        else log_history_draw_wrapped_combat(&entry,2,width,"net",first,rows-first);
        int ax=0,ay=0,dx=0,dy=0;
        if(first==0) {
            for(int y=0;y<Term->hgt;y++) {
                memcpy(full_attrs[y],Term->scr->a[y],width);
                memcpy(full_chars[y],Term->scr->c[y],width);
            }
            assert(find_icon(roll.attacker_attr,roll.attacker_char,&ax,&ay)==1);
            assert(find_icon(roll.defender_attr,roll.defender_char,&dx,&dy)==1);
            if(bigtile && !ascii) {
                assert(Term->scr->a[ay][ax+1]==255);
                assert(Term->scr->a[dy][dx+1]==255);
            }
        }
        assert(Term_fresh()==0);
        SDL_SetRenderTarget(g_state.renderer,view->canvas);
        if(first==0) {
            icon_pixels(view,ax,ay);icon_pixels(view,dx,dy);
            char path[256];
            strnfmt(path,sizeof(path),"%s/log-%d-big%d-double%d-ascii%d-auto%d.png",
                "scripts/output/combat-log-tiles",width,bigger,bigtile,ascii,automatic);
            SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
            assert(pixels && IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
        }
        /* Slices must be the same rows as the full entry, and fit the body. */
        for(int y=0;y<rows-first;y++) {
            assert(!memcmp(full_attrs[2+first+y],Term->scr->a[2+y],width));
            assert(!memcmp(full_chars[2+first+y],Term->scr->c[2+y],width));
        }
        for(int row=2+rows-first;row<Term->hgt;row++)for(int col=0;col<width;col++)
            assert(Term->scr->c[row][col]==' ');
        if(rows==1)break;
    }
    SDL_SetRenderTarget(g_state.renderer,NULL);
    SDL_DestroyTexture(view->font_atlas);view->font_atlas=NULL;
    SDL_DestroyTexture(view->canvas);view->canvas=NULL;
}

int main(void)
{
    maxima limits={0};player_type player={0};player_other options={0};
    z_info=&limits;p_ptr=&player;op_ptr=&options;
    log_set_quiet(true);SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.monospace_font,"lib/xtra/font/VictorMono-Medium.ttf",sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,"lib/xtra/font/Cinzel-Medium.ttf",sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,"lib/xtra/font/EBGaramond-Regular.ttf",sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Combat log fixture",1600,900,SDL_WINDOW_HIDDEN);
    assert(g_state.window);g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer);g_state.system_scale=1;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){angband_color_table[i][1],angband_color_table[i][2],angband_color_table[i][3],255};
    assert(sdl_load_tileset_texture());
    sdl_view *view=&g_views[PANE_MAIN];term_init(&view->t,80,24,256);
    Term_activate(&view->t);term_screen=Term;Term->data=(void*)(uintptr_t)PANE_MAIN;
    Term->xtra_hook=fixture_xtra;Term->curs_hook=fixture_cursor;
    Term->wipe_hook=callback_sdl_wipe;Term->text_hook=callback_sdl_text;
    Term->pict_hook=callback_sdl_pict;
    const int widths[]={12,30,40,80,120};
    for(int big=0;big<2;big++)for(int ascii=0;ascii<2;ascii++)
        for(int double_tile=0;double_tile<2;double_tile++)for(int automatic=0;automatic<2;automatic++)
            for(size_t i=0;i<N_ELEMENTS(widths);i++)check(widths[i],big,double_tile,ascii,automatic);
    puts("Combat full log: SDL icon pixels, wrapped/narrow rows, normal/bigger font, single/double tiles, ASCII, auto attacks and slices: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    commands = (ROOT / "src/cmd/ui/cmd-ui-main-menu.c").read_text()
    # main-sdl-private.h already includes the unguarded extern declarations.
    commands = commands.replace('#include "externs.h"', '')
    source.write_text(HARNESS.replace("/* FULL LOG SOURCE */", commands), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-main-menu.c.obj")
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD / "_deps" / dep) for dep in
        ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
