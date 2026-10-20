#!/usr/bin/env python3
"""Exercise unique kills, hero identities and all six quest reward paths.

Uses production engine objects and disposable profiles. Only presentation and
the reward-choice UI are doubled; no real profile or store account is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_achievement_expansion import CHECKS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-legends"

TESTS = r'''
errr init_quest_info(void);
static bool decline_gift;
int __wrap_quest_reward_book_choice(cptr title,cptr texts[],int count,cptr prompt,
    const int* choices,cptr labels[],const bool attrs[],int n,int initial,
    void (*inspect)(int))
{
    (void)title;(void)texts;(void)count;(void)prompt;(void)labels;
    (void)attrs;(void)initial;(void)inspect;
    assert(n>0); return decline_gift ? -1 : choices[0];
}
static void hero(int index)
{
    assert(index>=0 && index<z_info->c_max);
    p_ptr->pcharacter=index;current_character_profile=&c_info[index];
}
static monster_type* ready(int race)
{
    monster_type* m=prepare(race);
    p_ptr->chp=p_ptr->mhp=20000;p_ptr->new_exp=100000;
    p_ptr->energy_use=100;hero(0);
    ++turn;
    varda_quest_begin_player_turn();
    return m;
}
static void kill(monster_type* m,int who)
{ assert(mon_take_hit((int)(m-mon_list),30000,NULL,who)); }
static void check_uniques(void)
{
    const int races[]={R_IDX_GOTHMOG,R_IDX_GLAURUNG,R_IDX_GORTHAUR,
        R_IDX_UNGOLIANT,R_IDX_ANCALAGON,R_IDX_CARCHAROTH};
    const int ids[]={SIL_ACH_THE_HIGH_CAPTAIN_FALLS,SIL_ACH_DAGNIR_GLAURUNGA,
        SIL_ACH_THE_ABHORRED,SIL_ACH_UNLIGHT_UNWOVEN,
        SIL_ACH_THE_DRAGON_HOST_BROKEN,SIL_ACH_THE_RED_MAW};
    for(size_t i=0;i<N_ELEMENTS(races);i++) {
        monster_type* m=ready(races[i]);
        assert(!mon_take_hit((int)(m-mon_list),0,NULL,-1) && !earned(ids[i]));
        kill(m,0);assert(!earned(ids[i])); // ambient damage
        m=ready(races[i]);kill(m,1);assert(!earned(ids[i])); // another monster
        m=ready(races[i]);p_ptr->wizard=true;kill(m,-1);assert(!earned(ids[i]));
        m=ready(races[i]);p_ptr->game_type=1;kill(m,-1);assert(!earned(ids[i]));
        m=ready(races[i]);p_ptr->achievement_flags|=SIL_ACH_RUN_TAINTED;
        kill(m,-1);assert(!earned(ids[i]));
        m=ready(races[i]);kill(m,-1);assert(earned(ids[i]));
    }
    assert(!earned(SIL_ACH_LORD_OF_THE_FOUNTAIN));
    assert(!earned(SIL_ACH_MASTER_OF_DOOM));
    assert(!earned(SIL_ACH_BEFORE_THE_RISING_OF_THE_SUN));
    puts("PASS six important uniques: surviving hits, ambient/monster kills, wizard/practice/tainted runs excluded");
}
static void check_heroes(void)
{
    const int heroes[]={11,28,29,25};
    const int races[]={R_IDX_GOTHMOG,R_IDX_GLAURUNG,R_IDX_MAEGLIN,R_IDX_ANCALAGON};
    const int ids[]={SIL_ACH_LORD_OF_THE_FOUNTAIN,SIL_ACH_MASTER_OF_DOOM,
        SIL_ACH_FOR_IDRIL_AND_EARENDIL,SIL_ACH_BEFORE_THE_RISING_OF_THE_SUN};
    for(size_t i=0;i<N_ELEMENTS(heroes);i++) {
        monster_type* m=ready(races[i]);
        // A matching custom name and copied powers do not change the hero template.
        SDL_strlcpy(op_ptr->full_name,c_name+c_info[heroes[i]].name,sizeof(op_ptr->full_name));
        u32b previous=c_info[0].flags_u;c_info[0].flags_u=c_info[heroes[i]].flags_u;
        kill(m,-1);c_info[0].flags_u=previous;assert(!earned(ids[i]));
        m=ready(races[i]);hero(heroes[i]);kill(m,0);assert(!earned(ids[i]));
        m=ready(41);hero(heroes[i]);kill(m,-1);assert(!earned(ids[i]));
        m=ready(races[i]);hero(heroes[i]);
        SDL_strlcpy(op_ptr->full_name,"A renamed hero",sizeof(op_ptr->full_name));
        // The persistent GUID still works if a profile occupies another index.
        character_profile original=c_info[0];c_info[0]=c_info[heroes[i]];hero(0);
        kill(m,-1);c_info[0]=original;assert(earned(ids[i]));
    }
    puts("PASS four historical deeds: persistent hero identity, renamed/reindexed templates, wrong hero/victim/source controls");
}
static void check_quests(void)
{
    const int ids[]={SIL_ACH_TULKAS_LAST_LAUGH,SIL_ACH_AULES_APPRENTICE,
        SIL_ACH_THE_DOOMSMANS_DUE,SIL_ACH_PITY_FOR_THE_FORSAKEN,
        SIL_ACH_THE_GREAT_HUNT,SIL_ACH_A_STAR_AGAINST_THE_BALROG};
    void (*interact[])(void)={tulkas_quest_interaction,aule_quest_interaction,
        mandos_quest_interaction,niena_quest_interaction,orome_quest_interaction,
        varda_quest_interaction};
    for(size_t i=0;i<N_ELEMENTS(ids);i++) {
        ready(41);interact[i]();assert(!earned(ids[i]));
    }
    ready(41);p_ptr->niena_quest=NIENA_QUEST_FAILED;
    niena_quest_interaction();assert(!earned(SIL_ACH_PITY_FOR_THE_FORSAKEN));
    ready(41);p_ptr->varda_quest=VARDA_QUEST_FAILED;
    varda_quest_interaction();assert(!earned(SIL_ACH_A_STAR_AGAINST_THE_BALROG));
    ready(41);p_ptr->varda_quest=VARDA_QUEST_SUCCESS;
    decline_gift=true;varda_quest_interaction();
    assert(p_ptr->varda_quest==VARDA_QUEST_SUCCESS && !earned(SIL_ACH_A_STAR_AGAINST_THE_BALROG));
    decline_gift=false;
    for(size_t i=0;i<N_ELEMENTS(ids);i++) {
        ready(41);
        switch(i) {
            case 0:p_ptr->tulkas_quest=TULKAS_QUEST_COMPLETE;
                p_ptr->tulkas_prize_a_idx=ART_ORCRIST;break;
            case 1:p_ptr->aule_quest=AULE_QUEST_SUCCESS;break;
            case 2:p_ptr->mandos_quest=MANDOS_QUEST_SUCCESS;break;
            case 3:p_ptr->niena_quest=NIENA_QUEST_SUCCESS;
                p_ptr->niena_monsters_seen=10;break;
            case 4:p_ptr->orome_quest=OROME_QUEST_SUCCESS;
                p_ptr->orome_target_type=1;p_ptr->orome_target_count=100;break;
            case 5:p_ptr->varda_quest=VARDA_QUEST_SUCCESS;break;
        }
        // A pending objective/reward never unlocks by merely taking a turn.
        sil_achievements_on_player_turn();assert(!earned(ids[i]));
        interact[i]();
        if(!earned(ids[i]) || events[ids[i]]!=1)
            fprintf(stderr,"Quest %zu: earned=%d events=%d\n",i+1,earned(ids[i]),events[ids[i]]);
        assert(earned(ids[i]) && events[ids[i]]==1);
        ++turn;interact[i]();assert(events[ids[i]]==1);
    }
    assert(sil_achievements_mask(1)==UINT64_C(0x3F)); // Stable quest indexes 64-69.
    assert(sil_achievements_saved());
    puts("PASS six Valar rewards: unfinished/failed/pending/cancelled gifts excluded; successful claims once, including word 2");
}
static void checks(void)
{
    ANGBAND_DIR_APEX=ANGBAND_DIR_USER;
    assert(init_quest_info()==0);sil_achievements_init();
    check_uniques();check_heroes();check_quests();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    for name in ("terminal_extra", "reset_map", "combat_fixture"):
        prefix += fixture_function(name) + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    base = BASE[:BASE.index("static void check_thrall(void)")]
    source = OUT / "check.c"
    source.write_text(prefix + base + TESTS + init + "    checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith("/src/main.c.obj")))
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = env["SDL_AUDIODRIVER"] = "dummy"
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                                  "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("sil_achievements_on_event", "handle_stuff", "update_stuff", "msg_print", "message_flush",
             "get_sdl_config_path", "pause_with_text", "quest_typewriter_menu_pages", "ui_question_ask",
             "get_aim_dir", "show_interaction_skill_roll_animation_actor", "hit_roll", "hit_roll_details",
             "quest_reward_book_choice")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + n for n in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
