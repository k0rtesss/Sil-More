#!/usr/bin/env python3
"""Real cache/combat checks; only the interactive question choice is injected.

Uses initialized engine templates and temporary data, never a player save.
Requires a current build-standard build. Lighting cases intentionally supply
stale render caches: environmental sources must decide remote visibility.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_vertical_cache as cache

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/vertical-combat-gameplay"

CHECKS = r'''
#include "cave/cave-vertical-combat.h"
#include "ui/question.h"
#include "cave/cave-light.h"
#include "cave/cave.h"
static int prompt_choice, prompt_calls, failures;
int __wrap_ui_question_ask(cptr title,cptr desc,const ui_question_option *options,
    int count,int y,int x,int initial)
{
    (void)title;(void)desc;(void)options;(void)y;(void)x;(void)initial;
    prompt_calls++;assert(count>0);return prompt_choice;
}
static void expect(bool condition,const char *description)
{
    printf("%s: %s\n",condition?"PASS":"FAIL",description);
    if(!condition)failures++;
}
static level_image *arena(void)
{
    fresh(9);memset(cave_light,0,MAX_DUNGEON_HGT*sizeof(*cave_light));
    assert(place_monster_one(20,22,31,false,false,NULL));
    mon_list[1].alertness=ALERTNESS_ALERT;
    level_image *lower=capture();
    fresh(8);character_generated=character_dungeon=true;character_icky=0;
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));player_quiver_reset_store();
    p_ptr->py=20;p_ptr->px=20;p_ptr->cur_light=2;
    cave_set_feat(20,21,FEAT_CHASM);
    cave_info[20][21]|=CAVE_SEEN|CAVE_VIEW|CAVE_FIRE;
    cave_m_idx[20][20]=-1;
    floors[0]=active=capture();floors[1]=lower;running=true;
    return lower;
}
static int emitter(int sign)
{
    for(int i=1;i<z_info->r_max;i++)
        if((sign<0?r_info[i].light<0:r_info[i].light>0)
            &&!(r_info[i].flags1&RF1_UNIQUE))return i;
    return 0;
}
static void add_emitter(level_image *floor,int race)
{
    /* Place via the real owning-map API, preserving the live player's state. */
    player_type before=*p_ptr;level_image_capture(active);
    level_image_restore(floor);
    assert(place_monster_one(20,23,race,false,false,NULL));
    level_image_capture(floor);level_image_restore(active);*p_ptr=before;
}
static void lighting(void)
{
    level_image *lower=arena();lower->info[20][22]|=CAVE_GLOW;
    expect(cave_vertical_combat_available(20,21),"fresh permanent glow with zero render cache is targetable");
    level_cache_clear();
    lower=arena();lower->light[20][22]=9;
    expect(!cave_vertical_combat_available(20,21),"departed hero torchlight in stale cache cannot reveal dark remote actors");
    level_cache_clear();
    lower=arena();lower->info[20][22]|=CAVE_GLOW;lower->light[20][22]=9;
    int dark=emitter(-1);assert(dark);add_emitter(lower,dark);
    /* Only the intended orc is an eligible target; the live emitter still
     * contributes physical light/darkness. This flag change is fixture-only. */
    u32b old_flags=r_info[dark].flags1;r_info[dark].flags1|=RF1_PEACEFUL;
    expect(!cave_vertical_combat_available(20,21),"physical darkness emitter overrides glow and stale positive light");
    printf("Darkness fixture race=%d light=%d\n",dark,r_info[dark].light);
    level_cache_clear();r_info[dark].flags1=old_flags;
    lower=arena();int bright=emitter(1);assert(bright);add_emitter(lower,bright);
    old_flags=r_info[bright].flags1;r_info[bright].flags1|=RF1_PEACEFUL;
    expect(cave_vertical_combat_available(20,21),"physical light emitter illuminates a fresh dark floor with zero render cache");
    printf("Light fixture race=%d light=%d\n",bright,r_info[bright].light);
    level_cache_clear();r_info[bright].flags1=old_flags;
}
static int kind(int tval)
{
    for(int i=1;i<z_info->k_max;i++)if(k_info[i].tval==tval)return i;
    return 0;
}
static void physical_sources_and_readonly(void)
{
    level_image *lower=arena();lower->feat[20][23]=FEAT_LAVA;
    expect(cave_vertical_combat_available(20,21),"current lava terrain illuminates without a visited render cache");
    lower->feat[20][23]=FEAT_WALL_EXTRA;lower->info[20][23]|=CAVE_WALL;
    lower->feat[20][24]=FEAT_LAVA;
    expect(!cave_vertical_combat_available(20,21),"canonical LOS blocks physical lava light behind a wall");
    level_cache_clear();
    lower=arena();
    int sword=kind(TV_SWORD);assert(sword);u32b old_flags=k_info[sword].flags1;
    k_info[sword].flags1|=TR1_SLAY_ORC;
    player_type before=*p_ptr;level_image_capture(active);level_image_restore(lower);
    object_type weapon;object_prep(&weapon,sword);weapon.number=1;
    assert(floor_carry(20,23,&weapon)>0);
    level_image_capture(lower);level_image_restore(active);*p_ptr=before;
    u64b rng=Rand_state_export();s32b clock=turn;
    byte *flow=malloc(sizeof(*cave_cost));assert(flow);
    memcpy(flow,cave_cost[FLOW_MONSTER_NOISE],sizeof(*cave_cost));
    byte fy=flow_center_y[FLOW_MONSTER_NOISE],fx=flow_center_x[FLOW_MONSTER_NOISE];
    int illumination=cave_light_cached_sample(lower,20,22);
    expect(illumination>0&&cave_vertical_combat_available(20,21),
        "real floor slay weapon glows from nearby vulnerable monsters");
    expect(rng==Rand_state_export()&&clock==turn
        &&!memcmp(flow,cave_cost[FLOW_MONSTER_NOISE],sizeof(*cave_cost))
        &&fy==flow_center_y[FLOW_MONSTER_NOISE]&&fx==flow_center_x[FLOW_MONSTER_NOISE],
        "physical sampling preserves RNG, clocks and live noise flow/centers");
    free(flow);
    bool same_los=true;
    for(int y=17;y<=24;y++)for(int x=17;x<=24;x++)
        if(los(20,20,y,x)!=cave_los_info(cave_info,20,20,y,x))same_los=false;
    expect(same_los,"parameterized cached LOS matches canonical active-map LOS");
    level_cache_clear();k_info[sword].flags1=old_flags;
}
static void ready_bow(void)
{
    object_prep(&inventory[INVEN_BOW],kind(TV_BOW));inventory[INVEN_BOW].number=1;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
    p_ptr->ammo_tval=TV_ARROW;p_ptr->skill_use[S_ARC]=200;
    object_type arrows;object_prep(&arrows,kind(TV_ARROW));arrows.number=5;
    assert(player_quiver_absorb_arrow(&arrows)==5);
    p_ptr->cur_light=10;prompt_choice=0;
}
static void shot_accounting(void)
{
    level_image *lower=arena();p_ptr->cur_light=10;
    object_prep(&inventory[INVEN_BOW],kind(TV_BOW));
    inventory[INVEN_BOW].number=1;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
    p_ptr->ammo_tval=TV_ARROW;p_ptr->skill_use[S_ARC]=30;
    object_type arrows;object_prep(&arrows,kind(TV_ARROW));arrows.number=5;
    assert(player_quiver_absorb_arrow(&arrows)==5);
    lower->monsters[1].hp=lower->monsters[1].maxhp=1000;
    /* The same index exists on the active floor, guarding target restoration. */
    assert(place_monster_one(25,25,41,false,false,NULL));
    p_ptr->target_set=true;p_ptr->target_who=1;p_ptr->target_row=p_ptr->target_col=25;
    p_ptr->health_who=1;
    int arrows_before=player_quiver_arrow_count();
    int weight_before=player_quiver_total_weight();
    int mon_before=mon_cnt,remote_before=lower->mon_cnt;
    int active_hp=mon_list[1].hp;
    prompt_choice=-1;prompt_calls=0;p_ptr->energy_use=0;
    expect(cave_vertical_combat_interact(20,21)&&prompt_calls==1
        &&p_ptr->energy_use==0&&player_quiver_arrow_count()==arrows_before,
        "cancelled real remote shot consumes no arrows or energy");
    prompt_choice=0;p_ptr->energy_use=0;
    expect(cave_vertical_combat_interact(20,21),"real remote shot executes");
    expect(player_quiver_arrow_count()==arrows_before-1
        &&player_quiver_total_weight()==weight_before-k_info[kind(TV_ARROW)].weight,
        "remote shot removes exactly one arrow and its quiver weight");
    expect(p_ptr->energy_use==100&&p_ptr->previous_action[0]==ACTION_ARCHERY,
        "remote shot charges one ordinary archery action");
    expect(p_ptr->depth==8&&p_ptr->py==20&&p_ptr->px==20
        &&p_ptr->target_set&&p_ptr->target_who==1&&p_ptr->health_who==1
        &&p_ptr->target_row==25&&p_ptr->target_col==25,
        "owning-map damage preserves active player coordinates and target index");
    expect(mon_cnt==mon_before&&lower->mon_cnt==remote_before
        &&mon_list[1].hp==active_hp&&lower->monsters_at[20][22]==1
        &&lower->monsters[1].hp>0,
        "nonlethal remote shot preserves both floors' monster ownership");
    level_cache_clear();
}
static void flying_ownership(void)
{
    level_image *lower=arena();
    player_type before=*p_ptr;level_image_capture(active);
    level_image_restore(lower);delete_monster(20,22);
    assert(place_monster_one(20,21,104,false,false,NULL));
    int source=cave_m_idx[20][21];monster_type *bat=&mon_list[source];
    assert(r_info[bat->r_idx].flags2&RF2_FLYING);
    bat->hp=bat->maxhp=100;bat->alertness=ALERTNESS_ALERT;bat->energy=99;
    int held=o_pop();assert(held);
    object_prep(&o_list[held],kind(TV_ARROW));o_list[held].number=3;
    o_list[held].held_m_idx=source;bat->hold_o_idx=held;
    level_image_capture(lower);level_image_restore(active);*p_ptr=before;
    int active_count=mon_cnt,remote_count=lower->mon_cnt;
    int active_objects=o_cnt,remote_objects=lower->o_cnt;
    int expected_energy=MIN(399,99+10*extract_energy[lower->monsters[source].mspeed])-100;
    cave_vertical_combat_reset();cave_vertical_combat_process();
    turn+=10;cave_vertical_combat_process();
    int arrived=0;
    for(int i=1;i<mon_max;i++)if(mon_list[i].r_idx==104){arrived=i;break;}
    expect(arrived>0&&mon_cnt==active_count+1&&lower->mon_cnt==remote_count-1
        &&!lower->monsters[source].r_idx&&!lower->monsters_at[20][21],
        "flying cached actor transfers exactly one monster to the active map");
    if(arrived) {
        int item=mon_list[arrived].hold_o_idx;
        expect(item>0&&o_list[item].held_m_idx==arrived&&o_list[item].number==3
            &&o_cnt==active_objects+1&&lower->o_cnt==remote_objects-1
            &&!lower->objects[held].k_idx,
            "flying actor transfers held stack exactly once with remapped ownership");
        expect(mon_list[arrived].energy==expected_energy
            &&!mon_list[arrived].ability_in_action,
            "flying transfer preserves action remainder without an unfinished action");
    }
    level_cache_clear();
}
static void brands(void)
{
    int arrow=kind(TV_ARROW);u32b old_arrow_flags=k_info[arrow].flags1;
    k_info[arrow].flags1|=TR1_BRAND_POIS;
    level_image *lower=arena();ready_bow();
    lower->monsters[1].hp=lower->monsters[1].maxhp=10000;
    expect(cave_vertical_combat_interact(20,21)&&lower->monsters[1].poisoned>0
        &&(lower->monsters[1].mflag&MFLAG_PLAYER_POISON),
        "real remote poison-brand hit applies canonical attributed poison");
    int poison=lower->monsters[1].poisoned,hp=lower->monsters[1].hp;
    u32b old_race_flags=r_info[31].flags1,old_spells=r_info[31].flags4;
    r_info[31].flags1|=RF1_NEVER_MOVE;r_info[31].flags4=0;
    cave_vertical_combat_reset();cave_vertical_combat_process();
    lower->monsters[1].energy=100-extract_energy[lower->monsters[1].mspeed];
    turn++;cave_vertical_combat_process();
    expect(poison>0&&lower->monsters[1].hp==hp-(poison+4)/5
        &&lower->monsters[1].poisoned==poison-(poison+4)/5,
        "one real remote actor action ticks poison exactly once on its owning floor");
    level_cache_clear();r_info[31].flags1=old_race_flags;r_info[31].flags4=old_spells;
    k_info[arrow].flags1=old_arrow_flags|TR1_VAMPIRIC;
    lower=arena();ready_bow();p_ptr->mhp=100;p_ptr->chp=50;
    lower->monsters[1].hp=1;
    expect(cave_vertical_combat_interact(20,21)&&!lower->monsters[1].r_idx
        &&p_ptr->chp==57&&p_ptr->killed_enemy_with_arrow,
        "living remote vampiric kill heals seven and records the archery kill");
    level_cache_clear();
    lower=arena();ready_bow();p_ptr->mhp=100;p_ptr->chp=50;
    lower->monsters[1].hp=1;
    u32b old_traits=r_info[31].flags3;r_info[31].flags3|=RF3_UNDEAD;
    expect(cave_vertical_combat_interact(20,21)&&!lower->monsters[1].r_idx
        &&p_ptr->chp==50,"nonliving remote vampiric kill does not heal");
    level_cache_clear();r_info[31].flags3=old_traits;k_info[arrow].flags1=old_arrow_flags;
    k_info[arrow].flags1|=TR1_BRAND_POIS;
    lower=arena();ready_bow();lower->monsters[1].hp=lower->monsters[1].maxhp=10000;
    u32b old_lore=l_list[31].flags3;
    r_info[31].flags3|=RF3_RES_POIS;l_list[31].flags3&=~RF3_RES_POIS;
    level_cache_poison_monster(9,1,player_quiver_arrow_object(player_quiver_selected_arrow_slot()),
        &inventory[INVEN_BOW],10);
    expect((l_list[31].flags3&RF3_RES_POIS)&&!lower->monsters[1].ml,
        "canonical explicit-shot poison transaction observes resistance then restores cached visibility");
    l_list[31].flags3&=~RF3_RES_POIS;
    expect(cave_vertical_combat_interact(20,21)&&!lower->monsters[1].poisoned
        &&(l_list[31].flags3&RF3_RES_POIS)&&!lower->monsters[1].ml,
        "explicit visible remote poison hit learns resistance without retaining cached visibility");
    level_cache_clear();r_info[31].flags3=old_traits;l_list[31].flags3=old_lore;
    k_info[arrow].flags1=old_arrow_flags;
}
static void run_checks(void)
{
    lighting();physical_sources_and_readonly();shot_accounting();flying_ownership();brands();assert(!failures);
    puts("Full-engine vertical combat gameplay PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = cache.ENGINE_FIXTURE[:cache.ENGINE_FIXTURE.index("static const char* guids[]")]
    init = cache.ENGINE_FIXTURE[cache.ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    # Reuse the established initialized-map fixture and real cache inclusion.
    helpers = cache.TESTS[:cache.TESTS.index("static vertical_vault_state blueprint(void)")]
    harness = prefix + cache.fixture_function("terminal_extra") + "\n" + cache.fixture_function("reset_map")
    harness += helpers + CHECKS + init
    harness += "    assert(!init_quest_info());assert(!init_st_info());assert(!init_partition_info());\n"
    harness += "    style_info=(style_type*)style_head.info_ptr;style_name=style_head.name_ptr;\n"
    harness += "    assert(!init_flavor_info());assert(!init_effect_info());assert(!init_skeleton_note_info());\n"
    harness += "    assert(!init_h_info());assert(!init_n_info());build_randart_tables();flavor_init();drop_system_init();character_generated=true;\n"
    harness += "    run_checks();SDL_Quit();return 0;\n}\n"
    sources = []
    for name, content in (("check.c", harness), ("writer.c", cache.WRITER), ("reader.c", cache.READER)):
        path = OUT / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj", "/src/level-generation/level-cache.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    wraps = ("level_cache_generation_links", "cave_vertical_generation_finish", "check_connectivity", "place_rubble_player", "terrain_history_finish", "update_view", "update_lore", "metarun_check_and_update_quests", "ui_question_ask")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources, "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + n for n in wraps), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
