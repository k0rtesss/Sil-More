#!/usr/bin/env python3
"""Translate real level images and exercise allocation-failure atomicity.

Uses initialized production engine objects from the current Windows build;
player saves and the live game are never opened. Run build-incremental.ps1 first.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/level-window"

TESTS = r'''
#include "level-generation/level-state.h"
#include "level-generation/level-stream.h"
#include "meta_state.h"

static SDL_malloc_func saved_malloc;
static SDL_calloc_func saved_calloc;
static SDL_realloc_func saved_realloc;
static SDL_free_func saved_free;
static int allocation_number, fail_allocation;
static void* fail_malloc(size_t size) {
    return ++allocation_number == fail_allocation ? NULL : saved_malloc(size);
}
static void* fail_calloc(size_t n,size_t size) {
    return ++allocation_number == fail_allocation ? NULL : saved_calloc(n,size);
}
static void* fail_realloc(void* p,size_t size) {
    return ++allocation_number == fail_allocation ? NULL : saved_realloc(p,size);
}
static level_image* test_image(int height,int width) {
    level_image* image=level_image_new(); assert(image);
    image->hgt=height;image->wid=width;image->py=image->px=112;
    image->depth=10;image->mon_max=12;image->o_max=10;
    image->stream.enabled=true;image->stream.y=-440;image->stream.x=880;
    image->partition_rows=image->partition_cols=5;image->partition_count=25;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
        image->feat[y][x]=(y==0||x==0||y==height-1||x==width-1)?FEAT_WALL_PERM:FEAT_WALL_EXTRA;
        image->info[y][x]=CAVE_WALL;
    }
    for(int i=0;i<25;i++) {
        image->modes[i]=i%6;image->densities[i]=i%3;
        image->population[i].chest_count=i+1;image->atmospheres[i]=i;
    }
    return image;
}
static void test_shift(int dy,int dx) {
    level_image* image=test_image(222,222);
    int y=110-dy,x=110-dx;
    image->feat[110][110]=FEAT_TRAP_DART;
    image->info[110][110]=CAVE_MARK|CAVE_HIDDEN|CAVE_ROOM|CAVE_GLOW|CAVE_VIEW|CAVE_SEEN|CAVE_TEMP;
    image->color[110][110]=17;image->natural[110][110]=1;image->rewired[110][110]=2;
    image->legendary[110][110]=345;image->light[110][110]=8;image->scent[110][110]=77;
    image->cells[110][110].due=999;image->cells[110][110].known_feat=FEAT_TRAP_DART;
    image->fixtures[110][110]=3;image->floods[110][110]=2;
    image->flood_surface[110][110]=4;image->flood_traps[110][110]=5;
    image->water_flow[110][110]=6;image->catastrophe_steps[110][110]=12345;
    image->chasm_drop[110][110]=3;image->chasm_up[110][110]=2;
    image->environment.source_count=1;image->sources[0].y=image->sources[0].x=110;
    image->events[0].kind=CAVE_EVENT_WARNING;image->events[0].y=image->events[0].x=110;
    image->catastrophe.ready=true;image->catastrophe.y=image->catastrophe.x=110;
    image->vertical.portal_count=1;image->vertical.portals[0].identity=123;
    image->vertical.portals[0].y=image->vertical.portals[0].x=110;
    image->vertical.portals[0].peer_y=image->vertical.portals[0].peer_x=111;
    image->vertical.basin_count=1;image->vertical.basins[0].y=image->vertical.basins[0].x=110;
    image->vertical_vaults.count=1;image->vertical_vaults.placements[0].identity=124;
    image->vertical_vaults.placements[0].y=image->vertical_vaults.placements[0].x=110;
    image->stream_site_count=1;image->stream_sites[0].y1=image->stream_sites[0].x1=109;
    image->stream_sites[0].y2=image->stream_sites[0].x2=111;
    image->hint_count=1;image->hint_lengths[0]=1;
    image->skeleton.hint_used_mask=7;image->skeleton.hint_use_counts[0]=2;
    image->skeleton.seen_count=1;image->skeleton.seen_ids[0]=42;image->skeleton.notes_shown=7;
    SDL_strlcpy(image->hints[0][0],"retained note",100);
    image->hint_metadata[0].source_y=image->hint_metadata[0].source_x=110;
    image->hint_metadata[0].destination_count=1;
    image->hint_metadata[0].destinations[0].kind=HINT_DESTINATION_PARTITION;
    image->hint_metadata[0].destinations[0].y=image->hint_metadata[0].destinations[0].x=111;
    monster_type* m=&image->monsters[3];m->r_idx=1;m->fy=m->fx=110;m->hp=23;m->site_token=999;
    m->target_y=m->target_x=111;m->ai.goal_y=m->ai.goal_x=111;
    m->ai.sense.kind=2;m->ai.sense.y=m->ai.sense.x=111;
    m->ai.sense.anchor_y=m->ai.sense.anchor_x=110;
    m->ai.sense.recent_count=1;m->ai.sense.recent_y[0]=m->ai.sense.recent_x[0]=109;
    m->world.home_y=m->world.home_x=110;m->world.target_y=m->world.target_x=111;
    m->routine.home_y=m->routine.home_x=110;m->routine.territory=13;
    m->routine.count=2;m->routine.y[0]=m->routine.x[0]=110;
    m->routine.y[1]=m->routine.x[1]=111;m->hold_o_idx=7;
    int oy=dy>0?2:dy<0?219:110,ox=dx>0?2:dx<0?219:110;
    image->monsters[9].r_idx=1;image->monsters[9].fy=oy;image->monsters[9].fx=ox;
    image->monsters_at[110][110]=3;image->monsters_at[oy][ox]=9;
    m->social_rival=9;
    for(int i=2;i<=7;i++)if(i==2||i==4||i==5||i==7) {
        image->objects[i].k_idx=1;image->objects[i].site_token=i+900;
        if(i<5)image->objects[i].iy=image->objects[i].ix=110;
        else image->objects[i].held_m_idx=3;
    }
    image->objects_at[110][110]=4;image->objects[4].next_o_idx=2;
    image->objects[7].next_o_idx=5;
    image->objects[8].k_idx=1;image->objects[8].held_m_idx=9;
    image->flow_y[FLOW_WANDERING_HEAD]=image->flow_x[FLOW_WANDERING_HEAD]=110;
    image->wandering_cost[0][110][110]=7;
    assert(level_window_translate(image,dy,dx,222,222));
    assert(image->stream.y==-440+dy&&image->stream.x==880+dx);
    assert(image->feat[y][x]==FEAT_TRAP_DART&&image->traps==1);
    assert(image->info[y][x]==(CAVE_MARK|CAVE_HIDDEN|CAVE_ROOM|CAVE_GLOW));
    assert(image->color[y][x]==17&&image->natural[y][x]==1&&image->rewired[y][x]==2);
    assert(image->legendary[y][x]==345&&image->light[y][x]==0&&image->scent[y][x]==77);
    assert(image->cells[y][x].due==999&&image->cells[y][x].known_feat==FEAT_TRAP_DART);
    assert(image->fixtures[y][x]==3&&image->floods[y][x]==2);
    assert(image->flood_surface[y][x]==4&&image->flood_traps[y][x]==5);
    assert(image->water_flow[y][x]==6&&image->catastrophe_steps[y][x]==12345);
    assert(image->chasm_drop[y][x]==3&&image->chasm_up[y][x]==2);
    assert(image->sources[0].y==y&&image->events[0].x==x&&image->catastrophe.y==y);
    assert(image->vertical.portals[0].y==y&&image->vertical.portals[0].peer_x==x+1);
    assert(image->vertical.basins[0].x==x&&image->vertical_vaults.placements[0].y==y);
    assert(image->stream_sites[0].y1==y-1&&image->stream_sites[0].x2==x+1);
    assert(image->hint_count==1&&image->hint_metadata[0].source_y==y);
    assert(image->skeleton.hint_used_mask==7&&image->skeleton.hint_use_counts[0]==2);
    assert(image->skeleton.seen_count==1&&image->skeleton.seen_ids[0]==42&&image->skeleton.notes_shown==7);
    assert(image->hint_metadata[0].destinations[0].x==x+1);
    assert(image->hint_metadata[0].destinations[0].id==((y)/44)*5+x/44);
    m=&image->monsters[3];assert(m->r_idx==1&&m->fy==y&&m->fx==x&&m->hp==23&&m->site_token==999);
    assert(m->target_x==x+1&&m->ai.goal_y==y+1&&m->ai.sense.anchor_x==x);
    assert(m->ai.sense.recent_y[0]==y-1&&m->world.target_y==y+1);
    assert(m->routine.y[1]==y+1&&m->routine.territory==((y-1)/44)*5+(x-1)/44+1);
    assert(!image->monsters[9].r_idx&&!m->social_rival&&image->mon_cnt==1);
    assert(image->repro==((r_info[1].flags2&RF2_MULTIPLY)?1:0));
    assert(image->monsters_at[y][x]==3&&image->objects_at[y][x]==4);
    assert(image->objects[4].next_o_idx==2&&image->objects[2].next_o_idx==0);
    assert(m->hold_o_idx==7&&image->objects[7].next_o_idx==5);
    assert(image->objects[5].held_m_idx==3&&!image->objects[5].iy);
    assert(!image->objects[8].k_idx&&image->o_cnt==4&&image->objects[7].site_token==907);
    assert(image->flow_y[FLOW_WANDERING_HEAD]==y&&image->wandering_cost[0][y][x]==7);
    for(int pi=0;pi<25;pi++) {
        int from_row=pi/5+dy/44,from_col=pi%5+dx/44;
        bool retained=from_row>=0&&from_row<5&&from_col>=0&&from_col<5;
        assert(image->population[pi].chest_count==(retained?from_row*5+from_col+1:0));
    }
    for(int i=0;i<222;i++)assert(image->feat[0][i]==FEAT_WALL_PERM&&image->feat[i][221]==FEAT_WALL_PERM);
    /* Incoming cells cannot inherit the old outer guard or map memory. */
    int incoming_y=dy<0?1:dy>0?220:110,incoming_x=dx<0?1:dx>0?220:110;
    assert(image->feat[incoming_y][incoming_x]==FEAT_WALL_EXTRA);
    assert(image->info[incoming_y][incoming_x]==CAVE_WALL);
    level_image_free(image);
}
static void test_clipped_knowledge(void) {
    level_image* image=test_image(222,222);
    image->hint_count=3;
    image->skeleton.hint_used_mask=UINT32_MAX;image->skeleton.notes_shown=99;
    SDL_memset(image->skeleton.hint_use_counts,3,sizeof(image->skeleton.hint_use_counts));
    image->skeleton.seen_count=2;image->skeleton.seen_ids[0]=42;image->skeleton.seen_ids[1]=43;
    for(int i=0;i<3;i++){image->hint_metadata[i].source_y=100;image->hint_metadata[i].source_x=100;}
    image->hint_metadata[0].source_y=4; /* source leaves */
    image->hint_metadata[1].destination_count=1;
    image->hint_metadata[1].destinations[0].kind=HINT_DESTINATION_FIXED_FEATURE;
    image->hint_metadata[1].destinations[0].y=4;image->hint_metadata[1].destinations[0].x=100;
    image->hint_lengths[2]=1;SDL_strlcpy(image->hints[2][0],"third",100);
    image->environment.source_count=3;image->sources[0].y=4;image->sources[0].x=100;
    image->sources[1].y=image->sources[1].x=100;image->sources[1].feature=FEAT_WATER;
    image->sources[1].used=2;image->cells[100][100].owner=2;
    image->cells[100][100].flags=ENV_ADDED_LIQUID;
    image->sources[2].y=4;image->sources[2].x=100;image->sources[2].feature=FEAT_LAVA;
    image->cells[101][100].owner=3; /* surviving reservoir, evicted arbitrary origin */
    image->objects[2].k_idx=1;image->objects[2].tval=TV_SKELETON;
    image->objects[2].iy=image->objects[2].ix=100;image->objects[2].pval=0;
    image->objects[2].ident=IDENT_KNOWN;image->objects[2].marked=1;image->objects[2].xtra1=77;
    monster_type* m=&image->monsters[3];m->r_idx=1;m->fy=m->fx=100;
    m->ai.goal_y=4;m->ai.goal_x=100;m->world.task=1;m->world.target_y=4;m->world.target_x=100;
    m->routine.home_y=m->routine.home_x=100;m->routine.count=2;m->routine.style=1;
    m->routine.y[0]=100;m->routine.x[0]=100;m->routine.y[1]=4;m->routine.x[1]=100;
    assert(level_window_translate(image,44,0,222,222));
    assert(image->hint_count==1&&!strcmp(image->hints[0][0],"third"));
    assert(image->skeleton.notes_shown==1&&!image->skeleton.hint_used_mask&&!image->skeleton.seen_count);
    for(int i=0;i<SKEL_HINT_MAX;i++)assert(!image->skeleton.hint_use_counts[i]);
    for(int i=0;i<SKELETON_NOTE_SEEN_MAX;i++)assert(image->skeleton.seen_ids[i]==-1);
    assert(image->objects[2].k_idx&&image->objects[2].tval==TV_SKELETON&&!image->objects[2].pval);
    assert(image->objects[2].ident==IDENT_KNOWN&&image->objects[2].marked&&image->objects[2].xtra1==77);
    assert(image->environment.source_count==2&&!image->monsters[3].ai.goal_y);
    assert(image->cells[56][100].owner==1&&image->sources[0].used==1);
    assert(image->cells[57][100].owner==2&&image->sources[1].y==57);
    assert(!image->monsters[3].world.task&&!image->monsters[3].routine.count);
    level_image_free(image);
    image=test_image(222,222);image->environment.source_count=ENV_SOURCES_MAX;
    for(int i=0;i<ENV_SOURCES_MAX;i++){image->sources[i].y=4;image->sources[i].x=100;}
    image->sources[ENV_SOURCES_MAX-1].feature=FEAT_WATER;
    image->sources[ENV_SOURCES_MAX-1].used=2;
    image->cells[100][100].owner=ENV_SOURCES_MAX;image->cells[100][100].flags=ENV_ADDED_LIQUID;
    assert(level_window_translate(image,44,0,222,222));
    assert(image->environment.source_count==1&&image->cells[56][100].owner==1);
    assert(image->sources[0].y==56&&image->sources[0].x==100&&image->sources[0].used==1);
    level_image_free(image);
    image=test_image(231,231);image->stream.enabled=false;
    image->feat[230][120]=FEAT_WALL_PERM;image->info[230][120]=CAVE_MARK|CAVE_WALL;
    image->feat[220][120]=FEAT_FLOOR;image->info[220][120]=CAVE_MARK;
    assert(level_window_translate(image,44,0,222,222));
    assert(image->feat[186][120]==FEAT_WALL_EXTRA&&image->info[186][120]==CAVE_WALL);
    assert(image->feat[176][120]==FEAT_FLOOR&&image->info[176][120]==CAVE_MARK);
    level_image_free(image);
    image=test_image(88,88); /* imported frame enabled before finite geometry expands */
    image->partition_rows=image->partition_cols=2;image->partition_count=4;
    assert(level_window_translate(image,0,0,222,222));
    assert(image->population[0].chest_count==1&&image->population[1].chest_count==2);
    assert(image->population[5].chest_count==3&&image->population[6].chest_count==4);
    assert(!image->population[2].chest_count);
    level_image_free(image);
}
static void test_atomic_failure(void) {
    level_image* image=test_image(222,222);
    level_image* before=SDL_malloc(sizeof(*before));assert(before);*before=*image;
    size_t object_bytes=z_info->o_max*sizeof(*image->objects);
    object_type* objects=SDL_malloc(object_bytes);assert(objects);SDL_memcpy(objects,image->objects,object_bytes);
    SDL_GetMemoryFunctions(&saved_malloc,&saved_calloc,&saved_realloc,&saved_free);
    for(int fail=1;fail<=3;fail++) {
        fail_allocation=fail;allocation_number=0;
        assert(SDL_SetMemoryFunctions(fail_malloc,fail_calloc,fail_realloc,saved_free));
        bool result=level_window_translate(image,44,0,222,222);
        assert(SDL_SetMemoryFunctions(saved_malloc,saved_calloc,saved_realloc,saved_free));
        assert(!result&&!memcmp(image,before,sizeof(*image)));
        assert(!memcmp(image->objects,objects,object_bytes));
    }
    image->stream.y=INT64_MAX;assert(!level_window_translate(image,44,0,222,222));
    image->stream.y=INT64_MIN;assert(!level_window_translate(image,-44,0,222,222));
    image->stream.y=before->stream.y;
    assert(!level_window_translate(image,44,0,MAX_DUNGEON_HGT+1,222));
    assert(!memcmp(image,before,sizeof(*image)));
    SDL_free(before);SDL_free(objects);level_image_free(image);
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n" + TESTS + init + r'''
    for(int y=-44;y<=44;y+=44)for(int x=-44;x<=44;x+=44)if(y||x)test_shift(y,x);
    test_clipped_knowledge();test_atomic_failure();
    puts("Level window: eight directions, complete state, ownership, memory clipping, skeleton clue renewal, source-limit reanchor, finite migration and allocation rollback PASS.");
    SDL_Quit();return 0;
}
''', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(("/src/main.c.obj", "/src/level-generation/level-window.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/level-generation/level-window.c"),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=90)


if __name__ == "__main__":
    main()
