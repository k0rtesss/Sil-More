#!/usr/bin/env python3
"""Execute production monster-visibility preview paths in a C fixture.

Uses actual engine types and update logic. View flags, perception dice, and
external side effects are controlled stubs; this does not test field of view.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-visibility-check"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static\s+)?(?:void|bool|int)\s+" + re.escape(name)
                      + r"\s*\([^;{]*\)\s*\{", source, re.M)
    assert match, f"Missing function: {name}"
    return source[match.start():source.index("\n}", match.end()) + 2]


PRODUCTION = "\n\n".join(function("src/monster/monster-update.c", name)
    for name in ("listen_visual_effects_suppressed", "detect_monster_noise_aux",
                 "detect_monster_noise", "listen",
                 "monster_passes_invisibility_check", "update_mon_aux",
                 "update_mon", "update_mon_for_generation"))
DISTANCE = function("src/cave/cave-geometry.c", "distance")

HARNESS = r'''
#include "angband.h"
#include "log/log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static player_type player_body;
player_type *p_ptr = &player_body;
static player_other options_body;
player_other *op_ptr = &options_body;
static monster_type monsters[4];
monster_type *mon_list = monsters;
static monster_race races[4];
monster_race *r_info = races;
static monster_lore lore[4];
monster_lore *l_list = lore;
static u16b cells[256][256];
u16b (*cave_info)[256] = cells;
s32b playerturn;
bool character_generated;
s16b character_icky, character_xtra;
byte misc_to_attr[256];
char misc_to_char[256];
static u64b rng_state;
static bool keen_seen;
static int side_effects, roll_diagnostics, roll_calls, forced_result;

u64b Rand_state_export(void) { return rng_state; }
void Rand_state_import(u64b state) { rng_state = state; }
u64b Rand_state_push(u64b state) {
    u64b old = rng_state; rng_state = state; return old;
}
void Rand_state_pop(u64b state) { rng_state = state; }
int skill_check(monster_type *a, int skill, int difficulty, monster_type *b) {
    (void)a; (void)b;
    ++roll_calls;
    rng_state = rng_state * 6364136223846793005ULL + 1;
    if (cheat_skill_rolls) ++roll_diagnostics;
    return forced_result != 9999 ? forced_result
        : (int)((rng_state >> 32) % 20) - 10 + skill - difficulty;
}
bool screen_startup_supporting_panes_hidden_active(void) { return false; }
int flow_dist(int flow, int y, int x) { (void)flow; (void)y; (void)x; return 5; }
bool singing(int song) { (void)song; return false; }
int ability_bonus(int skill, int ability) { (void)skill; (void)ability; return 0; }
bool graphics_are_ascii(void) { return true; }
void print_rel(char c, byte a, int y, int x) { (void)c; (void)a; (void)y; (void)x; ++side_effects; }
void move_cursor_relative(int y, int x) { (void)y; (void)x; ++side_effects; }
errr Term_fresh(void) { ++side_effects; return 0; }
int monster_skill(monster_type *m, int skill) { (void)m; (void)skill; return 0; }
bool seen_by_keen_senses(int y, int x) { (void)y; (void)x; return keen_seen; }
bool monster_clear_vala_state(monster_type *m) { (void)m; ++side_effects; return false; }
void calc_monster_speed(int y, int x) { (void)y; (void)x; ++side_effects; }
void target_set_monster(int m) { (void)m; ++side_effects; }
void health_track(int m) { (void)m; ++side_effects; }
void lite_spot(int y, int x) { (void)y; (void)x; ++side_effects; }
void disturb(int a, int b) { (void)a; (void)b; ++side_effects; }
void ident_see_invisible(const monster_type *m) { (void)m; ++side_effects; }
void ident_haunted(void) { ++side_effects; }
s32b adjusted_mon_exp(const monster_race *r, bool kill) { (void)r; (void)kill; return 10; }
void gain_exp(s32b amount) { p_ptr->exp += amount; ++side_effects; }
void monster_desc_race(char *s, size_t n, int r) { (void)r; snprintf(s,n,"test"); ++side_effects; }
void do_cmd_note(char *s, int d) { (void)s; (void)d; ++side_effects; }
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt; ++side_effects;
}
static char *test_format(const char *s, ...) { (void)s; ++side_effects; return "test"; }
static size_t test_strlcpy(char *dest, const char *src, size_t len) {
    snprintf(dest, len, "%s", src); ++side_effects; return strlen(src);
}
#define format test_format
#define SDL_strlcpy test_strlcpy
''' + DISTANCE + "\n" + PRODUCTION + r'''

static void reset(void) {
    memset(&player_body, 0, sizeof(player_body));
    memset(&options_body, 0, sizeof(options_body));
    memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races));
    memset(lore, 0, sizeof(lore));
    memset(cells, 0, sizeof(cells));
    p_ptr->py = p_ptr->px = 10;
    monsters[1].r_idx = 1;
    monsters[1].fy = 10; monsters[1].fx = 12;
    cells[10][12] = CAVE_VIEW | CAVE_SEEN;
    playerturn = 0; rng_state = 1234567;
    keen_seen = false;
    side_effects = roll_diagnostics = roll_calls = 0;
    forced_result = 9999;
    character_generated = true; character_icky = character_xtra = 0;
}

static void preview(bool expected) {
    player_type old_player = player_body;
    player_other old_options = options_body;
    monster_type old_monster = monsters[1];
    monster_lore old_lore[4];
    memcpy(old_lore, lore, sizeof(lore));
    int old_effects = side_effects, old_diagnostics = roll_diagnostics;
    u64b old_rng = rng_state;
    update_mon_for_generation(1);
    assert(monsters[1].ml == expected);
    assert(monsters[1].cdis == distance(p_ptr->py, p_ptr->px,
        monsters[1].fy, monsters[1].fx));
    old_monster.ml = monsters[1].ml;
    old_monster.cdis = monsters[1].cdis;
    assert(!memcmp(&old_monster, &monsters[1], sizeof(old_monster)));
    assert(!memcmp(&old_player, &player_body, sizeof(old_player)));
    assert(!memcmp(&old_options, &options_body, sizeof(old_options)));
    assert(!memcmp(old_lore, lore, sizeof(lore)));
    assert(side_effects == old_effects && roll_diagnostics == old_diagnostics);
    assert(rng_state == old_rng);
}

static void visibility_cases(void) {
    reset(); preview(true);
    cells[10][12] = 0; preview(false);
    cells[10][12] = CAVE_VIEW; preview(false);
    keen_seen = true; preview(true);
    keen_seen = false;
    p_ptr->telepathy = 1; preview(true);
    races[1].flags2 = RF2_MINDLESS; preview(false);
    monsters[1].mflag = MFLAG_MARK; preview(true);
    monsters[1].mflag = 0; cheat_monsters = true; preview(true);
    cheat_monsters = false;
    monsters[1].encountered = true; races[1].flags1 = RF1_NEVER_MOVE;
    preview(true);
    reset(); p_ptr->blind = true; preview(false);
    reset(); p_ptr->niena_quest = NIENA_QUEST_ACTIVE; preview(true);
    assert(!p_ptr->niena_monsters_seen && !p_ptr->encounter_exp && !p_ptr->exp);
    /* Normal commit still awards an encounter and updates lore/quests. */
    monsters[1].ml = false;
    update_mon(1, true);
    assert(monsters[1].ml && monsters[1].encountered);
    assert(lore[1].psights == 1 && p_ptr->encounter_exp == 10);
    assert(p_ptr->niena_monsters_seen == 1 && side_effects > 0);
}

static void invisible_cases(void) {
    reset(); races[1].flags2 = RF2_INVISIBLE;
    cheat_skill_rolls = true;
    p_ptr->skill_use[S_PER] = 100; preview(true);
    p_ptr->skill_use[S_PER] = -100; preview(false);
    p_ptr->skill_use[S_PER] = 4;
    update_mon_for_generation(1);
    bool expected = monsters[1].ml;
    preview(expected);
    assert(roll_calls == 4 && roll_diagnostics == 0);
    u64b saved = rng_state;
    monsters[1].ml = false;
    update_mon(1, true);
    assert(monsters[1].ml == expected && rng_state == saved);
    update_mon(1, true);
    assert(monsters[1].ml == expected && rng_state == saved);
    assert(roll_diagnostics == 2);
    playerturn = 1;
    update_mon(1, true);
    assert(rng_state != saved && roll_diagnostics == 3);
    saved = rng_state;
    update_mon_for_generation(1);
    assert(rng_state == saved && roll_diagnostics == 3);
}

static void listening_cases(void) {
    reset();
    p_ptr->active_ability[S_PER][PER_LISTEN] = true;
    cells[10][12] = CAVE_VIEW; /* Dark, but in the actual visual field. */
    monsters[1].noise = 7;
    cheat_skill_rolls = true;
    forced_result = 0; preview(false);
    forced_result = 10; preview(false); /* Sound icon alone cannot trigger it. */
    forced_result = 11; preview(true); /* Dramatic success reveals the monster. */
    assert(monsters[1].noise == 7 && roll_diagnostics == 0);
    monsters[1].ml = false;
    u64b saved = rng_state;
    update_mon(1, true);
    assert(monsters[1].ml && monsters[1].noise == 0);
    assert(rng_state == saved && roll_diagnostics == 1);
    assert(monsters[1].encountered && lore[1].psights == 1);
    /* Immobile monsters cannot be found by listening. */
    monsters[1].ml = false;
    monsters[1].encountered = false;
    races[1].flags1 = RF1_NEVER_MOVE;
    preview(false);
    races[1].flags1 = 0;
    /* Outside LOS listening may reveal ml, but the tutorial's separate LOS
     * predicate still excludes the monster (tested in the tutorial harness). */
    cells[10][12] = 0;
    preview(true);
}

int main(void) {
    visibility_cases(); invisible_cases(); listening_cases();
    puts("Monster visibility preview: production paths, listening thresholds, no preview effects, turn-zero determinism, later RNG: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-Wall", "-Wextra", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
