#!/usr/bin/env python3
"""Run the actual Wizard editor dispatch for explicit water/ice control chords.
Printable Staff/Ring/Skeleton glyphs and Space/Tab behavior remain available.
Independent --water-baseline/--ice-baseline disable only the chosen new chord.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_wizard_targeting import CHECKS as EDITOR_CHECKS

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/qa9-wizard-terrain-shortcuts'
EXTRA=r'''
static object_type* edited_object(void)
{
    assert(cave_o_idx[5][4]>0 && o_cnt==1);
    return &o_list[cave_o_idx[5][4]];
}
static void check_printable_objects(void)
{
    const char staff[]={'4','_',ESCAPE};
    prepare_editor("Printable underscore still creates a Staff",staff,sizeof(staff));
    run_editor();
    assert(edited_object()->tval==TV_STAFF && cave_feat[5][4]==FEAT_FLOOR);
    int first_staff=edited_object()->k_idx;
    const char cycle[]={'4','_',' ',ESCAPE};
    prepare_editor("Space still cycles Staff kinds",cycle,sizeof(cycle));
    run_editor();
    assert(edited_object()->tval==TV_STAFF && edited_object()->k_idx!=first_staff);
    int next_staff=edited_object()->k_idx;
    const char reroll[]={'4','_',' ','\t',ESCAPE};
    prepare_editor("Tab still rerolls the selected Staff kind",reroll,sizeof(reroll));
    run_editor();
    assert(edited_object()->tval==TV_STAFF && edited_object()->k_idx==next_staff);
    const char ring[]={'4','=',ESCAPE};
    prepare_editor("Printable equals still creates a Ring",ring,sizeof(ring));
    run_editor();
    assert(edited_object()->tval==TV_RING && cave_feat[5][4]==FEAT_FLOOR);
    const char skeleton[]={'4','~',ESCAPE};
    prepare_editor("Printable tilde still creates its authored Skeleton",skeleton,sizeof(skeleton));
    run_editor();
    assert(edited_object()->tval==TV_SKELETON && cave_feat[5][4]==FEAT_FLOOR);
    assert(!bells && !deleted && !placed && !mon_cnt);
}
static void check_terrain_chords(void)
{
    const char water[]={'4',KTRL('W'),ESCAPE};
    prepare_editor("Ctrl+W creates shallow water without an object",water,sizeof(water));
    run_editor();
    assert(cave_feat[5][4]==FEAT_WATER && cave_feat[5][5]==FEAT_FLOOR);
    assert(!o_cnt && !mon_cnt && !bells);
    const char ice[]={'4',KTRL('F'),ESCAPE};
    prepare_editor("Ctrl+F creates solid ice without a Ring",ice,sizeof(ice));
    run_editor();
    assert(cave_feat[5][4]==FEAT_ICE && cave_feat[5][5]==FEAT_FLOOR);
    assert(!o_cnt && !mon_cnt && !bells);
    const char under_player[]={KTRL('W'),KTRL('F'),ESCAPE};
    prepare_editor("Both chords preserve the player's negative occupancy",under_player,sizeof(under_player));
    run_editor();
    assert(cave_feat[5][5]==FEAT_ICE && cave_m_idx[5][5]==-1);
    assert(!o_cnt && !mon_cnt && !bells);
    const char water_cycle[]={'4',KTRL('W'),' ','\t',ESCAPE};
    prepare_editor("Space cycles water's terrain glyph and Tab leaves terrain intact",water_cycle,sizeof(water_cycle));
    run_editor();
    assert(cave_feat[5][4]==FEAT_LAVA && !o_cnt && !mon_cnt && !bells);
    const char ice_cycle[]={'4',KTRL('F'),' ','\t',ESCAPE};
    prepare_editor("Space retains the ice/bridge glyph cycle",ice_cycle,sizeof(ice_cycle));
    run_editor();
    assert(cave_feat[5][4]==FEAT_BRIDGE_WATER_H && !o_cnt && !mon_cnt && !bells);
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group()
    group.add_argument('--water-baseline',action='store_true')
    group.add_argument('--ice-baseline',action='store_true')
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    prefix+='cptr __wrap_get_sdl_config_path(void){return "fixture-sdl.json";}\n'
    checks=EDITOR_CHECKS.replace("if (key == '`') {", "if (key == '`' || key == KTRL('W') || key == KTRL('F')) {")
    checks=checks.replace("assert(Term_keypress('`') == 0);", "char literal=key; assert(Term_keypress(literal) == 0);",1)
    checks=checks.replace("assert(key == '`');", "assert(key == literal);",1)
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    init+='    assert(init_flavor_info()==0); flavor_init();\n'
    init+='    check_printable_objects(); check_numeric_movement(); check_typed_movement();\n'
    init+='    check_lava_shortcut(); check_terrain_chords();\n'
    init+='    puts("PASS: actual Wizard editor water/ice chords, raw input scope, printable object glyphs, movement and terrain/object cycles."); SDL_Quit();return 0;\n}\n'
    (OUT/'check.c').write_text(prefix+checks+EXTRA+init)
    targeting=(ROOT/'src/ui/targeting/targeting.c').read_text()
    if args.water_baseline or args.ice_baseline:
        chord='W' if args.water_baseline else 'F'
        branch=" || query == KTRL('"+chord+"')"
        assert targeting.count(branch)==1
        targeting=targeting.replace(branch,'',1)
    (OUT/'targeting.c').write_text(targeting)
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/ui/targeting/targeting.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wrapped=('inkey_movement_context','bell','delete_monster_idx','place_monster_one','get_sdl_config_path')
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'targeting.c'),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+p for p in wrapped),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=15)


if __name__=='__main__':main()
