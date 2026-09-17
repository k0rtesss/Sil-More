#!/usr/bin/env python3
"""Check legacy Quiver migration using production loader and inventory code.

Requires a current standard Windows build; does not open player saves.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/legacy-quiver-load"

HARNESS = r'''
#include "fs/load-notes-inventory.c"
#include <assert.h>
static object_type held[INVEN_TOTAL];
static object_kind kinds[2];
static maxima limits;
static object_type arrows(int count)
{
    object_type obj = {0};
    obj.k_idx = 1; obj.tval = TV_ARROW; obj.number = count;
    obj.pickup_slot = INVEN_QUIVER1; obj.ident = IDENT_KNOWN;
    return obj;
}
static void reset(void)
{
    memset(held, 0, sizeof(held));
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    p_ptr->inven_cnt = p_ptr->equip_cnt = 0;
}
static int carried_arrows(void)
{
    int count = 0;
    for (int i = 0; i < player_pack_entry_count(); i++)
    {
        object_type* obj = player_pack_entry_at(i);
        assert(obj && obj->k_idx && obj->number > 0);
        assert(obj->storage == OBJECT_STORAGE_PACK);
        assert(obj->pickup_slot == -1 && !obj->pickup);
        count += obj->number;
    }
    return count;
}
int main(void)
{
    inventory = held; k_info = kinds; z_info = &limits;
    limits.k_max = 2; kinds[1].tval = TV_ARROW; kinds[1].aware = true;
    for (int count = 1; count <= 99; count++)
    {
        reset();
        held[INVEN_QUIVER1] = arrows(count); p_ptr->equip_cnt = 1;
        assert(!migrate_legacy_quiver());
        assert(!held[INVEN_QUIVER1].k_idx && !p_ptr->equip_cnt);
        assert(player_quiver_arrow_count() == MIN(count, QUIVER_ARROW_CAPACITY));
        assert(carried_arrows() + player_quiver_arrow_count() == count);
    }
    reset();
    held[INVEN_QUIVER1] = arrows(30); p_ptr->equip_cnt = 1;
    held[0] = arrows(30); held[1] = arrows(20); p_ptr->inven_cnt = 2;
    assert(!migrate_legacy_quiver());
    assert(player_quiver_arrow_count() == 48);
    assert(carried_arrows() == 32 && p_ptr->inven_cnt == 2);

    reset();
    held[0] = arrows(10); held[1] = arrows(10); p_ptr->inven_cnt = 2;
    assert(!migrate_legacy_quiver());
    assert(player_quiver_arrow_count() == 20);
    assert(carried_arrows() == 0 && p_ptr->inven_cnt == 0);

    reset();
    held[INVEN_QUIVER1] = arrows(99); p_ptr->equip_cnt = 1;
    for (int i = 0; i < INVEN_PACK; i++) held[i] = arrows(1);
    p_ptr->inven_cnt = INVEN_PACK;
    assert(!migrate_legacy_quiver());
    assert(player_quiver_arrow_count() == 48);
    assert(carried_arrows() == 51 + INVEN_PACK);
    assert(p_ptr->inven_cnt == INVEN_PACK + 1);
    assert(player_carried_extra_entry_count() == 1);
    reset();
    puts("Legacy Quiver: 1..99 arrows, mixed old slots, adjacent Pack compaction, and full Pack preserve every arrow PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/load-notes-inventory.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith(excluded)), encoding="utf-8")
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
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()
