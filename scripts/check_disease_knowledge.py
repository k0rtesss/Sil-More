#!/usr/bin/env python3
"""Exercise production disease menu rows and item text without opening a game.

Uses real formatting and disease rules; checks text/hidden knowledge, not SDL
layout, input navigation, or a complete Gem use.
"""
import os
from pathlib import Path
import subprocess
from check_disease import function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/disease-knowledge-check"

PRELUDE = r'''
#include "angband.h"
#include <assert.h>
#include <stdio.h>
static player_type player;
player_type *p_ptr = &player;
static object_kind kinds[2];
object_kind *k_info = kinds;
static ego_item_type egos[1];
ego_item_type *e_info = egos;
static int rolls[2], calls, healing_previews;
u32b Rand_div(u32b bound) {assert(bound==2 && calls<2);return rolls[calls++];}
static char output[1024];
static void p_text_out(cptr text) {
    size_t end=strlen(output);strnfcat(output,sizeof(output),&end,"%s",text);
}
static void p_text_out_c(byte color,cptr s) {(void)color;p_text_out(s);}
int consumable_healing_points(const object_type *item) {
    ++healing_previews;return item->sval==SV_FOOD_HEALING?20:0;
}
static char s[100][200], t[100][200];
static bool good[100];
'''

TESTS = r'''
int main(void) {
    assert(rows()==0 && calls==0);
    p_ptr->diseased=100;p_ptr->disease_name=1;p_ptr->disease_cure=SV_FOOD_HEALING;
    rolls[0]=1;assert(rows()==1 && calls==1);
    assert(!strcmp(s[0],"Disease: Unknown") && strstr(t[0],"Herb cure: Unknown"));
    assert(!strstr(t[0],"herb of Healing") && !good[0]);
    calls=0;rolls[0]=0;rolls[1]=1;assert(rows()==1 && calls==2);
    assert(strstr(s[0],disease_name()) && strstr(t[0],"Herb cure: Unknown"));
    calls=0;rolls[0]=1;assert(rows()==1 && calls==1);
    assert(strstr(s[0],disease_name()) && strstr(t[0],"Herb cure: Unknown"));
    calls=0;rolls[0]=0;assert(rows()==1 && calls==1);
    assert(strstr(t[0],"Cure: a herb of Healing") && strstr(t[0],"usual effects"));
    calls=0;assert(rows()==1 && !calls);
    p_ptr->disease_knowledge=0;p_ptr->active_ability[S_PER][PER_ALCHEMY]=true;
    assert(rows()==1 && !calls && strstr(t[0],"Cure: a herb of Healing"));
    object_type herb={.k_idx=1,.tval=TV_FOOD,.sval=SV_FOOD_HEALING,.number=1};
    assert(!describe_consumable_healing(&herb) && !output[0] && !healing_previews);
    kinds[1].aware=true;
    assert(describe_consumable_healing(&herb) && !healing_previews);
    assert(strstr(output,disease_name()) && strstr(output,"nourishment do not apply"));
    output[0]=0;p_ptr->disease_knowledge=0;p_ptr->mhp=50;p_ptr->chp=20;
    assert(describe_consumable_healing(&herb) && healing_previews==1);
    assert(strstr(output,"20 health") && !strstr(output,disease_name()));
    p_ptr->disease_knowledge=DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE;
    for(int name=1;name<=DISEASE_NAME_COUNT;name++)
        for(int cure=0;cure<DISEASE_HERB_COUNT;cure++) {
            p_ptr->disease_name=name;p_ptr->disease_cure=cure;herb.sval=cure;
            assert(rows()==1 && !calls && strstr(s[0],disease_name()));
            assert(strstr(t[0],disease_cure_name()) && strlen(t[0])<sizeof(t[0])-1);
            output[0]=0;assert(describe_consumable_healing(&herb));
            assert(strstr(output,disease_name()) && healing_previews==1);
        }
    puts("Disease menu and item text: unknown/name/cure, retained discoveries, Alchemist, all names/herbs, no hidden cure disclosure: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    menu = function("src/spell/spell-utility.c", "self_knowledge")
    menu = menu[menu.index("    /* One diagnosis attempt"):menu.index("    // Get item flags")]
    helpers = "\n".join(function("src/player/effects.c", name) for name in
                        ["disease_name", "disease_cure_name", "disease_identify", "disease_herb_matches"])
    rows = "static int rows(void) {int i=0;\n" + menu + "\nreturn i;\n}\n"
    source = OUT / "check.c"
    source.write_text(PRELUDE + helpers + rows
                      + function("src/object/object-info.c", "describe_consumable_healing")
                      + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "-Werror", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), str(ROOT / "src/format.c"), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=OUT, env=env, check=True,
                            timeout=30, capture_output=True, text=True)
    (OUT / "result.txt").write_text(result.stdout, encoding="utf-8")
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
