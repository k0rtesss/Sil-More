#!/usr/bin/env python3
"""Roundtrip achievement/player-poison state with production save/load codecs.

Requires current build-standard engine objects. Uses temporary profiles only.
The v30 fixture is production writer source with exactly the v31 additions
removed, rather than a second hand-written serialization implementation.
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
size_t fixture_write(bool legacy, const monster_type* monster, byte* buffer,
    size_t capacity, size_t* player_end)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    if (legacy) fixture_legacy_extra(); else save_write_extra();
    *player_end = (size_t)SDL_TellIO(fff);
    save_wr_u32b(0xA1B2C3D4U);
    if (legacy) fixture_legacy_monster(monster); else save_wr_monster(monster);
    save_wr_u32b(0xB4C3D2E1U);
    save_wr_u32b(v_stamp); save_wr_u32b(x_stamp);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
int fixture_read(const byte* buffer, size_t length, int extra,
    monster_type* monster, size_t player_end)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
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
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
'''

TESTS = r'''
#include "achievements.h"
size_t fixture_write(bool,const monster_type*,byte*,size_t,size_t*);
int fixture_read(const byte*,size_t,int,monster_type*,size_t);
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
}
static void prepare_player(void)
{
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(10);
    character_generated=true; p_ptr->playing=true;
    p_ptr->chp=1; p_ptr->mhp=80;
    p_ptr->achievement_flags=SIL_ACH_RUN_FLAGS|0x80000000U;
    p_ptr->achievement_kills=0x12345678U; p_ptr->achievement_singing_turns=70000;
    p_ptr->achievement_comeback_depth=10;
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
static void test_save(void)
{
    assert(VERSION_MAJOR==0 && VERSION_MINOR==9 && VERSION_PATCH==8 && VERSION_EXTRA==31);
    prepare_player();
    monster_type source={0}, restored;
    source.r_idx=41; source.image_r_idx=7; source.fy=8; source.fx=9;
    source.hp=source.maxhp=120; source.poisoned=27; source.player_poisoned=11;
    source.vengeance=1; source.smite_recovery=2;
    source.ai.observations[MON_AI_FIRE].value=2;
    source.ai.observations[MON_AI_FIRE].ttl=40;
    source.ai.observations[MON_AI_FIRE].turn=775;
    size_t current_end,legacy_end;
    size_t size=fixture_write(false,&source,current,sizeof(current),&current_end);
    size_t oldsize=fixture_write(true,&source,legacy,sizeof(legacy),&legacy_end);
    assert(current_end==legacy_end+15 && size==oldsize+17);
    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(current,size,31,&restored,current_end)==0);
    assert_player_tail();
    assert(p_ptr->achievement_flags==SIL_ACH_RUN_FLAGS);
    assert(p_ptr->achievement_kills==0x12345678U && p_ptr->achievement_singing_turns==70000);
    assert(p_ptr->achievement_comeback_depth==10);
    assert(restored.poisoned==27 && restored.player_poisoned==11);
    assert(restored.vengeance==1 && restored.smite_recovery==2);
    assert(restored.ai.observations[MON_AI_FIRE].value==2);
    assert(restored.ai.observations[MON_AI_FIRE].ttl==40);
    assert(restored.ai.observations[MON_AI_FIRE].turn==775);
    puts("Current v31: achievement fields, post-block quests, player poison, AI, sentinels and both checksums PASS");

    clear_for_load(); memset(&restored,0x55,sizeof(restored));
    assert(fixture_read(legacy,oldsize,30,&restored,legacy_end)==0);
    assert_player_tail();
    assert(p_ptr->achievement_flags==0 && p_ptr->achievement_kills==0);
    assert(p_ptr->achievement_singing_turns==0 && p_ptr->achievement_comeback_depth==-1);
    assert(restored.poisoned==27 && restored.player_poisoned==0);
    assert(restored.vengeance==1 && restored.smite_recovery==2);
    assert(restored.ai.observations[MON_AI_FIRE].value==2);
    p_ptr->playing=true; p_ptr->depth=0;
    sil_achievements_on_escape(1);
    assert(sil_achievements_mask() & (1U<<SIL_ACH_BACK_TO_STARLIGHT));
    assert(!(sil_achievements_mask() & (1U<<SIL_ACH_THE_QUIET_ROAD)));
    p_ptr->depth=10;
    sil_achievements_on_monster_killed(1,-1);
    assert(!(sil_achievements_mask() & (1U<<SIL_ACH_BY_A_THREAD)));
    puts("Legacy v30: exact stream/checksum alignment, safe defaults, no false pacifist or 1-HP achievement PASS");

    for (int poison=-5;poison<=99;poison+=104) {
        prepare_player(); source.player_poisoned=poison;
        size=fixture_write(false,&source,current,sizeof(current),&current_end);
        clear_for_load();
        assert(fixture_read(current,size,31,&restored,current_end)==0);
        assert(restored.player_poisoned==(poison<0?0:27));
    }
    prepare_player(); p_ptr->achievement_comeback_depth=-1;
    size=fixture_write(false,&source,current,sizeof(current),&current_end);
    clear_for_load();
    assert(fixture_read(current,size,31,&restored,current_end)==0);
    assert(!(p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK));
    puts("Current v31: player poison clamps to [0,total poison], invalid comeback depth clears pending condition PASS");

    decode(current,plain,size);
    const byte marker[]={0xAC,7,0,0,0,0x78,0x56,0x34,0x12,0x70,0x11,1,0,0xFF,0xFF};
    size_t found=0; int matches=0;
    for(size_t i=0;i+sizeof(marker)<=current_end;i++)
        if(!memcmp(plain+i,marker,sizeof(marker))) { found=i; matches++; }
    assert(matches==1); plain[found]=0xAD;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,31,&restored,current_end)!=0);
    puts("Current v31: corrupt achievement block marker rejected PASS");
    decode(current,plain,size); plain[size-1]^=1;
    encode(plain,modified,size); clear_for_load();
    assert(fixture_read(modified,size,31,&restored,current_end)!=0);
    puts("Current v31: corrupt encoded checksum rejected by production footer validator PASS");
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
    block = ("    wr_byte(0xAC);\n"
             "    wr_u32b(p_ptr->achievement_flags & SIL_ACH_RUN_FLAGS);\n"
             "    wr_u32b(p_ptr->achievement_kills);\n"
             "    wr_u32b(p_ptr->achievement_singing_turns);\n"
             "    wr_s16b(p_ptr->achievement_comeback_depth);\n")
    assert player.count(block) == 1, "Update the precise v31 player-field fixture filter"
    player = player[:player.index("\nvoid wr_randarts(void)")]
    player = player.replace(block, "").replace("void wr_extra(void)", "void fixture_legacy_extra(void)")
    monster = function(save, "wr_monster")
    poison = "    wr_s16b(m_ptr->player_poisoned);\n"
    assert monster.count(poison) == 1
    monster = monster.replace(poison, "").replace("void wr_monster(", "void fixture_legacy_monster(")
    flags = save[save.index("#define SAVE_MON_FLAGS"):save.index('/*\n * Write a "monster"')]
    old_writer = player + '\n#include "monster/monster-ai.h"\n' + flags + "\n" + monster
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
    for name, text in (("check.c", harness), ("writer.c", WRITER), ("reader.c", reader), ("legacy.c", old_writer)):
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
