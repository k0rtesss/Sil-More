#!/usr/bin/env python3
"""Exercise production vault eligibility and forced quest selection in isolation.

The fixture compiles the exact helper and quest-selection function bodies, with
placement callbacks that record whether an impossible template was attempted.
No game saves, configuration, or deployed data are read or written.
"""

from pathlib import Path
import json
import os
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "src/level-generation"
OUT = ROOT / "scripts/output/vault-depth-eligibility"


def extract_function(path, name):
    source = path.read_text(encoding="utf-8-sig")
    match = re.search(r"^bool\s+" + re.escape(name) + r"\s*\(", source, re.M)
    assert match, f"Missing production function {name}"
    brace = source.index("{", match.start())
    # Ignore comments and quoted literals when counting C braces.
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, brace):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():token.end()]
    raise AssertionError(f"Unclosed production function {name}")


def tomb_payload():
    rows = []
    selected = False
    max_depth = None
    for line in (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            selected = line.startswith("N:400:")
        elif selected and line.startswith("X:"):
            max_depth = int(line.split(":")[4])
        elif selected and line.startswith("D:"):
            rows.append(line[2:])
    assert rows and len(set(map(len, rows))) == 1
    assert max_depth == 14, "Tomb maximum depth must be 700 ft"
    assert any("W" in row for row in rows), "Tomb fixture must exercise barrow wight restriction"
    return rows


STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define MORGOTH_DEPTH 20
#define VLT_QUEST 1
#define QUEST_MAX_INITIATED_PER_RUN 2
#define QUEST_ID_AULE 2
#define QUEST_ID_MANDOS 3
#define METARUN_QUEST_AULE 2
#define METARUN_QUEST_MANDOS 3
#define MANDOS_QUEST_NOT_STARTED 0
#define S_SMT 0
#define FEAT_WALL_OUTER 1
#define FEAT_WALL_INNER 2
#define FEAT_FLOOR 3
#define FEAT_WALL_EXTRA 4
#define CAVE_ICKY 1
#define CAVE_ROOM 2
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define log_trace(...) fake_log(__VA_ARGS__)
#define log_debug(...) fake_log(__VA_ARGS__)
#define genlog_quest(...) fake_log(__VA_ARGS__)
typedef const char *cptr;
static void fake_log(cptr format, ...) { (void)format; }
typedef struct {
    int typ, flags, depth, max_depth, rarity, text, name, hgt, wid;
} vault_type;
static vault_type vaults[2];
static vault_type *v_info = vaults;
static char *v_text;
static char *v_name = "the Tomb of the King";
static struct { int v_max; } limits;
static struct { int depth, mandos_quest, cur_map_hgt, cur_map_wid;
                int skill_base[1], skill_use[1]; } player;
#define p_ptr (&player)
#define z_info (&limits)
static int cave_feat[256][256], cave_m_idx[256][256], cave_info[256][256];
static bool qv_placed_this_level;
static int placement_calls, exhaustive_calls, processed;
static bool placement_succeeds;
static bool quest_can_initiate_more(void) { return true; }
static int quest_initiated_count_this_run(void) { return 0; }
static bool quest_vault_surface_roll_allows(const vault_type *v, int d)
{ (void)v; (void)d; return true; }
static bool vault_template_has_duruin(const vault_type *v) { (void)v; return false; }
static bool vault_template_has_aule(const vault_type *v) { (void)v; return false; }
static bool vault_template_has_mandos(const vault_type *v) { (void)v; return true; }
static bool check_quest_eligibility(int q, int d) { (void)q; (void)d; return true; }
static bool quest_metarun_blocked(int q, int m) { (void)q; (void)m; return false; }
static void level_gen_debug_note_quest_vault_name(cptr n) { (void)n; }
static void level_gen_debug_activate_quest_vault_name(cptr n) { (void)n; }
static int rand_range(int low, int high) { return (low + high) / 2; }
static bool place_room_forced(int y, int x, vault_type *v)
{ (void)y; (void)x; (void)v; ++placement_calls; return placement_succeeds; }
static bool place_room_forced_exhaustive(vault_type *v, int *y, int *x)
{ (void)v; (void)y; (void)x; ++exhaustive_calls; return false; }
static void process_quest_vault_area(int y, int x, vault_type *v)
{ (void)y; (void)x; (void)v; ++processed; }
'''


TESTS = r'''
static void reset_case(int depth)
{
    memset(&player, 0, sizeof(player));
    player.depth = depth;
    player.cur_map_hgt = player.cur_map_wid = 100;
    placement_calls = exhaustive_calls = processed = 0;
    qv_placed_this_level = false;
    placement_succeeds = true;
}

int main(void)
{
    bool eligible;
    v_text = TOMBTEXT;
    limits.v_max = 1;
    vaults[0] = (vault_type){.typ=7, .flags=VLT_QUEST, .depth=8,
        .max_depth=0, .rarity=10, .hgt=TOMBHEIGHT, .wid=TOMBWIDTH};

    assert(vault_is_valid_for_depth(&vaults[0], 13));
    assert(vault_is_valid_for_depth(&vaults[0], 14));
    assert(!vault_is_valid_for_depth(&vaults[0], 15));

    reset_case(14);
    eligible = false;
    assert(try_quest_vault_type(7, &eligible));
    assert(eligible && placement_calls == 1 && exhaustive_calls == 0);
    assert(qv_placed_this_level && processed == 1);

    for (int depth = 15; depth <= 16; depth++) {
        reset_case(depth);
        eligible = true;
        assert(!try_quest_vault_type(7, &eligible));
        assert(!eligible && placement_calls == 0 && exhaustive_calls == 0);
        assert(!qv_placed_this_level && processed == 0);
        /* The caller can omit its eligibility output pointer. */
        assert(!try_quest_vault_type(7, NULL));
    }

    reset_case(14);
    placement_succeeds = false;
    eligible = false;
    assert(!try_quest_vault_type(7, &eligible));
    assert(eligible && placement_calls == 11 && exhaustive_calls == 1);
    assert(!qv_placed_this_level && processed == 0);

    /* A later valid candidate must still be selected after an invalid one. */
    reset_case(15);
    limits.v_max = 2;
    vaults[1] = vaults[0];
    vaults[1].text = VALIDOFFSET;
    vaults[1].hgt = vaults[1].wid = 1;
    eligible = false;
    assert(try_quest_vault_type(7, &eligible));
    assert(eligible && placement_calls == 1 && processed == 1);

    limits.v_max = 1;
    vaults[0].hgt = vaults[0].wid = 1;
    v_text = "7";
    assert(vault_is_valid_for_depth(&vaults[0], 19));
    assert(!vault_is_valid_for_depth(&vaults[0], 20));
    reset_case(20);
    eligible = true;
    assert(!try_quest_vault_type(7, &eligible));
    assert(!eligible && placement_calls == 0 && exhaustive_calls == 0);

    v_text = ".";
    assert(vault_is_valid_for_depth(&vaults[0], 15));
    assert(vault_is_valid_for_depth(&vaults[0], 20));
    puts("Vault depth eligibility: W at 13/14/15, chasm at 19/20, forced quest rejection, valid fallback, and geometry retry: PASS");
    return 0;
}
'''


def main():
    rows = tomb_payload()
    payload = "".join(rows)
    source = STUBS + "\n" + extract_function(
        GEN / "level-generation-rooms-vaults.c", "vault_is_valid_for_depth")
    source += "\n" + extract_function(
        GEN / "level-generation-room-build.c", "try_quest_vault_type")
    tests = TESTS.replace("TOMBTEXT", json.dumps(payload + "."))
    tests = tests.replace("TOMBHEIGHT", str(len(rows))).replace("TOMBWIDTH", str(len(rows[0])))
    source += "\n" + tests.replace("VALIDOFFSET", str(len(payload)))
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(source, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    "-O0", str(fixture), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()
