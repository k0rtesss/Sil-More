#!/usr/bin/env python3
"""Check Utumno persistence and environment with the initialized production engine.

Requires a current build-incremental.ps1 build. Real dungeon/options serializers
use SDL memory streams. The adjacent forge/death player record is extracted
verbatim from production, rather than exercising the whole player save pipeline.
Fresh and cached template initialization both run in a temporary data directory;
no player save, configuration or deployment is modified.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS as ENGINE_FIXTURE
from check_monster_scent_save import (
    WRITER as DUNGEON_WRITER,
    READER as DUNGEON_READER,
    TESTS as DUNGEON_TESTS,
    fixture_function,
)

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/utumno-state"


def source_span(path, first, last):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    start = source.index(first)
    end = source.index(last, start) + len(last)
    return source[start:end]


WRITER = r'''
size_t fixture_write_flags(byte* buffer, size_t capacity)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    __WRITE_FLAGS__
    wr_u32b(0x87654321U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
size_t fixture_write_options(byte* buffer, size_t capacity)
{
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    wr_options();
    wr_u32b(0x12345678U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''

READER = r'''
void fixture_read_flags(const byte* buffer, size_t length, int extra,
    u32b* sentinel, size_t* consumed)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    __READ_FLAGS__
    rd_u32b(sentinel);
    *consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
}
void fixture_read_options(const byte* buffer, size_t length, int extra,
    u32b* sentinel, size_t* consumed)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    rd_options();
    rd_u32b(sentinel);
    *consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
}
'''

TESTS = r'''
size_t fixture_write_flags(byte*, size_t);
void fixture_read_flags(const byte*, size_t, int, u32b*, size_t*);
size_t fixture_write_options(byte*, size_t);
void fixture_read_options(const byte*, size_t, int, u32b*, size_t*);
size_t fixture_write_dungeon(byte*, size_t, size_t*);
int fixture_read_dungeon(const byte*, size_t, int, u32b*, size_t*);
static byte encoded[65536], plain[65536], modified[65536];

static void check_flags(void)
{
    for (int visited = 0; visited < 2; ++visited)
        for (int returning = 0; returning < 2; ++returning)
            for (int dead = 0; dead < 2; ++dead)
            {
                p_ptr->unique_forge_made = true;
                p_ptr->unique_forge_seen = false;
                p_ptr->utumno_forge_visited = visited;
                p_ptr->utumno_return_to_throne = returning;
                p_ptr->is_dead = dead;
                size_t length = fixture_write_flags(encoded, sizeof(encoded));
                assert(length == 9);
                p_ptr->unique_forge_made = false;
                p_ptr->unique_forge_seen = true;
                p_ptr->utumno_forge_visited = !visited;
                p_ptr->utumno_return_to_throne = !returning;
                p_ptr->is_dead = !dead;
                size_t consumed; u32b sentinel;
                fixture_read_flags(encoded, length, 13, &sentinel, &consumed);
                assert(consumed == length && sentinel == 0x87654321U);
                assert(p_ptr->unique_forge_made && !p_ptr->unique_forge_seen);
                assert(p_ptr->utumno_forge_visited == visited);
                assert(p_ptr->utumno_return_to_throne == (visited && returning));
                assert(p_ptr->is_dead == dead);

                /* .12 has the same neighboring record, without the two new
                 * bytes. Remove them in plaintext and rebuild the XOR stream. */
                decode(encoded, plain, length);
                memmove(plain + 2, plain + 4, length - 4);
                length -= 2;
                encode(plain, modified, length);
                p_ptr->utumno_forge_visited = true;
                p_ptr->utumno_return_to_throne = true;
                p_ptr->is_dead = !dead;
                fixture_read_flags(modified, length, 12, &sentinel, &consumed);
                assert(consumed == length && sentinel == 0x87654321U);
                assert(!p_ptr->utumno_forge_visited && !p_ptr->utumno_return_to_throne);
                assert(p_ptr->unique_forge_made && !p_ptr->unique_forge_seen);
                assert(p_ptr->is_dead == dead);
            }
    puts("Player flag record: .13 combinations/normalization, .12 defaults, following death byte and sentinel alignment PASS.");
}

static void check_options(void)
{
    assert(OPT_utumno_corridors == 127);
    assert(option_text[OPT_utumno_corridors]);
    assert(!option_norm[OPT_utumno_corridors]);
    assert(!op_ptr->opt[OPT_utumno_corridors]); /* actual init_other default */
    for (int enabled = 0; enabled < 2; ++enabled)
    {
        op_ptr->opt[OPT_utumno_corridors] = enabled;
        size_t length = fixture_write_options(encoded, sizeof(encoded));
        for (int extra = 12; extra <= 13; ++extra)
        {
            op_ptr->opt[OPT_utumno_corridors] = !enabled;
            u32b sentinel; size_t consumed;
            fixture_read_options(encoded, length, extra, &sentinel, &consumed);
            assert(sentinel == 0x12345678U && consumed == length);
            assert(op_ptr->opt[OPT_utumno_corridors] == (extra == 13 && enabled));
        }
    }
    op_ptr->opt[OPT_utumno_corridors] = false;
    puts("Real options record: initialized default off, .13 false/true, .12 forced off including a stale reserved bit, stream alignment PASS.");
}

static void check_dungeon_depths(void)
{
    const int depths[] = {UTUMNO_DEPTH, UTUMNO_FORGE_DEPTH};
    for (size_t i = 0; i < N_ELEMENTS(depths); ++i)
    {
        fresh_map();
        p_ptr->depth = depths[i];
        p_ptr->py = 6; p_ptr->px = 7;
        cave_feat[7][7] = FEAT_LAVA;
        cave_feat[8][7] = FEAT_ICE;
        cave_feat[8][8] = FEAT_MORE;
        cave_feat[8][9] = FEAT_LESS;
        cave_info[8][8] = CAVE_MARK | CAVE_G_VAULT;
        size_t dungeon_size;
        size_t length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
        fresh_map();
        u32b sentinel; size_t consumed;
        assert(fixture_read_dungeon(encoded, length, 13, &sentinel, &consumed) == 0);
        assert(sentinel == 0xA1B2C3D4U && consumed == length);
        assert(p_ptr->depth == depths[i] && p_ptr->py == 6 && p_ptr->px == 7);
        assert(cave_feat[7][7] == FEAT_LAVA && cave_feat[8][7] == FEAT_ICE);
        assert(cave_feat[8][8] == FEAT_MORE && cave_feat[8][9] == FEAT_LESS);
        assert((cave_info[8][8] & (CAVE_MARK | CAVE_G_VAULT)) == (CAVE_MARK | CAVE_G_VAULT));
    }
    puts("Real dungeon record: depths 22/23, player position, elemental terrain, stairs, vault flags and following sentinel PASS.");
}

static void set_regions(void)
{
    partition_meta_save regions = {0};
    regions.grid_rows = 1; regions.grid_cols = regions.partition_count = 3;
    regions.modes[0] = QUAD_MODE_ROOMY;
    regions.modes[1] = regions.modes[2] = QUAD_MODE_BIG_CAVE;
    regions.big_cave_types[1] = BIG_CAVE_ICE;
    regions.big_cave_types[2] = BIG_CAVE_FIRE;
    level_partition_meta_set(&regions);
}

static void check_resistance(void)
{
    fresh_map();
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    set_regions();
    int x_by_region[3] = {-1, -1, -1};
    for (int x = 2; x < p_ptr->cur_map_wid - 2; ++x)
    {
        int region = level_partition_index_for_point(5, x);
        assert(region >= 0 && region < 3);
        if (x_by_region[region] < 0) x_by_region[region] = x;
    }
    for (int i = 0; i < 3; ++i) assert(x_by_region[i] > 0);
    p_ptr->py = 5; p_ptr->px = x_by_region[0];
    p_ptr->depth = MORGOTH_DEPTH;
    calc_bonuses();
    int base_fire = p_ptr->resist_fire, base_cold = p_ptr->resist_cold;
    const u32b flags[] = {0, CAVE_ICKY, CAVE_G_VAULT, CAVE_MORGOTH_TUNNEL};
    for (int region = 0; region < 3; ++region)
        for (size_t flag = 0; flag < N_ELEMENTS(flags); ++flag)
        {
            p_ptr->depth = UTUMNO_DEPTH;
            p_ptr->px = x_by_region[region];
            cave_info[5][p_ptr->px] = flags[flag];
            calc_bonuses();
            assert(p_ptr->resist_fire == base_fire - 1);
            assert(p_ptr->resist_cold == base_cold - 1);
            /* With and without temporary protection, preview every regional
             * destination. This catches the old fire-cave adjustment at both
             * ends of a movement preview, including lethal ground contact. */
            for (int oppose = 0; oppose < 2; ++oppose)
                for (int airborne = 0; airborne < 2; ++airborne)
                {
                    p_ptr->oppose_fire = oppose;
                    for (int destination = 0; destination < 3; ++destination)
                        assert(player_lava_damage_at(5, x_by_region[destination], airborne)
                            == player_lava_damage(airborne));
                }
            p_ptr->oppose_fire = 0;
            cave_info[5][p_ptr->px] = 0;
        }
    /* Verify normal regional penalties still work outside Utumno. */
    p_ptr->depth = 19;
    p_ptr->px = x_by_region[1]; calc_bonuses();
    assert(p_ptr->resist_fire == base_fire && p_ptr->resist_cold == base_cold - 1);
    p_ptr->px = x_by_region[2]; calc_bonuses();
    assert(p_ptr->resist_fire == base_fire - 1 && p_ptr->resist_cold == base_cold);
    const int exits[] = {UTUMNO_FORGE_DEPTH, MORGOTH_DEPTH, 19};
    for (size_t i = 0; i < N_ELEMENTS(exits); ++i)
    {
        p_ptr->depth = exits[i]; p_ptr->px = x_by_region[0]; calc_bonuses();
        assert(p_ptr->resist_fire == base_fire && p_ptr->resist_cold == base_cold);
    }
    puts("Real calc_bonuses: depth22 exactly -1 fire/cold in rooms/ice/fire/vaults; lava previews with protection/flight; exit restoration and normal cave penalties PASS.");
}

static void check_forbidden_travel(void)
{
    const int depths[] = {UTUMNO_DEPTH, UTUMNO_FORGE_DEPTH};
    for (size_t i = 0; i < N_ELEMENTS(depths); ++i)
        for (int enabled = 0; enabled < 2; ++enabled)
        {
            fresh_map(); p_ptr->depth = depths[i];
            op_ptr->opt[OPT_utumno_corridors] = enabled;
            p_ptr->leaving = false;
            p_ptr->utumno_forge_visited = i != 0;
            p_ptr->utumno_return_to_throne = false;
            teleport_player_level();
            assert(p_ptr->depth == depths[i] && !p_ptr->leaving);
            assert(p_ptr->utumno_forge_visited == (i != 0));
            assert(!p_ptr->utumno_return_to_throne);
            cave_feat[p_ptr->py][p_ptr->px] = FEAT_TRAP_false_FLOOR;
            int hp = p_ptr->chp;
            hit_trap(p_ptr->py, p_ptr->px);
            assert(p_ptr->depth == depths[i] && !p_ptr->leaving && p_ptr->chp == hp);
            assert(cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR);
            assert(!p_ptr->utumno_return_to_throne);
        }
    puts("Real teleport/hit_trap: no 22->21, 22->23 bypass or 23->24; option-on/off; flags, HP and leaving state preserved PASS.");
}

static void check_loot_and_score(void)
{
    fresh_map();
    drop_system_init();
    const int depths[] = {UTUMNO_DEPTH, UTUMNO_FORGE_DEPTH};
    for (size_t i = 0; i < N_ELEMENTS(depths); ++i)
    {
        p_ptr->depth = depths[i];
        object_type artefact = {0};
        assert(drop_generate_guaranteed_artefact(depths[i], depths[i],
            DROP_QUALITY_SUPERB, DROP_TYPE_JEWELRY, NULL, &artefact));
        assert(artefact.k_idx && artefact.name1);
        high_score score = {0};
        strnfmt(score.max_dun, sizeof(score.max_dun), "%d", depths[i]);
        strnfmt(score.cur_dun, sizeof(score.cur_dun), "%d", depths[i]);
        score_breakdown result = score_calculate_breakdown(&score);
        assert(result.max_depth == depths[i] && result.cur_depth == depths[i]);
        assert(result.descent_points == 10 * MORGOTH_DEPTH && !result.depth_up);
    }
    puts("Real drop catalog/score: artefacts remain eligible at 22/23; recorded depths preserved with descent score capped at 20 PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    writer = DUNGEON_WRITER + WRITER.replace("__WRITE_FLAGS__", source_span(
        "src/fs/save-player.c", "wr_byte(p_ptr->unique_forge_made ? 1 : 0);",
        "wr_byte(p_ptr->is_dead ? 1 : 0);"))
    reader = DUNGEON_READER + READER.replace("__READ_FLAGS__", source_span(
        "src/fs/load-player.c", "    rd_bool(&p_ptr->unique_forge_made);",
        "    rd_bool(&p_ptr->is_dead);"))
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += '\n#include "cave/cave-fixtures.h"\n#include "cave/cave-flood.h"\n'
    prefix += '#include "cave/cave-water-flow.h"\n'
    prefix += '#include "player/player-upkeep-internal.h"\n'
    prefix += '#include "score/score_logic.h"\n'
    helpers = DUNGEON_TESTS[DUNGEON_TESTS.index("static void decode("):
                            DUNGEON_TESTS.index("static void test_current_roundtrip(")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + fixture_function("reset_map")
    # TESTS needs the helper declarations, and the helper definitions need no test globals.
    harness += helpers + TESTS + init
    harness += """
    check_options(); check_flags(); check_dungeon_depths();
    check_resistance(); check_forbidden_travel(); check_loot_and_score();
    puts("Utumno state integration: PASS.");
    SDL_Quit(); return 0;
}
"""
    sources = []
    for name, content in (("check.c", harness), ("writer.c", writer), ("reader.c", reader)):
        path = OUT / name
        path.write_text(content, encoding="utf-8")
        sources.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj")
    objects = [p for p in objects if not p.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        for label in ("Fresh", "Cached"):
            print(f"{label} template initialization:", flush=True)
            subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                           cwd=data, env=env, check=True, timeout=90)
            cache = Path(data) / "drops.raw"
            assert cache.exists(), "Drop catalog cache was not written"
            if label == "Fresh":
                cached_mtime = cache.stat().st_mtime_ns
            else:
                assert cache.stat().st_mtime_ns == cached_mtime, "Cached catalog was rebuilt"


if __name__ == "__main__":
    main()
