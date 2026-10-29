#!/usr/bin/env python3
"""Exercise production Blitz completion rules, restored signed stacks and profiles.

Compiles achievements.c and blitz.c with real SDL/cJSON persistence. Presentation
and provider boundaries are doubled; all profiles are disposable, and no game
window, player save or provider account is opened.
"""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-blitz"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "achievements.h"
#include "blitz.h"
#include "metarun.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static player_type player;
static player_other options;
player_type *p_ptr = &player;
player_other *op_ptr = &options;
bool character_generated = true;
cptr ANGBAND_DIR_APEX;
cptr ANGBAND_DIR_METARUN;
metarun metar;
maxima *z_info;
monster_type *mon_list;
monster_race *r_info;
s16b mon_max;
static int silmarils, notices;
int silmarils_possessed(void) { return silmarils; }
void metarun_apply_runtime_effects(void) {}
bool oath_invalid(int oath) { (void)oath;return false; }
void sil_play_games_sync(const uint64_t *masks) { (void)masks; }
uint64_t sil_play_games_unlocked(unsigned word) { (void)word;return 0; }
void sil_achievement_character_on_player_turn(void) {}
void sil_achievement_character_on_monster_killed(int m,int who) { (void)m;(void)who; }
void sil_achievement_lore_on_kill(int m,int who) { (void)m;(void)who; }
void sil_achievement_lore_on_escape(int n) { (void)n; }
void sil_achievement_lore_on_travel(int from,int to) { (void)from;(void)to; }
void sil_achievement_lore_reset(void) {}
void sil_achievements_discovery_reset(bool fresh) { (void)fresh; }
void sil_achievements_on_partition_entry(int y,int x) { (void)y;(void)x; }
void sil_achievements_song_begin(void) {}
void screen_save(void) {}
void screen_load(void) {}
void metarun_show_poetry_scene(cptr title,byte title_attr,cptr line1,byte attr1,
    cptr line2,byte attr2,cptr prompt)
{ (void)title;(void)title_attr;(void)line1;(void)attr1;(void)line2;(void)attr2;(void)prompt; }
void log_log(int level,const char *file,int line,const char *fmt,...)
{ (void)level;(void)file;(void)line;(void)fmt; }
size_t strnfmt(char *out,size_t size,cptr fmt,...)
{
    va_list args;va_start(args,fmt);
    int result=vsnprintf(out,size,fmt,args);va_end(args);
    return result<0 ? 0 : (size_t)result;
}
bool path_build(char *out,size_t size,cptr base,cptr name)
{ return snprintf(out,size,"%s/%s",base,name)<(int)size; }
void msg_print(cptr text) { assert(strstr(text,"Achievement unlocked:")); }
void sdl_popup_notification_show(cptr text)
{ assert(strstr(text,"Achievement unlocked:"));++notices; }
static bool earned(int id) { return sil_achievement_unlocked_at(id)!=0; }
int main(int argc,char **argv)
{
    assert(argc==3);const char *scenario=argv[1];ANGBAND_DIR_APEX=argv[2];
    assert(SDL_Init(0));sil_achievements_init();
    bool reload=!strcmp(scenario,"reload");
    if(reload) {
        assert(earned(SIL_ACH_SWIFT_AS_THE_WIND)&&earned(SIL_ACH_TENFOLD_DOOM));
        assert(!notices);SDL_Quit();return 0;
    }
    run_mode_set_current(RUN_MODE_BLITZ);
    player.achievement_flags=SIL_ACH_RUN_STARTED;
    player.is_dead=true;player.leaving=true;player.escaped=true;
    player.chp=player.mhp=20;silmarils=1;
    int8_t stacks[METAR_CURSE_SLOTS]={0};stacks[0]=10;
    bool win=true,hard=true;
    if(!strcmp(scenario,"morgoth")) {
        player.escaped=false;player.morgoth_slain=true;player.depth=MORGOTH_DEPTH;
        silmarils=0;
    } else if(!strcmp(scenario,"nine")||!strcmp(scenario,"progression")) { stacks[0]=9;hard=false; }
    else if(!strcmp(scenario,"eleven")) stacks[0]=11;
    else if(!strcmp(scenario,"blessings-cancel")) { stacks[1]=-1;hard=false; }
    else if(!strcmp(scenario,"signed-multiplicity")) { stacks[0]=12;stacks[1]=-2; }
    else if(!strcmp(scenario,"negative")) { stacks[0]=-10;hard=false; }
    else if(!strcmp(scenario,"last-slot")) {
        stacks[0]=0;stacks[METAR_CURSE_SLOTS-1]=10;
    } else if(!strcmp(scenario,"setup-only")) {
        blitz_current_setup_mutable()->curse_count=100;
        stacks[0]=0;hard=false;
    } else if(!strcmp(scenario,"tale-only")) {
        metar.curse_stacks[0]=100;stacks[0]=0;hard=false;
    } else if(!strcmp(scenario,"empty-escape")) { silmarils=0;win=false; }
    else if(!strcmp(scenario,"death")) {
        player.escaped=false;player.chp=0;win=false;
    } else if(!strcmp(scenario,"dead-with-jewel")) { player.chp=-1;win=false; }
    else if(!strcmp(scenario,"abandon")) { player.escaped=false;win=false; }
    else if(!strcmp(scenario,"still-playing")) { player.playing=true;win=false; }
    else if(!strcmp(scenario,"alive")) { player.is_dead=false;win=false; }
    else if(!strcmp(scenario,"not-leaving")) { player.leaving=false;win=false; }
    else if(!strcmp(scenario,"underground-escape")) { player.depth=1;win=false; }
    else if(!strcmp(scenario,"story")) { run_mode_set_current(RUN_MODE_STORY);win=false; }
    else if(!strcmp(scenario,"legacy")) { player.achievement_flags=0;win=false; }
    else if(!strcmp(scenario,"tainted")) {
        player.achievement_flags|=SIL_ACH_RUN_TAINTED;win=false;
    } else if(!strcmp(scenario,"remembered-taint")) {
        player.playing=true;player.is_dead=false;player.energy_use=100;
        options.opt[OPT_CHEAT]=true;sil_achievements_on_player_turn();
        options.opt[OPT_CHEAT]=false;player.playing=false;player.is_dead=true;
        assert(player.achievement_flags&SIL_ACH_RUN_TAINTED);win=false;
    } else if(!strcmp(scenario,"wizard")) { player.wizard=true;win=false; }
    else if(!strcmp(scenario,"practice")) { player.game_type=1;win=false; }
    else if(!strcmp(scenario,"noscore")) { player.noscore=1;win=false; }
    else if(!strcmp(scenario,"cheat")) { options.opt[OPT_CHEAT]=true;win=false; }
    else if(!strcmp(scenario,"cheat-history")) { options.opt[OPT_SCORE]=true;win=false; }
    else if(!strcmp(scenario,"unborn")) { character_generated=false;win=false; }
    else assert(!strcmp(scenario,"ten"));
    /* The same production restoration route used after loading signed bytes.
     * Erasing the source proves completion reads runtime state, not setup. */
    blitz_runtime_restore(stacks,0);
    memset(stacks,0,sizeof(stacks));
    bool was_playing=player.playing,was_dead=player.is_dead;
    player.playing=true;player.is_dead=false;
    sil_achievements_on_event(SIL_ACH_SWIFT_AS_THE_WIND);
    sil_achievements_on_event(SIL_ACH_TENFOLD_DOOM);
    assert(!earned(SIL_ACH_SWIFT_AS_THE_WIND)&&!earned(SIL_ACH_TENFOLD_DOOM));
    player.playing=was_playing;player.is_dead=was_dead;
    sil_achievements_on_run_completed();
    assert(earned(SIL_ACH_SWIFT_AS_THE_WIND)==win);
    assert(earned(SIL_ACH_TENFOLD_DOOM)==(win&&hard));
    uint64_t when=sil_achievement_unlocked_at(SIL_ACH_SWIFT_AS_THE_WIND);
    int expected=(int)win+(int)(win&&hard);
    assert(notices==expected&&sil_achievements_saved());
    sil_achievements_on_run_completed();
    assert(notices==expected&&sil_achievement_unlocked_at(SIL_ACH_SWIFT_AS_THE_WIND)==when);
    if(!strcmp(scenario,"progression")) {
        stacks[0]=10;blitz_runtime_restore(stacks,0);
        sil_achievements_on_run_completed();
        assert(earned(SIL_ACH_TENFOLD_DOOM)&&notices==2);
        assert(sil_achievement_unlocked_at(SIL_ACH_SWIFT_AS_THE_WIND)==when);
    }
    SDL_Quit();return 0;
}
'''


def main():
    # Pin the owning lifecycle contract: both victories finalize the run, and
    # completion is reported before any postmortem summary can reset Blitz.
    lifecycle = (ROOT / "src/game/game-lifecycle.c").read_text(encoding="utf-8")
    escape = lifecycle.split("void do_cmd_escape(", 1)[1].split("void do_cmd_morgoth_victory(", 1)[0]
    morgoth = lifecycle.split("void do_cmd_morgoth_victory(", 1)[1].split("void ", 1)[0]
    for body in (escape, morgoth):
        assert "p_ptr->is_dead = true" in body and "p_ptr->playing = false" in body
        assert "p_ptr->leaving = true" in body
    assert "p_ptr->escaped = true" in escape and "p_ptr->morgoth_slain = true" in morgoth
    assert lifecycle.index("sil_achievements_on_run_completed();") < lifecycle.index("blitz_show_end_summary((byte)blitz_silmarils)")
    for path in ("src/fs/save-player.c", "src/fs/load-player.c"):
        text = (ROOT / path).read_text(encoding="utf-8")
        assert "METAR_CURSE_SLOTS" in text and "SIL_ACH_RUN_FLAGS" in text
    assert "blitz_runtime_restore(stacks," in (ROOT / "src/fs/load-player.c").read_text(encoding="utf-8")
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        str(BUILD / "_deps/SDL"), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0", "-Wall", "-Wextra",
        "-ffunction-sections", "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), *(str(ROOT / "src" / name) for name in ("achievements.c", "blitz.c", "cJSON.c")),
        "-L", str(BUILD / "_deps/SDL"), "-lSDL3", "-lm", "-Wl,--gc-sections", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    cases = ("ten", "morgoth", "nine", "eleven", "progression", "blessings-cancel", "signed-multiplicity",
             "negative", "last-slot", "setup-only", "tale-only", "empty-escape", "death",
             "dead-with-jewel", "abandon", "still-playing", "alive", "not-leaving",
             "underground-escape", "story", "legacy", "tainted", "remembered-taint", "wizard", "practice",
             "noscore", "cheat", "cheat-history", "unborn")
    with tempfile.TemporaryDirectory(prefix="profiles-", dir=OUT) as directory:
        for case in cases:
            profile = Path(directory) / case
            subprocess.run([str(exe), case, str(profile)], env=env, check=True, timeout=15)
            path = profile / "achievements.json"
            payload = json.loads(path.read_text()) if path.exists() else {"unlocked": {}}
            if case == "ten":
                assert set(payload["unlocked"]) == {"SWIFT_AS_THE_WIND", "TENFOLD_DOOM"}
                subprocess.run([str(exe), "reload", str(profile)], env=env, check=True, timeout=15)
            # Blitz outcomes must never create Story/Tale completion history.
            if case != "story":
                assert not payload.get("tales")
    print(f"PASS: {len(cases)} Blitz completion/eligibility cases, signed restored stacks, exact-once grants and profile reload.")


if __name__ == "__main__":
    main()
