#!/usr/bin/env python3
"""Roundtrip achievement/player-poison state with production save/load codecs.

Requires current build-standard engine objects. Uses temporary profiles only.
Legacy expansion layouts use committed production writers from revisions
658f9417 (before achievements), 3bf72847 (1.1.0.7) and 8b056f4c (1.1.0.8).
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-save-check"

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
void fixture_legacy_extra(void);
void fixture_legacy_monster(const monster_type*);
void fixture_previous_extra(void);
void fixture_previous_monster(const monster_type*);
void fixture_v8_extra(void);
void fixture_v8_monster(const monster_type*);
size_t fixture_write(int legacy, const monster_type* monster, byte* buffer,
    size_t capacity, size_t* player_end)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    if (legacy == 1) fixture_legacy_extra();
    else if (legacy == 2) fixture_previous_extra();
    else if (legacy == 3) fixture_v8_extra();
    else save_write_extra();
    *player_end = (size_t)SDL_TellIO(fff);
    save_wr_u32b(0xA1B2C3D4U);
    if (legacy == 1) fixture_legacy_monster(monster);
    else if (legacy == 2) fixture_previous_monster(monster);
    else if (legacy == 3) fixture_v8_monster(monster);
    else save_wr_monster(monster);
    save_wr_u32b(0xB4C3D2E1U);
    save_wr_u32b(v_stamp); save_wr_u32b(x_stamp);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
size_t fixture_write_object(const object_type* object, byte* buffer, size_t capacity)
{
    fff=SDL_IOFromMem(buffer,capacity); assert(fff);
    xor_byte=0;v_stamp=x_stamp=save_byte_offset=0;write_error=false;
    save_wr_item(object);save_wr_u32b(0x1234ABCDU);
    size_t length=(size_t)SDL_TellIO(fff);
    assert(!write_error);SDL_CloseIO(fff);fff=NULL;return length;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
size_t fixture_read_offset;
int fixture_read(const byte* buffer, size_t length, int extra,
    monster_type* monster, size_t player_end)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = VERSION_MAJOR; sf_minor = VERSION_MINOR;
    sf_patch = VERSION_PATCH; sf_extra = extra;
    /* FEATURE_FLAGS */
    int result = load_read_extra();
    if (!result) {
        assert((size_t)SDL_TellIO(fff) == player_end);
        u32b sentinel = 0;
        load_rd_u32b(&sentinel); assert(sentinel == 0xA1B2C3D4U);
        load_rd_monster(monster);
        load_rd_u32b(&sentinel); assert(sentinel == 0xB4C3D2E1U);
        result = rd_savefile_checksums();
        assert((size_t)SDL_TellIO(fff) == length);
    }
    fixture_read_offset=(size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
void fixture_read_object(const byte* buffer,size_t length,object_type* object)
{
    fff=SDL_IOFromConstMem(buffer,length);assert(fff);
    xor_byte=0;v_check=x_check=load_byte_offset=0;
    sf_major=VERSION_MAJOR;sf_minor=VERSION_MINOR;sf_patch=VERSION_PATCH;sf_extra=VERSION_EXTRA;
    assert(load_rd_item(object)==0);
    u32b sentinel;load_rd_u32b(&sentinel);assert(sentinel==0x1234ABCDU);
    assert((size_t)SDL_TellIO(fff)==length);SDL_CloseIO(fff);fff=NULL;
}
'''

TESTS = r'''
#include "achievements.h"
#include "achievements-lore.h"
size_t fixture_write(int,const monster_type*,byte*,size_t,size_t*);
int fixture_read(const byte*,size_t,int,monster_type*,size_t);
size_t fixture_write_object(const object_type*,byte*,size_t);
void fixture_read_object(const byte*,size_t,object_type*);
extern size_t fixture_read_offset;
static byte current[65536], legacy[65536], modified[65536], plain[65536];
static void decode(const byte* source, byte* target, size_t size)
{
    byte previous = 0;
    for (size_t i=0;i<size;i++)
    { byte value=source[i]; target[i]=value^previous; previous=value; }
}
static void encode(const byte* source, byte* target, size_t size)
{
    byte previous=0;
    for (size_t i=0;i<size;i++) { previous^=source[i]; target[i]=previous; }
}
static void clear_for_load(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    p_ptr->achievement_flags=0xFFFFFFFFU;
    p_ptr->achievement_kills=p_ptr->achievement_singing_turns=0xFFFFFFFFU;
    p_ptr->achievement_comeback_depth=29;
    p_ptr->achievement_discovery_flags=0xFFFFFFFFU;
    p_ptr->achievement_rage_epoch=0xFFFFFFFFU;
    p_ptr->achievement_lore_flags=0xFFFFFFFFU;
    p_ptr->achievement_ringil_wounds=255;
    p_ptr->achievement_glorfindel_depth=30;
    p_ptr->achievement_balrogs=255;
    p_ptr->disease_immunity=255;
    memset(p_ptr->achievement_artefact_rings,0xFF,sizeof(p_ptr->achievement_artefact_rings));
    memset(p_ptr->achievement_frozen_tiles,0xFF,sizeof(p_ptr->achievement_frozen_tiles));
}
static void prepare_player(void)
{
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(10);
    character_generated=true; p_ptr->playing=true;
    p_ptr->chp=1; p_ptr->mhp=80;
    p_ptr->achievement_flags=SIL_ACH_RUN_FLAGS|0x80000000U;
    p_ptr->achievement_kills=0x12345678U; p_ptr->achievement_singing_turns=70000;
    p_ptr->achievement_comeback_depth=10;
    p_ptr->achievement_fall_depth=8;
    p_ptr->achievement_discovery_flags=SIL_ACH_DISC_FLAGS|0x80000000U;
    p_ptr->achievement_rage_epoch=0x11223344U;
    p_ptr->achievement_lore_flags=SIL_ACH_LORE_FLAGS;
    p_ptr->achievement_ringil_wounds=6;
    p_ptr->achievement_balrogs=0x45;
    p_ptr->achievement_glorfindel_depth=5;
    p_ptr->achievement_artefact_rings[0]=z_info->art_rand_max;
    p_ptr->achievement_artefact_rings[1]=z_info->art_rand_max+1;
    p_ptr->achievement_water_from_y=1;p_ptr->achievement_water_from_x=2;
    p_ptr->achievement_water_entry_y=3;p_ptr->achievement_water_entry_x=4;
    p_ptr->achievement_poison_from_y=5;p_ptr->achievement_poison_from_x=6;
    p_ptr->achievement_poison_entry_y=7;p_ptr->achievement_poison_entry_x=8;
    p_ptr->achievement_labyrinth_index=9;
    p_ptr->achievement_labyrinth_y=10;p_ptr->achievement_labyrinth_x=11;
    int ice=12*MAX_DUNGEON_WID+13;
    p_ptr->achievement_frozen_tiles[ice/8]|=1u<<(ice%8);
    ++ice;p_ptr->achievement_frozen_tiles[ice/8]|=1u<<(ice%8);
    int door=8*MAX_DUNGEON_WID+9;
    p_ptr->achievement_jammed_doors[door/8] |= 1u<<(door%8);
    ++door;
    p_ptr->achievement_jammed_doors[door/8] |= 1u<<(door%8);
    p_ptr->tulkas_target_r_idx=41; p_ptr->niena_level=17;
    p_ptr->skill_base[S_SNG]=20;
    p_ptr->insight_points=29; p_ptr->insight_milestones=1;
    p_ptr->insight_monster_types=RF3_ORC;
    p_ptr->insight_stat_invested[A_STR]=4;
    p_ptr->insight_ability_upgraded[S_SNG][1]=1;
    p_ptr->insight_ruleset=INSIGHT_RULESET_REWORKED; p_ptr->light_dimmed=true;
    p_ptr->free_active_weapon_change_used=1;
    p_ptr->quest_followup_state[0]=QUEST_STATE_ACTIVE;
    p_ptr->quest_followup_depth[0]=12; p_ptr->quest_followup_progress[0]=7;
    turn=888; playerturn=777;
    SDL_strlcpy(op_ptr->full_name,"Achievement codec",sizeof(op_ptr->full_name));
    SDL_strlcpy(p_ptr->died_from,"alive",sizeof(p_ptr->died_from));
}
static void assert_player_tail(void)
{
    assert(p_ptr->tulkas_target_r_idx==41 && p_ptr->niena_level==17);
    assert(turn==888 && playerturn==777);
    assert(!strcmp(op_ptr->full_name,"Achievement codec"));
    assert(p_ptr->chp==1 && p_ptr->mhp==80);
    assert(p_ptr->skill_base[S_SNG]==20);
    assert(p_ptr->insight_points==29 && p_ptr->insight_milestones==1);
    assert(p_ptr->insight_monster_types==RF3_ORC);
    assert(p_ptr->insight_stat_invested[A_STR]==4 && p_ptr->insight_ability_upgraded[S_SNG][1]);
    assert(p_ptr->insight_ruleset==INSIGHT_RULESET_REWORKED && p_ptr->light_dimmed);
    assert(p_ptr->free_active_weapon_change_used==1);
    assert(p_ptr->quest_followup_state[0]==QUEST_STATE_ACTIVE);
    assert(p_ptr->quest_followup_depth[0]==12 && p_ptr->quest_followup_progress[0]==7);
}
static void assert_discovery_defaults(const monster_type* restored)
{
    assert(!p_ptr->achievement_discovery_flags && !p_ptr->achievement_rage_epoch);
    assert(p_ptr->achievement_water_from_y==-1 && p_ptr->achievement_water_entry_x==-1);
    assert(p_ptr->achievement_poison_from_y==-1 && p_ptr->achievement_poison_entry_x==-1);
    assert(p_ptr->achievement_labyrinth_index==-1 && p_ptr->achievement_labyrinth_y==-1);
    assert(!restored->achievement_doom_rage_epoch && !restored->achievement_elbereth_turn);
    assert(!restored->achievement_first_wolf);
    assert(!p_ptr->achievement_lore_flags && !p_ptr->achievement_ringil_wounds);
    assert(p_ptr->achievement_glorfindel_depth==-1 && !p_ptr->achievement_balrogs);
    for(int i=0;i<3;i++)assert(!p_ptr->achievement_artefact_rings[i]);
    for(size_t i=0;i<sizeof(p_ptr->achievement_frozen_tiles);i++)assert(!p_ptr->achievement_frozen_tiles[i]);
}
static void test_save(void)
{
    assert(VERSION_MAJOR==1 && VERSION_MINOR==1 && VERSION_PATCH==0 && VERSION_EXTRA==9);
    prepare_player();
    monster_type source={0}, restored;
    source.r_idx=41; source.image_r_idx=7; source.fy=8; source.fx=9;
    source.mflag=MFLAG_PLAYER_KNOCKBACK;
    source.hp=source.maxhp=120; source.poisoned=27; source.player_poisoned=11; source.potion_poisoned=7;
    source.vengeance=1; source.smite_recovery=2;
    source.achievement_doom_rage_epoch=0x11223344U;
    source.achievement_elbereth_turn=778;source.achievement_first_wolf=true;
    source.ai.observations[MON_AI_FIRE].value=2;
    source.ai.observations[MON_AI_FIRE].ttl=40;
    source.ai.observations[MON_AI_FIRE].turn=775;
    size_t current_end,legacy_end;
    size_t size=fixture_write(false,&source,current,sizeof(current),&current_end);
    size_t oldsize=fixture_write(true,&source,legacy,sizeof(legacy),&legacy_end);
    assert(current_end==legacy_end+78 && size==oldsize+91);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(current,size,VERSION_EXTRA,&restored,current_end)==0);
    assert_player_tail();
    assert(p_ptr->achievement_flags==SIL_ACH_RUN_FLAGS);
    assert(p_ptr->achievement_kills==0x12345678U && p_ptr->achievement_singing_turns==70000);
    assert(p_ptr->achievement_comeback_depth==10);
    assert(restored.poisoned==27 && restored.player_poisoned==11 && restored.potion_poisoned==7);
    assert(!(restored.mflag & MFLAG_PLAYER_KNOCKBACK));
    assert(p_ptr->achievement_fall_depth==8);
    int first=8*MAX_DUNGEON_WID+9, second=first+1;
    assert(p_ptr->achievement_jammed_doors[first/8] & (1u<<(first%8)));
    assert(p_ptr->achievement_jammed_doors[second/8] & (1u<<(second%8)));
    assert(restored.vengeance==1 && restored.smite_recovery==2);
    assert(restored.ai.observations[MON_AI_FIRE].value==2);
    assert(restored.ai.observations[MON_AI_FIRE].ttl==40);
    assert(restored.ai.observations[MON_AI_FIRE].turn==775);
    assert(p_ptr->achievement_discovery_flags==SIL_ACH_DISC_FLAGS);
    assert(p_ptr->achievement_rage_epoch==0x11223344U);
    assert(p_ptr->achievement_water_from_y==1 && p_ptr->achievement_water_from_x==2);
    assert(p_ptr->achievement_water_entry_y==3 && p_ptr->achievement_water_entry_x==4);
    assert(p_ptr->achievement_poison_from_y==5 && p_ptr->achievement_poison_from_x==6);
    assert(p_ptr->achievement_poison_entry_y==7 && p_ptr->achievement_poison_entry_x==8);
    assert(p_ptr->achievement_labyrinth_index==9 && p_ptr->achievement_labyrinth_y==10 && p_ptr->achievement_labyrinth_x==11);
    int ice=12*MAX_DUNGEON_WID+13;
    assert(p_ptr->achievement_frozen_tiles[ice/8]&(1u<<(ice%8)));
    ++ice;assert(p_ptr->achievement_frozen_tiles[ice/8]&(1u<<(ice%8)));
    assert(restored.achievement_doom_rage_epoch==0x11223344U);
    assert(restored.achievement_elbereth_turn==778 && restored.achievement_first_wolf);
    assert(p_ptr->achievement_lore_flags==SIL_ACH_LORE_FLAGS);
    assert(p_ptr->achievement_balrogs==0x45);
    assert(p_ptr->achievement_ringil_wounds==6 && p_ptr->achievement_glorfindel_depth==5);
    assert(p_ptr->achievement_artefact_rings[0]==z_info->art_rand_max
        && p_ptr->achievement_artefact_rings[1]==z_info->art_rand_max+1
        && p_ptr->achievement_artefact_rings[2]==0);
    puts("Current 1.1.0.9: achievement fields, post-block quests, player poison, AI, sentinels and both checksums PASS");

    size_t v8_end;
    size_t v8_size=fixture_write(3,&source,modified,sizeof(modified),&v8_end);
    assert(current_end==v8_end+17 && size==v8_size+17);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(modified,v8_size,8,&restored,v8_end)==0);
    assert_player_tail();
    assert(!p_ptr->achievement_lore_flags && !p_ptr->achievement_ringil_wounds);
    assert(p_ptr->achievement_glorfindel_depth==-1 && !p_ptr->achievement_balrogs);
    assert(!p_ptr->achievement_artefact_rings[0] && !p_ptr->achievement_artefact_rings[1]
        && !p_ptr->achievement_artefact_rings[2]);
    assert(!(p_ptr->achievement_flags & (SIL_ACH_RUN_CHASM_SHAFT_ROUTE | SIL_ACH_RUN_COMEBACK)));
    assert(p_ptr->achievement_discovery_flags==SIL_ACH_DISC_FLAGS);
    assert(p_ptr->achievement_fall_depth==8 && restored.potion_poisoned==7);
    assert(restored.achievement_doom_rage_epoch==0x11223344U);
    assert(!p_ptr->disease_immunity);
    puts("Legacy 1.1.0.8: exact stream/checksum alignment, expansion tails preserved, safe lore/Balrog/immunity and route/comeback defaults PASS");

    prepare_player(); p_ptr->achievement_flags &= ~SIL_ACH_RUN_CHASM_SHAFT_ROUTE;
    size_t invalid_end;
    size_t invalid_size=fixture_write(false,&source,modified,sizeof(modified),&invalid_end);
    clear_for_load();
    assert(fixture_read(modified,invalid_size,VERSION_EXTRA,&restored,invalid_end)==0);
    assert(!(p_ptr->achievement_flags & SIL_ACH_RUN_CHASM_SHAFT_ROUTE));
    puts("Current route: a disqualified descent remains disqualified after save/resume PASS");

    object_type forged={0},loaded;
    object_prep(&forged,lookup_kind(TV_SOFT_ARMOR,SV_ARMOUR_OF_GALVORN));
    forged.ident|=IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS;
    size_t object_size=fixture_write_object(&forged,modified,sizeof(modified));
    memset(&loaded,0,sizeof(loaded));fixture_read_object(modified,object_size,&loaded);
    assert((loaded.ident&(IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS))
        ==(IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS));
    puts("Current 1.1.0.9: object codec preserves both own-forged provenance bits PASS");

    size_t previous_end;
    size_t previous_size=fixture_write(2,&source,modified,sizeof(modified),&previous_end);
    assert(current_end==previous_end+63 && size==previous_size+74);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(modified,previous_size,7,&restored,previous_end)==0);
    assert_player_tail();
    assert_discovery_defaults(&restored);
    assert(p_ptr->achievement_flags==(SIL_ACH_RUN_FLAGS
        & ~(SIL_ACH_RUN_FALLING | SIL_ACH_RUN_CHASM_SHAFT_ROUTE | SIL_ACH_RUN_COMEBACK)));
    assert(p_ptr->achievement_fall_depth==-1 && restored.potion_poisoned==0);
    for(size_t i=0;i<sizeof(p_ptr->achievement_jammed_doors);++i)
        assert(!p_ptr->achievement_jammed_doors[i]);
    assert(restored.player_poisoned==11);
    puts("Legacy 1.1.0.7: existing run/poison history retained, new provenance defaults empty PASS");

    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(legacy,oldsize,6,&restored,legacy_end)==0);
    assert_player_tail();
    assert_discovery_defaults(&restored);
    assert(p_ptr->achievement_flags==0 && p_ptr->achievement_kills==0);
    assert(p_ptr->achievement_singing_turns==0 && p_ptr->achievement_comeback_depth==-1);
    assert(restored.poisoned==27 && restored.player_poisoned==0 && restored.potion_poisoned==0);
    assert(restored.vengeance==1 && restored.smite_recovery==2);
    assert(restored.ai.observations[MON_AI_FIRE].value==2);
    p_ptr->playing=true; p_ptr->depth=0;
    sil_achievements_on_escape(1);
    assert(sil_achievements_mask(0) & (1U<<SIL_ACH_BACK_TO_STARLIGHT));
    assert(!(sil_achievements_mask(0) & (1U<<SIL_ACH_THE_QUIET_ROAD)));
    p_ptr->depth=10;
    sil_achievements_on_monster_killed(1,-1);
    assert(!(sil_achievements_mask(0) & (1U<<SIL_ACH_BY_A_THREAD)));
    puts("Legacy 1.1.0.6: exact stream/checksum alignment, safe defaults, no false pacifist or 1-HP achievement PASS");

    prepare_player(); source.mflag |= MFLAG_PLAYER_POISON;
    oldsize=fixture_write(true,&source,legacy,sizeof(legacy),&legacy_end);
    clear_for_load();
    assert(fixture_read(legacy,oldsize,6,&restored,legacy_end)==0);
    assert(restored.poisoned==27 && restored.player_poisoned==27);
    source.mflag &= ~MFLAG_PLAYER_POISON;
    puts("Legacy 1.1.0.6: player poison flag retains pending quest credit PASS");

    for (int version=6;version<=8;version++) {
        int writer=version-5;
        for (int countdown=0;countdown<=100;countdown++) {
            prepare_player();
            p_ptr->diseased=countdown;
            p_ptr->disease_name=countdown?8:0;
            p_ptr->disease_cure=countdown?SV_FOOD_SICKNESS:0;
            p_ptr->disease_knowledge=countdown?DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE:0;
            p_ptr->stat_disease[A_CON]=countdown?-1:0;
            p_ptr->stat_drain[A_CON]=-2;
            size_t disease_end;
            size_t disease_size=fixture_write(writer,&source,modified,sizeof(modified),&disease_end);
            clear_for_load();
            assert(fixture_read(modified,disease_size,version,&restored,disease_end)==0);
            assert_player_tail();
            assert(p_ptr->diseased==countdown*2 && !p_ptr->disease_immunity);
            assert(p_ptr->disease_name==(countdown?8:0));
            assert(p_ptr->stat_disease[A_CON]==(countdown?-1:0));
            assert(p_ptr->stat_drain[A_CON]==-2);
            disease_size=fixture_write(false,&source,modified,sizeof(modified),&disease_end);
            clear_for_load();
            assert(fixture_read(modified,disease_size,VERSION_EXTRA,&restored,disease_end)==0);
            assert(p_ptr->diseased==countdown*2 && !p_ptr->disease_immunity);
            assert_player_tail();
        }
    }
    puts("Legacy 1.1.0.6/7/8: all 101 historical countdowns migrate once, immunity defaults empty, disease/skill/expansion tails and checksums preserved across 606 codec roundtrips PASS");

    for (int poison=-5;poison<=99;poison+=104) {
        prepare_player(); source.player_poisoned=poison;
        size=fixture_write(false,&source,current,sizeof(current),&current_end);
        clear_for_load();
        assert(fixture_read(current,size,VERSION_EXTRA,&restored,current_end)==0);
        assert(restored.player_poisoned==(poison<0?0:27));
        assert(restored.potion_poisoned<=restored.player_poisoned);
    }
    source.player_poisoned=11;
    for (int poison=-5;poison<=99;poison+=104) {
        prepare_player(); source.potion_poisoned=poison;
        size=fixture_write(false,&source,current,sizeof(current),&current_end);
        clear_for_load();
        assert(fixture_read(current,size,VERSION_EXTRA,&restored,current_end)==0);
        assert(restored.potion_poisoned==(poison<0?0:11));
    }
    prepare_player(); p_ptr->achievement_comeback_depth=-1;
    size=fixture_write(false,&source,current,sizeof(current),&current_end);
    clear_for_load();
    assert(fixture_read(current,size,VERSION_EXTRA,&restored,current_end)==0);
    assert(!(p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK));
    puts("Current 1.1.0.9: player poison clamps to [0,total poison], invalid comeback depth clears pending condition PASS");

    decode(current,plain,size);
    const byte marker[]={0xAC,SIL_ACH_RUN_FLAGS,0,0,0,0x78,0x56,0x34,0x12,0x70,0x11,1,0,0xFF,0xFF};
    size_t found=0; int matches=0;
    for(size_t i=0;i+sizeof(marker)<=current_end;i++)
        if(!memcmp(plain+i,marker,sizeof(marker))) { found=i; matches++; }
    assert(matches==1); plain[found]=0xAD;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
    puts("Current 1.1.0.9: corrupt achievement block marker rejected PASS");
    decode(current,plain,size);
    const byte extension[]={0xAD,8,0,2,0,8,9,8,10};
    found=0; matches=0;
    for(size_t i=0;i+sizeof(extension)<=current_end;i++)
        if(!memcmp(plain+i,extension,sizeof(extension))) {found=i; matches++;}
    assert(matches==1);
    for(int bad=0;bad<4;bad++) {
        decode(current,plain,size);
        if(bad==0) plain[found]=0xAE;
        if(bad==1) plain[found+3]=plain[found+4]=0xFF;
        if(bad==2) plain[found+5]=0xFF;
        if(bad==3) plain[found+8]=9;
        encode(plain,modified,size); clear_for_load();
        assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
    }
    puts("Current 1.1.0.9: potion ownership clamps; invalid extension/count/coordinates/duplicate doors rejected PASS");
    decode(current,plain,size);
    const byte discovery[]={0xAE,(byte)SIL_ACH_DISC_FLAGS,(byte)(SIL_ACH_DISC_FLAGS>>8),
        (byte)(SIL_ACH_DISC_FLAGS>>16),(byte)(SIL_ACH_DISC_FLAGS>>24),0x44,0x33,0x22,0x11,
        1,0,2,0,3,0,4,0,5,0,6,0,7,0,8,0,9,0,10,0,11,0,2,0,12,13,12,14};
    found=0;matches=0;
    for(size_t i=0;i+sizeof(discovery)<=current_end;i++)
        if(!memcmp(plain+i,discovery,sizeof(discovery))){found=i;matches++;}
    assert(matches==1);
    for(int bad=0;bad<7;bad++) {
        decode(current,plain,size);
        if(bad==0)plain[found]=0xAF;
        if(bad==1)plain[found+9]=plain[found+10]=0xFF;
        if(bad==2)plain[found+17]=plain[found+18]=0xFF;
        if(bad==3)plain[found+25]=25;
        if(bad==4)plain[found+31]=plain[found+32]=0xFF;
        if(bad==5)plain[found+33]=0xFF;
        if(bad==6){plain[found+35]=12;plain[found+36]=13;}
        encode(plain,modified,size);clear_for_load();
        assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
        /* Reject in the field decoder itself, before the checksummed tail. */
        assert(fixture_read_offset<current_end);
    }
    puts("Current 1.1.0.9: invalid discovery marker/crossing/poison/labyrinth/ice count/ice coordinate/duplicate ice rejected before checksum PASS");
    decode(current,plain,size);
    const byte lore[]={0xAF,SIL_ACH_LORE_FLAGS,0,0,0,6,5,0,
        (byte)z_info->art_rand_max,(byte)(z_info->art_rand_max>>8),
        (byte)(z_info->art_rand_max+1),(byte)((z_info->art_rand_max+1)>>8),0,0};
    found=0;matches=0;
    for(size_t i=0;i+sizeof(lore)<=current_end;i++)
        if(!memcmp(plain+i,lore,sizeof(lore))){found=i;matches++;}
    assert(matches==1);
    for(int bad=0;bad<6;bad++) {
        decode(current,plain,size);
        if(bad==0)plain[found]=0xB0;
        if(bad==1)plain[found+5]=8;
        if(bad==2)plain[found+6]=UTUMNO_DEPTH+1;
        if(bad==3)plain[found+8]=plain[found+9]=0xFF;
        if(bad==4){plain[found+10]=plain[found+8];plain[found+11]=plain[found+9];}
        if(bad==5)plain[found+8]=plain[found+9]=0;
        encode(plain,modified,size);clear_for_load();
        assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
        assert(fixture_read_offset<current_end);
    }
    puts("Current lore: invalid marker/wounds/floor/artefact ID/duplicate rings/gapped rings rejected before checksum PASS");
    decode(current,plain,size);
    const byte balrogs[]={0xB0,0x45};found=0;matches=0;
    for(size_t i=0;i+sizeof(balrogs)<=current_end;i++)
        if(!memcmp(plain+i,balrogs,sizeof(balrogs))){found=i;matches++;}
    assert(matches==1);
    for(int bad=0;bad<2;bad++) {
        decode(current,plain,size);
        if(bad==0)plain[found]=0xB1;else plain[found+1]=0x80;
        encode(plain,modified,size);clear_for_load();
        assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
        assert(fixture_read_offset<current_end);
    }
    puts("Current Balrog block: corrupt marker and impossible eighth bit rejected PASS");
    decode(current,plain,size); plain[size-1]^=1;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,VERSION_EXTRA,&restored,current_end)!=0);
    puts("Current 1.1.0.9: corrupt encoded checksum rejected by production footer validator PASS");
}
'''


def function(source, name):
    match = re.search(r"^void " + name + r"\([^\n]*\)\n\{", source, re.M)
    assert match, name
    return source[match.start():source.index("\n}", match.end()) + 2]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    def historical_writer(ref, label):
        def historical(path):
            return subprocess.check_output(["git", "show", f"{ref}:{path}"],
                                           cwd=ROOT, text=True, encoding="utf-8")
        save = historical("src/fs/save.c")
        player = historical("src/fs/save-player.c")
        player = player[:player.index("\nvoid wr_randarts(void)")]
        player = player.replace("save_write_partition_meta", f"fixture_{label}_partition_meta")
        player = player.replace("void wr_extra(void)", f"void fixture_{label}_extra(void)")
        monster = function(save, "wr_monster").replace(
            "void wr_monster(", f"void fixture_{label}_monster(")
        flags = save[save.index("#define SAVE_MON_FLAGS"):save.index('/*\n * Write a "monster"')]
        return player + '\n#include "monster/monster-ai.h"\n' + flags + "\n" + monster

    old_writer = historical_writer("658f9417", "legacy")
    previous_writer = historical_writer("3bf72847", "previous")
    v8_writer = historical_writer("8b056f4c", "v8")
    load = (ROOT / "src/fs/load.c").read_text(encoding="utf-8")
    # Use the complete load_player setup, including the Varda availability bit
    # which the inner auxiliary reader inherits from its caller.
    flags_start = load.rindex("    savefile_has_runtime_overrides =")
    flags_end = load.index(";", load.index("    savefile_has_combat_history =", flags_start)) + 1
    reader = READER.replace("    /* FEATURE_FLAGS */", load[flags_start:flags_end])
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += '#include "monster/monster-ai.h"\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += TESTS + init + "    ANGBAND_DIR_APEX=argv[2];\n    test_save(); SDL_Quit(); return 0;\n}\n"
    sources = []
    for name, text in (("check.c", harness), ("writer.c", WRITER), ("reader.c", reader), ("legacy.c", old_writer), ("previous.c", previous_writer), ("v8.c", v8_writer)):
        path = OUT / name
        path.write_text(text, encoding="utf-8")
        sources.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "-UNDEBUG",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile],
                       cwd=profile, env=env, check=True, timeout=90)


if __name__ == "__main__":
    main()
