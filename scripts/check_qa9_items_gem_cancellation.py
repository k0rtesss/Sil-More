#!/usr/bin/env python3
"""Check gem costs, light identification and lantern oil with isolated engine state."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-items-gem-cancellation'

CHECKS = r'''
#include "init/init2-internal.h"
#include "object/object-internal.h"
#include "object/object-ui-select.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int next_key, input_calls, truce_calls;
static int sanctity_picker_calls, sanctity_target_count;
static const object_type* sanctity_expected_target;
static bool real_identification, confirm_identification;
static int identify_picker_calls;
static const object_type* identify_expected_target;
static int identify_expected_item;
void fixture_supply_light_name(char* name, size_t size,
    const object_type* light, bool equipped, bool compact);
bool __real_object_choice_overlay(cptr title, cptr desc,
    const object_choice_entry* entries, int count, int initial, int* selected);
bool __wrap_object_choice_overlay(cptr title, cptr desc,
    const object_choice_entry* entries, int count, int initial, int* selected)
{
    if (real_identification && !strcmp(title,"Identify which item?")) {
        identify_picker_calls++;
        assert(count==1 && entries[0].o_ptr==identify_expected_target);
        assert(entries[0].item==identify_expected_item);
        assert(strstr(entries[0].text,"unknown"));
        if (identify_expected_item<0) assert(!strcmp(entries[0].label,"-)"));
        *selected=0;
        return confirm_identification;
    }
    if (strcmp(title,"Cleanse which item?"))
        return __real_object_choice_overlay(title,desc,entries,count,initial,selected);
    sanctity_picker_calls++;
    sanctity_target_count=count;
    assert(count==1 && entries[0].o_ptr==sanctity_expected_target);
    if (!object_known_p(entries[0].o_ptr))
        assert(strstr(entries[0].text,"unknown"));
    return false;
}
char __wrap_inkey(void)
{ assert(++input_calls==1); return (char)next_key; }
bool __real_display_unified_identify_menu(bool include_floor, int* item,
    object_type** object);
/* The real no-target scan is used; presentation is supplied at the same
 * target-picker boundary exercised with Escape and confirmation live. */
bool __wrap_display_unified_identify_menu(bool include_floor, int* item,
    object_type** object)
{
    if (real_identification || object_known_p(&inventory[0]))
        return __real_display_unified_identify_menu(include_floor,item,object);
    assert(++input_calls==1);
    assert(next_key==ESCAPE); return false;
}
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_light_area(int dd, int ds, int rad)
{ (void)dd; (void)ds; (void)rad; return false; }
bool __real_ident_spell(bool include_floor);
/* Successful identification's lore/XP/redraw work requires a running game;
 * this fixture supplies its outcome and checks the real gem command cost. */
bool __wrap_ident_spell(bool include_floor)
{
    if (real_identification || next_key!='a') return __real_ident_spell(include_floor);
    assert(++input_calls==1);
    k_info[inventory[0].k_idx].aware=true; object_known(&inventory[0]);
    return true;
}
void __wrap_break_truce(bool obvious) { (void)obvious; truce_calls++; }
bool __wrap_tutorial_game_action_allowed(const char *action, const object_type *item)
{ (void)action; (void)item; return true; }
static object_type* prepare_gem(int sval)
{
    object_type incoming;
    int kind=lookup_kind(TV_GEM,sval);
    assert(kind);
    supplies_reset_store();
    object_prep(&incoming,kind); incoming.number=3;
    object_aware(&incoming); object_known(&incoming);
    assert(supplies_absorb_object(&incoming));
    return supplies_entry_at(0);
}
static void use_gem(object_type* gem, int key)
{
    p_ptr->energy_use=0; input_calls=truce_calls=0; next_key=key;
    p_ptr->previous_action[0]=7;
    supplies_begin_action(0);
    do_cmd_use_gem(gem,SUPPLIES_INDEX);
    supplies_end_action();
}
static void check_gem_cancellation(void)
{
    int horn=lookup_kind(TV_HORN,SV_HORN_FORCE);
    assert(horn);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    p_ptr->py=p_ptr->px=2;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    object_type* gem=prepare_gem(SV_GEM_UNDERSTANDING);
    object_prep(&inventory[0],horn); inventory[0].number=1;
    object_aware(&inventory[0]); object_known(&inventory[0]);
    /* All currently held items are identified. */
    use_gem(gem,ESCAPE);
    assert(!p_ptr->energy_use && !input_calls && !truce_calls);
    assert(p_ptr->previous_action[0]==7);
    assert(supplies_entry_units(0)==3 && gem->xtra1==0);
    object_prep(&inventory[0],horn); inventory[0].number=1;
    k_info[horn].aware=false;
    use_gem(gem,ESCAPE);
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7);
    assert(supplies_entry_units(0)==3 && gem->xtra1==0);
    assert(!object_known_p(&inventory[0]));
    use_gem(gem,'a');
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(supplies_entry_units(0)==2 && object_known_p(&inventory[0]));
    assert(supplies_entry_at(0)->xtra1==1);
    object_copy(&o_list[1],supplies_entry_at(0));
    supplies_reset_store();
    o_list[1].iy=p_ptr->py; o_list[1].ix=p_ptr->px;
    o_list[1].next_o_idx=0; o_max=2; cave_o_idx[p_ptr->py][p_ptr->px]=1;
    p_ptr->energy_use=0; truce_calls=input_calls=0; next_key=ESCAPE;
    do_cmd_use_gem(&o_list[1],-1);
    assert(!p_ptr->energy_use && !truce_calls && o_list[1].number==2);
    object_wipe(&o_list[1]); cave_o_idx[p_ptr->py][p_ptr->px]=0;
    gem=prepare_gem(SV_GEM_SANCTITY);
    use_gem(gem,ESCAPE); /* No cursed/jinxed target. */
    assert(!p_ptr->energy_use && !truce_calls && supplies_entry_units(0)==3);
    gem->number=0;
    use_gem(gem,ESCAPE);
    assert(!p_ptr->energy_use && !input_calls && !truce_calls);
    /* A real Light use still pays and consumes one gem, even with no
     * monsters to affect. */
    gem=prepare_gem(SV_GEM_LIGHT);
    use_gem(gem,ESCAPE);
    assert(p_ptr->energy_use==100 && truce_calls==1);
    assert(supplies_entry_units(0)==2);
    puts("Understanding: Supplies/floor cancellation free, confirmed identify costs one turn/gem; Sanctity/empty refusals free, actual Light use paid PASS.");
}
static void check_recharging(void)
{
    object_type before[2];
    int staff=lookup_kind(TV_STAFF,SV_STAFF_IMPRISONMENT);
    assert(staff);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    cave_o_idx[p_ptr->py][p_ptr->px]=0; o_max=1;
    for (int i=0;i<2;i++) {
        object_prep(&inventory[i],staff); inventory[i].number=1;
        inventory[i].pval=8+2*i;
        object_aware(&inventory[i]); object_known(&inventory[i]);
    }
    object_type* gem=prepare_gem(SV_GEM_RECHARGING);
    k_info[gem->k_idx].aware=false; k_info[gem->k_idx].tried=false;
    memcpy(before,inventory,sizeof(before));
    u64b rng=Rand_state_export();
    use_gem(gem,ESCAPE); /* Real Recharge selector, with eligible targets. */
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && supplies_entry_units(0)==3);
    assert(!k_info[gem->k_idx].aware && !k_info[gem->k_idx].tried);
    assert(!memcmp(before,inventory,sizeof(before)) && Rand_state_export()==rng);
    object_aware(gem); object_known(gem);
    use_gem(gem,'a'); /* Real carried recharge: exactly one bundled charge. */
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(inventory[0].pval==10 && inventory[1].pval==10);
    assert(supplies_entry_units(0)==2 && supplies_entry_at(0)->xtra1==1);
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=true;
    use_gem(supplies_entry_at(0),'b');
    assert(inventory[0].pval==10 && inventory[1].pval==14);
    assert(p_ptr->energy_use==100 && supplies_entry_units(0)==1);
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=false;
    object_copy(&o_list[2],supplies_entry_at(0)); o_list[2].number=3;
    supplies_reset_store();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    object_prep(&o_list[1],staff); o_list[1].number=1; o_list[1].pval=0;
    object_aware(&o_list[1]); object_known(&o_list[1]);
    o_list[1].ident|=IDENT_EMPTY;
    for (int i=1;i<=2;i++) { o_list[i].iy=p_ptr->py; o_list[i].ix=p_ptr->px; }
    o_list[1].next_o_idx=2; o_list[2].next_o_idx=0;
    o_max=3; cave_o_idx[p_ptr->py][p_ptr->px]=1;
    p_ptr->energy_use=0; truce_calls=input_calls=0; next_key=ESCAPE;
    p_ptr->previous_action[0]=7; rng=Rand_state_export();
    do_cmd_use_gem(&o_list[2],-2);
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && o_list[2].number==3);
    assert(o_list[1].pval==0 && (o_list[1].ident&IDENT_EMPTY));
    assert(Rand_state_export()==rng);
    truce_calls=input_calls=0; next_key='-';
    do_cmd_use_gem(&o_list[2],-2); /* Real floor recharge and empty recovery. */
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(o_list[1].pval==CHANNELING_CHARGE_MULTIPLIER);
    assert(!(o_list[1].ident&IDENT_EMPTY) && o_list[2].number==2);
    object_wipe(&o_list[1]); cave_o_idx[p_ptr->py][p_ptr->px]=2;
    p_ptr->energy_use=0; p_ptr->previous_action[0]=7;
    truce_calls=input_calls=0; rng=Rand_state_export();
    do_cmd_use_gem(&o_list[2],-2); /* Actual no-eligible-Staff scan. */
    assert(!input_calls && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && o_list[2].number==2);
    assert(Rand_state_export()==rng);
    gem=prepare_gem(SV_GEM_WARDING);
    use_gem(gem,ESCAPE); /* Floor occupied by a gem: genuine refusal stays paid. */
    assert(p_ptr->energy_use==100 && truce_calls==1 && supplies_entry_units(0)==3);
    puts("Recharging: real carried/floor target cancel and no-target free; paid charge transfer/Channeling/empty recovery; Warding refusal paid PASS.");
}
static void check_sanctity_target(const object_type* expected)
{
    object_type* gem=supplies_entry_at(0);
    sanctity_expected_target=expected;
    sanctity_picker_calls=sanctity_target_count=0;
    u64b rng=Rand_state_export();
    use_gem(gem,ESCAPE);
    assert(sanctity_picker_calls==(expected ? 1 : 0));
    assert(sanctity_target_count==(expected ? 1 : 0));
    assert(!input_calls && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && supplies_entry_units(0)==3);
    assert(gem->xtra1==0 && Rand_state_export()==rng);
}
static void check_sanctity_targets(void)
{
    int lantern=lookup_kind(TV_LIGHT,SV_LIGHT_LANTERN);
    int staff=lookup_kind(TV_STAFF,SV_STAFF_IMPRISONMENT);
    assert(lantern && staff);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    cave_o_idx[p_ptr->py][p_ptr->px]=0; o_max=1;
    prepare_gem(SV_GEM_SANCTITY);
    object_type* light=&inventory[INVEN_LITE];
    object_prep(light,lantern); light->number=1;
    object_aware(light);
    assert(!object_known_p(light) && object_is_unidentified_for_display(light));
    char desc[160]; object_desc_display(desc,sizeof(desc),light,true,3);
    assert(strstr(desc,"Brass Lantern") && strstr(desc,"unknown"));
    check_sanctity_target(light); /* Unidentified lantern remains a legal probe. */
    object_known(light);
    check_sanctity_target(NULL); /* Fully identified, uncursed lantern. */
    light->ident|=IDENT_CURSED;
    check_sanctity_target(light); /* A real curse remains targetable. */
    light->ident&=~IDENT_CURSED;
    object_set_ego_prefix(light,EGO_FLICKERING_SHADOW);
    assert(object_has_ego_flag4(light,TR4_JINX));
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING]=false;
    check_sanctity_target(NULL);
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING]=true;
    check_sanctity_target(light);
    p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING]=false;
    object_set_ego_prefix(light,0);
    object_prep(&inventory[0],staff); inventory[0].number=1;
    object_aware(&inventory[0]);
    assert(object_is_unidentified_for_display(&inventory[0]));
    check_sanctity_target(&inventory[0]); /* Unknown staff still permits a probe. */
    inventory[0].ident|=IDENT_UNCURSED;
    check_sanctity_target(NULL); /* An already cleansed item stays excluded. */
    object_wipe(&inventory[0]);
    object_copy(&o_list[1],light); object_wipe(light);
    o_list[1].ident&=~IDENT_KNOWN;
    o_list[1].iy=p_ptr->py; o_list[1].ix=p_ptr->px;
    o_list[1].next_o_idx=0; o_max=2; cave_o_idx[p_ptr->py][p_ptr->px]=1;
    check_sanctity_target(&o_list[1]); /* Unknown floor lantern also stays eligible. */
    o_list[1].ident|=IDENT_CURSED;
    check_sanctity_target(&o_list[1]);
    puts("Sanctity: unknown lanterns/staffs, real curses and removable jinxes eligible; known clean/cleansed items excluded; cancellation free PASS.");
}
static void reset_lights(void)
{
    supplies_reset_store();
    player_carried_extra_reset_store(); player_quiver_reset_store();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    memset(cave_o_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_o_idx));
    memset(p_ptr,0,sizeof(*p_ptr));
    for (int i=0;i<A_MAX;i++) p_ptr->stat_base[i]=3;
    p_ptr->py=p_ptr->px=2;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=20;
    cave_feat[2][2]=FEAT_FLOOR; o_max=1;
}
static void check_light_name(const object_type* light, bool unknown)
{
    object_type before=*light;
    int oil=p_ptr->lamp_oil, fuel=player_light_fuel(light);
    u64b rng=Rand_state_export();
    char name[160];
    assert(object_is_unidentified_for_display(light)==unknown);
    object_desc_display(name,sizeof(name),light,true,3);
    assert((strstr(name,"unknown")!=NULL)==unknown);
    object_desc_display(name,sizeof(name),light,false,3);
    assert((strstr(name,"not identified")!=NULL)==unknown);
    object_desc_floor_display(name,sizeof(name),light,true,3);
    assert((strstr(name,"unknown")!=NULL)==unknown);
    object_desc_display(name,sizeof(name),light,false,4);
    assert((strstr(name,"{?}")!=NULL)==unknown);
    for (int compact=0;compact<2;compact++) {
        fixture_supply_light_name(name,sizeof(name),light,false,compact);
        assert((strstr(name,"unknown")!=NULL)==unknown);
        fixture_supply_light_name(name,sizeof(name),light,true,compact);
        assert((strstr(name,"unknown")!=NULL)==unknown);
        assert(strstr(name,"[equipped]"));
    }
    object_choice_entry entry;
    object_choice_entry_make(&entry,INVEN_LITE,(object_type*)light,NULL,"Light");
    assert((strstr(entry.text,"unknown")!=NULL)==unknown);
    bool old_slate=unidentified_items_slate;
    unidentified_items_slate=true;
    assert((object_display_color(light,TERM_WHITE)==TERM_SLATE)==unknown);
    unidentified_items_slate=old_slate;
    assert(!memcmp(light,&before,sizeof(before)) && p_ptr->lamp_oil==oil);
    assert(player_light_fuel(light)==fuel && Rand_state_export()==rng);
}
static void check_light_names(void)
{
    reset_lights();
    const int svals[]={SV_LIGHT_TORCH,SV_LIGHT_LANTERN,SV_LIGHT_LESSER_JEWEL,
        SV_LIGHT_MALLORN,SV_LIGHT_FEANORIAN,SV_LIGHT_SILMARIL};
    for (unsigned i=0;i<sizeof(svals)/sizeof(svals[0]);i++) {
        object_type light;
        int kind=lookup_kind(TV_LIGHT,svals[i]); assert(kind);
        object_prep(&light,kind); object_aware(&light);
        if (fuelable_light_p(&light)) light.timeout=1000;
        bool difficulty=object_uses_smithing_difficulty(&light);
        int group=player_light_carry_group(&light);
        check_light_name(&light,!object_known_p(&light));
        object_known(&light);
        check_light_name(&light,false);
        assert(object_uses_smithing_difficulty(&light)==difficulty);
        assert(player_light_carry_group(&light)==group);
    }
    const int egos[]={130,73,EGO_FLICKERING_SHADOW};
    for (unsigned i=0;i<sizeof(egos)/sizeof(egos[0]);i++) {
        object_type light;
        object_prep(&light,lookup_kind(TV_LIGHT,SV_LIGHT_LANTERN));
        object_set_ego_prefix(&light,egos[i]);
        char hidden[160], known[160];
        check_light_name(&light,true);
        object_desc_display(hidden,sizeof(hidden),&light,true,3);
        object_type spoiled=light; spoiled.ident|=IDENT_SPOIL;
        check_light_name(&spoiled,false);
        object_known(&light);
        check_light_name(&light,false);
        object_desc_display(known,sizeof(known),&light,true,3);
        assert(strcmp(hidden,known));
        assert(!strstr(hidden,"Bright") && !strstr(hidden,"true Sight")
            && !strstr(hidden,"Flickering"));
    }
    puts("Light names/colors: all six light kinds, hidden affixes, floor/choice/compact names and spoiler views; no object/oil/fuel/RNG mutation PASS.");
}
static void check_light_identification(void)
{
    const int locations[]={INVEN_LITE,0,SUPPLIES_INDEX+1,-1};
    for (unsigned i=0;i<sizeof(locations)/sizeof(locations[0]);i++) {
        reset_lights();
        prepare_gem(SV_GEM_UNDERSTANDING);
        object_type lamp, *light;
        object_prep(&lamp,lookup_kind(TV_LIGHT,SV_LIGHT_LANTERN));
        object_aware(&lamp); lamp.timeout=1300;
        int item=locations[i];
        if (item==SUPPLIES_INDEX+1) {
            assert(supplies_absorb_object(&lamp));
            light=supplies_entry_at(1);
        } else if (item<0) {
            light=&o_list[1]; object_copy(light,&lamp);
            light->iy=p_ptr->py; light->ix=p_ptr->px; light->marked=true;
            cave_o_idx[2][2]=1; o_max=2;
        } else {
            light=&inventory[item]; object_copy(light,&lamp);
            light->timeout=0; player_set_lamp_oil(1300);
        }
        check_light_name(light,true);
        int fuel=player_light_fuel(light), oil=p_ptr->lamp_oil;
        int capacity=player_lamp_oil_capacity();
        object_type before=*light;
        identify_expected_target=light; identify_expected_item=item;
        identify_picker_calls=0; real_identification=true;
        confirm_identification=false;
        u64b rng=Rand_state_export();
        use_gem(supplies_entry_at(0),ESCAPE);
        assert(identify_picker_calls==1 && !input_calls);
        assert(!p_ptr->energy_use && !truce_calls && supplies_entry_units(0)==3);
        assert(!memcmp(light,&before,sizeof(before)) && Rand_state_export()==rng);
        confirm_identification=true;
        use_gem(supplies_entry_at(0),'a');
        assert(identify_picker_calls==2 && !input_calls);
        assert(object_known_p(light) && p_ptr->energy_use==100 && truce_calls==1);
        assert(supplies_entry_units(0)==2 && supplies_entry_at(0)->xtra1==1);
        check_light_name(light,false);
        assert(player_light_fuel(light)==fuel && p_ptr->lamp_oil==oil);
        assert(player_lamp_oil_capacity()==capacity);
        use_gem(supplies_entry_at(0),ESCAPE);
        assert(identify_picker_calls==2 && !p_ptr->energy_use && !truce_calls);
        assert(supplies_entry_units(0)==2);
        real_identification=false;
    }
    puts("Understanding: real identification of equipped/Pack/Supplies/floor lanterns clears markers, preserves fuel/capacity, and charges exactly one gem/turn; cancellation free PASS.");
}
static void check_lantern_oil(void)
{
    reset_lights();
    object_type lamp, known, flask;
    int kind=lookup_kind(TV_LIGHT,SV_LIGHT_LANTERN); assert(kind);
    object_prep(&lamp,kind); object_aware(&lamp); lamp.timeout=1700;
    known=lamp; object_known(&known); known.timeout=2400;
    assert(!object_similar(&lamp,&known));
    check_light_name(&lamp,true); check_light_name(&known,false);
    assert(!object_similar(&lamp,&known));
    assert(supplies_absorb_object(&lamp) && supplies_absorb_object(&known));
    assert(supplies_entry_count()==2 && player_oil_container_slots_used()==4);
    assert(player_lamp_oil_capacity()==2*FUEL_LAMP && player_lamp_oil()==4100);
    object_type* unknown=supplies_entry_at(0);
    assert(unknown->timeout==0 && supplies_entry_at(1)->timeout==0);
    check_light_name(unknown,true); check_light_name(supplies_entry_at(1),false);
    assert(player_light_fuel(unknown)==4100 && player_light_max_fuel(unknown)==2*FUEL_LAMP);
    p_ptr->energy_use=0;
    supplies_begin_action(0); do_cmd_wield(unknown,SUPPLIES_INDEX); supplies_end_action();
    object_type* light=&inventory[INVEN_LITE];
    assert(light->k_idx==kind && p_ptr->energy_use==100);
    assert(supplies_entry_count()==1 && !object_known_p(light));
    check_light_name(light,true);
    assert(player_light_fuel(light)==4100 && player_lamp_oil_capacity()==2*FUEL_LAMP);
    calc_torch(); assert(p_ptr->cur_light==2);
    player_light_set_fuel(light,player_light_fuel(light)-1);
    assert(player_lamp_oil()==4099 && !light->timeout);
    p_ptr->energy_use=0; do_cmd_takeoff(light,INVEN_LITE);
    assert(!light->k_idx && p_ptr->energy_use==100 && supplies_entry_count()==2);
    assert(player_lamp_oil()==4099 && player_oil_container_slots_used()==4);
    assert(player_lamp_oil_capacity()==2*FUEL_LAMP);
    int unknown_index=-1;
    for (int j=0;j<supplies_entry_count();j++)
        if (!object_known_p(supplies_entry_at(j))) unknown_index=j;
    assert(unknown_index>=0);
    unknown=supplies_entry_at(unknown_index); check_light_name(unknown,true);
    ident(unknown); check_light_name(unknown,false); combine_pack();
    int lantern_count=0;
    for (int j=0;j<supplies_entry_count();j++) {
        assert(object_known_p(supplies_entry_at(j)));
        lantern_count+=supplies_entry_units(j);
    }
    assert(lantern_count==2 && player_lamp_oil()==4099);
    int transfer=-1,lost=-1;
    player_set_lamp_oil(9000);
    assert(player_prepare_lantern_drop(1,&transfer,&lost));
    assert(transfer==4000 && lost==0 && player_lamp_oil()==5000);
    reset_lights();
    object_prep(&lamp,kind); object_aware(&lamp); lamp.timeout=1700;
    assert(supplies_absorb_object(&lamp));
    assert(supplies_take_one(0,&inventory[INVEN_LITE]));
    light=&inventory[INVEN_LITE];
    object_prep(&flask,lookup_kind(TV_FLASK,0)); flask.pval=1200;
    object_copy(&o_list[1],&flask); o_max=2; o_list[1].iy=o_list[1].ix=2;
    cave_o_idx[2][2]=1;
    p_ptr->energy_use=0; do_cmd_refuel_lamp(&o_list[1],-1);
    assert(p_ptr->energy_use==100 && player_lamp_oil()==2900);
    assert(!o_list[1].k_idx && !light->timeout && player_lamp_oil_capacity()==FUEL_LAMP);
    check_light_name(light,!object_known_p(light));
    assert(object_break_brass_lantern(light));
    assert(object_has_broken_prefix(light));
    object_copy(&o_list[1],light); o_list[1].iy=o_list[1].ix=2;
    cave_o_idx[2][2]=1;
    p_ptr->energy_use=0; do_cmd_refuel_lamp(&o_list[1],-1);
    assert(!p_ptr->energy_use && player_lamp_oil()==2900);
    puts("Lantern mechanics: unknown/known stacking, real wield/takeoff, pooled capacity/fuel, radius, consumption, identification, drop transfer, real refuel and broken refusal PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    assert(init_flavor_info()==0); flavor_init();\n'
                      '    check_gem_cancellation(); check_recharging(); check_sanctity_targets();\n'
                      '    check_light_names(); check_light_identification(); check_lantern_oil(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    knowledge = OUT / 'knowledge.c'
    knowledge.write_text(
        '#include "' + (ROOT / 'src/cmd/ui/cmd-ui-knowledge.c').as_posix() + '"\n'
        'void fixture_supply_light_name(char* name, size_t size,\n'
        '    const object_type* light, bool equipped, bool compact) {\n'
        '    supply_list_entry entry = {0}; entry.equipped=equipped;\n'
        '    supply_entry_display_name(name,size,&entry,light,SUPPLY_GROUP_LIGHTS,compact);\n'
        '}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith(('/src/main.c.obj',
                                     '/src/cmd/ui/cmd-ui-knowledge.c.obj'))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(knowledge), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *(f'-Wl,--wrap={name}' for name in ('display_unified_identify_menu','message_flush','handle_stuff','inkey',
                        'break_truce','tutorial_game_action_allowed',
                        'light_area','ident_spell','object_choice_overlay')),
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
