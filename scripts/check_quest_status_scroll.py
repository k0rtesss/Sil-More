#!/usr/bin/env python3
"""Exercise production Quests input and rendered scrolling in a private SDL canvas."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/quest-status-scroll'

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "log/log.h"
#include <assert.h>
/* HINT_SOURCE */
/* QUEST_SOURCE */
static int pos, ending;
static cptr output_dir;
static errr terminal_extra(int action,int value) { (void)action;(void)value;return 0; }
void __wrap_handle_stuff(void) {}
void __wrap_sdl_present_if_needed(sdl_view* view) { (void)view; }
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void) { return false; }
static void capture(cptr name)
{
    char path[1024];strnfmt(path,sizeof(path),"%s/%s.png",output_dir,name);
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(pixels&&IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
}
static void assert_final_tail_visible(void)
{
    sdl_hint_quest_layout layout;assert(sdl_hint_quest_layout_compute(&layout));
    TTF_Font* font=sdl_story_font_for_height_slot(layout.body_px,SDL_STORY_FONT_SLOT_NARRATIVE);
    assert(font);float y=0,last_bottom=0;int last=-1;
    for(int i=0;i<g_hint_quest.block_count;i++) {
        y+=sdl_hint_quest_block_height(font,&layout,&g_hint_quest.blocks[i],NULL);
        if(g_hint_quest.blocks[i].text[0]) { last=i;last_bottom=y; }
        if(i+1<g_hint_quest.block_count)y+=layout.block_gap;
    }
    assert(last>=0&&strstr(g_hint_quest.blocks[last].text,"deeper"));
    float bottom=layout.body.y+last_bottom-g_hint_quest.scroll_y;
    assert(bottom>layout.body.y&&bottom<=layout.body.y+layout.body.h+1);
}
char __wrap_inkey(void)
{
    assert(sdl_hint_quest_menu_active());sdl_hint_quest_menu_render();
    assert(g_hint_quest.max_scroll_y>0&&g_hint_quest.body_rect.h>0);
    assert(pos<=71);
    if(pos==0) { assert(quest_status_tabs_focus&&g_hint_quest.scroll_y==0);capture("top"); }
    if(pos==1)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y==0);
    if(pos==2)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y>0);
    if(pos==3)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y==0);
    if(pos==4)assert(quest_status_tabs_focus);
    if(pos==5)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y>0);
    if(pos==6)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y==0);
    if(pos==38) {
        assert(g_hint_quest.scroll_y==g_hint_quest.max_scroll_y);
        assert_final_tail_visible();capture("final-tail");
    }
    if(pos==70)assert(!quest_status_tabs_focus&&g_hint_quest.scroll_y==0);
    if(pos==71)assert(quest_status_tabs_focus);
    int index=pos++;
    if(index==0||index==1)return '2';
    if(index==2||index==3||index==70)return '8';
    if(index==4)return '3';
    if(index==5)return '9';
    if(index<38)return '3';
    if(index<70)return '9';
    return ending==0?ESCAPE:ending==1?']':'[';
}
int main(int argc,char** argv)
{
    assert(argc==3);output_dir=argv[2];setbuf(stdout,NULL);
    player_type player={0};player_other options={0};maxima limits={0};
    object_type items[INVEN_TOTAL]={0};
    p_ptr=&player;op_ptr=&options;z_info=&limits;inventory=items;
    p_ptr->playing=true;p_ptr->py=p_ptr->px=2;p_ptr->depth=3;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;p_ptr->chp=p_ptr->mhp=100;p_ptr->new_exp=123;
    items[INVEN_LITE].timeout=281;turn=1001;playerturn=100;
    character_generated=character_dungeon=true;log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));assert(TTF_Init());
    sdl_config_set_defaults(&config);sdl_sync_palette();config.use_unsafe_area=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;g_state.system_scale=1;
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Quests scroll fixture",768,576,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,768,576};
    sdl_view* view=&g_views[PANE_MAIN];assert(!term_init(&view->t,96,36,256));
    view->t.xtra_hook=terminal_extra;Term_activate(&view->t);term_screen=&view->t;angband_term[0]=&view->t;
    view->term_ready=true;view->rect=(SDL_Rect){0,0,768,576};view->cell_w=8;view->cell_h=16;view->cols=96;view->rows=36;
    for(ending=0;ending<3;ending++) {
        pos=0;hint_quest_page page=do_cmd_quest_status_page();
        if(pos!=72)fprintf(stderr,"Quests closed after %d inputs instead of consuming body navigation (expected 72).\n",pos);
        assert(pos==72);
        assert(page==(ending==0?HINT_QUEST_PAGE_EXIT:ending==1?HINT_QUEST_PAGE_THRALLS:HINT_QUEST_PAGE_HINTS));
        assert(turn==1001&&playerturn==100&&items[INVEN_LITE].timeout==281&&p_ptr->chp==100&&p_ptr->new_exp==123);
        sdl_hint_quest_menu_hide();
    }
    puts("Quests real parser/render: tabs/body Up/Down/Page keys, reachable final text, B close, both shoulders and unchanged clocks/resources PASS");
    sdl_story_font_cache_clear();term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();SDL_Quit();return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    quest = (ROOT/'src/quest/quest-status.c').read_text().replace('#include "externs.h"','')
    if args.baseline:
        old = '        if (quest_status_handle_body_navigation(ch))\n            continue;\n'
        assert quest.count(old)==1
        quest = quest.replace(old,'')
    hint = (ROOT/'src/sdl/ui/sdl-hint-quest-menu.c').read_text()
    source = OUT/'check.c'
    source.write_text(HARNESS.replace('/* HINT_SOURCE */',hint).replace('/* QUEST_SOURCE */',quest))
    cmake = BUILD/'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake/'objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/quest/quest-status.c.obj','/src/sdl/ui/sdl-hint-quest-menu.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        '-Wl,--wrap=inkey,--wrap=handle_stuff,--wrap=sdl_present_if_needed,--wrap=sdl_refresh_supporting_panes_layout,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen',
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    subprocess.run([str(exe),str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf'),str(OUT)],cwd=ROOT,env=env,check=True,timeout=30)


if __name__=='__main__':
    main()
