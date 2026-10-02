#!/usr/bin/env python3
"""Check template cache recovery, allocation bounds, wizard targeting and loot.

Build standard first. Recompiles the cache reader and uses isolated runtime
directories, current templates, and production engine objects throughout.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/additional-review-fixes"

TESTS = r'''
#include "fs/io_sdl.h"
#include "fs/path.h"
int fixture_read_raw(SDL_IOStream*, header*);

static void check_cache_recovery(void)
{
    char path[1024];
    assert(path_build(path, sizeof(path), ANGBAND_DIR_DATA, "object.raw"));
    SDL_IOStream* file = SDL_IOFromFile(path, "rb"); assert(file);
    size_t length = (size_t)SDL_GetIOSize(file);
    byte* bytes = mem_alloc_array(length + 1, byte); assert(bytes);
    assert(SDL_ReadIO(file, bytes, length) == length); SDL_CloseIO(file);
    header original = k_head;
    size_t cuts[] = {1, sizeof(header)-1, sizeof(header),
        sizeof(header)+original.info_size-1,
        sizeof(header)+original.info_size,
        sizeof(header)+original.info_size+original.name_size-1,
        sizeof(header)+original.info_size+original.name_size,
        length-1, length+1};
    for (size_t i=0; i<N_ELEMENTS(cuts); i++) {
        header candidate = original;
        file = SDL_IOFromConstMem(bytes, cuts[i]); assert(file);
        assert(fixture_read_raw(file, &candidate) != 0);
        assert(!memcmp(&candidate, &original, sizeof(candidate)));
        SDL_CloseIO(file);
    }
    /* Serialized pointers/callbacks must not replace current process state. */
    header stored;
    memcpy(&stored, bytes, sizeof(stored));
    stored.parse_info_txt = NULL;
    stored.info_ptr = stored.name_ptr = stored.text_ptr = NULL;
    memcpy(bytes, &stored, sizeof(stored));
    header candidate = original;
    file = SDL_IOFromConstMem(bytes, length); assert(file);
    assert(!fixture_read_raw(file, &candidate)); SDL_CloseIO(file);
    assert(candidate.parse_info_txt == original.parse_info_txt);
    assert(!memcmp(candidate.info_ptr, original.info_ptr, original.info_size));
    assert(!memcmp(candidate.name_ptr, original.name_ptr, original.name_size));
    assert(!memcmp(candidate.text_ptr, original.text_ptr, original.text_size));
    free_info(&candidate);
    /* A cache with a valid header and incomplete body is newer than its text
     * template. Public initialization must regenerate it, not publish blanks. */
    file = SDL_IOFromFile(path, "wb"); assert(file);
    assert(SDL_WriteIO(file, bytes, sizeof(header)) == sizeof(header));
    SDL_CloseIO(file);
    free_info(&k_head);
    assert(!init_k_info());
    assert(k_info[1].name && !strcmp(k_name+k_info[1].name, "& Serpentine Ring~"));
    file = SDL_IOFromFile(path, "rb"); assert(file);
    assert((size_t)SDL_GetIOSize(file) == length); SDL_CloseIO(file);
    mem_free(bytes);
    puts("Template caches: incomplete/trailing streams rejected atomically, valid bytes/callback retained, public regeneration PASS.");
}

static void check_allocation_bounds(void)
{
    object_kind kinds[2] = {0}; monster_race races[2] = {0};
    ego_item_type egos[2] = {0}; char names[1024] = {0};
    header head = {.info_num=2, .info_ptr=kinds, .name_ptr=names};
    char kind_name[]="N:1:boundary object"; error_idx=-1;
    assert(!parse_k_info(kind_name, &head));
    int depths[] = {0, MAX_DEPTH-1, MAX_DEPTH, 255, -1};
    for (size_t i=0; i<N_ELEMENTS(depths); i++) {
        char line[64]; strnfmt(line, sizeof(line), "A:%d/1", depths[i]);
        int result = parse_k_info(line, &head);
        assert((result == 0) == (depths[i]>=0 && depths[i]<MAX_DEPTH));
    }
    head.info_ptr=races; head.name_size=0;
    char monster_name[]="N:1:boundary monster"; error_idx=-1;
    assert(!parse_r_info(monster_name, &head));
    for (size_t i=0; i<N_ELEMENTS(depths); i++) {
        char line[64]; strnfmt(line, sizeof(line), "W:%d:1", depths[i]);
        int result = parse_r_info(line, &head);
        assert((result == 0) == (depths[i]>=0 && depths[i]<MAX_DEPTH));
    }
    head.info_ptr=egos; head.name_size=0;
    char ego_name[]="N:1:boundary ego"; error_idx=-1;
    assert(!parse_e_info(ego_name, &head));
    for (size_t i=0; i<N_ELEMENTS(depths); i++) {
        char line[64]; strnfmt(line, sizeof(line), "W:%d:1:0:1", depths[i]);
        int result = parse_e_info(line, &head);
        assert((result == 0) == (depths[i]>=0 && depths[i]<MAX_DEPTH));
        strnfmt(line, sizeof(line), "A:%d/1", depths[i]);
        result = parse_e_info(line, &head);
        assert((result == 0) == (depths[i]>=0 && depths[i]<MAX_DEPTH));
    }
    /* Raw caches bypass the parsers: reject them before any table mutation. */
    alloc_entry* table = alloc_kind_table; int size = alloc_kind_size;
    byte old_depth=k_info[1].locale[0], old_chance=k_info[1].chance[0];
    k_info[1].locale[0]=MAX_DEPTH; k_info[1].chance[0]=1;
    assert(init_alloc() == PARSE_ERROR_OUT_OF_BOUNDS);
    k_info[1].locale[0]=old_depth; k_info[1].chance[0]=old_chance;
    old_depth=r_info[1].level; old_chance=r_info[1].rarity;
    r_info[1].level=MAX_DEPTH; r_info[1].rarity=1;
    assert(init_alloc() == PARSE_ERROR_OUT_OF_BOUNDS);
    r_info[1].level=old_depth; r_info[1].rarity=old_chance;
    old_depth=e_info[1].level; old_chance=e_info[1].rarity;
    e_info[1].level=MAX_DEPTH; e_info[1].rarity=1;
    assert(init_alloc() == PARSE_ERROR_OUT_OF_BOUNDS);
    e_info[1].level=old_depth; e_info[1].rarity=old_chance;
    assert(alloc_kind_table == table && alloc_kind_size == size);
    puts("Allocation bounds: object/monster/ego parser boundaries and cached-record rejection before table mutation PASS.");
}

static void check_wizard_target_capacity(void)
{
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=88;
    p_ptr->py=p_ptr->px=2; p_ptr->wy=p_ptr->wx=1;
    p_ptr->rage=0; g_labyrinth_view_active=false; use_bigtile=false;
    for (int y=0; y<88; y++) for (int x=0; x<88; x++) {
        cave_feat[y][x]=(y==0 || x==0 || y==87 || x==87) ? FEAT_WALL_PERM : FEAT_CHASM;
        cave_info[y][x]=CAVE_MARK;
        cave_o_idx[y][x]=cave_m_idx[y][x]=0;
    }
    cave_feat[2][2]=FEAT_LESS; cave_m_idx[2][2]=-1;
    byte* saved_x=temp_x; byte* saved_y=temp_y;
    temp_x=mem_alloc_array(8192,byte); temp_y=mem_alloc_array(8192,byte);
    assert(temp_x && temp_y);
    int widths[]={32,64,65,80};
    for (size_t n=0; n<N_ELEMENTS(widths); n++) {
        assert(!Term_resize(widths[n],24));
        memset(temp_x,0xA5,8192); memset(temp_y,0xA5,8192);
        get_sorted_target_list(TARGET_WIZ,0);
        assert(temp_n == MIN(widths[n]*24,TEMP_MAX));
        for (int i=TEMP_MAX; i<8192; i++)
            assert(temp_x[i]==0xA5 && temp_y[i]==0xA5);
        bool seen[88][88]={0};
        for (int i=0; i<temp_n; i++) {
            assert(in_bounds_fully(temp_y[i],temp_x[i]));
            assert(!seen[temp_y[i]][temp_x[i]]);
            seen[temp_y[i]][temp_x[i]]=true;
        }
    }
    mem_free(temp_x); mem_free(temp_y); temp_x=saved_x; temp_y=saved_y; temp_n=0;
    puts("Wizard targets: below/exact/over capacity, sorted unique legal grids and both memory guards PASS.");
}

static void check_no_artefacts(void)
{
    p_ptr->depth=object_level=20;
    drop_system_init();
    drop_profile profile; drop_profile_default(&profile);
    profile.weight_weapon=100;
    profile.weight_armor=profile.weight_jewelry=profile.weight_supply=0;
    object_type item;
    for (int restriction=1; restriction<=3; restriction++) {
        adult_no_artefacts=(restriction&1)!=0;
        birth_no_artefacts=(restriction&2)!=0;
        for (int mode=OB_GEN_MODE_CHEST; mode<=OB_GEN_MODE_MONSTER_DROP; mode++) {
            if (mode!=OB_GEN_MODE_CHEST && mode!=OB_GEN_MODE_MONSTER_DROP) continue;
            object_generation_mode=mode; Rand_state_init(12345);
            int generated=0;
            for (int n=0; n<200; n++) {
                object_wipe(&item);
                if (make_object_with_profile(&item,DROP_QUALITY_GREAT,DROP_TYPE_UNTHEMED,&profile)) {
                    generated++; assert(!item.name1);
                }
            }
            assert(generated>0);
        }
        object_wipe(&item);
        assert(!drop_generate_guaranteed_artefact(20,20,DROP_QUALITY_GREAT,
            DROP_TYPE_UNTHEMED,&profile,&item));
        assert(!item.k_idx);
    }
    /* Positive control: the exact original sequence still permits artefacts. */
    adult_no_artefacts=birth_no_artefacts=false;
    object_generation_mode=OB_GEN_MODE_MONSTER_DROP; Rand_state_init(12345);
    bool found=false;
    for (int n=0; n<200; n++) {
        object_wipe(&item);
        if (make_object_with_profile(&item,DROP_QUALITY_GREAT,DROP_TYPE_UNTHEMED,&profile) && item.name1) {
            found=true; break;
        }
    }
    assert(found);
    puts("No artefacts: 1200 chest/monster attempts, separate birth/adult restrictions, direct guaranteed API, enabled positive control PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix=HARNESS[:HARNESS.index("static const char* guids[]")]
    terminal=HARNESS[HARNESS.index("static errr terminal_extra("):HARNESS.index("static void reset_map(")]
    init=HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init=init[:init.index("    check_templates();")]
    source=OUT/"check.c"
    source.write_text(prefix+terminal+TESTS+init+'''
    char metarun_dir[1024];
    assert(path_build(metarun_dir,sizeof(metarun_dir),argv[2],"metaruns"));
    assert(SDL_CreateDirectory(metarun_dir));
    ANGBAND_DIR_APEX=argv[2]; ANGBAND_DIR_METARUN=metarun_dir;
    check_cache_recovery(); check_allocation_bounds();
    check_wizard_target_capacity(); check_no_artefacts();
    SDL_Quit(); return 0;
}
''',encoding="utf-8")
    reader=OUT/"reader.c"
    reader.write_text('#include "init/init-info.c"\nint fixture_read_raw(SDL_IOStream* f,header* h){return init_info_raw(f,h);}\n',encoding="utf-8")
    cmake=BUILD/"CMakeFiles/sil-more.dir"
    objects=shlex.split((cmake/"objects1.rsp").read_text())
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects
        if not p.endswith(("/src/main.c.obj","/src/init/init-info.c.obj"))),encoding="utf-8")
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([*(str(BUILD/"_deps"/x) for x in
        ("SDL","SDL_ttf","SDL_image","SDL_mixer")),
        "C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(reader),"@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp","-o",str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=OUT) as data:
        result=subprocess.run([str(exe),str(ROOT/"lib/edit"),data],cwd=data,env=env,
            capture_output=True,text=True,timeout=45)
        (OUT/"validation.log").write_text(result.stdout+result.stderr,encoding="utf-8")
        print(result.stdout+result.stderr)
        result.check_returncode()


if __name__=="__main__":
    main()
