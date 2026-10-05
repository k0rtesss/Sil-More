#!/usr/bin/env python3
"""Render real option rows and check yes/no values remain readable beside Reset."""
from pathlib import Path

import check_options_tutorial as runner
from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")] + r'''
#include "sdl/ui/sdl-screens.c"
void __wrap_tutorial_continue(void) { fixture.active=false; }
bool __wrap_sdl_touch_only_device_active(void) { return false; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static void check_values(int width,int height,int selected)
{
    static const char *labels[]={
        "Original quest 1: Tulkas", "Original quest 2: Aule",
        "Original quest 3: Mandos", "Original quest 4: Nienna",
        "Original quest 5: Orome", "Original quest 6: Varda",
        "Beta quest 7: Aule ore", "Beta quest 8: Mandos souls",
        "Beta quest 9: Orome hunt", "Beta quest 10: Orome wraiths",
        "Beta quest 11: Nienna and Morgoth", "Beta quest 12: Nienna pacifist",
        "Beta quest 13: Tulkas orcs", "Beta quest 14: Tulkas and Morgoth",
        "Beta quest 15: Varda shadow", "Beta quest 16: Varda and Ungoliant",
        "Quest rules (Beta)", "Quest rewards (Beta)",
        "Quest challenges (Beta)", "Quest lineage (Beta)"};
    fixture_width=width; fixture_height=height;
    SDL_strlcpy(fixture_id,"option-values",sizeof(fixture_id));
    g_state.window=SDL_CreateWindow("Option value fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,96,36,256);
    Term_activate(&view->t); term_screen=&view->t;
    character_icky=1;
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_begin_select(selected,"Quest Options");
    sdl_character_sheet_screen_set_select_menu_style(true);
    for (int i=0;i<(int)N_ELEMENTS(labels);i++) {
        if (i==0 || i==6 || i==16)
            sdl_character_sheet_screen_add_select_heading(i==0?"Original quests":i==6?"Beta quests":"Beta systems");
        char line[160]; strnfmt(line,sizeof(line),"%s\t%s",labels[i],i%2?"no":"yes");
        sdl_character_sheet_screen_add_select_row(i,line,TERM_L_GREEN,"");
        sdl_character_sheet_screen_set_last_select_row_reset(100+i);
    }
    sdl_character_sheet_screen_set_select_description(
        "Disabling this quest preserves its existing progress.");
    fixture_assert(sdl_character_sheet_screen_commit_select(selected));
    fixture_assert(sdl_render_current_window_frame());
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels!=NULL);
    char file[96]; strnfmt(file,sizeof(file),"option-values-%dx%d-%d.png",width,height,selected);
    fixture_assert(IMG_SavePNG(pixels,file));
    int checked=0;
    for (int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if (hit->choice<0 || hit->choice>=(int)N_ELEMENTS(labels) || hit->choice==selected)
            continue;
        SDL_FRect rect=hit->rect;
        /* Check only complete rows, outside the title and description bands. */
        SDL_FRect visible=g_sdl_character_sheet_screen.select_scroll_rect;
        if (rect.y<visible.y || rect.y+rect.h>visible.y+visible.h) continue;
        int min_x=width,min_y=height,max_x=-1,max_y=-1,count=0;
        for (int y=MAX(0,(int)rect.y);y<MIN(height,(int)(rect.y+rect.h));y++)
            for (int x=MAX(0,(int)rect.x);x<MIN(width,(int)(rect.x+rect.w));x++) {
                Uint8 r,g,b,a;
                fixture_assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
                /* Only values are green: labels/headings/Reset use neutral colours. */
                if (g>r+40 && g>b+40) {
                    min_x=MIN(min_x,x); max_x=MAX(max_x,x);
                    min_y=MIN(min_y,y); max_y=MAX(max_y,y); count++;
                }
            }
        if (count<20 || max_x-min_x<12 || max_y-min_y<8) {
            fprintf(stderr,"Option %d @ %dx%d has unreadable value: %d pixels, %dx%d\n",
                hit->choice,width,height,count,max_x-min_x+1,max_y-min_y+1);
            exit(1);
        }
        checked++;
    }
    fixture_assert(checked>=2);
    printf("Option yes/no values %dx%d focus %d: %d readable rows beside Reset PASS\n",width,height,selected,checked);
    SDL_DestroySurface(pixels);
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0};
    fixture_assert(argc==3); setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    char *slash=strrchr(config.story_font,'\\');
    if (!slash) slash=strrchr(config.story_font,'/');
    fixture_assert(slash!=NULL);
    SDL_strlcpy(slash+1,"Cinzel-Medium.ttf",sizeof(config.story_font)-(size_t)(slash+1-config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.use_unsafe_area=true; g_state.system_scale=1; z_info=&limits; p_ptr=&player;
    for (int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){angband_color_table[i][1],
        angband_color_table[i][2],angband_color_table[i][3],255};
    check_values(768,576,0); check_values(768,576,16);
    check_values(1280,720,0); check_values(580,1280,16);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    runner.OUT = Path(__file__).resolve().parent / "output/option-value-visibility"
    runner.HARNESS = HARNESS
    runner.main()


if __name__ == "__main__":
    main()
