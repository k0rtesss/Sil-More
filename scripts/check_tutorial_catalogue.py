#!/usr/bin/env python3
"""Validate authored gameplay lessons against the live source/data inventory.

Checks actual serial IDs, schema/buffer bounds, decision safety, known effect
coverage, semantic subjects, source paths and runtime producer mappings. The
optional continuous review document retains runtime placeholders. Structural
coverage is not a claim of physical-device or full mechanic replay.
"""
from pathlib import Path
import argparse
import collections
import fnmatch
import json
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOGUE = ROOT / 'lib/help/tutorials.json'
REFERENCE = ROOT / 'docs/tutorial-reference.md'


def top_level_expressions(text):
    """Split C initializer expressions while retaining commas inside calls."""
    values, start, depth, quote, escaped = [], 0, 0, None, False
    for index, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
        elif char == ',' and depth == 0:
            values.append(text[start:index].strip())
            start = index + 1
    if text[start:].strip():
        values.append(text[start:].strip())
    return values


def records(path):
    result = []
    current = None
    for line in (ROOT / path).read_text(encoding='utf-8-sig').splitlines():
        if line.startswith('N:'):
            _, serial, name = line.split(':', 2)
            current = {'serial': int(serial), 'name': name, 'fields': {}}
            result.append(current)
        elif current and len(line) > 1 and line[1] == ':':
            current['fields'].setdefault(line[0], []).append(line[2:].strip())
    return result


def source_inventory():
    sources = {str(path.relative_to(ROOT)).replace('\\', '/'): path.read_text(
        encoding='utf-8-sig', errors='replace') for path in (ROOT / 'src').rglob('*.c')}
    core = sources['src/tutorial/tutorial-game.c']
    combined = '\n'.join(sources.values())
    item_wrapper = re.search(r'static void observe_item_action\([^)]*\)\s*\{(.*?)\n\}', core, re.S)
    if item_wrapper:
        assert 'observe(id, type, name, detail);' in item_wrapper.group(1), 'Item observation wrapper no longer delegates the lesson ID'
        assert 'offered_item_actions[i] = true' in item_wrapper.group(1), 'Item candidate tracking is missing'
    observer_names = r'(?:observe|world_observe|observe_item_action)'
    direct = set(re.findall(r'\b' + observer_names + r'\(\s*"([a-z0-9_.-]+)"', combined))
    for expression in re.findall(r'\b' + observer_names + r'\(\s*([^,]+),', combined):
        direct.update(re.findall(r'"([a-z0-9_.-]+)"', expression))
    direct.update(re.findall(r'\btutorial_game_explain(?:_now)?\(\s*"([a-z0-9_.-]+)"', combined))
    for expression in re.findall(r'\btutorial_game_lifecycle\(\s*([^;)]+)\)', combined):
        direct.update(re.findall(r'"(tale\.[a-z0-9_-]+)"', expression))
    # Explicit parallel arrays in observe_extra_states are dispatched via ids[i].
    extra_states = re.search(r'static void observe_extra_states\(bool seed\)(.*?)(?=\nstatic bool gameplay_available)', core, re.S)
    if extra_states:
        block = extra_states.group(1)
        assert 'tutorial_observe(ids[i], &context)' in block
        assert re.search(r'bool now\[\]\s*=\s*\{', block)
        array = re.search(r'static const char \*ids\[\]\s*=\s*\{([^}]+)\}', block, re.S)
        assert array, 'Extra status dispatch lacks its ID array'
        ids = re.findall(r'"(status\.[a-z0-9_-]+)"', array.group(1))
        predicates = re.search(r'bool now\[\]\s*=\s*\{(.*?)\};', block, re.S)
        assert predicates and len(top_level_expressions(predicates.group(1))) == len(ids), 'Extra status IDs/predicates are out of alignment'
        direct.update(ids)
    direct.update(re.findall(r'\btutorial_game_identified\([^;]+?,\s*"([a-z0-9_.-]+)"', '\n'.join(sources.values())))
    direct.update('status.' + name for name in re.findall(r'CONDITION\((\w+),', core) if name != 'FIELD')
    direct.update('status.' + name for name in ('hungry', 'weak', 'starving', 'voice', 'health'))
    direct.update('status.drain' + str(i) for i in range(4))
    # Nearby hazards use parallel public feature/lesson arrays so Normal mode
    # gets warnings even when detailed terrain cards are filtered out.
    nearby = re.search(r'static void observe_nearby\(void\)(.*?)(?=\nvoid tutorial_game_start)', core, re.S)
    if nearby:
        block = nearby.group(1)
        hazards = re.search(r'const int hazards\[\]\s*=\s*\{([^}]+)\}', block)
        ids = re.search(r'const char \*hazard_ids\[\]\s*=\s*\{([^}]+)\}', block)
        assert hazards and ids and 'observe(hazard_ids[i]' in block, 'Nearby hazard producer is missing'
        hazard_ids = re.findall(r'"(world\.[a-z]+)"', ids.group(1))
        assert len(top_level_expressions(hazards.group(1))) == len(hazard_ids), 'Hazard IDs/features are out of alignment'
        direct.update(hazard_ids)
    direct.update(('identification.item', 'item.artefact', 'item.first_description', 'item.description'))
    menu_ids = set(re.findall(r'\btutorial_game_menu\(\s*"([a-z0-9_-]+)"', '\n'.join(sources.values())))
    for expression in re.findall(r'\btutorial_game_menu\(\s*([^,]+),', combined):
        menu_ids.update(re.findall(r'"([a-z0-9_-]+)"', expression))
    # Knowledge pages route through a public indexed tab-id array.
    for text in sources.values():
        for array in re.findall(r'(?:tutorial|lesson)[\w_]*\s*\[[^]]*\]\s*=\s*\{([^}]+)\}', text):
            menu_ids.update(re.findall(r'"((?:knowledge-)[a-z-]+)"', array))
    direct.update('menu.' + name for name in menu_ids)
    return sources, direct


def validate_resource_routes():
    paths = (ROOT / 'src/init/init-paths.c').read_text(encoding='utf-8-sig')
    normal = paths.split('#else /* VM */', 1)[1]
    assert re.search(r'ANGBAND_DIR_HELP\s*=\s*path_build\(\s*buf,\s*sizeof\(buf\),\s*ANGBAND_DIR,\s*"help"\)', normal), 'Normal startup does not resolve the installed help directory'
    core = (ROOT / 'src/tutorial/tutorial.c').read_text(encoding='utf-8-sig')
    assert re.search(r'path_build\(path,\s*sizeof\(path\),\s*ANGBAND_DIR_HELP,\s*"tutorials.json"\)', core), 'Lazy tutorial loader no longer uses the installed help resource route'
    batch = (ROOT / 'build-cmake.bat').read_text(encoding='utf-8-sig')
    for folder in ('sil-more-windows-sdl3', 'sil-more-windows-sdl3-portable'):
        assert re.search(r'^xcopy\s+[^\r\n]*\blib\\help\s+' + re.escape(folder) + r'\\lib\\help\s*$', batch, re.M | re.I), 'Windows help staging missing: ' + folder
    cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8-sig')
    assert re.search(r'file\(GLOB\s+SIL_RES_HELP\b[^)]*"lib/help/\*\.json"', cmake), 'iOS help resources exclude the JSON catalogue'
    assert re.search(r'foreach\(_file\s+\$\{SIL_RES_HELP\}\)[\s\S]*?MACOSX_PACKAGE_LOCATION\s+"Resources/lib/help"', cmake), 'iOS help resources have no bundle destination'
    assert re.search(r'set\(SIL_ALL_RESOURCES\b[^)]*\$\{SIL_RES_HELP\}', cmake), 'iOS resource list omits help'
    assert 'target_sources(sil-more PRIVATE ${SIL_ALL_RESOURCES})' in cmake, 'iOS resource list is not attached to the executable'
    gradle = (ROOT / 'android/app/build.gradle').read_text(encoding='utf-8-sig')
    sync = re.search(r"tasks\.register\('syncGameAssets',\s*Sync\)\s*\{(.*?)^\}", gradle, re.S | re.M)
    assert sync, 'Android assets Sync task missing'
    block = sync.group(1)
    assert "new File(repoRoot, 'lib')" in block and 'from(gameLibDir)' in block
    assert "'generated/assets/lib'" in block and 'into(outDir)' in block
    assert 'assets.srcDirs' in gradle and '${buildDir}/generated/assets' in gradle
    assert re.search(r"tasks\.named\('preBuild'\)[\s\S]*?dependsOn\(tasks\.named\('syncGameAssets'\)\)", gradle)
    for line in block.splitlines():
        if not line.strip().startswith('exclude '):
            continue
        for pattern in re.findall(r"['\"]([^'\"]+)['\"]", line):
            original_pattern = pattern
            while True:
                assert not any(fnmatch.fnmatch(candidate, pattern) for candidate in ('help', 'help/tutorials.json')), 'Android excludes tutorial catalogue: ' + original_pattern
                if not pattern.startswith('**/'):
                    break
                pattern = pattern[3:]


def validate():
    validate_resource_routes()
    data = json.loads(CATALOGUE.read_text(encoding='utf-8'))
    assert data['version'] == 1
    lessons = data['lessons']
    assert 0 < len(lessons) <= 768
    by_id = {}
    for lesson in lessons:
        id = lesson['id']
        assert re.fullmatch('[a-z0-9_.-]+', id), id
        assert id not in by_id, f'duplicate {id}'
        by_id[id] = lesson
        assert lesson.get('level') in ('normal', 'extended'), f'{id}: assign Normal or Extended explicitly'
        assert type(lesson.get('priority')) is int, f'{id}: priority must be an integer'
        assert len(id.encode()) < 80 and len(lesson['title'].encode()) < 160, id
        assert 0 < len(lesson['steps']) <= 16, id
        assert lesson.get('sources') and lesson.get('trigger', {}).get('predicate'), id
        for path in lesson['sources'] + [lesson['trigger']['source']]:
            assert (ROOT / path).is_file(), f'{id}: missing source {path}'
        for step in lesson['steps']:
            assert step['kind'] in ('info', 'action', 'decision'), id
            assert 0 < len(step['text'].encode()) < 2048, id
            assert len(step.get('action', '').encode()) < 128, id
            assert len(step.get('anchor', '').encode()) < 48, id
            assert len(step.get('subject_type', '').encode()) < 48, id
            if step['kind'] == 'action':
                assert step.get('action'), id
                assert not id.startswith(('ability.', 'effect.', 'terrain.')), f'{id}: must not force a purchase, consumable or terrain hazard'
            if step['kind'] != 'action':
                assert not step.get('action'), f'{id}: decision explanation must not perform a purchase'
            placeholders = re.findall(r'\{([a-z_-]+)\}', step['text'])
            assert set(placeholders) <= {'subject', 'context', 'detail'}, (id, placeholders)
            expanded = step['text'].replace('{subject}', 'x' * 159)
            expanded = expanded.replace('{context}', 'x' * 383).replace('{detail}', 'x' * 383)
            assert len(expanded.encode()) < 2048, f'{id}: live context can truncate this card'
    abilities = records('lib/edit/ability.txt')
    expected_abilities = {f"ability.{a['serial']}.preview" for a in abilities}
    actual_abilities = {id for id in by_id if id.startswith('ability.')}
    assert actual_abilities == expected_abilities, (expected_abilities - actual_abilities, actual_abilities - expected_abilities)
    for ability in abilities:
        lesson = by_id[f"ability.{ability['serial']}.preview"]
        skill, slot, _ = map(int, ability['fields']['I'][0].split(':')[:3])
        assert lesson['ability']['skill'] == skill and lesson['ability']['slot'] == slot
        symbol = lesson['ability']['runtime_symbol']
        assert lesson['ability']['runtime_sources'], f"{lesson['id']}: no runtime source"
        for path in lesson['ability']['runtime_sources']:
            assert re.search(r'\b' + re.escape(symbol) + r'\b', (ROOT / path).read_text(encoding='utf-8-sig')), (lesson['id'], path)
        assert all(step['kind'] == 'decision' for step in lesson['steps'])
    effects = {f"effect.{obj['serial']}" for obj in records('lib/edit/object.txt')
               if obj['fields'].get('I') and int(obj['fields']['I'][0].split(':')[0]) in (55, 56, 66, 75, 80)}
    assert effects == {id for id in by_id if id.startswith('effect.')}, 'Known-effect kind coverage differs'
    expected_terrain = {f"terrain.{entry['serial']}" for entry in records('lib/edit/terrain.txt')
                        if entry['name'] != 'unused' and entry['serial'] not in (0, 1, 10, 11, 48, 56, 57, 58, 59)}
    assert expected_terrain == {id for id in by_id if id.startswith('terrain.')}, 'Terrain coverage differs'
    for id, lesson in by_id.items():
        if 'alias_of' not in lesson:
            continue
        target = lesson['alias_of']
        assert id.startswith('terrain.') and target in expected_terrain, f'{id}: invalid terrain representative'
        assert 'alias_of' not in by_id[target] and target != id, f'{id}: circular/chained terrain alias'
        assert lesson['level'] == by_id[target]['level'], f'{id}: alias has a different mode'
    opening = by_id['opening.move']['steps']
    assert [step['kind'] for step in opening] == ['info', 'info', 'action']
    assert opening[-1]['action'] == 'move'
    assert all(step['kind'] == 'info' for step in by_id['combat.first_monster']['steps']), 'Seeing a creature must not force stealth'
    assert by_id['combat.first_monster']['priority'] > by_id['combat.first_adjacent']['priority'], 'Explain the first creature before attack practice'
    assert by_id['combat.stealth']['steps'][-1]['action'] == 'stealth'
    for hazard in ('water', 'lava', 'ice', 'poison'):
        lesson = by_id['world.' + hazard]
        assert lesson['level'] == 'normal', f'{hazard}: basic terrain warnings must reach Normal players'
        assert all(step['kind'] == 'info' for step in lesson['steps']), f'{hazard}: a warning must not require entering the hazard'
    for hazard in ('lava', 'poison'):
        assert by_id['world.' + hazard]['priority'] > by_id['opening.move']['priority'], 'Urgent terrain danger must precede basic movement'
    level_rank = {'normal': 1, 'extended': 2}
    for item in ('staff', 'horn', 'bow', 'throwing'):
        chain = ['item.first_description', 'item.' + item, 'item.' + item + '.ready']
        if item in ('bow', 'throwing'):
            chain.append('item.' + item + '.active')
        chain.append('item.' + item + '.use')
        for prerequisite, next_lesson in zip(chain, chain[1:]):
            assert level_rank[by_id[prerequisite]['level']] <= level_rank[by_id[next_lesson]['level']], f'{next_lesson}: prerequisite {prerequisite} is suppressed at this level'
    for id in ('opening.move', 'item.first_description', 'combat.first_monster', 'combat.first_adjacent',
               'status.health', 'status.poisoned', 'status.hunger.remedy', 'world.skeleton', 'world.chest'):
        assert by_id[id]['level'] == 'normal', f'{id}: core controls/survival/feature introduction belongs in Normal'
    for lesson in lessons:
        if lesson['id'].startswith(('ability.', 'effect.', 'monster.', 'terrain.')):
            assert lesson['level'] == 'extended', lesson['id']
    for feature in ('skeleton', 'chest'):
        assert 'item.' + feature not in by_id, 'Feature must not use the generic equipment lesson chain'
        assert all(step['kind'] == 'info' and not step.get('action')
                   for step in by_id['world.' + feature]['steps']), feature
    for item in ('staff', 'horn'):
        assert all(step['kind'] == 'info' for step in by_id['item.' + item]['steps'])
        for suffix, action in (('.ready', 'ready'), ('.use', 'use-item')):
            required = [step for step in by_id['item.' + item + suffix]['steps'] if step['kind'] == 'action']
            assert len(required) == 1 and required[0]['action'] == action and required[0]['subject_type'] == item
    for id, lesson in by_id.items():
        if id.startswith('status.') and id.endswith('.remedy'):
            condition = id[len('status.'):-len('.remedy')]
            assert lesson['steps'][-1]['subject_type'] == condition, id
            assert lesson['steps'][-1]['action'] == 'use-item', id
    # Lock in known stale-template corrections: review against owning runtime.
    text = lambda id: ' '.join(s['text'] for s in by_id[id]['steps'])
    for serial, phrase in ((162, '+1 Grace'), (163, '+1 Dexterity'), (164, '+1 Constitution'), (166, '+5 Smithing'), (167, '+1 Strength'), (169, '+1 light radius')):
        assert phrase in text(f'ability.{serial}.preview'), serial
    assert 'Xd1' in text('ability.47.preview')
    assert '8 instead of 10' in text('ability.1.preview')
    assert 'shield prevents' in text('ability.8.preview')
    sources, direct = source_inventory()
    missing = sorted(direct - by_id.keys())
    assert not missing, 'Runtime producer lacks lesson: ' + ', '.join(missing)
    # Dynamic producer families are checked by their backing data and exact
    # format strings. Presence in the catalogue alone is not trigger evidence.
    core = sources['src/tutorial/tutorial-game.c']
    dynamic = set(expected_abilities | effects | expected_terrain)
    assert 'ability.%d.preview' in core and 'terrain.%d' in core and 'effect.%d' in core
    item_types = set(re.findall(r'"([a-z-]+)"', core[core.index('static const char *item_type('):core.index('static bool item_is_remedy')]))
    dynamic.update('item.' + type for type in item_types)
    # Remedy availability is explicitly guarded by item_is_remedy; not every
    # named condition has an available remedy or action trigger.
    conditions = {name for name in re.findall(r'CONDITION\((\w+),', core) if name != 'FIELD'}
    explicit_remedies = set(re.findall(r'\boffer_remedy\(\s*"([a-z0-9_-]+)"', core))
    assert 'offer_remedy(condition->id + 7' in core
    dynamic.update('status.' + condition + '.remedy' for condition in conditions | explicit_remedies if 'status.' + condition + '.remedy' in by_id)
    if 'item.%s.ready' in core: dynamic.update('item.' + type + '.ready' for type in ('staff', 'horn', 'bow', 'throwing'))
    if 'item.%s.active' in core: dynamic.update('item.' + type + '.active' for type in ('bow', 'throwing'))
    if 'item.%s.use' in core: dynamic.update('item.' + type + '.use' for type in ('staff', 'horn', 'bow', 'throwing'))
    world = sources.get('src/tutorial/tutorial-world.c', '')
    monster_producers = {'monster.rf' + group + '_' + flag.lower()
                         for group, flag in re.findall(r'\bMF\(([1-4]),\s*(\w+)\)', world)}
    assert not (monster_producers - by_id.keys()), 'Missing monster lore lessons: ' + ', '.join(sorted(monster_producers - by_id.keys()))
    dynamic.update(monster_producers)
    if 'quest.%d' in world:
        dynamic.update('quest.' + str(i) for i in range(1, 7))
    if 'world.partition.%d' in world:
        dynamic.update('world.partition.' + str(i) for i in range(1, 7))
    if 'world.partition.%s' in world:
        dynamic.update('world.partition.' + element for element in ('fire', 'cold', 'poison') if '"' + element + '"' in world)
    unwired = sorted(set(by_id) - direct - dynamic)
    counts = collections.Counter(id.split('.')[0] for id in by_id)
    return lessons, counts, unwired, sources


def write_reference(lessons, counts, unwired):
    levels = collections.Counter(lesson['level'] for lesson in lessons)
    lines = ['# Gameplay tutorial reference', '',
             'This continuous document contains the authored lessons from `lib/help/tutorials.json`. '
             'The UI owns wrapping and pagination. `{subject}`, `{detail}` and `{context}` remain runtime placeholders; '
             'they contain only information already available to the hero.', '',
             'Info and decision explanations use Continue. Required action steps complete only after the matching '
             'real action commits. Reading, skipping and reviewing are free; game actions retain their normal '
             'costs and consequences. The archive turns every step into a read-only explanation.', '',
             f"The catalogue contains {len(lessons)} lessons, including {counts['ability']} ability previews. "
             'Every live ability serial, item kind handled by the aware-effect producer and meaningful public terrain serial '
             'has a checked entry. Equivalent terrain variants share an automatic lesson; their old entries remain for '
             'saved archive history. This checks source/data coverage, not physical-device interaction.', '']
    lines += ['## Resource route', '',
              '`src/init/init-paths.c` resolves `ANGBAND_DIR_HELP` from the installed data root. '
              '`src/tutorial/tutorial.c` lazily reads `tutorials.json` from that directory. '
              '`build-cmake.bat` stages Help for both Windows deployments; `CMakeLists.txt` '
              'includes Help JSON in iOS resources; `android/app/build.gradle` synchronizes '
              'the game library into Android assets before building. The validator checks '
              'these source routes; this is not confirmation of a device installation.', '']
    lines += ['## Tutorial modes', '',
              f"Default: **Extended**. The catalogue has **{levels['normal']} Normal** lessons and "
              f"**{levels['extended']} Extended** lessons. The card's single mode button cycles "
              '**Disabled → Normal → Extended → Disabled**.', '',
              'Normal covers core controls, survival, general item handling and its complete action chains, '
              'storage, main menus, combat fundamentals and Tale events. Extended includes all Normal lessons '
              'and adds individual abilities and item effects, learned monster traits, terrain and region details, '
              'individual quest introductions and specialist status or knowledge pages.', '',
              'Filtering a lesson never marks it learned or skipped. Lowering the mode withdraws an Extended '
              'card and pending Extended observations while keeping completed steps in the Tale history. '
              'The learned archive remains read-only and can show encountered lessons from either level, '
              'including when gameplay tutorials are Disabled.', '',
              'Characters deferred during an upgrade remain exempt from automatic tutorials for that character. '
              'Changing mode or resetting the Tale lesson history does not bypass that gate. The upgrade notice '
              'belongs to its own native UI and is independent of catalogue filtering.', '',
              'Skeletons and chests use Normal informational feature lessons. Their old generic item IDs remain '
              'untouched if present in saved history, and they do not enter examine/equip/use tutorial chains.', '']
    lines += ['## Presentation order', '',
              'Cards are selected by descending priority at a safe input boundary, not by their position in this document. '
              'Equal priorities keep observation order. A higher-priority card can interrupt between steps; the earlier '
              'lesson resumes at its saved step when its context is still relevant. Menu and purchase explanations '
              'take focus at their owning input boundary so they precede the choice they describe. Reading does not advance game time.', '',
              'Gameplay observations are checked again against current conditions, reachable items and visible subjects. '
              'Expired observations are withdrawn without marking them completed or skipped; a fresh encounter can offer '
              'them again. Required actions complete only after the matching real action succeeds or commits.', '']
    if unwired:
        lines += ['## Trigger audit', '', 'These authored entries do not currently have a detected direct or data-backed producer. '
                  'They are coverage gaps, not completed runtime coverage:', '', ', '.join('`'+id+'`' for id in unwired), '']
    for lesson in lessons:
        lines += ['## ' + lesson['title'], '', '`' + lesson['id'] + '`', '']
        lines += ['Level: **' + lesson['level'].capitalize() + '**.', '']
        lines += [f"Priority: **{lesson['priority']}** (higher appears first).", '']
        if lesson.get('alias_of'):
            lines += [f"Archive compatibility entry. New encounters use `{lesson['alias_of']}` for this terrain family.", '']
        for number, step in enumerate(lesson['steps'], 1):
            lines += [f"**{number}. {step['kind'].capitalize()}**", '', step['text'], '']
            if step['kind'] == 'action':
                subject = step.get('subject_type')
                lines += [f"Required action: `{step['action']}`" + (f"; subject: `{subject}`." if subject else '.'), '']
        lines += ['Trigger: ' + lesson['trigger']['predicate'], '', 'Sources: ' + ', '.join('`'+source+'`' for source in lesson['sources']) + '.', '']
    REFERENCE.write_text('\n'.join(lines), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-reference', action='store_true')
    parser.add_argument('--strict-triggers', action='store_true', help='Fail if any authored entry lacks producer evidence.')
    args = parser.parse_args()
    lessons, counts, unwired, _ = validate()
    if args.write_reference:
        write_reference(lessons, counts, unwired)
    levels = collections.Counter(lesson['level'] for lesson in lessons)
    print(f"Tutorial catalogue: PASS; {levels['normal']} Normal, {levels['extended']} Extended; " + ', '.join(f'{count} {name}' for name, count in sorted(counts.items())))
    if unwired:
        print('Trigger gaps (authored but no detected producer): ' + ', '.join(unwired))
        if args.strict_triggers:
            raise SystemExit(1)


if __name__ == '__main__':
    main()
