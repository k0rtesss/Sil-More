#!/usr/bin/env python3
"""Check the tale's connectivity and real template/generation/reading routes.

Build with build-incremental.ps1 first. All generated data stays in a temporary
directory under scripts/output; no player saves or configuration are used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import textwrap

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/unfinished-tale"


def records(name):
    result = {}
    current = None
    previous = -1
    for line in (ROOT / "lib/edit" / name).read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            _, number, title = line.split(":", 2)
            number = int(number)
            assert number > previous, (name, number)
            previous = number
            current = {"name": title, "lines": []}
            result[number] = current
        elif current is not None and len(line) > 1 and line[1] == ":":
            current["lines"].append(line)
    return result


def check_layout():
    vault = records("vault.txt")[520]
    rows = [line[2:] for line in vault["lines"] if line.startswith("D:")]
    assert len({len(row) for row in rows}) == 1
    width, height = len(rows[0]), len(rows)
    assert "X:8:16:25:19" in vault["lines"]
    assert "F:LIGHT" in vault["lines"]
    assert all("TEST" not in line for line in vault["lines"])
    walkable = {(y, x) for y, row in enumerate(rows)
                for x, char in enumerate(row) if char not in "# "}
    entrances = {(y, x) for y, x in walkable if rows[y][x] == "$"}
    seen = set(entrances)
    todo = list(entrances)
    while todo:
        y, x = todo.pop()
        for point in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
            if point in walkable and point not in seen:
                seen.add(point)
                todo.append(point)
    assert seen == walkable, "Disconnected archive room or scroll"
    for token in "E5689=":
        assert sum(row.count(token) for row in rows) == 1, token
    others = records("vault.txt")
    for number, entry in others.items():
        if number != 520:
            assert not any(set(line[2:]) & set("E5689=")
                           for line in entry["lines"] if line.startswith("D:")), number
    objects = records("object.txt")
    for number in range(492, 497):
        entry = objects[number]
        text = "".join(line[2:] for line in entry["lines"] if line.startswith("D:"))
        assert len(textwrap.wrap(text, width=46)) <= 17, "Scroll overflows note reader"
        assert "T:32:25" in entry["lines"]
        assert not any(line.startswith("A:") for line in entry["lines"])
    print(f"Layout: {width} x {height}, connected; five distinct scrolls fit the reader.")


HARNESS = r'''
#include "angband.h"
#include "init/init2-internal.h"
#include "level-generation/level-generation-internal.h"
#include <assert.h>

static term test_term;
static int reading_kind;
static int captured;

static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT && reading_kind)
    {
        char output[8192];
        size_t pos = 0;
        for (int y = 0; y < Term->hgt; y++)
            for (int x = 0; x < Term->wid; x++)
            {
                char ch = Term->scr->c[y][x];
                if (ch != ' ' && ch != '\0' && pos < sizeof(output) - 1)
                    output[pos++] = ch;
            }
        output[pos] = '\0';
        const char* source = k_text + k_info[reading_kind].text;
        char compact[2048];
        size_t n = 0;
        for (; *source && n < sizeof(compact) - 1; source++)
            if (*source != ' ' && *source != '\n') compact[n++] = *source;
        compact[n] = '\0';
        assert(strstr(output, compact));
        assert(strstr(output, "(pressanykey)"));
        captured++;
        Term_keypress(' ');
    }
    return 0;
}

static void reset_map(void)
{
    memset(cave_info, 0, MAX_DUNGEON_HGT * sizeof(*cave_info));
    memset(cave_feat, FEAT_WALL_EXTRA, MAX_DUNGEON_HGT * sizeof(*cave_feat));
    memset(cave_o_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_o_idx));
    memset(cave_m_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_m_idx));
    memset(o_list, 0, z_info->o_max * sizeof(*o_list));
    memset(mon_list, 0, MAX_MONSTERS * sizeof(*mon_list));
    memset(dun, 0, sizeof(*dun));
    o_max = mon_max = 1;
    o_cnt = mon_cnt = 0;
    r_info[402].cur_num = 0;
    p_ptr->depth = 16;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 88;
    p_ptr->py = p_ptr->px = 2;
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    setbuf(stdout, NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = argv[2];
    ANGBAND_DIR_USER = argv[2];
    ANGBAND_DIR_PREF = argv[2];
    assert(term_init(&test_term, 80, 24, 256) == 0);
    test_term.xtra_hook = terminal_extra;
    angband_term[0] = &test_term;
    Term_activate(&test_term);
    assert(init_z_info() == 0);
    assert(init_f_info() == 0);
    assert(init_k_info() == 0);
    assert(init_b_info() == 0);
    assert(init_a_info() == 0);
    assert(init_e_info() == 0);
    assert(init_r_info() == 0);
    assert(init_v_info() == 0);
    assert(init_style_info() == 0);
    assert(init_other() == 0);
    assert(init_alloc() == 0);
    assert(monster_lookup_guid_text("90921d863b6a4eaa") == 402);
    assert((r_info[402].x_attr & 0x3f) == 21);
    assert((r_info[402].x_char & 0x3f) == 15);
    for (int i = 0; i < alloc_race_size; i++)
        assert(alloc_race_table[i].index != 402);
    for (int i = 0; i < alloc_kind_size; i++)
        assert(alloc_kind_table[i].index < 492 || alloc_kind_table[i].index > 496);
    puts("Real template parsers: PASS; dragon and scrolls excluded from random allocation.");
    r_info[402].max_num = 1;
    reset_map();
    p_ptr->depth = 15;
    assert(!vault_type8_is_eligible(520, false));
    p_ptr->depth = 16;
    assert(vault_type8_is_eligible(520, false));
    p_ptr->depth = 19;
    assert(vault_type8_is_eligible(520, false));
    p_ptr->depth = 20;
    assert(!vault_type8_is_eligible(520, false));
    p_ptr->depth = 16;
    p_ptr->greater_vaults[0] = 520;
    assert(!vault_type8_is_eligible(520, false));
    p_ptr->greater_vaults[0] = 0;

    /* Random treasure is independent of this test: avoid generating the full
     * drop database while exercising the real fixed-content placement route. */
    char* layout = v_text + v_info[520].text;
    for (char* p = layout; *p; p++)
        if (strchr("!*&~", *p)) *p = '.';
    unsigned orientations = 0;
    for (int diagonal = 0; diagonal <= 1; diagonal++)
    for (int seed = 1; seed <= 32; seed++)
    {
        reset_map();
        Rand_state_init(seed);
        op_ptr->vault_drop_frequency = VDF_MEAGER;
        assert(build_vault(44, 44, &v_info[520], diagonal));
        int notes = 0, dragons = 0, sy = 0, sx = 0;
        bool kinds[5] = {0};
        for (int i = 1; i < o_max; i++)
        {
            object_type* o = &o_list[i];
            if (o->tval != TV_NOTE) continue;
            assert(o->k_idx >= 492 && o->k_idx <= 496);
            int n = o->k_idx - 492;
            assert(!kinds[n]);
            kinds[n] = true;
            assert(cave_o_idx[o->iy][o->ix] == i);
            assert(cave_feat[o->iy][o->ix] == FEAT_FLOOR);
            notes++;
            if (n == 3) { sy = o->iy; sx = o->ix; }
        }
        for (int i = 1; i < mon_max; i++)
            if (mon_list[i].r_idx == 402)
            {
                dragons++;
                assert(mon_list[i].alertness < ALERTNESS_UNWARY);
            }
        assert(notes == 5 && dragons == 1);
        assert(!place_vault_monster_token('E', 2, 3)); /* unique cap */
        for (int fy = 0; fy <= 1; fy++)
        for (int fx = 0; fx <= 1; fx++)
        {
            /* Scroll 9 is at template (10,25); use scroll 5's y for vertical flip. */
            int first_y = 0, first_x = 0;
            for (int i = 1; i < o_max; i++)
                if (o_list[i].k_idx == 492) { first_y=o_list[i].iy; first_x=o_list[i].ix; }
            int ay = fy ? 20-16 : 16;
            int ax = fx ? 30-25 : 25;
            if ((!diagonal && first_y == 34+ay && sx == 29+ax)
                || (diagonal && first_x == 34+ay && sy == 29+ax))
                orientations |= 1u << (diagonal*4 + fy*2 + fx);
        }
    }
    assert(orientations == 255);
    puts("Real vault generation: 64 placements, all eight orientations, five scrolls at Meager loot, one sleeping unique: PASS.");
    puts("Depth limits (800-950 ft), once-per-run vault and unique cap: PASS.");
    for (int k = 492; k <= 496; k++)
    {
        object_type scroll;
        object_prep(&scroll, k);
        Term_clear();
        reading_kind = k;
        note_info_screen(&scroll);
        reading_kind = 0;
    }
    assert(captured == 5);
    puts("Real note reader: all five complete texts and dismissal prompts fit: PASS.");
    SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    check_layout()
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
