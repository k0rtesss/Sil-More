#!/usr/bin/env python3
"""Check the actual Insight attribute menu and narrow native birth footers.

Uses isolated offscreen SDL fixtures and standard-build objects. No save or
user configuration is read or written; captures live under scripts/output.
"""
from pathlib import Path
import os
import shlex
import subprocess

from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/insight-attribute-menu"

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include <math.h>
static void fixture_record_text(TTF_Font *font,cptr text, SDL_FRect rect);
/* ACTUAL SCREENS SOURCE */
/* ACTUAL ABILITIES SOURCE */

bool __wrap_sdl_touch_only_device_active(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }
static int input_index, bell_count, confirm_count;
static int test_case;
static const int keys[]={ '2','2','2','\r','d','\r',ESCAPE };
static int selected_glyph_checks;
static void fixture_record_text(TTF_Font *font,cptr text,SDL_FRect rect)
{
    bool selected_label=(g_sdl_character_sheet_screen.context==SDL_CHARACTER_SHEET_BIRTH_SKILLS
            && g_sdl_character_sheet_screen.selected_index==S_SNG && !strncmp(text,"Song",4))
        || (g_sdl_character_sheet_screen.context==SDL_CHARACTER_SHEET_BIRTH_STATS
            && g_sdl_character_sheet_screen.selected_index==A_GRA && !strncmp(text,"Gra",3));
    if(!selected_label || !SDL_RenderClipEnabled(g_state.renderer)) return;
    SDL_Rect clip;
    SDL_GetRenderClipRect(g_state.renderer,&clip);
    SDL_Surface *glyph=TTF_RenderText_Blended(font,text,0,(SDL_Color){255,255,255,255});
    fixture_assert(glyph);
    int first=glyph->h,last=-1;
    for(int y=0;y<glyph->h;y++) for(int x=0;x<glyph->w;x++) {
        Uint8 r,g,b,a;
        fixture_assert(SDL_ReadSurfacePixel(glyph,x,y,&r,&g,&b,&a));
        if(a) { first=MIN(first,y); last=MAX(last,y); }
    }
    float ink_top=rect.y+first*rect.h/glyph->h;
    float ink_bottom=rect.y+(last+1)*rect.h/glyph->h;
    SDL_DestroySurface(glyph);
    if(ink_top<clip.y-1 || ink_bottom>clip.y+clip.h+1)
        fprintf(stderr,"selected %s text rect y %.1f h %.1f, actual glyph ink %.1f..%.1f, real clip y %d h %d\n",text,rect.y,rect.h,ink_top,ink_bottom,clip.y,clip.h);
    fixture_assert(ink_top>=clip.y-1);
    fixture_assert(ink_bottom<=clip.y+clip.h+1);
    ++selected_glyph_checks;
}

static void inside(SDL_FRect r)
{
    fixture_assert(r.w>0 && r.h>0);
    fixture_assert(r.x>=-1 && r.y>=-1);
    fixture_assert(r.x+r.w<=fixture_width+1);
    fixture_assert(r.y+r.h<=fixture_height+1);
}
static bool overlap(SDL_FRect a,SDL_FRect b)
{ return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h; }
static void capture(cptr name)
{
    char path[160];
    strnfmt(path,sizeof(path),"%s-%dx%d.png",name,fixture_width,fixture_height);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels && IMG_SavePNG(pixels,path));
    SDL_DestroySurface(pixels);
}
static SDL_FRect hit(int choice)
{
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        if(g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fprintf(stderr,"missing hit %d in context %d, input %d, case %d, count %d scroll %d/%d\n",choice,g_sdl_character_sheet_screen.context,input_index,test_case,g_sdl_character_sheet_screen.hit_count,g_sdl_character_sheet_screen.sheet_scroll,g_sdl_character_sheet_screen.sheet_scroll_max);
    capture("missing-hit");
    fixture_assert(false); return (SDL_FRect){0};
}
static void ink(SDL_FRect rect)
{
    SDL_Rect area={(int)rect.x+6,(int)rect.y+6,(int)rect.w-12,(int)rect.h-12};
    fixture_assert(area.w>0 && area.h>0);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&area);
    fixture_assert(pixels);
    int count=0;
    for(int y=0;y<pixels->h;y++) for(int x=0;x<pixels->w;x++) {
        Uint8 r,g,b,a;
        fixture_assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
        if(a && r<50 && g<50 && b<50) count++;
    }
    SDL_DestroySurface(pixels);
    fixture_assert(count>8);
}
static char tap_key(SDL_FRect rect)
{
    SDL_Event ev={0};
    ev.tfinger.timestamp=SDL_GetTicksNS();
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=(rect.x+rect.w/2)/fixture_width;
    ev.tfinger.y=(rect.y+rect.h/2)/fixture_height;
    ev.type=SDL_EVENT_FINGER_DOWN;
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
    ev.type=SDL_EVENT_FINGER_UP;
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
    char key=0;
    fixture_assert(Term_inkey(&key,false,true)==0);
    char extra=0;
    fixture_assert(Term_inkey(&extra,false,false)!=0);
    return key;
}

char __wrap_inkey(void)
{
    fixture_assert(input_index<(int)N_ELEMENTS(keys));
    fixture_assert(sdl_character_sheet_screen_active());
    fixture_assert(g_sdl_character_sheet_screen.select_row_count==A_MAX);
    fixture_assert(strstr(g_sdl_character_sheet_screen.select_title,"Attributes"));
    fixture_assert(g_sdl_character_sheet_screen.select_description[0]);
    fixture_assert(sdl_render_current_window_frame());
    SDL_FRect back=hit(-1),choose=hit(-2);
    inside(back); inside(choose); fixture_assert(!overlap(back,choose));
    ink(back); ink(choose);
    SDL_FRect scroll=g_sdl_character_sheet_screen.select_scroll_rect;
    inside(scroll);
    fixture_assert(scroll.y+scroll.h<=MIN(back.y,choose.y)+1);
    int selected=g_sdl_character_sheet_screen.selected_index;
    inside(hit(selected));
    if(input_index==3) {
        fixture_assert(selected==A_GRA);
        capture(test_case==0?"insight-no-funds":test_case==1?"insight-max":"insight-cancel");
        int saved_scroll=g_sdl_character_sheet_screen.sheet_scroll;
        int max_scroll=g_sdl_character_sheet_screen.sheet_scroll_max;
        g_sdl_character_sheet_screen.sheet_scroll=max_scroll;
        fixture_assert(sdl_render_current_window_frame());
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==max_scroll);
        fixture_assert(g_sdl_character_sheet_screen.last_desc_line_h>0);
        capture("insight-description-end");
        ink(hit(-1)); ink(hit(-2));
        g_sdl_character_sheet_screen.sheet_scroll=saved_scroll;
        fixture_assert(sdl_render_current_window_frame());
        inside(hit(A_GRA));
    }
    int index=input_index++;
    if(index==3 || index==5) return tap_key(choose);
    if(index==6) return tap_key(back);
    return keys[index];
}
void __wrap_bell(cptr reason)
{
    fixture_assert(reason && reason[0]);
    if(test_case==0) fixture_assert(strstr(reason,"enough insight"));
    if(test_case==1) fixture_assert(strstr(reason,"cannot be increased"));
    ++bell_count;
}
bool __wrap_get_check(cptr prompt)
{
    fixture_assert(test_case==2);
    fixture_assert(strstr(prompt,"Increase Grace"));
    fixture_assert(!sdl_character_sheet_screen_active());
    ++confirm_count;
    return false;
}
void __wrap_handle_stuff(void) {}

static void check_insight(int ruleset,int which)
{
    test_case=which; input_index=bell_count=confirm_count=0;
    p_ptr->insight_ruleset=ruleset;
    p_ptr->insight_points=which==2?100:0;
    for(int i=0;i<A_MAX;i++) {
        p_ptr->stat_base[i]=which==1?BASE_STAT_MAX:2;
        p_ptr->insight_stat_invested[i]=0;
    }
    s32b before_turn=turn,before_points=p_ptr->insight_points;
    int before_stats[A_MAX];
    for(int i=0;i<A_MAX;i++) before_stats[i]=p_ptr->stat_base[i];
    fixture_assert(which==1?insight_stat_increase_cost(A_GRA)==0:
        insight_stat_increase_cost(A_GRA)>0);
    ability_browser_train_attributes();
    fixture_assert(input_index==(int)N_ELEMENTS(keys));
    fixture_assert(bell_count==(which==2?0:2));
    fixture_assert(confirm_count==(which==2?2:0));
    fixture_assert(!sdl_character_sheet_screen_active());
    fixture_assert(turn==before_turn && p_ptr->insight_points==before_points);
    for(int i=0;i<A_MAX;i++) {
        fixture_assert(p_ptr->stat_base[i]==before_stats[i]);
        fixture_assert(p_ptr->insight_stat_invested[i]==0);
    }
}

static void check_birth_footer(bool skills)
{
    int base[S_MAX]={0},gain[S_MAX]={0},costs[S_MAX]={0};
    character_generated=character_dungeon=false; turn=0;
    int before_checks=selected_glyph_checks;
    ui_menu_click_begin();
    if(skills) sdl_character_sheet_screen_show_birth_skills(base,gain,costs,S_SNG,5000);
    else sdl_character_sheet_screen_show_birth_stats(base,costs,A_GRA,5000);
    fixture_assert(sdl_render_current_window_frame());
    fixture_assert(selected_glyph_checks>before_checks);
    SDL_FRect back=hit(-1),confirm=hit(-2),defaults=hit(-4);
    inside(back); inside(confirm); inside(defaults);
    fixture_assert(!overlap(back,confirm) && !overlap(back,defaults) && !overlap(confirm,defaults));
    bool stacked=defaults.y+defaults.h<=MIN(back.y,confirm.y);
    if(stacked) fixture_assert(defaults.w>back.w && defaults.w>confirm.w);
    else {
        fixture_assert(fabsf(defaults.y-back.y)<=1 && fabsf(defaults.y-confirm.y)<=1);
        fixture_assert(back.x+back.w<=defaults.x && defaults.x+defaults.w<=confirm.x);
    }
    ink(back); ink(confirm); ink(defaults);
    TTF_Font *font=sdl_story_font_for_height_slot(g_sdl_character_sheet_prompt_px,
        g_sdl_character_sheet_prompt_slot);
    fixture_assert(font);
    cptr labels[]={"Back","Confirm","Defaults"};
    SDL_FRect rects[]={back,confirm,defaults};
    for(int i=0;i<3;i++) {
        int w=0,h=0;
        fixture_assert(TTF_GetStringSize(font,labels[i],0,&w,&h));
        if(w>rects[i].w*.84f) {
            fixture_assert(TTF_GetStringSize(font,i==0?"<":i==1?"OK":"Defaults",0,&w,&h));
        }
        if(w>rects[i].w+1) fprintf(stderr,"birth %s footer %s px %d glyph %dx%d rect %.1f %.1f\n",skills?"skills":"stats",labels[i],g_sdl_character_sheet_prompt_px,w,h,rects[i].w,rects[i].h);
        fixture_assert(w<=rects[i].w+1);
        fixture_assert(h<=rects[i].h+1);
    }
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *row=&g_sdl_character_sheet_screen.hits[i];
        if(row->choice>=0) {
            inside(row->rect);
            fixture_assert(!overlap(row->rect,back) && !overlap(row->rect,confirm) && !overlap(row->rect,defaults));
        }
    }
    capture(skills?"birth-skills-footer":"birth-stats-footer");
    int choices[]={-1,-2,-4};
    SDL_FRect buttons[]={back,confirm,defaults};
    for(int i=0;i<3;i++) {
        int actual=0,action=0;
        char key=tap_key(buttons[i]);
        fixture_assert(ui_menu_click_take_action(&actual,&action));
        fixture_assert(actual==choices[i] && action==UI_MENU_CLICK_PRIMARY);
        fixture_assert(key==(choices[i]==-1?ESCAPE:'\r'));
    }
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    character_generated=character_dungeon=true;
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0}; player_other options={0};
    object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==6); setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config); config.bigger_font=true;
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM; config.use_unsafe_area=true;
    config.aux_view_font_size=24; g_state.system_scale=1;
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    player.playing=true; character_generated=character_dungeon=true;
    fixture_width=atoi(argv[2]); fixture_height=atoi(argv[3]); fixture_font=24;
    SDL_strlcpy(fixture_id,"insight-attributes/birth-footer",sizeof(fixture_id));
    g_state.window=SDL_CreateWindow("Attribute fixture",fixture_width,fixture_height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); fixture_assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256); Term_activate(&view->t); term_screen=&view->t;
    character_icky=1; fixture.active=false;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_SLATE]=(SDL_Color){150,150,150,255};
    g_state.palette[TERM_WHITE]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,210,220,255};
    check_birth_footer(false); check_birth_footer(true);
    printf("%dx%d: Stats/Skills footer bounds/glyphs/nonoverlap: PASS\n",fixture_width,fixture_height);
    for(int rules=INSIGHT_RULESET_LEGACY;rules<=INSIGHT_RULESET_REWORKED;rules++)
        for(int which=0;which<3;which++) check_insight(rules,which);
    printf("%dx%d: both Insight rulesets, Grace selection, rejected/cancelled purchases, clean exit, zero turns, Stats/Skills footer bounds/glyphs/nonoverlap: PASS\n",fixture_width,fixture_height);
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    # externs.h contains enum definitions without an include guard. The SDL
    # fixture prefix already includes it, so omit its second inclusion only.
    abilities = (ROOT / "src/cmd/ui/cmd-ui-abilities.c").read_text()
    abilities = abilities.replace('#include "externs.h"', '')
    screens = (ROOT / "src/sdl/ui/sdl-screens.c").read_text()
    start = screens.index("static SDL_FRect sdl_char_sheet_draw_text_aligned(")
    end = screens.index("static SDL_FRect sdl_char_sheet_draw_text_alpha(", start)
    drawing = screens[start:end].replace("SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);",
        "fixture_record_text(font, text, dst);\n    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);")
    screens = screens[:start] + drawing + screens[end:]
    source.write_text(HARNESS.replace("/* ACTUAL ABILITIES SOURCE */", abilities)
                      .replace("/* ACTUAL SCREENS SOURCE */", screens), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/cmd/ui/cmd-ui-abilities.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                 if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision",
             "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout",
             "sdl_touch_thumb_current_bounds", "sdl_map_grid_cell_rect",
             "sdl_touch_only_device_active", "sdl_get_layout_screen_rect",
             "inkey", "bell", "get_check", "handle_stuff"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    failures = []
    for width, height in [(320, 568), (360, 800), (800, 360), (1080, 2400), (2400, 1080)]:
        result = subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(width), str(height), str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf")],
                       cwd=OUT, env=env, timeout=60)
        if result.returncode:
            failures.append(f"{width}x{height}")
    if failures:
        raise SystemExit("Failed render fixtures: " + ", ".join(failures))


if __name__ == "__main__":
    main()
