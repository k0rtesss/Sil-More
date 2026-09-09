#!/usr/bin/env python3
"""Exercise production lava generation in isolated maps and vault dry routes.

Compiles the actual generation/access sources with in-memory map primitives;
does not launch the game or read player saves/configuration.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/lava-generation-check"

HARNESS = r'''
#include "level-generation/level-generation-lava.c"
@ACCESS@
#include <assert.h>
#include <stdio.h>

static player_type player;
player_type* p_ptr = &player;
static dun_data dungeon;
dun_data* dun = &dungeon;
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = features;
static u16b info[MAX_DUNGEON_HGT][256];
u16b (*cave_info)[256] = info;
static s16b monsters[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b objects[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
s16b (*cave_m_idx)[MAX_DUNGEON_WID] = monsters;
s16b (*cave_o_idx)[MAX_DUNGEON_WID] = objects;
layout_anchor_kind_t room_anchor_kind[CENT_MAX];
quadrant_mode_t current_partition_modes[25];
big_cave_type_t current_partition_big_cave_types[25];
static unsigned state;
u32b Rand_div(u32b n) { state=state*1664525u+1013904223u; return n ? (state>>8)%n : 0; }
int distance(int y1,int x1,int y2,int x2) {
    int y=ABS(y1-y2),x=ABS(x1-x2);return MAX(y,x)+MIN(y,x)/2;
}
int level_partition_index_for_point(int y,int x) {(void)y;return x<48?0:1;}
byte cave_fixture_at(int y,int x) {return y==12&&x==13;}
void cave_set_feat(int y,int x,int feat) {features[y][x]=feat;}
void log_log(int level,const char* file,int line,const char* fmt,...) {
    (void)level;(void)file;(void)line;(void)fmt;
}
static void reset(void) {
    memset(&dungeon,0,sizeof(dungeon));
    memset(features,FEAT_WALL_EXTRA,sizeof(features));memset(info,0,sizeof(info));
    memset(monsters,0,sizeof(monsters));memset(objects,0,sizeof(objects));
    memset(lava_proposed,0,sizeof(lava_proposed));
    player.cur_map_hgt=40;player.cur_map_wid=80;
    current_partition_modes[0]=current_partition_modes[1]=QUAD_MODE_BIG_CAVE;
    current_partition_big_cave_types[0]=BIG_CAVE_FIRE;
    current_partition_big_cave_types[1]=BIG_CAVE_ICE;
    dun->cent_n=1;dun->cent[0]=(coord){20,5};
    dun->corner[0]=(rectangle){.y1=4,.x1=4,.y2=35,.x2=74};
    room_anchor_kind[0]=LAYOUT_ANCHOR_CA_BLOB;
    for(int y=4;y<=35;y++)for(int x=4;x<=74;x++) {
        features[y][x]=FEAT_FLOOR;info[y][x]=CAVE_ROOM;
    }
    features[10][10]=FEAT_MORE;features[30][30]=FEAT_LESS;
    objects[11][11]=1;monsters[11][12]=1;
    info[15][15]|=CAVE_ICKY;info[15][16]|=CAVE_G_VAULT;
}
static int lava_count(void) {
    int n=0;for(int y=0;y<40;y++)for(int x=0;x<80;x++)n+=features[y][x]==FEAT_LAVA;return n;
}
static void connected(void) {
    static int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(access,0,sizeof(access));flood_access(10,10,access,false);
    for(int y=4;y<=35;y++)for(int x=4;x<=74;x++)
        if(player_passable(y,x,false))assert(access[y][x]);
}
int main(void) {
    for(unsigned seed=0;seed<100;seed++) {
        reset();state=seed;
        int river=lava_river(dun->corner[0],0);assert(river>=10);
        int pool=lava_pool(dun->corner[0],0);assert(pool>=5);
        connected();
        reset();state=seed;place_cave_lava();assert(lava_count()>=15);connected();
        for(int y=4;y<=35;y++)for(int x=48;x<=74;x++)assert(features[y][x]==FEAT_FLOOR);
        assert(features[10][10]==FEAT_MORE&&features[30][30]==FEAT_LESS);
        assert(features[11][11]==FEAT_FLOOR&&features[11][12]==FEAT_FLOOR);
        assert(features[12][13]==FEAT_FLOOR&&features[15][15]==FEAT_FLOOR&&features[15][16]==FEAT_FLOOR);
        for(int y=18;y<=22;y++)for(int x=4;x<=7;x++)
            if(distance(y,x,20,5)<=2)assert(features[y][x]==FEAT_FLOOR);
    }
    reset();current_partition_big_cave_types[0]=BIG_CAVE_ICE;place_cave_lava();assert(!lava_count());
    reset();current_partition_modes[0]=QUAD_MODE_CAVEY;place_cave_lava();assert(!lava_count());
    reset();dun->is_quest[0]=true;place_cave_lava();assert(!lava_count());
    /* A one-square neck must stay dry even though both chambers contain lava. */
    for(unsigned seed=0;seed<100;seed++) {
        reset();state=seed;
        for(int y=4;y<=35;y++)for(int x=22;x<=24;x++)features[y][x]=FEAT_WALL_EXTRA;
        for(int x=22;x<=24;x++)features[20][x]=FEAT_FLOOR;
        place_cave_lava();connected();
        for(int x=22;x<=24;x++)assert(features[20][x]==FEAT_FLOOR);
    }
    /* The generation flood never treats lava as a safe route, even in its
     * permissive rubble/chasm mode. */
    reset();features[20][20]=FEAT_LAVA;
    assert(!player_passable(20,20,false)&&!player_passable(20,20,true));
    puts("Lava generation: 200 seeded maps, pools/rivers, dry connectivity, narrow necks, protected cells, fire-only partitions: PASS");
    return 0;
}
'''


def components(rows):
    dry = {(y, x) for y, row in enumerate(rows) for x, char in enumerate(row)
           if char not in " #:%7`"}
    result = []
    while dry:
        todo = [dry.pop()]
        group = set(todo)
        while todo:
            y, x = todo.pop()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nxt = (y + dy, x + dx)
                    if nxt in dry:
                        dry.remove(nxt)
                        group.add(nxt)
                        todo.append(nxt)
        result.append(group)
    return result


def vault_tests():
    text = (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8")
    for serial, name in ((402, "Gothmog's hall"), (420, "Flame Pits")):
        block = text.split(f"N:{serial}:", 1)[1].split("\nN:", 1)[0]
        rows = [line[2:] for line in block.splitlines() if line.startswith("D:")]
        assert len({len(row) for row in rows}) == 1
        assert sum(row.count("`") for row in rows) >= 4
        original = [row.replace("`", "7" if serial == 420 else ".") for row in rows]
        after = components(rows)
        for before in components(original):
            survivors = before & set().union(*after)
            assert any(survivors <= group for group in after), name
        print(f"{name} ({serial}): lava present, original dry routes preserved: PASS")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    access = (ROOT / "src/level-generation/level-generation-access.c").read_text(encoding="utf-8")
    access = access[access.index("bool player_passable("):access.index("void label_rooms(")]
    source.write_text(HARNESS.replace("@ACCESS@", access), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O1", "-g", "-ffunction-sections",
                    "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "-Wl,--gc-sections", "-o", str(exe)],
                   cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=60)
    vault_tests()


if __name__ == "__main__":
    main()
