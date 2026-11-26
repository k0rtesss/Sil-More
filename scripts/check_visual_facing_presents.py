#!/usr/bin/env python3
"""Check which direction changes require a visible facing refresh."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/visual-facing-presents"

FIXTURE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define ABS abs
typedef unsigned char byte;
typedef short s16b;
static const int ddx[] = {0,-1,0,1,-1,0,1,-1,0,1};
static struct {
    int py, px, visual_facing_dir, previous_action[2];
    bool restoring;
} player = {10, 10, 7, {7, 7}, false}, *p_ptr = &player;
static bool character_dungeon = true, mirror_player_tile_facing = true;
static bool mirror_monster_tile_facing = true;
static bool option_fixture, *op_ptr = &option_fixture;
static int playerturn = 20;
typedef struct { int fy, fx, visual_facing_dir; bool ml; } monster_type;
static int updates, fresh, tile_y, tile_x, batch;
static bool graphics_are_ascii(void) { return false; }
static bool in_bounds(int y, int x) { return y >= 0 && x >= 0; }
static void handle_stuff(void) { updates++; }
static void Term_fresh(void) { assert(batch == 1); fresh++; }
static void sdl_present_batch_begin(void) { assert(batch == 0); batch++; }
static void sdl_present_batch_end(void) { assert(batch == 1); batch--; }
static void lite_spot(int y, int x) { tile_y = y; tile_x = x; }

__PRODUCTION__

static void expect_refresh(bool needed, int y, int x)
{
    assert(!batch && updates == 1 && fresh == needed);
    if (needed)
        assert(tile_y == y && tile_x == x);
    else
        assert(tile_y == -1 && tile_x == -1);
    updates = fresh = 0;
    tile_y = tile_x = -1;
}

int main(void)
{
    tile_y = tile_x = -1;
    player_set_visual_facing_dir_immediate(4);
    assert(player.visual_facing_dir == 4);
    expect_refresh(false, 0, 0);

    player_set_visual_facing_dir_immediate(6);
    expect_refresh(true, 10, 10);
    player_set_visual_facing_dir_immediate(8);
    expect_refresh(true, 10, 10);
    /* Keep the previous-action fallback for a stationary vertical face. */
    player_set_visual_facing_dir_immediate(8);
    expect_refresh(true, 10, 10);

    player.visual_facing_dir = 7;
    playerturn = 1;
    player_set_visual_facing_dir_immediate(4);
    expect_refresh(true, 10, 10);
    playerturn = 20;
    player.restoring = true;
    player_set_visual_facing_dir_immediate(7);
    expect_refresh(true, 10, 10);
    player.restoring = false;

    /* A deferred real flip must still refresh at the new player position
     * when a same-side target is selected before the next paint. */
    player_set_visual_facing_dir(9);
    player.py = 11;
    player_set_visual_facing_target_immediate(11, 11);
    assert(player.visual_facing_dir == 6);
    expect_refresh(true, 11, 10);
    player_set_visual_facing_target_immediate(10, 11);
    assert(player.visual_facing_dir == 9);
    expect_refresh(false, 0, 0);

    monster_type monster = {.fy=6,.fx=4,.visual_facing_dir=9,.ml=true};
    monster_set_visual_facing_target_immediate(&monster, 6, 5);
    assert(monster.visual_facing_dir == 6);
    expect_refresh(false, 0, 0);
    monster_set_visual_facing_target_immediate(&monster, 6, 3);
    expect_refresh(true, 6, 4);
    monster_set_visual_facing_target_immediate(&monster, 5, 4);
    expect_refresh(true, 6, 4);
    puts("Facing refresh: same side, real flip, vertical fallback, first paint, restoration and target PASS");
    return 0;
}
'''


def main():
    direction = (ROOT / "src/ui/targeting/direction.c").read_text(encoding="utf-8")
    start = direction.index("int dir_from_delta(")
    end = direction.index('/*\n * Get an "aiming direction"', start)
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    source.write_text(FIXTURE.replace("__PRODUCTION__", direction[start:end]), encoding="utf-8")
    env = dict(os.environ, PATH=os.pathsep.join(
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", os.environ["PATH"]]))
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-Werror",
                    str(source), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=10)


if __name__ == "__main__":
    main()
