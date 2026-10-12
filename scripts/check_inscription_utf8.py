#!/usr/bin/env python3
"""Check real compact object names and complete Details inscriptions at UTF-8 cuts."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/inscription-utf8'
CHECKS=r'''
#include "support/utf8.h"
void fixture_details_head(const object_type* item);
static char details[16384];
static void collect_text(byte color,cptr text)
{ (void)color;SDL_strlcat(details,text,sizeof(details)); }
static bool valid_utf8(cptr text)
{
    while(*text) {
        int size=utf8_sequence_len(text);
        if(size<1 || ((unsigned char)*text>=0x80 && size==1))return false;
        text+=size;
    }
    return true;
}
static void check_note(object_type* item,cptr note)
{
    assert(strlen(note)<=79&&valid_utf8(note));
    item->obj_note=quark_add(note);object_type original;object_copy(&original,item);
    char compact[256];object_desc(compact,sizeof(compact),item,true,3);
    assert(strlen(compact)<=76);
    for(int max=1;max<=200;max++) {
        char name[256];memset(name,0x7f,sizeof(name));
        object_desc(name,max,item,true,3);
        if(!valid_utf8(name))fprintf(stderr,"Invalid compact description at capacity%d note%s\n",max,note);
        assert(valid_utf8(name)&&strlen(name)<(size_t)max);
        assert(!memcmp(&original,item,sizeof(original))&&!strcmp(quark_str(item->obj_note),note));
    }
    details[0]=0;text_out_hook=collect_text;fixture_details_head(item);text_out_hook=text_out_to_screen;
    assert(valid_utf8(details));
    assert(strstr(details,"Inscription:")&&strstr(details,note));
    assert(!memcmp(&original,item,sizeof(original))&&!strcmp(quark_str(item->obj_note),note));
}
static void checks(void)
{
    assert(quarks_init()==0);
    assert(init_flavor_info()==0);flavor_init();
    object_type staff;object_prep(&staff,lookup_kind(TV_STAFF,SV_STAFF_IMPRISONMENT));
    staff.number=1;staff.pval=4*CHANNELING_CHARGE_MULTIPLIER;object_aware(&staff);object_known(&staff);
    char base[256];object_desc(base,sizeof(base),&staff,true,3);
    int available=75-(int)strlen(base)-2;assert(available>8&&available<70);
    check_note(&staff,"keep");
    char short_name[256];object_desc(short_name,sizeof(short_name),&staff,true,3);
    char expected[256];strnfmt(expected,sizeof(expected),"%s {keep}",base);assert(!strcmp(short_name,expected));
    char note[80];memset(note,'a',79);note[79]=0;check_note(&staff,note);
    const char* glyphs[]={"\xc3\xa9","\xe4\xb8\xad","\xf0\x9f\x98\x80"};
    for(int g=0;g<3;g++)for(int shift=0;shift<=g+2;shift++) {
        int size=g+2;int prefix=available-size+shift;
        memset(note,'a',79);memcpy(note+prefix,glyphs[g],size);note[79]=0;
        check_note(&staff,note);
    }
    cptr authored="M\xc3\xa9moire: F\xc3\xab" "anor, for\xc3\xaat, \xc3\xa9p\xc3\xa9" "e; garder ce b\xc3\xa2ton pour la porte du Valar. fin";
    assert(strlen(authored)==79);check_note(&staff,authored);
    /* The unknown floor path has its own bounded suffix concatenation. */
    object_type floor;object_prep(&floor,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    floor.number=1;floor.obj_note=quark_add(authored);floor.ident=0;
    char untouched='x';object_desc_floor(&untouched,0,&floor,true,3);assert(untouched=='x');
    object_desc_floor(NULL,0,&floor,true,3);
    for(int max=1;max<=200;max++) {
        char name[256];object_desc_floor(name,max,&floor,true,3);
        assert(valid_utf8(name)&&strlen(name)<(size_t)max);
    }
    puts("Real object descriptions: ASCII and 2/3/4-byte boundaries valid, compact budget retained, full 79-byte Details note and object/quark identity preserved PASS");
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'checks(); SDL_Quit(); return 0;\n}\n')
    desc=(ROOT/'src/object/object-desc.c').read_text()
    if args.baseline:
        old='t = object_desc_append_compact_inscription(t, b + 75, u);'
        assert desc.count(old)==1
        desc=desc.replace(old,'while ((t < b + 75) && *u) object_desc_chr_macro(t, *u++);')
    desc_source=OUT/'desc.c';desc_source.write_text(desc)
    info=(ROOT/'src/object/object-info.c').read_text()
    info_source=OUT/'info.c';info_source.write_text(info+'\nvoid fixture_details_head(const object_type* item) { (void)screen_out_head(item); }\n')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/object/object-desc.c.obj','/src/object/object-info.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(desc_source),str(info_source),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__=='__main__':main()
