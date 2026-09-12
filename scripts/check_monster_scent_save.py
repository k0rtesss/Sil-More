#!/usr/bin/env python3
"""Roundtrip the real dungeon and monster readers/writers through SDL memory IO.

Requires a current Windows build. Uses the initialized full-engine fixture from
check_new_monsters.py and temporary raw/config directories only. Corrupt-tail
tests intentionally exercise reader failures; no player save is opened.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS as ENGINE_FIXTURE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/monster-scent-save"


def fixture_function(name):
    match = re.search(r"^static [^\n]+\b" + name + r"\([^\n]*\)\n\{", ENGINE_FIXTURE, re.M)
    assert match, name
    return ENGINE_FIXTURE[match.start():ENGINE_FIXTURE.index("\n}", match.end()) + 2]


WRITER = r'''
#include "fs/save.c"
#include <assert.h>
size_t fixture_write_dungeon(byte* buffer, size_t capacity, size_t* dungeon_size)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    save_write_dungeon();
    *dungeon_size = (size_t)SDL_TellIO(fff);
    save_wr_u32b(0xA1B2C3D4U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error && length == *dungeon_size + 4);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
size_t fixture_write_monster(const monster_type* monster, byte* buffer,
    size_t capacity)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    save_wr_monster(monster);
    size_t record_size = (size_t)SDL_TellIO(fff);
    save_wr_u32b(0xB4C3D2E1U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error && length == record_size + 4);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''

READER = r'''
#include "fs/load.c"
#include <assert.h>
int fixture_read_dungeon(const byte* buffer, size_t length, int extra,
    u32b* sentinel, size_t* consumed)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    savefile_has_runtime_overrides = savefile_has_monster_shatter = true;
    savefile_has_song_duels = savefile_has_thrall_quest = true;
    savefile_has_thrall_quest_requested = savefile_has_cave_info_hi = true;
    savefile_has_cave_rewired = savefile_has_cave_natural = true;
    savefile_has_item_bonuses = true;
    objects_count_prefetch = 0xFFFF; color_rle_pair_prefetched = false;
    int result = load_read_dungeon();
    *sentinel = 0;
    if (!result) load_rd_u32b(sentinel);
    *consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
int fixture_read_monster(const byte* buffer, size_t length, int extra,
    monster_type* monster, u32b* sentinel, size_t* consumed)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    savefile_has_runtime_overrides = savefile_has_monster_shatter = true;
    savefile_has_song_duels = savefile_has_thrall_quest = true;
    savefile_has_thrall_quest_requested = savefile_has_cave_info_hi = true;
    savefile_has_cave_rewired = savefile_has_cave_natural = true;
    savefile_has_item_bonuses = true;
    load_rd_monster(monster);
    *sentinel = 0;
    load_rd_u32b(sentinel);
    *consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return 0;
}
'''

TESTS = r'''
size_t fixture_write_dungeon(byte*, size_t, size_t*);
int fixture_read_dungeon(const byte*, size_t, int, u32b*, size_t*);
size_t fixture_write_monster(const monster_type*, byte*, size_t);
int fixture_read_monster(const byte*, size_t, int, monster_type*, u32b*, size_t*);
static byte encoded[65536], plain[65536], modified[65536];
static byte expected[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

static void decode(const byte* source, byte* target, size_t size)
{
    byte previous = 0;
    for (size_t i = 0; i < size; ++i)
    { byte current = source[i]; target[i] = current ^ previous; previous = current; }
}
static void encode(const byte* source, byte* target, size_t size)
{
    byte previous = 0;
    for (size_t i = 0; i < size; ++i)
    { previous ^= source[i]; target[i] = previous; }
}
static void fresh_map(void)
{
    character_dungeon = false;
    memset(p_ptr, 0, sizeof(*p_ptr));
    reset_map(10);
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    p_ptr->py = 5; p_ptr->px = 5;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    playerturn = 5432;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            bool border = !y || !x || y == p_ptr->cur_map_hgt-1
                || x == p_ptr->cur_map_wid-1;
            cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
            cave_info[y][x] = border ? CAVE_WALL : 0;
            cave_when[y][x] = cave_rewired[y][x] = cave_natural[y][x] = 0;
            cave_color[y][x] = 0;
        }
    cave_fixtures_clear();
    for (int i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
    { flow_center_y[i] = flow_center_x[i] = 0; wandering_pause[i] = 0; }
    scent_when = 100;
}

static void test_current_roundtrip(void)
{
    fresh_map();
    assert(place_monster_one(8, 8, 41, false, false, NULL));
    monster_type* m = &mon_list[cave_m_idx[8][8]];
    m->alertness = ALERTNESS_ALERT;
    m->poisoned = 27; m->vengeance = 1; m->smite_recovery = 1;
    m->consecutive_attacks = 2;
    m->ai.observations[MON_AI_FIRE].value = 2;
    m->ai.observations[MON_AI_FIRE].ttl = 40;
    m->ai.observations[MON_AI_FIRE].turn = playerturn - 7;
    m->ai.observations[MON_AI_FLANKING].value = 3;
    m->ai.observations[MON_AI_FLANKING].ttl = 20;
    m->ai.observations[MON_AI_FLANKING].turn = playerturn - 2;
    m->ai.observations[MON_AI_IMPALE].value = 3;
    m->ai.observations[MON_AI_IMPALE].ttl = 20;
    m->ai.observations[MON_AI_IMPALE].turn = playerturn - 3;
    m->ai.observations[MON_AI_WHIRLWIND].value = -2;
    m->ai.observations[MON_AI_WHIRLWIND].ttl = 20;
    m->ai.observations[MON_AI_WHIRLWIND].turn = playerturn - 4;
    m->ai.observations[MON_AI_FOLLOW_THROUGH].value = 1;
    m->ai.observations[MON_AI_FOLLOW_THROUGH].ttl = 20;
    m->ai.observations[MON_AI_FOLLOW_THROUGH].turn = playerturn - 5;
    m->ai.observations[MON_AI_SLAY_FEAR].value = -3;
    m->ai.observations[MON_AI_SLAY_FEAR].ttl = 20;
    m->ai.observations[MON_AI_SLAY_FEAR].turn = playerturn - 6;
    /* This slot held the old catch-all multi-target memory. */
    m->ai.observations[MON_AI_MULTI_TARGET].value = 3;
    m->ai.observations[MON_AI_MULTI_TARGET].ttl = 20;
    m->ai.observations[MON_AI_MULTI_TARGET].turn = playerturn - 1;
    m->ai.sense.kind = MON_SENSE_SCENT;
    m->ai.sense.y = m->ai.sense.anchor_y = 8;
    m->ai.sense.x = m->ai.sense.anchor_x = 8;
    m->ai.sense.scent_age = 20;
    m->ai.sense.stale_decisions = 5; m->ai.sense.search_decisions = 3;
    m->ai.sense.recent_count = 4; m->ai.sense.recent_next = 2;
    for (int n = 0; n < 4; n++)
    { m->ai.sense.recent_y[n] = 8; m->ai.sense.recent_x[n] = 4+n; }
    m->ai.sense.observed_turn = playerturn;
    m->ai.cast_reserve = 2; m->ai.goal_y = 8; m->ai.goal_x = 10;
    m->ai.goal_age = 3; m->ai.waits = 2;
    m->ai.previous_y = 8; m->ai.previous_x = 7;
    m->ai.player_y = 5; m->ai.player_x = 5; m->ai.player_action = 6;
    m->ai.player_action_turn = playerturn - 1;
    m->ai.attack_y = 5; m->ai.attack_x = 5; m->ai.attack_chain = 2;
    m->ai.attack_turn = playerturn - 2;
    m->ai.cast_checked = m->ai.cast_available = true;
    monster_ai_state expected_ai = m->ai;
    memset(&expected_ai.observations[MON_AI_MULTI_TARGET], 0,
        sizeof(expected_ai.observations[MON_AI_MULTI_TARGET]));
    expected_ai.cast_checked = expected_ai.cast_available = false;

    cave_feat[9][8] = FEAT_ICE; cave_feat[9][9] = FEAT_POISON;
    cave_feat[9][10] = FEAT_LAVA; cave_feat[9][11] = FEAT_WATER;
    for (int y = 1; y < p_ptr->cur_map_hgt-1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid-1; x++)
            cave_when[y][x] = scent_when + ((y * 7 + x * 3) % 81);
    cave_when[7][7] = 0;
    cave_when[8][8] = scent_when + 20;
    cave_when[9][8] = scent_when;
    cave_when[9][9] = scent_when + 40;
    cave_when[9][10] = scent_when + 80;
    cave_when[9][11] = scent_when + 4; /* water normalizes to absent */
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            expected[y][x] = scent_export_cell(y, x);
    size_t dungeon_size, length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
    decode(encoded, plain, length);
    size_t scent_start = dungeon_size - 2 - 20 * 24;
    assert(plain[scent_start] == 0xE6 && plain[scent_start+1] == 0x5C);

    fresh_map(); scent_when = 17; cave_when[7][7] = 18;
    u32b sentinel; size_t consumed;
    assert(fixture_read_dungeon(encoded, length, 7, &sentinel, &consumed) == 0);
    assert(sentinel == 0xA1B2C3D4U && consumed == length);
    assert(p_ptr->cur_map_hgt == 20 && p_ptr->cur_map_wid == 24);
    for (int y = 0; y < 20; y++) for (int x = 0; x < 24; x++)
        assert(scent_export_cell(y, x) == expected[y][x]);
    assert(get_scent(9, 8) == 0 && get_scent(9, 9) == 40 && get_scent(9, 10) == 80);
    assert(get_scent(9, 11) == -1 && get_scent(7, 7) == -1);
    m = &mon_list[cave_m_idx[8][8]];
    assert(m->r_idx == 41 && mon_max == 2);
    assert(m->poisoned == 27 && m->vengeance == 1 && m->smite_recovery == 1);
    assert(m->skip_next_turn && m->consecutive_attacks == 2);
    assert(memcmp(&m->ai, &expected_ai, sizeof(expected_ai)) == 0);
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 2);
    assert(monster_ai_confidence(m, MON_AI_FLANKING) == 3);
    assert(monster_ai_confidence(m, MON_AI_IMPALE) == 3);
    assert(monster_ai_confidence(m, MON_AI_WHIRLWIND) == -2);
    assert(monster_ai_confidence(m, MON_AI_FOLLOW_THROUGH) == 1);
    assert(monster_ai_confidence(m, MON_AI_SLAY_FEAR) == -3);
    assert(!monster_ai_confidence(m, MON_AI_MULTI_TARGET));
    puts("Real .7 dungeon roundtrip: normalized ages, ice/poison/lava/water, split monster AI/history/recovery, following sentinel PASS.");

    /* Keep genuine bytes up to each tested truncation boundary. */
    const size_t truncations[] = { scent_start, scent_start+1,
        scent_start+2, scent_start+2+20*12, dungeon_size-1 };
    for (size_t n = 0; n < sizeof(truncations)/sizeof(truncations[0]); n++)
    {
        fresh_map();
        assert(fixture_read_dungeon(encoded, truncations[n], 7, &sentinel, &consumed) != 0);
    }
    /* Re-encode the changed decoded byte, preserving the rest of the stream. */
    plain[scent_start] = 0;
    encode(plain, modified, length); fresh_map();
    assert(fixture_read_dungeon(modified, length, 7, &sentinel, &consumed) != 0);
    plain[scent_start] = 0xE6;
    byte original_age = plain[scent_start + 2 + 8*24 + 8];
    plain[scent_start + 2 + 8*24 + 8] = 82;
    encode(plain, modified, length); fresh_map();
    assert(fixture_read_dungeon(modified, length, 7, &sentinel, &consumed) != 0);
    plain[scent_start + 2 + 8*24 + 8] = original_age;
    puts("Real .7 reader: five scent header/grid truncations, malformed magic and out-of-range age rejected PASS.");
}

static void test_monster_record_legacy_compatibility(void)
{
    monster_type source = {0}, restored;
    source.r_idx = 41; source.image_r_idx = 7;
    source.fy = 8; source.fx = 8; source.hp = source.maxhp = 120;
    source.alertness = ALERTNESS_ALERT; source.energy = -40;
    source.morale = 17; source.consecutive_attacks = 2;
    source.ai.observations[MON_AI_FIRE].value = 2;
    source.ai.observations[MON_AI_FIRE].ttl = 40;
    source.ai.observations[MON_AI_FIRE].turn = playerturn - 2;
    source.ai.observations[MON_AI_IMPALE].value = 3;
    source.ai.observations[MON_AI_IMPALE].ttl = 20;
    source.ai.observations[MON_AI_IMPALE].turn = playerturn - 2;
    source.ai.observations[MON_AI_WHIRLWIND].value = -2;
    source.ai.observations[MON_AI_WHIRLWIND].ttl = 20;
    source.ai.observations[MON_AI_WHIRLWIND].turn = playerturn - 3;
    source.ai.observations[MON_AI_FOLLOW_THROUGH].value = 1;
    source.ai.observations[MON_AI_FOLLOW_THROUGH].ttl = 20;
    source.ai.observations[MON_AI_FOLLOW_THROUGH].turn = playerturn - 4;
    source.ai.observations[MON_AI_SLAY_FEAR].value = -3;
    source.ai.observations[MON_AI_SLAY_FEAR].ttl = 20;
    source.ai.observations[MON_AI_SLAY_FEAR].turn = playerturn - 5;
    source.ai.sense.kind = MON_SENSE_SHARED_TRACE;
    source.ai.sense.y = 8; source.ai.sense.x = 8;
    source.ai.sense.anchor_y = 7; source.ai.sense.anchor_x = 8;
    source.ai.sense.scent_age = 11;
    source.ai.sense.stale_decisions = 2; source.ai.sense.search_decisions = 3;
    source.ai.sense.recent_count = 2; source.ai.sense.recent_next = 1;
    source.ai.sense.recent_y[0] = 8; source.ai.sense.recent_x[0] = 7;
    source.ai.sense.recent_y[1] = 7; source.ai.sense.recent_x[1] = 8;
    source.ai.sense.observed_turn = playerturn - 1;
    source.ai.cast_reserve = 2;
    source.ai.goal_y = 6; source.ai.goal_x = 8; source.ai.goal_age = 3;
    source.ai.previous_y = 8; source.ai.previous_x = 9; source.ai.waits = 1;
    source.ai.player_y = 5; source.ai.player_x = 5; source.ai.player_action = 6;
    source.ai.player_action_turn = playerturn - 1;
    source.ai.attack_y = 5; source.ai.attack_x = 5; source.ai.attack_chain = 2;
    source.ai.attack_turn = playerturn - 2;

    size_t modern_length = fixture_write_monster(&source, encoded, sizeof(encoded));
    u32b sentinel; size_t consumed;
    memset(&restored, 0x55, sizeof(restored));
    assert(fixture_read_monster(encoded, modern_length, 7, &restored,
        &sentinel, &consumed) == 0);
    assert(sentinel == 0xB4C3D2E1U && consumed == modern_length);
    assert(restored.ai.observations[MON_AI_FIRE].value == 2);
    assert(restored.ai.observations[MON_AI_IMPALE].value == 3);
    assert(restored.ai.observations[MON_AI_WHIRLWIND].value == -2);
    assert(restored.ai.observations[MON_AI_FOLLOW_THROUGH].value == 1);
    assert(restored.ai.observations[MON_AI_SLAY_FEAR].value == -3);
    assert(!restored.ai.observations[MON_AI_MULTI_TARGET].value);
    assert(restored.ai.sense.kind == MON_SENSE_SHARED_TRACE);
    assert(restored.ai.sense.observed_turn == playerturn - 1);
    assert(restored.ai.player_action == 6
        && restored.ai.player_action_turn == playerturn - 1);
    assert(restored.ai.attack_chain == 2
        && restored.ai.attack_turn == playerturn - 2);

    /* Remove the four appended .7 observations and make the retired .6 slot
     * nonzero, as a historical save could have written its catch-all value. */
    decode(encoded, plain, modern_length);
    size_t record_size = modern_length - 4;
    size_t ai_start = record_size - MON_AI_FEATURE_COUNT * 7 - (22 + 13 + 8);
    size_t appended = (MON_AI_FEATURE_COUNT - MON_AI_IMPALE) * 7;
    size_t removed_start = ai_start + MON_AI_IMPALE * 7;
    memmove(plain + removed_start, plain + removed_start + appended,
        modern_length - removed_start);
    size_t old_slot = ai_start + MON_AI_MULTI_TARGET * 7;
    plain[old_slot] = 3; plain[old_slot + 1] = 0; plain[old_slot + 2] = 20;
    size_t legacy_length = modern_length - appended;
    encode(plain, modified, legacy_length);
    memset(&restored, 0x55, sizeof(restored));
    assert(fixture_read_monster(modified, legacy_length, 6, &restored,
        &sentinel, &consumed) == 0);
    assert(sentinel == 0xB4C3D2E1U && consumed == legacy_length);
    assert(restored.ai.observations[MON_AI_FIRE].value == 2);
    assert(!restored.ai.observations[MON_AI_MULTI_TARGET].value
        && !restored.ai.observations[MON_AI_MULTI_TARGET].ttl
        && !restored.ai.observations[MON_AI_MULTI_TARGET].turn);
    for (int f = MON_AI_IMPALE; f < MON_AI_FEATURE_COUNT; ++f)
        assert(!restored.ai.observations[f].value
            && !restored.ai.observations[f].ttl
            && !restored.ai.observations[f].turn);
    assert(restored.ai.sense.kind == MON_SENSE_SHARED_TRACE);
    assert(restored.ai.sense.observed_turn == playerturn - 1);
    assert(restored.ai.player_action == 6
        && restored.ai.player_action_turn == playerturn - 1);
    assert(restored.ai.attack_chain == 2
        && restored.ai.attack_turn == playerturn - 2);
    puts("Synthetic .6 monster record: exactly 29 observations, retired multi-target cleared, split features absent, sense/history and following sentinel preserved PASS.");
}

static void test_legacy_absence(void)
{
    /* No monster records: .5 and .6 dungeon lanes differ only in the scent
     * suffix. Legacy monster records are separately checked by the poison suite. */
    fresh_map();
    size_t dungeon_size, length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
    decode(encoded, plain, length);
    size_t old_dungeon_size = dungeon_size - 2 - 20*24;
    memmove(plain + old_dungeon_size, plain + dungeon_size, 4);
    length = old_dungeon_size + 4;
    encode(plain, modified, length);
    fresh_map(); cave_when[7][7] = scent_when;
    u32b sentinel; size_t consumed;
    assert(fixture_read_dungeon(modified, length, 5, &sentinel, &consumed) == 0);
    assert(sentinel == 0xA1B2C3D4U && consumed == length && mon_max == 1);
    for (int y = 0; y < 20; y++) for (int x = 0; x < 24; x++)
        assert(get_scent(y, x) == -1);
    puts("Real .5 dungeon: absent scent suffix initializes empty trail and preserves following sentinel PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += '\n#include "monster/monster-ai.h"\n#include "monster/monster-senses.h"\n'
    prefix += '#include "cave/cave-fixtures.h"\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n"
    harness += fixture_function("reset_map") + "\n" + TESTS + "\n" + init
    harness += '    test_current_roundtrip(); test_monster_record_legacy_compatibility(); test_legacy_absence();\n'
    harness += '    puts("Monster dungeon scent persistence integration: PASS.");\n'
    harness += '    SDL_Quit(); return 0;\n}\n'
    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER), ("reader.c", READER)):
        source = OUT / name
        source.write_text(content, encoding="utf-8")
        sources.append(str(source))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj")
    objects = [p for p in objects if not p.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=90)


if __name__ == "__main__":
    main()
