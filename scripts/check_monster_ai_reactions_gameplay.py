#!/usr/bin/env python3
"""Demonstrate player techniques through SDL and audit actual AI witnesses."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_stress_gameplay as stress

play, audit, engine = stress.play, stress.audit, stress.engine
ROOT=stress.ROOT
OUT=ROOT/'scripts/output/monster-ai-round9/reactions'


def instrument(destination):
    stress.instrument(destination)
    play.FIXTURE=play.FIXTURE.replace(
        'p_ptr->stat_base[A_CON] = number(spec, "constitution", 25);',
        'p_ptr->stat_base[A_CON] = number(spec, "constitution", 25);\n'
        '    p_ptr->stat_base[A_STR] = number(spec, "strength", 3);')
    play.FIXTURE=play.FIXTURE.replace('lookup_kind(TV_SWORD, SV_LONG_SWORD)',
        'lookup_kind(number(spec, "weapon_tval", TV_SWORD), number(spec, "weapon_sval", SV_LONG_SWORD))')
    play.FIXTURE+=r'''
static void fixture_technique(const char* kind, int detail, int evidence, int y, int x)
{
    char path[1400];
    strnfmt(path, sizeof(path), "%s/techniques.jsonl", SDL_getenv("CONTROL_TEST_PROFILE"));
    FILE* file = fopen(path, "ab"); assert(file);
    fprintf(file, "{\"kind\":\"%s\",\"detail\":%d,\"evidence\":%d,\"turn\":%ld,"
        "\"player_turn\":%ld,\"y\":%d,\"x\":%d}\n",
        kind, detail, evidence, (long)turn, (long)playerturn, y, x);
    assert(fclose(file) == 0);
}
void __real_monster_ai_player_attack(monster_type*, int);
void __wrap_monster_ai_player_attack(monster_type* target, int type)
{
    int y = target ? target->fy : 0, x = target ? target->fx : 0;
    __real_monster_ai_player_attack(target, type);
    fixture_technique("player_attack", type, 0, y, x);
}
void __real_monster_ai_witness(int, int, int, int);
void __wrap_monster_ai_witness(int feature, int evidence, int y, int x)
{
    __real_monster_ai_witness(feature, evidence, y, x);
    fixture_technique("witness", feature, evidence, y, x);
}
'''


def definitions():
    return {name:int(value,0) for name,value in re.findall(
        r'^#define\s+(ATT_\w+|TV_POLEARM|TV_SWORD|SV_SPEAR|SV_GREAT_SWORD)\s+(0x[0-9a-fA-F]+|\d+)\b',
        (ROOT/'src/defines.h').read_text(),re.M)}


def cases():
    common=dict(recognition=0,evasion=35,melee=0,allow_death=True)
    definitions_by_name=definitions()
    spear=dict(weapon_tval=definitions_by_name['TV_POLEARM'],weapon_sval=definitions_by_name['SV_SPEAR'],unarmoured=1)
    heavy=dict(weapon_tval=definitions_by_name['TV_SWORD'],weapon_sval=definitions_by_name['SV_GREAT_SWORD'],unarmoured=1)
    table=[
        ('flanking',2,6,10,'ATT_FLANKING',dict(unarmoured=1),['KP8','KP2']*10),
        ('retreat',2,9,11,'ATT_CONTROLLED_RETREAT',{},['KP6']*10+['KP5']*10),
        ('opportunist',3,4,12,'ATT_OPPORTUNIST',dict(flee=True),['KP5']*24),
        ('zone',0,10,13,'ATT_ZONE_OF_CONTROL',dict(race=408,song=8,song_skill=70),['KP5']*24),
        ('polearm',0,4,14,'ATT_POLEARM',dict(spear,approach=True),['KP5']*24),
        ('riposte',2,8,15,'ATT_RIPOSTE',{},['KP5']*24),
        ('charge',0,5,16,None,dict(approach=True,corridor=True),['KP4']*8+['KP5']*12),
        ('knockback',0,2,17,None,dict(heavy,melee=15,strength=12),['KP4']*24+['KP5']*8),
        ('exchange',3,3,18,None,{},['X','KP4']+['KP5']*18),
        ('impale',0,7,29,'ATT_IMPALE',dict(spear,line=True),['KP4']*8+['KP5']*12),
        ('whirlwind',0,9,30,'ATT_WHIRLWIND',dict(ring=True),['KP4']*8+['KP5']*12),
        ('follow-through',0,6,31,'ATT_FOLLOW_THROUGH',dict(melee=35,kill=True),['KP4']+['KP5']*20),
    ]
    for name,skill,ability,feature,attack,extra,keys in table:
        extra=copy.deepcopy(extra)
        race=extra.pop('race',403)
        party=[dict(race=race,x=39,heard=1,morale=150)]
        if extra.pop('flee',False):party[0].update(morale=-600,stance=1)
        if extra.pop('approach',False):party[0]['x']=34
        if extra.pop('corridor',False):
            extra['tiles']=[dict(y1=1,y2=38,x1=1,x2=62,feat=63),
                            dict(y1=20,x1=29,x2=45,feat=1)]
        if extra.pop('line',False):party.append(dict(race=82,x=38,heard=1,morale=150))
        if extra.pop('ring',False):party += [dict(race=82,x=40,y=19,heard=1,morale=150),dict(race=61,x=41,heard=1,morale=150)]
        if extra.pop('kill',False):
            party=[dict(race=41,x=39,hp_percent=10,heard=1,morale=150),dict(race=403,x=39,y=19,heard=1,morale=150)]
        spec=dict(common,**extra,abilities=[dict(skill=skill,ability=ability)],monsters=party)
        for quarter in range(4):
            rotated,inputs=audit.rotate(spec,keys,quarter)
            yield f'{name}-demonstrated-r{quarter}',rotated,inputs,dict(feature=feature,attack=attack,demonstrated=True)
        if name in ('flanking','retreat','charge','knockback','exchange','impale','whirlwind','follow-through'):
            # The character owns the ability, but waiting alone does not
            # demonstrate these active techniques to an enemy.
            for quarter in (0,2):
                rotated,inputs=audit.rotate(spec,['KP5']*20,quarter)
                yield f'{name}-unused-r{quarter}',rotated,inputs,dict(feature=feature,demonstrated=False)


def verify(result,check):
    stress.verify(result,{})
    frames,_=engine.read_run(result)
    path=Path(result['profile'])/'techniques.jsonl'
    events=[json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []
    index=check['feature']
    assert not any(m['knowledge'][index] for m in frames[0]['monsters']), 'Technique known before being demonstrated'
    observed=any(m['knowledge'][index]>0 for frame in frames for m in frame['monsters'])
    if check['demonstrated']:
        if check.get('attack'):
            expected=definitions()[check['attack']]
            assert any(e['kind']=='player_attack' and e['detail']==expected for e in events), 'Requested player technique never occurs'
        else:
            assert any(e['kind']=='witness' and e['detail']==index for e in events), 'Requested displacement/charge never occurs'
        assert observed, 'A visible demonstrated technique is never learned'
    else:
        assert not observed, 'Ability is learned without a demonstration'
    result['reaction_check']=check
    result['technique_events']=len(events)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--filter',default='')
    parser.add_argument('--seed',type=int,default=20261119)
    parser.add_argument('--negative-equivalence',action='store_true',
                        help='Compare unused learned abilities with otherwise identical characters lacking them')
    parser.add_argument('--reference-report',type=Path,action='append',default=[],
                        help='Completed technique report containing unused-ability controls; may be repeated')
    parser.add_argument('--output-dir',type=Path,default=OUT)
    args=parser.parse_args()
    destination=args.output_dir.resolve()
    instrument(destination)
    exe,env=play.build(extra_wraps=('cave_set_feat','monster_ai_player_attack','monster_ai_witness'))
    env['SIL_LOG_LEVEL']='DEBUG'
    env['AI_PLAYTEST_SIGNATURE']=hashlib.sha256((env['AI_PLAYTEST_SIGNATURE']+Path(__file__).read_text()).encode()).hexdigest()
    (destination/'driver-source.py').write_bytes(Path(__file__).read_bytes())
    results,failures=[],[]
    if args.negative_equivalence:
        references={}
        reference_reports=args.reference_report
        if not reference_reports:
            canonical=OUT/f'reactions-{args.seed}.json'
            reference_reports=[canonical] if canonical.exists() else [
                OUT.parent/directory/f'reactions-{args.seed}.json'
                for directory in ('reactions-full','reactions-unused-heavy')]
        for report in reference_reports:
            for result in json.loads(report.read_text())['results']:
                if '-unused-' in result['case_name']:references[result['case_name']]=result
        assert len(references)==16,'The sixteen unused-ability controls are required first'
        comparisons=[]
        for name,reference in references.items():
            profile=Path(reference['profile'])
            spec=json.loads((profile/'scenario.json').read_text())
            spec['abilities']=[]
            frames,_=engine.read_run(reference)
            keys=[frame['key'] for frame in frames[1:]]
            try:
                result=verify(play.run(exe,audit.case_env(env,spec),'without-'+name,spec,keys),reference['reaction_check'])
                stress.catalogue.transparency.compare_runs(reference,result)
                result.update(case_name='without-'+name,comparison_reference=reference['profile'])
                results.append(result)
                comparisons.append(dict(reference=reference['profile'],without_ability=result['profile']))
            except Exception as exc:
                failure=dict(name=name,error=str(exc),traceback=traceback.format_exc())
                failures.append(failure)
                print(json.dumps(failure),flush=True)
            (destination/f'reactions-{args.seed}.json').write_text(json.dumps(
                dict(results=results,failures=failures,comparisons=comparisons),indent=2))
        print(f'Unused-ability action equivalence: {len(results)} passed, {len(failures)} failures',flush=True)
        raise SystemExit(bool(failures))
    for name,spec,keys,check in cases():
        if args.filter and not re.search(args.filter,name):continue
        spec=dict(copy.deepcopy(spec),seed=args.seed)
        try:
            result=verify(play.run(exe,audit.case_env(env,spec),name,spec,keys),check)
            result['case_name']=name
            results.append(result)
        except Exception as exc:
            failure=dict(name=name,error=str(exc),traceback=traceback.format_exc())
            failures.append(failure)
            print(json.dumps(failure),flush=True)
        report=destination/f'reactions-{args.seed}.json'
        temporary=report.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
        temporary.replace(report)
    print(f'Player technique scenes: {len(results)} passed, {len(failures)} failures',flush=True)
    raise SystemExit(bool(failures))


if __name__=='__main__':main()
