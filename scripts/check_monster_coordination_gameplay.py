#!/usr/bin/env python3
"""Exercise commander/archer outcomes through real headless SDL inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_extended_gameplay as engine

ROOT = engine.ROOT
OUT = ROOT / "scripts/output/monster-coordination"


def troop(race, x, y=20, **changes):
    return dict(engine.actor(race, x, y), **changes)


def cases():
    result = []
    def add(name, spec, turns=32, keys=None, check="archery"):
        result.append((name, spec, keys if keys is not None else ["KP5"]*turns, check))
    for family, commander, archer in [("orc",91,51),("easterling",409,103)]:
        add(f"{family}-ordinary-pair",dict(monsters=[
            troop(commander,37),troop(archer,36)]),26)
        add(f"{family}-challenge-pair",dict(song=1,song_skill=35,evasion=20,monsters=[
            troop(commander,37),troop(archer,36)]),26,check="challenge_melee")
        add(f"{family}-weak-challenge-pair",dict(song=1,song_skill=0,evasion=24,monsters=[
            troop(commander,37),troop(archer,36)]),26,check="challenge_melee")
    add("challenge-mixed-morale",dict(song=1,song_skill=35,evasion=24,monsters=[
        troop(91,37),troop(51,36,19),troop(51,36,21,hp_percent=55)]),26,
        check="challenge_melee")
    add("challenge-scattered-squad",dict(song=1,song_skill=35,evasion=24,monsters=[
        troop(91,35),troop(41,36,19),troop(61,31,21),troop(51,34,22)]),28,
        check="challenge_melee")
    add("challenge-stops-moving-target",dict(song=1,song_skill=35,evasion=24,monsters=[
        troop(91,37),troop(51,36)]),keys=["KP5"]*16+["s","s"]+["KP6"]*4+["KP5"]*12,
        check="challenge_ends")
    # Challenge costs one Voice every three turns; exercise actual exhaustion.
    add("challenge-voice-exhaustion",dict(song=1,song_skill=35,evasion=24,monsters=[
        troop(91,37),troop(51,36)]),keys=["KP5"]*105+["KP6"]*4+["KP5"]*12,
        check="challenge_ends")
    add("wounded-ally-withdrawal-cover",dict(evasion=24,monsters=[
        troop(91,36),troop(61,38,hp_percent=40,morale=-180,stance=1),
        troop(61,36,21,morale=100),troop(51,35,18)]),36,check="cover")
    add("crowded-orc-squad",dict(evasion=26,monsters=[
        troop(91,36),troop(41,36,17),troop(41,36,19),troop(41,36,21),
        troop(61,36,23),troop(61,34,18),troop(61,34,22),
        troop(51,33,17),troop(51,33,20),troop(51,33,23)]),38,check="crowd")
    add("two-orc-captains",dict(evasion=24,monsters=[
        troop(91,37,18),troop(76,36,22),troop(61,36,17),
        troop(41,36,23),troop(51,34,18),troop(51,34,22)]),36,
        check="captains")
    add("fankil-and-orcs-shared-band",dict(evasion=24,monsters=[
        troop(409,37,18,group=8),troop(82,36,17,group=8),
        troop(103,35,18,group=8),troop(91,37,22,group=8),
        troop(41,36,23,group=8),troop(51,35,22,group=8)]),36,
        check="authority")
    corridor=[dict(y1=1,y2=38,x1=1,x2=62,feat=63),
              dict(y1=20,x1=24,x2=50,feat=1),
              dict(y1=15,y2=25,x1=36,x2=46,feat=1),
              dict(y1=20,x1=35,feat=32)]
    add("challenge-doorway",dict(evasion=24,song=1,song_skill=35,tiles=corridor,
        monsters=[troop(91,34),troop(41,33),troop(51,31)]),28,
        check="challenge_melee")
    add("archers-moving-with-room-exit",dict(evasion=26,tiles=[
        dict(y1=15,y2=25,x1=43,feat=56),dict(y1=23,x1=43,feat=4)],
        monsters=[troop(409,36),troop(82,36,18),troop(82,36,22),
                  troop(103,34,19),troop(103,34,21)]),
        keys=["KP5"]*10+["KP6"]*5+["KP2"]*5+["KP4"]*5+["KP8"]*5+["KP5"]*10,
        check="moving")
    return result


def varied_cases():
    """Different authored leaders, routes, contact changes and action speeds."""
    result = []
    def add(name, monsters, turns=42, keys=None, **fields):
        check = fields.pop("check", {})
        spec = dict(evasion=35, monsters=monsters, **fields)
        result.append((name, spec, keys if keys is not None else ["KP5"]*turns, check))

    add("varied-uldor-ranged-command", [
        troop(115,36),troop(82,37,18),troop(82,37,22),
        troop(103,34,19),troop(103,34,21),troop(121,33)],
        check=dict(shots="all", melee=True))
    add("varied-baugon-ranged-command", [
        troop(76,37),troop(41,37,18),troop(61,37,22),
        troop(51,34,19),troop(51,34,21)],check=dict(shots="all",melee=True))
    for commander in (165,185):
        add(f"varied-cats-command-{commander}", [
            troop(commander,36),troop(177,37,18),troop(154,37,22),
            troop(175,34,19),troop(175,34,21)],check=dict(shots="all",melee=True))
    add("varied-giants-boulder-line", [
        troop(135,36,18),troop(146,36,22),troop(114,34,19),troop(114,34,21)],
        turns=52,check=dict(shots="any"))
    for commander in (186,241):
        add(f"varied-balrog-cross-kin-{commander}", [
            troop(commander,36),troop(214,36,18),troop(91,36,22),
            troop(61,35,23),troop(51,33,19),troop(51,33,21)],
            allow_death=True,check=dict(shots="all",melee=True))
    add("varied-ranged-command-challenge", [
        troop(76,37),troop(61,36,19),troop(51,35,21)],
        song=1,song_skill=28,check=dict(challenge=True,melee=True))
    add("varied-uldor-killed", [
        troop(115,39,hp_percent=10),troop(82,36,18,morale=1000),
        troop(121,36,22,morale=1000),troop(103,34,19,morale=1000),
        troop(103,34,21,morale=1000)],melee=50,
        keys=["KP4"]*2+["KP5"]*36,check=dict(shots="any",killed=115))
    add("varied-captain-flees", [
        troop(91,37,hp_percent=30,morale=-300,stance=1),
        troop(81,36,19,morale=1000),troop(61,36,21,morale=1000),
        troop(51,34,18,morale=1000),troop(51,34,22,morale=1000)],
        check=dict(shots="all",melee=True,fleeing=1))
    add("varied-unwary-rear-archers", [
        troop(91,37),troop(41,36,19),
        troop(51,35,21,alertness=-10,heard=0),
        troop(51,34,18,alertness=-10,heard=0)],check=dict(shots="any",melee=True))
    archers=[troop(51,35+i%2,15+i) for i in range(11)]
    add("varied-eleven-archer-volley",archers,turns=48,check=dict(shots="all"))
    flood=[troop(54,35,16),troop(91,35,24),troop(76,34,20)]
    flood += [troop(41 if i%2 else 61,36,15+i) for i in range(10)]
    flood += [troop(51,32,16+i*2) for i in range(5)]
    add("varied-eighteen-orc-pincer",flood,turns=52,
        check=dict(shots="any",melee=True,allow_surrounded=True))
    moving=["KP5"]*8+["KP6"]*6+["KP2"]*4+["KP4"]*6+["KP8"]*4+["KP5"]*18
    add("varied-moving-uldor-volley",[
        troop(115,36),troop(82,37,18),troop(103,34,19),troop(103,34,21)],
        keys=moving,check=dict(shots="all"))
    add("varied-moving-cat-volley",[
        troop(165,36),troop(154,37,18),troop(175,34,19),troop(175,34,21)],
        keys=moving,check=dict(shots="all",melee=True))
    wall=[dict(y1=1,y2=38,x1=39,feat=63),dict(y1=20,x1=39,feat=4)]
    add("varied-close-door-during-pursuit",[
        troop(91,37),troop(41,36,19),troop(61,36,21),
        troop(51,34,19),troop(51,34,21)],tiles=wall,
        # A single adjacent door closes immediately; no direction prompt.
        keys=["c"]+["KP6"]*4+["KP5"]*42,
        check=dict(shots="all",melee=True,door=True))
    river=[dict(y1=1,y2=38,x1=38,feat=84),dict(y1=20,x1=38,feat=88)]
    add("varied-river-bridge-deployment",[
        troop(91,36),troop(41,36,18),troop(61,36,22),
        troop(51,33,19),troop(51,33,21)],tiles=river,
        check=dict(shots="all",melee=True))
    add("varied-threatened-firing-ground",[
        troop(91,37,19),troop(61,37,21),troop(51,37),troop(51,35,18)],
        environment=1,environment_cells=[dict(y=20,x=37,pending=85,due=160)],
        check=dict(shots="all",melee=True,hazard=True))
    add("varied-thawing-firing-ground",[
        troop(91,37,19),troop(61,37,21),troop(51,37),troop(51,35,18)],
        environment=1,tiles=[dict(y1=20,x1=37,feat=86)],
        environment_cells=[dict(y=20,x=37,pending=84,due=160)],
        check=dict(shots="all",melee=True))
    add("varied-minimal-archer-recognition",[
        troop(91,37),troop(41,36,18),troop(51,36,19),troop(51,36,21)],
        glow=0,torch_fuel=0,check=dict(melee=True,low_sight=True))
    add("varied-commander-around-wall",[
        troop(91,35,18),troop(81,36,22),troop(51,33,18),troop(51,33,22)],
        tiles=[dict(y1=17,y2=23,x1=38,feat=63)],
        keys=["KP2"]*4+["KP4"]*4+["KP8"]*8+["KP6"]*4+["KP5"]*24,
        check=dict(shots="any",melee=True))
    add("varied-separated-archer-bands",[
        troop(91,36,17,group=8),troop(51,34,16,group=8),troop(51,34,18,group=8),
        troop(54,36,23,group=9),troop(51,34,22,group=9),troop(51,34,24,group=9)],
        relations=[dict(a=8,b=9,relation=1)],check=dict(social=True))
    layouts={
        "two-door-rooms":[dict(y1=1,y2=38,x1=1,x2=62,feat=63),
            dict(y1=14,y2=26,x1=25,x2=34,feat=1),
            dict(y1=14,y2=26,x1=38,x2=48,feat=1),
            dict(y1=20,x1=34,x2=38,feat=1),
            dict(y1=20,x1=35,feat=32),dict(y1=20,x1=37,feat=32)],
        "pillared-loop":[dict(y1=18,y2=22,x1=37,feat=63),
            dict(y1=17,y2=23,x1=42,feat=63),dict(y1=20,x1=42,feat=1)],
        "deep-water-bridge":[dict(y1=10,y2=30,x1=38,x2=39,feat=100),
            dict(y1=20,x1=38,x2=39,feat=98)],
        "ice-and-poison":[dict(y1=15,y2=25,x1=37,feat=86),
            dict(y1=18,y2=22,x1=36,feat=87)],
    }
    for family,commander,front1,front2,archer in [
            ("orc",91,41,61,51),("cat",165,177,154,175)]:
        party=[troop(commander,33),troop(front1,34,18),troop(front2,34,22),
               troop(archer,31,17),troop(archer,31,23)]
        for layout,tiles in layouts.items():
            if layout=="pillared-loop":
                keys=["KP5"]*12+["KP6"]*4+["KP2"]*4+["KP4"]*8+["KP8"]*8+["KP6"]*4+["KP2"]*4+["KP5"]*20
            else:
                keys=["KP5"]*12+["KP6"]*4+["KP2"]*4+["KP4"]*4+["KP8"]*4+["KP5"]*28
            add(f"terrain-{family}-{layout}",party,tiles=tiles,keys=keys,confirm_acid=True,
                check=dict(shots="all",melee=True))
    corridor=[dict(y1=1,y2=38,x1=1,x2=62,feat=63),
              dict(y1=20,x1=24,x2=46,feat=1)]
    for feature in (85,2,100,49,87):
        add(f"warning-corridor-post-{feature}",[troop(91,29),troop(51,31)],
            turns=30,recognition=0,environment=1,tiles=corridor,
            environment_cells=[dict(y=20,x=x,pending=feature,due=10000) for x in (32,33)],
            check=dict(shots="all",warned=True))
        add(f"warning-native-detour-{feature}",[troop(41,31,morale=1000,stance=3)],
            turns=24,recognition=0,environment=1,
            environment_cells=[dict(y=y,x=x,pending=feature,due=10000)
                               for y in range(19,22) for x in range(32,35)],
            check=dict(melee=True,warned=True,solo=True))
    add("warning-native-player-endpoint",[troop(41,36,morale=1000,stance=3)],
        turns=16,recognition=0,environment=1,
        environment_cells=[dict(y=20,x=40,pending=85,due=10000)],
        check=dict(melee=True,warned=True,solo=True))
    return result


def verify_varied(result, check):
    frames,events=engine.read_run(result)
    engine.verify(result,"activity")
    # Occasional missiles supplement melee. Baugon's authored 15% bow, like
    # a giant's boulder, need not fire after he reaches useful melee contact.
    missile_ids={m["actor_id"] for m in frames[0]["monsters"]
                 if m["missile"] and m["ranged_frequency"] > 15}
    shots=[e for e in events if e["kind"]=="ranged" and e["detail"] in (96,97,98)]
    pending={}
    for event in events:
        if event["kind"]=="ranged_begin":
            pending[event["actor_id"]]=event
        elif event["kind"]=="ranged" and event["detail"] in (96,97,98):
            attempt=pending[event["actor_id"]]
            assert attempt["projection"]==2,"A missile is committed through an occupied/blocked lane"
            assert attempt["sees_player"],"A missile starts without personal recognition"
    ordered=0
    for frame in frames:
        by_id={m["index"]:m for m in frame["monsters"]}
        for m in frame["monsters"]:
            if not m["order_valid"]:continue
            ordered+=1
            leader=by_id[m["commander"]]
            assert leader["command_authority"] & (1<<m["command_kin"])
            assert not leader["confused"] and not leader["stunned"] and leader["stance"]!=1
            assert sum(a["order_valid"] and a["commander"]==leader["index"]
                       for a in frame["monsters"])<=8
    if not check.get("social") and not check.get("solo"):
        assert ordered,"No valid coordination observed"
    if check.get("shots"):
        if not shots and check.get("allow_surrounded"):
            # Eighteen soldiers can fill all eight contact squares before a
            # bow gets its first casting opportunity. A completed melee ring
            # is useful pressure and leaves no legal incoming projectile ray.
            attacking={e["actor_id"] for e in events if e["kind"]=="melee"
                       and e["player_turn"]>=frames[0]["player_turn"]+16}
            rings=[{m["actor_id"] for m in f["monsters"] if engine.dist(f,m)==1}
                   for f in frames if f["player_turn"]>=frames[0]["player_turn"]+16]
            assert any(len(ring)==8 and ring<=attacking for ring in rings), (
                "No missile contribution or complete attacking melee ring")
        else:
            assert shots,"No missile contribution"
        if check["shots"]=="all":
            for index in missile_ids:
                ready=[m for f in frames if f["player_turn"]>=frames[0]["player_turn"]+16
                       for m in f["monsters"] if m["actor_id"]==index
                       and m["hp"]*2>m["max_hp"] and m["stance"]!=1 and m["sees_player"]
                       and not m["confused"] and not m["stunned"]]
                if len(ready)>=8:
                    assert any(e["actor_id"]==index and e["player_turn"]>=frames[0]["player_turn"]+16
                               for e in shots),f"Healthy missile actor {index} stops contributing"
    if check.get("melee"):
        assert any(e["kind"]=="melee" for e in events),"No frontline engagement"
    if check.get("killed"):
        assert not any(m["race"]==check["killed"] for m in frames[-1]["monsters"])
        assert any(m["role"]==1 and m["race"]!=check["killed"]
                   for f in frames[4:] for m in f["monsters"]),"No successor after commander death"
    if check.get("fleeing"):
        assert frames[0]["monsters"][0]["stance"]==1
        assert any(m["role"]==1 and m["index"]!=check["fleeing"]
                   for f in frames[1:8] for m in f["monsters"]),"Fleeing captain retains command"
    if check.get("door"):
        assert any(32<=f["door_probe"]<48 for f in frames),"Door never actually closes"
        assert all(f["player"]["x"]>=40 for f in frames),"Player reopens the door"
        assert any(f["door_probe"] in (4,5) for f in frames[4:]),"Pursuers do not reopen/bash the door"
    if check.get("hazard"):
        assert any(f["hazard_probe"]==85 for f in frames),"Scheduled lava never commits"
        assert any(m["index"]==3 and (m["y"],m["x"])!=(20,37)
                   for f in frames[1:6] for m in f["monsters"]),"Archer stays on threatened ground"
    if check.get("low_sight"):
        assert any(m["missile"] and m["sight_range"]<=1 for f in frames for m in f["monsters"])
    if check.get("challenge"):
        assert any(e["kind"]=="melee" and e["actor_id"] in missile_ids for e in events)
    if check.get("social"):
        assert any(m["social"] for f in frames for m in f["monsters"]),"Hostile bands never react"
    if check.get("warned"):
        assert all(m["pending_hazard"]==0 for f in frames for m in f["monsters"]),(
            "A fighting monster re-enters announced harmful terrain")
    assert frames[-1]["player_turn"]-frames[0]["player_turn"]>=len(frames)//2,(
        "Most inputs stalled at a prompt")
    return dict(missile_shots=len(shots),ordered_actor_frames=ordered)


def verify(result, check):
    frames, events = engine.read_run(result)
    engine.verify(result,"activity")
    archers={m["index"] for m in frames[0]["monsters"] if m["race"] in (51,103)}
    shots=[e for e in events if e["kind"]=="ranged" and e["detail"] in (96,97)]
    if check=="challenge_melee":
        exposure={}
        for f in frames:
            for m in f["monsters"]:
                if f["player"]["song"]==1 and m["stance"]==3:
                    exposure[m["index"]]=exposure.get(m["index"],0)+1
        affected={index for index,count in exposure.items() if count>=8}
        assert affected & archers,"No archer was affected by Challenge"
        for index in affected:
            assert any(e["kind"]=="melee" and e["index"]==index for e in events),(
                f"Challenge-affected actor {index} stays out of melee")
        for index in affected:
            first=min(f["player_turn"] for f in frames for m in f["monsters"]
                      if m["index"]==index and m["stance"]==3)
            assert not any(e["index"]==index and e["player_turn"]>=first for e in shots),(
                "An affected actor fires while compelled to melee")
    elif check=="challenge_ends":
        assert any(f["player"]["song"]==100 for f in frames),"Voice does not run out"
        end=next(f["player_turn"] for f in frames if f["player"]["song"]==100)
        assert any(e["kind"]=="ranged" and e["player_turn"]>end for e in shots),(
            "Archer never resumes shooting after Challenge ends")
    elif check=="cover":
        assert any(m["job"]==4 and m["order_valid"] for f in frames for m in f["monsters"]),(
            "No withdrawal cover is assigned")
        assert shots,"Archer contributes no fire"
    else:
        for index in archers:
            assert any(e["index"]==index and e["player_turn"]>=frames[0]["player_turn"]+14
                       for e in shots),f"Archer {index} stops contributing"
        if check=="crowd":
            for f in frames:
                for leader in {m["commander"] for m in f["monsters"] if m["order_valid"]}:
                    assert sum(m["commander"]==leader and m["order_valid"] for m in f["monsters"])<=8
        elif check=="authority":
            by_id={m["index"]:m for m in frames[0]["monsters"]}
            for f in frames:
                for m in f["monsters"]:
                    if not m["order_valid"]:continue
                    leader=by_id[m["commander"]]["race"]
                    if m["race"] in (91,41,51):
                        assert leader!=409,"Fankil acquires unauthored Orc authority"
                    if m["race"] in (409,82,103):
                        assert leader not in (91,41,51),"Orc captain commands Men"
    return dict(archer_shots=len(shots),archer_melee=sum(e["kind"]=="melee"
        and e["index"] in archers for e in events))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed",type=int,default=20261116)
    parser.add_argument("--filter",default="")
    parser.add_argument("--varied",action="store_true")
    parser.add_argument("--report",type=Path,default=OUT/"matrix.json")
    args=parser.parse_args()
    engine.OUT=OUT
    engine.instrument()
    exe,env=engine.play.build()
    env["AI_PLAYTEST_SIGNATURE"]=hashlib.sha256((env["AI_PLAYTEST_SIGNATURE"]+
        Path(__file__).read_text()+Path(engine.__file__).read_text()).encode()).hexdigest()
    results=[];failures=[]
    for name,spec,keys,check in (varied_cases() if args.varied else cases()):
        if args.filter and not re.search(args.filter,name):continue
        try:
            result=engine.play.run(exe,env,name,dict(spec,seed=args.seed),keys)
            result.update(verify_varied(result,check) if args.varied else verify(result,check))
            result["outcome_check"]=check
            results.append(result)
        except Exception as exc:
            failures.append(dict(name=name,error=str(exc),traceback=traceback.format_exc()))
            print(json.dumps(failures[-1]),flush=True)
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
    print(f"Coordination encounters: {len(results)} passed, {len(failures)} failed",flush=True)
    if failures:raise SystemExit(1)


if __name__=="__main__":main()
