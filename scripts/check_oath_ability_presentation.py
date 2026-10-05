#!/usr/bin/env python3
"""Check classic oath effects and living/dead ability-browser presentation.

Uses production browser code, shipped templates and isolated engine data.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/oath-ability-presentation"

CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "%s line %d\n", #expr, __LINE__); exit(1); } } while (0)

static bool description_has(const ability_browser_desc_line *lines, int count,
    cptr text)
{
    for (int i = 0; i < count; i++)
        if (strstr(lines[i].text, text)) return true;
    return false;
}

static ability_browser_entry special_entry(int ability)
{
    ability_browser_entry entry = {0};
    entry.abilitynum = ability;
    entry.b_ptr = &b_info[ability_index(S_SPC, ability)];
    SDL_strlcpy(entry.name, b_name + entry.b_ptr->name, sizeof(entry.name));
    p_ptr->have_ability[S_SPC][ability] = true;
    p_ptr->innate_ability[S_SPC][ability] = true;
    return entry;
}

static void check_oaths(void)
{
    extern bool death_spectator_mode;
    const int oaths[] = {OATH_MERCY, OATH_SILENCE, OATH_IRON, OATH_VALOROUS};
    const int abilities[] = {SPC_OATH_MERCY, SPC_OATH_SILENCE,
        SPC_OATH_IRON, SPC_OATH_VALOROUS};
    const cptr effects[] = {"+1 Grace", "+1 Dexterity", "+1 Constitution", "+1 Strength"};
    char state[32];
    p_ptr->new_exp = 12345;
    turn = 123; playerturn = 12;
    ability_browser_entry gift = special_entry(SPC_NIENA_MERCY);
    p_ptr->active_ability[S_SPC][gift.abilitynum] = true;
    ability_browser_entry_state(state, sizeof(state), S_SPC, &gift);
    assert(streq(state, "grant"));
    p_ptr->active_ability[S_SPC][gift.abilitynum] = false;
    ability_browser_entry_state(state, sizeof(state), S_SPC, &gift);
    assert(streq(state, "off"));
    for (int i = 0; i < 4; i++)
    {
        ability_browser_entry entry = special_entry(abilities[i]);
        assert(entry.b_ptr->effect && strstr(b_text + entry.b_ptr->effect, effects[i]));
        p_ptr->active_ability[S_SPC][abilities[i]] = false;
        p_ptr->oath_type = oaths[i];
        p_ptr->oaths_broken = 0;
        ability_browser_entry_state(state, sizeof(state), S_SPC, &entry);
        assert(streq(state, "off"));
        p_ptr->oaths_broken = (byte)(1U << (oaths[i] - 1));
        ability_browser_entry_state(state, sizeof(state), S_SPC, &entry);
        assert(streq(state, "lost"));
        char living[32], dead[32];
        strnfmt(living, sizeof(living), "%.24s", oath_permanent_message(oaths[i]));
        strnfmt(dead, sizeof(dead), "%.24s", oath_death_message(oaths[i]));
        assert(living[0] && dead[0] && !streq(living, dead));
        for (int mode = 0; mode < 3; mode++)
        {
            p_ptr->is_dead = mode == 1;
            death_spectator_mode = mode == 2;
            ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
            int count = ability_browser_build_description(S_SPC, &entry, lines, 100);
            assert(description_has(lines, count, "Broken oath"));
            assert(description_has(lines, count, mode ? dead : living));
            assert(!description_has(lines, count, mode ? living : dead));
        }
    }
    assert(p_ptr->new_exp == 12345 && turn == 123 && playerturn == 12);
    death_spectator_mode = false; p_ptr->is_dead = false;
    puts("Classic oath browser: four correct effects, gifts grant/off, intact oath off, broken oath lost, living/dead/Final Look text and free browsing PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n"
    browser = (ROOT / "src/cmd/ui/cmd-ui-abilities.c").read_text(encoding="utf-8")
    # ENGINE_FIXTURE already includes this unguarded declaration header.
    browser = browser.replace('#include "externs.h"\n', "", 1)
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + browser + "\n" + CHECKS + init +
                      "    check_oaths(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(("/src/main.c.obj",
                                      "/src/cmd/ui/cmd-ui-abilities.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()
