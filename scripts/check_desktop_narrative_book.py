#!/usr/bin/env python3
"""Render real desktop narrative books and run Statistics' real input loop.

The input-loop fixture substitutes authored book content only, retaining the
production key parser and SDL scrolling. No player save or shared build changes.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
from check_phone_books_layout import brace_end

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/desktop-narrative-book'

HARNESS=r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "metarun.h"
#include <assert.h>
static SDL_FRect frame_rect;
static struct {char text[256];SDL_FRect box;bool clipped;} records[512];
static int record_count;
static void record(TTF_Font* font,cptr text,SDL_FRect box,float wrap)
{
    (void)font;(void)wrap;assert(record_count<512);
    SDL_strlcpy(records[record_count].text,text,sizeof(records[0].text));
    records[record_count].box=box;records[record_count++].clipped=SDL_RenderClipEnabled(g_state.renderer);
}
/* SCREENS */
static int width,height,phase,down_steps,before_scroll,before_page;
extern int fixture_build_count,fixture_selected_run;
void fixture_statistics_loop(void);
void fixture_statistics_setup(void);
SDL_Rect __wrap_sdl_get_layout_screen_rect(void){return (SDL_Rect){0,0,width,height};}
void __wrap_screen_save(void){character_icky++;}
void __wrap_screen_load(void){character_icky--;}
void __wrap_screen_push_supporting_panes_hidden(void){}
void __wrap_screen_pop_supporting_panes_hidden(void){}
void __wrap_message_flush(void){}
void __wrap_handle_stuff(void){}
bool __wrap_metarun_tale_management_available(void){return false;}
static void frame(void)
{
    record_count=0;frame_rect=(SDL_FRect){0};
    if(g_sdl_character_sheet_screen.page_turn_active)
        g_sdl_character_sheet_screen.page_turn_start_ns=SDL_GetTicksNS()-2*SDL_NS_PER_SECOND;
    SDL_Rect canvas={0,0,width,height};sdl_character_sheet_screen_render_canvas(&canvas);
    for(int i=0;i<3&&g_sdl_character_sheet_screen.page_turn_active;i++) {
        g_sdl_character_sheet_screen.page_turn_start_ns=SDL_GetTicksNS()-2*SDL_NS_PER_SECOND;
        sdl_character_sheet_screen_render_canvas(&canvas);
    }
}
static void snapshot(cptr name)
{
    char path[120];strnfmt(path,sizeof(path),"%s-%dx%d-%s.png",name,width,height,config.bigger_font?"big":"normal");
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels&&IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
}
static void geometry(int contents,bool short_title)
{
    cptr title=short_title?"A nearby chronicle":"The Chronicle of the Long Defiance";
    sdl_character_sheet_screen_begin_book(title);
    sdl_character_sheet_screen_set_book_close_button(true);
    cptr labels[]={"I. Statistics","II. Blessings","III. Curses","IV. Difficulty","V. Tales"};
    for(int i=0;i<contents;i++)sdl_character_sheet_screen_add_book_contents(labels[i],5000+i,i);
    sdl_character_sheet_screen_add_book_paragraph("A readable narrative body with a final ENDTAIL marker.");
    sdl_character_sheet_screen_commit_book();frame();
    SDL_FRect title_box={0};
    for(int i=0;i<record_count;i++)if(!strcmp(records[i].text,title))title_box=records[i].box;
    assert(title_box.h>0&&title_box.y+title_box.h<=frame_rect.y+1);
    SDL_FRect font_button=sdl_menu_font_button_bounds();
    if(config.bigger_font&&font_button.w>0)assert(!SDL_HasRectIntersectionFloat(&title_box,&font_button));
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    assert(view.h>=g_sdl_character_sheet_screen.narrative_body_px);
    for(int i=0;i<contents;i++) {
        SDL_FRect hit={0},text={0};
        for(int j=0;j<g_sdl_character_sheet_screen.hit_count;j++)if(g_sdl_character_sheet_screen.hits[j].choice==5000+i)hit=g_sdl_character_sheet_screen.hits[j].rect;
        for(int j=0;j<record_count;j++)if(!strcmp(records[j].text,labels[i]))text=records[j].box;
        assert(hit.w>0&&hit.h>0&&text.w>0&&text.h>0);
        assert(text.h<=hit.h+1&&text.y>=hit.y-1&&text.y+text.h<=hit.y+hit.h+1);
        assert(hit.x>=0&&hit.y>=0&&hit.x+hit.w<=width&&hit.y+hit.h<=height);
        for(int j=0;j<i;j++)for(int k=0;k<g_sdl_character_sheet_screen.hit_count;k++)if(g_sdl_character_sheet_screen.hits[k].choice==5000+j)
            assert(!SDL_HasRectIntersectionFloat(&hit,&g_sdl_character_sheet_screen.hits[k].rect));
    }
    snapshot(contents?"chapters":short_title?"nearby":"no-contents");
    sdl_character_sheet_screen_hide();
}
char __wrap_inkey(void)
{
    frame();
    int tales=sdl_character_sheet_screen_book_contents_page(4);
    if(phase==0) {
        sdl_character_sheet_screen_set_book_page(tales);frame();
        assert(g_sdl_character_sheet_screen.sheet_scroll_max>0);phase=1;
    }
    if(phase==1) {
        if(g_sdl_character_sheet_screen.sheet_scroll<g_sdl_character_sheet_screen.sheet_scroll_max) {
            assert(++down_steps<80);return '3';
        }
        bool tail=false;
        for(int i=0;i<record_count;i++)if(strstr(records[i].text,"ENDTAIL")) {
            SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
            tail=records[i].box.y>=view.y-1&&records[i].box.y+records[i].box.h<=view.y+view.h+1;
        }
        assert(tail);
        snapshot("statistics-tail");before_scroll=g_sdl_character_sheet_screen.sheet_scroll;
        before_page=sdl_character_sheet_screen_select_page();phase=2;return '9';
    }
    if(phase==2) {
        assert(g_sdl_character_sheet_screen.sheet_scroll<before_scroll);
        assert(sdl_character_sheet_screen_select_page()==before_page&&fixture_selected_run==0);
        phase=3;return '2';
    }
    if(phase==3){assert(fixture_selected_run==1);phase=4;return '8';}
    if(phase==4){assert(fixture_selected_run==0);before_page=sdl_character_sheet_screen_select_page();phase=5;return '4';}
    if(phase==5){assert(sdl_character_sheet_screen_select_page()==before_page-1);phase=6;return '6';}
    if(phase!=6||sdl_character_sheet_screen_select_page()!=before_page)fprintf(stderr,"phase%d page%d expected%d turning%d\n",phase,sdl_character_sheet_screen_select_page(),before_page,sdl_character_sheet_screen_page_turning());
    assert(phase==6&&sdl_character_sheet_screen_select_page()==before_page);phase=7;return ESCAPE;
}
int main(int argc,char** argv)
{
    assert(argc==6);setbuf(stdout,NULL);log_set_quiet(true);
    width=atoi(argv[1]);height=atoi(argv[2]);config.bigger_font=false;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());
    sdl_config_set_defaults(&config);g_state.system_scale=1;config.use_unsafe_area=true;
    SDL_strlcpy(config.monospace_font,argv[3],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[4],sizeof(config.story_font));SDL_strlcpy(config.story_font2,argv[5],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Narrative fixture",width,height,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
    maxima limits={0};player_type player={0};player_other options={0};object_type items[INVEN_TOTAL]={0};
    z_info=&limits;p_ptr=&player;op_ptr=&options;inventory=items;fixture_statistics_setup();
    term t;assert(!term_init(&t,80,24,256));Term_activate(&t);term_screen=&t;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){angband_color_table[i][1],angband_color_table[i][2],angband_color_table[i][3],255};
    for(int big=0;big<2;big++) {
        config.bigger_font=big;for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++)config.menu_bigger_font[menu]=big;
        geometry(5,false);geometry(0,false);geometry(0,true);
        phase=down_steps=0;fixture_build_count=0;fixture_selected_run=-1;
        turn=1231;playerturn=123;fixture_statistics_loop();assert(phase==7&&fixture_build_count==3);
        assert(turn==1231&&playerturn==123&&!p_ptr->energy_use);
    }
    printf("Desktop narrative %dx%d: title/Contents/body/hits + real Statistics PageUp/Down/run/chapter/back/free clocks PASS\n",width,height);
    return 0;
}
'''

BUILD_BODY=r'''
{
    fixture_build_count++;if(state->selected_run<0)state->selected_run=0;
    fixture_selected_run=state->selected_run;
    sdl_character_sheet_screen_begin_book("The Chronicle of the Long Defiance");
    sdl_character_sheet_screen_set_book_close_button(true);
    const char* labels[]={"I. Statistics","II. Blessings","III. Curses","IV. Difficulty","V. Tales"};
    for(int i=0;i<5;i++) {
        sdl_character_sheet_screen_add_book_contents(labels[i],STORY_BOOK_PAGE_BASE+i,i);
        if(i)sdl_character_sheet_screen_break_book_page();
        sdl_character_sheet_screen_add_book_paragraph(labels[i]);
        if(i==4) {
            sdl_character_sheet_screen_add_book_action("Tale 1",STORY_BOOK_RUN_BASE);
            char text[1800]="";
            for(int n=0;n<8;n++)SDL_strlcat(text,"A long selected tale detail retains its readable totals, history, blessings and curses. ",sizeof(text));
            SDL_strlcat(text,"ENDTAIL",sizeof(text));sdl_character_sheet_screen_add_book_paragraph(text);
        }
    }
    sdl_character_sheet_screen_commit_book();sdl_character_sheet_screen_set_book_page(restore_page);
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scroll-baseline',action='store_true');parser.add_argument('--layout-baseline',action='store_true')
    parser.add_argument('--contents-baseline',action='store_true');parser.add_argument('--title-baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    screens=(ROOT/'src/sdl/ui/sdl-screens.c').read_text()
    if args.layout_baseline or args.contents_baseline:
        start=screens.index('static bool sdl_char_sheet_inline_contents(');end=brace_end(screens,start)
        screens=screens[:start]+screens[start:end].split('{',1)[0]+'{return g_sdl_narrative_portrait_rendering;}'+screens[end:]
    if args.layout_baseline or args.title_baseline:
        start=screens.index('#else\n        if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_NARRATIVE)\n        {\n            int measured_w = 0, measured_h = 0;')
        end=screens.index('#endif\n        cptr display_suffix',start)
        screens=screens[:start]+'#else\n'+screens[end:]
    for signature in ['static SDL_FRect sdl_char_sheet_draw_text_aligned(','static SDL_FRect sdl_char_sheet_draw_text_alpha(']:
        start=screens.index(signature);end=brace_end(screens,start)
        body=screens[start:end].replace('SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);','record(font,text,dst,max_w);SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);')
        screens=screens[:start]+body+screens[end:]
    start=screens.index('void sdl_char_sheet_draw_page_frame(');opening=screens.index('{',start)+1
    screens=screens[:opening]+'\n    frame_rect=(SDL_FRect){px,py,pw,ph};'+screens[opening:]
    (OUT/'check.c').write_text(HARNESS.replace('/* SCREENS */',screens),encoding='utf-8')
    stats=(ROOT/'src/metarun/metarun-stats-ui.c').read_text()
    stats=stats.replace('#include "metarun-internal.h"','#include "metarun/metarun-internal.h"')
    start=stats.index('static void story_book_sdl_build(');opening=stats.index('{',start);end=brace_end(stats,start)
    stats=stats[:start]+'int fixture_build_count,fixture_selected_run;\n'+stats[start:opening]+BUILD_BODY+stats[end:]
    if args.scroll_baseline:
        old="else if (key == '9' || key == '3')";assert stats.count(old)==1;stats=stats.replace(old,'else if (false)')
    (OUT/'stats.c').write_text(stats+'\nvoid fixture_statistics_loop(void){story_book_show_sdl(false);}\nvoid fixture_statistics_setup(void){current_run=0;metarun_max=2;}\n',encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    exclude=('/src/main.c.obj','/src/sdl/ui/sdl-screens.c.obj','/src/metarun/metarun-stats-ui.c.obj')
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(exclude)),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';wraps=('inkey','screen_save','screen_load','screen_push_supporting_panes_hidden','screen_pop_supporting_panes_hidden','message_flush','handle_stuff','sdl_get_layout_screen_rect','metarun_tale_management_available')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'stats.c'),'@'+str(OUT/'objects.rsp'),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    for w,h in [(768,576),(1280,900)]:
        subprocess.run([str(exe),str(w),str(h),str(ROOT/'lib/xtra/font/VictorMono-Medium.ttf'),str(ROOT/'lib/xtra/font/Cinzel-Medium.ttf'),str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf')],cwd=OUT,env=env,check=True,timeout=30)

if __name__=='__main__':main()
