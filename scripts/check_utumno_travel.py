#!/usr/bin/env python3
"""Exercise the real stair commands against built engine objects.

Run build-incremental.ps1 first. Uses isolated temporary template caches.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

import check_new_monsters as engine


CHECKS = r'''
static void travel_fixture(int depth, int feat)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(depth);
    cave_feat[p_ptr->py][p_ptr->px] = feat;
    p_ptr->chp = p_ptr->mhp = 100;
    min_depth_counter = 0;
    op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
    utumno_corridors = true;
    birth_ironman = false;
    birth_discon_stair = false;
    g_vault_name[0] = '\0';
}

static void check_blocked_descent(int depth, int feat)
{
    int old_stairs = p_ptr->stairs_taken;
    do_cmd_go_down();
    assert(p_ptr->depth == depth);
    assert(cave_feat[p_ptr->py][p_ptr->px] == feat);
    assert(!p_ptr->leaving && !p_ptr->energy_use);
    assert(p_ptr->stairs_taken == old_stairs);
    assert(!p_ptr->create_stair);
}

static void check_utumno_travel(void)
{
    /* Shafts never follow ordinary +1/+2 routing at the throne depth. */
    travel_fixture(MORGOTH_DEPTH, FEAT_MORE_SHAFT);
    utumno_corridors = false;
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE_SHAFT);

    travel_fixture(MORGOTH_DEPTH, FEAT_MORE_SHAFT);
    min_depth_counter = 19 * 150000;
    assert(min_depth() == MORGOTH_DEPTH);
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE_SHAFT);

    travel_fixture(MORGOTH_DEPTH, FEAT_MORE_SHAFT);
    p_ptr->morgoth_hall_entered = true;
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE_SHAFT);

    travel_fixture(MORGOTH_DEPTH, FEAT_MORE_SHAFT);
    p_ptr->on_the_run = true;
    assert(min_depth() == 0);
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE_SHAFT);

    travel_fixture(MORGOTH_DEPTH, FEAT_MORE_SHAFT);
    p_ptr->staircasiness = 10000; /* Ordinary stairs would always crumble. */
    do_cmd_go_down();
    assert(p_ptr->depth == UTUMNO_DEPTH && p_ptr->leaving);
    assert(!p_ptr->create_stair && p_ptr->chp == 100);
    assert(p_ptr->stairs_taken == 1 && p_ptr->energy_use == 100);
    assert(!p_ptr->utumno_forge_visited);

    /* A later option change or expired clock cannot strand the expedition. */
    travel_fixture(UTUMNO_DEPTH, FEAT_MORE);
    utumno_corridors = false;
    min_depth_counter = 19 * 150000;
    p_ptr->staircasiness = 10000;
    do_cmd_go_down();
    assert(p_ptr->depth == UTUMNO_FORGE_DEPTH && p_ptr->leaving);
    assert(!p_ptr->create_stair && p_ptr->chp == 100);

    travel_fixture(UTUMNO_DEPTH, FEAT_LESS_SHAFT);
    do_cmd_go_up();
    assert(p_ptr->depth == UTUMNO_DEPTH && !p_ptr->leaving);

    travel_fixture(UTUMNO_FORGE_DEPTH, FEAT_LESS);
    utumno_corridors = false;
    birth_ironman = true;
    p_ptr->oath_type = OATH_IRON;
    p_ptr->utumno_forge_visited = true;
    p_ptr->staircasiness = 10000;
    min_depth_counter = 19 * 150000;
    do_cmd_go_up();
    assert(p_ptr->depth == MORGOTH_DEPTH && p_ptr->leaving);
    assert(p_ptr->utumno_return_to_throne);
    assert(p_ptr->utumno_forge_visited && !p_ptr->oaths_broken);
    assert(!p_ptr->on_the_run && !p_ptr->create_stair && p_ptr->chp == 100);

    travel_fixture(MORGOTH_DEPTH, FEAT_MORE);
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE);
    p_ptr->utumno_forge_visited = true;
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE);
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_MORE_SHAFT;
    check_blocked_descent(MORGOTH_DEPTH, FEAT_MORE_SHAFT);

    travel_fixture(UTUMNO_FORGE_DEPTH, FEAT_MORE);
    check_blocked_descent(UTUMNO_FORGE_DEPTH, FEAT_MORE);

    /* Ordinary travel and the throne's normal retreat restriction survive. */
    travel_fixture(18, FEAT_MORE_SHAFT);
    do_cmd_go_down();
    assert(p_ptr->depth == MORGOTH_DEPTH && p_ptr->leaving);
    assert(p_ptr->create_stair == FEAT_LESS_SHAFT);
    travel_fixture(19, FEAT_MORE);
    do_cmd_go_down();
    assert(p_ptr->depth == MORGOTH_DEPTH && p_ptr->create_stair == FEAT_LESS);
    travel_fixture(MORGOTH_DEPTH, FEAT_LESS);
    do_cmd_go_up();
    assert(p_ptr->depth == 19 && p_ptr->leaving);
    travel_fixture(MORGOTH_DEPTH, FEAT_LESS);
    p_ptr->morgoth_hall_entered = true;
    do_cmd_go_up();
    assert(p_ptr->depth == MORGOTH_DEPTH && !p_ptr->leaving);
    puts("Utumno travel: directed routing, locks, option changes, Ironman/oath return, no rerolls, ordinary stairs PASS.");
}

static void check_utumno_earthquakes(void)
{
    bool ordinary_floor_changed = false;
    bool branch_wall_changed = false;
    for (int depth = 19; depth <= UTUMNO_FORGE_DEPTH; ++depth)
    {
        if (depth == MORGOTH_DEPTH || depth == 21) continue;
        for (int option = 0; option <= 1; ++option)
        for (int seed = 1; seed <= 20; ++seed)
        {
            travel_fixture(depth, FEAT_FLOOR);
            utumno_corridors = option;
            Rand_state_init(seed);
            /* An exposed crossing, openable door, and the indispensable
             * stairs/forge. The player is outside the earthquake radius. */
            for (int y = 34; y <= 46; ++y)
                for (int x = 34; x <= 46; ++x)
                    cave_set_feat(y, x, FEAT_LAVA);
            for (int x = 34; x <= 46; ++x)
                cave_set_feat(40, x, FEAT_FLOOR);
            cave_set_feat(40, 42, FEAT_DOOR_HEAD);
            cave_set_feat(41, 40, FEAT_LESS);
            cave_set_feat(41, 41, FEAT_MORE);
            cave_set_feat(41, 42, FEAT_FORGE_UNIQUE_TAIL);
            cave_set_feat(42, 40, FEAT_WALL_PERM);
            cave_set_feat(39, 40, FEAT_WALL_EXTRA);
            earthquake(40, 40, -1, -1, 6, -1);
            assert(cave_feat[41][40] == FEAT_LESS);
            assert(cave_feat[41][41] == FEAT_MORE);
            assert(cave_feat[41][42] == FEAT_FORGE_UNIQUE_TAIL);
            assert(cave_feat[42][40] == FEAT_WALL_PERM);
            for (int x = 34; x <= 46; ++x)
            {
                int expected = x == 42 ? FEAT_DOOR_HEAD : FEAT_FLOOR;
                if (depth >= UTUMNO_DEPTH)
                    assert(cave_feat[40][x] == expected);
                else if (cave_feat[40][x] != expected)
                    ordinary_floor_changed = true;
            }
            if (depth >= UTUMNO_DEPTH && cave_feat[39][40] != FEAT_WALL_EXTRA)
                branch_wall_changed = true;
        }
    }
    assert(ordinary_floor_changed && branch_wall_changed);

    /* Terrain protection must not turn Shatter into harmless scenery. */
    travel_fixture(UTUMNO_DEPTH, FEAT_FLOOR);
    p_ptr->py = 40;
    p_ptr->px = 39;
    p_ptr->chp = p_ptr->mhp = 10000;
    cave_m_idx[40][39] = -1;
    earthquake(40, 40, -1, -1, 6, -1);
    assert(p_ptr->chp < 10000 && !p_ptr->is_dead);
    puts("Utumno earthquakes: 120 seeded crossings retain stairs/forge/routes; walls collapse, ordinary floor mutates, player damage remains PASS.");
}
'''


def main():
    out = engine.ROOT / "scripts/output/utumno-travel"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    harness = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    begin = harness.index("    check_templates();", harness.index("int main("))
    end = harness.index("    SDL_Quit();", begin)
    harness = harness[:begin] + "    check_utumno_travel();\n    check_utumno_earthquakes();\n" + harness[end:]
    source.write_text(harness, encoding="utf-8")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/cmd/movement/cmd-movement.c.obj",
            "/src/spell/spell-terrain.c.obj"))),
        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(engine.BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(engine.ROOT / "src/cmd/movement/cmd-movement.c"),
        str(engine.ROOT / "src/spell/spell-terrain.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        assert Path(data).resolve().is_relative_to(out.resolve())
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
