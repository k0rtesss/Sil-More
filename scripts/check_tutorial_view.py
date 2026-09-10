#!/usr/bin/env python3
"""Compare generation preview with settled production field of view.

Runs real vinfo initialization, cave-view.c, geometry, and emitter contribution.
Unrelated commit/asset systems are stubs. This is not a rendered gameplay test.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-view-check"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    match = re.search(r"^int\s+" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", source, re.M)
    assert match
    return source[match.start():source.index("\n}", match.end()) + 2]


HARNESS = r'''
#include "angband.h"
#include "cave/cave-internal.h"
#include "cave/cave-light.h"
#include <assert.h>
#include <stdarg.h>

static u16b info[MAX_DUNGEON_HGT][256];
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b light[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b views[65536], temps[65536];
static monster_type monsters[4];
static monster_race races[4];
static object_type objects[4], gear[INVEN_TOTAL];
static u16b expected_info[MAX_DUNGEON_HGT][256];
static s16b expected_light[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int commits, curse_delta;

void note_spot(int y, int x) { (void)y; (void)x; ++commits; }
void lite_spot(int y, int x) { (void)y; (void)x; ++commits; }
void update_mon(int i, bool full) { (void)i; (void)full; ++commits; }
void disturb(int a, int b) { (void)a; (void)b; ++commits; }
void lava_light(void) {}
void object_flags(const object_type *o, u32b *a, u32b *b, u32b *c) {
    (void)o; *a = *b = *c = 0;
}
bool player_equipment_slot_counts_as_equipped(int slot) { (void)slot; return true; }
bool weapon_glows(const object_type *o) { (void)o; return false; }
int curse_flag_delta_cur(u32b flag) { return flag == CUR_LIGHTP ? curse_delta : 0; }
void cave_light_remember_raw(int y, int x, int n, int delta) {
    (void)y; (void)x; (void)n; (void)delta; ++commits;
}
void cave_light_remember_darkening(int y, int x, int delta) {
    (void)y; (void)x; (void)delta; ++commits;
}
void quit(cptr reason) { fprintf(stderr, "quit: %s\n", reason); exit(1); }
char *format(cptr fmt, ...) {
    static char out[512]; va_list args; va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args); va_end(args); return out;
}
void ang_sort(void *u, void *v, int n) {
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0 && !ang_sort_comp(u, v, j - 1, j); --j)
            ang_sort_swap(u, v, j - 1, j);
}
''' + function("src/cave/cave-light.c", "cave_light_contribution") + r'''

static void wall(int y, int x) {
    features[y][x] = FEAT_WALL_EXTRA;
    info[y][x] |= CAVE_WALL;
}

static void reset(int scenario) {
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(info, 0, sizeof(info)); memset(features, 0, sizeof(features));
    memset(light, 0, sizeof(light)); memset(views, 0, sizeof(views));
    memset(temps, 0, sizeof(temps)); memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races)); memset(objects, 0, sizeof(objects));
    memset(gear, 0, sizeof(gear));
    cave_info = info; cave_feat = features; cave_light = light;
    view_g = views; temp_g = temps; view_n = temp_n = 0;
    mon_list = monsters; r_info = races; o_list = objects; inventory = gear;
    mon_max = 2; o_max = 1;
    monsters[1].r_idx = 1; monsters[1].fy = 25; monsters[1].fx = 27;
    p_ptr->py = p_ptr->px = 25;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 50;
    p_ptr->old_light = 17; p_ptr->cur_light = 2;
    p_ptr->active_ability[S_PER][PER_KEEN_SENSES] = true;
    for (int y = 0; y < MAX_DUNGEON_HGT; ++y)
        for (int x = 0; x < MAX_DUNGEON_WID; ++x) {
            if (y == 0 || x == 0 || y >= 49 || x >= 49) wall(y, x);
            else { features[y][x] = FEAT_FLOOR; info[y][x] = 0; }
        }
    curse_delta = 0;
    switch (scenario) {
    case 0: /* Permanently lit room. */
        for (int y = 1; y < 49; ++y)
            for (int x = 1; x < 49; ++x) info[y][x] |= CAVE_GLOW;
        break;
    case 1: /* Nearby monster behind an opaque wall, diagonal still visible. */
        wall(25, 26); break;
    case 2: p_ptr->cur_light = 0; break;
    case 3: p_ptr->blind = true; break;
    case 4: break; /* Keen Senses sees the zero-light edge of the torch. */
    case 5: curse_delta = 1; break;
    case 6: races[1].light = 2; curse_delta = 1; break;
    case 7: races[1].light = -2; break;
    default: assert(false);
    }
    commits = 0;
}

static void compare_case(int scenario) {
    reset(scenario);
    update_view_for_generation();
    assert(commits == 0 && p_ptr->old_light == 17);
    assert(view_n > 0);
    if (scenario == 0) assert(player_can_see_bold(25, 40));
    if (scenario == 1) {
        assert(!player_has_los_bold(25, 27));
        assert(player_has_los_bold(26, 26));
    }
    if (scenario == 2) {
        assert(player_has_los_bold(25, 27));
        assert(!player_can_see_bold(25, 27));
    }
    if (scenario == 3) assert(!player_can_see_bold(25, 26));
    if (scenario == 4) assert(seen_by_keen_senses(25, 28));
    if (scenario == 5) {
        assert(!seen_by_keen_senses(25, 28));
        assert(seen_by_keen_senses(25, 27));
    }
    bool keen = seen_by_keen_senses(25, 28);
    memcpy(expected_info, info, sizeof(info));
    memcpy(expected_light, light, sizeof(light));
    int expected_n = view_n;

    reset(scenario);
    /* Normal startup performs multiple view updates. The ordinary curse path
     * retains its old view count, so compare the settled second and third. */
    for (int pass = 0; pass < 3; ++pass) {
        update_view();
        if (pass == 0) continue;
        assert(commits > 0 && view_n == expected_n);
        for (int y = 0; y < MAX_DUNGEON_HGT; ++y)
            for (int x = 0; x < MAX_DUNGEON_WID; ++x) {
                assert((info[y][x] & (CAVE_VIEW | CAVE_SEEN | CAVE_FIRE)) ==
                    (expected_info[y][x] & (CAVE_VIEW | CAVE_SEEN | CAVE_FIRE)));
                assert(light[y][x] == expected_light[y][x]);
            }
        assert(seen_by_keen_senses(25, 28) == keen);
    }
}

int main(void) {
    assert(vinfo_init() == 0);
    for (int scenario = 0; scenario < 8; ++scenario) compare_case(scenario);
    puts("Production FOV preview: lit/dark/wall/corner/blind/Keen Senses/curse/emitter cases and no preview commits: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", str(BUILD / "_deps/SDL"),
        env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-Wall", "-Wextra", "-O1", "-flto", "-fwhole-program",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/cave/cave-view.c"), str(ROOT / "src/cave/cave-geometry.c"),
        str(ROOT / "src/variable.c"), str(ROOT / "src/tables.c"),
        str(BUILD / "_deps/SDL/libSDL3.dll.a"), "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
