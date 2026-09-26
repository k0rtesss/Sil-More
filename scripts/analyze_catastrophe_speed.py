#!/usr/bin/env python3
"""Measure production catastrophe pacing without modifying tuning or player saves.

Build first. Outputs CSVs in scripts/output/catastrophe-speed. Each generated
map is serialized once and restored for every material/anger comparison.
"""
from pathlib import Path
import csv
import os
import shlex
import subprocess
import sys
import statistics
import hashlib
import json
import tempfile
from check_utumno_generation import HARNESS
from check_living_dungeon_save import WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/catastrophe-speed'

SIMULATION = r'''
#include "cave/cave-events.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
size_t fixture_write_dungeon(byte*,size_t,size_t*);
int fixture_read_dungeon(const byte*,size_t,int,u32b*,size_t*);
static byte snapshot[4*1024*1024];
static size_t snapshot_length;
static byte initial[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte ground[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte hit[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int ground_count;
static int paired_y,paired_x;
static FILE *summary,*curves;
static const char* names[]={"auto","water","acid","lava","ice","chasm"};

static bool dry(int f) {
    return f==FEAT_FLOOR||f==FEAT_OPEN||f==FEAT_BROKEN||FEAT_IS_TRAP(f)
        ||FEAT_IS_BRIDGE(f)||(f>=FEAT_DOOR_HEAD&&f<=FEAT_DOOR_TAIL)
        ||f==FEAT_SECRET||f==FEAT_WARDED||f==FEAT_WARDED2||f==FEAT_WARDED3
        ||(f>=FEAT_STAIR_HEAD&&f<=FEAT_STAIR_TAIL);
}
static int feature(int k) {
    int f[]={0,FEAT_WATER,FEAT_POISON,FEAT_LAVA,FEAT_ICE,FEAT_CHASM};return f[k];
}
static bool hazard(int k,int f) {
    return k==CATA_WATER?(f==FEAT_WATER||f==FEAT_DEEP_WATER)
        :k==CATA_ICE?FEAT_IS_ICE(f):f==feature(k);
}
static int route(void) {
    static int q[MAX_DUNGEON_HGT*MAX_DUNGEON_WID];
    static int dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(dist,0,sizeof(dist));int head=0,tail=1;
    q[0]=p_ptr->py*MAX_DUNGEON_WID+p_ptr->px;dist[p_ptr->py][p_ptr->px]=1;
    while(head<tail) {
        int at=q[head++],y=at/MAX_DUNGEON_WID,x=at%MAX_DUNGEON_WID;
        if(cave_feat[y][x]==FEAT_LESS||cave_feat[y][x]==FEAT_LESS_SHAFT)return dist[y][x]-1;
        for(int d=0;d<8;d++) {
            int ny=y+ddy_ddd[d],nx=x+ddx_ddd[d];
            if(!in_bounds_fully(ny,nx)||dist[ny][nx]||!dry(cave_feat[ny][nx]))continue;
            if(ny!=y&&nx!=x&&(!dry(cave_feat[ny][x])||!dry(cave_feat[y][nx])))continue;
            dist[ny][nx]=dist[y][x]+1;q[tail++]=ny*MAX_DUNGEON_WID+nx;
        }
    }
    return -1;
}
static void observer(void) {
    p_ptr->is_dead=p_ptr->leaving=p_ptr->on_the_run=false;
    p_ptr->game_type=0;p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=30000;
    p_ptr->resist_fire=p_ptr->resist_cold=p_ptr->resist_pois=20;
    p_ptr->energy_use=100;character_dungeon=true;cheat_timestop=false;
}
static void remember_map(void) {
    ground_count=0;
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++) {
        initial[y][x]=cave_feat[y][x];
        ground[y][x]=dry(initial[y][x])&&!catastrophe_protected(y,x);
        ground_count+=ground[y][x];
    }
}
static void save_map(void) {
    size_t end;snapshot_length=fixture_write_dungeon(snapshot,sizeof(snapshot),&end);
}
static void restore_map(void) {
    u32b sentinel;size_t consumed;
    character_dungeon=false;
    wipe_o_list();wipe_mon_list();
    assert(!fixture_read_dungeon(snapshot,snapshot_length,VERSION_EXTRA,&sentinel,&consumed));
    assert(consumed==snapshot_length&&sentinel==0xA1B2C3D4U);
    observer();
}
static void run(const char* scenario,int sample,int kind,int anger,int horizon) {
    observer();turn=1000;playerturn=1;p_ptr->morgoth_state=anger;
    catastrophe_reset_run();cave_events_reset();memset(hit,0,sizeof(hit));
    catastrophe_state s=catastrophe_get_state();
    s.random=0x72af9183U+(u32b)sample*1619U;assert(catastrophe_restore_state(s));
    int initial_route=route(),lost=-1,near=-1;
    int times[4]={-1,-1,-1,-1},thresholds[4]={10,50,90,99};
    int crossings[4]={-1,-1,-1,-1},distances[4]={10,25,50,100};
    if(!strcmp(scenario,"paired")) {
        /* Controlled source experiment: same exposed floor on each restored
         * generated map, for every material. Bypass selection, not physics. */
        s=catastrophe_get_state();s.kind=kind;s.y=paired_y;s.x=paired_x;s.ready=true;
        assert(catastrophe_restore_state(s));assert(catastrophe_restore_cell(s.y,s.x,1));
        assert(cave_environment_catastrophe_change(s.y,s.x,feature(kind)));
        catastrophe_begin_action();catastrophe_end_action();
    } else if(!catastrophe_start(kind))return;
    s=catastrophe_get_state();int oy=s.y,ox=s.x;
    int touched=0,active=0,deep=0,changed=0,last_change=0,max_radius=0;
    for(int step=1;step<=horizon;step++) {
        if(step>1) {
            playerturn++;turn+=10;catastrophe_begin_action();
            player_lava_begin_action();player_poison_terrain_begin_action();
            catastrophe_end_action();player_lava_end_action();player_poison_terrain_end_action();
        }
        assert(!p_ptr->is_dead&&!p_ptr->leaving);
        int prior=touched;touched=active=deep=changed=max_radius=0;
        for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
            int f=cave_feat[y][x];bool owned=catastrophe_owns(y,x);
            if(owned&&(kind==CATA_ICE||f!=initial[y][x])&&ground[y][x])hit[y][x]=1;
            if(hit[y][x]) {
                touched++;
                max_radius=MAX(max_radius,MAX(abs(y-oy),abs(x-ox)));
                if(near<0&&distance(y,x,p_ptr->py,p_ptr->px)<=5)near=step;
            }
            active+=owned&&hazard(kind,f);deep+=owned&&f==FEAT_DEEP_WATER;
            changed+=owned&&f!=initial[y][x];
        }
        if(touched>prior)last_change=step;
        for(int i=0;i<4;i++) {
            if(times[i]<0&&touched*100>=ground_count*thresholds[i])times[i]=step;
            if(crossings[i]<0&&max_radius>=distances[i])crossings[i]=step;
        }
        if(initial_route>=0&&lost<0&&route()<0)lost=step;
        fprintf(curves,"%s,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d\n",scenario,sample,
            p_ptr->depth,names[kind],anger,step,ground_count,touched,active,deep,changed,max_radius);
    }
    fprintf(summary,"%s,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        scenario,sample,p_ptr->depth,p_ptr->cur_map_hgt,p_ptr->cur_map_wid,names[kind],anger,
        ground_count,oy,ox,times[0],times[1],times[2],times[3],near,initial_route,lost,
        touched,active,deep,changed,last_change,crossings[0],crossings[1],crossings[2],crossings[3]);
    fflush(summary);fflush(curves);
}
static void artificial(int side,bool corridor,int kind) {
    character_dungeon=false;wipe_o_list();wipe_mon_list();player_wipe();
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    p_ptr->depth=10;p_ptr->cur_map_hgt=corridor?5:side+2;p_ptr->cur_map_wid=side+2;
    p_ptr->py=1;p_ptr->px=1;
    cave_environment_reset();cave_flood_clear();cave_water_flow_reset();
    memset(cave_m_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_m_idx));
    memset(cave_o_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_o_idx));
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_ROOMY;level_partition_meta_set(&meta);
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++) {
        int f=(y>0&&x>0&&y<p_ptr->cur_map_hgt-1&&x<p_ptr->cur_map_wid-1
            &&(!corridor||y==2))?FEAT_FLOOR:FEAT_WALL_PERM;
        cave_feat[y][x]=f;cave_info[y][x]=f==FEAT_FLOOR?0:CAVE_WALL;
        cave_natural[y][x]=0;cave_color[y][x]=COLOR_STYLE_BASE;
    }
    if(corridor){p_ptr->py=2;p_ptr->px=side;}
    cave_set_feat(p_ptr->py,p_ptr->px,FEAT_MORE);cave_m_idx[p_ptr->py][p_ptr->px]=-1;
    if(!corridor)cave_set_feat(1,side,FEAT_LESS);
    observer();remember_map();
    cave_set_feat(corridor?2:side/2,corridor?1:side/2,feature(kind));cave_environment_seed();
}
int main(int argc,char** argv) {
    assert(argc==5);setbuf(stdout,NULL);log_set_level(LOG_ERROR);
    bool paired_only=!strcmp(argv[4],"paired-only");
    assert(SDL_Init(SDL_INIT_EVENTS));assert(term_init(&test_term,80,24,256)==0);
    test_term.xtra_hook=dummy_xtra;angband_term[0]=&test_term;Term_activate(&test_term);
    initialize(argv[1],argv[2]);small_maps=extra_stairs=false;
    char path[1024];snprintf(path,sizeof(path),"%s/summary.csv",argv[3]);summary=fopen(path,paired_only?"a":"w");assert(summary);
    snprintf(path,sizeof(path),"%s/curves.csv",argv[3]);curves=fopen(path,paired_only?"a":"w");assert(curves);
    if(!paired_only) {
    fputs("scenario,sample,depth,height,width,kind,anger,ground,origin_y,origin_x,t10,t50,t90,t99,near_player,initial_exit_distance,exit_lost,ground_hit_end,active_end,deep_end,changed_end,last_ground_gain,d10,d25,d50,d100\n",summary);
    fputs("scenario,sample,depth,kind,anger,step,ground,ground_hit,active,deep,changed,radius\n",curves);
    int sizes[]={30,50,100};
    for(int size=0;size<4;size++)for(int k=1;k<CATA_KIND_MAX;k++)for(int a=0;a<=6;a+=2) {
        int side=size<3?sizes[size]:200;
        artificial(side,size==3,k);run(size==3?"corridor":"chamber",side,k,a,200);
    }
    puts("80 chamber/corridor runs complete.");
    }
    int depths[]={2,5,10,15,18};
    for(int di=0;di<5;di++)for(int seed=0;seed<3;seed++) {
        if(paired_only&&seed)continue;
        int sample=1000+depths[di]*10+seed;
        character_dungeon=false;wipe_o_list();wipe_mon_list();player_wipe();
        rp_ptr=&p_info[0];current_character_profile=&c_info[0];observer();
        p_ptr->depth=p_ptr->max_depth=depths[di];p_ptr->fixed_forge_count=3;
        turn=1000;playerturn=1;character_generated=true;Rand_state_init(sample);attempts=0;
        partition_passes=tunnel_passes=terrain_passes=population_passes=first_size=0;
        generate_cave();observer();cave_set_feat(p_ptr->py,p_ptr->px,FEAT_MORE);
        remember_map();save_map();
        int cases=0;
        if(!paired_only)for(int k=1;k<CATA_KIND_MAX;k++)for(int a=0;a<=6;a+=2) {
            restore_map();if(!catastrophe_can_start(k))continue;
            run("generated",sample,k,a,200);cases++;
        }
        if(seed==0) {
            restore_map();int best=100000;
            for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
                if(cave_feat[y][x]==FEAT_FLOOR&&!catastrophe_protected(y,x)) {
                    int d=distance(y,x,p_ptr->cur_map_hgt/2,p_ptr->cur_map_wid/2);
                    if(d<best){best=d;paired_y=y;paired_x=x;}
                }
            assert(best<100000);
            for(int k=1;k<CATA_KIND_MAX;k++)for(int a=0;a<=6;a+=2) {
                restore_map();run("paired",sample,k,a,200);cases++;
            }
        }
        printf("Map %d: depth %d, %dx%d, %d dry ground cells, %d available comparisons.\n",
            sample,depths[di],p_ptr->cur_map_hgt,p_ptr->cur_map_wid,ground_count,cases);
    }
    fclose(summary);fclose(curves);SDL_Quit();puts("Simulation complete.");return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    files = [ROOT / 'src/cave/cave-catastrophe.c', ROOT / 'src/cave/cave-environment.c',
             BUILD / 'CMakeFiles/sil-more.dir/src/cave/cave-catastrophe.c.obj']
    metadata = {'actions_per_run': 200, 'sha256': {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
    (OUT / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    source = OUT / 'simulation.c'
    source.write_text(HARNESS[:HARNESS.index('int main(int argc,char **argv)')] + SIMULATION,
                      encoding='utf-8')
    writer, reader = OUT / 'writer.c', OUT / 'reader.c'
    writer.write_text(WRITER, encoding='utf-8')
    reader.write_text(READER, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects = [p for p in objects if not p.endswith(('/src/main.c.obj', '/src/fs/save.c.obj', '/src/fs/load.c.obj'))]
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / name) for name in
                                   ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
                                  'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    wraps = ['get_sdl_config_path', 'level_gen_screen_start_attempt', 'process_player',
             'apply_quadrant_generation_modes', 'connect_rooms_stairs', 'place_dungeon_terrain',
             'run_partition_monster_pass']
    exe = OUT / 'simulation.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O2',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(writer), str(reader),
                    '@' + str(response), '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *('-Wl,--wrap=' + name for name in wraps), '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='data-', dir=OUT) as data:
        mode = 'paired-only' if '--paired-only' in sys.argv else 'all'
        subprocess.run([str(exe), str(ROOT), data, str(OUT), mode], cwd=data, env=env,
                       check=True, timeout=900)
    with (OUT / 'summary.csv').open(newline='') as f:
        rows = list(csv.DictReader(f))
    print(f'{len(rows)} production-controller simulations saved to {OUT}', flush=True)
    report()


def report():
    rows = list(csv.DictReader((OUT / 'summary.csv').open(newline='')))
    curves = list(csv.DictReader((OUT / 'curves.csv').open(newline='')))
    kinds = ['water', 'acid', 'lava', 'ice', 'chasm']
    angers = [0, 2, 4, 6]
    labels = ['Water', 'Acid', 'Lava', 'Ice', 'Chasm']
    keys = [(r['scenario'], r['sample'], r['kind'], r['anger']) for r in rows]
    assert len(keys) == len(set(keys)), 'Duplicate scenario: rerun without --paired-only.'
    assert len(curves) == len(rows) * 200
    for r in curves:
        assert 0 <= int(r['ground_hit']) <= int(r['ground'])
        if r['scenario'] in ['chamber', 'corridor']:
            # Physical travel is bounded, independently of area supply.
            rounds = 2 if r['kind'] in ['water', 'acid'] else 1
            assert int(r['radius']) <= int(r['step'])*rounds, r
    # Corridor fronts must move, but cannot chain a whole passage in one action.
    for r in rows:
        if r['scenario'] == 'corridor':
            rounds = 2 if r['kind'] in ['water', 'acid'] else 1
            assert int(r['d100']) >= 100//rounds, r
    for scenario in ['generated', 'paired']:
        groups = {}
        for r in rows:
            if r['scenario'] == scenario:
                groups.setdefault((r['sample'], r['kind']), set()).add((r['origin_y'], r['origin_x']))
        assert all(len(v) == 1 for v in groups.values()), 'Anger comparison changed origins.'
    def selected(scenario, kind, anger, sample=None):
        return [r for r in rows if r['scenario'] == scenario and r['kind'] == kind
                and int(r['anger']) == anger and (sample is None or int(r['sample']) == sample)]
    def cover(scenario, kind, anger, step):
        return [100 * int(r['ground_hit']) / int(r['ground']) for r in curves
                if r['scenario'] == scenario and r['kind'] == kind
                and int(r['anger']) == anger and int(r['step']) == step]
    def table(header, body):
        return '\n'.join(['| ' + ' | '.join(header) + ' |',
                           '| ' + ' | '.join(['---'] * len(header)) + ' |',
                           *['| ' + ' | '.join(map(str, r)) + ' |' for r in body]])
    chamber = [[int(selected('chamber', k, a, 50)[0]['t90']) for a in angers] for k in kinds]
    corridor = [[int(selected('corridor', k, a)[0]['d100']) for a in angers] for k in kinds]
    maps = selected('generated', 'water', 0)
    summary = ['# Morgoth’s Wrath: measured pacing', '',
               f'{len(rows)} production-controller runs, 200 completed actions each; 15 generated maps at 100/250/500/750/900 ft.',
               f'Generated dimensions: {min(int(r["height"]) for r in maps)}–{max(int(r["height"]) for r in maps)} tiles per side; '
               f'{min(int(r["ground"]) for r in maps):,}–{max(int(r["ground"]) for r in maps):,} original dry-ground tiles.', '',
               '## Method', '',
               '- 60 open-chamber runs (30×30, 50×50, 100×100), 20 narrow-corridor runs, 176 normal-source generated runs, and 100 matched-source generated runs.',
               '- Normal-source runs use normal eligible-source selection. Matched-source runs put each material on the same central floor of five real generated layouts; they isolate propagation from source-location differences.',
               '- Every material/anger run restores the same serialized map. Anger comparisons use identical origins and controller random seeds.',
               '- Coverage means cumulative original dry ground converted (or reached by cold): floors, doors, traps and bridges, excluding protected support, walls and already-existing hazards. It is not the percentage of the rectangular map including rock.',
               '- Actual catastrophe, terrain conversion, wall wear, bridge failure, thermal interaction, cold/contact/fall handlers are linked from the production build. Ambient geology and monster movement are not run. The observer remains on a protected stair.',
               '- An action is one completed player action, including immediate activation as action 1. Menus do not count. A recorded lost route means no dry path from the stationary start; this is not a human escape playtest.', '',
               '## Open 50×50 chamber: actions to affect 90% of dry ground', '',
               table(['Material', *[f'Anger {a}' for a in angers]], [[labels[i], *chamber[i]] for i in range(5)]), '',
               '## One-tile corridor: actions to advance 100 tiles', '',
               table(['Material', *[f'Anger {a}' for a in angers]], [[labels[i], *corridor[i]] for i in range(5)]), '',
               '## Generated layouts: median coverage after 40 actions, same source location', '',
               table(['Material', *[f'Anger {a}' for a in angers]],
                     [[labels[i], *[f'{statistics.median(cover("paired", k, a, 40)):.1f}%' for a in angers]] for i,k in enumerate(kinds)]), '',
               'Five maps per row/anger. Cold crosses intact doors and rock. Water follows passages; acid/lava/chasm also spend supply on freshly breached rock.', '',
               '## Normal eligible sources: median coverage after 40 actions', '',
               table(['Material', 'Maps', *[f'Anger {a}' for a in angers]],
                     [[labels[i], len(selected('generated', k, 0)),
                       *[f'{statistics.median(cover("generated", k, a, 40)):.1f}%' for a in angers]] for i,k in enumerate(kinds)]), '',
               'Natural ice-source evidence is only one map. The matched-source tests above provide a separate five-layout comparison.', '',
               '## Time to 90% on generated maps with normal sources', '']
    successful = []
    for i,k in enumerate(kinds):
        row = [labels[i]]
        for a in [0, 6]:
            r = selected('generated', k, a)
            times = [int(x['t90']) for x in r if int(x['t90']) >= 0]
            row.append(f'{min(times)}–{max(times)} actions ({len(times)}/{len(r)} maps)' if times else f'Not reached by 200 actions (0/{len(r)} maps)')
        successful.append(row)
    summary += [table(['Material', 'Anger 0', 'Anger 6'], successful), '',
                '## Interpretation', '',
                '- Source supply is fixed for each material and anger; there is no eight-action acceleration.',
                '- Water/acid travel 1 to 2 new layers per action (average 1 + anger/6), with each substep snapshot bounded. Lava, ice and chasm travel at most one layer, with transmission taking 1 to 2 actions depending on anger; solid cold transmission adds one action.',
                '- Water spreading and deepening share the same source supply. Connected older water deepens after at least 6/5/4/3 subsequent actions at anger 0/2/4/6, and may take longer in a broad basin.',
                '- Cold crosses intact doors, walls and bridges. Existing water takes additional cooling time; lava keeps its shared melting interaction. Cold exposure can reach a structure without replacing it with ice.',
                '- Wall and bridge contact occurs once per action. These runs exercise production propagation; they are not human escape playtests.', '',
                'Raw results: `summary.csv` and `curves.csv`. Reproduce with `python scripts/analyze_catastrophe_speed.py`; use `--report-only` to regenerate this report.']
    baseline_path = OUT / 'before-pacing/summary.csv'
    if baseline_path.exists():
        baseline = list(csv.DictReader(baseline_path.open(newline='')))
        comparison = []
        for label, kind in zip(labels, kinds):
            old = next(r for r in baseline if r['scenario']=='chamber' and r['sample']=='50'
                       and r['kind']==kind and r['anger']=='6')
            new = selected('chamber', kind, 6, 50)[0]
            comparison.append([label, old['t90'], new['t90']])
        summary += ['', '## Before / after: 50×50 chamber, anger 6, actions to 90%', '',
                    table(['Material', 'Before', 'After'], comparison)]
    summary += ['', '## Generated levels at anger 6: median dry-ground coverage', '',
                table(['Material', '40 actions', '80 actions', '120 actions', '200 actions'],
                      [[label, *[f'{statistics.median(cover("generated",kind,6,t)):.1f}%'
                                 for t in (40,80,120,200)]] for label,kind in zip(labels,kinds)]),
                '', 'Build/source hashes are recorded in `build.json`.']
    (OUT / 'report.md').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    sys.path.insert(0, str(OUT / 'plot-deps'))
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print('Report saved; matplotlib unavailable, skipping charts.')
        return
    colors = ['#8db9d7', '#4596c4', '#d67c3b', '#aa3036']
    plt.rcParams.update({'font.size': 11, 'axes.spines.top': False, 'axes.spines.right': False})
    fig, axs = plt.subplots(1, 2, figsize=(14, 5.5), layout='constrained')
    for j,a in enumerate(angers):
        x = np.arange(5) + (j - 1.5) * .19
        axs[0].bar(x, [v[j] for v in chamber], .18, color=colors[j], label=f'Anger {a}')
    axs[0].set_xticks(range(5), labels); axs[0].set_ylabel('Completed actions')
    axs[0].set_title('50×50 open chamber: time to 90% coverage')
    axs[0].legend(frameon=False, ncol=2); axs[0].set_ylim(0, max(max(v) for v in chamber)*1.15)
    axs[1].imshow(np.array(corridor), cmap='YlOrRd_r', vmin=0, vmax=max(max(v) for v in corridor), aspect='auto')
    axs[1].set_xticks(range(4), [f'Anger {a}' for a in angers]); axs[1].set_yticks(range(5), labels)
    for i in range(5):
        for j in range(4): axs[1].text(j, i, str(corridor[i][j]), ha='center', va='center', fontsize=17)
    axs[1].set_title('100 tiles down a narrow corridor: actions')
    fig.suptitle('Current catastrophe pacing — production simulations', fontsize=17)
    fig.savefig(OUT / 'pacing.png', dpi=170); plt.close(fig)
    fig, axs = plt.subplots(1, 2, figsize=(14, 5.5), sharey=True, layout='constrained')
    for ax,scenario,title in zip(axs, ['generated', 'paired'], ['Normal eligible sources', 'Same source on five generated layouts']):
        for j,a in enumerate(angers):
            medians = [statistics.median(cover(scenario, k, a, 40)) for k in kinds]
            ax.bar(np.arange(5) + (j - 1.5)*.19, medians, .18, color=colors[j], label=f'Anger {a}')
        ticks = [f'{labels[i]}\nn={len(selected(scenario,k,0))}' for i,k in enumerate(kinds)]
        ax.set_xticks(range(5), ticks); ax.set_title(title); ax.set_ylim(0, 105)
        ax.grid(axis='y', alpha=.2); ax.set_axisbelow(True)
    axs[0].set_ylabel('Median % of original dry ground affected')
    axs[0].legend(frameon=False, ncol=2)
    fig.suptitle('Generated maps: coverage after 40 actions', fontsize=17)
    fig.savefig(OUT / 'generated-coverage.png', dpi=170); plt.close(fig)
    print(f'Report and charts saved: {OUT}')


if __name__ == '__main__':
    report() if '--report-only' in sys.argv else main()
