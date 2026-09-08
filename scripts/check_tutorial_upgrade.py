"""Exercise real 0.9.8 tutorial migration/notice and character byte I/O.

Uses isolated in-memory streams and a fake notice presenter; no real save,
metarun or configuration file is read or written.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-upgrade-check"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for end in range(brace, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if not depth:
                return source[start:end + 1]
    raise AssertionError(signature)


HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include "tutorial/tutorial.h"
#include "ui/question.h"
#include "log/log.h"
#include <assert.h>
#include <stdarg.h>

__OLD_META_LAYOUT__
_Static_assert(sizeof(metarun) == sizeof(metarun_pre098), "Meta record size changed");
_Static_assert(offsetof(metarun, tutorial_upgrade_pending) == offsetof(metarun_pre098, reserved_runtime), "Notice must use reserved byte");

static player_type player;
player_type *p_ptr = &player;
metarun metar;
static metarun tale;
static bool managing, blitz;
static bool current_save;
static byte bytes[8];
static int position, notices, saves, answer, save_result;
static char last_message[700];
static tutorial_mode mode = TUTORIAL_MODE_EXTENDED;
bool savefile_version_at_least(byte a, byte b, byte c, byte d)
{ assert(a==0 && b==9 && c==8 && d==0); return current_save; }
void rd_byte(byte *value) { *value = bytes[position++]; }
void wr_byte(byte value) { bytes[position++] = value; }
bool run_mode_is_blitz(void) { return blitz; }
metarun *metarun_current_mutable(void) { return &tale; }
tutorial_mode get_sdl_gameplay_tutorial_mode(void) { return mode; }
errr save_metaruns(void) { ++saves; return save_result; }
size_t strnfmt(char *buf, size_t size, const char *fmt, ...)
{ va_list args; va_start(args,fmt); int n=SDL_vsnprintf(buf,size,fmt,args); va_end(args); return n<0?0:(size_t)n; }
void log_log(int level, const char *file, int line, const char *fmt, ...)
{ (void)level; (void)file; (void)line; (void)fmt; }
int ui_question_ask_overlay(cptr title,cptr text,const ui_question_option *options,
    int count,int y,int x,int initial)
{
    assert(managing && !strcmp(title,"Updated to 0.9.8"));
    assert(count==1 && !strcmp(options[0].label,"Continue"));
    assert(y==UI_QUESTION_GLOBAL && x==UI_QUESTION_GLOBAL && initial==0);
    SDL_strlcpy(last_message,text,sizeof(last_message)); ++notices; return answer;
}

__FUNCTIONS__

int main(void)
{
    assert(VERSION_MAJOR==0 && VERSION_MINOR==9 && VERSION_PATCH==8 && VERSION_EXTRA==0);
    assert(!strcmp(VERSION_STRING,"0.9.8"));
    /* Legacy saves consume no new byte and remain deferred across resaves. */
    bytes[0]=0x7e; position=0; current_save=false;
    rd_tutorial_character_state();
    assert(player.tutorial_deferred && position==0 && bytes[0]==0x7e);
    wr_tutorial_character_state(); assert(position==1 && bytes[0]==1);
    player.tutorial_deferred=false; current_save=true; position=0;
    rd_tutorial_character_state(); assert(player.tutorial_deferred && position==1);
    /* Next new hero starts clear; its own 0.9.8 save remains clear on load. */
    memset(&player,0,sizeof(player)); position=0;
    wr_tutorial_character_state(); assert(bytes[0]==0 && position==1);
    player.tutorial_deferred=true; position=0;
    rd_tutorial_character_state(); assert(!player.tutorial_deferred);

    meta_file_header old={.version_major=0,.version_minor=9,.version_patch=7,.version_extra=14};
    meta_file_header now={.version_major=0,.version_minor=9,.version_patch=8,.version_extra=0};
    metarun tales[3]={{.id=1},{.id=2},{.id=3}};
    for (int i=0;i<3;++i) metarun_migrate_tutorial_notice(&tales[i],&old);
    for (int i=0;i<3;++i) assert(tales[i].tutorial_upgrade_pending==1);
    tales[0].tutorial_upgrade_pending=0;
    for (int i=0;i<3;++i) metarun_migrate_tutorial_notice(&tales[i],&now);
    assert(!tales[0].tutorial_upgrade_pending && tales[1].tutorial_upgrade_pending);
    /* Raw roundtrip keeps each Tale's notice after the header is current. */
    metarun copy[3]; memcpy(copy,tales,sizeof(copy));
    assert(!copy[0].tutorial_upgrade_pending && copy[2].tutorial_upgrade_pending);
    tale=copy[1]; metar=tale; player.tutorial_deferred=true; answer=0;
    tutorial_game_upgrade_notice();
    assert(notices==1 && saves==1 && !tale.tutorial_upgrade_pending);
    assert(strstr(last_message,"after this one dies") && player.tutorial_deferred && !managing);
    tutorial_game_upgrade_notice(); assert(notices==1);
    tale=copy[2]; metar=tale; mode=TUTORIAL_MODE_DISABLED;
    tutorial_game_upgrade_notice();
    assert(notices==2 && strstr(last_message,"saved tutorial setting is Disabled"));
    assert(player.tutorial_deferred); /* Notice/mode do not bypass old hero lock. */
    tale.tutorial_upgrade_pending=1; metar=tale; save_result=-1;
    tutorial_game_upgrade_notice(); assert(tale.tutorial_upgrade_pending && metar.tutorial_upgrade_pending);
    save_result=0; answer=-1; int old_saves=saves;
    tutorial_game_upgrade_notice(); assert(tale.tutorial_upgrade_pending && saves==old_saves);
    answer=0; player.tutorial_deferred=false; mode=TUTORIAL_MODE_EXTENDED;
    tutorial_game_upgrade_notice(); assert(strstr(last_message,"new character") && strstr(last_message,"available now"));
    tale.tutorial_upgrade_pending=1; blitz=true; int old_notices=notices;
    tutorial_game_upgrade_notice(); assert(notices==old_notices && tale.tutorial_upgrade_pending);
    puts("Tutorial upgrade: PASS (0.9.8 version, byte migration/resave/new hero, unchanged meta layout, per-Tale notices, Off/Blitz, acknowledgment/failure)");
    return 0;
}
'''


def main():
    load = (ROOT / "src/fs/load-player.c").read_text()
    save = (ROOT / "src/fs/save-player.c").read_text()
    persistence = (ROOT / "src/metarun/metarun-persistence.c").read_text()
    game = (ROOT / "src/tutorial/tutorial-game.c").read_text()
    birth = (ROOT / "src/birth/birth-setup.c").read_text()
    header = (ROOT / "src/metarun.h").read_text()
    assert "rd_s32b(&min_depth_counter);\n    rd_tutorial_character_state();" in load
    assert "wr_s32b(min_depth_counter);\n    wr_tutorial_character_state();" in save
    assert "memset(p_ptr, 0, sizeof(player_type));" in function(birth, "void player_wipe(")
    assert "tutorial_set_character_blocked(p_ptr->tutorial_deferred);" in function(game, "void tutorial_game_start(")
    assert "p_ptr->tutorial_deferred" in function(game, "static bool gameplay_available(")
    assert "p_ptr->tutorial_deferred" in function(game, "void tutorial_game_lifecycle(")
    assert persistence.index("metarun_migrate_tutorial_notice(&metaruns[i], &header)") < persistence.index("metar = metaruns[current_run]")
    assert "interface_settings_migrated || tutorial_settings_migrated" in persistence
    old = re.search(r"typedef struct metarun\s*\{.*?\}\s*metarun;", header, re.S).group()
    old = re.sub(r"    byte tutorial_upgrade_pending;[^\n]*\n", "", old)
    old = old.replace("reserved_runtime[30]", "reserved_runtime[31]")
    old = old.replace("typedef struct metarun", "typedef struct metarun_pre098").replace("} metarun;", "} metarun_pre098;")
    bodies = [function(load, "static void rd_tutorial_character_state("),
              function(save, "static void wr_tutorial_character_state("),
              function(persistence, "static bool metarun_header_before("),
              function(persistence, "static void metarun_migrate_tutorial_notice("),
              function(game, "static void tutorial_game_upgrade_notice(")]
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__OLD_META_LAYOUT__", old).replace("__FUNCTIONS__", "\n\n".join(bodies)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-Wall", "-Wextra",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "_deps/SDL/libSDL3.dll.a", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
