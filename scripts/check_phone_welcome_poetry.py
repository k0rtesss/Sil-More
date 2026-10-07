#!/usr/bin/env python3
"""Check actual welcome/poetry texture scale, wrapped bands and input dispatch."""
from pathlib import Path
import os
import shlex
import subprocess
from check_phone_aux_layout import HARNESS as AUX

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build-standard"
OUT=ROOT/"scripts/output/phone-welcome-poetry-check"
PREFIX=AUX[:AUX.index("static void check_touch_panels(void)")]
PREFIX=PREFIX.replace("footer_glyphs[64]","footer_glyphs[512]").replace("footer_glyph_count<64","footer_glyph_count<512")
PREFIX=PREFIX.replace("if(footer_capture && dest) {",r'''if(footer_capture && dest) {
        float tw=0,th=0; fixture_assert(SDL_GetTextureSize(texture,&tw,&th));
        float sw=source?source->w:tw,sh=source?source->h:th;
        if(SDL_fabsf(dest->w-sw)>1 || SDL_fabsf(dest->h-sh)>1)
            fprintf(stderr,"shrunk texture %.1fx%.1f -> %.1fx%.1f\n",sw,sh,dest->w,dest->h);
        fixture_assert(SDL_fabsf(dest->w-sw)<=1 && SDL_fabsf(dest->h-sh)<=1);
''')
INIT=AUX[AUX.index("int main(int argc,char **argv)"):AUX.index("    for(int big=0;big<2;big++)")]
HARNESS=PREFIX+r'''
#include "sdl/ui/sdl-screens.c"
#include "sdl/input/sdl-screen-pointer.c"
bool __wrap_sdl_touch_pane_point_to_slot(float x,float y,int* slot) {return false;}
static void start_capture(void) {footer_glyph_count=0;footer_capture=true;}
static void stop_capture(void) {footer_capture=false;fixture_assert(footer_glyph_count>0);}
static void welcome(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    SDL_strlcpy(fixture_id,"welcome-full-size",sizeof(fixture_id));
    for(int style=0;style<INTRO_STYLE_MAX;style++) {
        fixture_assert(sdl_welcome_screen_show_intro(style,true));
        fixture_assert(sdl_welcome_screen_show_menu(true,false));
        g_sdl_standalone_pager.offset=0;
        start_capture(); sdl_welcome_screen_render_canvas(&canvas); stop_capture();
        sdl_welcome_layout_line lines[SDL_WELCOME_MAX_LINES];
        sdl_welcome_layout_metrics metrics={0};
        int count=sdl_welcome_prepare_layout(&canvas,lines,SDL_WELCOME_MAX_LINES,&metrics);
        fixture_assert(count>0);
        for(int i=0;i<count;i++) {
            int w=0,h=0;
            fixture_assert(TTF_GetStringSizeWrapped(lines[i].font,
                sdl_welcome_display_text(lines[i].source->text),0,(int)lines[i].box.w,&w,&h));
            fixture_assert(lines[i].box.h+1>=h);
            TTF_Font* expected=sdl_story_font_for_height_slot(
                sdl_welcome_font_px_for_role(metrics.base_px,lines[i].source->role),
                sdl_welcome_slot_for_role(lines[i].source->role));
            fixture_assert(TTF_GetFontSize(expected)==TTF_GetFontSize(lines[i].font));
        }
        inside(g_sdl_welcome_screen.continue_rect); inside(g_sdl_welcome_screen.quit_rect);
        ink(g_sdl_welcome_screen.continue_rect,sdl_ui_role_font_px(SDL_UI_FONT_CONTROL));
        ink(g_sdl_welcome_screen.quit_rect,sdl_ui_role_font_px(SDL_UI_FONT_CONTROL));
        fixture_assert(g_sdl_welcome_screen.continue_rect.h>=sdl_ui_min_tap_px());
        fixture_assert(!SDL_HasRectIntersectionFloat(&g_sdl_welcome_screen.continue_rect,&g_sdl_welcome_screen.quit_rect));
        char name[96]; strnfmt(name,sizeof(name),"welcome-style%d-start",style); capture(name);
        if(g_sdl_standalone_pager.maximum>0) {
            float old=g_sdl_standalone_pager.offset;
            SDL_FRect next=g_sdl_standalone_pager.buttons[1];
            Term_flush(); g_sdl_blocking_key_wait=true;
            fixture_assert(sdl_pointer_activate_welcome_screen_at(next.x+next.w/2,next.y+next.h/2));
            fixture_assert(g_sdl_standalone_pager.offset>old);
            char key=0; fixture_assert(Term_inkey(&key,false,true)!=0);
            sdl_welcome_screen_render_canvas(&canvas);
            float x=metrics.column_x+metrics.column_w*.5f,y=metrics.top+80*fixture_density;
            old=g_sdl_standalone_pager.offset;
            fixture_assert(sdl_welcome_touch_handle_pointer_down(x,y,7));
            fixture_assert(sdl_welcome_touch_handle_pointer_motion(x,y-80*fixture_density,7));
            fixture_assert(sdl_welcome_touch_handle_pointer_up(x,y-80*fixture_density,7));
            fixture_assert(g_sdl_standalone_pager.offset>=old);
            fixture_assert(Term_inkey(&key,false,true)!=0);
            g_sdl_standalone_pager.offset=g_sdl_standalone_pager.maximum;
            start_capture(); sdl_welcome_screen_render_canvas(&canvas); stop_capture();
            sdl_welcome_layout_line last=lines[count-1];
            last.box.y-=g_sdl_standalone_pager.offset;
            int tw=0,th=0;
            fixture_assert(TTF_GetStringSizeWrapped(last.font,
                sdl_welcome_display_text(last.source->text),0,(int)last.box.w,&tw,&th));
            fixture_assert(last.box.y>=metrics.top-1);
            fixture_assert(last.box.y+last.box.h<=g_sdl_standalone_pager.buttons[0].y+1);
            ink(last.box,sdl_welcome_font_px_for_role(metrics.base_px,last.source->role));
            strnfmt(name,sizeof(name),"welcome-style%d-end",style); capture(name);
        }
        Term_flush(); g_sdl_blocking_key_wait=true;
        SDL_FRect button=g_sdl_welcome_screen.continue_rect;
        fixture_assert(sdl_pointer_activate_welcome_screen_at(button.x+button.w/2,button.y+button.h/2));
        char key=0; fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
        button=g_sdl_welcome_screen.quit_rect;
        fixture_assert(sdl_pointer_activate_welcome_screen_at(button.x+button.w/2,button.y+button.h/2));
        fixture_assert(Term_inkey(&key,false,true)==0 && key==ESCAPE);
    }
    fixture_assert(sdl_welcome_screen_show_loading("Loading the next chapter of this long and remembered tale..."));
    start_capture(); sdl_welcome_screen_render_canvas(&canvas); stop_capture(); capture("welcome-loading");
    fixture_assert(sdl_welcome_screen_set_status("The chapter is ready."));
    start_capture(); sdl_welcome_screen_render_canvas(&canvas); stop_capture();
    sdl_welcome_screen_hide(); g_sdl_blocking_key_wait=false;
}
static void poetry(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    static cptr prompts[]={
        "[Press any key to continue your tale]", "[Press any key to face temptation]",
        "[Press any key to continue]", "[Press any key to face the echoes]",
        "[Press any key to conclude your tale]", "[Press any key to witness the consequences]",
        "[Press any key to return to Middle-earth]", "[Press any key to face your destiny]",
        "[Press any key to face your judgment]", "[Press any key to return to the game]",
        "[Press any key to close the chronicle]"};
    SDL_strlcpy(fixture_id,"poetry-complete-prompts",sizeof(fixture_id));
    for(int i=0;i<(int)N_ELEMENTS(prompts);i++) {
        sdl_poetry_screen_begin("The tale is concluded",
            "Their mingled light is dead and gone, for the beauty of Telperion remains a memory.\n\nRemember this hero and every decision made in the depths of Angband.",
            "Your next chapter awaits.",prompts[i]);
        sdl_poetry_screen_update(true,TERM_YELLOW,true,TERM_WHITE,true,TERM_L_BLUE,true);
        sdl_poetry_screen_set_alpha(255,255,255,255);
        start_capture(); sdl_poetry_screen_render_canvas(&canvas); stop_capture();
        SDL_FRect prompt=footer_glyphs[footer_glyph_count-1]; inside(prompt);
        int w=0,h=0;
        TTF_Font* font=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_CONTROL),SDL_WELCOME_STORY_FONT_SLOT);
        SDL_FRect content=sdl_welcome_content_rect(&canvas);
        fixture_assert(TTF_GetStringSizeWrapped(font,prompts[i],0,(int)content.w,&w,&h));
        fixture_assert(SDL_fabsf(prompt.h-h)<=1 && SDL_fabsf(prompt.w-w)<=1);
        fixture_assert(prompt.y+prompt.h<=canvas.h-sdl_welcome_bottom_margin(&canvas)+1);
        SDL_FRect ink_box=prompt; float inset=4*fixture_density;
        ink_box.x-=inset;ink_box.y-=inset;ink_box.w+=2*inset;ink_box.h+=2*inset;
        ink(ink_box,sdl_ui_role_font_px(SDL_UI_FONT_CONTROL));
        char name[96]; strnfmt(name,sizeof(name),"poetry-prompt%d",i); capture(name);
    }
    sdl_poetry_screen_begin_choices("Choose the next path of your remembered tale");
    for(int i=0;i<3;i++) {
        sdl_poetry_screen_add_choice(i,"A complete descriptive choice that can wrap onto another line",
            "The complete consequence remains readable at the selected font size.");
        sdl_poetry_screen_set_choice_visible(i,true,TERM_WHITE,TERM_WHITE);
        sdl_poetry_screen_set_choice_alpha(i,255);
    }
    sdl_poetry_screen_set_alpha(255,255,255,255);
    sdl_poetry_screen_set_prompt("[Choose a path, or return to Middle-earth without accepting this fate]",true);
    start_capture(); sdl_poetry_screen_render_canvas(&canvas); stop_capture();
    inside(g_sdl_poetry_screen.prompt_rect); capture("poetry-choices");
    sdl_poetry_screen_hide();
}
''' + INIT + r'''
    for(int i=0;i<16;i++) {
        angband_color_table[i][1]=angband_color_table[i][2]=angband_color_table[i][3]=220;
    }
    for(int big=0;big<2;big++) {config.bigger_font=big;fixture.active=false;welcome();poetry();}
    printf("Welcome/poetry %dx%d @%.3f Off/On texture1:1, all intros/prompts/actions/paging: PASS\n",
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
        "/src/sdl/ui/sdl-song-menu.c.obj","/src/sdl/ui/sdl-question-menu.c.obj",
        "/src/sdl/ui/sdl-screens.c.obj","/src/sdl/input/sdl-screen-pointer.c.obj")
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)))
    env=os.environ.copy();env["PATH"]=os.pathsep.join(
        [str(BUILD/"_deps"/dep) for dep in ["SDL","SDL_ttf","SDL_image","SDL_mixer"]]
        +["C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    wraps=["tutorial_get_view","tutorial_is_active","tutorial_revision","get_sdl_gameplay_tutorial_mode",
        "SDL_WaitEvent","sdl_touch_round_layer_controls_active","sdl_touch_round_compute_layout",
        "sdl_touch_thumb_current_bounds","sdl_map_grid_cell_rect","sdl_touch_only_device_active",
        "sdl_touch_only_mobile_device_active","sdl_get_layout_screen_rect","sdl_overlay_pane_anchor_rect",
        "SDL_GetDisplayContentScale","sdl_terminal_menu_font_px","sdl_mobile_lifecycle_handle_event",
        "sdl_touch_tutorial_device_available","SDL_RenderTexture","sdl_touch_pane_point_to_slot"]
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
