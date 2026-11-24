#!/usr/bin/env python3
"""Exercise discovery and targeting for chests, doors, skeletons and floor traps.

Uses current Windows CMake engine objects and isolated templates/configs.
--baseline restores only the old adjacent-only open picker for verification.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/underfoot-chests'
CHECKS = r'''
#include "spell/spell-projection-internal.h"
#include "ui/question.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int calls,chosen_y,chosen_x,choices;
static bool cancel_choice;
static char question_title[100];
static bool open_for_sound, talk_offered;
static int open_sounds, failed_pick_sounds;
void __real_sound_at(int event, int y, int x);
void __wrap_sound_at(int event, int y, int x)
{
    if (open_for_sound && event == MSG_CHEST_OPEN) open_sounds++;
    if (open_for_sound && event == MSG_LOCKPICK_FAIL) failed_pick_sounds++;
    __real_sound_at(event, y, x);
}
static bool generate_ring;
static bool generated_at_depth[26];
bool __real_do_cmd_open_chest(int y,int x,s16b index);
/* The one-time help normally persists a setting in the live SDL profile. */
cptr __wrap_ui_question_interaction_help_once(void) { return ""; }
bool __wrap_drop_generate_object_profiled_depths(int depth, int penalty_depth,
    drop_quality quality, int type, int bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    (void)penalty_depth; (void)quality; (void)type; (void)bonus; (void)profile;
    assert(object_generation_mode == OB_GEN_MODE_CHEST && allow_artefacts);
    assert(depth > 0 && depth < (int)N_ELEMENTS(generated_at_depth));
    if (generated_at_depth[depth]) return false;
    generated_at_depth[depth] = true;
    if (generate_ring) {
        assert(make_fake_artefact(out, ART_BARAHIR));
        a_info[ART_BARAHIR].cur_num = 1;
    } else {
        object_prep(out, lookup_kind(TV_FOOD, SV_FOOD_BREAD));
    }
    return true;
}
int __wrap_ui_question_ask(cptr title, cptr desc, const ui_question_option* options,
    int count, int y, int x, int initial)
{
    (void)desc; (void)y; (void)x; (void)initial;
    SDL_strlcpy(question_title, title, sizeof(question_title));
    talk_offered = false;
    for (int i = 0; i < count; i++)
        if (streq(options[i].label, "Talk")) talk_offered = true;
    if (open_for_sound) {
        for (int i = 0; i < count; i++) if (options[i].key == 'o') return i;
    }
    return -1;
}
bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    assert(y == 5 && x == 6 && strstr(prompt, "bash"));
    return false;
}
bool __wrap_tutorial_game_action_allowed(const char *action,const object_type *item)
{ (void)action; (void)item; return true; }
bool __wrap_do_cmd_open_chest(int y,int x,s16b index)
{
    assert(index==1); calls++; chosen_y=y; chosen_x=x;
    /* Match the production chest menu's free Escape path. */
    p_ptr->energy_use=0; return false;
}
bool __wrap_get_grid_choice_dir(cptr prompt,const int ys[],const int xs[],
    const int dirs[],int count,int *dir)
{
    bool under=false,east=false;
    assert(strstr(prompt,"Open")); assert(count==2); choices++;
    for (int i=0;i<count;i++) {
        if (dirs[i]==5) { under=true; assert(ys[i]==5 && xs[i]==5); }
        if (dirs[i]==6) { east=true; assert(ys[i]==5 && xs[i]==6); }
    }
    assert(under && east);
    if (cancel_choice) return false;
    *dir=5; return true;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    reset_map(1); p_ptr->py=p_ptr->px=5;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=100;
    for (int y=0;y<20;y++) for (int x=0;x<20;x++)
        cave_info[y][x]=CAVE_MARK|CAVE_GLOW|CAVE_SEEN;
    calls=choices=0; cancel_choice=false; repeat_clear();
    question_title[0]='\0';
    chest_trap_minigame=true; lockpick_minigame=false;
}
static void chest_at(int y,int x,int pval)
{
    object_prep(&o_list[1],lookup_kind(TV_CHEST,2));
    o_list[1].number=1; o_list[1].pval=pval; o_list[1].marked=true;
    o_list[1].iy=y; o_list[1].ix=x; cave_o_idx[y][x]=1; o_max=2;
}
static void check_discovery(void)
{
    prepare(); chest_at(5,6,5);
    o_list[1].marked = false;
    cave_info[5][6] = 0;
    do_cmd_open();
    assert(!calls && !choices && !p_ptr->energy_use);
    character_dungeon = true;
    int command = 0;
    assert(!grid_interact_question(5,6,&command,NULL));
    assert(!command && streq(question_title,"Darkness"));

    cave_feat[5][6] = FEAT_DOOR_HEAD + 1;
    lockpick_minigame = true;
    assert(!grid_interact_question(5,6,&command,NULL));
    assert(!command && streq(question_title,"Darkness"));

    /* Known water can conceal an object that was previously marked. */
    prepare(); chest_at(5,6,5); cave_feat[5][6] = FEAT_WATER;
    assert(!object_is_visible(&o_list[1]));
    p_ptr->command_dir = 6; do_cmd_open();
    assert(!calls && !p_ptr->energy_use);
    assert(!grid_interact_question(5,6,&command,NULL));
    assert(streq(question_title,"Shallow water"));
    o_list[1].tval = TV_SKELETON; o_list[1].sval = SV_SKELETON_ELF;
    assert(!grid_interact_question(5,6,&command,NULL));
    assert(streq(question_title,"Shallow water"));
    do_cmd_alter(); assert(o_list[1].pval == 5);

    o_list[1].tval = TV_CHEST; p_ptr->skill_use[S_PER] = 5;
    assert(object_is_visible(&o_list[1]));
    do_cmd_open(); assert(calls == 1);
    prepare(); chest_at(5,5,5); o_list[1].marked = false;
    p_ptr->blind = true; do_cmd_open(); assert(calls == 1);
    prepare(); chest_at(5,6,5);
    mon_list[1].r_idx = R_IDX_ALERT_HUMAN_THRALL;
    mon_list[1].fy = 5; mon_list[1].fx = 6; mon_list[1].ml = true;
    mon_max = 2; cave_m_idx[5][6] = 1;
    assert(!grid_interact_question(5,6,&command,NULL) && talk_offered);
    puts("Open, Alter and the map interaction use discovered objects; an underfoot chest remains reachable by touch: PASS");
}

static void check_disarm_targets(void)
{
    prepare();
    cave_feat[5][4] = FEAT_TRAP_DART;
    cave_info[5][4] |= CAVE_HIDDEN;
    cave_feat[5][6] = FEAT_TRAP_CALTROPS;
    p_ptr->skill_use[S_PER] = 100;
    do_cmd_disarm();
    assert(!choices && cave_feat[5][6] == FEAT_FLOOR && p_ptr->energy_use == 100);
    assert(cave_feat[5][4] == FEAT_TRAP_DART && (cave_info[5][4] & CAVE_HIDDEN));

    prepare(); chest_trap_minigame = false; chest_at(5,6,5);
    assert(!object_known_p(&o_list[1]) && object_chest_trap_flags(&o_list[1]));
    p_ptr->skill_use[S_PER] = 100;
    do_cmd_disarm();
    assert(p_ptr->energy_use == 100 && object_known_p(&o_list[1]) && o_list[1].pval == 5);
    do_cmd_disarm(); assert(o_list[1].pval == -5);
    puts("Disarm highlights discovered floor traps and allows legacy chest inspection before disarming: PASS");

    prepare(); cave_feat[5][6] = FEAT_DOOR_HEAD + 8;
    do_cmd_open();
    assert(!p_ptr->energy_use && cave_feat[5][6] == FEAT_DOOR_HEAD + 8);
    puts("Declining to bash a stuck door leaves the turn available: PASS");
}

static void check_search_discovery(void)
{
    prepare(); chest_at(5,6,5);
    o_list[2] = o_list[1]; o_list[2].pval = 0; o_list[2].next_o_idx = 1;
    cave_o_idx[5][6] = 2; o_max = 3;
    do_cmd_open();
    assert(calls == 1 && chosen_y == 5 && chosen_x == 6);

    prepare();
    chest_trap_minigame = false;
    chest_at(5,6,5);
    o_list[1].marked = false;
    cave_feat[5][6] = FEAT_DEEP_WATER;
    cave_info[5][6] = CAVE_SEEN;
    p_ptr->skill_use[S_PER] = 4;
    search();
    assert(!o_list[1].marked && !object_known_p(&o_list[1]));
    p_ptr->skill_use[S_PER] = 100;
    p_ptr->blind = 2;
    search();
    assert(!o_list[1].marked && !object_known_p(&o_list[1]));
    p_ptr->blind = 0;
    search();
    assert(o_list[1].marked && object_known_p(&o_list[1]));

    prepare(); chest_trap_minigame = false; chest_at(5,5,5);
    cave_feat[5][5] = FEAT_DEEP_WATER;
    p_ptr->skill_use[S_PER] = 100;
    p_ptr->blind = 2;
    perceive();
    assert(object_known_p(&o_list[1]));

    prepare(); chest_trap_minigame = false; chest_at(5,6,5);
    o_list[1].marked = false;
    cave_info[5][6] = 0;
    p_ptr->skill_use[S_PER] = 100;
    perceive(); /* Natural perception can discover an adjacent mechanism in the dark. */
    assert(object_known_p(&o_list[1]));
    do_cmd_open();
    assert(calls == 1 && chosen_y == 5 && chosen_x == 6);

    /* A new pile can sit on previously remembered dry floor. */
    prepare(); chest_at(5,6,5);
    o_list[1].marked = false;
    o_list[2] = o_list[1]; o_list[1].next_o_idx = 2; o_max = 3;
    cave_info[5][6] = CAVE_MARK;
    search();
    assert(o_list[1].marked && o_list[2].marked);

    prepare(); chest_trap_minigame = false; chest_at(5,6,5);
    cave_feat[5][6] = FEAT_TRAP_WEB;
    p_ptr->skill_use[S_PER] = 100;
    bool saved_generated = character_generated;
    character_generated = true;
    u32b start = message_sequence(0);
    search();
    assert(object_known_p(&o_list[1]));
    for (int i = 0; i < message_num() && message_sequence(i) > start; i++)
        assert(!strstr(message_str(i), "You have found a trap."));
    character_generated = saved_generated;
    puts("Search finds reachable items on known ground and respects underwater visibility and underfoot touch.");
}

static void prepare_chest_loot(int ground, bool artefact_loot)
{
    prepare();
    character_dungeon = true;
    chest_trap_minigame = false;
    for (int y=0; y<20; y++) for (int x=0; x<20; x++) {
        cave_feat[y][x] = FEAT_WALL_EXTRA;
        cave_info[y][x] = CAVE_MARK | CAVE_WALL;
    }
    cave_set_feat(5,5,ground);
    cave_m_idx[5][5] = -1;
    generate_ring = artefact_loot;
    object_prep(&o_list[1], lookup_kind(TV_CHEST,
        artefact_loot ? SV_CHEST_SMALL_STEEL : SV_CHEST_SMALL_WOODEN));
    o_list[1].pval = -5; o_list[1].marked = true;
    o_list[1].iy = o_list[1].ix = 5;
    cave_o_idx[5][5] = 1; o_max = 2; o_cnt = 1;
    memset(generated_at_depth, 0, sizeof(generated_at_depth));
    a_info[ART_BARAHIR].cur_num = 0;
}

static void check_chest_loot(void)
{
    /* An artefact can replace its own opened shell when there is no other
     * landing. Recycling the shell's object index must preserve its power. */
    prepare_chest_loot(FEAT_FLOOR, true);
    int old_limit = z_info->o_max;
    z_info->o_max = 2;
    __real_do_cmd_open_chest(5,5,1);
    z_info->o_max = old_limit;
    assert(o_cnt == 1 && cave_o_idx[5][5] == 1 && o_list[1].name1 == ART_BARAHIR);
    assert(o_list[1].pval == a_info[ART_BARAHIR].pval && o_list[1].pval > 0);

    prepare_chest_loot(FEAT_WATER, true);
    __real_do_cmd_open_chest(5,5,1);
    assert(!o_list[1].pval && o_cnt == 1 && !a_info[ART_BARAHIR].cur_num);

    /* A destroyed chest frees the only landing for its surviving contents.
     * The second shell exercises older floor piles processed by project_o. */
    prepare_chest_loot(FEAT_FLOOR, false);
    object_copy(&o_list[2], &o_list[1]); o_list[2].pval = -6;
    o_list[1].next_o_idx = 2; o_cnt = 2; o_max = 3;
    assert(project_o(-1,5,5,0,0,0,GF_ACID));
    assert(generated_at_depth[5] && generated_at_depth[6]);
    int idx = cave_o_idx[5][5];
    assert(o_cnt == 1 && idx && o_list[idx].tval == TV_FOOD && o_list[idx].number == 2);
    puts("Chest loot: recycled artefact retains its power, unplaced artefact stays eligible, destroyed shells release their floor space: PASS");
}

static void check_lock_sound(void)
{
    for (int minigame = 0; minigame < 2; minigame++) {
        prepare(); chest_at(5,6,4);
        chest_trap_minigame = minigame;
        generated_at_depth[4] = true;
        p_ptr->skill_use[S_PER] = 100;
        open_sounds = failed_pick_sounds = 0; open_for_sound = true;
        __real_do_cmd_open_chest(5,6,1);
        open_for_sound = false;
        assert(!o_list[1].pval && open_sounds == 1 && !failed_pick_sounds);
    }
    puts("Successful lockpicking plays the chest opening sound in both modes: PASS");
}
static void checks(void)
{
    prepare(); chest_at(5,5,5); do_cmd_open();
    assert(calls==1 && chosen_y==5 && chosen_x==5 && !p_ptr->energy_use);
    prepare(); chest_at(5,6,5); do_cmd_open();
    assert(calls==1 && chosen_y==5 && chosen_x==6 && !p_ptr->energy_use);
    prepare(); cave_feat[5][6]=FEAT_DOOR_HEAD; do_cmd_open();
    assert(!calls && cave_feat[5][6]==FEAT_OPEN && p_ptr->energy_use==100);
    prepare(); chest_at(5,5,5); cave_feat[5][6]=FEAT_DOOR_HEAD;
    cancel_choice=true; do_cmd_open();
    assert(choices==1 && !calls && !p_ptr->energy_use && cave_feat[5][6]==FEAT_DOOR_HEAD);
    prepare(); chest_at(5,5,5); cave_feat[5][6]=FEAT_DOOR_HEAD; do_cmd_open();
    assert(choices==1 && calls==1 && chosen_y==5 && chosen_x==5);
    prepare(); chest_at(5,5,0); do_cmd_open();
    assert(!calls && !p_ptr->energy_use);
    prepare(); do_cmd_open(); assert(!calls && !p_ptr->energy_use);
    puts("Open targets: underfoot/adjacent chests, adjacent door, mixed selection, free cancellation and empty chest/grid PASS.");
    check_discovery(); check_disarm_targets(); check_search_discovery(); check_chest_loot(); check_lock_sound();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init + '    assert(init_flavor_info()==0); flavor_init();\n'
                      '    checks(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded = ['/src/main.c.obj']
    extra = []
    if args.baseline:
        text = (ROOT / 'src/cmd/world/cmd-interact.c').read_text(encoding='utf-8')
        old = 'get_interact_dir("Open what?", grid_is_open_target, true,'
        assert text.count(old) == 1
        baseline = OUT / 'cmd-interact-baseline.c'
        baseline.write_text(text.replace(old, old.replace('true,','false,')), encoding='utf-8')
        extra.append(str(baseline)); excluded.append('/src/cmd/world/cmd-interact.c.obj')
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith(tuple(excluded))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), *extra, '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *(f'-Wl,--wrap={name}' for name in ('tutorial_game_action_allowed',
                        'do_cmd_open_chest','get_grid_choice_dir','ui_question_ask','get_check_near',
                        'drop_generate_object_profiled_depths','sound_at','ui_question_interaction_help_once')), '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
