#!/usr/bin/env python3
"""Exercise two-weapon menu choices, real equips and previews using engine objects.

Requires build-incremental.ps1. Templates and writable state use a temporary
profile; no player saves are opened. --baseline proves the missing-menu bug.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/two-weapon-selection"
CHECKS = r'''
#include "ui/question.h"
#include "item_set.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "player/player-upkeep-internal.h"
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while(0)
static char wanted[320];
static int pair_rows, active_rows, expected_rows=-1;
static int attack,dd,ds,attack2,dd2,ds2;
static bool single, rendered_submenu, forbid_shield_rows;
extern bool render_check;
extern void render_options(const char*,const ui_question_option*,const object_type* const*,int,int);
extern void fixture_render_init(void);
extern void fixture_render_resize(int,int,bool);
extern void fixture_render_finish(void);
extern bool fixture_try_ability(int skill,int ability);
extern bool prereqs(int skill,int ability);
extern void fixture_ability_state(char* buf,size_t size);
void __wrap_update_stuff(void) {
    if (p_ptr->update & PU_BONUS) {
        p_ptr->update &= ~PU_BONUS;
        calc_bonuses();
    }
}
void __wrap_handle_stuff(void) { __wrap_update_stuff(); }
void __wrap_msg_print(cptr text) { (void)text; }
bool __wrap_get_check(cptr text) { (void)text; return true; }
bool __wrap_tutorial_game_action_allowed(const char* a,const object_type* o)
{ (void)a; (void)o; return true; }
bool __wrap_tutorial_game_active_weapon_allowed(int kind) { (void)kind; return true; }
void __wrap_tutorial_game_action_done(const char* a,const object_type* o)
{ (void)a; (void)o; }
void __wrap_tutorial_game_identified(const object_type* o,const char* e)
{ (void)o; (void)e; }
char __wrap_inkey(void) { assert(!"Unexpected UI input"); return 0; }
int __wrap_ui_question_ask_objects_with_help(const char* title,const char* desc,
    const ui_question_option* options,const object_type* const* icons,
    int count,int y,int x,int def)
{
    (void)title;(void)desc;(void)icons;(void)y;(void)x;
    bool categories=false;
    int group=-1, grouped_pairs=0;
    assert(def>=0 && def<count);
    for(int i=0;i<count;i++) {
        const char* label=options[i].label;
        if(forbid_shield_rows) assert(!strstr(label,"Shield") && !strstr(label,"shield"));
        if(!strncmp(label,"Melee (",7) || !strncmp(label,"Two weapons (",13))
            categories=true;
        if(!strncmp(label,"Two weapons (",13))
            assert(sscanf(label,"Two weapons (%d)",&grouped_pairs)==1);
        if(!strncmp(label,single?"Melee (":"Two weapons (",single?7:13))
            group=i;
    }
    if(categories) {
        if(expected_rows>=0) assert(grouped_pairs==expected_rows);
        if(render_check && rendered_submenu) {
            rendered_submenu=false;
            return -1;
        }
        if(!wanted[0]) return -1;
        assert(group>=0 && !options[group].disabled);
        if(render_check) render_options(title,options,icons,count,group);
        return group;
    }
    int selected=-1;
    pair_rows=active_rows=0;
    assert(def>=0 && def<count);
    for(int i=0;i<count;i++) {
        const char* label=options[i].label;
        bool pair=strstr(label,"Two weapons\t")!=NULL;
        if(strstr(label,"[active]")) active_rows++;
        if(pair) { pair_rows++; assert(strstr(label,"; off-hand (")); }
        if(wanted[0] && strstr(label,wanted)
            && (single ? !strncmp(label,"Melee\t",6) : pair)) {
            if(selected>=0) continue;
            assert(!options[i].disabled);
            selected=i;
            if(!single) {
                const char* stats=strrchr(label,'\t'); assert(stats);
                assert(sscanf(stats+1,"(%d,%dd%d); off-hand (%d,%dd%d)",
                    &attack,&dd,&ds,&attack2,&dd2,&ds2)==6);
            }
        }
    }
    if(expected_rows>=0) assert(pair_rows==expected_rows);
    if(wanted[0]) assert(selected>=0);
    if(render_check) {
        render_options(title,options,icons,count,selected>=0?selected:def);
        rendered_submenu=streq(options[0].label,"Back to categories");
        return -1;
    }
    return selected;
}
static void put(int slot,int tval,int sval)
{
    object_type* o=&inventory[slot];
    object_prep(o,lookup_kind(tval,sval)); assert(o->k_idx);
    o->number=1; o->storage=OBJECT_STORAGE_HARNESS;
    object_aware(o); object_known(o);
    player_active_weapon_assign_harness_color(o);
}
static void prepare(bool learned,bool ranged)
{
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(1);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    supplies_reset_store(); player_pack_action_reset();
    p_ptr->py=p_ptr->px=5;cave_m_idx[5][5]=-1;
    p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->food=PY_FOOD_FULL-1000;p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->active_weapon_mode=ranged?PLAYER_ACTIVE_WEAPON_RANGED_1:PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->skill_base[S_MEL]=20;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=5;
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]=learned;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=learned;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=learned;
    current_character_profile=&c_info[p_ptr->pcharacter];
    character_dungeon=character_generated=true;turn=100;playerturn=10;
    put(INVEN_WIELD,TV_SWORD,SV_SHORT_SWORD);p_ptr->equip_cnt=1;
    if(ranged) { put(INVEN_BOW,TV_BOW,SV_SHORT_BOW);p_ptr->equip_cnt++; }
    put(0,TV_SWORD,SV_DAGGER);p_ptr->inven_cnt=1;
    wanted[0]=0;expected_rows=-1;single=forbid_shield_rows=false;
    calc_bonuses();
}
static void accompaniment(void)
{
    object_set_ego_suffix(&inventory[0],35);
    assert(object_apply_ego_affix(&inventory[0],35,true));
    assert(object_grants_ability(&inventory[0],S_MEL,MEL_TWO_WEAPON));
    object_known(&inventory[0]);
}
static void want_pair(const object_type* main,const object_type* off)
{
    char a[120],b[120];
    object_type main_weapon=*main,offhand_weapon=*off;
    main_weapon.number=offhand_weapon.number=1;
    object_desc(a,sizeof(a),&main_weapon,true,3);
    object_desc(b,sizeof(b),&offhand_weapon,true,3);
    strnfmt(wanted,sizeof(wanted),"Main: %s + Off-hand: %s",a,b);
    single=false;expected_rows=-1;p_ptr->energy_use=0;
}
static void verify_pair(int main_kind,int off_kind)
{
    calc_bonuses();
    assert(player_active_weapon_is_melee());
    assert(inventory[INVEN_WIELD].k_idx==main_kind && inventory[INVEN_ARM].k_idx==off_kind);
    assert(inventory[INVEN_WIELD].number==1 && inventory[INVEN_ARM].number==1);
    assert(p_ptr->skill_use[S_MEL]==attack && p_ptr->mdd==dd && p_ptr->mds==ds);
    assert(p_ptr->skill_use[S_MEL]+p_ptr->offhand_mel_mod==attack2);
    assert(p_ptr->mdd2==dd2 && p_ptr->mds2==ds2 && dd2>0 && ds2>0);
    int a,d,s;assert(player_active_weapon_offhand_stats_preview(&a,&d,&s));
    assert(a==attack2 && d==dd2 && s==ds2);
    assert(!player_pack_action_pending());
}
static void want_shield(const object_type* weapon,const object_type* shield)
{
    char main[120],off[120];
    object_desc(main,sizeof(main),weapon,true,3);object_desc(off,sizeof(off),shield,true,3);
    strnfmt(wanted,sizeof(wanted),"%s + %s",main,off);
    single=true;expected_rows=-1;p_ptr->energy_use=0;
}
static int count_kind(int kind)
{
    int count=0;
    for(int i=INVEN_WIELD;i<INVEN_TOTAL;i++)
        if(inventory[i].k_idx==kind) count+=inventory[i].number;
    for(int i=0;i<player_pack_entry_count();i++) {
        const object_type* o=player_pack_entry_at(i);
        if(o->k_idx==kind) count+=o->number;
    }
    return count;
}
static void check_stacked_pairs(void)
{
    for(int source=0;source<3;source++)for(int quantity=2;quantity<=3;quantity++) {
        prepare(true,source!=0);put(0,TV_POLEARM,SV_HAND_AXE);
        inventory[0].number=quantity;
        int item=0,kind=inventory[0].k_idx;
        if(source==1) {
            object_type extra=inventory[0];object_wipe(&inventory[0]);
            p_ptr->inven_cnt=0;assert(player_carried_extra_load(&extra));
            item=CARRIED_EXTRA_INDEX;
        } else if(source==2) {
            inventory[INVEN_WIELD]=inventory[0];object_wipe(&inventory[0]);
            inventory[INVEN_WIELD].pickup_slot=PICKUP_SLOT_ACTIVE_THROWING;
            p_ptr->inven_cnt=0;item=INVEN_WIELD;
        }
        calc_bonuses();
        player_type before=*p_ptr;
        object_type items[INVEN_TOTAL];memcpy(items,inventory,sizeof(items));
        object_type stack=*player_inventory_object(item);
        expected_rows=source==2?1:3;do_cmd_toggle_active_weapon();
        assert(!memcmp(&before,p_ptr,sizeof(before)));
        assert(!memcmp(items,inventory,sizeof(items)));
        assert(!memcmp(&stack,player_inventory_object(item),sizeof(stack)));
        want_pair(&stack,&stack);expected_rows=source==2?1:3;
        assert(do_cmd_active_item(item));verify_pair(kind,kind);
        assert(count_kind(kind)==quantity && p_ptr->energy_use==100);
        assert(p_ptr->equip_cnt==(source?3:2));
        want_pair(&inventory[INVEN_WIELD],&inventory[INVEN_ARM]);
        do_cmd_toggle_active_weapon();verify_pair(kind,kind);
        assert(active_rows==1 && !p_ptr->energy_use && count_kind(kind)==quantity);
        object_desc(wanted,sizeof(wanted),&inventory[INVEN_WIELD],true,3);
        single=true;do_cmd_toggle_active_weapon();calc_bonuses();
        assert(!inventory[INVEN_ARM].k_idx && count_kind(kind)==quantity);
    }
    puts("Stacks of two/three hand axes: Harness, extra storage and active throwing hand split one per hand; previews, cancellation, remainder and repeat selection PASS.");

    prepare(true,false);put(0,TV_POLEARM,SV_HAND_AXE);
    object_wipe(&inventory[INVEN_WIELD]);p_ptr->equip_cnt=0;
    inventory[0].number=2;want_pair(&inventory[0],&inventory[0]);expected_rows=1;
    do_cmd_toggle_active_weapon();verify_pair(inventory[INVEN_WIELD].k_idx,inventory[INVEN_ARM].k_idx);
    assert(!player_pack_entry_count());
    puts("A single stack with empty hands equips both members without duplicating the source PASS.");

    prepare(false,false);accompaniment();inventory[0].number=2;
    int dagger=inventory[0].k_idx;
    want_pair(&inventory[0],&inventory[0]);do_cmd_toggle_active_weapon();
    verify_pair(dagger,dagger);assert(count_kind(dagger)==2);
    assert(p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    puts("A stack of Accompaniment daggers can supply its own Two Weapon Fighting grant PASS.");

    prepare(true,false);put(0,TV_POLEARM,SV_HAND_AXE);
    expected_rows=2;do_cmd_toggle_active_weapon();
    prepare(false,false);put(0,TV_POLEARM,SV_HAND_AXE);inventory[0].number=2;
    expected_rows=0;do_cmd_toggle_active_weapon();
    prepare(true,false);put(0,TV_POLEARM,SV_HAND_AXE);inventory[0].number=2;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=false;
    expected_rows=0;do_cmd_toggle_active_weapon();
    puts("A lone weapon cannot fill both hands; stacked pairs still require active Two Weapon Fighting PASS.");

    prepare(true,true);put(INVEN_WIELD,TV_POLEARM,SV_HAND_AXE);
    inventory[INVEN_WIELD].number=2;
    inventory[INVEN_WIELD].pickup_slot=PICKUP_SLOT_ACTIVE_THROWING;
    inventory[INVEN_WIELD].ident|=IDENT_CURSED;
    expected_rows=0;do_cmd_toggle_active_weapon();
    puts("A cursed active throwing stack cannot be split by moving its members between hands PASS.");
}
static void checks(void)
{
    prepare(true,false);
    int sword=inventory[INVEN_WIELD].k_idx,dagger=inventory[0].k_idx;
    check_stacked_pairs();

    prepare(true,false);put(1,TV_SHIELD,SV_ROUND_SHIELD);p_ptr->inven_cnt=2;
    object_set_ego_prefix(&inventory[1],173);
    assert(object_apply_ego_affix(&inventory[1],173,true));object_known(&inventory[1]);
    assert(streq(e_name+e_info[173].name,"(Broken)"));
    assert(!object_has_broken_prefix(&inventory[1]));
    want_shield(&inventory[INVEN_WIELD],&inventory[1]);do_cmd_toggle_active_weapon();
    assert(inventory[INVEN_ARM].tval==TV_SHIELD && object_ego_prefix(&inventory[INVEN_ARM])==173);
    assert(p_ptr->energy_use==100);
    puts("Authored (Broken) damaged shield prefix remains selectable and actually equips PASS.");

    prepare(true,false);put(1,TV_LIGHT,SV_LIGHT_LANTERN);p_ptr->inven_cnt=2;
    assert(object_break_brass_lantern(&inventory[1]));
    assert(object_has_broken_prefix(&inventory[1]));
    p_ptr->energy_use=0;
    assert(!do_cmd_wield_to_slot(&inventory[1],1,INVEN_LITE));
    assert(!inventory[INVEN_LITE].k_idx && inventory[1].k_idx && !p_ptr->energy_use);
    put(1,TV_POLEARM,SV_SPEAR);assert(object_break_shafted_weapon_by_fire(&inventory[1]));
    assert(object_has_broken_prefix(&inventory[1]));
    assert(!do_cmd_wield_to_slot(&inventory[1],1,INVEN_WIELD));
    assert(inventory[INVEN_WIELD].k_idx==sword && !p_ptr->energy_use);
    puts("Broken brass lanterns and fire-broken weapons still cannot equip or spend a turn PASS.");

    prepare(true,false);p_ptr->pcharacter=4;current_character_profile=&c_info[4];
    assert(c_info[4].flags_u & UNQ_MEL_MAEDHROS);
    inventory[0].number=3;object_set_ego_prefix(&inventory[0],174);
    assert(object_apply_ego_affix(&inventory[0],174,true));object_known(&inventory[0]);
    put(1,TV_SHIELD,SV_ROUND_SHIELD);p_ptr->inven_cnt=2;calc_bonuses();
    assert(p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]);
    assert(!p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]);
    assert(!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    int chipped_item=-1;
    for(int i=0;i<player_pack_entry_count();i++) {
        int handle=player_pack_entry_handle_at(i);
        if(player_inventory_object(handle)->k_idx==dagger) chipped_item=handle;
    }
    assert(chipped_item>=0);
    assert(!do_cmd_can_wield_offhand(player_inventory_object(chipped_item)));
    forbid_shield_rows=true;expected_rows=0;do_cmd_toggle_active_weapon();
    p_ptr->energy_use=0;
    assert(!do_cmd_wield_to_slot(player_inventory_object(chipped_item),chipped_item,INVEN_ARM));
    assert(!inventory[INVEN_ARM].k_idx && !p_ptr->energy_use);
    player_inventory_object(chipped_item)->storage=OBJECT_STORAGE_PACK;
    assert(!do_cmd_wield_to_slot(player_inventory_object(chipped_item),chipped_item,INVEN_ARM));
    assert(!player_pack_action_pending() && !p_ptr->energy_use);
    puts("Maedhros: old learned TWF stays inactive; shields, pair choices and forced off-hand equips are blocked PASS.");

    prepare(false,false);p_ptr->pcharacter=4;current_character_profile=&c_info[4];
    p_ptr->new_exp=100000;
    assert(!prereqs(S_MEL,MEL_TWO_WEAPON));
    assert(!fixture_try_ability(S_MEL,MEL_TWO_WEAPON));
    assert(p_ptr->new_exp==100000 && !p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]);
    char unavailable[32];fixture_ability_state(unavailable,sizeof(unavailable));
    assert(streq(unavailable,"unavailable"));
    accompaniment();calc_bonuses();expected_rows=0;do_cmd_toggle_active_weapon();
    assert(!do_cmd_can_wield_offhand(&inventory[0]));
    assert(do_cmd_wield_to_slot(&inventory[0],0,INVEN_WIELD));calc_bonuses();
    assert(!p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON] && !p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    prepare(false,false);p_ptr->active_ability[S_PER][PER_QUICK_STUDY]=true;
    assert(prereqs(S_MEL,MEL_TWO_WEAPON));
    puts("Maedhros: ability purchase is unavailable without spending XP; Accompaniment cannot grant it; other heroes retain eligibility PASS.");

    prepare(true,false);
    player_type before=*p_ptr;
    object_type items[INVEN_TOTAL];memcpy(items,inventory,sizeof(items));
    expected_rows=2;do_cmd_toggle_active_weapon();
    assert(!memcmp(&before,p_ptr,sizeof(before)));
    assert(!memcmp(items,inventory,sizeof(items)));
    want_pair(&inventory[INVEN_WIELD],&inventory[0]);expected_rows=2;
    do_cmd_toggle_active_weapon();verify_pair(sword,dagger);assert(p_ptr->energy_use==100);
    want_pair(&inventory[INVEN_ARM],&inventory[INVEN_WIELD]);
    do_cmd_toggle_active_weapon();verify_pair(dagger,sword);assert(p_ptr->energy_use==100);
    want_pair(&inventory[INVEN_WIELD],&inventory[INVEN_ARM]);
    do_cmd_toggle_active_weapon();verify_pair(dagger,sword);
    assert(active_rows==1 && p_ptr->energy_use==0);
    puts("Learned TWF: both arrangements, hand swap, one active row, no-op cost and exact two-attack previews PASS.");

    for(int ranged=0;ranged<2;ranged++)for(int main=0;main<2;main++) {
        prepare(false,ranged);accompaniment();
        want_pair(&inventory[main?0:INVEN_WIELD],&inventory[main?INVEN_WIELD:0]);
        assert(do_cmd_active_item(0));
        verify_pair(main?dagger:sword,main?sword:dagger);
        assert(p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]);
        assert(p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
        assert(!p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]);
        assert(p_ptr->energy_use==100);
        want_pair(&inventory[INVEN_ARM],&inventory[INVEN_WIELD]);
        do_cmd_toggle_active_weapon();
        verify_pair(main?sword:dagger,main?dagger:sword);
    }
    puts("Authored Accompaniment: either hand, focused item menu, from melee/bow, grant activation and repeated hand swaps PASS.");

    prepare(true,false);want_pair(&inventory[INVEN_WIELD],&inventory[0]);
    do_cmd_toggle_active_weapon();
    object_desc(wanted,sizeof(wanted),&inventory[INVEN_WIELD],true,3);
    single=true;p_ptr->energy_use=0;
    do_cmd_toggle_active_weapon();calc_bonuses();
    assert(!inventory[INVEN_ARM].k_idx && !p_ptr->mdd2 && p_ptr->energy_use==100);
    puts("Single-weapon choice from dual wielding stows the second weapon PASS.");

    prepare(false,false);expected_rows=0;do_cmd_toggle_active_weapon();
    accompaniment();inventory[0].ident &= ~IDENT_KNOWN;
    expected_rows=0;do_cmd_toggle_active_weapon();
    object_known(&inventory[0]);
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    expected_rows=0;do_cmd_toggle_active_weapon();
    puts("No ability, unknown grant and deliberately disabled learned TWF omit unavailable pairs PASS.");

    prepare(true,false);put(INVEN_ARM,TV_SHIELD,SV_ROUND_SHIELD);
    inventory[INVEN_ARM].ident|=IDENT_CURSED;
    expected_rows=0;do_cmd_toggle_active_weapon();
    prepare(true,false);put(INVEN_WIELD,TV_HAFTED,SV_QUARTERSTAFF);
    assert(k_info[inventory[INVEN_WIELD].k_idx].flags3 & TR3_TWO_HANDED);
    expected_rows=0;do_cmd_toggle_active_weapon();
    puts("Cursed shield and incompatible handedness block unusable two-weapon combinations PASS.");

    prepare(true,false);put(0,TV_SWORD,SV_SHORT_SWORD);
    want_pair(&inventory[INVEN_WIELD],&inventory[0]);do_cmd_toggle_active_weapon();
    verify_pair(sword,sword);
    puts("Two identical physical weapons remain separate and fill both hands PASS.");

    prepare(false,false);accompaniment();
    object_type extra=inventory[0];object_wipe(&inventory[0]);
    assert(player_carried_extra_load(&extra));
    put(0,TV_SWORD,SV_SHORT_SWORD);object_wipe(&inventory[INVEN_WIELD]);
    p_ptr->equip_cnt=0;
    want_pair(&inventory[0],player_inventory_object(CARRIED_EXTRA_INDEX));
    do_cmd_toggle_active_weapon();verify_pair(sword,dagger);
    puts("Empty hand and two Harness sources, including extra carried storage, equip both weapons PASS.");

    prepare(false,false);accompaniment();
    object_copy(&inventory[INVEN_WIELD],&inventory[0]);
    put(0,TV_SWORD,SV_SHORT_SWORD);put(1,TV_POLEARM,SV_HAND_AXE);p_ptr->inven_cnt=2;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=true;calc_bonuses();
    /* The active dagger's grant cannot justify a final pair which removes it. */
    expected_rows=4;wanted[0]=0;do_cmd_toggle_active_weapon();
    puts("Replacing an item-granted ability never advertises unsupported mundane pairs PASS.");

    char path[1024],line[1024];header head;init_header(&head,1,1);
    item_sets_reset();path_build(path,sizeof(path),ANGBAND_DIR_EDIT,"set.txt");
    SDL_IOStream* file=sdl_fopen(path,"r");assert(file);
    assert(!init_info_txt(file,line,&head,parse_set_info));sdl_fclose(file);
    assert(!item_sets_finalize());
    prepare(false,false);
    assert(make_fake_artefact(&inventory[INVEN_WIELD],65));
    assert(make_fake_artefact(&inventory[0],64));
    inventory[INVEN_WIELD].number=inventory[0].number=1;
    inventory[0].storage=OBJECT_STORAGE_HARNESS;
    object_known(&inventory[INVEN_WIELD]);object_known(&inventory[0]);
    want_pair(&inventory[INVEN_WIELD],&inventory[0]);
    do_cmd_toggle_active_weapon();
    verify_pair(inventory[INVEN_WIELD].k_idx,inventory[INVEN_ARM].k_idx);
    assert(inventory[INVEN_WIELD].name1==65 && inventory[INVEN_ARM].name1==64);
    assert(!p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]);
    puts("Authored matched artefacts remain selectable together without learned TWF PASS.");
    p_ptr->pcharacter=4;current_character_profile=&c_info[4];calc_bonuses();
    assert(!inventory[INVEN_ARM].k_idx && !p_ptr->mdd2);
    wanted[0]=0;expected_rows=0;do_cmd_toggle_active_weapon();
    puts("Maedhros: paired artefacts cannot bypass one-hand restriction; legacy off-hand gear is safely stowed PASS.");
}
static void check_render(void)
{
    fixture_render_init();
    const int sizes[][2]={{1280,800},{360,800}};
    for(int size=0;size<2;size++)for(int big=0;big<2;big++) {
        fixture_render_resize(sizes[size][0],sizes[size][1],big);
        prepare(false,false);accompaniment();
        /* Long inscriptions exercise both item names and both combat previews. */
        inventory[0].obj_note=quark_add("A companion for the long road beneath the mountains");
        inventory[INVEN_WIELD].obj_note=quark_add("A trusted blade for the long road beneath the mountains");
        want_pair(&inventory[INVEN_WIELD],&inventory[0]);do_cmd_toggle_active_weapon();
        prepare(true,false);put(0,TV_POLEARM,SV_HAND_AXE);inventory[0].number=2;
        want_pair(&inventory[0],&inventory[0]);do_cmd_toggle_active_weapon();
    }
    fixture_render_finish();
    puts("SDL weapon menu: desktop/portrait, Normal/Big Font, long names and complete previews render within bounds PASS.");
}
'''

RENDER_SUPPORT = r'''
#include "angband.h"
#include "sdl/ui/sdl-question-menu.c"
#include "ui/question.h"
#include <assert.h>
bool render_check;
static int render_width,render_height;
bool __wrap_sdl_overlay_pane_anchor_rect(enum pane_type pane,SDL_Rect* out)
{
    (void)pane;
    if(!render_check) return false;
    *out=(SDL_Rect){0,0,render_width,render_height};return true;
}
void render_options(const char* title,const ui_question_option* options,
    const object_type* const* icons,int count,int selected)
{
    sdl_question_menu_begin(title);
    for(int i=0;i<count;i++) {
        char key[4];strnfmt(key,sizeof(key),"%c)",options[i].key);
        sdl_question_menu_add_object_entry(i,key,options[i].label,options[i].attr,icons[i]);
        assert(streq(g_question_menu.entries[i].text,options[i].label));
    }
    sdl_question_menu_set_highlight(selected);sdl_question_menu_finish();
    sdl_question_menu_layout_info layout;assert(sdl_question_menu_layout(&layout));
    assert(layout.visible_count>0);
    bool table=false;
    for(int i=0;i<count;i++) if(strchr(options[i].label,'\t')) table=true;
    assert(layout.has_table==table);
    assert(layout.panel.x>=0 && layout.panel.y>=0);
    assert(layout.panel.x+layout.panel.w<=render_width+1);
    assert(layout.panel.y+layout.panel.h<=render_height+1);
    SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255);SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render();
    char file[120];strnfmt(file,sizeof(file),"%s-%dx%d-%s.png",
        table?"two-weapons":"weapon-groups",render_width,render_height,
        config.bigger_font?"big":"normal");
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
    assert(IMG_SavePNG(pixels,file));SDL_DestroySurface(pixels);
    sdl_question_menu_clear();
}
void fixture_render_init(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,"C:/Windows/Fonts/georgia.ttf",sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,"C:/Windows/Fonts/arial.ttf",sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font,"C:/Windows/Fonts/consola.ttf",sizeof(config.monospace_font));
    g_state.window=SDL_CreateWindow("Two-weapon fixture",1280,800,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
    g_state.system_scale=1;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_L_BLUE]=(SDL_Color){100,185,250,255};
    render_check=true;
}
void fixture_render_resize(int width,int height,bool big)
{
    render_width=width;render_height=height;
    assert(SDL_SetWindowSize(g_state.window,width,height));
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    config.bigger_font=big;
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++)config.menu_bigger_font[i]=big;
}
void fixture_render_finish(void)
{
    sdl_ui_text_cache_clear();sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + "    assert(!init_flavor_info()); flavor_init(); checks(); "
        + ("check_render(); " if args.render else "")
        + "SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    render_source = OUT / "render.c"
    render_source.write_text(RENDER_SUPPORT, encoding="utf-8")
    abilities_source = OUT / "abilities.c"
    abilities_source.write_text('#include "cmd/ui/cmd-ui-abilities.c"\n'
        'bool fixture_try_ability(int skill,int ability) { return ability_browser_activate_choice(skill,ability); }\n'
        'void fixture_ability_state(char* buf,size_t size) { ability_browser_entry entry={0}; '
        'entry.abilitynum=MEL_TWO_WEAPON; ability_browser_entry_state(buf,size,S_MEL,&entry); }\n', encoding="utf-8")
    excluded = ["/src/main.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj",
        "/src/cmd/ui/cmd-ui-abilities.c.obj"]
    extra = []
    if args.baseline:
        baseline = OUT / "baseline.c"
        baseline.write_bytes(subprocess.check_output(["git", "show", "HEAD:src/player/player-active-weapon.c"], cwd=ROOT))
        extra.append(str(baseline))
        excluded.append("/src/player/player-active-weapon.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(tuple(excluded))))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrapped = ("handle_stuff", "update_stuff", "msg_print", "get_check", "inkey",
        "tutorial_game_action_allowed", "tutorial_game_active_weapon_allowed",
        "tutorial_game_action_done", "tutorial_game_identified", "ui_question_ask_objects_with_help",
        "sdl_overlay_pane_anchor_rect")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(render_source), str(abilities_source), *extra, "@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+s for s in wrapped), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state],
            cwd=OUT if args.render else state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
