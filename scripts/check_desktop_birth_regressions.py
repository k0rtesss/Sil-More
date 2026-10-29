#!/usr/bin/env python3
"""Exercise desktop birth input, race lore, and intro pagination offscreen.

Uses the production SDL renderer/event dispatcher with the dummy video driver,
an isolated in-memory profile, and no saves or deployed game artifacts.
"""
from pathlib import Path
import os

import check_options_tutorial as runner
from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
SCREENS = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")
start = SCREENS.index("static SDL_FRect sdl_char_sheet_draw_text_aligned(")
end = SCREENS.index("static SDL_FRect sdl_char_sheet_draw_text_alpha(", start)
SCREENS = SCREENS[:start] + SCREENS[start:end].replace(
    "sdl_ui_render_text_texture(texture, &src, &dst);",
    "record_text(text, attr, dst);\n    sdl_ui_render_text_texture(texture, &src, &dst);",
) + SCREENS[end:]

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#include <math.h>
#include "ui/colors.h"
static void record_text(cptr, byte, SDL_FRect);
/* SCREENS */

void __wrap_tutorial_continue(void) { fixture.active=false; }
bool __wrap_sdl_touch_only_device_active(void) { return false; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static struct { char text[768]; byte attr; SDL_FRect rect; } painted[1024];
static int painted_count;
static void record_text(cptr text, byte attr, SDL_FRect rect)
{
    if (painted_count >= (int)N_ELEMENTS(painted)) return;
    SDL_strlcpy(painted[painted_count].text,text,sizeof(painted[painted_count].text));
    painted[painted_count].attr=attr; painted[painted_count++].rect=rect;
}
static void frame(void)
{
    painted_count=0;
    fixture_assert(sdl_render_current_window_frame());
}
static void capture(cptr stem)
{
    char file[128];
    strnfmt(file,sizeof(file),"%s-%dx%d.png",stem,fixture_width,fixture_height);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels && IMG_SavePNG(pixels,file));
    SDL_DestroySurface(pixels);
}
static SDL_FRect row_hit(int choice)
{
    for (int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        if (g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fprintf(stderr,"missing row %d\n",choice); fixture_assert(false);
    return (SDL_FRect){0};
}
static void take_click(int want, int action)
{
    int choice=-99,got=-99;
    fixture_assert(ui_menu_click_take_action(&choice,&got));
    fixture_assert(choice==want && got==action);
    ui_menu_click_clear();
    char key;
    while(!Term_inkey(&key,false,true)) {}
}
static void check_selection_hover(int choice, cptr label)
{
    SDL_FRect row=row_hit(choice);
    int selected=g_sdl_character_sheet_screen.selected_index;
    char description[4096];
    SDL_strlcpy(description,g_sdl_character_sheet_screen.select_description,sizeof(description));
    fixture_assert(choice!=selected);
    fixture_assert(sdl_character_sheet_screen_handle_pointer_motion(row.x+row.w/2,row.y+row.h/2));
    fixture_assert(!ui_menu_click_has_pending());
    fixture_assert(g_sdl_character_sheet_screen.selected_index==selected);
    fixture_assert(g_sdl_character_sheet_screen.focus_choice==selected);
    fixture_assert(streq(g_sdl_character_sheet_screen.select_description,description));
    frame();
    bool highlighted=false;
    for(int i=0;i<painted_count;i++)
        if(streq(painted[i].text,label)) highlighted=painted[i].attr==TERM_DARK;
    fixture_assert(highlighted);
    if(fixture_width==1280 && fixture_height==720)
        capture(g_sdl_character_sheet_screen.select_intro[0] ? "race-hover" : "hero-hover");
    fixture_assert(sdl_character_sheet_screen_handle_pointer_motion(-1,-1));
    fixture_assert(!ui_menu_click_has_pending());
    frame();
    bool cleared=false;
    for(int i=0;i<painted_count;i++)
        if(streq(painted[i].text,label)) cleared=painted[i].attr==TERM_WHITE;
    fixture_assert(cleared);
    fixture_assert(g_sdl_character_sheet_screen.selected_index==selected);
    fixture_assert(streq(g_sdl_character_sheet_screen.select_description,description));
}
static float srgb_linear(Uint8 channel)
{
    float value = channel / 255.0f;
    return value <= 0.04045f ? value / 12.92f
        : powf((value + 0.055f) / 1.055f, 2.4f);
}
static float contrast_on_black(int attr)
{
    const byte* c = angband_color_table[attr];
    return (0.2126f * srgb_linear(c[1]) + 0.7152f * srgb_linear(c[2])
        + 0.0722f * srgb_linear(c[3]) + 0.05f) / 0.05f;
}
static void assert_painted_color(SDL_Surface* pixels, cptr label, int attr)
{
    const byte* target = angband_color_table[attr];
    int best_distance=1000, best_r=0, best_g=0, best_b=0;
    bool found=false;
    for (int i=0;i<painted_count;i++) {
        if (!streq(painted[i].text,label)) continue;
        SDL_FRect rect=painted[i].rect;
        for (int y=MAX(0,(int)rect.y);y<MIN(pixels->h,(int)SDL_ceilf(rect.y+rect.h));y++)
            for (int x=MAX(0,(int)rect.x);x<MIN(pixels->w,(int)SDL_ceilf(rect.x+rect.w));x++) {
                Uint8 r,g,b,a;
                fixture_assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
                int distance=ABS((int)r-target[1])+ABS((int)g-target[2])+ABS((int)b-target[3]);
                if (distance<best_distance) {best_distance=distance;best_r=r;best_g=g;best_b=b;}
            }
        found=true;
        break;
    }
    fixture_assert(found && best_distance<=9);
    printf("%s pixel (%d,%d,%d), target (%d,%d,%d), contrast %.2f:1\n",
        label,best_r,best_g,best_b,target[1],target[2],target[3],contrast_on_black(attr));
}
static void check_hero_and_skill(void)
{
    static cptr stats[]={"STR\t+2","DEX\t+3","CON\t+3","GRA\t+4"};
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(false);
    sdl_character_sheet_screen_begin_select(0,"Choose your hero");
    sdl_character_sheet_screen_add_select_row(0,"Feanor",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_row(1,"Maedhros",TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_title_detail("Feanor, Spirit of Fire"," ***",TERM_GREEN);
    sdl_character_sheet_screen_set_select_description("A bright spirit.\n\nA long tale of pride and courage.");
    sdl_character_sheet_screen_set_select_detail_size_hint(4,0,4);
    for(int i=0;i<4;i++) sdl_character_sheet_screen_add_select_detail(stats[i],TERM_L_BLUE,"Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("Melee penalty",TERM_RED,"Penalty");
    sdl_character_sheet_screen_add_select_detail("Archery grand penalty",TERM_L_RED,"Grand penalty");
    sdl_character_sheet_screen_add_select_detail("Kinslayer",TERM_UMBER,"Curse");
    sdl_character_sheet_screen_add_select_detail("Song affinity",TERM_L_GREEN,"Affinity");
    fixture_assert(sdl_character_sheet_screen_commit_select(0));
    frame();
    bool penalty=false,grand_penalty=false,curse=false,affinity=false;
    for(int i=0;i<painted_count;i++) {
        if(streq(painted[i].text,"Melee penalty")) penalty=painted[i].attr==TERM_L_RED;
        if(streq(painted[i].text,"Archery grand penalty")) grand_penalty=painted[i].attr==TERM_L_RED;
        if(streq(painted[i].text,"Kinslayer")) curse=painted[i].attr==TERM_L_UMBER;
        if(streq(painted[i].text,"Song affinity")) affinity=painted[i].attr==TERM_L_GREEN;
    }
    fixture_assert(penalty && grand_penalty && curse && affinity);
    if(fixture_width==1280 && fixture_height==720) {
        SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
        fixture_assert(pixels);
        fixture_assert(contrast_on_black(TERM_L_RED)>contrast_on_black(TERM_RED));
        fixture_assert(contrast_on_black(TERM_L_RED)>=4.5f);
        fixture_assert(contrast_on_black(TERM_L_UMBER)>contrast_on_black(TERM_UMBER));
        fixture_assert(contrast_on_black(TERM_L_UMBER)>=4.5f);
        assert_painted_color(pixels,"Melee penalty",TERM_L_RED);
        assert_painted_color(pixels,"Archery grand penalty",TERM_L_RED);
        assert_painted_color(pixels,"Kinslayer",TERM_L_UMBER);
        SDL_DestroySurface(pixels);
        cptr name=getenv("BIRTH_CONTRAST_CAPTURE");
        capture(name && name[0] ? name : "contrast-classic-after");
    }
    check_selection_hover(1,"Maedhros");
    SDL_FRect row=row_hit(1);
    fixture_assert(sdl_character_sheet_screen_handle_pointer_button(row.x+row.w/2,row.y+row.h/2,UI_MENU_CLICK_PRIMARY));
    take_click(1,UI_MENU_CLICK_PRIMARY);
    fixture_assert(sdl_character_sheet_screen_commit_select(1));
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(false); frame();
    row=row_hit(1);
    fixture_assert(sdl_character_sheet_screen_handle_pointer_button(row.x+row.w/2,row.y+row.h/2,UI_MENU_CLICK_PRIMARY));
    take_click(1,UI_MENU_CLICK_PRIMARY);
    sdl_character_sheet_screen_hide();

    int old[S_MAX]={0},gain[S_MAX]={0},cost[S_MAX]={0};
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(false);
    sdl_character_sheet_screen_show_birth_skills(old,gain,cost,S_MEL,800); frame();
    row=row_hit(S_ARC);
    fixture_assert(sdl_character_sheet_screen_handle_pointer_motion(row.x+row.w/2,row.y+row.h/2));
    fixture_assert(!ui_menu_click_has_pending());
    fixture_assert(sdl_char_sheet_choice_focused(S_MEL) && !sdl_char_sheet_choice_focused(S_ARC));
    fixture_assert(sdl_character_sheet_screen_handle_pointer_button(row.x+row.w/2,row.y+row.h/2,UI_MENU_CLICK_PRIMARY));
    take_click(S_ARC,UI_MENU_CLICK_PRIMARY);
    sdl_character_sheet_screen_hide();
}
static void check_race(void)
{
    static cptr names[]={"Feanorians","Fingolfinrim","Finarfinrim","Sindar","Naugrim","Edain"};
    static cptr lore="Proud-hearted sons of flame, heirs bound eternally by their father's "
        "relentless Oath, pursuing stolen Jewels through ruin and despair. "
        "Noble in pride yet shadowed by deeds darkened by betrayal and grief; "
        "their swords ever turned against kin, blood staining their legacy. "
        "Driven to exile, oath-bound fate guiding their feet toward sorrow, "
        "foretold to fade in flame and ruin beneath desperate quest's end, "
        "leaving memory of greatness marred forever by tragic pride.";
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(false);
    sdl_character_sheet_screen_begin_select(0,"The War of the Jewels");
    sdl_character_sheet_screen_set_select_intro("The tale of the war.");
    sdl_character_sheet_screen_set_select_frame("The trial.","The charge.");
    sdl_character_sheet_screen_open_select_choice_page();
    for(int i=0;i<6;i++) {
        if(i==0) sdl_character_sheet_screen_add_select_heading("The Noldor — exiled High Elves of three royal houses, whose names you may take:");
        if(i==3) sdl_character_sheet_screen_add_select_heading("The other free peoples of Beleriand:");
        sdl_character_sheet_screen_add_select_row(i,names[i],TERM_WHITE,"");
        sdl_character_sheet_screen_add_select_description_candidate(lore);
    }
    sdl_character_sheet_screen_set_select_description(lore);
    fixture_assert(sdl_character_sheet_screen_commit_select(0)); frame();
    check_selection_hover(1,"Fingolfinrim");
    int max=g_sdl_character_sheet_screen.sheet_scroll_max;
    if(max>0) {
        fixture_assert(sdl_character_sheet_scroll_active());
        SDL_Event key={0}; key.type=SDL_EVENT_KEY_DOWN; key.key.key=SDLK_PAGEDOWN;
        for(int n=0;n<100 && g_sdl_character_sheet_screen.sheet_scroll<max;n++) {
            fixture_assert(sdl_character_sheet_screen_handle_event(&key)); frame();
        }
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==max);
        SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            const sdl_character_sheet_hit* h=&g_sdl_character_sheet_screen.hits[i];
            if(h->choice>=0 && h->choice<6)
                fixture_assert(h->rect.y>=view.y-1 && h->rect.y+h->rect.h<=view.y+view.h+1);
        }
        key.key.key=SDLK_PAGEUP;
        fixture_assert(sdl_character_sheet_screen_handle_event(&key));
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll<max);
        SDL_Event wheel={0}; wheel.type=SDL_EVENT_MOUSE_WHEEL; wheel.wheel.y=-100;
        fixture_assert(sdl_character_sheet_screen_handle_event(&wheel)); frame();
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==max);
    }
    bool tail=false;
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    for(int i=0;i<painted_count;i++) if(strstr(painted[i].text,"pride.")) {
        SDL_FRect r=painted[i].rect;
        tail=max==0 ? r.y+r.h<=fixture_height
            : (r.y>=view.y-1 && r.y+r.h<=view.y+view.h+1);
    }
    fixture_assert(tail); capture("race-lore");
    printf("Race lore %dx%d: last line visible, scroll=%d: PASS\n",fixture_width,fixture_height,max);
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}
static void check_intro(void)
{
    static cptr text="Far below, Morgoth waits upon his throne—iron-dark and crowned in flame. "
        "Upon his brow shine three Silmarils, stolen stars. He senses your stirring. He knows you will come.";
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    fixture_assert(sdl_tale_screen_begin("THE NAMELESS CHRONICLE"));
    sdl_tale_screen_set_manuscript(true); sdl_tale_screen_add_entry("",text);
    sdl_tale_screen_set_prompt("[Enter] turn the page  *  [Esc] skip intro",true,false);
    sdl_tale_layout_metrics metrics; fixture_assert(sdl_tale_screen_metrics(&metrics,&canvas));
    sdl_tale_screen_build_layout(&metrics);
    for(int page=0;page<g_sdl_tale_screen.page_count;page++) {
        int start=g_sdl_tale_screen.page_starts[page],end=g_sdl_tale_screen.page_starts[page+1];
        fixture_assert(end>start);
        fixture_assert((end-start)*metrics.body_line_h<=metrics.body_h+1);
        if(page+1<g_sdl_tale_screen.page_count) {
            cptr last=g_sdl_tale_screen.layout_lines[end-1].text;
            size_t len=strlen(last);
            fixture_assert(len>0 && (last[len-1]=='.' || last[len-1]=='!' || last[len-1]=='?'));
        }
        if(fixture_width==1024 && fixture_height==768) {
            printf("  page %d:",page+1);
            for(int line=start;line<end;line++) printf(" [%s]",g_sdl_tale_screen.layout_lines[line].text);
            printf("\n");
        }
    }
    printf("Intro %dx%d: %d bounded pages and sentence endings: PASS\n",fixture_width,fixture_height,g_sdl_tale_screen.page_count);
    sdl_tale_screen_hide();
}
static void check(int width,int height)
{
    fixture_width=width; fixture_height=height;
    g_state.window=SDL_CreateWindow("Birth fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); fixture_assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN]; term_init(&view->t,80,24,256);
    Term_activate(&view->t); term_screen=&view->t; character_icky=1;
    config.bigger_font=false;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=false;
    check_race();
    if(width>=768) check_hero_and_skill();
    config.bigger_font=true;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=true;
    check_intro();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}
int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0}; object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==3);
    setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"); fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.use_unsafe_area=true; g_state.system_scale=1; z_info=&limits; p_ptr=&player; inventory=items;
    player.song1=player.song2=SNG_NOTHING;
    ANGBAND_DIR_PREF=getenv("SDL_BIRTH_PREF");
    fixture_assert(ANGBAND_DIR_PREF && ui_colors_load_palette_presets());
    fixture_assert(ui_colors_apply_palette_preset("classic"));
    fixture_assert(streq(ui_colors_current_palette_preset(),"classic"));
    fixture_assert(angband_color_table[TERM_RED+TERM_SHADE][1]==144);
    sdl_sync_palette();
    check(640,480); check(768,576); check(1024,768); check(1280,720); check(1920,1080);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''.replace("/* SCREENS */", SCREENS)


def main():
    os.environ["SDL_BIRTH_PREF"] = str(ROOT / "lib/pref")
    runner.OUT = ROOT / "scripts/output/desktop-birth-regressions"
    runner.HARNESS = HARNESS
    runner.main()


if __name__ == "__main__":
    main()
