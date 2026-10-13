#!/usr/bin/env python3
"""Exercise real SDL dispatch and description footer actions without a save.

Only the description anchor is fixed by the fixture; action labels, hit testing,
touch labels, keyboard translation and controller dispatch are production code.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/description-controller-actions'

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "object/object-info.h"
#include "sdl/input/sdl-gamepad.c"
void fixture_footer(cptr footer,const object_info_screen_action* actions,int count);
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect* rect)
{ (void)pane;*rect=(SDL_Rect){0,0,1024,768};return true; }
static void expect(int key)
{
    char ch=0;int result=Term_inkey(&ch,false,true);
    if(key) { if(result || ch!=key)fprintf(stderr,"expected %d got %d result %d\n",key,ch,result);assert(!result&&ch==key); }
    else assert(result);
}
static void button(int id)
{
    SDL_Event ev={0};ev.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    ev.gbutton.button=id;ev.gbutton.down=true;sdl_handle_event(&g_state,&ev);
    ev.type=SDL_EVENT_GAMEPAD_BUTTON_UP;ev.gbutton.down=false;sdl_handle_event(&g_state,&ev);
}
static void pointer_action(cptr token,int key)
{
    description_overlay_layout layout;assert(sdl_description_overlay_layout(&layout));
    cptr text=sdl_description_overlay_footer_text(&g_description_overlay);
    cptr pos=strstr(text,token);assert(pos);
    TTF_Font* font=sdl_story_font_for_height_slot(layout.cell_h,STORY_FONT_SLOT_SECONDARY);
    float advance=(float)(pos-text)*layout.cell_w;
    assert(!font); /* Exercise production monospace fallback hit testing. */
    float x=layout.text_x+advance+1,y=layout.footer_y+layout.cell_h/2;
    assert(sdl_description_overlay_footer_action_at(x,y)==key);
    assert(sdl_description_overlay_handle_footer_pointer(x,y));expect(key);expect(0);
}
static void checks(void)
{
    static byte attrs[80*100];static char chars[80*100];
    memset(chars,' ',sizeof(chars));
    sdl_config_set_defaults(&config);config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;g_gamepad_state.pad_count=1;
    character_icky=1;character_dungeon=true;character_generated=true;
    g_description_overlay=(description_overlay_state){.active=true,.interactive=true,
        .attrs=attrs,.chars=chars,.width=80,.height=100,.target_cols=80};
    long old_turn=turn;long old_player_turn=playerturn;
    const object_info_screen_action all[]={{'x',"x Equip..."},{' ',"Space Pick Up..."},{ESCAPE,"Esc Close"}};
    const int remaps[]={GAMEPAD_BIND_NONE,GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_CTRL,GAMEPAD_BIND_ALT,'z'};
    for(int mask=0;mask<4;mask++) {
        object_info_screen_action actions[3];int count=0;char footer[160]="";
        for(int i=0;i<2;i++)if(mask&(1<<i)) {actions[count++]=all[i];SDL_strlcat(footer,all[i].token,sizeof(footer));SDL_strlcat(footer,"  ",sizeof(footer));}
        actions[count++]=all[2];SDL_strlcat(footer,all[2].token,sizeof(footer));
        fixture_footer(footer,actions,count);
        cptr displayed=sdl_description_overlay_footer_text(&g_description_overlay);
        assert(strstr(displayed,"<B> Close")&&!strstr(displayed,"Esc")&&!strstr(displayed,"Space"));
        assert((strstr(displayed,"<X> Equip...")!=NULL)==!!(mask&1));
        assert((strstr(displayed,"<A> Pick Up...")!=NULL)==!!(mask&2));
        for(unsigned r=0;r<N_ELEMENTS(remaps);r++)for(int mod=0;mod<4;mod++) {
            config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH]=remaps[r];
            config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_WEST]=remaps[r];
            config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST]=remaps[r];
            sdl_gamepad_reset_modifiers();
            const int mods[]={GAMEPAD_BIND_NONE,GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_CTRL,GAMEPAD_BIND_ALT};
            if(mod)sdl_gamepad_apply_modifier(mods[mod],true);
            button(SDL_GAMEPAD_BUTTON_SOUTH);expect(mask&2?' ':'\r');expect(0);
            button(SDL_GAMEPAD_BUTTON_WEST);expect('x');expect(0);
            button(SDL_GAMEPAD_BUTTON_EAST);expect(ESCAPE);expect(0);
            if(mod)sdl_gamepad_apply_modifier(mods[mod],false);
        }
        char label[64];
        if(mask&1){assert(sdl_description_overlay_footer_action_label('x',label,sizeof(label))&&!strcmp(label,"Equip..."));pointer_action("<X> Equip...",'x');}
        if(mask&2){assert(sdl_description_overlay_footer_action_label(' ',label,sizeof(label))&&!strcmp(label,"Pick Up..."));pointer_action("<A> Pick Up...",' ');}
        assert(sdl_description_overlay_footer_action_label(ESCAPE,label,sizeof(label))&&!strcmp(label,"Close"));pointer_action("<B> Close",ESCAPE);
        /* Keyboard Return retains scrolling even with explicit pickup. */
        SDL_Event key;memset(&key,0,sizeof(key));key.type=SDL_EVENT_KEY_DOWN;key.key.down=true;key.key.key=SDLK_RETURN;key.key.scancode=SDL_SCANCODE_RETURN;key.key.mod=SDL_KMOD_NONE;
        sdl_handle_event(&g_state,&key);expect('\r');expect(0);
        config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
        fixture_footer(footer,actions,count);
        assert(!strcmp(sdl_description_overlay_footer_text(&g_description_overlay),footer));
        for(int i=0;i<count;i++)pointer_action(actions[i].token,actions[i].key);
        config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    }
    fixture_footer(NULL,NULL,0);button(SDL_GAMEPAD_BUTTON_SOUTH);expect('\r');expect(0);
    assert(strstr(sdl_description_overlay_footer_text(&g_description_overlay),"<B> Close"));
    /* A stale footer cannot turn a different modal's confirm into pickup. */
    fixture_footer("Space Pick Up",&all[1],1);
    g_description_overlay.interactive=false;button(SDL_GAMEPAD_BUTTON_SOUTH);expect('\r');
    g_description_overlay.interactive=true;g_description_overlay.active=false;
    button(SDL_GAMEPAD_BUTTON_SOUTH);expect('\r');expect(0);
    assert(turn==old_turn&&playerturn==old_player_turn);
    puts("Description real SDL dispatch: all action combinations/remaps/modifiers, keyboard Return, footer/touch labels and pointer semantic IDs PASS");
}
int main(void)
{
    setbuf(stdout,NULL);log_set_quiet(true);assert(SDL_Init(SDL_INIT_EVENTS));assert(TTF_Init());
    term t;assert(!term_init(&t,80,24,256));Term_activate(&t);term_screen=&t;
    checks();term_nuke(&t);TTF_Quit();SDL_Quit();return 0;
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true',help='Restore only modal physical A Return dispatch')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    gamepad=(ROOT/'src/sdl/input/sdl-gamepad.c').read_text()
    if args.baseline:
        old="""Term_keypress(g_description_overlay.active
                && g_description_overlay.interactive
                && sdl_description_overlay_has_footer_action(' ')
            ? ' ' : '\\r');"""
        assert gamepad.count(old)==1
        gamepad=gamepad.replace(old,"Term_keypress('\\r');")
    (OUT/'gamepad.c').write_text(gamepad,encoding='utf-8')
    (OUT/'check.c').write_text(HARNESS.replace('#include "sdl/input/sdl-gamepad.c"','#include "gamepad.c"'),encoding='utf-8')
    info=(ROOT/'src/object/object-info.c').read_text()
    (OUT/'info.c').write_text(info+'\nvoid fixture_footer(cptr f,const object_info_screen_action* a,int n){object_info_configure_footer(f,a,n);}\n',encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir'
    objects=shlex.split((cmake/'objects1.rsp').read_text())
    exclude=('/src/main.c.obj','/src/sdl/input/sdl-gamepad.c.obj','/src/object/object-info.c.obj','/src/sdl/input/sdl-tooltips.c.obj','/src/sdl/core/sdl-events.c.obj')
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(exclude)),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([
        *(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'info.c'),str(ROOT/'src/sdl/input/sdl-tooltips.c'),str(ROOT/'src/sdl/core/sdl-events.c'),
        '@'+str(OUT/'objects.rsp'),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        '-Wl,--wrap=sdl_overlay_pane_anchor_rect','-o',str(exe)],cwd=BUILD,env=env,check=True)
    subprocess.run([str(exe)],cwd=OUT,env=env,check=True,timeout=20)

if __name__=='__main__':main()
