#!/usr/bin/env python3
"""Check automatic inscriptions across every real player-held item store."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-items-autoinscriptions'

CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static void check_autoinscriptions(void)
{
    object_type incoming;
    int arrows=lookup_kind(TV_ARROW,1);
    int sword=lookup_kind(TV_SWORD,SV_SHORT_SWORD);
    int bread=lookup_kind(TV_FOOD,SV_FOOD_BREAD);
    assert(arrows && sword && bread);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    supplies_reset_store();
    object_prep(&inventory[0],arrows); inventory[0].number=3;
    object_prep(&incoming,arrows); incoming.number=5;
    assert(player_quiver_absorb_arrow(&incoming)==5);
    object_prep(&o_list[1],arrows); o_list[1].number=16; o_max=2;
    object_prep(&inventory[INVEN_WIELD],sword); inventory[INVEN_WIELD].number=1;
    object_prep(&incoming,sword); incoming.number=1;
    assert(player_carried_extra_load(&incoming));
    object_prep(&incoming,bread); incoming.number=3;
    assert(supplies_absorb_object(&incoming));
    assert(add_autoinscription(arrows,"ready !d"));
    assert(inventory[0].obj_note==quark_add("ready !d"));
    assert(player_quiver_store_entry_at(0)->obj_note==inventory[0].obj_note);
    assert(o_list[1].obj_note==inventory[0].obj_note);
    assert(add_autoinscription(sword,"blade !d"));
    assert(inventory[INVEN_WIELD].obj_note==quark_add("blade !d"));
    assert(player_carried_extra_entry_at(0)->obj_note==inventory[INVEN_WIELD].obj_note);
    assert(add_autoinscription(bread,"food !d"));
    assert(supplies_entry_at(0)->obj_note==quark_add("food !d"));
    /* Updating a rule reaches all stores, while removing it preserves a
     * different manual inscription and unrelated kinds. */
    assert(add_autoinscription(arrows,"new !d"));
    assert(player_quiver_store_entry_at(0)->obj_note==quark_add("new !d"));
    inventory[0].obj_note=quark_add("personal");
    obliterate_autoinscription(arrows);
    assert(!player_quiver_store_entry_at(0)->obj_note && !o_list[1].obj_note);
    assert(inventory[0].obj_note==quark_add("personal"));
    assert(inventory[INVEN_WIELD].obj_note==quark_add("blade !d"));
    obliterate_autoinscription(sword);
    assert(!inventory[INVEN_WIELD].obj_note && !player_carried_extra_entry_at(0)->obj_note);
    obliterate_autoinscription(bread);
    assert(!supplies_entry_at(0)->obj_note);
    assert(player_quiver_arrow_count()==5 && inventory[0].number==3 && o_list[1].number==16);
    assert(supplies_entry_units(0)==3);
    puts("Autoinscriptions: Pack, overflow, Quiver, equipment, Supplies and floor add/update/remove; manual notes and counts preserved PASS.");
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
                      '    check_autoinscriptions(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
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
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
