#!/usr/bin/env python3
"""Production contextual travel, touch/controller routing and status checks.

Injects only the core portal query/travel boundaries. Actual Alter, player
popup, radial collection/activation, status collectors and terminal painting
run against initialized engine data. No player save or host UI is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_context_stairs as stairs

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/vertical-ui-check"

CHECKS = r'''
#include "sdl/main-sdl-private.h"
#include "cave/cave-vertical.h"
#include "cave/cave-events.h"
#include "cave/cave-water-flow.h"
#include "rng.h"
#include "ui/status/status-internal.h"
/* SDL intentionally reserves no terminal status row. Exercise the retained
 * classic/compact painters in an isolated classic-row configuration. */
#undef ROW_STATUS
#define ROW_STATUS (Term->hgt - 1)
static bool portal_enabled;
static int current_strength, queued_command, queued_count, queued_pickup;
static cave_vertical_state vertical_state;
bool __wrap_cave_vertical_can_travel(bool up) {
    return portal_enabled && up==expected_up;
}
const cave_vertical_state* __wrap_cave_vertical_get(void) { return &vertical_state; }
int __wrap_cave_vertical_current(int y,int x,int* direction) {
    assert(y==p_ptr->py&&x==p_ptr->px);(void)direction;return current_strength;
}
bool __wrap_cave_vertical_describe(int y,int x,char* text,size_t size) {
    assert(y==p_ptr->py&&x==p_ptr->px);
    SDL_strlcpy(text,"A connected watercourse; the destination is below.",size);
    return portal_enabled;
}
void __wrap_sdl_enqueue_bypassed_command(int command) {
    queued_count++;queued_command=command;
}
void __wrap_do_cmd_queue_floor_context_action(floor_context_action_kind kind) {
    assert(kind==FLOOR_CONTEXT_ACTION_PICKUP_CONTEXT);queued_pickup++;
}
static void own_square(void) {
    reset_map(3);p_ptr->playing=true;p_ptr->is_dead=p_ptr->leaving=false;
    character_generated=character_dungeon=true;character_icky=false;
    cave_info[p_ptr->py][p_ptr->px]=CAVE_MARK|CAVE_SEEN;
    p_ptr->food=PY_FOOD_ALERT;p_ptr->pspeed=2;p_ptr->resting=0;
    p_ptr->energy_use=0;p_ptr->confused=p_ptr->command_arg=p_ptr->command_rep=0;
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    memset(&vertical_state,0,sizeof(vertical_state));current_strength=0;
    queued_count=queued_pickup=0;queued_command=0;
    confirmations=travel_calls=0;requested_direction=5;
    sdl_question_menu_clear();
}
static void line_text(int row,int start,int width,char* out) {
    for(int i=0;i<width;i++)out[i]=Term->scr->c[row][start+i];
    out[width]=0;
}
static void vertical_ui_checks(void) {
    sdl_config_set_defaults(&config);set_sdl_show_context_square_popups(true);
    for(int up=0;up<2;up++)for(int refused=0;refused<2;refused++) {
        own_square();portal_enabled=true;expected_up=up;travel_refuses=refused;
        cave_feat[p_ptr->py][p_ptr->px]=FEAT_DEEP_WATER;
        object_prep(&o_list[1],lookup_kind(TV_CHEST,SV_CHEST_SMALL_WOODEN));
        o_list[1].iy=p_ptr->py;o_list[1].ix=p_ptr->px;o_list[1].pval=1;
        cave_o_idx[p_ptr->py][p_ptr->px]=1;o_max=2;
        do_cmd_alter();
        assert(!confirmations&&travel_calls==1);
        assert(p_ptr->energy_use==(refused?0:100));
        assert(p_ptr->depth==3&&!p_ptr->leaving);
    }
    puts("Alter on current water portals delegates exactly once with zero inherited cost; cancellation/refusal stays free, UI adds no second danger prompt: PASS");
    for(int up=0;up<2;up++) {
        own_square();portal_enabled=true;expected_up=up;
        int key=-1;char label[32];int command=up?'<':'>';
        assert(touch_shortcut_context_action(' ',false,&key,label,sizeof(label)));
        assert(key==' '&&strcmp(label,up?"Ascend":"Descend")==0);
        assert(touch_shortcut_context_action(command,false,&key,label,sizeof(label)));
        assert(key==command&&!p_ptr->energy_use&&!travel_calls&&!confirmations);
        assert(!touch_shortcut_context_action(up?'>':'<',false,&key,label,sizeof(label)));
        assert(do_cmd_context_square_action_popup());
        assert(g_question_menu.context_hint&&g_question_menu.has_anchor);
        assert(g_question_menu.anchor_y==p_ptr->py&&g_question_menu.anchor_x==p_ptr->px);
        assert(g_question_menu.button_count==1&&g_question_menu.buttons[0].choice==command);
        assert(strstr(g_question_menu.desc,"watercourse"));
        inkey_flag=true;
        assert(sdl_question_menu_activate_context_choice(command));
        assert(queued_count==1&&queued_command==command&&!p_ptr->energy_use&&!travel_calls);
        player_action_menu_entry entries[SDL_PLAYER_ACTION_MAX];
        int count=sdl_player_action_menu_collect(entries),found=0;
        for(int i=0;i<count;i++) {
            if(entries[i].kind==(up?SDL_PLAYER_ACTION_ASCEND:SDL_PLAYER_ACTION_DESCEND)) {
                assert(entries[i].command==command);found++;
                assert(entries[i].tile_attr&TILE_FLAG);
            }
            assert(entries[i].kind!=SDL_PLAYER_ACTION_USE);
        }
        assert(found==1&&count<=10);
        int kind=up?SDL_PLAYER_ACTION_ASCEND:SDL_PLAYER_ACTION_DESCEND;
        assert(sdl_player_action_menu_default_kind()==kind);
        sdl_player_action_menu_activate_kind(kind,false);
        assert(queued_count==2&&queued_command==command&&!p_ptr->energy_use&&!travel_calls);
        assert(do_cmd_context_square_action_popup());p_ptr->px++;
        assert(!sdl_question_menu_activate_context_choice(command)&&queued_count==2);
        p_ptr->px--;
        portal_enabled=false;
        sdl_player_action_menu_activate_kind(kind,false);assert(queued_count==2);
        count=sdl_player_action_menu_collect(entries);found=0;
        for(int i=0;i<count;i++)if(entries[i].kind==SDL_PLAYER_ACTION_USE)found++;
        assert(found==1);
    }
    /* A Pick Up row on a passage must retain its destination chooser rather
     * than silently turning /5 into travel. The shared queue is a UI boundary. */
    own_square();portal_enabled=true;expected_up=false;inkey_flag=true;
    object_prep(&o_list[1],lookup_kind(TV_SWORD,SV_DAGGER));
    o_list[1].iy=p_ptr->py;o_list[1].ix=p_ptr->px;o_list[1].marked=true;
    cave_o_idx[p_ptr->py][p_ptr->px]=1;o_max=2;o_cnt=1;
    assert(object_can_choose_pack_or_harness(&o_list[1]));
    assert(do_cmd_context_square_action_popup());
    bool pickup=false,descend=false;
    for(int i=0;i<g_question_menu.button_count;i++) {
        pickup|=g_question_menu.buttons[i].choice==' '
            &&strcmp(g_question_menu.buttons[i].text,"Pick Up")==0;
        descend|=g_question_menu.buttons[i].choice=='>';
    }
    assert(pickup&&descend);
    assert(sdl_question_menu_activate_context_choice(' '));
    assert(queued_pickup==1&&!queued_count&&!p_ptr->energy_use&&!travel_calls);
    puts("Player-square popup, shared mobile/controller bindings and radial entry queue ordinary directional commands; stale choices and separate floor pickup stay safe: PASS");
    own_square();portal_enabled=false;p_ptr->resting=8;
    cave_feat[p_ptr->py][p_ptr->px]=FEAT_DEEP_WATER;
    vertical_state.submerged=1;vertical_state.breath=8;current_strength=3;
    status_pane_entry entries[SDL_STATUS_PANE_MAX_ENTRIES];
    int count=sdl_status_pane_collect(entries,N_ELEMENTS(entries));
    assert(count>=3&&strcmp(entries[0].label,"Air")==0&&strcmp(entries[0].detail,"8")==0);
    assert(entries[0].attr==TERM_L_BLUE&&strcmp(entries[1].label,"Current")==0);
    assert(strcmp(entries[1].detail,"strong")==0&&entries[1].attr==TERM_ORANGE);
    assert(sdl_status_pane_collect(entries,1)==1&&strcmp(entries[0].label,"Air")==0);
    char text[128];prt_state();line_text(ROW_STATE,COL_STATE,10,text);assert(strstr(text,"Air 8"));
    prt_terrain();line_text(ROW_TERRAIN,COL_TERRAIN,5,text);assert(strstr(text,"Cur3"));
    prt_status_line_compact();line_text(ROW_STATE,0,Term->wid,text);
    assert(strstr(text,"Air 8")&&strstr(text,"Current strong")&&strstr(text,"Rest"));
    byte attr;vertical_state.breath=3;
    assert(status_vertical_air_text(text,sizeof(text),&attr)&&attr==TERM_RED&&strcmp(text,"Air 3")==0);
    vertical_state.breath=5;
    assert(status_vertical_air_text(text,sizeof(text),&attr)&&attr==TERM_ORANGE);
    vertical_state.submerged=0;current_strength=0;
    count=sdl_status_pane_collect(entries,N_ELEMENTS(entries));
    for(int i=0;i<count;i++)assert(strcmp(entries[i].label,"Air")&&strcmp(entries[i].label,"Current"));
    prt_state();line_text(ROW_STATE,COL_STATE,10,text);assert(strstr(text,"Rest"));
    prt_terrain();line_text(ROW_TERRAIN,COL_TERRAIN,5,text);assert(!strstr(text,"Cur"));
    puts("SDL full-size Air/Current entries, short-pane Air priority, semantic low-air colors, legacy/compact text and status removal/rest preservation: PASS");
}
static void vertical_audio_checks(void) {
    own_square();cave_events_reset();cave_water_flow_reset();
    u64b rng=Rand_state_export();s32b clock_before=turn;
    vertical_state.portal_count=1;
    cave_vertical_portal* portal=&vertical_state.portals[0];
    portal->y=portal->x=10;portal->kind=CAVE_VERTICAL_FALL;portal->material=FEAT_WATER;
    cave_feat[10][10]=FEAT_CHASM;p_ptr->blind=1;
    assert(!cave_info[10][10]);
    assert(cave_flowing_water_sound_level_at(10,10)==CAVE_FLOWING_LIQUID_SOUND_RADIUS+1);
    assert(cave_flowing_water_sound_level_at(10,16)==1);
    assert(cave_flowing_water_sound_level_at(10,17)==0);
    assert(!cave_info[10][10]&&cave_sound_mask_at(10,10)==0);
    for(int y=0;y<p_ptr->cur_map_hgt;y++)cave_feat[y][12]=FEAT_WALL_PERM;
    cave_events_terrain_changed();
    assert(cave_flowing_water_sound_level_at(10,11)>0);
    assert(cave_flowing_water_sound_level_at(10,13)==0);
    portal->flags=CAVE_VERTICAL_CLOSED;cave_events_terrain_changed();
    assert(cave_flowing_water_sound_level_at(10,10)==0);
    portal->flags=0;portal->material=FEAT_LAVA;portal->kind=CAVE_VERTICAL_LAVA_FALL;
    cave_events_terrain_changed();
    assert(cave_lava_sound_level_at(10,10)==CAVE_LAVA_SOUND_RADIUS+1);
    assert(cave_flowing_water_sound_level_at(10,10)==0);
    cave_feat[10][10]=FEAT_FLOOR;cave_events_terrain_changed();
    assert(cave_lava_sound_level_at(10,10)==0);
    assert(!cave_info[10][10]&&Rand_state_export()==rng&&turn==clock_before);
    puts("Unseen/blind waterfall and ceiling sources use local acoustic range and wall blocking; closed/dry portals are silent, lava uses its own loop, no map/RNG/clock/gameplay-noise mutation: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = stairs.ENGINE_FIXTURE
    prefix = fixture[:fixture.index("static const char* guids[]")]
    # SDL's private header already includes the repository's unguarded
    # externs.h. Generation-only declarations are irrelevant to this fixture.
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"',
                            '#include "sdl/main-sdl-private.h"')
    prefix += stairs.fixture_function("terminal_extra") + "\n" + stairs.fixture_function("reset_map") + "\n"
    for statement in ("    memset(dun, 0, sizeof(*dun));\n",
                      "    current_build_vault_exact_token=false;\n",
                      "    current_build_vault_type=0;\n"):
        prefix = prefix.replace(statement, "")
    init = fixture[fixture.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + stairs.CHECKS + CHECKS + init
                      + "checks(); vertical_ui_checks(); vertical_audio_checks(); SDL_Quit(); return 0;\n}\n")
    files = ("src/cmd/world/cmd-interact.c", "src/cmd/item/cmd-item-core.c",
             "src/sdl/input/sdl-player-actions.c", "src/sdl/ui/sdl-question-menu.c",
             "src/ui/status/status-line.c", "src/sdl/ui/sdl-panes.c", "src/cave/cave-events.c")
    status_wrapper = OUT / "status-line-check.c"
    status_wrapper.write_text('#include "angband.h"\n#undef ROW_STATUS\n'
                              '#define ROW_STATUS (Term->hgt - 1)\n'
                              '#include "ui/status/status-line.c"\n')
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj",) + tuple("/" + p + ".obj" for p in files)
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    symbols = ("get_rep_dir", "get_check_near", "do_cmd_go_up", "do_cmd_go_down",
               "handle_stuff", "msg_print", "tutorial_game_command_allowed",
               "cave_vertical_can_travel", "cave_vertical_get", "cave_vertical_current",
               "cave_vertical_describe", "sdl_enqueue_bypassed_command",
               "do_cmd_queue_floor_context_action")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(status_wrapper if p == "src/ui/status/status-line.c" else ROOT / p) for p in files),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *("-Wl,--wrap=" + name for name in symbols), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=40)


if __name__ == "__main__":
    main()
