#!/usr/bin/env python3
"""Exercise production achievement rules and real SDL persistence in temporary profiles.

The UI notification and platform service boundaries are doubles. No player
profile, Steam account, Play Games account, or game window is opened.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievements-check"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "achievements.h"
#include "cJSON.h"
#include "metarun.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static player_type player;
static player_other options;
player_type *p_ptr = &player;
player_other *op_ptr = &options;
cptr ANGBAND_DIR_APEX;
bool character_generated = true;
static int notifications, messages, syncs;
static int carried_silmarils;
int silmarils_possessed(void) { return carried_silmarils; }
static uint64_t remote, submitted, remote_upper, submitted_upper;
maxima *z_info;
monster_type *mon_list;
monster_race *r_info;
s16b mon_max;
#define ROAD_BIT (UINT64_C(1) << SIL_ACH_THE_ROAD_GOES_EVER_ON)
static Uint64 ticks;
static bool fail_flush, fail_rename;
static bool broken_oath, blitz;
metarun metar;
bool oath_invalid(int oath) { (void)oath; return broken_oath; }
bool run_mode_is_blitz(void) { return blitz; }
static int8_t curse_stacks[METAR_CURSE_SLOTS];
int8_t* active_curse_stacks(void) { return curse_stacks; }

Uint64 __wrap_SDL_GetTicks(void) { return ticks; }
bool __real_SDL_FlushIO(SDL_IOStream *file);
bool __wrap_SDL_FlushIO(SDL_IOStream *file)
{
    if (fail_flush) { SDL_SetError("test failed flush"); return false; }
    return __real_SDL_FlushIO(file);
}
bool __real_SDL_RenamePath(const char *from, const char *to);
bool __wrap_SDL_RenamePath(const char *from, const char *to)
{
    if (fail_rename) { SDL_SetError("test failed rename"); return false; }
    return __real_SDL_RenamePath(from, to);
}
void log_log(int level, const char *file, int line, const char *fmt, ...)
{ (void)level; (void)file; (void)line; (void)fmt; }
size_t strnfmt(char *out, size_t size, cptr fmt, ...)
{
    va_list args; va_start(args, fmt);
    int result = vsnprintf(out, size, fmt, args); va_end(args);
    return result < 0 ? 0 : (size_t)result;
}
bool path_build(char *out, size_t size, cptr base, cptr name)
{ return snprintf(out, size, "%s/%s", base, name) < (int)size; }
void msg_print(cptr text) { assert(strstr(text, "Achievement unlocked:")); ++messages; }
void sdl_popup_notification_show(cptr text)
{ assert(strstr(text, "Achievement unlocked:")); ++notifications; }
void sil_play_games_sync(const uint64_t* masks) { submitted = masks[0]; submitted_upper = masks[1]; ++syncs; }
uint64_t sil_play_games_unlocked(unsigned word) { return word == 0 ? remote : word == 1 ? remote_upper : 0; }
#ifdef SIL_STEAMWORKS
void sil_steamworks_sync_achievements(const uint64_t* masks) { (void)masks; }
uint64_t sil_steamworks_achievements_unlocked(unsigned word) { (void)word; return 0; }
#endif

/* Gameplay callback boundaries; full production routes have separate fixtures. */
void sil_achievement_character_on_player_turn(void) {}
void sil_achievement_character_on_monster_killed(int m,int who) { (void)m;(void)who; }
void sil_achievement_lore_on_kill(int m,int who) { (void)m;(void)who; }
void sil_achievement_lore_on_escape(int n) { (void)n; }
void sil_achievement_lore_on_travel(int from,int to) { (void)from;(void)to; }
void sil_achievement_lore_reset(void) {}
void sil_achievements_discovery_reset(bool fresh) { (void)fresh; }
void sil_achievements_on_partition_entry(int y,int x) { (void)y;(void)x; }
void sil_achievements_song_begin(void) {}
static void clean_player(void)
{
    memset(&player, 0, sizeof(player));
    memset(&options, 0, sizeof(options));
    carried_silmarils = 0;
    player.playing = true;
    player.chp = player.mhp = 20;
    player.energy_use = 100;
    player.song1 = player.song2 = SNG_NOTHING;
    character_generated = true;
}
static void try_all(void)
{
    for (int id = SIL_ACH_NO_RINGS_ATTACHED; id < SIL_ACH_COUNT; ++id)
        sil_achievements_on_event((sil_achievement_id)id);
    sil_achievements_on_door_jammed(8, 9);
    sil_achievements_on_door_bashed(8, 9);
    sil_achievements_on_fall();
    sil_achievements_on_depth(10);
    sil_achievements_on_silmaril();
    sil_achievements_on_escape(1);
    sil_achievements_on_smithing_completed(true);
    sil_achievements_on_light_kill();
    sil_achievements_on_morgoth_defeated();
    assert(sil_achievements_mask(0) == 0);
    assert(!notifications && !messages);
}
static void blocked_games(void)
{
    clean_player(); character_generated = false; try_all();
    clean_player(); player.playing = false; try_all();
    clean_player(); player.is_dead = true; try_all();
    clean_player(); player.wizard = true; try_all();
    clean_player(); player.game_type = 1; try_all();
    for (int bit = 1; bit <= 8; bit <<= 1) {
        clean_player(); player.noscore = bit; try_all();
    }
    for (int opt = OPT_CHEAT; opt < OPT_CHEAT + 12; ++opt) {
        clean_player(); options.opt[opt] = true; try_all();
    }
    for (int opt = OPT_SCORE; opt < OPT_SCORE + 6; ++opt) {
        clean_player(); options.opt[opt] = true; try_all();
    }
    clean_player();
}
static char *saved_file(size_t *size)
{
    char name[1200]; snprintf(name, sizeof(name), "%s/achievements.json", ANGBAND_DIR_APEX);
    return SDL_LoadFile(name, size);
}
static void earn_all(void)
{
    blocked_games();
    sil_achievements_on_depth(1); assert(!sil_achievements_mask(0));
    sil_achievements_on_depth(2); assert(!sil_achievements_mask(0));
    sil_achievements_on_depth(3); assert(sil_achievements_mask(0) == ROAD_BIT);
    assert(notifications == 1 && messages == 1);
    sil_achievements_on_depth(3); assert(notifications == 1 && messages == 1);
    sil_achievements_on_escape(0); assert(sil_achievements_mask(0) == ROAD_BIT);
    player.depth = 1; sil_achievements_on_escape(1); assert(sil_achievements_mask(0) == ROAD_BIT);
    sil_achievements_on_depth(10);
    uint64_t first = sil_achievement_unlocked_at(SIL_ACH_INTO_THE_DEEP);
    assert(first && sil_achievements_mask(0) == (1 | ROAD_BIT) && sil_achievements_saved());
    sil_achievements_on_depth(20);
    assert(notifications == 2 && sil_achievement_unlocked_at(0) == first);
    clean_player(); /* A new character and a new Tale do not reset the profile. */
    assert(sil_achievements_mask(0) == (1 | ROAD_BIT));
    sil_achievements_on_silmaril();
    sil_achievements_on_escape(1);
    assert(sil_achievements_mask(0) == (7 | ROAD_BIT) && notifications == 4 && messages == 4);
    sil_achievements_on_silmaril(); sil_achievements_on_escape(1);
    assert(notifications == 4 && sil_achievements_saved());
    sil_achievements_update();
    assert(submitted == (7 | ROAD_BIT) && syncs == 1);
    sil_achievements_update(); assert(syncs == 1);
    ticks = 1000; sil_achievements_update(); assert(syncs == 2 && submitted == (7 | ROAD_BIT));
}
static void failed_write(void)
{
    sil_achievements_on_depth(10);
    size_t size; char *original = saved_file(&size); assert(original);
    fail_flush = true;
    sil_achievements_on_silmaril();
    assert(sil_achievements_mask(0) == (3 | ROAD_BIT) && !sil_achievements_saved());
    size_t kept_size; char *kept = saved_file(&kept_size);
    assert(kept && size == kept_size && !memcmp(kept, original, size)); SDL_free(kept);
    fail_flush = false; fail_rename = true;
    sil_achievements_flush();
    kept = saved_file(&kept_size);
    assert(kept && size == kept_size && !memcmp(kept, original, size));
    SDL_free(kept); SDL_free(original);
    fail_rename = false;
    ticks = 30001; sil_achievements_update();
    assert(sil_achievements_saved() && sil_achievements_mask(0) == (3 | ROAD_BIT));
}
static void preserve_invalid_file(void)
{
    size_t size; char *original = saved_file(&size); assert(original);
    sil_achievements_on_depth(10); sil_achievements_flush();
    assert(!sil_achievements_saved());
    size_t kept_size; char *kept = saved_file(&kept_size);
    assert(kept && size == kept_size && !memcmp(kept, original, size));
    SDL_free(kept); SDL_free(original);
}
static bool earned(int id) { return sil_achievement_unlocked_at(id) != 0; }
static void expanded_conditions(const char *test)
{
    sil_achievements_begin_run();
    player.depth = 5;
    if (!strcmp(test, "new-events")) {
        assert(SIL_ACH_COUNT == 112);
#ifdef SIL_STEAMWORKS
        assert(SIL_ACH_ACTIVE_COUNT == 100);
#else
        assert(SIL_ACH_ACTIVE_COUNT == 112);
#endif
        int hidden = 0;
        for (int id = 0; id < SIL_ACH_COUNT; ++id)
            if (sil_achievement_get(id)) hidden += sil_achievement_get(id)->hidden;
#ifdef SIL_STEAMWORKS
        assert(hidden == 47);
#else
        assert(hidden == 50);
#endif
        assert(!sil_achievement_get(SIL_ACH_THREE_LIGHTS_IN_EXILE)->hidden);
        assert(sil_achievement_revealed(SIL_ACH_THREE_LIGHTS_IN_EXILE));
        assert(!strcmp(sil_achievement_get(SIL_ACH_THE_QUIET_ROAD)->title,
            "Not to Strike Without Need"));
        assert(!strcmp(sil_achievement_get(SIL_ACH_NO_RINGS_ATTACHED)->title,
            "No Rings Attached"));
        for (int id = SIL_ACH_NO_RINGS_ATTACHED; id < SIL_ACH_COUNT; ++id) {
            int before = notifications;
            sil_achievements_on_event((sil_achievement_id)id);
            if (!sil_achievement_get(id) || id == SIL_ACH_SWIFT_AS_THE_WIND
                || id == SIL_ACH_TENFOLD_DOOM) {
                assert(!earned(id) && notifications == before);
                continue;
            }
            assert(earned(id) && notifications == before + 1);
            sil_achievements_on_event((sil_achievement_id)id);
            assert(notifications == before + 1);
        }
        assert(sil_achievements_saved());
    } else if (!strcmp(test, "door-history")) {
        sil_achievements_on_door_bashed(8, 9);
        assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
        sil_achievements_on_door_jammed(8, 9);
        sil_achievements_on_door_jammed(8, 10);
        sil_achievements_on_door_bashed(9, 9);
        assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
        sil_achievements_forget_door(8, 9);
        sil_achievements_on_door_bashed(8, 9);
        assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
        sil_achievements_on_level_entry(true);
        sil_achievements_on_door_bashed(8, 10);
        assert(earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
    } else if (!strcmp(test, "door-level")) {
        sil_achievements_on_door_jammed(8, 9);
        sil_achievements_on_level_entry(false);
        sil_achievements_on_door_bashed(8, 9);
        assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
        options.opt[OPT_cheat_peek] = true;
        sil_achievements_on_door_jammed(8, 9);
        options.opt[OPT_cheat_peek] = false;
        sil_achievements_on_door_bashed(8, 9);
        assert(!earned(SIL_ACH_KNOCK_WITH_YOUR_HEAD));
        sil_achievements_on_door_jammed(-1, 8);
        sil_achievements_on_door_jammed(MAX_DUNGEON_HGT, 0);
    } else if (!strcmp(test, "fall-arrival")) {
        sil_achievements_on_level_entry(false);
        assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        player.is_dead = true;
        sil_achievements_on_fall();
        assert(!(player.achievement_flags & SIL_ACH_RUN_FALLING));
        player.is_dead = false;
        sil_achievements_on_fall();
        assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        assert(player.achievement_fall_depth == 5);
        sil_achievements_on_level_entry(true); /* No descent occurred. */
        assert(!earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        sil_achievements_on_fall();
        player.depth += 2;
        sil_achievements_on_level_entry(false);
        assert(earned(SIL_ACH_DOWN_TO_GOBLIN_TOWN));
        assert(!(player.achievement_flags & SIL_ACH_RUN_FALLING));
        assert(player.achievement_fall_depth == -1);
    } else assert(false);
}
static void escape_challenges(const char *test)
{
    if (!strcmp(test, "triple")) {
        sil_achievements_begin_run(); sil_achievements_on_escape(3);
        assert(earned(SIL_ACH_THREE_LIGHTS_IN_EXILE) && earned(SIL_ACH_THE_QUIET_ROAD));
        assert(!earned(SIL_ACH_WORD_UNBROKEN));
    } else if (!strcmp(test, "pacifist")) {
        sil_achievements_begin_run();
        sil_achievements_on_monster_killed(1, 0); sil_achievements_on_monster_killed(1, 4);
        assert(!player.achievement_kills);
        sil_achievements_on_escape(1); assert(earned(SIL_ACH_THE_QUIET_ROAD));
    } else if (!strcmp(test, "killer")) {
        sil_achievements_begin_run(); sil_achievements_on_monster_killed(1, -1);
        sil_achievements_on_escape(1); assert(!earned(SIL_ACH_THE_QUIET_ROAD));
    } else if (!strcmp(test, "legacy-run")) {
        sil_achievements_on_escape(1); assert(!earned(SIL_ACH_THE_QUIET_ROAD));
    } else if (!strcmp(test, "oath")) {
        player.oath_type = 1; broken_oath = true;
        sil_achievements_on_escape(1); assert(!earned(SIL_ACH_WORD_UNBROKEN));
        broken_oath = false; sil_achievements_on_escape(1); assert(earned(SIL_ACH_WORD_UNBROKEN));
    } else if (!strcmp(test, "song")) {
        player.skill_base[S_SNG] = 19;
        for (int i=0; i<120; ++i) sil_achievements_on_player_turn();
        assert(!player.achievement_singing_turns);
        player.song1 = SNG_ELBERETH;
        player.energy_use = 0; sil_achievements_on_singing_turn();
        assert(!player.achievement_singing_turns); player.energy_use = 100;
        for (int i=0; i<99; ++i) sil_achievements_on_singing_turn();
        sil_achievements_on_escape(1); assert(!earned(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN));
        player.achievement_singing_turns = 0;
        player.skill_base[S_SNG] = 20; sil_achievements_on_escape(1);
        assert(earned(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN));
    } else if (!strcmp(test, "gates")) {
        sil_achievements_on_level_entry(false); assert(!earned(SIL_ACH_THERE_AND_BACK_AGAIN));
        player.morgoth_hall_entered = true; player.depth = 1;
        sil_achievements_on_level_entry(false); assert(!earned(SIL_ACH_THERE_AND_BACK_AGAIN));
        player.depth = 0; carried_silmarils = 1;
        sil_achievements_on_level_entry(false); assert(!earned(SIL_ACH_THERE_AND_BACK_AGAIN));
        carried_silmarils = 3;
        sil_achievements_on_escape(3); assert(!earned(SIL_ACH_THERE_AND_BACK_AGAIN));
        carried_silmarils = 0;
        player.depth = 0; sil_achievements_on_level_entry(false);
        assert(earned(SIL_ACH_THERE_AND_BACK_AGAIN));
    } else if (!strcmp(test, "taint")) {
        sil_achievements_begin_run(); options.opt[OPT_cheat_peek] = true;
        sil_achievements_on_player_turn(); options.opt[OPT_cheat_peek] = false;
        sil_achievements_on_escape(3); assert(!sil_achievements_mask(0));
    } else assert(false);
}
static void comeback(void)
{
    maxima limits = {0};
    monster_type monsters[2] = {0};
    monster_race races[2] = {0};
    limits.r_max = 2; z_info = &limits;
    monsters[1].r_idx = 1; mon_list = monsters; mon_max = 2; r_info = races;
    player.chp = 2;
    sil_achievements_on_monster_killed(1, -1); assert(!earned(SIL_ACH_BY_A_THREAD));
    player.depth = 5; player.chp = 1;
    sil_achievements_on_monster_killed(1, 0); assert(!earned(SIL_ACH_BY_A_THREAD));
    races[1].flags1 = RF1_PEACEFUL;
    sil_achievements_on_monster_killed(1, -1); assert(!earned(SIL_ACH_BY_A_THREAD));
    races[1].flags1 = 0;
    /* A save/level boundary or previous healing cannot impose an old hit window. */
    player.chp = 2; sil_achievements_on_healing(); player.chp = 1;
    sil_achievements_on_level_entry(false);
    assert(!(player.achievement_flags & SIL_ACH_RUN_COMEBACK));
    sil_achievements_on_monster_killed(1,-1); assert(earned(SIL_ACH_BY_A_THREAD));
}
static void craft_and_combat(void)
{
    sil_achievements_on_smithing_completed(false);
    assert(earned(SIL_ACH_FORGED_BY_YOUR_HAND) && !earned(SIL_ACH_A_NAME_WORTH_REMEMBERING));
    sil_achievements_on_smithing_completed(true); assert(earned(SIL_ACH_A_NAME_WORTH_REMEMBERING));
    sil_achievements_on_light_kill(); assert(earned(SIL_ACH_LIGHT_OF_THE_ELDAR));
    sil_achievements_on_morgoth_defeated(); assert(earned(SIL_ACH_THE_SHADOW_FALLS));
    int before = notifications;
    sil_achievements_on_light_kill(); sil_achievements_on_morgoth_defeated();
    sil_achievements_on_smithing_completed(true); assert(notifications == before);
}
static void tale(const char *test)
{
    metar.id = 17; player.is_dead = true; player.playing = false;
    player.pcharacter = 1;
    if (!strcmp(test, "tale-first")) {
        sil_achievements_on_run_completed(); sil_achievements_on_run_completed();
        assert(!earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(test, "tale-second")) {
        player.pcharacter = 2; sil_achievements_on_run_completed();
        sil_achievements_on_tale_completed(); assert(earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(test, "tale-repeat")) {
        sil_achievements_on_run_completed(); sil_achievements_on_tale_completed();
        assert(!earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(test, "tale-blitz")) {
        blitz = true; sil_achievements_on_run_completed(); player.pcharacter = 2;
        sil_achievements_on_run_completed(); sil_achievements_on_tale_completed();
        assert(!earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(test, "tale-discard")) {
        sil_achievements_on_run_completed();
        metar.id = 18; player.pcharacter = 2;
        sil_achievements_on_run_completed();
        sil_achievements_on_tale_discarded(17);
        metar.id = 17; sil_achievements_on_tale_completed();
        assert(!earned(SIL_ACH_THE_LONG_TALE));
        metar.id = 18; player.pcharacter = 3;
        sil_achievements_on_run_completed(); sil_achievements_on_tale_completed();
        assert(earned(SIL_ACH_THE_LONG_TALE));
    } else assert(false);
}
int main(int argc, char **argv)
{
    assert(argc == 3); ANGBAND_DIR_APEX = argv[2]; clean_player();
    assert(SDL_Init(0)); sil_achievements_init();
    assert(!sil_achievement_get(-1) && !sil_achievement_get(SIL_ACH_COUNT));
    assert((sil_achievement_get(SIL_ACH_RELEASE_FROM_BONDAGE) != NULL)
        == sil_achievement_enabled(SIL_ACH_RELEASE_FROM_BONDAGE));
    assert((sil_achievement_get(SIL_ACH_A_WILL_OF_YOUR_OWN) != NULL)
        == sil_achievement_enabled(SIL_ACH_A_WILL_OF_YOUR_OWN));
    assert(!sil_achievement_unlocked_at(-1));
    if (!strcmp(argv[1], "earn")) earn_all();
    else if (!strcmp(argv[1], "reload")) {
        assert(sil_achievements_mask(0) == (7 | ROAD_BIT) && sil_achievements_saved());
        sil_achievements_update(); assert(submitted == (7 | ROAD_BIT) && !notifications);
    } else if (!strcmp(argv[1], "failed-write")) failed_write();
    else if (!strcmp(argv[1], "invalid")) preserve_invalid_file();
    else if (!strcmp(argv[1], "import")) {
        remote = 2; sil_achievements_update();
        assert(sil_achievements_mask(0) == 2 && sil_achievements_saved() && !notifications);
        ticks = 1000; remote = 0; sil_achievements_update();
        assert(sil_achievements_mask(0) == 2 && submitted == 2);
    } else if (!strcmp(argv[1], "retired")) {
        uint64_t retired = (UINT64_C(1) << SIL_ACH_RELEASE_FROM_BONDAGE)
            | (UINT64_C(1) << SIL_ACH_A_WILL_OF_YOUR_OWN);
#ifdef SIL_STEAMWORKS
        assert(!(sil_achievement_word_mask(0) & retired));
#else
        assert((sil_achievement_word_mask(0) & retired) == retired);
#endif
        remote = retired; sil_achievements_update();
        sil_achievements_on_event(SIL_ACH_RELEASE_FROM_BONDAGE);
        sil_achievements_on_event(SIL_ACH_A_WILL_OF_YOUR_OWN);
        sil_achievements_flush();
        uint64_t expected = 1;
#ifndef SIL_STEAMWORKS
        expected |= retired;
#endif
        assert(sil_achievements_mask(0) == expected && !notifications);
        assert(earned(SIL_ACH_RELEASE_FROM_BONDAGE) == sil_achievement_enabled(SIL_ACH_RELEASE_FROM_BONDAGE));
        assert(earned(SIL_ACH_A_WILL_OF_YOUR_OWN) == sil_achievement_enabled(SIL_ACH_A_WILL_OF_YOUR_OWN));
    } else if (!strcmp(argv[1], "unknown")) sil_achievements_on_depth(10);
    else if (!strcmp(argv[1], "limit-earn")) {
        metar.id = 17; sil_achievements_on_depth(10);
        assert(earned(SIL_ACH_INTO_THE_DEEP) && sil_achievements_saved());
    } else if (!strcmp(argv[1], "limit-reload")) {
        metar.id = 17;
        assert(earned(SIL_ACH_INTO_THE_DEEP) && sil_achievements_saved());
        player.is_dead = true; player.playing = false; player.pcharacter = 2;
        sil_achievements_on_run_completed(); sil_achievements_on_tale_completed();
        assert(earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(argv[1], "bounded-write")) {
        assert(sil_achievements_saved() && !sil_achievements_mask(0));
        preserve_invalid_file();
    } else if (!strcmp(argv[1], "limit-read")) {
        assert(earned(SIL_ACH_INTO_THE_DEEP) && sil_achievements_saved());
    } else if (!strcmp(argv[1], "discarded-reload")) {
        metar.id = 0; player.is_dead = true; player.playing = false;
        player.pcharacter = 2;
        sil_achievements_on_run_completed(); sil_achievements_on_tale_completed();
        assert(!earned(SIL_ACH_THE_LONG_TALE));
    }
    else if (!strcmp(argv[1], "new-events") || !strncmp(argv[1], "door-", 5)
        || !strcmp(argv[1], "fall-arrival")) expanded_conditions(argv[1]);
    else if (!strcmp(argv[1], "comeback")) comeback();
    else if (!strcmp(argv[1], "craft-combat")) craft_and_combat();
    else if (!strncmp(argv[1], "tale-", 5)) tale(argv[1]);
    else if (!strcmp(argv[1], "all-import")) {
        remote = sil_achievement_word_mask(0); remote_upper = UINT64_MAX; sil_achievements_update();
        assert(sil_achievements_mask(0) == sil_achievement_word_mask(0) && !notifications);
        assert(sil_achievements_mask(1) == sil_achievement_word_mask(1));
        assert(sil_achievements_mask(SIL_ACH_WORD_COUNT) == 0);
    }
    else if (!strcmp(argv[1], "all-reload")) {
        assert(sil_achievements_mask(0) == sil_achievement_word_mask(0));
        assert(sil_achievements_mask(1) == sil_achievement_word_mask(1));
        sil_achievements_update(); assert(submitted_upper == sil_achievement_word_mask(1));
    }
    else escape_challenges(argv[1]);
    SDL_Quit(); puts("Achievement production rules/persistence case: PASS"); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        str(BUILD / "_deps/SDL"), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0", "-Wall", "-Wextra",
        *(["-DSIL_STEAMWORKS"] if os.environ.get("ACHIEVEMENTS_STEAM") == "1" else []),
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/achievements.c"), str(ROOT / "src/cJSON.c"),
        "-L", str(BUILD / "_deps/SDL"), "-lSDL3", "-lm",
        "-Wl,--wrap=SDL_FlushIO,--wrap=SDL_RenamePath,--wrap=SDL_GetTicks",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)

    def run(case, profile):
        subprocess.run([str(exe), case, str(profile)], env=env, check=True, timeout=15)

    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as directory:
        base = Path(directory)
        profile = base / "normal"
        run("earn", profile)
        run("reload", profile)
        payload = json.loads((profile / "achievements.json").read_text())
        assert payload["version"] == 1 and len(payload["unlocked"]) == 4
        for case in ("failed-write", "import"):
            run(case, base / case)
        for case in ("triple", "pacifist", "killer", "legacy-run", "oath", "song", "gates", "taint", "comeback", "craft-combat", "tale-repeat", "tale-blitz", "tale-discard", "new-events", "door-history", "door-level", "fall-arrival"):
            run(case, base / case)
        run("tale-first", base / "long-tale")
        run("tale-second", base / "long-tale")
        run("all-import", base / "all")
        run("all-reload", base / "all")
        for case, data in (
            ("corrupt", "{incomplete"),
            ("future", '{"version":2,"unlocked":{}}'),
            ("oversized", " " * 65537),
        ):
            profile = base / case
            profile.mkdir()
            (profile / "achievements.json").write_text(data)
            run("invalid", profile)
        profile = base / "unknown"
        profile.mkdir()
        (profile / "achievements.json").write_text(json.dumps({
            "version": 1, "extra": "preserved", "unlocked": {"FUTURE_ACH": 123}}))
        run("unknown", profile)
        payload = json.loads((profile / "achievements.json").read_text())
        assert payload["extra"] == "preserved" and payload["unlocked"]["FUTURE_ACH"] == 123
        profile = base / "retired"
        profile.mkdir()
        (profile / "achievements.json").write_text(json.dumps({
            "version": 1, "unlocked": {"INTO_THE_DEEP": 123,
                "RELEASE_FROM_BONDAGE": 456, "A_WILL_OF_YOUR_OWN": 789,
                "FUTURE_ACH": 321}}))
        run("retired", profile)
        payload = json.loads((profile / "achievements.json").read_text())
        assert payload["unlocked"] == {"INTO_THE_DEEP": 123, "FUTURE_ACH": 321,
            "RELEASE_FROM_BONDAGE": 456, "A_WILL_OF_YOUR_OWN": 789}
        # Recover profiles written above the former 64 KiB cap. Bound optional
        # history while retaining the current Tale's exact character set.
        profile = base / "history-pressure"
        profile.mkdir()
        tales = {str(i): {"0:1": True} for i in range(3000)}
        (profile / "achievements.json").write_text(json.dumps({
            "version": 1, "unlocked": {}, "tales": tales}))
        assert (profile / "achievements.json").stat().st_size > 65536
        run("limit-earn", profile)
        payload = json.loads((profile / "achievements.json").read_text())
        assert len(json.dumps(payload["tales"], separators=(",", ":"))) <= 48 * 1024
        assert payload["tales"]["17"] == {"0:1": True}
        assert "0" not in payload["tales"]
        discarded_profile = base / "discarded-history"
        shutil.copytree(profile, discarded_profile)
        run("discarded-reload", discarded_profile)
        run("limit-reload", profile)
        profile = base / "near-limit"
        profile.mkdir()
        extension = "x" * 65480
        (profile / "achievements.json").write_text(json.dumps({
            "version": 1, "unlocked": {}, "extra": extension}))
        run("unknown", profile)
        assert (profile / "achievements.json").stat().st_size > 65536
        run("limit-read", profile)
        payload = json.loads((profile / "achievements.json").read_text())
        assert payload["extra"] == extension and payload["unlocked"]["INTO_THE_DEEP"] > 0
        # Refuse an oversized replacement when unknown extension data leaves
        # no optional history to trim; the previous profile stays readable.
        profile = base / "bounded-write"
        profile.mkdir()
        payload = json.dumps({"version": 1, "unlocked": {}, "extra": ""},
                             separators=(",", ":"))
        payload = payload.replace('"extra":""', '"extra":"' +
                                  "x" * (4 * 1024 * 1024 - 10 - len(payload)) + '"')
        (profile / "achievements.json").write_text(payload)
        run("bounded-write", profile)
        run("bounded-write", profile)
    print("Achievements: eligibility, exact-once notifications, profile reload, atomic failures, retry, remote merge and schema preservation PASS.")


if __name__ == "__main__":
    main()
