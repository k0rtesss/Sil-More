#!/usr/bin/env python3
"""Render the mobile hero picker and verify compact big-text flow and input.

Uses production SDL code with isolated player/config fixtures. No saves or user
settings are opened. Captures are written under scripts/output.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/big-character-selection"

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include <math.h>
/* BIRTH DECLARATIONS */
static float density;
static void record_preview_text(TTF_Font*, cptr, SDL_FRect);
/* SCREENS SOURCE */

float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static SDL_FRect stat_text[8];
static int stat_text_count;
static int trait_text_count;
static bool final_lore_visible;
static void record_preview_text(TTF_Font* font, cptr text, SDL_FRect rect)
{
    fixture_assert(rect.x>=-1 && rect.x+rect.w<=fixture_width+1);
    if (!strcmp(text,"Str") || !strcmp(text,"Dex")
        || !strcmp(text,"Con") || !strcmp(text,"Gra")
        || !strcmp(text,"+2") || !strcmp(text,"+3") || !strcmp(text,"+4")) {
        if (stat_text_count<8) stat_text[stat_text_count++]=rect;
    }
    if (strstr(text,"affinity") || strstr(text,"penalty")
        || strstr(text,"Artisan")) trait_text_count++;
    if (strstr(text,"LOREEND")) final_lore_visible=true;
}
static SDL_FRect hit(int choice)
{
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        if(g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fprintf(stderr,"missing hit %d\n",choice);
    fixture_assert(false); return (SDL_FRect){0};
}
static void capture(cptr name)
{
    char file[128];
    strnfmt(file,sizeof(file),"%s-%dx%d.png",name,fixture_width,fixture_height);
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels && IMG_SavePNG(pixels,file));
    SDL_DestroySurface(pixels);
}
static void touch(Uint32 type,float x,float y)
{
    SDL_Event ev={0}; ev.type=type;
    ev.tfinger.timestamp=SDL_GetTicksNS();
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=x/fixture_width; ev.tfinger.y=y/fixture_height;
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
}
static void tap(SDL_FRect r)
{
    touch(SDL_EVENT_FINGER_DOWN,r.x+r.w/2,r.y+r.h/2);
    touch(SDL_EVENT_FINGER_UP,r.x+r.w/2,r.y+r.h/2);
}
static void clear_input(void)
{
    char key;
    while(!Term_inkey(&key,false,true)) {}
    int choice;
    while(ui_menu_click_take(&choice)) {}
}
static void preview(int selected,bool long_ability,bool fallen,bool empty)
{
    const char* stats[]={"Str\t+2","Dex\t+3","Con\t+3","Gra\t+4"};
    birth_compact_flag_line traits[48];
    int trait_count=collect_character_trait_lines(0,1,false,traits,N_ELEMENTS(traits),NULL);
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_begin_select(selected,"Choose your hero");
    for(int i=0;i<9;i++) {
        sdl_character_sheet_screen_add_select_row(i,"Hero",TERM_WHITE,"");
        if(fallen && i==selected)
            sdl_character_sheet_screen_set_last_select_row_confirmable(false);
    }
    sdl_character_sheet_screen_set_select_title_detail(
        fallen ? "Celebrimbor, Last of the House of Fëanor (fallen)"
            : "Fëanor, Spirit of Fire", " *** mighty",TERM_GREEN);
    sdl_character_sheet_screen_set_select_description(/* DESCRIPTION */);
    sdl_character_sheet_screen_set_select_detail_size_hint(4,2,9);
    sdl_character_sheet_screen_set_select_ability_rows(empty?0:2);
    for(int i=0;i<4;i++)
        sdl_character_sheet_screen_add_select_detail(stats[i],i?TERM_L_BLUE:TERM_L_GREEN,
            "Attribute bonus");
    if(!empty) {
        sdl_character_sheet_screen_add_select_detail(long_ability
            ? "Song of Unbreakable and Everlasting Defiance" : "Artifice",TERM_RED,"Starting ability");
        sdl_character_sheet_screen_add_select_detail("Jeweller",TERM_RED,"Starting ability");
        for(int i=0;i<trait_count;i++)
            sdl_character_sheet_screen_add_select_detail(traits[i].txt,traits[i].attr,"Trait details");
    }
    fixture_assert(sdl_character_sheet_screen_commit_select(selected));
    stat_text_count=trait_text_count=0; final_lore_visible=false;
    fixture_assert(sdl_render_current_window_frame());
}
static void check_layout(void)
{
    preview(0,false,false,false);
    SDL_FRect scroll=g_sdl_character_sheet_screen.select_scroll_rect;
    SDL_FRect back=hit(-1),choose=hit(-2),next=hit(SDL_SELECT_CLICK_CAROUSEL_NEXT);
    fixture_assert(back.y>=scroll.y+scroll.h && choose.y>=scroll.y+scroll.h);
    fixture_assert(next.y+next.h<=scroll.y);
    fixture_assert(next.h>=sdl_ui_min_tap_px());
    fixture_assert(g_sdl_character_sheet_screen.last_body_px==sdl_menu_role_font_px(SDL_UI_FONT_BODY));
    if(fixture_height>fixture_width) {
        fixture_assert(stat_text_count==8);
        if(fixture_height>=800) fixture_assert(trait_text_count>0);
        /* Each stat value stays close to its label and both columns share
         * the same two baselines, instead of four screen-wide rows. */
        fixture_assert(fabsf(stat_text[0].y-stat_text[2].y)<1);
        fixture_assert(fabsf(stat_text[4].y-stat_text[6].y)<1);
        for(int i=0;i<8;i+=2) {
            float gap=stat_text[i+1].x-stat_text[i].x-stat_text[i].w;
            fixture_assert(gap>=0 && gap<g_sdl_character_sheet_screen.last_body_line_h);
        }
        SDL_FRect first=hit(SDL_CHAR_SHEET_INFO_CHOICE_BASE);
        SDL_FRect second=hit(SDL_CHAR_SHEET_INFO_CHOICE_BASE+1);
        fixture_assert(first.y==second.y && first.x+first.w<=second.x);
        fixture_assert(first.h>=sdl_ui_min_tap_px());
        tap(first);
        fixture_assert(g_sdl_character_sheet_screen.hover_choice==SDL_CHAR_SHEET_INFO_CHOICE_BASE);
        fixture_assert(!ui_menu_click_has_pending());
        g_sdl_character_sheet_screen.hover_choice=SDL_CHAR_SHEET_NO_HOVER;
        clear_input();
    }
    capture("hero-compact");
    tap(next);
    int choice=-99;
    fixture_assert(ui_menu_click_take(&choice) && choice==SDL_SELECT_CLICK_CAROUSEL_NEXT);
    clear_input();
    tap(choose);
    fixture_assert(ui_menu_click_take(&choice) && choice==-2);
    clear_input();
    tap(back);
    fixture_assert(ui_menu_click_take(&choice) && choice==-1);
    clear_input();
    /* Long content remains reachable with a vertical drag and exact scroll
     * extent, even in short landscape. Changing hero resets that scroll. */
    int maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
    fixture_assert(maximum>0);
    float x=scroll.x+scroll.w/2, y=scroll.y+scroll.h*.8f;
    touch(SDL_EVENT_FINGER_DOWN,x,y);
    touch(SDL_EVENT_FINGER_MOTION,x,y-MIN(70.0f,scroll.h*.6f));
    touch(SDL_EVENT_FINGER_UP,x,y-MIN(70.0f,scroll.h*.6f));
    fixture_assert(g_sdl_character_sheet_screen.sheet_scroll>0);
    fixture_assert(!ui_menu_click_has_pending());
    clear_input();
    g_sdl_character_sheet_screen.sheet_scroll=maximum;
    fixture_assert(sdl_render_current_window_frame());
    capture("hero-lore-end");
    fixture_assert(final_lore_visible);
    preview(4,true,false,false);
    fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==0);
    hit(SDL_SELECT_CLICK_CAROUSEL_PREV); hit(SDL_SELECT_CLICK_CAROUSEL_NEXT);
    capture("hero-long-ability");
    preview(8,false,true,false);
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        fixture_assert(g_sdl_character_sheet_screen.hits[i].choice!=8
            && g_sdl_character_sheet_screen.hits[i].choice!=SDL_SELECT_CLICK_CAROUSEL_NEXT
            && g_sdl_character_sheet_screen.hits[i].choice!=-2);
    capture("hero-fallen");
    preview(0,false,false,true);
    for(int i=0;i<g_debug_sheet_row_count;i++) {
        fixture_assert(strcmp(g_debug_sheet_rows[i].text,"Abilities"));
        fixture_assert(strcmp(g_debug_sheet_rows[i].text,"Traits"));
    }
    printf("Hero picker %dx%d density %.2f: compact stats, readable text, bounded controls, touch/scroll, long abilities, fallen hero and empty sections: PASS\n",
        fixture_width,fixture_height,density);
}
int main(int argc,char** argv)
{
    maxima limits={0}; player_type player={0};
    player_race races[1]={0}; character_profile heroes[2]={0};
    p_info=races; c_info=heroes;
    races[0].flags=/* RACE FLAGS */;
    heroes[1].flags=/* HERO FLAGS */;
    heroes[1].flags_u=/* HERO UNIQUE FLAGS */;
    fixture_assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    fixture_width=atoi(argv[1]); fixture_height=atoi(argv[2]); density=atof(argv[3]);
    SDL_strlcpy(fixture_id,"big-character-selection",sizeof(fixture_id));
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) && TTF_Init());
    sdl_config_set_defaults(&config); config.bigger_font=true;
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++) config.menu_bigger_font[i]=true;
    config.show_menu_font_button=true; config.use_unsafe_area=true;
    SDL_strlcpy(config.story_font,argv[4],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[5],sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font,argv[6],sizeof(config.monospace_font));
    g_state.system_scale=density; z_info=&limits; p_ptr=&player;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){angband_color_table[i][1],
        angband_color_table[i][2],angband_color_table[i][3],255};
    g_state.window=SDL_CreateWindow("Hero picker fixture",fixture_width,fixture_height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
    sdl_view* view=&g_views[PANE_MAIN]; term_init(&view->t,80,24,256);
    Term_activate(&view->t); term_screen=&view->t; character_icky=1;
    check_layout();
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    screens = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")
    start = screens.index("static SDL_FRect sdl_char_sheet_draw_text_aligned(")
    end = screens.index("static SDL_FRect sdl_char_sheet_draw_text_alpha(", start)
    draw = screens[start:end].replace("SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);",
        "record_preview_text(font, text, dst);\n    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);")
    screens = screens[:start] + draw + screens[end:]
    # The SDL harness already includes the unguarded externs.h declarations.
    birth_header = (ROOT / "src/birth/birth-internal.h").read_text(encoding="utf-8").replace(
        '#include "externs.h"', '')
    character = (ROOT / "lib/edit/character.txt").read_text(encoding="utf-8")
    feanor = character.split("N:1:", 1)[1].split("N:2:", 1)[0]
    race = (ROOT / "lib/edit/race.txt").read_text(encoding="utf-8").split(
        "N:0:", 1)[1].split("N:1:", 1)[0]
    def flags(block, prefix, macro):
        return " | ".join(macro + flag.strip() for line in block.splitlines()
                          if line.startswith(prefix + ":") for flag in line[2:].split("|")) or "0"
    welcome = next(line[2:] for line in feanor.splitlines() if line.startswith("B:"))
    lore = " ".join(line[2:] for line in feanor.splitlines() if line.startswith("D:"))
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("/* BIRTH DECLARATIONS */", birth_header).replace(
        "/* SCREENS SOURCE */", screens).replace(
        "/* DESCRIPTION */", json.dumps(welcome + "\n\n" + lore + " LOREEND", ensure_ascii=False)).replace(
        "/* RACE FLAGS */", flags(race, "F", "RHF_")).replace(
        "/* HERO FLAGS */", flags(feanor, "F", "RHF_")).replace(
        "/* HERO UNIQUE FLAGS */", flags(feanor, "U", "UNQ_")), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj", "/src/sdl/ui/sdl-screens.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD / "_deps" / dep) for dep in
        ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision", "get_sdl_gameplay_tutorial_mode",
        "SDL_WaitEvent", "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
        "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect", "sdl_touch_only_device_active",
        "sdl_get_layout_screen_rect", "SDL_GetDisplayContentScale"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, scale in [(320, 568, 1), (360, 800, 1), (580, 1280, 1.6),
                                  (800, 360, 1), (1080, 2400, 3), (2400, 1080, 3)]:
        subprocess.run([str(exe), str(width), str(height), str(scale),
            str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
            str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf")], cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
