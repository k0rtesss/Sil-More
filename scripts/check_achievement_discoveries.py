#!/usr/bin/env python3
"""Exercise production discovery, early, Will and song routes in disposable profiles."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_achievement_expansion import CHECKS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-discoveries"

HELPERS = {
    "doors": '#include "cmd/world/cmd-interact.c"\nbool fixture_close(int y,int x) { return do_cmd_close_aux(y,x); }\n',
    "abilities": '#include "cmd/ui/cmd-ui-abilities.c"\nbool fixture_buy(int s,int a) { return ability_browser_activate_choice(s,a); }\n',
}

TESTS = r'''
#include "player/killer.h"
#include "player/player-song-internal.h"
bool fixture_close(int,int);
bool fixture_buy(int,int);
static bool confirm = true;
static int check_result = -99999, forced_die, dice_index;
bool __wrap_get_check(cptr text) { (void)text; return confirm; }
bool __wrap_get_check_near(int y,int x,cptr text) { (void)y;(void)x;(void)text;return confirm; }
int __real_skill_check(monster_type*,int,int,monster_type*);
int __wrap_skill_check(monster_type* a,int s,int d,monster_type* b)
{ return check_result == -99999 ? __real_skill_check(a,s,d,b) : check_result; }
u32b __real_Rand_div(u32b);
u32b __wrap_Rand_div(u32b sides)
{ if(!forced_die)return __real_Rand_div(sides); return (++dice_index%2)?sides-1:0; }

static monster_type* ready(int race)
{
    monster_type* m=prepare(race);
    p_ptr->pcharacter=0;current_character_profile=&c_info[0];
    p_ptr->chp=p_ptr->mhp=1000;p_ptr->csp=p_ptr->msp=1000;
    p_ptr->leaving=p_ptr->is_dead=p_ptr->leaping=false;
    p_ptr->energy_use=100;
    cave_m_idx[p_ptr->py][p_ptr->px]=0;p_ptr->py=10;p_ptr->px=9;
    cave_m_idx[10][9]=-1;
    sil_achievements_begin_run();killer_reset();
    check_result=-99999;forced_die=0;confirm=true;
    return m;
}
static void remove_enemy(void)
{ if(cave_m_idx[10][10]>0)delete_monster_idx(cave_m_idx[10][10]); }
static void step(int y,int x)
{ monster_swap(p_ptr->py,p_ptr->px,y,x); assert(p_ptr->py==y && p_ptr->px==x); }
static void kill_player(monster_type* m)
{ killer_mark_monster(m);take_hit(p_ptr->chp,"a wolf");assert(p_ptr->is_dead); }
static void partition(int mode,int cave)
{
    partition_meta_save meta;memset(&meta,0,sizeof(meta));
    meta.grid_rows=meta.grid_cols=meta.partition_count=1;
    meta.modes[0]=mode;meta.big_cave_types[0]=cave;level_partition_meta_set(&meta);
}
static void checks(void)
{
    assert(init_flavor_info()==0);flavor_init();
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();
    const char* name=getenv("SIL_DISCOVERY_CASE");assert(name);
    monster_type* m=ready(41);
    if(!strcmp(name,"depth")) {
        sil_achievements_on_depth(1);assert(!earned(SIL_ACH_THE_ROAD_GOES_EVER_ON));
        sil_achievements_on_depth(2);assert(!earned(SIL_ACH_THE_ROAD_GOES_EVER_ON));
        sil_achievements_on_depth(3);assert(earned(SIL_ACH_THE_ROAD_GOES_EVER_ON));
    } else if(!strcmp(name,"purchase")) {
        p_ptr->new_exp=1000000;
        for(int a=1;a<4;a++)p_ptr->innate_ability[S_MEL][a]=true;
        confirm=false;assert(!fixture_buy(S_MEL,0));assert(!earned(SIL_ACH_SOMETHING_TOOKISH));
        confirm=true;s32b before=p_ptr->new_exp;
        assert(fixture_buy(S_MEL,0));assert(p_ptr->new_exp<before);
        assert(earned(SIL_ACH_SOMETHING_TOOKISH));
    } else if(!strcmp(name,"web")) {
        cave_feat[p_ptr->py][p_ptr->px]=FEAT_TRAP_WEB;
        check_result=-1;assert(!break_free_of_web());assert(!earned(SIL_ACH_FLIES_AND_SPIDERS));
        check_result=1;assert(break_free_of_web());assert(earned(SIL_ACH_FLIES_AND_SPIDERS));
    } else if(!strcmp(name,"wolf")) {
        m=ready(11);sil_achievements_on_monster_seen((int)(m-mon_list));
        assert(m->achievement_first_wolf);
        mon_list[2]=*m;mon_list[2].achievement_first_wolf=false;mon_max=3;
        kill_player(&mon_list[2]);assert(!earned(SIL_ACH_NOT_EVEN_CARCHAROTH));
        m=ready(11);sil_achievements_on_monster_seen((int)(m-mon_list));
        p_ptr->achievement_kills=1;kill_player(m);assert(!earned(SIL_ACH_NOT_EVEN_CARCHAROTH));
        m=ready(11);sil_achievements_on_monster_seen((int)(m-mon_list));
        kill_player(m);assert(earned(SIL_ACH_NOT_EVEN_CARCHAROTH));
    } else if(!strcmp(name,"water")) {
        remove_enemy();cave_feat[10][10]=FEAT_WATER;
        step(10,10);step(10,11);assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION));
        step(10,10);step(10,9);
        cave_feat[10][11]=FEAT_DEEP_WATER;
        step(10,10);step(10,11);step(10,10);step(10,11);step(10,12);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION)); /* Only two distinct cells. */
        step(10,11);step(10,10);step(10,9);
        cave_feat[11][10]=cave_feat[12][10]=FEAT_WATER;
        step(10,10);step(11,10);step(12,10);step(12,9);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION)); /* Back to the same bank. */
        step(11,9);step(10,9);
        cave_feat[10][12]=FEAT_WATER;
        cave_feat[10][13]=FEAT_BRIDGE_WATER_H;
        step(10,10);step(10,11);step(10,12);step(10,13);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION)); /* A bridge is not a bank. */
        step(10,12);step(10,11);step(10,10);step(10,9);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION));
        cave_feat[10][13]=FEAT_FLOOR;
        step(10,10);step(10,11);step(10,12);step(10,13);
        assert(earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else if(!strcmp(name,"water-mixed")) {
        remove_enemy();cave_feat[10][10]=FEAT_WATER;cave_feat[10][11]=FEAT_ICE;
        cave_feat[10][12]=FEAT_WATER;
        step(10,10);step(10,11);step(10,12);step(10,13);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION)); /* Ice is not water. */
        step(10,12);step(10,11);step(10,10);step(10,9);
        cave_feat[10][13]=FEAT_DEEP_WATER;
        step(10,10);step(10,11);step(10,12);step(10,13);step(10,14);
        assert(earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else if(!strcmp(name,"water-angled")) {
        remove_enemy();cave_feat[11][10]=cave_feat[12][11]=cave_feat[13][12]=FEAT_WATER;
        step(11,10);step(12,11);step(13,12);step(14,13);
        assert(earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else if(!strcmp(name,"water-bend") || !strcmp(name,"poison-bend")) {
        bool water=!strcmp(name,"water-bend");int feat=water?FEAT_WATER:FEAT_POISON;
        int achievement=water?SIL_ACH_THE_FORDS_OF_SIRION:SIL_ACH_THROUGH_NAN_DUNGORTHEB;
        remove_enemy();p_ptr->resist_pois=3;
        cave_feat[10][10]=cave_feat[10][11]=cave_feat[10][12]=cave_feat[11][11]=feat;
        step(10,10);step(11,11);step(10,12);step(10,13);
        assert(earned(achievement)); /* A winding path can cross genuine banks. */
    } else if(!strcmp(name,"water-shore") || !strcmp(name,"poison-shore")) {
        bool water=!strcmp(name,"water-shore");int feat=water?FEAT_WATER:FEAT_POISON;
        int achievement=water?SIL_ACH_THE_FORDS_OF_SIRION:SIL_ACH_THROUGH_NAN_DUNGORTHEB;
        remove_enemy();p_ptr->resist_pois=3;
        cave_feat[10][10]=cave_feat[11][10]=cave_feat[12][10]=cave_feat[12][11]=feat;
        step(10,10);step(11,10);step(12,10);step(12,11);step(12,12);
        assert(!earned(achievement)); /* Following a curved shore is not crossing it. */
    } else if(!strcmp(name,"water-teleport")) {
        remove_enemy();cave_feat[10][10]=cave_feat[10][11]=cave_feat[10][12]=FEAT_WATER;
        step(10,10);step(10,12);step(10,13);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION));
        step(10,12);step(10,11);step(10,10);step(10,9);
        assert(earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else if(!strcmp(name,"ice")) {
        remove_enemy();cave_feat[10][10]=FEAT_ICE;
        step(10,10);step(10,11);assert(!earned(SIL_ACH_HELCARAXE));
        step(10,10);step(10,9);cave_feat[10][10]=FEAT_WATER;
        project_f(1,10,10,0,0,0,0,GF_COLD);
        step(10,10);step(10,11);assert(!earned(SIL_ACH_HELCARAXE));
        step(10,10);step(10,9);cave_feat[10][10]=FEAT_WATER;
        project_f(-1,10,10,0,0,0,0,GF_COLD);
        int index=10*MAX_DUNGEON_WID+10;
        assert(p_ptr->achievement_frozen_tiles[index/8]&(1u<<(index%8)));
        cave_set_feat(10,10,FEAT_MELTING_ICE);
        assert(p_ptr->achievement_frozen_tiles[index/8]&(1u<<(index%8)));
        cave_set_feat(10,10,FEAT_WATER);
        assert(!(p_ptr->achievement_frozen_tiles[index/8]&(1u<<(index%8))));
        project_f(1,10,10,0,0,0,0,GF_COLD);
        step(10,10);step(10,11);assert(!earned(SIL_ACH_HELCARAXE));
        step(10,10);step(10,9);cave_feat[10][10]=FEAT_WATER;
        project_f(-1,10,10,0,0,0,0,GF_COLD);
        step(10,10);step(10,9);assert(!earned(SIL_ACH_HELCARAXE));
        step(10,10);step(10,11);assert(earned(SIL_ACH_HELCARAXE));
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else if(!strncmp(name,"fire",4)) {
        remove_enemy();
        cave_feat[10][10]=FEAT_ICE;project_f(1,10,10,0,0,0,0,GF_FIRE);
        assert(!earned(SIL_ACH_FIRE_AND_WATER));
        m=ready(41);
        u32b old=r_info[m->r_idx].flags1;r_info[m->r_idx].flags1|=RF1_PEACEFUL;
        cave_feat[10][10]=FEAT_ICE;project_f(-1,10,10,0,0,0,0,GF_FIRE);
        assert(!earned(SIL_ACH_FIRE_AND_WATER));r_info[m->r_idx].flags1=old;
        cave_feat[10][10]=FEAT_ICE;
        project_f(!strcmp(name,"fire-monster")?1:!strcmp(name,"fire-environment")?0:-1,
            10,10,0,0,0,0,GF_FIRE);
        assert(earned(SIL_ACH_FIRE_AND_WATER));
    } else if(!strcmp(name,"lava")) {
        remove_enemy();p_ptr->resist_fire=4;
        cave_feat[10][10]=FEAT_LAVA;cave_set_feat(10,11,FEAT_WALL_EXTRA);
        p_ptr->leaping=true;step(10,10);p_ptr->previous_action[1]=6;continue_leap();
        assert(!earned(SIL_ACH_THE_FIRES_OF_THANGORODRIM));
        ready(41);remove_enemy();p_ptr->resist_fire=4;
        cave_feat[10][10]=FEAT_LAVA;p_ptr->leaping=true;step(10,10);
        p_ptr->previous_action[1]=6;continue_leap();
        assert(!p_ptr->is_dead && p_ptr->chp<p_ptr->mhp);
        assert(earned(SIL_ACH_THE_FIRES_OF_THANGORODRIM));
    } else if(!strcmp(name,"poison")) {
        remove_enemy();p_ptr->resist_pois=3;cave_feat[10][10]=FEAT_POISON;
        step(10,10);step(10,11);assert(!earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
        step(10,10);step(10,9);cave_feat[10][11]=FEAT_POISON;
        step(10,10);step(10,11);step(10,10);step(10,11);step(10,12);
        assert(!earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
        step(10,11);step(10,10);step(10,9);
        cave_feat[11][10]=cave_feat[12][10]=FEAT_POISON;
        step(10,10);step(11,10);step(12,10);step(12,9);
        assert(!earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
        step(11,9);step(10,9);cave_feat[10][12]=FEAT_POISON;
        cave_feat[10][13]=FEAT_BRIDGE_POISON_H;
        step(10,10);step(10,11);step(10,12);step(10,13);
        assert(!earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
        step(10,12);step(10,11);step(10,10);step(10,9);
        assert(!earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
        cave_feat[10][13]=FEAT_FLOOR;
        step(10,10);step(10,11);step(10,12);step(10,13);assert(!p_ptr->is_dead);
        assert(earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
    } else if(!strcmp(name,"poison-angled")) {
        remove_enemy();p_ptr->resist_pois=3;
        cave_feat[11][10]=cave_feat[12][11]=cave_feat[13][12]=FEAT_POISON;
        step(11,10);step(12,11);step(13,12);step(14,13);
        assert(earned(SIL_ACH_THROUGH_NAN_DUNGORTHEB));
    } else if(!strcmp(name,"labyrinth")) {
        remove_enemy();partition_meta_save meta;memset(&meta,0,sizeof(meta));
        meta.grid_rows=1;meta.grid_cols=meta.partition_count=2;
        meta.modes[1]=3;level_partition_meta_set(&meta);
        int border=1;
        while(border<p_ptr->cur_map_wid-2 && level_partition_index_for_point(10,border)==0)border++;
        assert(border<p_ptr->cur_map_wid-2);
        step(10,border-1);step(10,border);step(10,border-1);
        assert(!earned(SIL_ACH_A_JOURNEY_IN_THE_DARK));
        step(10,border);step(11,border);step(12,border);step(12,border-1);
        assert(earned(SIL_ACH_A_JOURNEY_IN_THE_DARK));
    } else if(!strcmp(name,"map")) {
        char text[256];
        metar.id=125;partition(3,0);sil_achievements_on_partition_entry(10,10);
        partition(4,0);sil_achievements_on_partition_entry(10,10);
        sil_achievements_on_level_entry(false);
        assert(sil_achievement_progress_text(SIL_ACH_A_MAP_OF_THE_IRON_HELLS,text,sizeof(text)));
        assert(strstr(text,"This run: 2/5 regions."));
        /* A long step/teleport clears crossings, but retains this run's visits. */
        sil_achievements_on_movement(10,10,12,12);
        partition(5,BIG_CAVE_ICE);sil_achievements_on_partition_entry(10,10);
        partition(5,BIG_CAVE_FIRE);sil_achievements_on_partition_entry(10,10);
        assert(!earned(SIL_ACH_A_MAP_OF_THE_IRON_HELLS));
        assert(sil_achievement_progress_text(SIL_ACH_A_MAP_OF_THE_IRON_HELLS,text,sizeof(text)));
        assert(strstr(text,"This run: 4/5 regions."));
        /* Another character in the same Tale starts with no earlier visits. */
        sil_achievements_begin_run();
        partition(5,BIG_CAVE_POIS);sil_achievements_on_partition_entry(10,10);
        assert(!earned(SIL_ACH_A_MAP_OF_THE_IRON_HELLS));
        assert(sil_achievement_progress_text(SIL_ACH_A_MAP_OF_THE_IRON_HELLS,text,sizeof(text)));
        assert(strstr(text,"This run: 1/5 regions."));
        sil_achievements_on_level_entry(true);
        partition(3,0);sil_achievements_on_partition_entry(10,10);
        partition(4,0);sil_achievements_on_partition_entry(10,10);
        partition(5,BIG_CAVE_ICE);sil_achievements_on_partition_entry(10,10);
        assert(!earned(SIL_ACH_A_MAP_OF_THE_IRON_HELLS));
        partition(5,BIG_CAVE_FIRE);sil_achievements_on_partition_entry(10,10);
        assert(earned(SIL_ACH_A_MAP_OF_THE_IRON_HELLS));
    } else if(!strcmp(name,"elbereth")) {
        ability(S_SNG,SNG_ELBERETH);p_ptr->song1=SNG_ELBERETH;
        m->hp=m->maxhp=1000;
        m->alertness=ALERTNESS_ALERT;m->stance=STANCE_CONFIDENT;m->tmp_morale=0;
        r_info[m->r_idx].flags2|=RF2_SMART;
        update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
        check_result=-1;sing();calc_morale(m);calc_stance(m);
        assert(!earned(SIL_ACH_A_ELBERETH_GILTHONIEL));
        check_result=100;sing();++playerturn;calc_morale(m);calc_stance(m);
        assert(m->stance==STANCE_FLEEING && earned(SIL_ACH_A_ELBERETH_GILTHONIEL));
    } else if(!strcmp(name,"freedom")) {
        ability(S_SNG,SNG_FREEDOM);p_ptr->song1=SNG_FREEDOM;
        update_flow(p_ptr->py,p_ptr->px,FLOW_PLAYER_NOISE);
        cave_feat[10][11]=FEAT_TRAP_DART;
        check_result=-1;sing();assert(!earned(SIL_ACH_RELEASE_FROM_BONDAGE));
        check_result=1;sing();assert(cave_feat[10][11]==FEAT_FLOOR);
        assert(earned(SIL_ACH_RELEASE_FROM_BONDAGE));
    } else if(!strcmp(name,"thresholds")) {
        int idx=(int)(m-mon_list);cave_m_idx[10][10]=0;m->fx=11;cave_m_idx[10][11]=idx;
        m->alertness=ALERTNESS_ALERT;cave_set_feat(10,10,FEAT_OPEN);
        fixture_close(10,10);assert(!earned(SIL_ACH_YOU_CANNOT_PASS));
        cave_set_feat(10,10,FEAT_OPEN);ability(S_SNG,SNG_THRESHOLDS);p_ptr->song1=SNG_THRESHOLDS;
        m->stance=STANCE_CONFIDENT;r_info[m->r_idx].flags1|=RF1_NEVER_MOVE;
        fixture_close(10,10);assert(!earned(SIL_ACH_YOU_CANNOT_PASS));
        r_info[m->r_idx].flags1&=~RF1_NEVER_MOVE;m->stance=STANCE_FLEEING;
        cave_set_feat(10,10,FEAT_OPEN);fixture_close(10,10);
        assert(!earned(SIL_ACH_YOU_CANNOT_PASS));
        m->stance=STANCE_CONFIDENT;cave_set_feat(10,10,FEAT_OPEN);
        fixture_close(10,10);assert(earned(SIL_ACH_YOU_CANNOT_PASS));
    } else if(!strcmp(name,"contest")) {
        ability(S_SNG,SNG_CONTEST);p_ptr->song1=SNG_CONTEST;
        p_ptr->song_target_idx=(int)(m-mon_list);p_ptr->song_target_song=SNG_CONTEST;
        m->song=SNG_MASTERY;check_result=100;
        for(int i=0;i<10 && !m->song_contest_completed;i++){sing();playerturn++;}
        assert(m->song_contest_completed && m->song_will_penalty>0);
        assert(earned(SIL_ACH_FELAGUNDS_ANSWER));
    } else if(!strcmp(name,"will")) {
        check_result=1;assert(!saving_throw(m,0));assert(!earned(SIL_ACH_A_WILL_OF_YOUR_OWN));
        check_result=0;assert(saving_throw(NULL,0));assert(!earned(SIL_ACH_A_WILL_OF_YOUR_OWN));
        assert(saving_throw(m,0));assert(earned(SIL_ACH_A_WILL_OF_YOUR_OWN));
    } else if(!strcmp(name,"indomitable")) {
        p_ptr->resist_fear=1;check_result=-1;ability(S_WIL,WIL_INDOMITABLE);
        assert(!allow_player_fear(m));assert(!earned(SIL_ACH_HURINS_DEFIANCE));
        p_ptr->chp=p_ptr->mhp/4;check_result=-20;assert(!allow_player_fear(m));
        assert(!earned(SIL_ACH_HURINS_DEFIANCE));
        check_result=-1;assert(!allow_player_fear(m));assert(earned(SIL_ACH_HURINS_DEFIANCE));
    } else if(!strcmp(name,"curse")) {
        object_type* o=&inventory[INVEN_LEFT];item(o,TV_RING,SV_RING_EVASION,1);
        o->ident|=IDENT_CURSED;p_ptr->equip_cnt=1;
        do_cmd_takeoff(o,INVEN_LEFT);assert(cursed_p(o));assert(!earned(SIL_ACH_NO_LONGER_BINDING));
        ability(S_WIL,WIL_CURSE_BREAKING);do_cmd_takeoff(o,INVEN_LEFT);
        assert(earned(SIL_ACH_NO_LONGER_BINDING));
    } else if(!strcmp(name,"checks")) {
        forced_die=1;dice_index=0;assert(skill_check(PLAYER,0,4,NULL)>0);
        assert(!earned(SIL_ACH_A_FOOLS_HOPE));
        dice_index=0;assert(skill_check(PLAYER,0,5,NULL)>0);
        assert(earned(SIL_ACH_A_FOOLS_HOPE));
    } else assert(false);
    printf("PASS discovery production route: %s\n",name);
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + BASE[:BASE.index("static void check_thrall(void)")]
                      + TESTS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    helpers = []
    for name, content in HELPERS.items():
        path = OUT / (name + ".c")
        path.write_text(content, encoding="utf-8")
        helpers.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/world/cmd-interact.c.obj",
                "/src/cmd/ui/cmd-ui-abilities.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print",
             "message_flush", "get_sdl_config_path", "pause_with_text", "quest_typewriter_menu_pages",
             "ui_question_ask", "get_aim_dir", "show_interaction_skill_roll_animation_actor",
             "hit_roll", "hit_roll_details", "get_check", "get_check_near", "skill_check", "Rand_div")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *helpers,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    cases = ("depth", "purchase", "web", "wolf", "water", "water-mixed", "water-angled", "water-bend", "water-shore", "water-teleport", "ice", "fire", "fire-monster", "fire-environment", "lava", "poison", "poison-angled", "poison-bend", "poison-shore",
             "labyrinth", "map", "elbereth", "freedom", "thresholds", "contest", "will",
             "indomitable", "curse", "checks")
    for case in cases:
        env["SIL_DISCOVERY_CASE"] = case
        with tempfile.TemporaryDirectory(prefix=case + "-", dir=OUT) as data:
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                           env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
