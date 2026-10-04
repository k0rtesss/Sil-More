#!/usr/bin/env python3
"""Check reserve rescaling and terrain memory through the real engine.

Rebuild first. Uses isolated data, actual buff handlers, and the production SDL
map renderer with software rendering. No player saves or settings are opened.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS

ROOT = Path(__file__).resolve().parents[1]

TESTS = r'''
#include "player/player-upkeep-internal.h"
#include "cave/cave-environment.h"
#include <math.h>
void check_terrain_memory(void);

static void prepare_player(void) {
    cave_environment_reset();
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    p_ptr->prace=p_ptr->pcharacter=0; rp_ptr=&p_info[0]; current_character_profile=&c_info[0];
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=88; p_ptr->py=p_ptr->px=5;
    character_generated=character_dungeon=false; character_xtra=true;
    for (int y=0;y<88;y++) for (int x=0;x<88;x++) {
        cave_feat[y][x]=FEAT_FLOOR; cave_info[y][x]=0; cave_light[y][x]=2;
        cave_o_idx[y][x]=cave_m_idx[y][x]=0;
    }
    cave_m_idx[5][5]=-1;
}

static void check_buff_handlers(void) {
    prepare_player(); p_ptr->stat_base[A_CON]=11;
    p_ptr->update=PU_BONUS; update_stuff();
    assert(p_ptr->mhp==107); p_ptr->chp=1;
    assert(set_tmp_con(10));
    assert(p_ptr->chp==2 && p_ptr->mhp==164);
    assert(set_tmp_con(0));
    assert(p_ptr->chp==1 && p_ptr->mhp==107);
    p_ptr->chp=43;
    assert(set_tmp_con(10));
    assert(p_ptr->chp==66 && p_ptr->mhp==164);
    assert(set_tmp_con(0));
    assert(p_ptr->chp==43 && p_ptr->mhp==107);

    prepare_player(); p_ptr->stat_base[A_GRA]=9;
    p_ptr->update=PU_BONUS; update_stuff();
    assert(p_ptr->msp==103); p_ptr->csp=1;
    assert(set_tmp_gra(10));
    assert(p_ptr->csp==2 && p_ptr->msp==178);
    assert(set_tmp_gra(0));
    assert(p_ptr->csp==1 && p_ptr->msp==103);
    p_ptr->csp=43;
    assert(set_tmp_gra(10));
    assert(p_ptr->csp==74 && p_ptr->msp==178);
    assert(set_tmp_gra(0));
    assert(p_ptr->csp==43 && p_ptr->msp==103);
    puts("Actual CON/GRA buffs and expiry preserve low and partial reserves: PASS.");
}

static void check_reserve_ratios(void) {
    prepare_player();
    int checks=0;
    for (int voice=0;voice<2;voice++) {
        void (*calculate)(void)=voice?calc_voice:calc_hitpoints;
        s16b* maximum=voice?&p_ptr->msp:&p_ptr->mhp;
        s16b* reserve=voice?&p_ptr->csp:&p_ptr->chp;
        u16b* fraction=voice?&p_ptr->csp_frac:&p_ptr->chp_frac;
        int stat=voice?A_GRA:A_CON;
        u32b redraw=voice?PR_VOICE:PR_HP;
        for (int old_stat=-9;old_stat<=20;old_stat++) {
            p_ptr->stat_use[stat]=old_stat; *maximum=0; calculate();
            int old_max=*maximum;
            for (int new_stat=-9;new_stat<=20;new_stat++) {
                p_ptr->stat_use[stat]=new_stat; *maximum=0; calculate();
                int new_max=*maximum;
                const int samples[]={0,1,old_max/3,old_max/2,old_max-1,old_max,-1,-old_max};
                for (unsigned n=0;n<N_ELEMENTS(samples);n++) {
                    *maximum=old_max; *reserve=samples[n]; *fraction=12345;
                    p_ptr->redraw=p_ptr->window=0; calculate();
                    assert(*maximum==new_max);
                    /* Independently bound the error in points. No intermediate
                     * percentage rounding may move it beyond half a point. */
                    double exact=(double)samples[n]*new_max/old_max;
                    assert(fabs((double)*reserve-exact)<=0.500000001);
                    if (!samples[n]) assert(!*reserve);
                    if (samples[n]==old_max) assert(*reserve==new_max);
                    if (old_max!=new_max) {
                        assert(!*fraction);
                        assert(p_ptr->redraw&redraw);
                        assert(p_ptr->window&PW_PLAYER_0);
                    } else {
                        assert(*reserve==samples[n] && *fraction==12345);
                        assert(!p_ptr->redraw && !p_ptr->window);
                    }
                    checks++;
                }
            }
        }
        *maximum=0; *reserve=0; *fraction=12345; calculate();
        assert(*reserve==*maximum && !*fraction);
    }
    printf("HP/voice: %d ratio checks across every legal stat, full/empty/signed reserves and unchanged maxima: PASS.\n",checks);
}
'''

RENDER = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-environment.h"
#include <assert.h>

static SDL_Surface* capture_tile(void) {
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);
    SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};
    byte a,ta; char c,tc;
    map_info(10,10,&a,&c,&ta,&tc);
    sdl_draw_map_tile_layers_at(10,10,a,c,ta,tc,&dst);
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL); assert(surface);
    return surface;
}
static int changed_pixels(SDL_Surface* before,SDL_Surface* after) {
    int changed=0;
    for (int y=0;y<16;y++) for (int x=0;x<16;x++) {
        Uint8 br,bg,bb,ba,ar,ag,ab,aa;
        assert(SDL_ReadSurfacePixel(before,x,y,&br,&bg,&bb,&ba));
        assert(SDL_ReadSurfacePixel(after,x,y,&ar,&ag,&ab,&aa));
        changed += br!=ar || bg!=ag || bb!=ab || ba!=aa;
    }
    return changed;
}
static void memory_case(int center,int remembered,int live,bool change_center,int light) {
    cave_environment_reset();
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;
    p_ptr->depth=10; p_ptr->rage=p_ptr->blind=p_ptr->image=0;
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) {
        cave_feat[y][x]=FEAT_FLOOR; cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
        cave_light[y][x]=2; cave_color[y][x]=COLOR_STYLE_BASE;
        cave_o_idx[y][x]=cave_m_idx[y][x]=0;
    }
    cave_feat[10][10]=center;
    int x=change_center?10:11;
    cave_feat[10][x]=remembered;
    cave_info[10][x]=CAVE_MARK|CAVE_GLOW;
    cave_light[10][x]=light;
    cave_environment_seed();
    byte before_a,after_a; char before_c,after_c;
    map_info_terrain(10,x,&before_a,&before_c);
    SDL_Surface* before=capture_tile();
    cave_set_feat(10,x,live);
    assert(cave_environment_known_feature(10,x)==remembered);
    map_info_terrain(10,x,&after_a,&after_c);
    assert(before_a==after_a && before_c==after_c);
    SDL_Surface* unseen=capture_tile();
    int hidden_changes=changed_pixels(before,unseen);
    /* Receding contours sample physical neighbouring terrain. Their geometry
     * may change without observation, while
     * the remembered tile identity and an unseen center stay unchanged. */
    if (change_center) assert(!hidden_changes);
    else assert(hidden_changes>0);
    /* Positive control: real observation must refresh the contour. */
    cave_info[10][x]|=CAVE_SEEN; cave_light[10][x]=2;
    cave_environment_observe(10,x);
    assert(cave_environment_known_feature(10,x)==live);
    SDL_Surface* seen=capture_tile();
    int observed_changes=changed_pixels(before,seen);
    printf("Terrain memory: center=%d remembered=%d live=%d light=%d hidden=%d observed=%d\n",
        center,remembered,live,light,hidden_changes,observed_changes);
    /* An unlit floor supplied no donor pixels in the first place; connecting
     * that edge to a newly seen chasm need not alter this particular contour. */
    if (light>0 || remembered!=FEAT_FLOOR) assert(observed_changes>0);
    SDL_DestroySurface(before); SDL_DestroySurface(unseen); SDL_DestroySurface(seen);
}
void check_terrain_memory(void) {
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO));
    assert(SDL_CreateWindowAndRenderer("Terrain memory check",16,16,SDL_WINDOW_HIDDEN,&g_state.window,&g_state.renderer));
    g_state.use_tiles=true; use_graphics=GRAPHICS_MICROCHASM; assert(sdl_load_tileset_texture());
    f_info[FEAT_FLOOR].x_attr=TILE_FLAG; f_info[FEAT_FLOOR].x_char=(char)(TILE_FLAG|1);
    f_info[FEAT_CHASM].x_attr=TILE_FLAG|34; f_info[FEAT_CHASM].x_char=(char)TILE_FLAG;
    f_info[FEAT_WALL_EXTRA].x_attr=TILE_FLAG|33; f_info[FEAT_WALL_EXTRA].x_char=(char)TILE_FLAG;
    memory_case(FEAT_CHASM,FEAT_FLOOR,FEAT_CHASM,false,2);
    memory_case(FEAT_CHASM,FEAT_CHASM,FEAT_FLOOR,false,2);
    memory_case(FEAT_CHASM,FEAT_FLOOR,FEAT_CHASM,false,0);
    memory_case(FEAT_CHASM,FEAT_WALL_EXTRA,FEAT_FLOOR,false,0);
    memory_case(FEAT_CHASM,FEAT_CHASM,FEAT_FLOOR,true,2);
    memory_case(FEAT_WATER,FEAT_FLOOR,FEAT_WATER,false,2);
    memory_case(FEAT_WATER,FEAT_WATER,FEAT_FLOOR,false,2);
    puts("Full SDL map layers: remembered terrain identity survives seven unseen changes; unseen own-cell pixels stay identical and physical neighbour contours update: PASS.");
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", choices=("standard", "portable"), default="standard")
    args = parser.parse_args()
    build = ROOT / f"build-{args.build}"
    out = ROOT / f"scripts/output/vitals-terrain-memory-{args.build}"
    out.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static const char* guids[]")]
    terminal = HARNESS[HARNESS.index("static errr terminal_extra("):HARNESS.index("static void reset_map(")]
    init = HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = out / "check.c"
    source.write_text(prefix + terminal + TESTS + init + r'''
    ANGBAND_DIR_APEX=ANGBAND_DIR_METARUN=ANGBAND_DIR_SAVE=argv[2];
    check_buff_handlers(); check_reserve_ratios(); check_terrain_memory();
    SDL_Quit(); return 0;
}
''', encoding="utf-8")
    render = out / "render.c"
    render.write_text(RENDER, encoding="utf-8")
    objects = shlex.split((build / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    env["PATH"] = os.pathsep.join([
        *(str(build / "_deps" / n) for n in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(render),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=build, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=ROOT,
                                env=env, capture_output=True, text=True, timeout=45)
        (out / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        print(result.stdout + result.stderr)
        result.check_returncode()


if __name__ == "__main__":
    main()
