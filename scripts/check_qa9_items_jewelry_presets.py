#!/usr/bin/env python3
"""Check jewelry identity and curse-blocked production preset application."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-items-jewelry-presets'

CHECKS = r'''
#include "ui/question.h"
#include "init/init2-internal.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int warnings;
bool __wrap_tutorial_game_action_allowed(const char *action, const object_type *item)
{ (void)action; (void)item; return true; }
int __wrap_ui_question_ask(cptr title, cptr desc,
    const ui_question_option* options, int count, int escape_choice,
    int outside_choice, int default_choice)
{
    (void)desc; (void)options; (void)escape_choice; (void)outside_choice;
    assert(streq(title,"Known cursed item") && count==2 && default_choice==1);
    warnings++; return 1;
}
static void prep(object_type* object, int tval, int sval)
{
    int kind=lookup_kind(tval,sval); assert(kind);
    object_prep(object,kind); object->number=1;
    k_info[kind].aware=true; object_known(object);
}
static void check_presets(void)
{
    object_type base, other;
    object_type before[INVEN_TOTAL];
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    supplies_reset_store(); jewelry_presets_reset();
    prep(&base,TV_RING,SV_RING_COWARDICE);
    base.ident&=~IDENT_CURSED;
    object_copy(&other,&base); other.ident|=IDENT_CURSED;
    assert(!jewelry_preset_objects_match(&base,&other));
    other.ident=base.ident|IDENT_SENSE;
    other.obj_note=quark_add("different plain note");
    assert(jewelry_preset_objects_match(&base,&other));
    other.pval++; assert(!jewelry_preset_objects_match(&base,&other));
    object_copy(&other,&base); other.evn++;
    assert(!jewelry_preset_objects_match(&base,&other));
    object_copy(&other,&base); other.skill_bonus[S_PER]++;
    assert(!jewelry_preset_objects_match(&base,&other));
    object_copy(&inventory[INVEN_LEFT],&base);
    prep(&inventory[INVEN_RIGHT],TV_RING,SV_RING_SECRETS);
    prep(&inventory[INVEN_NECK],TV_AMULET,SV_AMULET_REGENERATION);
    assert(jewelry_preset_store_current(0));
    object_copy(&inventory[0],&inventory[INVEN_LEFT]);
    object_copy(&inventory[1],&inventory[INVEN_RIGHT]);
    object_copy(&inventory[2],&inventory[INVEN_NECK]);
    inventory[INVEN_LEFT].ident|=IDENT_CURSED;
    prep(&inventory[INVEN_RIGHT],TV_RING,SV_RING_FROST);
    prep(&inventory[INVEN_NECK],TV_AMULET,SV_AMULET_CON);
    assert(!jewelry_preset_is_equipped(0));
    memcpy(before,inventory,sizeof(before));
    p_ptr->energy_use=0; warnings=0;
    assert(!do_cmd_jewelry_preset_apply(0));
    assert(!p_ptr->energy_use && !warnings);
    assert(!memcmp(before,inventory,sizeof(before)));
    /* The reverse transition must reach the existing known-curse warning
     * instead of silently considering the uncursed copy settled. */
    object_copy(&inventory[INVEN_LEFT],&base);
    inventory[INVEN_LEFT].ident|=IDENT_CURSED;
    prep(&inventory[INVEN_RIGHT],TV_RING,SV_RING_SECRETS);
    prep(&inventory[INVEN_NECK],TV_AMULET,SV_AMULET_REGENERATION);
    assert(jewelry_preset_store_current(1));
    object_copy(&inventory[0],&inventory[INVEN_LEFT]);
    inventory[INVEN_LEFT].ident&=~IDENT_CURSED;
    memcpy(before,inventory,sizeof(before));
    p_ptr->energy_use=0; warnings=0;
    assert(!do_cmd_jewelry_preset_apply(1));
    assert(warnings==1 && !p_ptr->energy_use);
    assert(!memcmp(before,inventory,sizeof(before)));
    puts("Jewelry presets: curse/modifier identity, plain notes interchangeable, worn curse preflight blocks all three changes, reverse curse-warning cancel free and atomic PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    assert(init_flavor_info()==0); flavor_init();\n'
                      '    check_presets(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith('/src/main.c.obj')), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    '-Wl,--wrap=tutorial_game_action_allowed', '-Wl,--wrap=ui_question_ask',
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
