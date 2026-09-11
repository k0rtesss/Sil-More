#!/usr/bin/env python3
"""Check the first-level tutorial monster safeguard with production code.

The C fixture extracts the exact tutorial trigger and its helper bodies
from their production files, then calls them with only the required globals
and callbacks. The Python checks also keep the final population check at the
intended point in ``generate_cave``.
"""

from pathlib import Path
import os
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-start-check"
GENERATION = ROOT / "src/level-generation/level-generation.c"
TUTORIAL_GAME = ROOT / "src/tutorial/tutorial-game.c"


def extract_function(path, name):
    """Extract one complete C function, preserving the production body."""
    source = path.read_text(encoding="utf-8-sig")
    marker = source.index(name)
    start = source.rfind("\n", 0, marker) + 1
    brace = source.index("{", marker)
    depth = 0
    in_block_comment = False
    in_line_comment = False
    in_string = False
    escaped = False
    for index in range(brace, len(source)):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
            continue
        if in_line_comment:
            if char == "\n":
                in_line_comment = False
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == "/" and next_char == "*":
            in_block_comment = True
            continue
        if char == "/" and next_char == "/":
            in_line_comment = True
            continue
        if char == '"':
            in_string = True
            continue
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"Unclosed function: {name}")


HELPERS = "\n\n".join([
    extract_function(GENERATION, "tutorial_start_needs_clear_area"),
    extract_function(TUTORIAL_GAME, "static bool tutorial_monster_observable"),
    extract_function(TUTORIAL_GAME, "static bool tutorial_first_monster_target"),
    extract_function(TUTORIAL_GAME, "static bool tutorial_stealth_target"),
    extract_function(TUTORIAL_GAME, "bool tutorial_game_first_monster_triggered"),
    extract_function(GENERATION, "static bool tutorial_start_triggers_monster"),
])


HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tutorial/tutorial.h"

static player_type player_body;
player_type *p_ptr = &player_body;
static monster_type monster_body[8];
monster_type *mon_list = monster_body;
s16b mon_max;
s32b playerturn;
static monster_race race_body[3];
monster_race *r_info = race_body;
static u16b cave_body[MAX_DUNGEON_HGT][256];
u16b (*cave_info)[256] = cave_body;
static int merciless_blocked_index;
static int cowardly_blocked_index;

bool merciless_attack(monster_type *monster)
{
    return merciless_blocked_index > 0
        && monster == &monster_body[merciless_blocked_index];
}
bool cowardly_attack(monster_type *monster)
{
    return cowardly_blocked_index > 0
        && monster == &monster_body[cowardly_blocked_index];
}

static s16b light_body[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
s16b (*cave_light)[MAX_DUNGEON_WID] = light_body;
static u16b view_body[VIEW_MAX], temp_body[TEMP_MAX];
u16b *view_g = view_body, *temp_g = temp_body;
int view_n;
bool character_dungeon;
byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte flow_center_y[MAX_FLOWS], flow_center_x[MAX_FLOWS];
byte update_center_y[MAX_FLOWS], update_center_x[MAX_FLOWS];
static u64b rng_state;
static bool preview_visible;
static int preview_phase;
void *SDLCALL SDL_calloc(size_t count, size_t size) { return calloc(count, size); }
void SDLCALL SDL_free(void *memory) { free(memory); }
u64b Rand_state_export(void) { return rng_state; }
void Rand_state_import(u64b state) { rng_state = state; }
void quit(cptr str) { (void)str; abort(); }

void calc_bonuses_for_preview(void)
{
    assert(character_dungeon);
    assert(preview_phase++ == 0);
    assert(p_ptr != &player_body && mon_list != monster_body);
    assert(cave_info != cave_body && cave_light != light_body);
    assert(view_g != view_body && temp_g != temp_body && view_n == 0);
    assert(p_ptr->py == player_body.py && mon_list[1].r_idx == 1);
    assert(cave_info[100][101] == cave_body[100][101]);
    p_ptr->chp = 987;
    ++rng_state;
}
void calc_torch(void)
{
    assert(character_dungeon);
    assert(preview_phase == 1 || preview_phase == 3);
    if (preview_phase == 3) assert(view_n == 1);
    ++preview_phase;
    p_ptr->cur_light = 3;
    ++rng_state;
}
void update_view_for_generation(void)
{
    assert(character_dungeon);
    assert(preview_phase == 2 || preview_phase == 4);
    ++preview_phase;
    /* Real equipped/floor weapon glow uses and overwrites this noise flow. */
    cave_cost[FLOW_MONSTER_NOISE][100][101] = 99;
    flow_center_y[FLOW_MONSTER_NOISE] = 98;
    flow_center_x[FLOW_MONSTER_NOISE] = 97;
    update_center_y[FLOW_MONSTER_NOISE] = 96;
    update_center_x[FLOW_MONSTER_NOISE] = 95;
    cave_info[100][101] = preview_visible ? CAVE_VIEW : 0;
    cave_light[100][101] = 10;
    view_g[0] = 42;
    temp_g[0] = 43;
    view_n = 1;
    ++rng_state;
}
void update_flow(int y, int x, int flow)
{
    assert(preview_phase++ == 5);
    assert(flow == FLOW_PLAYER_NOISE && y == p_ptr->py && x == p_ptr->px);
    cave_cost[flow][y][x] = 0;
    flow_center_y[flow] = update_center_y[flow] = y;
    flow_center_x[flow] = update_center_x[flow] = x;
}
void update_mon_for_generation(int index)
{
    assert(preview_phase++ == 6);
    assert(cave_cost[FLOW_PLAYER_NOISE][p_ptr->py][p_ptr->px] == 0);
    assert(index == 1);
    mon_list[index].ml = preview_visible;
    mon_list[index].hp = 456;
    ++rng_state;
}

static bool blitz;
static tutorial_mode selected_mode;
static tutorial_status first_monster_status;
static int sync_calls;

bool run_mode_is_blitz(void) { return blitz; }
tutorial_mode get_sdl_gameplay_tutorial_mode(void) { return selected_mode; }
void tutorial_sync_tale(void) { ++sync_calls; }
tutorial_status tutorial_lesson_status(const char *id)
{
    assert(id && !strcmp(id, "combat.first_monster"));
    return first_monster_status;
}

/* Keep the exact production implementations in this fixture. */
''' + HELPERS + r'''

static void reset_state(void)
{
    memset(&player_body, 0, sizeof(player_body));
    memset(monster_body, 0, sizeof(monster_body));
    memset(race_body, 0, sizeof(race_body));
    memset(cave_body, 0, sizeof(cave_body));
    merciless_blocked_index = cowardly_blocked_index = 0;
    p_ptr->py = p_ptr->px = 100;
    playerturn = 0;
    mon_max = 1;
    blitz = false;
    selected_mode = TUTORIAL_MODE_EXTENDED;
    first_monster_status = TUTORIAL_UNSEEN;
    sync_calls = 0;
}

static void gate_tests(void)
{
    reset_state();
    assert(tutorial_start_needs_clear_area());
    assert(sync_calls == 1);

    first_monster_status = TUTORIAL_IN_PROGRESS;
    assert(tutorial_start_needs_clear_area());
    assert(sync_calls == 2);

    first_monster_status = TUTORIAL_COMPLETED;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 3);
    first_monster_status = TUTORIAL_SKIPPED;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 4);

    /* The generation gate is only for a newly started turn-zero map. */
    first_monster_status = TUTORIAL_UNSEEN;
    playerturn = 1;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 4);

    playerturn = 0;
    p_ptr->tutorial_deferred = true;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 4);
    p_ptr->tutorial_deferred = false;

    blitz = true;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 4);
    blitz = false;

    selected_mode = TUTORIAL_MODE_DISABLED;
    assert(!tutorial_start_needs_clear_area());
    assert(sync_calls == 4);

    selected_mode = TUTORIAL_MODE_NORMAL;
    assert(tutorial_start_needs_clear_area());
    assert(sync_calls == 5);
}

static void trigger_tests(void)
{
    reset_state();
    assert(!tutorial_game_first_monster_triggered());

    /* Empty/dead entries, even at a visible coordinate, never trigger. */
    mon_max = 3;
    monster_body[1].fy = 100;
    monster_body[1].fx = 101;
    monster_body[1].ml = true;
    cave_info[100][101] = CAVE_VIEW;
    assert(!tutorial_game_first_monster_triggered());

    monster_body[1].r_idx = 1;
    assert(tutorial_game_first_monster_triggered());

    /* Nearby monsters are fine when hidden or outside real player LOS. */
    monster_body[1].ml = false;
    assert(!tutorial_game_first_monster_triggered());
    monster_body[1].ml = true;
    cave_info[100][101] = CAVE_SEEN | CAVE_MARK;
    assert(!tutorial_game_first_monster_triggered());
    cave_info[100][101] = CAVE_VIEW;

    race_body[1].flags1 |= RF1_PEACEFUL;
    assert(!tutorial_game_first_monster_triggered());
    race_body[1].flags1 &= ~RF1_PEACEFUL;

#define CHECK_INFORMATION_FIELD(field) do { \
    p_ptr->field = 1; \
    assert(tutorial_game_first_monster_triggered()); \
    p_ptr->field = 0; \
    assert(tutorial_game_first_monster_triggered()); \
} while (0)
    CHECK_INFORMATION_FIELD(afraid);
    CHECK_INFORMATION_FIELD(confused);
    CHECK_INFORMATION_FIELD(truce);
    CHECK_INFORMATION_FIELD(rage);
    CHECK_INFORMATION_FIELD(entranced);
#undef CHECK_INFORMATION_FIELD
    p_ptr->image=1;
    assert(!tutorial_game_first_monster_triggered());
    p_ptr->image=0;

    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    assert(tutorial_game_first_monster_triggered());
    p_ptr->niena_quest = 0;
    assert(tutorial_game_first_monster_triggered());

    merciless_blocked_index = 1;
    assert(tutorial_game_first_monster_triggered());
    merciless_blocked_index = 0;
    cowardly_blocked_index = 1;
    assert(tutorial_game_first_monster_triggered());
    cowardly_blocked_index = 0;

    p_ptr->stun = 100;
    assert(tutorial_game_first_monster_triggered());
    p_ptr->stun = 101;
    assert(tutorial_game_first_monster_triggered());
    p_ptr->stun = 0;

    /* One blocked creature must not hide a later legal visible target. */
    merciless_blocked_index = 1;
    monster_body[2] = monster_body[1];
    monster_body[2].fx = 102;
    cave_info[100][102] = CAVE_VIEW;
    assert(tutorial_game_first_monster_triggered());
    monster_body[2].ml = false;
    assert(tutorial_game_first_monster_triggered());

    /* Awareness is useful for every hero; only a distant unaware enemy is a
     * suitable opportunity to practice stealth. */
    assert(!tutorial_stealth_target(&monster_body[1]));
    monster_body[1].fx=102; cave_info[100][102]=CAVE_VIEW;
    monster_body[1].alertness=ALERTNESS_UNWARY;
    assert(tutorial_stealth_target(&monster_body[1]));
    monster_body[1].alertness=ALERTNESS_ALERT;
    assert(!tutorial_stealth_target(&monster_body[1]));
    monster_body[1].alertness=ALERTNESS_UNWARY;
    p_ptr->rage=1; assert(!tutorial_stealth_target(&monster_body[1])); p_ptr->rage=0;
    p_ptr->entranced=1; assert(!tutorial_stealth_target(&monster_body[1])); p_ptr->entranced=0;
    p_ptr->stun=101; assert(!tutorial_stealth_target(&monster_body[1])); p_ptr->stun=0;
    p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
    assert(tutorial_stealth_target(&monster_body[1]));
}

static void preview_tests(void)
{
    static byte noise_before[2][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static u16b info_before[MAX_DUNGEON_HGT][256];
    static s16b light_before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static u16b view_before[VIEW_MAX], temp_before[TEMP_MAX];
    reset_state();
    mon_max = 3; /* Slot two is dead and must not be updated. */
    mon_list[1].r_idx = 1;
    mon_list[1].fy = 100;
    mon_list[1].fx = 101;
    cave_info[100][101] = CAVE_MARK;
    cave_light[100][101] = 17;
    view_g[0] = 18;
    temp_g[0] = 19;
    view_n = 20;
    rng_state = 12345;
    for (int i = 0; i < 2; ++i) {
        int flow = i ? FLOW_MONSTER_NOISE : FLOW_PLAYER_NOISE;
        memset(cave_cost[flow], 24 + i, sizeof(cave_cost[flow]));
        memcpy(noise_before[i], cave_cost[flow], sizeof(noise_before[i]));
        flow_center_y[flow] = 30 + i;
        flow_center_x[flow] = 32 + i;
        update_center_y[flow] = 34 + i;
        update_center_x[flow] = 36 + i;
    }
    const player_type player_before = player_body;
    monster_type monsters_before[8];
    memcpy(monsters_before, monster_body, sizeof(monster_body));
    memcpy(info_before, cave_body, sizeof(cave_body));
    memcpy(light_before, light_body, sizeof(light_body));
    memcpy(view_before, view_body, sizeof(view_body));
    memcpy(temp_before, temp_body, sizeof(temp_body));
    for (int visible = 0; visible <= 1; ++visible) {
        character_dungeon = visible; /* Restore either original flag value. */
        preview_visible = visible;
        preview_phase = 0;
        assert(tutorial_start_triggers_monster() == preview_visible);
        assert(preview_phase == 7);
        assert(character_dungeon == visible);
        for (int i = 0; i < 2; ++i) {
            int flow = i ? FLOW_MONSTER_NOISE : FLOW_PLAYER_NOISE;
            assert(!memcmp(cave_cost[flow], noise_before[i], sizeof(noise_before[i])));
            assert(flow_center_y[flow] == 30 + i && flow_center_x[flow] == 32 + i);
            assert(update_center_y[flow] == 34 + i && update_center_x[flow] == 36 + i);
        }
        assert(p_ptr == &player_body && mon_list == monster_body);
        assert(cave_info == cave_body && cave_light == light_body);
        assert(view_g == view_body && temp_g == temp_body);
        assert(!memcmp(&player_body, &player_before, sizeof(player_body)));
        assert(!memcmp(monster_body, monsters_before, sizeof(monster_body)));
        assert(!memcmp(cave_body, info_before, sizeof(cave_body)));
        assert(!memcmp(light_body, light_before, sizeof(light_body)));
        assert(!memcmp(view_body, view_before, sizeof(view_body)));
        assert(!memcmp(temp_body, temp_before, sizeof(temp_body)));
        assert(cave_info[100][101] == CAVE_MARK);
        assert(cave_light[100][101] == 17);
        assert(view_g[0] == 18 && temp_g[0] == 19 && view_n == 20);
        assert(rng_state == 12345);
    }
}

int main(void)
{
    gate_tests();
    trigger_tests();
    preview_tests();
    puts("Tutorial start safeguard helpers: lifecycle, visibility/LOS, awareness under oaths/conditions, useful stealth, and scratch preview isolation: PASS");
    return 0;
}
'''


def source_guards():
    """Check that the retry is made after every population source is done."""
    source = GENERATION.read_text(encoding="utf-8-sig")
    entry = source.index("const bool protect_tutorial_start = tutorial_start_needs_clear_area();")
    generation_loop = source.index("while (true)", entry)
    final_population = source.index("if (tutorial_start_triggers_monster())", generation_loop)
    cave_done = source.index("if (cave_gen())", generation_loop)
    message = source.index("/*message*/", final_population)
    assert entry < generation_loop < cave_done < final_population < message
    apply_pending = source.index("apply_pending_quest_states();", final_population)
    assert final_population < apply_pending


def main():
    source_guards()
    if not BUILD.exists():
        raise SystemExit(f"Missing configured build directory: {BUILD}")
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")

    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")
    ])
    exe = OUT / "check.exe"
    command = [
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-Wall", "-Wextra", "-O0", "-g", "-ffunction-sections",
        "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), "-o", str(exe),
    ]
    subprocess.run(command, cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)
    print("Tutorial start source ordering: PASS")


if __name__ == "__main__":
    main()
