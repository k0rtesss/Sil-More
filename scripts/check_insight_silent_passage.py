#!/usr/bin/env python3
"""Exercise Silent Passage attacks, hearing and exchanges in the production engine.

Run after the standard incremental build. Fixtures use temporary raw template
data; input and sight wrappers make opposing sensory cases deterministic.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

CHECKS = r'''
#include "melee/melee-movement.h"
#include "monster/monster-senses.h"
static bool sight;
static int audio_requests;
bool __wrap_get_rep_dir(int* dir) { *dir=4; return true; }
bool __wrap_get_aim_dir(int* dir,int range) { (void)range; *dir=5; return true; }
bool __wrap_monster_has_sight(const monster_type* m) { (void)m; return sight; }
void __wrap_sound(int type) { (void)type; ++audio_requests; }

static monster_type* fixture(int rules,int alert,int hp) {
    monster_type* m=combat_fixture(82,1000);
    p_ptr->insight_ruleset=rules;
    p_ptr->active_ability[S_STL][STL_SILENT_PASSAGE]=true;
    p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES]=true;
    p_ptr->have_ability[S_STL][STL_SILENT_PASSAGE]=true;
    p_ptr->have_ability[S_STL][STL_EXCHANGE_PLACES]=true;
    p_ptr->innate_ability[S_STL][STL_SILENT_PASSAGE]=true;
    p_ptr->innate_ability[S_STL][STL_EXCHANGE_PLACES]=true;
    p_ptr->skill_use[S_MEL]=p_ptr->skill_use[S_ARC]=1000;
    p_ptr->skill_base[S_MEL]=p_ptr->skill_base[S_ARC]=1000;
    p_ptr->skill_use[S_STL]=50;
    p_ptr->skill_base[S_STL]=50;
    stealth_score=50;
    p_ptr->mdd=20; p_ptr->mds=10; p_ptr->ads=10;
    p_ptr->stat_use[A_STR]=10;
    p_ptr->stat_base[A_STR]=10;
    player_quiver_reset_store();
    m->alertness=alert; m->hp=m->maxhp=hp;
    r_info[82].pd=r_info[82].ps=0;
    r_info[82].flags1 &= ~(RF1_NEVER_MOVE|RF1_HIDDEN_MOVE|RF1_PEACEFUL);
    r_info[82].flags2=RF2_SMART;
    r_info[82].flags3 &= ~(RF3_HURT_LITE|RF3_HURT_FIRE|RF3_HURT_COLD);
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));
    cave_m_idx[p_ptr->py][p_ptr->px]=-1;
    for(int y=0;y<p_ptr->cur_map_hgt;y++)
        for(int x=0;x<p_ptr->cur_map_wid;x++)
            cave_info[y][x]=CAVE_FIRE|CAVE_VIEW|CAVE_SEEN;
    playerturn=100; player_attacked=player_attack_audible=attacked_player=false;
    silent_passage_exchange_target=0;
    audio_requests=0; sight=false; cheat_timestop=false;
    return m;
}

static void check_melee(void) {
    int rules[]={INSIGHT_RULESET_CLASSIC,INSIGHT_RULESET_LEGACY,INSIGHT_RULESET_REWORKED};
    for(int i=0;i<3;i++) for(int alert=0;alert<2;alert++) {
        monster_type* m=fixture(rules[i],alert?ALERTNESS_ALERT:ALERTNESS_UNWARY,1);
        py_attack_aux(m->fy,m->fx,ATT_MAIN);
        assert(!m->r_idx && player_attacked && audio_requests>0);
        assert(player_attack_audible==(i!=2 || alert));
    }
    monster_type* m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_UNWARY,1);
    p_ptr->active_ability[S_STL][STL_SILENT_PASSAGE]=false;
    py_attack_aux(m->fy,m->fx,ATT_MAIN);
    assert(!m->r_idx && player_attack_audible);
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_UNWARY,20000);
    py_attack_aux(m->fy,m->fx,ATT_MAIN);
    assert(m->r_idx && player_attack_audible && m->alertness>=ALERTNESS_ALERT);
    m->alertness=ALERTNESS_UNWARY; m->hp=1;
    py_attack_aux(m->fy,m->fx,ATT_MAIN);
    assert(!m->r_idx && player_attack_audible); /* earlier wound stays audible */
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_UNWARY,1);
    p_ptr->skill_use[S_MEL]=-1000;
    py_attack_aux(m->fy,m->fx,ATT_MAIN);
    assert(m->r_idx && player_attack_audible);
    puts("Melee: quiet non-alert kills, ordinary audio requests, classic/legacy/disabled/alert/miss/wound and mixed-action noise PASS.");
}

static void check_projectiles(void) {
    for(int throwing=0;throwing<2;throwing++)
    for(int rules=INSIGHT_RULESET_CLASSIC;rules<=INSIGHT_RULESET_REWORKED;rules++)
    for(int alert=0;alert<2;alert++) {
        monster_type* m=fixture(rules,alert?ALERTNESS_ALERT:ALERTNESS_UNWARY,1);
        p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
        target_set_monster((int)(m-mon_list));
        if(throwing) {
            object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_POLEARM,SV_SPEAR));
            inventory[INVEN_WIELD].pickup_slot=PICKUP_SLOT_ACTIVE_THROWING;
            assert(player_active_throwing_weapon_slot()==INVEN_WIELD);
            do_cmd_throw_from_slot(INVEN_WIELD);
        } else {
            object_prep(&inventory[INVEN_BOW],lookup_kind(TV_BOW,SV_SHORT_BOW));
            p_ptr->ammo_tval=TV_ARROW;
            object_type arrow;
            object_prep(&arrow,lookup_kind(TV_ARROW,SV_NORMAL_ARROW)); arrow.number=3;
            assert(player_quiver_absorb_arrow(&arrow)==3);
            do_cmd_fire(1);
        }
        if(m->r_idx || !player_attacked) {
            fprintf(stderr,"projectile fixture: throwing=%d rules=%d alert=%d hp=%d attacked=%d energy=%d mode=%d quiver=%d\n",
                throwing,rules,alert,m->hp,player_attacked,p_ptr->energy_use,p_ptr->active_weapon_mode,player_quiver_arrow_count());
            for(int message=0;message<MIN(5,message_num());message++) fprintf(stderr,"%s\n",message_str(message));
            fprintf(stderr,"combat=%d pos=(%d,%d) target=(%d,%d) arc=%d bow=%d/%d sides=%d flags=%x\n",combat_number,p_ptr->py,p_ptr->px,m->fy,m->fx,p_ptr->skill_use[S_ARC],inventory[INVEN_BOW].dd,inventory[INVEN_BOW].ds,p_ptr->ads,cave_info[m->fy][m->fx]);
            if(combat_number) fprintf(stderr,"attack=%d evasion=%d damage=%d protection=%d\n",combat_rolls[0][0].att,combat_rolls[0][0].evn,combat_rolls[0][0].dam,combat_rolls[0][0].prot);
        }
        assert(!m->r_idx && player_attacked);
        if(player_attack_audible!=(rules!=INSIGHT_RULESET_REWORKED || alert))
            fprintf(stderr,"projectile noise: throw=%d rules=%d alert=%d audible=%d silent=%d have=%d combat=%d\n",throwing,rules,alert,player_attack_audible,p_ptr->active_ability[S_STL][STL_SILENT_PASSAGE],p_ptr->have_ability[S_STL][STL_SILENT_PASSAGE],combat_number);
        assert(player_attack_audible==(rules!=INSIGHT_RULESET_REWORKED || alert));
    }
    puts("Production bow and throw commands: direct non-alert kills are quiet, alert kills remain audible, ranged bookkeeping retained PASS.");
}

static int perception(bool audible,bool attacked,bool visible) {
    monster_type* m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    r_info[82].per=10; sight=visible;
    update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
    player_attack_audible=audible; player_attacked=attacked;
    Rand_state_init(6721);
    monster_perception(true,true,0);
    assert(!player_attacked && !player_attack_audible);
    assert(p_ptr->consecutive_attacks==(attacked?1:0));
    return m->alertness;
}
static void check_perception(void) {
    int baseline=perception(false,false,false);
    assert(perception(false,true,false)==baseline);
    assert(perception(true,true,false)==baseline+2);
    baseline=perception(false,false,true);
    assert(perception(false,true,true)==baseline+2); /* visual attack survives */
    assert(perception(true,true,true)==baseline+4);
    monster_type* m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_UNWARY,20000);
    player_attacked=true; attacked_player=true;
    monster_perception(true,true,-100);
    assert(m->alertness>=ALERTNESS_ALERT); /* real incoming combat still heard */
    puts("Production perception: quiet attacks retain Concentration and visual witnesses; hearing excludes only their attack bonus and keeps incoming combat PASS.");
}

static void check_exchange(void) {
    for(int rules=INSIGHT_RULESET_CLASSIC;rules<=INSIGHT_RULESET_REWORKED;rules++)
    for(int alert=0;alert<2;alert++) {
        monster_type* m=fixture(rules,alert?ALERTNESS_ALERT:ALERTNESS_MIN,20000);
        int start_alert=m->alertness;
        do_cmd_exchange();
        assert(p_ptr->py==10 && p_ptr->px==10 && m->fy==10 && m->fx==11);
        bool quiet=rules==INSIGHT_RULESET_REWORKED && !alert;
        if(quiet) {
            assert(m->alertness==start_alert && silent_passage_exchange_target>0);
            process_move(m,p_ptr->py,p_ptr->px,false);
            assert(m->alertness==start_alert);
            update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
            r_info[82].per=100;
            int action_stealth=stealth_score;
            p_ptr->skill_use[S_STL]+=5; /* destination bonus recomputation */
            monster_perception(true,true,action_stealth);
            assert(m->alertness==start_alert && !silent_passage_exchange_target);
            monster_perception(true,true,action_stealth);
            assert(m->alertness>=ALERTNESS_ALERT); /* later independent sensing */
        } else {
            assert(m->alertness>=ALERTNESS_ALERT && !silent_passage_exchange_target);
            if(alert) assert(combat_number>0); /* alert target retains reaction */
        }
    }
    monster_type* m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    do_cmd_exchange(); update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
    monster_perception(true,false,-100);
    assert(m->alertness>=ALERTNESS_ALERT); /* independent event in same action */
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    do_cmd_exchange(); update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
    r_info[82].per=100;
    monster_perception(true,true,p_ptr->skill_use[S_STL]-5);
    assert(m->alertness>=ALERTNESS_ALERT); /* unrelated terrain action noise */
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    do_cmd_exchange(); update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
    r_info[82].per=100; attacked_player=true;
    monster_perception(true,true,p_ptr->skill_use[S_STL]);
    assert(m->alertness>=ALERTNESS_ALERT); /* another foe's attack stays audible */
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    do_cmd_exchange(); int idx=(int)(m-mon_list); delete_monster_idx(idx);
    assert(!silent_passage_exchange_target); /* no immunity for slot reuse */
    m=fixture(INSIGHT_RULESET_REWORKED,ALERTNESS_MIN,20000);
    current_build_vault_exact_token=true;
    assert(place_monster_one(12,12,82,false,true,NULL));
    current_build_vault_exact_token=false;
    silent_passage_exchange_target=cave_m_idx[12][12];
    delete_monster_idx((int)(m-mon_list)); compact_monsters(0);
    assert(silent_passage_exchange_target==cave_m_idx[12][12]);
    wipe_mon_list(); assert(!silent_passage_exchange_target);
    puts("Exchange: original sleeping alertness preserved through immediate contact and one passive roll; later sensing, independent noise, alert reactions and old rules remain real PASS.");
}
'''


def main():
    out = engine.ROOT / "scripts/output/insight-silent-passage"
    out.mkdir(parents=True, exist_ok=True)
    prefix, original_main = engine.HARNESS.split("int main(int argc,char** argv)", 1)
    setup = original_main.split("    check_templates();", 1)[0]
    source = out / "check.c"
    source.write_text(prefix + CHECKS + "\nint main(int argc,char** argv)" + setup
                      + "    check_melee(); check_projectiles(); check_perception(); check_exchange();\n"
                      + '    puts("Silent Passage engine integration PASS."); SDL_Quit(); return 0;\n}\n',
                      encoding="utf-8")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith(("/src/main.c.obj",
                                      "/src/cmd/combat/cmd-ranged.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(engine.BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    wraps = ["get_rep_dir", "get_aim_dir", "monster_has_sight", "sound"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(engine.ROOT / "src/cmd/combat/cmd-ranged.c"),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *["-Wl,--wrap=" + name for name in wraps], "-o", str(exe)],
                   cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        assert Path(data).resolve().is_relative_to(out.resolve())
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
