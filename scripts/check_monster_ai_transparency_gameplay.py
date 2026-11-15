#!/usr/bin/env python3
"""Real SDL-input encounters and action-log checks, without a game window."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_adversarial_gameplay as audit

play, engine, coordination = audit.play, audit.engine, audit.coordination
OUT = audit.ROOT / 'scripts/output/monster-ai-round5'
troop = coordination.troop


def cases():
    result = []
    families = [
        ('captain', [91, 81, 61, 51, 51]),
        ('uldor', [115, 82, 121, 103, 103]),
        ('baugon', [76, 41, 61, 51, 51]),
        ('cat-prince', [165, 177, 154, 175, 175]),
        ('cat-queen', [185, 177, 154, 175, 175]),
        ('giants', [135, 146, 114, 114, 114]),
    ]
    positions = [(20, 36), (18, 37), (22, 37), (17, 34), (23, 34)]
    screens = [dict(y1=16, y2=19, x1=36, feat=63),
               dict(y1=22, y2=25, x1=38, feat=63)]
    crossings = [
        dict(y1=10, y2=30, x1=38, feat=100),
        dict(y1=18, x1=38, feat=98), dict(y1=22, x1=38, feat=98)]
    moving = ['KP5']*10 + ['KP2']*3 + ['KP6']*3 + ['KP8']*6 + ['KP4']*3 + ['KP2']*3 + ['KP5']*20
    songs = ['KP5']*10 + ['s', 'b'] + ['KP5']*10 + ['s', 'q'] + ['KP5']*10 + ['s', 's'] + ['KP5']*16
    for family, races in families:
        for layout, tiles in [('screens', screens), ('two-crossings', crossings)]:
            party = [troop(race, x, y) for race, (y, x) in zip(races, positions)]
            for state in ('recovery', 'song-changes'):
                spec = dict(evasion=42, will=25, monsters=copy.deepcopy(party),
                            tiles=copy.deepcopy(tiles), allow_death=True)
                check = dict(shots='any')
                if state == 'recovery':
                    spec['monsters'][0]['confused'] = 3
                    spec['monsters'][1]['recovery'] = 1
                    spec['monsters'][3]['stunned'] = 4
                    keys = moving
                else:
                    spec.update(song=14, song_skill=0, abilities=[
                        dict(skill=7, ability=1), dict(skill=7, ability=16)])
                    keys = songs
                    check['songs'] = [14, 1, 16, 100]
                result.append((f'{family}-{layout}-{state}', spec, keys, check))
        spec = dict(evasion=42, will=25, monsters=copy.deepcopy(party),
                    tiles=copy.deepcopy(crossings), allow_death=True)
        spec['monsters'][0].update(hp_percent=25, poison=12, morale=-100, stance=1)
        # Nearby followers can legitimately rout with an injured leader;
        # demand an observed morale/command response rather than forced fire.
        result.append((f'{family}-crossings-wounded-leader', spec, moving,
                       dict(solo=True, wounded_leader=True)))

    for name, x, recognition, light, starts in [
            ('remote', 12, 0, 10000, True), ('max-sight-outside', 19, 0, 10000, True),
            ('max-sight-boundary', 20, 0, 10000, False), ('near', 37, 0, 10000, False),
            ('dark-alert-observer', 34, 1, 0, False), ('lit-recognized', 39, 1, 10000, False)]:
        spec = dict(evasion=42, song_skill=80, recognition=recognition, glow=0,
                    torch_fuel=light, abilities=[dict(skill=7, ability=13)],
                    monsters=[troop(51, x, energy=-100)])
        result.append((f'disguise-start-{name}', spec, ['s', 'n'] + ['KP5']*16,
                       dict(solo=True, disguise_starts=starts)))
    spec = dict(evasion=42, recognition=0, hallucination=80,
                monsters=[troop(91, 36), troop(61, 37, 18), troop(51, 34, 19)])
    result.append(('hallucinating-command-and-archer', spec, ['KP5']*24,
                   dict(shots='any', hallucinating=True)))
    return result


def messages(result):
    frames, _ = engine.read_run(result)
    return sorted({text for frame in frames for text in frame.get('messages', [])
                   if re.search(r'hold formation|allies to advance|allies to regroup|leads its (?:pack|group)|firing position', text)})


def pressure_cases():
    """Log-driven regressions and mixed formations in constrained approaches."""
    result = []
    def add(name, spec, keys=None, turns=32, **check):
        result.append((name, dict(evasion=40, allow_death=True, **spec),
                       keys if keys is not None else ['KP5']*turns, check))
    hybrids = dict(recognition=0, monsters=[troop(146,36), troop(114,35,18), troop(114,35,22)])
    mixed = dict(recognition=0, monsters=[troop(146,36), troop(135,35,18),
                                         troop(114,35,22), troop(114,34,20)])
    bows = dict(monsters=[troop(409,36), troop(82,37,18), troop(82,37,22),
                          troop(103,34,19), troop(103,34,21), troop(103,33)])
    for quarter in range(4):
        for name, spec, check in [
                ('hybrid-giants', hybrids, dict(hybrid_melee='all')),
                ('mixed-giant-ranges', mixed, dict(hybrid_melee='any', shots='any')),
                ('shielded-three-bows', bows, dict(shots='all'))]:
            rotated, keys = audit.rotate(spec, ['KP5']*28, quarter)
            add(f'pressure-{name}-{quarter}', rotated, keys, **check)
    # The same occasional missiles remain available when a real obstacle
    # prevents melee. Role selection must not remove the attack itself.
    moat = copy.deepcopy(hybrids)
    moat['tiles'] = [dict(y1=19,y2=21,x1=39,x2=41,feat=2),
                     dict(y1=20,x1=40,feat=1)]
    add('pressure-hybrids-across-chasm-moat', moat, turns=32, shots='any', no_melee=True)
    # Both neutral bands fight the player, but custom allegiance must keep
    # the two commanders from recruiting one another's soldiers.
    bands = dict(monsters=[troop(91,36,17,group=8), troop(61,36,19,group=8),
                           troop(51,34,18,group=8), troop(76,36,23,group=9),
                           troop(41,36,21,group=9), troop(51,34,22,group=9)])
    for quarter in range(4):
        rotated, keys = audit.rotate(bands, ['KP5']*36, quarter)
        add(f'pressure-neutral-command-bands-{quarter}', rotated, keys,
            shots='any', separate_bands=True)
    corner_party = dict(monsters=[troop(91,36), troop(61,35,18),
                                  troop(41,35,22), troop(51,34,19),troop(51,34,21)])
    for corner, py, px, quarter in [('nw',3,3,2), ('ne',3,60,0),
                                    ('sw',36,3,2), ('se',36,60,0)]:
        spec, keys = audit.rotate(corner_party, ['KP5']*32, quarter)
        dy, dx = py-20, px-40
        spec.update(player_y=py, player_x=px)
        for m in spec['monsters']:
            m['y'] += dy; m['x'] += dx
        add(f'pressure-map-corner-{corner}', spec, keys, shots='any', melee=True)
    choke = dict(recognition=0, tiles=[
        dict(y1=1,y2=38,x1=1,x2=62,feat=63),
        dict(y1=14,y2=26,x1=39,x2=48,feat=1),
        dict(y1=20,x1=25,x2=39,feat=1),
        dict(y1=17,y2=23,x1=32,feat=1), dict(y1=20,x1=38,feat=32)],
        monsters=[troop(91,35),troop(51,28),troop(146,32,18),troop(114,32,22)])
    releasing = ['KP5']*10+['KP6']*3+['KP2']*2+['KP5']*28
    for quarter in range(4):
        spec, keys = audit.rotate(choke, releasing, quarter)
        add(f'pressure-released-door-queue-{quarter}', spec, keys,
            melee=True, hybrid_melee='any', archer_response=True)
    for visible in (False, True):
        for label, emitter_x in [('dim',45), ('bright',41)]:
            spec = dict(recognition=0,
                monsters=[troop(146,emitter_x,energy=-800), troop(51,35)])
            if not visible:
                spec['tiles'] = [dict(y1=1,y2=38,x1=38,feat=63)]
            add(f'pressure-light-{"visible" if visible else "hidden"}-{label}',
                spec, turns=6, solo=True, light_visible=visible)
    return result


def verify_light_pairs(results):
    """Use completed-action diagnostics to compare the isolated light probes."""
    by_name = {row['name']:row for row in results}
    comparisons = []
    for kind in ('hidden','visible'):
        names = [f'pressure-light-{kind}-{value}' for value in ('dim','bright')]
        if not all(name in by_name for name in names): continue
        observations = []
        for name in names:
            text = (Path(by_name[name]['profile'])/'log.txt').read_text(encoding='utf-8')
            record = next(line for line in text.splitlines()
                          if '[monster-ai]' in line and 'world_turn=' in line and 'race=51 ' in line)
            values = {key:int(value) for key,value in re.findall(
                r'\b(morale|player_seen|own_light|player_light|stance)=(-?\d+)',record)}
            assert len(values) == 5, 'Incomplete morale diagnostic'
            assert values['player_seen'] == (kind == 'visible')
            observations.append(values)
        dim, bright = observations
        assert dim['player_light'] == 3 and bright['player_light'] > 3
        assert dim['own_light'] == bright['own_light']
        difference = bright['morale']-dim['morale']
        expected = 0 if kind == 'hidden' else -(bright['player_light']-3)*10
        if kind == 'hidden':
            assert difference == 0, 'Unseen player light changes morale'
        else:
            # Completed actions include the existing morale feedback when an
            # actor enters fleeing stance. The engine-level test isolates the
            # exact light penalty; this live encounter also permits that rout.
            assert difference <= expected, 'Visible light loses its morale penalty'
            if difference < expected:
                assert bright['stance'] == 1, 'Unexplained additional morale loss'
        comparisons.append(dict(kind=kind,dim=dim,bright=bright))
    return comparisons


def verify_log(result):
    profile = Path(result['profile'])
    log = (profile/'log.txt').read_text(encoding='utf-8', errors='replace')
    lines = [line for line in log.splitlines() if '[monster-ai]' in line]
    frames, events = engine.read_run(result)
    if not lines:
        # The default log deliberately omits unchanged sleepers. Validate
        # that exact state instead of requiring a misleading active record.
        assert events and all(e['kind'] == 'action' and e['skipped']
                              and (e['y'], e['x']) == (e['old_y'], e['old_x']) for e in events)
        assert all(m['alertness'] < -10 for f in frames for m in f['monsters'])
        initial_hp = {m['actor_id']: m['hp'] for m in frames[0]['monsters']}
        assert all(m['hp'] == initial_hp[m['actor_id']] for f in frames for m in f['monsters'])
        return dict(logged_actions=0, logged_casts=0, tactical_messages=[],
                    quiet_sleepers=True)
    action_lines = [line for line in lines if 'world_turn=' in line]
    assert action_lines
    assert all('role=' in line and 'sense=' in line and 'order_valid=' in line
               and 'completed=' in line for line in action_lines)
    # DEBUG suppresses only unchanged sleepers. Every completed live awake
    # action must have a corresponding record with the real coordinates.
    pattern = re.compile(r'player_turn=(\d+) world_turn=(\d+) actor=(\d+) race=(\d+).*?from=\((\d+),(\d+)\) to=\((\d+),(\d+)\)')
    records = {tuple(map(int, match.groups())) for line in action_lines
               if (match := pattern.search(line))}
    previous_hp = {m['actor_id']: m['hp'] for m in frames[0]['monsters']}
    for event in events:
        if event['kind'] != 'action':
            continue
        identity = (event['player_turn'], event['turn'], event['index'], event['race'],
                    event['old_y'], event['old_x'], event['y'], event['x'])
        # Wandering may put a creature to sleep during an otherwise completed
        # action. Match the logger's unchanged-sleeper rule as well as skips.
        quiet_sleeper = (event.get('alertness', 0) < -10
                         and (event['y'], event['x']) == (event['old_y'], event['old_x'])
                         and event['hp'] == previous_hp.get(event['actor_id']))
        assert identity in records or event['skipped'] or quiet_sleeper, f'Missing action log: {event}'
        previous_hp[event['actor_id']] = event['hp']
    cast_lines = [line for line in lines if ' attack=' in line]
    casts = [e for e in events if e['kind'] == 'ranged']
    assert len(cast_lines) == len(casts), 'Ranged diagnostic count differs from completed casts'
    return dict(logged_actions=len(action_lines), logged_casts=len(cast_lines),
                tactical_messages=messages(result))


def verify(result, check):
    generic = {key: value for key, value in check.items()
               if key in ('shots', 'solo', 'social', 'melee')}
    coordination.verify_varied(result, generic)
    frames, events = engine.read_run(result)
    if 'songs' in check:
        assert set(check['songs']) <= {f['player']['song'] for f in frames}, 'Song transitions were not exercised'
    if 'disguise_starts' in check:
        start = next(f for f in frames if f['key'] == 'n')
        assert (start['player']['song'] == 13) == check['disguise_starts']
        first = frames[0]['monsters'][0]
        assert first['sees_player'] != check['disguise_starts'], 'Scene does not distinguish actual observation'
        if not check['disguise_starts']:
            assert any('while observed' in text for f in frames for text in f.get('messages', []))
    if check.get('hallucinating'):
        assert not messages(result), 'Tactical explanations reveal real identities while hallucinating'
    if check.get('wounded_leader'):
        leader = frames[0]['monsters'][0]['actor_id']
        assert any(e['actor_id'] == leader and e['hp'] < frames[0]['monsters'][0]['hp']
                   for e in events), 'Poison never affects the wounded commander'
        assert any(m['actor_id'] != leader and (m['stance'] == 1 or m['order_valid'])
                   for f in frames[3:] for m in f['monsters']), 'No follower morale or command response'
    if check.get('hybrid_melee'):
        hybrids = {m['actor_id'] for m in frames[0]['monsters'] if m['race'] in (114,146)}
        fighting = {e['actor_id'] for e in events if e['kind'] == 'melee'} & hybrids
        assert fighting, 'Melee-oriented giants remain parked at missile posts'
        if check['hybrid_melee'] == 'all':
            assert fighting == hybrids, 'Some melee-oriented giants never reach melee'
    if check.get('no_melee'):
        assert not any(e['kind'] == 'melee' for e in events), 'Melee crosses the impassable moat'
        assert all(m['feat'] != 2 for f in frames for m in f['monsters'])
    if check.get('archer_response'):
        bows = {m['actor_id'] for m in frames[0]['monsters'] if m['race'] == 51}
        assert bows
        for identity in bows:
            fired = any(e['kind'] == 'ranged' and e['actor_id'] == identity
                        and e['detail'] in (96,97) for e in events)
            # Giants emit light. In the cramped approach this can overcome
            # an orc's morale before it has a clear shot; fleeing is valid.
            routed = any(m['actor_id'] == identity and m['stance'] == 1
                         and m['morale'] < 0 and f['player']['tile_light'] >= 4
                         for f in frames for m in f['monsters'])
            assert fired or routed, 'Queued archer neither contributes nor responds to the light threat'
            assert all(not m['order_valid'] for f in frames for m in f['monsters']
                       if m['actor_id'] == identity and m['stance'] == 1)
    if 'light_visible' in check:
        assert all(m['sees_player'] == check['light_visible'] for f in frames
                   for m in f['monsters'] if m['race'] == 51)
    if check.get('separate_bands'):
        groups = {m['actor_id']: m['social_group'] for m in frames[0]['monsters']}
        assert set(groups.values()) == {8,9}
        seen_groups = set()
        for f in frames:
            leaders = {m['index']:m for m in f['monsters']}
            for m in f['monsters']:
                if not m['order_valid']: continue
                assert m['social_group'] == leaders[m['commander']]['social_group']
                seen_groups.add(m['social_group'])
        assert seen_groups == {8,9}, 'Both bands did not exercise independent command'
    return verify_log(result)


def compare_runs(first, second):
    first_frames, first_events = engine.read_run(first)
    second_frames, second_events = engine.read_run(second)
    assert first_events == second_events, 'Logging changed scheduled actions or RNG outcomes'
    assert len(first_frames) == len(second_frames)
    for a, b in zip(first_frames, second_frames):
        for key in ('player', 'monsters', 'player_turn', 'turn', 'feature_probes', 'bridges'):
            assert a.get(key) == b.get(key), f'Logging changed {key} after input {a["key"]}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['new', 'pressure', 'baseline', 'adversarial', 'equivalence', 'persistence'], default='new')
    parser.add_argument('--seed', type=int, default=20261113)
    parser.add_argument('--filter', default='')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--old-fixture', type=Path)
    parser.add_argument('--log-level', choices=['TRACE', 'DEBUG'], default='DEBUG')
    parser.add_argument('--output-dir', type=Path, default=OUT)
    args = parser.parse_args()
    args.output_dir = args.output_dir.resolve()
    audit.OUT = args.output_dir
    audit.instrument()
    exe, env = audit.build()
    env['SIL_LOG_LEVEL'] = args.log_level
    digest = hashlib.sha256(env['AI_PLAYTEST_SIGNATURE'].encode())
    for module in (Path(__file__), Path(audit.__file__), Path(engine.__file__),
                   Path(coordination.__file__)):
        digest.update(module.read_bytes())
    env['AI_PLAYTEST_SIGNATURE'] = digest.hexdigest()
    report = args.report or args.output_dir/f'{args.mode}-{args.seed}.json'
    results, failures, comparisons = [], [], []
    selected = cases()
    if args.mode == 'pressure':
        selected = pressure_cases()
    elif args.mode == 'baseline':
        selected = [(name, spec, keys, dict(solo=True))
                    for name, spec, keys in play.scenarios()
                    if not name.startswith(('species-', 'ordinary-'))]
    elif args.mode == 'adversarial':
        selected = audit.cases()
    elif args.mode == 'equivalence':
        selected = [case for case in cases() if case[0] in (
            'captain-screens-recovery', 'uldor-two-crossings-song-changes',
            'hallucinating-command-and-archer')]
    elif args.mode == 'persistence':
        selected = [(name, spec, keys[:12], '') for name, spec, keys, check in cases()
                    if name in ('captain-screens-recovery', 'uldor-two-crossings-song-changes',
                                'cat-prince-two-crossings-recovery')]
    for name, spec, keys, check in selected:
        if args.filter and not re.search(args.filter, name):
            continue
        spec = dict(spec, seed=args.seed)
        try:
            if args.mode == 'persistence':
                result = engine.run_persistence(exe, audit.case_env(env, spec), name, spec, keys,
                                                 ['KP5']*24, '')
                for branch in result['branches']:
                    branch.update(verify_log(branch))
            else:
                result = play.run(exe, audit.case_env(env, spec), name, spec, keys)
                if args.mode == 'adversarial':
                    result.update(audit.verify(result, check), **verify_log(result))
                elif args.mode == 'baseline':
                    result.update(verify_log(result))
                else:
                    result.update(verify(result, check))
                if args.mode == 'equivalence':
                    for level in ('WARN', 'TRACE'):
                        alternate = play.run(exe, audit.case_env(dict(env, SIL_LOG_LEVEL=level), spec),
                                             name+'-'+level.lower(), spec, keys)
                        compare_runs(result, alternate)
                        comparisons.append(dict(reference=result['profile'], other=alternate['profile'], level=level))
                        results.append(alternate)
                    if args.old_fixture and not spec.get('hallucination'):
                        old = play.run(args.old_fixture, audit.case_env(env, spec), name+'-before-logging', spec, keys)
                        compare_runs(result, old)
                        comparisons.append(dict(reference=result['profile'], other=old['profile'], level='before-changes'))
                        results.append(old)
            result['outcome_check'] = check
            results.append(result)
        except Exception as exc:
            failures.append(dict(name=name, error=str(exc), traceback=traceback.format_exc()))
            print(json.dumps(failures[-1]), flush=True)
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(dict(results=results, failures=failures, comparisons=comparisons), indent=2))
    if args.mode == 'pressure':
        comparisons.extend(verify_light_pairs(results))
        report.write_text(json.dumps(dict(results=results, failures=failures, comparisons=comparisons), indent=2))
    print(f'{args.mode}: {len(results)} sessions, {len(failures)} failures', flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
