#!/usr/bin/env python3
"""Check Polearm Mastery's authored active-role rule with previews and real throws.

--baseline compiles committed combat-stats.c to prove the active-Throwing leak.
--rolls-only isolates command/roll checks from the prospective-preview checks.
Temporary engine data and input boundaries never touch player saves.
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
OUT = ROOT / "scripts/output/qa9-combat-polearm-roles"
PREVIEW = r'''
#include "player/player-active-weapon.c"
int fixture_preview_attack(bool throwing)
{
    active_weapon_preview preview;
    player_type before=*p_ptr;
    object_type items[INVEN_TOTAL]; memcpy(items,inventory,sizeof(items));
    if(!active_weapon_choice_preview(throwing ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE,
        INVEN_WIELD,INVEN_WIELD,-1,-1,&preview)) abort();
    if(memcmp(&before,p_ptr,sizeof(before)) || memcmp(items,inventory,sizeof(items))) abort();
    return preview.attack;
}
'''
CHECKS = r'''
#include "player/player-upkeep-internal.h"
#undef assert
#define assert(test) do { if (!(test)) { fprintf(stderr,"FAIL: %s line %d\n",#test,__LINE__); exit(1); } } while (0)
extern int fixture_preview_attack(bool throwing);
static bool aim_allowed=true;
static int attacks[8],attack_count;
bool __wrap_get_aim_dir(int* dir,int range) { (void)range; *dir=5; return aim_allowed; }
void __wrap_handle_stuff(void) { }
void __wrap_update_stuff(void) { }
void __wrap_msg_print(cptr text) { (void)text; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action; (void)item; return true; }
void __wrap_tutorial_game_action_done(const char* action,const object_type* item)
{ (void)action; (void)item; }
int __real_hit_roll(int,int,const monster_type*,const monster_type*,bool);
int __real_hit_roll_details(int,int,const monster_type*,const monster_type*,bool,int*,int*);
int __wrap_hit_roll(int attack,int evasion,const monster_type* a,const monster_type* d,bool show)
{
    if(a==PLAYER) attacks[attack_count++]=attack;
    return __real_hit_roll(attack,evasion,a,d,show);
}
int __wrap_hit_roll_details(int attack,int evasion,const monster_type* a,const monster_type* d,
    bool show,int* ad,int* ed)
{
    if(a==PLAYER) attacks[attack_count++]=attack;
    return __real_hit_roll_details(attack,evasion,a,d,show,ad,ed);
}
static void owned(int ability,bool active)
{
    p_ptr->innate_ability[S_MEL][ability]=true;
    p_ptr->have_ability[S_MEL][ability]=true;
    p_ptr->active_ability[S_MEL][ability]=active;
}
static void prepare(bool mastery,bool throwing,int scenario)
{
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    monster_type* m=combat_fixture(81,0);
    m->hp=m->maxhp=20000;
    p_ptr->depth=1;
    p_ptr->food=PY_FOOD_FULL-1000;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->skill_base[S_MEL]=8;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=3;
    owned(MEL_THROWING,true); owned(MEL_POLEARMS,mastery); owned(MEL_POWER_THROW,true);
    int main_kind=lookup_kind(TV_POLEARM,SV_SPEAR);
    if(scenario==1) main_kind=lookup_kind(TV_SWORD,SV_SHORT_SWORD);
    object_prep(&inventory[INVEN_WIELD],main_kind);
    inventory[INVEN_WIELD].number=scenario==1 ? 1 : 3;
    inventory[INVEN_WIELD].pickup_slot=throwing ? PICKUP_SLOT_ACTIVE_THROWING : -1;
    p_ptr->equip_cnt=1;
    p_ptr->active_weapon_mode=throwing ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE;
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++)
        cave_info[y][x]=CAVE_FIRE|CAVE_VIEW|CAVE_SEEN;
    cave_m_idx[p_ptr->py][p_ptr->px]=-1;
    if(scenario) {
        object_type source;
        object_prep(&source,lookup_kind(TV_POLEARM,scenario==1 ? SV_SPEAR : SV_HAND_AXE));
        source.number=3;
        assert(player_carried_extra_load(&source));
    }
    character_dungeon=character_generated=true;
    calc_bonuses();
    p_ptr->previous_action[1]=5;
    p_ptr->focused=false;
    target_set_monster((int)(m-mon_list));
    attack_count=0; memset(attacks,0,sizeof(attacks));
    aim_allowed=true; p_ptr->energy_use=0;
    player_active_weapon_begin_player_turn();
}
static void checks(bool with_preview)
{
    if(with_preview) {
    int previews[2][2][2];
    for(int mastery=0;mastery<2;mastery++)for(int current=0;current<2;current++) {
        prepare(mastery,current,0);
        for(int proposed=0;proposed<2;proposed++)
            previews[mastery][current][proposed]=fixture_preview_attack(proposed);
    }
    for(int current=0;current<2;current++)for(int proposed=0;proposed<2;proposed++) {
        assert(previews[1][current][proposed]-previews[0][current][proposed]==(proposed?0:2));
        assert(previews[0][current][proposed]==previews[0][!current][proposed]);
        assert(previews[1][current][proposed]==previews[1][!current][proposed]);
    }
    puts("PASS: 8 current/prospective-role Mastery previews, exact state restoration, melee +2 / Throwing +0.");
    }
    int rolls[3][2][2];
    for(int scenario=0;scenario<3;scenario++)for(int mastery=0;mastery<2;mastery++) {
        prepare(mastery,scenario==0,scenario);
        object_type before=inventory[INVEN_WIELD];
        object_type extra={0};
        if(scenario)extra=*player_inventory_object(CARRIED_EXTRA_INDEX);
        /* Cancelled aiming is free and preserves the physical source. */
        aim_allowed=false;
        if(scenario)do_cmd_throw(false); else do_cmd_throw_from_slot(INVEN_WIELD);
        assert(!p_ptr->energy_use && !attack_count);
        assert(!memcmp(&before,&inventory[INVEN_WIELD],sizeof(before)));
        if(scenario)assert(!memcmp(&extra,player_inventory_object(CARRIED_EXTRA_INDEX),sizeof(extra)));
        aim_allowed=true;
        if(scenario)do_cmd_throw(false); else do_cmd_throw_from_slot(INVEN_WIELD);
        assert(p_ptr->energy_use==100);
        assert(attack_count==(scenario?2:1));
        for(int i=0;i<attack_count;i++)rolls[scenario][mastery][i]=attacks[i];
        assert(inventory[INVEN_WIELD].number==(scenario?before.number:2));
        if(scenario)assert(player_inventory_object(CARRIED_EXTRA_INDEX)->number==2);
    }
    if(rolls[0][1][0]!=rolls[0][0][0])
        fprintf(stderr,"Active Throwing Mastery off/on roll modifiers: %d / %d (expected equal)\n",
            rolls[0][0][0],rolls[0][1][0]);
    assert(rolls[0][1][0]-rolls[0][0][0]==0);
    /* Active sword + Harness spear: Mastery applies to the thrown polearm only. */
    assert(rolls[1][1][0]-rolls[1][0][0]==2);
    assert(rolls[1][1][1]-rolls[1][0][1]==0);
    /* Active spear + Harness hand axe: remove the main spear's bonus from the
     * thrown roll, retain it for the actual melee strike. */
    assert(rolls[2][1][0]-rolls[2][0][0]==0);
    assert(rolls[2][1][1]-rolls[2][0][1]==2);
    puts("PASS: 6 real throw/Power Throw commands and roll modifiers, source-specific melee-role bonus, free cancellation and one-action/ammo accounting.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--rolls-only", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks(" + ("false" if args.rolls_only else "true")
        + "); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    preview = OUT / "preview.c"
    preview.write_text(PREVIEW, encoding="utf-8")
    stats = ROOT / "src/player/combat-stats.c"
    if args.baseline:
        stats = OUT / "stats-baseline.c"
        stats.write_bytes(subprocess.check_output(["git", "-c", f"safe.directory={ROOT.as_posix()}",
            "show", "HEAD:src/player/combat-stats.c"], cwd=ROOT))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/player/player-active-weapon.c.obj", "/src/player/combat-stats.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrapped = ("get_aim_dir", "handle_stuff", "update_stuff", "msg_print", "hit_roll", "hit_roll_details",
        "tutorial_game_action_allowed", "tutorial_game_action_done")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(preview), str(stats), "@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+s for s in wrapped), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
