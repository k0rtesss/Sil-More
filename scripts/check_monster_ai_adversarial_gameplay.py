#!/usr/bin/env python3
"""Headless encounters covering direction changes, handoffs and active songs."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_coordination_gameplay as coordination

engine = coordination.engine
play = engine.play
ROOT = engine.ROOT
OUT = ROOT / "scripts/output/monster-ai-round4"
troop = coordination.troop


def instrument():
    engine.OUT = OUT
    engine.instrument()
    play.FIXTURE = play.FIXTURE.replace(
        'extern bool current_build_vault_exact_token;',
        'extern bool current_build_vault_exact_token;\n'
        'static bool fixture_recording_terrain;')
    play.FIXTURE = play.FIXTURE.replace(
        'if (waiting && character_dungeon && p_ptr) {',
        'if (waiting && character_dungeon && p_ptr) {\n'
        '        fixture_recording_terrain = true;', 1)
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(o, "plan_age", m->squad.plan_age);',
        'cJSON_AddNumberToObject(o, "plan_age", m->squad.plan_age);\n'
        '            cJSON_AddNumberToObject(o, "plan_turn", m->squad.plan_turn);\n'
        '            cJSON_AddNumberToObject(o, "social_group", m->social_group);\n'
        '            cJSON_AddBoolToObject(o, "fooled", song_disguise_monster_is_fooled(m));\n'
        '            cJSON_AddBoolToObject(o, "los_player", los(m->fy, m->fx, p_ptr->py, p_ptr->px));\n'
        '            cJSON_AddNumberToObject(o, "target_y", m->squad.target_y);\n'
        '            cJSON_AddNumberToObject(o, "target_x", m->squad.target_x);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(player, "song", p_ptr->song1);',
        'cJSON_AddNumberToObject(player, "song", p_ptr->song1);\n'
        '        cJSON_AddNumberToObject(player, "song2", p_ptr->song2);\n'
        '        cJSON_AddNumberToObject(player, "voice", p_ptr->csp);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(player, "evasion", p_ptr->skill_use[S_EVN]);',
        'cJSON_AddNumberToObject(player, "evasion", p_ptr->skill_use[S_EVN]);\n'
        '        cJSON_AddNumberToObject(player, "constitution", p_ptr->stat_use[A_CON]);\n'
        '        cJSON_AddBoolToObject(player, "stand_fast", p_ptr->stand_fast);')
    # UI and terrain observations have no effect on the game or its RNG.
    anchor = '        cJSON* player = cJSON_AddObjectToObject(root, "player");'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, r'''
        cJSON* feature_values = cJSON_AddArrayToObject(root, "feature_probes");
        const char* probe_text = SDL_getenv("AI_FEATURE_PROBES");
        cJSON* requested = probe_text ? cJSON_Parse(probe_text) : NULL;
        cJSON* probe;
        cJSON_ArrayForEach(probe, requested) {
            int y = number(probe, "y", -1), x = number(probe, "x", -1);
            assert(in_bounds(y, x));
            cJSON_AddItemToArray(feature_values, cJSON_CreateNumber(cave_feat[y][x]));
        }
        cJSON_Delete(requested);
        cJSON* player = cJSON_AddObjectToObject(root, "player");''')
    tail = r'\"projection\":%d}\n",'
    assert tail in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(
        tail, r'\"projection\":%d,\"player_y\":%d,\"player_x\":%d,\"song\":%d,\"valid_order\":%d,\"target_y\":%d,\"target_x\":%d}\n",')
    play.FIXTURE = play.FIXTURE.replace(
        'projectable(m->fy, m->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK));',
        'projectable(m->fy, m->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK),\n'
        '        p_ptr->py, p_ptr->px, p_ptr->song1, monster_squad_order_valid(m),\n'
        '        m->squad.target_y, m->squad.target_x);', 1)
    play.FIXTURE += r'''
void __real_cave_set_feat(int, int, int);
void __wrap_cave_set_feat(int y, int x, int feature)
{
    int before = fixture_recording_terrain && character_dungeon && p_ptr
        && in_bounds(y, x) ? cave_feat[y][x] : -1;
    __real_cave_set_feat(y, x, feature);
    if (before < 0 || before == cave_feat[y][x]) return;
    const char* text = SDL_getenv("AI_FEATURE_PROBES");
    cJSON* requested = text ? cJSON_Parse(text) : NULL;
    cJSON* probe;
    bool wanted = false;
    cJSON_ArrayForEach(probe, requested)
        if (number(probe, "y", -1) == y && number(probe, "x", -1) == x)
            wanted = true;
    cJSON_Delete(requested);
    if (!wanted) return;
    char path[1400];
    strnfmt(path, sizeof(path), "%s/terrain-events.jsonl", SDL_getenv("CONTROL_TEST_PROFILE"));
    FILE* file = fopen(path, "ab"); assert(file);
    fprintf(file, "{\"turn\":%ld,\"player_turn\":%ld,\"y\":%d,\"x\":%d,\"before\":%d,\"after\":%d}\n",
        (long)turn, (long)playerturn, y, x, before, cave_feat[y][x]);
    assert(fclose(file) == 0);
}
'''


def build():
    return play.build(extra_wraps=("cave_set_feat",))


def point(y, x, turns):
    dy, dx = y - 20, x - 40
    for _ in range(turns):
        dy, dx = dx, -dy
    return 20 + dy, 40 + dx


def rotate(spec, keys, turns):
    result = copy.deepcopy(spec)
    for m in result["monsters"]:
        m["y"], m["x"] = point(m.get("y", 20), m["x"], turns)
    result["player_y"], result["player_x"] = point(
        result.get("player_y", 20), result.get("player_x", 40), turns)
    for tile in result.get("tiles", []):
        if (tile.get("y1"), tile.get("y2"), tile.get("x1"), tile.get("x2")) == (1, 38, 1, 62):
            continue  # The rectangular map's solid background stays solid.
        corners = [point(y, x, turns)
                   for y in (tile["y1"], tile.get("y2", tile["y1"]))
                   for x in (tile["x1"], tile.get("x2", tile["x1"]))]
        tile.update(y1=min(y for y, x in corners), y2=max(y for y, x in corners),
                    x1=min(x for y, x in corners), x2=max(x for y, x in corners))
        if turns % 2 and 88 <= tile["feat"] <= 99:
            tile["feat"] ^= 1  # Rotate authored horizontal/vertical bridges.
    for probe in result.get("feature_probes", []):
        probe["y"], probe["x"] = point(probe["y"], probe["x"], turns)
    directions = {
        "KP1": (1, -1), "KP2": (1, 0), "KP3": (1, 1), "KP4": (0, -1),
        "KP6": (0, 1), "KP7": (-1, -1), "KP8": (-1, 0), "KP9": (-1, 1)}
    reverse = {value: key for key, value in directions.items()}
    rotated = []
    for key in keys:
        if key in directions:
            y, x = directions[key]
            yy, xx = point(20 + y, 40 + x, turns)
            key = reverse[yy - 20, xx - 40]
        rotated.append(key)
    return result, rotated


def cases():
    result = []
    def add(name, spec, count=36, keys=None, check=None):
        scene = dict(evasion=40)
        scene.update(copy.deepcopy(spec))
        result.append((name, scene, list(keys) if keys is not None else ["KP5"]*count,
                       check or {}))

    for start, energy in [(37, -100), (37, -200), (37, -300),
                          (36, -200), (36, -300), (35, -300)]:
        add(f"handoff-poison-{start}-{abs(energy)}", dict(monsters=[
            troop(91, start, hp_percent=40, morale=1000),
            troop(41, start-1, 19, hp_percent=40, morale=1000),
            troop(61, start-1, 21, hp_percent=40, morale=1000),
            troop(51, start-2, 22, morale=1000),
            troop(76, 36, 18, poison=100, confused=1, energy=energy, morale=1000)]),
            count=28, check=dict(budget=True, shots="any"))

    wall = [dict(y1=1, y2=38, x1=39, feat=63), dict(y1=20, x1=39, feat=32)]
    for archer in (175, 51):
        add(f"song-disguise-compaction-{archer}",
            dict(song_skill=60, will=60, abilities=[dict(skill=7, ability=13)],
                 tiles=wall, monsters=[
                     troop(41, 37, hp_percent=1, poison=10, energy=-300, morale=1000),
                     troop(archer, 36, 21, morale=1000)]),
            keys=["s", "n", "o"]+["KP5"]*12,
            check=dict(solo=True, disguise=True, compact=True))

    for song, letter in [(1, "b"), (14, "o"), (16, "q")]:
        add(f"song-switch-and-stop-{song}", dict(
            song_skill=25, abilities=[dict(skill=7, ability=song)],
            monsters=[troop(409, 37), troop(82, 36, 18),
                      troop(103, 35, 19), troop(103, 35, 21)]),
            keys=["s", letter]+["KP5"]*6+["s", "s"]+["KP6"]*3+["KP5"]*22,
            check=(dict(song_cycle=song, solo=True, sleep_song=True)
                   if song == 14 else dict(song_cycle=song, shots="any")))
    add("song-disguise-to-challenge",
        dict(song_skill=60, will=60, abilities=[dict(skill=7, ability=13),
                                              dict(skill=7, ability=1)],
             tiles=wall, monsters=[troop(91, 37, morale=500),
                                   troop(51, 36, 21, morale=500)]),
        keys=["s", "n", "o"]+["KP5"]*4+["s", "b"]+["KP5"]*18,
        check=dict(solo=True, disguise=True, challenge_transition=True))

    original = {name: (spec, keys, check)
                for name, spec, keys, check in coordination.varied_cases()}
    for name in ("varied-uldor-ranged-command", "varied-cats-command-165",
                 "varied-giants-boulder-line", "varied-eighteen-orc-pincer"):
        spec, keys, check = original[name]
        for quarter in (1, 2, 3):
            rotated, moves = rotate(spec, keys, quarter)
            add(f"rotated-{quarter}-{name}", rotated, keys=moves, check=check)
    for family in ("orc", "cat"):
        spec, keys, check = original[f"terrain-{family}-two-door-rooms"]
        for quarter in (1, 2, 3):
            scene = dict(spec, feature_probes=[dict(y=20, x=35), dict(y=20, x=37)])
            rotated, moves = rotate(scene, keys, quarter)
            add(f"rotated-{quarter}-{family}-two-doors", rotated, keys=moves,
                check=dict(check, door_probes=True))
    for family, quarter in (("orc", 1), ("orc", 3), ("cat", 2)):
        spec, keys, check = original[f"terrain-{family}-pillared-loop"]
        rotated, moves = rotate(spec, keys, quarter)
        add(f"rotated-{quarter}-{family}-pillar-loop", rotated, keys=moves, check=check)
    for family, quarter in (("orc", 1), ("cat", 3)):
        spec, keys, check = original[f"terrain-{family}-deep-water-bridge"]
        rotated, moves = rotate(spec, keys, quarter)
        add(f"rotated-{quarter}-{family}-deep-bridge", rotated, keys=moves, check=check)

    for quarter in (0, 1, 2, 3):
        spec = dict(tiles=[dict(y1=20, x1=36, feat=80)], monsters=[
            troop(91, 37, hp_percent=30, morale=-300, stance=1, energy=200),
            troop(81, 36, 19, morale=1000), troop(61, 36, 21, morale=1000),
            troop(51, 34, 18, morale=1000), troop(51, 34, 22, morale=1000)])
        rotated, moves = rotate(spec, ["KP5"]*36, quarter)
        add(f"stairs-succession-{quarter}", rotated, keys=moves,
            check=dict(stair_escape=True, shots="any"))

    for quarter in (0, 2):
        spec = dict(recognition=0, tiles=[
                        dict(y1=1,y2=38,x1=1,x2=62,feat=63),
                        dict(y1=20,x1=25,x2=40,feat=1),
                        dict(y1=20,x1=39,feat=4)],
                    feature_probes=[dict(y=20, x=39)],
                    monsters=[troop(115, 32), troop(103, 34), troop(103, 35)])
        rotated,moves=rotate(spec,(["c"]+["KP5"]*5)*5+["KP5"]*20,quarter)
        add(f"repeated-door-closures-{quarter}", rotated, keys=moves,
            check=dict(cycling_door=True, shots="any"))
    for quarter in (0, 2):
        spec = dict(evasion=25, constitution=3, allow_death=True, monsters=[
            troop(51, 37, 19), troop(51, 37, 21),
            troop(214, 39), troop(241, 38, 18)])
        rotated, moves = rotate(spec, ["KP5"]*18, quarter)
        add(f"knockback-invalidates-orders-{quarter}", rotated, keys=moves,
            check=dict(knockback=True, melee=True))
    return result


def verify(result, check):
    coordination.verify_varied(result, check)
    frames, events = engine.read_run(result)
    for f in frames:
        for m in f["monsters"]:
            if m["order_valid"]:
                assert (m["target_y"], m["target_x"]) == (f["player"]["y"], f["player"]["x"])
                assert not m["fooled"]
    for e in events:
        if e["valid_order"]:
            assert (e["target_y"], e["target_x"]) == (e["player_y"], e["player_x"])
    if check.get("budget"):
        primary = frames[0]["monsters"][0]["actor_id"]
        death = next(i for i, f in enumerate(frames)
                     if not any(m["race"] == 76 for m in f["monsters"]))
        before = next(m for m in frames[death-1]["monsters"] if m["actor_id"] == primary)
        after = next(m for m in frames[death]["monsters"] if m["actor_id"] == primary)
        if before["budget_plan"] == 2 and before["hp"]*2 <= before["max_hp"]:
            assert after["budget_plan"] == 2, "Commander death erased a survivor's personal budget"
            assert after["plan_age"] >= before["plan_age"]
        assert not any(m["actor_id"] == primary and m["role"] == 1 and m["plan"] == 2
                       for f in frames[death+1:] for m in f["monsters"]), (
            "A survivor renews regrouping after the temporary commander dies")
    if check.get("disguise"):
        active = [f for f in frames if f["player"]["song"] == 13]
        assert active, "Disguise never starts"
        # Songs apply on the player's action cadence. A creature that just
        # walked into recognition range is eligible on the next song tick;
        # that boundary is distinct from losing an already applied effect.
        first_seen, exposed = {}, []
        for frame in active:
            visible = set()
            for m in frame["monsters"]:
                if not m["sees_player"] or m["alertness"] < 0:
                    continue
                identity = m["actor_id"]
                visible.add(identity)
                since = first_seen.setdefault(identity, frame["player_turn"])
                if frame["player_turn"] > since:
                    exposed.append(m)
            first_seen = {identity: since for identity, since in first_seen.items()
                          if identity in visible}
        assert exposed and all(m["fooled"] for m in exposed), "Compaction loses Disguise pacification"
        assert not any(e["kind"] == "ranged" and e["song"] == 13
                       and e["detail"] in (96,97,98) for e in events)
    if check.get("compact"):
        identity = frames[0]["monsters"][-1]["actor_id"]
        indices = {m["index"] for f in frames for m in f["monsters"]
                   if m["actor_id"] == identity}
        assert len(indices) > 1, "No live array compaction"
    if check.get("song_cycle"):
        assert any(f["player"]["song"] == check["song_cycle"] for f in frames)
        assert frames[-1]["player"]["song"] == 100
    if check.get("sleep_song"):
        assert any(m["alertness"] < -10 for f in frames if f["player"]["song"] == 14
                   for m in f["monsters"]), "Lorien never puts a monster to sleep"
    if check.get("challenge_transition"):
        assert any(f["player"]["song"] == 1 for f in frames)
        archer_melee = any(e["kind"] == "melee" and e["race"] == 51 for e in events)
        # This scene has one door at (20,39), with the player at (20,40).
        # Challenge must advance the bow to that bottleneck; it cannot walk
        # through its captain while the captain occupies the sole contact tile.
        waiting_at_choke = any(f["player"]["song"] == 1
            and any(m["race"] == 51 and (m["y"], m["x"]) == (20,38)
                    and m["min_range"] <= 1 for m in f["monsters"])
            and any(m["race"] == 91 and (m["y"], m["x"]) == (20,39)
                    for m in f["monsters"]) for f in frames)
        assert archer_melee or waiting_at_choke, "Challenged archer never approaches melee contact"
        assert not any(e["kind"] == "ranged" and e["race"] == 51 and e["song"] == 1 for e in events)
    if check.get("door_probes"):
        assert all(len(f["feature_probes"]) == 2 for f in frames)
        assert any(all(feature in (4,5) for feature in f["feature_probes"])
                   for f in frames), "The two doors never open"
    if check.get("stair_escape"):
        captain = next(m["actor_id"] for m in frames[0]["monsters"] if m["race"] == 91)
        assert not any(m["actor_id"] == captain for m in frames[-1]["monsters"]), "Captain never leaves via the stairs"
        assert any("flees" in text and "stair" in text
                   for f in frames for text in f.get("messages", [])), "Captain never uses the stairs"
        escape=next(i for i,f in enumerate(frames) if not any(m["actor_id"]==captain for m in f["monsters"]))
        assert any(m["role"]==1 and m["order_valid"] for f in frames[escape:]
                   for m in f["monsters"]), "No live successor after stair escape"
    if check.get("cycling_door"):
        assert all(len(f["feature_probes"]) == 1 for f in frames)
        path=Path(result["profile"])/"terrain-events.jsonl"
        assert path.exists(),"No actual door transitions"
        changes=[json.loads(line) for line in path.read_text().splitlines()]
        assert sum(32<=e["after"]<48 and e["before"] in (4,5) for e in changes)>=2, "Door does not close repeatedly"
        assert sum(e["after"] in (4,5) and 32<=e["before"]<48 for e in changes)>=2, "Pursuers do not reopen the door"
    if check.get("knockback"):
        assert len({(f["player"]["y"], f["player"]["x"]) for f in frames}) > 1, "No actual knockback"
    return dict(missile_shots=sum(e["kind"] == "ranged" and e["detail"] in (96,97,98)
                                 for e in events))


def case_env(env, spec):
    return dict(env, AI_FEATURE_PROBES=json.dumps(spec.get("feature_probes", [])))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=20261203)
    parser.add_argument("--filter", default="")
    parser.add_argument("--report", type=Path, default=OUT/"matrix.json")
    args = parser.parse_args()
    instrument()
    exe, env = build()
    env["AI_PLAYTEST_SIGNATURE"] = hashlib.sha256(
        (env["AI_PLAYTEST_SIGNATURE"] + Path(__file__).read_text()
         + Path(engine.__file__).read_text() + Path(coordination.__file__).read_text()).encode()).hexdigest()
    results, failures = [], []
    for name, spec, keys, check in cases():
        if args.filter and not re.search(args.filter, name):
            continue
        spec["seed"] = args.seed
        try:
            result = play.run(exe, case_env(env, spec), name, spec, keys)
            result.update(verify(result, check))
            result["outcome_check"] = check
            results.append(result)
        except Exception as exc:
            failures.append(dict(name=name, error=str(exc), traceback=traceback.format_exc()))
            print(json.dumps(failures[-1]), flush=True)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(dict(results=results, failures=failures), indent=2))
    print(f"Adversarial encounters: {len(results)} passed, {len(failures)} failed", flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
