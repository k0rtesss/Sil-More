#!/usr/bin/env python3
"""Check ordinary stair arrivals through the production Nienna lottery and generator."""

import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/nienna-generation"
SOURCES = (
    "src/level-generation/level-generation-quests.c",
    "src/level-generation/level-generation-terrain-placement.c",
)

CHECKS = r'''
extern int quest_lottery_winner;
extern int calculate_nearest_down_stair_distance_from(int y, int x);
extern bool stairs_within_los(int y, int x);
extern void dungeon(void);

static int attempts;
static int expected_return_stair;
enum { FORCED_MAP_SIZE = 121 };
static bool force_start_miss;
static int forced_draws, repeated_stair_scans;

void __wrap_keyboard_preset_maybe_show_first_game_selection(void) {}
void __wrap_sdl_touch_maybe_show_first_game_tutorial(void) {}
void __wrap_sdl_mouse_maybe_show_first_game_tutorial(void) {}

u32b __real_Rand_div(u32b bound);
u32b __wrap_Rand_div(u32b bound)
{
    u32b draw = __real_Rand_div(bound);
    if (force_start_miss && bound == FORCED_MAP_SIZE) {
        ++forced_draws;
        return 0;
    }
    return draw;
}

void __real_alloc_object_global(int set, int kind, int count, bool out_of_sight);
void __wrap_alloc_object_global(int set, int kind, int count, bool out_of_sight)
{
    if (!force_start_miss)
        __real_alloc_object_global(set, kind, count, out_of_sight);
}

int __real_calculate_nearest_down_stair_distance_from(int y, int x);
int __wrap_calculate_nearest_down_stair_distance_from(int y, int x)
{
    if (force_start_miss)
        ++repeated_stair_scans;
    return __real_calculate_nearest_down_stair_distance_from(y, x);
}

void __real_level_gen_screen_start_attempt(void);
void __wrap_level_gen_screen_start_attempt(void)
{
    assert(++attempts <= 20);
    __real_level_gen_screen_start_attempt();
}

bool __real_place_rubble_player(void);
bool __wrap_place_rubble_player(void)
{
    bool placed = __real_place_rubble_player();
    if (placed || quest_lottery_winner != QUEST_ID_NIENA)
        return placed;

    /* A sampled miss must not reject a Nienna map with a valid start. */
    static struct { int y, x; } stairs[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int count = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y) {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x) {
            if (!cave_down_stairs_bold(y, x))
                continue;
            stairs[count].y = y;
            stairs[count++].x = x;
        }
    }
    if (!count)
        return false;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y) {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x) {
            if (!cave_naked_bold(y, x) || (cave_info[y][x] & CAVE_ICKY))
                continue;
            int nearest = 9999;
            for (int i = 0; i < count; ++i) {
                int dist = distance(y, x, stairs[i].y, stairs[i].x);
                nearest = MIN(nearest, dist);
            }
            if (nearest < 87 || (p_ptr->create_stair && stairs_within_los(y, x)))
                continue;
            fprintf(stderr, "Attempt %d rejected legal start (%d,%d) at distance %d\n",
                attempts, y, x, nearest);
            assert(!"Nienna map rejected despite a legal start");
        }
    }
    return false;
}

/* Stop the real dungeon entry at its first player action. */
void __wrap_process_player(void)
{
    assert(cave_feat[p_ptr->py][p_ptr->px] == expected_return_stair);
    assert(!p_ptr->create_stair);
    expected_return_stair = 0;
    p_ptr->leaving = true;
    p_ptr->energy = 0;
}

static void prepare_stair(int depth, int feature)
{
    wipe_o_list();
    wipe_mon_list();
    player_wipe();
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    character_generated = true;
    p_ptr->playing = true;
    p_ptr->tutorial_deferred = true;
    p_ptr->depth = p_ptr->max_depth = depth;
    p_ptr->py = p_ptr->px = 10;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 30;
    p_ptr->chp = p_ptr->mhp = 100;
    cave_feat[10][10] = feature;
    cave_m_idx[10][10] = -1;
    turn = 2000;
    playerturn = 200;
    min_depth_counter = 0;
}

static void check_arrival(int initial_depth, int used_stair, int return_stair,
    bool expect_trial, unsigned seed)
{
    prepare_stair(initial_depth, used_stair);
    birth_discon_stair = return_stair == 0;
    if (used_stair == FEAT_MORE)
        do_cmd_go_down();
    else
        do_cmd_go_up();
    assert(p_ptr->depth == 18 && p_ptr->leaving);
    assert(p_ptr->create_stair == return_stair);

    attempts = 0;
    Rand_state_init(seed);
    generate_cave();
    assert(character_dungeon && p_ptr->create_stair == return_stair);
    int down_stairs = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
            down_stairs += cave_down_stairs_bold(y, x);
    printf("Seed %u arrival %d: lottery %d, down stairs %d, attempts %d\n",
        seed, return_stair, quest_lottery_winner, down_stairs, attempts);
    fflush(stdout);

    if (!expect_trial) {
        p_ptr->leaving = false;
        p_ptr->restoring = true;
        p_ptr->energy = 100;
        for (int i = 1; i < mon_max; ++i)
            mon_list[i].energy = 0;
        expected_return_stair = return_stair;
        dungeon();
        assert(!expected_return_stair);
    }

    if (expect_trial) {
        assert(quest_lottery_winner == QUEST_ID_NIENA);
        assert(p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT);
        assert(down_stairs > 0);
        assert(calculate_nearest_down_stair_distance_from(p_ptr->py, p_ptr->px) >= 87);
    } else {
        assert(quest_lottery_winner != QUEST_ID_NIENA);
        assert(p_ptr->niena_quest == NIENA_QUEST_NOT_STARTED && down_stairs > 0);
    }
}

static void check_forced_fallback(void)
{
    prepare_stair(18, FEAT_FLOOR);
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = FORCED_MAP_SIZE;
    p_ptr->py = p_ptr->px = 0;
    p_ptr->create_stair = FEAT_LESS;
    quest_lottery_winner = QUEST_ID_NIENA;
    for (int y = 0; y < FORCED_MAP_SIZE; ++y) {
        for (int x = 0; x < FORCED_MAP_SIZE; ++x) {
            cave_o_idx[y][x] = cave_m_idx[y][x] = 0;
            cave_info[y][x] = CAVE_WALL;
            cave_feat[y][x] = FEAT_WALL_EXTRA;
        }
    }
    cave_set_feat(2, 3, FEAT_MORE_SHAFT);
    cave_set_feat(110, 86, FEAT_MORE);
    cave_set_feat(88, 3, FEAT_FLOOR); /* Too close to the down shaft. */
    cave_set_feat(95, 5, FEAT_FLOOR); /* Far enough, but in LOS of the up stair. */
    cave_set_feat(95, 6, FEAT_LESS);
    cave_set_feat(98, 5, FEAT_FLOOR);
    assert(stairs_within_los(95, 5) && !stairs_within_los(98, 5));

    Rand_state_init(12345);
    force_start_miss = true;
    assert(__real_place_rubble_player());
    force_start_miss = false;
    assert(forced_draws == 202 && !repeated_stair_scans);
    assert(p_ptr->py == 98 && p_ptr->px == 5);
    assert(calculate_nearest_down_stair_distance_from(98, 5) == 87);

    cave_m_idx[98][5] = 0;
    cave_set_feat(98, 5, FEAT_WALL_EXTRA);
    p_ptr->py = p_ptr->px = 0;
    forced_draws = 0;
    Rand_state_init(12345);
    force_start_miss = true;
    assert(!__real_place_rubble_player());
    force_start_miss = false;
    assert(forced_draws == 202 && !repeated_stair_scans);
    assert(!p_ptr->py && !p_ptr->px);

    puts("Forced fallback: 101 missed probes, distance and stair LOS preserved, no legal start rejected.");
}

static void checks(void)
{
    prepare_stair(18, FEAT_FLOOR);
    flavor_init();
    drop_system_init();

    const unsigned seed = 7;
    Rand_state_init(seed);
    debug_run_quest_roulette();
    assert(quest_lottery_winner == QUEST_ID_NIENA);

    check_arrival(19, FEAT_LESS, FEAT_MORE, false, seed);
    check_arrival(20, FEAT_LESS_SHAFT, FEAT_MORE_SHAFT, false, seed);
    check_arrival(17, FEAT_MORE, FEAT_LESS, true, seed);
    check_arrival(19, FEAT_LESS, 0, true, seed);
    check_forced_fallback();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + "    checks(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + path + ".obj" for path in SOURCES))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in SOURCES),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=level_gen_screen_start_attempt", "-Wl,--wrap=process_player",
        "-Wl,--wrap=place_rubble_player",
        "-Wl,--wrap=Rand_div", "-Wl,--wrap=alloc_object_global",
        "-Wl,--wrap=calculate_nearest_down_stair_distance_from",
        "-Wl,--wrap=keyboard_preset_maybe_show_first_game_selection",
        "-Wl,--wrap=sdl_touch_maybe_show_first_game_tutorial",
        "-Wl,--wrap=sdl_mouse_maybe_show_first_game_tutorial",
        "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=env, check=True, timeout=100)


if __name__ == "__main__":
    main()
