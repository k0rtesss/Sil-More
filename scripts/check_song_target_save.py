#!/usr/bin/env python3
"""Keep duel-song targets through compaction and complete production save/load.

Run after build-incremental.ps1. All saves, data and configuration are temporary.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/song-target-save"

CHECKS = r'''
static void setup_target(int song, bool hole)
{
    player_wipe();
    reset_map(5);
    turn = playerturn = 100;
    SDL_strlcpy(op_ptr->full_name, "SongTarget", sizeof(op_ptr->full_name));
    p_ptr->song1 = song;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->playing = true;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->csp = p_ptr->msp = 100;
    p_ptr->food = PY_FOOD_FULL;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    current_build_vault_exact_token = true;
    assert(place_monster_one(10, 10, R_IDX_GORTHAUR, false, true, NULL));
    if (hole) {
        assert(place_monster_one(10, 11, R_IDX_GOTHMOG, false, true, NULL));
        delete_monster_idx(cave_m_idx[10][10]);
    }
    current_build_vault_exact_token = false;
    int idx = cave_m_idx[10][hole ? 11 : 10];
    assert(idx == (hole ? 2 : 1));
    p_ptr->target_who = p_ptr->health_who = idx;
    p_ptr->song_target_idx = song == SNG_NOTHING ? 0 : idx;
    p_ptr->song_target_song = song;
    mon_list[idx].song_contest_stacks = 2;
    mon_list[idx].song_lament_stacks = 1;
}

static void check_target(int song, bool hole)
{
    int idx = cave_m_idx[10][hole ? 11 : 10];
    assert(idx == 1 && mon_max == 2 && mon_list[idx].r_idx);
    assert(p_ptr->song_target_idx == (song == SNG_NOTHING ? 0 : idx));
    assert(p_ptr->song_target_song == song && p_ptr->song1 == song);
    assert(mon_list[idx].song_contest_stacks == 2);
    assert(mon_list[idx].song_lament_stacks == 1);
}

static void checks(void)
{
    const int songs[] = { SNG_NOTHING, SNG_CONTEST, SNG_LAMENT };
    build_randart_tables();
    flavor_init();
    for (unsigned s = 0; s < N_ELEMENTS(songs); ++s) {
        int song = songs[s];
        setup_target(song, true);
        compact_monsters(0);
        check_target(song, true);
        assert(p_ptr->target_who == 1 && p_ptr->health_who == 1);

        for (int hole = 0; hole < 2; ++hole) {
            setup_target(song, hole);
            strnfmt(savefile, sizeof(savefile), "song-%d-hole-%d", song, hole);
            assert(save_player());
            check_target(song, hole);
            assert(p_ptr->target_who == 1 && p_ptr->health_who == 1);
            p_ptr->song_target_idx = 0;
            p_ptr->song_target_song = SNG_NOTHING;
            assert(load_player() && character_loaded && !p_ptr->is_dead);
            check_target(song, hole);
            delete_monster_idx(1);
            assert(p_ptr->song_target_idx == 0);
            assert(p_ptr->song_target_song == SNG_NOTHING);
        }
    }
    puts("PASS: Contest, Lament and untargeted songs; live compaction, 6 full save/reload roundtrips, duel stacks and removed-target cleanup.");
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    setbuf(stdout, NULL);
    log_set_quiet(true);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    ANGBAND_DIR_SAVE = ANGBAND_DIR_APEX = argv[2];
    assert(term_init(&test_term, 80, 24, 256) == 0);
    test_term.xtra_hook = terminal_extra;
    angband_term[0] = &test_term;
    Term_activate(&test_term);
    assert(init_z_info() == 0); assert(init_rt_info() == 0);
    assert(init_f_info() == 0); assert(init_k_info() == 0);
    assert(init_b_info() == 0); assert(init_a_info() == 0);
    assert(init_e_info() == 0); assert(init_r_info() == 0);
    assert(init_v_info() == 0); assert(init_p_info() == 0);
    assert(init_c_info() == 0); assert(init_oath_info() == 0);
    assert(init_cu_info() == 0); assert(init_mb_info() == 0);
    assert(init_h_info() == 0); assert(init_st_info() == 0);
    assert(init_partition_info() == 0); assert(init_quest_info() == 0);
    assert(init_effect_info() == 0); assert(init_flavor_info() == 0);
    assert(init_skeleton_note_info() == 0); assert(init_n_info() == 0);
    assert(init_style_info() == 0); assert(init_other() == 0);
    assert(init_alloc() == 0);
    rp_ptr = &p_info[0]; current_character_profile = &c_info[0];
    checks();
    SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static const char* guids[]")]
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n"
                      + fixture_function("reset_map") + CHECKS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile],
                       cwd=profile, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
