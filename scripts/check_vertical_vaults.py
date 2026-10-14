#!/usr/bin/env python3
"""Exercise the production paired-vault state/transform module and blueprints.

The isolated C fixture compiles the entire production module, replacing only
engine dependencies (placement/RNG/room bookkeeping). It does not claim full
gameplay replay or monster/loot balance validation.
"""
from collections import deque
from pathlib import Path
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/vertical-vaults"


def records():
    result = {}
    current = None
    for line in (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            current = int(line.split(":", 2)[1])
            result[current] = {"rows": [], "markers": []}
        elif current is not None:
            if line.startswith("D:"): result[current]["rows"].append(line[2:])
            elif line.startswith("Y:"): result[current]["pair"] = tuple(map(int, line[2:].split(":")))
            elif line.startswith("P:"): result[current]["markers"].append(tuple(map(int, line[2:].split(":"))))
    return result


def check_circulation(vault):
    rows = vault["rows"]
    assert len(rows) == 23 and {len(r) for r in rows} == {39}
    stairs = [(y, x) for y, row in enumerate(rows) for x, ch in enumerate(row) if ch in "<>"]
    assert len(stairs) == 1
    seen = {stairs[0]}
    queue = deque(seen)
    while queue:
        y, x = queue.popleft()
        for yy, xx in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
            if 0 <= yy < len(rows) and 0 <= xx < len(rows[0]) and (yy, xx) not in seen:
                if rows[yy][xx] not in ' #$/:%_"`{7':
                    seen.add((yy, xx))
                    queue.append((yy, xx))
    approaches = [(y, x) for role, y, x in vault["markers"] if role == 8]
    assert len(approaches) >= 2 and all(point in seen for point in approaches)
    assert all((y, x) in seen for y, row in enumerate(rows) for x, ch in enumerate(row) if ch in "*~")
    assert all((y, x) in seen for role, y, x in vault["markers"] if role in (1, 6, 7))

    def profile(y, x):
        token = rows[y][x]
        if token == '"': return 3
        if token == "_": return 2
        if token != ".": return 0
        return int(any(0 <= yy < len(rows) and 0 <= xx < len(rows[0])
                       and rows[yy][xx] in '_"'
                       for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2)))

    for transform in range(8):
        def point(y, x):
            if transform & 1: y = len(rows) - 1 - y
            if transform & 2: x = len(rows[0]) - 1 - x
            return (x, y) if transform & 4 else (y, x)
        cells = {point(y, x): (token, profile(y, x))
                 for y, row in enumerate(rows) for x, token in enumerate(row)}
        # The gate marker becomes a real door, whose feature remains protected.
        gates = {point(y, x) for role, y, x in vault["markers"] if role == 1}
        for high in (False, True):
            walkable = {p for p, (token, band) in cells.items()
                        if token not in ' #$/:%_"`{7' and (not high or band != 1 or p in gates)}
            visited = {point(*stairs[0])}
            pending = deque(visited)
            while pending:
                y, x = pending.popleft()
                for neighbor in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
                    if neighbor in walkable and neighbor not in visited:
                        visited.add(neighbor)
                        pending.append(neighbor)
            targets = [point(y, x) for role, y, x in vault["markers"] if role in (1, 7, 8)]
            targets += [p for p, (token, _) in cells.items() if token in "*~"]
            assert all(p in visited for p in targets), (transform, high, targets)
        # A submerged award has no normal dry route, and gains one only when
        # the authored tank is empty. Geometry transforms preserve that route.
        prizes = [point(y, x) for role, y, x in vault["markers"] if role == 9]
        drained = {p for p, (token, _) in cells.items() if token not in " #$/:%`{7"}
        visited = {point(*stairs[0])}
        pending = deque(visited)
        while pending:
            y, x = pending.popleft()
            for neighbor in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
                if neighbor in drained and neighbor not in visited:
                    visited.add(neighbor)
                    pending.append(neighbor)
        assert all(p in visited and cells[p][1] >= 2 for p in prizes)


STUB = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t byte;
typedef uint16_t u16b;
typedef int16_t s16b;
typedef uint32_t u32b;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MORGOTH_DEPTH 20
#define VLT_NO_ROTATION 1
#define ROOM_KIND_GREATER_VAULT 8
#define LAYOUT_ANCHOR_SETPIECE 1
struct { int v_max,o_max; } limits = {527,100}, *z_info = &limits;
struct { int depth, cur_map_hgt, cur_map_wid, blind, image; } player = {8,66,66,0,0}, *p_ptr = &player;
struct { int cent_n; struct {int y,x;} cent[20]; struct {int y1,x1,y2,x2;} corner[20]; int kind[20]; } dungeon, *dun = &dungeon;
static int chosen_transform, build_calls, reward_suppression;
static bool actual_v, actual_h, actual_d;
static bool cache_reject;
#define FEAT_FLOOR 1
#define FEAT_WATER 84
#define FEAT_LAVA 85
#define FEAT_DEEP_WATER 100
#define FEAT_ICE 86
#define CAVE_SEEN 1
#define VDG_NORMAL 0
#define PARTITION_DROP_SOURCE_FLOOR 1
#define DROP_QUALITY_NORMAL 0
#define DROP_TYPE_NOT_DAMAGED 1
typedef struct {int profile;} partition_drop_profile;
static byte cave_feat[128][128];
static int cave_o_idx[128][128];
static u16b cave_info[128][128];
static void cave_set_feat(int y,int x,int feature) {cave_feat[y][x]=(byte)feature;}
static struct {int k_idx;} o_list[100];
static int o_max=1,reward_mints,reward_rolls,reward_messages;
static bool reward_gate=true,worker_fixture,character_dungeon=true;
static bool in_bounds_fully(int y,int x)
{return y>0&&x>0&&y<player.cur_map_hgt-1&&x<player.cur_map_wid-1;}
static bool level_cache_in_worker(void) {return worker_fixture;}
static bool vault_drop_passes(int kind) {assert(kind==VDG_NORMAL);reward_rolls++;return reward_gate;}
static int player_generation_depth(void) {return player.depth;}
static int dieroll(int n) {assert(n==5);return 1;}
static int drop_mode_for_point(int y,int x) {(void)y;(void)x;return 0;}
static partition_drop_profile partition_drop_profile_for_mode_source_cfg(int mode,int source)
{assert(mode==0&&source==PARTITION_DROP_SOURCE_FLOOR);return (partition_drop_profile){0};}
static void place_object_with_profile_params(int y,int x,int depth,int penalty,
    int quality,int type,bool artefacts,int multiplier,u32b ident,const partition_drop_profile *profile)
{
    assert(depth==player.depth&&penalty==depth+1&&quality==DROP_QUALITY_NORMAL);
    assert(type==DROP_TYPE_NOT_DAMAGED&&!artefacts&&multiplier==1&&!ident&&profile);
    int slot=1;while(slot<o_max&&o_list[slot].k_idx)slot++;
    assert(slot<limits.o_max);
    o_list[slot].k_idx=1;cave_o_idx[y][x]=slot;o_max=MAX(o_max,slot+1);reward_mints++;
}
static void msg_print(const char *message) {assert(message&&*message);reward_messages++;}
static bool level_cache_generation_vault_fits(int a,int b,int c,int d)
{(void)a;(void)b;(void)c;(void)d;return !cache_reject;}
static int room_capacity_limit(void) { return 20; }
static void mark_room_anchor_meta(int a,int b,bool c) {(void)a;(void)b;(void)c;}
static bool one_in_(int n) {(void)n;return true;}
static int rand_int(int n) {return chosen_transform % n;}
static int rand_range(int a,int b) {(void)b;return a;}
static bool solid_rock(int a,int b,int c,int d) {(void)a;(void)b;(void)c;(void)d;return true;}
'''

PARSER_STUB = r'''
typedef int errr;
typedef const char *cptr;
typedef struct { int info_num; void *info_ptr; } header;
static int error_idx = -1;
#define PARSED_RECORD(TYPE,HEAD) ((error_idx >= 0) ? (TYPE*)(HEAD)->info_ptr + error_idx : NULL)
#define MAX_FLAG_SETS 1
#define VLT 0
#define ALLOW_TEMPLATES
enum { PARSE_ERROR_GENERIC=1, PARSE_ERROR_NON_SEQUENTIAL_RECORDS,
PARSE_ERROR_TOO_MANY_ENTRIES, PARSE_ERROR_OUT_OF_MEMORY,
PARSE_ERROR_MISSING_RECORD_HEADER, PARSE_ERROR_INVALID_FLAG,
PARSE_ERROR_VAULT_NOT_RECTANGULAR, PARSE_ERROR_VAULT_TOO_BIG,
PARSE_ERROR_UNDEFINED_DIRECTIVE };
static char parsed_text[1024*1024];
static u32b text_end=1;
static u32b add_name(header *h, cptr s) {(void)h;(void)s;return 1;}
static bool add_text(u32b *offset, header *h, cptr s)
{
    (void)h;
    if (!*offset) *offset=text_end++;
    strcpy(parsed_text + text_end - 1,s);
    text_end += strlen(s);
    return true;
}
static errr grab_one_flag(u32b **flags,cptr kind,cptr what)
{(void)flags;(void)kind;(void)what;return 0;}
'''

TESTS = r'''
static bool build_vault(int y, int x, vault_type *v, bool diagonal)
{
    ++build_calls;
    actual_d = diagonal;
    assert(vertical_vault_build_transform(&actual_v, &actual_h));
    int h = diagonal ? v->wid : v->hgt;
    int w = diagonal ? v->hgt : v->wid;
    assert(vertical_vault_fits(y-h/2,x-w/2,y-h/2+h-1,x-w/2+w-1));
    reward_suppression = vertical_vault_suppress_rewards();
    for(int row=0;row<v->hgt;row++)for(int col=0;col<v->wid;col++) {
        int rr=actual_v?v->hgt-1-row:row,cc=actual_h?v->wid-1-col:col;
        int yy=y-h/2+(diagonal?cc:rr),xx=x-w/2+(diagonal?rr:cc);
        char token=v_text[v->text+row*v->wid+col];
        cave_feat[yy][xx]=fixture_token_feature(token);
        cave_o_idx[yy][xx]=0;cave_info[yy][xx]=CAVE_SEEN;
    }
    return true;
}
static void check_liquid_profile(const vertical_vault_state *state)
{
    vertical_vault_state before=current;
    const vertical_vault_placement *p=&state->placements[0];
    for(int layer=0;layer<2;layer++) {
        const vault_type *v=&v_info[layer?p->lower_template:p->upper_template];
        const char *data=v_text+v->text;
        for(int row=-1;row<=v->hgt;row++)for(int col=-1;col<=v->wid;col++) {
            int rr=(p->transform&1)?v->hgt-1-row:row;
            int cc=(p->transform&2)?v->wid-1-col:col;
            int y=p->y+((p->transform&4)?cc:rr);
            int x=p->x+((p->transform&4)?rr:cc);
            int expected=0;
            if(row>=0&&col>=0&&row<v->hgt&&col<v->wid) {
                char token=data[row*v->wid+col];
                if(token=='"')expected=3;
                else if(token=='_')expected=2;
                else if(token=='.') {
                    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
                        int yy=row+dy,xx=col+dx;
                        if(yy<0||xx<0||yy>=v->hgt||xx>=v->wid)continue;
                        char adjacent=data[yy*v->wid+xx];
                        if(adjacent=='_'||adjacent=='"')expected=1;
                    }
                }
            }
            assert(vertical_vault_liquid_profile(state,p->upper_depth+layer,p->identity,y,x)==expected);
            assert(!vertical_vault_liquid_profile(state,p->upper_depth+2,p->identity,y,x));
            assert(!vertical_vault_liquid_profile(state,p->upper_depth+layer,p->identity^1U,y,x));
        }
    }
    assert(!vertical_vault_liquid_profile(NULL,p->upper_depth,p->identity,p->y,p->x));
    assert(!memcmp(&before,&current,sizeof(current)));
}
int main(void)
{
    INIT_BLUEPRINTS
    assert(fixture_token_feature('`')==FEAT_LAVA);
    assert(fixture_token_feature('"')==FEAT_DEEP_WATER);
    assert(fixture_token_feature('_')==FEAT_WATER);
    char invalid_self[]="Y:526:0", invalid_layer[]="Y:525:2";
    char invalid_role[]="P:10:1:1", invalid_row[]="P:4:255:1";
    char trailing[]="P:4:1:1:unexpected";
    assert(parse_v_info(invalid_self,&head)==PARSE_ERROR_GENERIC);
    assert(parse_v_info(invalid_layer,&head)==PARSE_ERROR_GENERIC);
    assert(parse_v_info(invalid_role,&head)==PARSE_ERROR_GENERIC);
    assert(parse_v_info(invalid_row,&head)==PARSE_ERROR_GENERIC);
    assert(parse_v_info(trailing,&head)==PARSE_ERROR_GENERIC);
    assert(pair_valid(525,526));
    char *submerged=v_text+v_info[525].text+6*v_info[525].wid+19;
    char original=*submerged;assert(original=='"');
    *submerged='`';assert(!pair_valid(525,526));*submerged=original;
    for (chosen_transform = 0; chosen_transform < 8; ++chosen_transform)
    {
        dun->cent_n = build_calls = 0;
        p_ptr->depth = 8;
        vertical_vault_begin(8,true);
        assert(vertical_vault_place());
        assert(build_calls == 1 && current.count == 1 && !reward_suppression);
        assert(actual_v == ((chosen_transform&1)!=0));
        assert(actual_h == ((chosen_transform&2)!=0));
        assert(actual_d == ((chosen_transform&4)!=0));
        assert(current.placements[0].transform == chosen_transform);
        assert(current.placements[0].reward_layers == 0);
        int before_mints=reward_mints;
        vertical_vault_release_rewards();
        assert(reward_mints==before_mints&&current.placements[0].reward_layers==0);
        vertical_vault_state upper, lower;
        vertical_vault_capture(&upper);
        check_liquid_profile(&upper);
        assert(!vertical_vault_fits(3,3,4,4));
        assert(vertical_vault_fits(62,62,63,63));
        vertical_vault_marker up_stair, down_stair, marker, prize;
        bool found = false;
        for (int i=0;vertical_vault_marker_at(i,&marker);++i) {
            if (marker.role == VERTICAL_VAULT_STAIR) { up_stair=marker;found=true; }
            if (marker.role == VERTICAL_VAULT_DRAIN_REWARD) prize=marker;
        }
        assert(found && up_stair.target_depth == 9);
        /* The lower half may be discovered before anyone enters the upper. */
        p_ptr->depth = 9;
        vertical_vault_begin(9,false);
        vertical_vault_inherit(&upper,9);
        dun->cent_n = 0;
        assert(vertical_vault_place() && !reward_suppression);
        assert(current.placements[0].reward_layers == 2);
        vertical_vault_release_rewards();
        assert(reward_mints==before_mints);
        for (int i=0;vertical_vault_marker_at(i,&marker);++i)
            if (marker.role == VERTICAL_VAULT_STAIR) down_stair=marker;
        assert(down_stair.target_depth == 8 && up_stair.y == down_stair.y && up_stair.x == down_stair.x);
        assert(down_stair.target_y == up_stair.y && down_stair.target_x == up_stair.x);
        assert(vertical_vault_visit(down_stair.y,down_stair.x)==526);
        assert(vertical_vault_visit(down_stair.y,down_stair.x)==0);
        assert(current.placements[0].discovered_layers == 2);
        vertical_vault_capture(&lower);
        check_liquid_profile(&upper); /* Explicit upper state while live lower. */
        /* Merge stale copies without losing reward/discovery bits. */
        p_ptr->depth = 8;
        vertical_vault_begin(8,false);
        vertical_vault_inherit(&upper,8);
        vertical_vault_inherit(&lower,8);
        assert(current.count == 1 && current.placements[0].reward_layers == 2);
        dun->cent_n = 0;
        assert(vertical_vault_place() && !reward_suppression);
        vertical_vault_release_rewards();
        assert(reward_mints==before_mints); /* Water still covers the prize. */
        cave_feat[prize.y][prize.x]=FEAT_ICE;vertical_vault_release_rewards();
        assert(reward_mints==before_mints);
        cave_feat[prize.y][prize.x]=FEAT_FLOOR;
        int saved_limit=limits.o_max;limits.o_max=o_max;
        vertical_vault_release_rewards();assert(current.placements[0].reward_layers==2);
        limits.o_max=saved_limit;
        cave_o_idx[prize.y][prize.x]=99;vertical_vault_release_rewards();
        assert(current.placements[0].reward_layers==2);cave_o_idx[prize.y][prize.x]=0;
        worker_fixture=(chosen_transform&1)!=0;
        int messages=reward_messages;
        vertical_vault_release_rewards();
        assert(reward_mints==before_mints+1&&current.placements[0].reward_layers==3);
        assert(reward_messages==messages+(worker_fixture?0:1));worker_fixture=false;
        assert(cave_o_idx[prize.y][prize.x]>0);
        cave_o_idx[prize.y][prize.x]=0; /* Picking it up must not respawn it. */
        vertical_vault_release_rewards();assert(reward_mints==before_mints+1);
        assert(vertical_vault_visit(up_stair.y,up_stair.x)==0);
        vertical_vault_capture(&upper);
        /* A lower-owned copy carries the consumed award through eviction. */
        p_ptr->depth=9;vertical_vault_begin(9,false);
        vertical_vault_inherit(&lower,9);vertical_vault_inherit(&upper,9);
        vertical_vault_capture(&lower);
        p_ptr->depth=8;vertical_vault_begin(8,false);vertical_vault_inherit(&lower,8);
        dun->cent_n=0;assert(vertical_vault_place()&&reward_suppression);
        cave_feat[prize.y][prize.x]=FEAT_FLOOR;vertical_vault_release_rewards();
        assert(reward_mints==before_mints+1&&current.placements[0].reward_layers==3);
        vertical_vault_restore(&lower);
        assert(current.count == 1);
        /* Evicted distant floors retain no vault archive. */
        vertical_vault_begin(12,false);
        vertical_vault_inherit(&lower,12);
        assert(current.count == 0 && vertical_vault_place());
        assert(!vertical_vault_marker_at(0,&marker));
        vertical_vault_begin(8,false);
        assert(vertical_vault_place() && current.count == 0);
    }
    /* Frequency settings are rolled once on exposure, never on wet builds. */
    p_ptr->depth=8;vertical_vault_begin(8,true);dun->cent_n=0;
    assert(vertical_vault_place());
    vertical_vault_marker marker;
    for(int i=0;vertical_vault_marker_at(i,&marker);i++)
        if(marker.role==VERTICAL_VAULT_DRAIN_REWARD)cave_feat[marker.y][marker.x]=FEAT_FLOOR;
    int before_mints=reward_mints, before_rolls=reward_rolls;
    reward_gate=false;vertical_vault_release_rewards();reward_gate=true;
    assert(reward_mints==before_mints&&reward_rolls==before_rolls+1);
    assert(current.placements[0].reward_layers==1);
    vertical_vault_release_rewards();assert(reward_mints==before_mints&&reward_rolls==before_rolls+1);
    cache_reject=true;
    vertical_vault_begin(8,true);
    assert(vertical_vault_place()&&current.count==0);
    v_info[526].vertical_companion = 1;
    assert(!pair_valid(525,526));
    v_info[526].vertical_companion = 525;
    v_info[526].wid--;
    assert(!pair_valid(525,526));
    v_info[526].wid++;
    vertical_vault_state probe={0};probe.count=1;
    probe.placements[0]=(vertical_vault_placement){.identity=123,.upper_template=525,
        .lower_template=526,.upper_depth=8,.y=3,.x=3};
    char *lava=v_text+v_info[525].text+4*v_info[525].wid+14;
    original=*lava;*lava='`';
    assert(pair_valid(525,526));
    assert(!vertical_vault_liquid_profile(&probe,8,123,7,17));
    *lava=original;
    puts("Vertical vaults: production liquid token cases (quote=deep water, backtick=lava), parser, 8 transforms/profiles/dry routes, deferred reward, capacity/frequency/worker handling, lower-first discovery and duplicate suppression: PASS");
    return 0;
}
'''


def main():
    # Generic parser consumes every V: before vault parsing. Infrastructure
    # must never accidentally reuse the reserved file-version directive.
    versions = [line for line in (ROOT / "lib/edit/vault.txt").read_text().splitlines() if line.startswith("V:")]
    assert len(versions) == 1 and re.fullmatch(r"V:\d+\.\d+\.\d+", versions[0])
    data = records()
    for id in (525, 526): check_circulation(data[id])
    assert data[525]["pair"] == (526, 0) and data[526]["pair"] == (525, 1)
    types = (ROOT / "src/types.h").read_text(encoding="utf-8-sig")
    vault_struct = re.search(r"struct vault_type\s*\{.*?\n\};", types, re.S).group()
    header = (ROOT / "src/level-generation/level-generation-vertical-vaults.h").read_text()
    header = re.sub(r'^#include.*$', '', header, flags=re.M)
    module = (ROOT / "src/level-generation/level-generation-vertical-vaults.c").read_text()
    module = re.sub(r'^#include.*$', '', module, flags=re.M)
    parser = (ROOT / "src/init/init-parse-vault.c").read_text()
    parser = re.sub(r'^#include.*$', '', parser, flags=re.M)
    # Compile the exact production liquid cases. A handwritten token-to-feature
    # mapping previously hid a deep-water/backtick-lava authoring mismatch.
    builder = (ROOT / "src/level-generation/level-generation-rooms-vaults.c").read_text()
    cases = []
    for token, feature in (("_", "FEAT_WATER"), ('"', "FEAT_DEEP_WATER"), ("`", "FEAT_LAVA")):
        pattern = (r"case\s+" + re.escape("'" + token + "'")
                   + r":\s*\{\s*cave_set_feat\(y,\s*x,\s*(?P<feature>FEAT_\w+)\);\s*break;\s*\}")
        matches = list(re.finditer(pattern, builder))
        assert len(matches) == 1 and matches[0]["feature"] == feature, (token, feature)
        cases.append(matches[0][0])
    liquid_cases = ("\nstatic byte fixture_token_feature(char token) { int y=0,x=0;"
                    "cave_feat[0][0]=FEAT_FLOOR;switch(token){\n" + "\n".join(cases)
                    + "\n}return cave_feat[0][0];}\n")
    init = r'''
    header head = {527,v_info};
    FILE *file=fopen(VAULT_FILE,"r");
    assert(file);
    char line[2048];
    while (fgets(line,sizeof(line),file)) {
        line[strcspn(line,"\r\n")]=0;
        if (!*line || line[0]=='#') continue;
        if (line[0]=='V') {
            int a,b,c;
            assert(sscanf(line+2,"%d.%d.%d",&a,&b,&c)==3);
            continue;
        }
        assert(parse_v_info(line,&head)==0);
    }
    fclose(file);
    v_text=parsed_text;
    assert(v_info[525].vertical_marker_count==8);
    assert(v_info[526].vertical_marker_count==6);
    '''.replace("VAULT_FILE", json.dumps((ROOT / "lib/edit/vault.txt").as_posix()))
    pre = STUB + vault_struct + "\ntypedef struct vault_type vault_type;\nstatic vault_type v_info[527];\nstatic char *v_text;\n" + header
    pre += "\nstatic bool build_vault(int y,int x,vault_type *v,bool d);\n"
    source = pre + PARSER_STUB + parser + module + liquid_cases + TESTS.replace("INIT_BLUEPRINTS", init)
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(source, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-Werror", "-O0", str(fixture), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__": main()
