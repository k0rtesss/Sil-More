"""Replay the production tile redraw and camera with a player near the right edge."""
from pathlib import Path
import os
import subprocess
import tempfile
import test_phone_camera_clearance as camera

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT/'src/sdl/render/sdl-term-callbacks.c').read_text()
start = source.index('void sdl_mark_tiles_mode_game_redraw(void)')
end = source.index('\n}\n',start)+3
mark = source[start:end]
prefix = camera.PREFIX.replace('unsigned redraw,window;',
                               'unsigned redraw,window,notice,update;')
prefix = prefix.replace('#define SCREEN_WID ((Term->wid+1)/2)',
                        '#define SCREEN_WID (use_bigtile?(Term->wid+1)/2:Term->wid)')
prefix += r'''
static bool character_generated=true;
#define PN_COMBINE 1
#define PN_REORDER 2
#define PU_PANEL 1
#define PU_BONUS 2
#define PU_HP 4
#define PU_MANA 8
#define PU_FORGET_VIEW 16
#define PU_UPDATE_VIEW 32
#define PU_MONSTERS 64
#define PR_BASIC 2
#define PR_EXTRA 4
#define PR_EQUIPPY 8
#define PR_RESIST 16
#define PW_INVEN 2
#define PW_EQUIP 4
#define PW_PLAYER_0 8
#define PW_MESSAGE 16
#define PW_MONSTER 32
#define PW_OBJECT 64
#define PW_MONLIST 128
#define PW_SUPPLY 256
'''
checks = r'''
static int check(void) {
    const int widths[]={100,72,45,34};
    for(int i=0;i<4;i++) {
        terminal=(struct term){widths[i],22};tile=32;grid_x=grid_y=0;
        blocker_count=0;use_bigtile=false;
        player=(struct player){.py=55,.px=126,.wy=44,
            .wx=126-widths[i]+3,.depth=2,.cur_map_hgt=132,.cur_map_wid=132};
        assert(panel_contains(player.py,player.px));
        /* The runtime toggle changes the projection before its Ctrl-R redraw. */
        use_bigtile=true;
        assert(!panel_contains(player.py,player.px));
        sdl_mark_tiles_mode_game_redraw();
        /* The normal update dispatcher consumes PU_PANEL before PR_MAP. */
        if(player.update&PU_PANEL)verify_panel();
        if(!panel_contains(player.py,player.px)) {
            fputs("Tiles toggle left the idle player offscreen\n",stderr);return 2;
        }
        assert(player.py==55&&player.px==126);
        assert(game_turn==96&&player_turn==10&&disturbed==0);
        use_bigtile=false;sdl_mark_tiles_mode_game_redraw();
        if(player.update&PU_PANEL)verify_panel();
        assert(panel_contains(player.py,player.px));
    }
    puts("Tiles/ASCII camera: 4 viewport widths, idle visibility and unchanged turns PASS");
    return 0;
}
int main(void){return check();}
'''


def main():
    msys=Path(r'C:/msys64/mingw64/bin')
    env=os.environ.copy()
    env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env['PATH']
    before=mark.replace('PU_PANEL | PU_BONUS','PU_BONUS')
    with tempfile.TemporaryDirectory(prefix='sil-tiles-camera-') as folder:
        folder=Path(folder)
        for name,body in [('fixed',mark),('before',before)]:
            c=folder/(name+'.c');exe=folder/(name+'.exe')
            c.write_text(prefix+camera.SOURCE+body+checks)
            subprocess.run([str(msys/'gcc.exe'),'-std=c17','-O0',str(c),'-o',str(exe)],
                           env=env,check=True,capture_output=True)
            result=subprocess.run([str(exe)],env=env,capture_output=True,text=True)
            if name=='fixed':
                assert result.returncode==0,result.stdout+result.stderr
                print(result.stdout.strip())
            else:
                assert result.returncode==2,result.stdout+result.stderr
                print('Before-change negative: reproduced the idle player disappearing')


if __name__=='__main__':
    main()
