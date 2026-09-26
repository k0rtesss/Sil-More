#!/usr/bin/env python3
"""Exercise optional Tale memories with real engine objects and temporary data.

Run build-incremental.ps1 first. No player files or visible game window are used.
The save fixture uses the production dungeon reader/writer, including a v24
fixture with the new area block removed and malformed v25 block rejection.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, WRITER, READER

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/meta-memories"

TESTS = r'''
#include "meta_state.h"
#include "metarun.h"
#include "blitz.h"
#include "sdl-config.h"
#include "fs/path.h"
#include "score/score_guid.h"
#include "cave/cave-environment.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
#include "player/player-song-internal.h"

size_t fixture_write_dungeon(byte*, size_t, size_t*);
int fixture_read_dungeon(const byte*, size_t, int, u32b*, size_t*);
bool fixture_place_legendary(void);
bool fixture_area_fit(const meta_dungeon_area*, int, int);
int fixture_meta_drop_count(int);
int fixture_smith_slot(void);
static int checks;
#define CHECK(t) do { checks++; if (!(t)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#t); exit(1); } } while(0)
static byte encoded[524288], plain[524288], legacy[524288];

static void set_options(bool value)
{
    for (int o = OPT_meta_artefact_memory; o <= OPT_meta_legendary_places; o++)
        op_ptr->opt[o] = value;
}

static void test_settings(void)
{
    sdl_config_reset_app_options_to_defaults();
    for (int o = OPT_meta_artefact_memory; o <= OPT_meta_legendary_places; o++) {
        CHECK(!option_norm[o] && !op_ptr->opt[o]);
        CHECK(option_is_app_persistent(o));
        CHECK(strstr(option_desc[o], "Beta:"));
        int count = 0;
        for (int i = 0; i < OPT_PAGE_PER; i++) count += option_page[GAMEPLAY_PAGE][i] == o;
        CHECK(count == 1);
    }
    static struct sdl_config cfg;
    char path[1024]; path_build(path, sizeof(path), ANGBAND_DIR_USER, "memory-settings.json");
    sdl_config_set_defaults(&cfg);
    for (int mask = 0; mask < 16; mask++) {
        for (int bit = 0; bit < 4; bit++) op_ptr->opt[OPT_meta_artefact_memory + bit] = (mask & (1 << bit)) != 0;
        CHECK(sdl_config_save(path, &cfg, NULL, 0));
        set_options(false);
        CHECK(sdl_config_load(path, &cfg, NULL, 0, NULL) == SDL_CONFIG_LOAD_OK);
        sdl_config_load_app_options(path);
        for (int bit = 0; bit < 4; bit++) CHECK(op_ptr->opt[OPT_meta_artefact_memory + bit] == ((mask & (1 << bit)) != 0));
    }
    SDL_IOStream* file = SDL_IOFromFile(path, "wb"); CHECK(file);
    CHECK(SDL_WriteIO(file, "{}", 2) == 2); SDL_CloseIO(file);
    CHECK(sdl_config_load(path, &cfg, NULL, 0, NULL) == SDL_CONFIG_LOAD_OK);
    sdl_config_load_app_options(path);
    for (int o = OPT_meta_artefact_memory; o <= OPT_meta_legendary_places; o++) CHECK(!op_ptr->opt[o]);
    set_options(true); run_mode_set_current(RUN_MODE_BLITZ);
    for (int o = OPT_meta_artefact_memory; o <= OPT_meta_legendary_places; o++) CHECK(!meta_memory_enabled(o));
    run_mode_set_current(RUN_MODE_STORY); set_options(false);
    puts("Gameplay options: 16 JSON combinations, missing-key defaults, Beta rows and Blitz exclusion PASS.");
}

static void test_knowledge(void)
{
    int a = 1;
    a_info[a].flags3 |= TR3_EASY_ID;
    CHECK(!metarun_record_artefact_revealed(a));
    op_ptr->opt[OPT_meta_artefact_memory] = true;
    CHECK(metarun_record_artefact_identification(a));
    a_info[a].seen = 0;
    metarun_apply_artefact_memory();
    CHECK(a_info[a].seen & ART_SEEN_REVEALED);
    CHECK(a_info[a].seen & ART_SEEN_METARUN_EASY_ID);
    op_ptr->opt[OPT_meta_artefact_memory] = false;
    metarun_apply_artefact_memory();
    CHECK(!(a_info[a].seen & (ART_SEEN_REVEALED | ART_SEEN_METARUN_EASY_ID)));
    op_ptr->opt[OPT_meta_artefact_memory] = true;
    metarun_apply_artefact_memory();
    CHECK(a_info[a].seen & ART_SEEN_REVEALED);
    CHECK(metarun_record_artefact_revealed(a)); /* Now learned by this hero. */
    op_ptr->opt[OPT_meta_artefact_memory] = false;
    metarun_apply_artefact_memory();
    CHECK(a_info[a].seen & ART_SEEN_REVEALED);
    metar.id++; a_info[a].seen = 0;
    op_ptr->opt[OPT_meta_artefact_memory] = true;
    metarun_apply_artefact_memory();
    CHECK(!a_info[a].seen);
    metar.id--; op_ptr->opt[OPT_meta_artefact_memory] = false;
    puts("Artefact knowledge: opt-in writing, inherited/local lore, disable and Tale isolation PASS.");
}

static void test_artefacts(void)
{
    int original = 0;
    for (int i = 1; i < z_info->art_norm_max; i++)
        if (a_info[i].tval && a_info[i].sval) { original = i; break; }
    CHECK(original);
    artefact_type art = a_info[original];
    art.guid = score_guid_random(); art.cur_num = art.found_num = art.seen = 0;
    SDL_strlcpy(art.name, "Fixture legacy", sizeof(art.name));
    object_type object = {0};
    object_prep(&object, lookup_kind(art.tval, art.sval));
    meta_artifact_record record;
    p_ptr->pcharacter = 1;
    CHECK(!meta_artifact_build_created_record(&record, &art, &object, 14, false, false));
    CHECK(meta_artifact_build_created_record(&record, &art, &object, 15, false, false));
    CHECK(!meta_artifact_register_created(&record));
    op_ptr->opt[OPT_meta_forged_artefacts] = true;
    CHECK(meta_artifact_register_created(&record));
    CHECK(meta_artifact_prepare_runtime() && !meta_artifact_runtime_count());
    metar.deaths++; SDL_strlcpy(op_ptr->full_name, "Renamed hero", sizeof(op_ptr->full_name));
    CHECK(meta_artifact_prepare_runtime() && !meta_artifact_runtime_count());
    p_ptr->pcharacter = 2;
    CHECK(meta_artifact_prepare_runtime() && meta_artifact_runtime_count() == 1);
    int slot = 0;
    for (int i = z_info->art_rand_max; i < z_info->art_max; i++)
        if (meta_artifact_runtime_slot_is_meta(i)) slot = i;
    CHECK(slot);
    CHECK(meta_artifact_unused_runtime_slot() == slot);
    op_ptr->opt[OPT_meta_forged_artefacts] = false;
    CHECK(meta_artifact_prepare_runtime());
    CHECK(!a_info[slot].tval && !meta_artifact_runtime_description(slot));
    op_ptr->opt[OPT_meta_forged_artefacts] = true;
    CHECK(meta_artifact_prepare_runtime()); CHECK(meta_artifact_runtime_slot_is_meta(slot));
    artefact_type* saved_slots = mem_alloc_array(z_info->art_max, artefact_type);
    memcpy(saved_slots, a_info, z_info->art_max * sizeof(*a_info));
    for (int i = z_info->art_rand_max; i < z_info->art_self_made_max - 2; i++)
        if (i != slot) a_info[i] = art;
    CHECK(fixture_smith_slot() == slot); /* A full legacy pool cannot block smithing. */
    a_info[slot].cur_num = 1; CHECK(fixture_smith_slot() == -1);
    memcpy(a_info, saved_slots, z_info->art_max * sizeof(*a_info)); mem_free_null(saved_slots);
    a_info[slot].cur_num = 1; a_info[slot].found_num = 1; a_info[slot].seen = ART_SEEN_REVEALED;
    CHECK(meta_artifact_unused_runtime_slot() == -1);
    CHECK(meta_artifact_prepare_runtime() && meta_artifact_runtime_count() == 1);
    CHECK(meta_artifact_runtime_slot_is_meta(slot));
    CHECK(a_info[slot].cur_num == 1 && a_info[slot].found_num == 1);
    CHECK(a_info[slot].seen == ART_SEEN_REVEALED);
    CHECK(meta_artifact_runtime_description(slot));
    drop_system_init();
    int entries = fixture_meta_drop_count(slot); CHECK(entries > 0);
    drop_system_init(); CHECK(fixture_meta_drop_count(slot) == entries);
    op_ptr->opt[OPT_meta_forged_artefacts] = false;
    drop_system_init(); CHECK(!fixture_meta_drop_count(slot));
    CHECK(a_info[slot].tval == art.tval && a_info[slot].cur_num == 1);
    op_ptr->opt[OPT_meta_forged_artefacts] = true;
    drop_system_init(); CHECK(fixture_meta_drop_count(slot) == entries);
    metar.id++;
    CHECK(meta_artifact_prepare_runtime() && !meta_artifact_runtime_count());
    metar.id--; op_ptr->opt[OPT_meta_forged_artefacts] = false;
    player_wipe();
    CHECK(!a_info[slot].tval && !a_info[slot].sval && !a_info[slot].name[0]);
    puts("Forged legacy: threshold, creator exclusion, GUID slot rebinding, counters, raw catalog isolation and Tale isolation PASS.");
}

static void test_revenge(void)
{
    const int race = 41, other = 42;
    monster_race original = r_info[race];
    CHECK(!(original.flags1 & (RF1_UNIQUE | RF1_QUESTOR)));
    meta_monster_death_event event = {0};
    event.r_idx = race; event.monster_guid = score_guid_from_u64(original.guid);
    event.character_guid = c_info[1].guid; event.depth = 3; event.turn = 100;
    SDL_strlcpy(event.character_name, "Fixture hero", sizeof(event.character_name));
    CHECK(!meta_monster_record_player_death(&event));
    op_ptr->opt[OPT_meta_revenge] = true;
    for (int n = 0; n < 4; n++) CHECK(meta_monster_record_player_death(&event));
    const meta_monster_record* record = meta_monster_find_record_for_race(race);
    CHECK(record && record->rank == 3 && record->kill_memory_count == 3);
    r_info[other].evn += 7;
    int unrelated = r_info[other].evn;
    CHECK(meta_monster_apply_runtime_overrides());
    CHECK(r_info[race].flags1 & RF1_UNIQUE);
    CHECK(r_info[race].max_num == 1 && r_info[race].hdice > original.hdice);
    int scaled = r_info[race].hdice;
    meta_monster_invalidate_runtime_overrides(); CHECK(meta_monster_apply_runtime_overrides());
    CHECK(r_info[race].hdice == scaled && r_info[other].evn == unrelated);
    for (int n = 1; n <= 10; n++) {
        CHECK(meta_monster_record_revenge_kill(race));
        if (n == 1 || n == 3 || n == 6 || n == 10) CHECK(meta_monster_revenge_bonus() == n);
    }
    op_ptr->opt[OPT_meta_revenge] = false;
    CHECK(meta_monster_apply_runtime_overrides());
    CHECK(!meta_monster_is_revenge_marked_race(race) && !meta_monster_revenge_bonus());
    CHECK(!(r_info[race].flags1 & RF1_UNIQUE));
    CHECK(r_info[race].hdice == original.hdice && r_info[other].evn == unrelated);
    CHECK(!meta_monster_record_revenge_kill(race));
    op_ptr->opt[OPT_meta_revenge] = true;
    CHECK(meta_monster_apply_runtime_overrides() && r_info[race].hdice == scaled);
    r_info[race].max_num = 0;
    meta_monster_invalidate_runtime_overrides(); CHECK(meta_monster_apply_runtime_overrides());
    CHECK(!r_info[race].max_num); /* Refresh must not resurrect a killed nemesis. */
    metar.id++;
    CHECK(meta_monster_apply_runtime_overrides());
    CHECK(!meta_monster_find_record_for_race(race) && !meta_monster_revenge_bonus());
    CHECK(r_info[race].hdice == original.hdice);
    metar.id--; op_ptr->opt[OPT_meta_revenge] = false;
    meta_state_reset_character();
    puts("Revenge: disabled writes, rank cap, exact bonuses, no compounding, selective restoration, death persistence and Tale isolation PASS.");
}

static void decode(const byte* src, byte* dst, size_t size)
{
    byte prev = 0;
    for (size_t i = 0; i < size; i++) { byte now = src[i]; dst[i] = now ^ prev; prev = now; }
}
static void encode(const byte* src, byte* dst, size_t size)
{
    byte prev = 0;
    for (size_t i = 0; i < size; i++) { prev ^= src[i]; dst[i] = prev; }
}

static void test_places(void)
{
    set_options(false); reset_map(5); cave_environment_reset();
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    cave_m_idx[2][2] = -1;
    meta_dungeon_record record = {0};
    record.meta.metarun_id = metar.id; record.meta.record_guid = score_guid_random();
    record.song_id = SNG_ELBERETH; record.depth = 5; record.hgt = record.wid = 3;
    record.mask_cell_count = 9; record.singer_y = record.singer_x = 1;
    record.singer_character_guid = c_info[1].guid; p_ptr->pcharacter = 2;
    record.affected_monster_count = 1;
    byte blob[29] = {255, 1};
    for (int i = 0; i < 9; i++) { blob[2+i] = FEAT_FLOOR; blob[11+i] = TERM_WHITE; blob[20+i] = META_DUNGEON_TILE_ROLE_ROOM; }
    CHECK(!meta_dungeon_register_legendary_area(&record, blob, sizeof(blob)));
    op_ptr->opt[OPT_meta_legendary_places] = true;
    CHECK(meta_dungeon_register_legendary_area(&record, blob, sizeof(blob)));
    meta_dungeon_area area = {.record = record, .tile_blob = blob, .tile_blob_size = sizeof(blob)};
    CHECK(fixture_area_fit(&area, 8, 8));
    dun->cent_n = 1; dun->is_quest[0] = true;
    dun->corner[0].y1 = dun->corner[0].x1 = 8; dun->corner[0].y2 = dun->corner[0].x2 = 10;
    CHECK(!fixture_area_fit(&area, 8, 8));
    dun->cent_n = 0;
    p_ptr->depth = 4; CHECK(!fixture_place_legendary());
    p_ptr->depth = 5;
    p_ptr->pcharacter = 1; CHECK(!fixture_place_legendary()); p_ptr->pcharacter = 2;
    op_ptr->opt[OPT_meta_legendary_places] = false; CHECK(!fixture_place_legendary());
    op_ptr->opt[OPT_meta_legendary_places] = true; CHECK(fixture_place_legendary());
    CHECK(!fixture_place_legendary());
    legendary_area_map_reset(); legendary_area_note_spawned(1, &area);
    legendary_area_id[2][2] = 1; legendary_area_note_player_position();
    CHECK(legendary_area_song_is_available(SNG_ELBERETH));
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = false;
    CHECK(!legendary_area_song_skill_bonus(SNG_ELBERETH));
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = true;
    CHECK(legendary_area_song_skill_bonus(SNG_ELBERETH) == 5);
    p_ptr->active_ability[S_SNG][SNG_ELBERETH] = false;
    p_ptr->song1 = SNG_ELBERETH;
    op_ptr->opt[OPT_meta_legendary_places] = false; legendary_area_note_player_position();
    CHECK(p_ptr->song1 == SNG_NOTHING && !legendary_area_song_is_available(SNG_ELBERETH));
    op_ptr->opt[OPT_meta_legendary_places] = true; legendary_area_note_player_position();
    p_ptr->song1 = SNG_ELBERETH; p_ptr->px = 3; legendary_area_note_player_position();
    CHECK(p_ptr->song1 == SNG_NOTHING);
    p_ptr->px = 2; legendary_area_note_player_position();
    size_t dungeon_size, consumed;
    size_t length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
    legendary_area_map_reset();
    u32b sentinel;
    CHECK(!fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    CHECK(sentinel == 0xA1B2C3D4U && consumed == length);
    CHECK(legendary_area_id[2][2] == 1 && legendary_area_song_is_available(SNG_ELBERETH));
    guid64 guid; bool seen;
    CHECK(legendary_area_get_save_record(1, &guid, &seen) && seen);
    op_ptr->opt[OPT_meta_legendary_places] = false;
    CHECK(!fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    CHECK(!legendary_area_song_is_available(SNG_ELBERETH));
    decode(encoded, plain, length);
    size_t start = 0;
    for (size_t i = 0; i + 5 < dungeon_size; i++)
        if (plain[i] == 0xF0 && plain[i+1] == 0xC1 && plain[i+2] == 1) { start = i; break; }
    CHECK(start);
    unsigned active = plain[start+3] | (plain[start+4] << 8);
    size_t end = start + 5 + active * 11, cells = 0;
    while (cells < 20*24) { cells += plain[end]; end += 3; }
    CHECK(plain[end] == 0xF0 && plain[end+1] == 0xC2);
    memcpy(legacy, plain, start); memcpy(legacy+start, plain+end, length-end);
    size_t old_length = length - (end-start);
    encode(legacy, encoded, old_length);
    CHECK(!fixture_read_dungeon(encoded, old_length, 24, &sentinel, &consumed));
    CHECK(sentinel == 0xA1B2C3D4U && consumed == old_length && !legendary_area_id[2][2]);
    plain[start+2] = 99; encode(plain, encoded, length);
    CHECK(fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    plain[start+2] = 1; plain[start+5+active*11] = 0; encode(plain, encoded, length);
    CHECK(fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    puts("Legendary places: opt-in, depth/fit/quest protection, grants, exit/toggle stop, v25/v24 streams and corrupt blocks PASS.");
}

static unsigned captured_count(void)
{
    meta_dungeon_area* areas = NULL; u32b count = 0;
    CHECK(meta_dungeon_load_for_current_metarun(&areas, &count));
    meta_dungeon_areas_free(areas, count);
    return count;
}

static void test_song_capture(void)
{
    metar.id += 2; set_options(false); reset_map(6); legendary_area_map_reset();
    p_ptr->cur_map_hgt = 20; p_ptr->cur_map_wid = 24;
    p_ptr->pcharacter = 0;
    for (int c = 1; c < z_info->c_max; c++)
        for (int i = 0; i < CHARACTER_ABILITY_MAX; i++)
            if (c_info[c].a_adj[i][0] == S_SNG && c_info[c].a_adj[i][1] == SNG_LORIEN)
                p_ptr->pcharacter = c;
    CHECK(p_ptr->pcharacter != 0);
    CHECK(place_monster_one(3, 3, 41, false, false, NULL));
    int monster = cave_m_idx[3][3];
    p_ptr->song1 = SNG_LORIEN; p_ptr->song2 = SNG_NOTHING;
    p_ptr->active_ability[S_SNG][SNG_LORIEN] = true;
    p_ptr->skill_use[S_SNG] = 60; p_ptr->csp = 100;
    sing(); CHECK(p_ptr->song1 == SNG_LORIEN); /* Known songs work with the beta off. */
    legendary_song_observe_begin(SNG_LORIEN, 60);
    legendary_song_observe_monster(monster, 0); legendary_song_observe_end(SNG_LORIEN, 60);
    CHECK(!captured_count());
    op_ptr->opt[OPT_meta_legendary_places] = true;
    for (int i = 0; i < 20; i++) {
        legendary_song_observe_begin(SNG_LORIEN, 14);
        legendary_song_observe_monster(monster, 0); legendary_song_observe_end(SNG_LORIEN, 14);
        legendary_song_observe_begin(SNG_LORIEN, 60); legendary_song_observe_end(SNG_LORIEN, 60);
    }
    CHECK(!captured_count());
    /* Exercise the real song effect hook while its observation window is open. */
    Rand_state_import(12345);
    for (int i = 0; i < 80 && !captured_count(); i++) {
        mon_list[monster].alertness = 100;
        update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
        legendary_song_observe_begin(SNG_LORIEN, 60);
        sing_song_of_lorien(1000);
        CHECK(mon_list[monster].alertness < 100);
        legendary_song_observe_end(SNG_LORIEN, 60);
    }
    CHECK(captured_count() == 1);
    for (int i = 0; i < 5; i++) {
        legendary_song_observe_begin(SNG_LORIEN, 60);
        legendary_song_observe_monster(monster, 0); legendary_song_observe_end(SNG_LORIEN, 60);
    }
    CHECK(captured_count() == 1);
    metar.id++; CHECK(!captured_count()); metar.id -= 3;
    puts("Real song effects: normal singing, disabled/below-threshold/no-effect capture, successful capture, deduplication and Tale isolation PASS.");
}
'''

PLACEMENT = r'''
#include "level-generation/level-generation.c"
bool fixture_place_legendary(void) { return place_legendary_area_for_depth(); }
bool fixture_area_fit(const meta_dungeon_area* area, int y, int x)
{ return legendary_area_fit_at(area, y, x); }
'''

DROPS = r'''
#include "drop_system.c"
int fixture_meta_drop_count(int a_idx)
{
    int count = 0;
    for (size_t i = 0; i < g_drop_count; i++)
        if (g_drop_entries[i].obj.name1 == a_idx) count++;
    return count;
}
'''

SMITHING = r'''
#include "cmd/ui/cmd-ui-smithing.c"
int fixture_smith_slot(void) { return smith_find_available_artefact_slot(); }
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += "\n" + TESTS + "\n" + init
    harness += '''
    ANGBAND_DIR_APEX = argv[2]; metar.id = 4242;
    r_base = mem_alloc_array(z_info->r_max, monster_race);
    memcpy(r_base, r_info, z_info->r_max * sizeof(*r_base));
    reset_map(5);
    test_settings(); test_knowledge(); test_artefacts(); test_revenge(); test_places(); test_song_capture();
    printf("Tale memory integration: %d checks PASS.\\n", checks);
    SDL_Quit(); return 0;
}
'''
    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER), ("reader.c", READER),
                          ("placement.c", PLACEMENT), ("drops.c", DROPS), ("smithing.c", SMITHING)):
        source = OUT / name
        source.write_text(content, encoding="utf-8")
        sources.append(str(source))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj",
                "/src/level-generation/level-generation.c.obj", "/src/drop_system.c.obj",
                "/src/cmd/ui/cmd-ui-smithing.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
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
