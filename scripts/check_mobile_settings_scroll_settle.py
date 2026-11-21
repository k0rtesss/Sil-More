#!/usr/bin/env python3
"""Check that handheld settings swipes settle on complete, usable rows."""
from pathlib import Path

import check_mobile_settings_layout as fixture

ROOT = Path(__file__).resolve().parents[1]
fixture.OUT = ROOT / "scripts/output/mobile-settings-scroll-settle"

fixture.TRACE = fixture.TRACE.replace(
    "static bool fixture_inside_settings_row;",
    "static float fixture_first_visible_text;\nstatic bool fixture_inside_settings_row;", 1)
fixture.TRACE = fixture.TRACE.replace(
    "    if (!fixture_inside_settings_row) return;",
    r'''    if (SDL_RenderClipEnabled(g_state.renderer)) {
        SDL_Rect viewport; SDL_GetRenderClipRect(g_state.renderer,&viewport);
        if (dst->y>=viewport.y-1 && dst->y+dst->h<=viewport.y+viewport.h+1)
            fixture_first_visible_text=MIN(fixture_first_visible_text,dst->y);
    }
    if (!fixture_inside_settings_row) return;''', 1)

fixture.CHECK = r'''
static void settings_touch(Uint32 type, float x, float y)
{
    SDL_Event ev={0};ev.type=type;
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1;ev.tfinger.fingerID=81;ev.tfinger.timestamp=SDL_GetTicksNS();
    ev.tfinger.x=x/fixture_width;ev.tfinger.y=y/fixture_height;
    fixture_assert(sdl_character_sheet_screen_handle_event(&ev));
}
static void settings_clear_actions(void)
{
    int choice=0,action=0;char key=0;
    while(ui_menu_click_take_action(&choice,&action)) {}
    while(Term_inkey(&key,false,true)==0) {}
}
static void settings_frame(bool seen[11], bool settled)
{
    fixture_first_visible_text=1.0e9f;
    fixture_assert(sdl_render_current_window_frame());
    SDL_FRect viewport=g_sdl_character_sheet_screen.select_scroll_rect;
    fixture_assert(viewport.h>0);
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice>=1 && hit->choice<=11) {
            seen[hit->choice-1]=true;
            fixture_assert(hit->rect.y>=viewport.y-1);
            fixture_assert(hit->rect.y+hit->rect.h<=viewport.y+viewport.h+1);
        }
    }
    if(settled && g_sdl_character_sheet_screen.sheet_scroll_max>0) {
        float allowance=g_sdl_character_sheet_screen.last_body_line_h*0.35f+3;
        if(fixture_first_visible_text>viewport.y+allowance)
            fprintf(stderr,"Settings blank top: scroll%d max%d first %.1f top %.1f allowance %.1f\n",
                g_sdl_character_sheet_screen.sheet_scroll,
                g_sdl_character_sheet_screen.sheet_scroll_max,fixture_first_visible_text,
                viewport.y,allowance);
        fixture_assert(fixture_first_visible_text<=viewport.y+allowance);
    }
}
static void settings_drag(bool down)
{
    SDL_FRect viewport=g_sdl_character_sheet_screen.select_scroll_rect;
    float x=viewport.x+viewport.w*.45f;
    float from=viewport.y+viewport.h*(down?.22f:.78f);
    float to=viewport.y+viewport.h*(down?.78f:.22f);
    settings_touch(SDL_EVENT_FINGER_DOWN,x,from);
    settings_touch(SDL_EVENT_FINGER_MOTION,x,to);
    settings_touch(SDL_EVENT_FINGER_UP,x,to);
    fixture_assert(!ui_menu_click_has_pending());
    char key=0;fixture_assert(Term_inkey(&key,false,true)!=0);
}
static void check_mobile_settings_no_crop(void)
{
    /* Fractional row tops round toward the previous pixel so the whole row
     * remains visible; tall rows and explanations keep free scrolling. */
    {
        float tops[]={0.0f,100.7f,600.0f}, heights[]={100.0f,100.0f,100.0f};
        fixture_assert(sdl_char_sheet_settings_settle_scroll(tops,heights,3,
            200.0f,-1.0f,0.0f,102,600)==100);
        heights[1]=500.0f;
        fixture_assert(sdl_char_sheet_settings_settle_scroll(tops,heights,3,
            200.0f,-1.0f,0.0f,350,600)==350);
        heights[1]=100.0f;
        fixture_assert(sdl_char_sheet_settings_settle_scroll(tops,heights,3,
            200.0f,190.0f,400.0f,250,600)==250);
    }
    static cptr rows[]={
        "Minimum Terminal Size\tcompact (50x16)", "Map Zoom\t4",
        "Starting Zoom Offset\t+1", "Camera Vertical Recenter Distance\t3",
        "Camera Horizontal Recenter Distance\t3", "Orientation\tportrait",
        "Portrait Button Wheel Position\tside lane", "Tiles [Alt+A]\tyes",
        "Use Unsafe Area (notch/cutout area)\tno", "Reset All Interface Settings",
        "Return to Options Menu"};
    for(int big=0;big<2;big++) {
        bool seen[11]={0};fixture_set_all_fonts(big);set_sdl_show_menu_font_button(true);
        ui_menu_click_begin();settings_clear_actions();
        sdl_character_sheet_screen_begin_select(1,"Display and Layout");
        sdl_character_sheet_screen_set_font_menu(SDL_MENU_FONT_SETTINGS);
        sdl_character_sheet_screen_set_select_menu_style(true);
        for(int i=0;i<(int)N_ELEMENTS(rows);i++) {
            sdl_character_sheet_screen_add_select_row(i+1,rows[i],TERM_WHITE,"");
            if(i<9) sdl_character_sheet_screen_set_last_select_row_reset(1001+i);
        }
        sdl_character_sheet_screen_set_select_description(
            "Smallest character grid the game will use. A larger grid fits more content by limiting zoom. Compact allows larger map text and tiles on small displays.");
        fixture_assert(sdl_character_sheet_screen_commit_select(1));
        settings_frame(seen,false);
        int maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
        if(maximum>0) {
            int count=0;
            while(g_sdl_character_sheet_screen.sheet_scroll<g_sdl_character_sheet_screen.sheet_scroll_max) {
                int before=g_sdl_character_sheet_screen.sheet_scroll;
                settings_drag(false);settings_frame(seen,true);
                fixture_assert(g_sdl_character_sheet_screen.selected_index==1);
                fixture_assert(g_sdl_character_sheet_screen.sheet_scroll>before);
                fixture_assert(++count<30);
                if(count==1) capture(big?"settings-drag-large":"settings-drag-normal");
            }
            bool last_visible=false;
            for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
                if(g_sdl_character_sheet_screen.hits[i].choice==11) last_visible=true;
            fixture_assert(last_visible);
            count=0;
            while(g_sdl_character_sheet_screen.sheet_scroll>0) {
                int before=g_sdl_character_sheet_screen.sheet_scroll;
                settings_drag(true);settings_frame(seen,true);
                fixture_assert(g_sdl_character_sheet_screen.sheet_scroll<before);
                fixture_assert(++count<30);
            }
        }
        for(int i=0;i<(int)N_ELEMENTS(rows);i++) {
            if(!seen[i]) fprintf(stderr,"Settings row %d unreachable\n",i+1);
            fixture_assert(seen[i]);
        }
        /* Rotate with Orientation selected and the list already scrolled.
         * The same row must remain visible without a blank upper strip. */
        fixture_assert(sdl_character_sheet_screen_commit_select(6));
        settings_frame(seen,false);
        /* Rotate immediately after release, before its pending settle frame. */
        if(g_sdl_character_sheet_screen.sheet_scroll_max>0)
            settings_drag(false);
        int old_width=fixture_width,old_height=fixture_height;
        fixture_width=old_height;fixture_height=old_width;
        fixture_assert(SDL_SetWindowSize(g_state.window,fixture_width,fixture_height));
        g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
        settings_frame(seen,true);
        bool orientation_visible=false;
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
            if(g_sdl_character_sheet_screen.hits[i].choice==6) orientation_visible=true;
        fixture_assert(orientation_visible);
        capture(big?"settings-rotation-large":"settings-rotation-normal");
        fixture_width=old_width;fixture_height=old_height;
        fixture_assert(SDL_SetWindowSize(g_state.window,fixture_width,fixture_height));
        g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
        fixture_assert(sdl_character_sheet_screen_commit_select(1));
        settings_frame(seen,false);
        /* A short tap still selects the intended visible setting. */
        const sdl_character_sheet_hit *target=NULL;
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
            if(g_sdl_character_sheet_screen.hits[i].choice==2)
                target=&g_sdl_character_sheet_screen.hits[i];
        fixture_assert(target);
        float x=target->rect.x+target->rect.w*.5f,y=target->rect.y+target->rect.h*.5f;
        settings_clear_actions();settings_touch(SDL_EVENT_FINGER_DOWN,x,y);
        settings_touch(SDL_EVENT_FINGER_UP,x,y);
        int choice=0,action=0;char key=0;
        fixture_assert(ui_menu_click_take_action(&choice,&action));
        fixture_assert(choice==2 && action==UI_MENU_CLICK_PRIMARY);
        fixture_assert(Term_inkey(&key,false,true)==0);
        sdl_character_sheet_screen_hide();ui_menu_click_clear();
    }
    printf("Settings swipe/rotation %dx%d @%.3f: full rows, no empty strip, all rows reachable, tap preserved: PASS\n",
        fixture_width,fixture_height,fixture_density);
}
'''


def main():
    fixture.main()


if __name__ == "__main__":
    main()
