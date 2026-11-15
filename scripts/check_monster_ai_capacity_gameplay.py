#!/usr/bin/env python3
"""Stress live squad capacity and mass commander loss through headless SDL."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import traceback

import check_monster_ai_stress_gameplay as stress

ROOT=stress.ROOT
play,engine,audit=stress.play,stress.engine,stress.audit
OUT=ROOT/'scripts/output/monster-ai-round9/capacity'


def cases():
    cells=[(y,x) for y in range(13,28) for x in range(25,56) if (y,x)!=(20,40)]
    cells.sort(key=lambda p:(max(abs(p[0]-20),abs(p[1]-40))+min(abs(p[0]-20),abs(p[1]-40))//2,p))
    for count,cull in [(40,False),(80,False),(128,False),(160,False),(240,False),(160,True)]:
        party=[]
        for i,(y,x) in enumerate(cells[:count]):
            race=91 if i%8==0 else 51 if i%3==0 else 61
            # The production per-race population limit remains enabled.
            if count>200 and race==61:race=41 if i%2 else 61
            actor=dict(race=race,x=x,y=y,heard=1,morale=150)
            if cull and race==91:actor.update(poison=30,hp_percent=25)
            party.append(actor)
        yield f'active-{count}'+('-commander-loss' if cull else ''),dict(
            recognition=0,evasion=70,will=40,allow_death=True,monsters=party), ['KP5']*32,dict(count=count,cull=cull)


def verify(result,check):
    stress.verify(result,{})
    frames,events=engine.read_run(result)
    identities={}
    indices={}
    maximum=0
    for frame in frames:
        by_index={m['index']:m for m in frame['monsters']}
        seen=set()
        commanders=set()
        for m in frame['monsters']:
            identity=m['actor_id']
            assert identity not in seen,'A live identity is duplicated'
            seen.add(identity)
            indices.setdefault(identity,set()).add(m['index'])
            assert identities.setdefault(identity,m['race'])==m['race'],'Compaction exchanges actor identities'
            if not m['order_valid']:continue
            assert m['commander'] in by_index
            commanders.add(m['commander'])
            assert not by_index[m['commander']]['confused'] and not by_index[m['commander']]['stunned']
        maximum=max(maximum,len(commanders))
        assert len(commanders)<=16,'More than sixteen squads planned in one pass'
        for commander in commanders:
            assert sum(m['order_valid'] and m['commander']==commander for m in frame['monsters'])<=8
    assert result['monster_actions']>=check['count']*8,'Large fixture does not actually schedule its actors'
    if check['count']>=128 and not check['cull']:
        assert maximum==16,'Fixture never reaches the sixteen-squad planning boundary'
    if check['cull']:
        dead={m['actor_id'] for m in frames[0]['monsters'] if m['race']==91}
        survivors={m['actor_id'] for m in frames[-1]['monsters']}
        assert dead and not dead & survivors,'Poisoned commanders never leave the array'
        if any(m['order_valid'] and m['role']==1 for m in frames[-1]['monsters']):
            result['mass_loss_outcome']='succession'
        else:
            # Twenty witnessed commander deaths can legitimately rout the
            # entire band. Fleeing survivors must cancel combat orders.
            assert all(m['stance']==1 and m['morale']<0 and not m['order_valid']
                       for m in frames[-1]['monsters']), 'Neither command nor a valid group rout'
            result['mass_loss_outcome']='rout'
        assert len(survivors)>=check['count']-len(dead)
    else:
        assert len(frames[-1]['monsters'])>=check['count'],'Uninjured crowd loses actors unexpectedly'
    result['capacity_check']=check
    result['maximum_squads']=maximum
    result['remapped_actors']=sum(len(used)>1 for used in indices.values())
    if check['cull']:assert result['remapped_actors']>0,'Mass loss never exercises live compaction'
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--filter',default='')
    parser.add_argument('--seed',type=int,default=20261125)
    parser.add_argument('--output-dir',type=Path,default=OUT)
    args=parser.parse_args()
    destination=args.output_dir.resolve()
    stress.instrument(destination)
    exe,env=audit.build()
    env['SIL_LOG_LEVEL']='DEBUG'
    env['AI_PLAYTEST_SIGNATURE']=hashlib.sha256((env['AI_PLAYTEST_SIGNATURE']+Path(__file__).read_text()).encode()).hexdigest()
    (destination/'driver-source.py').write_bytes(Path(__file__).read_bytes())
    results,failures=[],[]
    for name,spec,keys,check in cases():
        if args.filter and name!=args.filter:continue
        spec=dict(spec,seed=args.seed)
        try:
            result=verify(play.run(exe,audit.case_env(env,spec),name,spec,keys),check)
            result['case_name']=name
            results.append(result)
        except Exception as exc:
            failure=dict(name=name,error=str(exc),traceback=traceback.format_exc())
            failures.append(failure)
            print(json.dumps(failure),flush=True)
        report=destination/'results.json'
        temporary=report.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
        temporary.replace(report)
    print(f'Capacity: {len(results)} passed, {len(failures)} failures',flush=True)
    raise SystemExit(bool(failures))


if __name__=='__main__':main()
