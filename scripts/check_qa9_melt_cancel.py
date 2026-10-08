#!/usr/bin/env python3
"""Exercise real smithing/Melt menus without losing cancelled pending work.

Only input and unrelated redraw/tutorial boundaries are wrapped. --baseline
restores the old reset placement while retaining the current save APIs.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-melt-cancel"
SMITH = r"""
#include <assert.h>
void fixture_pending_alloy(void)
{
    smith_clear_alloy_state(&smith_alloy);
    object_prep(smith_o_ptr,lookup_kind(TV_MAIL,4));
    smith_o_ptr->number=1;
    assert(smith_apply_alloy(smith_o_ptr,&smith_alloy,SMITH_ALLOY_MITHRIL));
    p_ptr->smithing=0;p_ptr->smithing_leftover=19;
}
int fixture_alloy(void) { return smith_alloy.type; }
"""
CHECKS = r"""
#include "supplies.h"
#include "sdl-config.h"
#undef assert
#define assert(expr) do { if(!(expr)) { fprintf(stderr,"%s: %s line %d\n",scenario,#expr,__LINE__); exit(1); } } while(0)
static const char* scenario="initialization";
static cptr keys;
extern void fixture_pending_alloy(void);
extern int fixture_alloy(void);
extern int forge_uses(int,int);
extern int mithril_carried(void);
extern int star_iron_carried(void);
char __wrap_inkey(void)
{
    if(inkey_scan) { inkey_scan=false;return 0; }
    assert(keys && *keys);return *keys++;
}
void __wrap_message_flush(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
void __wrap_handle_stuff(void)
{ p_ptr->update=0;p_ptr->redraw=0;p_ptr->window=0; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action;(void)item;return true; }
static void prepare(bool metal)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(1);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    p_ptr->py=p_ptr->px=5;p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->food=PY_FOOD_FULL;p_ptr->new_exp=p_ptr->exp=100000;
    p_ptr->skill_base[S_SMT]=100;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=p_ptr->stat_use[i]=20;
    for(int i=0;i<ABILITIES_MAX;i++)p_ptr->active_ability[S_SMT][i]=true;
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE]=false;
    cave_m_idx[5][5]=-1;cave_feat[5][5]=FEAT_FORGE_NORMAL_HEAD+3;
    character_dungeon=character_generated=true;character_icky=0;inkey_flag=true;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    object_prep(&inventory[0],lookup_kind(TV_METAL,SV_METAL_MITHRIL));inventory[0].number=99;
    object_prep(&inventory[1],inventory[0].k_idx);inventory[1].number=86;
    if(metal) {
        object_prep(&inventory[2],lookup_kind(TV_SWORD,30));
        inventory[2].weight=60;inventory[2].number=1;
        inventory[2].obj_note=quark_add("melt fixture source");
    }
    fixture_pending_alloy();
    turn=1001;playerturn=100;p_ptr->energy_use=0;
}
static void run(cptr name,cptr input,bool metal,bool confirmed)
{
    scenario=name;prepare(metal);keys=input;
    object_type work=*smith_o_ptr;
    object_type source=inventory[2];
    int alloy=fixture_alloy();
    do_cmd_smithing_screen();
    assert(!*keys && !p_ptr->smithing);
    assert(turn==1001 && playerturn==100 && !p_ptr->energy_use);
    assert(forge_uses(5,5)==3 && p_ptr->new_exp==100000);
    for(int i=0;i<A_MAX;i++)assert(p_ptr->stat_base[i]==20);
    assert(mithril_carried()==185);
    if(confirmed) {
        assert(!p_ptr->smithing_leftover && !smith_o_ptr->k_idx);
        assert(star_iron_carried()==60);
        for(int i=0;i<player_pack_entry_count();i++)
            assert(player_pack_entry_at(i)->tval!=TV_SWORD);
    } else {
        assert(p_ptr->smithing_leftover==19);
        assert(!memcmp(smith_o_ptr,&work,sizeof(work)) && fixture_alloy()==alloy);
        assert(!star_iron_carried());
        assert(!memcmp(&inventory[2],&source,sizeof(source)));
    }
}
static void checks(void)
{
    set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
    run("picker Escape","e\033\033",true,false);
    run("picker Back","e4\033",true,false);
    run("confirmation No","ean\033\033",true,false);
    run("confirmation Escape","ea\033\033\033",true,false);
    run("no suitable items","e\033",false,false);
    run("confirmed conversion","eay\033",true,true);
    puts("Melt menus: Escape/Back/No/no-source retain pending alloy work; confirmed conversion preserves metal totals, stat/forge/clock rules PASS.");
}
"""

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline",action="store_true")
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    production=ROOT/"src/cmd/ui/cmd-ui-smithing.c"
    if args.baseline:
        old=production.read_text(encoding="utf-8")
        reset=("            /* Browsing or declining a melt must retain interrupted work.\n"
               "             * Only the confirmed conversion abandons that work. */\n"
               "            p_ptr->smithing_leftover = 0;\n\n")
        menu="            if (meltable_metal_items_carried())\n            {\n                melt_menu();"
        assert old.count(reset)==1 and old.count(menu)==1
        old=old.replace(reset,"").replace(menu,
            "            if (meltable_metal_items_carried())\n            {\n"
            "                p_ptr->smithing_leftover = 0;\n                melt_menu();")
        production=OUT/"baseline-smithing.c"
        production.write_text(old,encoding="utf-8")
    smith=OUT/"smithing.c"
    smith.write_text('#include "'+production.as_posix()+'"\n'+SMITH,encoding="utf-8")
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/"check.c"
    source.write_text(prefix+CHECKS+init+'    checks();SDL_Quit();return 0;\n}\n',encoding="utf-8")
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-smithing.c.obj'))),encoding="utf-8")
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(smith),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in ('inkey','message_flush','msg_print','handle_stuff','tutorial_game_action_allowed')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)

if __name__=='__main__':main()
