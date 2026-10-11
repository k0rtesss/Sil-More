"""Paint overlapping quick controls and question panels with production order.

Uses the actual foreground render sequence with tiny rectangle painters, and
the production question input-policy predicates. Android chest rolls supply
the real-font/rendering and pointer smoke test.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
present = (ROOT / "src/sdl/render/sdl-present.c").read_text()
if "--baseline" in sys.argv:
    revision = sys.argv[sys.argv.index("--baseline") + 1]
    present = subprocess.check_output(
        ["git", "show", revision + ":src/sdl/render/sdl-present.c"], cwd=ROOT, text=True)
start = present.index("    /* Drawn after the description popup")
end = present.index("    sdl_touch_exit_button_render();", start)
sequence = present[start:end]

question = (ROOT / "src/sdl/ui/sdl-question-menu.c").read_text()
def function(marker):
    start = question.index(marker)
    opening = question.index("{", start)
    depth, cursor = 1, opening + 1
    while depth:
        depth += (question[cursor] == "{") - (question[cursor] == "}")
        cursor += 1
    return question[start:cursor]

# The dispatcher must consult modal policy before any quick-panel hit test.
events = (ROOT / "src/sdl/core/sdl-events.c").read_text()
assert events.index("else if (sdl_question_menu_blocks_input())") < events.index("sdl_touch_top_panel_handle_pointer_down(")
assert events.index("&& sdl_question_overlay_consume_pointer(ev)") < events.index("sdl_touch_top_panel_handle_pointer_down(")

harness = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static struct {bool active, blocking_input, nonblocking;} g_question_menu;
static bool g_touch_tutorial_suppress_runtime_top_panel, hide_main_menu_overlays, banner;
static int pixel[4]; /* overlap, question-only, quick-only, banner-only */
static void sdl_touch_thumb_render(void) {}
static void sdl_touch_top_panel_render(void) {pixel[0]=pixel[2]=1;}
static void sdl_question_menu_render(void) {
    if (g_question_menu.active) pixel[0]=pixel[1]=2;
}
static bool sdl_question_menu_context_hint_active(void) {return false;}
static bool sdl_object_tooltip_uses_screen_rect(void) {return false;}
static void sdl_object_tooltip_render(void) {}
static void sdl_narrative_banner_render(void) {
    if (banner) pixel[0]=pixel[2]=pixel[3]=3;
}
"""
main = r"""
static void paint(void) {
    pixel[0]=pixel[1]=pixel[2]=pixel[3]=0;
SEQUENCE
}
int main(void) {
    /* Ordinary gameplay retains all quick controls. */
    paint(); assert(pixel[0]==1 && pixel[1]==0 && pixel[2]==1);
    /* Banner yields quick-button hits; overlapping buttons must remain visible. */
    banner=true;
    paint(); assert(pixel[0]==1 && pixel[2]==1 && pixel[3]==3);
    /* Interactive question and animated/final dice own foreground pixels. */
    g_question_menu.active=true;
    for(int mode=0;mode<3;mode++) {
        g_question_menu.blocking_input=(mode==1);
        g_question_menu.nonblocking=(mode==2);
        paint(); assert(pixel[0]==2 && pixel[1]==2 && pixel[2]==1);
        assert(sdl_question_menu_blocks_input()==(mode==1));
        assert(sdl_question_menu_captures_pointer()==(mode==0));
    }
    g_touch_tutorial_suppress_runtime_top_panel=true;
    paint(); assert(pixel[0]==2 && pixel[2]==3 && pixel[3]==3);
    hide_main_menu_overlays=true;
    paint(); assert(pixel[0]==0 && pixel[1]==0 && pixel[2]==0);
    puts("Question / Quick Touch overlap and modal policy checks passed");
}
""".replace("SEQUENCE", sequence)
msys = Path(r"C:\msys64\mingw64\bin")
cc = os.environ.get("CC") or shutil.which("gcc") or str(msys / "gcc.exe")
env = os.environ.copy()
if msys.exists():
    env["PATH"] = str(msys) + os.pathsep + str(msys.parent / "usr/bin") + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="sil-question-layer-") as directory:
    directory = Path(directory)
    source = directory / "check.c"
    target = directory / ("check.exe" if os.name == "nt" else "check")
    source.write_text(harness + function("bool sdl_question_menu_blocks_input(")
                      + function("bool sdl_question_menu_captures_pointer(") + main)
    subprocess.run([cc, "-std=c17", str(source), "-o", str(target)], check=True, env=env)
    subprocess.run([str(target)], check=True, env=env)
