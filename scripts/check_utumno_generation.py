#!/usr/bin/env python3
"""Exercise the optional route against linked production generation and data.

Requires a completed build-incremental.ps1. All runtime files stay in a
temporary directory; exported maps are written to scripts/output/utumno.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/utumno"

HARNESS = r'''
#include "angband.h"
#include "init/init2-internal.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-themes.h"
#include "level-generation/level-generation-terrain.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include <assert.h>
#include <stdio.h>
static term test_term;
extern void dungeon(void);
static char private_config[1024];
cptr __wrap_get_sdl_config_path(void) { return private_config; }
static errr dummy_xtra(int n,int v) { (void)v;if(n==TERM_XTRA_EVENT)Term_keypress(' ');return 0; }
static int attempts;
static bool small_maps, extra_stairs;
static int partition_passes, tunnel_passes, terrain_passes, population_passes;
static int first_size, seen_densities, seen_partition_counts;
static unsigned long long partition_signatures[8];
static int signature_count;

/* Observe the production pipeline: a bespoke Utumno layout must not pass. */
void __real_apply_quadrant_generation_modes(void);
void __wrap_apply_quadrant_generation_modes(void) {
    if (!partition_passes) first_size=p_ptr->cur_map_hgt;
    partition_passes++;
    __real_apply_quadrant_generation_modes();
}
bool __real_connect_rooms_stairs(void);
bool __wrap_connect_rooms_stairs(void) {
    tunnel_passes++; return __real_connect_rooms_stairs();
}
void __real_place_dungeon_terrain(void);
void __wrap_place_dungeon_terrain(void) {
    terrain_passes++; __real_place_dungeon_terrain();
}
int __real_run_partition_monster_pass(const partition_population_plan*,int);
int __wrap_run_partition_monster_pass(const partition_population_plan* plans,int n) {
    population_passes++; return __real_run_partition_monster_pass(plans,n);
}
static bool startup_probe;
void __real_process_player(void);
void __wrap_process_player(void) {
    if(!startup_probe) {__real_process_player();return;}
    assert(p_ptr->morgoth_hall_entered&&min_depth()==MORGOTH_DEPTH);
    p_ptr->leaving=true;p_ptr->energy=0;
}
void __real_level_gen_screen_start_attempt(void);
void __wrap_level_gen_screen_start_attempt(void) {
    assert(++attempts <= 30);
    __real_level_gen_screen_start_attempt();
}
static void initialize(const char *root,const char *tmp) {
    char path[1024];
    #define ASSET(field,name) do { snprintf(path,sizeof(path),"%s/lib/%s",root,name);field=SDL_strdup(path); }while(0)
    ASSET(ANGBAND_DIR,"");ASSET(ANGBAND_DIR_EDIT,"edit");
    ASSET(ANGBAND_DIR_FILE,"file");ASSET(ANGBAND_DIR_HELP,"help");
    ASSET(ANGBAND_DIR_INFO,"info");ASSET(ANGBAND_DIR_PREF,"pref");
    ASSET(ANGBAND_DIR_XTRA,"xtra");ASSET(ANGBAND_DIR_SCRIPT,"script");
    #define PRIVATE(field,name) do { snprintf(path,sizeof(path),"%s/%s",tmp,name);assert(SDL_CreateDirectory(path));field=SDL_strdup(path); }while(0)
    PRIVATE(ANGBAND_DIR_DATA,"data");PRIVATE(ANGBAND_DIR_APEX,"meta");
    PRIVATE(ANGBAND_DIR_METARUN,"meta/metaruns");PRIVATE(ANGBAND_DIR_SAVE,"save");
    PRIVATE(ANGBAND_DIR_USER,"user");PRIVATE(ANGBAND_DIR_BONE,"bone");
    snprintf(private_config,sizeof(private_config),"%s/user/sil_sdl.json",tmp);
    #define INIT(fn) assert(fn()==0)
    INIT(init_z_info);INIT(init_rt_info);INIT(init_f_info);INIT(init_k_info);
    INIT(init_b_info);INIT(init_a_info);ensure_artifact_guids();ensure_artifact_spawn_numbers();
    INIT(init_e_info);INIT(init_r_info);INIT(init_v_info);INIT(init_h_info);
    INIT(init_st_info);INIT(init_style_info);
    style_info=(style_type*)style_head.info_ptr;style_name=style_head.name_ptr;
    INIT(init_partition_info);assert(terrain_themes_load());
    INIT(init_cu_info);INIT(init_mb_info);INIT(init_p_info);INIT(init_c_info);
    INIT(init_flavor_info);INIT(init_effect_info);INIT(init_skeleton_note_info);
    INIT(init_quest_info);INIT(init_oath_info);INIT(init_other);INIT(init_alloc);
    INIT(init_n_info);build_randart_tables();
    flavor_init();drop_system_init();
}
static bool safe_tile(int y,int x) {
    if(!in_bounds_fully(y,x))return false;
    int f=cave_feat[y][x];
    if(f==FEAT_CHASM||f==FEAT_LAVA||f==FEAT_POISON||f==FEAT_RUBBLE)return false;
    if(f==FEAT_SECRET||(f>=FEAT_DOOR_HEAD&&f<=FEAT_DOOR_TAIL))return true;
    return f<FEAT_WALL_HEAD||f>FEAT_WALL_TAIL;
}
static byte accessible[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static void walking_flood(void) {
    static int queue[MAX_DUNGEON_HGT*MAX_DUNGEON_WID];
    int head=0,tail=0;
    memset(accessible,0,sizeof(accessible));
    assert(safe_tile(p_ptr->py,p_ptr->px));
    accessible[p_ptr->py][p_ptr->px]=1;
    queue[tail++]=p_ptr->py*MAX_DUNGEON_WID+p_ptr->px;
    while(head<tail) {
        int idx=queue[head++],y=idx/MAX_DUNGEON_WID,x=idx%MAX_DUNGEON_WID;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
            int ny=y+dy,nx=x+dx;
            if(!safe_tile(ny,nx)||accessible[ny][nx])continue;
            accessible[ny][nx]=1;queue[tail++]=ny*MAX_DUNGEON_WID+nx;
        }
    }
}
static bool dry_vault_tile(int y,int x,int vault) {
    if(!safe_tile(y,x)||terrain_vault_id_at(y,x)!=vault)return false;
    int f=cave_feat[y][x];
    return f!=FEAT_WATER&&f!=FEAT_DEEP_WATER&&!FEAT_IS_ICE(f)&&f!=FEAT_SECRET;
}
static void check_workshop_access(int depth) {
    if(depth!=22&&depth!=23)return;
    int vault=depth==22?523:524,sy=-1,sx=-1;
    if(depth==23){sy=p_ptr->py;sx=p_ptr->px;}
    else for(int y=1;y<p_ptr->cur_map_hgt-1&&sy<0;y++)
        for(int x=1;x<p_ptr->cur_map_wid-1;x++)
            if(dry_vault_tile(y,x,vault)&&terrain_vault_symbol_at(y,x)=='.')
                {sy=y;sx=x;break;}
    assert(sy>=0&&dry_vault_tile(sy,sx,vault));
    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT*MAX_DUNGEON_WID];
    const int dy[4]={-1,1,0,0},dx[4]={0,0,-1,1};
    int head=0,tail=0,rewards=0,stairs=0;
    memset(reached,0,sizeof(reached));reached[sy][sx]=1;
    queue[tail++]=sy*MAX_DUNGEON_WID+sx;
    while(head<tail) {
        int at=queue[head++],y=at/MAX_DUNGEON_WID,x=at%MAX_DUNGEON_WID;
        for(int dir=0;dir<4;dir++) {
            int ny=y+dy[dir],nx=x+dx[dir];
            if(!dry_vault_tile(ny,nx,vault)||reached[ny][nx])continue;
            reached[ny][nx]=1;queue[tail++]=ny*MAX_DUNGEON_WID+nx;
        }
    }
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++) {
        if(terrain_vault_id_at(y,x)!=vault)continue;
        char token=terrain_vault_symbol_at(y,x);
        /* Ordinary level generation may scatter rubble onto empty floors.
         * All fixed destinations must retain a dry route without digging,
         * secret-door discovery or even a diagonal squeeze. */
        bool needs_access=strchr("~0uQ<>",token)!=NULL;
        if(needs_access&&!reached[y][x]) {
            printf("Unreachable vault%d token%c at%d,%d feature%d from%d,%d\n",
                vault,token,y,x,cave_feat[y][x],sy,sx);
            assert(reached[y][x]);
        }
        if(token=='~'){assert(cave_o_idx[y][x]);rewards++;}
        if(token=='<'||token=='>')stairs++;
        int mi=cave_m_idx[y][x];
        if(depth==23&&mi>0&&mon_list[mi].r_idx==411)
            assert(!los(p_ptr->py,p_ptr->px,y,x));
    }
    assert(rewards==(depth==22?2:3)&&stairs==1);
    if(depth==23)assert(cave_feat[p_ptr->py][p_ptr->px]==FEAT_FLOOR);
    printf("PASS depth%d dry cardinal routes to guardian, rewards and stair%s\n",
        depth,depth==23?"; sheltered forge arrival":"");
}
static void export_map(const char *out,int depth) {
    char path[1024];snprintf(path,sizeof(path),"%s/level-%d.txt",out,depth);
    FILE *f=fopen(path,"w");assert(f);
    for(int y=0;y<p_ptr->cur_map_hgt;y++) {
        for(int x=0;x<p_ptr->cur_map_wid;x++) {
            int feat=cave_feat[y][x],mi=cave_m_idx[y][x];
            char c=safe_tile(y,x)?'.':'#';
            if(feat==FEAT_WATER||feat==FEAT_DEEP_WATER)c='~';
            if(FEAT_IS_ICE(feat))c='i';if(feat==FEAT_LAVA)c='L';
            if(feat==FEAT_MORE)c='>';if(feat==FEAT_MORE_SHAFT)c='v';
            if(feat==FEAT_LESS)c='<';if(cave_forge_bold(y,x))c='0';
            if(feat>=FEAT_DOOR_HEAD&&feat<=FEAT_DOOR_TAIL)c='+';
            if(cave_o_idx[y][x])c='*';
            if(mi>0)c=mon_list[mi].r_idx==410?'O':mon_list[mi].r_idx==411?'N':mon_list[mi].r_idx==404?'H':'m';
            if(y==p_ptr->py&&x==p_ptr->px)c='@';fputc(c,f);
        }
        fputc('\n',f);
    }
    fclose(f);
}
static int check(int depth,int seed,bool visited,bool enabled,const char *out) {
    wipe_o_list();wipe_mon_list();player_wipe();
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->depth=depth;p_ptr->max_depth=depth;p_ptr->fixed_forge_count=3;
    p_ptr->utumno_forge_visited=visited;p_ptr->utumno_return_to_throne=visited&&depth==20;
    op_ptr->opt[OPT_utumno_corridors]=enabled;
    op_ptr->opt[OPT_smaller_level_size]=small_maps;
    op_ptr->opt[OPT_more_stairs]=extra_stairs;
    op_ptr->vault_drop_frequency=seed==2?VDF_MEAGER:VDF_NORMAL;
    playerturn=1;character_generated=true;Rand_state_init(seed);attempts=0;
    partition_passes=tunnel_passes=terrain_passes=population_passes=first_size=0;
    printf("Generate depth%d seed%d visited%d enabled%d small%d extra-stairs%d\n",depth,seed,visited,enabled,small_maps,extra_stairs);
    generate_cave();walking_flood();check_workshop_access(depth);
    int down=0,up=0,shafts=0,forges=0,mighty=0,water=0,ice=0,lava=0,ondotur=0,nambatur=0,helcamo=0,chests=0;
    bool partitions[PARTITION_META_MAX]={0};
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++) {
        int f=cave_feat[y][x];
        if(f==FEAT_MORE){
            down++;
            if(!accessible[y][x]) {
                export_map(out,depth);
                printf("Unreachable stair%d,%d from arrival%d,%d\n",y,x,p_ptr->py,p_ptr->px);
            }
            assert(accessible[y][x]);
        }
        if(f==FEAT_LESS||f==FEAT_LESS_SHAFT)up++;
        if(f==FEAT_MORE_SHAFT) {
            shafts++;int pi=level_partition_index_for_point(y,x);
            assert(pi>=0&&!partitions[pi]);partitions[pi]=true;
            assert(!(cave_info[y][x]&CAVE_G_VAULT));
        }
        if(cave_forge_bold(y,x)) {forges++;if(depth==23){assert(f==FEAT_FORGE_UNIQUE_TAIL);assert(accessible[y][x]);}}
        mighty+=(f>=FEAT_FORGE_UNIQUE_HEAD&&f<=FEAT_FORGE_UNIQUE_TAIL);
        water+=(f==FEAT_WATER||f==FEAT_DEEP_WATER);ice+=FEAT_IS_ICE(f);lava+=(f==FEAT_LAVA);
        if(depth==22)assert(f!=FEAT_CHASM&&f!=FEAT_POISON);
        if(depth==23&&f==FEAT_LESS)assert(!(cave_info[y][x]&CAVE_G_VAULT));
    }
    for(int i=1;i<mon_max;i++) {
        int r=mon_list[i].r_idx;ondotur+=r==410;nambatur+=r==411;helcamo+=r==404;
    }
    for(int i=1;i<o_max;i++)if(o_list[i].tval==TV_CHEST) {chests++;if(depth==23)assert(accessible[o_list[i].iy][o_list[i].ix]);}
    if(depth==22) {
        assert(down==1&&up==0&&shafts==0&&mighty==0&&water>0&&ice>0&&lava>0);
        assert(ondotur==1&&helcamo==1&&nambatur==0);
        assert(partition_passes>=1&&tunnel_passes>=1&&terrain_passes>=1&&population_passes>=1);
        assert(p_ptr->cur_map_hgt==p_ptr->cur_map_wid);
        assert(p_ptr->cur_map_hgt>=(small_maps?13:16)*PANEL_HGT);
        assert(p_ptr->cur_map_hgt<=(small_maps?18:21)*PANEL_HGT);
        seen_partition_counts|=1<<current_partition_count;
        unsigned long long signature=1469598103934665603ULL;
        for(int i=0;i<current_partition_count;i++) {
            assert(current_partition_modes[i]!=QUAD_MODE_CHASM&&current_partition_modes[i]!=QUAD_MODE_LABYRINTH);
            assert(current_partition_big_cave_types[i]!=BIG_CAVE_POIS);
            seen_densities|=1<<current_partition_densities[i];
            signature=(signature^(unsigned)current_partition_modes[i])*1099511628211ULL;
        }
        if(signature_count<8)partition_signatures[signature_count++]=signature;
    }
    if(depth==23)assert(down==0&&up==1&&shafts==0&&forges==1&&nambatur==1&&chests==3&&p_ptr->utumno_forge_visited);
    if(depth==20) {
        assert(shafts==(enabled?6:0));assert(down==(visited?1:0));
        if(visited)assert(cave_feat[p_ptr->py][p_ptr->px]==FEAT_MORE&&!p_ptr->utumno_return_to_throne);
    }
    printf("PASS depth%d %dx%d partitions%d attempts%d stairs%d/%d/%d water%d ice%d lava%d forge%d chest%d monsters%d\n",depth,p_ptr->cur_map_hgt,p_ptr->cur_map_wid,current_partition_count,attempts,down,up,shafts,water,ice,lava,forges,chests,mon_cnt);
    if(seed==1&&!small_maps)export_map(out,depth);
    return first_size;
}
static void go_to_feature(int feature) {
    for(int y=1;y<p_ptr->cur_map_hgt-1;y++)for(int x=1;x<p_ptr->cur_map_wid-1;x++)
        if(cave_feat[y][x]==feature) {
            assert(!cave_m_idx[y][x]||cave_m_idx[y][x]==-1);
            cave_m_idx[p_ptr->py][p_ptr->px]=0;player_place(y,x);return;
        }
    assert(!"required travel feature missing");
}
static void finish_transition(void) {
    assert(p_ptr->leaving&&!p_ptr->create_stair);
    wipe_o_list();wipe_mon_list();attempts=0;generate_cave();
    p_ptr->leaving=false;
}
static void check_route_chain(const char *out) {
    check(20,6,false,true,out);
    min_depth_counter=0;assert(min_depth()<20);
    go_to_feature(FEAT_MORE_SHAFT);do_cmd_go_down();
    assert(p_ptr->depth==22&&!p_ptr->utumno_forge_visited);
    finish_transition();assert(r_info[410].cur_num==1&&r_info[404].cur_num==1);
    go_to_feature(FEAT_MORE);do_cmd_go_down();
    assert(p_ptr->depth==23&&!p_ptr->utumno_forge_visited);
    finish_transition();assert(p_ptr->utumno_forge_visited);
    assert(r_info[410].cur_num==0&&r_info[404].cur_num==0&&r_info[411].cur_num==1);
    go_to_feature(FEAT_LESS);do_cmd_go_up();
    assert(p_ptr->depth==20&&p_ptr->utumno_return_to_throne);
    finish_transition();
    assert(p_ptr->utumno_forge_visited&&!p_ptr->utumno_return_to_throne);
    assert(r_info[411].cur_num==0&&r_info[R_IDX_MORGOTH].cur_num==1);
    assert(cave_feat[p_ptr->py][p_ptr->px]==FEAT_MORE);
    assert(cave_info[p_ptr->py][p_ptr->px]&CAVE_G_VAULT);
    assert(p_ptr->stairs_taken==3);
    /* Run the real dungeon arrival processing and stop at its first player
     * action, after it has applied the throne-room entry/min-depth contract. */
    for(int i=1;i<mon_max;i++)mon_list[i].energy=0;
    p_ptr->energy=100;p_ptr->max_depth=23;p_ptr->restoring=true;
    startup_probe=true;dungeon();startup_probe=false;p_ptr->restoring=false;
    assert(p_ptr->morgoth_hall_entered&&min_depth()==20);
    p_ptr->leaving=false;do_cmd_go_down();
    assert(p_ptr->depth==20&&!p_ptr->leaving&&p_ptr->stairs_taken==3);
    puts("PASS actual20->22->23->20 command/generation chain, unique lifecycle, private arrival, hall lock, sealed reentry");
}
static void check_forge_reservation(void) {
    p_ptr->depth=20;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=66;
    cave_info[5][5]=0;Rand_state_init(98765);utumno_corridors=true;
    for(int i=0;i<1000;i++) {
        p_ptr->unique_forge_made=false;place_forge(5,5);
        assert(cave_feat[5][5]<FEAT_FORGE_UNIQUE_HEAD||cave_feat[5][5]>FEAT_FORGE_UNIQUE_TAIL);
    }
    utumno_corridors=false;bool found=false;Rand_state_init(98765);
    for(int i=0;i<1000&&!found;i++) {
        p_ptr->unique_forge_made=false;place_forge(5,5);
        found=cave_feat[5][5]>=FEAT_FORGE_UNIQUE_HEAD&&cave_feat[5][5]<=FEAT_FORGE_UNIQUE_TAIL;
    }
    assert(found);
    puts("PASS mighty forge reserved outside23 when enabled; original random forge remains possible when disabled");
}
static void check_depth22_population(void) {
    bool grew=false;
    const quadrant_mode_t modes[]={QUAD_MODE_ROOMY,QUAD_MODE_CAVEY,QUAD_MODE_RUINED,QUAD_MODE_BIG_CAVE};
    for(unsigned m=0;m<N_ELEMENTS(modes);m++) {
        const partition_rule_config *cfg=partition_config_get(partition_kind_from_mode(modes[m]));
        assert(cfg);
        for(int floors=100;floors<=3000;floors+=29) {
            int at20=partition_depth_bonus_monsters(modes[m],floors,20);
            int at22=partition_depth_bonus_monsters(modes[m],floors,22);
            assert(at22>=at20);
            assert(at22<=MAX(1,floors/MAX(1,cfg->depth_monsters.hard_cap_divisor)));
            if(at22>at20)grew=true;
        }
    }
    assert(grew);
    puts("PASS depth22 monster scaling continues beyond20 and retains configured floor-density caps");
}
int main(int argc,char **argv) {
    assert(argc==4);setbuf(stdout,NULL);log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));assert(term_init(&test_term,80,24,256)==0);
    test_term.xtra_hook=dummy_xtra;angband_term[0]=&test_term;Term_activate(&test_term);
    initialize(argv[1],argv[2]);
    int sizes22[4], normal_first[4];
    for(int small=0;small<2;small++) {
        small_maps=small;
        for(int seed=1;seed<=4;seed++) {
            extra_stairs=(seed%2==0);
            int size=check(22,seed,false,true,argv[3]);
            if(!small)normal_first[seed-1]=sizes22[seed-1]=size;
            else assert(size==normal_first[seed-1]-3*PANEL_HGT);
        }
    }
    assert((seen_densities&(1<<DENSITY_SPARSE))&&(seen_densities&(1<<DENSITY_NORMAL))&&(seen_densities&(1<<DENSITY_DENSE)));
    assert(seen_partition_counts&&(seen_partition_counts&(seen_partition_counts-1)));
    bool varied=false;
    for(int i=1;i<signature_count;i++)if(partition_signatures[i]!=partition_signatures[0])varied=true;
    assert(varied);
    small_maps=extra_stairs=false;
    check(23,1,false,true,argv[3]);check(23,2,false,false,argv[3]);
    check(20,1,false,true,argv[3]);check(20,2,true,true,argv[3]);
    check(20,3,true,false,argv[3]);check(20,4,false,false,argv[3]);
    bool deeper_size=false;
    for(int seed=1;seed<=4;seed++) {
        int size20=check(20,seed,false,true,argv[3]);
        assert(sizes22[seed-1]>=size20);
        if(sizes22[seed-1]>size20)deeper_size=true;
    }
    assert(deeper_size);
    puts("PASS shared procedural passes, varying partitions/densities, smaller-level setting, depth22 size scaling beyond20");
    check_route_chain(argv[3]);check_forge_reservation();check_depth22_population();
    puts("Utumno production generation checks PASS");SDL_Quit();return 0;
}
'''

def check_workshop_templates():
    """The authored plans must connect before procedural rubble is scattered."""
    from collections import deque

    records = {}
    for line in (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("N:"):
            serial = int(line.split(":")[1])
            records[serial] = []
        elif line.startswith("D:"):
            records[serial].append(line[2:])
    for serial in (523, 524):
        rows = records[serial]
        assert rows and all(len(row) == len(rows[0]) for row in rows)
        dry = {(y, x) for y, row in enumerate(rows) for x, token in enumerate(row)
               if token in ".+~0uQ<>"}
        start = (2, len(rows[0]) // 2) if serial == 524 else min(dry)
        assert start in dry
        reached = {start}
        queue = deque([start])
        while queue:
            y, x = queue.popleft()
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                cell = (y + dy, x + dx)
                if cell in dry and cell not in reached:
                    reached.add(cell)
                    queue.append(cell)
        assert reached == dry, (serial, sorted(dry - reached))
        print(f"PASS vault{serial} all authored work areas connect by dry cardinal paths", flush=True)


def main():
    check_workshop_templates()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_sdl_config_path",
        "-Wl,--wrap=level_gen_screen_start_attempt", "-Wl,--wrap=process_player",
        "-Wl,--wrap=apply_quadrant_generation_modes", "-Wl,--wrap=connect_rooms_stairs",
        "-Wl,--wrap=place_dungeon_terrain", "-Wl,--wrap=run_partition_monster_pass",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as temp:
        subprocess.run([str(exe), str(ROOT), temp, str(OUT)], cwd=temp, env=env, check=True, timeout=180)
    colors = {"#": "#20242c", ".": "#747b86", "~": "#237fca", "i": "#b6e9f0", "L": "#ef6a2d",
              "@": "#36ff81", "0": "#ffff4d", "O": "#dca9ff", "N": "#ff42d1", "H": "#ffffff",
              "<": "#ffec54", ">": "#ffec54", "v": "#ffec54", "m": "#cb7694", "*": "#efce91", "+": "#ac9d65"}
    for depth in (20,22,23):
        rows=(OUT/f"level-{depth}.txt").read_text().splitlines()
        pixels=b"".join((b"\0"+b"".join(bytes.fromhex(colors.get(token,"#ffffff")[1:])*5 for token in row))*5 for row in rows)
        def chunk(kind,payload):
            return struct.pack(">I",len(payload))+kind+payload+struct.pack(">I",zlib.crc32(kind+payload))
        png=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",len(rows[0])*5,len(rows)*5,8,2,0,0,0))
        png+=chunk(b"IDAT",zlib.compress(pixels))+chunk(b"IEND",b"")
        (OUT/f"level-{depth}.png").write_bytes(png)

if __name__ == "__main__":
    main()
