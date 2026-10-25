#!/usr/bin/env python3
"""Cross-check shared difficulty, Python, drop caches, and actual crafting.

Uses production Windows engine objects and disposable profiles. Input and
unrelated presentation are doubled; pricing, acceptance, work, fees and output
are real. No player profile or provider account is opened.
"""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import tempfile

import calc_artefact_difficulty as calc
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_reforge_work_transaction import CHECKS, SMITH

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/smithing-difficulty"

# Snapshot of the pre-unification fees; difficulty discounts must not refund fees.
# Values: STR, DEX, CON, GRA, experience, mithril, star iron.
RESOURCE_FEES = {
    1: (0, 0, 0, 1, 100, 0, 0),
    2: (0, 0, 0, 0, 400, 0, 0),
    3: (0, 0, 0, 0, 350, 0, 0),
    4: (0, 1, 0, 1, 0, 0, 0),
    5: (0, 0, 1, 1, 150, 0, 0),
    6: (0, 0, 0, 3, 0, 0, 0),
    7: (0, 0, 0, 0, 400, 0, 0),
    8: (0, 0, 0, 0, 300, 0, 0),
    9: (0, 0, 0, 1, 650, 0, 0),
    10: (0, 0, 3, 0, 0, 0, 0),
    11: (0, 0, 0, 0, 600, 0, 0),
    21: (0, 0, 0, 1, 0, 0, 0),
    22: (0, 0, 5, 0, 400, 0, 0),
    24: (0, 0, 0, 0, 300, 0, 0),
    25: (1, 0, 0, 0, 0, 0, 0),
    26: (0, 0, 0, 0, 550, 0, 0),
    28: (0, 0, 0, 0, 250, 0, 0),
    29: (0, 0, 0, 0, 200, 0, 0),
    32: (0, 1, 0, 0, 0, 0, 70),
    34: (0, 0, 1, 1, 0, 0, 0),
    36: (0, 0, 2, 0, 0, 0, 0),
    38: (0, 0, 1, 3, 0, 150, 0),
    39: (1, 0, 0, 0, 250, 0, 0),
    40: (0, 0, 0, 0, 450, 0, 0),
    41: (0, 0, 1, 0, 50, 120, 0),
    42: (2, 0, 0, 0, 0, 0, 0),
    43: (0, 0, 1, 0, 250, 0, 0),
    44: (0, 0, 0, 0, 50, 0, 0),
    45: (1, 0, 0, 0, 0, 0, 0),
    46: (0, 2, 0, 1, 0, 0, 0),
    47: (0, 0, 0, 1, 500, 40, 0),
    48: (0, 0, 0, 0, 150, 0, 0),
    49: (0, 0, 0, 0, 300, 0, 0),
    50: (2, 0, 0, 0, 300, 0, 0),
    51: (0, 0, 0, 0, 250, 0, 0),
    52: (2, 0, 0, 0, 0, 0, 0),
    53: (3, 0, 0, 0, 0, 0, 0),
    54: (4, 0, 0, 0, 0, 0, 0),
    55: (2, 0, 0, 0, 0, 0, 0),
    56: (1, 0, 2, 0, 0, 0, 0),
    57: (2, 0, 0, 0, 200, 0, 0),
    58: (0, 0, 0, 0, 350, 0, 0),
    59: (0, 0, 0, 2, 0, 0, 0),
    60: (0, 0, 0, 0, 500, 0, 0),
    61: (0, 0, 0, 0, 300, 0, 0),
    62: (0, 0, 0, 1, 450, 0, 0),
    63: (0, 0, 0, 0, 300, 0, 0),
    64: (0, 0, 0, 0, 300, 0, 0),
    65: (0, 0, 0, 0, 300, 0, 0),
    66: (2, 0, 0, 1, 0, 0, 0),
    67: (0, 0, 0, 0, 450, 0, 0),
    69: (2, 0, 0, 0, 0, 0, 50),
    70: (3, 0, 0, 0, 300, 0, 50),
    71: (2, 0, 0, 0, 350, 0, 50),
    72: (0, 1, 0, 0, 0, 0, 0),
    74: (1, 0, 0, 0, 350, 0, 0),
    75: (2, 0, 0, 0, 0, 0, 0),
    76: (2, 0, 1, 1, 0, 0, 0),
    80: (4, 1, 0, 0, 0, 20, 0),
    81: (2, 1, 0, 1, 0, 20, 0),
    82: (0, 1, 0, 1, 0, 20, 0),
    83: (0, 0, 0, 1, 0, 0, 0),
    84: (2, 0, 0, 0, 0, 0, 0),
    85: (0, 1, 0, 0, 250, 0, 0),
    87: (2, 0, 0, 0, 300, 0, 0),
    88: (0, 1, 0, 0, 0, 0, 0),
    89: (2, 0, 0, 0, 0, 0, 0),
    90: (5, 0, 0, 0, 550, 0, 0),
    91: (0, 0, 1, 0, 0, 0, 0),
    92: (1, 1, 0, 0, 0, 0, 0),
    94: (3, 0, 0, 0, 0, 0, 0),
    95: (2, 0, 0, 0, 0, 0, 0),
    96: (0, 0, 0, 0, 400, 0, 0),
    97: (2, 0, 0, 0, 450, 0, 0),
    98: (2, 0, 0, 0, 0, 0, 0),
    99: (5, 0, 0, 0, 0, 0, 0),
    100: (0, 0, 0, 0, 850, 0, 0),
    101: (0, 2, 0, 0, 0, 0, 0),
    104: (0, 0, 0, 0, 850, 0, 0),
    105: (0, 0, 0, 3, 0, 0, 0),
    107: (0, 0, 0, 0, 350, 0, 0),
    108: (2, 0, 0, 0, 0, 0, 0),
    114: (1, 0, 0, 0, 0, 0, 0),
    117: (0, 0, 0, 0, 350, 0, 0),
    118: (0, 0, 0, 0, 450, 0, 0),
    119: (0, 0, 0, 0, 400, 0, 0),
    120: (2, 0, 0, 0, 0, 0, 0),
    121: (2, 0, 2, 0, 0, 0, 0),
    122: (0, 0, 0, 0, 400, 0, 0),
    123: (0, 0, 0, 0, 650, 40, 0),
    124: (0, 0, 0, 0, 400, 0, 0),
    126: (1, 0, 0, 0, 150, 80, 0),
    127: (0, 0, 1, 1, 0, 150, 0),
    128: (0, 0, 0, 0, 300, 0, 0),
    129: (1, 0, 0, 0, 0, 0, 0),
    131: (0, 0, 0, 0, 750, 0, 0),
    132: (0, 0, 1, 0, 0, 0, 0),
    133: (0, 0, 0, 0, 350, 0, 0),
    134: (0, 0, 0, 0, 400, 0, 0),
    137: (0, 0, 0, 0, 150, 0, 0),
    138: (0, 0, 0, 0, 150, 0, 0),
    142: (0, 1, 0, 0, 700, 0, 0),
    143: (0, 0, 0, 0, 400, 0, 0),
    146: (0, 1, 0, 0, 0, 0, 0),
    147: (0, 1, 0, 1, 0, 0, 0),
    148: (0, 0, 0, 1, 300, 0, 0),
    154: (0, 1, 0, 0, 0, 0, 0),
    155: (0, 0, 0, 0, 450, 0, 0),
    156: (0, 0, 0, 0, 500, 0, 0),
    158: (0, 1, 0, 0, 0, 0, 0),
    160: (1, 0, 0, 0, 250, 0, 0),
    161: (0, 0, 0, 0, 150, 0, 0),
    162: (0, 0, 0, 0, 300, 0, 0),
    163: (0, 1, 0, 1, 400, 0, 0),
    165: (1, 0, 0, 0, 150, 0, 0),
    167: (0, 1, 0, 0, 200, 10, 0),
    168: (1, 0, 0, 0, 50, 0, 0),
    170: (0, 0, 0, 1, 250, 0, 0),
    171: (0, 0, 0, 3, 0, 10, 0),
    179: (3, 0, 0, 0, 0, 0, 0),
}

SMITH_HELPERS = r'''
#include <assert.h>
#include "object/object-difficulty.h"
static int draft_stat, draft_before, draft_after;
void fixture_build_negative_draft(void)
{
    static const u32b neg[A_MAX]={TR1_NEG_STR,TR1_NEG_DEX,TR1_NEG_CON,TR1_NEG_GRA};
    static const u32b pos[A_MAX]={TR1_STR,TR1_DEX,TR1_CON,TR1_GRA};
    memset(smith_a_ptr,0,sizeof(*smith_a_ptr));
    SDL_strlcpy(smith_a_ptr->name,"A real negative-stat project",sizeof(smith_a_ptr->name));
    object_prep(smith_o_ptr,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    smith_o_ptr->name1=smith_a_name;smith_o_ptr->number=1;smith_o_ptr->weight=15;
    smith_o_ptr->att+=3;smith_o_ptr->ds+=3;
    smith_o_ptr->ident|=IDENT_KNOWN|IDENT_SPOIL;
    draft_before=object_difficulty(smith_o_ptr);
    assert(draft_before==55);
    assert(too_difficult(smith_o_ptr));
    smith_a_ptr->flags1|=neg[draft_stat];
    smith_bonus_entry entry={0};
    entry.kind=SMT_BONUS_ENTRY_STAT;entry.index=draft_stat;
    entry.flag_pos=pos[draft_stat];entry.flag_neg=neg[draft_stat];
    assert(smith_adjust_bonus_entry(&entry,-1));
    assert(object_difficulty(smith_o_ptr)==draft_before-10);
    assert(smith_adjust_bonus_entry(&entry,-1));
    add_artefact_details();
    draft_after=object_difficulty(smith_o_ptr);
    assert(draft_after==33&&object_smithing_difficulty(smith_o_ptr)==33);
    assert(!too_difficult(smith_o_ptr));
    --p_ptr->skill_use[S_SMT];assert(too_difficult(smith_o_ptr));
    ++p_ptr->skill_use[S_SMT];
    (void)object_difficulty(smith_o_ptr);
}
void fixture_set_draft_stat(int stat){draft_stat=stat;}
int fixture_draft_rating(void){return draft_after;}
void fixture_check_costs(const object_type* object)
{
    *smith_o_ptr=*object;
    memset(&smith_alloy,0,sizeof(smith_alloy));
    (void)object_difficulty(smith_o_ptr);
    const int actual[]={smithing_cost.str,smithing_cost.dex,smithing_cost.con,smithing_cost.gra,
        smithing_cost.exp,smithing_cost.mithril,smithing_cost.star_iron};
    assert(!memcmp(actual,expected_fees[object->name1],sizeof(actual)));
    assert(smithing_cost.uses==3&&smithing_cost.drain==0&&smithing_cost.smt==0);
    assert(!smithing_cost.alloy_weight&&!smithing_cost.alloy_metal&&!smithing_cost.alloy_mastery);
    assert(!smithing_cost.weaponsmith&&!smithing_cost.armoursmith&&!smithing_cost.jeweller
        &&!smithing_cost.artifice&&!smithing_cost.enchantment);
}
void fixture_hero_adjustments(void)
{
    u32b old=c_info[0].flags_u;
    object_type o;artefact_type* a=&a_info[ART_CELEG_AITHORN];
    object_prep(&o,lookup_kind(a->tval,a->sval));o.name1=ART_CELEG_AITHORN;object_into_artefact(&o,a);
    c_info[0].flags_u=UNQ_SMT_TELCHAR;
    assert(object_difficulty_base(&o)==50&&object_difficulty(&o)==32);
    a=&a_info[ART_CALRIS];object_prep(&o,lookup_kind(a->tval,a->sval));
    o.name1=ART_CALRIS;object_into_artefact(&o,a);
    c_info[0].flags_u=UNQ_SMT_FEANOR;
    assert(object_difficulty_base(&o)==68&&object_difficulty(&o)==51);
    a=&a_info[ART_MAIRON];object_prep(&o,lookup_kind(a->tval,a->sval));
    o.name1=ART_MAIRON;object_into_artefact(&o,a);
    c_info[0].flags_u=UNQ_SMT_CELEBRIMBOR;
    assert(object_difficulty(&o)==25);
    u32b flags=a->flags3;a->flags3|=TR3_ENCHANTABLE;
    assert(object_difficulty(&o)==25);a->flags3=flags;
    c_info[0].flags_u=old;object_wipe(smith_o_ptr);
    puts("PASS explicit hero discounts, neutral item values and single-rounding Enchantable handling");
}
void fixture_penalties(void)
{
    static const struct {int set;u32b flag;int credit;} penalties[]={
        {2,TR2_DANGER,5},{2,TR2_DARKNESS,2},{2,TR2_AGGRAVATE,3},{2,TR2_HAUNTED,5},
        {2,TR2_VUL_COLD,4},{2,TR2_VUL_FIRE,4},{2,TR2_VUL_POIS,4},{2,TR2_TRAITOR,2},
        {3,TR3_CUMBERSOME,3},{4,TR4_UNLIGHT,5},{2,TR2_SLOWNESS,15},{2,TR2_HUNGER,3},
        {2,TR2_FEAR,5},{3,TR3_LIGHT_CURSE,3},{3,TR3_HEAVY_CURSE,4},{3,TR3_PERMA_CURSE,8}
    };
    memset(smith_a_ptr,0,sizeof(*smith_a_ptr));
    object_prep(smith_o_ptr,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    smith_o_ptr->name1=smith_a_name;smith_o_ptr->weight=15;smith_o_ptr->att+=3;smith_o_ptr->ds+=3;
    int neutral=object_difficulty(smith_o_ptr);assert(neutral==55);
    for(unsigned i=0;i<N_ELEMENTS(penalties);++i){
        u32b* flag=penalties[i].set==2?&smith_a_ptr->flags2:
            penalties[i].set==3?&smith_a_ptr->flags3:&smith_a_ptr->flags4;
        *flag=penalties[i].flag;
        assert(object_difficulty(smith_o_ptr)==neutral-penalties[i].credit);
        assert(object_smithing_difficulty(smith_o_ptr)==neutral-penalties[i].credit);
        *flag=0;
    }
    const int skills[]={S_ARC,S_STL,S_PER,S_WIL,S_SMT,S_SNG};
    const u32b skill_flags[]={TR1_ARC,TR1_STL,TR1_PER,TR1_WIL,TR1_SMT,TR1_SNG};
    for(unsigned i=0;i<N_ELEMENTS(skills);++i){
        smith_a_ptr->flags1=skill_flags[i];smith_o_ptr->skill_bonus[skills[i]]=-2;
        int credit=skills[i]==S_PER||skills[i]==S_WIL?5:7;
        assert(object_difficulty(smith_o_ptr)==neutral-credit);
        assert(object_smithing_difficulty(smith_o_ptr)==neutral-credit);
        smith_o_ptr->skill_bonus[skills[i]]=0;
    }
    smith_a_ptr->flags1=0;smith_a_ptr->flags3=TR3_LIGHT_CURSE;
    assert(object_difficulty(smith_o_ptr)==neutral-3);
    smith_o_ptr->ident|=IDENT_UNCURSED;
    assert(object_difficulty(smith_o_ptr)==neutral);
    object_wipe(smith_o_ptr);
    puts("PASS all four stat penalties, six skill penalties, sixteen drawback/curse flags and uncursing");
}
'''

DROP_HELPER = r'''
#include <assert.h>
#include "drop_system.c"
#include "object/object-difficulty.h"
void fixture_emit_object(const object_type*,const char*);
void fixture_drop_catalogue(void)
{
    drop_system_init();assert(g_drop_count>100);
    size_t count=g_drop_count;
    for(size_t i=0;i<count;++i){
        drop_entry* entry=&g_drop_entries[i];
        assert(entry->difficulty==(entry->category==DROP_CAT_SUPPLY?0:object_difficulty_base(&entry->obj)));
        fixture_emit_object(&entry->obj,"catalogue");
    }
    char path[1024];path_build(path,sizeof(path),ANGBAND_DIR_DATA,"drops.raw");
    FILE* file=fopen(path,"r+b");assert(file);assert(!fseek(file,4,SEEK_SET));
    u32b old=DROP_RAW_VERSION-1;assert(fwrite(&old,sizeof(old),1,file)==1);fclose(file);
    clear_drop_entries();assert(!load_drop_raw());
    drop_system_init();assert(g_drop_count==count);
    file=fopen(path,"rb");assert(file);drop_raw_header header;
    assert(fread(&header,sizeof(header),1,file)==1);fclose(file);
    assert(header.version==DROP_RAW_VERSION);
    printf("PASS %zu real cached drop entries and old-cache invalidation\n",count);
}
'''

TESTS = r'''
#include "cJSON.h"
#include "object/object-difficulty.h"
#include "player/player-upkeep-internal.h"
void fixture_build_negative_draft(void);
void fixture_set_draft_stat(int);
int fixture_draft_rating(void);
void fixture_check_costs(const object_type*);
void fixture_hero_adjustments(void);
void fixture_penalties(void);
void fixture_drop_catalogue(void);
static bool inject_draft;
static const char* fixture_stat_names[]={"STR","DEX","CON","GRA"};
static const char* fixture_skill_names[]={"ARCHERY","STEALTH","PERCEPTION","WILL","SMITHING","SONG"};
static const int skill_ids[]={S_ARC,S_STL,S_PER,S_WIL,S_SMT,S_SNG};
static int number(const cJSON* row,const char* name)
{const cJSON* v=cJSON_GetObjectItemCaseSensitive(row,name);return cJSON_IsNumber(v)?v->valueint:0;}
static void field(cJSON* row,const char* name,int value){cJSON_AddNumberToObject(row,name,value);}
void fixture_emit_object(const object_type* o,const char* group)
{
    if(!o->k_idx)return;
    int base=object_difficulty_base(o);assert(object_difficulty((object_type*)o)==base);
    if(object_uses_smithing_difficulty(o))assert(object_smithing_difficulty(o)==base);
    cJSON* row=cJSON_CreateObject();assert(row);
    cJSON_AddStringToObject(row,"group",group);
    cJSON_AddStringToObject(row,"type",o->name1?"artefact":object_has_ego(o)?"special":"normal");
    field(row,"idx",o->name1);field(row,"k_idx",o->k_idx);field(row,"difficulty",base);
    field(row,"att",o->att);field(row,"evn",o->evn);field(row,"dd",o->dd);field(row,"ds",o->ds);
    field(row,"pd",o->pd);field(row,"ps",o->ps);field(row,"pval",o->pval);field(row,"weight",o->weight);
    object_kind* k=&k_info[o->k_idx];
    field(row,"tval",o->tval);field(row,"sval",o->sval);field(row,"base_att",k->att);
    field(row,"base_evn",k->evn);field(row,"base_dd",k->dd);field(row,"base_ds",k->ds);
    field(row,"base_pd",k->pd);field(row,"base_ps",k->ps);field(row,"base_pval",k->pval);
    field(row,"base_level",k->level);field(row,"base_weight",k->weight);
    cJSON* stats=cJSON_AddObjectToObject(row,"stat_bonus_values");
    cJSON* skills=cJSON_AddObjectToObject(row,"skill_bonus_values");
    for(int i=0;i<A_MAX;++i)field(stats,fixture_stat_names[i],o->stat_bonus[i]);
    for(unsigned i=0;i<N_ELEMENTS(skill_ids);++i)field(skills,fixture_skill_names[i],o->skill_bonus[skill_ids[i]]);
    cJSON* abilities=cJSON_AddArrayToObject(row,"ability_list");
    for(int i=0;i<o->abilities;++i){cJSON* a=cJSON_CreateArray();
        cJSON_AddItemToArray(a,cJSON_CreateNumber(o->skilltype[i]));
        cJSON_AddItemToArray(a,cJSON_CreateNumber(o->abilitynum[i]));cJSON_AddItemToArray(abilities,a);}
    u32b flags[4];object_flags4(o,&flags[0],&flags[1],&flags[2],&flags[3]);
    cJSON* names=cJSON_AddArrayToObject(row,"flags");
    for(unsigned i=0;i<N_ELEMENTS(audit_flags);++i)
        if(flags[audit_flags[i].set]&audit_flags[i].bit)cJSON_AddItemToArray(names,cJSON_CreateString(audit_flags[i].name));
    char* line=cJSON_PrintUnformatted(row);assert(line);printf("OBJECT %s\n",line);free(line);cJSON_Delete(row);
}
static void fixed_catalogue(void)
{
    int count=0;
    for(int idx=1;idx<ART_ULTIMATE;++idx){
        artefact_type* a=&a_info[idx];if(!a->tval||!a->name[0])continue;
        object_type o;object_prep(&o,lookup_kind(a->tval,a->sval));
        o.name1=idx;object_into_artefact(&o,a);
        fixture_check_costs(&o);fixture_emit_object(&o,"fixed");++count;
    }
    object_wipe(smith_o_ptr);printf("PASS %d fixed artefacts and preserved resource fees\n",count);
}
static void python_generated(void)
{
    FILE* file=fopen(getenv("SMITHING_DIFFICULTY_INPUT"),"rb");assert(file);
    assert(!fseek(file,0,SEEK_END));long size=ftell(file);assert(size>0&&size<20000000);
    rewind(file);char* text=malloc(size+1);assert(text);assert(fread(text,1,size,file)==(size_t)size);
    text[size]=0;fclose(file);cJSON* rows=cJSON_Parse(text);free(text);assert(cJSON_IsArray(rows));
    int count=0;const cJSON* row;
    cJSON_ArrayForEach(row,rows){
        int kind=number(row,"k_idx");if(!kind)continue;
        object_type o;object_prep(&o,kind);
        object_set_ego_prefix(&o,number(row,"prefix_idx"));object_set_ego_suffix(&o,number(row,"suffix_idx"));
        o.att=number(row,"att");o.evn=number(row,"evn");o.dd=number(row,"dd");o.ds=number(row,"ds");
        o.pd=number(row,"pd");o.ps=number(row,"ps");o.pval=number(row,"pval");o.weight=number(row,"weight");
        const cJSON* stats=cJSON_GetObjectItemCaseSensitive(row,"stat_bonus_values");
        const cJSON* skills=cJSON_GetObjectItemCaseSensitive(row,"skill_bonus_values");
        for(int i=0;i<A_MAX;++i)o.stat_bonus[i]=number(stats,fixture_stat_names[i]);
        for(unsigned i=0;i<N_ELEMENTS(skill_ids);++i)o.skill_bonus[skill_ids[i]]=number(skills,fixture_skill_names[i]);
        o.abilities=0;const cJSON* ability;const cJSON* list=cJSON_GetObjectItemCaseSensitive(row,"ability_list");
        cJSON_ArrayForEach(ability,list){assert(o.abilities<N_ELEMENTS(o.skilltype));
            o.skilltype[o.abilities]=cJSON_GetArrayItem(ability,0)->valueint;
            o.abilitynum[o.abilities++]=cJSON_GetArrayItem(ability,1)->valueint;}
        int expected=number(row,"difficulty"),actual=object_difficulty_base(&o);
        if(expected!=actual)fprintf(stderr,"Generated row %d kind%d prefix%d suffix%d: Python%d C%d\n",
            count,kind,object_ego_prefix(&o),object_ego_suffix(&o),expected,actual);
        assert(expected==actual);assert(object_difficulty(&o)==actual);++count;
    }
    cJSON_Delete(rows);printf("PASS %d Python-generated normal/single/dual ego records against C\n",count);
}
static void real_craft(int stat)
{
    prepare(3);player_carried_extra_reset_store();
    p_ptr->pcharacter=0;current_character_profile=&c_info[0];
    p_ptr->skill_base[S_SMT]=200;p_ptr->skill_use[S_SMT]=33;
    p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]=false;
    for(int a=0;a<ABILITIES_MAX;++a)
        p_ptr->innate_ability[S_SMT][a]=p_ptr->have_ability[S_SMT][a]=true;
    fixture_set_draft_stat(stat);inject_draft=true;
    long exp=p_ptr->new_exp;keys="222222\r";do_cmd_smithing_screen();
    assert(!*keys&&!inject_draft&&fixture_draft_rating()==33);
    assert(p_ptr->smithing==330&&p_ptr->smithing_leftover==330);
    assert(p_ptr->new_exp==exp&&forge_uses(5,5)==3);
    for(int i=0;i<3;++i)work_one();
    keys="\r";do_cmd_smithing_screen();assert(!*keys&&p_ptr->smithing==327);
    for(int i=0;i<327;++i)work_one();
    assert(!p_ptr->smithing&&!p_ptr->smithing_leftover&&!smith_o_ptr->k_idx);
    assert(p_ptr->new_exp==exp&&forge_uses(5,5)==0);
    object_type* made=NULL;
    for(int i=0;i<INVEN_TOTAL;++i)if(inventory[i].k_idx&&inventory[i].name1>=z_info->art_rand_max)made=&inventory[i];
    for(int i=0;i<player_carried_extra_entry_count();++i){object_type* o=player_inventory_object(CARRIED_EXTRA_INDEX+i);
        if(o&&o->k_idx&&o->name1>=z_info->art_rand_max)made=o;}
    assert(made&&made->stat_bonus[stat]==-2&&object_smithing_difficulty(made)==33);
    object_copy(&inventory[INVEN_WIELD],made);p_ptr->equip_cnt=1;calc_bonuses();
    assert(p_ptr->stat_equip_mod[stat]==-2);
    printf("PASS real %s -2: preview/gate55->33, Accept330 turns, Resume, exact fees, final item/equipped penalty\n",fixture_stat_names[stat]);
}
static void difficulty_checks(void)
{
    scenario="unified difficulty";prepare(30);
    p_ptr->pcharacter=0;current_character_profile=&c_info[0];
    p_ptr->skill_use[S_SMT]=p_ptr->skill_base[S_SMT]=1000;
    p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]=false;
    fixed_catalogue();python_generated();fixture_drop_catalogue();
    fixture_penalties();fixture_hero_adjustments();
    for(int stat=0;stat<A_MAX;++stat)real_craft(stat);
}
'''



def generated_records():
    objects = calc.parse_object_file(ROOT / "lib/edit/object.txt")
    egos = calc.parse_special_file(ROOT / "lib/edit/special.txt")
    by_ego = {e["idx"]: e for e in egos}
    calc.populate_objects_dict(objects)
    calc.parse_ability_file(ROOT / "lib/edit/ability.txt")
    rows = calc.generate_normal_variants(objects)
    for ego in egos:
        rows.extend(calc.generate_special_variants(ego, objects))
    rows.extend(calc.generate_dual_ego_variants(egos, objects))
    for row in rows:
        row["difficulty"] = calc.calculate_difficulty(row)
        if row["type"] == "special":
            ego = by_ego[row["special_idx"]]
            row["prefix_idx" if ego["is_prefix"] else "suffix_idx"] = ego["idx"]
    return rows


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = generated_records()
    input_file = OUT / "python-generated.json"
    input_file.write_text(json.dumps(rows), encoding="utf-8")
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    flag_source = (ROOT / "src/init/init-flags.c").read_text(encoding="utf-8")
    flags = re.findall(r'\{\s*"([A-Z0-9_]+)"\s*,\s*TR([1-4])\s*,\s*(TR[1-4]_[A-Z0-9_]+)\s*\}', flag_source)
    assert len(flags) > 90
    prefix += "static const struct {const char* name;int set;u32b bit;} audit_flags[]={\n"
    prefix += ",\n".join('{"%s",%d,%s}' % (name, int(group)-1, symbol) for name, group, symbol in flags) + "\n};\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    work = CHECKS[:CHECKS.index("static void checks(void)")]
    work = work.replace('char __wrap_inkey(void)\n{',
        'char __wrap_inkey(void)\n{\n    if(inject_draft){inject_draft=false;fixture_build_negative_draft();}')
    declarations = "static bool inject_draft;\nvoid fixture_build_negative_draft(void);\n"
    tests = TESTS.replace("static bool inject_draft;\n", "")
    source = OUT / "check.c"
    source.write_text(prefix + declarations + work + tests + init
        + "    ANGBAND_DIR_APEX=ANGBAND_DIR_USER; assert(!init_flavor_info()); flavor_init(); difficulty_checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    costs = "static const int expected_fees[ART_ULTIMATE][7]={\n"
    costs += ",\n".join("[%d]={%s}" % (idx, ",".join(map(str, fees)))
        for idx, fees in RESOURCE_FEES.items()) + "\n};\n"
    smith = OUT / "smith.c"
    smith.write_text(SMITH + "\n#include <assert.h>\n" + costs + SMITH_HELPERS, encoding="utf-8")
    drop = OUT / "drop.c"
    drop.write_text(DROP_HELPER, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj", "/src/drop_system.c.obj"))))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["SMITHING_DIFFICULTY_INPUT"] = str(input_file)
    exe = OUT / "check.exe"
    wraps = ("inkey", "open_inventory_item_select_menu", "handle_stuff", "msg_print", "request_command",
        "process_command", "get_sdl_config_path", "tutorial_game_action_allowed")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(smith), str(drop), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), profile], cwd=profile, env=env,
            text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    (OUT / "engine.log").write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print("\n".join(result.stdout.splitlines()[-15:]), flush=True)
        result.check_returncode()
    artefacts = {a["idx"]: a for a in calc.parse_artefact_file(ROOT / "lib/edit/artefact.txt")[0]}
    count = 0
    for line in result.stdout.splitlines():
        if not line.startswith("OBJECT "):
            print(line)
            continue
        row = json.loads(line[7:])
        if row["group"] == "fixed":
            expected = calc.calculate_difficulty(dict(artefacts[row["idx"]], type="artefact"))
            assert expected == row["difficulty"], (row["idx"], expected, row["difficulty"])
        expected = calc.calculate_difficulty(row)
        assert expected == row["difficulty"], (row, expected)
        count += 1
    print(f"PASS {count} actual fixed/catalogue records against Python; shared item/drop/crafting parity")


if __name__ == "__main__":
    main()
