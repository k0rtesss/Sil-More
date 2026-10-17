#!/usr/bin/env python3
"""Verify packed vault records cannot inherit quest tokens from later templates.

Uses initialized real templates and production content gates, without user saves.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/vault-quest-gating'
CHECKS=r'''
#include "quest/quest-runtime.h"
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
static void checks(void)
{
    /* A packed record has one terminator, followed by other vault records.
     * Neither following NPC token belongs to the six-cell current vault. */
    char records[]="\0......\0L...\0N...\0";
    char* original=v_text;v_text=records;
    vault_type tiny={0};tiny.text=1;tiny.hgt=3;tiny.wid=2;
    assert(!vault_template_has_aule(&tiny)&&!vault_template_has_mandos(&tiny));
    records[6]='L';assert(vault_template_has_aule(&tiny));
    records[6]='N';assert(!vault_template_has_aule(&tiny)&&vault_template_has_mandos(&tiny));
    assert(!vault_template_has_aule(NULL)&&!vault_template_has_mandos(NULL));
    v_text=original;
    int checked=0,aule=-1,mandos=-1;
    for(int i=1;i<z_info->v_max;i++) {
        vault_type* v=&v_info[i];
        if(!v->text||!v->hgt||!v->wid)continue;
        size_t size=(size_t)v->hgt*v->wid;
        assert(strlen(v_text+v->text)==size);
        bool a=memchr(v_text+v->text,'L',size)!=NULL;
        bool m=memchr(v_text+v->text,'N',size)!=NULL;
        assert(vault_template_has_aule(v)==a&&vault_template_has_mandos(v)==m);
        checked++;
        if(!quest_followup_vault(i)&&!vault_template_has_duruin(v)) {
            if(a&&!m)aule=i;
            if(m&&!a)mandos=i;
        }
    }
    printf("Real templates checked=%d Aule=%d Mandos=%d\n",checked,aule,mandos);
    assert(checked>100&&aule>0&&mandos>0);
    for(int id=1;id<=16;id++)quest_set_enabled(id,false);
    /* This real greater vault has no quest giver. Disabling unrelated base
     * quests must not make its mandatory placement impossible at950ft. */
    vault_type* aerie=&v_info[406];
    assert(!memchr(v_text+aerie->text,'L',(size_t)aerie->hgt*aerie->wid));
    assert(!memchr(v_text+aerie->text,'N',(size_t)aerie->hgt*aerie->wid));
    assert(vault_is_valid_for_depth(aerie,19));
    quest_set_enabled(QUEST_ID_NIENA_MORGOTH,true);
    assert(vault_is_valid_for_depth(aerie,19));
    assert(!vault_is_valid_for_depth(&v_info[aule],10));
    quest_set_enabled(QUEST_ID_AULE,true);
    assert(vault_is_valid_for_depth(&v_info[aule],10));
    assert(!vault_is_valid_for_depth(&v_info[mandos],10));
    quest_set_enabled(QUEST_ID_MANDOS,true);
    assert(vault_is_valid_for_depth(&v_info[mandos],10));
    printf("Packed-record isolation, all%d real vault token scans, Ancalagon950ft with all quests off/q11 only, real Aule%d/Mandos%d disabled/enabled gates PASS.\n",checked,aule,mandos);
}
static void generation_check(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(20);
    for(int id=1;id<=16;id++)quest_set_enabled(id,id==QUEST_ID_NIENA_MORGOTH);
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=3;
    p_ptr->max_depth=20;p_ptr->mhp=p_ptr->chp=100;p_ptr->food=PY_FOOD_FULL;
    turn=1234;playerturn=100;min_depth_counter=0;
    for(int y=0;y<88;y++)for(int x=0;x<88;x++)
        cave_set_feat(y,x,y&&x&&y<87&&x<87?FEAT_FLOOR:FEAT_WALL_PERM);
    cave_set_feat(10,10,FEAT_LESS);cave_environment_seed();
    cave_vertical_reset();vertical_vault_reset();Rand_state_init(102512);
    character_generated=true;level_cache_start();
    Uint64 deadline=SDL_GetTicks()+30000;
    while(!level_cache_ready(19)){assert(SDL_GetTicks()<deadline);level_cache_poll();SDL_Delay(1);}
    const level_image* destination=level_cache_floor(19);assert(destination&&destination->depth==19&&destination->vault_name[0]);
    printf("Generated depth19 greater vault: %s\n",destination->vault_name);
    level_cache_clear();
    puts("Real SDL worker generates depth19 from resident20 with onlyq11 enabled (Aule/Mandos disabled) PASS.");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    init+='''assert(!init_quest_info());assert(!init_st_info());assert(!init_partition_info());
    style_info=(style_type*)style_head.info_ptr;style_name=style_head.name_ptr;
    assert(!init_flavor_info());assert(!init_effect_info());assert(!init_skeleton_note_info());
    assert(!init_h_info());assert(!init_n_info());build_randart_tables();flavor_init();drop_system_init();
    checks();generation_check();SDL_Quit();return 0;}'''
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init,encoding='utf-8')
    production=ROOT/'src/level-generation/level-generation-rooms-special.c'
    if args.baseline:
        baseline=subprocess.check_output(['git','show','HEAD:src/level-generation/level-generation-rooms-special.c'],cwd=ROOT,text=True)
        production=OUT/'baseline.c';production.write_text(baseline,encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/level-generation/level-generation-rooms-special.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(production),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:
        result=subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
        print(result.stdout,end='');(OUT/('baseline.log' if args.baseline else 'results.log')).write_text(result.stdout,encoding='utf-8');result.check_returncode()

if __name__=='__main__':main()
