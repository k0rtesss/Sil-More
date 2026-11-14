#!/usr/bin/env python3
"""Exercise the monster catalogue and legacy mechanics through headless SDL.

The linked fixture only prepares disposable encounters and observes production
state. Every subsequent player action uses GameControl's SDL event interface.
Build standard first; reports retain inputs, logs, screenshots and full traces.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_transparency_gameplay as transparency

audit = transparency.audit
play, engine = transparency.play, transparency.engine
ROOT = play.ROOT
OUT = ROOT / "scripts/output/monster-ai-round7"


def feature(name):
    source = (ROOT / 'src/defines.h').read_text()
    match = re.search(r'^#define\s+FEAT_'+re.escape(name)+r'\s+(0x[0-9A-Fa-f]+|[0-9]+)\b',source,re.M)
    assert match, name
    return int(match[1],0)


DEEP_WATER = feature('DEEP_WATER')


def catalogue():
    records = []
    source = (ROOT / "lib/edit/monster.txt").read_text(encoding="utf-8-sig")
    for block in re.split(r"(?m)(?=^N:)", source):
        match = re.match(r"N:(\d+):([^\r\n]+)", block)
        if not match:
            continue
        flags = {flag.strip() for line in re.findall(r"(?m)^[FS]:([^\r\n]*)", block)
                 for flag in line.split("|") if flag.strip()}
        records.append(dict(race=int(match[1]), name=match[2], flags=sorted(flags),
                            blows=re.findall(r"(?m)^B:([^\r\n]*)", block),
                            stats=bool(re.search(r"(?m)^I:", block))))
    return records


def instrument():
    audit.OUT = OUT
    audit.instrument()
    play.FIXTURE = play.FIXTURE.replace('#include "cJSON.h"',
        '#include "cJSON.h"\n#include "melee/melee-attack.h"')
    play.FIXTURE = play.FIXTURE.replace(
        'm->confused = number(entry, "confused", 0);',
        'm->confused = number(entry, "confused", 0);\n'
        '        m->mana = number(entry, "mana", m->mana);')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(player, "poison", p_ptr->poisoned);', r'''
        cJSON_AddNumberToObject(player, "poison", p_ptr->poisoned);
        cJSON_AddNumberToObject(player, "confused", p_ptr->confused);
        cJSON_AddNumberToObject(player, "afraid", p_ptr->afraid);
        cJSON_AddNumberToObject(player, "entranced", p_ptr->entranced);
        cJSON_AddNumberToObject(player, "slow", p_ptr->slow);
        cJSON_AddNumberToObject(player, "blind", p_ptr->blind);
        cJSON_AddNumberToObject(player, "stun", p_ptr->stun);
        cJSON_AddNumberToObject(player, "cut", p_ptr->cut);''')
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(o, "race", m->r_idx);', r'''
            cJSON_AddNumberToObject(o, "race", m->r_idx);
            cJSON_AddNumberToObject(o, "flags1", r->flags1);
            cJSON_AddNumberToObject(o, "flags2", r->flags2);
            cJSON_AddNumberToObject(o, "flags3", r->flags3);
            cJSON_AddNumberToObject(o, "flags4", r->flags4);
            cJSON_AddNumberToObject(o, "flags5", r->flags5);
            cJSON_AddNumberToObject(o, "held_object", m->hold_o_idx);''')
    play.FIXTURE = play.FIXTURE.replace(
        '        /* Terrain also detects real doors, webs, and bridge transformations. */', r'''
        cJSON* objects = cJSON_AddArrayToObject(root, "objects");
        for (int i = 1; i < o_max; ++i) {
            const object_type* item = &o_list[i];
            if (!item->k_idx) continue;
            cJSON* object = cJSON_CreateObject();
            cJSON_AddItemToArray(objects, object);
            cJSON_AddNumberToObject(object, "kind", item->k_idx);
            cJSON_AddNumberToObject(object, "held_by", item->held_m_idx);
            cJSON_AddNumberToObject(object, "y", item->iy);
            cJSON_AddNumberToObject(object, "x", item->ix);
            cJSON_AddNumberToObject(object, "number", item->number);
            cJSON_AddBoolToObject(object, "cursed", cursed_p(item));
        }
        /* Terrain also detects real doors, webs, and bridge transformations. */''')
    play.FIXTURE = play.FIXTURE.replace('    cJSON_Delete(spec);', r'''
    cJSON* objects = cJSON_GetObjectItemCaseSensitive(spec, "floor_objects");
    cJSON_ArrayForEach(entry, objects) {
        object_type object;
        object_prep(&object, lookup_kind(TV_SWORD, SV_LONG_SWORD));
        object.number = number(entry, "number", 1);
        if (number(entry, "cursed", 0)) object.ident |= IDENT_CURSED;
        assert(floor_carry(number(entry, "y", 20), number(entry, "x", 37), &object));
    }
    cJSON_Delete(spec);''')
    # Verify eligibility at the exact cast call, before it spends resources.
    tail = r'\"target_x\":%d}\n",'
    assert tail in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(tail,
        r'\"target_x\":%d,\"legal_cast\":%d,\"confused\":%d,\"stunned\":%d,\"skip_next\":%d,\"feat\":%d}\n",')
    play.FIXTURE = play.FIXTURE.replace(
        'm->squad.target_y, m->squad.target_x);',
        'm->squad.target_y, m->squad.target_x,\n'
        '        !strcmp(kind, "ranged_begin") && monster_ranged_attack_legal(m, detail),\n'
        '        m->confused, m->stunned, m->skip_next_turn, cave_feat[m->fy][m->fx]);', 1)


def actor(race, x=36, y=20, **fields):
    return dict(race=race, x=x, y=y, heard=1, **fields)


def catalogue_cases():
    cases = []
    for race in catalogue():
        flags = set(race['flags'])
        # Hallucination-only identities deliberately have no combat statistics.
        if not race['stats'] or not (race['blows'] or any(
                flag.startswith('SPELL_PCT_') for flag in flags)):
            continue
        spec = dict(recognition=0, evasion=24, allow_death=True,
                    monsters=[actor(race['race'])])
        cases.append((f"catalogue-{race['race']}", spec, ['KP5'] * 22,
                      dict(catalogue_race=race['race'])))
    return cases


def legacy_cases():
    cases = []
    def add(name, spec, count=36, check=None, keys=None):
        cases.append((name, dict(recognition=0, evasion=24, allow_death=True, **spec),
                      keys if keys is not None else ['KP5'] * count, check or {}))
    corridor = [dict(y1=1,y2=38,x1=1,x2=62,feat=63),
                dict(y1=20,x1=25,x2=48,feat=1)]
    # A sealed room keeps recipients in earshot and removes the competing
    # melee attack; explicit membership makes these mixed races actual allies.
    support_room = [dict(y1=1,y2=38,x1=1,x2=62,feat=63),
                    dict(y1=17,y2=23,x1=28,x2=37,feat=1),
                    dict(y1=17,y2=23,x1=38,x2=39,feat=2),
                    dict(y1=20,x1=40,feat=1)]
    # Actual support recipients, rather than an empty cry in a solo race probe.
    for race in (31,63,121,407):
        add(f'legacy-shriek-{race}', dict(tiles=support_room,monsters=[
            actor(race,36,group=8,alertness=20), actor(41,34,19,alertness=-30,group=8),
            actor(61,34,21,alertness=-30,group=8)]), 60, dict(cast=104))
    for race in (54,91,95,105,125,409):
        add(f'legacy-rally-{race}', dict(tiles=support_room,monsters=[actor(race,36,group=8,alertness=20),
            actor(41,34,19,morale=-150,stance=1,group=8),
            actor(61,34,21,morale=-150,stance=1,group=8)]),80,dict(cast=120))
    for name,race,attack in [('cold',33,100),('fire',53,99),('poison',73,101),
                             ('dark',94,102),('web',71,119),('scare',113,108),
                             ('slow',404,111),('hold',117,110),('confusion',402,109),
                             ('darkness',75,106),('spider-hatch',44,112)]:
        x=38 if name=='scare' else 36
        # Non-flying casters must use their authored control rather than
        # replacing it with a stronger adjacent physical attack.
        moat = [dict(y1=1,y2=38,x1=x+1,x2=39,feat=2)] if name in (
            'scare','slow','hold','confusion') else []
        add('legacy-spell-'+name,dict(tiles=moat,monsters=[actor(race,x)],will=30),
            60,dict(cast=attack))
    add('legacy-oaths',dict(will=30,monsters=[actor(244,34)]),42,dict(cast=116,spawn=153),
        ['KP6']*8+['KP8']*8+['KP5']*26)
    add('legacy-multiplication',dict(monsters=[actor(101,36)]),70,dict(multiply=101))
    for race in (115,125,196):
        add(f'legacy-item-pickup-{race}',dict(song=1 if race==115 else 100,
            song_skill=30,tiles=corridor, floor_objects=[dict(x=37),dict(x=38,cursed=1)],
            monsters=[actor(race,36,stance=3,morale=1000)]),22,dict(pickup=True))
    for race in (51,71,117,164,244,404,411):
        for status in ('confused','stunned'):
            add(f'legacy-{status}-{race}',dict(monsters=[actor(race,37,**{status:8})]),
                28,dict(status=status))
    for race in (51,103,114,404,411):
        add(f'legacy-empty-mana-{race}',dict(monsters=[actor(race,37,mana=0)]),
            28,dict(empty_mana=True))
    for race in (13,14,16,17):
        add(f'legacy-peaceful-{race}',dict(monsters=[actor(race,37)]),18,dict(peaceful=True))
    # Constrain movement to isolate underwater attack suppression and escape.
    for race in (51,82,103,114,404,407):
        add(f'legacy-water-escape-{race}',dict(tiles=corridor+[
            dict(y1=20,x1=36,x2=38,feat=DEEP_WATER)],
            monsters=[actor(race,37,exposed=1,stance=3,morale=1000)]),28,
            dict(water=True))
    for race in (403,406,407,408,411):
        add(f'legacy-blind-player-{race}',dict(blindness=20,
            monsters=[actor(race,36)]),28,dict(blind=True))
    # Reuse already authored production checks for hard-to-stage old behavior.
    for name,spec,keys,check in engine.scenarios():
        if name.startswith(('legacy-','dragon-lair-','grotesque-los','archers-torch')):
            cases.append((name,spec,keys,dict(engine_check=check)))
    return cases


def water_cases():
    cases=[]
    solid=dict(y1=1,y2=38,x1=1,x2=62,feat=63)
    corridor=[solid,dict(y1=20,x1=25,x2=50,feat=1)]
    for race in (51,103):
        for width in (3,5,9):
            half=width//2
            for vertical in (False,True):
                if vertical:
                    tiles=[solid,dict(y1=10,y2=35,x1=36,feat=1),
                           dict(y1=21-half,y2=21+half,x1=36,feat=DEEP_WATER)]
                    spec=dict(player_y=30,player_x=36,tiles=tiles,
                              monsters=[actor(race,36,21,exposed=1,morale=1000,stance=3)])
                else:
                    tiles=corridor+[dict(y1=20,x1=36-half,x2=36+half,feat=DEEP_WATER)]
                    spec=dict(player_x=45,tiles=tiles,
                              monsters=[actor(race,36,exposed=1,morale=1000,stance=3)])
                cases.append((f'water-width-{width}-{"vertical" if vertical else "horizontal"}-{race}',
                    spec,['KP5']*52,dict(water=True,cast=96 if race==51 else 97)))
        base=dict(tiles=corridor+[dict(y1=20,x1=36,x2=38,feat=DEEP_WATER)],
                  monsters=[actor(race,37,exposed=1,morale=1000,stance=3)])
        cases.append((f'water-challenge-{race}',dict(base,song=1,song_skill=30),
                      ['KP5']*36,dict(water=True,melee=True)))
        cases.append((f'water-confusion-{race}',dict(base,
            monsters=[actor(race,37,exposed=1,morale=1000,stance=3,confused=6)]),
            ['KP5']*44,dict(water=True,engagement=True)))
        cases.append((f'water-harmful-bank-{race}',dict(base,
            tiles=base['tiles']+[dict(y1=20,x1=35,feat=85)]),
            ['KP5']*40,dict(water=True,avoid_feature=85,melee=True)))
        cases.append((f'water-warned-bank-{race}',dict(base,environment=1,
            environment_cells=[dict(y=20,x=35,flags=1,pending=85,due=10000)]),
            ['KP5']*40,dict(water=True,avoid_pending=True,melee=True)))
        cases.append((f'water-no-shore-{race}',dict(base,
            tiles=[solid,dict(y1=20,x1=25,x2=39,feat=DEEP_WATER),dict(y1=20,x1=40,feat=1)]),
            ['KP5']*22,dict(no_shore=True)))
        cases.append((f'water-floods-around-{race}',dict(base,environment=1,
            # Keep a dry bypass so the environment's route protection permits
            # this flood instead of cancelling the corridor's only crossing.
            tiles=corridor+[dict(y1=19,x1=35,x2=39,feat=1)],
            feature_probes=[dict(y=20,x=x) for x in (36,37,38)],
            environment_cells=[dict(y=20,x=x,flags=0,pending=DEEP_WATER,due=120)
                               for x in (36,37,38)]),
            ['KP5']*40,dict(water=True,flood=True)))
    cases.append(('water-flying-drake',dict(tiles=corridor+[
        dict(y1=20,x1=36,x2=38,feat=DEEP_WATER)],monsters=[actor(245,37)]),
        ['KP5']*36,dict(flying=True,cast=100)))
    cases.append(('water-shallow-archer',dict(tiles=corridor+[
        dict(y1=20,x1=36,x2=38,feat=feature('WATER'))],monsters=[actor(51,37,morale=1000,stance=3)]),
        ['KP5']*28,dict(shallow=True,cast=96)))
    return [(name,dict(recognition=0,evasion=24,allow_death=True,**spec),keys,check)
            for name,spec,keys,check in cases]


def verify(result, check):
    frames,events=engine.read_run(result)
    engine.verify(result,'activity')
    result.update(transparency.verify_log(result))
    pending={}
    for e in events:
        if e['kind']=='ranged_begin': pending[e['actor_id']]=e
        if e['kind']=='ranged':
            before=pending[e['actor_id']]
            assert before['legal_cast'], f'Illegal committed ranged action: {before}'
            assert not before['confused'] and not before['skip_next'] and not before['recovery']
            if e['detail'] in (96,97,98,119):
                assert before['projection']==2, 'Blocked projectile lane'
            if e['detail'] not in (104,112,114,115,116,120) and e['race']!=251:
                assert before['sees_player'], 'Player-targeted cast without personal sight'
    for f in frames:
        for m in f['monsters']:
            assert 0 <= m['mana'] <= 100
            assert 0 < m['hp'] <= m['max_hp']
            assert all(-3 <= value <= 3 for value in m['knowledge'])
            if m['mindless']: assert not any(m['knowledge'])
    if 'cast' in check:
        assert any(e['kind']=='ranged' and e['detail']==check['cast'] for e in events), (
            f'Required cast {check["cast"]} never happens')
    if check.get('melee'): assert any(e['kind']=='melee' for e in events)
    if check.get('engagement'):
        assert any(e['kind'] in ('melee','ranged') for e in events), 'No engagement after recovering'
        assert not any(m['confused'] for m in frames[-1]['monsters']), 'Confusion never recovers'
    if 'spawn' in check:
        assert any(m['race']==check['spawn'] for f in frames for m in f['monsters'])
    if 'multiply' in check:
        assert max(sum(m['race']==check['multiply'] for m in f['monsters']) for f in frames)>1
    if check.get('peaceful'):
        assert not any(e['kind'] in ('melee','reaction','ranged') for e in events)
        assert not any(m['order_valid'] for f in frames for m in f['monsters'])
    if check.get('pickup'):
        assert any(o['held_by'] for f in frames for o in f['objects']), 'No item pickup'
        assert all(not o['held_by'] for f in frames for o in f['objects'] if o['cursed'])
        assert sum(o['number'] for o in frames[0]['objects'])==sum(o['number'] for o in frames[-1]['objects'])
    if check.get('water'):
        fly=next(m for m in frames[0]['monsters'])['race']==407
        assert not any(e['kind']=='melee' and e['feat']==DEEP_WATER for e in events) or fly
        assert any(m['feat']!=DEEP_WATER for f in frames[1:] for m in f['monsters']), 'No escape from deep water'
    if check.get('no_shore'):
        assert all(m['feat']==DEEP_WATER for f in frames for m in f['monsters'])
        assert not any(e['kind'] in ('melee','ranged') for e in events)
    if check.get('shallow'):
        assert any(e['kind']=='ranged_begin' and e['feat']==feature('WATER') for e in events)
    if check.get('flood'):
        assert any(DEEP_WATER in f['feature_probes'] for f in frames), 'Flood never commits'
    if check.get('avoid_pending'):
        assert not any(m['pending_hazard'] for f in frames for m in f['monsters'])
    if 'avoid_feature' in check:
        assert not any(m['feat']==check['avoid_feature'] for f in frames for m in f['monsters'])
    if check.get('avoid_pending') or 'avoid_feature' in check:
        dry=next(i for i,f in enumerate(frames) if all(m['feat']!=DEEP_WATER for m in f['monsters']))
        assert all(m['feat']!=DEEP_WATER for f in frames[dry:] for m in f['monsters']), (
            'Archer re-enters deep water after escaping to its only safe bank')
    if 'engine_check' in check: engine.verify(result,check['engine_check'])
    log=(Path(result['profile'])/'log.txt').read_text(encoding='utf-8',errors='replace')
    assert not re.search(r'\b(?:ERROR|FATAL)\s',log), 'Error in encounter log'
    selected=list(re.finditer(r'\[monster-ai\] player_turn=(\d+) actor=(\d+) race=(\d+) '
        r'selected=melee blow=(\d+) method=(\d+) effect=(\d+) ordinary=(\d+) smite=(\d+)',log))
    committed=Counter(tuple(int(m[i]) for i in (1,2,3,7)) for m in selected)
    completed=Counter((e['player_turn'],e['index'],e['race'],int(e['kind']=='melee'))
                      for e in events if e['kind'] in ('melee','reaction'))
    assert committed==completed, 'Melee choice diagnostics differ from actual ordinary/reaction attacks'
    result['melee_choices']=dict(Counter(f'race={m[3]} blow={m[4]} effect={m[6]} smite={m[8]}'
                                       for m in selected))
    result['cast_types']=sorted({e['detail'] for e in events if e['kind']=='ranged'})
    result['race_ids']=sorted({m['race'] for f in frames for m in f['monsters']})
    result['peak_monsters']=max(len(f['monsters']) for f in frames)
    result['outcome_check']=check
    return result


def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode',choices=['catalogue','legacy','water','persistence','equivalence'],default='catalogue')
    parser.add_argument('--seed',type=int,default=20261113)
    parser.add_argument('--filter',default='')
    parser.add_argument('--output-dir',type=Path,default=OUT)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    OUT=args.output_dir.resolve()
    instrument()
    exe,env=audit.build()
    env['SIL_LOG_LEVEL']='DEBUG'
    digest=hashlib.sha256(env['AI_PLAYTEST_SIGNATURE'].encode())
    digest.update(Path(__file__).read_bytes())
    env['AI_PLAYTEST_SIGNATURE']=digest.hexdigest()
    report=args.report or OUT/f'{args.mode}-{args.seed}.json'
    selected=catalogue_cases() if args.mode=='catalogue' else legacy_cases()
    if args.mode=='water': selected=water_cases()
    if args.mode=='persistence': selected=engine.persistence_cases()
    if args.mode=='equivalence':
        selected=[c for c in legacy_cases() if c[0] in (
            'legacy-item-pickup-196','legacy-confused-411','legacy-spell-confusion')]
        selected += [c for c in water_cases() if c[0] in (
            'water-width-9-horizontal-51','water-warned-bank-103')]
        selected.append(('logging-hallucinating-smite',dict(recognition=0,hallucination=30,
            allow_death=True,monsters=[actor(411,39)]),['KP5']*28,{}))
    results,failures,comparisons=[],[],[]
    for case in selected:
        name,spec,*tail=case
        if args.filter and not re.search(args.filter,name):continue
        spec=dict(spec,seed=args.seed)
        try:
            if args.mode=='persistence':
                prefix,suffix,check=tail
                result=engine.run_persistence(exe,audit.case_env(env,spec),name,spec,prefix,suffix,check)
                for branch in result['branches']:verify(branch,{})
            else:
                keys,check=tail
                result=verify(play.run(exe,audit.case_env(env,spec),name,spec,keys),check)
                if args.mode=='equivalence':
                    for level in ('WARN','TRACE'):
                        other=play.run(exe,audit.case_env(dict(env,SIL_LOG_LEVEL=level),spec),name+'-'+level,spec,keys)
                        transparency.compare_runs(result,other)
                        comparisons.append(dict(reference=result['profile'],other=other['profile'],level=level))
                        results.append(other)
            results.append(result)
        except Exception as exc:
            failure=dict(name=name,error=str(exc),traceback=traceback.format_exc())
            failures.append(failure)
            print(json.dumps(failure),flush=True)
        report.parent.mkdir(parents=True,exist_ok=True)
        report.write_text(json.dumps(dict(results=results,failures=failures,comparisons=comparisons),indent=2))
    print(f'{args.mode}: {len(results)} sessions, {len(failures)} failures',flush=True)
    if failures:raise SystemExit(1)


if __name__=='__main__':main()
