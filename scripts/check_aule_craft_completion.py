#!/usr/bin/env python3
"""Exercise Aule's actual process_player craft completion with real templates.

Uses the existing initialized engine/Reforge fixture, temporary caches only.
--baseline compiles the prior committed completion order as a negative control.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_reforge_work_transaction import CHECKS as WORK_CHECKS, SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/aule-craft-completion"
EXTRA_SMITH = r'''
#include <assert.h>
int fixture_work_rating(void)
{
    object_type object = *smith_o_ptr;
    smithing_cost_type cost = smithing_cost;
    smith_alloy_state alloy = smith_alloy;
    int rating = smithing_work_difficulty();
    assert(!memcmp(&object, smith_o_ptr, sizeof(object)));
    assert(!memcmp(&cost, &smithing_cost, sizeof(cost)));
    assert(!memcmp(&alloy, &smith_alloy, sizeof(alloy)));
    return rating;
}
int fixture_ring(int pval, int* fee)
{
    object_prep(smith_o_ptr, lookup_kind(TV_RING, SV_RING_SECRETS));
    smith_o_ptr->pval = pval; smith_o_ptr->number = 1;
    smith_o_ptr->skill_bonus[S_PER] = pval;
    smith_o_ptr->ident |= IDENT_SPOIL;
    object_aware(smith_o_ptr); object_known(smith_o_ptr);
    int rating = object_difficulty(smith_o_ptr);
    *fee = smithing_cost.exp;
    assert(fixture_work_rating() == rating);
    p_ptr->smithing = p_ptr->smithing_leftover = 1;
    return rating;
}
'''
CHECKS = r'''
extern int fixture_ring(int,int*);
extern int fixture_work_rating(void);
void __wrap_tutorial_game_identified(const object_type* item, const char* event)
{ (void)item; (void)event; }
static int outputs(void)
{
    int count=0;
    for(int i=0;i<INVEN_TOTAL;i++)
        if(inventory[i].k_idx && inventory[i].unused1) count+=inventory[i].number;
    for(int i=0;i<player_carried_extra_entry_count();i++) {
        object_type* o=player_inventory_object(CARRIED_EXTRA_INDEX+i);
        if(o && o->k_idx && o->unused1) count+=o->number;
    }
    return count;
}
static void quest_work(int pval,bool beta,bool wrong_forge,bool wrong_depth,int expected)
{
    prepare(3); player_carried_extra_reset_store();
    op_ptr->opt[OPT_quest_2]=true; op_ptr->opt[OPT_quest_rules_beta]=beta;
    p_ptr->aule_quest=AULE_QUEST_ACTIVE;
    p_ptr->quest_test_sandbox=1;
    p_ptr->aule_level=wrong_depth?2:1;
    p_ptr->aule_forge_y=p_ptr->py; p_ptr->aule_forge_x=p_ptr->px+(wrong_forge?1:0);
    int fee; int rating=fixture_ring(pval,&fee);
    printf("%s: displayed/calculated difficulty %d\n",scenario,rating);
    long xp=p_ptr->new_exp, exp=p_ptr->exp; int uses=forge_uses(p_ptr->py,p_ptr->px);
    work_one();
    assert(p_ptr->aule_quest==expected && p_ptr->aule_last_object_diff==rating);
    assert(!smith_o_ptr->k_idx && !p_ptr->smithing && !p_ptr->smithing_leftover);
    assert(outputs()==1 && p_ptr->new_exp==xp-fee && p_ptr->exp==exp);
    assert(forge_uses(p_ptr->py,p_ptr->px)==uses-1);
}
static void checks(void)
{
    scenario="below Beta threshold"; quest_work(1,true,false,false,AULE_QUEST_ACTIVE);
    scenario="below classic threshold"; quest_work(1,false,false,false,AULE_QUEST_ACTIVE);
    scenario="classic qualifying work"; quest_work(3,false,false,false,AULE_QUEST_SUCCESS);
    scenario="Beta rejects classic quality"; quest_work(3,true,false,false,AULE_QUEST_ACTIVE);
    scenario="Beta qualifying work"; quest_work(4,true,false,false,AULE_QUEST_SUCCESS);
    scenario="Beta wrong forge"; quest_work(4,true,true,false,AULE_QUEST_ACTIVE);
    scenario="Beta wrong depth"; quest_work(4,true,false,true,AULE_QUEST_ACTIVE);
    scenario="classic any forge"; quest_work(4,false,true,true,AULE_QUEST_SUCCESS);
    scenario="paid Reforge completion"; prepare(3);
    op_ptr->opt[OPT_quest_2]=true; op_ptr->opt[OPT_quest_rules_beta]=true;
    p_ptr->aule_quest=AULE_QUEST_ACTIVE; p_ptr->aule_level=1;
    p_ptr->aule_forge_y=p_ptr->py; p_ptr->aule_forge_x=p_ptr->px;
    int prefix,turns,dex; assert(fixture_reforge_plan(selected_item,&prefix,&turns,&dex)>=0);
    assert(fixture_begin(selected_item,prefix));
    int rating=fixture_work_rating();
    long xp=p_ptr->new_exp; int uses=forge_uses(p_ptr->py,p_ptr->px);
    s16b drain[A_MAX]; memcpy(drain,p_ptr->stat_drain,sizeof(drain));
    p_ptr->smithing=p_ptr->smithing_leftover=1; work_one();
    assert(!smith_o_ptr->k_idx && !p_ptr->smithing_leftover && finished() && source_count()==1);
    assert(p_ptr->new_exp==xp && forge_uses(p_ptr->py,p_ptr->px)==uses);
    assert(!memcmp(drain,p_ptr->stat_drain,sizeof(drain)));
    assert(p_ptr->aule_last_object_diff==rating);
    assert(p_ptr->aule_quest==(rating>=25?AULE_QUEST_SUCCESS:AULE_QUEST_ACTIVE));
    puts("Aule completion: real ordinary/Reforge output, accepted fees, forge uses, work cleanup, classic/Beta quality and forge/depth gates PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + WORK_CHECKS[:WORK_CHECKS.index("static void checks(void)")]
                      + CHECKS + init + "    assert(!init_quest_info()); assert(!init_flavor_info()); flavor_init(); checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = OUT / "smith.c"
    smith.write_text(SMITH + EXTRA_SMITH, encoding="utf-8")
    core = ROOT / "src/dungeon/dungeon-player.c"
    if args.baseline:
        old = subprocess.check_output(["git", "-c", f"safe.directory={ROOT.as_posix()}",
            "show", "HEAD:src/dungeon/dungeon-player.c"], cwd=ROOT, text=True, encoding="utf-8")
        core = OUT / "core-baseline.c"
        core.write_text(old, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj", "/src/dungeon/dungeon-player.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", "-I" + str(ROOT / "src/dungeon"), str(source), str(smith), str(core), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *(f"-Wl,--wrap={name}" for name in (
            "inkey", "open_inventory_item_select_menu", "handle_stuff", "msg_print", "request_command",
            "process_command", "get_sdl_config_path", "tutorial_game_action_allowed",
            "level_gen_screen_begin", "run_quest_initiated_count", "tutorial_game_identified")), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
