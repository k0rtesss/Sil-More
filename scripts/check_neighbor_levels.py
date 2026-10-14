#!/usr/bin/env python3
"""Exercise real background generation, retained maps, links and save lanes.

Uses isolated engine state and temporary data; never opens a player save.
Requires a current build-standard Windows build.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/neighbor-levels"

WRITE_LEVELS = r'''
size_t fixture_write_levels(byte* buffer, size_t capacity, size_t* cache_offset)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    save_write_partition_meta();
    save_write_dungeon();
    *cache_offset = (size_t)SDL_TellIO(fff);
    level_cache_write();
    save_wr_u32b(0xA1B2C3D4U);
    save_wr_u32b(0x10203040U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''

READ_LEVELS = r'''
int fixture_read_levels(const byte* buffer, size_t length)
{
    character_dungeon = false;
    mon_max = o_max = 1;
    mon_cnt = o_cnt = num_repro = 0;
    memset(mon_list, 0, MAX_MONSTERS * sizeof(*mon_list));
    memset(o_list, 0, z_info->o_max * sizeof(*o_list));
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = VERSION_MAJOR; sf_minor = VERSION_MINOR;
    sf_patch = VERSION_PATCH; sf_extra = VERSION_EXTRA;
    savefile_has_runtime_overrides = savefile_has_monster_shatter = true;
    savefile_has_song_duels = savefile_has_thrall_quest = true;
    savefile_has_thrall_quest_requested = savefile_has_cave_info_hi = true;
    savefile_has_cave_rewired = savefile_has_cave_natural = true;
    savefile_has_cave_water_flow = savefile_has_cave_flood_trap_kinds = true;
    savefile_has_item_bonuses = true;
    savefile_has_partition_meta = savefile_has_partition_meta_types = true;
    objects_count_prefetch = 0xFFFF; color_rle_pair_prefetched = false;
    int result = load_read_partition_meta();
    if (result) fprintf(stderr, "Current partition read failed at %u\n", load_byte_offset);
    if (!result) {
        result = load_read_dungeon();
        if (result) fprintf(stderr, "Current dungeon read failed at %u\n", load_byte_offset);
    }
    if (!result) {
        result = level_cache_read();
        if (result) fprintf(stderr, "Neighbour cache read failed at %u\n", load_byte_offset);
    }
    if (!result) {
        u32b sentinel = 0, tail = 0;
        load_rd_u32b(&sentinel); load_rd_u32b(&tail);
        if (sentinel != 0xA1B2C3D4U || tail != 0x10203040U
            || (size_t)SDL_TellIO(fff) != length) result = -1;
    }
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
'''

CHECKS = r'''
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
#include "cave/cave-environment.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-flood.h"
#include "meta_state.h"

size_t fixture_write_levels(byte*, size_t, size_t*);
int fixture_read_levels(const byte*, size_t);
void __wrap_load_note(cptr text) { fprintf(stderr, "Loader: %s\n", text); }

#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "FAIL: %s (line %d)\n", #expr, __LINE__); exit(1); } } while (0)

static u64b terrain_hash(void)
{
    u64b hash = 1469598103934665603ULL;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++) {
            hash ^= cave_feat[y][x]; hash *= 1099511628211ULL;
        }
    return hash;
}

static void wait_for(int depth)
{
    Uint64 start = SDL_GetTicks();
    while (!level_cache_ready(depth)) {
        level_cache_poll();
        assert(SDL_GetTicks() - start < 90000);
        SDL_Delay(5);
    }
    printf("Depth %d ready after %llu ms\n", depth,
        (unsigned long long)(SDL_GetTicks() - start));
}

static void travel(int depth, int y, int x)
{
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = y; p_ptr->px = x; cave_m_idx[y][x] = -1;
    p_ptr->depth = depth; p_ptr->leaving = true;
    Uint64 start = SDL_GetTicksNS();
    assert(level_cache_change_level());
    assert(p_ptr->depth == depth);
    assert(p_ptr->py == y && p_ptr->px == x);
    assert(cave_m_idx[y][x] == -1);
    printf("Cached transition to %d: %.3f ms\n", depth,
        (SDL_GetTicksNS() - start) / 1000000.0);
}

static void checks(void)
{
    assert(init_quest_info() == 0);
    assert(init_st_info() == 0);
    assert(init_partition_info() == 0);
    reset_map(10);
    for (int y = 0; y < 88; y++) for (int x = 0; x < 88; x++)
        cave_feat[y][x] = !y || !x || y == 87 || x == 87 ? FEAT_WALL_PERM : FEAT_FLOOR;
    p_ptr->depth = p_ptr->max_depth = 10;
    p_ptr->py = p_ptr->px = 5;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->playing = true;
    playerturn = 100; turn = 1000;
    cave_m_idx[5][5] = -1;
    cave_feat[5][5] = FEAT_MORE;
    cave_feat[6][6] = FEAT_LESS;
    cave_feat[10][10] = FEAT_CHASM;
    cave_feat[10][11] = FEAT_CHASM;
    cave_feat[35][35] = FEAT_RUBBLE;
    cave_feat[40][40] = FEAT_WATER;
    cave_feat[40][41] = FEAT_BRIDGE_WATER_H;
    character_generated = character_dungeon = true;
    Rand_state_init(324928374ULL);
    cave_environment_reset(); cave_environment_seed();
    environment_cell bridge = *cave_environment_cell_at(40, 41);
    bridge.integrity = 40; bridge.work = 3;
    assert(cave_environment_restore_cell(40, 41, bridge));
    op_ptr->opt[OPT_smaller_level_size] = true;
    level_cache_start();
    player_type before_background = *p_ptr;
    u64b random = Rand_state_export(), terrain = terrain_hash();
    SDL_Delay(100);
    assert(Rand_state_export() == random);
    assert(p_ptr->depth == 10 && p_ptr->chp == 100);
    assert(terrain_hash() == terrain);
    wait_for(11); wait_for(9);
    /* A newly available lower view needs a repaint, without a gameplay tick.
     * This is the only permitted player-state change from worker delivery. */
    before_background.redraw |= PR_MAP;
    assert(!memcmp(&before_background, p_ptr, sizeof(*p_ptr)));
    assert(terrain_hash() == terrain);
    assert(p_ptr->depth == 10 && p_ptr->chp == 100);
    assert(level_cache_chasm_fall_depth(10, 10) >= 12);
    int bottom = level_cache_chasm_fall_depth(10, 10);
    assert(bottom == level_cache_chasm_fall_depth(10, 11));
    puts("Background generation preserves active terrain, player and random stream: PASS.");

    int race = 54;
    mon_max = 2; mon_cnt = 1;
    memset(&mon_list[1], 0, sizeof(mon_list[1]));
    mon_list[1].r_idx = race; mon_list[1].fy = mon_list[1].fx = 20;
    mon_list[1].hp = 7; mon_list[1].maxhp = 10;
    cave_m_idx[20][20] = 1; r_info[race].cur_num++;
    int arrow_kind = 0;
    for (int i = 1; i < z_info->k_max; i++) if (k_info[i].tval == TV_ARROW) { arrow_kind = i; break; }
    assert(arrow_kind);
    o_max = 2; o_cnt = 1;
    object_prep(&o_list[1], arrow_kind);
    o_list[1].iy = o_list[1].ix = 21; o_list[1].number = 5;
    cave_o_idx[21][21] = 1;
    p_ptr->chp = 73;
    environment_state ecology = cave_environment_get_state();
    travel(11, 5, 5);
    assert(cave_feat[5][5] == FEAT_LESS);
    assert(cave_feat[10][10] == FEAT_CHASM);
    assert(level_cache_chasm_above(10, 10));
    assert(level_cache_chasm_fall_depth(10, 10) == bottom);
    assert(!level_cache_ready(9));
    cave_feat[30][30] = FEAT_RUBBLE;
    u64b eleven = terrain_hash();
    turn += 120;
    travel(10, 5, 5);
    assert(p_ptr->chp == 73);
    assert(terrain_hash() == terrain);
    assert(mon_list[1].r_idx == race && mon_list[1].hp == 7);
    assert(cave_m_idx[20][20] == 1);
    assert(cave_o_idx[21][21] == 1 && o_list[1].k_idx == arrow_kind && o_list[1].number == 5);
    assert(cave_environment_cell_at(40, 41)->integrity == 40);
    assert(cave_environment_cell_at(40, 41)->work == 3);
    assert(cave_environment_get_state().random == ecology.random);
    assert(cave_environment_get_state().last_turn == ecology.last_turn + 120);
    travel(11, 5, 5);
    assert(terrain_hash() == eleven);
    travel(10, 5, 5);
    wait_for(9);
    puts("Stairs align; maps, wounded monsters and paused ecology survive revisits; distant floors expire: PASS.");

    size_t offset = 0;
    byte* buffer = SDL_malloc(16 * 1024 * 1024); assert(buffer);
    size_t length = fixture_write_levels(buffer, 16 * 1024 * 1024, &offset);
    assert(p_ptr->depth == 10 && p_ptr->chp == 73 && terrain_hash() == terrain);
    level_cache_clear();
    for (int i = 0; i < z_info->r_max; i++) r_info[i].cur_num = 0;
    assert(fixture_read_levels(buffer, length) == 0);
    assert(level_cache_ready(9) && level_cache_ready(11));
    assert(p_ptr->depth == 10 && terrain_hash() == terrain);
    travel(11, 5, 5); assert(terrain_hash() == eleven);
    travel(10, 5, 5);
    level_cache_clear();
    byte original = buffer[offset]; buffer[offset] ^= 0x20;
    assert(fixture_read_levels(buffer, length) != 0);
    buffer[offset] = original;
    level_cache_clear();
    assert(fixture_read_levels(buffer, length - 20) != 0);
    level_cache_clear();
    SDL_free(buffer);
    puts("Retained-floor save/load roundtrip and corrupt/truncated cache rejection: PASS.");

    /* A real procedural floor exercises inherited openings across rooms,
     * vault protection, population and the final connectivity checks. */
    wipe_o_list(); wipe_mon_list();
    p_ptr->depth = p_ptr->max_depth = 14;
    p_ptr->leaving = false; p_ptr->playing = true;
    term* saved_term = Term; Term = NULL;
    Rand_state_init(41419217ULL);
    generate_cave();
    Term = saved_term;
    assert(character_dungeon);
    int sy = -1, sx = -1;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_feat[y][x] == FEAT_MORE) { sy = y; sx = x; }
    assert(sy > 0);
    byte chasms[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = {{0}};
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) chasms[y][x] = cave_feat[y][x] == FEAT_CHASM;
    u64b real_terrain = terrain_hash();
    level_cache_start(); wait_for(15); wait_for(13);
    assert(terrain_hash() == real_terrain);
    travel(15, sy, sx); assert(cave_feat[sy][sx] == FEAT_LESS);
    for (int y = 1; y < MAX_DUNGEON_HGT - 1; y++)
        for (int x = 1; x < MAX_DUNGEON_WID - 1; x++)
            if (chasms[y][x]) assert(cave_feat[y][x] == FEAT_CHASM && level_cache_chasm_above(y, x));
    travel(14, sy, sx); assert(terrain_hash() == real_terrain);
    level_cache_clear();
    puts("Real procedural rooms, populations, stairs and inherited chasm footprint: PASS.");

    reset_map(18);
    cave_feat[6][6] = FEAT_MORE;
    p_ptr->py = p_ptr->px = 6; cave_m_idx[6][6] = -1;
    cave_environment_reset(); cave_environment_seed();
    level_cache_start();
    p_ptr->depth = 19; p_ptr->create_stair = FEAT_LESS; p_ptr->leaving = true;
    assert(level_cache_change_level());
    assert(p_ptr->depth == 18 && !p_ptr->leaving && !p_ptr->create_stair);
    assert(cave_feat[6][6] == FEAT_MORE);
    buffer = SDL_malloc(16 * 1024 * 1024); assert(buffer);
    length = fixture_write_levels(buffer, 16 * 1024 * 1024, &offset);
    level_cache_clear();
    assert(fixture_read_levels(buffer, length) == 0);
    SDL_free(buffer);
    cave_m_idx[6][6] = 0; p_ptr->py = p_ptr->px = 8; cave_m_idx[8][8] = -1;
    Uint64 deadline = SDL_GetTicks() + 90000;
    while (!p_ptr->leaving) { level_cache_poll(); SDL_Delay(5); assert(SDL_GetTicks() < deadline); }
    assert(level_cache_change_level());
    assert(p_ptr->depth == 19 && p_ptr->py == 6 && p_ptr->px == 6);
    level_cache_clear();
    puts("Unfinished destination keeps gameplay responsive and retains its exact arrival coordinate: PASS.");

    reset_map(1);
    cave_feat[6][6] = FEAT_LESS;
    cave_feat[5][5] = FEAT_MORE;
    p_ptr->py = p_ptr->px = 6; cave_m_idx[6][6] = -1;
    cave_environment_reset(); cave_environment_seed();
    level_cache_start(); wait_for(2); wait_for(0);
    travel(0, 6, 6); assert(cave_feat[6][6] == FEAT_MORE);
    bool exit_step = false;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
        if ((dy || dx) && cave_floor_bold(6 + dy, 6 + dx)) exit_step = true;
    assert(exit_step);
    travel(1, 6, 6); assert(cave_feat[6][6] == FEAT_LESS);
    level_cache_clear();
    puts("Authored Gates endpoint retains a connected stair at the exact world coordinate: PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    writer = OUT / "writer.c"
    writer.write_text('#include "level-generation/level-cache.h"\n' + WRITER + WRITE_LEVELS, encoding="utf-8")
    reader = OUT / "reader.c"
    reader.write_text('#include "level-generation/level-cache.h"\n' + READER + READ_LEVELS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
         "/src/level-generation/level-cache.c.obj", "/src/level-generation/level-state.c.obj",
         "/src/level-generation/level-generation.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(writer), str(reader),
                    *(str(ROOT / "src/level-generation" / name) for name in
                      ("level-cache.c", "level-state.c", "level-generation.c")),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=load_note", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=240)


if __name__ == "__main__":
    main()
