#!/usr/bin/env python3
"""Render Big font character-sheet pages and replay navigation without player files."""
from pathlib import Path
import os
import shlex
import subprocess

from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/debug-character-sheet"

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#include "sdl/ui/sdl-screens.c"

static float fixture_density=1.0f;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return fixture_density; }
#ifdef SIL_IOS
void sdl_ios_request_orientation(bool portrait) {}
void sdl_ios_install_orientation_observer(SDL_Window *window) {}
bool sdl_ios_get_safe_area_insets(SDL_Window *window,
    int *left,int *right,int *top,int *bottom) { return false; }
#endif

void __wrap_tutorial_continue(void) { fixture.active=false; }
bool __wrap_sdl_touch_only_device_active(void) { return false; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static SDL_Texture *measured_texture;
static int measured_w, measured_h;
static char measured_text[256];
static bool checking_text;
static SDL_FRect text_rects[256];
static int text_count;
static char all_text[65536];
SDL_Texture *__real_sdl_ui_text_texture(TTF_Font *,cptr,SDL_Color,int *,int *);
static void measure_text(SDL_Texture *texture,TTF_Font *font,cptr text,int w,int h)
{
    if (checking_text && texture) {
        measured_texture=texture; measured_w=w; measured_h=h;
        SDL_strlcpy(measured_text,text,sizeof(measured_text));
        fixture_assert(TTF_GetFontHeight(font)>=(SIL_SDL_MOBILE_BUILD
            ? (int)SDL_ceilf(fixture_density*24) : 42));
        SDL_strlcat(all_text,text,sizeof(all_text));
        SDL_strlcat(all_text,"\n",sizeof(all_text));
    }
}
SDL_Texture *__wrap_sdl_ui_text_texture(TTF_Font *font,cptr text,SDL_Color color,int *w,int *h)
{
    SDL_Texture *texture=__real_sdl_ui_text_texture(font,text,color,w,h);
    measure_text(texture,font,text,*w,*h);
    return texture;
}
SDL_Texture *__real_sdl_ui_wrapped_text_texture(TTF_Font *,cptr,int,SDL_Color,int *,int *);
SDL_Texture *__wrap_sdl_ui_wrapped_text_texture(TTF_Font *font,cptr text,int width,SDL_Color color,int *w,int *h)
{
    SDL_Texture *texture=__real_sdl_ui_wrapped_text_texture(font,text,width,color,w,h);
    measure_text(texture,font,text,*w,*h);
    return texture;
}
bool __real_SDL_RenderTexture(SDL_Renderer *,SDL_Texture *,const SDL_FRect *,const SDL_FRect *);
bool __wrap_SDL_RenderTexture(SDL_Renderer *renderer,SDL_Texture *texture,
    const SDL_FRect *src,const SDL_FRect *dst)
{
    if (checking_text && texture==measured_texture && dst) {
        /* A large font that is scaled back down would fail these checks. */
        if (dst->w<measured_w-1.0f || dst->h<measured_h-1.0f)
            printf("Shrunk '%s': %dx%d -> %.1fx%.1f\n",measured_text,
                measured_w,measured_h,dst->w,dst->h);
        fixture_assert(dst->w>=measured_w-1.0f && dst->h>=measured_h-1.0f);
        fixture_assert(dst->x>=0 && dst->y>=0);
        fixture_assert(dst->x+dst->w<=fixture_width+1);
        fixture_assert(dst->y+dst->h<=fixture_height+1);
        for (int i=0;i<text_count;++i)
            fixture_assert(!SDL_HasRectIntersectionFloat(dst,&text_rects[i]));
        fixture_assert(text_count<256);
        text_rects[text_count++]=*dst;
    }
    return __real_SDL_RenderTexture(renderer,texture,src,dst);
}

static void frame(void)
{
    text_count=0; measured_texture=NULL; checking_text=true;
    fixture_assert(sdl_render_current_window_frame());
    checking_text=false;
    fixture_assert(text_count>=5); /* Section heading, content, and three controls. */
}

static SDL_FRect control(int choice)
{
    for (int i=0;i<g_sdl_character_sheet_screen.hit_count;++i)
        if (g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fixture_assert(false);
    return (SDL_FRect){0};
}

static void key_event(SDL_Keycode key,char expected)
{
    SDL_Event ev={0}; char actual=0;
    ev.type=SDL_EVENT_KEY_DOWN; ev.key.key=key; ev.key.down=true;
    sdl_handle_event(&g_state,&ev);
    fixture_assert(Term_inkey(&actual,false,true)==0 && actual==expected);
    fixture_assert(Term_inkey(&actual,false,true)!=0);
}

static void pointer_event(int choice,bool touch)
{
    SDL_FRect r=control(choice); SDL_Event ev={0};
    int clicked=0, action=0; char key=0;
    if (touch) {
        ev.type=SDL_EVENT_FINGER_DOWN;
        ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
        ev.tfinger.fingerID=7;
        ev.tfinger.x=(r.x+r.w/2)/fixture_width;
        ev.tfinger.y=(r.y+r.h/2)/fixture_height;
    } else {
        ev.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
        ev.button.which=1; ev.button.button=SDL_BUTTON_LEFT;
        ev.button.x=r.x+r.w/2; ev.button.y=r.y+r.h/2;
    }
    sdl_handle_event(&g_state,&ev);
    fixture_assert(ui_menu_click_take_action(&clicked,&action));
    fixture_assert(clicked==choice && action==UI_MENU_CLICK_PRIMARY);
    fixture_assert(Term_inkey(&key,false,true)==0);
    fixture_assert(key=='\r'); /* The loop consumes the pending semantic choice. */
    ev.type=touch?SDL_EVENT_FINGER_UP:SDL_EVENT_MOUSE_BUTTON_UP;
    sdl_handle_event(&g_state,&ev);
    fixture_assert(Term_inkey(&key,false,true)!=0);
}

static void gamepad(SDL_GamepadButton button,char expected)
{
    SDL_Event ev={0}; char key=0;
    ev.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    ev.gbutton.button=button; ev.gbutton.down=true;
    sdl_handle_event(&g_state,&ev);
    fixture_assert(Term_inkey(&key,false,true)==0 && key==expected);
    ev.type=SDL_EVENT_GAMEPAD_BUTTON_UP; ev.gbutton.down=false;
    sdl_handle_event(&g_state,&ev);
    fixture_assert(Term_inkey(&key,false,true)!=0);
}

static void check(int width,int height,bool big)
{
    config.bigger_font=big;
    fixture_width=width; fixture_height=height; all_text[0]='\0';
    SDL_strlcpy(fixture_id,"debug-character-sheet",sizeof(fixture_id));
    g_state.window=SDL_CreateWindow("Big font character sheet fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256);
    Term_activate(&view->t); term_screen=&view->t;
    character_icky=1;
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(true);
    sdl_screen_back_gesture_begin();
    sdl_character_sheet_screen_begin_debug();
    frame();
    int pages=g_sdl_character_sheet_screen.debug_page_count;
    int px=g_sdl_character_sheet_screen.last_body_px;
    fixture_assert(pages>=5);
#if SIL_SDL_MOBILE_BUILD
    fixture_assert(px==(int)SDL_ceilf(fixture_density*24));
#else
    int normal_px=sdl_char_sheet_clampi((int)(MIN(width,height)*.065f),28,56);
    fixture_assert(px==normal_px+(normal_px+1)/2);
#endif
    fixture_assert(!sdl_character_sheet_screen_debug_turn_page(-1));
    for (int page=0;page<pages;++page) {
        fixture_assert(g_sdl_character_sheet_screen.debug_page==page);
        fixture_assert(g_sdl_character_sheet_screen.last_body_px==px);
        SDL_FRect close=control(ESCAPE);
        fixture_assert(close.h>=44);
        for (int i=0;i<text_count;++i)
            if (text_rects[i].y<close.y)
                fixture_assert(text_rects[i].y+text_rects[i].h<close.y);
        char path[96];
        strnfmt(path,sizeof(path),"big-font-sheet-%dx%d-%s-%02d.png",width,height,big?"on":"off",page+1);
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
        fixture_assert(pixels && IMG_SavePNG(pixels,path));
        SDL_DestroySurface(pixels);
        if (page+1<pages) {
            fixture_assert(sdl_character_sheet_screen_debug_turn_page(1));
            frame();
        }
    }
    fixture_assert(!sdl_character_sheet_screen_debug_turn_page(1));
    fixture_assert(strstr(all_text,"Perception") && strstr(all_text,"Smithing"));
    fixture_assert(strstr(all_text,"Voice") && strstr(all_text,"Offhand"));
    fixture_assert(strstr(all_text,"Attributes") && strstr(all_text,"Traits"));
    /* Enlarged trait names may wrap or span a page boundary. */
    fixture_assert(strstr(all_text,"Creator of") && strstr(all_text,"Angrist")
        && strstr(all_text,"Background"));
    fixture_assert(strstr(all_text,"END_OF_HISTORY"));
    key_event(SDLK_PAGEUP,'['); key_event(SDLK_PAGEDOWN,']');
    key_event(SDLK_LEFT,'['); key_event(SDLK_RIGHT,']');
    gamepad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,'[');
    gamepad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,']');
    gamepad(SDL_GAMEPAD_BUTTON_DPAD_LEFT,'[');
    gamepad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,']');
    gamepad(SDL_GAMEPAD_BUTTON_SOUTH,' ');
    gamepad(SDL_GAMEPAD_BUTTON_EAST,ESCAPE);
    pointer_event('[',false); pointer_event('[',true);
    fixture_assert(sdl_character_sheet_screen_debug_turn_page(-1));
    frame(); pointer_event(']',false); pointer_event(']',true);
    pointer_event(ESCAPE,false); pointer_event(ESCAPE,true);

    /* Reflow a late page after a rotation; reopening starts at the overview. */
    fixture_width=height; fixture_height=width;
    fixture_assert(SDL_SetWindowSize(g_state.window,height,width));
    g_state.safe_area=(SDL_Rect){0,0,height,width};
    frame();
    fixture_assert(g_sdl_character_sheet_screen.debug_page
        <g_sdl_character_sheet_screen.debug_page_count);
    sdl_character_sheet_screen_hide();
    sdl_character_sheet_screen_begin_debug(); frame();
    fixture_assert(g_sdl_character_sheet_screen.debug_page==0);
    sdl_character_sheet_screen_hide();
    config.bigger_font=false;
    sdl_character_sheet_screen_begin_live(-1);
    fixture_assert(g_sdl_character_sheet_screen.context==SDL_CHARACTER_SHEET_LIVE);
    fixture_assert(!sdl_character_sheet_screen_debug_turn_page(1));
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    sdl_screen_back_gesture_end();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear(); sdl_mono_font_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
    printf("Big font character sheet %dx%d (mode %s): %d pages, %dpx body, no shrinking/overlap/clipping; keyboard, mouse, touch, controller: PASS\n",
        width,height,big?"on":"off",pages,px);
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0};
    player_race race={0}; character_profile hero={0};
    object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==2); setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.monospace_font,argv[1],sizeof(config.monospace_font));
    config.use_unsafe_area=true; g_state.system_scale=1;
    z_info=&limits; p_ptr=&player; p_info=&race; c_info=&hero; c_name="";
    inventory=items;
    hero.flags_u=~0u; race.flags=RHF_MEL_AFFINITY|RHF_SMT_AFFINITY;
    player.chp=58; player.mhp=71; player.csp=21; player.msp=29;
    player.song1=SNG_NOTHING; player.song2=SNG_NOTHING;
    player.mdd=2; player.mds=9; player.mdd2=1; player.mds2=5;
    player.add=1; player.ads=8; player.cur_light=4; player.depth=12;
    player.total_weight=887; player.new_exp=12345; player.exp=567890;
    for (int i=0;i<A_MAX;++i) { player.stat_base[i]=2; player.stat_use[i]=4; }
    for (int i=0;i<S_MAX;++i) {
        player.skill_use[i]=28; player.skill_base[i]=15;
        player.skill_stat_mod[i]=4; player.skill_equip_mod[i]=7; player.skill_misc_mod[i]=2;
    }
    player.active_ability[S_MEL][MEL_RAPID_ATTACK]=true;
    SDL_strlcpy(op_ptr->full_name,"AnUnbrokenNameThatMustWrapSafely",sizeof(op_ptr->full_name));
    SDL_strlcpy(player.history,"You were born among the Noldor and have wandered far beneath the mountains. "
        "The paths of your people are long, and the memory of the starlight has followed you into the darkness. "
        "You carry a blade from a forgotten forge and seek the light that once belonged to the world. "
        "Every name and every line must stay readable, even on a small screen. END_OF_HISTORY",sizeof(player.history));
    turn=1234; playerturn=98765;
    for (int i=0;i<16;++i) g_state.palette[i]=(SDL_Color){angband_color_table[i][1],angband_color_table[i][2],angband_color_table[i][3],255};
    for (int big=0;big<2;big++) {
#if SIL_SDL_MOBILE_BUILD
        fixture_density=1; check(360,800,big); check(800,360,big);
        fixture_density=1.5f; check(580,1280,big); check(1280,580,big);
        fixture_density=2; check(720,1600,big); check(1600,720,big);
        fixture_density=2.75f; check(1080,2400,big); check(2400,1080,big);
#else
        check(1920,1080,big); check(1280,720,big); check(768,576,big);
        check(580,1280,big); check(360,800,big); check(800,360,big); check(320,568,big);
#endif
    }
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "tutorial_continue", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_get_layout_screen_rect",
             "sdl_ui_text_texture", "sdl_ui_wrapped_text_texture", "SDL_RenderTexture",
             "SDL_GetDisplayContentScale"]
    mobile_sources = ["src/sdl/core/sdl-state.c", "src/sdl/core/sdl-layout.c",
                      "src/sdl/config/sdl-settings.c", "src/sdl/render/sdl-fonts.c",
                      "src/sdl-config.c", "src/sdl/input/sdl-touch-controls.c",
                      "src/sdl/ui/sdl-panes.c"]
    for mobile in (False, True):
        modules = mobile_sources if mobile else []
        excluded = tuple("/" + p + ".obj" for p in modules) + (
            "/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
            "/src/sdl/ui/sdl-screens.c.obj")
        response = OUT / "objects.rsp"
        response.write_text("\n".join('"' + obj + '"' for obj in objects
                                     if not obj.endswith(excluded)), encoding="utf-8")
        exe = OUT / ("check-mobile.exe" if mobile else "check.exe")
        command = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g"]
        if mobile:
            command.append("-DSIL_IOS")
        command += ["@CMakeFiles/sil-more.dir/includes_C.rsp", str(source)]
        command += [str(ROOT / p) for p in modules]
        command += ["@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)]
        subprocess.run(command, cwd=BUILD, env=env, check=True)
        output = OUT / ("mobile" if mobile else "desktop")
        output.mkdir(exist_ok=True)
        subprocess.run([str(exe), str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf")],
                       cwd=output, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
