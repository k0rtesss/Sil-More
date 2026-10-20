#!/usr/bin/env python3
"""Production travel, surface-death and Orodruth achievement controls.

Each scenario runs in a separate disposable profile under scripts/output.
Only presentation/input is doubled; movement, damage, traps and sight use the
current standard-build engine objects. No real profiles or accounts are used.
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
OUT = ROOT / "scripts/output/achievement-journeys"

TESTS = r'''
static bool confirm_travel;
bool fixture_horn(object_type*);
bool __wrap_get_check_near(int y,int x,cptr prompt)
{ (void)y;(void)x;(void)prompt;return confirm_travel; }
bool __wrap_get_aim_dir_vertical(int* dir,int range)
{ (void)range;*dir=DIRECTION_DOWN;return true; }
void __wrap_object_desc(char* buf,size_t size,const object_type* o,bool pref,int mode)
{ (void)o;(void)pref;(void)mode;SDL_strlcpy(buf,"fixture item",size); }
static void setup(void)
{
    prepare(41);sil_achievements_begin_run();
    p_ptr->depth=1;p_ptr->pcharacter=0;p_ptr->csp=p_ptr->msp=1000;
    p_ptr->skill_use[S_WIL]=10000;
    min_depth_counter=0;turn=playerturn=1;
    birth_ironman=false;utumno_corridors=false;confirm_travel=true;
}
static bool route(void)
{ return (p_ptr->achievement_flags&SIL_ACH_RUN_CHASM_SHAFT_ROUTE)!=0; }
static void entry(void) { sil_achievements_on_level_entry(false); }
static void down(int feat)
{
    p_ptr->leaving=false;p_ptr->staircasiness=0;
    cave_feat[p_ptr->py][p_ptr->px]=feat;
    do_cmd_go_down();entry();
}
static void finish_route(void)
{
    while(p_ptr->depth<MORGOTH_DEPTH) {
        int old=p_ptr->depth;down(FEAT_MORE_SHAFT);
        assert(p_ptr->depth==MIN(old+2,MORGOTH_DEPTH));
    }
}
static void restrict_run(const char* mode)
{
    if(!strcmp(mode,"wizard"))p_ptr->wizard=true;
    else if(!strcmp(mode,"practice"))p_ptr->game_type=1;
    else if(!strcmp(mode,"tainted"))p_ptr->achievement_flags|=SIL_ACH_RUN_TAINTED;
    else if(!strcmp(mode,"cheat"))op_ptr->opt[OPT_CHEAT]=true;
    else if(!strcmp(mode,"score"))p_ptr->noscore=1;
}
static void checks(void)
{
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();setup();
    const char* scenario=getenv("JOURNEY_SCENARIO");assert(scenario);
    bool positive=false;
    int target=SIL_ACH_NO_STEPS_REQUIRED;
    if(!strncmp(scenario,"route-",6)) {
        const char* mode=scenario+6;
        assert(route());
        if(!strcmp(mode,"stairs")) { down(FEAT_MORE);assert(p_ptr->depth==2&&!route()); }
        else if(!strcmp(mode,"cancel")) {
            p_ptr->niena_quest=NIENA_QUEST_ACTIVE;confirm_travel=false;
            down(FEAT_MORE);assert(p_ptr->depth==1&&route());
            p_ptr->niena_quest=NIENA_QUEST_NOT_STARTED;confirm_travel=true;positive=true;
        }
        else if(!strcmp(mode,"up")) {
            down(FEAT_MORE_SHAFT);assert(p_ptr->depth==3);
            cave_feat[p_ptr->py][p_ptr->px]=FEAT_LESS;
            p_ptr->staircasiness=0;p_ptr->leaving=false;
            do_cmd_go_up();entry();assert(p_ptr->depth==2&&route());positive=true;
        }
        else if(!strcmp(mode,"chasm")||!strcmp(mode,"false-floor")) {
            int feat=!strcmp(mode,"chasm")?FEAT_CHASM:FEAT_TRAP_false_FLOOR;
            cave_feat[p_ptr->py][p_ptr->px]=feat;
            hit_trap(p_ptr->py,p_ptr->px);entry();
            positive=feat==FEAT_CHASM;
            assert(p_ptr->depth==(positive?3:2)&&route()==positive);
        }
        else if(!strcmp(mode,"teleport")) {
            /* Surface forces downward teleport, even while standing on shaft. */
            p_ptr->depth=0;cave_feat[p_ptr->py][p_ptr->px]=FEAT_MORE_SHAFT;
            teleport_player_level();entry();assert(p_ptr->depth==1&&!route());
        }
        else if(!strcmp(mode,"horn")) {
            object_type horn;item(&horn,TV_HORN,SV_HORN_BLASTING,1);
            cave_feat[p_ptr->py][p_ptr->px]=FEAT_MORE_SHAFT;
            assert(fixture_horn(&horn));entry();assert(p_ptr->depth==2&&!route());
        }
        else if(!strcmp(mode,"legacy")) p_ptr->achievement_flags=SIL_ACH_RUN_STARTED;
        else if(!strcmp(mode,"shaft")) positive=true;
        else if(!strcmp(mode,"dead")) { p_ptr->is_dead=true; }
        else restrict_run(mode);
        assert(!earned(target));finish_route();
        assert(earned(target)==positive);
        if(positive) { entry();assert(earned(target)); }
    }
    else if(!strncmp(scenario,"death-",6)) {
        target=SIL_ACH_THERE_AND_ALMOST_BACK_AGAIN;
        const char* mode=scenario+6;p_ptr->depth=0;p_ptr->chp=p_ptr->mhp=100;
        assert(!earned(target));
        if(!strcmp(mode,"deep"))p_ptr->depth=1;
        else if(!strcmp(mode,"escaped"))p_ptr->escaped=true;
        else if(!strcmp(mode,"victory"))p_ptr->morgoth_slain=true;
        else if(!strcmp(mode,"amulet")) {
            item(&inventory[INVEN_NECK],TV_AMULET,SV_AMULET_LAST_CHANCES,1);
            p_ptr->equip_cnt=1;
        }
        else restrict_run(mode);
        if(!strcmp(mode,"living")) { entry();take_hit(1,"fixture surface wound");assert(!p_ptr->is_dead); }
        else if(!strcmp(mode,"suicide")) { p_ptr->is_dead=true;p_ptr->playing=false;sil_achievements_on_run_completed(); }
        else {
            take_hit(101,"fixture surface wound");
            if(!strcmp(mode,"amulet"))assert(p_ptr->chp>0&&!p_ptr->is_dead&&!inventory[INVEN_NECK].k_idx);
            else assert(p_ptr->is_dead);
        }
        positive=!strcmp(mode,"real");assert(earned(target)==positive);
    }
    else if(!strncmp(scenario,"utumno-",7)) {
        target=SIL_ACH_THE_HALLS_OF_UTUMNO;
        const char* mode=scenario+7;
        if(!strcmp(mode,"arrival")) {
            p_ptr->depth=MORGOTH_DEPTH;entry();assert(!earned(target));
            utumno_corridors=true;down(FEAT_MORE_SHAFT);
            assert(p_ptr->depth==UTUMNO_DEPTH);positive=true;
        }
        else { p_ptr->depth=!strcmp(mode,"forge")?UTUMNO_FORGE_DEPTH:UTUMNO_DEPTH;restrict_run(mode);entry(); }
        assert(earned(target)==positive);
    }
    else if(!strncmp(scenario,"forge-",6)) {
        target=SIL_ACH_WHERE_GROND_WAS_WROUGHT;
        const char* mode=scenario+6;
        p_ptr->depth=UTUMNO_FORGE_DEPTH;
        cave_feat[9][9]=FEAT_FORGE_UNIQUE_HEAD;cave_info[9][9]=CAVE_MARK;
        entry();assert(!earned(target));
        if(!strcmp(mode,"ordinary"))cave_feat[9][9]=FEAT_FORGE_HEAD;
        if(!strcmp(mode,"wrong-depth"))p_ptr->depth=UTUMNO_DEPTH;
        restrict_run(mode);
        if(strcmp(mode,"remembered"))cave_info[9][9]|=CAVE_SEEN;
        note_spot(9,9);positive=!strcmp(mode,"seen");
        assert(earned(target)==positive);
        if(positive) { note_spot(9,9);assert(events[target]==1); }
    }
    else assert(false);
    sil_achievements_flush();assert(sil_achievements_saved());
    printf("PASS %s: %s\n",scenario,positive?"unlocked":"excluded");
}
'''

SCENARIOS = (
    *("route-" + x for x in ("stairs", "false-floor", "teleport", "horn", "legacy",
                              "wizard", "practice", "tainted", "cheat", "score", "dead",
                              "shaft", "cancel", "up", "chasm")),
    *("death-" + x for x in ("deep", "living", "suicide", "amulet", "escaped", "victory",
                              "wizard", "practice", "tainted", "cheat", "score", "real")),
    *("utumno-" + x for x in ("forge", "wizard", "practice", "tainted", "arrival")),
    *("forge-" + x for x in ("remembered", "ordinary", "wrong-depth", "wizard", "practice", "tainted", "seen")),
)


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
    helper = OUT / "horn.c"
    helper.write_text('#include "use-obj.c"\nbool fixture_horn(object_type* o) { bool ident=false; return play_instrument(o,&ident); }\n', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects
                                   if not o.endswith(("/src/main.c.obj", "/src/use-obj.c.obj"))))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                                  "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print", "message_flush",
             "get_sdl_config_path", "pause_with_text", "quest_typewriter_menu_pages", "ui_question_ask",
             "get_aim_dir", "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details",
             "get_check_near", "get_aim_dir_vertical", "object_desc")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(helper), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + n for n in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for scenario in SCENARIOS:
        with tempfile.TemporaryDirectory(prefix=scenario + "-", dir=OUT) as data:
            env["JOURNEY_SCENARIO"] = scenario
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
