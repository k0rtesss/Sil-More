#!/usr/bin/env python3
"""Play deterministic achievement scenarios through the production engine.

Uses disposable profiles and current Windows build objects. Combat, damage,
singing, smithing, Silmaril cutting and escape run their real implementations;
only presentation, confirmations and hit rolls use the existing fixture doubles.
The discovery, expansion and character suites cover the other catalogue entries.
"""
from pathlib import Path
import argparse
import hashlib
import importlib
import json
import os
import re
import shlex
import subprocess
import tempfile
from unittest.mock import patch

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_achievement_expansion import CHECKS as BASE, HELPERS
from check_achievement_events import SMITH
from check_achievement_characters import ITEMS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-playtest"

TESTS = r'''
void fixture_prepare_smithing(bool, bool);
bool fixture_cut(void);
static monster_type* ready(int race)
{
    monster_type* m = prepare(race);
    p_ptr->pcharacter = 0; current_character_profile = &c_info[0];
    p_ptr->chp = p_ptr->mhp = 1000;
    p_ptr->csp = p_ptr->msp = 500;
    p_ptr->energy_use = 100;
    p_ptr->leaving = p_ptr->is_dead = false;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    sil_achievements_begin_run();
    return m;
}
static void choose_character(u32b flag)
{
    for (int i = 0; i < z_info->c_max; ++i)
        if (c_info[i].flags_u & flag) {
            p_ptr->pcharacter = i; current_character_profile = &c_info[i]; return;
        }
    assert(false);
}
static void crown(void)
{
    int idx = o_pop(); assert(idx > 0);
    object_type* o = &o_list[idx];
    object_prep(o, lookup_kind(a_info[ART_MORGOTH_3].tval, a_info[ART_MORGOTH_3].sval));
    o->name1 = ART_MORGOTH_3; o->number = 1;
    o->iy = p_ptr->py; o->ix = p_ptr->px;
    cave_o_idx[p_ptr->py][p_ptr->px] = idx;
}
static void check_case(void)
{
    assert(init_flavor_info() == 0); flavor_init();
    ANGBAND_DIR_APEX = ANGBAND_DIR_USER;
    sil_achievements_init();
    const char* name = getenv("SIL_ACH_PLAYTEST_CASE"); assert(name);
    monster_type* m = ready(41);
    if (!strcmp(name, "depth")) {
        sil_achievements_on_depth(1); assert(!sil_achievements_mask());
        sil_achievements_on_depth(2); assert(earned(SIL_ACH_THE_ROAD_GOES_EVER_ON));
        assert(!earned(SIL_ACH_INTO_THE_DEEP));
        sil_achievements_on_depth(10); assert(earned(SIL_ACH_INTO_THE_DEEP));
    } else if (!strcmp(name, "comeback")) {
        take_hit(998, "fixture hit");
        assert(p_ptr->chp == 2 && !(p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK));
        ready(41); take_hit(999, "fixture hit");
        assert(p_ptr->chp == 1 && (p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK));
        assert(mon_take_hit(cave_m_idx[10][10], 100, NULL, -1));
        assert(earned(SIL_ACH_BY_A_THREAD));
    } else if (!strcmp(name, "light")) {
        int race = 0;
        for (int i = 1; i < z_info->r_max; ++i)
            if ((r_info[i].flags3 & (RF3_UNDEAD | RF3_HURT_LITE))
                == (RF3_UNDEAD | RF3_HURT_LITE)) { race = i; break; }
        assert(race); ready(race);
        cave_light[10][10] = 20;
        project_m(-1, 10, 10, 20, 20, 1000, GF_LIGHT, PROJECT_SILENT);
        assert(!cave_m_idx[10][10] && earned(SIL_ACH_LIGHT_OF_THE_ELDAR));
    } else if (!strcmp(name, "smithing")) {
        fixture_prepare_smithing(false, false); create_smithing_item();
        assert(earned(SIL_ACH_FORGED_BY_YOUR_HAND));
        assert(!earned(SIL_ACH_A_NAME_WORTH_REMEMBERING));
        fixture_prepare_smithing(true, false); create_smithing_item();
        assert(earned(SIL_ACH_A_NAME_WORTH_REMEMBERING));
    } else if (!strcmp(name, "silmaril")) {
        item(&inventory[INVEN_WIELD], TV_SWORD, SV_SHORT_SWORD, 1);
        p_ptr->equip_cnt = 1; p_ptr->mdd = 20; p_ptr->mds = 20;
        crown(); assert(fixture_cut());
        assert(earned(SIL_ACH_LIGHT_IN_DARKNESS));
        assert(silmarils_possessed() == 1);
    } else if (!strcmp(name, "escape")) {
        p_ptr->oath_type = OATH_IRON; assert(!oath_invalid(OATH_IRON));
        p_ptr->morgoth_hall_entered = true;
        p_ptr->skill_base[S_SNG] = 20;
        p_ptr->song1 = SNG_ELBERETH; ability(S_SNG, SNG_ELBERETH);
        for (int i = 0; i < 100; ++i) sing();
        assert(p_ptr->achievement_singing_turns == 100);
        item(&inventory[0], TV_LIGHT, SV_LIGHT_SILMARIL, 3);
        assert(silmarils_possessed() == 3);
        p_ptr->depth = 0; do_cmd_escape(silmarils_possessed());
        assert(p_ptr->escaped && p_ptr->is_dead && !p_ptr->playing);
        assert(earned(SIL_ACH_BACK_TO_STARLIGHT) && earned(SIL_ACH_THREE_LIGHTS_IN_EXILE));
        assert(earned(SIL_ACH_THE_QUIET_ROAD) && earned(SIL_ACH_WORD_UNBROKEN));
        assert(earned(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN) && earned(SIL_ACH_THERE_AND_BACK_AGAIN));
    } else if (!strcmp(name, "victory")) {
        m = ready(R_IDX_MORGOTH); int idx = (int)(m - mon_list);
        assert(!mon_take_hit(idx, m->hp, NULL, -1));
        assert(p_ptr->morgoth_second_wind && !earned(SIL_ACH_THE_SHADOW_FALLS));
        assert(mon_take_hit(idx, m->hp, NULL, -1));
        assert(p_ptr->morgoth_slain && p_ptr->is_dead && !p_ptr->playing);
        assert(earned(SIL_ACH_THE_SHADOW_FALLS));
    } else if (!strcmp(name, "tale")) {
        metar.id = 76425; p_ptr->is_dead = true; p_ptr->playing = false;
        p_ptr->pcharacter = 1; sil_achievements_on_run_completed();
        sil_achievements_on_run_completed(); assert(!earned(SIL_ACH_THE_LONG_TALE));
        p_ptr->pcharacter = 2; sil_achievements_on_run_completed();
        sil_achievements_on_tale_completed(); assert(earned(SIL_ACH_THE_LONG_TALE));
    } else if (!strcmp(name, "singing-payment")) {
        p_ptr->song1 = SNG_MASTERY; ability(S_SNG, SNG_MASTERY);
        p_ptr->csp = 1; sing();
        assert(p_ptr->csp == 0 && p_ptr->achievement_singing_turns == 0);
        p_ptr->csp = 2; sing();
        assert(p_ptr->csp == 0 && p_ptr->achievement_singing_turns == 1);
        p_ptr->csp = 1; p_ptr->song1 = SNG_SILENCE;
        ability(S_SNG, SNG_SILENCE); p_ptr->song_duration = 0; sing();
        assert(p_ptr->csp == 1 && p_ptr->achievement_singing_turns == 2);
        p_ptr->song1 = SNG_MASTERY; p_ptr->csp = 1;
        p_ptr->achievement_singing_turns = 99; sing();
        assert(p_ptr->achievement_singing_turns == 99);
        p_ptr->skill_base[S_SNG] = 20; p_ptr->depth = 0; do_cmd_escape(1);
        assert(!earned(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN));
    } else if (!strcmp(name, "victory-riposte")) {
        m = ready(R_IDX_MORGOTH); p_ptr->morgoth_second_wind = true;
        choose_character(UNQ_MEL_MAEDHROS);
        item(&inventory[INVEN_WIELD], TV_SWORD, SV_BASTARD_SWORD, 1);
        ability(S_MEL, MEL_CONTROL); p_ptr->equip_cnt = 1;
        p_ptr->mdd = 20; p_ptr->mds = 20;
        py_attack_aux(10, 10, ATT_RIPOSTE);
        assert(p_ptr->morgoth_slain && p_ptr->is_dead && !p_ptr->playing);
        assert(earned(SIL_ACH_THE_SHADOW_FALLS));
        assert(earned(SIL_ACH_THE_HASTY_STROKE));
        assert(earned(SIL_ACH_ON_THE_OTHER_HAND));
    } else if (!strcmp(name, "victory-trap")) {
        ready(R_IDX_MORGOTH); p_ptr->morgoth_second_wind = true;
        cave_feat[10][10] = FEAT_TRAP_DEADFALL; cave_rewired[10][10] = 100;
        m_engage_rewired_trap(10, 10);
        assert(p_ptr->morgoth_slain && earned(SIL_ACH_OFT_EVIL_WILL));
    } else if (!strcmp(name, "victory-potion")) {
        ready(R_IDX_MORGOTH); p_ptr->morgoth_second_wind = true;
        object_type potion; item(&potion, TV_POTION, SV_POTION_ORCISH_LIQUOR, 1);
        assert(fixture_potion(&potion));
        assert(p_ptr->morgoth_slain && earned(SIL_ACH_FOR_EXTERNAL_USE_ONLY));
    } else if (!strcmp(name, "victory-hail")) {
        m = ready(R_IDX_MORGOTH); p_ptr->morgoth_second_wind = true;
        item(&inventory[INVEN_BOW], TV_BOW, SV_SHORT_BOW, 1);
        object_type arrow; item(&arrow, TV_ARROW, SV_NORMAL_ARROW, 5);
        assert(player_quiver_absorb_arrow(&arrow) == 5);
        p_ptr->equip_cnt = 1; p_ptr->ammo_tval = TV_ARROW; p_ptr->add = 10; p_ptr->ads = 20;
        p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
        ability(S_ARC, ARC_DEADLY_HAIL); p_ptr->killed_enemy_with_arrow = true;
        target_set_monster((int)(m - mon_list)); do_cmd_fire(1);
        assert(p_ptr->morgoth_slain && earned(SIL_ACH_A_FEW_LEAVES));
    } else if (!strcmp(name, "victory-power-throw")) {
        m = ready(R_IDX_MORGOTH); p_ptr->morgoth_second_wind = true;
        ability(S_MEL, MEL_THROWING); ability(S_MEL, MEL_POWER_THROW);
        item(&inventory[INVEN_WIELD], TV_SWORD, SV_SHORT_SWORD, 1); p_ptr->equip_cnt = 1;
        object_type spear; item(&spear, TV_POLEARM, SV_SPEAR, 3);
        assert(player_carried_extra_load(&spear));
        p_ptr->previous_action[1] = 5; p_ptr->focused = false;
        p_ptr->mdd = 10; p_ptr->mds = 20;
        target_set_monster((int)(m - mon_list)); player_active_weapon_begin_player_turn();
        do_cmd_throw(false);
        assert(roll_number == 2 && p_ptr->morgoth_slain && earned(SIL_ACH_TWO_WEAPONS));
    } else if (!strcmp(name, "terminal-exclusions")) {
        p_ptr->is_dead = true; p_ptr->playing = false; p_ptr->leaving = true;
        sil_achievements_on_event(SIL_ACH_THE_HASTY_STROKE);
        assert(!earned(SIL_ACH_THE_HASTY_STROKE));
        p_ptr->morgoth_slain = true; p_ptr->wizard = true;
        sil_achievements_on_event(SIL_ACH_THE_HASTY_STROKE);
        assert(!earned(SIL_ACH_THE_HASTY_STROKE));
        p_ptr->wizard = false; p_ptr->escaped = true;
        sil_achievements_on_event(SIL_ACH_THE_HASTY_STROKE);
        assert(!earned(SIL_ACH_THE_HASTY_STROKE));
        p_ptr->escaped = false;
        sil_achievements_on_event(SIL_ACH_THE_FORDS_OF_SIRION);
        assert(!earned(SIL_ACH_THE_FORDS_OF_SIRION));
    } else assert(false);
    sil_achievements_flush(); assert(sil_achievements_saved());
    printf("PASS achievement playtest scenario: %s; earned mask %llu\n",
        name, (unsigned long long)sil_achievements_mask());
}
'''


def check_scenarios():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "playtest.c"
    source.write_text(prefix + BASE[:BASE.index("static void check_thrall(void)")]
                      + TESTS + init + "    check_case(); SDL_Quit(); return 0;\n}\n",
                      encoding="utf-8")
    helpers = []
    for name, content in (("smith", SMITH), ("items", ITEMS), ("ranged", HELPERS["ranged"])):
        path = OUT / ("playtest-" + name + ".c")
        path.write_text(content, encoding="utf-8")
        helpers.append(str(path))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj",
                "/src/cmd/item/cmd-item-core.c.obj", "/src/cmd/combat/cmd-ranged.c.obj")
    response = OUT / "playtest-objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print",
             "message_flush", "get_sdl_config_path", "pause_with_text",
             "quest_typewriter_menu_pages", "ui_question_ask", "get_aim_dir",
             "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details")
    exe = OUT / "playtest.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *helpers,
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *("-Wl,--wrap=" + name for name in wraps), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    results = []
    cases = ("depth", "comeback", "light", "smithing", "silmaril", "escape", "victory", "tale",
             "singing-payment", "victory-riposte", "victory-trap", "victory-potion",
             "victory-hail", "victory-power-throw", "terminal-exclusions")
    requested = os.environ.get("SIL_ACH_PLAYTEST_ONLY")
    if requested:
        cases = tuple(name for name in cases if name in requested.split(","))
        assert cases, requested
    for case in cases:
        env["SIL_ACH_PLAYTEST_CASE"] = case
        with tempfile.TemporaryDirectory(prefix=case + "-", dir=OUT) as data:
            run = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
                                 env=env, timeout=30, capture_output=True, text=True)
            print(run.stdout, end="", flush=True)
            if run.stderr:
                print(run.stderr, end="", flush=True)
            profile = Path(data) / "achievements.json"
            progress = json.loads(profile.read_text()) if profile.exists() else {}
            results.append(dict(case=case, exit_code=run.returncode, progress=progress))
    (OUT / "engine-scenarios.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    assert all(row["exit_code"] == 0 for row in results), "Failed production scenarios"


def check_catalogue():
    """Retain unlock evidence before the existing production fixtures clean up."""
    enum = (ROOT / "src/achievements.h").read_text().split("typedef enum sil_achievement_id {", 1)[1]
    keys = re.findall(r"\bSIL_ACH_([A-Z][A-Z0-9_]*)(?:\s*=\s*\d+)?\s*,",
                      enum.split("} sil_achievement_id;", 1)[0])
    evidence = {key: [] for key in keys}
    original_directory = tempfile.TemporaryDirectory
    active_suite = ""

    class RecordedDirectory(original_directory):
        def __exit__(self, *args):
            if args[0] is None:
                for profile in Path(self.name).rglob("achievements.json"):
                    progress = json.loads(profile.read_text(encoding="utf-8"))
                    for key, timestamp in progress.get("unlocked", {}).items():
                        if key in evidence and timestamp and active_suite not in evidence[key]:
                            evidence[key].append(active_suite)
            return super().__exit__(*args)

    production = ("check_achievement_playtest", "check_achievement_expansion",
                  "check_achievement_discoveries", "check_achievement_characters")
    for active_suite in production:
        print("Running production suite:", active_suite, flush=True)
        with patch.object(tempfile, "TemporaryDirectory", RecordedDirectory):
            if active_suite == "check_achievement_playtest":
                check_scenarios()
            else:
                importlib.import_module(active_suite).main()
    supporting = ("check_achievements", "check_achievement_events", "check_achievement_save",
                  "check_achievement_steam", "check_achievement_viewer",
                  "check_android_achievement_catalogue")
    for name in supporting:
        print("Running supporting suite:", name, flush=True)
        module = importlib.import_module(name)
        if name == "check_android_achievement_catalogue":
            assert module.catalogue_keys() == keys
            print("PASS Android provider catalogue matches all native achievement indexes")
        else:
            module.main()
    missing = [key for key, suites in evidence.items() if not suites]
    sources = [ROOT / "src" / name for name in (
        "achievements.c", "achievements.h", "achievements-discovery.c",
        "achievements-character.c", "player/player-songs.c")]
    receipt = dict(total=len(keys), covered=len(keys) - len(missing), missing=missing,
                   production_suites=list(production), supporting_suites=list(supporting),
                   source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                  for path in sources},
                   achievements=[dict(index=index, key=key, suites=evidence[key])
                                 for index, key in enumerate(keys)])
    (OUT / "catalogue-coverage.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    assert not missing, "No production unlock evidence for: " + ", ".join(missing)
    print(f"PASS complete catalogue: {len(keys)}/{len(keys)} achievements with production unlock evidence")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true", help="Run every achievement suite and verify catalogue coverage")
    args = parser.parse_args()
    if args.all:
        check_catalogue()
    else:
        check_scenarios()


if __name__ == "__main__":
    main()
