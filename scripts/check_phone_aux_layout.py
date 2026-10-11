#!/usr/bin/env python3
"""Check native phone questions, songs and tutorial type at physical density.

Compiles the owned renderers privately against configured SDL objects. Leaves
screenshots in scripts/output/phone-aux-check; never opens user data.
"""
from pathlib import Path
import os
import shlex
import subprocess
from check_gameplay_tutorial_render import HARNESS as CARD
from check_bigger_font_layout import HARNESS as BASE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/phone-aux-check"
PREFIX = CARD[:CARD.index("static void field(")].replace(
    '#define sdl_touch_tutorial_draw_text_line',
    '#include "sdl/main-sdl-private.h"\n#undef SIL_SDL_MOBILE_BUILD\n'
    '#define SIL_SDL_MOBILE_BUILD 1\n#include "sdl/render/sdl-fonts.c"\n'
    '#include "sdl/input/sdl-touch-tutorial.c"\n'
    'float fixture_draw_text_line(cptr,float,float,float,int,SDL_Color,bool);\n'
    '#define sdl_touch_tutorial_draw_text_line', 1
).replace('float scale=w>width && width>0?width/w:1;',
          'if(width>0) TTF_GetStringSizeWrapped(font,text,0,(int)width,&w,&h); float scale=1;')
HELPERS = BASE[BASE.index("static void inside(SDL_FRect r)\n{"):BASE.index("static void check_config(void)")]
CHECKS = BASE[BASE.index("static void check_songs(void)"):BASE.index("static void check_character_allocation(void)")]
CHECKS = CHECKS.replace('fixture_assert(layout.visible_count>0);',
    'fixture_assert(layout.visible_count>0); fixture_assert(layout.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY)); '
    'for(int i=0;i<g_song_menu.count;i++) if(layout.rows[i].w>0) '
    '{ fixture_assert(layout.rows[i].h>=sdl_ui_min_tap_px()); inside(layout.rows[i]); }')
CHECKS = CHECKS.replace('fixture_assert(layout.button_count==3);',
    'fixture_assert(layout.button_count==3); fixture_assert(layout.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY)); '
    'fixture_assert(layout.row_h>=sdl_ui_min_tap_px()); '
    'if(layout.close_button) fixture_assert(layout.close_rect.h>=sdl_ui_min_tap_px());')
CHECKS = CHECKS.replace('sdl_song_menu_render(); capture("songs-last-page");',
    'sdl_song_menu_render(); capture("songs-last-page"); '
    'ink(layout.rows[SDL_SONG_MENU_MAX_ENTRIES-1],layout.font_px);')
CHECKS = CHECKS.replace('sdl_question_menu_render(); capture("question-actions");',
    'sdl_question_menu_render(); capture("question-actions"); '
    'fixture_assert(SDL_GetRectIntersectionFloat(&layout.buttons[2],&layout.entries_rect,&visible)); '
    'ink(visible,layout.font_px);')
CHECKS = CHECKS.replace('sdl_question_menu_set_highlight(17);',
    'if(!layout.actions_in_list) for(int i=0;i<3;i++) { '
    'fixture_assert(layout.buttons[i].h>=sdl_ui_min_tap_px()); '
    'fixture_assert(layout.buttons[i].w>=sdl_ui_min_tap_px()); '
    'ink(layout.buttons[i],layout.font_px); tap(layout.buttons[i]); '
    'int actual=0,action=0; char key=0; '
    'fixture_assert(ui_menu_click_take_action(&actual,&action)); '
    'fixture_assert(actual==101+i && action==UI_MENU_CLICK_PRIMARY); '
    'fixture_assert(Term_inkey(&key,false,true)==0 && key==\'\\r\'); } '
    'sdl_question_menu_set_highlight(17);')
TUTORIAL = BASE[BASE.index("static void check_large_tutorial(void)"):BASE.index("static void check_live_sheet(void)")]
# The legacy check_large_tutorial can precede check_live_sheet in either order.
if not TUTORIAL:
    TUTORIAL = BASE[BASE.index("static void check_large_tutorial(void)"):BASE.index("static void check_welcome_cache(void)")]
TUTORIAL = TUTORIAL[:TUTORIAL.index("\nstatic void", 1)] if "\nstatic void" in TUTORIAL[1:] else TUTORIAL
TUTORIAL = TUTORIAL.replace('for(int b=0;b<3;b++) inside(tutorial_buttons[b]);',
    'for(int b=0;b<3;b++) { inside(tutorial_buttons[b]); '
    'fixture_assert(tutorial_buttons[b].h>=sdl_ui_min_tap_px()); '
    'fixture_assert(tutorial_buttons[b].w>=sdl_ui_min_tap_px()); '
    'ink(tutorial_buttons[b],sdl_ui_role_font_px(SDL_UI_FONT_CONTROL)); }')
HARNESS = PREFIX + r'''
#include "sdl/ui/sdl-song-menu.c"
#include "sdl/ui/sdl-question-menu.c"
static float fixture_density;
static bool footer_capture;
static SDL_FRect footer_glyphs[64];
static int footer_glyph_count;
bool __real_SDL_RenderTexture(SDL_Renderer*,SDL_Texture*,const SDL_FRect*,const SDL_FRect*);
bool __wrap_SDL_RenderTexture(SDL_Renderer* renderer,SDL_Texture* texture,
    const SDL_FRect* source,const SDL_FRect* dest)
{
    if(footer_capture && dest) {
        fixture_assert(footer_glyph_count<64);
        footer_glyphs[footer_glyph_count++]=*dest;
    }
    return __real_SDL_RenderTexture(renderer,texture,source,dest);
}
bool __wrap_sdl_touch_tutorial_device_available(void) { return true; }
int __wrap_sdl_terminal_menu_font_px(void) { return sdl_ui_role_font_px(SDL_UI_FONT_BODY); }
bool __wrap_sdl_mobile_lifecycle_handle_event(const SDL_Event *ev) { return false; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id) { return fixture_density; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
bool __wrap_sdl_touch_only_mobile_device_active(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect *rect)
{ *rect=(SDL_Rect){0,0,fixture_width,fixture_height}; return true; }
static void capture(cptr name)
{
    char path[180]; strnfmt(path,sizeof(path),"%s-%dx%d-%.3f-%s.png",name,
        fixture_width,fixture_height,fixture_density,config.bigger_font?"on":"off");
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels && IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
}
static void ink(SDL_FRect box,int px)
{
    float inset=4*sdl_ui_density_scale();
    SDL_Rect area={(int)(box.x+inset),(int)(box.y+inset),
        MAX(1,(int)(box.w-2*inset)),MAX(1,(int)(box.h-2*inset))};
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&area);
    fixture_assert(pixels); int count=0,first=area.h,last=-1;
    for(int y=0;y<pixels->h;y++) for(int x=0;x<pixels->w;x++) {
        Uint8 r,g,b,a; fixture_assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
        if(r>150 && g>150 && b>150) {count++; first=MIN(first,y); last=MAX(last,y);}
    }
    SDL_DestroySurface(pixels);
    fixture_assert(count>px && last-first+1>=px*.4f);
}
''' + HELPERS + CHECKS + TUTORIAL + r'''
static void check_touch_panels(void)
{
    SDL_strlcpy(fixture_id,"touch-panels",sizeof(fixture_id));
    SDL_Rect screen={0,0,fixture_width,fixture_height};
    sdl_touch_tutorial_header_layout header;
    fixture_assert(sdl_touch_tutorial_header_compute(&screen,"Choose Touch Preset",
        "Pick the <t>control layout</t> to use now, or <a>replay the tutorial</a> before choosing.",
        0,0,sdl_touch_tutorial_default_header_y(&screen),&header));
    fixture_assert(header.title_px==sdl_ui_role_font_px(SDL_UI_FONT_TITLE));
    fixture_assert(header.body_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    SDL_FRect choices[SDL_TOUCH_TUTORIAL_CHOICE_COUNT];
    g_touch_profile_list.offset=0;
    sdl_touch_tutorial_choice_layout(&screen,choices);
    for(int i=0;i<SDL_TOUCH_TUTORIAL_CHOICE_COUNT;i++) {
        SDL_FRect full=g_touch_profile_list.cards[i];
        g_touch_profile_list.offset=sdl_touch_pane_clampf(
            g_touch_profile_list.offset+full.y-g_touch_profile_list.viewport.y,
            0,g_touch_profile_list.maximum);
        sdl_touch_tutorial_choice_layout(&screen,choices);
        inside(choices[i]); fixture_assert(choices[i].h>=sdl_ui_min_tap_px());
        fixture_assert(choices[i].y>=header.panel.y+header.panel.h);
        fixture_assert(sdl_touch_tutorial_choice_hit(choices,
            choices[i].x+choices[i].w/2,choices[i].y+choices[i].h/2)==i);
        for(int j=0;j<i;j++) if(choices[j].w>0)
            fixture_assert(!SDL_HasRectIntersectionFloat(&choices[i],&choices[j]));
    }
    fixture_assert(g_touch_profile_list.offset==g_touch_profile_list.maximum);
    SDL_FRect last=g_touch_profile_list.cards[SDL_TOUCH_TUTORIAL_CHOICE_COUNT-1];
    fixture_assert(last.y+last.h<=g_touch_profile_list.viewport.y+g_touch_profile_list.viewport.h+1);
    g_touch_profile_list.offset=0; sdl_touch_tutorial_choice_layout(&screen,choices);
    SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255); SDL_RenderClear(g_state.renderer);
    sdl_touch_tutorial_draw_header(&screen,"Choose Touch Preset",
        "Pick the <t>control layout</t> to use now, or <a>replay the tutorial</a> before choosing.",0,0);
    SDL_Rect clip={(int)g_touch_profile_list.viewport.x,(int)g_touch_profile_list.viewport.y,
        (int)g_touch_profile_list.viewport.w,(int)g_touch_profile_list.viewport.h};
    SDL_SetRenderClipRect(g_state.renderer,&clip);
    for(int i=0;i<SDL_TOUCH_TUTORIAL_CHOICE_COUNT;i++)
        sdl_touch_tutorial_draw_choice_card(&g_touch_profile_list.cards[i],
            &sdl_touch_tutorial_choices[i],i,i==0,i==sdl_touch_tutorial_current_choice_index());
    SDL_SetRenderClipRect(g_state.renderer,NULL);
    capture("touch-profile");
}
static void check_touch_footer(void)
{
    SDL_strlcpy(fixture_id,"touch-footer-safe-area",sizeof(fixture_id));
    int top=fixture_height>fixture_width ? (int)(68*fixture_density) : 0;
    SDL_Rect screen={0,top,fixture_width,fixture_height-top};
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    SDL_SetRenderDrawColor(g_state.renderer,6,28,42,255);
    SDL_RenderFillRect(g_state.renderer,&(SDL_FRect){0,top,fixture_width,fixture_height-top});
    float header_bottom=sdl_touch_tutorial_draw_header(&screen,"Dungeon & Menus","",0,3);
    sdl_touch_tutorial_draw_info_panel_before(&screen,20,header_bottom+8*fixture_density,
        fixture_width-40,"Touch shortcuts",
        "<t>Map/player:</t> <a>tap</a> to path or target; <a>hold</a> for actions.",
        screen.y+screen.h-sdl_touch_tutorial_footer_height(&screen));
    tutorial_panel_part=0; tutorial_panel_parts=1;
    SDL_Rect previous_clip={screen.x,screen.y,screen.w,screen.h-24};
    SDL_SetRenderClipRect(g_state.renderer,&previous_clip);
    footer_glyph_count=0; footer_capture=true;
    sdl_touch_tutorial_draw_footer(&screen,false,false);
    footer_capture=false;
    SDL_Rect restored_clip;
    SDL_GetRenderClipRect(g_state.renderer,&restored_clip);
    fixture_assert(SDL_RenderClipEnabled(g_state.renderer));
    fixture_assert(restored_clip.h==previous_clip.h && restored_clip.y==previous_clip.y);
    SDL_SetRenderClipRect(g_state.renderer,NULL);
    fixture_assert(footer_glyph_count>=9);
    int px=sdl_ui_role_font_px(SDL_UI_FONT_BODY);
    TTF_Font *font=sdl_touch_tutorial_font_for_height(px);
    float bottom_pad=MAX(8*fixture_density,8);
    for(int i=0;i<footer_glyph_count;i++) {
        SDL_FRect rect=footer_glyphs[i]; inside(rect);
        fixture_assert(rect.y>=screen.y);
        fixture_assert(rect.y+rect.h<=screen.y+screen.h-bottom_pad+1);
        fixture_assert(rect.h>=TTF_GetFontHeight(font));
        SDL_FRect probe=rect;
        float inset=4*fixture_density;
        probe.x-=inset; probe.y-=inset;
        probe.w+=2*inset; probe.h+=2*inset;
        ink(probe,px);
    }
    capture("touch-footer-safe-area");
}
static void check_question_backdrop(void)
{
    SDL_strlcpy(fixture_id,"question-backdrop",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_question_menu_begin("Gameplay Tutorial Mode");
    sdl_question_menu_set_desc("Choose Disabled, Normal or Extended. Normal teaches core controls and survival; Extended adds detailed mechanics.");
    sdl_question_menu_add_entry(0,"d)","Disabled",TERM_WHITE);
    sdl_question_menu_add_entry(1,"n)","Normal",TERM_WHITE);
    sdl_question_menu_add_entry(2,"e)","Extended",TERM_WHITE);
    sdl_question_menu_finish();
    sdl_question_menu_layout_info layout;
    fixture_assert(sdl_question_menu_layout(&layout));
    SDL_Rect area={(int)layout.panel.x+2,(int)layout.panel.y+2,
        (int)layout.panel.w-4,(int)layout.panel.h-4};
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render();
    SDL_Surface *reference=SDL_RenderReadPixels(g_state.renderer,&area);
    fixture_assert(reference);
    /* A white inherited Options selection is the strongest possible contrast.
     * Deliberately leave an unrelated pane clip to catch partial backdrop fills. */
    SDL_SetRenderDrawColor(g_state.renderer,255,255,255,255); SDL_RenderClear(g_state.renderer);
    SDL_Rect old_clip={0,0,fixture_width/2,fixture_height};
    SDL_SetRenderClipRect(g_state.renderer,&old_clip);
    sdl_question_menu_render();
    SDL_Surface *over_white=SDL_RenderReadPixels(g_state.renderer,&area);
    fixture_assert(over_white && over_white->w==reference->w && over_white->h==reference->h);
    for(int y=0;y<reference->h;y++) for(int x=0;x<reference->w;x++) {
        Uint8 r1,g1,b1,a1,r2,g2,b2,a2;
        fixture_assert(SDL_ReadSurfacePixel(reference,x,y,&r1,&g1,&b1,&a1));
        fixture_assert(SDL_ReadSurfacePixel(over_white,x,y,&r2,&g2,&b2,&a2));
        fixture_assert(r1==r2 && g1==g2 && b1==b2 && a1==a2);
    }
    SDL_DestroySurface(reference); SDL_DestroySurface(over_white);
    capture("question-opaque-backdrop");
    sdl_question_menu_clear(); ui_menu_click_clear();
}
int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0}; player_other options={0};
    object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    fixture_width=atoi(argv[1]); fixture_height=atoi(argv[2]); fixture_density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font2,argv[4],sizeof(config.story_font2));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.monospace_font,argv[6],sizeof(config.monospace_font));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM; config.use_unsafe_area=true;
    g_state.system_scale=fixture_density; z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    g_state.window=SDL_CreateWindow("Phone aux fixture",fixture_width,fixture_height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window); g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer); g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256); Term_activate(&view->t); term_screen=&view->t;
    character_icky=1; character_generated=character_dungeon=true; player.playing=true;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    for(int big=1;big<2;big++) {
        config.bigger_font=big; fixture.active=false;
        check_songs(); check_questions(); check_large_tutorial(); check_touch_panels();
        check_touch_footer();
        check_question_backdrop();
    }
    printf("Aux %dx%d @%.3f Big font songs/questions/tutorials: PASS\n",fixture_width,fixture_height,fixture_density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/input/sdl-touch-tutorial.c.obj", "/src/sdl/render/sdl-fonts.c.obj",
                "/src/sdl/ui/sdl-song-menu.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_touch_only_mobile_device_active",
             "sdl_get_layout_screen_rect", "sdl_overlay_pane_anchor_rect", "SDL_GetDisplayContentScale",
             "sdl_terminal_menu_font_px", "sdl_mobile_lifecycle_handle_event",
             "sdl_touch_tutorial_device_available", "SDL_RenderTexture"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap="+w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width,height,density in [(720,1600,2),(1080,2340,2.75),(1080,2400,2.625)]:
        for w,h in [(width,height),(height,width)]:
            subprocess.run([str(exe),str(w),str(h),str(density),
                str(ROOT/"lib/xtra/font/EBGaramond-Regular.ttf"),
                str(ROOT/"lib/xtra/font/Cinzel-Medium.ttf"),
                str(ROOT/"lib/xtra/font/VictorMono-Medium.ttf")],cwd=OUT,env=env,check=True,timeout=60)


if __name__ == "__main__":
    main()
