#!/usr/bin/env python3
"""Exercise production binary runs.db upserts without player files.

Checks alive insert/save/death timestamps, stable IDs, detail resizing and
zero-created-time fallback. --baseline removes only creation-time preservation.
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
OUT = ROOT / 'scripts/output/run-history-timestamps'
CHECKS = r'''
#include "metarun.h"
#include "score/score_runs.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static score_record_v1 record_at(unsigned index,unsigned expected_count)
{
    score_db_header header;
    score_record_v1 record;
    SDL_IOStream *file=SDL_IOFromFile("runs.db","rb");
    assert(file && SDL_ReadIO(file,&header,sizeof(header))==sizeof(header));
    assert(!memcmp(header.magic,SCORE_DB_MAGIC,4) && header.record_count==expected_count);
    for (unsigned i=0;i<=index;i++) {
        score_run_detail_header_v1 detail;
        assert(SDL_ReadIO(file,&record,sizeof(record))==sizeof(record));
        if (i==index) break;
        assert(SDL_ReadIO(file,&detail,sizeof(detail))==sizeof(detail));
        assert(score_runs_skip_detail_payload(file,&detail));
    }
    SDL_CloseIO(file); return record;
}
static void stamp(time_t when,score_record_status status,u32b expected_id,
    u32b expected_created,unsigned expected_count)
{
    high_score legacy={0};
    u32b id=0xffffffffu;
    assert(score_runs_record_current_run_with_id(&legacy,when,status,&id));
    assert(id==expected_id);
    score_record_v1 record=record_at(id,expected_count);
    assert(record.record_id==expected_id && record.chronological_idx==expected_id);
    if (record.created_utc!=expected_created || record.completed_utc!=(u32b)when)
        fprintf(stderr,"Snapshot %lu: created %u (expected %u), completed %u\n",
            (unsigned long)when,record.created_utc,expected_created,record.completed_utc);
    assert(record.created_utc==expected_created && record.completed_utc==(u32b)when);
    assert(record.status==status);
}
static void check_timestamps(cptr state)
{
    char meta_directory[1024];
    strnfmt(meta_directory,sizeof(meta_directory),"%s%smetaruns",state,PATH_SEP);
    ANGBAND_DIR_APEX=(char*)state; ANGBAND_DIR_METARUN=meta_directory;
    memset(p_ptr,0,sizeof(*p_ptr));
    p_ptr->chp=p_ptr->mhp=20; p_ptr->playing=true;
    SDL_strlcpy(op_ptr->full_name,"Timestamp Fixture",sizeof(op_ptr->full_name));
    metar.id=777;
    stamp(1000,SCORE_RECORD_ALIVE,0,1000,1);
    stamp(2000,SCORE_RECORD_ALIVE,0,1000,1);
    p_ptr->innate_ability[S_MEL][MEL_POWER]=true;
    stamp(2500,SCORE_RECORD_ALIVE,0,1000,1);
    p_ptr->is_dead=true;
    stamp(3000,SCORE_RECORD_DEAD,0,1000,1);
    p_ptr->is_dead=false;
    stamp(0,SCORE_RECORD_ALIVE,1,0,2);
    stamp(4000,SCORE_RECORD_ALIVE,1,4000,2);
    p_ptr->is_dead=true;
    stamp(5000,SCORE_RECORD_DEAD,1,4000,2);
    SDL_IOStream *file=SDL_IOFromFile("runs.db","rb");
    score_db_header header;
    assert(file && score_runs_validate_history_db(file,&header));
    SDL_CloseIO(file);
    puts("Run history timestamps: alive/save/death, resized details, stable IDs and zero-time fallback PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    check_timestamps(argv[2]); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    excluded = ['/src/main.c.obj']
    extra = []
    if args.baseline:
        text = (ROOT / 'src/score/score_runs.c').read_text(encoding='utf-8')
        block = '        if (existing.created_utc != 0)\n            record.created_utc = existing.created_utc;\n'
        assert text.count(block) == 1
        baseline = OUT / 'score-runs-baseline.c'
        baseline.write_text(text.replace(block, '', 1), encoding='utf-8')
        extra.append(str(baseline)); excluded.append('/src/score/score_runs.c.obj')
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
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp','-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()
