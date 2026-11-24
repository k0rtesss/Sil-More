#!/usr/bin/env python3
"""Exercise fatal damage attribution and run history with isolated engine state."""

import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/damage-attribution"
SOURCES = (
    "src/player/killer.c", "src/spell/spell-damage.c",
    "src/spell/spell-projection-effects.c", "src/melee/melee-attack.c",
    "src/cmd/combat/cmd-combat.c", "src/cmd/world/cmd-interact-chest.c",
)

CHECKS = r'''
#include "player/killer.h"
#include "score/score_guid.h"
#include "score/score_paths.h"
#include "score/score_runs.h"
#include "cmd/world/cmd-interact-chest.h"
#include "metarun.h"

extern bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ);
extern void process_player(void);

static int wolf_death_calls;
static bool first_wolf_death;
void __wrap_message_flush(void)
{
    message_discard_pending();
}
void __wrap_sil_achievements_on_wolf_death(bool first)
{
    ++wolf_death_calls;
    first_wolf_death = first;
}

static int prepare_attack(int race)
{
    wipe_o_list();
    wipe_mon_list();
    player_wipe();
    killer_reset();
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    p_ptr->depth = p_ptr->max_depth = 8;
    p_ptr->playing = true;
    p_ptr->tutorial_deferred = true;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 30;
    p_ptr->py = p_ptr->px = 10;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->resist_fire = p_ptr->resist_pois = 1;
    p_ptr->skill_use[S_PER] = 100;
    p_ptr->energy = 100;
    character_generated = character_dungeon = true;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            cave_m_idx[y][x] = cave_o_idx[y][x] = 0;
            cave_info[y][x] = CAVE_VIEW | CAVE_SEEN | CAVE_FIRE;
            cave_light[y][x] = 1;
            bool edge = !y || !x || y == 29 || x == 29;
            cave_set_feat(y, x, edge ? FEAT_WALL_PERM : FEAT_FLOOR);
        }
    }
    cave_m_idx[10][10] = -1;
    monster_type target = {.r_idx = race, .hp = 100, .maxhp = 100,
        .alertness = ALERTNESS_VERY_ALERT, .ml = true, .cdis = 1,
        .achievement_first_wolf = race == 11};
    int index = monster_place(10, 9, &target);
    assert(index > 0);
    Rand_state_init(468);
    turn = 800;
    playerturn = 80;
    wolf_death_calls = 0;
    first_wolf_death = false;
    return index;
}

static void configure_wolf(int effect, bool hit)
{
    monster_race* race = &r_info[11];
    memset(race->blow, 0, sizeof(race->blow));
    race->blow[0].method = RBM_BITE;
    race->blow[0].effect = effect;
    race->blow[0].att = hit ? 80 : -80;
    race->blow[0].dd = race->blow[0].ds = 1;
}

static void check_record(score_killer_kind kind, int race, cptr cause)
{
    assert(p_ptr->is_dead && !strcmp(p_ptr->died_from, cause));
    const killer_info* killer = killer_last();
    if (killer && (killer->kind != kind || killer->race_index != race))
        fprintf(stderr, "Death by %s recorded killer kind %d and race %d, expected %d and %d.\n",
            cause, killer->kind, killer->race_index, kind, race);
    assert(killer && killer->kind == kind && killer->race_index == race);
    guid64 guid = score_guid_from_u64(race ? r_info[race].guid : 0);
    assert(!memcmp(&killer->guid, &guid, sizeof(guid)));
    assert(wolf_death_calls == 1 && first_wolf_death == (race == 11));
    assert(strstr(notes_buffer, cause));

    high_score legacy = {0};
    char path[1024];
    SDL_strlcpy(op_ptr->full_name, "Death Fixture", sizeof(op_ptr->full_name));
    metar.id = 777;
    assert(score_runs_record_current_run(&legacy, 12345, SCORE_RECORD_DEAD));
    assert(score_build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME));
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    score_db_header header;
    score_record_v1 record;
    assert(io && SDL_ReadIO(io, &header, sizeof(header)) == sizeof(header));
    assert(header.record_count == 1);
    assert(SDL_ReadIO(io, &record, sizeof(record)) == sizeof(record));
    assert(SDL_CloseIO(io));
    assert(record.killer_kind == kind && record.killer_race_index == race);
    assert(!memcmp(&record.killer_guid, &guid, sizeof(guid)));
    assert(!strcmp(record.cause_of_death, cause));
}

static void acid_trap(void)
{
    p_ptr->chp = 1;
    cave_set_feat(10, 10, FEAT_TRAP_ACID);
    hit_trap(10, 10);
    check_record(SCORE_KILLER_TRAP, 0, "an acid trap");
}

static void flame_chest(void)
{
    chest_trap_minigame = false;
    object_type chest;
    object_prep(&chest, lookup_kind(TV_CHEST, SV_CHEST_SMALL_STEEL));
    chest.pval = 16;
    assert(object_chest_trap_flags(&chest) == CHEST_FLAME);
    int index = floor_carry(10, 10, &chest);
    assert(index > 0);
    p_ptr->chp = 1;
    p_ptr->skill_use[S_PER] = 100;
    do_cmd_open_chest(10, 10, index);
    check_record(SCORE_KILLER_TRAP, 0, "a trapped chest");
}

static void checks(cptr scenario)
{
    bool venom = !strcmp(scenario, "status-chest")
        || !strcmp(scenario, "generic-status") || !strcmp(scenario, "poison");
    int race = venom ? 73 /* Green serpent. */
        : !strcmp(scenario, "fatal-projection") ? 53 /* Red serpent. */ : 11;
    int attacker = prepare_attack(race);
    if (!strcmp(scenario, "bite-trap") || !strcmp(scenario, "miss-trap")) {
        bool hit = !strcmp(scenario, "bite-trap");
        configure_wolf(RBE_HURT, hit);
        assert(make_attack_normal(&mon_list[attacker]));
        assert(!p_ptr->is_dead && (p_ptr->chp < 100) == hit);
        acid_trap();
    } else if (!strcmp(scenario, "status-chest")) {
        assert(project_p(attacker, 10, 10, 4, 2, 0, GF_POIS));
        assert(p_ptr->poisoned && p_ptr->chp == 100);
        flame_chest();
    } else if (!strcmp(scenario, "generic-bite")
        || !strcmp(scenario, "generic-status")
        || !strcmp(scenario, "generic-blocked-flame")) {
        if (!strcmp(scenario, "generic-bite")) {
            configure_wolf(RBE_HURT, true);
            assert(make_attack_normal(&mon_list[attacker]));
            assert(p_ptr->chp < 100 && !p_ptr->is_dead);
        } else if (venom) {
            assert(project_p(attacker, 10, 10, 4, 2, 0, GF_POIS));
            assert(p_ptr->poisoned && p_ptr->chp == 100);
        } else {
            p_ptr->resist_fire = 100;
            killer_mark_other(SCORE_KILLER_TRAP);
            fire_dam_pure(1, 1, false, "a trapped chest");
            assert(p_ptr->chp == 100);
        }
        take_hit(p_ptr->chp, "unmarked damage");
        check_record(SCORE_KILLER_OTHER, 0, "unmarked damage");
    } else if (!strcmp(scenario, "fatal-bite") || !strcmp(scenario, "fatal-fire")) {
        configure_wolf(RBE_HURT, true);
        assert(make_attack_normal(&mon_list[attacker]));
        assert(p_ptr->chp < 100 && !p_ptr->is_dead);
        p_ptr->chp = 1;
        if (!strcmp(scenario, "fatal-fire"))
            configure_wolf(RBE_FIRE, true);
        assert(make_attack_normal(&mon_list[attacker]));
        check_record(SCORE_KILLER_MONSTER, 11, "a Wolf");
    } else if (!strcmp(scenario, "fatal-projection")) {
        char cause[80];
        monster_desc(cause, sizeof(cause), &mon_list[attacker], 0x88);
        p_ptr->chp = 1;
        assert(project_p(attacker, 10, 10, 4, 2, 0, GF_FIRE));
        check_record(SCORE_KILLER_MONSTER, race, cause);
    } else if (!strcmp(scenario, "poison")) {
        assert(project_p(attacker, 10, 10, 4, 2, 0, GF_POIS));
        assert(p_ptr->poisoned && p_ptr->chp == 100);
        p_ptr->chp = 1;
        p_ptr->resting = 1;
        process_player();
        check_record(SCORE_KILLER_OTHER, 0, "poison");
    } else {
        assert(!strcmp(scenario, "knockback"));
        configure_wolf(RBE_HURT, true);
        r_info[11].flags2 |= RF2_KNOCK_BACK;
        p_ptr->depth = p_ptr->max_depth = MORGOTH_DEPTH;
        p_ptr->stat_use[A_CON] = -100;
        cave_set_feat(10, 11, FEAT_CHASM);
        assert(make_attack_normal(&mon_list[attacker]));
        check_record(SCORE_KILLER_FALL, 0, "falling into the abyss");
    }
    printf("%s: actual fatal source, Chronicle text and run record agree.\n", scenario);
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + '    checks(SDL_getenv("DEATH_FIXTURE")); sdl_quit_hook(NULL); return 0;\n}\n',
        encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + path + ".obj" for path in SOURCES))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in SOURCES),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=message_flush", "-Wl,--wrap=sil_achievements_on_wolf_death",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    results = []
    for scenario in ("bite-trap", "miss-trap", "status-chest", "generic-bite",
                     "generic-status", "generic-blocked-flame", "fatal-bite",
                     "fatal-fire", "fatal-projection", "poison", "knockback"):
        with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
            result = subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                env=dict(env, CONTROL_TEST_PROFILE=profile, DEATH_FIXTURE=scenario),
                timeout=20)
            results.append(result)
    for result in results:
        result.check_returncode()


if __name__ == "__main__":
    main()
