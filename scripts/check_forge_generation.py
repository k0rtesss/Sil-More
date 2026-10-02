#!/usr/bin/env python3
"""Check ordinary forge generation after retiring guaranteed milestones.

Requires a completed standard build. Links production objects and uses the
existing generation fixture's isolated runtime/template setup. All generated
files stay under scripts/output/forge-generation.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_utumno_generation import HARNESS as GENERATION_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/forge-generation"

# Share initialization and observation wrappers, without the Utumno tests.
SETUP = GENERATION_HARNESS.split("static bool safe_tile(", 1)[0]
SETUP = SETUP.replace("assert(++attempts <= 30)", "assert(++attempts <= 100)")
POPULATION_WRAPPER = """int __wrap_run_partition_monster_pass(const partition_population_plan* plans,int n) {
    population_passes++; return __real_run_partition_monster_pass(plans,n);
}"""
assert POPULATION_WRAPPER in SETUP
SETUP = SETUP.replace(POPULATION_WRAPPER, r'''
static bool fill_object_capacity;
static bool saw_object_overflow;
int __wrap_run_partition_monster_pass(const partition_population_plan* plans,int n) {
    population_passes++;
    int result = __real_run_partition_monster_pass(plans,n);
    if (fill_object_capacity) {
        fill_object_capacity = false;
        /* Exercise the existing late rejection with legally linked objects,
         * rather than assigning allocation counters or bypassing the engine. */
        for (int y=1;y<p_ptr->cur_map_hgt-1 && o_max<z_info->o_max;y++)
            for (int x=1;x<p_ptr->cur_map_wid-1 && o_max<z_info->o_max;x++) {
                if (!cave_floor_bold(y,x) || cave_o_idx[y][x] || cave_m_idx[y][x])
                    continue;
                object_type object;
                object_prep(&object,1);
                object.number=1;
                floor_carry(y,x,&object);
            }
        assert(o_max==z_info->o_max && o_cnt==z_info->o_max-1);
    }
    return result;
}
void __real_level_gen_screen_note_failure(const char *why);
void __wrap_level_gen_screen_note_failure(const char *why) {
    if (why && !strcmp(why,"too many objects")) saw_object_overflow=true;
    __real_level_gen_screen_note_failure(why);
}
''')

HARNESS = SETUP + r'''
typedef struct {
    unsigned long long map_signature;
    int forges;
} result_t;

static result_t generate(int depth,int seed,int legacy_count,bool overflow) {
    wipe_o_list();
    wipe_mon_list();
    player_wipe();
    rp_ptr=&p_info[0];
    current_character_profile=&c_info[0];
    p_ptr->playing=true;
    p_ptr->chp=p_ptr->mhp=100;
    p_ptr->depth=p_ptr->max_depth=depth;
    p_ptr->fixed_forge_count=legacy_count;
    op_ptr->opt[OPT_utumno_corridors]=false;
    playerturn=1;
    character_generated=true;
    fill_object_capacity=overflow;
    saw_object_overflow=false;
    attempts=0;
    Rand_state_init(seed);
    generate_cave();
    assert(p_ptr->fixed_forge_count==legacy_count);
    if (overflow) assert(saw_object_overflow && attempts>=2);
    result_t result={1469598103934665603ULL,0};
    for (int y=0;y<p_ptr->cur_map_hgt;y++)
        for (int x=0;x<p_ptr->cur_map_wid;x++) {
            result.map_signature^=cave_feat[y][x];
            result.map_signature*=1099511628211ULL;
            result.forges+=cave_forge_bold(y,x)!=0;
        }
    assert(p_ptr->forge_count==result.forges);
    return result;
}

int main(int argc,char **argv) {
    assert(argc==3);
    setbuf(stdout,NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    assert(term_init(&test_term,80,24,256)==0);
    test_term.xtra_hook=dummy_xtra;
    angband_term[0]=&test_term;
    Term_activate(&test_term);
    initialize(argv[1],argv[2]);

    const int depths[]={2,6,10,12};
    int ordinary_forges=0, ordinary_maps_without_forges=0;
    for (unsigned d=0;d<N_ELEMENTS(depths);d++) {
        int depth=depths[d], maps_without_forges=0;
        for (int seed=1;seed<=8;seed++) {
            result_t fresh=generate(depth,seed,0,false);
            result_t old_hero=generate(depth,seed,3,false);
            assert(fresh.map_signature==old_hero.map_signature);
            assert(fresh.forges==old_hero.forges);
            maps_without_forges+=fresh.forges==0;
            ordinary_forges+=fresh.forges;
        }
        ordinary_maps_without_forges+=maps_without_forges;
        printf("PASS depth%d: zero-forge maps%d/8; legacy count does not affect generation\n",
               depth,maps_without_forges);
    }
    /* Ordinary deep rooms may happen to include a forge in every sampled
     * map. Prove both outcomes globally; counter equivalence above checks
     * the removal at every former milestone and after skipped depths. */
    assert(ordinary_forges>0 && ordinary_maps_without_forges>0);
    printf("PASS ordinary forge-bearing rooms generated%d forges in32 sampled maps\n",ordinary_forges);

    for (int count=0;count<=3;count++) {
        generate(2,3,count,true);
        printf("PASS actual object-overflow retry leaves legacy count%d unchanged\n",count);
    }
    puts("Forge generation checks PASS");
    SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [path for path in objects if not path.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + path + '"' for path in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrappers = ("get_sdl_config_path", "level_gen_screen_start_attempt", "process_player",
                "apply_quadrant_generation_modes", "connect_rooms_stairs", "place_dungeon_terrain",
                "run_partition_monster_pass", "level_gen_screen_note_failure")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wrappers),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="runtime-", dir=OUT) as temp:
        result = subprocess.run([str(exe), str(ROOT), temp], cwd=temp, env=env,
                                capture_output=True, text=True, timeout=180)
    (OUT / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    print(result.stderr, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
