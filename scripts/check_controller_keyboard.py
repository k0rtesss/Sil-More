#!/usr/bin/env python3
"""Exercise production text prompts, controller routing and rendered glyphs.

Uses the configured standard build. No player configuration or save is opened.
The virtual-controller game smoke test remains separate from this fixture.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/controller-keyboard"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>

static int step, scenario;
static SDL_JoystickID fixture_pad;
static TTF_Font* detection_font;
static bool rendering;
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect* rect)
{
    (void)pane;
    if (!rendering) return false;
    *rect = sdl_get_layout_screen_rect(); return true;
}
static void tap(int button)
{
    SDL_Event ev = {0}; ev.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    ev.gbutton.which = fixture_pad;
    ev.gbutton.button = button; ev.gbutton.down = true;
    sdl_handle_event(&g_state,&ev);
    ev.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    ev.gbutton.down = false; sdl_handle_event(&g_state,&ev);
}
static void axis(int id, int value)
{
    SDL_Event ev = {0};ev.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    ev.gaxis.which=fixture_pad;
    ev.gaxis.axis=id;ev.gaxis.value=value;sdl_handle_event(&g_state,&ev);
}
static int queued(void)
{
    char key = 0; assert(!Term_inkey(&key, false, true));
    return (unsigned char)key;
}
static void empty(void)
{
    char key = 0; assert(Term_inkey(&key, false, true));
}
void __wrap_message_flush(void) {}
static errr extra(int action, int value) {(void)action;(void)value;return 0;}
static errr text(int x,int y,int n,byte a,const char* s)
{(void)x;(void)y;(void)n;(void)a;(void)s;return 0;}
static errr wipe(int x,int y,int n) {(void)x;(void)y;(void)n;return 0;}
static errr cursor(int x,int y) {(void)x;(void)y;return 0;}

char __wrap_inkey(void)
{
    assert(sdl_controller_keyboard_active());
    if (scenario == 1) {
        tap(SDL_GAMEPAD_BUTTON_EAST);
    } else if (scenario == 2) {
        if (step++ == 0) {
            /* Navigate into the action row and activate Clear. */
            tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
            tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
            for (int i=0;i<3;i++) tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            tap(SDL_GAMEPAD_BUTTON_SOUTH);
        } else tap(SDL_GAMEPAD_BUTTON_START);
    } else {
        switch (step++) {
        case 0: tap(SDL_GAMEPAD_BUTTON_SOUTH); break; /* a replaces default */
        case 1:
            tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            tap(SDL_GAMEPAD_BUTTON_SOUTH); break; /* s */
        case 2: tap(SDL_GAMEPAD_BUTTON_WEST); break; /* delete s */
        case 3:
            tap(SDL_GAMEPAD_BUTTON_NORTH);
            tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            tap(SDL_GAMEPAD_BUTTON_SOUTH); break; /* D */
        case 4: tap(SDL_GAMEPAD_BUTTON_START); break;
        default: assert(false);
        }
    }
    return (char)queued();
}

static void check_prompts(void)
{
    sdl_config_set_defaults(&config);
    config.gamepad_enabled = true;
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    character_icky = 1;
    const int bindings[] = {GAMEPAD_BIND_NONE, GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL, GAMEPAD_BIND_ALT, 'f'};
    for (int style=0;style<SDL_GAMEPAD_GLYPH_COUNT;style++) {
    set_sdl_gamepad_glyph_style(style);
    for (unsigned i=0;i<N_ELEMENTS(bindings);++i) {
        for (int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;b++)
            config.gamepad_button_bindings[b] = bindings[i];
        for (scenario=0;scenario<3;scenario++) {
            char value[32] = "Existing"; step = 0;
            bool accepted = get_string_panel("Name", value, sizeof(value));
            assert(accepted == (scenario != 1));
            assert(!strcmp(value, scenario == 0 ? "aD" : scenario == 1 ? "Existing" : ""));
            assert(!sdl_controller_keyboard_active()); empty();
            assert(!sdl_gamepad_shift_active() && !sdl_gamepad_ctrl_active()
                && !sdl_gamepad_alt_active());
        }
    }
    }
    set_sdl_gamepad_glyph_style(SDL_GAMEPAD_GLYPH_AUTO);
    sdl_text_input_begin(); sdl_text_input_begin();
    sdl_text_input_end(); assert(sdl_controller_keyboard_active());
    /* Both sticks work independently of configurable movement bindings. */
    axis(SDL_GAMEPAD_AXIS_LEFTX, 20000);
    tap(SDL_GAMEPAD_BUTTON_SOUTH); assert(queued() == 's');
    axis(SDL_GAMEPAD_AXIS_LEFTX, 22000); /* held direction is not a second step */
    tap(SDL_GAMEPAD_BUTTON_SOUTH); assert(queued() == 's');
    axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
    axis(SDL_GAMEPAD_AXIS_RIGHTX, 20000);
    tap(SDL_GAMEPAD_BUTTON_SOUTH); assert(queued() == 'd');
    tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    tap(SDL_GAMEPAD_BUTTON_SOUTH); assert(queued() == '=');
    axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767); empty();
    sdl_text_input_end(); assert(!sdl_controller_keyboard_active()); empty();
    config.input_ui_mode = SDL_INPUT_UI_MODE_PLATFORM;
    sdl_text_input_begin(); assert(!sdl_controller_keyboard_active());
    tap(SDL_GAMEPAD_BUTTON_SOUTH); /* connecting/using a pad during a prompt */
    assert(sdl_controller_keyboard_active() && queued() == 'a');
    sdl_text_input_end(); empty();
}

static void check_detection(void)
{
    const int vendors[]={0x045e,0x054c,0x054c};
    const int products[]={0x028e,0x05c4,0x0ce6};
    const SDL_GamepadType types[]={SDL_GAMEPAD_TYPE_XBOX360,
        SDL_GAMEPAD_TYPE_PS4,SDL_GAMEPAD_TYPE_PS5};
    const char* names[]={"Xbox 360 Controller","DualShock 4","DualSense"};
    SDL_JoystickID ids[3];
    Uint64 previous_pixels=0;
    sdl_config_set_defaults(&config);
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    for(int i=0;i<3;i++) {
        SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
        desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.vendor_id=vendors[i];desc.product_id=products[i];desc.name=names[i];
        desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
        desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
        desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
        ids[i]=SDL_AttachVirtualJoystick(&desc);assert(ids[i]);
        sdl_gamepad_open(ids[i]);
        fixture_pad=ids[i];
        tap(SDL_GAMEPAD_BUTTON_GUIDE);empty();
        if(sdl_gamepad_prompt_type()!=types[i])
            fprintf(stderr,"device %s: expected %d got %d\n",names[i],types[i],sdl_gamepad_prompt_type());
        assert(sdl_gamepad_prompt_type()==types[i]);
        assert(sdl_gamepad_playstation_prompts()==(i>0));
        if(i) {
            assert(strstr(sdl_gamepad_button_label(SDL_GAMEPAD_BUTTON_SOUTH),"Cross"));
            assert(strstr(sdl_gamepad_trigger_label(0),"L2"));
            assert(!strcmp(sdl_gamepad_button_label(SDL_GAMEPAD_BUTTON_BACK),i==1?"Share":"Create"));
        }
        sdl_text_input_begin();tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(queued()=='a');
        tap(SDL_GAMEPAD_BUTTON_WEST);assert(queued()=='\b');
        tap(SDL_GAMEPAD_BUTTON_EAST);assert(queued()==ESCAPE);
        sdl_text_input_end();empty();
        if(detection_font) {
            SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
            SDL_Color white={255,255,255,255};int w,h;
            SDL_Texture* texture=sdl_ui_text_texture(detection_font,
                "<A> <B> <X> <Y> <LB> <LT> <View>",white,&w,&h);assert(texture);
            SDL_FRect dst={0,0,w,h};SDL_RenderTexture(g_state.renderer,texture,NULL,&dst);
            SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
            Uint64 hash=1469598103934665603ULL;
            for(size_t p=0;p<(size_t)pixels->pitch*pixels->h;p++)
                hash=(hash^((Uint8*)pixels->pixels)[p])*1099511628211ULL;
            SDL_DestroySurface(pixels);
            if(i) assert(hash!=previous_pixels);
            previous_pixels=hash;
        }
    }
    /* A saved preference overrides a masked or emulated device identity. */
    set_sdl_gamepad_glyph_style(SDL_GAMEPAD_GLYPH_XBOX);
    assert(!sdl_gamepad_playstation_prompts());
    set_sdl_gamepad_glyph_style(SDL_GAMEPAD_GLYPH_DUALSHOCK);
    assert(sdl_gamepad_prompt_type()==SDL_GAMEPAD_TYPE_PS4);
    set_sdl_gamepad_glyph_style(SDL_GAMEPAD_GLYPH_DUALSENSE);
    assert(sdl_gamepad_prompt_type()==SDL_GAMEPAD_TYPE_PS5);
    for(int i=2;i>=0;i--) {sdl_gamepad_close(ids[i]);assert(SDL_DetachVirtualJoystick(ids[i]));}
    fixture_pad=0;
    set_sdl_gamepad_glyph_style(SDL_GAMEPAD_GLYPH_AUTO);
    assert(!sdl_gamepad_playstation_prompts());
}

static void check_preferences(void)
{
    struct sdl_config loaded;
    for(int style=0;style<SDL_GAMEPAD_GLYPH_COUNT;style++) {
        set_sdl_gamepad_glyph_style(style);
        assert(sdl_config_save("button-icons.json",&config,NULL,0));
        assert(sdl_config_load("button-icons.json",&loaded,NULL,0,NULL)==SDL_CONFIG_LOAD_OK);
        assert(loaded.gamepad_glyph_style==style);
    }
    const char* json[]={"{}","{\"gamepad\":{\"buttonIcons\":\"invalid\"}}"};
    for(int i=0;i<2;i++) {
        FILE* file=fopen("old-icons.json","wb");assert(file);fputs(json[i],file);fclose(file);
        assert(sdl_config_load("old-icons.json",&loaded,NULL,0,NULL)==SDL_CONFIG_LOAD_OK);
        assert(loaded.gamepad_glyph_style==SDL_GAMEPAD_GLYPH_AUTO);
    }
    set_sdl_gamepad_glyph_style(999);assert(get_sdl_gamepad_glyph_style()==SDL_GAMEPAD_GLYPH_AUTO);
}

static void check_glyph_legibility(const char* font_path)
{
    /* Inspect the ink inside the circle, excluding its outline. Full font
     * line boxes used to leave only a few pixels for these button letters. */
    for (int px=16;px<=48;px+=8) {
        TTF_Font* font=TTF_OpenFont(font_path,px); assert(font);
        const char* tokens[]={"<A>","<B>","<X>","<Y>"};
        for (unsigned i=0;i<N_ELEMENTS(tokens);i++) {
            SDL_Surface* surface=TTF_RenderText_Blended(font,tokens[i],0,
                (SDL_Color){255,255,255,255}); assert(surface);
            sdl_controller_glyphs_paint(font,surface,tokens[i],0);
            /* Keep only the centre of the disc so the outline cannot make a
             * tiny letter appear taller than its actual ink. */
            int left=surface->w/2-surface->h*3/10;
            int right=surface->w/2+surface->h*3/10;
            int top=surface->h/5, bottom=surface->h*4/5;
            int first=bottom,last=-1;
            for(int y=top;y<bottom;y++) for(int x=left;x<right;x++) {
                float dx=x+0.5f-surface->w*0.5f;
                float dy=y+0.5f-surface->h*0.5f;
                if(dx*dx+dy*dy>surface->h*surface->h*0.27f*0.27f) continue;
                Uint8 r,g,b,a;
                assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
                if(a>=128) { first=MIN(first,y); last=MAX(last,y); }
            }
            if(last-first+1<surface->h*0.36f)
                fprintf(stderr,"%s @ %d: button letter only %d pixels in a %d pixel row\n",
                    tokens[i],px,last-first+1,surface->h);
            assert(last-first+1>=surface->h*0.36f);
            SDL_DestroySurface(surface);
        }
        TTF_CloseFont(font);
    }
}

static void check_glyphs(const char* font_path, const char* png_path)
{
    TTF_Font* font = TTF_OpenFont(font_path, 32); assert(font);
    const char* label = "<A> Confirm  <B> Back  <X> More  <Y> Case\n<LB>/<RB> Pages  <LT>/<RT> Triggers  <Menu> Menu  <View> Details";
    SDL_Color white = {255,255,255,255};
    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font,label,0,white,640);
    assert(surface);
    size_t size = (size_t)surface->pitch * surface->h;
    void* before = malloc(size); assert(before); memcpy(before,surface->pixels,size);
    sdl_controller_glyphs_paint(font,surface,label,640);
    assert(memcmp(before,surface->pixels,size));
    SDL_Surface* preview = SDL_CreateSurface(surface->w,surface->h,SDL_PIXELFORMAT_RGBA32);
    assert(preview);
    SDL_FillSurfaceRect(preview,NULL,SDL_MapSurfaceRGBA(preview,28,32,40,255));
    SDL_BlitSurface(surface,NULL,preview,NULL);
    assert(IMG_SavePNG(preview,png_path));
    SDL_DestroySurface(preview);
    free(before);SDL_DestroySurface(surface);TTF_CloseFont(font);
}

static void check_layouts(const char* font_path,const char* glyph_path)
{
    rendering = true;
    SDL_strlcpy(config.story_font,font_path,sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,font_path,sizeof(config.story_font2));
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    config.use_unsafe_area = true;
    g_state.system_scale = 1;
    for (int i=0;i<256;i++) g_state.palette[i]=(SDL_Color){
        angband_color_table[i][1],angband_color_table[i][2],
        angband_color_table[i][3],255};
    g_state.window = SDL_CreateWindow("Controller keyboard fixture",1280,720,0);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window,NULL);
    assert(g_state.renderer);
    int previous = sdl_menu_font_set_render_context(SDL_MENU_FONT_QUESTIONS);
    const int sizes[][2] = {{1280,720},{800,360},{360,640},{640,360}};
    for (int bigger=0;bigger<2;bigger++) for (unsigned i=0;i<N_ELEMENTS(sizes);i++) {
        assert(SDL_SetWindowSize(g_state.window,sizes[i][0],sizes[i][1]));
        SDL_PumpEvents();
        set_sdl_menu_bigger_font(SDL_MENU_FONT_QUESTIONS,bigger);
        sdl_text_input_begin();
        sdl_question_menu_begin("Name:");
        sdl_question_menu_set_text_input(true);
        sdl_question_menu_add_entry(100,"","Controller Hero_",TERM_YELLOW);
        sdl_question_menu_set_highlight(100);
        sdl_question_menu_finish();
        SDL_Rect panel=sdl_controller_keyboard_rect();
        SDL_Rect screen=sdl_get_layout_screen_rect();
        assert(panel.w==screen.w && panel.y>0 && panel.y+panel.h<=screen.h);
        SDL_Rect visible=sdl_text_input_visible_rect(screen);
        assert(visible.h>0 && visible.y+visible.h<=panel.y);
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);
        SDL_RenderClear(g_state.renderer);
        sdl_question_menu_render();sdl_controller_keyboard_render();
        SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
        char path[1024];SDL_strlcpy(path,glyph_path,sizeof(path));
        char* slash=strrchr(path,'\\');if(!slash)slash=strrchr(path,'/');assert(slash);
        strnfmt(slash+1,sizeof(path)-(slash+1-path),"%s-keyboard-%dx%d-%s.png",
            sdl_gamepad_glyph_style_label(get_sdl_gamepad_glyph_style()),
            sizes[i][0],sizes[i][1],bigger?"big":"normal");
        assert(IMG_SavePNG(pixels,path));SDL_DestroySurface(pixels);
        sdl_question_menu_clear();sdl_text_input_end();
    }
    sdl_menu_font_set_render_context(previous);
    sdl_ui_text_cache_clear();sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer);g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window);g_state.window=NULL;
}

int main(int argc,char** argv)
{
    assert(argc == 3); log_set_quiet(true);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMEPAD)); assert(TTF_Init());
    term t; assert(!term_init(&t,80,24,256)); Term_activate(&t); term_screen = &t;
    t.xtra_hook=extra;t.text_hook=text;t.wipe_hook=wipe;t.curs_hook=cursor;
    check_prompts();check_detection();check_preferences();
    g_state.window=SDL_CreateWindow("Controller detection fixture",640,96,0);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,NULL);assert(g_state.renderer);
    detection_font=TTF_OpenFont(argv[1],32);assert(detection_font);
    check_detection();sdl_ui_text_cache_clear();TTF_CloseFont(detection_font);detection_font=NULL;
    SDL_DestroyRenderer(g_state.renderer);g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window);g_state.window=NULL;
    for(int style=1;style<SDL_GAMEPAD_GLYPH_COUNT;style++) {
        set_sdl_gamepad_glyph_style(style);
        if(style==SDL_GAMEPAD_GLYPH_XBOX) check_glyph_legibility(argv[1]);
        char path[1024];SDL_strlcpy(path,argv[2],sizeof(path));
        char* slash=strrchr(path,'\\');if(!slash)slash=strrchr(path,'/');assert(slash);
        strnfmt(slash+1,sizeof(path)-(slash+1-path),"glyphs-%s.png",sdl_gamepad_glyph_style_label(style));
        check_glyphs(argv[1],path);check_layouts(argv[1],path);
    }
    term_nuke(&t);TTF_Quit();SDL_Quit();
    puts("Xbox/DualShock/DualSense: real SDL text editing/cancel/remaps/modifiers, virtual device detection, hotplug, icon preference roundtrip and 24 rendered keyboard layouts PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / p) for p in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=inkey",
        "-Wl,--wrap=message_flush", "-Wl,--wrap=sdl_overlay_pane_anchor_rect",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    font = ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"
    subprocess.run([str(exe), str(font), str(OUT / "glyphs.png")],
        cwd=OUT, env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()
