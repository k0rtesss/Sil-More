#!/usr/bin/env python3
"""Check authored terrain permissions and generation-only vault ownership."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/terrain-vault-check"
HARNESS = r'''
#include "angband.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include <assert.h>
static vault_type vaults[523];
vault_type *v_info = vaults;
int main(void) {
    terrain_vault_reset();
    assert(terrain_vault_id_at(2,3) == -1);
    assert(terrain_vault_instance_at(2,3) == -1);
    assert(terrain_vault_symbol_at(2,3) == ' ');
    vaults[331].typ = 7;
    vaults[331].flags = VLT_TERRAIN_CROSSING;
    terrain_vault_begin();
    terrain_vault_record(2,3,&vaults[331],'#');
    assert(terrain_vault_id_at(2,3) == 331);
    assert(terrain_vault_policy_at(2,3) == (TERRAIN_VAULT_CROSSING | TERRAIN_VAULT_FLOOD));
    assert(terrain_vault_symbol_at(2,3) == '#');
    int first = terrain_vault_instance_at(2,3);
    terrain_vault_record(2,4,&vaults[331],' ');
    assert(terrain_vault_id_at(2,4) == -1);
    terrain_vault_begin();
    terrain_vault_record(20,30,&vaults[331],'.');
    assert(terrain_vault_id_at(20,30) == 331);
    assert(terrain_vault_instance_at(20,30) != first);
    vaults[132].typ = 6;
    vaults[132].flags = VLT_TERRAIN_FLOOD;
    terrain_vault_record(4,3,&vaults[132],':');
    assert(terrain_vault_policy_at(4,3) == (TERRAIN_VAULT_CROSSING | TERRAIN_VAULT_FLOOD | TERRAIN_VAULT_RUINED));
    for(int type=0;type<=11;type++) {
        vaults[400].typ = type;
        vaults[400].flags = VLT_TERRAIN_CROSSING | VLT_TERRAIN_FLOOD | VLT_TERRAIN_REPAIRED;
        if(type<8) vaults[400].flags |= VLT_QUEST;
        terrain_vault_record(5,type,&vaults[400],'u');
        assert(terrain_vault_id_at(5,type) == 400);
        assert(terrain_vault_policy_at(5,type) == (type<8 ? 15 : 0));
        vaults[400].flags = 0;
        terrain_vault_record(6,type,&vaults[400],'.');
        assert(terrain_vault_policy_at(6,type) == (type<8 ? 3 : 0));
        assert(terrain_vault_symbol_at(5,type) == 'u');
    }
    terrain_vault_record(-1,0,&vaults[132],'.');
    terrain_vault_record(MAX_DUNGEON_HGT,0,&vaults[132],'.');
    assert(terrain_vault_id_at(-1,0) == -1);
    assert(terrain_vault_policy_at(0,MAX_DUNGEON_WID) == 0);
    assert(terrain_vault_instance_at(-1,0) == -1);
    terrain_vault_reset();
    assert(terrain_vault_id_at(2,3) == -1);
    assert(terrain_vault_policy_at(4,3) == 0);
    assert(terrain_vault_instance_at(20,30) == -1);
    puts("Exact footprints, repeated-instance ownership, immutable overrides and retry reset: PASS");
    return 0;
}
'''


STRUCTURE_HARNESS = r'''
#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "level-generation/level-generation-terrain-structures.h"
#include "cave/cave-bridge.h"
#include <assert.h>
static vault_type vaults[4];
vault_type *v_info = vaults;
static player_type player;
player_type *p_ptr = &player;
static dun_data dungeon;
dun_data *dun = &dungeon;
layout_anchor_kind_t room_anchor_kind[CENT_MAX];
static byte original[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = original;
static s16b monsters[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b objects[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
s16b (*cave_m_idx)[MAX_DUNGEON_WID] = monsters;
s16b (*cave_o_idx)[MAX_DUNGEON_WID] = objects;
static byte hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte saved[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
u32b Rand_div(u32b m) { return 1 % m; }
static void setup(int typ, unsigned flags, bool metadata) {
    terrain_vault_reset(); terrain_vault_begin();
    memset(&dungeon,0,sizeof(dungeon));
    memset(hard,0,sizeof(hard)); memset(work,0,sizeof(work));
    memset(objects,0,sizeof(objects)); memset(monsters,0,sizeof(monsters));
    p_ptr->cur_map_hgt=40; p_ptr->cur_map_wid=50;
    vaults[1].typ=typ; vaults[1].flags=flags;
    memset(original,FEAT_WALL_EXTRA,sizeof(original));
    for(int y=8;y<=26;y++)for(int x=8;x<=32;x++) {
        original[y][x]=(y<12 || y>22 || x<12 || x>28) ? FEAT_WALL_OUTER : FEAT_FLOOR;
        if(metadata) terrain_vault_record(y,x,&vaults[1],'#');
    }
    if(!metadata) {
        dun->cent_n=1; dun->corner[0]=(rectangle){8,8,26,32};
        room_anchor_kind[0]=LAYOUT_ANCHOR_ROOM;
    }
    memcpy(shadow,original,sizeof(shadow)); memcpy(saved,original,sizeof(saved));
    for(int y=3;y<32;y++)for(int x=17;x<=19;x++) {
        shadow[y][x]=FEAT_WATER; work[y][x]=TERRAIN_LANDMARK_TERRAIN;
    }
}
int main(void) {
    terrain_structure_stats stats;
    setup(7,VLT_QUEST|VLT_TERRAIN_FLOOD,true);
    hard[18][20]=1; objects[21][22]=1; monsters[20][23]=1;
    terrain_structure_apply(hard,work,shadow,FEAT_WATER,&stats);
    assert(stats.breached==1 && stats.ruined_walls>0 && stats.rubble_tiles>0);
    assert(shadow[18][20]==saved[18][20] && !terrain_structure_cell(18,20));
    assert(memcmp(original,saved,sizeof(saved))==0);
    for(int y=1;y<39;y++)for(int x=1;x<49;x++)if(work[y][x]==TERRAIN_LANDMARK_RUBBLE) {
        assert(saved[y][x]==FEAT_WALL_OUTER && !objects[y][x] && !monsters[y][x]);
        assert(terrain_structure_cell(y,x)==2);
    }
    setup(7,VLT_TERRAIN_REPAIRED,true);
    terrain_structure_apply(hard,work,shadow,FEAT_WATER,&stats);
    assert(stats.repaired==1 && stats.repair_tiles>=3);
    int repair_row=-1, run=0;
    for(int y=8;y<=26;y++)for(int x=8;x<=32;x++)if(work[y][x]==TERRAIN_LANDMARK_REPAIR) {
        if(repair_row<0)repair_row=y;
        assert(y==repair_row && shadow[y][x]==FEAT_BRIDGE_WATER_H);
        run++;
    }
    assert(run==3 && memcmp(original,saved,sizeof(saved))==0);
    setup(7,VLT_TERRAIN_FLOOD,true);
    for(int y=0;y<40;y++)for(int x=0;x<50;x++)
        if(work[y][x]==TERRAIN_LANDMARK_TERRAIN)shadow[y][x]=FEAT_CHASM;
    terrain_structure_apply(hard,work,shadow,FEAT_CHASM,&stats);
    assert(stats.breached==1 && stats.ruined_walls>0);
    assert(shadow[18][18]==FEAT_CHASM); /* The fracture crosses room floors. */
    assert(shadow[9][20]==FEAT_CHASM); /* Adjacent masonry can collapse. */
    for(int y=8;y<=26;y++)for(int x=8;x<=32;x++)
        if(saved[y][x]==FEAT_FLOOR && (x<17||x>19))assert(shadow[y][x]!=FEAT_CHASM);
    setup(7,0,false);
    terrain_structure_apply(hard,work,shadow,FEAT_WATER,&stats);
    assert(stats.flooded==1 && stats.ruined_walls>0);
    for(int type=8;type<=10;type++) {
        setup(type,VLT_TERRAIN_FLOOD|VLT_TERRAIN_REPAIRED,true);
        byte before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]; memcpy(before,shadow,sizeof(before));
        terrain_structure_apply(hard,work,shadow,FEAT_WATER,&stats);
        assert(!stats.flooded && !stats.breached && !stats.repaired);
        assert(memcmp(before,shadow,sizeof(before))==0 && !terrain_structure_cell(12,18));
    }
    puts("Transactional ruined quest flood, coherent repairs, room exposure, immutable types and hard/actor rubble exclusions: PASS");
    return 0;
}
'''


def check_catalogue():
    records = {}
    for line in (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("N:"):
            _, serial, name = line.split(":", 2)
            record = {"name": name, "flags": set()}
            records[int(serial)] = record
        elif line.startswith("X:"):
            record["x"] = line
        elif line.startswith("F:"):
            record["flags"].update(line[2:].replace("|", " ").split())
    expected = {
        46: ("Collapsed Cross", "X:6:12:12", "TERRAIN_FLOOD"),
        54: ("Collapsed corner", "X:6:1:2", "TERRAIN_FLOOD"),
        132: ("Collapsed Keep", "X:6:8:10", "TERRAIN_FLOOD"),
        210: ("Cave in", "X:6:12:5", "TERRAIN_FLOOD"),
        324: ("Collapsed Chamber", "X:7:9:5", "TERRAIN_FLOOD"),
        331: ("Fort", "X:7:5:2", "TERRAIN_REPAIRED"),
        343: ("Split hall", "X:7:10:3", "TERRAIN_CROSSING"),
        461: ("Duruin Bastion", "X:6:11:3", "TERRAIN_REPAIRED"),
        462: ("Orc Armory", "X:7:2:8:10", "TERRAIN_REPAIRED"),
    }
    approved = {i for i, r in records.items()
                if r["flags"] & {"TERRAIN_CROSSING", "TERRAIN_FLOOD", "TERRAIN_REPAIRED"}}
    assert approved == set(expected), approved
    for serial, (name, extra, flag) in expected.items():
        r = records[serial]
        assert (r["name"], r["x"]) == (name, extra)
        assert flag in r["flags"]
    assert not records[522]["flags"] & {"TERRAIN_CROSSING", "TERRAIN_FLOOD", "TERRAIN_REPAIRED"}
    print("Nine reviewed style templates; stable serials/depths/rarities; quest rooms mutable: PASS")


def main():
    check_catalogue()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/level-generation/level-generation-terrain-vaults.c"),
        "-o", str(exe)], cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)

    source.write_text(STRUCTURE_HARNESS, encoding="utf-8")
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/level-generation/level-generation-terrain-vaults.c"),
        str(ROOT / "src/level-generation/level-generation-terrain-structures.c"),
        str(ROOT / "src/cave/cave-bridge.c"),
        str(ROOT / "src/level-generation/level-generation-terrain-access.c"),
        "-o", str(exe)], cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
