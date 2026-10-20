#!/usr/bin/env python3
"""Production routes for the twelve expansion achievements and negative controls.

Uses current Windows build objects, dummy SDL, temporary engine/profile data and
UI input doubles. It never opens real saves, accounts or sends SDL input events.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-expansion"

HELPERS = {
    "ranged": '#include "cmd/combat/cmd-ranged.c"\nbool fixture_potion(object_type* o) { return thrown_potion_effects(o,10,10); }\n',
    "food": '#include "use-obj.c"\nbool fixture_food(object_type* o) { bool ident=false; return eat_food(o,&ident); }\n',
    "doors": '#include "cmd/world/cmd-interact.c"\nbool fixture_bash(int y,int x) { return do_cmd_bash_aux(y,x,NULL,NULL); }\n',
}

CHECKS = r'''
#include "achievements.h"
#include "ui/question.h"
#include "spell/spell-projection-internal.h"
#undef assert
#define assert(test) do { if (!(test)) { fprintf(stderr,"FAIL: %s line %d\n",#test,__LINE__); exit(1); } } while(0)
bool fixture_potion(object_type*);
bool fixture_food(object_type*);
bool fixture_bash(int,int);
void m_engage_rewired_trap(int,int);
void continue_leap(void);
static int events[SIL_ACH_COUNT], question_choice=-1, actor_result=-1;
static int roll_number, miss_roll;
void __real_sil_achievements_on_event(sil_achievement_id);
void __wrap_sil_achievements_on_event(sil_achievement_id id)
{ assert(id>=0 && id<SIL_ACH_COUNT); ++events[id]; __real_sil_achievements_on_event(id); }
void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
void __wrap_message_flush(void) {}
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
void __wrap_pause_with_text(const char d[][100],int r,int c,const char e[][100],byte a)
{ (void)d;(void)r;(void)c;(void)e;(void)a; }
void __wrap_quest_typewriter_menu_pages(cptr t,cptr texts[],int n,byte a,byte b,int p)
{ (void)t;(void)texts;(void)n;(void)a;(void)b;(void)p; }
int __wrap_ui_question_ask(cptr t,cptr d,const ui_question_option* o,int n,int y,int x,int initial)
{ (void)t;(void)d;(void)o;(void)n;(void)y;(void)x;(void)initial;return question_choice; }
bool __wrap_get_aim_dir(int* dir,int range) { (void)range;*dir=5;return true; }
int __wrap_show_interaction_skill_roll_animation_actor(monster_type* m,cptr t,cptr a,int y,int x,int s,int d,skill_roll_details* r)
{ (void)m;(void)t;(void)a;(void)y;(void)x;(void)s;(void)d;(void)r;return actor_result; }
int __wrap_hit_roll(int a,int e,const monster_type* attacker,const monster_type* defender,bool show)
{ (void)a;(void)e;(void)attacker;(void)defender;(void)show;return ++roll_number==miss_roll ? -100 : 100; }
int __wrap_hit_roll_details(int a,int e,const monster_type* attacker,const monster_type* defender,bool show,int* ad,int* ed)
{ if(ad)*ad=10;if(ed)*ed=1;return __wrap_hit_roll(a,e,attacker,defender,show); }
static bool earned(int id) { return sil_achievement_unlocked_at(id)!=0; }
static monster_type* prepare(int race)
{
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    monster_type* m=combat_fixture(race,0);
    p_ptr->depth=5;p_ptr->game_type=0;
    p_ptr->food=PY_FOOD_FULL-1000;p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->achievement_flags=SIL_ACH_RUN_STARTED;
    p_ptr->achievement_fall_depth=-1;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=p_ptr->stat_use[i]=10;
    for(int i=0;i<S_MAX;i++)p_ptr->skill_base[i]=p_ptr->skill_use[i]=100;
    for(int y=0;y<p_ptr->cur_map_hgt;y++)for(int x=0;x<p_ptr->cur_map_wid;x++)
        cave_info[y][x]=CAVE_MARK|CAVE_FIRE|CAVE_VIEW|CAVE_SEEN;
    cave_m_idx[p_ptr->py][p_ptr->px]=-1;
    character_generated=character_dungeon=true;character_icky=true;
    memset(op_ptr->opt+OPT_CHEAT,0,(OPT_MAX-OPT_CHEAT)*sizeof(op_ptr->opt[0]));
    m->hp=1;m->maxhp=100;
    roll_number=miss_roll=0;question_choice=-1;actor_result=-1;
    return m;
}
static void item(object_type* o,int t,int s,int n)
{ object_prep(o,lookup_kind(t,s));assert(o->k_idx);o->number=n;o->ident|=IDENT_KNOWN; }
static void ability(int skill,int ability)
{ p_ptr->innate_ability[skill][ability]=p_ptr->have_ability[skill][ability]=p_ptr->active_ability[skill][ability]=true; }
static void check_thrall(void)
{
    monster_type* m=prepare(R_IDX_ALERT_HUMAN_THRALL);m->thrall_quest_item=THRALL_QUEST_DAGGER;
    item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    complete_thrall_quest(m,INVEN_WIELD);assert(!events[SIL_ACH_NO_RINGS_ATTACHED]);
    item(&inventory[INVEN_WIELD],TV_SWORD,SV_DAGGER,1);p_ptr->equip_cnt=1;
    complete_thrall_quest(m,INVEN_WIELD);
    assert(events[SIL_ACH_NO_RINGS_ATTACHED]==1 && earned(SIL_ACH_NO_RINGS_ATTACHED));
    assert(m->thrall_quest_completed==THRALL_QUEST_STATE_REWARD_PENDING);
    puts("PASS thrall: wrong item excluded; requested item consumed and event before deferred reward");
}
static void check_traps(void)
{
    monster_type* m=prepare(41);int idx=(int)(m-mon_list);
    cave_feat[10][10]=FEAT_TRAP_DEADFALL;cave_rewired[10][10]=0;
    m_engage_rewired_trap(10,10);assert(m->hp==1 && !events[SIL_ACH_OFT_EVIL_WILL]);
    cave_rewired[10][10]=100;actor_result=-1;m->hp=30000;
    m_engage_rewired_trap(10,10);assert(m->hp<30000 && !events[SIL_ACH_OFT_EVIL_WILL]);
    m=prepare(R_IDX_MORGOTH);cave_feat[10][10]=FEAT_TRAP_DEADFALL;cave_rewired[10][10]=100;
    m_engage_rewired_trap(10,10);assert(m->r_idx && p_ptr->morgoth_second_wind && !events[SIL_ACH_OFT_EVIL_WILL]);
    m=prepare(41);idx=(int)(m-mon_list);cave_feat[10][10]=FEAT_TRAP_DEADFALL;cave_rewired[10][10]=100;
    m_engage_rewired_trap(10,10);assert(!mon_list[idx].r_idx && events[SIL_ACH_OFT_EVIL_WILL]==1);
    m=prepare(41);m->ml=false;actor_result=100;
    r_info[41].flags2&=~RF2_MINDLESS;
    /* Unseen uses the real skill check, so make success certain. */
    m->hp=30000;cave_feat[10][10]=FEAT_TRAP_DART;cave_rewired[10][10]=1;
    r_info[41].per=100;
    m_engage_rewired_trap(10,10);assert(!events[SIL_ACH_ANGBAND_MAINTENANCE]);
    m=prepare(41);actor_result=100;cave_feat[10][10]=FEAT_TRAP_DART;cave_rewired[10][10]=1;
    m_engage_rewired_trap(10,10);assert(events[SIL_ACH_ANGBAND_MAINTENANCE]==1 && !cave_rewired[10][10]);
    puts("PASS rewired traps: unwired/nonlethal/Morgoth transition excluded; kill and visible dismantle delivered; unseen excluded");
}
static void check_potions(void)
{
    object_type potion;monster_type* m=prepare(41);r_info[41].flags3&=~(RF3_RES_POIS|RF3_RES_FIRE);
    int idx=(int)(m-mon_list);
    monster_poison_add_from(idx,10,-1);assert(monster_poison_tick(idx));
    assert(!events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    m=prepare(41);idx=(int)(m-mon_list);monster_poison_add(idx,10);assert(monster_poison_tick(idx));
    assert(!events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    m=prepare(41);idx=(int)(m-mon_list);monster_poison_add_from(idx,10,2);assert(monster_poison_tick(idx));
    assert(!events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    m=prepare(R_IDX_MORGOTH);item(&potion,TV_POTION,SV_POTION_ORCISH_LIQUOR,1);
    assert(fixture_potion(&potion));assert(m->r_idx && !events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    m=prepare(41);m->hp=30000;item(&potion,TV_POTION,SV_POTION_ORCISH_LIQUOR,1);
    assert(fixture_potion(&potion));assert(m->hp>0 && !events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    m=prepare(41);idx=(int)(m-mon_list);item(&potion,TV_POTION,SV_POTION_POISON,1);
    assert(fixture_potion(&potion));assert(m->potion_poisoned>0 && !events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]);
    assert(monster_poison_tick(idx));assert(events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]==1 && earned(SIL_ACH_FOR_EXTERNAL_USE_ONLY));
    m=prepare(41);idx=(int)(m-mon_list);item(&potion,TV_POTION,SV_POTION_ORCISH_LIQUOR,1);
    assert(fixture_potion(&potion));assert(!mon_list[idx].r_idx && events[SIL_ACH_FOR_EXTERNAL_USE_ONLY]==2);
    puts("PASS thrown potions: real fire burst and delayed potion poison; ordinary/ambient poison and phase transitions excluded");
}
static void check_herbs(void)
{
    object_type herb;prepare(41);item(&herb,TV_FOOD,SV_FOOD_SICKNESS,1);
    assert(fixture_food(&herb));assert(p_ptr->diseased && !events[SIL_ACH_ATHELAS_OUT_OF_STOCK]);
    p_ptr->disease_cure=SV_FOOD_RAGE;item(&herb,TV_FOOD,SV_FOOD_RAGE,1);
    assert(fixture_food(&herb));assert(!p_ptr->diseased && !events[SIL_ACH_ATHELAS_OUT_OF_STOCK]);
    p_ptr->diseased=100;p_ptr->disease_cure=SV_FOOD_SICKNESS;
    item(&herb,TV_FOOD,SV_FOOD_SICKNESS,1);assert(fixture_food(&herb));
    assert(!p_ptr->diseased && events[SIL_ACH_ATHELAS_OUT_OF_STOCK]==1);
    puts("PASS herbs: healthy sickness and another correct herb excluded; correct Herb of Sickness cure delivered");
}
static void check_knockback(void)
{
    monster_type* m=prepare(41);m->hp=30000;int idx=(int)(m-mon_list);
    cave_feat[10][9]=FEAT_CHASM;r_info[41].flags2|=RF2_FLYING;
    assert(knock_back(10,11,10,10));assert(mon_list[idx].r_idx && !events[SIL_ACH_FLY_YOU_FOOL]);
    m=prepare(41);m->hp=30000;r_info[41].flags2&=~RF2_FLYING;cave_feat[10][9]=FEAT_CHASM;
    assert(knock_back(10,12,10,10));assert(!events[SIL_ACH_FLY_YOU_FOOL]);
    m=prepare(41);m->hp=30000;idx=(int)(m-mon_list);cave_feat[10][9]=FEAT_CHASM;
    assert(knock_back(10,11,10,10));assert(!mon_list[idx].r_idx && events[SIL_ACH_FLY_YOU_FOOL]==1);
    assert(!p_ptr->achievement_kills); /* A surviving fall is no kill. */
    m=prepare(41);cave_feat[10][9]=FEAT_CHASM;
    assert(knock_back(10,11,10,10));assert(p_ptr->achievement_kills==1);
    m=prepare(41);cave_feat[10][9]=FEAT_CHASM;
    assert(knock_back(10,12,10,10));assert(!p_ptr->achievement_kills);
    puts("PASS knockback: player pushes surviving nonflyer into chasm; flying victim and nonplayer source excluded");
}
static void check_leap_fall(void)
{
    prepare(41);delete_monster_idx(cave_m_idx[10][10]);p_ptr->leaping=true;
    p_ptr->previous_action[1]=6;cave_feat[10][11]=FEAT_CHASM;cave_set_feat(10,12,FEAT_WALL_SOLID);
    continue_leap();assert(!events[SIL_ACH_CABED_EN_ARAS]);
    prepare(41);p_ptr->leaping=true;p_ptr->previous_action[1]=6;cave_feat[10][11]=FEAT_CHASM;
    cave_set_feat(10,12,FEAT_CHASM);continue_leap();assert(!events[SIL_ACH_CABED_EN_ARAS]);
    prepare(41);p_ptr->leaping=true;p_ptr->previous_action[1]=6;cave_feat[10][11]=FEAT_CHASM;
    current_build_vault_exact_token=true;assert(place_monster_one(10,12,41,false,true,NULL));
    current_build_vault_exact_token=false;continue_leap();assert(!events[SIL_ACH_CABED_EN_ARAS]);
    prepare(41);p_ptr->leaping=true;p_ptr->previous_action[1]=6;cave_feat[10][11]=FEAT_CHASM;
    cave_set_feat(10,12,FEAT_TRAP_PIT);p_ptr->chp=1;continue_leap();
    assert(p_ptr->is_dead && !events[SIL_ACH_CABED_EN_ARAS]);
    prepare(41);p_ptr->leaping=true;p_ptr->previous_action[1]=6;cave_feat[10][11]=FEAT_CHASM;
    continue_leap();assert(events[SIL_ACH_CABED_EN_ARAS]==1 && p_ptr->px==12);
    prepare(41);p_ptr->depth=5;hit_trap(10,11);assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
    sil_achievements_on_level_entry(false);assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
    cave_feat[10][11]=FEAT_TRAP_false_FLOOR;hit_trap(10,11);
    assert(p_ptr->depth==6 && (p_ptr->achievement_flags&SIL_ACH_RUN_FALLING));
    p_ptr->depth=5;sil_achievements_on_level_entry(false);
    assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN) && !(p_ptr->achievement_flags&SIL_ACH_RUN_FALLING));
    hit_trap(10,11);assert(p_ptr->depth==6);
    p_ptr->is_dead=true;sil_achievements_on_level_entry(false);assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
    prepare(41);cave_feat[10][11]=FEAT_CHASM;hit_trap(10,11);
    assert(p_ptr->depth==7 && !earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
    p_ptr->leaving=false;sil_achievements_on_level_entry(false);assert(earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
    puts("PASS leap/fall: blocked/chasm/lethal landings excluded, safe leap delivered; arrival needed, death excluded; survived chasm arrival delivered");
}
static void check_doors(void)
{
    prepare(41);delete_monster_idx(cave_m_idx[10][10]);lockpick_minigame=true;question_choice=0;
    cave_set_feat(10,10,FEAT_DOOR_HEAD+0x08);(void)fixture_bash(10,10);
    assert(cave_feat[10][10]==FEAT_OPEN || cave_feat[10][10]==FEAT_BROKEN);
    assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
    prepare(41);delete_monster_idx(cave_m_idx[10][10]);question_choice=0;
    cave_set_feat(10,10,FEAT_DOOR_HEAD+1);p_ptr->skill_use[S_PER]=-1000;
    for(int i=0;i<100 && cave_feat[10][10]<FEAT_DOOR_HEAD+8;i++)do_cmd_open_aux(10,10);
    assert(cave_feat[10][10]>=FEAT_DOOR_HEAD+8);
    int index=10*MAX_DUNGEON_WID+10;
    assert(p_ptr->achievement_jammed_doors[index/8]&(1u<<(index%8)));
    cave_set_feat(11,11,FEAT_DOOR_HEAD+0x08);(void)fixture_bash(11,11);
    assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
    assert(p_ptr->achievement_jammed_doors[index/8]&(1u<<(index%8)));
    p_ptr->restoring=true;cave_set_feat(10,10,FEAT_DOOR_HEAD+0x0a);
    assert(p_ptr->achievement_jammed_doors[index/8]&(1u<<(index%8)));
    p_ptr->restoring=false;cave_set_feat(10,10,FEAT_FLOOR);
    assert(!(p_ptr->achievement_jammed_doors[index/8]&(1u<<(index%8))));
    cave_set_feat(10,10,FEAT_DOOR_HEAD+1);
    for(int i=0;i<100 && cave_feat[10][10]<FEAT_DOOR_HEAD+8;i++)do_cmd_open_aux(10,10);
    assert(cave_feat[10][10]>=FEAT_DOOR_HEAD+8);p_ptr->stat_use[A_STR]=1000;
    do_cmd_open_aux(10,10);assert(earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
    assert(cave_feat[10][10]==FEAT_OPEN || cave_feat[10][10]==FEAT_BROKEN);
    puts("PASS doors: generated jam excluded; real lockpick jam provenance, restored terrain preservation, replacement clearing and same-door bash");
}
static void check_melee(void)
{
    monster_type* m=prepare(41);item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    p_ptr->mdd=10;p_ptr->mds=10;p_ptr->equip_cnt=1;
    py_attack_aux(10,10,ATT_MAIN);assert(!m->r_idx && !events[SIL_ACH_THE_HASTY_STROKE]);
    m=prepare(41);item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    p_ptr->mdd=10;p_ptr->mds=10;p_ptr->equip_cnt=1;
    m=prepare(R_IDX_MORGOTH);item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    p_ptr->mdd=10;p_ptr->mds=10;p_ptr->equip_cnt=1;
    py_attack_aux(10,10,ATT_RIPOSTE);assert(m->r_idx && p_ptr->morgoth_second_wind && !events[SIL_ACH_THE_HASTY_STROKE]);
    m=prepare(41);item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);
    p_ptr->mdd=10;p_ptr->mds=10;p_ptr->equip_cnt=1;
    py_attack_aux(10,10,ATT_RIPOSTE);assert(!m->r_idx && events[SIL_ACH_THE_HASTY_STROKE]==1);
    puts("PASS Riposte: real ATT_RIPOSTE kill delivered; ordinary melee kill excluded");
}
static void ranged_setup(bool hail)
{
    monster_type* m=prepare(41);item(&inventory[INVEN_BOW],TV_BOW,SV_SHORT_BOW,1);
    object_type arrow;item(&arrow,TV_ARROW,SV_NORMAL_ARROW,10);assert(player_quiver_absorb_arrow(&arrow)==10);
    p_ptr->equip_cnt=1;p_ptr->ammo_tval=TV_ARROW;p_ptr->add=5;p_ptr->ads=20;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
    ability(S_ARC,ARC_DEADLY_HAIL);p_ptr->killed_enemy_with_arrow=hail;
    target_set_monster((int)(m-mon_list));
}
static void check_ranged(void)
{
    ranged_setup(false);do_cmd_fire(1);assert(!cave_m_idx[10][10] && !events[SIL_ACH_A_FEW_LEAVES]);
    ranged_setup(true);mon_list[cave_m_idx[10][10]].hp=30000;do_cmd_fire(1);
    assert(cave_m_idx[10][10] && !events[SIL_ACH_A_FEW_LEAVES]);
    ranged_setup(true);do_cmd_fire(1);assert(!cave_m_idx[10][10] && events[SIL_ACH_A_FEW_LEAVES]==1);
    puts("PASS Deadly Hail: empowered real arrow kill delivered; ordinary arrow kill excluded");
    for(int miss=2;miss>=0;miss--) {
        monster_type* m=prepare(81);
        ability(S_MEL,MEL_THROWING);ability(S_MEL,MEL_POWER_THROW);
        item(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD,1);p_ptr->equip_cnt=1;
        object_type spear;item(&spear,TV_POLEARM,SV_SPEAR,3);assert(player_carried_extra_load(&spear));
        p_ptr->previous_action[1]=5;p_ptr->focused=false;p_ptr->mdd=10;p_ptr->mds=20;
        target_set_monster((int)(m-mon_list));player_active_weapon_begin_player_turn();
        miss_roll=miss;do_cmd_throw(false);assert(roll_number==2 && !m->r_idx);
        assert(events[SIL_ACH_TWO_WEAPONS]==(miss?0:1));
    }
    puts("PASS Power Throw: both-hit kill delivered; either single component missing excluded");
}
static void checks(void)
{
    assert(init_flavor_info()==0);flavor_init();
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;sil_achievements_init();
    if(getenv("SIL_FIXTURE_FALSE_FLOOR")) {
        prepare(41);cave_feat[10][11]=FEAT_TRAP_false_FLOOR;hit_trap(10,11);
        assert(p_ptr->depth==6 && !earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        p_ptr->leaving=false;sil_achievements_on_level_entry(false);
        assert(earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        puts("PASS false floor: real fall stays pending until successful living arrival");return;
    }
    check_thrall();check_traps();check_potions();check_herbs();check_knockback();check_leap_fall();check_doors();check_melee();check_ranged();
    for(int id=SIL_ACH_NO_RINGS_ATTACHED;id<SIL_ACH_THE_ROAD_GOES_EVER_ON;id++)assert(earned(id));
    puts("PASS all twelve expansion achievements earned through production routes");
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
    source.write_text(prefix + CHECKS + init
                      + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    helpers = []
    for name, content in HELPERS.items():
        path = OUT / (name + ".c")
        path.write_text(content, encoding="utf-8")
        helpers.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    # Compile changed event sources directly, so stale production objects cannot
    # accidentally make an event assertion pass against a different revision.
    units = ("cmd/combat/cmd-combat", "monster/monster-move", "monster/monster-poison",
             "dungeon/dungeon-player", "cave/cave-flow", "achievements")
    excluded = tuple("/src/" + unit + ".c.obj" for unit in units) + (
        "/src/main.c.obj", "/src/cmd/combat/cmd-ranged.c.obj", "/src/use-obj.c.obj",
        "/src/cmd/world/cmd-interact.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print",
             "message_flush", "get_sdl_config_path", "pause_with_text",
             "quest_typewriter_menu_pages", "ui_question_ask", "get_aim_dir",
             "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details")
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *helpers,
        *(str(ROOT / "src" / (unit + ".c")) for unit in units),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    # Separate profiles prove each arrival path awards a previously locked deed.
    for false_floor in (False, True):
        run_env = dict(env)
        if false_floor:
            run_env["SIL_FIXTURE_FALSE_FLOOR"] = "1"
        else:
            run_env.pop("SIL_FIXTURE_FALSE_FLOOR", None)
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as data:
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                           env=run_env, check=True, timeout=45)


if __name__ == "__main__":
    main()
