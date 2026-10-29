#!/usr/bin/env python3
"""Roundtrip achievement/player-poison state with production save/load codecs.

Requires current build-standard engine objects. Uses temporary profiles only.
The v34, v33 and v30 fixtures remove precisely the newer fields from the production
writer, rather than maintaining hand-written alternate codecs.
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
void fixture_v34_extra(void);
void fixture_v34_monster(const monster_type*);
void fixture_v37_extra(void);
void fixture_v38_extra(void);
void fixture_v39_extra(void);
size_t fixture_write(int legacy, const monster_type* monster, byte* buffer,
    size_t capacity, size_t* player_end)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    if (legacy == 1) fixture_legacy_extra();
    else if (legacy == 2) fixture_previous_extra();
    else if (legacy == 3) fixture_v34_extra();
    else if (legacy == 4) fixture_v37_extra();
    else if (legacy == 5) fixture_v38_extra();
    else if (legacy == 6) fixture_v39_extra(); else save_write_extra();
    *player_end = (size_t)SDL_TellIO(fff);
    save_wr_u32b(0xA1B2C3D4U);
    if (legacy == 1) fixture_legacy_monster(monster);
    else if (legacy == 2) fixture_previous_monster(monster);
    else if (legacy == 3) fixture_v34_monster(monster); else save_wr_monster(monster);
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
    sf_major = extra < 0 ? VERSION_MAJOR : 0;
    sf_minor = extra < 0 ? VERSION_MINOR : 9;
    sf_patch = extra < 0 ? VERSION_PATCH : 8;
    sf_extra = extra < 0 ? VERSION_EXTRA : extra;
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
static byte current[262144], legacy[262144], modified[262144], plain[262144];
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
}
static void assert_discovery_defaults(const monster_type* restored)
{
    assert(!p_ptr->achievement_discovery_flags && !p_ptr->achievement_rage_epoch);
    assert(p_ptr->achievement_water_from_y==-1 && p_ptr->achievement_water_entry_x==-1);
    assert(p_ptr->achievement_poison_from_y==-1 && p_ptr->achievement_poison_entry_x==-1);
    assert(p_ptr->achievement_labyrinth_index==-1 && p_ptr->achievement_labyrinth_y==-1);
    assert(!restored->achievement_doom_rage_epoch && !restored->achievement_elbereth_turn);
    assert(!restored->achievement_first_wolf);
    for(size_t i=0;i<sizeof(p_ptr->achievement_frozen_tiles);i++)assert(!p_ptr->achievement_frozen_tiles[i]);
}
static void test_save(void)
{
    assert(VERSION_MAJOR*1000000 + VERSION_MINOR*10000 + VERSION_PATCH*100 + VERSION_EXTRA >= 90839);
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
    size_t v39_end;
    size_t v39_size=fixture_write(6,&source,modified,sizeof(modified),&v39_end);
    assert(current_end==v39_end+1 && size==v39_size+1);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(modified,v39_size,39,&restored,v39_end)==0);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(current,size,-1,&restored,current_end)==0);
    assert_player_tail();
    assert(p_ptr->achievement_flags==SIL_ACH_RUN_FLAGS);
    assert(p_ptr->achievement_kills==0x12345678U && p_ptr->achievement_singing_turns==70000);
    assert(p_ptr->achievement_comeback_depth==10);
    assert(p_ptr->achievement_balrogs==0x45);
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
    assert(p_ptr->achievement_ringil_wounds==6 && p_ptr->achievement_glorfindel_depth==5);
    assert(p_ptr->achievement_artefact_rings[0]==z_info->art_rand_max
        && p_ptr->achievement_artefact_rings[1]==z_info->art_rand_max+1
        && p_ptr->achievement_artefact_rings[2]==0);
    puts("Current format: achievement fields, post-block quests, player poison, AI, sentinels and both checksums PASS");

    size_t v37_end;
    size_t v37_size=fixture_write(4,&source,modified,sizeof(modified),&v37_end);
    assert(current_end==v37_end+17 && size==v37_size+17);
    // Previous writers have no lore block; older route provenance remains exact.
    for(int previous=35;previous<=37;previous++) {
        clear_for_load();memset(&restored,0,sizeof(restored));
        assert(fixture_read(modified,v37_size,previous,&restored,v37_end)==0);
        assert_player_tail();
        assert(p_ptr->achievement_flags==((previous<37
            ? SIL_ACH_RUN_FLAGS & ~SIL_ACH_RUN_CHASM_SHAFT_ROUTE : SIL_ACH_RUN_FLAGS)
            & ~SIL_ACH_RUN_COMEBACK));
        assert(!p_ptr->achievement_balrogs);
        assert(!p_ptr->achievement_lore_flags && !p_ptr->achievement_ringil_wounds);
        assert(p_ptr->achievement_glorfindel_depth==-1);
        for(int i=0;i<3;i++)assert(!p_ptr->achievement_artefact_rings[i]);
    }
    puts("Legacy v35/v36/v37: safe lore defaults, correct historical route eligibility, exact stream/checksum alignment PASS");
    size_t v38_end;
    prepare_player();
    size_t v38_size=fixture_write(5,&source,modified,sizeof(modified),&v38_end);
    assert(current_end==v38_end+3 && size==v38_size+3);
    clear_for_load();memset(&restored,0,sizeof(restored));
    assert(fixture_read(modified,v38_size,38,&restored,v38_end)==0);
    assert_player_tail();
    assert(!p_ptr->achievement_balrogs);
    assert(!(p_ptr->achievement_flags&SIL_ACH_RUN_COMEBACK));
    assert(p_ptr->achievement_ringil_wounds==6);
    puts("Legacy v38: empty adventure Balrog mask, unknown healing window cleared, lore and checksums preserved PASS");
    prepare_player();p_ptr->achievement_flags &= ~SIL_ACH_RUN_CHASM_SHAFT_ROUTE;
    size_t invalid_end;
    size_t invalid_size=fixture_write(false,&source,modified,sizeof(modified),&invalid_end);
    clear_for_load();memset(&restored,0,sizeof(restored));
    assert(fixture_read(modified,invalid_size,-1,&restored,invalid_end)==0);
    assert(!(p_ptr->achievement_flags & SIL_ACH_RUN_CHASM_SHAFT_ROUTE));
    puts("Current route: a disqualified descent remains disqualified after save/resume PASS");
    clear_for_load();memset(&restored,0,sizeof(restored));
    assert(fixture_read(current,size,-1,&restored,current_end)==0);
    assert(p_ptr->achievement_flags & SIL_ACH_RUN_CHASM_SHAFT_ROUTE);

    object_type forged={0},loaded;
    object_prep(&forged,lookup_kind(TV_SOFT_ARMOR,SV_ARMOUR_OF_GALVORN));
    forged.ident|=IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS;
    size_t object_size=fixture_write_object(&forged,modified,sizeof(modified));
    memset(&loaded,0,sizeof(loaded));fixture_read_object(modified,object_size,&loaded);
    assert((loaded.ident&(IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS))
        ==(IDENT_ACH_EOL_GALVORN|IDENT_ACH_TELCHAR_SHARPNESS));
    puts("Current format: object codec preserves both own-forged provenance bits PASS");

    size_t v34_end;
    size_t v34_size=fixture_write(3,&source,modified,sizeof(modified),&v34_end);
    assert(current_end==v34_end+54 && size==v34_size+63);
    clear_for_load();memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(modified,v34_size,34,&restored,v34_end)==0);
    assert_player_tail();assert_discovery_defaults(&restored);
    assert(p_ptr->achievement_fall_depth==8 && restored.potion_poisoned==7);
    puts("Legacy v34: exact stream/checksum alignment; discovery, ice, rage, fear and first-wolf provenance defaults empty PASS");

    size_t previous_end;
    size_t previous_size=fixture_write(2,&source,modified,sizeof(modified),&previous_end);
    assert(current_end==previous_end+63 && size==previous_size+74);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(modified,previous_size,33,&restored,previous_end)==0);
    assert_player_tail();
    assert_discovery_defaults(&restored);
    assert(p_ptr->achievement_flags==(SIL_ACH_RUN_FLAGS
        & ~(SIL_ACH_RUN_FALLING | SIL_ACH_RUN_CHASM_SHAFT_ROUTE | SIL_ACH_RUN_COMEBACK)));
    assert(p_ptr->achievement_fall_depth==-1 && restored.potion_poisoned==0);
    for(size_t i=0;i<sizeof(p_ptr->achievement_jammed_doors);++i)
        assert(!p_ptr->achievement_jammed_doors[i]);
    assert(restored.player_poisoned==11);
    puts("Legacy v33: existing run/poison history retained, new provenance defaults empty PASS");

    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(legacy,oldsize,30,&restored,legacy_end)==0);
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
    puts("Legacy v30: exact stream/checksum alignment, safe defaults, no false pacifist or 1-HP achievement PASS");

    for (int poison=-5;poison<=99;poison+=104) {
        prepare_player(); source.player_poisoned=poison;
        size=fixture_write(false,&source,current,sizeof(current),&current_end);
        clear_for_load();
        assert(fixture_read(current,size,-1,&restored,current_end)==0);
        assert(restored.player_poisoned==(poison<0?0:27));
        assert(restored.potion_poisoned<=restored.player_poisoned);
    }
    source.player_poisoned=11;
    for (int poison=-5;poison<=99;poison+=104) {
        prepare_player(); source.potion_poisoned=poison;
        size=fixture_write(false,&source,current,sizeof(current),&current_end);
        clear_for_load();
        assert(fixture_read(current,size,-1,&restored,current_end)==0);
        assert(restored.potion_poisoned==(poison<0?0:11));
    }
    prepare_player(); p_ptr->achievement_comeback_depth=-1;
    size=fixture_write(false,&source,current,sizeof(current),&current_end);
    clear_for_load();
    assert(fixture_read(current,size,-1,&restored,current_end)==0);
    assert(!(p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK));
    puts("Current format: player poison clamps to [0,total poison], invalid comeback depth clears pending condition PASS");

    decode(current,plain,size);
    const byte marker[]={0xAC,SIL_ACH_RUN_FLAGS,0,0,0,0x78,0x56,0x34,0x12,0x70,0x11,1,0,0xFF,0xFF};
    size_t found=0; int matches=0;
    for(size_t i=0;i+sizeof(marker)<=current_end;i++)
        if(!memcmp(plain+i,marker,sizeof(marker))) { found=i; matches++; }
    assert(matches==1); plain[found]=0xAD;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
    puts("Current format: corrupt achievement block marker rejected PASS");
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
        assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
    }
    puts("Current format: potion ownership clamps; invalid extension/count/coordinates/duplicate doors rejected PASS");
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
        assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
        /* Reject in the field decoder itself, before the checksummed tail. */
        assert(fixture_read_offset<current_end);
    }
    puts("Current format: invalid discovery marker/crossing/poison/labyrinth/ice count/ice coordinate/duplicate ice rejected before checksum PASS");
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
        assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
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
        assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
        assert(fixture_read_offset<current_end);
    }
    puts("Current Balrog block: corrupt marker and impossible eighth bit rejected PASS");
    decode(current,plain,size); plain[size-1]^=1;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,-1,&restored,current_end)!=0);
    puts("Current format: corrupt encoded checksum rejected by production footer validator PASS");
}

static void test_crossing_resume(void)
{
    for (int water=0;water<=1;water++) for (int legacy=1;legacy>=0;legacy--) {
        prepare_player();sil_achievements_begin_run();
        p_ptr->playing=true;p_ptr->is_dead=false;
        int feat=water?FEAT_WATER:FEAT_POISON;
        int achievement=water?SIL_ACH_THE_FORDS_OF_SIRION:SIL_ACH_THROUGH_NAN_DUNGORTHEB;
        cave_feat[10][9]=cave_feat[10][13]=FEAT_FLOOR;
        cave_feat[10][10]=cave_feat[10][11]=cave_feat[10][12]=feat;
        sil_achievements_on_movement(10,9,10,10);
        sil_achievements_on_movement(10,10,10,11);
        assert((p_ptr->achievement_discovery_flags & SIL_ACH_DISC_LIQUID_FLAGS)
            ==(SIL_ACH_DISC_LIQUID_FIRST|SIL_ACH_DISC_LIQUID_SECOND));
        if(legacy) p_ptr->achievement_discovery_flags &= ~SIL_ACH_DISC_LIQUID_FLAGS;
        monster_type source={0},restored={0};source.r_idx=41;source.hp=source.maxhp=20;
        size_t player_end;
        size_t size=fixture_write(0,&source,current,sizeof(current),&player_end);
        clear_for_load();
        assert(fixture_read(current,size,-1,&restored,player_end)==0);
        p_ptr->cur_map_hgt=p_ptr->cur_map_wid=88;p_ptr->playing=true;p_ptr->is_dead=false;
        assert((p_ptr->achievement_discovery_flags & SIL_ACH_DISC_LIQUID_FLAGS)
            ==(legacy?0:(SIL_ACH_DISC_LIQUID_FIRST|SIL_ACH_DISC_LIQUID_SECOND)));
        sil_achievements_on_movement(10,11,10,12);
        sil_achievements_on_movement(10,12,10,13);
        assert((sil_achievement_unlocked_at(achievement)!=0)==!legacy);
    }
    puts("Crossing progress: distinct water/poison cells survive save/resume; unknown old progress never grants a false award PASS");
}
'''


def function(source, name):
    match = re.search(r"^void " + name + r"\([^\n]*\)\n\{", source, re.M)
    assert match, name
    return source[match.start():source.index("\n}", match.end()) + 2]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    save = (ROOT / "src/fs/save.c").read_text(encoding="utf-8")
    player = (ROOT / "src/fs/save-player.c").read_text(encoding="utf-8")
    # Every alternate writer below predates per-run disease immunity (0.9.9.2).
    player = player.replace("    wr_byte(p_ptr->disease_immunity);\n", "")
    block = ("    wr_byte(0xAC);\n"
             "    wr_u32b(p_ptr->achievement_flags & SIL_ACH_RUN_FLAGS);\n"
             "    wr_u32b(p_ptr->achievement_kills);\n"
             "    wr_u32b(p_ptr->achievement_singing_turns);\n"
             "    wr_s16b(p_ptr->achievement_comeback_depth);\n")
    assert player.count(block) == 1, "Update precise v31 player-field fixture filter"
    player = player[:player.index("\nvoid wr_randarts(void)")]
    v39_writer = player.replace("void wr_extra(void)", "void fixture_v39_extra(void)")
    balrog_start = player.index("    /* 0.9.8.39:")
    balrog_end = player.index('    log_info("SAVE: About to write quest marker', balrog_start)
    v38_player = player[:balrog_start] + player[balrog_end:]
    v38_writer = v38_player.replace("void wr_extra(void)", "void fixture_v38_extra(void)")
    lore_start = player.index("    /* 0.9.8.38:")
    lore_end = player.index('    log_info("SAVE: About to write quest marker', lore_start)
    v37_player = player[:lore_start] + player[lore_end:]
    v37_writer = v37_player.replace("void wr_extra(void)", "void fixture_v37_extra(void)")
    discovery_start = player.index("    /* 0.9.8.35:")
    discovery_end = player.index('    log_info("SAVE: About to write quest marker', discovery_start)
    v34_player = player[:discovery_start] + player[discovery_end:]
    start = v34_player.index("    /* 0.9.8.34:")
    end = v34_player.index('    log_info("SAVE: About to write quest marker', start)
    previous_player = v34_player[:start] + v34_player[end:]
    monster = function(save, "wr_monster")
    potion = "    wr_s16b(m_ptr->potion_poisoned);\n"
    assert monster.count(potion) == 1
    discovery_monster = ("    wr_u32b(m_ptr->achievement_doom_rage_epoch);\n"
                         "    wr_u32b(m_ptr->achievement_elbereth_turn);\n"
                         "    wr_byte(m_ptr->achievement_first_wolf ? 1 : 0);\n")
    assert monster.count(discovery_monster) == 1
    v34_monster = monster.replace(discovery_monster, "")
    previous_monster = v34_monster.replace(potion, "")
    flags = save[save.index("#define SAVE_MON_FLAGS"):save.index('/*\n * Write a "monster"')]
    v34_writer = v34_player.replace("void wr_extra(void)", "void fixture_v34_extra(void)")
    v34_writer += '\n#include "monster/monster-ai.h"\n' + flags + "\n"
    v34_writer += v34_monster.replace("void wr_monster(", "void fixture_v34_monster(")
    previous_writer = previous_player.replace("void wr_extra(void)", "void fixture_previous_extra(void)")
    previous_writer += '\n#include "monster/monster-ai.h"\n' + flags + "\n"
    previous_writer += previous_monster.replace("void wr_monster(", "void fixture_previous_monster(")
    old_player = previous_player.replace(block, "").replace("void wr_extra(void)", "void fixture_legacy_extra(void)")
    poison = "    wr_s16b(m_ptr->player_poisoned);\n"
    assert previous_monster.count(poison) == 1
    old_monster = previous_monster.replace(poison, "").replace("void wr_monster(", "void fixture_legacy_monster(")
    old_writer = old_player + '\n#include "monster/monster-ai.h"\n' + flags + "\n" + old_monster
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
    harness += TESTS + init + "    ANGBAND_DIR_APEX=argv[2];\n    test_save(); test_crossing_resume(); SDL_Quit(); return 0;\n}\n"
    sources = []
    for name, text in (("check.c", harness), ("writer.c", WRITER), ("reader.c", reader), ("legacy.c", old_writer), ("previous.c", previous_writer), ("v34.c", v34_writer), ("v37.c", v37_writer), ("v38.c", v38_writer), ("v39.c", v39_writer)):
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
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile],
                       cwd=profile, env=env, check=True, timeout=90)


if __name__ == "__main__":
    main()
