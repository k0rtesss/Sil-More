#!/usr/bin/env python3
"""Render short item-setup questions and long Help over an inventory backdrop."""
from pathlib import Path
import os
import shlex
import subprocess
from check_phone_aux_layout import HARNESS as AUX

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build-standard"
OUT=ROOT/"scripts/output/phone-short-question-check"
PREFIX=AUX[:AUX.index("static void check_touch_panels(void)")]
PREFIX=PREFIX.replace("*rect=(SDL_Rect){0,0,fixture_width,fixture_height};",
    "int top=fixture_height>fixture_width?(int)(68*fixture_density):0; "
    "*rect=(SDL_Rect){0,top,fixture_width,fixture_height-top};")
INIT=AUX[AUX.index("int main(int argc,char **argv)"):AUX.index("    for(int big=0;big<2;big++)")]
HARNESS=PREFIX+r'''
static cptr setup_help="Make this item active, including a compatible Harness shield and arrows where available, or choose where to keep it. [active] marks your current setup. Only active combat gear grants combat bonuses. Changing only arrows is free. Other changes keep their normal turn cost and ability exceptions. Pack actions take three turns.";
static void background_text(cptr text,float x,float y,int px)
{
    int w=0,h=0;
    TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_MENU);
    SDL_Texture *texture=sdl_ui_text_texture(font,text,(SDL_Color){0,230,230,255},&w,&h);
    fixture_assert(texture);
    SDL_FRect dst={x,y,w,h}; SDL_RenderTexture(g_state.renderer,texture,NULL,&dst);
}
static void inventory_background(void)
{
    float dp=fixture_density;
    int top=fixture_height>fixture_width?(int)(68*dp):0;
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    background_text("< Inventory >",0,top,sdl_ui_role_font_px(SDL_UI_FONT_TITLE));
    cptr lines[]={"Carried equipment", "a) Shortsword (+0,1d7) [+1]", "Harness", "b) Wooden bow", "c) Leather armour", "d) Torch", "e) Arrows", "Volume: 3 / Weight: 9.0 lb"};
    float y=top+64*dp;
    for(int i=0;i<8;i++,y+=64*dp)
        background_text(lines[i],0,y,sdl_ui_role_font_px(SDL_UI_FONT_BODY));
}
static void draw(cptr name)
{
    inventory_background(); sdl_question_menu_render(); capture(name);
}
static void short_question(void)
{
    SDL_strlcpy(fixture_id,"short-item-question",sizeof(fixture_id));
    ui_menu_click_begin(); sdl_question_menu_begin("Choose item setup");
    sdl_question_menu_set_help(setup_help);
    sdl_question_menu_add_entry(0,"a)",
        "Active setup: Melee [active] a Shortsword (+0,1d7) [+1] (+11,1d9)",TERM_L_RED);
    sdl_question_menu_add_entry(1,"b)","Return to Harness",TERM_WHITE);
    for(int i=0;i<2;i++) {
        g_question_menu.entries[i].has_icon=true;
        g_question_menu.entries[i].icon_attr=TERM_WHITE;
        g_question_menu.entries[i].icon_char='/';
    }
    sdl_question_menu_set_highlight(0); sdl_question_menu_finish();
    sdl_question_menu_layout_info closed,opened;
    fixture_assert(sdl_question_menu_layout(&closed)); inside(closed.panel);
    fixture_assert(closed.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    fixture_assert(closed.info_rect.h>=sdl_ui_min_tap_px());
    fixture_assert(closed.close_rect.h>=sdl_ui_min_tap_px());
    fixture_assert(!g_question_menu.help_open);
    for(int i=0;i<2;i++) fixture_assert(closed.rows[i].h>=sdl_ui_min_tap_px());
    float content_h=closed.rows[0].h+closed.rows[1].h;
    if(content_h<=closed.entries_rect.h+1) {
        fixture_assert(closed.entries_rect.h-content_h<=1);
        fixture_assert(closed.max_scroll_offset==0);
        fixture_assert(closed.rows[1].y+closed.rows[1].h<=closed.panel.y+closed.panel.h);
    }
    draw("setup-closed");
    fixture_assert(sdl_question_menu_handle_pointer(closed.info_rect.x+closed.info_rect.w/2,
        closed.info_rect.y+closed.info_rect.h/2,UI_MENU_CLICK_PRIMARY));
    fixture_assert(g_question_menu.help_open && sdl_question_menu_layout(&opened));
    inside(opened.panel); fixture_assert(opened.panel.h>=closed.panel.h);
    fixture_assert(opened.help_desc_h>0 && opened.help_content_h>=content_h+opened.help_desc_h);
    draw("setup-help");
    if(opened.max_scroll_offset>0) {
        fixture_assert(sdl_question_menu_scroll_offset_by(&opened,100000));
        fixture_assert(sdl_question_menu_layout(&opened));
        fixture_assert(g_question_menu.help_scroll_offset==opened.max_scroll_offset);
        fixture_assert(opened.rows[1].y+opened.rows[1].h<=opened.entries_rect.y+opened.entries_rect.h+1);
        draw("setup-help-end");
    }
    fixture_assert(sdl_question_menu_toggle_help());
    fixture_assert(sdl_question_menu_layout(&opened));
    fixture_assert(SDL_fabsf(opened.panel.h-closed.panel.h)<=1);
    /* The original choices are still exact and usable after reading Help. */
    for(int i=0;i<2;i++) {
        int delta=(int)(opened.rows[i].y-opened.entries_rect.y);
        if(delta>0 && opened.max_scroll_offset>0) {
            g_question_menu.scroll_follow_highlight=false;
            sdl_question_menu_scroll_offset_by(&opened,delta);
            fixture_assert(sdl_question_menu_layout(&opened));
        }
        SDL_FRect hit;
        fixture_assert(SDL_GetRectIntersectionFloat(&opened.rows[i],&opened.entries_rect,&hit));
        fixture_assert(hit.h>=sdl_ui_min_tap_px());
        fixture_assert(sdl_question_menu_handle_pointer(hit.x+hit.w/2,hit.y+hit.h/2,UI_MENU_CLICK_PRIMARY));
        int choice=-1,action=0;
        fixture_assert(ui_menu_click_take_action(&choice,&action));
        fixture_assert(choice==i && action==UI_MENU_CLICK_PRIMARY);
    }
    sdl_question_menu_clear(); ui_menu_click_clear();
}
''' + INIT + r'''
    for(int big=0;big<2;big++) {config.bigger_font=big; fixture.active=false; short_question();}
    printf("Short question %dx%d @%.3f Off/On natural height, Help paging, IDs: PASS\n",
        fixture_width,fixture_height,fixture_density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/"check.c"; source.write_text(HARNESS,encoding="utf-8")
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
