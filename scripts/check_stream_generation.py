#!/usr/bin/env python3
"""Exercise the real bounded stream builder against current full-engine objects.

Build first. Reads installed templates into an isolated temporary data directory;
assertions remain enabled, including when linked engine objects use -DNDEBUG.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/stream-generation"

TESTS = r'''
#include "level-generation/level-state.h"
#include "level-generation/level-stream.h"
#include "level-generation/level-generation-terrain-history.h"
#include "cave/cave-water-flow.h"
#include "cave/cave-bridge.h"
#include <inttypes.h>
static int wanted_mode;
int __wrap_mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks,
    const int* counts, int count)
{ (void)depth; (void)blocks; (void)counts; (void)count; return (int)mode==wanted_mode ? 1 : 0; }
static level_image *before, *after;
static player_type player_before;
static int y1,y2,x1,x2;
static bool inside(int y,int x) { return y>=y1 && y<=y2 && x>=x1 && x<=x2; }
static bool walkable(int y,int x)
{
    int f=cave_feat[y][x];
    return f==FEAT_FLOOR || cave_feat_is_bridge(f) || cave_stair_bold(y,x)
        || f==FEAT_OPEN || f==FEAT_BROKEN || f==FEAT_GLYPH;
}
static void prepare(int mode, int serial, bool overlap, bool water)
{
    character_dungeon=false;
    reset_map(20);
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=LEVEL_STREAM_SIZE;
    p_ptr->py=p_ptr->px=10;
    p_ptr->chp=42; p_ptr->food=7000; p_ptr->energy=91;
    turn=123456;
    terrain_generation_reset();
    terrain_history_reset();
    cave_environment_reset();
    cave_water_flow_reset();
    remember_partition_grid(5,5,25);
    for (int i=0;i<25;i++) {
        current_partition_modes[i]=QUAD_MODE_ROOMY;
        current_partition_densities[i]=DENSITY_NORMAL;
        current_partition_big_cave_types[i]=BIG_CAVE_NONE;
        current_partition_bridge_styles[i]=-1;
    }
    level_stream_frame frame={0};
    frame.enabled=true; frame.seed=7123; frame.serial=serial;
    frame.y=frame.x=-88;
    if(water) {
        bool found=false;
        for(int seed=0;seed<100 && !found;seed++) {
            frame.seed=seed; frame.y=frame.x=0;
            for(int y=89;y<=132 && !found;y++) for(int x=89;x<=132;x++) {
                int f,d;
                if(level_stream_major_feature(seed,20,y-1,x-1,&f,&d)
                    && (f==FEAT_WATER || f==FEAT_DEEP_WATER)) { found=true; break; }
            }
        }
        assert(found);
    }
    level_stream_set_frame(frame);
    assert(level_stream_partition_bounds(12,&y1,&y2,&x1,&x2));
    for(int y=0;y<LEVEL_STREAM_SIZE;y++) for(int x=0;x<LEVEL_STREAM_SIZE;x++) {
        cave_feat[y][x]=inside(y,x)?FEAT_WALL_EXTRA:FEAT_FLOOR;
        cave_info[y][x]=inside(y,x)?CAVE_WALL:0;
        cave_color[y][x]=(byte)(COLOR_STYLE_BASE+1+(x+y)%7);
        cave_light[y][x]=(x+y)%3;
        cave_natural[y][x]=(x+y)%2;
        cave_rewired[y][x]=0;
        cave_when[y][x]=(x+y)%251;
    }
    for(int x=x1;x<=x2;x++) cave_water_flow_set(y1-1,x,CAVE_WATER_FLOW_SOUTH);
    if(overlap) for(int y=y1+2;y<y1+21;y++) for(int x=x1-4;x<x1+10;x++) {
        cave_feat[y][x]=x==x1+9?FEAT_WALL_PERM:FEAT_FLOOR;
        cave_info[y][x]=CAVE_ICKY|CAVE_ROOM;
    }
    current_build_vault_exact_token=true;
    assert(place_monster_one(10,10,403,true,true,NULL));
    current_build_vault_exact_token=false;
    int k=0;
    for(int i=1;i<z_info->k_max;i++) if(k_info[i].tval==TV_FOOD) { k=i; break; }
    assert(k);
    object_type o; object_prep(&o,k); o.number=1;
    assert(floor_carry(11,11,&o));
    environment_state environment={0};
    environment.random=12345; environment.last_turn=turn; environment.ready=true;
    environment.source_count=1;
    assert(cave_environment_restore_state(environment));
    environment_source source={12,13,FEAT_WATER,8,2,1,123656};
    assert(cave_environment_restore_source(0,source));
    environment_cell cell={0};
    cell.base_feat=cell.known_feat=FEAT_FLOOR;
    cell.flags=ENV_NATURAL; cell.integrity=42;
    assert(cave_environment_restore_cell(10,11,cell));
    cave_event_emit(CAVE_EVENT_DIG,12,12,8);
    wanted_mode=mode;
    Rand_state_init(1717+serial*787);
    level_image_capture(before);
    player_before=*p_ptr;
}
#define CELL(field) do { if(memcmp(&before->field[y][x], &after->field[y][x], \
    sizeof(before->field[y][x]))) { fprintf(stderr,"changed %s at %d,%d mode=%d\n", \
    #field,y,x,wanted_mode); abort(); } } while(0)
static void check_preserved(void)
{
    level_image_capture(after);
    for(int y=0;y<LEVEL_STREAM_SIZE;y++) for(int x=0;x<LEVEL_STREAM_SIZE;x++) {
        if(inside(y,x) && !(before->info[y][x]&CAVE_ICKY)) continue;
        CELL(info); CELL(feat); CELL(color); CELL(natural); CELL(rewired);
        CELL(legendary); CELL(light); CELL(objects_at); CELL(monsters_at);
        CELL(scent); CELL(cells); CELL(fixtures); CELL(floods);
        CELL(flood_surface); CELL(flood_traps); CELL(water_flow); CELL(catastrophe_steps);
        for(int g=0;g<MAX_WANDERING_GROUPS;g++)
            assert(before->wandering_cost[g][y][x]==after->wandering_cost[g][y][x]);
    }
    for(int i=1;i<before->mon_max;i++)
        assert(!memcmp(&before->monsters[i],&after->monsters[i],sizeof(monster_type)));
    for(int i=1;i<before->o_max;i++)
        assert(!memcmp(&before->objects[i],&after->objects[i],sizeof(object_type)));
    assert(turn==123456 && p_ptr->chp==42 && p_ptr->food==7000 && p_ptr->energy==91);
    assert(p_ptr->depth==player_before.depth && p_ptr->py==10 && p_ptr->px==10);
    assert(!memcmp(before->events,after->events,sizeof(before->events)));
    assert(before->next_event==after->next_event);
    assert(!memcmp(&before->environment,&after->environment,sizeof(before->environment)));
    assert(!memcmp(before->sources,after->sources,sizeof(environment_source)));
#define RUN_FIELD(name) assert(p_ptr->name==player_before.name);
    LEVEL_GENERATED_PLAYER_FIELDS(RUN_FIELD)
#undef RUN_FIELD
    for(int i=0;i<25;i++) if(i!=12) {
        assert(before->modes[i]==after->modes[i]);
        assert(before->densities[i]==after->densities[i]);
        assert(before->big_caves[i]==after->big_caves[i]);
        assert(before->bridge_styles[i]==after->bridge_styles[i]);
        assert(!memcmp(&before->population[i],&after->population[i],sizeof(before->population[i])));
    }
}
static void check_seams(void)
{
    level_stream_frame frame=level_stream_get_frame();
    s64b py=level_stream_floor_div(frame.y+y1-1,44);
    s64b px=level_stream_floor_div(frame.x+x1-1,44);
    int north=x1+8+level_stream_hash(frame.seed,p_ptr->depth,py,px,111)%28;
    int south=x1+8+level_stream_hash(frame.seed,p_ptr->depth,py+1,px,111)%28;
    int west=y1+8+level_stream_hash(frame.seed,p_ptr->depth,py,px,112)%28;
    int east=y1+8+level_stream_hash(frame.seed,p_ptr->depth,py,px+1,112)%28;
    assert(walkable(y1,north) && walkable(y2,south));
    assert(walkable(west,x1) && walkable(east,x2));
    bool seen[44][44]={{false}};
    int queue[1936],head=0,tail=0;
    queue[tail++]=north-x1; seen[0][north-x1]=true;
    while(head<tail) {
        int n=queue[head++],y=y1+n/44,x=x1+n%44;
        const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};
        for(int d=0;d<4;d++) {
            int yy=y+dy[d],xx=x+dx[d];
            if(!inside(yy,xx) || seen[yy-y1][xx-x1] || !walkable(yy,xx)) continue;
            seen[yy-y1][xx-x1]=true; queue[tail++]=(yy-y1)*44+xx-x1;
        }
    }
    assert(seen[43][south-x1] && seen[west-y1][0] && seen[east-y1][43]);
}
static u64b terrain_checksum(void)
{
    u64b h=0;
    for(int y=y1;y<=y2;y++) for(int x=x1;x<=x2;x++)
        h=h*UINT64_C(1099511628211)^cave_feat[y][x]^((u64b)cave_color[y][x]<<8);
    return h;
}
static void checks(void)
{
    before=level_image_new(); after=level_image_new(); assert(before && after);
    u64b first=0;
    for(int mode=0;mode<=QUAD_MODE_BIG_CAVE;mode++) {
        prepare(mode,1,false,false);
        level_stream_generate_partition(12);
        check_preserved(); check_seams();
        assert((int)current_partition_modes[12]==mode);
        for(int y=y1;y<=y2;y++) for(int x=x1;x<=x2;x++)
            assert(cave_feat[y][x]!=FEAT_MORE && cave_feat[y][x]!=FEAT_MORE_SHAFT);
        if(!mode) first=terrain_checksum();
        printf("bounded mode %d passed; monsters=%d objects=%d\n",mode,mon_cnt,o_cnt);
    }
    prepare(QUAD_MODE_ROOMY,2,false,false);
    puts("checking fresh revisit");
    level_stream_generate_partition(12); check_preserved(); check_seams();
    assert(terrain_checksum()!=first);
    prepare(QUAD_MODE_ROOMY,3,true,false);
    puts("checking protected partial legacy overlap");
    level_stream_generate_partition(12); check_preserved(); check_seams();
    prepare(QUAD_MODE_CAVEY,4,false,true);
    puts("checking coordinate water continuity");
    level_stream_generate_partition(12); check_preserved(); check_seams();
    int wet=0;
    level_stream_frame frame=level_stream_get_frame();
    for(int y=y1;y<=y2;y++) for(int x=x1;x<=x2;x++) {
        int f,d;
        if(level_stream_major_feature(frame.seed,20,frame.y+y-1,frame.x+x-1,&f,&d)) {
            int under=cave_bridge_underlay(cave_feat[y][x]);
            if(!cave_stair_bold(y,x) && !cave_m_idx[y][x] && !cave_o_idx[y][x])
                assert(under==f);
            wet++;
        }
    }
    assert(wet>0);
    for(int depth=22;depth<=23;depth++) {
        prepare(QUAD_MODE_BIG_CAVE,depth,false,false);
        p_ptr->depth=depth;
        level_image_capture(before); player_before=*p_ptr;
        level_stream_generate_partition(12); check_preserved(); check_seams();
        for(int y=y1;y<=y2;y++) for(int x=x1;x<=x2;x++) assert(!cave_stair_bold(y,x));
    }
    level_image_free(before); level_image_free(after);
    puts("PASS six bounded modes, outside state, resident actors, protected legacy overlap, seam reachability, fresh revisit, water continuity and run-state preservation");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + TESTS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith("/src/main.c.obj")))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-O0", "-g", "-UNDEBUG", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=mode_weight_for_depth", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                       env=env, check=True, timeout=120)


if __name__ == "__main__":
    main()
