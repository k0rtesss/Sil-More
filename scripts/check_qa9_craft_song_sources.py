#!/usr/bin/env python3
"""Exercise actual equipment bonus updates and singing after a source is lost.

Uses initialized engine objects and isolated template caches. --baseline links
the committed song implementation as a negative control, without shared builds.
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
OUT = ROOT / "scripts/output/qa9-craft-song-sources"
CHECKS = r'''
#include "player/player-upkeep-internal.h"
static void setup(bool learned_major, bool learned_minor)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    reset_map(1);
    for(int i=0;i<A_MAX;i++) p_ptr->stat_base[i]=3;
    p_ptr->skill_base[S_SNG]=5;
    p_ptr->msp=p_ptr->csp=100;
    p_ptr->mhp=p_ptr->chp=100;
    p_ptr->innate_ability[S_SNG][SNG_ELBERETH]=learned_major;
    p_ptr->innate_ability[S_SNG][SNG_FREEDOM]=learned_minor;
    object_type* item=&inventory[INVEN_NECK];
    object_prep(item,lookup_kind(TV_AMULET,SV_AMULET_SELF_MADE));
    item->abilities=3;
    int abilities[]={SNG_ELBERETH,SNG_FREEDOM,SNG_WOVEN_THEMES};
    for(int i=0;i<3;i++) {
        item->skilltype[i]=S_SNG; item->abilitynum[i]=abilities[i];
        p_ptr->active_ability[S_SNG][abilities[i]]=true;
    }
    calc_bonuses();
    p_ptr->song1=SNG_ELBERETH; p_ptr->song2=SNG_FREEDOM;
    p_ptr->song_duration=0;
}
static void remove_source(void)
{
    object_wipe(&inventory[INVEN_NECK]);
    calc_bonuses();
    assert(!p_ptr->have_ability[S_SNG][SNG_WOVEN_THEMES]);
    assert(!p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]);
}
static void checks(void)
{
    setup(true,true);
    s32b clock=turn; int player_clock=playerturn;
    remove_source();
    assert(turn==clock && playerturn==player_clock);
    assert(p_ptr->active_ability[S_SNG][SNG_ELBERETH]);
    assert(p_ptr->active_ability[S_SNG][SNG_FREEDOM]);
    int voice=p_ptr->csp;
    sing();
    assert(p_ptr->song1==SNG_ELBERETH && p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->csp==voice-1 && p_ptr->song_duration==1);
    assert((p_ptr->redraw & PR_SONG) && (p_ptr->update & PU_BONUS));
    p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]=true;
    sing();
    assert(p_ptr->song1==SNG_ELBERETH && p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->csp==voice-2); /* Reacquisition never resurrects the minor theme. */

    setup(true,false); remove_source(); voice=p_ptr->csp; sing();
    assert(p_ptr->song1==SNG_ELBERETH && p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->csp==voice-1);
    setup(false,true); remove_source(); voice=p_ptr->csp; sing();
    assert(p_ptr->song1==SNG_NOTHING && p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->csp==voice);
    setup(false,false); remove_source(); sing();
    assert(p_ptr->song1==SNG_NOTHING && p_ptr->song2==SNG_NOTHING);

    setup(true,true);
    p_ptr->active_ability[S_SNG][SNG_FREEDOM]=false;
    voice=p_ptr->csp;
    sing();
    assert(p_ptr->song1==SNG_ELBERETH && p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->csp==voice-1);
    setup(true,true); p_ptr->csp=0; sing();
    assert(p_ptr->song1==SNG_NOTHING && p_ptr->song2==SNG_NOTHING);
    puts("Equipment updates and real sing: learned major survives lost weaving/minor, item-only major stops, no resurrection, Voice and UI clocks PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += '#include "log/log.h"\n'
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    songs = ROOT / "src/player/player-songs.c"
    if args.baseline:
        songs = OUT / "songs-baseline.c"
        songs.write_text(subprocess.check_output(["git", "show", "HEAD:src/player/player-songs.c"],
            cwd=ROOT, text=True, encoding="utf-8"), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/player/player-songs.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(songs), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
