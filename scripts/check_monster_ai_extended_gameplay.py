#!/usr/bin/env python3
"""Play compound AI encounters through SDL, with disposable saves and telemetry.

Requires the configured Windows build. Player actions use GameControl; the
fixture only sets up encounters and adds read-only observations of the engine.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import traceback
import uuid

import check_monster_ai_gameplay as play

ROOT = play.ROOT
OUT = ROOT / "scripts/output/monster-ai-extended"


def instrument() -> None:
    play.OUT = OUT
    # The SDL private header already includes externs.h. Extract only the
    # production layout enum instead of re-including that unguarded header.
    layout=(ROOT/"src/level-generation/level-generation-internal.h").read_text()
    modes=re.search(r"typedef enum quadrant_mode\s*\{.*?\}\s*quadrant_mode_t;",layout,re.S)
    assert modes
    play.FIXTURE = play.FIXTURE.replace('#include "cJSON.h"',
        '#include "cJSON.h"\n'+modes.group(0))
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(root, "player_turn", playerturn);',
        'cJSON_AddNumberToObject(root, "player_turn", playerturn);\n'
        '        cJSON_AddStringToObject(root, "savefile", savefile);')
    play.FIXTURE = play.FIXTURE.replace(
        "inventory[INVEN_LITE].timeout = 10000;",
        'inventory[INVEN_LITE].timeout = number(spec, "torch_fuel", 10000);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(player, "light", p_ptr->cur_light);',
        'cJSON_AddNumberToObject(player, "light", p_ptr->cur_light);\n'
        '        cJSON_AddNumberToObject(player, "fuel", inventory[INVEN_LITE].timeout);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(o, "plan", m->squad.plan);',
        'cJSON_AddNumberToObject(o, "plan", m->squad.plan);\n'
        '            cJSON_AddNumberToObject(o, "plan_age", m->squad.plan_age);\n'
        '            cJSON_AddNumberToObject(o, "budget_plan", m->squad.budget_plan);\n'
        '            cJSON_AddNumberToObject(o, "confused", m->confused);\n'
        '            cJSON_AddNumberToObject(o, "stunned", m->stunned);\n'
        '            cJSON_AddNumberToObject(o, "sight_range", monster_sight_range(m));\n'
        '            cJSON_AddBoolToObject(o, "mindless", (r->flags2 & RF2_MINDLESS) != 0);\n'
        '            cJSON_AddBoolToObject(o, "order_valid", monster_squad_order_valid(m));\n'
        '            cJSON_AddNumberToObject(o, "home_y", m->routine.home_y);\n'
        '            cJSON_AddNumberToObject(o, "home_x", m->routine.home_x);\n'
        '            cJSON_AddNumberToObject(o, "partition", level_partition_index_for_point(m->fy, m->fx));\n'
        '            cJSON_AddBoolToObject(o, "icky", (cave_info[m->fy][m->fx] & CAVE_ICKY) != 0);\n'
        '            cJSON_AddBoolToObject(o, "home_icky", (cave_info[m->routine.home_y][m->routine.home_x] & CAVE_ICKY) != 0);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(o, "race", m->r_idx);',
        'cJSON_AddNumberToObject(o, "race", m->r_idx);\n'
        '            cJSON_AddBoolToObject(o, "missile", (r->flags4 & RF4_ARCHERY_MASK) != 0);\n'
        '            cJSON_AddNumberToObject(o, "ranged_frequency", r->freq_ranged);\n'
        '            cJSON_AddNumberToObject(o, "command_grade", r->command_grade);\n'
        '            cJSON_AddNumberToObject(o, "command_style", r->command_style);\n'
        '            cJSON_AddNumberToObject(o, "command_kin", r->command_kin);\n'
        '            cJSON_AddNumberToObject(o, "command_authority", r->command_authority);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(root, "player_turn", playerturn);',
        'cJSON_AddNumberToObject(root, "player_turn", playerturn);\n'
        '        cJSON_AddNumberToObject(root, "door_probe", cave_feat[20][39]);\n'
        '        cJSON_AddNumberToObject(root, "hazard_probe", cave_feat[20][37]);\n'
        '        cJSON_AddStringToObject(root, "question", g_touch_pane_yes_no_prompt_active ? g_touch_pane_yes_no_prompt_text : "");')
    play.FIXTURE = play.FIXTURE.replace('    fprintf(file,',
        '    monster_type before_move = *m;\n'
        '    before_move.fy = old_y; before_move.fx = old_x;\n'
        '    fprintf(file,', 1)
    event_tail = 'sees_player\\":%d}\\n",'
    assert event_tail in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(event_tail, 'sees_player\\":%d,\\"saw_before_move\\":%d,\\"sense\\":%d,\\"home_y\\":%d,\\"home_x\\":%d}\\n",')
    play.FIXTURE = play.FIXTURE.replace(
        'm->min_range, m->best_range, monster_ai_can_see_player(m));',
        'm->min_range, m->best_range, monster_ai_can_see_player(m),\n'
        '        monster_has_sight(&before_move), m->ai.sense.kind, m->routine.home_y, m->routine.home_x);')
    play.FIXTURE = play.FIXTURE.replace(
        r'\"home_x\":%d}\n",',
        r'\"home_x\":%d,\"projection\":%d}\n",')
    play.FIXTURE = play.FIXTURE.replace(
        'm->ai.sense.kind, m->routine.home_y, m->routine.home_x);',
        'm->ai.sense.kind, m->routine.home_y, m->routine.home_x,\n'
        '        projectable(m->fy, m->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK));')
    play.FIXTURE = play.FIXTURE.replace(
        '    bool committed = __real_make_attack_ranged(m, attack);',
        '    action_event("ranged_begin", m, attack, m->fy, m->fx, false, false);\n'
        '    bool committed = __real_make_attack_ranged(m, attack);')
    play.FIXTURE = play.FIXTURE.replace('    cJSON* monsters = cJSON_GetObjectItemCaseSensitive(spec, "monsters");', r'''
    cJSON* icky = cJSON_GetObjectItemCaseSensitive(spec, "icky");
    cJSON_ArrayForEach(entry, icky) {
        int y1 = number(entry, "y1", 1), x1 = number(entry, "x1", 1);
        int y2 = number(entry, "y2", y1), x2 = number(entry, "x2", x1);
        assert(in_bounds(y1, x1) && in_bounds(y2, x2));
        for (int y = y1; y <= y2; ++y) for (int x = x1; x <= x2; ++x)
            cave_info[y][x] |= CAVE_ICKY;
    }
    if (number(spec, "lair_partition", 0)) {
        partition_meta_save meta = {0};
        meta.grid_rows = 1; meta.grid_cols = 2; meta.partition_count = 2;
        meta.modes[0] = QUAD_MODE_BIG_CAVE; meta.modes[1] = QUAD_MODE_ROOMY;
        level_partition_meta_set(&meta);
    }
    cJSON* monsters = cJSON_GetObjectItemCaseSensitive(spec, "monsters");''')
    play.FIXTURE = play.FIXTURE.replace('    cJSON_Delete(spec);', r'''
    cJSON* scents = cJSON_GetObjectItemCaseSensitive(spec, "scents");
    if (cJSON_IsArray(scents)) {
        scent_restore_begin();
        cJSON_ArrayForEach(entry, scents) {
            int y = number(entry, "y", -1), x = number(entry, "x", -1);
            int age = number(entry, "age", -1);
            assert(in_bounds_fully(y, x) && age >= 0 && age <= SMELL_STRENGTH);
            scent_restore_cell(y, x, (byte)(age + 1));
        }
    }
    cJSON_Delete(spec);''')
    anchor = '    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {'
    resume = r'''
    const char* resume = SDL_getenv("AI_PLAYTEST_LOAD");
    if (resume && *resume) {
        player_wipe();
        SDL_strlcpy(savefile, resume, sizeof(savefile));
        assert(load_player() && character_loaded && character_dungeon);
        op_ptr->delay_factor = 0;
        rp_ptr = &p_info[p_ptr->prace];
        current_character_profile = &c_info[p_ptr->pcharacter];
        flavor_init();
        character_generated = true;
        character_icky = character_xtra = 0;
        p_ptr->playing = p_ptr->restoring = true;
        tutorial_set_enabled(false);
        set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
        sdl_config_set_default_movement_bindings(&config, SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        sdl_config_mark_keyboard_preset_prompt_seen();
        sdl_config_mark_mouse_tutorial_seen();
        sdl_config_mark_touch_tutorial_seen();
        sdl_config_mark_character_wheel_tutorial_seen();
        use_sound = false;
        op_ptr->level_entry_narrative_mode = 0;
        set_sdl_show_context_square_popups(false);
        screen_set_startup_supporting_panes_hidden(false);
        screen_set_startup_touch_pane_hidden(false);
        sdl_welcome_screen_hide();
        p_ptr->update |= PU_BONUS | PU_TORCH | PU_HP | PU_MANA | PU_UPDATE_VIEW | PU_MONSTERS;
        update_stuff();
        p_ptr->redraw |= PR_BASIC | PR_EXTRA | PR_MAP;
        Term_flush();
        inkey_next_set(NULL);
        inkey_xtra = false;
        dungeon();
        sdl_control_shutdown();
        return 0;
    }
'''
    play.adapter.HARNESS = play.adapter.HARNESS.replace(anchor, resume + anchor)


def actor(race: int, x: int, y: int = 20, **fields) -> dict:
    return dict(race=race, x=x, y=y, heard=1, morale=0, **fields)


def scenarios() -> list[tuple[str, dict, list[str], str]]:
    cases = []
    def add(name, spec, count=36, keys=None, check="activity"):
        cases.append((name, spec, keys if keys is not None else ["KP5"] * count, check))
    corridor = [dict(y1=1, y2=38, x1=1, x2=62, feat=63),
                dict(y1=20, x1=20, x2=50, feat=1)]
    room = dict(y1=15, y2=25, x1=36, x2=46, feat=1)
    orcs = [actor(91, 33), actor(41, 34), actor(61, 31), actor(51, 30)]
    add("orc-doorway-deployment", dict(tiles=corridor+[room, dict(y1=20,x1=35,feat=32)],
        monsters=orcs), 44, check="deployed_archery")
    add("orc-locked-doorway-deployment", dict(tiles=corridor+[room, dict(y1=20,x1=35,feat=33)],
        monsters=orcs), 44, check="deployed_archery")
    add("easterling-doorway-deployment", dict(tiles=corridor+[room,dict(y1=20,x1=35,feat=32)],
        monsters=[actor(409,33),actor(82,34),actor(82,31),actor(103,30)]), 44,
        check="deployed_archery")
    add("captain-killed-in-combat", dict(melee=50, monsters=[
        actor(91,39,hp_percent=10),dict(actor(41,36,19),morale=1000),
        dict(actor(61,36,21),morale=1000),dict(actor(51,35,18),morale=1000)]),
        keys=["KP4"]*2+["KP5"]*32, check="succession")
    for status in ("confused", "stunned"):
        add(f"captain-recovers-{status}", dict(monsters=[
            actor(91,37,**{status:6}),actor(41,37,19),actor(61,37,21),actor(51,36,22)]),
            32, check="recovery_command")
    add("two-commanders-nearby", dict(evasion=22,monsters=[
        actor(91,36,18),actor(41,36,17),actor(51,33,18),
        actor(409,36,22),actor(82,36,23),actor(103,33,22)]),40,check="two_squads")
    add("shadow-cross-kin-squad", dict(evasion=24,monsters=[
        actor(409,36),actor(408,36,18),actor(82,35,22),actor(103,33,19),actor(103,33,21)]),
        keys=["KP5"]*10+(["KP8"]*3+["KP6"]*3+["KP2"]*3+["KP4"]*3)*2,
        check="moving_archery")
    for fuel,glow in [(12,0),(12,1)]:
        add(f"archers-torch-expires-glow-{glow}",dict(torch_fuel=fuel,glow=glow,
            monsters=[actor(103,37),actor(82,36,18)]),32,check="fuel")
    add("disguised-player-archery",dict(abilities=[dict(skill=3,ability=0)],
        monsters=[actor(103,35),actor(91,34,18),actor(51,33,22)]),36,check="archery")
    add("crowded-pillars-moving-player",dict(evasion=22,tiles=[
        dict(y1=18,y2=22,x1=35,feat=56),dict(y1=15,y2=17,x1=41,feat=56)],
        monsters=[actor(91,37),actor(41,33,17),actor(61,33,23),actor(51,32,18),actor(51,32,22)]),
        keys=["KP5"]*12+(["KP6"]*3+["KP8"]*4+["KP4"]*3+["KP2"]*4)*2,
        check="moving_archery")
    add("poison-pool-mixed-squad",dict(evasion=22,tiles=[
        dict(y1=18,y2=22,x1=33,x2=35,feat=87)],monsters=[
        actor(91,36),actor(41,34,19,exposed=1),actor(61,34,21,exposed=1),
        actor(51,33,20,exposed=1)]),36,check="pool_escape")
    add("grotesque-los-loss",dict(tiles=[dict(y1=15,y2=24,x1=42,feat=56)],
        monsters=[actor(163,37)]),keys=["KP5"]*4+["KP6"]+["KP8"]*7+["KP6"]*3+["KP5"]*20,
        check="unwatched")
    for race,name in [(154,"cat"),(11,"wolf"),(405,"dragon")]:
        add(f"{name}-turning-pursuit",dict(stealth=20,evasion=22,player_y=10,player_x=35,tiles=[
            dict(y1=1,y2=38,x1=1,x2=62,feat=63),
            dict(y1=10,x1=8,x2=42,feat=1),dict(y1=10,y2=25,x1=42,feat=1),
            dict(y1=25,x1=8,x2=42,feat=1)],monsters=[dict(actor(race,20,10),morale=1000)]),
            keys=["KP6"]*7+["KP2"]*15+["KP4"]*18+["KP5"]*16,
            check="territorial_guard" if race==405 else "tracking")
        if race==405:
            # A remote sound without a local trail still does not draw a
            # territorial dragon away from its hoard.
            old_name,spec,keys,check=cases.pop()
            cases.append(("dragon-territorial-guard",spec,keys,check))
    add("legacy-brood-reinforcements",dict(evasion=24,monsters=[actor(44,36)]),
        36,check="brood")
    add("legacy-gorthaur-oaths",dict(evasion=24,will=15,monsters=[actor(244,34)]),
        keys=["KP6"]*8+["KP8"]*8+["KP5"]*10,check="oaths")
    add("legacy-watcher-proximity",dict(monsters=[actor(203,38)]),
        keys=["KP5"]*6+["KP6"]*5+["KP5"]*6,check="screech")
    add("legacy-wight-darkness",dict(evasion=24,will=50,monsters=[actor(75,37)]),
        44,check="darkness")
    cases.extend(dragon_lair_cases())
    return cases


def prepared_lair(race: int = 405, home_x: int = 35, **extra) -> dict:
    return dict(player_x=58,evasion=30,stealth=30,will=30,
        monsters=[dict(actor(race,home_x),morale=1000)],
        scents=[dict(y=20,x=x,age=22-(x-home_x)) for x in range(home_x,home_x+12)],
        **extra)


def dragon_lair_cases() -> list[tuple]:
    ids=[124,164,176,181,195,202,215,222,233,243,245,246,402,405]
    local=dict(player_y=18,player_x=35,evasion=30,stealth=25,will=30,
        allow_death=True,tiles=[dict(y1=19,y2=23,x1=36,feat=56)])
    cases=[(f"dragon-lair-local-{race}",dict(local,
        monsters=[dict(actor(race,35,20),morale=1000)]),
        ["KP5"]+["KP6"]*3+["KP2"]*2+["KP5"]*10,"lair_local") for race in ids]
    cases += [
        ("dragon-lair-radius",prepared_lair(),["KP5"]*30,"lair_boundary"),
        ("dragon-lair-vault",prepared_lair(icky=[dict(y1=17,y2=23,x1=32,x2=38)]),
            ["KP5"]*30,"lair_vault"),
        ("dragon-lair-partition",prepared_lair(home_x=30,lair_partition=1),
            ["KP5"]*30,"lair_partition"),
        ("dragon-lair-water-gap",prepared_lair(tiles=[dict(y1=20,x1=37,x2=39,feat=84)]),
            ["KP5"]*30,"lair_water"),
        ("dragon-lair-expired",dict(prepared_lair(),scents=[dict(y=20,x=35,age=80)]),
            ["KP5"]*16,"lair_expired"),
    ]
    return cases


def read_run(result: dict) -> tuple[list[dict], list[dict]]:
    profile=Path(result["profile"])
    frames=json.loads((profile/"trace.json").read_text())
    events=[json.loads(line) for line in (profile/"actions.jsonl").read_text().splitlines()]
    return frames,events


def dist(frame: dict, m: dict) -> int:
    dy=abs(frame["player"]["y"]-m["y"]); dx=abs(frame["player"]["x"]-m["x"])
    return max(dy,dx)+min(dy,dx)//2


def verify(result: dict, check: str) -> None:
    frames,events=read_run(result)
    shots=[e for e in events if e["kind"]=="ranged" and e["detail"] in (96,97,98)]
    assert all(e["sees_player"] for e in shots), "Missile fired without personal recognition"
    assert all(not any(m["knowledge"]) for f in frames for m in f["monsters"] if m["mindless"])
    for frame in frames:
        commanders={m["commander"] for m in frame["monsters"] if m["order_valid"]}
        for commander in commanders:
            orders=[(m["order_y"],m["order_x"]) for m in frame["monsters"]
                    if m["commander"]==commander and m["order_valid"]]
            assert len(orders)==len(set(orders)), "One squad reserves overlapping positions"
    if check in ("archery","deployed_archery","moving_archery"):
        assert shots,"Archers never fire"
        if check=="deployed_archery":
            assert any(e["player_turn"]>=frames[0]["player_turn"]+20 for e in shots),"Archers stop contributing after deployment"
            assert any(sum(dist(f,m)==1 for m in f["monsters"])>=2 for f in frames),"Front line never deploys"
    elif check=="succession":
        assert not any(m["race"]==91 for m in frames[-1]["monsters"]),"Captain not killed"
        assert any(m["role"]==1 and m["race"]!=91 for f in frames[3:] for m in f["monsters"]),"No successor"
    elif check=="recovery_command":
        assert any(m["race"]!=91 and m["role"]==1 for f in frames for m in f["monsters"]),"No replacement during incapacity"
        assert any(m["race"]==91 and m["role"]==1 and not m["confused"] and not m["stunned"]
                   for f in frames[8:] for m in f["monsters"]),"Recovered captain never resumes command"
    elif check=="two_squads":
        assert any(len({m["commander"] for m in f["monsters"] if m["order_valid"]})>=2 for f in frames),"No simultaneous commands"
    elif check=="fuel":
        assert frames[0]["player"]["fuel"]>0 and frames[-1]["player"]["fuel"]==0
        assert any(e["kind"] in ("melee","ranged") for e in events),"No engagement through lighting change"
    elif check=="pool_escape":
        for initial in frames[0]["monsters"]:
            if initial["feat"]!=87:continue
            assert any(m["index"]==initial["index"] and m["feat"]!=87 and m["hp"]>0
                       for f in frames[1:8] for m in f["monsters"]),"Squad member cannot leave pool"
    elif check=="unwatched":
        assert any(not m["visible"] and (m["y"],m["x"])!=(20,37)
                   for f in frames for m in f["monsters"]),"Grotesque never advances when unseen"
    elif check=="tracking":
        assert any(not m["sees_player"] and m["sense"] in (3,4)
                   for f in frames for m in f["monsters"]),"No nonvisual pursuit"
        assert any(m["x"]==42 and m["y"]>10 for f in frames for m in f["monsters"]),"Tracker never rounds corner"
    elif check=="territorial_guard":
        initial=frames[0]["monsters"][0]
        assert all(not m["sees_player"] and (m["y"],m["x"])==(initial["y"],initial["x"])
                   for f in frames for m in f["monsters"]),"Territorial dragon pursues an unseen remote player"
    elif check=="brood":
        assert any(e["kind"]=="ranged" and e["detail"]==112 for e in events),"No paid hatch action"
        assert any(len(f["monsters"])>1 for f in frames),"Hatch creates no reinforcements"
    elif check=="oaths":
        assert any(e["kind"]=="ranged" and e["detail"]==116 for e in events),"Oaths never starts"
        assert any(m["race"]==153 for f in frames for m in f["monsters"]),"Oaths summons no oathwraiths"
    elif check=="screech":
        assert any(e["kind"]=="ranged" and e["detail"]==105 for e in events),"Nearby watcher never screeches"
        assert all((e["y"],e["x"])==(20,38) for e in events if e["kind"]=="action")
        assert not any(e["kind"]=="ranged" and e["player_turn"]>=frames[0]["player_turn"]+12
                       for e in events),"Watcher attacks beyond short sight"
    elif check=="darkness":
        assert any(e["kind"]=="ranged" and e["detail"]==106 for e in events),"Wight never uses darkness"
    elif check.startswith("lair_"):
        scent_moves=[e for e in events if e["kind"]=="action" and e["sense"] in (3,4)
                     and not e["saw_before_move"] and (e["y"],e["x"])!=(e["old_y"],e["old_x"])]
        for event in scent_moves:
            dy=abs(event["y"]-event["home_y"]);dx=abs(event["x"]-event["home_x"])
            assert max(dy,dx)+min(dy,dx)//2<=6,"Scent pursuit leaves the lair radius"
        if check=="lair_expired":
            assert result["moves"]==0,"Expired scent starts a pursuit"
            return
        assert scent_moves,"No actual unseen scent movement"
        if check=="lair_local":return
        assert all(not m["sees_player"] for f in frames for m in f["monsters"]),"Remote boundary fixture becomes a visible fight"
        for frame in frames:
            for m in frame["monsters"]:
                assert m["icky"]==m["home_icky"],"Scent pursuit crosses a vault boundary"
                if m["territory"]:
                    assert m["partition"]==m["territory"]-1,"Scent pursuit crosses its partition"
        if check=="lair_boundary":
            assert any(e["x"]==41 for e in scent_moves),"Dragon does not reach the permitted boundary"
        if check=="lair_vault":
            assert max(m["x"] for f in frames for m in f["monsters"])<=38
        if check=="lair_water":
            assert max(m["x"] for f in frames for m in f["monsters"])<40,"Dragon follows an erased trail across a wide water gap"


def close_game(process, game) -> None:
    if process.poll() is not None:return
    try:
        reply=game.key("Ctrl+x",capture=False)
        if not reply["ok"]:
            raise play.ControlError(reply.get("error","Cannot close the test session"))
        process.wait(timeout=5)
    except (play.ControlError,subprocess.TimeoutExpired):
        process.terminate();process.wait(timeout=5)


def wait_loaded(profile: Path, process, game) -> dict:
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        assert process.poll() is None,f"Load exited {process.returncode}: {profile}"
        if (profile/"telemetry.json").exists() and (game.directory/"session.json").exists():
            response=game.observe(capture=False)
            assert response["ok"]
            if response["state"]["dungeon"] and response["state"]["waiting_for_input"]:
                return play.telemetry(profile)
        time.sleep(.025)
    raise AssertionError(f"Loaded dungeon never becomes ready: {profile}")


def loaded_branch(exe: Path, env: dict, checkpoint: Path, save_name: str,
                  name: str, keys: list[str]) -> dict:
    profile=OUT/(name+"-"+uuid.uuid4().hex[:8]);profile.mkdir()
    for folder in ("save","data","user","meta","bone"):
        source=checkpoint/folder
        if source.exists():shutil.copytree(source,profile/folder)
    game=play.GameControl(profile/"control",timeout=30)
    process=subprocess.Popen([str(exe),"--windowed","--control-dir",str(game.directory)],cwd=ROOT,
        env=dict(env,CONTROL_TEST_PROFILE=str(profile),
                 AI_PLAYTEST_LOAD=str(profile/"save"/save_name)),
        stdout=(profile/"stdout.txt").open("w"),stderr=(profile/"stderr.txt").open("w"),
        creationflags=subprocess.CREATE_NO_WINDOW if os.name=="nt" else 0)
    frames=[]
    try:
        initial=wait_loaded(profile,process,game)
        frames.append(dict(key="loaded",**play.brief(initial)))
        game.observe(output=profile/"initial.png")
        for key in keys:
            reply=game.key(key,capture=False)
            assert reply["ok"] and not reply["closed"],f"Replay closed: {profile}"
            assert reply["state"]["video_driver"]=="dummy"
            frame=dict(key=key,**play.brief(play.telemetry(profile)))
            frames.append(frame)
            positions=[(m["y"],m["x"]) for m in frame["monsters"]]
            assert len(positions)==len(set(positions))
            assert (frame["player"]["y"],frame["player"]["x"]) not in positions
        game.observe(output=profile/"final.png")
        result=dict(name=name,profile=str(profile),initial=frames[0],final=frames[-1],
            actions=len(keys),player_turns=frames[-1]["player_turn"]-frames[0]["player_turn"])
        result.update(play.validate(profile,{},frames))
        return result
    finally:
        (profile/"trace.json").write_text(json.dumps(frames,indent=2))
        close_game(process,game)


def persistence_cases() -> list[tuple]:
    scent=next(s for n,s,k,c in scenarios() if n=="wolf-turning-pursuit")
    return [
        ("pursuing-squad",dict(monsters=[actor(91,35),actor(41,34,18),actor(61,34,22),actor(51,33,21)]),
         ["KP5"]*10,["KP6"]*3+["KP8"]*3+["KP5"]*12,"squad"),
        ("scent-around-corner",scent,["KP6"]*7+["KP2"]*12,["KP2"]*3+["KP4"]*12+["KP5"]*8,"scent"),
        ("observed-fire-resistance",dict(fire_resistance=1,recognition=0,monsters=[actor(181,35)]),
         ["KP5"]*12,["KP5"]*12,"learning"),
        ("smite-recovery",dict(monsters=[actor(411,39)]),["KP5"]*2,["KP5"]*8,"smite"),
        ("bridge-worker",dict(environment=1,player_x=50,tiles=[
            dict(y1=1,y2=38,x1=30,feat=63),dict(y1=20,x1=13,feat=88)],
            environment_cells=[dict(y=20,x=13,flags=16,bridge=88,underlay=84,material=1,integrity=40)],
            monsters=[actor(41,12,alertness=-10)]),["KP5"]*6,["KP5"]*16,"bridge"),
        ("dragon-lair-scent",prepared_lair(),["KP5"]*3,["KP5"]*18,"dragon_lair"),
        ("dragon-lair-search",prepared_lair(),["KP5"]*8,["KP5"]*12,"dragon_search"),
        ("dragon-vault-scent",prepared_lair(icky=[dict(y1=17,y2=23,x1=32,x2=38)]),
         ["KP5"]*2,["KP5"]*18,"dragon_vault"),
        ("dragon-partition-scent",prepared_lair(home_x=30,lair_partition=1),
         ["KP5"]*2,["KP5"]*18,"dragon_partition"),
    ]


def persistent_monsters(frame: dict) -> list[tuple]:
    fields=("race","y","x","hp","max_hp","mana","energy","poison","recovery","vengeance",
            "concentration","alertness","sense","sense_y","sense_x","cast_reserve",
            "search","home_y","home_x","territory","icky","home_icky")
    return sorted(tuple(m[k] for k in fields)+(tuple(m["knowledge"]),) for m in frame["monsters"])


def run_persistence(exe: Path, env: dict, name: str, spec: dict,
                    prefix: list[str], suffix: list[str], check: str) -> dict:
    profile,process,game=play.start(exe,env,"checkpoint-"+name,spec)
    frames=[]
    try:
        frames.append(dict(key="initial",**play.brief(play.telemetry(profile))))
        for key in prefix:
            reply=game.key(key,capture=False);assert reply["ok"] and not reply["closed"]
            frames.append(dict(key=key,**play.brief(play.telemetry(profile))))
        if check=="smite":
            for _ in range(14):
                if any(m["recovery"]==1 for m in frames[-1]["monsters"]):break
                reply=game.key("KP5",capture=False);assert reply["ok"] and not reply["closed"]
                frames.append(dict(key="KP5",**play.brief(play.telemetry(profile))))
            assert any(m["recovery"]==1 for m in frames[-1]["monsters"]),"No Smite checkpoint"
        if check=="dragon_search":
            for _ in range(16):
                if any(m["sense"]==4 for m in frames[-1]["monsters"]):break
                reply=game.key("KP5",capture=False);assert reply["ok"] and not reply["closed"]
                frames.append(dict(key="KP5",**play.brief(play.telemetry(profile))))
            assert any(m["sense"]==4 for m in frames[-1]["monsters"]),"No local search checkpoint"
        reply=game.key("Ctrl+s",output=profile/"checkpoint.png")
        assert reply["ok"] and not reply["closed"]
        before=play.telemetry(profile)
        save=Path(before["savefile"])
        assert save.resolve().is_relative_to(profile.resolve()) and save.is_file()
    finally:
        (profile/"trace.json").write_text(json.dumps(frames,indent=2))
        close_game(process,game)
    first=loaded_branch(exe,env,profile,save.name,name+"-load-a",suffix)
    second=loaded_branch(exe,env,profile,save.name,name+"-load-b",suffix)
    assert first["initial"]["player_turn"]==before["player_turn"]
    assert first["initial"]["player"]==before["player"],"Loaded player state differs"
    assert persistent_monsters(first["initial"])==persistent_monsters(before),"Loaded monster state differs"
    assert first["final"]["player"]==second["final"]["player"],"Identical loaded inputs change player outcomes"
    assert persistent_monsters(first["final"])==persistent_monsters(second["final"]),"Identical loaded inputs change monster outcomes"
    frames_after,events=read_run(first)
    second_frames,second_events=read_run(second)
    assert len(frames_after)==len(second_frames)
    for a,b in zip(frames_after,second_frames):
        assert a["player"]==b["player"] and a["player_turn"]==b["player_turn"]
        assert persistent_monsters(a)==persistent_monsters(b),"Loaded replays diverge before the final turn"
    assert events==second_events,"Identical loaded inputs produce different scheduled actions"
    if check=="squad":
        assert any(m["order_valid"] for f in frames_after[2:] for m in f["monsters"])
        assert any(e["kind"]=="ranged" for e in events)
    elif check=="scent":
        assert any(not m["sees_player"] and m["sense"] in (3,4) for f in frames_after for m in f["monsters"])
        assert len({(m["y"],m["x"]) for f in frames_after for m in f["monsters"]})>=4
    elif check=="learning":
        assert any(m["knowledge"][0]>0 for m in first["initial"]["monsters"])
    elif check=="smite":
        action=next(e for e in events if e["kind"]=="action")
        assert action["skipped"] and action["recovery"]==2 and not action["melee"]
    elif check=="bridge":
        assert first["final"]["bridges"][0]["integrity"]==100
    elif check in ("dragon_lair","dragon_search","dragon_vault","dragon_partition"):
        assert any(m["sense"] in (3,4) for m in first["initial"]["monsters"])
        for frame in frames_after:
            for m in frame["monsters"]:
                dy=abs(m["y"]-m["home_y"]);dx=abs(m["x"]-m["home_x"])
                assert max(dy,dx)+min(dy,dx)//2<=6 and not m["sees_player"]
                if check=="dragon_vault":
                    assert m["icky"] and m["home_icky"] and m["x"]<=38
                if check=="dragon_partition":
                    assert m["territory"]==1 and m["partition"]==0
        if check=="dragon_lair":
            assert max(m["x"] for f in frames_after for m in f["monsters"])==41
        elif check=="dragon_search":
            assert first["final"]["monsters"][0]["sense"]==0,"Saved search never exhausts"
    return dict(name=name,checkpoint=str(profile),save_sha256=hashlib.sha256(save.read_bytes()).hexdigest(),
        before=play.brief(before),branches=[first,second],check=check,passed=True,
        fixture_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),build_signature=env["AI_PLAYTEST_SIGNATURE"])


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed",type=int,default=20261114)
    parser.add_argument("--filter",default="")
    parser.add_argument("--persistence",action="store_true")
    parser.add_argument("--report",type=Path,default=OUT/"matrix-result.json")
    args=parser.parse_args()
    instrument()
    exe,env=play.build()
    env["AI_PLAYTEST_SIGNATURE"]=hashlib.sha256((env["AI_PLAYTEST_SIGNATURE"]+
        Path(__file__).read_text()).encode()).hexdigest()
    results=[];failures=[]
    if args.persistence:
        if args.report==OUT/"matrix-result.json":args.report=OUT/"persistence-result.json"
        for name,spec,prefix,suffix,check in persistence_cases():
            if args.filter and not re.search(args.filter,name):continue
            try:
                result=run_persistence(exe,env,name,dict(spec,seed=args.seed),prefix,suffix,check)
                results.append(result)
                print(json.dumps(dict(name=name,passed=True,checkpoint=result["checkpoint"])),flush=True)
            except Exception as exc:
                failures.append(dict(name=name,error=str(exc),exception=type(exc).__name__,traceback=traceback.format_exc()))
                print(json.dumps(failures[-1]),flush=True)
            args.report.parent.mkdir(parents=True,exist_ok=True)
            args.report.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
        print(f"Persistent encounters: {len(results)} passed, {len(failures)} failed",flush=True)
        if failures:raise SystemExit(1)
        return
    for name,spec,keys,check in scenarios():
        if args.filter and not re.search(args.filter,name):continue
        spec=dict(spec,seed=args.seed)
        try:
            result=play.run(exe,env,name,spec,keys)
            verify(result,check)
            result["outcome_check"]=check
            results.append(result)
        except Exception as exc:
            failures.append(dict(name=name,error=str(exc),exception=type(exc).__name__,traceback=traceback.format_exc()))
            print(json.dumps(failures[-1]),flush=True)
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
    print(f"Compound encounters: {len(results)} passed, {len(failures)} failed",flush=True)
    if failures:raise SystemExit(1)


if __name__=="__main__":main()
