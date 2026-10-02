#!/usr/bin/env python3
"""Exercise physical bridge underlays through actual flood and contact paths."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_living_dungeon_save import ENGINE_FIXTURE, fixture_function, FRESH_MAP

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bridge-underlay"

TESTS = r'''
static int checks;
#define CHECK(t) do { ++checks; if (!(t)) { fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#t); exit(1); } } while (0)
static void clean(void) {
    fresh_map(); turn=1000; character_dungeon=true; p_ptr->energy_use=100;
    op_ptr->environment_speed=ENVIRONMENT_SPEED_SLOW;
    partition_meta_save meta={0}; meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_ROOMY; level_partition_meta_set(&meta);
}
static void check_light(bool lava) {
    memset(cave_light,0,sizeof(*cave_light)*MAX_DUNGEON_HGT);
    lava_light(); CHECK(cave_light[10][10]==(lava?3:0));
    CHECK(cave_light[10][11]==(lava?2:0));
}
static void check_audio(bool lava, bool liquid) {
    CHECK((cave_lava_sound_level_at(10,10)>0)==lava);
    CHECK((cave_flowing_water_sound_level_at(10,10)>0)==liquid);
}
static void test_controls(void) {
    const int materials[]={FEAT_LAVA,FEAT_POISON,FEAT_WATER,FEAT_DEEP_WATER,FEAT_CHASM,FEAT_ICE};
    for(int bridge=0;bridge<2;++bridge) for(int n=0;n<6;++n) {
        clean(); int f=materials[n];
        cave_set_feat(10,10,bridge?cave_bridge_feature(f,false):f);
        /* Initial load/generation has no environment metadata yet. */
        CHECK(!cave_environment_get_state().ready);
        CHECK(cave_environment_live_underlay(10,10)==f);
        check_light(f==FEAT_LAVA);
        check_audio(f==FEAT_LAVA,f==FEAT_WATER||f==FEAT_DEEP_WATER||f==FEAT_POISON);
        cave_environment_seed();
        CHECK(cave_environment_live_underlay(10,10)==f);
        CHECK(cave_environment_cell_at(10,10)->heat==(f==FEAT_LAVA?12:(!bridge && f==FEAT_ICE)?-8:0));
        check_light(f==FEAT_LAVA);
        check_audio(f==FEAT_LAVA,f==FEAT_WATER||f==FEAT_DEEP_WATER||f==FEAT_POISON);
    }
    CHECK(cave_environment_live_underlay(-1,10)==FEAT_NONE);
    CHECK(cave_environment_live_underlay(10,p_ptr->cur_map_wid)==FEAT_NONE);
    puts("PASS: ordinary and encoded bridge materials, pre-seed fallback, seeded heat/light/audio, and invalid coordinates.");
}
static void test_flood(int kind) {
    clean(); cave_set_feat(10,10,FEAT_BRIDGE_LAVA_H);
    cave_info[10][10]|=CAVE_SEEN|CAVE_MARK;
    cave_set_feat(10,9,FEAT_TRAP_FLOOD);
    cave_flood_set_trap_kind(10,9,kind);
    cave_environment_seed(); check_light(true); check_audio(true,false);
    /* Physical consumers must not use remembered lava after an unseen edit. */
    cave_info[10][10]&=~CAVE_SEEN;
    cave_flood_begin_action(); cave_flood_trigger(10,9); cave_flood_end_action();
    for(int n=0;n<2;++n) { cave_flood_begin_action(); cave_flood_end_action(); }
    int material=kind==CAVE_FLOOD_KIND_ACID?FEAT_POISON:FEAT_WATER;
    const environment_cell* c=cave_environment_cell_at(10,10);
    CHECK(c->integrity==(kind==CAVE_FLOOD_KIND_ACID?36:68));
    CHECK(c->underlay==material && cave_feat[10][10]==FEAT_BRIDGE_LAVA_H);
    CHECK(cave_environment_display_underlay(10,10)==FEAT_LAVA);
    CHECK(cave_environment_live_underlay(10,10)==material);
    check_light(false); check_audio(false,true);
    turn+=100; cave_environment_process();
    CHECK(cave_environment_cell_at(10,10)->heat==0);
    CHECK(cave_environment_live_underlay(10,10)==material);
    cave_info[10][10]|=CAVE_SEEN; cave_environment_observe(10,10);
    CHECK(cave_environment_display_underlay(10,10)==material);
    printf("PASS: actual %s flood replaces surviving lava bridge underlay, clears lava light/heat/audio, and preserves unseen memory.\n",kind==CAVE_FLOOD_KIND_ACID?"acid":"water");
}
static void test_same_turn_contact(void) {
    clean(); cave_set_feat(10,10,FEAT_BRIDGE_WATER_H); cave_environment_seed();
    check_audio(false,true); check_light(false); /* Prime cached ambient fields. */
    CHECK(cave_environment_catastrophe_contact(10,10,FEAT_LAVA,0,1));
    CHECK(cave_feat[10][10]==FEAT_BRIDGE_WATER_H);
    CHECK(cave_environment_cell_at(10,10)->integrity==91);
    CHECK(cave_environment_live_underlay(10,10)==FEAT_LAVA);
    check_audio(true,false); check_light(true); /* Same turn; no feature edit. */
    turn+=100; cave_environment_process();
    CHECK(cave_environment_cell_at(10,10)->heat==12);
    /* The ordinary flood contact path must invalidate the same cache too. */
    cave_environment_flood_bridge(10,10,FEAT_POISON,1);
    CHECK(cave_environment_live_underlay(10,10)==FEAT_POISON);
    check_audio(false,true); check_light(false);
    puts("PASS: lava entering a water bridge creates live heat/light/audio; both contact paths refresh audio within the same turn.");
    /* Destruction leaves historical bridge metadata, but it is not live. */
    cave_set_feat(10,10,FEAT_FLOOR);
    CHECK((cave_environment_cell_at(10,10)->flags&ENV_BRIDGE)!=0);
    CHECK(cave_environment_cell_at(10,10)->integrity==0);
    CHECK(cave_environment_live_underlay(10,10)==FEAT_FLOOR);
    check_audio(false,false); check_light(false);
    puts("PASS: destroyed bridge metadata cannot create ghost liquid sources.");
}
static void test_reset_and_legacy_floor(void) {
    clean(); cave_set_feat(10,10,FEAT_BRIDGE_WATER_H); cave_environment_seed();
    CHECK(cave_environment_catastrophe_contact(10,10,FEAT_LAVA,0,1));
    CHECK(cave_environment_live_underlay(10,10)==FEAT_LAVA);
    cave_environment_reset();
    CHECK(cave_environment_live_underlay(10,10)==FEAT_WATER);
    check_light(false); check_audio(false,true);
    cave_environment_seed(); CHECK(cave_environment_cell_at(10,10)->heat==0);
    clean(); cave_info[10][10]|=CAVE_CHASM_AREA;
    cave_set_feat(9,10,FEAT_CHASM); cave_set_feat(11,10,FEAT_CHASM);
    CHECK(cave_environment_live_underlay(10,10)==FEAT_FLOOR);
    cave_environment_seed();
    CHECK(cave_environment_cell_at(10,10)->flags&ENV_BRIDGE);
    CHECK(cave_environment_live_underlay(10,10)==FEAT_CHASM);
    puts("PASS: reset/reseed uses encoded fallback; historical floor-over-chasm spans use their live underlay.");
}
static void test_live_quenching(void) {
    for(int water=0;water<2;++water) {
        clean();
        cave_set_feat(10,10,water?FEAT_BRIDGE_LAVA_H:FEAT_BRIDGE_WATER_H);
        cave_set_feat(10,11,FEAT_LAVA); cave_environment_seed();
        /* Isolate thermal contact from ordinary source growth/recession. */
        for(int i=0;i<cave_environment_get_state().source_count;++i) {
            environment_source s=*cave_environment_source_at(i);
            s.next_turn=turn+10000; CHECK(cave_environment_restore_source(i,s));
        }
        p_ptr->update=0;
        cave_environment_flood_bridge(10,10,water?FEAT_WATER:FEAT_LAVA,1);
        CHECK((p_ptr->update&(PU_UPDATE_VIEW|PU_MONSTERS))==(PU_UPDATE_VIEW|PU_MONSTERS));
        CHECK(cave_environment_live_underlay(10,10)==(water?FEAT_WATER:FEAT_LAVA));
        for(int n=0;n<5;++n) { turn+=100; cave_environment_process(); }
        CHECK(cave_feat[10][11]==(water?FEAT_FLOOR:FEAT_LAVA));
    }
    puts("PASS: lava quenching uses current water/lava bridge underlay rather than the former encoded material.");
}
static void test_live_reheat(void) {
    for(int water=0;water<2;++water) {
        clean();
        cave_set_feat(10,10,water?FEAT_BRIDGE_LAVA_H:FEAT_BRIDGE_WATER_H);
        cave_set_feat(10,12,FEAT_LAVA); cave_environment_seed();
        int id=cave_environment_cell_at(10,12)->owner;
        CHECK(id>0);
        for(int i=0;i<cave_environment_get_state().source_count;++i) {
            environment_source s=*cave_environment_source_at(i);
            s.next_turn=turn+10000;
            if(i==id-1) { s.used=s.capacity=1; s.phase=0; s.next_turn=turn; }
            CHECK(cave_environment_restore_source(i,s));
        }
        /* Restore one valid cooled, paid-for fringe cell beside its reservoir. */
        environment_cell fringe=*cave_environment_cell_at(10,11);
        fringe.owner=id; fringe.base_feat=FEAT_FLOOR; fringe.flags|=ENV_DEPOSIT;
        CHECK(cave_environment_restore_cell(10,11,fringe));
        cave_environment_flood_bridge(10,10,water?FEAT_WATER:FEAT_LAVA,1);
        turn+=100; cave_environment_process();
        CHECK(cave_environment_pending_hazard(10,11)==(water?0:FEAT_LAVA));
        CHECK(cave_environment_source_at(id-1)->used==1);
    }
    puts("PASS: existing spent lava fringe can reheat beside live lava but remains quenched beside live water, with supply unchanged.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for header in ("cave/cave-environment.h", "cave/cave-events.h", "cave/cave-flood.h",
                   "cave/cave-fixtures.h", "cave/cave-water-flow.h", "cave/cave-bridge.h"):
        prefix += '\n#include "' + header + '"\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n" +
                      fixture_function("reset_map") + "\n" + FRESH_MAP + "\n" + TESTS + "\n" + init +
                      "    test_controls();test_flood(CAVE_FLOOD_KIND_ACID);test_flood(CAVE_FLOOD_KIND_WATER);"
                      "test_same_turn_contact();test_reset_and_legacy_floor();test_live_quenching();test_live_reheat();\n"
                      '    printf("Bridge underlay: %d checks PASS.\\n",checks);SDL_Quit();return 0;\n}\n',
                      encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                                env=env, text=True, capture_output=True, timeout=30)
        (OUT / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr[-5000:])
            result.check_returncode()


if __name__ == "__main__":
    main()
