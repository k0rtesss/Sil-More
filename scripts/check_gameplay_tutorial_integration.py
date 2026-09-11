#!/usr/bin/env python3
"""Exercise real tutorial core + game action guards in an isolated fixture."""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess
import check_gameplay_tutorial as core

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-integration-check"

HARNESS = core.HARNESS.split("int main(", 1)[0] + r'''
__GAME_IMPLEMENTATION__
__RANGED_AIM_IMPLEMENTATION__
static bool eat_food(object_type *,bool *);
static bool quaff_potion(object_type *,bool *);
static bool use_staff(object_type *,bool *);
static bool use_gem(object_type *,bool *);
static bool play_instrument(object_type *,bool *);
static bool activate_object(object_type *);
__USE_OBJECT_IMPLEMENTATION__
__DESCRIPTION_IMPLEMENTATION__

static player_type test_player;
player_type *p_ptr=&test_player;
static player_other test_options;
player_other *op_ptr=&test_options;
static maxima test_maxima;
maxima *z_info=&test_maxima;
static ability_type test_abilities[1];
ability_type *b_info=test_abilities;
char *b_name="Ability";
static object_type test_floor_objects[4];
object_type *o_list=test_floor_objects;
static object_type test_inventory[INVEN_TOTAL], test_extra, test_supply;
object_type *inventory=test_inventory;
static object_kind test_kinds[8];
object_kind *k_info=test_kinds;
static monster_type test_monsters[3];
monster_type *mon_list=test_monsters;
static monster_race test_races[3];
monster_race *r_info=test_races;
static monster_lore test_lore[3];
monster_lore *l_list=test_lore;
s16b mon_max=2;
static ego_item_type test_egos[2];
ego_item_type *e_info=test_egos;
bool character_generated=true;
s32b playerturn=50;
const s16b ddx[10]={0,-1,0,1,-1,0,1,-1,0,1};
const s16b ddy[10]={0,1,1,1,0,0,0,-1,-1,-1};
static u16b test_info[12][256];
static byte test_feat[12][MAX_DUNGEON_WID];
static s16b test_mon_idx[12][MAX_DUNGEON_WID];
static s16b test_obj_idx[12][MAX_DUNGEON_WID];
u16b (*cave_info)[256]=test_info;
byte (*cave_feat)[MAX_DUNGEON_WID]=test_feat;
s16b (*cave_m_idx)[MAX_DUNGEON_WID]=test_mon_idx;
s16b (*cave_o_idx)[MAX_DUNGEON_WID]=test_obj_idx;
static bool mock_merciless, mock_cowardly;
static bool mock_broken,mock_smith_forbidden;
static int extra_count,supply_count;
static int world_start_calls;
static int active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;
static u16b test_path[4];
static int test_path_count;
static bool effect_commits=true;
static int effect_calls;
static bool pack_pending;
metarun metar;
metarun *metarun_current_mutable(void) { return &test_tale; }
errr save_metaruns(void) { return 0; }
int ui_question_ask_overlay(cptr title,cptr desc,const ui_question_option *options,
    int count,int anchor_y,int anchor_x,int default_index)
{ (void)title;(void)desc;(void)options;(void)count;(void)anchor_y;(void)anchor_x;(void)default_index;return 0; }
bool death_spectator_active(void) { return false; }
bool player_can_treat_as_throwing(const object_type *o) { (void)o; return false; }
bool player_power_throw_ready(void) { return false; }
bool object_has_broken_prefix(const object_type *item) { (void)item; return mock_broken; }
bool smith_oath_forbids_object(const object_type *item) { (void)item; return mock_smith_forbidden; }
bool player_equipment_slot_counts_as_equipped(int slot) { return slot>=INVEN_WIELD && slot<INVEN_TOTAL; }
bool merciless_attack(monster_type *m) { (void)m; return mock_merciless; }
bool cowardly_attack(monster_type *m) { (void)m; return mock_cowardly; }
int player_carried_extra_entry_count(void) { return extra_count; }
object_type *player_carried_extra_entry_at(int n) { return n==0?&test_extra:NULL; }
int supplies_entry_count(void) { return supply_count; }
object_type *supplies_entry_at(int n) { return n==0?&test_supply:NULL; }
void object_desc(char *buf,size_t size,const object_type *o,int pref,int mode)
{ (void)o;(void)pref;(void)mode; SDL_strlcpy(buf,"Known item",size); }
bool get_sdl_gameplay_tutorial_enabled(void) { return true; }
tutorial_mode get_sdl_gameplay_tutorial_mode(void) { return tutorial_get_mode(); }
int player_active_weapon_mode(void) { return 0; }
int player_active_weapon_kind(void) { return active_weapon_kind; }
int min_depth(void) { return 0; }
void tutorial_world_start(void) { ++world_start_calls; }
void tutorial_world_checkpoint(void) {}
void sdl_mouse_path_cancel(void) {}
bool player_pack_action_pending(void) { return pack_pending; }
bool player_active_weapon_is_melee(void) { return active_weapon_kind==PLAYER_ACTIVE_WEAPON_KIND_MELEE; }
int ability_index(int skill,int ability) { (void)skill;(void)ability;return 0; }
int player_quiver_arrow_slots(int *slots,int count) { (void)slots;(void)count;return 0; }
int player_quiver_selected_arrow_slot(void) { return -1; }
int player_inventory_handle_for_object(const object_type *item)
{ for(int i=0;i<INVEN_TOTAL;++i) if(item==&inventory[i]) return i;return -1; }
bool player_inventory_handle_is_equipped(int slot) { return slot>=INVEN_WIELD && slot<INVEN_TOTAL; }
enum inventory_limit_group inventory_limit_group_for_object(const object_type *item)
{ return item->storage==OBJECT_STORAGE_HARNESS?INV_LIMIT_HARNESS:INV_LIMIT_PACK; }
bool object_can_choose_pack_or_harness(const object_type *item) { (void)item;return true; }
bool inventory_type_slot_available(const object_type *item,bool record) { (void)item;(void)record;return true; }
s16b wield_slot(const object_type *item) { (void)item;return INVEN_BODY; }
void monster_desc(char *buf,size_t size,const monster_type *m,int mode)
{ (void)mode; SDL_snprintf(buf,size,"Monster %d",m->r_idx); }
void msg_print(cptr message) { (void)message; }
int project_path(u16b *path,int range,int y,int x,int *ty,int *tx,u32b flags)
{ (void)range;(void)y;(void)x;(void)ty;(void)tx;(void)flags;
  assert(test_path_count>=0 && test_path_count<=4);
  for (int i=0;i<test_path_count;++i) path[i]=test_path[i];
  return test_path_count; }
static bool mock_use_effect(object_type *item,bool *ident)
{
    (void)item; ++effect_calls;
    if (!effect_commits) return false;
    *ident=true;
    p_ptr->stat_drain[A_GRA]=0;
    return true;
}
static bool eat_food(object_type *o,bool *ident) { return mock_use_effect(o,ident); }
static bool quaff_potion(object_type *o,bool *ident) { return mock_use_effect(o,ident); }
static bool use_staff(object_type *o,bool *ident) { return mock_use_effect(o,ident); }
static bool use_gem(object_type *o,bool *ident) { return mock_use_effect(o,ident); }
static bool play_instrument(object_type *o,bool *ident) { return mock_use_effect(o,ident); }
static bool activate_object(object_type *o) { (void)o; return false; }

static void activate(const char *id,const char *type)
{
    tutorial_context context={0};
    tutorial_invalidate_context();
    ++test_tale.id;
    tutorial_sync_tale();
    SDL_strlcpy(context.subject_type,type,sizeof(context.subject_type));
    tutorial_observe(id,&context);
    tutorial_checkpoint(true);
    assert(tutorial_is_active());
}

static void activate_description(const char *id)
{
    tutorial_view view;
    object_info_overlay_clear();
    mock_description_capture=true;
    mock_description_present=true;
    mock_description_events=0;
    mock_description_keys=0;
    activate(id, "staff");
    assert(tutorial_get_view(&view));
    if (view.can_continue) { tutorial_continue(); tutorial_checkpoint(true); }
    assert(tutorial_action_waiting());
}

static void check_description_routes(const object_type *item)
{
    tutorial_view view;
    const object_type *objects[]={item};
    int energy=p_ptr->energy_use, health=p_ptr->chp, voice=p_ptr->csp;
    s32b turn_before=playerturn;
    int quantity=item->number, charges=item->pval;
    puts("Checking actual inline/modal description presentation and tutorial completion");
    fflush(stdout);

    activate_description("item.first_description");
    mock_description_capture=false;
    assert(!object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
    mock_description_capture=true; mock_description_present=false;
    assert(!object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
    mock_description_present=true;
    assert(object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_COMPLETED);

    activate_description("item.first_description");
    mock_description_capture=false;
    (void)object_info_screen_multi_with_actions(objects,NULL,1,NULL,NULL,0);
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
    mock_description_capture=true; mock_description_present=false;
    (void)object_info_screen_multi_with_actions(objects,NULL,1,NULL,NULL,0);
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
    mock_description_present=true;
    (void)object_info_screen_multi_with_actions(objects,NULL,1,NULL,NULL,0);
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_COMPLETED);
    assert(character_icky==0);

    /* Redrawing the same inline description cannot credit an unseen action.
     * The next step needs a horn, so an already visible staff cannot satisfy it. */
    activate_description("description-two-actions");
    assert(object_info_overlay_show_multi(objects,NULL,1));
    assert(object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_lesson_status("description-two-actions")==TUTORIAL_IN_PROGRESS);
    tutorial_game_wait();
    assert(tutorial_get_view(&view) && view.step==2 && !strcmp(view.action_subject,"horn"));
    assert(object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_lesson_status("description-two-actions")==TUTORIAL_IN_PROGRESS);
    object_type horn=*item; horn.tval=TV_HORN;
    const object_type *horns[]={&horn};
    assert(object_info_overlay_show_multi(horns,NULL,1));
    assert(tutorial_lesson_status("description-two-actions")==TUTORIAL_COMPLETED);

    activate_description("description-two-actions");
    mock_description_keys=2; /* Scroll/repaint before closing the same modal. */
    (void)object_info_screen_multi_with_actions(objects,NULL,1,NULL,NULL,0);
    assert(tutorial_lesson_status("description-two-actions")==TUTORIAL_IN_PROGRESS);
    tutorial_game_wait();
    assert(tutorial_get_view(&view) && view.step==2);

    /* The player had already opened the matching preview while reading info. */
    object_info_overlay_clear();
    activate("item.first_description","staff");
    assert(object_info_overlay_show_multi(objects,NULL,1));
    assert(tutorial_get_view(&view) && view.can_continue && view.step==1);
    tutorial_game_wait(); /* Mock frontend sends Continue; real wait resolves visible preview. */
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_COMPLETED);
    assert(mock_description_events>0);

    /* Closing it before Continue must leave the examine step waiting. */
    object_info_overlay_clear();
    activate("item.first_description","staff");
    assert(object_info_overlay_show_multi(objects,NULL,1));
    object_info_overlay_clear();
    tutorial_game_wait();
    assert(tutorial_get_view(&view) && !view.can_continue && view.step==2);
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
    object_info_overlay_clear();
    assert(p_ptr->energy_use==energy && p_ptr->chp==health && p_ptr->csp==voice);
    assert(playerturn==turn_before && item->number==quantity && item->pval==charges);
    assert(character_icky==0);
}

static void check_feature_tips(void)
{
    tutorial_view view;
    int energy=p_ptr->energy_use, voice=p_ptr->csp;
    s32b turn_before=playerturn;
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    for (int i=0;i<2;++i) {
        object_type feature={.k_idx=6+i,.tval=i?TV_CHEST:TV_SKELETON,.number=1,.pval=1};
        const char *id=i?"world.chest":"world.skeleton";
        tutorial_invalidate_context(); ++test_tale.id; tutorial_sync_tale();
        reached_kinds[feature.k_idx]=false;
        tutorial_game_item(&feature);
        assert(queue_count==1 && !reached_kinds[feature.k_idx]);
        assert(!strcmp(catalogue[queue[0].lesson].id,id));
        assert(tutorial_lesson_status("item.first_description")==TUTORIAL_UNSEEN);
        assert(tutorial_lesson_status("item.description")==TUTORIAL_UNSEEN);
        tutorial_checkpoint(true);
        assert(tutorial_get_view(&view) && view.can_continue && !strcmp(view.id,id));
        tutorial_continue(); assert(tutorial_lesson_status(id)==TUTORIAL_COMPLETED);
        activate_description("item.first_description");
        tutorial_game_item_described(&feature);
        tutorial_game_identified(&feature,"identification.item");
        assert(!reached_kinds[feature.k_idx]);
        assert(tutorial_lesson_status("item.first_description")==TUTORIAL_IN_PROGRESS);
        assert(tutorial_lesson_status("item.description")==TUTORIAL_UNSEEN);
        assert(queue_count==1 && !strcmp(catalogue[queue[0].lesson].id,id));
    }
    assert(p_ptr->energy_use==energy && p_ptr->csp==voice && playerturn==turn_before);
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
}

static bool pending_lesson(const char *id)
{
    int index=lesson_index(id);
    if(active==index && index>=0) return true;
    for(int i=0;i<queue_count;++i) if(queue[i].lesson==index) return true;
    return false;
}

static void live_checkpoint(void)
{
    waiting=true; /* Inspect offers without a simulated frontend reading them. */
    tutorial_game_checkpoint();
    waiting=false;
}

static void check_live_checkpoints(void)
{
    tutorial_view view;
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,sizeof(test_inventory));
    memset(mon_list,0,sizeof(test_monsters));
    memset(cave_m_idx,0,sizeof(test_mon_idx));
    memset(cave_o_idx,0,sizeof(test_obj_idx));
    memset(cave_info,0,sizeof(test_info));
    p_ptr->playing=true; p_ptr->py=p_ptr->px=5;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=12;
    p_ptr->chp=p_ptr->mhp=20; p_ptr->csp=p_ptr->msp=20;
    p_ptr->food=PY_FOOD_ALERT; op_ptr->hitpoint_warn=3;
    playerturn=50; started=false; ++test_tale.id;
    tutorial_game_start();

    /* A condition queued during another lesson expires when the condition ends. */
    p_ptr->poisoned=5; live_checkpoint();
    assert(pending_lesson("status.poisoned"));
    p_ptr->poisoned=0; live_checkpoint();
    assert(!pending_lesson("status.poisoned"));
    p_ptr->stun=55; live_checkpoint();
    assert(pending_lesson("status.heavy_stun"));
    p_ptr->stun=0; live_checkpoint();
    assert(!pending_lesson("status.heavy_stun"));
    p_ptr->chp=3; live_checkpoint(); assert(pending_lesson("status.health"));
    p_ptr->chp=20; live_checkpoint(); assert(!pending_lesson("status.health"));
    p_ptr->csp=0; live_checkpoint(); assert(pending_lesson("status.voice"));
    p_ptr->csp=20; live_checkpoint(); assert(!pending_lesson("status.voice"));
    p_ptr->food=PY_FOOD_STARVE-1; live_checkpoint(); assert(pending_lesson("status.starving"));
    p_ptr->food=PY_FOOD_WEAK+1; live_checkpoint();
    assert(!pending_lesson("status.starving") && pending_lesson("status.hungry"));
    p_ptr->food=PY_FOOD_ALERT; live_checkpoint(); assert(!pending_lesson("status.hungry"));

    /* Merely sensed/distant terrain is not a reached hazard. Transformation
     * removes the old lesson and offers the new physical feature. */
    cave_feat[5][6]=FEAT_LAVA; cave_info[5][6]=CAVE_MARK;
    live_checkpoint(); assert(!pending_lesson("world.lava"));
    cave_info[5][6]|=CAVE_SEEN;
    live_checkpoint(); assert(pending_lesson("world.lava"));
    cave_feat[5][6]=FEAT_ICE;
    live_checkpoint(); assert(!pending_lesson("world.lava") && pending_lesson("world.ice"));
    cave_feat[5][6]=FEAT_FLOOR;
    live_checkpoint(); assert(!pending_lesson("world.ice"));
    test_floor_objects[1]=(object_type){.k_idx=1,.tval=TV_STAFF,.number=1,.marked=true};
    cave_o_idx[5][6]=1;
    live_checkpoint(); assert(pending_lesson("item.first_description"));
    cave_o_idx[5][6]=0;
    live_checkpoint(); assert(!pending_lesson("item.first_description"));
    assert(terrain_lesson_feature(7)==6 && terrain_lesson_feature(8)==6);
    assert(terrain_lesson_feature(32)==32 && terrain_lesson_feature(39)==33);
    assert(terrain_lesson_feature(47)==40);
    assert(terrain_lesson_feature(64)==64 && terrain_lesson_feature(69)==65);
    assert(terrain_lesson_feature(70)==70 && terrain_lesson_feature(75)==71);
    assert(terrain_lesson_feature(76)==76 && terrain_lesson_feature(79)==77);

    /* Adjacent and alert enemies get awareness before attack, never a stealth
     * suggestion. A pacifist still learns what a visible enemy means. */
    active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_MELEE;
    mon_list[1]=(monster_type){.r_idx=1,.ml=true,.fy=5,.fx=6,.alertness=ALERTNESS_ALERT};
    cave_m_idx[5][6]=1; cave_info[5][6]|=CAVE_VIEW;
    live_checkpoint();
    assert(tutorial_get_view(&view) && !strcmp(view.id,"combat.first_monster"));
    assert(!pending_lesson("combat.stealth"));
    tutorial_continue(); live_checkpoint();
    assert(tutorial_get_view(&view) && !strcmp(view.id,"combat.first_adjacent"));
    tutorial_skip();
    cave_m_idx[5][6]=0; mon_list[1].fx=7;
    cave_m_idx[5][7]=1; cave_info[5][7]=CAVE_VIEW;
    mon_list[1].alertness=ALERTNESS_UNWARY;
    p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
    live_checkpoint(); assert(pending_lesson("combat.stealth"));
    mon_list[1].alertness=ALERTNESS_ALERT;
    live_checkpoint(); assert(!pending_lesson("combat.stealth"));
    mon_list[1].ml=false;
    live_checkpoint(); assert(!pending_lesson("combat.first_adjacent"));

    /* Returning to gameplay ends menu-only context; forced movement must not
     * satisfy a practice move and a same-depth regenerated map drops subjects. */
    tutorial_game_menu("inventory","Inventory fixture");
    assert(pending_lesson("menu.inventory"));
    live_checkpoint(); assert(!pending_lesson("menu.inventory"));
    tutorial_observe("move",NULL); tutorial_checkpoint(true);
    tutorial_game_menu("inventory","Inventory fixture");
    mock_description_events=0;
    tutorial_game_wait();
    assert(tutorial_lesson_status("menu.inventory")==TUTORIAL_COMPLETED);
    assert(tutorial_get_view(&view) && !strcmp(view.action,"move"));
    ++p_ptr->px; live_checkpoint();
    assert(tutorial_lesson_status("move")==TUTORIAL_IN_PROGRESS);
    tutorial_game_start();
    assert(level_changed && !tutorial_is_active());
    live_checkpoint();
    assert(!level_changed && tutorial_lesson_status("move")==TUTORIAL_IN_PROGRESS);
    puts("Live tutorial checkpoints: expiry, hazards, awareness/stealth scenarios, mode-safe actions and same-depth map refresh: PASS");
}

int main(void)
{
    tutorial_view view;
    object_type staff={.k_idx=1,.tval=TV_STAFF,.sval=SV_STAFF_SLUMBER,.number=1,.pval=CHANNELING_CHARGE_MULTIPLIER,.ident=IDENT_KNOWN};
    object_type horn={.k_idx=4,.tval=TV_HORN,.sval=SV_HORN_TERROR,.number=1};
    object_type potion={.k_idx=2,.tval=TV_POTION,.sval=SV_POTION_ANTIDOTE,.number=1};
    object_type healing={.k_idx=3,.tval=TV_POTION,.sval=SV_POTION_HEALING,.number=1};
    assert(SDL_Init(0));
    p_ptr->playing=true; p_ptr->cur_map_hgt=12; p_ptr->cur_map_wid=12;
    p_ptr->py=5; p_ptr->px=5;
    for (int y=0;y<12;++y) for (int x=0;x<12;++x) cave_feat[y][x]=FEAT_FLOOR;
    started=true;
    test_tale.id=200;
    assert(tutorial_load_catalogue("catalogue.json"));
    tutorial_sync_tale();

    mon_list[1]=(monster_type){.r_idx=1,.ml=true,.fy=5,.fx=6,.alertness=ALERTNESS_ALERT};
    cave_m_idx[5][6]=1; cave_info[5][6]=CAVE_VIEW;
    k_info[1].aware=true;

    activate("item.staff.use","staff");
    assert(tutorial_game_action_allowed("use-item",&staff));
    assert(!tutorial_game_action_allowed("use-item",&potion));
    assert(!tutorial_game_action_allowed("drop",&staff));
    assert(tutorial_game_action_allowed("open-menu",NULL));
    assert(tutorial_game_action_allowed("examine",&staff));
    k_info[1].aware=false; assert(!tutorial_game_action_allowed("use-item",&staff)); k_info[1].aware=true;
    staff.ident=0; assert(!tutorial_game_action_allowed("use-item",&staff)); staff.ident=IDENT_KNOWN;
    staff.pval=0; assert(!tutorial_game_action_allowed("use-item",&staff)); staff.pval=CHANNELING_CHARGE_MULTIPLIER;
    l_list[1].flags3=RF3_NO_SLEEP;
    assert(!tutorial_game_action_allowed("use-item",&staff)); l_list[1].flags3=0;
    mon_list[1].alertness=ALERTNESS_UNWARY-1;
    assert(!tutorial_game_action_allowed("use-item",&staff)); mon_list[1].alertness=ALERTNESS_ALERT;
    r_info[1].flags1=RF1_PEACEFUL;
    assert(!tutorial_game_action_allowed("use-item",&staff)); r_info[1].flags1=0;
    staff.sval=SV_STAFF_SELF_KNOWLEDGE;
    mon_list[1].ml=false; assert(tutorial_game_action_allowed("use-item",&staff));
    p_ptr->truce=true; assert(!tutorial_game_action_allowed("use-item",&staff)); p_ptr->truce=false;
    mock_broken=true; assert(!tutorial_game_action_allowed("use-item",&staff)); mock_broken=false;
    staff.sval=SV_STAFF_SLUMBER; mon_list[1].ml=true;

    activate("equip-test","");
    assert(tutorial_game_action_allowed("equip",&staff));
    mock_smith_forbidden=true; assert(!tutorial_game_action_allowed("equip",&staff));
    activate("change-test","");
    assert(!tutorial_game_action_allowed("change-active",&staff));
    mock_smith_forbidden=false; assert(tutorial_game_action_allowed("change-active",&staff));
    object_type armour={.k_idx=7,.tval=TV_SOFT_ARMOR,.number=1,.ident=IDENT_KNOWN};
    activate("item.armour.equip","armour");
    assert(tutorial_game_action_allowed("equip",&armour));
    armour.ident=0; assert(!tutorial_game_action_allowed("equip",&armour)); armour.ident=IDENT_KNOWN;
    armour.ident|=IDENT_CURSED; assert(!tutorial_game_action_allowed("equip",&armour)); armour.ident=IDENT_KNOWN;
    inventory[INVEN_BODY]=armour;
    assert(!tutorial_game_action_allowed("equip",&armour)); memset(&inventory[INVEN_BODY],0,sizeof(armour));

    activate("item.horn.use","horn");
    k_info[4].aware=true; p_ptr->csp=20;
    assert(tutorial_game_action_allowed("use-item",&horn));
    p_ptr->csp=19; assert(!tutorial_game_action_allowed("use-item",&horn));
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=true; p_ptr->csp=10;
    assert(tutorial_game_action_allowed("use-item",&horn));
    p_ptr->csp=9; assert(!tutorial_game_action_allowed("use-item",&horn));
    p_ptr->csp=20; p_ptr->active_ability[S_WIL][WIL_CHANNELING]=false;
    l_list[1].flags3=RF3_NO_FEAR;
    assert(!tutorial_game_action_allowed("use-item",&horn)); l_list[1].flags3=0;
    horn.sval=SV_HORN_WARNING;
    assert(!tutorial_game_action_allowed("use-item",&horn)); horn.sval=SV_HORN_TERROR;
    assert(tutorial_game_target_allowed(5,6));
    assert(!tutorial_game_target_allowed(5,7));
    r_info[1].flags1=RF1_PEACEFUL;
    assert(!tutorial_game_target_allowed(5,6)); r_info[1].flags1=0;
    mon_list[1].ml=false; assert(!tutorial_game_target_allowed(5,6)); mon_list[1].ml=true;
    cave_m_idx[5][6]=0;

    activate("status.poisoned.remedy","poisoned");
    p_ptr->poisoned=10;
    assert(!tutorial_game_action_allowed("use-item",&potion));
    k_info[2].aware=true;
    assert(tutorial_game_action_allowed("use-item",&potion));
    k_info[3].aware=true;
    assert(!tutorial_game_action_allowed("use-item",&healing));
    assert(tutorial_game_action_allowed("examine",&healing));
    p_ptr->poisoned=0;
    assert(!tutorial_game_action_allowed("use-item",&potion));
    p_ptr->poisoned=10;
    assert(!available_remedy("poisoned"));
    test_supply=potion; supply_count=1;
    assert(available_remedy("poisoned"));
    p_ptr->entranced=1; assert(!available_remedy("poisoned")); p_ptr->entranced=0;
    p_ptr->stun=101; assert(!available_remedy("poisoned")); p_ptr->stun=0;
    supply_count=0; test_extra=potion; extra_count=1;
    assert(available_remedy("poisoned")); extra_count=0;

    {
        bool identified=false;
        object_type miruvor={.k_idx=5,.tval=TV_POTION,.sval=SV_POTION_MIRUVOR,.number=1};
        object_type herb={.k_idx=6,.tval=TV_FOOD,.sval=SV_FOOD_HEALING,.number=1};
        activate("status.diseased.remedy","diseased");
        p_ptr->diseased=50; k_info[5].aware=true; k_info[6].aware=true;
        k_info[3].aware=false;
        assert(!tutorial_game_action_allowed("use-item",&healing));
        k_info[3].aware=true;
        assert(tutorial_game_action_allowed("use-item",&healing));
        assert(tutorial_game_action_allowed("use-item",&miruvor));
        assert(!tutorial_game_action_allowed("use-item",&potion));
        assert(!tutorial_game_action_allowed("use-item",&herb));
        herb.sval=SV_FOOD_RESTORATION;
        assert(!tutorial_game_action_allowed("use-item",&herb));
        assert(!available_remedy("diseased"));
        test_supply=miruvor; supply_count=1;
        assert(available_remedy("diseased"));
        p_ptr->entranced=1; assert(!available_remedy("diseased")); p_ptr->entranced=0;
        p_ptr->stun=101; assert(!available_remedy("diseased")); p_ptr->stun=0;
        supply_count=0; test_extra=healing; extra_count=1;
        assert(available_remedy("diseased")); extra_count=0;
        effect_calls=0; effect_commits=false;
        assert(!use_object(&healing,&identified) && effect_calls==1);
        assert(tutorial_lesson_status("status.diseased.remedy")==TUTORIAL_IN_PROGRESS);
        effect_commits=true;
        assert(use_object(&healing,&identified) && identified);
        assert(tutorial_lesson_status("status.diseased.remedy")==TUTORIAL_COMPLETED);
        p_ptr->diseased=0;
    }

    {
        bool identified=false;
        object_type grace={.k_idx=5,.tval=TV_POTION,.sval=SV_POTION_GRA,.number=1};
        object_type restoration={.k_idx=6,.tval=TV_FOOD,.sval=SV_FOOD_RESTORATION,.number=1};
        activate("status.drain.remedy","drain");
        p_ptr->stat_drain[A_GRA]=-3; k_info[5].aware=false; effect_calls=0;
        assert(!use_object(&grace,&identified) && effect_calls==0);
        assert(!use_object(&healing,&identified) && effect_calls==0);
        k_info[5].aware=true; effect_commits=false;
        assert(!use_object(&grace,&identified) && effect_calls==1);
        assert(tutorial_lesson_status("status.drain.remedy")==TUTORIAL_IN_PROGRESS);
        assert(p_ptr->stat_drain[A_GRA]==-3);
        effect_commits=true;
        assert(use_object(&grace,&identified) && identified && p_ptr->stat_drain[A_GRA]==0);
        assert(!item_is_remedy(&grace,"drain")); /* Eligibility disappeared after effect. */
        assert(tutorial_lesson_status("status.drain.remedy")==TUTORIAL_COMPLETED);
        activate("status.drain.remedy","drain");
        p_ptr->stat_drain[A_GRA]=-1; k_info[6].aware=true;
        assert(use_object(&restoration,&identified) && p_ptr->stat_drain[A_GRA]==0);
        assert(tutorial_lesson_status("status.drain.remedy")==TUTORIAL_COMPLETED);
    }

    activate("move","");
    cave_info[5][6]=CAVE_MARK;
    assert(tutorial_game_command_allowed(';',6));
    assert(tutorial_game_command_allowed(';',0)); /* Opening a direction prompt is free. */
    assert(!tutorial_game_command_allowed(';',42));
    assert(!tutorial_game_command_allowed(';',-1));
    assert(!tutorial_game_command_allowed(';',5));
    p_ptr->px=0; assert(!tutorial_game_command_allowed(';',4)); p_ptr->px=5;
    assert(!tutorial_game_command_allowed('z',0));
    cave_info[5][6]=CAVE_MARK; cave_feat[5][6]=FEAT_TRAP_HEAD;
    assert(!tutorial_game_command_allowed(';',6));
    cave_feat[5][6]=FEAT_CHASM;
    assert(!tutorial_game_command_allowed(';',6));
    const int hazard_features[]={FEAT_LAVA,FEAT_POISON,FEAT_WATER,FEAT_ICE};
    for(int i=0;i<4;++i) {
        cave_feat[5][6]=hazard_features[i];
        assert(!tutorial_game_command_allowed(';',6));
    }
    cave_info[5][6]=CAVE_WALL; cave_feat[5][6]=FEAT_WALL_EXTRA;
    assert(!tutorial_game_command_allowed(';',6));
    cave_info[5][6]=0; cave_feat[5][6]=FEAT_FLOOR;
    assert(!tutorial_game_command_allowed(';',6)); /* Unexplored is not known-safe floor. */
    cave_info[5][6]=CAVE_MARK;
    p_ptr->confused=1; assert(!tutorial_game_command_allowed(';',6)); p_ptr->confused=0;
    assert(p_ptr->energy_use==0);

    activate("attack","monster");
    mon_list[1]=(monster_type){.r_idx=1,.ml=true,.fy=5,.fx=6};
    cave_m_idx[5][6]=1;
    assert(tutorial_game_command_allowed(';',6));
    p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
    assert(!tutorial_game_command_allowed(';',6)); p_ptr->niena_quest=0;
    p_ptr->afraid=1; assert(!tutorial_game_command_allowed(';',6)); p_ptr->afraid=0;
    p_ptr->truce=1; assert(!tutorial_game_command_allowed(';',6)); p_ptr->truce=0;
    r_info[1].flags1=RF1_PEACEFUL;
    assert(!tutorial_game_command_allowed(';',6)); r_info[1].flags1=0;
    mon_list[1].ml=false; assert(!tutorial_game_command_allowed(';',6)); mon_list[1].ml=true;
    mock_merciless=true; assert(!tutorial_game_command_allowed(';',6)); mock_merciless=false;
    mock_cowardly=true; assert(!tutorial_game_command_allowed(';',6)); mock_cowardly=false;
    tutorial_game_attack(&mon_list[1]); /* A committed miss is still an attack. */
    assert(tutorial_lesson_status("attack")==TUTORIAL_COMPLETED);

    activate("throw","");
    active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_THROWING;
    assert(tutorial_game_command_allowed('f',0));
    active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;
    assert(!tutorial_game_command_allowed('f',0));

    activate("item.bow.use","bow");
    test_path[0]=GRID(5,6); test_path_count=1;
    assert(tutorial_ranged_aim_allowed(8,5,6,true));
    assert(!tutorial_ranged_aim_allowed(8,5,7,true));
    assert(tutorial_ranged_aim_allowed(8,5,99,false));
    test_path_count=0; assert(!tutorial_ranged_aim_allowed(8,5,99,false));
    test_path_count=2; test_path[1]=GRID(5,7); cave_m_idx[5][7]=2;
    mon_list[2]=(monster_type){.r_idx=2,.ml=true,.fy=5,.fx=7};
    r_info[2].flags1=RF1_PEACEFUL;
    assert(!tutorial_ranged_aim_allowed(8,5,6,true));
    mon_list[2].ml=false;
    assert(tutorial_ranged_aim_allowed(8,5,6,true));
    tutorial_checkpoint(false);
    assert(!tutorial_game_target_allowed(5,7));
    cave_m_idx[5][7]=0; test_path_count=1;
    activate("fire","");
    assert(tutorial_game_command_allowed('f',0));
    active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_THROWING;
    assert(!tutorial_game_command_allowed('f',0));
    active_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;

    activate("item.staff.ready","staff");
    assert(tutorial_game_begin_action("ready",&staff));
    tutorial_checkpoint(false); /* Pack's paid multi-turn access hides the card. */
    assert(!tutorial_get_view(&view));
    assert(tutorial_game_action_allowed("ready",&staff));
    tutorial_game_action_done("ready",&staff);
    tutorial_game_end_action();
    assert(tutorial_lesson_status("item.staff.ready")==TUTORIAL_COMPLETED);

    activate("item.staff.ready","staff");
    assert(tutorial_game_begin_action("ready",&staff));
    tutorial_checkpoint(false);
    tutorial_game_action_done("equip",&staff); /* Paid WIELD continuation really readies it. */
    tutorial_game_end_action();
    assert(tutorial_lesson_status("item.staff.ready")==TUTORIAL_COMPLETED);

    activate("menu-test","");
    tutorial_game_menu("equipment","Browse equipment");
    assert(tutorial_lesson_status("menu-test")==TUTORIAL_IN_PROGRESS);
    tutorial_game_menu("inventory","Browse inventory");
    assert(tutorial_lesson_status("menu-test")==TUTORIAL_COMPLETED);
    activate_description("item.first_description");
    tutorial_game_item_described(&staff);
    assert(tutorial_lesson_status("item.first_description")==TUTORIAL_COMPLETED);
    check_description_routes(&staff);
    check_feature_tips();

    activate("item.staff.use","staff");
    tutorial_skip(); tutorial_set_enabled(false);
    assert(tutorial_replay("item.staff.use"));
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.can_continue);
    assert(!tutorial_game_action_allowed("use-item",&staff));
    tutorial_continue(); assert(!tutorial_is_active());

    tutorial_set_enabled(true);
    activate("move",""); tutorial_skip();
    tutorial_game_start();
    assert(tutorial_lesson_status("move")==TUTORIAL_SKIPPED);
    assert(!tutorial_is_active());
    {
        int starts=world_start_calls, old_depth=previous_depth;
        p_ptr->depth=old_depth+1;
        tutorial_game_start(); /* dungeon() calls this on ordinary level entry. */
        assert(previous_depth==old_depth && world_start_calls==starts);
        p_ptr->restoring=true;
        tutorial_game_start();
        assert(previous_depth==p_ptr->depth && world_start_calls==starts+1);
        p_ptr->restoring=false;
        reached_kinds[1]=true;
        ++test_tale.id;
        tutorial_game_start();
        assert(observed_tale==test_tale.id && world_start_calls==starts+2);
        assert(!reached_kinds[1]);
        reached_kinds[1]=true; playerturn=0;
        tutorial_game_start();
        assert(world_start_calls==starts+3 && !reached_kinds[1]);
        p_ptr->tutorial_deferred=true; p_ptr->restoring=true;
        tutorial_game_start();
        assert(tutorial_character_blocked() && tutorial_get_mode()==TUTORIAL_MODE_EXTENDED);
        tutorial_game_item(&staff); tutorial_checkpoint(true);
        assert(!tutorial_is_active());
        tutorial_set_mode(TUTORIAL_MODE_NORMAL); tutorial_reset_tale();
        tutorial_set_mode(TUTORIAL_MODE_EXTENDED); tutorial_game_item(&staff);
        assert(tutorial_character_blocked() && !tutorial_is_active() && queue_count==0);
        p_ptr->tutorial_deferred=false; p_ptr->restoring=false; playerturn=0;
        tutorial_game_start();
        assert(!tutorial_character_blocked());
    }
    check_live_checkpoints();
    tutorial_shutdown(); SDL_Quit();
    puts("Gameplay tutorial integration: PASS (typed actions, known instruments/remedies, post-cure completion, ranged path safety, movement/oath gates, hidden paid actions, inline/modal description success/failure and visibility, Off replay, level/reload history)");
    return 0;
}
'''


def c_function(source, name):
    match = re.search(r'^(?:static\s+)?(?:void|bool|char)\s+' + re.escape(name)
                      + r'\s*\([^;{]*\)\s*\{', source, re.M)
    assert match, f'Missing source function: {name}'
    return source[match.start():source.index('\n}', match.end()) + 2]


def check_browser_preview_routes():
    source = (ROOT / 'src/cmd/ui/cmd-ui-knowledge.c').read_text(encoding='utf-8-sig')
    helper = c_function(source, 'supply_update_tutorial_preview')
    assert 'sdl_gameplay_tutorial_set_menu_preview(available, shown)' in helper
    assert 'supply_controller_info_key(label, sizeof(label), allow_secondary)' in helper
    assert 'sdl_gameplay_tutorial_set_menu_preview_control(label)' in helper
    browser = c_function(source, 'do_cmd_knowledge_supplies')
    branches = re.findall(r'supply_update_tutorial_preview\(([^;]+)\);\s*char ch = inkey\(\);', browser)
    assert len(branches) == 3, 'Every Equipped/Inventory/Supplies input branch must publish its preview state before waiting'
    assert len(re.findall(r'char ch = inkey\(\);', browser)) == len(branches), 'A browser input branch bypasses preview state publication'
    for branch, availability in zip(branches, ('equip_entry_cnt > 0', 'inventory_entry_cnt > 0', 'entry_cnt > 0')):
        assert availability in branch and 'overlay_cache.active' in branch, availability
    assert 'SUPPLY_GROUP_JEWELRY_PRESETS' in branches[2]
    exit_route = browser[browser.rindex('sdl_gameplay_tutorial_set_menu_preview(false, false)'):]
    assert exit_route.index('object_info_overlay_clear();') < exit_route.index('screen_load();'), 'Browser must clear description and tutorial preview state before restoring gameplay'
    cache_reset = c_function(source, 'supply_overlay_cache_reset')
    assert 'object_info_overlay_clear();' in cache_reset and 'cache->active = false;' in cache_reset


DESCRIPTION_STUBS = r'''
#include "object/object-info.h"
#define OBJECT_INFO_MAX_ACTIONS 8
#define OBJECT_INFO_UNDERSTANDING_KEY 'i'
__CAPTURE_TYPE__
static object_info_screen_capture object_info_overlay_capture;
static bool object_info_overlay_capture_active;
static bool new_paragraph;
static bool mock_description_capture=true, mock_description_present=true;
static int mock_description_events, mock_description_keys;
s16b character_icky;
void (*text_out_hook)(byte,cptr);
int text_out_wrap, text_out_indent;
void text_out_to_screen(byte a,cptr s) { (void)a;(void)s; }
bool story_object_desc_enabled(void) { return false; }
static bool object_info_blocked_by_hallucination(void) { return p_ptr->image>0; }
static int object_info_screen_preferred_capture_width(bool story) { (void)story; return 40; }
static bool object_info_screen_capture_build(const object_type **objects,
    const char **headings,int count,object_info_screen_capture *capture,
    int width,bool story,bool interactive)
{
    (void)objects;(void)headings;(void)count;(void)width;(void)story;(void)interactive;
    if (!mock_description_capture) return false;
    memset(capture,0,sizeof(*capture)); capture->width=40; capture->height=5; capture->target_cols=40;
    return true;
}
static void object_info_screen_capture_free(object_info_screen_capture *capture)
{ memset(capture,0,sizeof(*capture)); }
bool sdl_description_overlay_present(const byte *a,const char *c,const byte *ta,
    const char *tc,const byte *s,const byte *health,int width,int height,
    int cols,int scroll,bool interactive,int *visible_rows,int *max_scroll)
{
    (void)a;(void)c;(void)ta;(void)tc;(void)s;(void)health;(void)width;(void)height;
    (void)cols;(void)scroll;(void)interactive;
    if (visible_rows) *visible_rows=3;
    if (max_scroll) *max_scroll=2;
    return mock_description_present;
}
void sdl_description_overlay_clear(void) {}
void sdl_description_overlay_set_footer(cptr t,bool always) { (void)t;(void)always; }
void sdl_description_overlay_clear_footer_actions(void) {}
void sdl_description_overlay_add_footer_action(int key,cptr token) { (void)key;(void)token; }
void ui_scroll_area_begin(int top,int bottom,int category) { (void)top;(void)bottom;(void)category; }
void ui_scroll_area_set_keys(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d; }
void ui_scroll_area_set_tap_key(int key) { (void)key; }
void ui_scroll_area_clear(void) {}
errr Term_get_size(int *width,int *height) { if(width)*width=80;if(height)*height=24;return 0; }
errr Term_fresh(void) { return 0; }
errr Term_xtra(int event,int value)
{
    tutorial_view view; (void)event;(void)value;
    assert(++mock_description_events<30);
    assert(tutorial_get_view(&view) && view.can_continue);
    tutorial_continue(); tutorial_checkpoint(true); return 0;
}
char inkey(void) { if(mock_description_keys>0){--mock_description_keys;return '2';}return ESCAPE; }
int target_dir(char key) { return key>='1'&&key<='9'?key-'0':0; }
int understanding_gem_count_for_item_description(const object_type *item) { (void)item;return 0; }
bool do_cmd_use_understanding_gem_on_item(const object_type *item) { (void)item;return false; }
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--object-info-source', type=Path,
                        default=ROOT / 'src/object/object-info.c',
                        help='Optional pre-fix source snapshot for demonstrating the regression.')
    args = parser.parse_args()
    check_browser_preview_routes()
    OUT.mkdir(parents=True, exist_ok=True)
    # Every fixture uses distinct Tales, and reruns start with clean test files.
    for file in OUT.glob("tale-*-tutorials.json"):
        file.unlink()
    lessons = []
    for lesson_id, action, subject in [
        ("item.staff.use", "use-item", "staff"),
        ("item.horn.use", "use-item", "horn"),
        ("status.poisoned.remedy", "use-item", "poisoned"),
        ("status.diseased.remedy", "use-item", "diseased"),
        ("status.drain.remedy", "use-item", "drain"),
        ("move", "move", ""), ("attack", "attack", "monster"),
        ("throw", "throw", ""), ("fire", "fire", ""),
        ("item.bow.use", "fire", "bow"),
        ("equip-test", "equip", ""), ("change-test", "change-active", ""),
        ("item.staff.ready", "ready", "staff"),
        ("item.armour.equip", "equip", "armour"),
        ("menu-test", "open-menu", "inventory"),
        ("item.first_description", "examine", ""),
    ]:
        lessons.append({"id": lesson_id, "title": lesson_id,
                        "steps": [{"kind": "action", "text": "Fixture",
                                   "action": action, "subject_type": subject}]})
    next(lesson for lesson in lessons if lesson['id']=='item.first_description')['steps'].insert(
        0, {'kind':'info','text':'Inspect the item description before choosing an action.'})
    lessons.append({'id':'description-two-actions','title':'Two different descriptions','steps':[
        {'kind':'action','text':'Read staff','action':'examine','subject_type':'staff'},
        {'kind':'action','text':'Read horn','action':'examine','subject_type':'horn'}]})
    for id in ('world.skeleton','world.chest','item.description'):
        lessons.append({'id':id,'title':id,'level':'normal',
                        'steps':[{'kind':'info','text':'Feature or description information.'}]})
    for id in ('status.poisoned','status.heavy_stun','status.health','status.voice',
               'status.hungry','status.weak','status.starving','world.water','world.lava',
               'world.ice','world.poison','menu.inventory'):
        lessons.append({'id':id,'title':id,'level':'normal',
                        'steps':[{'kind':'info','text':'Current context: {subject}'}]})
    lessons.extend([
        {'id':'combat.first_monster','title':'Awareness','priority':65,'steps':[{'text':'Creature awareness.'}]},
        {'id':'combat.first_adjacent','title':'Attack','priority':45,'steps':[{'text':'Attack.','kind':'action','action':'attack','subject_type':'monster'}]},
        {'id':'combat.stealth','title':'Stealth','priority':40,'steps':[{'text':'Stealth.','kind':'action','action':'stealth'}]},
    ])
    (OUT / "catalogue.json").write_text(json.dumps({"version": 1, "lessons": lessons}))
    source = OUT / "check.c"
    # externs.h contains unguarded enum declarations. A unity fixture has
    # already read it through tutorial.c; omit only that repeated include.
    game = (ROOT / "src/tutorial/tutorial-game.c").read_text()
    game = game.replace('#include "externs.h"', '')
    game = game.replace('#include "tutorial-game.h"', '#include "tutorial/tutorial-game.h"')
    game = game.replace('#include "tutorial-world.h"', '#include "tutorial/tutorial-world.h"')
    ranged = (ROOT / "src/cmd/combat/cmd-ranged.c").read_text()
    aim_start = ranged.index("static bool tutorial_ranged_aim_allowed(")
    aim = ranged[aim_start:ranged.index("\nstatic int breakage_chance", aim_start)]
    use_source = (ROOT / "src/use-obj.c").read_text()
    use_start = use_source.index("bool use_object(object_type* o_ptr, bool* ident)")
    use_body = use_source[use_start:use_source.index("\n}", use_start) + 2]
    description_source = args.object_info_source.read_text(encoding='utf-8-sig')
    capture_type = re.search(r'typedef struct object_info_screen_capture\b.*?\} object_info_screen_capture;',
                             description_source, re.S).group(0)
    description = DESCRIPTION_STUBS.replace('__CAPTURE_TYPE__', capture_type) + '\n' + '\n'.join(
        c_function(description_source, name) for name in (
            'object_info_overlay_capture_release', 'object_info_overlay_clear',
            'object_info_overlay_show_multi', 'object_info_screen_key_is_action',
            'object_info_screen_action_key_available', 'object_info_understanding_action_token',
            'object_info_configure_footer', 'object_info_screen_capture_view',
            'object_info_screen_multi_with_actions'))
    source.write_text(HARNESS.replace("__GAME_IMPLEMENTATION__", game)
                      .replace("__RANGED_AIM_IMPLEMENTATION__", aim)
                      .replace("__USE_OBJECT_IMPLEMENTATION__", use_body)
                      .replace("__DESCRIPTION_IMPLEMENTATION__", description), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
                                   str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "-Wno-unused-function", "-O2", "-fwhole-program",
                    "-ffunction-sections", "-fdata-sections",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/cJSON.c"), "_deps/SDL/libSDL3.dll.a", "-Wl,--gc-sections",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
