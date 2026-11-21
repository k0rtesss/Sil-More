#!/usr/bin/env python3
"""Run local player-system regressions, deliberately excluding monster AI.

Every child uses the dummy SDL drivers. Existing suites own isolated writable
profiles; this runner retains their stdout, timings, exit status and provenance.
Run build-cmake.bat standard before this script.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    "interface": (
        "game_control", "control_json_utf8", "control_screenshot_sharing",
        "qa9_control_response_sharing", "question_shortcuts", "context_stairs",
        "context_tunnel", "help_controller_runtime", "keyboard_aiming",
        "qa9_menu_action_energy", "option_metadata",
        "option_value_visibility", "settings_categories",
    ),
    "creation": (
        "birth_defaults", "birth_monster_reset", "starting_xp_options",
        "desktop_birth_regressions", "oath_prompt", "oath_ability_presentation",
        "qa9_smith_birth_reset",
    ),
    "inventory": (
        "harness_equipment_state", "active_weapon_groups", "offhand_equipment",
        "paired_offhand", "two_weapon_selection", "pack_equipment_readiness",
        "equipment_comparison", "equipment_action_routing", "qa9_equipment_shortcuts",
        "inventory_inscriptions", "pickup_destination_preview", "carried_services",
        "restorative_floor_use", "submerged_items", "underfoot_chests",
        "qa9_items_selector_shortcuts", "qa9_items_gem_cancellation",
        "qa9_items_jewelry_presets", "qa9_items_autoinscriptions",
    ),
    "player_combat": (
        "combat_action_energy", "qa9_combat_weapon_roles", "qa9_combat_polearm_roles",
        "qa9_combat_exchange_redirect", "qa9_combat_peaceful_primary",
        "qa9_combat_weapon_credit_save", "interaction_roll_actor",
        "silmaril_possession", "kinslayer", "truce", "postmortem_identification", "final_look",
        "rest_interruptions", "vitals_and_terrain_memory",
    ),
    "crafting_songs": (
        "smithing_difficulty", "smithing_cost_details", "calc_artefact_difficulty",
        "reforge_work_transaction", "qa9_smith_alloy_save", "fletchery_sources",
        "qa9_craft_song_sources", "aule_craft_completion", "gameplay_review_fixes",
        "qa9_melt_cancel", "qa9_horn_browser_aim", "ability_browser_shortcuts",
        "qa9_browser_shortcuts",
    ),
    "terrain": (
        "terrain_access", "terrain_data", "terrain_generation", "dungeon_terrain_pipeline",
        "terrain_landmarks", "terrain_transitions", "terrain_vaults",
        "bridges", "bridge_underlay", "water", "water_flood", "flood_surface_save",
        "ice", "ice_combat", "melting_ice", "lava", "lava_generation",
        "poison_terrain", "illusory_walls", "quartz_walls", "forge_generation",
        "level_transition_turns", "teleport_bounds", "vault_depth_eligibility",
    ),
    "quests_lore": (
        "thrall_interaction", "quest_book_context", "quest_formula_bounds",
        "qa9_delvings_discovery", "qa9_trees_light", "qa9_varda_reward_defer",
        "minimum_quest_dialogue", "minimum_quest_monster", "minimum_thrall",
        "minimum_thrall_journal", "minimum_tulkas_reward", "minimum_varda_reward",
        "generic_lore", "disease", "disease_knowledge", "skeleton_hint_areas",
        "unfinished_tale", "utumno_state", "utumno_travel", "utumno_generation",
        "qa9_crown_cancel", "qa9_narrative_modal_priority",
    ),
    "persistence": (
        "save_flush", "save_notes", "message_history_save", "song_target_save",
        "disease_save", "legacy_quiver_load",
        "persistence_review_fixes", "remaining_review_paths", "run_history_timestamps",
        "blessing_knowledge",
        "template_versions", "artefact_databases", "character_dump_layout",
    ),
    "tutorials": (
        "gameplay_tutorial", "gameplay_tutorial_integration", "gameplay_tutorial_sdl",
        "tutorial_archive", "tutorial_catalogue", "tutorial_start", "tutorial_upgrade",
        "tutorial_view", "tutorial_world", "options_tutorial",
        "minimum_text", "minimum_quest_dialogue",
    ),
    "achievements": ("achievement_playtest",),
    "profiles_audio": ("audio_review_fixes", "progress_reset", "cloud_saves", "cloud_sync", "cloud_profile_paths"),
}

MIXED_MONSTER_CHECKS = {
    "rest_interruptions": "Includes monster alarms, recognition and scripted movement; outside player-only scope",
    "water": "Includes monster movement and energy rules; use the live player water scenarios in this scope",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "scripts/output/player-systems-audit")
    parser.add_argument("--groups", nargs="+", choices=GROUPS, default=list(GROUPS))
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--only", nargs="+", help="Run named check scripts from the selected groups")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    log_dir = args.output / "regressions"
    log_dir.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
    seen: set[str] = set()
    jobs = []
    for group in args.groups:
        for name in GROUPS[group]:
            if name not in seen and name not in MIXED_MONSTER_CHECKS:
                jobs.append((group, name))
                seen.add(name)
    if args.only:
        unavailable = set(args.only)-{name for _, name in jobs}
        if unavailable:
            parser.error("Unavailable or excluded from player-only scope: " + ", ".join(sorted(unavailable)))
        jobs = [(group,name) for group,name in jobs if name in args.only]
    receipt = {
        "started_at": datetime.now(timezone.utc).isoformat(),
        "monster_ai": "excluded by user request",
        "excluded": {
            "monster_ai_suites": "User requested separate monster AI testing",
            "tutorial_visibility": "Monster-state preview fixture belongs to separate monster work",
            "living_dungeon_save": "Mixed environment/monster-job fixture; its synthetic legacy monster stream is stale",
            "normal_ui_compat": "Pinned historical UI no longer links against current frontend APIs; current UI is exercised separately",
            **MIXED_MONSTER_CHECKS,
        },
        "build_sha256": hashlib.sha256((ROOT / "build-standard/sil-more.exe").read_bytes()).hexdigest(),
        "tests": [],
    }
    for index, (group, name) in enumerate(jobs, 1):
        source = ROOT / "scripts" / f"check_{name}.py"
        command = [sys.executable, str(source)]
        if name == "achievement_playtest":
            command.append("--all")
        if name == "dungeon_terrain_pipeline":
            command.extend(["--output", str(args.output / "terrain-pipeline")])
        if name == "terrain_landmarks":
            command.extend(["--input", str(args.output / "terrain-pipeline")])
        started = time.monotonic()
        log = log_dir / f"{index:03d}-{name}.log"
        with log.open("w", encoding="utf-8") as out:
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                        stderr=subprocess.STDOUT, timeout=args.timeout,
                                        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                code = result.returncode
                error = None
            except subprocess.TimeoutExpired:
                code, error = -1, f"timeout after {args.timeout}s"
        test = dict(group=group, name=name, exit_code=code, error=error,
                    seconds=round(time.monotonic()-started, 3), log=str(log),
                    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        receipt["tests"].append(test)
        receipt["passed"] = sum(t["exit_code"] == 0 for t in receipt["tests"])
        receipt["failed"] = len(receipt["tests"])-receipt["passed"]
        (args.output / "regression-summary.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
        print(f"{index}/{len(jobs)} {'PASS' if code == 0 else 'FAIL'} {group}/{name} ({test['seconds']}s)", flush=True)
    receipt["finished_at"] = datetime.now(timezone.utc).isoformat()
    (args.output / "regression-summary.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    return int(bool(receipt["failed"]))


if __name__ == "__main__":
    raise SystemExit(main())
