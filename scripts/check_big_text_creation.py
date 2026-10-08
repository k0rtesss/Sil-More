#!/usr/bin/env python3
"""Render large-text creation screens and replay hero-intro scrolling offscreen.

Uses the actual mobile renderers, fonts and event dispatcher at phone density.
No player data is loaded. Captures are written to scripts/output.
"""
from pathlib import Path
import os
import shlex
import subprocess

from check_phone_welcome_poetry import HARNESS as WELCOME, INIT

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/big-text-creation"

PREFIX = WELCOME[:WELCOME.index("static void indicator(void)")]
SCREENS = (ROOT / "src/sdl/ui/sdl-screens.c").read_text()
start = SCREENS.index("static SDL_FRect sdl_char_sheet_draw_text_aligned(")
end = SCREENS.index("SDL_FRect sdl_char_sheet_draw_text(", start)
SCREENS = SCREENS[:start] + SCREENS[start:end].replace(
    "SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);",
    "creation_record(text, dst);\n    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);",
) + SCREENS[end:]
PREFIX = PREFIX.replace('#include "sdl/ui/sdl-screens.c"',
    "static void creation_record(cptr text, SDL_FRect rect);\n" + SCREENS)

HARNESS = PREFIX + r'''
static struct { char text[256]; SDL_FRect rect; } creation_text[512];
static int creation_count;
static void creation_record(cptr text, SDL_FRect rect)
{
    if (!footer_capture) return;
    fixture_assert(creation_count < N_ELEMENTS(creation_text));
    SDL_strlcpy(creation_text[creation_count].text,text,sizeof(creation_text[0].text));
    creation_text[creation_count++].rect=rect;
}
static void creation_begin(cptr name)
{
    SDL_strlcpy(fixture_id,name,sizeof(fixture_id));
    creation_count=0; start_capture();
}
static SDL_FRect creation_find(cptr text)
{
    for(int i=0;i<creation_count;i++) if(streq(creation_text[i].text,text))
        return creation_text[i].rect;
    fprintf(stderr,"Missing text %s in %s\n",text,fixture_id);
    fixture_assert(false); return (SDL_FRect){0};
}
static void creation_characters(void)
{
    static cptr stats[]={"Str\t+1","Dex\t+2","Con\t+5","Gra\t+2"};
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    sdl_character_sheet_screen_begin_select(1,"Character");
    sdl_character_sheet_screen_add_select_row(0,"Feanor",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_row(1,"Maedhros",TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_title_detail("Maedhros the Red","*** mighty",TERM_L_GREEN);
    sdl_character_sheet_screen_set_select_description("The Oath drives you on, eldest of the dispossessed; the hand you lost remembers Thangorodrim.\n\nEldest son of fire-hearted Feanor, once chained in torment, yet unbroken in spirit.");
    sdl_character_sheet_screen_set_select_detail_size_hint(4,1,1);
    sdl_character_sheet_screen_set_select_ability_rows(1);
    for(int i=0;i<4;i++) sdl_character_sheet_screen_add_select_detail(stats[i],TERM_L_GREEN,"Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("Vengeance",TERM_L_GREEN,"Starting ability");
    sdl_character_sheet_screen_add_select_detail("Kinslayer",TERM_WHITE,"Trait");
    fixture_assert(sdl_character_sheet_screen_commit_select(1));
    creation_begin("character-selection");
    sdl_character_sheet_screen_render_canvas(&canvas); stop_capture();
    float previous=-1;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(!streq(hit->desc,"Attribute bonus")) continue;
        SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
        if(hit->rect.y+sdl_ui_min_tap_px()<=view.y+view.h)
            fixture_assert(hit->rect.h>=sdl_ui_min_tap_px()-1);
        if(previous>=0) fixture_assert(hit->rect.y-previous<=sdl_ui_min_tap_px()+1);
        previous=hit->rect.y;
    }
    if(fixture_width==580 && fixture_height==1280) inside(creation_find("Vengeance"));
    capture("character-selection");
    g_sdl_character_sheet_screen.sheet_scroll=g_sdl_character_sheet_screen.sheet_scroll_max;
    creation_begin("character-selection-end");
    sdl_character_sheet_screen_render_canvas(&canvas); stop_capture();
    SDL_FRect trait=creation_find("Kinslayer"); inside(trait);
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice>=SDL_CHAR_SHEET_INFO_CHOICE_BASE) {
            SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
            fixture_assert(hit->rect.y>=view.y-1 && hit->rect.y+hit->rect.h<=view.y+view.h+1);
        }
    }
    capture("character-selection-end");
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}
static void creation_skills(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    int base[S_MAX]={0},gain[S_MAX]={0},costs[S_MAX]={0};
    gain[S_MEL]=gain[S_EVN]=5; gain[S_PER]=gain[S_WIL]=3;
    for(int i=0;i<S_MAX;i++) {
        p_ptr->skill_base[i]=gain[i]; p_ptr->skill_stat_mod[i]=4;
        p_ptr->skill_misc_mod[i]=i==S_MEL||i==S_PER?1:i==S_WIL?2:i==S_STL?-1:0;
        p_ptr->skill_use[i]=gain[i]+4+p_ptr->skill_misc_mod[i];
    }
    character_generated=character_dungeon=false;
    sdl_character_sheet_screen_show_birth_skills(base,gain,costs,S_MEL,800);
    creation_begin("skills"); sdl_character_sheet_screen_render_canvas(&canvas); stop_capture();
    float value_x=-1, formula_x=-1;
    int labels=0,values=0,formulas=0;
    for(int i=0;i<creation_count;i++) {
        cptr text=creation_text[i].text; SDL_FRect r=creation_text[i].rect;
        if(strstr(text,"Cost:")) {
            if(value_x<0) value_x=r.x;
            fixture_assert(SDL_fabsf(r.x-value_x)<1); values++;
        }
        if(strstr(text," = ") && !strstr(text,"Cost:")) {
            if(formula_x<0) formula_x=r.x;
            fixture_assert(SDL_fabsf(r.x-formula_x)<1); formulas++;
        }
        for(int skill=0;skill<S_MAX;skill++) if(skill!=S_SPC && streq(text,skill_names_full[skill])) {
            labels++;
        }
    }
    fixture_assert(labels>=1 && values==labels);
    fixture_assert(formulas==0 || formulas==labels);
    if(formulas) fixture_assert(formula_x==value_x);
    capture("skills");
    sdl_character_sheet_screen_show_birth_skills(base,gain,costs,S_SNG,800);
    creation_begin("skills-last"); sdl_character_sheet_screen_render_canvas(&canvas); stop_capture();
    SDL_FRect song=creation_find("Song"); inside(song);
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    fixture_assert(song.y>=view.y-1 && song.y+song.h<=view.y+view.h+1);
    capture("skills-last"); sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    character_generated=character_dungeon=true;
}
static void creation_lamp(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    sdl_character_sheet_screen_begin_book("The Chronicle of the Long Defiance");
    sdl_character_sheet_screen_add_book_contents("I. Statistics",0,0);
    sdl_character_sheet_screen_add_book_contents("II. Blessings",1,1);
    sdl_character_sheet_screen_add_book_contents("III. Curses",2,2);
    sdl_character_sheet_screen_add_book_contents("IV. Difficulty",3,3);
    sdl_character_sheet_screen_add_book_contents("V. Tales",4,4);
    sdl_character_sheet_screen_add_book_paragraph("I - The Measure of the Tale");
    sdl_character_sheet_screen_set_book_lamp(216,400,0);
    g_sdl_narrative_portrait_rendering=fixture_height>fixture_width;
    creation_begin("chronicle-light"); sdl_character_sheet_screen_render_canvas(&canvas); stop_capture();
    SDL_FRect label=creation_find("Light 216 / 400"); inside(label);
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        SDL_FRect footer=g_sdl_character_sheet_screen.hits[i].rect;
        if(g_sdl_character_sheet_screen.hits[i].choice<0)
            fixture_assert(!SDL_HasRectIntersectionFloat(&label,&footer));
    }
    capture("chronicle-light"); g_sdl_narrative_portrait_rendering=false;
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}
static void creation_tale(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    fixture_assert(sdl_tale_screen_begin("=== The Tale So Far ==="));
    sdl_tale_screen_add_entry("Nienna's Mercy","A cold, empty darkness surrounds you. Your memories scatter like ashes on the wind, leaving only faint sparks of courage within. You sense deep sorrow, gentle and unseen - Nienna's quiet weeping steadies your uncertain heart.");
    sdl_tale_screen_set_active_entry(0,255);
    sdl_tale_screen_set_prompt("[Tap to continue]  *  [Back] fast forward",true,false);
    creation_begin("tale-controls"); sdl_tale_screen_render_canvas(&canvas); stop_capture();
    SDL_FRect next=g_sdl_tale_screen.prompt_next_rect,skip=g_sdl_tale_screen.prompt_skip_rect;
    inside(next); inside(skip);
    fixture_assert(!SDL_HasRectIntersectionFloat(&next,&skip));
    SDL_FRect label=creation_find("[Back] fast forward"); inside(label);
    fixture_assert(label.y>=skip.y && label.y+label.h<=skip.y+skip.h+1);
    char key=0; fixture_assert(sdl_tale_screen_handle_pointer(skip.x+skip.w/2,skip.y+skip.h/2));
    fixture_assert(Term_inkey(&key,false,true)==0 && key==ESCAPE);
    fixture_assert(sdl_tale_screen_handle_pointer(next.x+next.w/2,next.y+next.h/2));
    fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
    float body_h=g_sdl_tale_screen.layout_body_h;
    sdl_tale_screen_set_prompt("[Tap to continue]",true,true);
    sdl_tale_screen_render_canvas(&canvas);
    fixture_assert(g_sdl_tale_screen.layout_body_h==body_h);
    sdl_tale_screen_set_prompt("[Tap to continue]  *  [Back] fast forward",true,false);
    sdl_tale_screen_render_canvas(&canvas); capture("tale-controls");
    do {
        sdl_tale_layout_metrics metrics; fixture_assert(sdl_tale_screen_metrics(&metrics,&canvas));
        creation_begin("tale-page"); sdl_tale_screen_render_canvas(&canvas); stop_capture();
        SDL_FRect title=creation_find("=== The Tale So Far ===");
        fixture_assert(title.y+title.h<=metrics.body_y);
        for(int i=0;i<creation_count;i++) {
            SDL_FRect r=creation_text[i].rect;
            if(r.y>=metrics.body_y && r.y<metrics.prompt_y)
                fixture_assert(r.y+r.h<=metrics.prompt_y);
        }
    } while(sdl_tale_screen_advance_page());
    sdl_tale_screen_hide();
}
static void creation_hero_drag(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    fixture_assert(sdl_pause_text_screen_begin());
    sdl_pause_text_screen_add_line("Maedhros the Red!",TERM_YELLOW,0);
    for(int i=0;i<32;i++) sdl_pause_text_screen_add_line("Into the vast and echoing gloom, down awful corridors that wind.",TERM_WHITE,i%4);
    sdl_pause_text_screen_set_visible_lines(100);
    sdl_pause_text_screen_render_canvas(&canvas);
    fixture_assert(g_sdl_standalone_pager.maximum>0); capture("hero-intro-first");
    SDL_FRect area=g_sdl_standalone_pager.viewport;
    float x=area.x+area.w/2,y=area.y+area.h/2,delta=40*fixture_density;
    g_sdl_blocking_key_wait=true;
    SDL_Event event={0}; event.tfinger.windowID=SDL_GetWindowID(g_state.window);
    event.tfinger.touchID=1; event.tfinger.fingerID=7;
    event.tfinger.x=x/fixture_width; event.tfinger.y=y/fixture_height;
    event.type=SDL_EVENT_FINGER_DOWN; sdl_handle_event(&g_state,&event);
    event.tfinger.y=(y-delta)/fixture_height;
    event.type=SDL_EVENT_FINGER_MOTION; sdl_handle_event(&g_state,&event);
    fixture_assert(SDL_fabsf(g_sdl_standalone_pager.offset-MIN(delta,g_sdl_standalone_pager.maximum))<1);
    event.tfinger.y=(y-delta-10)/fixture_height; sdl_handle_event(&g_state,&event);
    fixture_assert(SDL_fabsf(g_sdl_standalone_pager.offset-MIN(delta+10,g_sdl_standalone_pager.maximum))<1);
    event.type=SDL_EVENT_FINGER_UP; sdl_handle_event(&g_state,&event);
    char key=0; fixture_assert(Term_inkey(&key,false,true)!=0);
    fixture_assert(sdl_pause_text_screen_active());
    sdl_pause_text_screen_render_canvas(&canvas); capture("hero-intro-drag");
    fixture_assert(sdl_standalone_screen_drag(x,y,100000));
    fixture_assert(g_sdl_standalone_pager.offset==g_sdl_standalone_pager.maximum);
    fixture_assert(sdl_standalone_screen_drag(x,y,-100000));
    fixture_assert(g_sdl_standalone_pager.offset==0);
    /* Mouse dragging follows the same continuous path, without dismissing. */
    event=(SDL_Event){0}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT; event.button.x=x; event.button.y=y;
    sdl_handle_event(&g_state,&event);
    event.type=SDL_EVENT_MOUSE_MOTION; event.motion.x=x; event.motion.y=y-delta;
    sdl_handle_event(&g_state,&event);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP; event.button.button=SDL_BUTTON_LEFT;
    event.button.x=x; event.button.y=y-delta; sdl_handle_event(&g_state,&event);
    fixture_assert(g_sdl_standalone_pager.offset>0);
    fixture_assert(Term_inkey(&key,false,true)!=0);
    sdl_pause_text_screen_hide(); g_sdl_blocking_key_wait=false;
}
''' + INIT + r'''
    config.bigger_font=true; fixture.active=false;
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,230,230,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){0,255,0,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){255,255,0,255};
    creation_characters(); creation_skills(); creation_lamp(); creation_tale(); creation_hero_drag();
    printf("Creation %dx%d @%.3f: compact stats, aligned calculations, lamp/footer containment, tale actions, hero touch/mouse drag PASS\n",fixture_width,fixture_height,fixture_density);
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
                "/src/sdl/ui/sdl-song-menu.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/input/sdl-screen-pointer.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision", "get_sdl_gameplay_tutorial_mode",
             "SDL_WaitEvent", "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect", "sdl_touch_only_device_active",
             "sdl_touch_only_mobile_device_active", "sdl_get_layout_screen_rect", "sdl_overlay_pane_anchor_rect",
             "SDL_GetDisplayContentScale", "sdl_terminal_menu_font_px", "sdl_mobile_lifecycle_handle_event",
             "sdl_touch_tutorial_device_available", "SDL_RenderTexture", "sdl_touch_pane_point_to_slot"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, density in [(580, 1280, 1.667), (360, 800, 1), (800, 360, 1),
                                    (720, 1600, 2), (1080, 2340, 2.75), (2340, 1080, 2.75)]:
        subprocess.run([str(exe), str(width), str(height), str(density),
                        str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
