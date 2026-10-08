#!/usr/bin/env python3
"""Check production gem command costs with controlled target/effect outcomes."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-items-gem-cancellation'

CHECKS = r'''
#include "init/init2-internal.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int next_key, input_calls, truce_calls;
char __wrap_inkey(void)
{ assert(++input_calls==1); return (char)next_key; }
bool __real_display_unified_identify_menu(bool include_floor, int* item,
    object_type** object);
/* The real no-target scan is used; presentation is supplied at the same
 * target-picker boundary exercised with Escape and confirmation live. */
bool __wrap_display_unified_identify_menu(bool include_floor, int* item,
    object_type** object)
{
    if (object_known_p(&inventory[0]))
        return __real_display_unified_identify_menu(include_floor,item,object);
    assert(++input_calls==1);
    assert(next_key==ESCAPE); return false;
}
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_light_area(int dd, int ds, int rad)
{ (void)dd; (void)ds; (void)rad; return false; }
bool __real_ident_spell(bool include_floor);
/* Successful identification's lore/XP/redraw work requires a running game;
 * this fixture supplies its outcome and checks the real gem command cost. */
bool __wrap_ident_spell(bool include_floor)
{
    if (next_key!='a') return __real_ident_spell(include_floor);
    assert(++input_calls==1);
    k_info[inventory[0].k_idx].aware=true; object_known(&inventory[0]);
    return true;
}
void __wrap_break_truce(bool obvious) { (void)obvious; truce_calls++; }
bool __wrap_tutorial_game_action_allowed(const char *action, const object_type *item)
{ (void)action; (void)item; return true; }
static object_type* prepare_gem(int sval)
{
    object_type incoming;
    int kind=lookup_kind(TV_GEM,sval);
    assert(kind);
    supplies_reset_store();
    object_prep(&incoming,kind); incoming.number=3;
    object_aware(&incoming); object_known(&incoming);
    assert(supplies_absorb_object(&incoming));
    return supplies_entry_at(0);
}
static void use_gem(object_type* gem, int key)
{
    p_ptr->energy_use=0; input_calls=truce_calls=0; next_key=key;
    p_ptr->previous_action[0]=7;
    supplies_begin_action(0);
    do_cmd_use_gem(gem,SUPPLIES_INDEX);
    supplies_end_action();
}
static void check_gem_cancellation(void)
{
    int horn=lookup_kind(TV_HORN,SV_HORN_FORCE);
    assert(horn);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    p_ptr->py=p_ptr->px=2;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    object_type* gem=prepare_gem(SV_GEM_UNDERSTANDING);
    object_prep(&inventory[0],horn); inventory[0].number=1;
    object_aware(&inventory[0]); object_known(&inventory[0]);
    /* All currently held items are identified. */
    use_gem(gem,ESCAPE);
    assert(!p_ptr->energy_use && !input_calls && !truce_calls);
    assert(p_ptr->previous_action[0]==7);
    assert(supplies_entry_units(0)==3 && gem->xtra1==0);
    object_prep(&inventory[0],horn); inventory[0].number=1;
    k_info[horn].aware=false;
    use_gem(gem,ESCAPE);
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7);
    assert(supplies_entry_units(0)==3 && gem->xtra1==0);
    assert(!object_known_p(&inventory[0]));
    use_gem(gem,'a');
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(supplies_entry_units(0)==2 && object_known_p(&inventory[0]));
    assert(supplies_entry_at(0)->xtra1==1);
    object_copy(&o_list[1],supplies_entry_at(0));
    supplies_reset_store();
    o_list[1].iy=p_ptr->py; o_list[1].ix=p_ptr->px;
    o_list[1].next_o_idx=0; o_max=2; cave_o_idx[p_ptr->py][p_ptr->px]=1;
    p_ptr->energy_use=0; truce_calls=input_calls=0; next_key=ESCAPE;
    do_cmd_use_gem(&o_list[1],-1);
    assert(!p_ptr->energy_use && !truce_calls && o_list[1].number==2);
    object_wipe(&o_list[1]); cave_o_idx[p_ptr->py][p_ptr->px]=0;
    gem=prepare_gem(SV_GEM_SANCTITY);
    use_gem(gem,ESCAPE); /* No cursed/jinxed target. */
    assert(!p_ptr->energy_use && !truce_calls && supplies_entry_units(0)==3);
    gem->number=0;
    use_gem(gem,ESCAPE);
    assert(!p_ptr->energy_use && !input_calls && !truce_calls);
    /* A real Light use still pays and consumes one gem, even with no
     * monsters to affect. */
    gem=prepare_gem(SV_GEM_LIGHT);
    use_gem(gem,ESCAPE);
    assert(p_ptr->energy_use==100 && truce_calls==1);
    assert(supplies_entry_units(0)==2);
    puts("Understanding: Supplies/floor cancellation free, confirmed identify costs one turn/gem; Sanctity/empty refusals free, actual Light use paid PASS.");
}
static void check_recharging(void)
{
    object_type before[2];
    int staff=lookup_kind(TV_STAFF,SV_STAFF_IMPRISONMENT);
    assert(staff);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    cave_o_idx[p_ptr->py][p_ptr->px]=0; o_max=1;
    for (int i=0;i<2;i++) {
        object_prep(&inventory[i],staff); inventory[i].number=1;
        inventory[i].pval=8+2*i;
        object_aware(&inventory[i]); object_known(&inventory[i]);
    }
    object_type* gem=prepare_gem(SV_GEM_RECHARGING);
    k_info[gem->k_idx].aware=false; k_info[gem->k_idx].tried=false;
    memcpy(before,inventory,sizeof(before));
    u64b rng=Rand_state_export();
    use_gem(gem,ESCAPE); /* Real Recharge selector, with eligible targets. */
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && supplies_entry_units(0)==3);
    assert(!k_info[gem->k_idx].aware && !k_info[gem->k_idx].tried);
    assert(!memcmp(before,inventory,sizeof(before)) && Rand_state_export()==rng);
    object_aware(gem); object_known(gem);
    use_gem(gem,'a'); /* Real carried recharge: exactly one bundled charge. */
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(inventory[0].pval==10 && inventory[1].pval==10);
    assert(supplies_entry_units(0)==2 && supplies_entry_at(0)->xtra1==1);
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=true;
    use_gem(supplies_entry_at(0),'b');
    assert(inventory[0].pval==10 && inventory[1].pval==14);
    assert(p_ptr->energy_use==100 && supplies_entry_units(0)==1);
    p_ptr->active_ability[S_WIL][WIL_CHANNELING]=false;
    object_copy(&o_list[2],supplies_entry_at(0)); o_list[2].number=3;
    supplies_reset_store();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    object_prep(&o_list[1],staff); o_list[1].number=1; o_list[1].pval=0;
    object_aware(&o_list[1]); object_known(&o_list[1]);
    o_list[1].ident|=IDENT_EMPTY;
    for (int i=1;i<=2;i++) { o_list[i].iy=p_ptr->py; o_list[i].ix=p_ptr->px; }
    o_list[1].next_o_idx=2; o_list[2].next_o_idx=0;
    o_max=3; cave_o_idx[p_ptr->py][p_ptr->px]=1;
    p_ptr->energy_use=0; truce_calls=input_calls=0; next_key=ESCAPE;
    p_ptr->previous_action[0]=7; rng=Rand_state_export();
    do_cmd_use_gem(&o_list[2],-2);
    assert(input_calls==1 && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && o_list[2].number==3);
    assert(o_list[1].pval==0 && (o_list[1].ident&IDENT_EMPTY));
    assert(Rand_state_export()==rng);
    truce_calls=input_calls=0; next_key='-';
    do_cmd_use_gem(&o_list[2],-2); /* Real floor recharge and empty recovery. */
    assert(input_calls==1 && p_ptr->energy_use==100 && truce_calls==1);
    assert(o_list[1].pval==CHANNELING_CHARGE_MULTIPLIER);
    assert(!(o_list[1].ident&IDENT_EMPTY) && o_list[2].number==2);
    object_wipe(&o_list[1]); cave_o_idx[p_ptr->py][p_ptr->px]=2;
    p_ptr->energy_use=0; p_ptr->previous_action[0]=7;
    truce_calls=input_calls=0; rng=Rand_state_export();
    do_cmd_use_gem(&o_list[2],-2); /* Actual no-eligible-Staff scan. */
    assert(!input_calls && !p_ptr->energy_use && !truce_calls);
    assert(p_ptr->previous_action[0]==7 && o_list[2].number==2);
    assert(Rand_state_export()==rng);
    gem=prepare_gem(SV_GEM_WARDING);
    use_gem(gem,ESCAPE); /* Floor occupied by a gem: genuine refusal stays paid. */
    assert(p_ptr->energy_use==100 && truce_calls==1 && supplies_entry_units(0)==3);
    puts("Recharging: real carried/floor target cancel and no-target free; paid charge transfer/Channeling/empty recovery; Warding refusal paid PASS.");
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
                      '    check_gem_cancellation(); check_recharging(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
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
                    *(f'-Wl,--wrap={name}' for name in ('display_unified_identify_menu','message_flush','handle_stuff','inkey',
                        'break_truce','tutorial_game_action_allowed',
                        'light_area','ident_spell')),
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
