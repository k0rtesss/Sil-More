#!/usr/bin/env python3
"""Additional compound encounters through the real, windowless SDL interface.

The fixture supplies starting terrain/statuses and read-only telemetry. Player
actions, status expiry, pursuit, attacks, songs and world changes use the game.
Separate workers own separate profiles, binaries and atomic result reports.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shlex
import traceback

import check_monster_ai_catalogue_gameplay as catalogue

play, engine, audit = catalogue.play, catalogue.engine, catalogue.audit
ROOT = play.ROOT
OUT = ROOT / 'scripts/output/monster-ai-round9'


def define(name):
    match = re.search(r'^#define\s+' + re.escape(name) + r'\s+(0x[0-9a-fA-F]+|\d+)\b',
                      (ROOT/'src/defines.h').read_text(), re.M)
    assert match, name
    return int(match[1], 0)


def instrument(destination):
    catalogue.OUT = destination
    catalogue.instrument()
    play.FIXTURE = play.FIXTURE.replace(
        'p_ptr->depth = p_ptr->max_depth = 10;',
        'p_ptr->depth = p_ptr->max_depth = number(spec, "depth", 10);')
    play.FIXTURE = play.FIXTURE.replace(
        'p_ptr->blind = number(spec, "blindness", 0);',
        'p_ptr->blind = number(spec, "blindness", 0);\n'
        '    p_ptr->fast = number(spec, "player_haste", 0);')
    play.FIXTURE = play.FIXTURE.replace(
        'm->stunned = number(entry, "stunned", 0);',
        'm->stunned = number(entry, "stunned", 0);\n'
        '        m->hasted = number(entry, "hasted", 0);\n'
        '        m->slowed = number(entry, "slowed", 0);\n'
        '        if (m->hasted || m->slowed) calc_monster_speed(m->fy, m->fx);')
    anchor = 'cJSON_AddNumberToObject(o, "speed", m->mspeed);'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, anchor + r'''
            cJSON_AddNumberToObject(o, "base_speed", r->speed);
            cJSON_AddNumberToObject(o, "hasted", m->hasted);
            cJSON_AddNumberToObject(o, "slowed", m->slowed);
            cJSON_AddBoolToObject(o, "social_valid", monster_social_valid(m));
            cJSON_AddNumberToObject(o, "scent_limit", monster_scent_limit(r));
            cJSON_AddNumberToObject(o, "scent_age", get_scent(m->fy, m->fx));
            cJSON* learned = cJSON_AddArrayToObject(o, "knowledge_turns");
            cJSON* expiry = cJSON_AddArrayToObject(o, "knowledge_ttls");
            for (int f = 0; f < MON_AI_FEATURE_COUNT; ++f) {
                cJSON_AddItemToArray(learned, cJSON_CreateNumber(m->ai.observations[f].turn));
                cJSON_AddItemToArray(expiry, cJSON_CreateNumber(m->ai.observations[f].ttl));
            }
''')


def combat_templates():
    return [r for r in catalogue.catalogue() if r['stats'] and
            (r['blows'] or any(f.startswith('SPELL_PCT_') for f in r['flags']))]


def catalogue_cases():
    solid = dict(y1=1,y2=38,x1=1,x2=62,feat=define('FEAT_WALL_PERM'))
    corner = [solid, dict(y1=18,y2=22,x1=28,x2=34,feat=1),
              dict(y1=20,x1=29,x2=42,feat=1), dict(y1=20,y2=27,x1=42,feat=1)]
    motion = ['KP5']*10 + (['KP2']*3+['KP6']*2+['KP8']*3+['KP4']*2)*2 + ['KP5']*14
    for race in combat_templates():
        yield f"corner-{race['race']}", dict(player_y=25,player_x=42,recognition=1,
            evasion=30,allow_death=True,tiles=corner,confirm_traps=True,decline_chasms=True,
            monsters=[dict(race=race['race'],x=32,y=20,heard=1,morale=150)]), (
                ['KP5']*28+['KP8']*2+['KP2']*2+['KP5']*8), dict(initial_hidden=True)
        status = 'confused' if 'NO_CONF' not in race['flags'] else 'stunned'
        fields = {} if status=='stunned' and 'NO_STUN' in race['flags'] else {status:6}
        actor = dict(race=race['race'],x=34,y=20,heard=1,morale=150,mana=0,**fields)
        yield f"recover-moving-{race['race']}", dict(recognition=1,evasion=30,will=30,
            allow_death=True,confirm_traps=True,decline_chasms=True,
            tiles=[dict(y1=16,y2=18,x1=36,feat=63),dict(y1=22,y2=25,x1=38,feat=63)],
            monsters=[actor]), motion, dict(expire=status if fields else '')


def mixed_cases():
    families = {
        'orcs':[91,81,61,51,51,41,76],
        'men':[409,82,103,103,121,115],
        'cats':[185,165,177,154,175,175],
        'giants':[135,146,114,114],
        'wolves':[205,172,11,11,11,172],
        'spiders':[242,224,211,171,71,44],
        'cross-kin':[241,186,214,91,61,51],
        'undead':[244,153,183,75,93,64],
        'dragons':[402,405,181,164],
        'archers':[91,61,61,51,51,51,51,51],
        'supports':[409,404,407,82,103],
    }
    positions=[(20,34),(18,33),(22,33),(17,31),(23,31),(19,30),(21,30),(20,28)]
    layouts = {
        'double-doors':[dict(y1=10,y2=30,x1=37,feat=63),
                        dict(y1=18,x1=37,feat=32),dict(y1=22,x1=37,feat=33)],
        'offset-pillars':[dict(y1=15,y2=19,x1=36,feat=56),dict(y1=22,y2=25,x1=38,feat=56)],
        'two-bridges':[dict(y1=10,y2=30,x1=37,x2=38,feat=100),
                       dict(y1=18,x1=37,x2=38,feat=98),dict(y1=23,x1=37,x2=38,feat=98)],
        'acid-islands':[dict(y1=18,y2=22,x1=36,feat=87),dict(y1=16,x1=35,x2=38,feat=87)],
        'ice-to-bank':[dict(y1=16,y2=24,x1=35,x2=36,feat=86)],
        'song-transitions':[],
    }
    moving=['KP5']*12+(['KP2']*3+['KP6']*2+['KP8']*3+['KP4']*2)*2+['KP5']*14
    for family,races in families.items():
        for layout,tiles in layouts.items():
            party=[dict(race=r,x=x,y=y,heard=1,morale=0)
                   for r,(y,x) in zip(races,positions)]
            # Incapacity and different action clocks stress succession and
            # shared plans without inventing any combat capability.
            party[0]['stunned']=3
            party[-1]['hasted']=5
            spec=dict(recognition=1,evasion=42,will=30,allow_death=True,
                      confirm_traps=True,confirm_acid=True,decline_chasms=True,
                      monsters=party,tiles=copy.deepcopy(tiles))
            keys=moving
            if layout=='song-transitions':
                spec.update(song=14,song_skill=8,abilities=[dict(skill=7,ability=1)])
                keys=['KP5']*10+['s','b']+['KP5']*12+['s','s']+['KP5']*24
            for quarter in range(4):
                rotated,inputs=audit.rotate(spec,keys,quarter)
                yield f'{family}-{layout}-r{quarter}',rotated,inputs,dict(mixed=True)


def sensory_cases():
    # No heard-player seed and an opaque divider make local scent the only
    # player-related evidence. Independent physical events are tested below.
    for race,limit in [(11,80),(154,40),(405,80),(73,0)]:
        for age in (0,39,40,41,79,80):
            spec=dict(recognition=1,player_x=58,stealth=30,evasion=35,
                tiles=[dict(y1=1,y2=38,x1=48,feat=63)],
                monsters=[dict(race=race,x=30,y=20,alertness=0,morale=150)],
                scents=[dict(y=20,x=x,age=age) for x in range(30,37)])
            yield f'scent-age-{age}-race-{race}',spec,['KP5']*24,dict(scent=True,limit=limit,start_age=age)
    for race in (11,154,405):
        for gap in (define('FEAT_WATER'),define('FEAT_DEEP_WATER'),define('FEAT_ICE'),define('FEAT_POISON')):
            spec=dict(recognition=1,player_x=58,stealth=30,evasion=35,
                tiles=[dict(y1=1,y2=38,x1=48,feat=63),dict(y1=18,y2=22,x1=33,feat=gap)],
                monsters=[dict(race=race,x=30,y=20,alertness=0,morale=150)],
                scents=[dict(y=20,x=x,age=18-(x-30)) for x in range(30,37)])
            yield f'scent-surface-{gap}-race-{race}',spec,['KP5']*32,dict(scent=True)
    for race in (41,82,91,407):
        for volume in (5,25,50):
            spec=dict(player_x=58,evasion=35,stealth=30,
                tiles=[dict(y1=1,y2=38,x1=45,feat=63)],
                events=[dict(y=20,x=35,volume=volume)],
                monsters=[dict(race=race,x=30,y=20,alertness=-20,morale=0)])
            yield f'physical-event-{race}-volume-{volume}',spec,['KP5']*32,dict(no_player_contact=True)


def timing_cases():
    templates={r['race']:r for r in catalogue.catalogue()}
    for race in (41,51,82,114,154,164,403,404,407,408,411,253):
        for status in ('normal','hasted','slowed'):
            if status=='slowed' and 'NO_SLOW' in templates[race]['flags']:continue
            actor=dict(race=race,x=37,heard=1,morale=150)
            if status!='normal':actor[status]=8
            yield f'clock-{race}-{status}',dict(recognition=0,evasion=38,will=30,
                allow_death=True,confirm_traps=True,decline_chasms=True,monsters=[actor]), (
                    ['KP5']*16+['KP6']*4+['KP8']*2+['KP4']*4+['KP2']*2+['KP5']*12),dict(expire=status if status!='normal' else '',speed_initialized=True)
    for race,resistance,feature in [(181,'fire',0),(134,'cold',1),(152,'poison',2)]:
        spec=dict(recognition=1,evasion=40,will=60,player_haste=60,allow_death=True,
            **{resistance+'_resistance':1},tiles=[dict(y1=1,y2=30,x1=43,feat=63),
                dict(y1=1,y2=38,x1=38,feat=2),
                dict(y1=18,y2=22,x1=35,x2=37,feat=63),dict(y1=20,x1=37,feat=1)],
            monsters=[dict(race=race,x=37,heard=1,morale=0)])
        yield f'learn-then-lose-sight-{resistance}',spec,(
            ['KP5']*28+['KP2']*12+['KP6']*10+['KP8']*12+['KP5']*48),dict(forget=feature)


def cases(group):
    return list({'catalogue':catalogue_cases,'mixed':mixed_cases,'sensory':sensory_cases,
                 'timing':timing_cases}[group]())


def verify(result,check):
    catalogue.verify(result,{})
    frames,events=engine.read_run(result)
    assert frames[-1]['player_turn']-frames[0]['player_turn']>=max(1,(len(frames)-1)//2), (
        'Most SDL inputs stopped at a prompt or blocked action')
    for frame in frames:
        for m in frame['monsters']:
            assert m['social_valid'], f'Invalid social state for actor {m["actor_id"]}'
            assert min(m['hasted'],m['slowed'],m['confused'],m['stunned'])>=0
            for value,learned,ttl in zip(m['knowledge'],m['knowledge_turns'],m['knowledge_ttls']):
                if ttl==0 or frame['player_turn']-learned>=ttl:
                    assert value==0,'Expired knowledge still affects decisions'
    if check.get('initial_hidden'):
        assert not any(m['sees_player'] for m in frames[0]['monsters']),'Corner setup already visible'
    if check.get('expire'):
        status=check['expire']
        identity=frames[0]['monsters'][0]['actor_id']
        last=next((m for m in frames[-1]['monsters'] if m['actor_id']==identity),None)
        if last and frames[-1]['player_turn']-frames[0]['player_turn']>=16:
            assert last[status]==0,f'{status} never expires'
        if check.get('speed_initialized') and status in ('hasted','slowed'):
            first=frames[0]['monsters'][0]
            expected=max(1,min(7,first['base_speed']+(1 if status=='hasted' else -1)))
            assert first['speed']==expected,'Initial timed speed has not been initialized'
    if check.get('scent') or check.get('no_player_contact'):
        assert all(not m['sees_player'] for f in frames for m in f['monsters'])
        assert not any(e['kind'] in ('melee','reaction') for e in events),'Hidden remote player attacked'
        assert all(e['detail'] in (104,112,114,115,116,120)
                   for e in events if e['kind']=='ranged'),'Offensive cast at unseen remote player'
    if check.get('scent') and check.get('limit')==0:
        assert not any(m['sense'] in (3,4) for f in frames for m in f['monsters']), 'Non-tracker acquires scent'
    if check.get('scent') and 'limit' in check and check['limit']>0:
        if check['start_age']>=check['limit']:
            assert not any(m['sense'] in (3,4) for f in frames for m in f['monsters']), 'Expired plateau starts pursuit'
        else:
            assert any(m['sense'] in (3,4) for f in frames for m in f['monsters']), 'Fresh local scent is ignored'
    if 'forget' in check:
        index=check['forget']
        assert any(m['knowledge'][index]>0 for f in frames for m in f['monsters']), 'No witnessed resistance evidence'
        assert any(not m['sees_player'] for m in frames[-1]['monsters']), 'Player never loses sight'
        assert all(m['knowledge'][index]==0 for m in frames[-1]['monsters']), 'Old resistance evidence survives hiding'
    result['stress_check']=check
    result['race_ids']=sorted({m['race'] for f in frames for m in f['monsters']})
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--groups',default='catalogue,mixed,sensory,timing')
    parser.add_argument('--seed',type=int,default=20261119)
    parser.add_argument('--worker',type=int,default=0)
    parser.add_argument('--workers',type=int,default=1)
    parser.add_argument('--filter',default='')
    parser.add_argument('--output-dir',type=Path,default=OUT)
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    assert 0<=args.worker<args.workers
    destination=args.output_dir.resolve()
    instrument(destination)
    object_paths=shlex.split((play.BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    def object_fingerprint():
        digest=hashlib.sha256()
        for name in object_paths:
            if name.endswith('/src/main.c.obj'):continue
            path=play.BUILD/name
            digest.update(name.encode())
            digest.update(path.read_bytes())
        return digest.hexdigest()
    objects_before=object_fingerprint()
    exe,env=audit.build()
    assert objects_before==object_fingerprint(),'Build objects changed while linking the fixture'
    env['SIL_LOG_LEVEL']='DEBUG'
    modules=[Path(m.__file__) for m in (catalogue,play,engine,audit,catalogue.transparency)] + [Path(__file__)]
    files=sorted({*modules,*(ROOT/'src').rglob('*.c'),*(ROOT/'src').rglob('*.h'),*(ROOT/'lib/edit').glob('*.txt')})
    manifest={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    manifest.update(production_executable=hashlib.sha256((play.BUILD/'sil-more.exe').read_bytes()).hexdigest(),
        fixture_source=hashlib.sha256(exe.with_suffix('.c').read_bytes()).hexdigest(),
        linked_objects=objects_before)
    signature=hashlib.sha256(json.dumps(manifest,sort_keys=True).encode()).hexdigest()
    env['AI_PLAYTEST_SIGNATURE']=signature
    (destination/'source-manifest.json').write_text(json.dumps(manifest,indent=2))
    report=destination/f'stress-{args.seed}-worker-{args.worker}.json'
    previous=json.loads(report.read_text()) if args.resume and report.exists() else {}
    results=previous.get('results',[])
    assert all(r['build_signature']==signature for r in results),'Cannot resume across source/build changes'
    complete={(r['stress_group'],r['case_name']) for r in results}
    failures=[]
    for group in args.groups.split(','):
        for name,spec,keys,check in cases(group):
            if args.filter and not re.search(args.filter,name):continue
            if int(hashlib.sha256((group+name).encode()).hexdigest(),16)%args.workers!=args.worker:continue
            if (group,name) in complete:continue
            spec=dict(copy.deepcopy(spec),seed=args.seed)
            try:
                result=play.run(exe,audit.case_env(env,spec),group+'-'+name,spec,keys)
                verify(result,check)
                result.update(stress_group=group,case_name=name)
                results.append(result)
            except Exception as exc:
                failure=dict(group=group,name=name,error=str(exc),traceback=traceback.format_exc())
                failures.append(failure)
                print(json.dumps(failure),flush=True)
            temporary=report.with_suffix('.tmp')
            temporary.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
            temporary.replace(report)
    print(f'Stress worker {args.worker}: {len(results)} passed, {len(failures)} failures',flush=True)
    raise SystemExit(bool(failures))


if __name__=='__main__':main()
