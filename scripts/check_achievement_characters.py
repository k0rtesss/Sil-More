#!/usr/bin/env python3
"""Production smithing, combat, song and resistance routes for eight hidden deeds.

Uses rebuilt Windows engine objects and a temporary data/profile directory.
Only UI side effects and attack hit rolls are doubled; no SDL input is sent.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_achievement_expansion import CHECKS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-characters"

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
#include <assert.h>
int fixture_smith(int tval,int sval,bool artifact,bool sharp,int uses)
{
    object_prep(smith_o_ptr,lookup_kind(tval,sval));
    smith_o_ptr->number=1;
    if(artifact) {
        artefact_type* a=smith_a_ptr;
        memset(a,0,sizeof(*a));
        SDL_strlcpy(a->name,"Fixture forge",sizeof(a->name));
        a->tval=tval;a->sval=sval;a->weight=smith_o_ptr->weight;
        a->dd=smith_o_ptr->dd;a->ds=smith_o_ptr->ds;
        a->pd=smith_o_ptr->pd;a->ps=smith_o_ptr->ps;
        a->flags1=k_info[smith_o_ptr->k_idx].flags1 | (sharp?TR1_SHARPNESS2:0);
        a->flags2=k_info[smith_o_ptr->k_idx].flags2;
        a->flags3=k_info[smith_o_ptr->k_idx].flags3;
        smith_o_ptr->name1=smith_a_name;
    }
    cave_feat[p_ptr->py][p_ptr->px]=FEAT_FORGE_NORMAL_HEAD+uses;
    object_difficulty(smith_o_ptr);
    assert(smithing_cost.uses<=uses);
    create_smithing_item();
    for(int i=0;i<player_pack_entry_count();i++) {
        int handle=player_pack_entry_handle_at(i);
        object_type* o=player_inventory_object(handle);
        if(o && o->tval==tval && o->sval==sval)return handle;
    }
    return -1;
}
'''
ITEMS = '#include "cmd/item/cmd-item-core.c"\nbool fixture_cut(void) { return prise_silmaril(); }\n'

TESTS = r'''
#include "achievements-character.h"
int fixture_smith(int,int,bool,bool,int);
bool fixture_cut(void);
int forge_uses(int,int);
int mithril_carried(void);
static monster_type* character(u32b unique,int race)
{
    monster_type* m=prepare(race);
    player_pack_action_reset();
    memset(events,0,sizeof(events));
    p_ptr->energy_use=100;p_ptr->csp=p_ptr->msp=100;
    for(int i=0;i<z_info->c_max;i++)if(c_info[i].flags_u&unique) {
        p_ptr->pcharacter=i;current_character_profile=&c_info[i];break;
    }
    assert(c_info[p_ptr->pcharacter].flags_u&unique);
    for(int a=0;a<ABILITIES_MAX;a++)ability(S_SMT,a);
    update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
    return m;
}
static void equip_forged(int handle,int slot)
{
    assert(handle>=0);
    object_type* o=player_inventory_object(handle);assert(o && o->k_idx);
    assert(do_cmd_wield_to_slot(o,handle,slot));
    for(int turns=0;turns<10 && player_pack_action_pending();turns++)player_pack_action_process();
    assert(!player_pack_action_pending());
    assert(inventory[slot].k_idx);
}
static void crown(void)
{
    int index=o_pop();assert(index>0);
    object_type* o=&o_list[index];
    object_prep(o,lookup_kind(a_info[ART_MORGOTH_3].tval,a_info[ART_MORGOTH_3].sval));
    o->name1=ART_MORGOTH_3;o->number=1;
    o->iy=p_ptr->py;o->ix=p_ptr->px;
    cave_o_idx[p_ptr->py][p_ptr->px]=index;
    p_ptr->mdd=20;p_ptr->mds=20;
}
static void smiths(void)
{
    character(UNQ_SMT_FEANOR,41);
    assert(fixture_smith(TV_SWORD,SV_SHORT_SWORD,true,false,2)>=0);
    assert(!events[SIL_ACH_A_SINGLE_SPARK]);
    character(UNQ_SMT_FEANOR,41);
    assert(fixture_smith(TV_SWORD,SV_SHORT_SWORD,false,false,1)>=0);
    assert(!events[SIL_ACH_A_SINGLE_SPARK]);
    character(UNQ_SMT_FEANOR,41);
    assert(fixture_smith(TV_SWORD,SV_SHORT_SWORD,true,false,1)>=0);
    assert(events[SIL_ACH_A_SINGLE_SPARK]==1 && forge_uses(p_ptr->py,p_ptr->px)==0);
    puts("PASS Feanor: actual artifact completion on last charge; spare charge and ordinary item excluded");

    character(UNQ_SMT_GAMIL,41);
    item(&inventory[0],TV_METAL,SV_METAL_MITHRIL,100);
    assert(fixture_smith(TV_HELM,SV_MITHRIL_HELM,false,false,3)>=0);
    assert(!events[SIL_ACH_OUT_OF_THIN_ORE] && mithril_carried()<100);
    character(UNQ_SMT_GAMIL,41);
    assert(fixture_smith(TV_HELM,SV_MITHRIL_HELM,false,false,3)>=0);
    assert(events[SIL_ACH_OUT_OF_THIN_ORE]==1 && mithril_carried()==0);
    assert(forge_uses(p_ptr->py,p_ptr->px)==0);
    puts("PASS Gamil: actual waived material cost/three forge charges; consumed mithril excluded");

    character(UNQ_SMT_EOL,41);
    item(&inventory[INVEN_BODY],TV_SOFT_ARMOR,SV_ARMOUR_OF_GALVORN,1);
    sil_achievement_character_on_player_turn();assert(!events[SIL_ACH_CLAD_IN_THE_NIGHT]);
    character(UNQ_SMT_EOL,41);
    int handle=fixture_smith(TV_SOFT_ARMOR,SV_ARMOUR_OF_GALVORN,false,false,1);
    assert(handle>=0 && !events[SIL_ACH_CLAD_IN_THE_NIGHT]);
    object_type found=*player_inventory_object(handle);
    found.ident&=~IDENT_ACH_EOL_GALVORN;
    assert(!object_similar(&found,player_inventory_object(handle)));
    equip_forged(handle,INVEN_BODY);
    assert(events[SIL_ACH_CLAD_IN_THE_NIGHT]>=1);
    puts("PASS Eol: forged provenance survives carry/equip; found and merely carried Galvorn excluded; stack merge blocked");

    character(UNQ_SMT_TELCHAR,41);
    handle=fixture_smith(TV_SWORD,SV_SHORT_SWORD,true,true,3);
    assert(handle>=0);equip_forged(handle,INVEN_WIELD);
    inventory[INVEN_WIELD].ident&=~IDENT_ACH_TELCHAR_SHARPNESS;
    crown();assert(fixture_cut());assert(!events[SIL_ACH_IRON_CLEAVER]);
    character(UNQ_SMT_TELCHAR,41);
    handle=fixture_smith(TV_SWORD,SV_SHORT_SWORD,true,true,3);
    equip_forged(handle,INVEN_WIELD);
    assert(inventory[INVEN_WIELD].ident&IDENT_ACH_TELCHAR_SHARPNESS);
    crown();assert(fixture_cut());assert(events[SIL_ACH_IRON_CLEAVER]==1);
    puts("PASS Telchar: actual Silmaril cut with the marked forged Sharpness II weapon; unmarked counterpart excluded");
}
static void maedhros(void)
{
    for(int mode=0;mode<4;mode++) {
        monster_type* m=character(UNQ_MEL_MAEDHROS,41);
        item(&inventory[INVEN_WIELD],TV_SWORD,mode==0?SV_SHORT_SWORD:SV_BASTARD_SWORD,1);
        p_ptr->equip_cnt=1;p_ptr->mdd=10;p_ptr->mds=20;
        if(mode!=1)ability(S_MEL,MEL_CONTROL);
        if(mode==2){item(&inventory[INVEN_ARM],TV_SHIELD,SV_ROUND_SHIELD,1);p_ptr->equip_cnt++;}
        py_attack_aux(10,10,ATT_MAIN);assert(!m->r_idx);
        assert(events[SIL_ACH_ON_THE_OTHER_HAND]==(mode==3?1:0));
    }
    monster_type* m=character(UNQ_MEL_MAEDHROS,41);
    item(&inventory[INVEN_WIELD],TV_SWORD,SV_BASTARD_SWORD,1);ability(S_MEL,MEL_CONTROL);
    p_ptr->equip_cnt=1;p_ptr->mdd=1;p_ptr->mds=1;m->hp=30000;
    py_attack_aux(10,10,ATT_MAIN);assert(m->r_idx && !events[SIL_ACH_ON_THE_OTHER_HAND]);
    puts("PASS Maedhros: actual lethal Subtlety critical with hand-and-a-half weapon; short sword, inactive Subtlety, shield, surviving target excluded");
}
static void check_singing(void)
{
    monster_type* m=character(UNQ_SNG_LUT,R_IDX_MORGOTH);
    p_ptr->song1=SNG_LORIEN;ability(S_SNG,SNG_LORIEN);
    p_ptr->skill_use[S_SNG]=1;sing();assert(!events[SIL_ACH_A_DREAM_UPON_THE_DARK_LORD]);
    p_ptr->skill_use[S_SNG]=1000;m->alertness=ALERTNESS_ALERT;
    sing();assert(m->alertness<ALERTNESS_UNWARY && events[SIL_ACH_A_DREAM_UPON_THE_DARK_LORD]);
    memset(events,0,sizeof(events));sing();assert(!events[SIL_ACH_A_DREAM_UPON_THE_DARK_LORD]);
    puts("PASS Luthien: actual paid Lorien sleep transition on Morgoth; failed contest and already asleep excluded");
    for(int mode=0;mode<4;mode++) {
        m=character(UNQ_WOVEN_MASTER,41);
        p_ptr->song1=SNG_ELBERETH;p_ptr->song2=mode==0?SNG_SILENCE:SNG_LORIEN;
        ability(S_SNG,p_ptr->song1);ability(S_SNG,p_ptr->song2);ability(S_SNG,SNG_WOVEN_THEMES);
        p_ptr->skill_use[S_SNG]=1000;
        if(mode==1)p_ptr->csp=1;
        if(mode==2)p_ptr->wizard=true;
        sing();
        assert((events[SIL_ACH_NEITHER_VOICE_DIMINISHED]>0)==(mode==3));
        assert((events[SIL_ACH_TWO_THEMES_ONE_VOICE]>0)==(mode==3));
    }
    puts("PASS Daeron: actual two enemy-affecting full-strength woven themes in one paid sing; passive theme, partial voice payment and wizard excluded");
    m=character(UNQ_WOVEN_MASTER,41);
    p_ptr->song1=SNG_ELBERETH;p_ptr->song2=SNG_SLAYING;
    ability(S_SNG,SNG_ELBERETH);ability(S_SNG,SNG_SLAYING);ability(S_SNG,SNG_WOVEN_THEMES);
    p_ptr->skill_use[S_SNG]=1000;sing();assert(!events[SIL_ACH_NEITHER_VOICE_DIMINISHED]);
    item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    p_ptr->equip_cnt=1;p_ptr->mdd=10;p_ptr->mds=20;
    py_attack_aux(10,10,ATT_MAIN);assert(!m->r_idx && events[SIL_ACH_NEITHER_VOICE_DIMINISHED]);
    assert(events[SIL_ACH_TWO_THEMES_ONE_VOICE]);
    m=character(UNQ_WOVEN_MASTER,41);
    p_ptr->song1=SNG_ELBERETH;p_ptr->song2=SNG_MASTERY;
    ability(S_SNG,SNG_ELBERETH);ability(S_SNG,SNG_MASTERY);ability(S_SNG,SNG_WOVEN_THEMES);
    p_ptr->skill_use[S_SNG]=1000;sing();assert(!events[SIL_ACH_NEITHER_VOICE_DIMINISHED]);
    m->energy=100;m->mflag|=MFLAG_ACTV;process_monsters(100);
    assert(m->skip_this_turn && events[SIL_ACH_NEITHER_VOICE_DIMINISHED]);
    assert(events[SIL_ACH_TWO_THEMES_ONE_VOICE]);
    puts("PASS Daeron late effects: paid scope retains Slaying melee execution and Mastery monster-turn result");
    m=character(UNQ_WOVEN_MASTER,31);
    m->hp=m->maxhp=30000;
    cave_light[m->fy][m->fx]=6;
    p_ptr->song1=SNG_CHALLENGE;p_ptr->song2=SNG_TREES;p_ptr->song_duration=1;
    ability(S_SNG,SNG_CHALLENGE);ability(S_SNG,SNG_TREES);ability(S_SNG,SNG_WOVEN_THEMES);
    p_ptr->skill_use[S_SNG]=100;int voice=p_ptr->csp;sing();
    assert(p_ptr->csp==voice && events[SIL_ACH_NEITHER_VOICE_DIMINISHED]);
    assert(events[SIL_ACH_TWO_THEMES_ONE_VOICE]);
    puts("PASS Daeron sustain: legitimate zero-cost song tick counts; ordinary voice/ability gate still required");
}
static void turin(void)
{
    monster_type* m=character(UNQ_WIL_TURIN,41);int idx=(int)(m-mon_list);
    p_ptr->pcharacter=0;p_ptr->free_act=100;
    assert(!allow_player_entrancement(m));assert(!m->achievement_doom_rage_epoch);
    mon_take_hit(idx,100,NULL,-1);assert(!events[SIL_ACH_DOOM_CAN_WAIT]);
    for(int mode=0;mode<4;mode++) {
        m=character(UNQ_WIL_TURIN,41);idx=(int)(m-mon_list);
        if(mode==3){p_ptr->rage=30;p_ptr->achievement_rage_epoch=0;}
        for(int tries=0;tries<100 && !m->achievement_doom_rage_epoch;tries++)allow_player_entrancement(m);
        assert(p_ptr->rage && m->achievement_doom_rage_epoch==p_ptr->achievement_rage_epoch);
        if(mode==0){set_rage(0);set_rage(10);}
        if(mode==1) {
            int y=m->fy,x=m->fx;delete_monster_idx(idx);
            current_build_vault_exact_token=true;assert(place_monster_one(y,x,41,false,true,NULL));current_build_vault_exact_token=false;
            idx=cave_m_idx[y][x];mon_list[idx].hp=1;
        }
        mon_take_hit(idx,100,NULL,-1);
        assert((events[SIL_ACH_DOOM_CAN_WAIT]>0)==(mode>=2));
    }
    puts("PASS Turin: actual unique entrancement resistance/rage then attacker kill; ordinary save, new rage episode and replacement attacker excluded");
}
static void checks(void)
{
    assert(init_flavor_info()==0);flavor_init();
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();
    smiths();maedhros();check_singing();turin();
    puts("PASS all eight character achievements through production routes and negative controls");
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    base = BASE[:BASE.index("static void check_thrall(void)")]
    source = OUT / "check.c"
    source.write_text(prefix + base + TESTS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    helpers=[]
    for name,content in (("smith",SMITH),("items",ITEMS)):
        path=OUT/(name+".c");path.write_text(content,encoding="utf-8");helpers.append(str(path))
    units=("achievements-character", "player/player-song-effects")
    excluded=("/src/main.c.obj","/src/cmd/ui/cmd-ui-smithing.c.obj","/src/cmd/item/cmd-item-core.c.obj",*("/src/"+u+".c.obj" for u in units))
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+o+'"' for o in objects if not o.endswith(excluded)),encoding="utf-8")
    env=os.environ.copy()
    env["SDL_VIDEODRIVER"]=env["SDL_AUDIODRIVER"]="dummy"
    env["PATH"]=os.pathsep.join([*(str(BUILD/"_deps"/n) for n in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),"C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    wraps=("sil_achievements_on_event","handle_stuff","update_stuff","msg_print","message_flush","get_sdl_config_path","pause_with_text","quest_typewriter_menu_pages","ui_question_ask","get_aim_dir","show_interaction_skill_roll_animation_actor","hit_roll","hit_roll_details")
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g","@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),*helpers,*(str(ROOT/"src"/(u+".c")) for u in units),"@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp",*("-Wl,--wrap="+name for name in wraps),"-o",str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-",dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/"lib/edit"),data],cwd=data,env=env,check=True,timeout=60)

if __name__ == "__main__":
    main()
