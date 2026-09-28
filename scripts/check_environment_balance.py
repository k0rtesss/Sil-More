#!/usr/bin/env python3
"""Long-run environmental balance fixtures linked to the production engine."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_living_dungeon_save import ENGINE_FIXTURE, fixture_function, FRESH_MAP

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/environment-balance'

TESTS = r'''
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#t); exit(1); } } while(0)
static void clean(int speed, unsigned seed)
{
    fresh_map(); turn=1000; character_dungeon=true;
    op_ptr->environment_speed=speed;p_ptr->py=2;p_ptr->px=2;
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_ROOMY;level_partition_meta_set(&meta);
    cave_environment_reset();
    (void)seed;
}
static void seed_environment(unsigned seed)
{
    cave_environment_seed();
    environment_state state=cave_environment_get_state();state.random=seed;
    CHECK(cave_environment_restore_state(state));
}
static void tick(int count)
{ for(int n=0;n<count;n++){turn+=10;cave_environment_process();} }
static void defer_sources(void)
{
    for(int i=0;i<cave_environment_get_state().source_count;i++) {
        environment_source s=*cave_environment_source_at(i);s.next_turn=turn+100000;
        CHECK(cave_environment_restore_source(i,s));
    }
}
static void test_rubble(void)
{
    for(int speed=0;speed<=2;speed++) {
        clean(speed,17);
        for(int y=3;y<=16;y++) {
            cave_set_feat(y,10,FEAT_CHASM);cave_set_feat(y,9,FEAT_RUBBLE);
            cave_set_feat(y,6,FEAT_RUBBLE); /* Dry piles persist. */
        }
        cave_set_feat(17,9,FEAT_MORE);seed_environment(17);
        environment_source s=*cave_environment_source_at(0);s.used=s.capacity;
        CHECK(cave_environment_restore_source(0,s));
        /* This is deliberately an old, full reservoir, with non-natural
         * rubble. Neither old restriction may disable gravity. */
        tick(800);
        for(int y=3;y<=16;y++) {
            CHECK(cave_feat[y][9]==FEAT_FLOOR);
            CHECK(cave_environment_cell_at(y,9)->flags&ENV_DEPOSIT);
            CHECK(cave_feat[y][6]==FEAT_RUBBLE);
        }
        CHECK(cave_feat[17][9]==FEAT_MORE);
        tick(2000);for(int y=3;y<=16;y++)CHECK(cave_feat[y][9]==FEAT_FLOOR);
    }
    clean(1,19);cave_set_feat(10,10,FEAT_RUBBLE);cave_set_feat(10,11,FEAT_CHASM);
    cave_info[10][10]|=CAVE_G_VAULT;seed_environment(19);tick(800);
    CHECK(cave_feat[10][10]==FEAT_RUBBLE);
    puts("Rubble: exposed old piles clear with full supply/non-natural banks; dry/authored piles and settled ledges persist PASS.");
}
static void test_local_wear(void)
{
    for(unsigned seed=1;seed<=64;seed++) {
        clean(1,seed);
        for(int y=3;y<=16;y++) {
            cave_set_feat(y,10,FEAT_CHASM);cave_set_feat(y,9,FEAT_WALL_EXTRA);
        }
        seed_environment(seed);
        environment_source s=*cave_environment_source_at(0);s.next_turn=turn;s.phase=0;
        CHECK(cave_environment_restore_source(0,s));tick(5);
        int worn=0;for(int y=3;y<=16;y++)worn+=cave_environment_cell_at(y,9)->integrity<100;
        CHECK(worn<=1);
    }
    clean(1,11);cave_set_feat(10,10,FEAT_BRIDGE_CHASM_H);cave_set_feat(9,10,FEAT_CHASM);
    seed_environment(11);tick(5000);
    CHECK(cave_feat[10][10]==FEAT_BRIDGE_CHASM_H);
    CHECK(cave_environment_cell_at(10,10)->integrity==100);
    clean(1,13);cave_set_feat(10,10,FEAT_WATER);cave_set_feat(10,11,FEAT_WALL_EXTRA);
    seed_environment(13);tick(5000);
    CHECK(cave_feat[10][11]==FEAT_WALL_EXTRA);
    CHECK(cave_environment_cell_at(10,11)->integrity==100);
    puts("Weathering: at most one wall per source pulse, quiet chasm bridges and still-water granite remain sound PASS.");
}
static void test_supply(void)
{
    const int fluids[]={FEAT_WATER,FEAT_LAVA,FEAT_POISON};
    for(int speed=0;speed<=2;speed++)for(int kind=0;kind<3;kind++) {
        clean(speed,31);cave_set_feat(10,10,fluids[kind]);seed_environment(31);
        bool rose=false,fell=false;int last=0,late_changes=0;
        for(int n=0;n<6000;n++) {
            byte before[20][24];
            for(int y=0;y<20;y++)for(int x=0;x<24;x++)before[y][x]=cave_feat[y][x];
            tick(1);int changed=0,added=0,footprint=0;
            for(int y=1;y<19;y++)for(int x=1;x<23;x++) {
                changed+=cave_feat[y][x]!=before[y][x];
                const environment_cell* c=cave_environment_cell_at(y,x);
                added+=(c->flags&ENV_ADDED_LIQUID)!=0;
                footprint+=(c->flags&(ENV_DEPOSIT|ENV_ADDED_LIQUID))!=0;
            }
            const environment_source* s=cave_environment_source_at(0);
            CHECK(changed<=4&&s->used<=s->capacity);
            CHECK(added<=s->used);
            if(fluids[kind]==FEAT_LAVA)CHECK(footprint<=s->capacity);
            if(n>=5000)late_changes+=changed;
            if(added>last)rose=true;if(rose&&added<last)fell=true;last=added;
        }
        CHECK(rose&&fell);
        CHECK(late_changes>=4); /* Sources must not go silent after exploration. */
        if(fluids[kind]==FEAT_LAVA) {
            CHECK(cave_environment_source_at(0)->used==cave_environment_source_at(0)->capacity);
        }
    }
    /* Cancelling a newly warned flood by an external terrain edit refunds
     * the supply immediately, instead of silently exhausting the source. */
    clean(1,41);cave_set_feat(10,10,FEAT_WATER);seed_environment(41);
    environment_source s=*cave_environment_source_at(0);s.next_turn=turn;s.phase=0;
    CHECK(cave_environment_restore_source(0,s));tick(5);
    int found=0;
    for(int y=1;y<19;y++)for(int x=1;x<23;x++)if(cave_environment_pending_hazard(y,x)) {
        found++;cave_set_feat(y,x,FEAT_WALL_EXTRA);
    }
    CHECK(found==1&&cave_environment_source_at(0)->used==0);
    puts("Supply: all liquids remain active after 5,000 actions; lava reuses a finite footprint; cancelled new flooding refunds supply PASS.");
}
static void test_shared_limit(void)
{
    clean(2,47);
    for(int y=4;y<=13;y+=3)for(int x=4;x<=19;x+=3)cave_set_feat(y,x,FEAT_WATER);
    seed_environment(47);CHECK(cave_environment_get_state().source_count==24);
    for(int i=0;i<24;i++) {
        environment_source s=*cave_environment_source_at(i);s.used=1;s.phase=6;s.next_turn=turn;
        CHECK(cave_environment_restore_source(i,s));
        environment_cell c=*cave_environment_cell_at(s.y,s.x);
        c.flags|=ENV_ADDED_LIQUID;c.base_feat=FEAT_FLOOR;
        CHECK(cave_environment_restore_cell(s.y,s.x,c));
    }
    tick(3);int dry=0;
    for(int y=4;y<=13;y+=3)for(int x=4;x<=19;x+=3)dry+=cave_feat[y][x]==FEAT_FLOOR;
    CHECK(dry==4);
    puts("Shared limit: 24 simultaneous recession sources commit only four changes in one world update PASS.");
}
static void test_lava_fringe_recovery(void)
{
    clean(1,49);cave_set_feat(10,10,FEAT_LAVA);seed_environment(49);
    environment_source s=*cave_environment_source_at(0);s.used=2;s.phase=0;s.next_turn=turn;
    CHECK(cave_environment_restore_source(0,s));
    for(int x=11;x<=12;x++) {
        environment_cell c=*cave_environment_cell_at(10,x);
        c.flags|=ENV_DEPOSIT;c.base_feat=FEAT_FLOOR;c.owner=0;
        CHECK(cave_environment_restore_cell(10,x,c));
    }
    tick(5);
    CHECK(cave_environment_cell_at(10,11)->owner==1);
    CHECK(cave_environment_cell_at(10,12)->owner==1);
    CHECK(cave_environment_pending_hazard(10,11)==FEAT_LAVA);
    /* A cancelled reheat retains the reserved footprint; it must not grant
     * another unit for paving elsewhere. A fresh cancelled flood is tested
     * separately and does refund its newly reserved unit. */
    cave_set_feat(10,11,FEAT_WALL_EXTRA);
    CHECK(cave_environment_source_at(0)->used==2);
    CHECK(cave_environment_cell_at(10,11)->owner==1);
    CHECK(!cave_environment_pending_hazard(10,11));
    puts("Saved lava fringe: formerly ownerless cooled cells resume within their spent footprint; cancelling a reheat cannot expand it PASS.");
}
static void test_heat_feedback(void)
{
    clean(1,51);seed_environment(51);
    environment_cell invalid=*cave_environment_cell_at(10,10);
    invalid.due=turn+100001;
    CHECK(!cave_environment_restore_cell(10,10,invalid));
    clean(1,53);
    for(int y=8;y<=12;y++)for(int x=8;x<=12;x++)cave_set_feat(y,x,FEAT_ICE);
    cave_set_feat(10,10,FEAT_WATER);seed_environment(53);defer_sources();tick(100);
    CHECK(cave_feat[10][10]==FEAT_ICE);
    CHECK(cave_environment_cell_at(10,10)->base_feat==FEAT_WATER);
    for(int y=8;y<=12;y++)for(int x=8;x<=12;x++)if(y!=10||x!=10)cave_set_feat(y,x,FEAT_FLOOR);
    tick(200);CHECK(cave_feat[10][10]==FEAT_WATER);
    clean(1,59);cave_set_feat(10,10,FEAT_LAVA);cave_set_feat(10,11,FEAT_ICE);
    seed_environment(59);defer_sources();
    bool melting=false,water=false,cooled=false;
    for(int n=0;n<200;n++) {
        tick(1);
        if(n<5)CHECK(cave_feat[10][11]==FEAT_ICE&&cave_feat[10][10]==FEAT_LAVA);
        if(cave_feat[10][11]==FEAT_MELTING_ICE)melting=true;
        if(cave_feat[10][11]==FEAT_WATER){CHECK(melting);water=true;}
        if(cave_feat[10][10]==FEAT_FLOOR){CHECK(water);cooled=true;}
    }
    CHECK(melting&&water&&cooled);
    clean(1,61);cave_set_feat(10,10,FEAT_LAVA);cave_set_feat(10,11,FEAT_ICE);
    seed_environment(61);defer_sources();p_ptr->py=10;p_ptr->px=11;cave_m_idx[10][11]=-1;
    tick(200);CHECK(cave_feat[10][11]==FEAT_ICE&&cave_feat[10][10]==FEAT_LAVA);
    puts("Thermal: new ice loses cold without an external source; normal-level contact melts in stages before quenching; occupants survive PASS.");
}
static void test_dry_minerals(void)
{
    clean(2,67);
    partition_meta_save meta={0};meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=QUAD_MODE_BIG_CAVE;level_partition_meta_set(&meta);
    for(int y=4;y<=14;y++)for(int x=4;x<=18;x++) {
        cave_set_feat(y,x,(y+x)%2?FEAT_QUARTZ:FEAT_WALL_EXTRA);cave_natural[y][x]=1;
    }
    seed_environment(67);tick(10000);CHECK(cave_environment_get_state().mineral_budget==8);
    puts("Minerals: dry neighboring quartz does not transmute a cave's granite PASS.");
    clean(2,71);level_partition_meta_set(&meta);
    for(int y=4;y<=13;y+=3)for(int x=4;x<=18;x+=3) {
        cave_set_feat(y,x,FEAT_WALL_EXTRA);cave_natural[y][x]=1;
        cave_set_feat(y-1,x,FEAT_QUARTZ);cave_natural[y-1][x]=1;
        cave_set_feat(y+1,x,FEAT_WATER);
    }
    seed_environment(71);defer_sources();tick(10000);
    CHECK(cave_environment_get_state().mineral_budget==0);
    int deposits=0;
    for(int y=4;y<=13;y+=3)for(int x=4;x<=18;x+=3) {
        if(cave_feat[y][x]==FEAT_QUARTZ) {
            deposits++;CHECK(cave_environment_cell_at(y,x)->flags&ENV_MINERAL_SPENT);
        }
    }
    CHECK(deposits==8);
    puts("Minerals: wet natural veins still grow, at most eight fresh deposits per level PASS.");
}
static void metrics(void)
{
    puts("scenario,speed,actions,mean_rubble,mean_damaged,mean_changed_bank,mean_added");
    for(int speed=0;speed<=2;speed++)for(int elapsed=1000;elapsed<=5000;elapsed+=4000) {
        int rubble=0,damaged=0,changed=0,added=0;
        for(unsigned seed=1;seed<=24;seed++) {
            clean(speed,seed);
            for(int y=3;y<=16;y++) {
                cave_set_feat(y,10,FEAT_CHASM);
                cave_set_feat(y,9,FEAT_WALL_EXTRA);
                cave_natural[y][9]=cave_natural[y][11]=1;
            }
            seed_environment(seed);tick(elapsed);
            for(int y=3;y<=16;y++) {
                rubble+=cave_feat[y][9]==FEAT_RUBBLE;
                damaged+=cave_feat[y][9]==FEAT_DAMAGED_WALL;
                changed+=cave_feat[y][9]!=FEAT_WALL_EXTRA;
            }
            added+=cave_environment_source_at(0)->used;
        }
        printf("chasm_bank,%d,%d,%.2f,%.2f,%.2f,%.2f\n",speed,elapsed,
            rubble/24.0,damaged/24.0,changed/24.0,added/24.0);
    }
}
/* Count activity and persistence separately. A small snapshot population can
 * mean either healthy turnover or a system that never does anything. */
static void activity_metrics(void)
{
    const int features[]={FEAT_CHASM,FEAT_WATER,FEAT_LAVA,FEAT_POISON};
    const char* names[]={"chasm","water","lava","acid"};
    puts("activity,window,mean_local_changes,mean_cracks,mean_collapses,mean_clears,mean_advances,mean_recedes,mean_peak_rubble,mean_peak_liquid,mean_rubble_lifetime");
    for(int kind=0;kind<4;kind++)for(int window=0;window<2;window++) {
        double total[8]={0};int total_lifetime=0,lifetimes=0;
        for(unsigned seed=1;seed<=24;seed++) {
            clean(1,seed);turn=1000+seed*100;
            if(kind==0)for(int y=3;y<=16;y++) {
                cave_set_feat(y,10,FEAT_CHASM);cave_set_feat(y,9,FEAT_WALL_EXTRA);
                cave_natural[y][9]=cave_natural[y][11]=1;
            }
            else for(int y=9;y<=11;y++)for(int x=9;x<=11;x++)cave_set_feat(y,x,features[kind]);
            seed_environment(seed);if(window)tick(1000);
            int counts[8]={0},born[20][24]={{0}};
            for(int n=0;n<200;n++) {
                byte old[20][24];for(int y=1;y<19;y++)for(int x=1;x<23;x++)old[y][x]=cave_feat[y][x];
                tick(1);int rubble=0,added=0;
                for(int y=1;y<19;y++)for(int x=1;x<23;x++) {
                    int f=cave_feat[y][x];const environment_cell* c=cave_environment_cell_at(y,x);
                    rubble+=f==FEAT_RUBBLE;
                    added+=(c->flags&ENV_ADDED_LIQUID)!=0 && f==features[kind];
                    if(f==old[y][x])continue;
                    if(distance(10,12,y,x)<=6 && los(10,12,y,x))counts[0]++;
                    counts[1]+=f==FEAT_DAMAGED_WALL||f==FEAT_CRACKED_QUARTZ;
                    counts[2]+=f==FEAT_RUBBLE;
                    counts[3]+=old[y][x]==FEAT_RUBBLE&&f==FEAT_FLOOR;
                    if(f==FEAT_RUBBLE)born[y][x]=n+1;
                    if(old[y][x]==FEAT_RUBBLE&&f==FEAT_FLOOR&&born[y][x]) {
                        int lifetime=n+1-born[y][x];
                        CHECK(lifetime<=60);
                        total_lifetime+=lifetime;lifetimes++;
                    }
                    counts[4]+=f==features[kind];
                    counts[5]+=old[y][x]==features[kind]&&f!=features[kind];
                }
                counts[6]=MAX(counts[6],rubble);counts[7]=MAX(counts[7],added);
            }
            CHECK(counts[6]<=2);
            if(kind==0&&!window)CHECK(counts[1]>=1&&counts[2]>=1&&counts[3]>=1);
            if(kind>0)CHECK(counts[0]>=1);
            for(int i=0;i<8;i++)total[i]+=counts[i]/24.0;
        }
        printf("%s,%s",names[kind],window?"1000-1200":"0-200");
        for(int i=0;i<8;i++)printf(",%.2f",total[i]);
        printf(",%.2f\n",lifetimes?(double)total_lifetime/lifetimes:0.0);
    }
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    for header in ('cave/cave-environment.h', 'cave/cave-water-flow.h', 'cave/cave-flood.h',
                   'cave/cave-events.h', 'cave/cave-fixtures.h',
                   'level-generation/level-generation-terrain-access.h'):
        prefix += f'\n#include "{header}"\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + fixture_function('terminal_extra') + '\n' +
                      fixture_function('reset_map') + '\n' + FRESH_MAP + '\n' + TESTS + '\n' + init +
                      '    test_rubble();test_local_wear();test_supply();test_shared_limit();test_lava_fringe_recovery();\n'
                      '    test_heat_feedback();test_dry_minerals();metrics();activity_metrics();\n'
                      '    printf("Balance checks: %d PASS.\\n",checks);\n'
                      '    SDL_Quit();return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    objects = [p for p in objects if not p.endswith('/src/main.c.obj')]
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / name) for name in
                                   ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
                                  'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O1',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='data-', dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT / 'lib/edit'), data], cwd=data,
                                env=env, capture_output=True, text=True, timeout=180)
        (OUT / 'validation.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()

if __name__ == '__main__':
    main()
