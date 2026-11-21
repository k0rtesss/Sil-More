#!/usr/bin/env python3
"""Measure deterministic encounters using Release objects and isolated SDL input.

Requires build-standard. Timings cover real monster processing, including its
logging/rendering, but exclude startup, input transport, and observer file I/O.
Action observers hash in memory rather than writing a file for every action.
Compare state/action/RNG fingerprints before and after an optimization; wall
times are diagnostic and deliberately have no machine-dependent pass threshold.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time

import check_monster_ai_gameplay as play

ROOT = play.ROOT
OUT = ROOT / "scripts/output/gameplay-performance"

OBSERVERS = r'''
static unsigned long long action_hash = 14695981039346656037ULL;
static unsigned long long action_count, monster_ns, squad_ns, tactical_ns;
static unsigned long long monster_calls, squad_calls, tactical_calls;
static void hash_number(unsigned long long value)
{
    for (int i = 0; i < 8; ++i) {
        action_hash ^= (value >> (i * 8)) & 255;
        action_hash *= 1099511628211ULL;
    }
}
static void action_event(const char* kind, const monster_type* m, int detail,
    int old_y, int old_x, bool melee, bool skipped)
{
    for (const char* c = kind; *c; ++c) hash_number((unsigned char)*c);
    const int values[] = {turn, playerturn, m->r_idx, (int)(m - mon_list),
        detail, m->fy, m->fx, old_y, old_x, melee, skipped, m->hp, m->mana,
        m->energy, m->alertness, m->min_range, m->best_range};
    for (int i = 0; i < (int)N_ELEMENTS(values); ++i) hash_number(values[i]);
    ++action_count;
}
void __real_process_monsters(s16b);
void __wrap_process_monsters(s16b energy)
{
    Uint64 start = SDL_GetTicksNS();
    __real_process_monsters(energy);
    monster_ns += SDL_GetTicksNS() - start; ++monster_calls;
}
void __real_monster_squad_prepare(void);
void __wrap_monster_squad_prepare(void)
{
    Uint64 start = SDL_GetTicksNS();
    __real_monster_squad_prepare();
    squad_ns += SDL_GetTicksNS() - start; ++squad_calls;
}
bool __real_get_move_tactical(monster_type*, int*, int*);
bool __wrap_get_move_tactical(monster_type* m, int* y, int* x)
{
    Uint64 start = SDL_GetTicksNS();
    bool result = __real_get_move_tactical(m, y, x);
    tactical_ns += SDL_GetTicksNS() - start; ++tactical_calls;
    return result;
}
int __real_monster_tactical_move_utility(monster_type*);
int __wrap_monster_tactical_move_utility(monster_type* m)
{
    Uint64 start = SDL_GetTicksNS();
    int result = __real_monster_tactical_move_utility(m);
    tactical_ns += SDL_GetTicksNS() - start; ++tactical_calls;
    return result;
}
'''

METRICS = r'''
        cJSON* perf = cJSON_AddObjectToObject(root, "performance");
        cJSON_AddNumberToObject(perf, "monster_ms", monster_ns / 1000000.0);
        cJSON_AddNumberToObject(perf, "squad_ms", squad_ns / 1000000.0);
        cJSON_AddNumberToObject(perf, "tactical_ms", tactical_ns / 1000000.0);
        cJSON_AddNumberToObject(perf, "monster_calls", monster_calls);
        cJSON_AddNumberToObject(perf, "squad_calls", squad_calls);
        cJSON_AddNumberToObject(perf, "tactical_calls", tactical_calls);
        cJSON_AddNumberToObject(perf, "actions", action_count);
        char digest[40];
        strnfmt(digest, sizeof(digest), "%08x%08x",
            (unsigned)(action_hash >> 32), (unsigned)action_hash);
        cJSON_AddStringToObject(root, "action_hash", digest);
        u64b rng_state = Rand_state_export();
        strnfmt(digest, sizeof(digest), "%08x%08x",
            (unsigned)(rng_state >> 32), (unsigned)rng_state);
        cJSON_AddStringToObject(root, "rng_state", digest);
'''


def instrument(destination: Path, reference: str | None = None):
    play.OUT = destination
    start = play.FIXTURE.index("static void action_event(")
    end = play.FIXTURE.index("void __real_monster_abilities_end_action", start)
    play.FIXTURE = play.FIXTURE[:start] + OBSERVERS + play.FIXTURE[end:]
    play.FIXTURE = play.FIXTURE.replace(
        "cJSON* root = cJSON_CreateObject();",
        "cJSON* root = cJSON_CreateObject();" + METRICS)
    functions = [("void", name, "monster_type* m", "m") for name in (
        "monster_ai_begin_turn", "monster_social_tick", "monster_abilities_begin_action",
        "monster_world_observe", "monster_senses_refresh")]
    functions += [("bool", name, "monster_type* m", "m") for name in (
        "monster_social_turn", "monster_world_turn")]
    functions += [("void", "process_move", "monster_type* m, int y, int x, bool bash", "m,y,x,bash"),
        ("bool", "get_move", "monster_type* m, int* y, int* x, bool* fear, bool target", "m,y,x,fear,target"),
        ("errr", "Term_fresh", "void", ""), ("void", "handle_stuff", "void", ""),
        ("void", "SDL_Delay", "Uint32 ms", "ms")]
    declarations, metrics = [], []
    for result, name, parameters, arguments in functions:
        declarations.append(f"static Uint64 timer_{name}; static unsigned count_{name};\n"
            f"{result} __real_{name}({parameters});\n{result} __wrap_{name}({parameters}) {{\n"
            f" Uint64 start = SDL_GetTicksNS();\n "
            + ("" if result == "void" else f"{result} value = ")
            + f"__real_{name}({arguments});\n timer_{name} += SDL_GetTicksNS()-start; ++count_{name};\n"
            + ("" if result == "void" else " return value;\n") + "}\n")
        metrics.append(f'cJSON_AddNumberToObject(perf, "{name}_ms", timer_{name}/1000000.0);'
            f'cJSON_AddNumberToObject(perf, "{name}_calls", count_{name});')
    play.FIXTURE = "\n".join(declarations) + play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(METRICS, METRICS + "\n".join(metrics))
    wraps = ("process_monsters", "monster_squad_prepare",
        "get_move_tactical", "monster_tactical_move_utility", *(f[1] for f in functions))
    exe, env = play.build(extra_wraps=wraps)
    if reference:
        # Recompile just the changed owning sources against the same Release
        # build/data. This preserves unrelated working-tree changes and avoids
        # rebuilding a second SDL tree for a controlled before/after comparison.
        objects = shlex.split((exe.parent / "objects.rsp").read_text())
        for source in ("src/melee/melee-movement.c", "src/melee/melee-movement-tactics.c",
                "src/melee/melee-process.c", "src/log/log.c", "src/log/bootstrap.c", "src/log/perf.c"):
            original = subprocess.run(["git", "show", f"{reference}:{source}"],
                cwd=ROOT, check=True, capture_output=True).stdout
            saved = exe.parent / (Path(source).stem + "-reference.c")
            saved.write_bytes(original)
            obj = saved.with_suffix(".o")
            subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_PERF_DIAGNOSTICS",
                "-DNDEBUG", "-std=c17", "-O3", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                "-I"+str(ROOT / "src/log"), "-c", str(saved), "-o", str(obj)],
                cwd=play.BUILD, env=env, check=True)
            suffix = source + ".obj"
            matches = [i for i,p in enumerate(objects) if p.endswith(suffix)]
            if source == "src/log/log.c":
                matches = [i for i,p in enumerate(objects) if p.endswith("/log.c.obj")]
            assert len(matches) == 1, (source, matches)
            objects[matches[0]] = str(obj)
        response = exe.parent / "reference-objects.rsp"
        response.write_text("\n".join('"'+p+'"' for p in objects))
        subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
            "@CMakeFiles/sil-more.dir/includes_C.rsp", str(exe.parent / "fixture.c"),
            "@"+str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            *("-Wl,--wrap="+name for name in ("sdl_control_poll", "monster_abilities_end_action",
                "make_attack_ranged", "make_attack_normal", "make_attack_reaction", "monster_social_remap", *wraps)),
            "-o", str(exe)], cwd=play.BUILD, env=env, check=True)
    return exe, env


def cases():
    yield "mixed-squad", dict(evasion=70, will=40, monsters=[
        dict(race=race, y=y, x=x, heard=1, morale=150)
        for race, y, x in [(91,20,34), (61,19,35), (61,21,35),
                           (51,18,33), (51,22,33)]])
    cells = [(y,x) for y in range(13,28) for x in range(25,56) if (y,x)!=(20,40)]
    cells.sort(key=lambda p:(max(abs(p[0]-20),abs(p[1]-40))
        + min(abs(p[0]-20),abs(p[1]-40))//2,p))
    yield "crowd-80", dict(recognition=0, evasion=70, will=40, monsters=[
        dict(race=91 if i%8==0 else 51 if i%3==0 else 61,
             y=y, x=x, heard=1, morale=150) for i,(y,x) in enumerate(cells[:80])])
    yield "blocked-archers", dict(evasion=70, will=40, monsters=[
        dict(race=51, y=y, x=34, heard=1, morale=150) for y in (18,20,22)] + [
        dict(race=61, y=y, x=36, heard=1, morale=150) for y in (19,20,21)],
        tiles=[dict(y1=16,y2=19,x1=38,feat=63), dict(y1=21,y2=24,x1=38,feat=63)])


def run(exe, env, name, spec, turns):
    profile, process, game = play.start(exe, env, name, spec)
    try:
        initial = play.telemetry(profile)
        states = []
        started = time.perf_counter()
        for _ in range(turns):
            response = game.key("KP5", capture=False)
            assert response["ok"] and not response["closed"], response
            state = play.telemetry(profile)
            assert state["player"]["hp"] > 0
            states.append({k:v for k,v in state.items() if k != "performance"})
        elapsed = time.perf_counter() - started
        final = play.telemetry(profile)
        assert final["player_turn"] - initial["player_turn"] == turns
        fingerprint = hashlib.sha256(json.dumps(states, sort_keys=True).encode()).hexdigest()
        text = (profile / "log.txt").read_text(encoding="utf-8")
        assert not any(f" {level} " in text for level in ("ERROR", "FATAL"))
        return dict(name=name, profile=str(profile), turns=turns,
            elapsed_ms=elapsed*1000, fingerprint=fingerprint,
            action_hash=final["action_hash"], rng_state=final["rng_state"],
            performance=final["performance"], log_bytes=len(text.encode()),
            debug_lines=sum(" DEBUG " in s for s in text.splitlines()))
    finally:
        if process.poll() is None:
            try:
                game.key("Ctrl+x", capture=False)
                process.wait(timeout=5)
            except (play.ControlError, subprocess.TimeoutExpired):
                process.terminate()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--turns", type=int, default=32)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--levels", default="default,INFO,DEBUG")
    parser.add_argument("--filter", default="")
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--reference", help="Git revision supplying the pre-change performance sources")
    args = parser.parse_args()
    destination = OUT / args.tag
    destination.mkdir(parents=True, exist_ok=True)
    exe, env = instrument(destination, args.reference)
    env.pop("SIL_PERF", None)
    results = []
    reference = json.loads(args.compare.read_text())["results"] if args.compare else []
    for name, spec in cases():
        if args.filter and args.filter not in name: continue
        for level in args.levels.split(","):
            for repeat in range(args.repeats):
                run_env = dict(env)
                run_env.pop("SIL_LOG_LEVEL", None)
                if level != "default": run_env["SIL_LOG_LEVEL"] = level
                result = run(exe, run_env, f"{name}-{level}-{repeat}", dict(spec, seed=20261116), args.turns)
                result.update(case=name, level=level, repeat=repeat)
                previous = next((r for r in reference if r["case"]==name and r["repeat"]==repeat
                    and r["level"]==level), None)
                if previous: assert result["fingerprint"] == previous["fingerprint"], f"Behavior drift: {name}/{level}"
                others = [r for r in results if r["case"]==name]
                if others: assert result["fingerprint"] == others[0]["fingerprint"], f"Logging changed behavior: {name}"
                results.append(result)
                print(json.dumps(result), flush=True)
                (destination / "summary.json").write_text(json.dumps(dict(results=results), indent=2))
    assert results, "No cases selected"


if __name__ == "__main__": main()
