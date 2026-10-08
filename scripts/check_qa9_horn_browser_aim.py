#!/usr/bin/env python3
"""Run the real Inventory/Horn/aim path and native-menu payment boundary.

Default uses built objects. --source privately compiles the current browser;
--baseline uses its committed version and must fail the map-ownership check.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_game_control import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-horn-browser-aim'

CHECKS = r'''
#include "ui/menu-click.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "support/movement-input.h"
#undef assert
#define assert(e) do { if (!(e)) { log_error("Assertion failed: %s line %d",#e,__LINE__); _Exit(1); } } while (0)
static bool in_aim, accept_play, cancel_aim, pointer_select, direct;
static int browser_inputs, aim_inputs, aim_calls, effects, noise_calls, truce_calls;
static int batch_depth;
static const char* marker="QA9 GAMEPLAY FRAME";
extern void __real_sdl_present_batch_begin(void);
extern void __real_sdl_present_batch_end(void);
void __wrap_sdl_present_batch_begin(void)
{ ++batch_depth; __real_sdl_present_batch_begin(); }
void __wrap_sdl_present_batch_end(void)
{ assert(batch_depth>0); --batch_depth; __real_sdl_present_batch_end(); }
static void assert_gameplay_ownership(void)
{
    assert(character_icky==0);
    assert(!screen_supporting_panes_hidden_active());
    assert(g_terminal_menu_scale_depth==0);
    assert(g_description_overlay_main_anchor_depth==0);
    assert(g_description_overlay_full_main_anchor_depth==0);
    assert(batch_depth==0);
    for (int x=0;marker[x];x++) {
        byte attr; char c;
        assert(Term_what(x,2,&attr,&c)==0 && c==marker[x]);
    }
}
extern void __real_sdl_pointer_aim_select_begin(int,bool);
extern void __real_sdl_pointer_aim_select_end(void);
void __wrap_sdl_pointer_aim_select_begin(int range,bool vertical)
{
    /* This boundary is entered by real get_aim_dir / get_aim_dir_vertical,
     * before the real cursor/prompt loop reads its first key. */
    assert_gameplay_ownership();
    ++aim_calls; in_aim=true;
    __real_sdl_pointer_aim_select_begin(range,vertical);
}
void __wrap_sdl_pointer_aim_select_end(void)
{ __real_sdl_pointer_aim_select_end(); in_aim=false; }
static char next_aim_key(void)
{
    assert(in_aim && ++aim_inputs<=2);
    if (cancel_aim) return ESCAPE;
    return aim_inputs==1 ? '6' : 'f';
}
extern char __real_inkey_movement_context(u16b context);
char __wrap_inkey_movement_context(u16b context)
{
    /* Its internal inkey call shares input.c and is not linker-wrapped.
     * Deliver fresh input, then exercise the real context owner and reader. */
    u16b previous=movement_input_active_context();
    assert(context==MOVEMENT_INPUT_CONTEXT_TARGETING);
    inkey_xtra=false;
    Term_keypress(next_aim_key());
    char key=__real_inkey_movement_context(context);
    assert(movement_input_active_context()==previous);
    return key;
}
char __wrap_inkey(void)
{
    inkey_flag=false; /* Nested modal inkey cleanup, restored by native caller. */
    if (in_aim) {
        return next_aim_key();
    }
    assert(!direct && ++browser_inputs<=2);
    if (browser_inputs==2) { assert(!accept_play); return ESCAPE; }
    if (pointer_select) {
        bool wake=false;
        assert(ui_menu_click_handle_choice_action(SUPPLY_CLICK_ENTRY_BASE,
            UI_MENU_CLICK_PRIMARY,&wake));
        return UI_MENU_CLICK_WAKE_KEY;
    }
    return 'a';
}
bool __wrap_get_check(cptr prompt)
{ assert(strstr(prompt,"Play ")); return accept_play; }
void __wrap_handle_stuff(void) {} /* Keep the fixture's gameplay sentinel frame. */
void __wrap_message_flush(void) {}
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action; (void)item; return true; }
void __wrap_tutorial_game_action_done(const char* action,const object_type* item)
{ (void)action; (void)item; }
void __wrap_monster_perception(bool centered,bool main_roll,int difficulty)
{ (void)centered; (void)main_roll; (void)difficulty; ++noise_calls; }
void __wrap_break_truce(bool obvious) { (void)obvious; ++truce_calls; }
bool __wrap_fire_arc(int typ,int dir,int dd,int ds,int dif,int rad,int degrees)
{
    (void)typ;(void)dd;(void)ds;(void)dif;(void)rad;(void)degrees;
    assert(dir==5 && p_ptr->target_col==p_ptr->px+1);
    ++effects;
    /* Typeahead generated during a projection must remain behind the paid
     * native-menu wake. Projection damage itself is outside this UI test. */
    if (!direct) Term_keypress(KTRL('Q'));
    return false;
}
static void run(int sval,bool channel,bool reject,bool cancel,bool pointer,
    bool direct_action,int voice)
{
    log_debug("qa9 case horn=%d channel=%d reject=%d cancel=%d pointer=%d direct=%d voice=%d",sval,channel,reject,cancel,pointer,direct_action,voice);
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_quiver_reset_store(); player_carried_extra_reset_store();
    supplies_reset_store(); player_pack_action_reset();
    for (int y=0;y<20;y++) for (int x=0;x<20;x++) {
        cave_feat[y][x]=FEAT_FLOOR;
        cave_info[y][x]=CAVE_MARK|CAVE_VIEW|CAVE_SEEN|CAVE_GLOW|CAVE_FIRE;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    mon_max=1; o_max=1;
    p_ptr->py=p_ptr->px=5; p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    cave_m_idx[5][5]=-1; p_ptr->playing=true;
    p_ptr->chp=p_ptr->mhp=100; p_ptr->csp=voice; p_ptr->msp=41;
    p_ptr->skill_use[S_WIL]=10; p_ptr->previous_action[0]=7;
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=channel;
    p_ptr->inven_cnt=1;
    object_prep(&inventory[0],lookup_kind(TV_HORN,sval));
    inventory[0].number=1; object_aware(&inventory[0]); object_known(&inventory[0]);
    k_info[inventory[0].k_idx].tried=false;
    character_dungeon=true; character_generated=false; character_icky=0;
    accept_play=!reject; cancel_aim=cancel; pointer_select=pointer; direct=direct_action;
    browser_inputs=aim_inputs=aim_calls=effects=noise_calls=truce_calls=0;
    in_aim=false; assert(batch_depth==0);
    Term_flush(); inkey_next_set(NULL); Term_clear();
    Term_putstr(0,2,-1,TERM_WHITE,marker); Term_fresh();
    inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    byte tester_tval=item_tester_tval;
    bool tester_full=item_tester_full;
    bool (*tester_hook)(const object_type*)=item_tester_hook;
    u64b rng=Rand_state_export();
    if (direct) do_cmd_play_instrument(&inventory[0],0);
    else sdl_main_menu_overlay_choose(MAIN_MENU_INVENTORY);
    bool paid=!reject && !cancel && voice>=(channel?10:20);
    assert(aim_calls==(!reject && voice>=(channel?10:20)));
    assert(p_ptr->energy_use==(paid?100:0));
    assert(p_ptr->csp==voice-(paid?(channel?10:20):0));
    assert(inventory[0].number==1 && !player_pack_action_pending());
    assert(effects==paid && noise_calls==paid && truce_calls==paid);
    assert(k_info[inventory[0].k_idx].tried==paid);
    assert(item_tester_tval==tester_tval && item_tester_full==tester_full
        && item_tester_hook==tester_hook);
    if (!paid) assert(Rand_state_export()==rng);
    assert(p_ptr->previous_action[0]==(paid?ACTION_MISC:7));
    assert(!in_aim && !g_pointer_aim.active && !g_pointer_aim.select_mode && temp_n==0);
    assert(movement_input_active_context()==MOVEMENT_INPUT_CONTEXT_DUNGEON);
    assert_gameplay_ownership();
    if (!direct) {
        char key=0;
        assert(inkey_flag);
        if (paid) {
            assert(Term_inkey(&key,false,true)==0 && key==UI_MENU_CLICK_WAKE_KEY);
            assert(Term_inkey(&key,false,true)==0 && key==KTRL('Q'));
        }
        assert(Term_inkey(&key,false,true)!=0);
    }
    character_dungeon=false;
}
static void checks(void)
{
    set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
    set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
    run(SV_HORN_WARNING,false,true,false,false,false,41);
    run(SV_HORN_WARNING,false,false,true,false,false,41);
    run(SV_HORN_WARNING,false,false,false,false,false,41);
    run(SV_HORN_WARNING,true,false,false,true,false,41);
    run(SV_HORN_WARNING,true,false,false,false,false,9);
    run(SV_HORN_BLASTING,false,false,true,false,false,41);
    run(SV_HORN_WARNING,false,false,true,false,true,41);
    run(SV_HORN_WARNING,false,false,false,false,true,41);
    puts("Horn browser aim: real browser/activation/aim restore gameplay ownership; cancellation, Voice, Channeling, pointer, wake priority and direct-use parity PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--source', action='store_true')
    mode.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index('static cptr make_path')]
    init = HARNESS[HARNESS.index('static cptr make_path'):HARNESS.index('    int last = ')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init + '    checks(); sdl_quit_hook(NULL); return 0;\n}\n')
    browser = None
    if args.source:
        browser = ROOT / 'src/cmd/ui/cmd-ui-knowledge.c'
    elif args.baseline:
        browser = OUT / 'browser-baseline.c'
        browser.write_bytes(subprocess.check_output(
            ['git','show','HEAD:src/cmd/ui/cmd-ui-knowledge.c'], cwd=ROOT))
    cmake = BUILD / 'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake / 'objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects
        if not p.endswith('/src/main.c.obj') and
        (browser is None or not p.endswith('/src/cmd/ui/cmd-ui-knowledge.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in
        ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    env.update(SDL_VIDEO_DRIVER='dummy',SDL_RENDER_DRIVER='software',
        SDL_AUDIO_DRIVER='dummy',CONTROL_TEST_ASSETS=str(ROOT/'lib'))
    wrapped = ('inkey','inkey_movement_context','get_check','handle_stuff','message_flush','fire_arc',
        'monster_perception','break_truce','sdl_present_batch_begin',
        'sdl_present_batch_end','sdl_pointer_aim_select_begin','sdl_pointer_aim_select_end',
        'tutorial_game_action_allowed','tutorial_game_action_done')
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),
        *([str(browser)] if browser else []),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *('-Wl,--wrap='+name for name in wrapped),'-o',str(exe)],
        cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as profile:
        subprocess.run([str(exe),'--windowed','--tiles'],cwd=ROOT,
            env=dict(env,CONTROL_TEST_PROFILE=profile),check=True,timeout=30)


if __name__ == '__main__':
    main()
