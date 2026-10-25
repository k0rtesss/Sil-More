#!/usr/bin/env python3
"""Production combat, smithing, pickup and persistence controls for accepted lore deeds.

Every scenario uses a fresh disposable profile. Input/presentation is doubled;
the engine performs weapon damage, deaths, arrow flight, pickup and smithing.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_achievement_expansion import CHECKS as BASE
from check_achievement_characters import SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-lore"

TESTS = r'''
#include "achievements-lore.h"
#include "metarun.h"
int fixture_smith(int,int,bool,bool,int);
void fixture_reforge(const object_type*);
void shatter_weapon(int);
extern bool fixture_evil;
bool __wrap_get_check_near(int y,int x,cptr prompt)
{ (void)y;(void)x;(void)prompt;return true; }
static void hero(int index)
{ p_ptr->pcharacter=index;current_character_profile=&c_info[index]; }
static void artefact(object_type* object,int index)
{
    assert(index>0 && index<z_info->art_max);
    item(object,a_info[index].tval,a_info[index].sval,1);
    object->name1=index;
}
static monster_type* setup(int race)
{
    monster_type* m=prepare(race);sil_achievements_begin_run();hero(0);
    p_ptr->prace=1;p_ptr->chp=p_ptr->mhp=20000;p_ptr->new_exp=100000;
    p_ptr->energy_use=100;p_ptr->csp=p_ptr->msp=10000;
    p_ptr->mdd=10;p_ptr->mds=20;p_ptr->equip_cnt=1;
    player_pack_action_reset();turn=playerturn=1;metar.id=71;
    return m;
}
static monster_type* spawn(int race)
{
    if(cave_m_idx[10][10]>0)delete_monster_idx(cave_m_idx[10][10]);
    r_info[race].max_num=1;current_build_vault_exact_token=true;
    assert(place_monster_one(10,10,race,false,true,NULL));
    current_build_vault_exact_token=false;
    monster_type* m=&mon_list[cave_m_idx[10][10]];
    m->hp=1;m->maxhp=100;m->ml=true;return m;
}
static void hit(void) { py_attack_aux(10,10,ATT_MAIN); }
static void direct_kill(monster_type* m,int who)
{ assert(mon_take_hit((int)(m-mon_list),30000,NULL,who)); }
static void checks(void)
{
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();
    assert(init_flavor_info()==0);flavor_init();
    const char* s=getenv("LORE_SCENARIO");assert(s);
    bool yes=true;int target=-1;
    if(!strncmp(s,"finrod",6)) {
        monster_type* m=setup(strstr(s,"ordinary-wolf")?11:R_IDX_WEREWOLF);
        hero(strstr(s,"wrong-hero")?0:9);
        target=SIL_ACH_THE_KING_WHO_KEPT_HIS_OATH;
        if(strstr(s,"armed")){artefact(&inventory[INVEN_WIELD],ART_STING);yes=false;}
        if(strstr(s,"wizard")){p_ptr->wizard=true;yes=false;}
        if(strstr(s,"wrong-hero")||strstr(s,"ordinary-wolf"))yes=false;
        if(strstr(s,"ambient")){direct_kill(m,0);yes=false;}else hit();
    }
    else if(!strncmp(s,"turin",5)) {
        monster_type* m=setup(R_IDX_MORGOTH);hero(28);
        artefact(&inventory[INVEN_WIELD],strstr(s,"wrong-blade")?ART_RINGIL:ART_GURTHANG);
        p_ptr->morgoth_second_wind=!strstr(s,"phase");
        target=SIL_ACH_HIS_DEATH_AND_FINAL_END;
        if(strstr(s,"phase")||strstr(s,"wrong-blade"))yes=false;
        if(strstr(s,"ambient")){direct_kill(m,0);yes=false;}else hit();
        if(yes)assert(p_ptr->morgoth_slain && !p_ptr->playing);
    }
    else if(!strncmp(s,"azaghal-throw",13)) {
        monster_type* m=setup(R_IDX_GLAURUNG);p_ptr->prace=4;
        ability(S_MEL,MEL_THROWING);ability(S_MEL,MEL_POWER_THROW);
        artefact(&inventory[INVEN_WIELD],ART_AZAGHAL);
        object_type spear;item(&spear,TV_POLEARM,SV_SPEAR,3);
        assert(player_carried_extra_load(&spear));
        p_ptr->previous_action[1]=5;p_ptr->focused=false;
        target_set_monster((int)(m-mon_list));player_active_weapon_begin_player_turn();
        miss_roll=strstr(s,"miss")?2:0;
        target=SIL_ACH_BELEGOST_REMEMBERS;do_cmd_throw(false);
        assert(roll_number==2&&!m->r_idx);yes=!strstr(s,"miss");
    }
    else if(!strncmp(s,"azaghal",7)) {
        setup(R_IDX_GLAURUNG);p_ptr->prace=strstr(s,"wrong-race")?1:4;
        artefact(&inventory[INVEN_WIELD],ART_AZAGHAL);
        target=SIL_ACH_BELEGOST_REMEMBERS;yes=!strstr(s,"wrong-race");hit();
    }
    else if(!strncmp(s,"sam",3)) {
        setup(R_IDX_SHELOB);artefact(&inventory[INVEN_WIELD],ART_STING);
        if(!strstr(s,"no-light"))item(&inventory[INVEN_LITE],TV_LIGHT,SV_LIGHT_SILMARIL,1);
        target=SIL_ACH_NO_PHIAL_REQUIRED;yes=!strstr(s,"no-light");hit();
    }
    else if(!strncmp(s,"shatter",7)) {
        setup(41);artefact(&inventory[INVEN_WIELD],strstr(s,"other")?ART_STING:ART_ANGRIST);
        target=SIL_ACH_NOT_THE_DOOM_OF_THE_SILMARILS;
        yes=!strstr(s,"other")&&!strstr(s,"third");
        shatter_weapon(strstr(s,"third")?3:2);
        assert(!inventory[INVEN_WIELD].k_idx);
    }
    else if(!strncmp(s,"ringil",6)) {
        monster_type* m=setup(R_IDX_MORGOTH);m->hp=m->maxhp=30000;
        artefact(&inventory[INVEN_WIELD],ART_RINGIL);
        target=SIL_ACH_SEVEN_TIMES_HE_CRIED;
        if(strstr(s,"fake")){for(int i=0;i<7;i++)mon_take_hit((int)(m-mon_list),1,NULL,-1);yes=false;}
        else for(int i=0;i<(strstr(s,"six")?6:7);i++)hit();
        if(strstr(s,"six"))yes=false;
        assert(p_ptr->achievement_ringil_wounds==(strstr(s,"fake")?0:yes?7:6));
        assert(!earned(target));p_ptr->depth=0;sil_achievements_on_escape(1);
    }
    else if(!strncmp(s,"cat",3)) {
        monster_type* m=setup(R_IDX_TEVILDO);target=SIL_ACH_THE_CAT_AND_THE_NECROMANCER;
        direct_kill(m,strstr(s,"ambient")?0:-1);assert(!earned(target));
        if(strstr(s,"split"))sil_achievements_begin_run();
        m=spawn(R_IDX_GORTHAUR);direct_kill(m,-1);
        yes=!strstr(s,"ambient")&&!strstr(s,"split");
    }
    else if(!strcmp(s,"dailir")) {
        monster_type* m=setup(41);object_type arrow;
        item(&inventory[INVEN_BOW],TV_BOW,SV_SHORT_BOW,1);
        artefact(&arrow,ART_DAILIR);assert(player_quiver_absorb_arrow(&arrow)==1);
        p_ptr->ammo_tval=TV_ARROW;p_ptr->add=5;p_ptr->ads=20;
        p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
        target=SIL_ACH_EVER_FOUND_AGAIN;target_set_monster((int)(m-mon_list));
        do_cmd_fire(1);assert(!m->r_idx && !earned(target));
        assert(p_ptr->achievement_lore_flags&SIL_ACH_LORE_DAILIR_KILL);
        int floor=0;for(int i=1;i<o_max;i++)if(o_list[i].k_idx&&o_list[i].name1==ART_DAILIR){floor=i;break;}
        assert(floor);py_pickup_aux_to_harness(floor);
        assert(p_ptr->achievement_lore_flags&SIL_ACH_LORE_DAILIR_RECOVERED);
        m=spawn(41);target_set_monster((int)(m-mon_list));do_cmd_fire(1);assert(!m->r_idx);
    }
    else if(!strncmp(s,"glorfindel",10)) {
        monster_type* m=setup(R_IDX_DURUIN);hero(10);
        artefact(&inventory[INVEN_WIELD],ART_STING);
        if(!strstr(s,"no-cliff"))cave_feat[p_ptr->py-1][p_ptr->px]=FEAT_CHASM;
        target=SIL_ACH_THE_GOLDEN_FLOWER_STANDS;hit();assert(!m->r_idx&&!earned(target));
        if(strstr(s,"dead"))p_ptr->is_dead=true;
        if(strstr(s,"revived")) {
            take_hit(30000,"fixture cliff defeat");
            assert(p_ptr->achievement_glorfindel_depth==-1);
            p_ptr->is_dead=false;p_ptr->playing=true;p_ptr->chp=20000;
        }
        sil_achievements_on_level_travel(5,4,FEAT_LESS);
        yes=!strstr(s,"dead")&&!strstr(s,"no-cliff")&&!strstr(s,"revived");
    }
    else if(!strncmp(s,"biter",5)) {
        monster_type* m=setup(R_IDX_ORC_CAPTAIN);m->morale=-10000;
        artefact(&inventory[INVEN_WIELD],ART_ORCRIST);
        artefact(&inventory[strstr(s,"stowed")?0:INVEN_ARM],ART_GLAMDRING);
        if(strstr(s,"already"))m->stance=STANCE_FLEEING;
        if(strstr(s,"bow")) {
            item(&inventory[INVEN_BOW],TV_BOW,SV_SHORT_BOW,1);
            p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
        }
        target=SIL_ACH_BITER_AND_BEATER;calc_stance(m);
        yes=!strstr(s,"stowed")&&!strstr(s,"already")&&!strstr(s,"bow");
    }
    else if(!strncmp(s,"throne",6)) {
        setup(41);hero(strstr(s,"wrong-hero")?0:15);p_ptr->depth=MORGOTH_DEPTH;
        artefact(&inventory[strstr(s,"carried")?0:INVEN_OUTER],ART_THURINGWETHIL);
        target=SIL_ACH_THURINGWETHIL_I_AM;sil_achievement_lore_on_throne_entry();
        yes=!strstr(s,"wrong-hero")&&!strstr(s,"carried");
    }
    else if(!strncmp(s,"escape",6)) {
        setup(41);hero(1);p_ptr->prace=5;p_ptr->depth=0;
        artefact(&inventory[0],strstr(s,"no-empty")?ART_MORGOTH_1:ART_MORGOTH_0);
        artefact(&inventory[1],ART_NARSIL);artefact(&inventory[2],ART_BARAHIR);
        item(&inventory[3],TV_LIGHT,SV_LIGHT_SILMARIL,strstr(s,"no-empty")?2:3);
        p_ptr->oath_type=OATH_MERCY;
        if(strstr(s,"broken"))p_ptr->oaths_broken|=OATH_MERCY_FLAG;
        target=SIL_ACH_THESE_THINGS_ARE_MINE;assert(silmarils_possessed()==3);
        sil_achievements_on_escape(silmarils_possessed());
        assert(earned(SIL_ACH_HEAVY_IS_THE_STOLEN_CROWN));
        assert(earned(SIL_ACH_THE_OATH_HE_SHOULD_HAVE_SWORN)==!strstr(s,"broken"));
        assert(earned(SIL_ACH_HEIRLOOMS_OF_THE_HIDDEN_KING));
    }
    else if(!strncmp(s,"rings",5)) {
        setup(41);hero(strstr(s,"wrong-hero")?0:46);
        p_ptr->skill_base[S_SMT]=p_ptr->skill_use[S_SMT]=1000;
        target=SIL_ACH_THREE_UNTOUCHED;fixture_evil=strstr(s,"evil")!=NULL;
        int first=-1;
        for(int i=0;i<(strstr(s,"reforge")?1:3);i++) {
            int slot=fixture_smith(TV_RING,SV_RING_SELF_MADE,!strstr(s,"ordinary"),false,20);
            if(!i)first=slot;
            if(i<2)assert(!earned(target));
        }
        if(strstr(s,"reforge")) {
            assert(first>=0);object_type copy=*player_inventory_object(first);
            inven_item_increase(first,-1);inven_item_optimize(first);
            fixture_reforge(&copy);fixture_reforge(&copy);
            assert(p_ptr->achievement_artefact_rings[0]&&!p_ptr->achievement_artefact_rings[1]);
        }
        yes=!strstr(s,"ordinary")&&!strstr(s,"wrong-hero")&&!strstr(s,"evil")&&!strstr(s,"reforge");
        if(yes)assert(p_ptr->achievement_artefact_rings[0]!=p_ptr->achievement_artefact_rings[1]
            &&p_ptr->achievement_artefact_rings[1]!=p_ptr->achievement_artefact_rings[2]);
    }
    else if(!strncmp(s,"curse",5)) {
        setup(41);hero(1);artefact(&inventory[INVEN_HEAD],ART_FEANOR);
        inventory[INVEN_HEAD].ident|=IDENT_CURSED;
        if(strstr(s,"other-light"))item(&inventory[0],TV_LIGHT,SV_LIGHT_SILMARIL,1);
        else artefact(&inventory[0],ART_FINWE_LAMP);
        target=SIL_ACH_HIS_FATHERS_LIGHT;do_cmd_wield(&inventory[0],0);
        yes=!strstr(s,"other-light");assert(!cursed_p(&inventory[INVEN_HEAD]));
    }
    else if(!strncmp(s,"balrogs",7)) {
        setup(41);target=SIL_ACH_AT_MOST_SEVEN;
        const int races[]={R_IDX_DURUIN,145,166,186,206,225,R_IDX_GOTHMOG};
        for(int i=0;i<7;i++) {
            if(i==6&&strstr(s,"split"))sil_achievements_begin_run();
            monster_type* m=spawn(strstr(s,"duplicates")?R_IDX_DURUIN:races[i]);
            direct_kill(m,-1);if(i<6)assert(!earned(target));
        }
        yes=!strstr(s,"duplicates")&&!strstr(s,"split");
    }
    else if(!strncmp(s,"contest",7)) {
        monster_type* m=setup(R_IDX_GORTHAUR);
        artefact(&inventory[strstr(s,"carried")?0:INVEN_LEFT],ART_MAIRON);
        p_ptr->song1=SNG_CONTEST;ability(S_SNG,SNG_CONTEST);
        p_ptr->song_target_idx=(int)(m-mon_list);p_ptr->song_target_song=SNG_CONTEST;
        m->song=SNG_MASTERY;
        p_ptr->skill_use[S_SNG]=10000;
        target=SIL_ACH_THE_RING_MAKER_OVERRULED;
        for(int i=0;i<10&&!m->song_contest_completed;i++){sing();playerturn++;}
        assert(m->song_contest_completed && m->song_will_penalty>0);
        yes=!strstr(s,"carried");
    }
    else assert(false);
    assert(target>=0);assert(earned(target)==yes);
    printf("PASS %s (%s)\n",s,yes?"earned":"excluded");
}
'''

SCENARIOS = (
    "finrod", "finrod-wrong-hero", "finrod-ordinary-wolf", "finrod-armed", "finrod-ambient", "finrod-wizard",
    "turin", "turin-phase", "turin-ambient", "turin-wrong-blade", "azaghal", "azaghal-wrong-race", "azaghal-throw", "azaghal-throw-miss",
    "sam", "sam-no-light", "shatter", "shatter-third", "shatter-other", "ringil", "ringil-six", "ringil-fake",
    "cat", "cat-split", "cat-ambient", "dailir", "glorfindel", "glorfindel-no-cliff", "glorfindel-dead", "glorfindel-revived",
    "biter", "biter-stowed", "biter-already", "biter-bow", "throne", "throne-wrong-hero", "throne-carried",
    "escape", "escape-no-empty", "escape-broken", "rings", "rings-ordinary", "rings-wrong-hero", "rings-evil", "rings-reforge",
    "curse", "curse-other-light", "balrogs", "balrogs-duplicates", "balrogs-split", "contest", "contest-carried",
)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + BASE[:BASE.index("static void check_thrall(void)")] + TESTS
                      + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = SMITH.replace("int fixture_smith(", "bool fixture_evil;\nint fixture_smith(")
    smith = smith.replace("smith_o_ptr->name1=smith_a_name;",
                          "if(fixture_evil)a->flags4|=TR4_EVIL_ITEM;\n        smith_o_ptr->name1=smith_a_name;")
    smith += "\nvoid fixture_reforge(const object_type* o) { object_copy(smith_o_ptr,o); smith_o_ptr->unused1=2; create_smithing_item(); }\n"
    helper = OUT / "smith.c"
    helper.write_text(smith, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects
        if not o.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj"))))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                                  "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print", "message_flush",
             "get_sdl_config_path", "pause_with_text", "quest_typewriter_menu_pages", "ui_question_ask",
             "get_aim_dir", "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details", "get_check_near")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(helper), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + n for n in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for scenario in SCENARIOS:
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as data:
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                           env={**env, "LORE_SCENARIO": scenario}, check=True, timeout=45)
    print(f"PASS {len(SCENARIOS)} production lore scenarios covering all 19 accepted deeds")


if __name__ == "__main__":
    main()
