#!/usr/bin/env python3
"""Roundtrip production player writer/reader and legacy 0.9.9.2 credit defaults.
Requires a full current-header build; never opens a player save.
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
OUT = ROOT / "scripts/output/qa9-combat-credit-save"
WRITER = r"""
#include "fs/save.c"
#include <assert.h>
extern void fixture_legacy_extra(void);
extern void fixture_reserved_probe_extra(void);
size_t fixture_write(byte* buffer,size_t capacity,int legacy)
{
    fff=SDL_IOFromMem(buffer,capacity); assert(fff);
    xor_byte=0; v_stamp=x_stamp=save_byte_offset=0; write_error=false;
    if(legacy==1) fixture_legacy_extra();
    else if(legacy==2) fixture_reserved_probe_extra();
    else save_write_extra();
    save_wr_u32b(0xA1B2C3D4U);
    size_t size=(size_t)SDL_TellIO(fff); assert(!write_error);
    SDL_CloseIO(fff); fff=NULL; return size;
}
"""
READER = r"""
#include "fs/load.c"
#include <assert.h>
bool fixture_supported(int extra)
{
    sf_major=0;sf_minor=9;sf_patch=9;sf_extra=extra;
    return savefile_version_supported();
}
int fixture_read(const byte* buffer,size_t size,int extra)
{
    fff=SDL_IOFromConstMem(buffer,size); assert(fff);
    xor_byte=0;v_check=x_check=load_byte_offset=0;
    sf_major=0;sf_minor=9;sf_patch=9;sf_extra=extra;
    savefile_has_song_duels=savefile_has_ability_timeline=true;
    savefile_has_varda_quest=savefile_has_skeleton_notes=true;
    savefile_has_skeleton_hint_mask32=savefile_has_skeleton_hint_counts=true;
    savefile_has_partition_meta=savefile_has_partition_meta_types=true;
    savefile_has_hint_messages=savefile_has_hint_message_meta=true;
    savefile_has_hint_message_destinations=savefile_has_morgoth_call_state=true;
    int result=load_read_extra();
    u32b sentinel=0;if(!result) load_rd_u32b(&sentinel);
    assert(!result && sentinel==0xA1B2C3D4U && SDL_TellIO(fff)==(Sint64)size);
    SDL_CloseIO(fff);fff=NULL;return result;
}
"""
CHECKS = r"""
extern size_t fixture_write(byte*,size_t,int);
extern int fixture_read(const byte*,size_t,int);
extern bool fixture_supported(int);
static void checks(void)
{
    byte buffer[262144];size_t canonical_size=0;
    assert(fixture_supported(2) && fixture_supported(3)
        && fixture_supported(VERSION_EXTRA)
        && !fixture_supported(VERSION_EXTRA + 1));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    for(int legacy=0;legacy<3;legacy++) for(int used=0;used<3;used++) {
        memset(p_ptr,0,sizeof(*p_ptr));
        p_ptr->song1=p_ptr->song2=SNG_NOTHING;
        p_ptr->insight_ruleset=INSIGHT_RULESET_CLASSIC;
        p_ptr->chp=p_ptr->mhp=100;
        p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
        p_ptr->active_ability[S_MEL][MEL_WARDEN]=1;
        p_ptr->free_active_weapon_change_used=used==2?255:used;
        p_ptr->insight_points=12345;p_ptr->stat_base[A_STR]=7;
        p_ptr->light_dimmed=true;
        size_t size=fixture_write(buffer,sizeof(buffer),legacy);
        if(!canonical_size)canonical_size=size;
        assert(size==canonical_size); /* Reserved byte preserves stream length. */
        p_ptr->free_active_weapon_change_used=255;
        p_ptr->insight_points=0;p_ptr->stat_base[A_STR]=0;p_ptr->light_dimmed=false;
        assert(!fixture_read(buffer,size,legacy?2:VERSION_EXTRA));
        assert(p_ptr->free_active_weapon_change_used==(!legacy && used!=0));
        assert(p_ptr->insight_points==12345 && p_ptr->stat_base[A_STR]==7 && p_ptr->light_dimmed);
        player_active_weapon_sync_loaded_state();
        p_ptr->restoring=true;
        player_active_weapon_begin_player_turn(); /* First process_player on reload. */
        p_ptr->restoring=false; /* request_command entry clears restoring. */
        bool expected_free=legacy || used==0;
        assert(player_active_weapon_change_is_free(PLAYER_ACTIVE_WEAPON_KIND_MELEE,PLAYER_ACTIVE_WEAPON_KIND_BOW)==expected_free);
        /* Free browsing/cancellation leaves the loaded credit unchanged. */
        assert(player_active_weapon_change_is_free(PLAYER_ACTIVE_WEAPON_KIND_MELEE,PLAYER_ACTIVE_WEAPON_KIND_BOW)==expected_free);
        player_active_weapon_begin_player_turn(); /* A paid action ended the turn. */
        assert(player_active_weapon_change_is_free(PLAYER_ACTIVE_WEAPON_KIND_MELEE,PLAYER_ACTIVE_WEAPON_KIND_BOW));
        player_active_weapon_free_change_commit();
        assert(!player_active_weapon_change_is_free(PLAYER_ACTIVE_WEAPON_KIND_MELEE,PLAYER_ACTIVE_WEAPON_KIND_BOW));
    }
    p_ptr->free_active_weapon_change_used=255;
    player_active_weapon_sync_loaded_state();
    assert(p_ptr->free_active_weapon_change_used==1);
    puts("PASS: 9 actual player stream roundtrips, current normalized credit, legacy default/padding gate, version bounds, restored first turn and paid reset.");
}
"""
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    legacy=subprocess.check_output(["git","-c",f"safe.directory={ROOT.as_posix()}","show","HEAD:src/fs/save-player.c"],cwd=ROOT).decode()
    # Keep the genuine previous writer body and its tutorial helper; shared partition writer stays production.
    helper=legacy[legacy.index("static void wr_tutorial_character_state"):legacy.index("void save_write_partition_meta")]
    body=legacy[legacy.index("void wr_extra(void)"):legacy.index("void wr_randarts(void)")]
    headers=legacy[:legacy.index("static void wr_tutorial_character_state")]
    # Nonzero old reserved padding probes the read version gate, independent of ordinary zero defaults.
    for name,probe in (("legacy",False),("reserved_probe",True)):
        selected=body.replace("void wr_extra(void)",f"void fixture_{name}_extra(void)")
        if probe:
            before=selected
            selected=selected.replace("wr_s32b(p_ptr->insight_points);\n    wr_byte(0);","wr_s32b(p_ptr->insight_points);\n    wr_byte(0xA5);")
            assert selected!=before
        (OUT/f"{name}.c").write_text(headers+helper+selected)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix+=fixture_function("terminal_extra")+"\n"
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init=init[:init.index("    check_templates();")]
    (OUT/"check.c").write_text(prefix+CHECKS+init+"    checks();SDL_Quit();return 0;\n}\n")
    (OUT/"writer.c").write_text(WRITER);(OUT/"reader.c").write_text(READER)
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=("/src/main.c.obj","/src/fs/save.c.obj","/src/fs/load.c.obj")
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(excluded)))
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([*(str(BUILD/"_deps"/p) for p in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),"C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g","@CMakeFiles/sil-more.dir/includes_C.rsp",*(str(OUT/p) for p in ("check.c","writer.c","reader.c","legacy.c","reserved_probe.c")),"@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp","-o",str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-",dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/"lib/edit"),state],cwd=state,env=env,check=True,timeout=30)
if __name__=="__main__":main()
