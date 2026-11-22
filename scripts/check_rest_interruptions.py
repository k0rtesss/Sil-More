#!/usr/bin/env python3
"""Exercise running, resting and interruptions through production gameplay code.

Build Windows first. Uses isolated template data and the real engine objects;
no player save, personal configuration or interactive window is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/rest-interruptions"

CHECKS = r'''
#include "cave/cave.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"

static int notices;
void __real_run_step(int dir);
void __wrap_run_step(int dir)
{
    assert(dir != 5); /* Running in place is the rest shortcut. */
    __real_run_step(dir);
}
void __wrap_msg_print(cptr text) { if (text) ++notices; }
void __wrap_msg_format(cptr format, ...) { if (format) ++notices; }
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}

static void arm_actions(void)
{
    p_ptr->resting = 50;
    p_ptr->running = 20;
    p_ptr->command_rep = 7;
    p_ptr->smithing = 5;
    p_ptr->resting_light_off = false;
    notices = 0;
}

static void expect_continuing(void)
{
    assert(p_ptr->resting == 50 && p_ptr->running == 20);
    assert(p_ptr->command_rep == 7 && p_ptr->smithing == 5);
}

static void expect_interrupted(void)
{
    assert(!p_ptr->resting && !p_ptr->running);
    assert(!p_ptr->command_rep && !p_ptr->smithing);
}

static monster_type* prepare(int x, bool resonance)
{
    monster_type* m = combat_fixture(41, -100);
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    p_ptr->skill_use[S_PER] = 1000;
    p_ptr->active_ability[S_PER][PER_LISTEN] = resonance;
    character_generated = false; /* Suppress sound-marker drawing only. */
    playerturn = 100;
    ignore_monster_alarms = false;
    m->routine.territory = 0;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    monster_swap(m->fy, m->fx, 10, x);
    return m;
}

static void check_food(void)
{
    prepare(20, false);
    p_ptr->food = PY_FOOD_FULL;
    arm_actions();
    assert(set_food(PY_FOOD_FULL - 1));
    assert(p_ptr->food == PY_FOOD_FULL - 1 && notices == 1);
    expect_continuing();
    assert(set_food(PY_FOOD_ALERT - 1));
    expect_interrupted();
    arm_actions();
    assert(set_food(PY_FOOD_WEAK - 1));
    expect_interrupted();
    arm_actions();
    assert(set_food(PY_FOOD_STARVE - 1));
    expect_interrupted();
    arm_actions();
    assert(set_food(PY_FOOD_FULL - 1));
    expect_interrupted();
    puts("Food: losing Full keeps actions and message; hunger, weakness, starvation and recovery still interrupt: PASS");
}

static void check_alarms(void)
{
    for (int ignored = 0; ignored <= 1; ++ignored) {
        monster_type* m = prepare(20, false);
        ignore_monster_alarms = ignored;
        m->ml = true;
        arm_actions();
        int before_noise = m->noise;
        warning_message(m);
        assert(notices >= 1 && m->noise == before_noise + 10);
        if (ignored) expect_continuing(); else expect_interrupted();
        arm_actions();
        before_noise = m->noise;
        shriek(m);
        assert(notices >= 1 && m->noise == before_noise + 10);
        if (ignored) expect_continuing(); else expect_interrupted();
        /* Cover the ranged dispatcher too, which formerly disturbed before
         * calling the shared shriek handler. */
        u32b saved_spells = r_info[m->r_idx].flags4;
        r_info[m->r_idx].flags4 |= RF4_SHRIEK;
        arm_actions();
        assert(make_attack_ranged(m, 96 + 8));
        if (ignored) expect_continuing(); else expect_interrupted();
        r_info[m->r_idx].flags4 = saved_spells;
    }
    puts("Alarms: warning calls and direct/dispatched shrieks respect the toggle; messages and monster noise remain: PASS");
}

static void check_resonance(void)
{
    monster_type* m = prepare(20, true);
    assert(m->ml && (m->mflag & MFLAG_LISTEN));
    for (int x = 19; x >= 16; --x) {
        arm_actions();
        process_move(m, 10, x, false);
        assert(m->fx == x && m->ml && (m->mflag & MFLAG_LISTEN));
        expect_continuing();
    }

    /* Sound-only recognition must stop exempting the monster as soon as it
     * enters real sight, even if Resonance had already set its ml flag. */
    cave_info[10][15] |= CAVE_VIEW | CAVE_SEEN;
    arm_actions();
    process_move(m, 10, 15, false);
    assert(m->fx == 15 && (m->mflag & MFLAG_VIEW));
    assert(!(m->mflag & MFLAG_LISTEN));
    expect_interrupted();
    cave_info[10][14] |= CAVE_VIEW | CAVE_SEEN;
    arm_actions();
    process_move(m, 10, 14, false);
    expect_interrupted();

    m = prepare(13, true);
    assert(m->ml && (m->mflag & MFLAG_LISTEN));
    arm_actions();
    process_move(m, 10, 12, false);
    assert(m->fx == 12 && m->cdis == 1 && (m->mflag & MFLAG_LISTEN));
    expect_interrupted();

    /* Detection and telepathy remain ordinary visibility sources. */
    m = prepare(20, true);
    m->mflag |= MFLAG_MARK;
    arm_actions();
    process_move(m, 10, 19, false);
    assert(m->fx == 19 && !(m->mflag & MFLAG_LISTEN));
    expect_interrupted();
    m = prepare(20, true);
    p_ptr->telepathy = 1;
    arm_actions();
    process_move(m, 10, 19, false);
    assert(m->fx == 19 && !(m->mflag & MFLAG_LISTEN));
    expect_interrupted();
    puts("Resonance: repeated distant movement continues; real sight, adjacency, detection and telepathy interrupt: PASS");
}

static void check_danger(void)
{
    monster_type* m = prepare(10, true);
    ignore_monster_alarms = true;
    arm_actions();
    take_hit(1, "rest interruption fixture");
    expect_interrupted();
    /* Visual-recognition rules require the attacker to see a lit player. */
    p_ptr->cur_light = 4;
    cave_info[p_ptr->py][p_ptr->px] |= CAVE_VIEW | CAVE_SEEN | CAVE_FIRE;
    arm_actions();
    assert(make_attack_normal(m));
    expect_interrupted();
    u32b saved_spells = r_info[m->r_idx].flags4;
    r_info[m->r_idx].flags4 |= RF4_SCREECH;
    m->mana = 100;
    cave_info[m->fy][m->fx] |= CAVE_FIRE;
    arm_actions();
    assert(make_attack_ranged(m, 96 + 9));
    expect_interrupted();
    r_info[m->r_idx].flags4 = saved_spells;
    puts("Immediate danger: damage, adjacent melee and harmful screech still interrupt with alarm ignoring enabled: PASS");
}

static void run_map(int y, int x)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(5);
    p_ptr->cur_map_hgt = 12;
    p_ptr->cur_map_wid = 16;
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 16; col++) {
            cave_feat[row][col] = FEAT_WALL_EXTRA;
            cave_info[row][col] = CAVE_WALL | CAVE_MARK;
        }
    }
    p_ptr->py = y; p_ptr->px = x;
    p_ptr->playing = true;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    cave_m_idx[y][x] = -1;
    cave_set_feat(y, x, FEAT_FLOOR);
    cave_info[y][x] |= CAVE_MARK;
}

static void check_edge_run(void)
{
    /* The southeast corridor turns east at the southern perimeter. */
    run_map(9, 6);
    cave_set_feat(10, 7, FEAT_FLOOR);
    cave_set_feat(10, 8, FEAT_FLOOR);
    cave_set_feat(10, 9, FEAT_FLOOR);
    p_ptr->command_dir = 3;
    do_cmd_run();
    assert(p_ptr->py == 10 && p_ptr->px == 7 && p_ptr->running);

    /* Padding beyond a small level must not affect the route. */
    cave_m_idx[12][8] = 1;
    mon_list[1].r_idx = 1;
    mon_list[1].ml = true;
    u32b old_flags = r_info[1].flags1;
    r_info[1].flags1 |= RF1_NEVER_MOVE;
    run_step(0);
    assert(p_ptr->py == 10 && p_ptr->px == 8 && p_ptr->running);
    r_info[1].flags1 = old_flags;
    puts("Run follows a corridor bend at the map perimeter: PASS");
}

static void check_run_in_place(void)
{
    run_map(6, 6);
    p_ptr->command_dir = 5;
    p_ptr->command_arg = 7;
    do_cmd_run();
    assert(p_ptr->resting == 7 && p_ptr->running == 0 && p_ptr->focused);
    assert(p_ptr->energy_use == 100 && p_ptr->py == 6 && p_ptr->px == 6);
    puts("Choosing the current square with Run starts a timed rest: PASS");
}

static void check_run_past_illusion(void)
{
    for (int light = 0; light <= 3; light += 3) {
        run_map(6, 5);
        for (int x = 4; x <= 10; x++) cave_set_feat(6, x, FEAT_FLOOR);
        cave_set_feat(5, 7, FEAT_ILLUSORY_WALL);
        cave_info[5][7] |= CAVE_MARK | CAVE_SEEN;
        cave_light[5][7] = light;
        assert((cave_illusion_opacity(5, 7) == 255) == !light);
        p_ptr->command_dir = 6;
        do_cmd_run();
        assert(p_ptr->px == 6 && p_ptr->running);
        run_step(0);
        assert(p_ptr->px == (light ? 6 : 7));
        assert((p_ptr->running > 0) == !light);
        assert(cave_feat[5][7] == FEAT_ILLUSORY_WALL);
    }
    puts("Running passes an opaque illusory wall and notices one revealed by light: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + """
    check_edge_run(); check_run_in_place(); check_run_past_illusion();
    check_food(); check_alarms(); check_resonance(); check_danger();
    SDL_Quit(); return 0;
}
""", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                   if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrappers = ("msg_print", "msg_format", "message_flush", "handle_stuff", "run_step")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *("-Wl,--wrap=" + name for name in wrappers), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
