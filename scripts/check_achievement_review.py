#!/usr/bin/env python3
"""Production checks for the reviewed conditions, rare finds and difficulty 65.

Uses disposable profiles, real combat/lore/smithing and the existing input doubles.
Does not open player saves or authenticate to an achievement provider.
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
OUT = ROOT / "scripts/output/achievement-review"

SMITH = r'''
#include "cmd/ui/cmd-ui-smithing.c"
#include <assert.h>
int fixture_difficult_artefact(int goal, bool custom)
{
    object_prep(smith_o_ptr,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    smith_o_ptr->number=1;
    object_kind* base=&k_info[smith_o_ptr->k_idx];
    artefact_type* art=smith_a_ptr;
    memset(art,0,sizeof(*art));
    SDL_strlcpy(art->name,"The Reviewed Blade",sizeof(art->name));
    art->tval=smith_o_ptr->tval;art->sval=smith_o_ptr->sval;
    art->dd=smith_o_ptr->dd;art->ds=smith_o_ptr->ds;
    art->pd=smith_o_ptr->pd;art->ps=smith_o_ptr->ps;
    art->flags1=base->flags1;art->flags2=base->flags2;art->flags3=base->flags3;
    if(custom)smith_o_ptr->name1=smith_a_name;
    bool found=false;
    for(int attack=0;attack<=8&&!found;attack++)
        for(int damage=0;damage<=4&&!found;damage++)
          for(int evasion=0;evasion<=3&&!found;evasion++)
            for(int extra_weight=0;extra_weight<=base->weight&&!found;extra_weight++) {
                smith_o_ptr->att=base->att+attack;
                smith_o_ptr->ds=base->ds+damage;
                smith_o_ptr->evn=base->evn+evasion;
                smith_o_ptr->weight=base->weight+extra_weight;
                art->att=smith_o_ptr->att;art->ds=smith_o_ptr->ds;art->evn=smith_o_ptr->evn;
                art->weight=smith_o_ptr->weight;
                if(object_difficulty(smith_o_ptr)==goal)found=true;
            }
    assert(found);
    int difficulty=object_difficulty(smith_o_ptr);
    assert(difficulty==goal);
    art->level=difficulty;
    cave_feat[p_ptr->py][p_ptr->px]=FEAT_FORGE_NORMAL_HEAD+20;
    assert(smithing_cost.uses<=20);
    create_smithing_item();
    return difficulty;
}
'''

TESTS = r'''
#include "achievements-lore.h"
void update_lore_aux(object_type*);
void update_lore(u32b);
void regenhp(int);
int fixture_difficult_artefact(int,bool);
static bool force_floor,fail_floor;
int __real_inven_carry(object_type*,bool);
int __wrap_inven_carry(object_type* o,bool combine)
{ return force_floor ? -1 : __real_inven_carry(o,combine); }
s16b __real_drop_near(object_type*,int,int,int);
s16b __wrap_drop_near(object_type* o,int chance,int y,int x)
{ return fail_floor ? 0 : __real_drop_near(o,chance,y,x); }

static monster_type* ready(int race)
{
    monster_type* m=prepare(race);
    p_ptr->pcharacter=0;current_character_profile=&c_info[0];
    p_ptr->chp=p_ptr->mhp=1000;p_ptr->csp=p_ptr->msp=1000;
    p_ptr->leaving=p_ptr->is_dead=p_ptr->leaping=false;p_ptr->energy_use=100;
    p_ptr->new_exp=1000000;
    cave_m_idx[p_ptr->py][p_ptr->px]=0;p_ptr->py=10;p_ptr->px=9;
    cave_m_idx[10][9]=-1;sil_achievements_begin_run();
    player_pack_action_reset();turn=playerturn=1;
    for(int a=0;a<ABILITIES_MAX;a++)ability(S_SMT,a);
    p_ptr->skill_base[S_SMT]=p_ptr->skill_use[S_SMT]=1000;
    return m;
}
static void fixed(object_type* object,int artefact)
{
    artefact_type* a=&a_info[artefact];
    item(object,a->tval,a->sval,1);object->name1=artefact;
    object_into_artefact(object,a);object->number=1;
    object->ident|=IDENT_KNOWN;
}
static void step(int y,int x)
{ monster_swap(p_ptr->py,p_ptr->px,y,x);assert(p_ptr->py==y&&p_ptr->px==x); }
static void checks(void)
{
    assert(init_flavor_info()==0);flavor_init();
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();
    const char* scenario=getenv("ACH_REVIEW_CASE");assert(scenario);
    monster_type* m=ready(41);
    if(!strncmp(scenario,"desperate-",10)) {
        bool final=strstr(scenario,"final")!=NULL;
        bool phase=strstr(scenario,"phase")!=NULL;
        bool ordinary=strstr(scenario,"ordinary")!=NULL;
        bool nonlethal=strstr(scenario,"nonlethal")!=NULL;
        m=ready(final||phase?R_IDX_MORGOTH:ordinary?41:54);
        p_ptr->chp=strstr(scenario,"above")?251:strstr(scenario,"below")?1:250;
        if(strstr(scenario,"rounding")){p_ptr->mhp=101;p_ptr->chp=26;}
        if(strstr(scenario,"peaceful"))r_info[m->r_idx].flags1|=RF1_PEACEFUL;
        if(strstr(scenario,"wizard"))p_ptr->wizard=true;
        if(strstr(scenario,"practice"))p_ptr->game_type=1;
        if(final)p_ptr->morgoth_second_wind=true;
        int who=strstr(scenario,"ambient")?0:strstr(scenario,"monster")?1:-1;
        bool killed=mon_take_hit((int)(m-mon_list),nonlethal?0:30000,NULL,who);
        assert(killed==(!nonlethal&&!phase));
        bool expected=(final||!strcmp(scenario,"desperate-unique")||strstr(scenario,"below"))
            &&who<0&&!p_ptr->wizard&&p_ptr->game_type==0;
        assert(earned(SIL_ACH_A_DESPERATE_VICTORY)==expected);
        if(final)assert(p_ptr->morgoth_slain);
    } else if(!strncmp(scenario,"aure-",5)) {
        if(strstr(scenario,"without-hit"))p_ptr->chp=1;
        else take_hit(strstr(scenario,"two-hp")?998:999,"a review fixture hit");
        if(strstr(scenario,"after-travel"))sil_achievements_on_level_entry(false);
        if(strstr(scenario,"peaceful"))r_info[m->r_idx].flags1|=RF1_PEACEFUL;
        if(strstr(scenario,"partial"))assert(hp_player(1,false,false));
        if(strstr(scenario,"regen")) {
            for(int i=0;i<100&&p_ptr->chp==1;i++){playerturn++;regenhp(1);}
            assert(p_ptr->chp>1&&p_ptr->chp<p_ptr->mhp);
        }
        if(strstr(scenario,"song")) {
            ability(S_SNG,SNG_STAUNCHING);p_ptr->song1=SNG_STAUNCHING;sing();
            assert(p_ptr->chp>1&&p_ptr->chp<p_ptr->mhp);
        }
        assert(mon_take_hit((int)(m-mon_list),30000,NULL,strstr(scenario,"other-killer")?0:-1));
        bool expected=!strcmp(scenario,"aure-no-healing")
            ||!strcmp(scenario,"aure-without-hit")||!strcmp(scenario,"aure-after-travel");
        assert(earned(SIL_ACH_BY_A_THREAD)==expected);
    } else if(!strncmp(scenario,"ice-",4)) {
        cave_feat[10][10]=FEAT_WATER;
        if(strstr(scenario,"monster")) {
            project_f((int)(m-mon_list),10,10,0,0,0,0,GF_COLD);
            delete_monster_idx((int)(m-mon_list));
        } else {
            fixed(&inventory[INVEN_WIELD],ART_RINGIL);p_ptr->equip_cnt=1;
            p_ptr->mdd=100;p_ptr->mds=20;py_attack_aux(10,10,ATT_MAIN);
            assert(!m->r_idx);
        }
        assert(cave_feat[10][10]==FEAT_ICE);
        step(10,10);step(10,11);
        assert(earned(SIL_ACH_HELCARAXE)==!strstr(scenario,"monster"));
    } else if(!strncmp(scenario,"forge-",6)) {
        bool ordinary=strstr(scenario,"ordinary")!=NULL;
        int difficulty=strstr(scenario,"64")?64:strstr(scenario,"66")?66:65;
        if(strstr(scenario,"wizard"))p_ptr->wizard=true;
        if(strstr(scenario,"drop")||strstr(scenario,"failed-delivery"))force_floor=true;
        if(strstr(scenario,"failed-delivery"))fail_floor=true;
        assert(fixture_difficult_artefact(difficulty,!ordinary)==difficulty);
        bool expected=difficulty>=65&&!ordinary&&!p_ptr->wizard&&!fail_floor;
        assert(earned(SIL_ACH_THEIR_WORK_HAD_NO_RIVAL)==expected);
        if(force_floor&&!fail_floor) {
            bool found=false;
            for(int i=1;i<o_max;i++)if(o_list[i].k_idx&&o_list[i].name1>=z_info->art_rand_max)found=true;
            assert(found);
        }
    } else if(!strncmp(scenario,"rare-",5)) {
        static const struct {const char* name;int artefact;int achievement;int difficulty;} finds[]={
            {"nauglamir",ART_NAUGLAMIR,SIL_ACH_FIND_NAUGLAMIR,46},
            {"gurthang",ART_GURTHANG,SIL_ACH_FIND_GURTHANG,60},
            {"melkor",ART_MELKOR,SIL_ACH_FIND_MELKOR,55},
            {"gothmog",ART_GOTHMOG_AXE,SIL_ACH_FIND_GOTHMOG_AXE,59},
            {"angrist",ART_ANGRIST,SIL_ACH_FIND_ANGRIST,40},
            {"belthronding",ART_BELTHRONDING,SIL_ACH_FIND_BELTHRONDING,48},
            {"idril",ART_IDRIL_ROBE,SIL_ACH_FIND_IDRIL_ROBE,40},
            {"celebrimbor",ART_CELEBRIMBOR_GLOVES,SIL_ACH_FIND_CELEBRIMBOR_GLOVES,21},
            {"melian",ART_MELIAN_RING,SIL_ACH_FIND_MELIAN_RING,54},
            {"ulmo",ART_ULMO_CLOAK,SIL_ACH_FIND_ULMO_CLOAK,56},
            {"fingolfin",ART_FINGOLFIN_SHIELD,SIL_ACH_FIND_FINGOLFIN_SHIELD,59},
            {"turgon",ART_TURGON_STAFF,SIL_ACH_FIND_TURGON_STAFF,49}
        };
        bool rejected=strstr(scenario,"mairon")||strstr(scenario,"starlight")
            ||strstr(scenario,"calris")||strstr(scenario,"celeg");
        unsigned choice=0;
        for(unsigned i=0;i<N_ELEMENTS(finds);++i)if(strstr(scenario,finds[i].name))choice=i;
        int artefact=finds[choice].artefact,target=finds[choice].achievement;
        if(rejected)artefact=strstr(scenario,"mairon")?ART_MAIRON:strstr(scenario,"starlight")?41:
            strstr(scenario,"celeg")?ART_CELEG_AITHORN:ART_CALRIS;
        assert(sil_achievement_get(target)->hidden);
        if(strstr(scenario,"wizard"))p_ptr->wizard=true;
        if(strstr(scenario,"practice"))p_ptr->game_type=1;
        if(strstr(scenario,"dead"))p_ptr->is_dead=true;
        bool floor=strstr(scenario,"floor")!=NULL;
        object_type* object;
        if(floor) {
            int idx=o_pop();assert(idx>0);object=&o_list[idx];
            object->iy=object->ix=10;cave_o_idx[10][10]=idx;
        } else object=&inventory[0];
        fixed(object,artefact);
        if(!rejected) {
            assert(a_info[artefact].rarity<=30);
            assert(object_difficulty(object)==object_smithing_difficulty(object));
            assert(object_smithing_difficulty(object)==finds[choice].difficulty);
            if(artefact>=20)assert(!(a_info[artefact].flags3&TR3_INSTA_ART));
            u32b f1,f2,f3,f4;object_flags4(object,&f1,&f2,&f3,&f4);
            if(artefact==ART_ANGRIST)assert(f1&TR1_SHARPNESS2);
            if(artefact==ART_GURTHANG)assert((f1&TR1_VAMPIRIC)&&(f3&TR3_WILL_DRAIN));
            if(artefact==ART_MELKOR)assert((f1&TR1_BRAND_POIS)&&(f4&TR4_UNLIGHT));
            if(artefact==ART_BELTHRONDING)assert((f1&TR1_ARC)&&(f1&TR1_PER));
            if(artefact==ART_IDRIL_ROBE)assert((f2&TR2_SPEED)&&object->skilltype[0]==S_PER
                &&object->abilitynum[0]==PER_LISTEN);
            if(artefact==ART_CELEBRIMBOR_GLOVES)assert((f1&TR1_SMT)
                &&object->skilltype[0]==S_SMT&&object->abilitynum[0]==SMT_EXPERTISE);
            if(artefact==ART_MELIAN_RING)assert(object->skill_bonus[S_PER]==6);
            if(artefact==ART_FINGOLFIN_SHIELD)assert(object->stat_bonus[A_DEX]==2&&(f2&TR2_LIGHT));
            if(artefact==ART_TURGON_STAFF)assert(object->stat_bonus[A_GRA]==3&&object->skill_bonus[S_WIL]==3);
        }
        if(floor){object->iy=object->ix=10;object->held_m_idx=0;}
        if(strstr(scenario,"known"))a_info[artefact].found_num=1;
        if(strstr(scenario,"unknown")) {
            object->ident&=~(IDENT_KNOWN|IDENT_SPOIL);
            p_ptr->skill_base[S_PER]=p_ptr->skill_use[S_PER]=0;
            p_ptr->skill_base[S_SMT]=p_ptr->skill_use[S_SMT]=0;
            p_ptr->stat_use[A_GRA]=0;
        }
        if(strstr(scenario,"created")) {
            a_info[z_info->art_rand_max]=a_info[artefact];object->name1=z_info->art_rand_max;
        }
        if(strstr(scenario,"unseen"))cave_info[10][10]=0;
        if(floor)update_lore(PU_UPDATE_VIEW);else update_lore_aux(object);
        bool expected=!p_ptr->wizard&&!p_ptr->is_dead&&p_ptr->game_type==0
            &&!strstr(scenario,"unknown")&&!strstr(scenario,"created")&&!strstr(scenario,"unseen")
            &&!rejected;
        assert(earned(target)==expected);
        for(unsigned i=0;i<N_ELEMENTS(finds);++i)
            assert(earned(finds[i].achievement)==(expected&&finds[i].achievement==target));
        if(strstr(scenario,"unknown"))assert(!object_known_p(object));
        uint64_t timestamp=sil_achievement_unlocked_at(target);
        if(expected) {
            if(floor)update_lore(PU_UPDATE_VIEW);else update_lore_aux(object);
            assert(sil_achievement_unlocked_at(target)==timestamp);
        }
    } else if(!strncmp(scenario,"orome-",6)) {
        p_ptr->orome_quest=OROME_QUEST_ACTIVE;
        p_ptr->orome_target_type=OROME_TARGET_WOLF;
        if(strstr(scenario,"wolves"))p_ptr->orome_wolves_killed=100;
        if(strstr(scenario,"spiders"))p_ptr->orome_spiders_killed=80;
        if(strstr(scenario,"serpents"))p_ptr->orome_serpents_killed=60;
        if(strstr(scenario,"vampires"))p_ptr->orome_vampires_killed=30;
        if(strstr(scenario,"short")) {
            p_ptr->orome_wolves_killed=99;p_ptr->orome_spiders_killed=79;
            p_ptr->orome_serpents_killed=59;p_ptr->orome_vampires_killed=29;
        }
        check_orome_quest_completion();
        assert((p_ptr->orome_quest==OROME_QUEST_SUCCESS)==!strstr(scenario,"short"));
    } else assert(false);
    sil_achievements_flush();assert(sil_achievements_saved());
    printf("PASS reviewed achievement: %s\n",scenario);
}
'''

SCENARIOS = (
    "desperate-unique", "desperate-below", "desperate-above", "desperate-rounding",
    "desperate-ordinary", "desperate-ambient", "desperate-monster", "desperate-peaceful",
    "desperate-wizard", "desperate-practice", "desperate-nonlethal", "desperate-final", "desperate-phase",
    "aure-no-healing", "aure-partial-healing", "aure-regen", "aure-song", "aure-two-hp",
    "aure-without-hit", "aure-after-travel", "aure-peaceful", "aure-other-killer",
    "ice-cold-melee", "ice-monster",
    "forge-64", "forge-65", "forge-66", "forge-ordinary-65", "forge-wizard-65", "forge-drop-65", "forge-failed-delivery-65",
    "rare-mairon", "rare-starlight", "rare-calris", "rare-celeg",
    "orome-wolves", "orome-spiders", "orome-serpents", "orome-vampires", "orome-short",

) + tuple(
    f"rare-{name}{suffix}"
    for name in ("nauglamir", "gurthang", "melkor", "gothmog", "angrist", "belthronding",
                 "idril", "celebrimbor", "melian", "ulmo", "fingolfin", "turgon")
    for suffix in ("", "-floor", "-already-known", "-unknown", "-created", "-floor-unseen",
                   "-wizard", "-dead", "-practice")
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
    helper = OUT / "smith.c"
    helper.write_text(SMITH, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj"))))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                                  "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print", "message_flush",
             "get_sdl_config_path", "pause_with_text", "quest_typewriter_menu_pages", "ui_question_ask",
             "get_aim_dir", "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details",
             "inven_carry", "drop_near")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(helper), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for scenario in SCENARIOS:
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as data:
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                           env={**env, "ACH_REVIEW_CASE": scenario}, check=True, timeout=45)
    print(f"PASS {len(SCENARIOS)} reviewed achievement scenarios")


if __name__ == "__main__":
    main()
