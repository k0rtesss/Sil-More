#!/usr/bin/env python3
"""Roundtrip pending alloys through production player/inventory save codecs.

Uses the exact writer inventory block, real rd_inventory, current templates and
real process_player completion. All generated data lives in temporary profiles.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/qa9-smith-alloy-save'
SMITH=r"""
#include "cmd/ui/cmd-ui-smithing.c"
#include <assert.h>
void fixture_plan(int weapon,int type)
{
    smith_clear_alloy_state(&smith_alloy);
    smith_clear_alloy_state(&smith2_alloy);smith_clear_alloy_state(&smith3_alloy);
    object_prep(smith_o_ptr,lookup_kind(weapon?TV_SWORD:TV_MAIL,weapon?SV_SHORT_SWORD:4));
    smith_o_ptr->number=1;smith_o_ptr->ident|=IDENT_KNOWN;
    smith_o_ptr->obj_note=quark_add("pending alloy save fixture");
    assert(smith_apply_alloy(smith_o_ptr,&smith_alloy,(smith_alloy_type)type));
    p_ptr->smithing=p_ptr->smithing_leftover=MAX(10,object_difficulty(smith_o_ptr)*10);
}
void fixture_quote(int q[10])
{
    q[0]=object_difficulty(smith_o_ptr);q[1]=smithing_cost.mithril;q[2]=smithing_cost.star_iron;
    q[3]=smithing_cost.uses;q[4]=smithing_cost.exp;q[5]=smithing_cost.str;
    q[6]=smithing_cost.dex;q[7]=smithing_cost.con;q[8]=smithing_cost.gra;q[9]=smithing_cost.drain;
}
void fixture_poison(void)
{
    smith_alloy=(smith_alloy_state){99,7,7,7,7};
    smith2_alloy=(smith_alloy_state){99,7,7,7,7};smith3_alloy=smith2_alloy;
}
bool fixture_backups_clear(void)
{
    smith_alloy_state empty={0};
    return !memcmp(&smith2_alloy,&empty,sizeof(empty))&&!memcmp(&smith3_alloy,&empty,sizeof(empty));
}
"""
CHECKS=r"""
#include "supplies.h"
#include "sdl-config.h"
#undef assert
#define assert(expr) do { if(!(expr)) { fprintf(stderr,"%s: %s line %d\n",scenario,#expr,__LINE__); exit(1); } } while(0)
static const char* scenario="initialization";
static cptr keys;
extern void fixture_plan(int,int);
extern void fixture_quote(int[10]);
extern void fixture_poison(void);
extern bool fixture_backups_clear(void);
extern size_t fixture_write_work(byte*,size_t,int);
extern int fixture_read_work(const byte*,size_t,int);
extern int mithril_carried(void),star_iron_carried(void),forge_uses(int,int);
extern void process_player(void);
char __wrap_inkey(void)
{ if(inkey_scan){inkey_scan=false;return 0;}assert(keys&&*keys);return *keys++; }
void __wrap_message_flush(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
void __wrap_handle_stuff(void) { p_ptr->update=p_ptr->redraw=p_ptr->window=0; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action;(void)item;return true; }
void __wrap_tutorial_game_identified(const object_type* item,const char* event)
{ (void)item;(void)event; }
void __wrap_request_command(void)
{
    fprintf(stderr,"Unexpected command: smithing=%d left=%d energy=%d\n",
        p_ptr->smithing,p_ptr->smithing_leftover,p_ptr->energy);
    assert(false);
}
void __wrap_process_command(void) { assert(false); }
static void prepare(int weapon,int type)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(1);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();jewelry_presets_reset();
    p_ptr->py=p_ptr->px=5;p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->depth=5;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->food=PY_FOOD_FULL-1000;p_ptr->new_exp=p_ptr->exp=100000;p_ptr->skill_base[S_SMT]=p_ptr->skill_use[S_SMT]=100;
    for(int a=0;a<A_MAX;a++)p_ptr->stat_base[a]=p_ptr->stat_use[a]=20;
    for(int i=0;i<ABILITIES_MAX;i++)p_ptr->innate_ability[S_SMT][i]=p_ptr->have_ability[S_SMT][i]=p_ptr->active_ability[S_SMT][i]=true;
    p_ptr->active_ability[S_SMT][SMT_EXPERTISE]=false;
    cave_m_idx[5][5]=-1;cave_feat[5][5]=FEAT_FORGE_NORMAL_HEAD+3;
    character_generated=character_dungeon=true;character_icky=0;inkey_flag=true;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    op_ptr->opt[OPT_insight_beta]=false;
    object_prep(&inventory[0],lookup_kind(TV_METAL,SV_METAL_MITHRIL));inventory[0].number=99;
    object_prep(&inventory[1],lookup_kind(TV_METAL,SV_METAL_STAR_IRON));inventory[1].number=99;
    object_type item;object_prep(&item,lookup_kind(TV_SWORD,SV_DAGGER));item.number=1;
    item.obj_note=quark_add("extra codec sentinel");assert(player_carried_extra_load(&item));
    object_prep(&item,lookup_kind(TV_ARROW,1));item.number=2;
    item.obj_note=quark_add("quiver codec sentinel");assert(player_quiver_absorb_arrow(&item)==2);
    object_prep(&item,lookup_kind(TV_FOOD,SV_FOOD_BREAD));item.number=3;assert(supplies_absorb_object(&item));
    object_prep(&item,lookup_kind(TV_RING,0));item.number=1;
    jewelry_preset_set_object(0,0,&item);jewelry_preset_set_name(0,"wire sentinel");
    fixture_plan(weapon,type);turn=1001;playerturn=100;
}
static void sentinels(void)
{
    bool extra=false;
    for(int i=0;i<player_pack_entry_count();i++){
        cptr note=quark_str(player_pack_entry_at(i)->obj_note);
        if(note&&streq(note,"extra codec sentinel"))extra=true;
    }
    assert(extra&&player_quiver_arrow_count()==2&&supplies_entry_units(0)==3);
    assert(streq(jewelry_preset_name(0),"wire sentinel"));
    assert(jewelry_preset_object(0,0)->k_idx==lookup_kind(TV_RING,0));
}
static void work_one(void)
{
    int left=p_ptr->smithing_leftover;long before=playerturn;
    p_ptr->energy=100;process_player();
    assert(p_ptr->energy_use==100&&playerturn==before+1&&p_ptr->smithing_leftover==left-1);
}
static object_type* output(int kind,int* number)
{
    object_type* found=NULL;*number=0;
    for(int i=0;i<player_pack_entry_count();i++){
        object_type* o=player_pack_entry_at(i);
        if(o&&o->k_idx==kind&&o->unused1==1){found=o;*number+=o->number;}
    }
    for(int i=1;i<o_max;i++)if(o_list[i].k_idx==kind&&o_list[i].unused1==1){found=&o_list[i];*number+=found->number;}
    return found;
}
static void matrix(int weapon,int type)
{
    scenario=weapon?(type==1?"mithril weapon":"star-iron weapon"):(type==1?"mithril armour":"star-iron armour");
    prepare(weapon,type);int quote[10],loaded[10];fixture_quote(quote);
    keys="\r";do_cmd_smithing_screen();assert(!*keys);
    work_one();work_one();disturb(0,0);
    int remaining=p_ptr->smithing_leftover;object_type object=*smith_o_ptr;
    byte expected[SMITHING_ALLOY_STATE_BYTES];smithing_alloy_save_state(expected);
    byte buffer[262144];size_t length=fixture_write_work(buffer,sizeof(buffer),4);assert(length);
    fixture_poison();object_wipe(smith_o_ptr);p_ptr->smithing_leftover=0;
    assert(!fixture_read_work(buffer,length,4));
    byte actual[SMITHING_ALLOY_STATE_BYTES];smithing_alloy_save_state(actual);
    assert(!memcmp(actual,expected,sizeof(actual))&&fixture_backups_clear());
    assert(p_ptr->smithing_leftover==remaining&&smith_o_ptr->k_idx==object.k_idx);
    assert(smith_o_ptr->att==object.att&&smith_o_ptr->ds==object.ds&&smith_o_ptr->evn==object.evn&&smith_o_ptr->ps==object.ps);
    sentinels();assert(mithril_carried()==99&&star_iron_carried()==99);
    for(int a=0;a<A_MAX;a++)p_ptr->stat_use[a]=p_ptr->stat_base[a];
    fixture_quote(loaded);assert(!memcmp(quote,loaded,sizeof(quote)));
    object_type once=*smith_o_ptr;
    smithing_alloy_load_state(actual);smithing_alloy_load_state(actual);
    assert(!memcmp(&once,smith_o_ptr,sizeof(once))&&fixture_backups_clear());
    p_ptr->energy_use=0;keys="\r";do_cmd_smithing_screen();assert(!*keys&&p_ptr->smithing==remaining);
    int before_m=mithril_carried(),before_s=star_iron_carried();
    for(int i=0;i<remaining;i++)work_one();
    assert(!p_ptr->smithing_leftover&&!smith_o_ptr->k_idx&&forge_uses(5,5)==3-quote[3]);
    assert(mithril_carried()==before_m-quote[1]&&star_iron_carried()==before_s-quote[2]);
    int count;object_type* made=output(object.k_idx,&count);assert(made&&count==1);
    assert(made->att==object.att&&made->ds==object.ds&&made->evn==object.evn&&made->ps==object.ps);
    smithing_alloy_save_state(actual);byte empty[SMITHING_ALLOY_STATE_BYTES]={0};assert(!memcmp(actual,empty,sizeof(actual)));
}
static void legacy_and_malformed(void)
{
    scenario="legacy v3 neutral metadata and stream boundary";prepare(0,1);disturb(0,0);
    byte old[262144],fresh[262144];size_t oldn=fixture_write_work(old,sizeof(old),3),newn=fixture_write_work(fresh,sizeof(fresh),4);
    assert(oldn&&newn==oldn+SMITHING_ALLOY_STATE_BYTES);
    fixture_poison();assert(!fixture_read_work(old,oldn,3));sentinels();
    byte state[SMITHING_ALLOY_STATE_BYTES],empty[SMITHING_ALLOY_STATE_BYTES]={0};smithing_alloy_save_state(state);
    assert(!memcmp(state,empty,sizeof(state))&&fixture_backups_clear());
    assert(mithril_carried()==99&&star_iron_carried()==99&&p_ptr->smithing_leftover>0);
    scenario="malformed wire neutral fallback and stream boundary";prepare(0,1);disturb(0,0);fixture_poison();
    newn=fixture_write_work(fresh,sizeof(fresh),4);assert(!fixture_read_work(fresh,newn,4));sentinels();
    smithing_alloy_save_state(state);assert(!memcmp(state,empty,sizeof(state))&&fixture_backups_clear());
    const byte bad[][SMITHING_ALLOY_STATE_BYTES]={{0,1,0,0,0},{99,0,0,0,0},{1,1,0,0,0},{2,0,0,0,2}};
    for(int i=0;i<4;i++){
        scenario="malformed shape metadata only";prepare(0,1);object_type object=*smith_o_ptr;fixture_poison();
        smithing_alloy_load_state(bad[i]);smithing_alloy_save_state(state);
        assert(!memcmp(state,empty,sizeof(state))&&fixture_backups_clear()&&!memcmp(&object,smith_o_ptr,sizeof(object)));
    }
    scenario="reforge rejects alloy metadata";prepare(0,1);smith_o_ptr->unused1=2;
    const byte armour[SMITHING_ALLOY_STATE_BYTES]={1,0,0,1,0};smithing_alloy_load_state(armour);
    smithing_alloy_save_state(state);assert(!memcmp(state,empty,sizeof(state))&&fixture_backups_clear());
    scenario="inapplicable blueprint neutral fallback";object_prep(smith_o_ptr,lookup_kind(TV_RING,0));
    smithing_alloy_load_state(armour);smithing_alloy_save_state(state);assert(!memcmp(state,empty,sizeof(state)));
    scenario="empty blueprint neutral fallback";object_wipe(smith_o_ptr);fixture_poison();smithing_alloy_load_state(armour);
    smithing_alloy_save_state(state);assert(!memcmp(state,empty,sizeof(state))&&fixture_backups_clear());
}
static void checks(void)
{
    assert(VERSION_EXTRA>=4&&SMITHING_ALLOY_STATE_BYTES==5);
    matrix(1,1);matrix(1,2);matrix(0,1);matrix(0,2);legacy_and_malformed();
    puts("Alloy v4: real writer/loader weapon+armour matrix, repeated-load reset, exact resumed costs/output, v3 stream alignment, malformed neutral fallback PASS.");
}
"""

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'    assert(!init_flavor_info());flavor_init();checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    smith=OUT/'smith.c';smith.write_text(SMITH,encoding='utf-8')
    save=(ROOT/'src/fs/save.c').read_text(encoding='utf-8')
    start=save.index('    wr_item(smith_o_ptr);',save.index('static bool wr_savefile'))
    end=save.index('    /* Player is not dead, write the dungeon */',start)
    block=save[start:end]
    begin=block.index('    {\n        byte alloy[SMITHING_ALLOY_STATE_BYTES];')
    stop=block.index('    log_trace(',begin)
    block=block[:begin]+'    if(extra>=4)\n'+block[begin:stop]+block[stop:]
    writer=OUT/'writer.c';writer.write_text('#include "fs/save.c"\nstatic bool fixture_inventory_block(int extra){int i;\n'+block+'wr_u32b(0xA110795AU);return !write_error;}\nsize_t fixture_write_work(byte* buf,size_t cap,int extra){fff=SDL_IOFromMem(buf,cap);xor_byte=0;v_stamp=x_stamp=save_byte_offset=0;write_error=false;wr_extra();bool ok=fixture_inventory_block(extra);size_t n=ok?(size_t)SDL_TellIO(fff):0;SDL_CloseIO(fff);fff=NULL;return n;}\n',encoding='utf-8')
    load=(ROOT/'src/fs/load.c').read_text(encoding='utf-8')
    start=load.index('    savefile_has_runtime_overrides =',load.index('static errr rd_savefile_new_aux'))
    flags=load[start:load.index('    /* Reset load byte offset',start)]
    reader=OUT/'reader.c';reader.write_text('#include "fs/load.c"\nint fixture_read_work(const byte* buf,size_t length,int extra){fff=SDL_IOFromConstMem(buf,length);xor_byte=0;v_check=x_check=load_byte_offset=0;sf_major=VERSION_MAJOR;sf_minor=VERSION_MINOR;sf_patch=VERSION_PATCH;sf_extra=extra;\n'+flags+'savefile_has_varda_quest=true;int result=rd_extra();if(!result)result=rd_inventory();u32b tail=0;if(!result){rd_u32b(&tail);if(tail!=0xA110795AU||SDL_TellIO(fff)!=(Sint64)length)result=-1;}SDL_CloseIO(fff);fff=NULL;return result;}\n',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-smithing.c.obj','/src/fs/save.c.obj','/src/fs/load.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(smith),str(writer),str(reader),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in ('inkey','message_flush','msg_print','handle_stuff','tutorial_game_action_allowed','tutorial_game_identified','request_command','process_command')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)

if __name__=='__main__':main()
