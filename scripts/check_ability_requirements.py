#!/usr/bin/env python3
"""Check parsed skill gates and atomic multi-skill ability purchases.

Run build-incremental.ps1 first. Uses real engine objects, a terminal fixture,
and temporary template caches. No player saves or visible game windows are used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/ability-requirements"

HARNESS = r'''
#include "angband.h"
#include "log/log.h"
#include "init/init2-internal.h"
#include <assert.h>

/* Capture the actual purchase question; only the user's answer is simulated. */
bool fixture_confirm(cptr prompt);
#define get_check fixture_confirm
#include "cmd/ui/cmd-ui-abilities.c"
#undef get_check

static term fixture_term;
static char confirmation[1024];
static int confirmation_count;
static bool accept_purchase;
extern char g_touch_pane_yes_no_prompt_text[];

bool fixture_confirm(cptr prompt)
{
    SDL_strlcpy(confirmation, prompt, sizeof(confirmation));
    sdl_touch_pane_begin_yes_no_prompt(prompt);
    size_t length = strlen(prompt);
    while (length && isspace((unsigned char)prompt[length - 1])) length--;
    assert(strlen(g_touch_pane_yes_no_prompt_text) == length);
    assert(!strncmp(g_touch_pane_yes_no_prompt_text, prompt, length));
    sdl_touch_pane_end_yes_no_prompt();
    confirmation_count++;
    return accept_purchase;
}

static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) Term_keypress(' ');
    return 0;
}

static void reset_player(void)
{
    memset(p_ptr->skill_base, 0, sizeof(p_ptr->skill_base));
    memset(p_ptr->skill_use, 0, sizeof(p_ptr->skill_use));
    memset(p_ptr->skill_equip_mod, 0, sizeof(p_ptr->skill_equip_mod));
    memset(p_ptr->skill_misc_mod, 0, sizeof(p_ptr->skill_misc_mod));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    for (int stat = 0; stat < A_MAX; ++stat) p_ptr->stat_base[stat] = 3;
    p_ptr->new_exp = 100000;
    p_ptr->knowledge_points = 77;
    op_ptr->opt[OPT_lore_beta] = false;
    confirmation[0] = '\0'; confirmation_count = 0;
    accept_purchase = false;
}

static errr parse_line(header* h, cptr line)
{
    char text[256];
    SDL_strlcpy(text, line, sizeof(text));
    return parse_b_info(text, h);
}

static void check_parser(void)
{
    ability_type entries[1] = {0};
    header h = {0};
    h.info_num = 1; h.info_ptr = entries;
    h.name_ptr = calloc(z_info->fake_name_size, 1);
    h.text_ptr = calloc(z_info->fake_text_size, 1);
    error_idx = -1;
    assert(!parse_line(&h, "N:0:Fixture"));
    assert(!parse_line(&h, "I:1:4:6"));
    assert(!parse_line(&h, "R:DEX:2:STEALTH:3:WILL:5:LORE:2"));
    assert(entries[0].stat_req[A_DEX] == 2 && entries[0].lore_req == 2);
    assert(entries[0].skill_req[S_STL] == 3 && entries[0].skill_req[S_WIL] == 5);
    assert(!parse_line(&h, "R:skill_archery:4:S_PER:2"));
    assert(ability_required_skill(&entries[0], S_ARC) == 6);
    assert(!parse_line(&h, "R:ARCHERY:8:STEALTH:2"));
    assert(ability_required_skill(&entries[0], S_ARC) == 8);
    assert(entries[0].skill_req[S_STL] == 3);
    assert(parse_line(&h, "R:SPECIAL:1"));
    assert(parse_line(&h, "R:STEALTH:101"));
    assert(parse_line(&h, "R:STEALTH:-1"));
    assert(parse_line(&h, "R:UNKNOWN:3"));
    assert(parse_line(&h, "R:WILL"));
    assert(!parse_line(&h, "R:0:2")); /* Existing numeric stat syntax. */
    assert(entries[0].stat_req[A_STR] == 2);
    free(h.name_ptr); free(h.text_ptr);
    puts("Parser: mixed stats/Lore/skills, aliases, duplicate maxima and invalid ranks PASS.");
}

static void check_selected_requirements(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    ability_type* contest = &b_info[ability_index(S_SNG, SNG_CONTEST)];
    assert(ambush->skill_req[S_STL] == 3 && contest->skill_req[S_WIL] == 5);
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_use[S_STL] = p_ptr->skill_equip_mod[S_STL] = 30;
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = true;
    assert(!prereqs(S_ARC, ARC_AMBUSH));
    p_ptr->skill_base[S_STL] = 3;
    assert(prereqs(S_ARC, ARC_AMBUSH));
    p_ptr->skill_base[S_SNG] = 12;
    assert(!prereqs(S_SNG, SNG_CONTEST));
    p_ptr->skill_base[S_WIL] = 5;
    assert(prereqs(S_SNG, SNG_CONTEST));
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = false;
    assert(!prereqs(S_SNG, SNG_CONTEST));
    int parents[] = {SNG_STAYING, SNG_DISGUISE, SNG_MASTERY};
    for (int i = 0; i < 3; ++i)
    {
        p_ptr->innate_ability[S_SNG][parents[i]] = true;
        assert(prereqs(S_SNG, SNG_CONTEST));
        p_ptr->innate_ability[S_SNG][parents[i]] = false;
    }
    p_ptr->skill_base[S_EVN] = 8;
    assert(!prereqs(S_EVN, EVN_FLANKING));
    p_ptr->have_ability[S_EVN][EVN_DODGING] = true;
    assert(!prereqs(S_EVN, EVN_FLANKING)); /* Equipment cannot teach a prerequisite. */
    p_ptr->innate_ability[S_EVN][EVN_DODGING] = true;
    assert(prereqs(S_EVN, EVN_FLANKING));
    p_ptr->skill_base[S_WIL] = 9;
    p_ptr->innate_ability[S_WIL][WIL_CURSE_BREAKING] = true;
    assert(!prereqs(S_WIL, WIL_MAJESTY));
    p_ptr->innate_ability[S_WIL][WIL_FORMIDABLE] = true;
    assert(prereqs(S_WIL, WIL_MAJESTY));
    assert(prereqs(S_SNG, SNG_SLAYING));
    p_ptr->innate_ability[S_WIL][WIL_FORMIDABLE] = false;
    p_ptr->innate_ability[S_WIL][WIL_INNER_LIGHT] = true;
    assert(prereqs(S_WIL, WIL_MAJESTY));
    p_ptr->innate_ability[S_SNG][SNG_STAUNCHING] = true;
    assert(!prereqs(S_SNG, SNG_SLAYING));
    p_ptr->innate_ability[S_SNG][SNG_CHALLENGE] = true;
    assert(prereqs(S_SNG, SNG_SLAYING));
    puts("Five selected abilities: real template gates, OR routes and Quick Study/base-rank isolation PASS.");
}

static bool description_has(ability_browser_desc_line* lines, int count, cptr text)
{
    for (int i = 0; i < count; ++i)
        if (strstr(lines[i].text, text)) return true;
    return false;
}

static void check_purchase(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    p_ptr->skill_base[S_MEL] = 9; /* Unrelated investment must survive. */
    int price = ability_purchase_exp_cost(S_ARC);
    int total = 2100 + 600 + price;
    ability_skill_training training;
    assert(ability_browser_plan_training(ambush, &training));
    assert(training.count == 2 && training.total_cost == 2700);
    char training_text[768];
    ability_browser_format_training(&training, training_text, sizeof(training_text));
    assert(strstr(training_text, "Archery 0 to 6 (2100 XP)"));
    assert(strstr(training_text, "Stealth 0 to 3 (600 XP)"));
    p_ptr->new_exp = total - 1;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(confirmation_count == 0 && p_ptr->new_exp == total - 1);
    assert(!p_ptr->skill_base[S_ARC] && !p_ptr->skill_base[S_STL]);
    assert(!p_ptr->innate_ability[S_ARC][ARC_AMBUSH]);
    p_ptr->new_exp = total;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(confirmation_count == 1 && p_ptr->new_exp == total);
    assert(!p_ptr->skill_base[S_ARC] && !p_ptr->skill_base[S_STL]);
    assert(strstr(confirmation, training_text));
    char total_text[64]; strnfmt(total_text, sizeof(total_text), "%d XP total", total);
    assert(strstr(confirmation, total_text));
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_ARC] == 6 && p_ptr->skill_base[S_STL] == 3);
    assert(p_ptr->skill_base[S_MEL] == 9 && p_ptr->knowledge_points == 77);
    assert(!p_ptr->new_exp && p_ptr->innate_ability[S_ARC][ARC_AMBUSH]);

    reset_player();
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_base[S_STL] = 2;
    assert(ability_browser_plan_training(ambush, &training));
    assert(training.count == 1 && training.total_cost == 300);
    p_ptr->new_exp = 300 + ability_purchase_exp_cost(S_ARC);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_STL] == 3 && !p_ptr->new_exp);
    assert(!strstr(confirmation, "Raise Archery"));

    reset_player();
    p_ptr->skill_base[S_ARC] = 6;
    p_ptr->skill_base[S_STL] = 5;
    assert(ability_browser_plan_training(ambush, &training));
    assert(!training.count && !training.total_cost);
    p_ptr->new_exp = ability_purchase_exp_cost(S_ARC);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->skill_base[S_STL] == 5 && !strstr(confirmation, "Raise"));

    reset_player();
    p_ptr->skill_base[S_SNG] = 12;
    p_ptr->innate_ability[S_SNG][SNG_STAYING] = true;
    p_ptr->new_exp = 1500 + ability_purchase_exp_cost(S_SNG);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(p_ptr->skill_base[S_WIL] == 5 && !p_ptr->new_exp);

    /* Acquired and starting gifts remain usable below the new learning gates. */
    p_ptr->skill_base[S_WIL] = 0;
    int questions = confirmation_count;
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(!p_ptr->active_ability[S_SNG][SNG_CONTEST]);
    assert(ability_browser_activate_choice(S_SNG, SNG_CONTEST));
    assert(p_ptr->active_ability[S_SNG][SNG_CONTEST]);
    assert(confirmation_count == questions && !p_ptr->skill_base[S_WIL]);
    puts("Real purchase: multi-skill total, cancel/insufficient atomicity, exact XP, partial/no training and acquired gifts PASS.");
}

static void check_display_and_existing_gates(void)
{
    reset_player();
    ability_type* ambush = &b_info[ability_index(S_ARC, ARC_AMBUSH)];
    ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
    for (int width = 30; width <= 80; width += 50)
    {
        int count = 0;
        ability_browser_add_prerequisites(lines, &count, S_ARC, ambush, width);
        assert(description_has(lines, count, "6 Archery base"));
        assert(description_has(lines, count, "3 Stealth base"));
        assert(description_has(lines, count, "Total XP:"));
        for (int i = 0; i < count; ++i) assert(strlen(lines[i].text) <= (size_t)width);
    }
    p_ptr->skill_base[S_SMT] = 12;
    p_ptr->active_ability[S_PER][PER_QUICK_STUDY] = true;
    p_ptr->stat_base[A_DEX] = 0;
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    ability_skill_training training;
    ability_type synthetic = *ambush;
    synthetic.skill_req[S_ARC] = 8;
    assert(ability_browser_plan_training(&synthetic, &training));
    assert(training.total_cost == 3600 + 600); /* Primary counted only once. */
    synthetic.skill_req[S_WIL] = BASE_SKILL_MAX + 1;
    assert(!ability_browser_plan_training(&synthetic, &training));

    /* A data-authored ability can require more than two trained skills. */
    ability_type saved_ambush = *ambush;
    for (int skill = 0; skill < S_SPC; ++skill) ambush->skill_req[skill] = 3;
    p_ptr->skill_base[S_SMT] = 0;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(strlen(confirmation) > 160 && strstr(confirmation, "Song 0 to 3"));
    assert(strstr(confirmation, "XP total"));
    *ambush = saved_ambush;

    reset_player();
    ability_type* study = &b_info[ability_index(S_PER, PER_QUICK_STUDY)];
    byte saved_requirement = study->skill_req[S_STL];
    study->skill_req[S_STL] = 3;
    op_ptr->opt[OPT_lore_beta] = true; p_ptr->lore = 2;
    p_ptr->new_exp = 699;
    assert(!ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(p_ptr->new_exp == 699 && p_ptr->knowledge_points == 77);
    assert(!p_ptr->skill_base[S_PER] && !p_ptr->skill_base[S_STL]);
    p_ptr->new_exp = 700; accept_purchase = true;
    assert(ability_browser_activate_choice(S_PER, PER_QUICK_STUDY));
    assert(!p_ptr->new_exp && p_ptr->knowledge_points == 77);
    assert(p_ptr->skill_base[S_PER] == 1 && p_ptr->skill_base[S_STL] == 3);
    study->skill_req[S_STL] = saved_requirement;
    puts("Wrapped requirements, duplicate primary ranks, stat gates and Lore-currency multi-skill purchase PASS.");
}

int main(int argc, char** argv)
{
    assert(argc == 3); setbuf(stdout, NULL); log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    assert(term_init(&fixture_term, 80, 24, 256) == 0);
    fixture_term.xtra_hook = terminal_extra;
    angband_term[0] = &fixture_term; Term_activate(&fixture_term);
    assert(!init_z_info()); assert(!init_f_info()); assert(!init_k_info());
    assert(!init_b_info()); assert(!init_a_info()); assert(!init_e_info());
    assert(!init_r_info()); assert(!init_v_info()); assert(!init_p_info());
    assert(!init_c_info()); assert(!init_oath_info()); assert(!init_cu_info());
    assert(!init_mb_info()); assert(!init_style_info()); assert(!init_other());
    assert(!init_alloc());
    rp_ptr = &p_info[5]; current_character_profile = &c_info[30];
    p_ptr->prace = 5; p_ptr->pcharacter = 30;
    p_ptr->py = p_ptr->px = 2; cave_feat[2][2] = FEAT_FLOOR;
    check_parser(); check_selected_requirements(); check_purchase();
    check_display_and_existing_gates();
    puts("Ability requirements integration: PASS.");
    SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(
        ("/src/main.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects))
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
