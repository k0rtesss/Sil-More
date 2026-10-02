#!/usr/bin/env python3
"""Exercise new-hero monster reset and saved Morgoth state with real engine code.

Build first. The between-games reset runs normally except for its introduction
screen; all templates, saves and Tale memories use temporary directories.
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
OUT = ROOT / "scripts/output/birth-monster-reset"

WRITER = r'''
#include "fs/save.c"
#include <assert.h>
size_t fixture_write_full(byte* buffer, size_t capacity)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    bool ok = wr_savefile();
    size_t size = (size_t)SDL_TellIO(fff);
    assert(SDL_CloseIO(fff)); fff = NULL; assert(ok);
    return size;
}
'''

TESTS = r'''
#include "birth/birth.h"
#include "meta_state.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "fs/io_sdl.h"
#include "fs/path.h"

size_t fixture_write_full(byte*, size_t);
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #t); exit(1); } } while (0)

static void check_morgoth_base(void)
{
    const monster_race* actual = &r_info[R_IDX_MORGOTH];
    const monster_race* base = &r_base[R_IDX_MORGOTH];
    CHECK(p_ptr->morgoth_state == 0);
    CHECK(actual->blow[0].att == base->blow[0].att && actual->blow[0].att == 20);
    CHECK(actual->evn == base->evn && actual->evn == 20);
    CHECK(actual->pd == base->pd && actual->pd == 5);
    CHECK(actual->ps == base->ps && actual->wil == base->wil);
    CHECK(actual->per == base->per && actual->light == base->light);
    CHECK(actual->blow[0].dd == base->blow[0].dd);
    CHECK(actual->blow[0].ds == base->blow[0].ds);
}

static void check_new_hero(void)
{
    p_ptr->morgoth_state = 0; anger_morgoth(5);
    CHECK(r_info[R_IDX_MORGOTH].blow[0].att == 60);
    CHECK(r_info[R_IDX_MORGOTH].evn == 60 && r_info[R_IDX_MORGOTH].pd == 12);
    re_init_some_things();
    l_list[41].tkills = 11;
    l_list[41].psights = 7; l_list[41].pkills = 4;
    bool old_option = op_ptr->opt[OPT_pacifist_attack_warning];
    op_ptr->opt[OPT_pacifist_attack_warning] = !old_option;
    player_wipe();
    check_morgoth_base();
    CHECK(l_list[41].tkills == 11);
    CHECK(!l_list[41].psights && !l_list[41].pkills);
    CHECK(op_ptr->opt[OPT_pacifist_attack_warning] == !old_option);
    CHECK(r_info[R_IDX_MORGOTH].cur_num == 0 && r_info[R_IDX_MORGOTH].max_num == 1);
    CHECK(r_info[41].cur_num == 0 && r_info[41].max_num == 100);
    op_ptr->opt[OPT_pacifist_attack_warning] = old_option;
    puts("New hero: actual anger5 -> between-games reset -> player_wipe restores canonical Morgoth, population caps, retained lifetime lore and unchanged options PASS.");
}

static void check_tale_overlay(void)
{
    const int race = 41;
    int base_dice = r_base[race].hdice;
    CHECK(!(r_base[race].flags1 & (RF1_UNIQUE | RF1_QUESTOR)));
    meta_monster_death_event event = {0};
    event.r_idx = race; event.monster_guid = score_guid_from_u64(r_base[race].guid);
    event.character_guid = c_info[1].guid; event.depth = 3; event.turn = 100;
    SDL_strlcpy(event.character_name, "Previous hero", sizeof(event.character_name));
    op_ptr->opt[OPT_meta_revenge] = true;
    CHECK(meta_monster_record_player_death(&event));
    CHECK(meta_monster_apply_runtime_overrides());
    CHECK(r_info[race].flags1 & RF1_UNIQUE);
    CHECK(r_info[race].hdice > base_dice);
    player_wipe();
    CHECK(op_ptr->opt[OPT_meta_revenge]);
    CHECK(r_info[race].hdice == base_dice && !(r_info[race].flags1 & RF1_UNIQUE));
    const meta_monster_record* record = meta_monster_find_record_for_race(race);
    CHECK(record && record->rank == 1 && record->kill_memory_count == 1);
    CHECK(meta_monster_apply_runtime_overrides());
    CHECK(r_info[race].flags1 & RF1_UNIQUE);
    CHECK(r_info[race].hdice > base_dice && r_info[race].max_num == 1);
    meta_monster_invalidate_runtime_overrides();
    int scaled = r_info[race].hdice;
    CHECK(meta_monster_apply_runtime_overrides() && r_info[race].hdice == scaled);
    op_ptr->opt[OPT_meta_revenge] = false; meta_state_reset_character();
    puts("Tale revenge: remembered record survives new hero, authored baseline restored, overlay reapplies without compounding PASS.");
}

static void check_saved_hero(void)
{
    static byte buffer[2000000];
    player_wipe(); reset_map(5);
    p_ptr->is_dead = false; turn = playerturn = 123;
    Rand_state_init(12345);
    SDL_strlcpy(op_ptr->full_name, "Saved hero", sizeof(op_ptr->full_name));
    anger_morgoth(5);
    size_t size = fixture_write_full(buffer, sizeof(buffer));
    CHECK(path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, "saved-hero"));
    SDL_IOStream* file = SDL_IOFromFile(savefile, "wb"); CHECK(file);
    CHECK(SDL_WriteIO(file, buffer, size) == size && SDL_CloseIO(file));
    player_wipe(); check_morgoth_base();
    character_loaded = character_loaded_dead = false;
    CHECK(load_player() && character_loaded);
    CHECK(p_ptr->morgoth_state == 5);
    CHECK(r_info[R_IDX_MORGOTH].blow[0].att == 60);
    CHECK(r_info[R_IDX_MORGOTH].evn == 60 && r_info[R_IDX_MORGOTH].pd == 12);
    CHECK(r_info[R_IDX_MORGOTH].ps == 5 && r_info[R_IDX_MORGOTH].wil == 50);
    CHECK(r_info[R_IDX_MORGOTH].per == 40);
    player_wipe(); check_morgoth_base();
    puts("Saved hero: complete save/load restores anger5 from canonical base; following new hero restores canonical stats again PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += "\n" + TESTS + "\n" + init + r'''
    char install[1024], meta[1024], tales[1024], saves[1024];
    CHECK(path_build(install, sizeof(install), argv[2], "install-apex"));
    CHECK(path_build(meta, sizeof(meta), argv[2], "meta"));
    CHECK(path_build(tales, sizeof(tales), meta, "metaruns"));
    CHECK(path_build(saves, sizeof(saves), argv[2], "save"));
    CHECK(SDL_CreateDirectory(install)); CHECK(SDL_CreateDirectory(tales));
    CHECK(SDL_CreateDirectory(saves));
    ANGBAND_DIR_APEX = install; ANGBAND_DIR_METARUN = tales;
    ANGBAND_DIR_SAVE = saves;
    r_base = mem_alloc_array(z_info->r_max, monster_race); CHECK(r_base);
    memcpy(r_base, r_info, z_info->r_max * sizeof(*r_base));
    metar.id = 4242;
    check_new_hero(); check_tale_overlay(); check_saved_hero();
    printf("Birth monster reset: %d checks PASS.\n", checks);
    SDL_Quit(); return 0;
}
'''
    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER),
                          ("birth.c", '#include "birth/birth-setup.c"\n'),
                          ("arrays.c", '#define display_introduction fixture_skip_intro\n'
                           '#include "init/init-arrays.c"\nvoid fixture_skip_intro(void) {}\n')):
        path = OUT / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj",
                "/src/birth/birth-setup.c.obj", "/src/init/init-arrays.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)))
    flags = (cmake / "flags.make").read_text()
    defines = shlex.split(re.search(r"^C_DEFINES = (.*)$", flags, re.M).group(1))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", *defines, "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                                env=env, capture_output=True, text=True, timeout=120)
        (OUT / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr[-6000:])
            result.check_returncode()


if __name__ == "__main__":
    main()
