#!/usr/bin/env python3
"""Render hero previews and check power ratings stay inside their panel."""
from pathlib import Path

import check_options_tutorial as runner
from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#include "sdl/ui/sdl-screens.c"

void __wrap_tutorial_continue(void) { fixture.active=false; }
bool __wrap_sdl_touch_only_device_active(void) { return false; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static SDL_FRect rating_rect(int choice)
{
    for (int i=0;i<g_sdl_character_sheet_screen.hit_count;++i)
        if (g_sdl_character_sheet_screen.hits[i].choice==choice)
            return g_sdl_character_sheet_screen.hits[i].rect;
    fixture_assert(false);
    return (SDL_FRect){0};
}

static void check_preview(int width,int height,int abilities)
{
    static const char *heroes[]={"Feanor", "Maedhros", "Curufin", "Celegorm",
        "Amrod", "Amras", "Maglor", "Caranthir", "Celebrimbor"};
    static const char *stats[]={"STR\t+2", "DEX\t+3", "CON\t+3", "GRA\t+4"};
    static const char *powers[]={"Mighty", "Strong", "Fair", "Weak"};
    static const char *stars[]={"***", "**", "*", "*"};
    SDL_FRect stat_panel, ability_panel;
    fixture_width=width; fixture_height=height;
    SDL_strlcpy(fixture_id,"hero-preview",sizeof(fixture_id));
    g_state.window=SDL_CreateWindow("Hero preview fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256);
    Term_activate(&view->t); term_screen=&view->t;
    character_icky=1;
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_begin_select(0,"Choose your hero");
    for (int i=0;i<9;++i)
        sdl_character_sheet_screen_add_select_row(i,heroes[i],TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_title_detail("Feanor, Spirit of Fire"," ***",TERM_GREEN);
    sdl_character_sheet_screen_set_select_description(
        "You rise aflame, spirit of fire and maker of Silmarils.\n\n"
        "Mighty spirit, proud among the Eldar, you fell under shadow. "
        "Your defiance and grief bind you to this tale, to face flame and ruin.");
    sdl_character_sheet_screen_set_select_detail_size_hint(4,2,9);
    sdl_character_sheet_screen_set_select_ability_rows(abilities);
    for (int i=0;i<4;++i)
        sdl_character_sheet_screen_add_select_detail(stats[i],TERM_L_BLUE,"Attribute bonus");
    for (int i=0;i<abilities;++i)
        sdl_character_sheet_screen_add_select_detail(i==0?"Artifice":"Jeweler",TERM_RED,"Starting ability");
    for (int i=0;i<9;++i)
        sdl_character_sheet_screen_add_select_detail("Melee Affinity",TERM_GREEN,"Trait bonus");
    sdl_character_sheet_screen_begin_select_rating_summary("Heroes Power");
    for (int i=0;i<4;++i)
        sdl_character_sheet_screen_add_select_rating(powers[i],stars[i],14-i,
            TERM_GREEN,"Heroes still alive");
    fixture_assert(sdl_character_sheet_screen_commit_select(0));
    fixture_assert(sdl_render_current_window_frame());
    fixture_assert(sdl_char_sheet_panel_rect("Stats",&stat_panel));
    fixture_assert(sdl_char_sheet_panel_rect("Abilities",&ability_panel));
    for (int i=0;i<4;++i) {
        SDL_FRect row=rating_rect(9100+i);
        fixture_assert(row.y>=stat_panel.y);
        fixture_assert(row.y+row.h<=stat_panel.y+stat_panel.h+0.5f);
        fixture_assert(!SDL_HasRectIntersectionFloat(&row,&ability_panel));
        fixture_assert(row.x>=0 && row.x+row.w<=width);
        fixture_assert(row.y>=0 && row.y+row.h<=height);
    }
    char file[80];
    strnfmt(file,sizeof(file),"hero-preview-%dx%d-%d.png",width,height,abilities);
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels!=NULL && IMG_SavePNG(pixels,file));
    SDL_DestroySurface(pixels);
    printf("Hero preview %dx%d / %d abilities: power ratings contained, no panel overlap: PASS\n",
        width,height,abilities);
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0};
    fixture_assert(argc==3);
    setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.use_unsafe_area=true;
    g_state.system_scale=1; z_info=&limits; p_ptr=&player;
    for (int i=0;i<16;++i)
        g_state.palette[i]=(SDL_Color){angband_color_table[i][1],
            angband_color_table[i][2],angband_color_table[i][3],255};
    check_preview(768,576,2);
    check_preview(768,576,1);
    check_preview(1280,720,2);
    check_preview(1920,1080,2);
    check_preview(580,1280,2);
    TTF_Quit(); SDL_Quit();
    return 0;
}
'''


def main():
    runner.OUT = Path(__file__).resolve().parent / "output/character-selection-layout"
    runner.HARNESS = HARNESS
    runner.main()


if __name__ == "__main__":
    main()
