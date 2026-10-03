#!/usr/bin/env python3
"""Execute tutorial core/formatter with all current public ability policies.

No engine objects, player saves, preferences or game window are used. Metadata
fixtures come from the authored stable/overlay records; parser validation has
its own engine harness. Live, archive and replay run production tutorial code.
"""
from pathlib import Path
import json
import os
import re
import subprocess
import tempfile
from check_gameplay_tutorial import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/insight-tutorials"


def function(source, name):
    start = re.search(r"^[^\n]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M).start()
    return source[start:source.index("\n}", start) + 2] + "\n"


def records(path):
    entries = {}
    current = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("N:"):
            _, serial, name = line.split(":", 2)
            current = entries[int(serial)] = {"name": name}
        elif current is not None and len(line) > 1 and line[1] == ":":
            current.setdefault(line[0], []).append(line[2:].split("#", 1)[0].strip())
    return entries


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    base = records(ROOT / "lib/edit/ability.txt")
    overlay = records(ROOT / "lib/edit/ability-insight.txt")
    names = texts = "\0"
    initializers = []
    for serial, entry in base.items():
        policy = overlay[serial]
        name = len(names.encode("utf-8")); names += entry["name"] + "\0"
        new_name = len(names.encode("utf-8")); names += policy.get("J", [entry["name"]])[0] + "\0"
        effect = len(texts.encode("utf-8")); texts += " ".join(entry.get("E", [""])) + "\0"
        new_effect = len(texts.encode("utf-8")); texts += " ".join(policy.get("F", entry.get("E", [""]))) + "\0"
        skill, local, _ = map(int, entry["I"][0].split(":"))
        kind, cost, learning, rank = map(int, policy["X"][0].split(":"))
        fields = [f".name={name}", f".policy_name={new_name}", f".effect={effect}", f".policy_effect={new_effect}",
                  f".skilltype={skill}", f".abilitynum={local}", f".policy_kind={kind}",
                  f".policy_cost={cost}", f".policy_skill={learning}", f".policy_level={rank}"]
        for letter, prefix in (("O", "or"), ("H", "and")):
            parents = [tuple(map(int, pair.split("/"))) for value in policy.get(letter, []) for pair in value.split(":")]
            fields.extend([f".policy_{prefix}_count={len(parents)}",
                           f".policy_{prefix}_skill={{{','.join(str(p[0]) for p in parents)}}}",
                           f".policy_{prefix}_ability={{{','.join(str(p[1]) for p in parents)}}}"])
        ranks = [0] * 9
        for requirement in policy.get("G", []):
            s, r = map(int, requirement.split(":")); ranks[s] = max(ranks[s], r)
        fields.append(".policy_skill_req={" + ",".join(map(str, ranks)) + "}")
        initializers.append(f"[{serial}]={{" + ",".join(fields) + "}")
    source = HARNESS.split("int main(", 1)[0]
    source += r'''
static player_type hero;
player_type *p_ptr=&hero;
static maxima limits;
maxima *z_info=&limits;
cptr skill_names_full[S_MAX]={"Melee","Archery","Evasion","Stealth","Perception","Will","Smithing","Song","Special"};
'''
    source += "static char names[]=" + json.dumps(names) + ";char *b_name=names;\n"
    source += "static char texts[]=" + json.dumps(texts) + ";char *b_text=texts;\n"
    source += "static ability_type entries[176]={" + ",\n".join(initializers) + "};ability_type *b_info=entries;\n"
    experience = (ROOT / "src/player/experience.c").read_text()
    source += function(experience, "insight_reworked_enabled")
    policy = (ROOT / "src/player/player-insight-abilities.c").read_text()
    for name in ("ability_display_name", "ability_effect_text"):
        source += function(policy, name)
    source += r'''
int ability_index(int skill,int local) {
    for(int i=0;i<176;i++)if(entries[i].name&&entries[i].skilltype==skill&&entries[i].abilitynum==local)return i;
    return -1;
}
'''
    game = (ROOT / "src/tutorial/tutorial-game.c").read_text()
    source += function(game, "tutorial_game_format_view")
    source += r'''
static void checks(void) {
    tutorial_view authored, modern, replay;
    tutorial_status before, after;
    int ability_cards=0;
    tutorial_set_view_formatter(tutorial_game_format_view);
    for(int i=0;i<tutorial_archive_count();i++) {
        hero.insight_ruleset=INSIGHT_RULESET_CLASSIC;
        assert(tutorial_archive_entry(i,&authored,&before));
        hero.insight_ruleset=INSIGHT_RULESET_LEGACY;
        assert(tutorial_archive_entry(i,&modern,&after));
        assert(!strcmp(authored.title,modern.title)&&!strcmp(authored.body,modern.body));
        hero.insight_ruleset=INSIGHT_RULESET_REWORKED;
        assert(tutorial_archive_entry(i,&modern,&after));
        assert(before==after&&authored.kind==modern.kind&&authored.step_count==modern.step_count);
        assert(!strcmp(authored.action,modern.action)&&!strcmp(authored.anchor,modern.anchor));
        int serial, end=0;
        if(sscanf(modern.id,"ability.%d.preview%n",&serial,&end)==1&&end&& !modern.id[end]) {
            ability_cards++;
            assert(!strcmp(modern.title,ability_display_name(&entries[serial])));
            if(entries[serial].policy_kind==ABILITY_POLICY_RETIRED)assert(strstr(modern.body,"retired"));
            else assert(strstr(modern.body,ability_effect_text(&entries[serial])));
            assert(!strstr(modern.body,"Quick Study bypass")&&!strstr(modern.body,"in place of Dodging"));
            assert(!strstr(modern.body,"base Grace")&&!strstr(modern.body,"base Dexterity"));
            assert(tutorial_replay(modern.id));
            tutorial_checkpoint(true);assert(tutorial_get_view(&replay));
            assert(!strcmp(modern.title,replay.title)&&!strcmp(modern.body,replay.body));
            assert(replay.kind==TUTORIAL_STEP_INFO&&replay.can_continue&&!replay.action[0]);
            tutorial_invalidate_context();
        }
        if(!strcmp(modern.id,"ability.80.preview")) {
            assert(!strcmp(modern.title,"Appraisal"));
            assert(strstr(modern.body,"500 base XP")&&strstr(modern.body,"Perception 1 invested"));
            assert(!strstr(modern.body,"bypass") || strstr(modern.body,"Inherited grants bypass"));
        }
        if(!strcmp(modern.id,"ability.63.preview")) assert(strstr(modern.body,"Evasion 6 invested")&&!strstr(modern.body,"Stealth"));
        if(!strcmp(modern.id,"ability.64.preview")) assert(strstr(modern.body,"Melee 7 invested"));
        if(!strcmp(modern.id,"ability.125.preview"))assert(strstr(modern.body,"Requires all: Enchantment")&&strstr(modern.body,"Requires one of:"));
        if(!strcmp(modern.id,"ability.89.preview"))assert(strstr(modern.body,"Harness")&&strstr(modern.body,"Warden"));
        if(!strcmp(modern.id,"ability.110.preview"))assert(strstr(modern.body,"Pack")&&strstr(modern.body,"Indomitable"));
        if(!strcmp(modern.id,"menu.abilities")) assert(strstr(modern.body,"one learning currency")&&strstr(modern.body,"no second mastery"));
        if(!strcmp(modern.id,"ability.174.preview")||!strcmp(modern.id,"ability.175.preview")) {
            assert(!strcmp(modern.title,serial==174?"Silent Passage":"Veil of Shadows"));
            tutorial_observe(modern.id,NULL);tutorial_checkpoint(true);assert(tutorial_get_view(&replay));
            assert(!strcmp(replay.body,modern.body)&&replay.kind==TUTORIAL_STEP_DECISION);
            tutorial_continue();tutorial_checkpoint(true);tutorial_invalidate_context();
        }
    }
    assert(ability_cards==112);
    /* Public presentation is independent of unseen actors, inventory or an
     * observation's contextual strings: only ability metadata and ruleset. */
    tutorial_view a={0},b={0};a.step=b.step=1;
    SDL_strlcpy(a.id,"ability.80.preview",sizeof(a.id));b=a;
    SDL_strlcpy(b.context.text,"secret fixture",sizeof(b.context.text));
    tutorial_game_format_view(&a);tutorial_game_format_view(&b);assert(!strcmp(a.body,b.body));
    puts("Insight tutorials: all112 public policies, live/archive/replay, Appraisal, moved skills, new abilities, retired stats, AND/OR, currency and classic/LEGACY preservation PASS.");
}
int main(int argc,char**argv) {
    assert(argc==2);assert(SDL_Init(0));limits.b_max=176;test_tale.id=781;
    assert(tutorial_load_catalogue(argv[1]));tutorial_set_mode(TUTORIAL_MODE_EXTENDED);tutorial_sync_tale();
    checks();tutorial_shutdown();SDL_Quit();return 0;
}
'''
    # JSON emits \u0000, which is not a valid C universal character. NUL
    # separators are fixed-width octal escapes so a following digit is safe.
    source = source.replace("\\u0000", "\\000")
    check = OUT / "check.c"; check.write_text(source, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(check), str(ROOT / "src/cJSON.c"), "_deps/SDL/libSDL3.dll.a", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/help/tutorials.json")], cwd=data, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
