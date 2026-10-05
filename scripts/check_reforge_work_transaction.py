#!/usr/bin/env python3
"""Exercise timed Reforge commands, paid work and saved pending transaction.

Uses production smithing/process_player and the actual player/object codecs in
an initialized engine. Only input/UI and unrelated redraw boundaries are wrapped.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import argparse
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/reforge-work-transaction'

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
bool fixture_reforge(void) { return smith_reforge_item(); }
bool fixture_begin(int slot,int prefix) { return smith_begin_reforge(slot,prefix); }
int fixture_menu_mask(void)
{
    bool valid[SMT_MENU_MAX];byte attr[SMT_MENU_MAX];char labels[SMT_MENU_MAX][32];
    smith_root_build_entries(valid,attr,labels);int result=0;
    for(int i=0;i<SMT_MENU_MAX;i++)if(valid[i])result|=1<<i;
    return result;
}
void fixture_normal_pending(void)
{
    object_prep(smith_o_ptr,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    smith_o_ptr->unused1=0;smith_o_ptr->number=1;
    (void)object_difficulty(smith_o_ptr);
    p_ptr->smithing_leftover=1;p_ptr->smithing=0;
}
int fixture_reforge_plan(int item,int *prefix,int *turns,int *dex)
{
    object_type *source=player_inventory_object(item);
    int count=0;
    for(int i=1;i<z_info->e_max && count<26;i++) {
        reforge_preview_type preview;
        if(!ego_prefix_can_apply_to_object(source,i)
            || !reforge_preview_build(source,i,&preview)) continue;
        if(preview.affordable) {
            *prefix=i; *turns=preview.turns; *dex=preview.cost.dex; return count;
        }
        count++;
    }
    return -1;
}
'''
CHECKS = r'''
#undef assert
#include <setjmp.h>
#define assert(expr) do { if(!(expr)) { fprintf(stderr,"%s: %s line %d\n",scenario,#expr,__LINE__); exit(1); } } while(0)
static const char *scenario="initialization";
static cptr keys;
static int selected_item,requests;
extern void process_player(void);
extern bool fixture_reforge(void);
extern bool fixture_begin(int,int);
extern int fixture_menu_mask(void);
extern void fixture_normal_pending(void);
extern int fixture_reforge_plan(int,int*,int*,int*);
extern int forge_uses(int,int);
extern size_t fixture_write_work(byte*,size_t);
extern int fixture_read_work(const byte*,size_t,int);
char __wrap_inkey(void)
{
    if(inkey_scan) { inkey_scan=false;return 0; }
    assert(keys && *keys);return *keys++;
}
bool __wrap_open_inventory_item_select_menu(int mode,cptr prompt,cptr failure,int *item)
{ (void)mode;(void)prompt;(void)failure;*item=selected_item;return true; }
void __wrap_handle_stuff(void) { p_ptr->update=0;p_ptr->redraw=0;p_ptr->window=0; }
void __wrap_msg_print(cptr message) { (void)message; }
void __wrap_request_command(void)
{
    requests++;fprintf(stderr,"Unexpected input: work=%d leftover=%d turn=%ld\n",p_ptr->smithing,p_ptr->smithing_leftover,(long)playerturn);assert(false);
}
void __wrap_process_command(void) { assert(false); }
bool __wrap_tutorial_game_action_allowed(const char *action,const object_type *item)
{ (void)action;(void)item;return true; }
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
static jmp_buf generation_boundary;
void __wrap_level_gen_screen_begin(void) { }
byte __wrap_run_quest_initiated_count(void)
{ longjmp(generation_boundary,1);return 0; }
static void replace_level_prefix(void)
{
    if(!setjmp(generation_boundary))generate_cave();
    character_dungeon=true;
}
static void prepare(int uses)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(1);
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));object_wipe(smith_o_ptr);
    p_ptr->py=p_ptr->px=5;p_ptr->chp=p_ptr->mhp=100;p_ptr->playing=true;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->food=PY_FOOD_FULL-1000;p_ptr->new_exp=p_ptr->exp=100000;p_ptr->skill_base[S_SMT]=100;
    for(int a=0;a<A_MAX;a++)p_ptr->stat_base[a]=20;
    for(int a=0;a<ABILITIES_MAX;a++)p_ptr->active_ability[S_SMT][a]=true;
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE]=false;
    cave_m_idx[5][5]=-1;cave_feat[5][5]=FEAT_FORGE_NORMAL_HEAD+uses;
    character_dungeon=character_generated=true;playerturn=100;requests=0;
    object_type source;object_prep(&source,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    source.number=1;source.discount=17;source.ident|=IDENT_KNOWN;
    source.obj_note=quark_add("preserved reforge inscription");
    assert(player_carried_extra_load(&source));selected_item=CARRIED_EXTRA_INDEX;
}
static int source_count(void)
{
    int count=0;
    for(int i=0;i<INVEN_TOTAL;i++)if(inventory[i].k_idx
        && inventory[i].tval==TV_SWORD && inventory[i].sval==SV_SHORT_SWORD)
        count+=inventory[i].number;
    for(int i=0;i<player_carried_extra_entry_count();i++) {
        object_type *o=player_inventory_object(CARRIED_EXTRA_INDEX+i);
        if(o && o->k_idx && o->tval==TV_SWORD && o->sval==SV_SHORT_SWORD)count+=o->number;
    }
    return count;
}
static object_type *finished(void)
{
    for(int i=0;i<INVEN_TOTAL;i++)if(inventory[i].k_idx && inventory[i].unused1==2)return &inventory[i];
    for(int i=0;i<player_carried_extra_entry_count();i++) {
        object_type *o=player_inventory_object(CARRIED_EXTRA_INDEX+i);
        if(o && o->k_idx && o->unused1==2)return o;
    }
    return NULL;
}
static void work_one(void)
{
    int left=p_ptr->smithing_leftover;long before=playerturn;
    p_ptr->energy=100;process_player();
    assert(p_ptr->energy_use==100 && playerturn==before+1 && !requests);
    assert(p_ptr->smithing_leftover==left-1);
}
static void checks(void)
{
    int prefix,turns,dex_cost;
    scenario="free prefix cancellation";prepare(1);
    int choice=fixture_reforge_plan(selected_item,&prefix,&turns,&dex_cost);assert(choice>=0);
    object_wipe(smith_o_ptr);keys="\033";
    assert(!fixture_reforge() && !*keys && !p_ptr->smithing_leftover);
    assert(source_count()==1 && !p_ptr->energy_use && playerturn==100);
    assert(forge_uses(5,5)==1 && p_ptr->stat_drain[A_DEX]==0);
    scenario="accept exact advertised work";
    char input[64];int n=0;input[n++]='f';for(int i=0;i<choice;i++)input[n++]='2';input[n++]='\r';input[n]=0;
    keys=input;do_cmd_smithing_screen();assert(!*keys);
    assert(p_ptr->smithing==turns);
    assert(p_ptr->smithing_leftover==turns && turns>=10);
    assert(smith_o_ptr->unused1==2 && smith_o_ptr->number==1 && smith_o_ptr->discount==17);
    assert(streq(quark_str(smith_o_ptr->obj_note),"preserved reforge inscription"));
    assert(object_ego_prefix(smith_o_ptr)==prefix && source_count()==0);
    assert(forge_uses(5,5)==0 && p_ptr->stat_drain[A_DEX]==-dex_cost);
    byte drain[sizeof(p_ptr->stat_drain)];memcpy(drain,p_ptr->stat_drain,sizeof(drain));
    /* Identifying the finished affix may legitimately grant experience. */
    long exp=p_ptr->new_exp-p_ptr->ident_exp;int smt=p_ptr->skill_base[S_SMT];
    for(int i=0;i<3;i++)work_one();
    scenario="interrupt/save/reload keeps paid source";disturb(0,0);
    assert(!p_ptr->smithing && p_ptr->smithing_leftover==turns-3);
    byte buffer[65536];size_t length=fixture_write_work(buffer,sizeof(buffer));
    object_wipe(smith_o_ptr);p_ptr->smithing_leftover=0;p_ptr->stat_drain[A_DEX]=0;
    assert(!fixture_read_work(buffer,length,VERSION_EXTRA));
    assert(p_ptr->smithing_leftover==turns-3 && smith_o_ptr->unused1==2);
    assert(object_ego_prefix(smith_o_ptr)==prefix && smith_o_ptr->discount==17);
    assert(streq(quark_str(smith_o_ptr->obj_note),"preserved reforge inscription"));
    assert(!memcmp(drain,p_ptr->stat_drain,sizeof(drain)) && p_ptr->new_exp-p_ptr->ident_exp==exp);
    scenario="level replacement conserves withdrawn paid work";
    object_type conserved=*smith_o_ptr;p_ptr->smithing=p_ptr->smithing_leftover;
    replace_level_prefix();
    assert(!p_ptr->smithing && p_ptr->smithing_leftover==turns-3);
    assert(!memcmp(&conserved,smith_o_ptr,sizeof(conserved)));
    scenario="pending work cannot be replaced";
    assert(fixture_menu_mask()==(1<<6));
    cave_feat[5][5]=FEAT_FLOOR;assert(fixture_menu_mask()==0);
    cave_feat[5][5]=FEAT_FORGE_NORMAL_HEAD;
    object_type pending=*smith_o_ptr;keys="abcdef\033";do_cmd_smithing_screen();
    assert(!*keys && p_ptr->smithing_leftover==turns-3);
    assert(!memcmp(&pending,smith_o_ptr,sizeof(pending)) && source_count()==0);
    scenario="resume exhausted forge";keys="\r";do_cmd_smithing_screen();
    assert(!*keys && p_ptr->smithing==turns-3);
    for(int i=3;i<turns;i++)work_one();
    assert(!p_ptr->smithing && !p_ptr->smithing_leftover && playerturn==100+turns);
    assert(!smith_o_ptr->k_idx && source_count()==1);
    object_type *result=finished();assert(result && object_ego_prefix(result)==prefix && result->discount==17);
    assert(streq(quark_str(result->obj_note),"preserved reforge inscription"));
    assert(!memcmp(drain,p_ptr->stat_drain,sizeof(drain)) && p_ptr->new_exp-p_ptr->ident_exp==exp);
    assert(p_ptr->skill_base[S_SMT]==smt && forge_uses(5,5)==0);
    scenario="old version25 ordinary unfinished craft";prepare(1);
    player_carried_extra_reset_store();fixture_normal_pending();
    replace_level_prefix();assert(!p_ptr->smithing_leftover);
    fixture_normal_pending();
    length=fixture_write_work(buffer,sizeof(buffer));object_wipe(smith_o_ptr);p_ptr->smithing_leftover=0;
    assert(!fixture_read_work(buffer,length,25));
    assert(smith_o_ptr->unused1==0 && p_ptr->smithing_leftover==1);
    keys="\r";do_cmd_smithing_screen();assert(!*keys && p_ptr->smithing==1);
    work_one();assert(!p_ptr->smithing_leftover && source_count()==1 && !finished());
    assert(forge_uses(5,5)==0);
    scenario="one source withdrawn from a stack";prepare(1);
    player_inventory_object(selected_item)->number=3;
    choice=fixture_reforge_plan(selected_item,&prefix,&turns,&dex_cost);assert(choice>=0);
    assert(fixture_begin(selected_item,prefix));
    assert(source_count()==2 && smith_o_ptr->number==1 && forge_uses(5,5)==0);
    assert(p_ptr->stat_drain[A_DEX]==-dex_cost);
    create_smithing_item();assert(source_count()==3 && finished()->number==1);
    scenario="cursed equipped source cannot be withdrawn";prepare(1);
    inventory[INVEN_WIELD]=*player_inventory_object(selected_item);
    player_carried_extra_reset_store();selected_item=INVEN_WIELD;
    inventory[INVEN_WIELD].ident|=IDENT_CURSED;
    choice=fixture_reforge_plan(selected_item,&prefix,&turns,&dex_cost);assert(choice>=0);
    object_wipe(smith_o_ptr);assert(!fixture_begin(selected_item,prefix));
    assert(inventory[INVEN_WIELD].k_idx && !p_ptr->smithing_leftover && forge_uses(5,5)==1);
    scenario="equipped source enters pending work";
    inventory[INVEN_WIELD].ident&=~IDENT_CURSED;
    assert(fixture_begin(selected_item,prefix));
    assert(!inventory[INVEN_WIELD].k_idx && source_count()==0 && smith_o_ptr->unused1==2);
    create_smithing_item();assert(source_count()==1 && finished());
    puts("Timed Reforge: free cancel, exact paid work, interrupted codec roundtrip, exhausted-forge resume, no overwrite/double payment, stack/equipped sources and old ordinary work PASS.");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true',help='Prove old instant prefix commit fails pending-work checks')
    args=parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    checks=CHECKS
    smith_text=SMITH
    if args.baseline:
        baseline=subprocess.check_output(['git','-c',f'safe.directory={ROOT.as_posix()}',
            'show','HEAD:src/cmd/ui/cmd-ui-smithing.c'],cwd=ROOT,text=True,encoding='utf-8')
        (OUT/'reforge-baseline.c').write_text(baseline,encoding='utf-8')
        smith_text=smith_text.replace('"cmd/ui/cmd-ui-smithing.c"','"reforge-baseline.c"')
        smith_text=smith_text.replace('return smith_begin_reforge(slot,prefix);','(void)slot;(void)prefix;return false;')
        checks=checks.replace('keys=input;do_cmd_smithing_screen();assert(!*keys);',
            'keys=input+1;assert(fixture_reforge() && !*keys);')
    source.write_text(prefix + checks + init + '    checks(); SDL_Quit(); return 0;\n}\n',encoding='utf-8')
    smith = OUT / 'smith.c';smith.write_text(smith_text,encoding='utf-8')
    writer = OUT / 'writer.c'
    writer.write_text('#include "fs/save.c"\nsize_t fixture_write_work(byte *buf,size_t capacity) {\n'
        'fff=SDL_IOFromMem(buf,capacity); xor_byte=0;v_stamp=x_stamp=save_byte_offset=0;write_error=false;\n'
        'wr_extra();wr_item(smith_o_ptr);size_t n=SDL_TellIO(fff);SDL_CloseIO(fff);fff=NULL;return n;\n}\n',encoding='utf-8')
    loader_text=(ROOT/'src/fs/load.c').read_text(encoding='utf-8')
    start=loader_text.index('    savefile_has_runtime_overrides =',loader_text.index('static errr rd_savefile_new_aux'))
    flags=loader_text[start:loader_text.index('    /* Reset load byte offset',start)]
    reader=OUT/'reader.c'
    reader.write_text('#include "fs/load.c"\nint fixture_read_work(const byte *buf,size_t length,int extra) {\n'
        'fff=SDL_IOFromConstMem(buf,length);xor_byte=0;v_check=x_check=load_byte_offset=0;\n'
        'sf_major=VERSION_MAJOR;sf_minor=VERSION_MINOR;sf_patch=VERSION_PATCH;sf_extra=extra;\n'
        +flags+'savefile_has_varda_quest=true;\nint result=rd_extra();if(!result)result=rd_item(smith_o_ptr);\n'
        'if(!result && SDL_TellIO(fff)!=(Sint64)length)result=-1;SDL_CloseIO(fff);fff=NULL;return result;\n}\n',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded=('/src/main.c.obj','/src/cmd/ui/cmd-ui-smithing.c.obj','/src/fs/save.c.obj','/src/fs/load.c.obj','/src/dungeon/dungeon-player.c.obj','/src/level-generation/level-generation.c.obj')
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(excluded)),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',
        str(source),str(smith),str(writer),str(reader),str(ROOT/'src/dungeon/dungeon-player.c'),str(ROOT/'src/level-generation/level-generation.c'),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in ('inkey','open_inventory_item_select_menu',
        'handle_stuff','msg_print','request_command','process_command','get_sdl_config_path','tutorial_game_action_allowed',
        'level_gen_screen_begin','run_quest_initiated_count')),
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)

if __name__=='__main__':main()
