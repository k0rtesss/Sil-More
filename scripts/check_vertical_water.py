#!/usr/bin/env python3
"""Production vertical terrain: inheritance, currents, time, budgets and travel.

Links the full initialized engine. Confirmation and the cache's packet/travel
boundary are injected; topology, fluid state and action rules are production.
No player save/configuration is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/vertical-water"

CHECKS = r'''
#include "cave/cave-vertical.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "cave/cave-water-flow.h"
#include "level-generation/level-generation-vertical-vaults.h"
static bool accept, receiving;
static int received, received_material, travel_depth, travel_y, travel_x, confirmations;
static int rise_warnings, overflow_warnings;
void __wrap_msg_print(cptr text)
{
    if(text&&strstr(text,"rising"))rise_warnings++;
    if(text&&strstr(text,"Overflow threatens"))overflow_warnings++;
}
bool __wrap_get_check_near(int y,int x,cptr prompt)
{ (void)y;(void)x;(void)prompt;confirmations++;return accept; }
int __wrap_min_depth(void) { return 0; }
bool __wrap_level_cache_travel_to(int depth,int y,int x)
{ travel_depth=depth;travel_y=y;travel_x=x;p_ptr->depth=depth;p_ptr->leaving=true;return true; }
bool __wrap_level_cache_vertical_receive(int depth,u32b id,int material,int amount)
{ assert(depth==7&&id==123&&(material==FEAT_WATER||material==FEAT_LAVA));if(receiving){received+=amount;received_material=material;}return receiving; }
void __wrap_varda_quest_fail_if_bastion_missed(void) {}
void __wrap_note_lost_greater_vault(void) {}
void __wrap_quest_followup_leave(int depth) { (void)depth; }
static cave_vertical_state source;
static void base(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(6);
    p_ptr->py=20;p_ptr->px=20;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->energy_use=0;p_ptr->playing=true;
    character_dungeon=character_generated=true;
    cave_m_idx[20][20]=-1;
    for(int y=0;y<88;y++)for(int x=0;x<88;x++)
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
    turn=100;playerturn=50;
    cave_environment_reset();cave_events_reset();vertical_vault_reset();cave_vertical_reset();
    memset(&source,0,sizeof(source));source.breath=12;source.last_turn=turn;
    source.portal_count=1;source.basin_count=2;
    cave_vertical_portal *p=&source.portals[0];
    p->identity=123;p->y=20;p->x=20;p->peer_depth=7;p->peer_y=24;p->peer_x=24;
    p->kind=CAVE_VERTICAL_STREAM;p->material=FEAT_WATER;p->strength=1;
    p->flags=CAVE_VERTICAL_DOWN|CAVE_VERTICAL_REVERSIBLE;
    for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
        p->local_patch[y][x]=FEAT_WATER;p->peer_patch[y][x]=FEAT_WATER;
        cave_feat[17+y][17+x]=FEAT_WATER;
    }
    source.basins[0]=(cave_vertical_basin){.identity=123,.depth=6,.y=20,.x=20,
        .volume=20,.capacity=144,.normal_volume=20,.material=FEAT_WATER,.radius=3};
    source.basins[1]=(cave_vertical_basin){.identity=123,.depth=7,.y=24,.x=24,
        .volume=72,.capacity=144,.normal_volume=72,.material=FEAT_WATER,.radius=3};
    received=received_material=travel_depth=confirmations=0;receiving=accept=true;
    rise_warnings=overflow_warnings=0;
    assert(cave_vertical_restore(&source));
}
static void checks(void)
{
    base();
    Rand_state_init(8101);int hard=cave_vertical_fall_damage(1,FEAT_FLOOR);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(1,FEAT_DEEP_WATER)==(hard+1)/2);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(1,FEAT_WATER)==(hard*3+3)/4);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(1,FEAT_ICE)==hard);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(1,FEAT_BRIDGE_DEEP_WATER_H)==hard);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(1,FEAT_LAVA)==hard);
    Rand_state_init(8101);int tall=damroll(6,4);
    Rand_state_init(8101);assert(cave_vertical_fall_damage(2,FEAT_FLOOR)==tall);
    source.portals[0].kind=CAVE_VERTICAL_FALL;
    source.portals[0].peer_patch[3][3]=FEAT_DEEP_WATER;
    assert(cave_vertical_restore(&source));
    Rand_state_init(8101);assert(cave_vertical_travel(false));
    assert(p_ptr->chp==100-(hard+1)/2&&travel_depth==7&&p_ptr->energy_use==100);
    puts("Production fall impact scales with height; deep/shallow water reduce without removing damage, ice/lava/bridges stay hard; player uses inherited receiving pool: PASS");

    base();
    p_ptr->depth=7;cave_m_idx[20][20]=0;p_ptr->py=p_ptr->px=3;
    cave_vertical_generation_begin(false);
    cave_vertical_generation_inherit(&source,6,7);
    const cave_vertical_state* state=cave_vertical_get();
    assert(state->portal_count==1&&state->portals[0].y==24&&state->portals[0].x==24);
    assert(state->portals[0].peer_depth==6&&state->portals[0].peer_y==20);
    assert(!(state->portals[0].flags&CAVE_VERTICAL_DOWN));
    assert(cave_vertical_generation_finish());
    assert(cave_feat[24][24]==FEAT_WATER&&cave_feat[4][4]==FEAT_FLOOR);
    puts("Reciprocal offset passage inherits only required patch: PASS");

    base();assert(cave_vertical_travel(false));
    assert(travel_depth==7&&p_ptr->energy_use==100&&confirmations==0);
    base();source.portals[0].peer_depth=5;
    source.portals[0].flags=CAVE_VERTICAL_REVERSIBLE;source.basins[1].depth=5;
    assert(cave_vertical_restore(&source));
    assert(cave_vertical_travel(true));
    assert(travel_depth==5&&travel_y==24&&travel_x==24
        &&p_ptr->energy_use==150&&p_ptr->chp==100&&confirmations==0);
    base();source.portals[0].peer_depth=5;
    source.portals[0].flags=CAVE_VERTICAL_REVERSIBLE|CAVE_VERTICAL_CLOSED;
    source.basins[1].depth=5;assert(cave_vertical_restore(&source));
    assert(cave_vertical_travel(true)&&travel_depth==0&&p_ptr->energy_use==0);
    source.portals[0].flags=CAVE_VERTICAL_REVERSIBLE;
    source.basins[0].flags=CAVE_VERTICAL_RESERVOIR;
    source.basins[0].volume=0;cave_feat[20][20]=FEAT_FLOOR;
    assert(cave_vertical_restore(&source));
    assert(cave_vertical_travel(true)&&travel_depth==5&&p_ptr->energy_use==100);
    puts("Stream passage travel: upstream water costs more, downstream and drained passages retain ordinary cost, closed passage is free: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_RAPIDS;source.portals[0].strength=2;
    assert(cave_vertical_restore(&source));
    int direction=0;
    assert(cave_vertical_current(20,18,&direction)==2&&direction==6);
    assert(cave_vertical_movement_energy(150,20,17,20,18)==100);
    assert(cave_vertical_movement_energy(150,20,19,20,18)==250);
    assert(cave_vertical_movement_energy(400,20,17,20,18)==133);
    cave_feat[20][18]=FEAT_ICE;
    assert(cave_vertical_current(20,18,&direction)==0);
    puts("Current depth independence, downstream cost and frozen footing: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_FALL;
    source.basins[0].flags=CAVE_VERTICAL_GATE;
    assert(cave_vertical_restore(&source));
    cave_vertical_process();assert(cave_vertical_get()->basins[0].volume==20);
    turn=110;cave_vertical_process();state=cave_vertical_get();
    assert(state->basins[0].volume==16&&state->portals[0].pending==4&&received==0);
    turn=120;cave_vertical_process();assert(received==0&&cave_vertical_get()->basins[0].volume==16);
    turn=130;cave_vertical_process();state=cave_vertical_get();
    assert(received==4&&state->basins[0].volume+state->portals[0].pending+received==20);
    cave_vertical_process();assert(received==4);
    receiving=false;turn=1000;cave_vertical_process();state=cave_vertical_get();
    assert(state->basins[0].volume+state->portals[0].pending+received==20);
    puts("Delayed finite sluice packets, missing recipient and same-clock dedup: PASS");

    base();source.portals[0].flags|=CAVE_VERTICAL_CLOSED;
    source.basins[0].flags=CAVE_VERTICAL_GATE;assert(cave_vertical_restore(&source));
    turn=150;cave_vertical_process();assert(cave_vertical_get()->basins[0].volume==20);
    source.portals[0].flags&=~CAVE_VERTICAL_CLOSED;source.last_turn=150;
    assert(cave_vertical_restore(&source));cave_feat[20][20]=FEAT_ICE;
    turn=160;cave_vertical_process();assert(cave_vertical_get()->basins[0].volume==20);
    puts("Closed and frozen outlets stop flow: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_SUMP;assert(cave_vertical_restore(&source));
    accept=false;assert(cave_vertical_travel(false));
    assert(confirmations==1&&p_ptr->energy_use==0&&travel_depth==0);
    accept=true;assert(cave_vertical_travel(false));
    assert(travel_depth==7&&travel_y==24&&travel_x==24&&p_ptr->energy_use==100);
    if (!cave_vertical_get()->submerged || cave_vertical_get()->breath!=11) {
        fputs("FAIL: accepted dive did not spend its first action of air\n",stderr);
        exit(1);
    }
    cave_vertical_end_action();assert(cave_vertical_get()->breath==11);
    puts("Diving cancel is free; accepted passage has one paid exact arrival and spends air once: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_SUMP;
    source.submerged=1;source.breath=4;assert(cave_vertical_restore(&source));
    cave_feat[20][20]=FEAT_DEEP_WATER;
    accept=false;assert(cave_vertical_travel(false));
    assert(cave_vertical_get()->breath==4&&p_ptr->energy_use==0&&travel_depth==0);
    accept=true;assert(cave_vertical_travel(false));
    assert(cave_vertical_get()->breath==3&&travel_depth==7);
    cave_vertical_end_action();assert(cave_vertical_get()->breath==3);
    puts("Repeated underwater travel preserves remaining air; cancellation spends none: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_SUMP;
    source.submerged=1;source.breath=0;assert(cave_vertical_restore(&source));
    cave_feat[20][20]=FEAT_DEEP_WATER;
    assert(cave_vertical_travel(false));
    assert(cave_vertical_get()->breath==0&&p_ptr->chp==96&&travel_depth==7);
    puts("A dive without air applies drowning damage instead of refilling it: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_RAPIDS;
    source.submerged=1;source.breath=0;assert(cave_vertical_restore(&source));
    cave_feat[20][20]=FEAT_DEEP_WATER;p_ptr->chp=3;p_ptr->energy_use=100;
    cave_vertical_end_action();
    if (!p_ptr->is_dead || travel_depth) {
        fputs("FAIL: lethal drowning committed another floor transition\n",stderr);
        exit(1);
    }
    puts("Lethal drowning ends the action before a current can transfer the player: PASS");

    base();source.portals[0].kind=CAVE_VERTICAL_SUMP;
    source.submerged=1;source.breath=4;assert(cave_vertical_restore(&source));
    assert(cave_vertical_travel(false));
    assert(cave_vertical_get()->breath==11);
    base();source.portals[0].kind=CAVE_VERTICAL_SUMP;
    p_ptr->total_weight=350;assert(cave_vertical_restore(&source));
    assert(cave_vertical_travel(false));
    assert(cave_vertical_get()->breath==8);
    puts("Shallow footing permits fresh air; carried weight limits a fresh dive: PASS");

    base();source.submerged=1;source.breath=4;assert(cave_vertical_restore(&source));
    cave_feat[20][20]=FEAT_DEEP_WATER;
    cave_vertical_end_action();assert(cave_vertical_get()->breath==4);
    p_ptr->energy_use=100;cave_vertical_end_action();assert(cave_vertical_get()->breath==3);
    cave_feat[20][20]=FEAT_WATER;cave_vertical_end_action();
    assert(!cave_vertical_get()->submerged&&cave_vertical_get()->breath==12);
    puts("Air advances only on paid actions and recovers in shallows: PASS");

    base();source.basins[0].flags=CAVE_VERTICAL_SOURCE;
    assert(cave_vertical_restore(&source));
    turn=110;cave_vertical_process();turn=120;cave_vertical_process();
    turn=130;cave_vertical_process();state=cave_vertical_get();
    assert(state->basins[0].volume==20&&received==4&&state->portals[0].pending==8);
    assert(state->basins[0].volume+received+state->portals[0].pending==32);
    base();source.basins[0].flags=CAVE_VERTICAL_SOURCE;
    source.portals[0].flags|=CAVE_VERTICAL_CLOSED;
    assert(cave_vertical_restore(&source));
    turn=120;cave_vertical_process();assert(cave_vertical_get()->basins[0].volume==28);
    assert(rise_warnings==1&&overflow_warnings==0);
    cave_vertical_process();assert(rise_warnings==1&&overflow_warnings==0);
    turn=10000;cave_vertical_process();assert(cave_vertical_get()->basins[0].volume==144);
    assert(rise_warnings==1&&overflow_warnings==1);
    cave_world_event warning;assert(cave_event_latest(&warning)&&warning.kind==CAVE_EVENT_WARNING);
    cave_vertical_process();assert(rise_warnings==1&&overflow_warnings==1);
    cave_vertical_state drained;cave_vertical_capture(&drained);
    drained.last_turn=turn; /* Manual drainage starts a fresh, caught-up episode. */
    drained.basins[0].volume=drained.basins[0].normal_volume;
    drained.portals[0].flags&=~CAVE_VERTICAL_CLOSED;
    assert(cave_vertical_restore(&drained));turn+=10;cave_vertical_process();
    assert(!(cave_vertical_get()->basins[0].flags&(CAVE_VERTICAL_RISE_WARNED|CAVE_VERTICAL_OVERFLOW_WARNED)));
    cave_vertical_capture(&drained);drained.portals[0].flags|=CAVE_VERTICAL_CLOSED;
    assert(cave_vertical_restore(&drained));turn+=10;cave_vertical_process();
    assert(rise_warnings==2&&overflow_warnings==1);
    puts("Maintained flow balances supply; blocked source rises within bounded basin: PASS");
    puts("Rising and overflow warnings precede persistent high water, deduplicate without time, reset after drainage and emit physical-origin sound events: PASS");

    base();source.basins[0].material=FEAT_LAVA;
    assert(cave_vertical_receive(&source,6,123,FEAT_WATER,8));
    assert(source.basins[0].volume==12&&source.basins[0].hot==6);
    assert(cave_vertical_receive(&source,6,123,FEAT_WATER,20));
    assert(source.basins[0].material==FEAT_WATER&&source.basins[0].volume==8);
    source.basins[0].volume=source.basins[0].capacity;source.basins[0].material=FEAT_LAVA;
    assert(cave_vertical_receive(&source,6,123,FEAT_WATER,1));
    assert(source.basins[0].volume==143);
    puts("Lava-water mixing consumes both supplies, admits quench at full capacity: PASS");

    base();source.basins[0].flags=CAVE_VERTICAL_RESERVOIR;
    assert(cave_vertical_restore(&source));turn=110;cave_vertical_process();
    cave_vertical_state mixed;cave_vertical_capture(&mixed);
    assert(mixed.portals[0].pending==4&&mixed.portals[0].pending_material==FEAT_WATER);
    assert(cave_vertical_receive(&mixed,6,123,FEAT_LAVA,20));
    assert(mixed.portals[0].material==FEAT_LAVA&&mixed.portals[0].pending_material==FEAT_WATER);
    p_ptr->resist_fire=3;assert(cave_vertical_restore(&mixed));turn=130;cave_vertical_process();
    assert(received==4&&received_material==FEAT_WATER);
    base();source.basins[0].flags=CAVE_VERTICAL_SOURCE;
    assert(cave_vertical_receive(&source,6,123,FEAT_LAVA,40));
    assert(source.portals[0].material==FEAT_WATER&&source.basins[0].material==FEAT_LAVA);
    p_ptr->resist_fire=3;assert(cave_vertical_restore(&source));turn=110;cave_vertical_process();
    assert(cave_vertical_get()->basins[0].volume==16&&cave_vertical_get()->portals[0].pending==0);
    puts("Queued packets retain their material; maintained sources retain their inlet chemistry: PASS");

    base();source.basins[0].material=source.portals[0].material=FEAT_LAVA;
    source.portals[0].kind=CAVE_VERTICAL_TUBE;
    source.basins[0].flags=CAVE_VERTICAL_RESERVOIR;
    source.basins[0].volume=4;source.basins[0].normal_volume=0;
    source.basins[1].volume=source.basins[1].normal_volume=0;
    source.basins[1].material=FEAT_LAVA;
    cave_feat[20][20]=FEAT_LAVA;assert(cave_vertical_restore(&source));
    assert(!cave_vertical_can_travel(false));
    turn=110;cave_vertical_process();state=cave_vertical_get();
    assert(state->basins[0].volume==0&&state->basins[0].hot==6);
    assert(cave_feat[20][20]==FEAT_FLOOR&&state->portals[0].pending==4);
    turn=170;cave_vertical_process();assert(cave_vertical_get()->basins[0].hot==0);
    assert(cave_vertical_can_travel(false));
    cave_vertical_state hot_peer;cave_vertical_capture(&hot_peer);
    hot_peer.basins[1].hot=1;assert(cave_vertical_restore(&hot_peer));
    assert(!cave_vertical_can_travel(false));
    hot_peer.basins[1].hot=0;assert(cave_vertical_restore(&hot_peer));
    p_ptr->energy_use=100;cave_vertical_end_action();assert(!p_ptr->leaving);
    assert(cave_vertical_travel(false)&&travel_depth==7&&confirmations==0);
    puts("Finite lava flow exposes a cooling, then usable conduit: PASS");

    base();source.portal_count=2;source.portals[0].flags|=CAVE_VERTICAL_CLOSED;
    source.portals[1]=source.portals[0];source.portals[1].kind=CAVE_VERTICAL_CONTROL;
    source.portals[1].y=21;source.portals[1].x=20;
    source.portals[1].flags|=CAVE_VERTICAL_GATE;
    assert(cave_vertical_restore(&source));cave_feat[21][20]=FEAT_DOOR_HEAD;
    cave_set_feat(21,20,FEAT_BROKEN);
    assert(cave_vertical_get()->portals[1].flags&CAVE_VERTICAL_BROKEN);
    assert(!(cave_vertical_get()->portals[0].flags&CAVE_VERTICAL_CLOSED));
    assert(cave_vertical_get()->basins[0].flags&CAVE_VERTICAL_RESERVOIR);
    puts("Destroying the real sluice opens a persistent finite release: PASS");
    cave_vertical_state gate_state; cave_vertical_capture(&gate_state);
    for(int p=0;p<gate_state.portal_count;p++) {
        gate_state.portals[p].flags&=~CAVE_VERTICAL_BROKEN;
        gate_state.portals[p].flags|=CAVE_VERTICAL_CLOSED;
    }
    cave_feat[21][20]=FEAT_DOOR_HEAD;assert(cave_vertical_restore(&gate_state));
    assert(cave_vertical_interact(21,20)&&cave_feat[21][20]==FEAT_OPEN);
    cave_set_feat(21,20,FEAT_FLOOR);
    assert(cave_vertical_get()->portals[1].flags&CAVE_VERTICAL_BROKEN);
    assert(cave_vertical_interact(21,20)&&cave_feat[21][20]==FEAT_FLOOR);
    puts("Destroying an already open sluice cannot recreate a closed gate: PASS");

    base();cave_feat[35][35]=FEAT_WATER;
    cave_water_flow_set(35,35,CAVE_WATER_FLOW_EAST);
    assert(cave_vertical_current(35,35,&direction)==1&&direction==6);
    assert(cave_vertical_movement_energy(150,35,34,35,35)==100);
    source.portals[0].flags|=CAVE_VERTICAL_CLOSED;assert(cave_vertical_restore(&source));
    cave_water_flow_set(20,18,CAVE_WATER_FLOW_EAST);
    assert(cave_vertical_current(20,18,&direction)==0);
    puts("Whole river flow supplies gentle current; a closed local outlet overrides stale visual flow: PASS");

    base();cave_feat[19][20]=FEAT_FLOOR;
    cave_set_feat(19,20,FEAT_WATER);
    assert(cave_vertical_get()->basins[0].volume==24);
    cave_set_feat(19,20,FEAT_ICE);cave_set_feat(19,20,FEAT_WATER);
    assert(cave_vertical_get()->basins[0].volume==24);
    puts("External flood volume joins network once; freezing/thawing creates none: PASS");

    unsigned kinds=0;
    const int materials[]={FEAT_WATER,FEAT_DEEP_WATER,FEAT_CHASM,FEAT_LAVA};
    for(unsigned material=0;material<N_ELEMENTS(materials);material++)
        for(int seed=1;seed<=48;seed++) {
            base();character_dungeon=false;
            for(int y=1;y<87;y++)for(int x=1;x<87;x++)cave_feat[y][x]=materials[material];
            Rand_state_init(seed);cave_vertical_generation_begin(true);
            assert(cave_vertical_generation_finish());
            state=cave_vertical_get();
            for(int p=0;p<state->portal_count;p++)kinds|=1u<<state->portals[p].kind;
        }
    const unsigned wanted=(1u<<CAVE_VERTICAL_STREAM)|(1u<<CAVE_VERTICAL_RAPIDS)
        |(1u<<CAVE_VERTICAL_FALL)|(1u<<CAVE_VERTICAL_SUMP)|(1u<<CAVE_VERTICAL_LAVA_FALL)
        |(1u<<CAVE_VERTICAL_LAKE)|(1u<<CAVE_VERTICAL_TUBE);
    assert((kinds&wanted)==wanted);
    puts("Production generation reaches every natural vertical feature over 192 seeded material fixtures: PASS");

    base();source.portal_count=CAVE_VERTICAL_PORTALS+1;assert(!cave_vertical_restore(&source));
    source.portal_count=1;source.portals[0].peer_x=MAX_DUNGEON_WID;
    assert(!cave_vertical_restore(&source));
    puts("Invalid vertical state is rejected: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p)
        for p in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in (
            "get_check_near", "min_depth", "level_cache_travel_to", "level_cache_vertical_receive",
            "msg_print",
            "varda_quest_fail_if_bastion_missed", "note_lost_greater_vault", "quest_followup_leave")),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
            env=env, check=True, timeout=25)


if __name__ == "__main__":
    main()
