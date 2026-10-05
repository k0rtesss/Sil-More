#!/usr/bin/env python3
"""Exercise production wizard Gem menus and numeric value entry.

Requires current Windows CMake objects. Uses isolated template/config data.
--baseline removes only the new Gem category to prove the reachability check.
--numeric-baseline restores only the former signed-minus behavior.
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
OUT = ROOT / 'scripts/output/wizard-gems'

CHECKS = r'''
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static char desired_name[80];
static int questions;
static cptr numeric_keys;
char __wrap_inkey(void)
{
    assert(numeric_keys && *numeric_keys);
    return *numeric_keys++;
}
int __wrap_ui_question_ask(cptr title,cptr desc,const ui_question_option *options,
    int count,int anchor_y,int anchor_x,int default_index)
{
    (void)desc; (void)anchor_y; (void)anchor_x; (void)default_index;
    assert(++questions<=4);
    if (streq(title,"Create Object Type")) {
        int next=-1;
        for (int i=0;i<count;i++) {
            if (streq(options[i].label,"Gem")) return i;
            if (options[i].key=='>') next=i;
        }
        assert(next>=0);
        return next;
    }
    assert(streq(title,"Create Gem"));
    for (int i=0;i<count;i++)
        if (streq(options[i].label,desired_name)) return i;
    assert(false); return -1;
}
static void check_gems(void)
{
    int checked=0;
    bool recharging=false;
    for (int kind=1;kind<z_info->k_max;kind++) {
        if (k_info[kind].tval!=TV_GEM || (k_info[kind].flags3&TR3_INSTA_ART))
            continue;
        questions=0;
        strip_name(desired_name,kind);
        assert(wiz_create_itemtype()==kind);
        assert(questions>=3);
        if (k_info[kind].sval==SV_GEM_RECHARGING) recharging=true;
        checked++;
    }
    assert(checked>0 && recharging);
    printf("Wizard Gem creation: all %d shipped Gem kinds reachable through production type/kind pages PASS.\n",checked);
}
static void number(cptr keys,long initial,long minimum,long maximum,long expected)
{
    long value=-999;
    numeric_keys=keys;
    bool old_hide=hide_cursor;
    bool result=debug_overlay_get_long("Numeric fixture",NULL,initial,
        minimum,maximum,&value);
    assert(numeric_keys && !*numeric_keys);
    assert(hide_cursor==old_hide);
    assert(result && value==expected);
}
static void check_numbers(void)
{
    number("--------------\r",20,-9,20,6);
    number("-5\r",20,-9,20,-5);
    number("_\r",20,-9,20,19);
    number("+++\r",6,-9,20,9);
    number("--r\r",20,-9,20,20);
    number("---\r",0,0,20,0);
    number("99\r",6,-9,20,20);
    number("-99\r",6,-9,20,-9);
    number("-52\b\r",6,-99,99,-5);
    number("123\r",6,-999,999,123);
    long value=123;
    numeric_keys="-\033";
    bool old_hide=hide_cursor;
    assert(!debug_overlay_get_long("Cancel",NULL,20,-9,20,&value));
    assert(value==123 && !*numeric_keys && hide_cursor==old_hide);
    puts("Wizard numeric loop: repeated +/- adjustments, signed typing, Reset, bounds, Backspace and cancellation PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--numeric-baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    wizard = (ROOT / 'src/wizard2.c').read_text(encoding='utf-8')
    # The initialized engine fixture already includes externs.h via its
    # generation header; that declaration file has no include guard.
    wizard = wizard.replace('#include "externs.h"\n', '', 1)
    if args.baseline:
        assert '{ TV_GEM, "Gem" }, ' in wizard
        wizard = wizard.replace('{ TV_GEM, "Gem" }, ', '', 1)
    if args.numeric_baseline:
        start = wizard.index("        case '-':\n        case '_':")
        end = wizard.index("        case '\\b':", start)
        wizard = wizard[:start] + r'''        case '-':
        case '_':
            if (min < 0 && entry_len == 0)
            {
                entry[entry_len++] = '-';
                entry[entry_len] = '\0';
                current = 0;
            }
            else
            {
                current = debug_overlay_clamp_long(current - 1, min, max);
                entry_len = 0;
                entry[0] = '\0';
            }
            break;

''' + wizard[end:]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + wizard + '\n' + CHECKS + init +
                      '    check_gems(); check_numbers(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith(('/src/main.c.obj','/src/wizard2.c.obj'))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=ui_question_ask','-Wl,--wrap=inkey',
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
