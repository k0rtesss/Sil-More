#!/usr/bin/env python3
"""Render full descriptions from the real chest builder through SDL questions.

No save is opened. Chest choice is intercepted before an action is accepted;
the production overlay stores/measures/renders the resulting text at 768x576.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/question-description-storage'

CHEST=r'''
#include "angband.h"
#include "ui/question.h"
static char captured[2048];
static int fixture_question(cptr title,cptr desc,const ui_question_option* options,int count,int y,int x,int selected)
{ (void)title;(void)options;(void)count;(void)y;(void)x;(void)selected;SDL_strlcpy(captured,desc,sizeof(captured));return -1; }
#define ui_question_ask fixture_question
#include "cmd/world/cmd-interact-chest.c"
#undef ui_question_ask
void fixture_chest_description(int pval,char* out,size_t size)
{
    object_prep(&o_list[1],lookup_kind(TV_CHEST,SV_CHEST_SMALL_JEWELLED));o_max=2;
    o_list[1].number=1;o_list[1].pval=pval;o_list[1].ident|=IDENT_CHEST_TRAP_FULL;
    chest_retry.active=true;chest_retry.y=1;chest_retry.x=1;
    SDL_strlcpy(chest_retry.previous,"Inspection succeeded. You identified the trap.",sizeof(chest_retry.previous));
    captured[0]=0;(void)do_cmd_chest_minigame(1,1,1);SDL_strlcpy(out,captured,size);
}
'''
CHECKS=r'''
#include "sdl/main-sdl-private.h"
#include "question.c"
void fixture_chest_description(int pval,char* out,size_t size);
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect* rect)
{(void)pane;*rect=(SDL_Rect){0,0,768,576};return true;}
int __wrap_sdl_main_menu_pane_font_px(void)
{return get_sdl_menu_bigger_font()?20:13;}
static void snapshot(cptr label)
{
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render();SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
    char path[2048];strnfmt(path,sizeof(path),"%s/%s.png",ANGBAND_DIR_USER,label);
    assert(IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
}
static void check_text(cptr label,cptr text,int big)
{
    config.bigger_font=big;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++)config.menu_bigger_font[menu]=big;
    sdl_question_menu_begin("Small jewelled chest (Gas Trap)");
    sdl_question_menu_set_desc(text);
    assert(!strcmp(sdl_question_menu_description(),text));
    sdl_question_menu_add_entry(0,"d)","Disarm: 64%",TERM_L_GREEN);
    sdl_question_menu_add_entry(1,"o)","Open anyway (lock): 55%",TERM_L_UMBER);
    int offset=0;sdl_question_menu_set_scroll_offset_target(&offset,false);
    sdl_question_menu_layout_info layout;assert(sdl_question_menu_layout(&layout));
    TTF_Font* font=sdl_story_font_for_height_slot(layout.font_px,SDL_STORY_FONT_SLOT_MENU);assert(font);
    int w=0,h=0;assert(TTF_GetStringSizeWrapped(font,text,0,(int)layout.desc_rect.w,&w,&h));
    printf("%s bytes=%zu retained=yes body=%d visible=%.0f scroll=%d in_list=%d action0=%.0f+%.0f panel=%.0f+%.0f\n",label,strlen(text),h,layout.desc_rect.h,layout.max_scroll_offset,layout.description_in_list,layout.rows[0].y,layout.rows[0].h,layout.panel.y,layout.panel.h);
    assert(layout.desc_rect.h+1>=h); /* Every line, including the tail, fits. */
    snapshot(label);
    assert(layout.rows[0].h>0&&layout.rows[1].h>0);
    assert(layout.rows[1].y+layout.rows[1].h<=layout.panel.y+layout.panel.h);
    assert(layout.panel.y>=0&&layout.panel.y+layout.panel.h<=576);
    sdl_question_menu_clear();assert(!g_question_full_description);
    /* Existing Help continues to retain the same full text. */
    sdl_question_menu_begin("Chest Help");sdl_question_menu_set_help(text);
    assert(!strcmp(sdl_question_menu_description(),text));
    sdl_question_menu_clear();assert(!g_question_full_description);
}
static void checks(void)
{
    char gas[2048]="",multi[2048]="",candidate[2048];
    for(int pval=1;pval<=25;pval++) {
        fixture_chest_description(pval,candidate,sizeof(candidate));
        if(strlen(candidate)>strlen(multi))SDL_strlcpy(multi,candidate,sizeof(multi));
        if(strstr(candidate,"Confusion gas: 4d4 turns of confusion.")&&strlen(candidate)==487)SDL_strlcpy(gas,candidate,sizeof(gas));
    }
    assert(strlen(gas)==487&&strstr(gas,"then the chest opens."));
    assert(strlen(multi)>strlen(gas));
    assert(strstr(multi,"Confusion gas:")&&strstr(multi,"Hallucinatory needle:")&&strstr(multi,"then the chest opens."));
    long clock=turn,player_clock=playerturn;
    for(int big=0;big<2;big++) {
        check_text(big?"gas-big":"gas-normal",gas,big);
        check_text(big?"multi-big":"multi-normal",multi,big);
    }
    assert(clock==turn&&player_clock==playerturn);
    puts("Full chest descriptions retained across normal/Big and set_desc/set_help, freed on clear, clocks unchanged PASS");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    q=(ROOT/'src/sdl/ui/sdl-question-menu.c').read_text()
    if args.baseline:
        old='g_question_full_description = text ? SDL_strdup(text) : NULL;';assert q.count(old)==2
        q=q.replace(old,'g_question_full_description = SIL_SDL_MOBILE_BUILD && get_sdl_menu_bigger_font() && text ? SDL_strdup(text) : NULL;')
    (OUT/'question.c').write_text(q,encoding='utf-8');(OUT/'chest.c').write_text(CHEST,encoding='utf-8')
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    prefix=prefix.replace('#include "level-generation/level-generation-internal.h"','#include "sdl/main-sdl-private.h"')
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    init=init.replace('assert(SDL_Init(SDL_INIT_EVENTS));','SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());')
    setup='''sdl_config_set_defaults(&config);g_state.system_scale=1;
    SDL_strlcpy(config.monospace_font,"'''+str(ROOT/'lib/xtra/font/VictorMono-Medium.ttf').replace('\\','/')+'''",sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,"'''+str(ROOT/'lib/xtra/font/Cinzel-Medium.ttf').replace('\\','/')+'''",sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,"'''+str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf').replace('\\','/')+'''",sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Question",768,576,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
    for(int i=0;i<MAX_COLORS;i++)g_state.palette[i]=(SDL_Color){220,220,220,255};
    checks();SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();SDL_Quit();return 0;\n}\n'''
    (OUT/'check.c').write_text(prefix+CHECKS+init+setup,encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    exclude=('/src/main.c.obj','/src/sdl/ui/sdl-question-menu.c.obj','/src/cmd/world/cmd-interact-chest.c.obj')
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(exclude)),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'chest.c'),'@'+str(OUT/'objects.rsp'),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=sdl_overlay_pane_anchor_rect,--wrap=sdl_main_menu_pane_font_px','-o',str(exe)],cwd=BUILD,env=env,check=True)
    # Retain rendered evidence while raw engine caches remain disposable.
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=OUT,env=env,check=True,timeout=20)
        import shutil
        for png in Path(state).glob('*.png'):shutil.copy2(png,OUT/png.name)

if __name__=='__main__':main()
