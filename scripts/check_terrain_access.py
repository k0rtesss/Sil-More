#!/usr/bin/env python3
"""Compile production generation traversal and exercise isolated synthetic maps.

Uses the configured Windows build's include paths. No saves or runtime state.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/terrain-access-check"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "level-generation/level-generation-terrain-access.h"
#include <assert.h>
static player_type player;
player_type *p_ptr = &player;
static byte map[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int access_map[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b monsters[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = map;
u16b (*cave_info)[256] = info;
s16b (*cave_m_idx)[MAX_DUNGEON_WID] = monsters;
void flood_access(int, int, int [MAX_DUNGEON_HGT][MAX_DUNGEON_WID], bool);
static void reset(void) {
    memset(&player, 0, sizeof(player));
    player.cur_map_hgt = player.cur_map_wid = 24;
    memset(map, FEAT_WALL_EXTRA, sizeof(map));
    memset(info, 0, sizeof(info));
    memset(monsters, 0, sizeof(monsters));
}
static void corridor(int hazard) {
    reset();
    for(int x=3;x<=9;x++) map[10][x]=FEAT_FLOOR;
    map[10][6]=hazard;
}
static void flood(int x) { terrain_generation_flood(10,x,reached,NULL); }
static void straight_jumps(void) {
    const int gaps[]={FEAT_CHASM,FEAT_LAVA,FEAT_POISON};
    for(int i=0;i<3;i++) {
        corridor(gaps[i]);
        assert(terrain_generation_jump(10,5,0,1,NULL));
        assert(terrain_generation_jump(10,7,0,-1,NULL));
        flood(3); assert(reached[10][9] && !reached[10][6]);
        flood(9); assert(reached[10][3] && !reached[10][6]);
        for(int ignore=0;ignore<2;ignore++) {
            memset(access_map,0,sizeof(access_map));
            flood_access(10,3,access_map,ignore);
            assert(access_map[10][9]);
            assert(access_map[10][6] == (ignore && gaps[i]==FEAT_CHASM));
        }
        map[10][7]=gaps[i];
        assert(!terrain_generation_jump(10,5,0,1,NULL));
        flood(3); assert(!reached[10][9]);
    }
    corridor(FEAT_WATER); flood(3);
    assert(reached[10][6] && reached[10][9]);
    puts("One-tile chasm/lava/poison jumps both ways, two-tile gaps, water walking, final access floods: PASS");
}
static void footing(void) {
    const int invalid[]={FEAT_WALL_EXTRA,FEAT_WALL_PERM,FEAT_SECRET,
        FEAT_DOOR_HEAD,FEAT_DOOR_TAIL,FEAT_RUBBLE,FEAT_CHASM,FEAT_LAVA,
        FEAT_POISON,FEAT_TRAP_PIT,FEAT_TRAP_SPIKED_PIT,FEAT_TRAP_WEB};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        corridor(FEAT_LAVA); map[10][7]=invalid[i];
        assert(!terrain_generation_jump(10,5,0,1,NULL));
        corridor(FEAT_LAVA); map[10][4]=invalid[i];
        assert(!terrain_generation_jump(10,5,0,1,NULL));
        corridor(FEAT_LAVA); map[10][5]=invalid[i];
        assert(!terrain_generation_jump(10,5,0,1,NULL));
    }
    for(int x=4;x<=7;x++) {
        corridor(FEAT_LAVA); monsters[10][x]=1;
        assert(!terrain_generation_jump(10,5,0,1,NULL));
    }
    corridor(FEAT_LAVA); monsters[10][5]=-1;
    assert(terrain_generation_jump(10,5,0,1,NULL));
    assert(!terrain_generation_jump(10,5,0,0,NULL));
    assert(!terrain_generation_jump(10,5,0,2,NULL));
    assert(!terrain_generation_jump(0,0,-1,-1,NULL));
    corridor(FEAT_LAVA); map[10][4]=FEAT_WALL_EXTRA;
    flood(5); assert(!reached[10][7]);
    puts("Unsafe/closed/occupied landing, takeoff, run-up and middle obstacles rejected; absent run-up rejected: PASS");
}
static void all_directions(void) {
    const int dy[]={1,1,1,0,-1,-1,-1,0};
    const int dx[]={-1,0,1,1,1,0,-1,-1};
    for(int direction=0;direction<8;direction++) {
        for(int approach=0;approach<8;approach++) {
            reset();
            map[10][10]=FEAT_FLOOR;
            map[10+dy[direction]][10+dx[direction]]=FEAT_CHASM;
            map[10+2*dy[direction]][10+2*dx[direction]]=FEAT_FLOOR;
            int ay=10-dy[approach], ax=10-dx[approach];
            /* An approach on the obstacle cannot be standing ground. */
            if(map[ay][ax]!=FEAT_CHASM) map[ay][ax]=FEAT_FLOOR;
            int delta=(approach-direction+8)%8;
            bool expected=delta==0||delta==1||delta==7;
            assert(terrain_generation_jump(10,10,dy[direction],dx[direction],NULL)==expected);
        }
    }
    puts("All eight leap directions accept exactly the three runtime cycle run-up directions: PASS");
}
static void shadows_and_doors(void) {
    corridor(FEAT_FLOOR);
    memcpy(shadow,map,sizeof(shadow)); shadow[10][6]=FEAT_LAVA;
    assert(!terrain_generation_jump(10,5,0,1,NULL));
    assert(terrain_generation_jump(10,5,0,1,shadow));
    terrain_generation_flood(10,3,reached,shadow);
    assert(reached[10][9] && !reached[10][6]);
    assert(map[10][6]==FEAT_FLOOR);
    const int doors[]={FEAT_DOOR_HEAD,FEAT_DOOR_TAIL,FEAT_SECRET};
    for(int i=0;i<3;i++) { corridor(doors[i]); flood(3); assert(reached[10][9]); }
    corridor(FEAT_RUBBLE); flood(3); assert(!reached[10][9]);
    corridor(FEAT_WALL_INNER);
    for(int y=8;y<=12;y++) for(int x=4;x<=8;x++) info[y][x]=CAVE_ICKY;
    flood(3); assert(!reached[10][9]);
    terrain_generation_flood(0,0,reached,NULL);
    assert(!reached[10][3]);
    reset();
    for(int y=1;y<23;y++) for(int x=1;x<23;x++) map[y][x]=FEAT_FLOOR;
    terrain_generation_flood(1,1,reached,NULL);
    for(int y=1;y<23;y++) for(int x=1;x<23;x++) assert(reached[y][x]);
    puts("Shadow proposals isolated, doors openable, rubble/authored vault walls blocked, bounds and flood reset: PASS");
}
int main(void) {
    straight_jumps(); footing(); all_directions(); shadows_and_doors();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    # Keep the complete production player_passable/flood_access functions;
    # unrelated item-placement routines need the entire game to link on MinGW.
    access = OUT / "access-check.c"
    production = (ROOT / "src/level-generation/level-generation-access.c").read_text(encoding="utf-8")
    access.write_text(production.split("void label_rooms(void)", 1)[0], encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "-ffunction-sections", "-fdata-sections",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/level-generation/level-generation-terrain-access.c"),
        str(access),
        "-Wl,--gc-sections", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
