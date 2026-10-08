#!/usr/bin/env python3
"""Render and operate real confirmations over a retained gameplay narrative.

Uses private SDL/template paths and production event dispatch/get_check. The
committed baseline obscures button pixels and consumes Escape into the banner.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-narrative-modal-priority"
CHECKS = r'''
extern char g_active_partition_banner_text[1024];
extern bool g_active_partition_banner_consumes_input;
extern void do_cmd_help_menu(void);
static errr (*real_extra)(int,int);
static int injected,answer_kind;
static char fallback_key='n';
static cptr prompt="Leave Duruin's Bastion and fail Varda's quest?";
static void banner(void)
{
    SDL_strlcpy(g_active_partition_banner_text,
        "The dry brown labour-pit brick recedes as dark packed earthen walls "
        "closes around you. It feels less like a fortress and more like a tomb "
        "burrowed by the great worms of the north.",1024);
    g_banner_force_redraw_remaining=3;
    g_active_partition_banner_consumes_input=false;
}
static SDL_Surface* frame(cptr suffix)
{
    char path[1200];
    g_state.need_present=true;
    assert(sdl_render_current_window_frame());
    SDL_Surface* image=SDL_RenderReadPixels(g_state.renderer,NULL);assert(image);
    strnfmt(path,sizeof(path),"%s-%s.png",SDL_getenv("PRIORITY_PNG"),suffix);
    assert(IMG_SavePNG(image,path));
    return image;
}
static bool same_button(SDL_Surface* a,SDL_Surface* b,SDL_FRect rect)
{
    for(int y=(int)rect.y+3;y<(int)(rect.y+rect.h)-3;y++)
        for(int x=(int)rect.x+3;x<(int)(rect.x+rect.w)-3;x++) {
            Uint8 ar,ag,ab,aa,br,bg,bb,ba;
            assert(SDL_ReadSurfacePixel(a,x,y,&ar,&ag,&ab,&aa));
            assert(SDL_ReadSurfacePixel(b,x,y,&br,&bg,&bb,&ba));
            if(ar!=br||ag!=bg||ab!=bb)return false;
        }
    return true;
}
static errr input_extra(int n,int v)
{
    if(n==TERM_XTRA_EVENT && v) {
        SDL_Event event={0};
        injected++;
        if(injected>1) {
            /* Let the baseline finish so its failure is an assertion, not a hang. */
            Term_keypress(fallback_key); return 0;
        }
        if(answer_kind==0) {
            event.type=SDL_EVENT_KEY_DOWN;
            event.key.windowID=SDL_GetWindowID(g_state.window);
            event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;
        } else {
            SDL_FRect yes,no;assert(sdl_touch_pane_yes_no_prompt_layout(NULL,NULL,&yes,&no));
            SDL_FRect rect=answer_kind==1?no:yes;
            event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.windowID=SDL_GetWindowID(g_state.window);
            event.button.which=1;event.button.button=SDL_BUTTON_LEFT;
            event.button.x=rect.x+rect.w/2;event.button.y=rect.y+rect.h/2;
        }
        sdl_handle_event(&g_state,&event);
        return 0;
    }
    return real_extra(n,v);
}
static void checks(void)
{
    assert(SDL_SetWindowSize(g_state.window,800,576));
    assert(SDL_SyncWindow(g_state.window));
    config.bigger_font=false;
    set_sdl_min_terminal_mode(SDL_MIN_TERMINAL_COMPACT);
    p_ptr->playing=true;p_ptr->is_dead=false;
    p_ptr->py=p_ptr->px=12;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=40;
    p_ptr->chp=p_ptr->mhp=100;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->csp=p_ptr->msp=34;p_ptr->food=PY_FOOD_FULL;
    p_ptr->depth=11;p_ptr->pspeed=2;
    for(int stat=0;stat<A_MAX;stat++)p_ptr->stat_base[stat]=p_ptr->stat_use[stat]=3;
    SDL_strlcpy(op_ptr->full_name,"Modal Fixture",sizeof(op_ptr->full_name));
    character_generated=character_dungeon=true;character_icky=0;
    set_sdl_main_view_scale(2);sdl_apply_config_no_redraw();
    Term_activate(term_screen);Term_clear();Term_fresh();
    turn=331;playerturn=33;
    banner();
    SDL_Surface* ordinary=frame("banner");SDL_DestroySurface(ordinary);
    sdl_touch_pane_begin_yes_no_prompt(prompt);
    SDL_FRect yes,no;assert(sdl_touch_pane_yes_no_prompt_layout(NULL,NULL,&yes,&no));
    SDL_Surface* modal=frame("modal");
    int remaining=g_banner_force_redraw_remaining;
    g_banner_force_redraw_remaining=0;
    SDL_Surface* reference=frame("reference");
    g_banner_force_redraw_remaining=remaining;
    bool unobscured=same_button(modal,reference,yes)&&same_button(modal,reference,no);
    SDL_DestroySurface(modal);SDL_DestroySurface(reference);
    sdl_touch_pane_end_yes_no_prompt();
    if(strcmp(SDL_getenv("PRIORITY_CASE"),"input"))assert(unobscured);
    assert(active_narrative_banner_visible());
    if(strcmp(SDL_getenv("PRIORITY_CASE"),"render")) {
        real_extra=Term->xtra_hook;Term->xtra_hook=input_extra;
        for(answer_kind=0;answer_kind<3;answer_kind++) {
            Term_flush();injected=0;banner();
            assert(get_check(prompt)==(answer_kind==2));
            assert(injected==1 && active_narrative_banner_visible());
            assert(g_banner_force_redraw_remaining==3);
            assert(!g_touch_pane_yes_no_prompt_active);
            assert(turn==331 && playerturn==33 && p_ptr->chp==100);
        }
        /* Interactive value pickers already hide banners visually; their
         * Escape must likewise cancel the picker without deleting its backdrop. */
        const ui_question_option options[]={
            {'a',"First answer",TERM_WHITE,false},
            {'b',"Second answer",TERM_WHITE,false}
        };
        Term_flush();injected=0;answer_kind=0;banner();
        assert(ui_question_ask_overlay("Modal picker",NULL,options,2,
            UI_QUESTION_GLOBAL,UI_QUESTION_GLOBAL,0)==-1);
        assert(injected==1 && active_narrative_banner_visible());
        assert(g_banner_force_redraw_remaining==3 && turn==331 && playerturn==33);

        /* This actual saved Help menu hides the gameplay banner, but preserves
         * it for the gameplay screen restored on Back. */
        Term_flush();injected=0;answer_kind=0;fallback_key=ESCAPE;banner();
        do_cmd_help_menu();
        printf("Saved Help Back: input events=%d banner visible=%d\n",
            injected,active_narrative_banner_visible());fflush(stdout);
        assert(injected==1 && active_narrative_banner_visible());
        assert(character_icky==0 && g_banner_force_redraw_remaining==3);

        char lore_path[1200];
        strnfmt(lore_path,sizeof(lore_path),"%s/lore.txt",SDL_getenv("CONTROL_TEST_PROFILE"));
        FILE* lore=fopen(lore_path,"w");assert(lore);
        fputs("A saved Lore screen owns Back while the gameplay narrative is hidden.\n",lore);
        fclose(lore);
        Term_flush();injected=0;banner();
        screen_save();assert(show_file(lore_path,"Lore fixture",0));screen_load();
        assert(injected==1 && active_narrative_banner_visible());
        assert(character_icky==0 && g_banner_force_redraw_remaining==3);

        /* Noninteractive busy questions own and swallow input before gameplay
         * dispatch. Escape must neither close their caller nor delete a banner. */
        Term_flush();banner();
        sdl_question_menu_begin("Busy confirmation");
        sdl_question_menu_add_entry(0,"","Please wait",TERM_WHITE);
        sdl_question_menu_set_blocking_input(true);sdl_question_menu_finish();
        SDL_Event event={0};event.type=SDL_EVENT_KEY_DOWN;
        event.key.windowID=SDL_GetWindowID(g_state.window);
        event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;
        sdl_handle_event(&g_state,&event);
        char key=0;assert(Term_inkey(&key,false,true)!=0);
        assert(sdl_question_menu_blocks_input() && active_narrative_banner_visible());
        sdl_question_menu_clear();
        assert(turn==331 && playerturn==33 && p_ptr->chp==100);
        Term->xtra_hook=real_extra;
    }
    ordinary=frame("banner-restored");SDL_DestroySurface(ordinary);
    assert(active_narrative_banner_visible());
    puts("Modal confirmation above retained narrative: button pixels, one Escape, Yes/No pointer dispatch, no time or narrative loss PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--case", choices=("all", "render", "input"), default="all")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    sources = []
    for relative in ("src/sdl/ui/sdl-menus.c", "src/sdl/core/sdl-events.c"):
        path = ROOT / relative
        if args.baseline:
            path = OUT / (Path(relative).stem + "-baseline.c")
            path.write_text(subprocess.check_output(["git", "show", "HEAD:" + relative],
                cwd=ROOT, text=True, encoding="utf-8"), encoding="utf-8")
        sources.append(str(path))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks();sdl_quit_hook(NULL);return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/sdl/ui/sdl-menus.c.obj", "/src/sdl/core/sdl-events.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"),
        PRIORITY_CASE=args.case, PRIORITY_PNG=str(OUT / ("baseline" if args.baseline else "fixed")))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *sources,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()
