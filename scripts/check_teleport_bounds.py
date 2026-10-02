#!/usr/bin/env python3
"""Exercise production near-teleports on open, crowded and invalid maps.

Uses an isolated engine harness and current templates, with a timeout for every
scenario. No character files are loaded or saved.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/teleport-bounds"

TESTS = r'''
void __wrap_handle_stuff(void) { p_ptr->update=0; }
void __wrap_msg_print(cptr text) { (void)text; }

static void test_teleport(const char* name)
{
    reset_map(10);
    p_ptr->cur_map_hgt=13; p_ptr->cur_map_wid=15;
    p_ptr->py=6; p_ptr->px=7;
    p_ptr->chp=p_ptr->mhp=100;
    memset(cave_m_idx,0,MAX_DUNGEON_HGT*sizeof(*cave_m_idx));
    p_ptr->leaping=true;
    for (int y=0;y<13;y++) for (int x=0;x<15;x++)
        cave_set_feat(y,x,FEAT_WALL_EXTRA);
    cave_set_feat(6,7,FEAT_FLOOR); cave_m_idx[6][7]=-1;
    if (!strcmp(name,"player-blocked")) {
        teleport_player_to(8,10);
        assert(p_ptr->py==6 && p_ptr->px==7 && p_ptr->leaping);
    } else if (!strcmp(name,"player-invalid")) {
        teleport_player_to(-100,-100);
        assert(p_ptr->py==6 && p_ptr->px==7 && p_ptr->leaping);
    } else if (!strcmp(name,"player-edge")) {
        cave_set_feat(1,1,FEAT_FLOOR);
        teleport_player_to(0,0);
        assert(p_ptr->py==1 && p_ptr->px==1 && !p_ptr->leaping);
    } else if (!strcmp(name,"player-exact")) {
        cave_set_feat(8,10,FEAT_FLOOR);
        teleport_player_to(8,10);
        assert(p_ptr->py==8 && p_ptr->px==10 && !p_ptr->leaping);
    } else {
        monster_type* monster=&mon_list[1];
        memset(monster,0,sizeof(*monster));
        monster->r_idx=10; monster->fy=4; monster->fx=4;
        monster->hp=monster->maxhp=100;
        cave_set_feat(4,4,FEAT_FLOOR); cave_m_idx[4][4]=1; mon_max=2;
        if (!strcmp(name,"monster-glyphs")) {
            for (int y=1;y<12;y++) for (int x=1;x<14;x++)
                if (!cave_m_idx[y][x]) cave_set_feat(y,x,FEAT_GLYPH);
        } else if (!strcmp(name,"monster-open")) {
            cave_set_feat(8,10,FEAT_FLOOR);
            monster->previous_action[0]=6; monster->consecutive_attacks=3;
        }
        teleport_towards(4,4,6,7);
        if (!strcmp(name,"monster-open")) {
            assert(monster->fy==8 && monster->fx==10);
            assert(monster->previous_action[0]==ACTION_MISC && !monster->consecutive_attacks);
        } else assert(monster->fy==4 && monster->fx==4 && cave_m_idx[4][4]==1);
    }
    printf("Teleport scenario %s: PASS\n",name);
}
'''


def main():
    cases = ("player-blocked","player-invalid","player-edge","player-exact",
             "monster-blocked","monster-glyphs","monster-open")
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case",choices=("all",*cases),default="all")
    parser.add_argument("--portable",action="store_true")
    args=parser.parse_args()
    build=ROOT/("build-portable" if args.portable else "build-standard")
    out=OUT/("portable" if args.portable else "standard")
    out.mkdir(parents=True,exist_ok=True)
    prefix=HARNESS[:HARNESS.index("static const char* guids[]")]
    init=HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init=init[:init.index("    check_templates();")].replace("assert(argc==3);","assert(argc==4);")
    source=out/"check.c"
    source.write_text(prefix+fixture_function("terminal_extra")+"\n"+fixture_function("reset_map")+
                      TESTS+init+"    test_teleport(argv[3]); SDL_Quit(); return 0;\n}\n",encoding="utf-8")
    objects=shlex.split((build/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response=out/"objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects
        if not obj.endswith(("/src/main.c.obj","/src/spell/spell-teleport.c.obj"))),encoding="utf-8")
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([
        *(str(build/"_deps"/name) for name in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),
        "C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    executable=out/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(ROOT/"src/spell/spell-teleport.c"),
                    "@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=handle_stuff","-Wl,--wrap=msg_print","-o",str(executable)],
                   cwd=build,env=env,check=True)
    for case in cases if args.case=="all" else [args.case]:
        with tempfile.TemporaryDirectory(prefix="data-",dir=out) as data:
            subprocess.run([str(executable),str(ROOT/"lib/edit"),data,case],
                           cwd=data,env=env,check=True,timeout=5)


if __name__=="__main__":
    main()
