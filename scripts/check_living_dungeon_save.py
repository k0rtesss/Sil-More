#!/usr/bin/env python3
"""Exercise the real v17 dungeon extension, compaction, and v16 migration.

Requires build-incremental.ps1. Uses only temporary engine fixture data.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, WRITER, READER, TESTS as SCENT_TESTS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/living-dungeon-save'

start = SCENT_TESTS.index('static void fresh_map(void)')
FRESH_MAP = SCENT_TESTS[start:SCENT_TESTS.index('\n}', start) + 2]
TESTS = r'''
size_t fixture_write_dungeon(byte*, size_t, size_t*);
int fixture_read_dungeon(const byte*, size_t, int, u32b*, size_t*);
static byte encoded[262144], plain[262144], modified[262144], corrupted[262144];
static environment_cell expected_cells[20][24];
static environment_source expected_source;
static environment_state expected_state;
static monster_world_state expected_world;
static cave_world_event expected_events[CAVE_EVENTS_MAX];
static u32b expected_serial;
static size_t extension_offset, extension_end, cell_offset, event_offset, world_offset;
static size_t saved_length;
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#t); exit(1); } } while(0)
static void decode(const byte* source, byte* target, size_t size)
{
    byte previous=0;
    for(size_t i=0;i<size;i++) { byte current=source[i]; target[i]=current^previous; previous=current; }
}
static void encode(const byte* source, byte* target, size_t size)
{
    byte previous=0;
    for(size_t i=0;i<size;i++) { previous^=source[i]; target[i]=previous; }
}
static unsigned word(const byte* p) { return p[0] + 256U*p[1]; }
static void clean_map(void)
{
    fresh_map(); turn=1000;
    cave_environment_reset();
}
static void make_fixture(void)
{
    clean_map();
    CHECK(place_monster_one(8,8,41,false,false,NULL));
    int first = cave_m_idx[8][8];
    CHECK(place_monster_one(8,9,42,false,false,NULL));
    int survivor = cave_m_idx[8][9]; CHECK(survivor > first);
    delete_monster_idx(first);
    monster_type* m = &mon_list[survivor];
    m->world = (monster_world_state){ .last_event=2, .initialized=1,
        .observation_kind=CAVE_EVENT_BUILD, .observation_y=9, .observation_x=10,
        .observation_age=7, .task=MON_WORLD_BRIDGE, .target_y=9, .target_x=11,
        .task_age=11, .retries=2, .cooldown=3, .home_y=8, .home_x=9, .supplies=5 };
    expected_world=m->world;
    cave_feat[9][10]=FEAT_WATER;
    cave_feat[9][11]=FEAT_BRIDGE_WATER_H;
    cave_environment_seed();
    environment_state state=cave_environment_get_state();
    state.random=0x12345678U; state.last_turn=990; state.source_count=1;
    state.mineral_budget=3; state.ready=true;
    CHECK(cave_environment_restore_state(state));
    environment_source source={.y=9,.x=10,.feature=FEAT_WATER,.capacity=12,
        .used=4,.phase=7,.next_turn=1030};
    CHECK(cave_environment_restore_source(0,source));
    environment_cell c=*cave_environment_cell_at(9,11);
    c.flags=ENV_BRIDGE|ENV_NATURAL; c.base_feat=FEAT_WATER;
    c.known_feat=FEAT_WATER; c.bridge_feat=FEAT_BRIDGE_WATER_H;
    c.underlay=FEAT_WATER; c.material=ENV_BRIDGE_WOOD;
    c.integrity=57; c.work=2; c.pending_feat=FEAT_WATER;
    c.owner=1; c.heat=4; c.due=1040;
    CHECK(cave_environment_restore_cell(9,11,c));
    c=*cave_environment_cell_at(10,12);
    c.flags=ENV_FLOOR_ICE|ENV_MINERAL_SPENT|ENV_DEPOSIT;
    c.base_feat=FEAT_FLOOR; c.known_feat=FEAT_ICE; c.heat=-7;
    CHECK(cave_environment_restore_cell(10,12,c));
    turn-=100; /* Expired sounds still identify the last wizard destination. */
    cave_event_emit(CAVE_EVENT_BUILD,9,11,12);
    cave_event_emit(CAVE_EVENT_WARNING,9,11,20);
    turn+=100;
    expected_state=cave_environment_get_state(); expected_source=*cave_environment_source_at(0);
    for(int y=0;y<20;y++) for(int x=0;x<24;x++) expected_cells[y][x]=*cave_environment_cell_at(y,x);
    memcpy(expected_events,cave_events_state(),sizeof(expected_events));
    expected_serial=cave_events_next_serial();
    saved_length=fixture_write_dungeon(encoded,sizeof(encoded),&extension_end);
    CHECK(mon_max==2 && cave_m_idx[8][9]==1);
    CHECK(!memcmp(&mon_list[1].world,&expected_world,sizeof(expected_world)));
    decode(encoded,plain,saved_length);
    const byte header[]={0x17,0xEC,1,0x78,0x56,0x34,0x12};
    int found=0;
    for(size_t i=0;i+sizeof(header)<=extension_end;i++) if(!memcmp(plain+i,header,sizeof(header)))
    { extension_offset=i; found++; }
    CHECK(found==1);
    cell_offset=extension_offset+13+10;
    size_t cursor=cell_offset; unsigned cells=0;
    while(cells<20*24) { unsigned run=word(plain+cursor); CHECK(run>0); cells+=run; cursor+=20; }
    CHECK(cells==20*24);
    event_offset=cursor;
    world_offset=event_offset+4+12*CAVE_EVENTS_MAX;
    CHECK(word(plain+world_offset)==2);
    CHECK(world_offset+2+18==extension_end);
}
static void test_current_roundtrip(void)
{
    make_fixture(); clean_map();
    u32b sentinel; size_t consumed;
    CHECK(fixture_read_dungeon(encoded,saved_length,17,&sentinel,&consumed)==0);
    CHECK(sentinel==0xA1B2C3D4U && consumed==saved_length);
    CHECK(mon_max==2 && cave_m_idx[8][9]==1);
    CHECK(!memcmp(&mon_list[1].world,&expected_world,sizeof(expected_world)));
    environment_state s=cave_environment_get_state();
    CHECK(s.random==expected_state.random && s.last_turn==expected_state.last_turn
        && s.ready==expected_state.ready && s.source_count==expected_state.source_count
        && s.mineral_budget==expected_state.mineral_budget);
    CHECK(!memcmp(cave_environment_source_at(0),&expected_source,sizeof(expected_source)));
    for(int y=0;y<20;y++) for(int x=0;x<24;x++)
        CHECK(!memcmp(cave_environment_cell_at(y,x),&expected_cells[y][x],sizeof(environment_cell)));
    CHECK(cave_environment_bridge_progress(9,11)==2);
    CHECK(cave_environment_pending_hazard(9,11)!=0);
    CHECK(cave_events_next_serial()==expected_serial);
    CHECK(!memcmp(cave_events_state(),expected_events,sizeof(expected_events)));
    cave_world_event latest;
    CHECK(cave_event_latest(&latest) && latest.serial==expected_serial-1);
    CHECK(latest.y==9 && latest.x==11 && latest.kind==CAVE_EVENT_WARNING);
    CHECK(!cave_event_for_listener(9,11,100,0,&latest));
    puts("v17: source budget, environmental cells, warning, bridge work, events and compacted actor job roundtrip PASS.");
}
static void test_legacy_v16(void)
{
    /* Append-only extension removal preserves the complete historical v16
     * dungeon (including scent, water flow, flood and surface-marker lanes). */
    memcpy(corrupted,plain,extension_offset);
    memcpy(corrupted+extension_offset,plain+extension_end,8);
    size_t length=extension_offset+8;
    encode(corrupted,modified,length);
    clean_map();
    cave_event_emit(CAVE_EVENT_COLLAPSE,5,5,30);
    u32b sentinel; size_t consumed;
    CHECK(fixture_read_dungeon(modified,length,16,&sentinel,&consumed)==0);
    CHECK(sentinel==0xA1B2C3D4U && consumed==length);
    CHECK(mon_max==2 && cave_m_idx[8][9]==1);
    monster_world_state empty={0};
    CHECK(!memcmp(&mon_list[1].world,&empty,sizeof(empty)));
    CHECK(cave_environment_get_state().ready);
    CHECK(cave_environment_bridge_progress(9,11)==0);
    CHECK(cave_environment_pending_hazard(9,11)==0);
    for(int i=0;i<CAVE_EVENTS_MAX;i++) CHECK(!cave_events_state()[i].serial);
    puts("v16: exact byte consumption, safe ecology seed, no stale job/event/work/warning PASS.");
}
static void reject_byte(size_t offset, byte value)
{
    memcpy(corrupted,plain,saved_length); corrupted[offset]=value;
    encode(corrupted,modified,saved_length); clean_map();
    u32b sentinel; size_t consumed;
    CHECK(fixture_read_dungeon(modified,saved_length,17,&sentinel,&consumed)!=0);
}
static void test_corruption(void)
{
    reject_byte(extension_offset,0); /* magic */
    reject_byte(extension_offset+11,ENV_SOURCES_MAX+1);
    reject_byte(extension_offset+12,9); /* mineral budget */
    reject_byte(extension_offset+13+4,64); /* used > capacity */
    reject_byte(cell_offset,0); /* first RLE run must be one byte: verify first */
    reject_byte(cell_offset+2,255); /* cell flags */
    reject_byte(cell_offset+2+12,127); /* heat */
    reject_byte(event_offset+4+11,65); /* event volume */
    reject_byte(world_offset,3); /* count mismatch */
    reject_byte(world_offset+2+9,255); /* world task */
    const size_t cuts[]={0,1,12,13,22,23};
    for(unsigned i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++) {
        clean_map(); u32b sentinel; size_t consumed;
        CHECK(fixture_read_dungeon(encoded,extension_offset+cuts[i],17,&sentinel,&consumed)!=0);
    }
    const size_t positions[]={cell_offset+3,event_offset+3,event_offset+15,
        world_offset+1,world_offset+8,extension_end-1,extension_end+7};
    for(unsigned i=0;i<sizeof(positions)/sizeof(positions[0]);i++) {
        clean_map(); u32b sentinel; size_t consumed;
        CHECK(fixture_read_dungeon(encoded,positions[i],17,&sentinel,&consumed)!=0);
    }
    puts("v17: invalid headers, budgets, RLE, cells, events, actor jobs and truncated tails rejected PASS.");
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+='\n#include "monster/monster-ai.h"\n#include "monster/monster-senses.h"\n#include "monster/monster-world.h"\n'
    prefix+='#include "cave/cave-fixtures.h"\n#include "cave/cave-flood.h"\n#include "cave/cave-water-flow.h"\n'
    prefix+='#include "cave/cave-environment.h"\n#include "cave/cave-events.h"\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    harness=prefix+fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'+FRESH_MAP+'\n'+TESTS+'\n'+init
    harness+='    test_current_roundtrip(); test_legacy_v16(); test_corruption();\n'
    harness+='    printf("Living dungeon persistence: %d checks PASS.\\n",checks);\n    SDL_Quit(); return 0;\n}\n'
    sources=[]
    for name,content in (('check.c',harness),('writer.c',WRITER),('reader.c',READER)):
        source=OUT/name; source.write_text(content,encoding='utf-8'); sources.append(str(source))
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded=('/src/main.c.obj','/src/fs/save.c.obj','/src/fs/load.c.obj')
    objects=[p for p in objects if not p.endswith(excluded)]
    response=OUT/'objects.rsp'; response.write_text('\n'.join('"'+p+'"' for p in objects),encoding='utf-8')
    env=os.environ.copy()
    env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/name) for name in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',*sources,'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        result=subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,
            capture_output=True,text=True,timeout=90)
        (OUT/'validation.log').write_text(result.stdout+result.stderr,encoding='utf-8')
        print(result.stdout,end='')
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()

if __name__=='__main__': main()
