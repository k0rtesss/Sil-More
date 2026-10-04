#!/usr/bin/env python3
"""Exercise every new Insight purchase and its production UI/learning/effects.

Uses fresh engine objects and isolated temporary data; never opens player saves.
Run build-incremental.ps1 first. Classic/previous Insight also have the dedicated
check_ability_requirements.py regression harness.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_ability_requirements import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/insight-abilities"
CHECKS = r'''
#include "object/object-inventory-limits.h"
static player_race fixture_race;
static void modern_reset(void)
{
    reset_player();
    p_ptr->insight_ruleset = INSIGHT_RULESET_REWORKED;
    p_ptr->light_dimmed = false;
    p_ptr->stealth_mode = false;
    p_ptr->oath_type = 0;
    p_ptr->bane_type = 0;
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    for (int i = 0; i < A_MAX; ++i) p_ptr->stat_base[i] = -3;
    for (int i = 0; i < S_SPC; ++i) p_ptr->skill_base[i] = 100;
    p_ptr->new_exp = 100000;
    p_ptr->insight_points = 7;
    accept_purchase = true;
}

static void check_policy_parser(void)
{
    ability_type entries[1]={0}; header h={0};
    h.info_num=1; h.info_ptr=entries;
    h.name_ptr=calloc(z_info->fake_name_size,1);
    h.text_ptr=calloc(z_info->fake_text_size,1);
    error_idx=-1;
    assert(!parse_line(&h,"N:0:Policy fixture"));
    assert(!parse_line(&h,"I:0:0:1"));
    assert(!parse_line(&h,"X:2:1:0:4"));
    assert(parse_line(&h,"O:")); assert(parse_line(&h,"H:"));
    assert(parse_line(&h,"O:0/0:")); assert(parse_line(&h,"H:0/0:"));
    assert(parse_line(&h,"O:8/0")); assert(parse_line(&h,"H:0/-1"));
    assert(!parse_line(&h,"O:0/1:0/4"));
    assert(!parse_line(&h,"H:2/0"));
    assert(entries[0].policy_or_count==2 && entries[0].policy_and_count==1);
    assert(parse_line(&h,"O:0/1"));
    free(h.name_ptr); free(h.text_ptr);
    puts("Policy parser: malformed/empty/truncated/duplicate parent lists reject, OR/AND retained PASS.");
}

static void check_complete_catalogue(void)
{
    int xp = 0, insight = 0, earned = 0, retired = 0, xp_sum = 0, ip_sum = 0;
    assert(!ability_policy_validate());
    for (int i = 0; i < z_info->b_max; ++i)
    {
        ability_type* ability = &b_info[i];
        if (!ability->name) continue;
        modern_reset();
        if (ability->policy_kind == ABILITY_POLICY_RETIRED)
        {
            ++retired;
            assert(!ability_policy_available(ability));
            assert(!prereqs(ability->skilltype, ability->abilitynum));
            assert(!ability_browser_activate_choice(ability->skilltype, ability->abilitynum));
            assert(p_ptr->new_exp == 100000 && p_ptr->insight_points == 7);
            continue;
        }
        if (ability->policy_kind == ABILITY_POLICY_EARNED)
        {
            ++earned;
            assert(!ability_browser_activate_choice(ability->skilltype, ability->abilitynum));
            continue;
        }
        assert(ability->policy_kind == ABILITY_POLICY_XP
            || ability->policy_kind == ABILITY_POLICY_INSIGHT);
        for (int j = 0; j < ability->policy_and_count; ++j)
            fixture_learn(ability->policy_and_skill[j], ability->policy_and_ability[j]);
        if (ability->policy_or_count)
            fixture_learn(ability->policy_or_skill[0], ability->policy_or_ability[0]);
        if (ability->skilltype == S_SNG && ability->abilitynum == SNG_WOVEN_THEMES)
        {
            fixture_learn(S_SNG, SNG_ELBERETH);
            fixture_learn(S_SNG, SNG_SILENCE);
        }
        if (ability->skilltype == S_PER && ability->abilitynum == PER_BANE)
            for (int r = 1; r < z_info->r_max; ++r) l_list[r].pkills = 4;
        assert(ability_stat_requirements_met(ability));
        assert(prereqs(ability->skilltype, ability->abilitynum));
        int fee = ability_purchase_xp(ability);
        int ip = ability_purchase_insight_cost(ability->skilltype, ability->abilitynum);
        if (ability->policy_kind == ABILITY_POLICY_XP)
        {
            ++xp; xp_sum += ability->policy_cost;
            assert(!ip && (fee == 500 || fee == 800));
            assert(!ability->policy_or_count && !ability->policy_and_count);
            p_ptr->insight_points = 0;
        }
        else
        {
            ++insight; ip_sum += ip;
            assert(!fee && (ip == 1 || ip == 2));
            p_ptr->new_exp = 0;
        }
        s32b before_xp = p_ptr->new_exp, before_ip = p_ptr->insight_points;
        assert(ability_browser_activate_choice(ability->skilltype, ability->abilitynum));
        assert(p_ptr->innate_ability[ability->skilltype][ability->abilitynum]);
        assert(p_ptr->new_exp == before_xp - fee);
        assert(p_ptr->insight_points == before_ip - ip);
    }
    assert(xp == 37 && insight == 52 && earned == 15 && retired == 8);
    assert(xp_sum == 25100 && ip_sum == 68);
    puts("Catalogue: all112 dispositions, every89 purchase, one currency, no XP parent toll or stat gate PASS.");
}

static void check_modern_prices_and_atomicity(void)
{
    modern_reset();
    const ability_type* power = &b_info[ability_index(S_MEL, MEL_POWER)];
    assert(ability_purchase_xp(power) == 500);
    fixture_learn(S_MEL, MEL_FINESSE); fixture_learn(S_MEL, MEL_CHARGE);
    assert(ability_purchase_xp(power) == 500);
    fixture_race.flags = RHF_MEL_AFFINITY;
    assert(ability_purchase_xp(power) == 450);
    current_character_profile->flags = RHF_MEL_AFFINITY | RHF_FREE;
    assert(ability_purchase_xp(power) == 300);
    fixture_race.flags = RHF_MEL_PENALTY;
    current_character_profile->flags = RHF_MEL_PENALTY;
    assert(ability_purchase_xp(power) == 600);
    fixture_race.flags = 0; current_character_profile->flags = 0;
    current_character_profile->flags_u = UNQ_MINSTREL;
    const ability_type* silence = &b_info[ability_index(S_SNG, SNG_SILENCE)];
    assert(ability_purchase_xp(silence) == 400);
    current_character_profile->flags_u = UNQ_EARENDIL;
    current_character_profile->flags = RHF_FREE;
    const ability_type* breaking = &b_info[ability_index(S_WIL, WIL_CURSE_BREAKING)];
    assert(ability_purchase_xp(breaking) == 250);
    current_character_profile->flags = current_character_profile->flags_u = 0;
    modern_reset();
    p_ptr->skill_base[S_ARC] = 5; p_ptr->skill_base[S_STL] = 2;
    p_ptr->new_exp = 1699; p_ptr->insight_points = 0;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->new_exp == 1699 && p_ptr->skill_base[S_ARC] == 5 && p_ptr->skill_base[S_STL] == 2);
    p_ptr->new_exp = 2000; accept_purchase = false;
    assert(!ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->new_exp == 2000 && p_ptr->skill_base[S_STL] == 2);
    accept_purchase = true;
    assert(ability_browser_activate_choice(S_ARC, ARC_AMBUSH));
    assert(p_ptr->new_exp == 300 && !p_ptr->insight_points);
    assert(p_ptr->skill_base[S_ARC] == 6 && p_ptr->skill_base[S_STL] == 3);
    modern_reset();
    p_ptr->skill_base[S_STL] = 0; p_ptr->skill_base[S_EVN] = 5;
    p_ptr->new_exp = 1400; p_ptr->insight_points = 0;
    assert(ability_browser_activate_choice(S_STL, STL_EXCHANGE_PLACES));
    assert(p_ptr->skill_base[S_STL] == 0 && p_ptr->skill_base[S_EVN] == 6);
    assert(!p_ptr->new_exp && p_ptr->innate_ability[S_STL][STL_EXCHANGE_PLACES]);
    assert(!p_ptr->innate_ability[S_EVN][EVN_CROWD_FIGHTING]);
    puts("Prices: static XP, affinity/penalty/Seafarer/Minstrel/floor; atomic mixed-rank training and Exchange identity PASS.");
}

static void check_modern_inheritance_and_parents(void)
{
    modern_reset();
    fixture_learn(S_MEL, MEL_CONTROL); /* inherited Subtlety, no Finesse */
    p_ptr->new_exp = 0; p_ptr->insight_points = 2;
    assert(prereqs(S_MEL, MEL_RAPID_ATTACK));
    assert(ability_browser_activate_choice(S_MEL, MEL_RAPID_ATTACK));
    assert(!p_ptr->innate_ability[S_MEL][MEL_FINESSE]);
    assert(!p_ptr->insight_points && !p_ptr->new_exp);
    modern_reset();
    fixture_learn(S_PER, PER_QUICK_STUDY);
    assert(!prereqs(S_EVN, EVN_FLANKING));
    p_ptr->have_ability[S_EVN][EVN_DODGING] = true;
    p_ptr->active_ability[S_EVN][EVN_DODGING] = true;
    assert(!prereqs(S_EVN, EVN_FLANKING));
    fixture_learn(S_EVN, EVN_DODGING); assert(prereqs(S_EVN, EVN_FLANKING));
    modern_reset();
    fixture_learn(S_SMT, SMT_ENCHANTMENT);
    assert(!prereqs(S_SMT, SMT_ARTEFACT));
    fixture_learn(S_SMT, SMT_WEAPONSMITH); assert(prereqs(S_SMT, SMT_ARTEFACT));
    modern_reset();
    fixture_learn(S_STL, STL_DISGUISE);
    assert(!prereqs(S_STL, STL_SILENT_PASSAGE));
    fixture_learn(S_STL, STL_EXCHANGE_PLACES); assert(prereqs(S_STL, STL_SILENT_PASSAGE));
    modern_reset();
    assert(!prereqs(S_SNG, SNG_WOVEN_THEMES));
    fixture_learn(S_SNG, SNG_SILENCE); assert(!prereqs(S_SNG, SNG_WOVEN_THEMES));
    fixture_learn(S_SNG, SNG_ELBERETH); assert(prereqs(S_SNG, SNG_WOVEN_THEMES));
    puts("Knowledge: inherited anchors, no ancestor debt, no Appraisal/equipment bypass, AND+OR and two-song weaving PASS.");
}

static void check_modern_retirement_and_capacity(void)
{
    modern_reset();
    for (int stat = 0; stat < A_MAX; ++stat) p_ptr->stat_base[stat] = 3;
    const int skills[] = {S_MEL,S_ARC,S_EVN,S_STL,S_PER,S_WIL,S_SMT,S_SNG};
    const int locals[] = {MEL_STR,ARC_DEX,EVN_DEX,STL_DEX,PER_GRA,WIL_CON,SMT_GRA,SNG_GRA};
    for (int i = 0; i < 8; ++i) fixture_learn(skills[i],locals[i]);
    for (int stat = 0; stat < A_MAX; ++stat) assert(player_permanent_stat(stat) == 3);
    calc_bonuses();
    for (int i = 0; i < 8; ++i) assert(!p_ptr->active_ability[skills[i]][locals[i]]
        && !p_ptr->have_ability[skills[i]][locals[i]]);
    int helm = 0;
    for (int i = 1; i < z_info->k_max; ++i) if (k_info[i].tval == TV_HELM) {helm=i;break;}
    assert(helm); object_prep(&inventory[INVEN_HEAD],helm);
    inventory[INVEN_HEAD].abilities=3;
    inventory[INVEN_HEAD].skilltype[0]=S_PER; inventory[INVEN_HEAD].abilitynum[0]=PER_GRA;
    inventory[INVEN_HEAD].skilltype[1]=S_WIL; inventory[INVEN_HEAD].abilitynum[1]=WIL_INDOMITABLE;
    inventory[INVEN_HEAD].skilltype[2]=S_MEL; inventory[INVEN_HEAD].abilitynum[2]=MEL_WARDEN;
    calc_bonuses();
    assert(!p_ptr->have_ability[S_PER][PER_GRA] && !p_ptr->active_ability[S_PER][PER_GRA]);
    assert(inventory_limit_limit_for_group(INV_LIMIT_PACK)==INVENTORY_PACK_VOLUME_CAP);
    assert(inventory_limit_limit_for_group(INV_LIMIT_HARNESS)==INVENTORY_HARNESS_VOLUME_CAP);
    fixture_learn(S_MEL,MEL_WARDEN); fixture_learn(S_WIL,WIL_INDOMITABLE);
    assert(inventory_limit_limit_for_group(INV_LIMIT_PACK)
        == INVENTORY_PACK_VOLUME_CAP+INVENTORY_CONSTITUTION_PACK_VOLUME_BONUS);
    assert(inventory_limit_limit_for_group(INV_LIMIT_HARNESS)
        == INVENTORY_HARNESS_VOLUME_CAP+INVENTORY_GRACE_HARNESS_VOLUME_BONUS);
    p_ptr->active_ability[S_MEL][MEL_WARDEN]=false;
    assert(inventory_limit_limit_for_group(INV_LIMIT_HARNESS)==INVENTORY_HARNESS_VOLUME_CAP);
    modern_reset();
    fixture_learn(S_SMT,SMT_ENCHANTMENT); fixture_learn(S_SMT,SMT_EXPERTISE);
    fixture_learn(S_SMT,SMT_ARTEFACT);
    p_ptr->stat_base[A_DEX]=-1; p_ptr->stat_base[A_GRA]=3;
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT)==150);
    assert(smithing_mastery_stat_bonus_scaled(SMT_EXPERTISE)==0);
    assert(smithing_mastery_stat_bonus_scaled(SMT_ARTEFACT)==300);
    assert(!insight_ability_upgrade_cost(S_SMT,SMT_ENCHANTMENT));
    p_ptr->stat_base[A_GRA]=5;
    assert(smithing_mastery_stat_bonus_scaled(SMT_ENCHANTMENT)==250);
    puts("Effects: all8 retired powers incl item aggregation, learned-only storage, integrated ongoing nonnegative Smithing PASS.");
}

static void check_modern_browser(cptr output)
{
    modern_reset();
    ability_browser_entry entries[ABILITY_BROWSER_ENTRIES_MAX];
    char summary[512]; ability_browser_build_summary(S_MEL,summary,sizeof(summary));
    assert(strstr(summary,"500/800 XP base or 1/2 Insight"));
    assert(!strstr(summary,"per rank"));
    int count=ability_browser_collect_entries(ABILITY_BROWSER_INSIGHT,entries,ABILITY_BROWSER_ENTRIES_MAX);
    assert(count==52);
    count=ability_browser_collect_entries(S_EVN,entries,ABILITY_BROWSER_ENTRIES_MAX);
    assert(count==11);
    bool exchange=false,crowd=false;
    for(int i=0;i<count;++i) {
        exchange |= entries[i].b_ptr->skilltype==S_STL && entries[i].abilitynum==STL_EXCHANGE_PLACES;
        crowd |= entries[i].b_ptr->skilltype==S_EVN && entries[i].abilitynum==EVN_CROWD_FIGHTING;
    }
    assert(exchange && crowd);
    count=ability_browser_collect_entries(S_MEL,entries,ABILITY_BROWSER_ENTRIES_MAX);
    assert(count==17);
    bool opportunist=false;
    for(int i=0;i<count;++i) opportunist |= entries[i].b_ptr->skilltype==S_STL && entries[i].abilitynum==STL_OPPORTUNIST;
    assert(opportunist);
    for(int skill=0;skill<S_SPC;++skill) {
        count=ability_browser_collect_entries(skill,entries,ABILITY_BROWSER_ENTRIES_MAX);
        for(int width=40;width<=100;width+=20) {
            Term_resize(width,30);
            for(int i=0;i<count;++i) {
                ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
                int n=ability_browser_build_description(skill,&entries[i],lines,width-4);
                for(int j=0;j<n;++j) assert(menu_text_display_width(lines[j].text)<=width-4);
                assert(!description_has(lines,n,"Quick Study bypasses"));
                assert(!description_has(lines,n,"permanent stats"));
            }
        }
    }
    modern_reset(); Term_resize(80,30);
    char path[1024]; strnfmt(path,sizeof(path),"%s/new-insight.txt",output);
    capture_path=path; input_sequence="\033"; Term_flush();
    do_cmd_ability_screen(); capture_path=input_sequence=NULL;
    const ability_type* appraisal=&b_info[ability_index(S_PER,PER_QUICK_STUDY)];
    assert(streq(ability_display_name(appraisal),"Appraisal"));
    for(int mode=INSIGHT_RULESET_CLASSIC;mode<=INSIGHT_RULESET_LEGACY;++mode) {
        p_ptr->insight_ruleset=mode;
        assert(streq(ability_display_name(appraisal),"Quick Study"));
        count=ability_browser_collect_entries(S_STL,entries,ABILITY_BROWSER_ENTRIES_MAX);
        for(int i=0;i<count;++i) assert(entries[i].abilitynum<STL_SILENT_PASSAGE);
    }
    Term_resize(80,24);
    puts("Browser: all8 categories, 52-IP index, stable moved identities, all descriptions40/60/80/100 columns, legacy isolation PASS.");
}

static void check_browser_hotkey_capture(cptr output, cptr label,
    cptr sequence, cptr selected_name)
{
    char path[1024], text[8192], expected[160];
    strnfmt(path, sizeof(path), "%s/hotkeys-%s.txt", output, label);
    capture_path = path; input_sequence = sequence; Term_flush();
    long xp = p_ptr->new_exp, ip = p_ptr->insight_points;
    do_cmd_ability_screen();
    assert(!*input_sequence);
    capture_path = input_sequence = NULL;
    assert(p_ptr->new_exp == xp && p_ptr->insight_points == ip);
    FILE* file = fopen(path, "r"); assert(file);
    size_t size = fread(text, 1, sizeof(text) - 1, file);
    text[size] = '\0'; fclose(file);
    strnfmt(expected, sizeof(expected), "%s:", selected_name);
    assert(strstr(text, expected));
    assert(!strstr(text, "i) ") && !strstr(text, "q) "));
    assert(!strstr(text, "{) ") && !strstr(text, "}) "));
}

static void check_browser_hotkeys(cptr output)
{
    modern_reset(); Term_resize(96, 36);
    p_ptr->insight_ruleset = INSIGHT_RULESET_CLASSIC;
    check_browser_hotkey_capture(output, "classic-ninth", "j\033", "Warden");
    check_browser_hotkey_capture(output, "classic-last", "s\033", "Strength");
    check_browser_hotkey_capture(output, "skills-command", "i\033\033", "Power");
    input_sequence = "q"; Term_flush(); do_cmd_ability_screen();
    assert(!*input_sequence); input_sequence = NULL;

    modern_reset();
    ability_browser_entry entries[ABILITY_BROWSER_ENTRIES_MAX];
    int count = ability_browser_collect_entries(ABILITY_BROWSER_INSIGHT,
        entries, ABILITY_BROWSER_ENTRIES_MAX);
    assert(count > 24);
    check_browser_hotkey_capture(output, "insight-last-letter",
        "\t\t\t\t\t\t\t\tz\033", ability_display_name(entries[23].b_ptr));
    char sequence[ABILITY_BROWSER_ENTRIES_MAX + 16];
    memset(sequence, '\t', 8);
    memset(sequence + 8, '2', count - 1);
    sequence[8 + count - 1] = ESCAPE; sequence[8 + count] = '\0';
    check_browser_hotkey_capture(output, "insight-overflow", sequence,
        ability_display_name(entries[count - 1].b_ptr));
    assert(!ability_browser_entry_letter(24));
    assert(ability_browser_entry_from_letter('i') < 0);
    assert(ability_browser_entry_from_letter('q') < 0);
    assert(ability_browser_entry_from_letter('{') < 0);
    assert(ability_browser_entry_from_letter('}') < 0);
    Term_resize(80, 24);
    puts("Browser hotkeys: actual j/s/z dispatch, i skills, q exit, overflow navigation, labels and unchanged currencies PASS.");
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix, original_main = HARNESS.split("int main(int argc, char** argv)", 1)
    setup = original_main.split("    check_parser();", 1)[0]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + "\nint main(int argc, char** argv)" + setup + r'''
    const player_race* original_race=rp_ptr; fixture_race=*rp_ptr; rp_ptr=&fixture_race;
    u32b race_flags=rp_ptr->flags, origin_flags=current_character_profile->flags;
    u32b origin_unique=current_character_profile->flags_u;
    fixture_race.flags=0; current_character_profile->flags=0; current_character_profile->flags_u=0;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    for(int y=0;y<20;++y) for(int x=0;x<20;++x) cave_feat[y][x]=FEAT_FLOOR;
    assert(!vinfo_init());
    check_policy_parser(); check_complete_catalogue(); check_modern_prices_and_atomicity();
    check_modern_inheritance_and_parents(); check_modern_retirement_and_capacity();
    check_modern_browser(argv[3]);
    check_browser_hotkeys(argv[3]);
    fixture_race.flags=race_flags; rp_ptr=original_race; current_character_profile->flags=origin_flags;
    current_character_profile->flags_u=origin_unique;
    puts("New Insight abilities engine integration PASS.");
    SDL_Quit(); return 0;
}
''', encoding="utf-8")
    cmake=BUILD/"CMakeFiles/sil-more.dir"
    objects=shlex.split((cmake/"objects1.rsp").read_text())
    objects=[p for p in objects if not p.endswith(("/src/main.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj"))]
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects), encoding="utf-8")
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([
        *(str(BUILD/"_deps"/name) for name in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),
        "C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),
        "@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp","-o",str(exe)],
        cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/"lib/edit"),data,str(OUT)],cwd=data,env=env,check=True,timeout=60)

if __name__ == "__main__":
    main()

