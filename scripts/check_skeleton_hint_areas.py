#!/usr/bin/env python3
"""Compile production hint grouping/discovery/area helpers in an isolated fixture.

Uses real engine headers and distance(), with synthetic map, object, monster and
saved-clue state. Executes drawing with instrumented SDL calls; no SDL window,
pixel rasterization, game smoke test, or player data access is involved.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/skeleton-hint-areas-check"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static\s+)?(?:void|bool|int)\s+" + re.escape(name)
                      + r"\s*\([^;{]*\)\s*\{", source, re.M)
    assert match, name
    return source[match.start():source.index("\n}", match.end()) + 2]


def assert_unique_skeleton_hint_is_text_only():
    source = (ROOT / "src/cmd/world/cmd-interact-chest.c").read_text(
        encoding="utf-8-sig")
    match = re.search(
        r"else if \(hint == SKEL_HINT_UNIQUE_MONSTER\)\s*\{(?P<body>.*?)"
        r"\n\s*\}\s*else if \(body_lines",
        source, re.S)
    assert match, "unique skeleton hint producer"
    assert "hint_message_meta_add_destination" not in match.group("body")


PRELUDE = r'''
#include "angband.h"
#include "externs.h"
#include <assert.h>
#include <stdio.h>
static player_type player;
player_type *p_ptr=&player;
static maxima maxima_fixture;
maxima *z_info=&maxima_fixture;
static object_type objects[8];
object_type *o_list=objects;
s16b o_max=8;
static monster_type monsters[8];
monster_type *mon_list=monsters;
s16b mon_max=8;
static artefact_type artefacts[8];
artefact_type *a_info=artefacts;
static u16b info[MAX_DUNGEON_HGT][256];
u16b (*cave_info)[256]=info;
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID]=features;
static int partition[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static hint_message_meta messages[16];
static byte message_count;
byte hint_messages_count_for_save(void) {return message_count;}
void hint_messages_message_meta(int i,hint_message_meta *out) {*out=messages[i];}
int level_partition_index_for_point(int y,int x) {return partition[y][x];}
bool sdl_minimap_focus_point_valid(int y,int x) {
    return y>=0 && x>=0 && y<p_ptr->cur_map_hgt && x<p_ptr->cur_map_wid;
}
bool sdl_minimap_hint_source_valid(const hint_message_meta *m) {
    return m && sdl_minimap_focus_point_valid(m->source_y,m->source_x);
}
static struct {SDL_Renderer *renderer;} g_state;
static SDL_FRect drawn_rects[20000];
static float drawn_lines[20000][4];
static int rect_count,line_count;
static Uint8 current_alpha;
static bool record_blend(SDL_Renderer *r,SDL_BlendMode mode) {
    (void)r;assert(mode==SDL_BLENDMODE_BLEND);return true;
}
static bool record_color(SDL_Renderer *r,Uint8 red,Uint8 green,Uint8 blue,Uint8 alpha) {
    (void)r;(void)red;(void)green;(void)blue;current_alpha=alpha;return true;
}
static bool record_rect(SDL_Renderer *r,const SDL_FRect *rect) {
    (void)r;assert(rect_count<20000);assert(current_alpha==80);
    drawn_rects[rect_count++]=*rect;return true;
}
static bool record_line(SDL_Renderer *r,float x1,float y1,float x2,float y2) {
    (void)r;assert(line_count<20000);assert(current_alpha==155);
    float *line=drawn_lines[line_count++];line[0]=x1;line[1]=y1;line[2]=x2;line[3]=y2;
    return true;
}
#define SDL_SetRenderDrawBlendMode record_blend
#define SDL_SetRenderDrawColor record_color
#define SDL_RenderFillRect record_rect
#define SDL_RenderLine record_line
'''

TESTS = r'''
static sdl_hint_area_set set;
static byte mask[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static void reset(void) {
    memset(&player,0,sizeof(player));player.cur_map_hgt=40;player.cur_map_wid=40;
    memset(info,0,sizeof(info));memset(features,0,sizeof(features));
    memset(objects,0,sizeof(objects));memset(monsters,0,sizeof(monsters));
    memset(artefacts,0,sizeof(artefacts));memset(partition,0xff,sizeof(partition));
    memset(messages,0,sizeof(messages));message_count=0;maxima_fixture.art_max=8;
}
static hint_message_destination *add(int kind,int id,int y,int x,int sy,int sx,int lo,int hi) {
    hint_message_meta *m=&messages[message_count++];
    m->source_y=sy;m->source_x=sx;m->destination_count=1;
    m->destinations[0]=(hint_message_destination){kind,y,x,id,lo,hi};
    return &m->destinations[0];
}
static int cells(void) {
    int n=0;
    for(int y=0;y<40;y++)for(int x=0;x<40;x++)n+=mask[y][x]!=0;
    return n;
}
static void grouping_and_intersection(void) {
    reset();
    add(HINT_DESTINATION_FIXED_FEATURE,FEAT_MORE,20,20,10,10,8,22);
    sdl_minimap_collect_hint_areas(&set);assert(set.group_count==1);
    assert(sdl_minimap_hint_group_mask(&set,0,mask));int first=cells();
    memcpy(before,mask,sizeof(mask));
    add(HINT_DESTINATION_FIXED_FEATURE,FEAT_MORE,20,20,10,30,8,22);
    sdl_minimap_collect_hint_areas(&set);assert(set.group_count==1);
    assert(sdl_minimap_hint_group_mask(&set,0,mask));assert(cells()>0&&cells()<first);
    for(int y=0;y<40;y++)for(int x=0;x<40;x++)assert(!mask[y][x]||before[y][x]);
    assert(mask[20][20]);
    add(HINT_DESTINATION_FIXED_FEATURE,FEAT_MORE,30,30,10,10,8,35);
    sdl_minimap_collect_hint_areas(&set);assert(set.group_count==2);
    puts("Same stairs intersect and shrink; different stairs remain separate: PASS");

    reset();add(HINT_DESTINATION_PARTITION,3,20,20,10,10,8,22);
    sdl_minimap_collect_hint_areas(&set);assert(sdl_minimap_hint_group_mask(&set,0,mask));
    memcpy(before,mask,sizeof(mask));
    add(HINT_DESTINATION_PARTITION,3,5,5,10,10,1,10);
    sdl_minimap_collect_hint_areas(&set);assert(set.group_count==1);
    assert(sdl_minimap_hint_group_mask(&set,0,mask));assert(memcmp(before,mask,sizeof(mask))==0);
    puts("Conflicting edge clues retain the previous nonempty area: PASS");
}
static void identities(void) {
    hint_message_destination a={HINT_DESTINATION_ARTEFACT,20,20,2,0,20},b=a;
    b.y=25;assert(sdl_minimap_hint_same_target(&a,&b));
    b.id=3;assert(!sdl_minimap_hint_same_target(&a,&b));
    a.kind=b.kind=HINT_DESTINATION_PARTITION;a.id=b.id=0;
    assert(sdl_minimap_hint_same_target(&a,&b));
    a.kind=b.kind=HINT_DESTINATION_GREAT_VAULT;assert(sdl_minimap_hint_same_target(&a,&b));
    a.kind=HINT_DESTINATION_UNIQUE_MONSTER;b.kind=HINT_DESTINATION_QUEST_GIVER;
    a.id=b.id=3;assert(sdl_minimap_hint_same_target(&a,&b));
    b.id=4;assert(!sdl_minimap_hint_same_target(&a,&b));
    puts("Artefact, region, vault and cross-kind monster identities: PASS");
}
static void discovery(void) {
    const int kinds[]={HINT_DESTINATION_FIXED_FEATURE,HINT_DESTINATION_FIXED_QUEST_SITE,
        HINT_DESTINATION_GREAT_VAULT,HINT_DESTINATION_ARTEFACT,HINT_DESTINATION_QUEST_GIVER,
        HINT_DESTINATION_UNIQUE_MONSTER,HINT_DESTINATION_PARTITION};
    for(unsigned i=0;i<sizeof(kinds)/sizeof(kinds[0]);i++) {
        reset();int k=kinds[i];
        add(k,2,20,20,10,10,8,22);add(k,2,20,20,10,30,8,22);
        objects[1].k_idx=1;objects[1].name1=2;objects[1].iy=20;objects[1].ix=20;
        monsters[1].r_idx=2;monsters[1].fy=20;monsters[1].fx=20;
        partition[20][20]=2;
        if(k==HINT_DESTINATION_GREAT_VAULT)info[20][20]=CAVE_G_VAULT;
        sdl_minimap_collect_hint_areas(&set);assert(set.group_count==1);
        if(k==HINT_DESTINATION_UNIQUE_MONSTER) {
            assert(!sdl_minimap_hint_group_mask(&set,0,mask));assert(cells()==0);
        } else {
            assert(sdl_minimap_hint_group_mask(&set,0,mask));assert(cells()>0);
        }
        if(k==HINT_DESTINATION_ARTEFACT)objects[1].marked=true;
        else if(k==HINT_DESTINATION_QUEST_GIVER||k==HINT_DESTINATION_UNIQUE_MONSTER)
            monsters[1].encountered=true;
        else info[20][20]|=CAVE_MARK;
        sdl_minimap_collect_hint_areas(&set);
        assert(!sdl_minimap_hint_group_mask(&set,0,mask));
    }
    reset();message_count=1;messages[0].source_y=10;messages[0].source_x=10;
    sdl_minimap_collect_hint_areas(&set);assert(set.group_count==0);
    puts("Unique monster destinations stay text-only; discovered destinations hide; legacy notes remain safe: PASS");
}
static void bounds(void) {
    reset();
    add(HINT_DESTINATION_FIXED_FEATURE,FEAT_MORE,20,20,10,10,8,22);
    add(HINT_DESTINATION_FIXED_FEATURE,FEAT_MORE,20,20,10,30,8,22);
    sdl_minimap_collect_hint_areas(&set);assert(sdl_minimap_hint_group_mask(&set,0,mask));
    int ey=10,ex=10,ly=10,lx=30;
    for(int y=0;y<40;y++)for(int x=0;x<40;x++)if(mask[y][x]) {
        ey=MIN(ey,y);ex=MIN(ex,x);ly=MAX(ly,y);lx=MAX(lx,x);
    }
    int miny=1000,minx=1000,maxy=-1,maxx=-1;bool any=false;
    sdl_minimap_expand_bounds_for_hints(&miny,&minx,&maxy,&maxx,&any);
    assert(any&&miny==ey&&minx==ex&&maxy==ly&&maxx==lx);
    info[20][20]=CAVE_MARK;miny=minx=1000;maxy=maxx=-1;any=false;
    sdl_minimap_expand_bounds_for_hints(&miny,&minx,&maxy,&maxx,&any);
    assert(any&&miny==10&&minx==10&&maxy==10&&maxx==30);
    reset();miny=minx=1000;maxy=maxx=-1;any=false;
    sdl_minimap_expand_bounds_for_hints(&miny,&minx,&maxy,&maxx,&any);
    assert(!any&&miny==1000&&minx==1000&&maxy==-1&&maxx==-1);
    puts("Production map bounds equal inferred mask plus sources; discovery removes area bounds: PASS");
}
static int exact_grid(float v) {int i=(int)v;assert(v==(float)i);return i;}
static void drawing_case(int miny,int minx,int maxy,int maxx) {
    const SDL_FRect dst={100,200,(maxx-minx+1)*8.0f,(maxy-miny+1)*6.0f};
    const SDL_Color fill={40,80,120,80};
    float labelx=-1,labely=-1;
    static int coverage[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int edges[MAX_DUNGEON_HGT][MAX_DUNGEON_WID][4];
    memset(coverage,0,sizeof(coverage));memset(edges,0,sizeof(edges));
    rect_count=line_count=0;
    bool drew=sdl_minimap_draw_hint_destination_area(mask,&dst,miny,minx,maxy,maxx,
        fill,&labelx,&labely);
    int expected=0;
    for(int y=MAX(0,miny);y<=MIN(MAX_DUNGEON_HGT-1,maxy);y++)
        for(int x=MAX(0,minx);x<=MIN(MAX_DUNGEON_WID-1,maxx);x++)expected+=!!mask[y][x];
    assert(drew==(expected>0));
    if(!expected) {assert(!rect_count&&!line_count&&labelx==-1&&labely==-1);return;}
    int label_grid_x=exact_grid((labelx-dst.x)/8.0f-0.5f)+minx;
    int label_grid_y=exact_grid((labely-dst.y)/6.0f-0.5f)+miny;
    assert(mask[label_grid_y][label_grid_x]);
    for(int i=0;i<rect_count;i++) {
        SDL_FRect *r=&drawn_rects[i];
        assert(r->x>=dst.x&&r->y>=dst.y&&r->x+r->w<=dst.x+dst.w&&r->y+r->h<=dst.y+dst.h);
        int y=exact_grid((r->y-dst.y)/6)+miny,x=exact_grid((r->x-dst.x)/8)+minx;
        int width=exact_grid(r->w/8);assert(width>0&&r->h==6);
        for(int j=0;j<width;j++) {assert(mask[y][x+j]);coverage[y][x+j]++;}
    }
    for(int i=0;i<line_count;i++) {
        float *l=drawn_lines[i];
        assert(l[0]>=dst.x&&l[1]>=dst.y&&l[2]<=dst.x+dst.w&&l[3]<=dst.y+dst.h);
        int x1=exact_grid((l[0]-dst.x)/8)+minx,y1=exact_grid((l[1]-dst.y)/6)+miny;
        int x2=exact_grid((l[2]-dst.x)/8)+minx,y2=exact_grid((l[3]-dst.y)/6)+miny;
        if(y1==y2) {
            assert(x2==x1+1);
            bool above=y1>0&&mask[y1-1][x1],below=y1<MAX_DUNGEON_HGT&&mask[y1][x1];
            assert(above!=below);
            if(below)edges[y1][x1][0]++;else edges[y1-1][x1][1]++;
        } else {
            assert(x1==x2&&y2==y1+1);
            bool left=x1>0&&mask[y1][x1-1],right=x1<MAX_DUNGEON_WID&&mask[y1][x1];
            assert(left!=right);
            if(right)edges[y1][x1][2]++;else edges[y1][x1-1][3]++;
        }
    }
    for(int y=MAX(0,miny);y<=MIN(MAX_DUNGEON_HGT-1,maxy);y++)
        for(int x=MAX(0,minx);x<=MIN(MAX_DUNGEON_WID-1,maxx);x++) {
            assert(coverage[y][x]==!!mask[y][x]);
            if(!mask[y][x])continue;
            assert(edges[y][x][0]==(y==0||!mask[y-1][x]));
            assert(edges[y][x][1]==(y==MAX_DUNGEON_HGT-1||!mask[y+1][x]));
            assert(edges[y][x][2]==(x==0||!mask[y][x-1]));
            assert(edges[y][x][3]==(x==MAX_DUNGEON_WID-1||!mask[y][x+1]));
        }
}
static void drawing(void) {
    memset(mask,0,sizeof(mask));drawing_case(0,0,39,39);
    for(int y=0;y<8;y++)for(int x=0;x<10;x++)mask[y][x]=(x<3||y<3);
    mask[15][15]=mask[16][16]=1;
    drawing_case(0,0,39,39);drawing_case(1,1,6,6);drawing_case(-2,-2,18,18);
    memset(mask,0,sizeof(mask));mask[MAX_DUNGEON_HGT-1][MAX_DUNGEON_WID-1]=1;
    drawing_case(MAX_DUNGEON_HGT-3,MAX_DUNGEON_WID-3,MAX_DUNGEON_HGT+1,MAX_DUNGEON_WID+1);
    puts("Production draw calls: exact single-fill coverage, exterior edges only, labels inside, empty masks and clipped bounds: PASS");
}
int main(void) {grouping_and_intersection();identities();discovery();bounds();drawing();return 0;}
'''


def main():
    assert_unique_skeleton_hint_is_text_only()
    source = (ROOT / "src/sdl/render/sdl-term-callbacks.c").read_text(encoding="utf-8-sig")
    start = source.index("typedef enum sdl_hint_destination_display")
    end = source.index("static void sdl_minimap_include_hint_point", start)
    implementation = source[start:end] + "\n".join(function(
        "src/sdl/render/sdl-term-callbacks.c", name) for name in (
            "sdl_minimap_include_hint_point", "sdl_minimap_expand_bounds_for_hints",
            "sdl_minimap_draw_hint_destination_area"))
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(PRELUDE + function("src/cave/cave-geometry.c", "distance")
                       + implementation + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O2",
                    "-Wall", "-Wextra", "-Werror", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(fixture), "-o", str(exe)], cwd=ROOT / "build-standard", env=env, check=True)
    result = subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30,
                            capture_output=True, text=True)
    (OUT / "result.txt").write_text(result.stdout, encoding="utf-8")
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
