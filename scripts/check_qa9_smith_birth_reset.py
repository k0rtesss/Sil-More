#!/usr/bin/env python3
"""Check real birth/reset clears pending Alloy work and named artefact drafts.

The live negative case is death -> new hero -> save before opening Smithing.
This fixture exercises that public reset boundary and the real Smithing browser.
--baseline compiles committed player_wipe and must fail the inherited-work check.
--draft-baseline omits only the two draft wipes from the current reset API.
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
OUT = ROOT / 'scripts/output/qa9-smith-birth-reset'

CHECKS = r'''
#include "init/init2-internal.h"
#undef assert
#define assert(e) do { if (!(e)) { fprintf(stderr,"Assertion failed: %s line %d\n",#e,__LINE__); fflush(stderr); _Exit(1); } } while (0)
extern object_type* smith2_o_ptr;
extern object_type* smith3_o_ptr;
extern bool enchant_then_numbers;
extern int mithril_carried(void),star_iron_carried(void);
static int inputs;
static void stage_reserved_drafts(void)
{
    assert(z_info->art_self_made_max-3 >= z_info->art_rand_max);
    for (int n=1;n<=2;n++) {
        artefact_type* draft=&a_info[z_info->art_self_made_max-n];
        memset(draft,0,sizeof(*draft));
        SDL_strlcpy(draft->name,"Previous hero unfinished Witness",sizeof(draft->name));
        draft->flags1=TR1_SHARPNESS;
        draft->tval=n==1 ? TV_SWORD : 0;
        draft->sval=n==1 ? 10 : 0;
        draft->dd=1; draft->ds=7; draft->evn=1;
        draft->weight=20; draft->level=24; draft->cur_num=1;
    }
}
static void assert_no_reserved_drafts(void)
{
    artefact_type empty={0};
    for (int n=1;n<=2;n++)
        assert(!memcmp(&a_info[z_info->art_self_made_max-n],&empty,sizeof(empty)));
}
char __wrap_inkey(void)
{ assert(++inputs==1); return ESCAPE; }
void __wrap_message_flush(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
void __wrap_handle_stuff(void)
{ p_ptr->update=p_ptr->redraw=p_ptr->window=0; }
static void stage_pending_work(void)
{
    const byte alloy[SMITHING_ALLOY_STATE_BYTES]={1,0,0,1,0};
    byte saved[SMITHING_ALLOY_STATE_BYTES];
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    supplies_reset_store();
    p_ptr->prace=0; p_ptr->pcharacter=1;
    p_ptr->py=p_ptr->px=5; p_ptr->depth=3; p_ptr->playing=true;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=100; p_ptr->skill_use[S_SMT]=35;
    p_ptr->smithing_leftover=19;
    cave_feat[5][5]=FEAT_FLOOR; cave_o_idx[5][5]=0;
    object_prep(smith_o_ptr,lookup_kind(TV_MAIL,4));
    smith_o_ptr->number=1; smith_o_ptr->weight=270;
    smith_o_ptr->att=-1; smith_o_ptr->evn=-2;
    smith_o_ptr->obj_note=quark_add("previous hero pending Mithril Mail");
    object_known(smith_o_ptr);
    /* This is the actual v4 reader contract: bonuses are already on the
     * restored blueprint; the API restores accounting without applying twice. */
    smithing_alloy_load_state(alloy);
    smithing_alloy_save_state(saved);
    assert(!memcmp(saved,alloy,sizeof(saved)));
    character_icky=0; character_generated=false;
    inputs=0;
    do_cmd_smithing_screen(); /* Real interrupted-work entry and Escape. */
    assert(inputs==1 && p_ptr->smithing_leftover==19);
    assert(smith2_o_ptr->k_idx==smith_o_ptr->k_idx);
    object_copy(smith3_o_ptr,smith_o_ptr);
    enchant_then_numbers=true;
    stage_reserved_drafts();
}
static void assert_reset(void)
{
    object_type empty;
    const byte zero[SMITHING_ALLOY_STATE_BYTES]={0};
    byte saved[SMITHING_ALLOY_STATE_BYTES];
    object_wipe(&empty);
    assert(!memcmp(smith_o_ptr,&empty,sizeof(empty)));
    assert(!memcmp(smith2_o_ptr,&empty,sizeof(empty)));
    assert(!memcmp(smith3_o_ptr,&empty,sizeof(empty)));
    assert(!enchant_then_numbers);
    assert_no_reserved_drafts();
    smithing_alloy_save_state(saved);
    assert(!memcmp(saved,zero,sizeof(saved)));
    /* Exporting an empty blueprint alone could hide stale private metadata.
     * A new neutral blueprint must also have no inherited material accounting. */
    object_prep(smith_o_ptr,lookup_kind(TV_MAIL,4));
    smithing_alloy_save_state(saved);
    assert(!memcmp(saved,zero,sizeof(saved)));
    smithing_reset_work();
}
static void check_birth(void)
{
    assert(z_info->c_max>1);
    for (int dead=0;dead<2;dead++) {
        stage_pending_work();
        character_loaded_dead=dead;
        player_wipe();
        /* Inspect before opening Smithing: that browser clears empty-work
         * blueprints and would otherwise mask a birth-reset omission. */
        assert(!p_ptr->smithing && !p_ptr->smithing_leftover);
        assert(!mithril_carried() && !star_iron_carried());
        if (dead) assert(p_ptr->pcharacter==1);
        assert_reset();
    }
    character_loaded_dead=false;
    stage_pending_work();
    /* max-3 remains a legitimate completed-artefact slot. The work reset
     * itself must preserve its complete definition, unlike new-hero birth. */
    artefact_type* completed=&a_info[z_info->art_self_made_max-3];
    memset(completed,0,sizeof(*completed));
    SDL_strlcpy(completed->name,"Completed hero keeps this",sizeof(completed->name));
    completed->tval=TV_SWORD; completed->sval=10;
    completed->flags1=TR1_SHARPNESS; completed->cur_num=1;
    completed->guid.hi=0x12345678; completed->guid.lo=0x87654321;
    artefact_type retained=*completed;
    smithing_reset_work(); assert_reset();
    assert(!memcmp(completed,&retained,sizeof(retained)));
    smithing_reset_work(); assert_reset(); /* Safe when already empty. */
    assert(!memcmp(completed,&retained,sizeof(retained)));
    puts("Smithing birth reset: fresh/dead-save player_wipe clears blueprints, Alloy accounting, named reserved drafts and UI flag; reset preserves completed artefact definitions and is idempotent PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    negatives = parser.add_mutually_exclusive_group()
    negatives.add_argument('--baseline', action='store_true')
    negatives.add_argument('--draft-baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
        '    assert(init_flavor_info()==0); flavor_init();\n'
        '    check_birth(); SDL_Quit(); return 0;\n}\n')
    birth = None
    smith = None
    if args.baseline:
        birth = OUT / 'birth-baseline.c'
        birth.write_bytes(subprocess.check_output(
            ['git','show','HEAD:src/birth/birth-setup.c'], cwd=ROOT))
    if args.draft_baseline:
        smith = OUT / 'smith-draft-baseline.c'
        current = (ROOT/'src/cmd/ui/cmd-ui-smithing.c').read_text()
        start = current.index('    if (a_info && z_info', current.index('void smithing_reset_work(void)'))
        end = current.index('    memset(&smithing_cost', start)
        omitted = current[start:end]
        assert 'artefact_wipe(i)' in omitted and 'art_self_made_max - 2' in omitted
        smith.write_text(current[:start]+current[end:])
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects
        if not p.endswith('/src/main.c.obj') and
        (birth is None or not p.endswith('/src/birth/birth-setup.c.obj')) and
        (smith is None or not p.endswith('/src/cmd/ui/cmd-ui-smithing.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in
        ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),
        *([str(birth)] if birth else []),*([str(smith)] if smith else []),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *('-Wl,--wrap='+name for name in ('inkey','message_flush','msg_print','handle_stuff')),
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,
            env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()
