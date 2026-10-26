#!/usr/bin/env python3
"""Exercise production SDL mouse-route continuation across streamed shifts.

Links the initialized real engine fixture and the current SDL path source.
Routes use production pathfinding and automatic command selection; only the
physical movement between selected commands is simulated on a known floor.
No native window, desktop input, or player save is used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/stream-mouse-path"

TESTS = r'''
#include "sdl/main-sdl-private.h"
#include "level-generation/level-stream.h"
#ifdef NDEBUG
#error This regression requires live assertions
#endif

static void known_floor(void) {
    SDL_memset(&g_mouse_path,0,sizeof(g_mouse_path));
    SDL_memset(&g_map_touch_press,0,sizeof(g_map_touch_press));
    SDL_memset(cave_m_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_m_idx));
    SDL_memset(cave_o_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_o_idx));
    SDL_memset(mon_list,0,MAX_MONSTERS*sizeof(*mon_list));
    SDL_memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    for(int y=0;y<MAX_DUNGEON_HGT;y++)for(int x=0;x<MAX_DUNGEON_WID;x++) {
        cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_MARK;cave_light[y][x]=2;
    }
    p_ptr->depth=10;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=222;
    p_ptr->py=p_ptr->px=110;p_ptr->playing=true;p_ptr->leaving=p_ptr->is_dead=false;
    character_generated=character_dungeon=true;character_icky=0;
    mon_max=o_max=1;g_views[PANE_MAIN].term_ready=true;
    cave_m_idx[110][110]=-1;g_map_touch_selected=false;
    Term_flush();
    assert(sdl_mouse_gameplay_context_active());
}
static void shift_player(int dy,int dx) {
    cave_m_idx[p_ptr->py][p_ptr->px]=0;
    p_ptr->py-=dy;p_ptr->px-=dx;
    cave_m_idx[p_ptr->py][p_ptr->px]=-1;
}
static void test_continuation(int dy,int dx) {
    known_floor();
    level_stream_frame frame={0};frame.enabled=true;frame.seed=123;frame.y=-1000;frame.x=2000;
    level_stream_set_frame(frame);
    s32b previous_turn=turn;
    assert(sdl_mouse_path_start_follow_grid(160,160));
    assert(g_mouse_path.follow_active&&g_mouse_path.path_valid&&g_mouse_path.path_len==50);
    s64b target_world_y=frame.y+g_mouse_path.target_y-1;
    s64b target_world_x=frame.x+g_mouse_path.target_x-1;
    shift_player(dy,dx);frame.y+=dy;frame.x+=dx;level_stream_set_frame(frame);
    sdl_mouse_path_rebase_window(dy,dx);
    assert(g_mouse_path.follow_active&&!g_mouse_path.path_valid&&!g_mouse_path.path_len);
    assert(!g_mouse_path.hover_visible&&g_mouse_path.path_wake_pending);
    assert(g_mouse_path.target_y==160-dy&&g_mouse_path.target_x==160-dx);
    assert(frame.y+g_mouse_path.target_y-1==target_world_y);
    assert(frame.x+g_mouse_path.target_x-1==target_world_x);
    int steps=0,command=0,dir=0;
    while(sdl_mouse_path_take_step_command(&command,&dir)) {
        assert(command==';'&&dir==3&&g_mouse_path.follow_active);
        assert(!g_mouse_path.path_wake_pending&&g_mouse_path.path_valid);
        cave_m_idx[p_ptr->py][p_ptr->px]=0;
        p_ptr->py+=ddy[dir];p_ptr->px+=ddx[dir];
        cave_m_idx[p_ptr->py][p_ptr->px]=-1;
        assert(++steps<=50);
    }
    assert(steps==50&&!g_mouse_path.follow_active);
    assert(p_ptr->py==160-dy&&p_ptr->px==160-dx&&turn==previous_turn);
    assert(!sdl_mouse_path_has_pending_key());
}
static void test_forgotten_target(int dy,int dx) {
    known_floor();
    g_mouse_path.follow_active=g_mouse_path.path_valid=g_mouse_path.path_wake_pending=true;
    g_mouse_path.target_y=dy>0?2:dy<0?219:100;
    g_mouse_path.target_x=dx>0?2:dx<0?219:100;
    g_mouse_path.path_len=5;
    g_mouse_path.recall_pending=true;
    g_mouse_path.recall_y=g_mouse_path.target_y;g_mouse_path.recall_x=g_mouse_path.target_x;
    sdl_mouse_path_rebase_window(dy,dx);
    assert(!g_mouse_path.follow_active&&!g_mouse_path.path_valid&&!g_mouse_path.path_len);
    assert(!g_mouse_path.path_wake_pending&&!g_mouse_path.recall_pending);
}
static void test_pending_references(void) {
    known_floor();
    g_mouse_path.follow_active=true;g_mouse_path.target_y=g_mouse_path.target_x=160;
    g_mouse_path.source_y=g_mouse_path.source_x=110;
    g_mouse_path.last_step_door_pending=true;
    g_mouse_path.last_step_door_y=g_mouse_path.last_step_door_x=100;
    g_mouse_path.last_step_player_y=g_mouse_path.last_step_player_x=99;
    g_mouse_path.stuck_door_bash_pending=g_mouse_path.grid_question_pending=g_mouse_path.recall_pending=true;
    g_mouse_path.stuck_door_bash_y=g_mouse_path.grid_question_y=g_mouse_path.recall_y=100;
    g_mouse_path.stuck_door_bash_x=g_mouse_path.grid_question_x=g_mouse_path.recall_x=100;
    sdl_mouse_path_rebase_window(44,-44);
    assert(g_mouse_path.follow_active&&g_mouse_path.source_y==66&&g_mouse_path.source_x==154);
    assert(g_mouse_path.last_step_door_pending&&g_mouse_path.last_step_door_y==56&&g_mouse_path.last_step_door_x==144);
    assert(g_mouse_path.last_step_player_y==55&&g_mouse_path.last_step_player_x==143);
    assert(g_mouse_path.stuck_door_bash_pending&&g_mouse_path.stuck_door_bash_y==56&&g_mouse_path.stuck_door_bash_x==144);
    assert(g_mouse_path.grid_question_pending&&g_mouse_path.grid_question_y==56&&g_mouse_path.grid_question_x==144);
    assert(g_mouse_path.recall_pending&&g_mouse_path.recall_y==56&&g_mouse_path.recall_x==144);
    /* Each pending record expires independently. A valid first point followed
     * by an evicted sibling must not retain a usable half-translated record. */
    for(int invalid=0;invalid<5;invalid++) {
        known_floor();g_mouse_path.follow_active=true;g_mouse_path.target_y=g_mouse_path.target_x=160;
        g_mouse_path.last_step_door_pending=true;
        g_mouse_path.last_step_door_y=g_mouse_path.last_step_door_x=100;
        g_mouse_path.last_step_player_y=g_mouse_path.last_step_player_x=99;
        g_mouse_path.stuck_door_bash_pending=g_mouse_path.grid_question_pending=g_mouse_path.recall_pending=true;
        g_mouse_path.stuck_door_bash_y=g_mouse_path.grid_question_y=g_mouse_path.recall_y=100;
        g_mouse_path.stuck_door_bash_x=g_mouse_path.grid_question_x=g_mouse_path.recall_x=100;
        if(invalid==0)g_mouse_path.last_step_door_y=2;
        if(invalid==1)g_mouse_path.last_step_player_y=2;
        if(invalid==2)g_mouse_path.stuck_door_bash_y=2;
        if(invalid==3)g_mouse_path.grid_question_y=2;
        if(invalid==4)g_mouse_path.recall_y=2;
        sdl_mouse_path_rebase_window(44,0);
        assert(g_mouse_path.follow_active);
        assert(g_mouse_path.last_step_door_pending==(invalid>1));
        assert(g_mouse_path.stuck_door_bash_pending==(invalid!=2));
        assert(g_mouse_path.grid_question_pending==(invalid!=3));
        assert(g_mouse_path.recall_pending==(invalid!=4));
        assert(g_mouse_path.last_step_door_y>=0&&g_mouse_path.last_step_player_y>=0);
        assert(g_mouse_path.stuck_door_bash_y>=0&&g_mouse_path.grid_question_y>=0&&g_mouse_path.recall_y>=0);
    }
    known_floor();sdl_mouse_path_rebase_window(-44,-44);
    assert(!g_mouse_path.follow_active&&!g_mouse_path.recall_pending);
    assert(!g_mouse_path.last_step_door_y&&!g_mouse_path.last_step_player_y);
    assert(!g_mouse_path.stuck_door_bash_y&&!g_mouse_path.grid_question_y&&!g_mouse_path.recall_y);
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    # Both private headers include the intentionally unguarded externs.h.
    # This fixture needs SDL declarations rather than generation internals.
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"\n', '')
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    terminal = "static errr terminal_extra(int action,int value) {(void)action;(void)value;return 0;}\n"
    source.write_text(prefix + terminal + "\n" + TESTS + init + r'''
    for(int y=-44;y<=44;y+=44)for(int x=-44;x<=44;x+=44)if(y||x) {
        test_continuation(y,x);test_forgotten_target(y,x);
    }
    test_pending_references();sdl_mouse_path_search_free();
    puts("Streaming SDL mouse path: eight-direction world target, actual follow commands to destination, forgotten cancellation, independent pending references PASS.");
    SDL_Quit();return 0;
}
''', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(("/src/main.c.obj", "/src/sdl/input/sdl-mouse-path.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/sdl/input/sdl-mouse-path.c"),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=90)


if __name__ == "__main__":
    main()
