#!/usr/bin/env python3
"""Verify complete multi-paragraph question prose and choices at phone density.

--negative disables the description scroll stream in the same source to
demonstrate that the reachability check detects clipped prose.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
from check_phone_short_question import PREFIX, INIT

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build-standard"
OUT=ROOT/"scripts/output/phone-question-description-check"
HARNESS=PREFIX+r'''
static void frame(cptr name)
{
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render(); capture(name);
}
static void paragraph_ink(SDL_FRect rect,int px)
{
    float inset=4*fixture_density;
    rect.x-=inset; rect.y-=inset; rect.w+=2*inset; rect.h+=2*inset;
    ink(rect,px);
}
static void description(void)
{
    char prose[8192]="FIRST PARAGRAPH BEGIN\n\n";
    for(int paragraph=0;paragraph<3;paragraph++) {
        for(int i=0;i<14;i++) SDL_strlcat(prose,
            "Read every instruction before choosing. Your action keeps its normal turn cost and remains a deliberate decision. ",sizeof(prose));
        SDL_strlcat(prose,"\n\n",sizeof(prose));
    }
    SDL_strlcat(prose,"LAST PARAGRAPH END",sizeof(prose));
    SDL_strlcpy(fixture_id,"complete-normal-description",sizeof(fixture_id));
    ui_menu_click_begin(); sdl_question_menu_begin("Read instructions");
    sdl_question_menu_set_desc(prose);
    sdl_question_menu_add_entry(11,"a)","Accept this choice",TERM_WHITE);
    sdl_question_menu_add_entry(22,"b)","Keep the current choice",TERM_WHITE);
    sdl_question_menu_add_button(33,"Back",TERM_WHITE);
    sdl_question_menu_set_highlight(11); sdl_question_menu_finish();
    fixture_assert(!g_question_menu.help_mode && !g_question_menu.help_open);
    fixture_assert(strlen(sdl_question_menu_description())==strlen(prose));
    sdl_question_menu_layout_info layout;
    fixture_assert(sdl_question_menu_layout(&layout));
    fixture_assert(layout.description_in_list && layout.scrollable);
    fixture_assert(g_question_menu.scroll_offset_ptr && *g_question_menu.scroll_offset_ptr==0);
    int px=sdl_ui_role_font_px(SDL_UI_FONT_BODY);
    TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_MENU);
    int w=0,h=0; fixture_assert(TTF_GetStringSizeWrapped(font,prose,0,(int)(layout.entries_rect.w+.5f),&w,&h));
    fixture_assert(SDL_fabsf(layout.scrolling_desc_h-h)<=1);
    int line_h=TTF_GetFontHeight(font);
    frame("description-first");
    paragraph_ink((SDL_FRect){layout.entries_rect.x,layout.entries_rect.y,
        layout.entries_rect.w,line_h},px);
    float x=layout.entries_rect.x+layout.entries_rect.w/2;
    float y=layout.entries_rect.y+layout.entries_rect.h*.7f;
    fixture_assert(sdl_question_menu_handle_touch_down(x,y,9));
    fixture_assert(sdl_question_menu_handle_touch_motion(x,y-60*fixture_density,9));
    fixture_assert(sdl_question_menu_handle_touch_up(x,y-60*fixture_density,9));
    fixture_assert(*g_question_menu.scroll_offset_ptr>0 && !ui_menu_click_has_pending());
    fixture_assert(sdl_question_menu_layout(&layout));
    int last_offset=MAX(0,h-line_h-(int)(layout.entries_rect.h/2));
    g_question_menu.scroll_follow_highlight=false;
    sdl_question_menu_scroll_offset_by(&layout,last_offset-*g_question_menu.scroll_offset_ptr);
    fixture_assert(sdl_question_menu_layout(&layout)); frame("description-last");
    SDL_FRect last={layout.entries_rect.x,
        layout.entries_rect.y+h-line_h-*g_question_menu.scroll_offset_ptr,
        layout.entries_rect.w,line_h};
    fixture_assert(last.y>=layout.entries_rect.y);
    fixture_assert(last.y+last.h<=layout.entries_rect.y+layout.entries_rect.h+1);
    paragraph_ink(last,px);
    sdl_question_menu_scroll_offset_by(&layout,100000);
    fixture_assert(sdl_question_menu_layout(&layout)); frame("description-choices");
    fixture_assert(*g_question_menu.scroll_offset_ptr==layout.max_scroll_offset);
    SDL_FRect hit;
    fixture_assert(SDL_GetRectIntersectionFloat(&layout.rows[1],&layout.entries_rect,&hit));
    fixture_assert(hit.h>=sdl_ui_min_tap_px());
    fixture_assert(sdl_question_menu_handle_pointer(hit.x+hit.w/2,hit.y+hit.h/2,UI_MENU_CLICK_PRIMARY));
    int choice=0,action=0;
    fixture_assert(ui_menu_click_take_action(&choice,&action));
    fixture_assert(choice==22 && action==UI_MENU_CLICK_PRIMARY);
    /* Semantic navigation still follows the selected row after reading. */
    sdl_question_menu_set_highlight(11);
    fixture_assert(sdl_question_menu_layout(&layout));
    fixture_assert(layout.rows[0].y>=layout.entries_rect.y-1);
    Term_flush();
    fixture_assert(sdl_question_menu_handle_pointer(layout.close_rect.x+layout.close_rect.w/2,
        layout.close_rect.y+layout.close_rect.h/2,UI_MENU_CLICK_PRIMARY));
    char key=0; fixture_assert(Term_inkey(&key,false,true)==0 && key==ESCAPE);
    sdl_question_menu_clear(); fixture_assert(!g_question_full_description);
    ui_menu_click_clear();
}
''' + INIT + r'''
    for(int big=0;big<2;big++) {config.bigger_font=big; fixture.active=false; description();}
    printf("Question description %dx%d @%.3f Off/On full prose, glyphs, swipe/choice/cancel: PASS\n",
        fixture_width,fixture_height,fixture_density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''


def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--negative",action="store_true"); args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    harness=HARNESS
    if args.negative:
        old=(ROOT/"src/sdl/ui/sdl-question-menu.c").read_text().replace(
            "out->description_in_list = true;", "out->description_in_list = false;")
        (OUT/"question-negative.c").write_text(old,encoding="utf-8")
        harness=harness.replace('#include "sdl/ui/sdl-question-menu.c"','#include "question-negative.c"')
    source=OUT/"check.c"; source.write_text(harness,encoding="utf-8")
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=("/src/main.c.obj","/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
        "/src/sdl/input/sdl-touch-tutorial.c.obj","/src/sdl/render/sdl-fonts.c.obj",
        "/src/sdl/ui/sdl-song-menu.c.obj","/src/sdl/ui/sdl-question-menu.c.obj")
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)))
    env=os.environ.copy(); env["PATH"]=os.pathsep.join(
        [str(BUILD/"_deps"/dep) for dep in ["SDL","SDL_ttf","SDL_image","SDL_mixer"]]
        +["C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    wraps=["tutorial_get_view","tutorial_is_active","tutorial_revision","get_sdl_gameplay_tutorial_mode",
        "SDL_WaitEvent","sdl_touch_round_layer_controls_active","sdl_touch_round_compute_layout",
        "sdl_touch_thumb_current_bounds","sdl_map_grid_cell_rect","sdl_touch_only_device_active",
        "sdl_touch_only_mobile_device_active","sdl_get_layout_screen_rect","sdl_overlay_pane_anchor_rect",
        "SDL_GetDisplayContentScale","sdl_terminal_menu_font_px","sdl_mobile_lifecycle_handle_event",
        "sdl_touch_tutorial_device_available","SDL_RenderTexture"]
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),"@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp","-Wl,"+",".join("--wrap="+w for w in wraps),
        "-o",str(exe)],cwd=BUILD,env=env,check=True)
    for width,height,density in [(720,1600,2),(1080,2340,2.75),(1080,2400,2.625)]:
        for w,h in [(width,height),(height,width)]:
            subprocess.run([str(exe),str(w),str(h),str(density),
                str(ROOT/"lib/xtra/font/EBGaramond-Regular.ttf"),str(ROOT/"lib/xtra/font/Cinzel-Medium.ttf"),
                str(ROOT/"lib/xtra/font/VictorMono-Medium.ttf")],cwd=OUT,env=env,check=True,timeout=60)


if __name__=="__main__": main()
