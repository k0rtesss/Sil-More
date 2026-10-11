#!/usr/bin/env python3
"""Check the real contextual stairs command's confirmation/energy boundary.

Direction, confirmation and travel are injected boundaries. Production Alter
runs on initialized terrain/player state; genuine travel owns its own charge,
and refused travel is represented by a zero-cost travel boundary.
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
OUT = ROOT / "scripts/output/context-stairs"

CHECKS = r'''
static bool answer, travel_refuses, expected_up;
static int confirmations, travel_calls, inherited_energy;
static int requested_direction=5;
bool __wrap_get_rep_dir(int *direction) { *direction=requested_direction;return true; }
bool __wrap_get_check_near(int y,int x,cptr prompt)
{
    assert(y==p_ptr->py && x==p_ptr->px);
    assert(strstr(prompt,expected_up?"ascend":"descend"));
    confirmations++;
    inherited_energy=p_ptr->energy_use;
    return answer;
}
static void travel(bool up)
{
    assert(up==expected_up);travel_calls++;
    assert(p_ptr->energy_use==0);
    if(!travel_refuses)p_ptr->energy_use=100;
}
void __wrap_do_cmd_go_up(void) { travel(true); }
void __wrap_do_cmd_go_down(void) { travel(false); }
void __wrap_handle_stuff(void) { p_ptr->update=p_ptr->redraw=p_ptr->window=0; }
void __wrap_msg_print(cptr message) { (void)message; }
bool __wrap_tutorial_game_command_allowed(char command,int direction)
{ (void)command;(void)direction;return true; }
static void checks(void)
{
    const int stairs[]={FEAT_LESS,FEAT_LESS_SHAFT,FEAT_MORE,FEAT_MORE_SHAFT};
    for(unsigned f=0;f<N_ELEMENTS(stairs);f++)for(int mode=0;mode<3;mode++) {
        reset_map(3);cave_feat[p_ptr->py][p_ptr->px]=stairs[f];
        cave_info[p_ptr->py][p_ptr->px]|=CAVE_MARK;
        memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
        object_prep(&inventory[INVEN_LITE],lookup_kind(TV_LIGHT,SV_LIGHT_TORCH));
        inventory[INVEN_LITE].number=1;inventory[INVEN_LITE].timeout=281;
        p_ptr->depth=3;p_ptr->create_stair=FEAT_MORE_SHAFT;
        p_ptr->leaving=false;p_ptr->stairs_taken=12;p_ptr->staircasiness=5200;
        p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
        p_ptr->aule_quest=AULE_QUEST_ACTIVE;
        p_ptr->mandos_quest=MANDOS_QUEST_ACTIVE;
        p_ptr->varda_quest=VARDA_QUEST_GIVER_PRESENT;
        p_ptr->energy_use=0;p_ptr->confused=0;p_ptr->command_arg=0;
        turn=1001;playerturn=100;
        expected_up=f<2;answer=mode!=0;travel_refuses=mode==2;
        confirmations=travel_calls=0;inherited_energy=-1;
        do_cmd_alter();
        fprintf(stderr,"Context %s%s %s: prompt energy=%d travel calls=%d final energy=%d\n",
            expected_up?"up":"down",f%2?" shaft":" stairs",
            mode==0?"No":mode==1?"Yes":"Yes/refused",
            inherited_energy,travel_calls,p_ptr->energy_use);
        assert(confirmations==1 && inherited_energy==0);
        assert(travel_calls==(answer?1:0));
        assert(p_ptr->energy_use==(mode==1?100:0));
        assert(p_ptr->depth==3 && p_ptr->create_stair==FEAT_MORE_SHAFT && !p_ptr->leaving);
        assert(p_ptr->stairs_taken==12 && p_ptr->staircasiness==5200);
        assert(p_ptr->niena_quest==NIENA_QUEST_ACTIVE && p_ptr->aule_quest==AULE_QUEST_ACTIVE);
        assert(p_ptr->mandos_quest==MANDOS_QUEST_ACTIVE && p_ptr->varda_quest==VARDA_QUEST_GIVER_PRESENT);
        assert(turn==1001 && playerturn==100 && inventory[INVEN_LITE].timeout==281);
    }
    /* Alter's ordinary paid attempt policy is unchanged outside self stairs. */
    reset_map(3);requested_direction=6;
    cave_info[p_ptr->py][p_ptr->px+1]|=CAVE_MARK;
    p_ptr->energy_use=0;confirmations=travel_calls=0;
    do_cmd_alter();
    assert(p_ptr->energy_use==100 && !confirmations && !travel_calls);
    puts("Context stairs/shafts: No free and unchanged state; Yes delegates once without inherited energy; accepted travel owns 100, refusal stays free PASS");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true",
                        help="Restore only the old inherited cost in the two stairs branches")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "checks(); SDL_Quit(); return 0;\n}\n")
    command = ROOT / "src/cmd/world/cmd-interact.c"
    if args.baseline:
        original = command.read_text()
        for direction in ("ascend", "descend"):
            prompt = original.index('"Are you sure you wish to ' + direction + '? "')
            start = original.rfind("    else if ", 0, prompt)
            end = original.index("\n    }", prompt) + len("\n    }")
            branch = original[start:end]
            assert branch.count("p_ptr->energy_use = 0;") == 1
            original = original[:start] + branch.replace("p_ptr->energy_use = 0;", "") + original[end:]
        command = OUT / "alter-baseline.c"
        command.write_text(original)
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/cmd/world/cmd-interact.c.obj")
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(command), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in
        ("get_rep_dir", "get_check_near", "do_cmd_go_up", "do_cmd_go_down", "handle_stuff",
         "msg_print", "tutorial_game_command_allowed")), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
