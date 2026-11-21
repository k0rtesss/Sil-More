#!/usr/bin/env python3
"""Long, resource-limited fights through the real offscreen SDL interface.

Only initial conditions and read-only observers live in the native fixture.
The controller uses its own character state, visible UI choices and an authored
route; it never selects actions from hidden monster telemetry.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import traceback
from collections import Counter, deque
import uuid

import check_monster_ai_stress_gameplay as stress

play, audit, engine = stress.play, stress.audit, stress.engine
ROOT = stress.ROOT
OUT = ROOT / 'scripts/output/monster-ai-round10'


def instrument(destination):
    stress.instrument(destination)
    # The observer is a test artifact. Publish a completed snapshot so a busy
    # Windows filesystem never exposes a truncated JSON document to Python.
    old = 'strnfmt(path, sizeof(path), "%s/telemetry.json", SDL_getenv("CONTROL_TEST_PROFILE"));'
    assert play.FIXTURE.count(old)==1
    play.FIXTURE = play.FIXTURE.replace(old, old.replace('telemetry.json','telemetry.tmp'))
    old = '        assert(fclose(file) == 0);\n        cJSON_free(bytes);'
    assert play.FIXTURE.count(old)==1
    play.FIXTURE = play.FIXTURE.replace(old, r'''
        assert(fclose(file) == 0);
        char complete[1400];
        strnfmt(complete, sizeof(complete), "%s/telemetry.json", SDL_getenv("CONTROL_TEST_PROFILE"));
        Uint64 deadline = SDL_GetTicksNS() + 1000 * SDL_NS_PER_MS;
        bool published = false;
        do {
            if ((published = SDL_RenamePath(path, complete))) break;
            SDL_Delay(1);
        } while (SDL_GetTicksNS() < deadline);
        assert(published);
        cJSON_free(bytes);''')
    play.FIXTURE = play.FIXTURE.replace('#include "cJSON.h"',
        '#include "cJSON.h"\n#include "supplies.h"')
    play.FIXTURE = play.FIXTURE.replace('    p_ptr->truce = number(spec, "truce", 0);',
        '    p_ptr->truce = number(spec, "truce", 0);\n'
        '    p_ptr->on_the_run = number(spec, "on_the_run", 0);\n'
        '    p_ptr->morgoth_hall_entered = number(spec, "morgoth_hall_entered", 0);\n'
        '    a_info[ART_MORGOTH_3].cur_num = number(spec, "crown_removed", 0);')
    anchor = '    cJSON_Delete(spec);'
    assert play.FIXTURE.count(anchor) == 1
    play.FIXTURE = play.FIXTURE.replace(anchor, r'''
    cJSON* carried = cJSON_GetObjectItemCaseSensitive(spec, "supplies");
    cJSON_ArrayForEach(entry, carried) {
        object_type item;
        int kind = lookup_kind(number(entry, "tval", TV_POTION), number(entry, "sval", SV_POTION_HEALING));
        assert(kind > 0);
        object_prep(&item, kind);
        item.number = number(entry, "number", 1);
        object_known(&item);
        k_info[kind].aware = true;
        assert(supplies_can_absorb_object(&item));
        assert(inven_carry(&item, true) >= 0);
    }
''')
    play.FIXTURE = play.FIXTURE.replace('    p_ptr->chp = p_ptr->mhp;',
        '    p_ptr->chp = number(spec, "initial_hp", p_ptr->mhp);')
    play.FIXTURE = play.FIXTURE.replace('    p_ptr->csp = p_ptr->msp;',
        '    p_ptr->csp = number(spec, "initial_voice", p_ptr->msp);\n'
        '    p_ptr->food = number(spec, "initial_food", p_ptr->food);\n'
        '    cJSON_Delete(spec);')
    anchor = '        cJSON* player = cJSON_AddObjectToObject(root, "player");'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, r'''
        u64b rng = Rand_state_export();
        cJSON_AddNumberToObject(root, "rng_hi", (u32b)(rng >> 32));
        cJSON_AddNumberToObject(root, "rng_lo", (u32b)rng);
        if (g_song_menu.active) {
            cJSON* menu = cJSON_AddArrayToObject(root, "song_choices");
            for (int i = 0; i < g_song_menu.count; ++i) {
                const sdl_song_menu_entry_state* row = &g_song_menu.entries[i];
                cJSON* choice = cJSON_CreateObject();
                cJSON_AddStringToObject(choice, "label", row->text);
                cJSON_AddStringToObject(choice, "key", row->letter);
                cJSON_AddItemToArray(menu, choice);
            }
        }
''' + anchor)
    anchor = '        cJSON_AddNumberToObject(player, "hp", p_ptr->chp);'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, anchor + r'''
        cJSON_AddNumberToObject(player, "max_voice", p_ptr->msp);
        cJSON_AddNumberToObject(player, "food", p_ptr->food);
        cJSON_AddNumberToObject(player, "supply_weight", supplies_total_weight());
        cJSON_AddNumberToObject(player, "supply_cap", supplies_current_weight_cap());
        cJSON* stock = cJSON_AddArrayToObject(player, "supplies");
        for (int i = 0; i < supplies_entry_count(); ++i) {
            const object_type* item = supplies_entry_at(i);
            if (!item || !item->k_idx) continue;
            cJSON* row = cJSON_CreateObject();
            cJSON_AddNumberToObject(row, "tval", item->tval);
            cJSON_AddNumberToObject(row, "sval", item->sval);
            cJSON_AddNumberToObject(row, "number", item->number);
            char name[256]; object_desc(name, sizeof(name), item, true, 3);
            cJSON_AddStringToObject(row, "name", name);
            cJSON_AddItemToArray(stock, row);
        }
''')
    anchor = '        /* Terrain also detects real doors, webs, and bridge transformations. */'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, r'''
        cJSON* visible = cJSON_AddArrayToObject(root, "visible_terrain");
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                if (player_can_see_bold(y, x)) {
                    cJSON* tile = cJSON_CreateIntArray((int[]){y, x, cave_feat[y][x]}, 3);
                    cJSON_AddItemToArray(visible, tile);
                }
''' + anchor)
    anchor = 'cJSON_AddNumberToObject(o, "world_task", m->world.task);'
    assert anchor in play.FIXTURE
    play.FIXTURE = play.FIXTURE.replace(anchor, anchor + r'''
            cJSON_AddNumberToObject(o, "world_age", m->world.task_age);
            cJSON_AddNumberToObject(o, "world_retries", m->world.retries);
            cJSON_AddNumberToObject(o, "world_cooldown", m->world.cooldown);
            cJSON_AddNumberToObject(o, "armour_sides_lost", m->armor_ps_reduction);
            cJSON* dd_lost = cJSON_AddArrayToObject(o, "blow_dice_lost");
            cJSON* ds_lost = cJSON_AddArrayToObject(o, "blow_sides_lost");
            for (int b = 0; b < MONSTER_BLOW_MAX; ++b) {
                cJSON_AddItemToArray(dd_lost, cJSON_CreateNumber(m->blow_dd_reduction[b]));
                cJSON_AddItemToArray(ds_lost, cJSON_CreateNumber(m->blow_ds_reduction[b]));
            }
''')


def stock(healing=20, voice=8, food=2):
    return [dict(sval=5, number=healing), dict(sval=6, number=voice),
            dict(tval=stress.define('TV_FOOD'), sval=37, number=food)]


def policy_for(spec, budget):
    """Use only player-owned state, visible menus and the fixture's known map."""
    pending_use = None
    confirming_use = None
    desired_song = None
    song_stage = 0
    route_index = 0
    last_movement = None
    rejected = set()
    cells = [[1] * 64 for _ in range(40)]
    for tile in spec.get('tiles', []):
        for y in range(tile.get('y1', 1), tile.get('y2', tile.get('y1', 1)) + 1):
            for x in range(tile.get('x1', 1), tile.get('x2', tile.get('x1', 1)) + 1):
                cells[y][x] = tile['feat']
    passable = {1, 4, 5, 32, 33, 34, 35, 36, 37, 84, 86, 88, 98}
    directions = [(0, 1, 'KP6'), (1, 0, 'KP2'), (0, -1, 'KP4'), (-1, 0, 'KP8')]
    stages = spec.get('song_stages', [])
    route = spec.get('waypoints', [(16, 44), (24, 44), (24, 40), (16, 40)])

    def choose(profile, frames):
        nonlocal pending_use, confirming_use, desired_song, song_stage, route_index, last_movement
        f = frames[-1]
        elapsed = f['player_turn'] - frames[0]['player_turn']
        assert len(frames) < budget * 3 + 256, 'Controller is stuck at a UI boundary'
        assert len(frames) < 13 or f['player_turn'] != frames[-13]['player_turn'], (
            'Unanswered input boundary: '+f.get('question',''))
        if elapsed >= budget:
            return None
        p = f['player']
        for y, x, feat in f.get('visible_terrain', []):
            cells[y][x] = feat
        text = '\n'.join(f.get('terminal', []))
        if f.get('question','') == 'Are you sure you wish to attack with no weapon?':
            return 'y'
        if confirming_use:
            question = f.get('question', '')
            assert question.startswith(('Quaff ', 'Eat ')) and confirming_use.lower() in question.lower(), question
            confirming_use = None
            return 'y'
        if pending_use and 'letter use' in text:
            for row in f['terminal']:
                match = re.search(r'\b([a-zA-Z])\) (.*?)\s+x(\d+)\s*$', row)
                if match and pending_use.lower() in match[2].lower() and int(match[3]) > 0:
                    confirming_use = pending_use
                    pending_use = None
                    return match[1]
            raise AssertionError(f'Required supply not selectable: {text}')
        choices = f.get('song_choices', [])
        if choices:
            for row in choices:
                if desired_song and desired_song.lower() in row['label'].lower():
                    desired_song = None
                    return row['key'][0]
            raise AssertionError(f'Song choice unavailable: {desired_song}, {choices}')
        assert not pending_use, f'Supply command did not open its browser: {text}'
        assert not desired_song, 'Song command did not open its choices'

        def owned(tval, sval):
            return sum(o['number'] for o in p['supplies'] if o['tval'] == tval and o['sval'] == sval)

        if p['hp'] < p['max_hp'] * spec.get('heal_threshold', .7) and owned(75, 5):
            pending_use = 'of Healing'
            return 'q'
        if p['song'] != 100 and p['voice'] <= 2 and owned(75, 6):
            pending_use = 'of Voice'
            return 'q'
        if p['food'] < 1800 and owned(80, 37):
            pending_use = 'Lembas'
            return 'E'
        if song_stage < len(stages) and elapsed >= stages[song_stage][0]:
            desired_song = stages[song_stage][1]
            song_stage += 1
            return 's'
        if len(frames) > 1 and last_movement and f['key'] == last_movement[0]:
            if f['player_turn'] == frames[-2]['player_turn']:
                rejected.add(last_movement[1])
        if (spec.get('stationary') or elapsed < spec.get('quiet_until', 0)
                or (not spec.get('continuous_movement') and elapsed % 160 < 64)):
            return 'KP5'
        pos = (p['y'], p['x'])
        if pos == tuple(route[route_index % len(route)]):
            route_index += 1
            rejected.clear()
        goal = tuple(route[route_index % len(route)])
        queue = deque([pos])
        first = {pos: None}
        while queue:
            y, x = queue.popleft()
            if (y, x) == goal:
                break
            for dy, dx, key in directions:
                point = (y + dy, x + dx)
                if (point not in first and 0 < point[0] < 39 and 0 < point[1] < 63
                        and cells[point[0]][point[1]] in passable and point not in rejected):
                    first[point] = first[(y, x)] or (key, point)
                    queue.append(point)
        if goal in first and first[goal]:
            last_movement = first[goal]
            return last_movement[0]
        rejected.clear()
        return 'KP5'

    return choose


def cases():
    common = dict(recognition=1, melee=0, will=20, allow_death=True,
                  supplies=stock(36, 6), confirm_acid=True, confirm_traps=True, decline_chasms=True)
    def party(races, positions=None, **status):
        positions = positions or [(20, 34), (18, 33), (22, 33), (17, 31), (23, 31), (19, 30), (21, 30), (20, 28)]
        return [dict(race=r, y=y, x=x, heard=1, morale=0, **status)
                for r, (y, x) in zip(races, positions)]
    pillars = [dict(y1=15, y2=19, x1=36, feat=56), dict(y1=22, y2=25, x1=38, feat=56)]
    doors = [dict(y1=1, y2=38, x1=37, feat=63), dict(y1=18, x1=37, feat=32), dict(y1=23, x1=37, feat=33)]
    rows = [
        ('orc-reserve-captain', dict(evasion=16, monsters=party([91,81,61,51,51,41,76]), tiles=pillars), 800),
        ('easterling-block-and-rally', dict(evasion=19, monsters=party([409,82,82,103,103,121,115]), tiles=doors), 800),
        ('concentration-and-sprint', dict(evasion=26, monsters=party([403,407,82,103]), tiles=pillars), 1000),
        ('shadow-and-zone', dict(evasion=28, song=8, song_skill=24,
             abilities=[dict(skill=7,ability=1),dict(skill=7,ability=5)],
             song_stages=[(80,'Challenge'),(160,'Stop'),(260,'Revealing'),(360,'Staunching'),(480,'Stop'),(600,'Revealing')],
             monsters=party([408,406,407,91,51]), tiles=pillars), 1000),
        ('cold-spirits-and-poison', dict(evasion=27, cold_resistance=1,
             monsters=party([403,404,134,152]), tiles=pillars), 800),
        ('dragons-lair-and-support', dict(evasion=29, fire_resistance=1,
             monsters=party([181,402,409]), tiles=pillars), 800),
        ('cats-command-and-invisibility', dict(evasion=31, song=8, song_skill=30,
             monsters=party([185,165,175,175]), tiles=doors), 800),
        ('oath-summons-and-hunger', dict(evasion=34, initial_food=2500,
             monsters=party([244,153,93,75]), tiles=pillars), 800),
        ('spiders-web-and-status', dict(evasion=30, poison_resistance=1,
             monsters=party([242,224,211,171]), tiles=doors), 800),
        ('spiders-lair-intrusion', dict(evasion=32, poison_resistance=1,
             monsters=party([242,224,211,171]), tiles=pillars,
             waypoints=[(20,34),(24,34),(16,34),(20,40)]), 1000),
        ('spiders-controlled-intrusion', dict(evasion=31, will=35, poison_resistance=1,
             monsters=party([224,71,407]), tiles=pillars,
             waypoints=[(20,34),(24,34),(16,34),(20,40)]), 1000),
        ('breath-moat-and-shadow', dict(evasion=26, cold_resistance=1,
             monsters=party([181,134,152,161,408],
                 positions=[(20,37),(19,38),(21,38),(22,38),(18,37)]),
             tiles=[dict(y1=1,y2=38,x1=39,feat=2)],
             waypoints=[(18,40),(22,40),(20,42),(20,40)]), 1000),
        ('giants-boulders-and-doors', dict(evasion=25, cold_resistance=1,
             monsters=party([135,146,114]), tiles=doors), 800),
        ('archer-water-detours', dict(evasion=18, monsters=party([91,61,51,51,51,76]),
             tiles=pillars+[dict(y1=16,y2=24,x1=34,x2=35,feat=stress.define('FEAT_DEEP_WATER'))]), 1200),
        ('timed-recovery-and-darkness', dict(evasion=27, glow=0, torch_fuel=120,
             monsters=party([403,407,408,134], hasted=24, confused=12, mana=0),
             tiles=pillars), 1000),
        ('smite-riposte-and-knockback', dict(evasion=33,
             abilities=[dict(skill=2,ability=8),dict(skill=0,ability=10),dict(skill=2,ability=9)],
             monsters=party([411,403,192,407]), tiles=pillars), 1000),
        ('earthquake-and-firing-lanes', dict(evasion=32,
             monsters=party([410,91,51,103,61]), tiles=doors), 1000),
        ('vengeance-and-regeneration', dict(evasion=33, poison_resistance=1, melee=8,
             monsters=party([253,214,192,151]), tiles=pillars), 1000),
        ('disarm-and-weapon-theft', dict(evasion=27,
             monsters=party([406,115,403,121]), tiles=pillars,
             floor_objects=[dict(x=37),dict(x=38,cursed=1)]), 1000),
        ('weapon-degradation-and-blow-switching', dict(evasion=34, fire_resistance=1,
             song=15, song_skill=60, abilities=[dict(skill=7,ability=1)],
             song_stages=[(250,'Challenge'),(400,'Stop')],
             monsters=party([406,241,115,407], positions=[(20,39),(19,38),(20,37),(21,38)]),
             tiles=pillars, floor_objects=[dict(x=38),dict(x=37,cursed=1)]), 1000),
        ('brood-web-and-command', dict(evasion=23, poison_resistance=1,
             monsters=party([44,71,409,82,103]), tiles=pillars), 1000),
        ('dimness-and-shadow-population', dict(evasion=21,
             monsters=party([101,23,64,408]), tiles=pillars, torch_fuel=500), 1000),
        ('watcher-screech-and-sprint', dict(evasion=24,
             monsters=party([203,407,403,103], positions=[(20,38),(18,34),(22,34),(20,33)]),
             tiles=pillars), 800),
        ('morgoth-songs-and-mixed-command', dict(evasion=29, on_the_run=1, crown_removed=1,
             depth=20, morgoth_hall_entered=1, song=4, song_skill=18,
             monsters=party([251,409,82,103]), tiles=pillars), 1000),
        ('morgoth-hall-gates-and-earthquakes', dict(evasion=29, crown_removed=1, depth=20,
             morgoth_hall_entered=1, monsters=party([251,411,91,51]), tiles=pillars), 1000),
    ]
    for name, extra, budget in rows:
        spec = dict(copy.deepcopy(common), **copy.deepcopy(extra))
        yield name, spec, budget
    for workers in (1, 3):
        for interrupt in (False, True):
            bridges = [(y, x) for y in (17, 19, 21, 23) for x in range(14, 22)]
            tiles = [dict(y1=1,y2=38,x1=30,feat=63)]
            if interrupt:
                tiles.append(dict(y1=20,x1=30,feat=32))
            tiles += [dict(y1=y,x1=x,feat=88) for y,x in bridges]
            cells = [dict(y=y,x=x,flags=16,bridge=88,underlay=84,material=1,integrity=40)
                     for y,x in bridges]
            spec = dict(copy.deepcopy(common), evasion=12, player_x=32 if interrupt else 50, environment=1,
                tiles=tiles, environment_cells=cells, stationary=not interrupt,
                quiet_until=6 if interrupt else 320, continuous_movement=interrupt,
                waypoints=[(20,14),(24,22),(16,22)],
                monsters=[dict(race=41,x=13,y=19+i,alertness=-10,morale=0) for i in range(workers)])
            spec['world_exhaustion'] = not interrupt
            spec['world_interruption'] = interrupt
            yield f'bridge-{workers}-workers-'+('early-contact' if interrupt else 'exhaustion'), spec, 800


def verify(result, budget):
    stress.verify(result, {})
    frames, events = engine.read_run(result)
    combat = [e for e in events if e['kind'] in ('melee', 'ranged', 'reaction')]
    start = frames[0]['player_turn']
    result['target_turns'] = budget
    first_combat = min((e['player_turn'] for e in combat), default=start)
    last_combat = max((e['player_turn'] for e in combat), default=start)
    result['first_combat_turn'] = first_combat-start if combat else None
    result['last_combat_turn'] = last_combat-start if combat else None
    result['combat_span'] = last_combat-first_combat+1 if combat else 0
    result['combat_events'] = len(combat)
    result['late_combat_events'] = sum(e['player_turn'] - start >= budget * .75 for e in combat)
    result['incoming_hp_loss'] = sum(max(0, a['player']['hp'] - b['player']['hp']) for a, b in zip(frames, frames[1:]))
    result['net_healing'] = sum(max(0, b['player']['hp'] - a['player']['hp']) for a, b in zip(frames, frames[1:]))
    result['healing_used'] = sum(b['key'] == 'y' and a.get('question', '').startswith('Quaff ')
        and 'of Healing' in a['question'] and b['player_turn'] > a['player_turn']
        for a, b in zip(frames, frames[1:]))
    result['voice_used'] = sum(b['key'] == 'y' and a.get('question', '').startswith('Quaff ')
        and 'of Voice' in a['question'] and b['player_turn'] > a['player_turn']
        for a, b in zip(frames, frames[1:]))
    result['food_used'] = sum(b['key'] == 'y' and a.get('question', '').startswith('Eat ')
        and b['player_turn'] > a['player_turn'] for a, b in zip(frames, frames[1:]))
    def healing_units(frame):
        return sum(o['number'] for o in frame['player']['supplies'] if o['tval']==75 and o['sval']==5)
    result['other_observed_healing_losses'] = max(0,
        sum(max(0, healing_units(a)-healing_units(b)) for a,b in zip(frames,frames[1:]))-result['healing_used'])
    assert result['died'] or result['player_turns'] >= budget, 'Long encounter stopped early'
    for frame in frames:
        p = frame['player']
        assert 0 <= p['supply_weight'] <= p['supply_cap'], 'Supplies exceeded normal carrying capacity'
    result['long_fight'] = result['combat_span'] >= 400 and len(combat) >= 100
    result['minimum_mana'] = {str(race):min(m['mana'] for f in frames for m in f['monsters'] if m['race']==race)
                              for race in result['race_ids']}
    result['songs_used'] = sorted({f['player']['song'] for f in frames})
    result['knowledge_features'] = sorted({i for f in frames for m in f['monsters']
                                            for i,value in enumerate(m['knowledge']) if value})
    spec = json.loads((Path(result['profile'])/'scenario.json').read_text())
    if spec.get('world_exhaustion'):
        assert all(m['supplies']==0 for m in frames[-1]['monsters']), 'Bridge workers never exhaust their finite supplies'
        # The first snapshot precedes the lazy production world initialization.
        used = len(frames[0]['monsters'])*24 - sum(m['supplies'] for m in frames[-1]['monsters'])
        progress = sum(4 for b in frames[-1]['bridges'] if b['integrity']==100) + sum(b['work'] for b in frames[-1]['bridges'])
        assert progress == used, f'Bridge work is not conserved: {progress} != {used}'
        result['world_work_units'] = progress
        result['world_completed_bridges'] = sum(b['integrity']==100 for b in frames[-1]['bridges'])
    if spec.get('world_interruption'):
        assert any(m['world_task']==2 for f in frames for m in f['monsters']), 'Workers never repair before contact'
        contact = [(f,m) for f in frames for m in f['monsters'] if m['sees_player']]
        assert contact, 'Player never reaches visible contact with a worker'
        assert any(m['world_task']==0 and (m['stance']==1 or m['action']!=10) for f,m in contact), (
            'Workers never abandon their work after noticing the player')
        result['contact_response'] = 'fleeing' if any(m['stance']==1 for f,m in contact) else 'combat'
        if spec.get('continuous_movement'):
            assert any(0 < m['supplies'] < 24 for f,m in contact), 'Early contact missed the finite-work phase'
    if result['name'].startswith(('spiders-lair-intrusion','spiders-controlled-intrusion')):
        assert result['died'] or result['combat_span'] >= 400, 'Lair intrusion never becomes a sustained fight'
        assert any(e['kind']=='melee' for e in events), 'No spider contact'
    if result['name'].startswith('breath-moat-and-shadow'):
        assert {99,100,101,102}.issubset(result['cast_types']), 'Not every staged breath occurs'
        assert result['combat_span'] >= 400, 'Ranged fight ends before the long-fight interval'
    session = json.loads((Path(result['profile'])/'control/session.json').read_text())
    assert not session['running'], 'Session did not close cleanly'
    return result


def pilot(exe, env):
    spec = dict(evasion=20, will=15, initial_hp=180, supplies=stock(),
                monsters=[dict(race=61, x=36, heard=1, morale=150)])
    profile, process, game = play.start(exe, audit.case_env(env, spec), 'supply-pilot', spec)
    frames = []
    try:
        for key in [None, 'q']:
            response = game.observe(output=profile/'initial.png') if key is None else game.key(key, output=profile/'potions.png')
            frames.append(dict(key=key, telemetry=play.telemetry(profile), state=response['state']))
        print(json.dumps(dict(profile=str(profile), terminal=frames[-1]['state']['terminal'],
                              player=frames[-1]['telemetry']['player'])), flush=True)
    finally:
        (profile/'pilot.json').write_text(json.dumps(frames, indent=2))
        if process.poll() is None:
            game.key('Escape', capture=False)
            engine.close_game(process, game)


def persistence(exe, env, spec, name, checkpoint_turns=400, suffix_turns=450):
    """Save after a long real fight, then drive two isolated loaded continuations."""
    prefix = policy_for(spec, checkpoint_turns)
    saved = False
    def until_save(profile, frames):
        nonlocal saved
        key = prefix(profile, frames)
        if key is None and not saved:
            assert not frames[-1].get('question') and not frames[-1].get('song_choices')
            saved = True
            return 'Ctrl+s'
        return key
    checkpoint = play.run(exe, audit.case_env(env, spec), name+'-checkpoint', spec, until_save)
    verify(checkpoint, checkpoint_turns)
    assert not checkpoint['died']
    checkpoint_path = Path(checkpoint['profile'])
    before = json.loads((checkpoint_path/'final-state.json').read_text())
    source_save = Path(before['savefile'])
    assert source_save.is_relative_to(checkpoint_path) and source_save.is_file()
    native_start = play.start
    def loaded_start(executable, environment, branch_name, scenario):
        profile = play.OUT / (branch_name+'-'+uuid.uuid4().hex[:8])
        profile.mkdir()
        (profile/'scenario.json').write_text(json.dumps(scenario, indent=2))
        for folder in ('save', 'data', 'user', 'meta', 'bone'):
            source = checkpoint_path/folder
            if source.exists():
                shutil.copytree(source, profile/folder)
        game = play.GameControl(profile/'control', timeout=30)
        process = play.subprocess.Popen([str(executable),'--windowed','--control-dir',str(game.directory)],
            cwd=ROOT, env=dict(environment, CONTROL_TEST_PROFILE=str(profile),
                CONTROL_TEST_LOG_BASE=str(profile/'game.exe'), AI_PLAYTEST_LOAD=str(profile/'save'/source_save.name)),
            stdout=(profile/'stdout.txt').open('w'), stderr=(profile/'stderr.txt').open('w'),
            creationflags=play.subprocess.CREATE_NO_WINDOW if play.os.name=='nt' else 0)
        try:
            engine.wait_loaded(profile, process, game)
        except Exception:
            engine.close_game(process, game)
            raise
        (profile/'session.json').write_text(json.dumps(dict(pid=process.pid, profile=str(profile))))
        return profile, process, game
    branches = []
    play.start = loaded_start
    try:
        for letter in ('a', 'b'):
            result = play.run(exe, audit.case_env(env, spec), name+'-load-'+letter, spec, policy_for(spec, suffix_turns))
            verify(result, suffix_turns)
            assert result['initial']['player_turn'] == before['player_turn']
            assert result['initial']['player'] == before['player'], 'Loaded character or supply state changed'
            assert engine.persistent_monsters(result['initial']) == engine.persistent_monsters(before), 'Loaded AI state changed'
            branches.append(result)
    finally:
        play.start = native_start
    first_frames, first_events = engine.read_run(branches[0])
    second_frames, second_events = engine.read_run(branches[1])
    assert len(first_frames) == len(second_frames)
    for a, b in zip(first_frames, second_frames):
        assert a['key'] == b['key'] and a['player_turn'] == b['player_turn']
        assert a['player'] == b['player'], 'Loaded character outcomes diverged'
        assert engine.persistent_monsters(a) == engine.persistent_monsters(b), 'Loaded AI decisions diverged'
    assert first_events == second_events, 'Loaded action histories diverged'
    return dict(name=name, checkpoint=checkpoint, branches=branches,
                save_sha256=hashlib.sha256(source_save.read_bytes()).hexdigest(), passed=True)


def direct_equivalence(report, selected, env, destination):
    """Compare a loaded continuation with uninterrupted combat in the same binary."""
    entry = next(r for r in json.loads(report.read_text())['results'] if r['name']==selected)
    checkpoint, branch = entry['checkpoint'], entry['branches'][0]
    checkpoint_frames, checkpoint_events = engine.read_run(checkpoint)
    loaded_frames, loaded_events = engine.read_run(branch)
    spec = json.loads((Path(checkpoint['profile'])/'scenario.json').read_text())
    exe = next(path for path in destination.glob('build-*/fixture.exe')
               if hashlib.sha256(path.read_bytes()).hexdigest()==checkpoint['fixture_sha256'])
    manifest = json.loads((destination/f'source-manifest-{checkpoint["seed"]}.json').read_text())
    for name, digest in manifest.items():
        if Path(name).parts[:2]==('lib','edit'):
            assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest, 'Templates changed since checkpoint'
    environment = dict(env, AI_PLAYTEST_SIGNATURE=checkpoint['build_signature'])
    prefix = [f['key'] for f in checkpoint_frames[1:]]
    suffix = policy_for(spec, branch['target_turns'])
    def replay(profile, frames):
        done = len(frames)-1
        return prefix[done] if done < len(prefix) else suffix(profile, frames[len(prefix):])
    result = play.run(exe, audit.case_env(environment, spec), selected+'-uninterrupted', spec, replay)
    verify(result, checkpoint['player_turns']+branch['target_turns'])
    frames, events = engine.read_run(result)
    assert events[:len(checkpoint_events)]==checkpoint_events, 'Frozen initial fight is not reproducible'
    suffix_frames = frames[len(prefix):]
    comparisons = []
    if len(suffix_frames)!=len(loaded_frames):
        comparisons.append(dict(input_counts=[len(suffix_frames),len(loaded_frames)]))
    for i, (a,b) in enumerate(zip(suffix_frames, loaded_frames)):
        if (a['player']!=b['player'] or a['player_turn']!=b['player_turn']
                or engine.persistent_monsters(a)!=engine.persistent_monsters(b)):
            comparisons.append(dict(frame=i, direct=a, loaded=b))
            break
    if events[len(checkpoint_events):]!=loaded_events:
        comparisons.append(dict(action_histories_differ=True))
    result['load_equivalence_differences'] = comparisons
    result['comparison_kind'] = 'diagnostic: transient orders and ranges rebuild on load'
    output = destination/f'direct-{selected}.json'
    output.write_text(json.dumps(dict(results=[result], failures=[]), indent=2))
    return result


def load_previous_save(exe, env, report, destination):
    entry = json.loads(report.read_text())['results'][0]
    checkpoint = entry['checkpoint']
    profile = Path(checkpoint['profile'])
    before = json.loads((profile/'final-state.json').read_text())
    save = Path(before['savefile'])
    header = list(save.read_bytes()[:4])
    assert header == [0,9,9,9], header
    spec = json.loads((profile/'scenario.json').read_text())
    result = engine.loaded_branch(exe, audit.case_env(env,spec), profile, save.name,
                                  'legacy-0999-loaded-by-09910', ['KP5']*8)
    (Path(result['profile'])/'scenario.json').write_text(json.dumps(spec,indent=2))
    result.update(died=False, seed=spec['seed'], legacy_header=header,
        fixture_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
        build_signature=env['AI_PLAYTEST_SIGNATURE'],
        damage=result['initial']['player']['hp']-result['final']['player']['hp'])
    verify(result,8)
    assert result['initial']['player']==before['player'], 'Legacy load changed character resources'
    assert engine.persistent_monsters(result['initial'])==engine.persistent_monsters(before), 'Legacy load changed saved AI evidence'
    assert all(m['budget_plan']==m['plan_age']==m['plan_turn']==0 for m in result['initial']['monsters'])
    (destination/'legacy-load.json').write_text(json.dumps(dict(results=[result],failures=[]),indent=2))
    return result


def review(destination):
    """Recheck frozen artifacts with the latest assertions; never replay input."""
    results, failures, sources, raw, resolved = [], [], [], [], []
    issue_file = destination/'adjudicated-test-issues.json'
    issues = json.loads(issue_file.read_text()) if issue_file.exists() else []
    by_key = {(i['source'],i['name'],i['error']):i for i in issues}
    paths = sorted([*destination.rglob('long-*.json'), *destination.glob('direct-*.json'),
                    *destination.glob('legacy-load.json')])
    for path in paths:
        data = json.loads(path.read_text())
        sources.append(str(path))
        for failure in data.get('failures', []):
            key = (path.relative_to(destination).as_posix(),failure['name'],failure['error'])
            issue = by_key.get(key)
            if not issue:
                failures.append(dict(source=str(path),**failure))
                continue
            resolved.append({k:v for k,v in issue.items() if k!='validated_result'})
            if 'validated_result' in issue:
                raw.append(issue['validated_result'])
            if issue.get('replacement_profile'):
                replacement = Path(issue['replacement_profile'])
                assert (replacement/'trace.json').is_file() and (replacement/'final-state.json').is_file()
                assert not json.loads((replacement/'control/session.json').read_text())['running']
        for entry in data.get('results', []):
            runs = [entry['checkpoint'], *entry['branches']] if 'branches' in entry else [entry]
            raw.extend(runs)
            if 'branches' in entry:
                before = entry['checkpoint']['final']
                save = Path(json.loads((Path(entry['checkpoint']['profile'])/'final-state.json').read_text())['savefile'])
                if list(save.read_bytes()[:4]) >= [0,9,9,10]:
                    def budgets(frame):
                        return sorted((m['race'],m['y'],m['x'],m['budget_plan'],m['plan_age'],m['plan_turn'])
                                      for m in frame['monsters'])
                    for branch in entry['branches']:
                        assert budgets(before)==budgets(branch['initial']), 'Post-fix load changed command budgets'
                        assert not any(m['commander'] or m['role'] for m in branch['initial']['monsters'])
    assert len({r['profile'] for r in raw})==len(raw), 'A session is counted twice'
    for result in raw:
        verify(result, result['target_turns'])
        frames, events = engine.read_run(result)
        result['cast_counts'] = dict(Counter(str(e['detail']) for e in events if e['kind']=='ranged'))
        result['peak_concentration'] = max(m['concentration'] for f in frames for m in f['monsters'])
        result['smite_recovery_samples'] = sum(m['recovery']>0 for f in frames for m in f['monsters'])
        result['commander_identities'] = sorted({m['actor_id'] for f in frames for m in f['monsters'] if m['role']==1})
        assert all(0 <= e['mana'] <= 100 for e in events)
        for frame in frames:
            reservations = [(m['commander'], m['order_y'], m['order_x'])
                            for m in frame['monsters'] if m['order_valid']]
            assert len(reservations)==len(set(reservations)), 'Duplicate valid reservations in one squad'
            assert all(0 <= m['supplies'] <= 24 and 0 <= m['plan_age'] <= 8 for m in frame['monsters'])
        log = (Path(result['profile'])/'log.txt').read_text(errors='replace')
        result['melee_effects'] = sorted({int(n) for n in re.findall(r'selected=melee blow=\d+ method=\d+ effect=(\d+)',log)})
        result['visible_unarmed_disarm_selections'] = (len(re.findall(
            r'race=(?:406|241) selected=melee blow=1 method=\d+ effect=30 ordinary=1 smite=\d+ from=\([^\n]+player_armed=0',log))
            if 'player_armed=' in log else None)
        if result['name'].startswith('weapon-degradation') and 'player_armed=' in log:
            assert result['visible_unarmed_disarm_selections']==0, 'Disarming selected against a visibly unarmed target'
            assert any('Are you sure you wish to attack with no weapon?'==f.get('question') for f in frames)
        result['held_item_pickup_observed'] = any(o['held_by'] for f in frames for o in f['objects'])
        results.append(result)
    summary = dict(
        accepted_sessions=len(results), long_fight_sessions=sum(r['long_fight'] for r in results),
        long_fight_definition='At least 400 turns between first/last combat events and at least 100 combat events.',
        player_turns=sum(r['player_turns'] for r in results),
        scheduled_monster_actions=sum(r['monster_actions'] for r in results),
        melee_attacks=sum(r['melee_attacks'] for r in results),
        reaction_attacks=sum(r['reaction_attacks'] for r in results),
        ranged_support_actions=sum(r['ranged_attacks'] for r in results),
        deaths=sum(r['died'] for r in results),
        longest_session=max((r['player_turns'] for r in results), default=0),
        healing_potions_used=sum(r['healing_used'] for r in results),
        voice_potions_used=sum(r['voice_used'] for r in results),
        food_used=sum(r['food_used'] for r in results),
        observed_races=sorted({race for r in results for race in r['race_ids']}),
        ranged_types=sorted({cast for r in results for cast in r['cast_types']}),
        knowledge_features=sorted({feature for r in results for feature in r['knowledge_features']}),
        melee_effects=sorted({effect for r in results for effect in r['melee_effects']}),
        resolved_test_issues=resolved, failures=failures, sources=sources,
        limits=['Authored test encounters; not a balance or exhaustive gameplay certification.',
                'Test characters have high Constitution, finite carried supplies and ordinary enemy statistics.',
                'All player actions use the real SDL event queue with dummy video/audio drivers.',
                'Firing posts and range caches rebuild on load; uninterrupted and loaded combat need not be bit-identical.'])
    (destination/'validated-sessions.json').write_text(json.dumps(results, indent=2))
    (destination/'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pilot', action='store_true')
    parser.add_argument('--persistence', action='store_true')
    parser.add_argument('--budget-check', action='store_true')
    parser.add_argument('--review', action='store_true')
    parser.add_argument('--direct-report', type=Path)
    parser.add_argument('--legacy-save-report', type=Path)
    parser.add_argument('--direct-case', default='easterling-block-and-rally')
    parser.add_argument('--filter', default='')
    parser.add_argument('--seed', type=int, default=20261125)
    parser.add_argument('--output-dir', type=Path, default=OUT)
    args = parser.parse_args()
    destination = args.output_dir.resolve()
    if args.review:
        raise SystemExit(bool(review(destination)['failures']))
    instrument(destination)
    object_paths = shlex.split((play.BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    def fingerprint():
        digest = hashlib.sha256()
        for name in object_paths:
            if name.endswith('/src/main.c.obj'):
                continue
            digest.update(name.encode())
            digest.update((play.BUILD/name).read_bytes())
        return digest.hexdigest()
    before = fingerprint()
    exe, env = audit.build()
    assert before == fingerprint(), 'CMake objects changed during fixture linking'
    env['SIL_LOG_LEVEL'] = 'DEBUG'
    if args.pilot:
        pilot(exe, env)
        return
    if args.direct_report:
        direct_equivalence(args.direct_report.resolve(), args.direct_case, env, destination)
        return
    if args.legacy_save_report:
        load_previous_save(exe,env,args.legacy_save_report.resolve(),destination)
        return
    if args.budget_check:
        spec = dict(seed=args.seed, recognition=0, evasion=30, will=30, stationary=True,
            monsters=[dict(race=409,x=34,y=18,heard=1,morale=150),
                      dict(race=82,x=36,y=20,heard=1,morale=150),
                      dict(race=82,x=31,y=20,heard=1,morale=150)])
        result = persistence(exe, env, spec, 'squad-budget', 3, 4)
        (destination/f'budget-{args.seed}.json').write_text(json.dumps(result, indent=2))
        before = result['checkpoint']['final']
        loaded = result['branches'][0]['initial']
        print('Command budget before/loaded:',[(m['race'],m['budget_plan'],m['plan_age'],m['plan_turn']) for m in before['monsters']],
              [(m['race'],m['budget_plan'],m['plan_age'],m['plan_turn']) for m in loaded['monsters']], flush=True)
        print('RNG before/loaded:',(before['rng_hi'],before['rng_lo']),(loaded['rng_hi'],loaded['rng_lo']),flush=True)
        for a,b in zip(before['monsters'],loaded['monsters']):
            assert (a['budget_plan'],a['plan_age'],a['plan_turn'])==(b['budget_plan'],b['plan_age'],b['plan_turn']), 'Loading renews the finite command budget'
        return
    files = sorted({Path(__file__), *(ROOT/'src').rglob('*.c'), *(ROOT/'src').rglob('*.h'),
                    *(ROOT/'lib/edit').glob('*.txt'), *(ROOT/'scripts').glob('check_monster*gameplay.py')})
    manifest = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    manifest.update(production_executable=hashlib.sha256((play.BUILD/'sil-more.exe').read_bytes()).hexdigest(),
                    fixture_source=hashlib.sha256(exe.with_suffix('.c').read_bytes()).hexdigest(),
                    linked_objects=before)
    env['AI_PLAYTEST_SIGNATURE'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    (destination/f'source-manifest-{args.seed}.json').write_text(json.dumps(manifest, indent=2))
    results, failures = [], []
    report = destination/f'long-{args.seed}.json'
    for name, spec, budget in cases():
        if args.filter and not re.search(args.filter, name):
            continue
        spec['seed'] = args.seed
        try:
            if args.persistence:
                result = persistence(exe, env, spec, name)
            else:
                result = play.run(exe, audit.case_env(env, spec), name, spec, policy_for(spec, budget))
                verify(result, budget)
            results.append(result)
        except Exception as exc:
            failure = dict(name=name, error=str(exc), traceback=traceback.format_exc())
            failures.append(failure)
            print(json.dumps(failure), flush=True)
        report.with_suffix('.tmp').write_text(json.dumps(dict(results=results, failures=failures), indent=2))
        report.with_suffix('.tmp').replace(report)
    print(f'Long fights: {len(results)} passed, {len(failures)} failures', flush=True)
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
