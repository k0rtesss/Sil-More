#!/usr/bin/env python3
"""Exercise real same-weapon melee/throwing role changes and free-change credit.

--baseline compiles the committed active-weapon source as a negative control.
--tooltip-baseline omits the cache invalidations to isolate the stale-tooltip case.
Uses isolated engine fixture data; does not open player saves.
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
OUT = ROOT / "scripts/output/qa9-combat-weapon-roles"
CHECKS = r'''
#include "player/player-active-weapon.c"
extern void fixture_set_tooltip(const char* text);
extern bool fixture_tooltip_cleared(void);
#undef assert
#define assert(test) do { if (!(test)) { fprintf(stderr,"FAIL: %s line %d\n",#test,__LINE__); exit(1); } } while (0)
extern void fixture_reset_map(void);
void __wrap_handle_stuff(void) { }
void __wrap_update_stuff(void) { }
void __wrap_msg_print(cptr text) { (void)text; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action; (void)item; return true; }
void __wrap_tutorial_game_action_done(const char* action,const object_type* item)
{ (void)action; (void)item; }
static void choose_role(bool throwing)
{
    active_weapon_choice choice = {0};
    choice.o_ptr=&inventory[INVEN_WIELD];
    choice.item=choice.target_slot=INVEN_WIELD;
    choice.mode=throwing ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE;
    choice.kind=throwing ? PLAYER_ACTIVE_WEAPON_KIND_THROWING : PLAYER_ACTIVE_WEAPON_KIND_MELEE;
    choice.shield_item=ACTIVE_WEAPON_SHIELD_NONE;
    choice.arrow_item=-1;
    p_ptr->energy_use=0;
    apply_active_weapon_choice_internal(&choice);
    assert(player_active_weapon_kind()==choice.kind);
    assert(inventory[INVEN_WIELD].k_idx && inventory[INVEN_WIELD].number==1);
}
static void prepare(int abilities, bool throwing)
{
    memset(p_ptr,0,sizeof(*p_ptr)); fixture_reset_map();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    p_ptr->py=p_ptr->px=5; cave_m_idx[5][5]=-1;
    p_ptr->playing=true; p_ptr->chp=p_ptr->mhp=100;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_POLEARM,SV_SPEAR));
    inventory[INVEN_WIELD].number=1; p_ptr->equip_cnt=1;
    if(throwing) inventory[INVEN_WIELD].pickup_slot=PICKUP_SLOT_ACTIVE_THROWING;
    else inventory[INVEN_WIELD].pickup_slot=-1;
    p_ptr->active_weapon_mode=throwing ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->active_ability[S_MEL][MEL_WARDEN]=(abilities&1)!=0;
    p_ptr->active_ability[S_MEL][MEL_THROWING]=(abilities&2)!=0;
    p_ptr->active_ability[S_ARC][ARC_VERSATILITY]=(abilities&4)!=0;
    player_active_weapon_begin_player_turn();
}
void checks(void)
{
    for(int abilities=0;abilities<8;abilities++) {
        for(int start=0;start<2;start++) {
            prepare(abilities,start);
            bool free=(abilities&3)==3;
            choose_role(!start);
            if(p_ptr->energy_use!=(free?0:100))
                fprintf(stderr,"Role %s -> %s, Warden=%d Throwing=%d Versatility=%d: energy=%d expected=%d\n",
                    start?"Throwing":"Melee",start?"Melee":"Throwing",
                    (abilities&1)!=0,(abilities&2)!=0,(abilities&4)!=0,
                    p_ptr->energy_use,free?0:100);
            assert(p_ptr->energy_use==(free?0:100));
            assert(p_ptr->free_active_weapon_change_used==free);
            assert(p_ptr->previous_action[0]==(free ? ACTION_NOTHING :
                (start ? ACTION_READY_MELEE : ACTION_MISC)));
            /* A free switch cannot pay for another switch before an action. */
            choose_role(start);
            assert(p_ptr->energy_use==100);
            /* Beginning the next paid player action restores one credit. */
            player_active_weapon_begin_player_turn();
            choose_role(!start);
            assert(p_ptr->energy_use==(free?0:100));
        }
    }
    puts("PASS: 16 real same-Spear role/ability cases, second-switch payment, paid-action credit reset.");
    /* A pointer can remain over the attack panel throughout keyboard changes.
     * Its cached name/role must disappear for choices and direct shortcuts. */
    prepare(3,false);
    fixture_set_tooltip("Current active weapon: Melee: Spear.");
    choose_role(true);
    assert(fixture_tooltip_cleared());
    fixture_set_tooltip("Current active weapon: Throwing: Spear.");
    choose_role(false);
    assert(fixture_tooltip_cleared());
    /* Same-mode replacement has no mode-change callback to clear the cache. */
    fixture_set_tooltip("Current active weapon: Spear.");
    choose_role(false);
    assert(fixture_tooltip_cleared());
    fixture_set_tooltip("Current active weapon: Spear.");
    inventory[INVEN_WIELD].pickup_slot=PICKUP_SLOT_ACTIVE_THROWING;
    assert(player_set_active_weapon_mode(PLAYER_ACTIVE_WEAPON_RANGED_1,false,true));
    assert(fixture_tooltip_cleared());
    puts("PASS: real tooltip cache clears for both role directions, same-mode choices and direct mode changes.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--tooltip-baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    checks = CHECKS
    if args.baseline:
        baseline = OUT / "active-baseline.c"
        baseline.write_bytes(subprocess.check_output(["git", "-c", f"safe.directory={ROOT.as_posix()}",
            "show", "HEAD:src/player/player-active-weapon.c"], cwd=ROOT))
        checks = checks.replace('"player/player-active-weapon.c"', '"' + baseline.as_posix() + '"')
    elif args.tooltip_baseline:
        baseline = OUT / "tooltip-baseline.c"
        baseline.write_text((ROOT / "src/player/player-active-weapon.c").read_text()
            .replace("    sdl_object_tooltip_clear();\n", ""), encoding="utf-8")
        checks = checks.replace('"player/player-active-weapon.c"', '"' + baseline.as_posix() + '"')
    source = OUT / "check.c"
    source.write_text(prefix + "\nvoid fixture_reset_map(void) { reset_map(1); }\nextern void checks(void);\n"
        + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    support = OUT / "roles.c"
    support.write_text(checks, encoding="utf-8")
    tooltip = OUT / "tooltip-state.c"
    tooltip.write_text('#include "sdl/main-sdl-private.h"\n'
        'void fixture_set_tooltip(const char* text) { g_object_tooltip.active=true; '
        'SDL_strlcpy(g_object_tooltip.text,text,sizeof(g_object_tooltip.text)); }\n'
        'bool fixture_tooltip_cleared(void) { return !g_object_tooltip.active '
        '&& !g_object_tooltip.text[0]; }\n', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/player/player-active-weapon.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrapped = ("handle_stuff", "update_stuff", "msg_print", "tutorial_game_action_allowed", "tutorial_game_action_done")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(support), str(tooltip), "@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+s for s in wrapped), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
