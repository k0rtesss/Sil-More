#!/usr/bin/env python3
"""Play compound skills/song/equipment/terrain workflows through dummy SDL.

Scene preparation is deterministic; every tested action uses GameControl.
Every observation asserts that the scene contains zero monsters. No AI tests.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shlex

import check_player_scenarios_gameplay as expanded

base = expanded.base
ROOT = base.ROOT
OUT = ROOT / "scripts/output/player-combinations"
original_fixture = expanded.fixture_source
OriginalScene = expanded.Scene

OBSERVER = r'''
static void observe_combinations(cJSON* root)
{
#define C(key,value) cJSON_AddNumberToObject(root,key,value)
    C("monster_count",mon_cnt);C("free_action",p_ptr->free_act);
    C("pending_stealth_stop",stop_stealth_mode);
    C("position_y",p_ptr->py);C("position_x",p_ptr->px);
    C("fire_stacks",p_ptr->resist_fire);C("cold_stacks",p_ptr->resist_cold);
    C("poison_stacks",p_ptr->resist_pois);C("regen",p_ptr->regenerate);
    C("sprinting",sprinting());C("light_armour",wearing_only_light_armour());
    C("song_main_effective",p_ptr->song1<SNG_MAX?song_effective_skill(p_ptr->song1):0);
    C("song_minor_effective",p_ptr->song2<SNG_MAX?song_effective_skill(p_ptr->song2):0);
    C("song_main_bonus",p_ptr->song1<SNG_MAX?ability_bonus(S_SNG,p_ptr->song1):0);
    C("song_minor_bonus",p_ptr->song2<SNG_MAX?ability_bonus(S_SNG,p_ptr->song2):0);
    C("glowing",(cave_info[p_ptr->py][p_ptr->px]&CAVE_GLOW)!=0);
    const s16b* arrays[]={p_ptr->skill_equip_mod,p_ptr->skill_misc_mod};
    const char* names[]={"skill_equipment","skill_misc"};
    for(int i=0;i<2;i++){
        cJSON* row=cJSON_AddArrayToObject(root,names[i]);
        for(int j=0;j<S_MAX;j++)cJSON_AddItemToArray(row,cJSON_CreateNumber(arrays[i][j]));
    }
    const char* anames[]={"active_abilities","owned_abilities","innate_abilities"};
    for(int type=0;type<3;type++){
        cJSON* rows=cJSON_AddArrayToObject(root,anames[type]);
        for(int skill=0;skill<S_MAX;skill++){
            cJSON* row=cJSON_CreateArray();cJSON_AddItemToArray(rows,row);
            for(int a=0;a<ABILITIES_MAX;a++){
                bool value=type==0?p_ptr->active_ability[skill][a]:
                    (type==1?p_ptr->have_ability[skill][a]:p_ptr->innate_ability[skill][a]);
                cJSON_AddItemToArray(row,cJSON_CreateBool(value));
            }
        }
    }
    cJSON* history=cJSON_AddArrayToObject(root,"action_history");
    for(int i=0;i<ACTION_MAX;i++)cJSON_AddItemToArray(history,cJSON_CreateNumber(p_ptr->previous_action[i]));
    if(g_song_menu.active){
        cJSON* menu=cJSON_AddObjectToObject(root,"song_menu");
        cJSON_AddStringToObject(menu,"title",g_song_menu.title);
        cJSON* choices=cJSON_AddArrayToObject(menu,"choices");
        for(int i=0;i<g_song_menu.count;i++){
            const sdl_song_menu_entry_state* entry=&g_song_menu.entries[i];
            cJSON* row=cJSON_CreateObject();cJSON_AddItemToArray(choices,row);
            cJSON_AddStringToObject(row,"key",entry->letter);
            cJSON_AddStringToObject(row,"label",entry->text);
        }
    }
#undef C
}
'''

ITEM_SETUP = r'''
        const cJSON* granted=cJSON_GetObjectItemCaseSensitive(entry,"grants");
        if(cJSON_IsArray(granted)){
            o.abilities=cJSON_GetArraySize(granted);assert(o.abilities<=N_ELEMENTS(o.skilltype));
            for(int g=0;g<o.abilities;g++){
                const cJSON* grant=cJSON_GetArrayItem(granted,g);
                int skill=number(grant,"skill",S_SNG),a=number(grant,"ability",SNG_TREES);
                assert(skill>=0&&skill<S_MAX&&a>=0&&a<ABILITIES_MAX);
                o.skilltype[g]=skill;o.abilitynum[g]=a;
                p_ptr->active_ability[skill][a]=true;
            }
        }
'''


def replace_once(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new, 1)


def fixture_source():
    source = original_fixture()
    source = replace_once(source, "static int work_pause_after", OBSERVER + "\nstatic int work_pause_after")
    source = replace_once(source, "#undef N\n}", "#undef N\n    observe_combinations(root);\n}")
    source = replace_once(source, '        if(number(entry,"ego",0))', ITEM_SETUP + '        if(number(entry,"ego",0))')
    source = replace_once(source, '    cJSON_AddNumberToObject(row,"broken",broken_p(o));',
        '    cJSON_AddNumberToObject(row,"broken",broken_p(o));\n'
        '    cJSON_AddNumberToObject(row,"weight",o->weight);\n'
        '    cJSON* grants=cJSON_AddArrayToObject(row,"grants");\n'
        '    for(int i=0;i<o->abilities;i++){cJSON* a=cJSON_CreateArray();cJSON_AddItemToArray(grants,a);\n'
        '        cJSON_AddItemToArray(a,cJSON_CreateNumber(o->skilltype[i]));\n'
        '        cJSON_AddItemToArray(a,cJSON_CreateNumber(o->abilitynum[i]));}\n')
    source = replace_once(source, '    cave_m_idx[p_ptr->py][p_ptr->px]=-1;',
        '    if(number(spec,"dark",0))for(int y=0;y<24;y++)for(int x=0;x<40;x++)cave_info[y][x]&=~CAVE_GLOW;\n'
        '    cave_m_idx[p_ptr->py][p_ptr->px]=-1;')
    return source


SONGS = {2: "Delvings", 3: "Freedom", 4: "Silence", 5: "Staunching",
         6: "Thresholds", 7: "Trees", 11: "Elveness", 12: "Staying"}
FREE_FIELDS = ("turn", "player_turn", "hp", "voice", "food", "song", "song2",
               "song_duration", "light", "rng", "items", "stats_use", "skills_use",
               "active_abilities", "owned_abilities", "y", "x", "poison", "cut", "pending_stealth_stop")


class Scene(OriginalScene):
    def brief(self):
        return {k: v for k, v in super().brief().items()
                if k not in ("active_abilities", "owned_abilities", "innate_abilities")}

    @property
    def question(self):
        return self.p.get("song_menu", {}) or super().question

    def press(self, *keys):
        result = super().press(*keys)
        assert self.p["monster_count"] == 0, "Monster AI must remain outside this campaign"
        return result

    def free_state(self):
        return {k: copy.deepcopy(self.p[k]) for k in FREE_FIELDS}

    def label(self, fragment):
        rows = []
        for line in self.terminal.splitlines():
            match = re.search(r"(?:^|\|)\s*([a-zA-Z0-9-])\)\s", line)
            if match and fragment.lower() in line.lower(): rows.append(match.group(1))
        assert len(rows) == 1, (fragment, rows, self.terminal)
        return rows[0]

    def cancel(self, *keys):
        previous = self.free_state()
        self.press(*keys, "Escape")
        assert self.free_state() == previous, ("Cancelled input changed gameplay", keys, self.brief())

    def sing(self, song):
        before = self.p["player_turn"]
        self.press("s")
        self.choose("Stop Singing" if song == 100 else SONGS[song])
        self.close_views()
        assert self.p["player_turn"] == before + (song != 100), (song, self.brief())

    def swap(self):
        before = self.p["player_turn"]
        self.press("s"); self.choose("Exchange themes")
        assert self.p["player_turn"] == before + 1

    def stable(self):
        assert self.p["monster_count"] == 0 and not self.p["dead"], self.brief()
        self.limits()

    def checkpoint(self):
        before = self.free_state()
        path = super().checkpoint()
        assert self.free_state() == before, ("Saving changed gameplay or queued a stance change", self.brief())
        return path


class Campaign(expanded.Campaign):
    def __init__(self, args):
        expanded.fixture_source = fixture_source
        expanded.Scene = Scene
        super().__init__(args)
        self.receipt.update(scope="Compound player skills, songs, equipment and terrain; zero monsters",
            combination_driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
        (self.exe.parent / "combination-driver.py").write_bytes(Path(__file__).read_bytes())
        objects = shlex.split((base.BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
        manifest = {obj: hashlib.sha256((base.BUILD/obj).read_bytes()).hexdigest() for obj in objects}
        expanded.write_report(self.exe.parent/"objects-manifest.json", manifest)
        self.receipt["objects_manifest"] = str(self.exe.parent/"objects-manifest.json")
        self.save()


def ability(skill, number):
    return dict(skill=skill, ability=number)


def abilities(*songs, heavy=True, leap=False, sprint=False):
    result = [ability(7, s) for s in (*songs, 9)]
    result += [ability(5, 4), ability(5, 7)]  # Inner Light and Hardiness.
    if heavy: result.append(ability(2, 7))
    if leap: result.append(ability(2, 4))
    if sprint: result.append(ability(2, 5))
    return result


def armour(heavy, grace=True):
    items = [dict(kind=123 if heavy else 122, slot=36), dict(kind=156, slot=27)]
    if heavy: items.append(dict(kind=30, slot=31))
    if grace: items.append(dict(kind=134, slot=29))
    return items


def effective(song, main, minor, full, hero=0):
    value = full if song == main or hero == 34 else full // 2
    if 4 in (main, minor) and song not in (4, 13, 14): value = max(0, value - full // 4)
    if {main, minor} == {3, 11}: value += (full + 5) // 10
    return value


def check_themes(s):
    p = s.p
    if p["song"] == 100: assert p["song2"] == 100; return
    for slot, song in (("main", p["song"]), ("minor", p["song2"])):
        if song == 100: continue
        expected = effective(song, p["song"], p["song2"], p["skills_use"][7], p["hero"])
        assert p[f"song_{slot}_effective"] == expected, (slot, expected, s.brief())
    if 7 in (p["song"], p["song2"]):
        score = effective(7, p["song"], p["song2"], p["skills_use"][7], p["hero"])
        assert p["light"] > s.initial["light"] and score > 0, s.brief()
    if 12 in (p["song"], p["song2"]):
        score = effective(12, p["song"], p["song2"], p["skills_use"][7], p["hero"])
        expected = s.initial["skills_use"][5] + p["stats_use"][3] - s.initial["stats_use"][3] + score // 2
        assert p["skills_use"][5] == expected, ("Staying Will", expected, s.brief())


def woven_expeditions(c):
    pairs = [(7, 12), (5, 7), (7, 11), (3, 11), (4, 7), (4, 12)]
    for main, minor in pairs:
        for heavy in (False, True):
            for song_skill in (12, 30):
                spec = dict(stats=7, skill_values=[20]*7+[song_skill, 0],
                    abilities=abilities(main, minor), items=armour(heavy),
                    tiles=[dict(y=12, x=13, feat=86), dict(y=12, x=14, feat=84), dict(y=12, x=15, feat=100)],
                    hp=35)

                def check(s, main=main, minor=minor):
                    s.cancel("s")
                    s.sing(main); s.sing(minor)
                    assert (s.p["song"], s.p["song2"]) == (main, minor)
                    check_themes(s)
                    s.cancel("s"); s.cancel("i")
                    song_evasion = s.p["skills_use"][2]
                    s.press("Right")
                    assert s.p["feat"] == 86 and s.p["skills_use"][2] == song_evasion-2, s.brief()
                    s.swap()
                    assert (s.p["song"], s.p["song2"]) == (minor, main)
                    check_themes(s)
                    for feat in (84, 100):
                        before = s.p["player_turn"]; voice = s.p["voice"]
                        s.press("Right")
                        assert s.p["feat"] == feat and s.p["player_turn"] == before+1, s.brief()
                        assert s.p["voice"] <= voice
                        check_themes(s); s.stable()
                    s.press("Right")
                    assert s.p["feat"] == 1
                    # Choosing the minor theme again stops just that theme for free.
                    before = s.p["player_turn"]
                    s.press("s"); s.choose(SONGS[main])
                    assert s.p["song"] == minor and s.p["song2"] == 100
                    assert s.p["player_turn"] == before
                    s.sing(100)
                    assert s.p["light"] == s.initial["light"]
                    assert s.p["skills_use"][5] == s.initial["skills_use"][5]
                    assert s.p["skills_use"][2] == s.initial["skills_use"][2]
                    s.stable()
                for hero in ((0, 34) if (main, minor) in ((7, 12), (4, 7)) else (0,)):
                    c.variant(f"woven-{main}-{minor}-{'heavy' if heavy else 'light'}-song{song_skill}-hero{hero}",
                              dict(spec, hero=hero), check, "woven_expedition")


def freedom_expeditions(c):
    for direction, key, dy, dx in (("east", "Right", 0, 1), ("north", "Up", -1, 0)):
        for partner in (7, 11):
            for heavy in (False, True):
                def tile(a, b, feat): return dict(y=12+a, x=12+b, feat=feat)
                spec = dict(stats=7, skills=40, abilities=abilities(3, partner), items=armour(heavy),
                    tiles=[tile(dy, dx, 39), tile(2*dy, 2*dx, 86), tile(0, -1, 32), tile(0, -2, 19),
                           tile(2, 0, 49), tile(2, 2, 2)],
                    hp=40)
                spec["items"].append(dict(kind=372, floor=1, y=13, x=13, pval=3))

                def check(s, partner=partner, key=key, dy=dy, dx=dx):
                    control_spec = copy.deepcopy(json.loads((s.profile/"scenario.json").read_text()))
                    control = Scene(base.start("without-freedom", control_spec, c.exe, c.env)); s.children.append(control)
                    control.sing(partner); control.press("z")
                    assert control.p["tiles"][12+dy][12+dx] == 39
                    assert control.p["tiles"][12][10] == 19 and control.p["tiles"][14][12] == 49
                    assert control.p["tiles"][14][14] == 2
                    assert next(o for o in control.p["items"] if o["kind"] == 372)["pval"] == 3
                    s.sing(3); s.sing(partner)
                    for _ in range(6):
                        if s.p["tiles"][12+dy][12+dx] == 32 and s.p["tiles"][12][10] == 1 and s.p["tiles"][14][12] == 1: break
                        s.press("z")
                    assert s.p["tiles"][12+dy][12+dx] == 32, s.brief()
                    assert s.p["tiles"][12][10] == 1 and s.p["tiles"][14][12] == 1
                    assert s.p["tiles"][14][14] == 1
                    assert next(o for o in s.p["items"] if o["kind"] == 372)["pval"] < 0
                    assert s.p["free_action"] >= 1
                    s.cancel("o")
                    s.press("o", "PageDown")
                    s.choose("Open:")
                    assert all(o["pval"] == 0 for o in s.p["items"] if o["kind"] == 372)
                    s.press("o", key)
                    assert s.p["tiles"][12+dy][12+dx] == 4
                    s.press(key, key)
                    assert (s.p["y"], s.p["x"]) == (12+2*dy, 12+2*dx)
                    assert s.p["feat"] == 86
                    check_themes(s); s.sing(100)
                    assert s.p["free_action"] == s.initial["free_action"]
                    s.stable()
                c.variant(f"freedom-{direction}-partner{partner}-{'heavy' if heavy else 'light'}", spec, check, "freedom_expedition")


def leaping_expeditions(c):
    for feature in (2, 84, 85, 87, 19):
        for heavy in (False, True):
            spec = dict(stats=9, skills=30, x=10, abilities=abilities(7, 12, leap=True, sprint=True),
                items=armour(heavy), tiles=[dict(y=12, x=12, feat=feature)], voice=80)

            def check(s, feature=feature):
                s.sing(7); s.sing(12)
                s.press("Right")
                assert s.p["x"] == 11
                previous = s.free_state()
                before_turn = s.p["player_turn"]
                s.press("Right")
                if feature in (84, 85, 87):
                    assert s.question.get("title") == "Choose how to cross", s.question
                    s.press("Escape")
                    assert s.free_state() == previous
                    s.press("Right"); s.choose("Jump over")
                assert not s.p["leaping"] and s.p["x"] == 13 and s.p["depth"] == s.initial["depth"], s.brief()
                assert s.p["player_turn"] == before_turn+2
                airborne = [t for t in s.turns() if t["leaping"] and t["position_x"] == 12]
                assert airborne and all(t["poison"] == 0 for t in airborne)
                assert s.p["poison"] == 0 and s.p["tiles"][12][12] == feature
                assert [(o["kind"], o["number"]) for o in s.p["items"]] == [(o["kind"], o["number"]) for o in s.initial["items"]]
                check_themes(s); s.stable()
                checkpoint = s.checkpoint()
                branch = Scene(base.start("loaded-airborne-crossing", dict(load_from=str(checkpoint)), c.exe, c.env))
                s.children.append(branch)
                assert branch.p == s.p, "Post-crossing full save changed state"
                for _ in range(3): s.press("Right"); branch.press("Right")
                assert branch.p == s.p, "Saved singing/sprinting continuation diverged"
            c.variant(f"leap-{feature}-{'heavy' if heavy else 'light'}", spec, check, "leaping_expedition")


def loaded_scene(c, parent, name, checkpoint):
    branch = Scene(base.start(name, dict(load_from=str(checkpoint)), c.exe, c.env))
    parent.children.append(branch)
    assert branch.p == parent.p, ("Full save changed compound player state", name,
        {k: (parent.p[k], branch.p.get(k)) for k in parent.p if parent.p[k] != branch.p.get(k)})
    return branch


def restore_voice(s):
    previous = s.free_state()
    s.press("q"); label = s.label("Voice"); s.press(label, "n"); s.close_views()
    assert s.free_state() == previous
    before = s.p["player_turn"]
    s.press("q"); s.press(s.label("Voice"), "y")
    assert s.p["player_turn"] == before+1 and s.total(319) == 1
    assert s.p["voice"] > previous["voice"], s.brief()


def crafting_expeditions(c):
    for craft in ("smith", "fletch"):
        for partner in (7, 12):
            for low_voice in (False, True):
                spec = dict(stats=7, skills=25, hp=20, cut=12, voice=8 if low_voice else 80,
                    pause_work_after=3, abilities=abilities(5, partner) +
                    [ability(6, 0) if craft == "smith" else ability(1, 1)],
                    items=armour(True) + [dict(kind=319, number=2)] +
                    ([] if craft == "smith" else [dict(kind=116, number=10, quiver=1)]),
                    x=11, tiles=[dict(y=12, x=11, feat=86), dict(y=12, x=12, feat=67)])

                def check(s, craft=craft, low_voice=low_voice, partner=partner):
                    s.sing(5); s.sing(partner)
                    assert s.p["cut"] == 0 and s.p["hp"] > s.initial["hp"]
                    s.press("Right")
                    assert s.p["feat"] == 67
                    assert s.question.get("title") == "A Forge of the Enemy"
                    s.choose("Enter and use the forge")
                    s.press("Escape")
                    before_work_light = s.p["light"]
                    before_work_fuel = next(o["timeout"] for o in s.p["items"] if o["kind"] == 128)
                    if craft == "smith": s.press("0", "a", "F")
                    else: s.press("-", "a")
                    if s.p["fixture_work_pause"]:
                        if craft == "smith" and not low_voice:
                            assert s.p["light"] == before_work_light+1
                            assert next(o["timeout"] for o in s.p["items"] if o["kind"] == 128) == before_work_fuel
                        s.press("Escape")
                    else:
                        assert low_voice and s.p["song"] == 100 and not s.p["smithing"] and not s.p["fletching"], s.brief()
                    remaining = s.p["smithing_leftover"] if craft == "smith" else 10-sum(
                        o["number"] for o in s.p["items"] if o["kind"] == 116 and o["attack"] == 3)
                    assert 0 < remaining < 10
                    assert not s.p["smithing"] and not s.p["fletching"] and s.p["cut"] == 0
                    if craft == "smith" and not low_voice: assert s.p["light"] == before_work_light
                    assert s.p["hp"] > s.initial["hp"]
                    if low_voice: assert s.p["voice"] <= 2
                    checkpoint = s.checkpoint()
                    branches = [loaded_scene(c, s, "loaded-singing-work", checkpoint) for _ in range(2)]
                    for branch in branches:
                        work_left = remaining
                        # A small reserve may survive the first interruption.
                        # Spend it on real resumed work before restoring Voice.
                        if low_voice and branch.p["song"] != 100:
                            if craft == "smith": branch.press("0", "Enter")
                            else: branch.press("-", "a")
                            assert branch.p["song"] == 100 and branch.p["song2"] == 100
                            assert not branch.p["smithing"] and not branch.p["fletching"]
                            work_left = branch.p["smithing_leftover"] if craft == "smith" else 10-sum(
                                o["number"] for o in branch.p["items"] if o["kind"] == 116 and o["attack"] == 3)
                            assert 0 < work_left < remaining
                        restore_voice(branch)
                        if craft == "smith":
                            before = branch.p["player_turn"]
                            branch.press("0", "Enter")
                            assert branch.p["player_turn"] == before+work_left
                            assert branch.p["feat"] == 66 and branch.total(56) == 5
                            assert branch.p["smithing_leftover"] == 0
                        else:
                            branch.press("-", "a")
                            assert branch.total(116) == 10
                            assert all(o["attack"] == 3 for o in branch.p["items"] if o["kind"] == 116)
                        if low_voice:
                            assert branch.p["song"] == 100 and branch.p["song2"] == 100
                        else:
                            assert branch.p["song"] == 5
                        branch.stable()
                    assert branches[0].p == branches[1].p, "Loaded singing/crafting continuations diverged"
                c.variant(f"craft-{craft}-partner{partner}-{'low' if low_voice else 'full'}-voice",
                          spec, check, "crafting_expedition")


def gear_source_expeditions(c):
    for learned_main in (False, True):
        for learned_minor in (False, True):
            for feature in (86, 100):
                learned = [ability(2, 7), ability(5, 4), ability(5, 7)]
                grants = [ability(7, 9)]
                for song, known in ((7, learned_main), (12, learned_minor)):
                    (learned if known else grants).append(ability(7, song))
                spec = dict(stats=7, skills=25, abilities=learned,
                    items=armour(True, grace=False)+[dict(kind=134, slot=29, grants=grants, note="Weaving source")],
                    tiles=[dict(y=12, x=13, feat=feature)])

                def check(s, learned_main=learned_main, learned_minor=learned_minor):
                    s.sing(7); s.sing(12); s.press("Right")
                    assert s.p["song2"] == 12
                    previous = s.free_state()
                    s.press("e"); key = s.label("Amulet of Grace"); s.press(key, "n"); s.close_views()
                    assert s.free_state() == previous
                    before = s.p["player_turn"]
                    s.press("e"); s.press(s.label("Amulet of Grace"), "y"); s.close_views()
                    assert s.p["player_turn"] == before+1
                    assert s.p["song"] == (7 if learned_main else 100) and s.p["song2"] == 100
                    assert not s.p["owned_abilities"][7][9] and not s.p["active_abilities"][7][9]
                    assert s.p["owned_abilities"][7][7] == learned_main
                    assert s.p["owned_abilities"][7][12] == learned_minor
                    assert s.total(134, "pack") == 1
                    assert s.p["stats_use"][3] == s.initial["stats_use"][3]-1
                    checkpoint = s.checkpoint()
                    branch = loaded_scene(c, s, "loaded-missing-song-source", checkpoint)
                    for actor in (s, branch):
                        actor.press("w"); actor.press(actor.label("Amulet of Grace"), "y"); actor.close_views()
                        assert actor.p["owned_abilities"][7][9]
                        assert actor.p["active_abilities"][7][9], "Equipped ability source did not reactivate"
                        assert actor.p["song"] == (7 if learned_main else 100) and actor.p["song2"] == 100
                        actor.press("Left"); actor.stable()
                    assert s.p == branch.p, "Restored gear/source continuation diverged"
                c.variant(f"gear-source-main{int(learned_main)}-minor{int(learned_minor)}-terrain{feature}",
                          spec, check, "gear_source_expedition")


def sprinting_expeditions(c):
    for heavy in (False, True):
        for partner in (4, 11):
            for sneaking in (False, True):
                spec = dict(stats=7, skills=25, abilities=abilities(7, partner, sprint=True),
                    items=armour(heavy), tiles=[dict(y=12, x=13, feat=86), dict(y=12, x=15, feat=84),
                        dict(y=12, x=16, feat=100)], dark=1)

                def check(s, partner=partner, sneaking=sneaking, heavy=heavy):
                    controls = copy.deepcopy(json.loads((s.profile/"scenario.json").read_text()))
                    controls["abilities"] = [a for a in controls["abilities"] if a != ability(2, 5)]
                    control = Scene(base.start("without-sprinting", controls, c.exe, c.env)); s.children.append(control)
                    starts = []
                    for actor in (s, control):
                        actor.sing(7); actor.sing(partner)
                        if sneaking:
                            before = actor.p["player_turn"]; old_stealth = actor.p["skills_use"][3]
                            actor.press("S")
                            assert actor.p["stealth"] and actor.p["player_turn"] == before
                            assert actor.p["skills_use"][3] > old_stealth
                        starts.append(actor.p["turn"])
                    speeds = []
                    for _ in range(8):
                        s.press("Right"); control.press("Right")
                        assert (s.p["x"], s.p["y"]) == (control.p["x"], control.p["y"])
                        assert s.p["player_turn"] == control.p["player_turn"]
                        assert s.p["voice"] == control.p["voice"]
                        speeds.append(s.p["sprinting"])
                        check_themes(s); check_themes(control); s.stable(); control.stable()
                    assert any(speeds) and not control.p["sprinting"]
                    first = speeds.index(1)+1
                    assert first == (4 if heavy else 3), ("Armour-dependent sprint run-up", first, speeds)
                    assert s.p["turn"]-starts[0] < control.p["turn"]-starts[1], "Sprinting did not shorten the mixed-terrain route"
                    checkpoint = s.checkpoint()
                    branch = loaded_scene(c, s, "loaded-singing-sprint", checkpoint)
                    for _ in range(3): s.press("Right"); branch.press("Right")
                    assert s.p == branch.p
                c.variant(f"sprint-{'heavy' if heavy else 'light'}-partner{partner}-{'sneak' if sneaking else 'walk'}",
                          spec, check, "sprinting_expedition")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUT)
    parser.add_argument("--seeds", nargs="+", type=int, default=[1701, 7307, 9919])
    parser.add_argument("--only", nargs="+")
    parser.add_argument("--groups", nargs="+")
    parser.add_argument("--max-failures", type=int, default=6)
    parser.add_argument("--workers", type=int, choices=(1, 2, 3, 4), default=3)
    parser.add_argument("--resume", nargs="+", type=Path)
    args = parser.parse_args()
    campaign = Campaign(args)
    woven_expeditions(campaign)
    freedom_expeditions(campaign)
    leaping_expeditions(campaign)
    crafting_expeditions(campaign)
    gear_source_expeditions(campaign)
    sprinting_expeditions(campaign)
    print(f"Selected {len(campaign.jobs)} compound scenarios; zero monsters; dummy SDL video/audio", flush=True)
    campaign.finish()
    assert campaign.results
    campaign.receipt["finished_at"] = datetime.now(timezone.utc).isoformat(); campaign.save()
    return int(bool(campaign.receipt["failed"]))


if __name__ == "__main__":
    raise SystemExit(main())
