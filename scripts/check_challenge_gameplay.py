#!/usr/bin/env python3
"""Full-engine challenge restrictions, lineage accounting, and resident stairs.

Uses initialized production objects and an isolated temporary data directory.
Explicit maps exercise cache integration; this is not a natural winning run.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_vertical_cache import WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/challenge-gameplay"

CHECKS = r'''
#include "metarun.h"
#include "blitz.h"
#include "quest/quest-challenges.h"
#include "quest/quest-runtime.h"
int get_start_xp(void);
#include "level-generation/level-cache.c"
extern metarun* metaruns;
extern s16b metarun_max, current_run;
void reset_defaults(metarun*);
bool fixture_make_artefact(object_type*);
void fixture_write_begin(byte*,size_t);
size_t fixture_write_end(void);
void fixture_read_begin(const byte*,size_t,int);
size_t fixture_read_end(void);
static int selection;
static const char* menu_keys;
char __real_inkey(void);
char __wrap_inkey(void)
{
    if(!menu_keys)return __real_inkey();
    assert(*menu_keys);return *menu_keys++;
}
void __wrap_metarun_save_persistent_settings(void) {}
void __wrap_save_pane_config_to_json(void) {}
int __wrap_quest_reward_book_choice(cptr title,cptr texts[],int text_count,
    cptr prompt,const int values[],cptr labels[],const bool enabled[],
    int count,int initial,void (*inspect)(int))
{
    (void)title;(void)texts;(void)text_count;(void)prompt;(void)labels;
    (void)enabled;(void)initial;(void)inspect;
    assert(count==6);
    for(int i=0;i<count;i++)assert(values[i]==i);
    return selection;
}
static void seed(int challenge)
{
    level_cache_clear();
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    reset_map(10); p_ptr->mhp=p_ptr->chp=100;
    p_ptr->food=PY_FOOD_FULL;
    p_ptr->quest_challenge=challenge;
    op_ptr->opt[OPT_quest_challenges_beta]=true;
    birth_discon_stair=birth_ironman=birth_no_artefacts=birth_fixed_exp=false;
    adult_discon_stair=adult_ironman=adult_no_artefacts=false;
    op_ptr->opt[OPT_ADULT+OPT_birth_fixed_exp-OPT_BIRTH]=false;
    playerturn=1;
    run_mode_set_current(RUN_MODE_STORY);
    for(int y=0;y<88;y++)for(int x=0;x<88;x++)
        if(!y||!x||y==87||x==87) cave_set_feat(y,x,FEAT_WALL_PERM);
    cave_vertical_reset();vertical_vault_reset();
}
static level_image* capture(void)
{
    level_image* image=level_image_new();assert(image);
    level_image_capture(image);image->visited=true;return image;
}
static int stairs(int direction)
{
    int n=0;
    for(int y=1;y<87;y++)for(int x=1;x<87;x++) {
        int f=cave_feat[y][x];
        n+=direction?(f==FEAT_MORE||f==FEAT_MORE_SHAFT):(f==FEAT_LESS||f==FEAT_LESS_SHAFT);
    }
    return n;
}
static void restrictions(void)
{
    seed(CHALLENGE_FIXED_50K_XP);assert(get_start_xp()==50000);
    p_ptr->exp=50000;p_ptr->new_exp=30000;gain_exp(1000);
    assert(p_ptr->exp==50000&&p_ptr->new_exp==30000);
    seed(0);birth_fixed_exp=true;assert(get_start_xp()==50000);
    p_ptr->exp=50000;p_ptr->new_exp=30000;gain_exp(1000);
    assert(p_ptr->exp==50000&&p_ptr->new_exp==30000);
    birth_fixed_exp=false;gain_exp(1000);assert(p_ptr->exp==51000);
    seed(CHALLENGE_TULKAS_BLUNT);
    object_type o;object_prep(&o,lookup_kind(TV_SWORD,SV_LONG_SWORD));
    assert(!quest_challenge_object_allowed(&o));assert(!quest_challenge_weapon_allowed(&o));
    assert(quest_challenge_melee_damage(&o,20)==0);
    o.tval=TV_POLEARM;assert(!quest_challenge_object_allowed(&o));
    o.tval=TV_BOW;assert(!quest_challenge_object_allowed(&o));
    o.tval=TV_ARROW;assert(!quest_challenge_object_allowed(&o));
    o.tval=TV_HAFTED;assert(quest_challenge_object_allowed(&o));
    assert(quest_challenge_melee_damage(&o,20)==20);
    assert(quest_challenge_weapon_allowed(NULL));assert(quest_challenge_forbid_ranged());
    p_ptr->energy_use=0;do_cmd_fire(0);assert(!p_ptr->energy_use);
    do_cmd_throw(false);assert(!p_ptr->energy_use);
    seed(CHALLENGE_TORCHLIGHT);
    for(int k=1;k<z_info->k_max;k++)if(k_info[k].tval==TV_LIGHT) {
        object_prep(&o,k);
        bool allowed=o.sval==SV_LIGHT_TORCH||o.sval==SV_LIGHT_MALLORN
            ||(k_info[k].flags3&TR3_INSTA_ART);
        assert(quest_challenge_object_allowed(&o)==allowed);
    }
    puts("Fixed XP birth and gains; blunt item/melee/real fire/throw command gates; every light kind PASS.");
}
static void lineage(void)
{
    int quests[]={QUEST_ID_MANDOS_TRAITOR,QUEST_ID_OROME_DRAGONS,QUEST_ID_NIENA_MORGOTH,QUEST_ID_TULKAS_ORCS,QUEST_ID_VARDA_SHADOW};
    for(int id=1;id<=5;id++) {
        seed(id);quest_set_enabled(quests[id-1],true);
        assert(!quest_challenge_unlocked(id));
        p_ptr->noscore=1;quest_challenge_unlock_for_quest(quests[id-1]);
        assert(!quest_challenge_unlocked(id));p_ptr->noscore=0;
        quest_challenge_unlock_for_quest(quests[id-1]);assert(quest_challenge_unlocked(id));
        quest_challenge_record_escape();assert(!quest_challenge_completion_count(id));
        object_prep(&inventory[0],lookup_kind(TV_LIGHT,SV_LIGHT_SILMARIL));inventory[0].number=1;
        assert(silmarils_possessed()==1);
        quest_challenge_record_escape();assert(quest_challenge_completion_count(id)==1);
        quest_challenge_record_escape();assert(quest_challenge_completion_count(id)==1);
        p_ptr->quest_challenge_recorded=0;p_ptr->quest_challenge_failed=1;
        quest_challenge_record_escape();assert(quest_challenge_completion_count(id)==1);
        p_ptr->quest_challenge_failed=0;op_ptr->opt[OPT_quest_challenges_beta]=false;
        quest_challenge_validate();assert(p_ptr->quest_challenge_failed);
        op_ptr->opt[OPT_quest_challenges_beta]=true;
        quest_challenge_record_escape();assert(quest_challenge_completion_count(id)==1);
    }
    assert(save_metaruns()==0);memset(metaruns,0,sizeof(*metaruns));
    assert(load_metaruns(false)==0);current_run=0;metar=metaruns[0];
    for(int id=1;id<=5;id++)assert(quest_challenge_unlocked(id)&&quest_challenge_completion_count(id)==1);
    for(int id=0;id<=5;id++) {
        seed(0);selection=id;quest_challenge_choose();
        assert(p_ptr->quest_challenge==id&&!p_ptr->quest_challenge_failed&&!p_ptr->quest_challenge_recorded);
    }
    for(int mode=0;mode<3;mode++) {
        seed(CHALLENGE_TORCHLIGHT);
        if(mode==0)p_ptr->noscore=1;
        if(mode==1)p_ptr->quest_test_sandbox=1;
        if(mode==2)run_mode_set_current(RUN_MODE_BLITZ);
        quest_challenge_validate();assert(p_ptr->quest_challenge_failed);
    }
    run_mode_set_current(RUN_MODE_STORY);
    puts("All five quest unlocks, no-score rejection, Silmaril requirement, idempotence, failed/toggled forfeits and disk lineage roundtrip PASS.");
    puts("All five birth selections plus ordinary journey; wizard/sandbox/Blitz qualification forfeits PASS (choice presentation boundary scripted).");
}
static void ordinary(void)
{
    seed(0);birth_ironman=true;min_depth_counter=0;
    cave_set_feat(2,2,FEAT_LESS);do_cmd_go_up();
    assert(p_ptr->depth==10&&!p_ptr->energy_use&&!p_ptr->leaving);
    object_prep(&inventory[0],lookup_kind(TV_LIGHT,SV_LIGHT_SILMARIL));inventory[0].number=1;
    do_cmd_go_up();assert(p_ptr->depth==9&&p_ptr->energy_use==100&&p_ptr->leaving);
    seed(0);adult_no_artefacts=true;birth_no_artefacts=true;
    object_generation_mode=OB_GEN_MODE_MONSTER_DROP;object_level=10;
    object_type o={0};assert(!make_guaranteed_artefact(&o,DROP_QUALITY_SUPERB,DROP_TYPE_UNTHEMED));
    adult_no_artefacts=false;birth_no_artefacts=false;
    puts("Ordinary Ironman real ascent blocked without Silmaril and succeeds with one; no-artefacts guaranteed-drop gate PASS.");
}
static void menu_route(void)
{
    seed(0);playerturn=0;p_ptr->exp=p_ptr->new_exp=PY_START_EXP;
    menu_keys="222222222\ry2y2y2y\033\033";do_cmd_options();assert(!*menu_keys);
    assert(birth_discon_stair&&birth_ironman&&birth_no_artefacts&&birth_fixed_exp);
    assert(adult_discon_stair&&adult_ironman&&adult_no_artefacts
        &&op_ptr->opt[OPT_ADULT+OPT_birth_fixed_exp-OPT_BIRTH]);
    assert(p_ptr->exp==PY_FIXED_EXP&&p_ptr->new_exp==PY_FIXED_EXP);
    playerturn=1;
    menu_keys="222222222\rn2n2n2n\033\033";do_cmd_options();assert(!*menu_keys);
    assert(birth_discon_stair&&birth_ironman&&birth_no_artefacts&&birth_fixed_exp);
    menu_keys=NULL;
    puts("Actual outer Options menu reaches Challenge Options; all four editable before first action and locked afterwards PASS.");
}
static void artefact_menu(void)
{
    seed(0);playerturn=0;
    int art=z_info->art_spec_max;
    artefact_type saved=a_info[art];
    a_info[art]=(artefact_type){0};a_info[art].tval=TV_SWORD;
    a_info[art].sval=SV_LONG_SWORD;a_info[art].rarity=1;a_info[art].level=1;
    object_type o;object_prep(&o,lookup_kind(TV_SWORD,SV_LONG_SWORD));
    assert(fixture_make_artefact(&o)&&o.name1==art);
    menu_keys="222222222\r22y\033\033";do_cmd_options();assert(!*menu_keys);
    object_prep(&o,lookup_kind(TV_SWORD,SV_LONG_SWORD));
    assert(birth_no_artefacts&&adult_no_artefacts&&!fixture_make_artefact(&o));
    menu_keys="222222222\r22n\033\033";do_cmd_options();assert(!*menu_keys);
    object_prep(&o,lookup_kind(TV_SWORD,SV_LONG_SWORD));
    assert(!birth_no_artefacts&&!adult_no_artefacts&&fixture_make_artefact(&o)&&o.name1==art);
    a_info[art]=saved;menu_keys=NULL;
    puts("Actual UI No-artefacts enable/disable immediately blocks/restores production make_artefact (guaranteed matching test template) PASS.");
}
static void cache_option_changes(void)
{
    for(int option=0;option<2;option++)for(int ready=0;ready<2;ready++)for(int initial=0;initial<2;initial++) {
        seed(0);playerturn=0;
        if(option)birth_no_artefacts=adult_no_artefacts=initial;
        else birth_discon_stair=adult_discon_stair=initial;
        cave_set_feat(10,10,FEAT_MORE);
        floors[0]=active=capture();level_image* original=active;running=true;
        reset_map(9);floors[2]=capture();level_image* visited=floors[2];
        reset_map(11);if(ready){floors[1]=capture();floors[1]->visited=false;}
        level_image_restore(original);
        level_job parked={0};parked.options=*op_ptr;parked.player=*p_ptr;parked.image=original;
        job=&parked;
        menu_keys=option?(initial?"222222222\r22n\033\033":"222222222\r22y\033\033")
            :(initial?"222222222\rn\033\033":"222222222\ry\033\033");
        do_cmd_options();assert(!*menu_keys);menu_keys=NULL;
        if(option)assert(birth_no_artefacts==!initial&&adult_no_artefacts==!initial);
        else assert(birth_discon_stair==!initial&&adult_discon_stair==!initial);
        assert(SDL_GetAtomicInt(&parked.cancel)&&generation_conflicts(&parked));
        assert(!floors[1]&&floors[0]==original&&floors[2]==visited);
        level_job next={0};next.depth=11;prepare_links(&next);
        assert(next.links.feat[10][10]==((option||initial)?FEAT_LESS:0));
        job=NULL;
    }
    level_cache_clear();
    puts("UI disconnected/no-artefacts on/off before and after neighbour publication cancels stale jobs, evicts unvisited maps, preserves visited maps and rebuilds correct reciprocal links PASS.");
}
static void single_stairs(void)
{
    seed(CHALLENGE_SINGLE_STAIR);
    cave_set_feat(2,2,FEAT_LESS);cave_set_feat(3,3,FEAT_LESS_SHAFT);
    cave_set_feat(4,4,FEAT_MORE);cave_set_feat(5,5,FEAT_MORE_SHAFT);
    level_job next={0};worker_job=&next;
    next.links.feat[6][6]=FEAT_LESS;next.links.feat[7][7]=FEAT_MORE;
    assert(level_cache_generation_links());worker_job=NULL;
    assert(stairs(0)==1&&stairs(1)==1&&cave_feat[2][2]==FEAT_LESS);
    quest_challenge_prune_stairs();assert(stairs(0)==1&&stairs(1)==1);
}
static void cache_links(void)
{
    for(int mode=0;mode<3;mode++) {
        seed(mode==2?CHALLENGE_DISCONNECTED:0);birth_discon_stair=mode==1;
        cave_set_feat(10,10,FEAT_MORE);cave_set_feat(12,12,FEAT_MORE_SHAFT);
        cave_set_feat(15,15,FEAT_CHASM);
        floors[0]=active=capture();running=true;active->chasm_drop[15][15]=3;
        level_job next={0};next.depth=11;prepare_links(&next);
        assert(next.links.feat[10][10]==(mode?0:FEAT_LESS));
        assert(next.links.feat[15][15]==FEAT_CHASM&&next.links.drop[15][15]==2);
        assert(next.links.stair_peer_count==(mode?0:1));
        memset(&next,0,sizeof(next));next.depth=12;prepare_links(&next);
        assert(next.links.feat[12][12]==(mode?0:FEAT_LESS_SHAFT));
        memset(&next,0,sizeof(next));next.depth=13;prepare_links(&next);
        assert(next.links.feat[15][15]==FEAT_FLOOR&&next.links.ceiling[15][15]);
    }
    single_stairs();
    puts("Ordinary, birth-disconnected, quest-disconnected reciprocal stair/shaft plans; physical holes/long falls unchanged; single-stair generation and entry pruning PASS.");
}
static void cache_arrivals(void)
{
    for(int mode=0;mode<3;mode++)for(int travel=0;travel<5;travel++) {
        seed(mode==2?CHALLENGE_DISCONNECTED:0);birth_discon_stair=mode==1;
        int destination=travel==1?12:11;
        p_ptr->py=p_ptr->px=10;
        cave_set_feat(10,10,travel==2?FEAT_CHASM:travel==1?FEAT_MORE_SHAFT:FEAT_MORE);
        level_image* source=capture();
        reset_map(destination);p_ptr->py=p_ptr->px=2;
        cave_set_feat(10,10,travel==2?FEAT_FLOOR:travel==1?FEAT_LESS_SHAFT:FEAT_LESS);
        level_image* target=capture();
        level_image_restore(source);floors[0]=active=source;running=true;
        /* Park the scheduler, so these explicit maps test the production
         * transition without launching unrelated background generation. */
        level_job parked={0};job=&parked;
        if(travel!=4)floors[1]=target;
        p_ptr->depth=destination;p_ptr->create_stair=travel==1?FEAT_LESS_SHAFT:FEAT_LESS;
        if(travel==3)assert(level_cache_travel_to(destination,10,10));
        assert(level_cache_change_level());
        if(travel==4) {
            assert(p_ptr->depth==10&&pending_depth==destination);
            floors[1]=target;level_cache_poll();assert(p_ptr->depth==destination&&p_ptr->leaving);
            assert(level_cache_change_level());
        }
        assert(p_ptr->depth==destination);
        if(mode&&(travel==0||travel==1||travel==4))
            assert(cave_feat[p_ptr->py][p_ptr->px]==FEAT_FLOOR);
        else assert(p_ptr->py==10&&p_ptr->px==10);
        job=NULL;
    }
    level_cache_clear();
    puts("Actual cached stairs, shafts, deferred stairs: birth and quest disconnected land independently; ordinary stairs, physical falls and explicit portal travel keep coordinates PASS.");
}
static void safe_arrivals(void)
{
    for(int mode=0;mode<3;mode++)for(int repeat=0;repeat<12;repeat++) {
        seed(CHALLENGE_DISCONNECTED);
        cave_set_feat(2,2,FEAT_MORE);floors[0]=active=capture();level_image* source=active;
        reset_map(11);p_ptr->cur_map_hgt=p_ptr->cur_map_wid=140;
        for(int y=0;y<140;y++)for(int x=0;x<140;x++)cave_set_feat(y,x,FEAT_WALL_EXTRA);
        /* Only the validated distant entry is legal. The closer ordinary
         * floor, an isolated protected vault and an occupied floor are bait. */
        cave_set_feat(120,120,FEAT_FLOOR);p_ptr->py=p_ptr->px=120;
        cave_set_feat(10,10,FEAT_FLOOR);
        cave_set_feat(20,20,FEAT_FLOOR);cave_info[20][20]|=CAVE_ICKY;
        cave_set_feat(25,25,FEAT_FLOOR);cave_o_idx[25][25]=1;
        cave_set_feat(5,5,FEAT_MORE);
        if(mode==0){cave_info[10][10]|=CAVE_ICKY;playerturn=1;}
        if(mode==1){mon_max=2;mon_list[1].r_idx=R_IDX_NIENA;}
        if(mode==2)p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
        floors[1]=capture();
        level_image_restore(source);running=true;
        level_job parked={0};job=&parked;
        p_ptr->depth=11;assert(level_cache_change_level());
        assert(p_ptr->py==120&&p_ptr->px==120);
        assert(!(cave_info[p_ptr->py][p_ptr->px]&CAVE_ICKY)&&!cave_o_idx[p_ptr->py][p_ptr->px]);
        if(mode)assert(calculate_nearest_down_stair_distance_from(p_ptr->py,p_ptr->px)>=87);
        job=NULL;
    }
    level_cache_clear();
    puts("Disconnected landings reject isolated protected vault/occupied floors; Nienna giver and accepted quest preserve >=87 downstairs distance (36 actual transitions) PASS.");
}
static void pending_roundtrip(void)
{
    static byte encoded[1024*1024];
    for(int mode=0;mode<3;mode++)for(int travel=0;travel<10;travel++) {
        seed(mode==2?CHALLENGE_DISCONNECTED:0);birth_discon_stair=mode==1;
        int departure=travel==5?UTUMNO_FORGE_DEPTH:travel==8?MORGOTH_DEPTH:travel==9?UTUMNO_DEPTH:10;
        int destination=travel==5?MORGOTH_DEPTH:travel==1?12:travel==6?9:travel==7?8
            :travel==8?UTUMNO_DEPTH:travel==9?UTUMNO_FORGE_DEPTH:11;
        p_ptr->depth=departure;p_ptr->py=p_ptr->px=10;
        const int features[]={FEAT_MORE,FEAT_MORE_SHAFT,FEAT_CHASM,FEAT_MORE,FEAT_MORE,
            FEAT_LESS,FEAT_LESS,FEAT_LESS_SHAFT,FEAT_MORE_SHAFT,FEAT_MORE};
        cave_set_feat(10,10,features[travel]);
        int landing=travel==3?30:10;
        if(travel==3||travel==4) {
            cave_vertical_state portal={0};portal.breath=12;portal.last_turn=turn;portal.portal_count=1;
            portal.portals[0]=(cave_vertical_portal){.identity=0x10ABCD,.y=10,.x=10,
                .peer_depth=destination,.peer_y=landing,.peer_x=landing,
                .kind=CAVE_VERTICAL_STREAM,.material=FEAT_WATER,.strength=1,
                .flags=CAVE_VERTICAL_DOWN|CAVE_VERTICAL_REVERSIBLE};
            assert(cave_vertical_restore(&portal));
        }
        level_image* source=capture();
        reset_map(destination);p_ptr->py=p_ptr->px=2;
        cave_set_feat(landing,landing,travel==2?FEAT_FLOOR:FEAT_LESS);
        cave_vertical_reset();level_image* target=capture();
        level_image_restore(source);floors[0]=active=source;running=true;
        level_job parked={0};job=&parked;
        if(travel==3||travel==4)assert(level_cache_travel_to(destination,landing,landing));
        else p_ptr->depth=destination;
        assert(level_cache_change_level()&&pending_depth==destination&&p_ptr->depth==departure);
        bool disconnected=mode&&(travel==0||travel==1||travel==6||travel==7);
        assert(pending_disconnected_stair==disconnected);
        fixture_write_begin(encoded,sizeof(encoded));level_cache_write();
        save_wr_u32b(0x1928ABCDU);size_t length=fixture_write_end();
        player_type player=*p_ptr;level_image* restored=capture();
        job=NULL;level_cache_clear();level_image_restore(restored);*p_ptr=player;
        fixture_read_begin(encoded,length,VERSION_EXTRA);
        assert(level_cache_read()==0);u32b sentinel;load_rd_u32b(&sentinel);
        assert(sentinel==0x1928ABCDU&&fixture_read_end()==length);
        if(travel==4)assert(cave_stair_bold(10,10)&&cave_vertical_get()->portal_count==1);
        assert(pending_disconnected_stair==disconnected);
        assert(pending_depth==destination&&pending_y==landing&&pending_x==landing);
        floors[1]=target;job=&parked;
        level_cache_poll();assert(p_ptr->depth==destination&&use_pending_origin);
        assert(level_cache_change_level());
        if(disconnected)assert(cave_feat[p_ptr->py][p_ptr->px]==FEAT_FLOOR);
        else assert(p_ptr->py==landing&&p_ptr->px==landing);
        job=NULL;level_image_free(restored);level_cache_clear();
    }
    puts("Real cache write/clear/read pending transition roundtrip: ordinary/birth/quest up/down stairs and shafts, physical falls, offset/same-column registered portals and all three same-column private Utumno routes; following save sentinel unchanged (30 cases) PASS.");
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true", help="Use committed cache to reproduce regressions")
    parser.add_argument("--review-baseline", choices=("landing", "adult", "published", "pending"), help="Remove one reviewed fix in a generated source copy")
    parser.add_argument("--case", choices=("all", "links", "arrivals", "single", "menu", "safety", "changes", "artefact", "pending"), default="all")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    checks = CHECKS
    if args.baseline:
        baseline = OUT / "cache-baseline.c"
        baseline.write_text(subprocess.check_output(["git", "show", "HEAD:src/level-generation/level-cache.c"],cwd=ROOT,text=True),encoding="utf-8")
        checks = checks.replace('"level-generation/level-cache.c"', '"' + baseline.as_posix() + '"')
        checks += '\nvoid level_cache_options_changed(void) {}\n'
    if args.review_baseline in ("landing", "published", "pending"):
        cache=(ROOT/"src/level-generation/level-cache.c").read_text(encoding="utf-8")
        if args.review_baseline=="landing":
            cache,n=re.subn(r'if \(cave_naked_bold\(y, x\).*?&& one_in_\(\+\+candidates\)\) \{',
                'if (cave_feat[y][x] == FEAT_FLOOR && !cave_m_idx[y][x] && player_passable(y, x, false) && one_in_(++candidates)) {',cache,flags=re.S)
        elif args.review_baseline=="published":
            cache,n=re.subn(r'    for \(int i = 0; i < LEVEL_CACHE_SLOTS; i\+\+\)\n        if \(floors\[i\] && floors\[i\] != active && !floors\[i\]->visited\)\n            evict_floor\(i\);','',cache)
        else:
            start=cache.index('    /* The pending origin is already persisted.')
            end=cache.index('    return 0;',start)
            cache=cache[:start]+cache[end:];n=1
        assert n==1
        baseline=OUT/"cache-review-baseline.c";baseline.write_text(cache,encoding="utf-8")
        checks=checks.replace('"level-generation/level-cache.c"','"'+baseline.as_posix()+'"')
    calls = {"all":"restrictions();lineage();ordinary();cache_links();cache_arrivals();safe_arrivals();menu_route();artefact_menu();cache_option_changes();pending_roundtrip();", "links":"cache_links();", "arrivals":"cache_arrivals();", "single":"single_stairs();", "menu":"menu_route();artefact_menu();", "safety":"safe_arrivals();", "changes":"cache_option_changes();", "artefact":"artefact_menu();", "pending":"pending_roundtrip();"}
    source.write_text(prefix + checks + init + r'''
    assert(!init_quest_info());
    ANGBAND_DIR_APEX=ANGBAND_DIR_METARUN=argv[2];
    metaruns=mem_alloc_array(1,metarun);metarun_max=1;current_run=0;
    reset_defaults(&metaruns[0]);metar=metaruns[0];
''' + calls[args.case] + r'''
    level_cache_clear();
    SDL_Quit();return 0;
}
''', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(("/src/main.c.obj", "/src/level-generation/level-cache.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj", "/src/object/object-make.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj"))))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD/"_deps"/p) for p in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),"C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    exe = OUT / "check.exe"
    settings = ROOT / "src/cmd/ui/cmd-ui-settings.c"
    if args.review_baseline=="adult":
        text=settings.read_text(encoding="utf-8")
        start=text.index("    if (opt == OPT_birth_discon_stair",text.index("static void option_apply_side_effects"))
        end=text.index("    for (int i = 0;",start)
        settings=OUT/"settings-review-baseline.c";settings.write_text(text[:start]+text[end:],encoding="utf-8")
    if args.baseline and args.case=="menu":
        settings = OUT / "settings-baseline.c"
        settings.write_text(subprocess.check_output(["git","show","HEAD:src/cmd/ui/cmd-ui-settings.c"],cwd=ROOT,text=True),encoding="utf-8")
    maker=OUT/"maker.c"
    maker.write_text('#include "object/object-make.c"\nbool fixture_make_artefact(object_type* o) { return make_artefact(o,true); }\n',encoding="utf-8")
    writer=OUT/"writer.c";writer.write_text(WRITER,encoding="utf-8")
    reader=OUT/"reader.c";reader.write_text(READER,encoding="utf-8")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g","@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(settings),str(maker),str(writer),str(reader),"@"+str(rsp),"@CMakeFiles/sil-more.dir/linkLibs.rsp","-Wl,--wrap=quest_reward_book_choice,--wrap=inkey,--wrap=metarun_save_persistent_settings,--wrap=save_pane_config_to_json","-o",str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/"lib/edit"),data],cwd=data,env=env,check=True,timeout=90)

if __name__ == "__main__":
    main()
