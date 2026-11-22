#!/usr/bin/env python3
"""Play player-system fixtures through real, windowless SDL GameControl.

The fixture only prepares a scene and observes player state. All tested actions
enter through SDL and the production dungeon loop. Monster AI is outside scope.
Profiles, screenshots, inputs, telemetry and game logs remain in scripts/output.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time
import traceback
import uuid

import qa9_sdl_fixture as adapter

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/player-systems-audit"
_fixture_processes: dict[Path, subprocess.Popen] = {}
sys.path.insert(0, str(ROOT / "tools"))
from game_control import GameControl, ControlError, read_json

HEADLESS_ASSERT = r'''
#undef assert
#define assert(expr) do { if(!(expr)) { \
    fprintf(stderr,"Fixture assertion: %s (%s:%d)\n",#expr,__FILE__,__LINE__); \
    fflush(stderr); _Exit(3); } } while(0)
'''

FIXTURE = r'''
#include "cJSON.h"
#include "birth/birth.h"
#include "tutorial/tutorial.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
void dungeon(void);
static int number(const cJSON* o, const char* name, int fallback)
{
    const cJSON* n = cJSON_GetObjectItemCaseSensitive(o, name);
    return cJSON_IsNumber(n) ? n->valueint : fallback;
}
static void item_json(cJSON* rows, const object_type* o, int handle, const char* store)
{
    if (!o || !o->k_idx || !o->number) return;
    cJSON* row = cJSON_CreateObject();
    cJSON_AddItemToArray(rows, row);
    cJSON_AddStringToObject(row, "store", store);
    cJSON_AddNumberToObject(row, "handle", handle);
    cJSON_AddNumberToObject(row, "kind", o->k_idx);
    cJSON_AddNumberToObject(row, "number", o->number);
    cJSON_AddNumberToObject(row, "tval", o->tval);
    cJSON_AddNumberToObject(row, "sval", o->sval);
    cJSON_AddNumberToObject(row, "pval", o->pval);
    cJSON_AddNumberToObject(row, "timeout", o->timeout);
    cJSON_AddNumberToObject(row, "known", object_known_p(o));
    cJSON_AddNumberToObject(row, "inscription", o->obj_note);
    cJSON_AddNumberToObject(row, "artifact", o->name1);
    cJSON_AddNumberToObject(row, "ego", o->name2);
    cJSON_AddNumberToObject(row, "attack", o->att);
    cJSON_AddNumberToObject(row, "damage_dice", o->dd);
    cJSON_AddNumberToObject(row, "damage_sides", o->ds);
    cJSON_AddNumberToObject(row, "storage", o->storage);
    cJSON_AddNumberToObject(row, "y", o->iy);
    cJSON_AddNumberToObject(row, "x", o->ix);
}
static void snapshot(bool waiting)
{
    if (!waiting || !character_dungeon || !p_ptr) return;
    cJSON* root = cJSON_CreateObject();
#define VALUE(key, value) cJSON_AddNumberToObject(root, key, value)
    VALUE("turn", turn); VALUE("player_turn", playerturn);
    VALUE("y", p_ptr->py); VALUE("x", p_ptr->px); VALUE("depth", p_ptr->depth);
    VALUE("hp", p_ptr->chp); VALUE("max_hp", p_ptr->mhp);
    VALUE("voice", p_ptr->csp); VALUE("max_voice", p_ptr->msp);
    VALUE("food", p_ptr->food); VALUE("poison", p_ptr->poisoned);
    VALUE("blind", p_ptr->blind); VALUE("confused", p_ptr->confused);
    VALUE("cut", p_ptr->cut); VALUE("stun", p_ptr->stun);
    VALUE("song", p_ptr->song1); VALUE("song2", p_ptr->song2);
    VALUE("song_duration", p_ptr->song_duration); VALUE("stealth", p_ptr->stealth_mode);
    VALUE("smithing", p_ptr->smithing); VALUE("fletching", p_ptr->fletching);
    VALUE("exp", p_ptr->exp); VALUE("new_exp", p_ptr->new_exp);
    VALUE("dead", p_ptr->is_dead); VALUE("playing", p_ptr->playing);
    VALUE("leaving", p_ptr->leaving); VALUE("active_weapon", p_ptr->active_weapon_mode);
    VALUE("weight", p_ptr->total_weight); VALUE("light", p_ptr->cur_light);
    VALUE("arrows", player_quiver_arrow_count());
    VALUE("feat", cave_feat[p_ptr->py][p_ptr->px]);
    cJSON* rows = cJSON_AddArrayToObject(root, "items");
    for (int i=0; i<INVEN_TOTAL; ++i) item_json(rows, &inventory[i], i, i<INVEN_WIELD?"pack":"equipped");
    for (int i=0; i<player_carried_extra_entry_count(); ++i)
        item_json(rows, player_carried_extra_entry_at(i), i, "extra");
    for (int i=0; i<player_quiver_store_entry_count(); ++i)
        item_json(rows, player_quiver_store_entry_at(i), i, "quiver");
    for (int i=0; i<supplies_entry_count(); ++i) item_json(rows, supplies_entry_at(i), i, "supplies");
    for (int i=1; i<o_max; ++i) item_json(rows, &o_list[i], i, "floor");
    cJSON* tiles = cJSON_AddArrayToObject(root, "tiles");
    for (int y=0; y<p_ptr->cur_map_hgt; ++y) {
        cJSON* row=cJSON_CreateArray(); cJSON_AddItemToArray(tiles,row);
        for (int x=0; x<p_ptr->cur_map_wid; ++x)
            cJSON_AddItemToArray(row,cJSON_CreateNumber(cave_feat[y][x]));
    }
    cJSON* targets = cJSON_AddArrayToObject(root, "targets");
    for (int i=1; i<mon_max; ++i) if (mon_list[i].r_idx) {
        cJSON* row=cJSON_CreateObject(); cJSON_AddItemToArray(targets,row);
        cJSON_AddNumberToObject(row,"race",mon_list[i].r_idx);
        cJSON_AddNumberToObject(row,"hp",mon_list[i].hp);
        cJSON_AddNumberToObject(row,"y",mon_list[i].fy);
        cJSON_AddNumberToObject(row,"x",mon_list[i].fx);
    }
    char path[1400];
    strnfmt(path,sizeof(path),"%s/telemetry.jsonl",SDL_getenv("CONTROL_TEST_PROFILE"));
    char* data=cJSON_PrintUnformatted(root);
    static char* previous;
    assert(data);
    /* Immutable complete lines avoid Windows rename/share races with readers.
     * Identical idle polls need no additional record. */
    if(!previous || strcmp(previous,data)) {
        FILE* f=fopen(path,"ab"); assert(f);
        size_t length=strlen(data);data[length]='\n';
        assert(fwrite(data,1,length+1,f)==length+1);assert(fclose(f)==0);
        data[length]='\0';SDL_free(previous);previous=SDL_strdup(data);assert(previous);
    }
    cJSON_free(data); cJSON_Delete(root);
#undef VALUE
}
bool __real_sdl_control_poll(bool waiting);
bool __wrap_sdl_control_poll(bool waiting)
{
    snapshot(waiting);
    return __real_sdl_control_poll(waiting);
}
static void prepare(void)
{
    char path[1400];
    strnfmt(path,sizeof(path),"%s/scenario.json",SDL_getenv("CONTROL_TEST_PROFILE"));
    size_t length; char* bytes=SDL_LoadFile(path,&length); assert(bytes);
    cJSON* spec=cJSON_ParseWithLength(bytes,length); SDL_free(bytes); assert(spec);
    player_wipe(); flavor_init(); Rand_state_init(number(spec,"seed",20261115));
    rp_ptr=&p_info[0]; current_character_profile=&c_info[0];
    SDL_strlcpy(op_ptr->full_name,"Player Audit",sizeof(op_ptr->full_name));
    process_player_name(true);
    const cJSON* loaded=cJSON_GetObjectItemCaseSensitive(spec,"load_path");
    if (cJSON_IsString(loaded)) {
        SDL_strlcpy(savefile,loaded->valuestring,sizeof(savefile));
        assert(load_player() && character_loaded);
        rp_ptr=&p_info[p_ptr->prace]; current_character_profile=&c_info[p_ptr->pcharacter];
        character_generated=character_dungeon=true; character_icky=character_xtra=0;
        p_ptr->playing=p_ptr->restoring=true; p_ptr->leaving=false;
        tutorial_set_enabled(false); set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
        sdl_config_set_default_movement_bindings(&config,SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        sdl_config_mark_keyboard_preset_prompt_seen(); sdl_config_mark_mouse_tutorial_seen();
        sdl_config_mark_touch_tutorial_seen(); sdl_config_mark_character_wheel_tutorial_seen();
        use_sound=false; op_ptr->delay_factor=0; op_ptr->level_entry_narrative_mode=0;
        set_sdl_show_context_square_popups(false);
        p_ptr->update=PU_BONUS|PU_TORCH|PU_HP|PU_MANA|PU_UPDATE_VIEW|PU_MONSTERS;
        update_stuff(); p_ptr->redraw=PR_BASIC|PR_EXTRA|PR_MAP;
        Term_flush(); inkey_next_set(NULL); inkey_xtra=false; cJSON_Delete(spec); return;
    }
    p_ptr->playing=p_ptr->restoring=true; p_ptr->wizard=false; p_ptr->noscore=0x000a;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->depth=p_ptr->max_depth=number(spec,"depth",5);
    p_ptr->cur_map_hgt=24; p_ptr->cur_map_wid=40;
    p_ptr->py=number(spec,"y",12); p_ptr->px=number(spec,"x",12);
    p_ptr->food=number(spec,"food",PY_FOOD_FULL);
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->exp=100000; p_ptr->new_exp=50000;
    for (int i=0;i<A_MAX;++i) p_ptr->stat_base[i]=number(spec,"stats",4);
    for (int i=0;i<S_MAX;++i) p_ptr->skill_base[i]=number(spec,"skills",15);
    p_ptr->skill_base[S_SMT]=number(spec,"smithing_skill",40);
    character_generated=character_dungeon=true; character_icky=character_xtra=0;
    turn=playerturn=100; tutorial_set_enabled(false);
    set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
    sdl_config_set_default_movement_bindings(&config,SDL_MOVEMENT_PRESET_CLASSIC_SIL);
    sdl_config_mark_keyboard_preset_prompt_seen(); sdl_config_mark_mouse_tutorial_seen();
    sdl_config_mark_touch_tutorial_seen(); sdl_config_mark_character_wheel_tutorial_seen();
    use_sound=false; op_ptr->delay_factor=0; op_ptr->level_entry_narrative_mode=0;
    set_sdl_show_context_square_popups(false);
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false); sdl_welcome_screen_hide();
    cave_environment_reset(); cave_events_reset();
    for (int y=0;y<24;++y) for (int x=0;x<40;++x) {
        cave_set_feat(y,x,(!y || !x || y==23 || x==39)?FEAT_WALL_PERM:FEAT_FLOOR);
        cave_info[y][x]|=CAVE_MARK|CAVE_GLOW;
    }
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));
    object_prep(&inventory[INVEN_BOW],lookup_kind(TV_BOW,SV_SHORT_BOW));
    object_prep(&inventory[INVEN_LITE],lookup_kind(TV_LIGHT,SV_LIGHT_TORCH));
    inventory[INVEN_LITE].timeout=3000;
    for (int i=INVEN_WIELD;i<INVEN_TOTAL;++i) if (inventory[i].k_idx) {
        object_known(&inventory[i]); p_ptr->total_weight+=inventory[i].weight; p_ptr->equip_cnt++;
    }
    cJSON* entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItemCaseSensitive(spec,"items")) {
        object_type o; int k=number(entry,"kind",64); assert(k>0 && k<z_info->k_max && k_info[k].name);
        object_prep(&o,k); o.number=number(entry,"number",1);
        o.pval=number(entry,"pval",o.pval); o.timeout=number(entry,"timeout",o.timeout);
        o.storage=number(entry,"storage",o.storage);
        if (number(entry,"known",1)) {object_known(&o); k_info[k].aware=true;}
        if (number(entry,"floor",0)) {
            int y=number(entry,"y",p_ptr->py),x=number(entry,"x",p_ptr->px);
            int idx=o_pop(); assert(idx); o_list[idx]=o; o_list[idx].iy=y; o_list[idx].ix=x;
            o_list[idx].marked=true; o_list[idx].next_o_idx=cave_o_idx[y][x]; cave_o_idx[y][x]=idx;
        } else if(number(entry,"slot",-1)>=INVEN_WIELD) {
            int slot=number(entry,"slot",-1); assert(slot<INVEN_TOTAL && !inventory[slot].k_idx);
            inventory[slot]=o; p_ptr->equip_cnt++; p_ptr->total_weight+=o.weight*o.number;
        } else if(number(entry,"quiver",0)) {
            assert(player_quiver_absorb_arrow(&o)==number(entry,"number",1));
        } else {assert(inven_carry(&o,true)>=0);}
    }
    cJSON_ArrayForEach(entry,cJSON_GetObjectItemCaseSensitive(spec,"abilities")) {
        int skill=number(entry,"skill",S_SNG),a=number(entry,"ability",SNG_TREES);
        assert(skill>=0 && skill<S_MAX && a>=0 && a<ABILITIES_MAX);
        p_ptr->innate_ability[skill][a]=p_ptr->have_ability[skill][a]=p_ptr->active_ability[skill][a]=true;
    }
    cJSON_ArrayForEach(entry,cJSON_GetObjectItemCaseSensitive(spec,"tiles")) {
        int y=number(entry,"y",p_ptr->py),x=number(entry,"x",p_ptr->px);
        assert(in_bounds_fully(y,x)); cave_set_feat(y,x,number(entry,"feat",FEAT_FLOOR));
    }
    cave_m_idx[p_ptr->py][p_ptr->px]=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItemCaseSensitive(spec,"targets")) {
        int y=number(entry,"y",12),x=number(entry,"x",13),race=number(entry,"race",11);
        assert(place_monster_one(y,x,race,false,true,NULL));
        monster_type* m=&mon_list[cave_m_idx[y][x]];
        m->hp=m->maxhp=number(entry,"hp",200); m->energy=-30000;
        m->alertness=ALERTNESS_UNWARY;
    }
    p_ptr->update=PU_BONUS|PU_TORCH|PU_HP|PU_MANA|PU_UPDATE_VIEW|PU_MONSTERS;
    update_stuff(); p_ptr->chp=number(spec,"hp",p_ptr->mhp);
    p_ptr->csp=number(spec,"voice",p_ptr->msp);
    p_ptr->poisoned=number(spec,"poison",0); p_ptr->cut=number(spec,"cut",0);
    p_ptr->blind=number(spec,"blind",0); p_ptr->confused=number(spec,"confused",0);
    p_ptr->energy=100; p_ptr->redraw=PR_BASIC|PR_EXTRA|PR_MAP;
    Term_flush(); inkey_next_set(NULL); inkey_xtra=false; cJSON_Delete(spec);
}
'''


def build(*, fixture: str | None = None, wrappers: tuple[str, ...] = (),
          entry: str | None = None) -> tuple[Path, dict]:
    directory = OUT / ("fixture-build-" + uuid.uuid4().hex[:8])
    directory.mkdir(parents=True)
    source = directory / "fixture.c"
    selected_fixture=(FIXTURE if fixture is None else fixture).replace(
        "void dungeon(void);",HEADLESS_ASSERT+"\nvoid dungeon(void);",1)
    harness = adapter.HARNESS.replace("static cptr make_path(",
        HEADLESS_ASSERT+"\nstatic cptr make_path(",1)
    harness = harness.replace("int main(int argc, char** argv)",
        selected_fixture + "\nint main(int argc, char** argv)")
    harness = harness[:harness.index("    int last = 0;")] + (entry or
        "    prepare(); dungeon(); snapshot(true); sdl_control_shutdown(); return 0;\n}\n")
    harness = harness.replace('ANGBAND_DIR_PREF = make_path(assets, "pref", false);',
                              'ANGBAND_DIR_PREF = make_path(profile, "pref", false);')
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = directory / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith("/src/main.c.obj")))
    env = os.environ.copy()
    env.pop("SIL_MORE_CONTROL_DIR", None)
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    executable = directory / "fixture.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=sdl_control_poll",
        *("-Wl,--wrap="+name for name in wrappers), "-o", str(executable)],
        cwd=BUILD, env=env, check=True)
    (directory / "provenance.json").write_text(json.dumps({
        "build_sha256": hashlib.sha256((BUILD / "sil-more.exe").read_bytes()).hexdigest(),
        "driver_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
    }, indent=2))
    return executable, env


def start(name: str, spec: dict, executable: Path, env: dict) -> Path:
    profile = OUT / (name + "-" + uuid.uuid4().hex[:8])
    profile.mkdir(parents=True)
    (profile / "screens").mkdir()
    shutil.copytree(ROOT / "lib/pref", profile / "pref")
    if "load_from" in spec:
        spec = dict(spec)
        saved = Path(spec.pop("load_from"))
        (profile / "save").mkdir()
        target = profile / "save" / saved.name
        shutil.copy2(saved, target)
        spec["load_path"] = str(target)
    (profile / "scenario.json").write_text(json.dumps(spec, indent=2))
    control = profile / "control"
    env = dict(env, CONTROL_TEST_PROFILE=str(profile))
    process = subprocess.Popen([str(executable), "--windowed", "--control-dir", str(control)],
        cwd=ROOT, env=env, stdout=(profile / "stdout.log").open("w"), stderr=subprocess.STDOUT,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    _fixture_processes[profile.resolve()]=process
    (profile / "playtest.json").write_text(json.dumps(dict(profile=str(profile), control=str(control), pid=process.pid, executable=str(executable)), indent=2))
    deadline = time.monotonic() + 30
    while not (control / "session.json").exists() or not any(
            (profile/name).exists() for name in ("telemetry.json", "telemetry.jsonl")):
        if process.poll() is not None:
            raise RuntimeError(f"Fixture exited {process.returncode}: {profile}")
        if time.monotonic() > deadline:
            raise TimeoutError(str(profile))
        time.sleep(.05)
    return profile


class FixtureControl(GameControl):
    def __init__(self,profile: Path):
        self.profile=profile.resolve()
        super().__init__(profile/"control",timeout=30)
    def session(self):
        process=_fixture_processes.get(self.profile)
        if process is not None and process.poll() is not None:
            raise ControlError(f"Fixture exited with {process.returncode}; inspect {self.profile/'stdout.log'}")
        return super().session()


def read_telemetry(profile: Path) -> dict | None:
    stream=profile/"telemetry.jsonl"
    if stream.exists():
        for _ in range(20):
            with stream.open("rb") as source:
                size=source.seek(0,2);source.seek(max(0,size-1048576))
                tail=source.read()
            end=tail.rfind(b"\n")
            if end>=0:
                start=tail.rfind(b"\n",0,end)+1
                value=json.loads(tail[start:end])
                if not isinstance(value,dict):raise ControlError("Invalid telemetry record")
                return value
            time.sleep(.005)
        raise ControlError("Fixture has not completed its first telemetry record")
    legacy=profile/"telemetry.json"
    return read_json(legacy) if legacy.exists() else None


def act(profile: Path, op: str = "observe", value=None, capture: bool = True) -> dict:
    game = FixtureControl(profile)
    trace = profile / "inputs.jsonl"
    index = sum(1 for _ in trace.open(encoding="utf-8")) if trace.exists() else 0
    options = dict(capture=capture)
    if capture:
        (profile / "screens").mkdir(exist_ok=True)
        options["output"] = profile / "screens" / f"{index:04d}.png"
    record = dict(index=index, op=op, value=value)
    try:
        if op == "key":
            response = game.key(value, **options)
        elif op == "text":
            response = game.text(value, **options)
        elif op == "click":
            response = game.click(*value, **options)
        else:
            response = game.observe(**options)
        record["response"] = response
        telemetry=read_telemetry(profile)
        if telemetry is not None:record["telemetry"]=telemetry
    except Exception as exc:
        record["observer_error"] = repr(exc)
        raise
    finally:
        with trace.open("a", encoding="utf-8") as out:
            out.write(json.dumps(record) + "\n")
    if not response.get("ok"):
        raise ControlError(str(response))
    return record


def compact(record: dict) -> dict:
    response = record["response"]
    state = response.get("state", {})
    return dict(closed=response.get("closed"), screenshot=response.get("screenshot"),
        state={k:v for k,v in state.items() if k != "terminal"},
        terminal=[s.rstrip() for s in state.get("terminal", []) if s.strip()],
        player={k:v for k,v in record.get("telemetry",{}).items() if k not in ("tiles", "items")},
        items=record.get("telemetry",{}).get("items"))


def suite(only=None) -> int:
    previous = OUT / "gameplay-summary.json"
    if previous.exists():
        shutil.copy2(previous, OUT / ("gameplay-history-"+uuid.uuid4().hex[:8]+".json"))
    executable, env = build()
    results = []

    def run(name, spec, exercise):
        if only and name not in only:
            return
        profile = start(name, spec, executable, env)
        current = act(profile)
        initial = current["telemetry"]

        def press(*keys):
            nonlocal current
            for key in keys:
                current = act(profile, "key", key)
            return current["telemetry"]

        def player():
            return current["telemetry"]

        def close_overlay():
            nonlocal current
            for _ in range(8):
                if not current["response"].get("state", {}).get("modal"):
                    return
                press("Escape")
            raise AssertionError("Overlay did not return to dungeon")

        result = dict(name=name, profile=str(profile), passed=False)
        try:
            exercise(press, player, close_overlay, profile, initial)
            result["passed"] = True
        except Exception:
            result["error"] = traceback.format_exc()
        finally:
            try:
                if GameControl(profile / "control").session().get("running"):
                    close_overlay()
                    for _ in range(6):
                        press("Ctrl+x")
                        if current["response"].get("closed"):
                            break
                        press("Escape")
                        if current["response"].get("closed"):
                            break
                    else:
                        raise AssertionError("Fixture did not close through its interface")
            except ControlError:
                pass  # A stair exit or an orderly quit already closed the fixture.
            except Exception:
                result["cleanup_error"] = traceback.format_exc()
                result["passed"] = False
        result["final"] = player()
        results.append(result)
        (OUT / "gameplay-summary.json").write_text(json.dumps(dict(
            monster_ai="excluded by user request", cases=results,
            passed=sum(r["passed"] for r in results), failed=sum(not r["passed"] for r in results)), indent=2))
        print(f"{'PASS' if result['passed'] else 'FAIL'} {name}: {profile.name}", flush=True)

    def browse(command):
        def check(press, player, close, profile, initial):
            press(*command)
            assert current_waiting(profile), "View did not yield an input boundary"
            close()
            assert player()["turn"] == initial["turn"] and player()["player_turn"] == initial["player_turn"], "Browsing spent time"
            assert player()["items"] == initial["items"], "Browsing changed items"
        return check

    for name, keys in (
        ("character-sheet", ("h",)), ("inventory-browser", ("i",)),
        ("equipment-browser", ("e",)), ("knowledge-browser", ("~",)),
        ("help-browser", ("?",)), ("abilities-browser", ("y",)),
        ("map-browser", ("M",)), ("look-browser", ("l",)),
        ("messages-browser", ("Ctrl+p",)), ("combat-history", ("Ctrl+q",)),
        ("settings-browser", ("O",)), ("quest-journal", ("m", "t")),
        ("achievements-browser", ("m", "v")), ("tale-statistics", ("m", "g")),
        ("about-browser", ("m", "b")), ("tutorial-cards", ("m", "u")),
        ("halls-of-mandos", ("m", "d")), ("tale-browser", ("m", "y")),
    ):
        run(name, {}, browse(keys))

    def movement(press, player, close, profile, initial):
        press("Right")
        assert (player()["y"],player()["x"]) == (12,13)
        press("Left")
        assert (player()["y"],player()["x"]) == (12,12)
        assert player()["player_turn"] == initial["player_turn"] + 2
        press("S"); assert player()["stealth"]
        press("S"); assert not player()["stealth"]
        assert player()["player_turn"] == initial["player_turn"] + 2
    run("movement-and-stealth", {}, movement)

    def door(press, player, close, profile, initial):
        press("o"); assert player()["tiles"][12][13] == 4
        press("c"); assert player()["tiles"][12][13] == 32
        assert player()["player_turn"] == initial["player_turn"]+2
    run("open-close-door", {"tiles":[dict(y=12,x=13,feat=32)]}, door)

    def restorative(command, kind, effect, experience=75):
        def check(press, player, close, profile, initial):
            press(command, "-", "n")
            assert player()["player_turn"] == initial["player_turn"]
            assert count_items(player(),kind,"floor") == 2
            assert player()["exp"] == initial["exp"]
            press("-", "y")
            assert player()["player_turn"] == initial["player_turn"]+1
            assert count_items(player(),kind,"floor") == 1
            assert count_items(player(),kind,"supplies") == 0
            assert player()["exp"] == initial["exp"]+experience
            assert effect(player(),initial)
        return check
    run("floor-healing", dict(hp=10,items=[dict(kind=318,floor=1,number=2,known=0)]),
        restorative("q",318,lambda p,i:p["hp"]>i["hp"]))
    run("floor-voice", dict(voice=1,items=[dict(kind=319,floor=1,number=2,known=0)]),
        restorative("q",319,lambda p,i:p["voice"]==p["max_voice"]))
    run("floor-antidote", dict(poison=12,items=[dict(kind=321,floor=1,number=2,known=0)]),
        restorative("q",321,lambda p,i:p["poison"]==0))
    run("floor-food", dict(food=500,items=[dict(kind=399,floor=1,number=2,known=0)]),
        restorative("E",399,lambda p,i:p["food"]>i["food"],0))
    run("floor-herb", dict(hp=10,items=[dict(kind=383,floor=1,number=2,known=0)]),
        restorative("E",383,lambda p,i:p["hp"]>i["hp"]))

    def pickup(press, player, close, profile, initial):
        press("g")
        for _ in range(3):
            if count_items(player(),399,"floor") == 0: break
            press("Enter")
        assert count_items(player(),399,"floor") == 0
        assert count_items(player(),399,"supplies") == 2
        assert player()["player_turn"] == initial["player_turn"]+1
    run("pickup-food", dict(items=[dict(kind=399,floor=1,number=2)]), pickup)

    def melee(press, player, close, profile, initial):
        press("Right")
        assert (player()["y"],player()["x"]) == (12,12)
        assert not player()["targets"] or player()["targets"][0]["hp"]<initial["targets"][0]["hp"]
        assert player()["player_turn"]==initial["player_turn"]+1
    run("player-melee", dict(skills=50,targets=[dict(race=11,y=12,x=13,hp=200)]), melee)

    def stair(press, player, close, profile, initial):
        press(">")
        if not player()["leaving"]: press("y")
        assert player()["depth"]==initial["depth"]+1
        assert player()["player_turn"]==initial["player_turn"]+1
    run("descend-stairs",dict(tiles=[dict(y=12,x=12,feat=81)]),stair)

    def terrain(press, player, close, profile, initial):
        press("Right")
        if player()["x"] == 12: press("y")
        assert player()["x"]==13
        assert player()["player_turn"]>initial["player_turn"]
        assert bool(player()["dead"]) == (player()["feat"] == 85)
    for name,feature in (("shallow-water",84),("deep-water",100),("ice",86),("lava",85)):
        run(name,dict(stats=12,tiles=[dict(y=12,x=13,feat=feature)]),terrain)

    def smith(press, player, close, profile, initial):
        press("0","a","a","a")
        assert player()["player_turn"]==initial["player_turn"]
        press("g")
        assert player()["smithing"]==0
        assert player()["player_turn"]==initial["player_turn"]+10
        assert player()["feat"]==66 and count_items(player(),56,"pack")==5
    run("smith-daggers",dict(tiles=[dict(y=12,x=12,feat=67)],
        abilities=[dict(skill=6,ability=0)]),smith)

    def fletch(press, player, close, profile, initial):
        press("-","Escape")
        assert player()["player_turn"]==initial["player_turn"]
        press("-","a")
        assert player()["fletching"]==0 and player()["player_turn"]>initial["player_turn"]
        assert count_items(player(),116,"pack")==10
        arrows=next(o for o in player()["items"] if o["kind"]==116)
        assert arrows["attack"]==3
    run("fletch-arrows",dict(abilities=[dict(skill=1,ability=1)],
        items=[dict(kind=116,number=10)]),fletch)

    def songs(press, player, close, profile, initial):
        press("s","Escape")
        assert player()["player_turn"]==initial["player_turn"]
        press("s","h","z")
        assert player()["song"]==7 and player()["light"]>initial["light"]
        stopped_at=player()["player_turn"]
        press("s","s")
        assert player()["song"]==100 and player()["player_turn"]==stopped_at
        assert player()["light"]==initial["light"]
    run("song-light-and-stop",dict(abilities=[dict(skill=7,ability=7),dict(skill=7,ability=5)]),songs)

    def rest(press, player, close, profile, initial):
        press("Z","n")
        assert player()["hp"]==player()["max_hp"] and player()["player_turn"]>initial["player_turn"]
        assert (player()["x"],player()["y"])==(initial["x"],initial["y"])
    run("rest-to-health",dict(hp=10),rest)

    def gem(press, player, close, profile, initial):
        press("u","Tab","Tab","a","n")
        assert player()["player_turn"]==initial["player_turn"]
        assert count_items(player(),192,"supplies")==1
        press("a","y")
        assert player()["player_turn"]==initial["player_turn"]+1
        assert count_items(player(),192,"supplies")==0
    run("use-gem",dict(items=[dict(kind=192)]),gem)

    def bow(press, player, close, profile, initial):
        press("f")
        assert player()["arrows"]==5 and player()["player_turn"]==initial["player_turn"]
        press("Tab","b")
        assert player()["active_weapon"]==2 and player()["player_turn"]==initial["player_turn"]+1
        press("f","Escape")
        assert player()["arrows"]==5 and player()["player_turn"]==initial["player_turn"]+1
        press("f","Right","f")
        assert player()["arrows"]==4 and player()["player_turn"]==initial["player_turn"]+2
    run("ready-bow-cancel-and-fire",dict(items=[dict(kind=116,number=5,quiver=1)]),bow)

    def horn(press, player, close, profile, initial):
        press("p","Escape")
        assert player()["voice"]==initial["voice"] and player()["player_turn"]==initial["player_turn"]
        press("p","Right","f")
        assert player()["voice"]==initial["voice"]-20
        assert player()["player_turn"]==initial["player_turn"]+1
        assert count_items(player(),250,"pack")==1
    run("horn-aim-cancel-and-play",dict(items=[dict(kind=250,storage=2)]),horn)

    def inscribe(press, player, close, profile, initial):
        press("{","a")
        act(profile,"text","Player audit \u03bb #1")
        press("Enter")
        assert player()["player_turn"]==initial["player_turn"]
        assert next(o for o in player()["items"] if o["kind"]==56)["inscription"]>0
    run("unicode-item-inscription",dict(items=[dict(kind=56,number=3)]),inscribe)

    def throw(press, player, close, profile, initial):
        press("t","Escape")
        assert player()["player_turn"]==initial["player_turn"]
        press("t","Right","f")
        assert player()["player_turn"]==initial["player_turn"]+1
        assert count_items(player(),56,"equipped")==2
        assert count_items(player(),56,"floor")==1
    run("throw-belt-dagger",dict(items=[dict(kind=56,number=3,slot=38)]),throw)

    def save_load(press, player, close, profile, initial):
        press("Right","Left","Ctrl+s")
        checkpoint=profile/"save/Player_Audit"
        assert checkpoint.is_file()
        saved=player()
        branches=[]
        for index in range(2):
            branch=start("loaded-branch",dict(load_from=str(checkpoint)),executable,env)
            r=act(branch)
            assert r["telemetry"]==saved, "Full save changed player/inventory/map state"
            for key in ("Right","Left","z"):
                r=act(branch,"key",key)
            branches.append(dict(profile=str(branch),final=r["telemetry"]))
            act(branch,"key","Ctrl+x")
        assert branches[0]["final"]==branches[1]["final"], "Loaded continuations diverged"
        (profile/"checkpoints.json").write_text(json.dumps(branches,indent=2))
    run("full-save-two-continuations",dict(items=[dict(kind=318,number=2),
        dict(kind=56,number=3),dict(kind=116,number=5,quiver=1)]),save_load)
    return int(any(not r["passed"] for r in results))


def current_waiting(profile: Path) -> bool:
    response=GameControl(profile / "control").observe(capture=False)
    return response.get("state",{}).get("waiting_for_input",False)


def count_items(player: dict, kind: int, store: str) -> int:
    return sum(o["number"] for o in player["items"] if o["kind"]==kind and o["store"]==store)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    suites = sub.add_parser("suite")
    suites.add_argument("--only", nargs="+")
    launch = sub.add_parser("launch")
    launch.add_argument("name")
    launch.add_argument("--spec", type=Path)
    action = sub.add_parser("action")
    action.add_argument("profile", type=Path)
    action.add_argument("keys", nargs="*")
    action.add_argument("--text")
    action.add_argument("--no-capture", action="store_true")
    args = parser.parse_args()
    if args.command == "suite":
        return suite(args.only)
    if args.command == "launch":
        executable, env = build()
        profile = start(args.name, json.loads(args.spec.read_text()) if args.spec else {}, executable, env)
        print(profile)
        print(json.dumps(compact(act(profile)), indent=2))
    else:
        if args.text is not None:
            record = act(args.profile, "text", args.text, not args.no_capture)
        else:
            record = act(args.profile) if not args.keys else None
            for key in args.keys:
                record = act(args.profile, "key", key, not args.no_capture)
                if record["response"].get("closed"):
                    break
        print(json.dumps(compact(record), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
