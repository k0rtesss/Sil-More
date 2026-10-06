#!/usr/bin/env python3
"""Exercise the real SDL larger-font layouts with isolated offscreen fixtures.

No player save or user config is opened. Screenshots and temporary JSON live
under scripts/output/bigger-font-check. Requires the configured standard build.
"""
from pathlib import Path
import os
import shlex
import subprocess

from check_gameplay_tutorial_render import HARNESS as CARD_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bigger-font-check"

HARNESS = CARD_HARNESS[:CARD_HARNESS.index("static void field(")].replace(
    "float scale=w>width && width>0?width/w:1;",
    "if (config.bigger_font && width>0) "
    "TTF_GetStringSizeWrapped(font,text,0,(int)width,&w,&h); "
    "float scale=config.bigger_font?1:(w>width && width>0?width/w:1);"
) + r'''
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 1
#include "sdl/ui/sdl-screens.c"
#include "sdl/ui/sdl-song-menu.c"
#include "sdl/ui/sdl-question-menu.c"
#include "sdl/ui/sdl-main-menu.c"
#include "sdl/ui/sdl-halls-screen.c"

bool __wrap_sdl_touch_only_device_active(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }

static void capture(cptr name)
{
    char path[160];
    strnfmt(path,sizeof(path),"%s-%dx%d-%s.png",name,fixture_width,
        fixture_height,config.bigger_font?"on":"off");
    SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(pixels!=NULL);
    fixture_assert(IMG_SavePNG(pixels,path));
    SDL_DestroySurface(pixels);
}

static bool capture_actual_general;
static int actual_general_frames;
static void inside(SDL_FRect r);
bool __real_sdl_character_sheet_screen_commit_select(int choice);
bool __wrap_sdl_character_sheet_screen_commit_select(int choice)
{
    bool result=__real_sdl_character_sheet_screen_commit_select(choice);
    if(capture_actual_general && result) {
        fixture_assert(!strcmp(g_sdl_character_sheet_screen.select_title,"General Settings"));
        fixture_assert(g_sdl_character_sheet_screen.select_row_count>8);
        fixture_assert(g_sdl_character_sheet_screen.select_description[0]);
        fixture_assert(sdl_render_current_window_frame());
        capture("actual-general-settings");
        const sdl_character_sheet_select_row *first=&g_sdl_character_sheet_screen.select_rows[0];
        fixture_assert(first->choice==0 && strstr(first->label,"On"));
        fixture_assert(first->reset_choice>=0);
        bool row_visible=false,reset_visible=false;
        float row_h=0,footer_top=fixture_height;
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
            if(hit->choice==first->choice) {
                inside(hit->rect); row_visible=true; row_h=hit->rect.h;
            }
            if(hit->choice==first->reset_choice) {
                inside(hit->rect); reset_visible=true;
            }
            if(hit->choice==-1 || hit->choice==-2) footer_top=MIN(footer_top,hit->rect.y);
        }
        fixture_assert(row_visible && reset_visible && row_h>0);
        SDL_FRect scroll_rect=g_sdl_character_sheet_screen.select_scroll_rect;
        inside(scroll_rect);
        fixture_assert(scroll_rect.y+scroll_rect.h<=footer_top+1);
        TTF_Font *desc_font=sdl_story_font_for_height_slot(
            g_sdl_character_sheet_screen.last_desc_px,SDL_STORY_FONT_SLOT_MENU);
        int lines=sdl_char_sheet_wrap_text(desc_font,
            g_sdl_character_sheet_screen.select_description,scroll_rect.w,NULL,0);
        float line_h=g_sdl_character_sheet_screen.last_desc_line_h;
        fixture_assert(lines>0 && line_h>0);
        float desc_h=lines*line_h;
        float help_end=row_h+line_h*.3f+desc_h;
        int target=MAX(0,(int)ceilf(help_end-scroll_rect.h));
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll_max>=target);
        g_sdl_character_sheet_screen.sheet_scroll=target;
        fixture_assert(sdl_render_current_window_frame());
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll==target);
        fixture_assert(help_end-target<=scroll_rect.h+1);
        capture("actual-general-help-end");
        /* Pixel evidence catches an evicted footer font even when its
         * button and hit rectangle are still present. Ignore the border. */
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            const sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
            if(hit->choice!=-1 && hit->choice!=-2) continue;
            SDL_Rect area={(int)hit->rect.x+6,(int)hit->rect.y+6,
                (int)hit->rect.w-12,(int)hit->rect.h-12};
            SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&area);
            fixture_assert(pixels!=NULL);
            int ink=0;
            for(int y=0;y<pixels->h;y++) for(int x=0;x<pixels->w;x++) {
                Uint8 r=0,g=0,b=0,a=0;
                fixture_assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
                if(a>0 && r<50 && g<50 && b<50) ink++;
            }
            SDL_DestroySurface(pixels);
            if(ink<=8) fprintf(stderr,"actual General footer choice%d has only%d ink pixels\n",hit->choice,ink);
            fixture_assert(ink>8);
        }
        actual_general_frames++;
        fixture_assert(Term_keypress(ESCAPE)==0);
    }
    return result;
}

static void check_actual_general(void)
{
    SDL_strlcpy(fixture_id,"actual-general",sizeof(fixture_id));
    int before=actual_general_frames;
    capture_actual_general=true;
    do_cmd_pane_settings();
    capture_actual_general=false;
    fixture_assert(actual_general_frames==before+1);
    fixture_assert(!sdl_character_sheet_screen_active());
    fixture_assert(config.bigger_font);
}

static void inside(SDL_FRect r)
{
    if(r.x<-1 || r.y<-1 || r.x+r.w>fixture_width+1 || r.y+r.h>fixture_height+1)
        fprintf(stderr,"rect %.1f %.1f %.1f %.1f outside %dx%d\n",r.x,r.y,r.w,r.h,
            fixture_width,fixture_height);
    fixture_assert(r.w>0 && r.h>0);
    fixture_assert(r.x>=-1 && r.y>=-1);
    fixture_assert(r.x+r.w<=fixture_width+1);
    fixture_assert(r.y+r.h<=fixture_height+1);
}

static void tap(SDL_FRect r)
{
    SDL_Event ev={0};
    ev.tfinger.timestamp=SDL_GetTicksNS();
    ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=(r.x+r.w/2)/fixture_width;
    ev.tfinger.y=(r.y+r.h/2)/fixture_height;
    ev.type=SDL_EVENT_FINGER_DOWN; sdl_handle_event(&g_state,&ev);
    ev.type=SDL_EVENT_FINGER_UP; sdl_handle_event(&g_state,&ev);
}

static void check_config(void)
{
    struct sdl_config original={0}, loaded={0};
    struct sdl_config_load_info info={0};
    sdl_config_set_defaults(&original);
    fixture_assert(!original.bigger_font);
    original.bigger_font=true;
    fixture_assert(sdl_config_save("roundtrip.json",&original,NULL,0));
    sdl_config_set_defaults(&loaded);
    sdl_config_load("roundtrip.json",&loaded,NULL,0,&info);
    fixture_assert(loaded.bigger_font);
    original.bigger_font=false;
    fixture_assert(sdl_config_save("roundtrip.json",&original,NULL,0));
    sdl_config_set_defaults(&loaded);
    sdl_config_load("roundtrip.json",&loaded,NULL,0,&info);
    fixture_assert(!loaded.bigger_font);
    FILE *old=fopen("old-config.json","wb");
    fixture_assert(old!=NULL);
    fputs("{\"sdl\":{\"auxViewFontSize\":24}}",old); fclose(old);
    sdl_config_set_defaults(&loaded);
    sdl_config_load("old-config.json",&loaded,NULL,0,&info);
    fixture_assert(!loaded.bigger_font);
    config.bigger_font=false; fixture_assert(sdl_ui_font_px(19)==19);
    config.bigger_font=true; fixture_assert(sdl_ui_font_px(19)==29);
    config.bigger_font=false;
    puts("Config: default Off, old config Off, On/Off roundtrip, odd pixel scaling: PASS");
}

static int check_settings(int rows, cptr title)
{
    SDL_strlcpy(fixture_id,title,sizeof(fixture_id));
    static const char *labels[]={"Bigger font\tOn", "Minimum Terminal Size\tAdaptive",
        "Main Terminal Scale\t1", "Terminal Menu Scale Offset\t+0",
        "Compact Inventory Menus\tyes", "Character Sheet Mode\tSDL",
        "View Pane Configuration", "Reset Settings to Defaults"};
    ui_menu_click_begin(); ui_menu_click_set_hover_enabled(true);
    sdl_character_sheet_screen_begin_select(1,title);
    sdl_character_sheet_screen_set_select_menu_style(true);
    for(int i=0;i<rows;i++) {
        sdl_character_sheet_screen_add_select_row(i+1,labels[i%8],TERM_WHITE,"");
        if(i<rows-1) sdl_character_sheet_screen_set_last_select_row_reset(1001+i);
    }
    fixture_assert(sdl_character_sheet_screen_commit_select(1));
    fixture_assert(sdl_render_current_window_frame());
    int px=g_sdl_character_sheet_screen.last_body_px;
    fixture_assert(px>0);
    capture(title);
    if(config.bigger_font) {
        TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_MENU);
        int w=0,h=0,buttons=0;
        fixture_assert(TTF_GetStringSize(font,"Reset",0,&w,&h));
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
            if(hit->choice!=1001) continue;
            buttons++;
            fixture_assert(w<=hit->rect.w*0.84f+1 && h<=hit->rect.h-8+1);
        }
        fixture_assert(buttons>0);
    }
    fixture_assert(sdl_character_sheet_screen_commit_select(rows));
    fixture_assert(sdl_render_current_window_frame());
    bool found=false;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *h=&g_sdl_character_sheet_screen.hits[i];
        if(h->choice!=rows) continue;
        inside(h->rect); found=true; tap(h->rect);
    }
    fixture_assert(found);
    int actual=0,action=0;
    fixture_assert(ui_menu_click_take_action(&actual,&action));
    fixture_assert(actual==rows && action==UI_MENU_CLICK_PRIMARY);
    char key=0;
    fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
    if(config.bigger_font && rows==40)
        fixture_assert(g_sdl_character_sheet_screen.sheet_scroll_max>0);
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    return px;
}

static void check_songs(void)
{
    SDL_strlcpy(fixture_id,"songs",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_song_menu_begin("Songs of Power");
    for(int i=0;i<SDL_SONG_MENU_MAX_ENTRIES;i++)
        sdl_song_menu_add_entry(i,"a)","Song of the Trees and Stars",TERM_WHITE);
    sdl_song_menu_set_highlight(0); sdl_song_menu_finish();
    sdl_song_menu_layout_info layout;
    fixture_assert(sdl_song_menu_layout(&layout)); inside(layout.panel);
    fixture_assert(layout.visible_count>0);
    int guard=0;
    while(layout.first_entry+layout.visible_count<g_song_menu.count) {
        inside(layout.next);
        fixture_assert(sdl_song_menu_handle_pointer(layout.next.x+layout.next.w/2,
            layout.next.y+layout.next.h/2,UI_MENU_CLICK_PRIMARY));
        fixture_assert(sdl_song_menu_layout(&layout));
        fixture_assert(++guard<=SDL_SONG_MENU_MAX_ENTRIES);
    }
    inside(layout.rows[SDL_SONG_MENU_MAX_ENTRIES-1]);
    SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255); SDL_RenderClear(g_state.renderer);
    sdl_song_menu_render(); capture("songs-last-page");
    fixture_assert(!ui_menu_click_has_pending());
    SDL_FRect last=layout.rows[SDL_SONG_MENU_MAX_ENTRIES-1];
    fixture_assert(sdl_song_menu_handle_pointer(last.x+last.w/2,last.y+last.h/2,
        UI_MENU_CLICK_PRIMARY));
    int actual=0,action=0; char key=0;
    fixture_assert(ui_menu_click_take_action(&actual,&action));
    fixture_assert(actual==SDL_SONG_MENU_MAX_ENTRIES-1 && action==UI_MENU_CLICK_PRIMARY);
    fixture_assert(Term_inkey(&key,false,true)==0);
    sdl_song_menu_clear(); ui_menu_click_clear();
}

static void check_questions(void)
{
    SDL_strlcpy(fixture_id,"questions",sizeof(fixture_id));
    ui_menu_click_begin(); sdl_question_menu_begin("Choose a Weapon");
    for(int i=0;i<18;i++)
        sdl_question_menu_add_entry(i,"a)",
            "Main hand\tLongsword of the Northern Mountains\t(+2, 3d7)\t4.0 lb",TERM_WHITE);
    sdl_question_menu_add_button(101,"Keep the selected weapon",TERM_WHITE);
    sdl_question_menu_add_button(102,"Compare with the alternate weapon",TERM_WHITE);
    sdl_question_menu_add_button(103,"Return to the inventory",TERM_WHITE);
    sdl_question_menu_finish();
    sdl_question_menu_layout_info layout;
    fixture_assert(sdl_question_menu_layout(&layout)); inside(layout.panel);
    SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255); SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render(); capture("question");
    if(layout.visible_count<=0) {
        SDL_Rect anchor={0}; sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION,&anchor);
        fprintf(stderr,"question font%d visible%d first%d panel%.1f,%.1f,%.1f,%.1f entries%.1f,%.1f,%.1f,%.1f anchor%d,%d,%d,%d titleh%.1f\n",
            layout.font_px,layout.visible_count,layout.first_entry,layout.panel.x,layout.panel.y,
            layout.panel.w,layout.panel.h,layout.entries_rect.x,layout.entries_rect.y,
            layout.entries_rect.w,layout.entries_rect.h,anchor.x,anchor.y,anchor.w,anchor.h,layout.title_row.h);
        for(int i=0;i<layout.button_count;i++) fprintf(stderr,"button%d %.1f %.1f %.1f %.1f\n",
            i,layout.buttons[i].x,layout.buttons[i].y,layout.buttons[i].w,layout.buttons[i].h);
    }
    fixture_assert(layout.has_table && layout.visible_count>0);
    if(fixture_width<fixture_height)
        fixture_assert(layout.compact_table && layout.table_column_count==1);
    fixture_assert(layout.button_count==3);
    TTF_Font *font=sdl_story_font_for_height_slot(layout.font_px,SDL_STORY_FONT_SLOT_MENU);
    for(int i=0;i<layout.button_count;i++) {
        if(!layout.actions_in_list) inside(layout.buttons[i]);
        int text_w=0,text_h=0;
        fixture_assert(TTF_GetStringSizeWrapped(font,g_question_menu.buttons[i].text,0,
            MAX(1,(int)(layout.buttons[i].w*0.84f)),&text_w,&text_h));
        if(text_h>layout.buttons[i].h+1)
            fprintf(stderr,"question button %d: text h %d exceeds button %.1f, width %.1f font %d\n",
                i,text_h,layout.buttons[i].h,layout.buttons[i].w,layout.font_px);
        fixture_assert(text_h<=layout.buttons[i].h+1);
    }
    for(int i=0;i<layout.button_count;i++) for(int j=i+1;j<layout.button_count;j++) {
        SDL_FRect overlap;
        fixture_assert(!SDL_GetRectIntersectionFloat(&layout.buttons[i],&layout.buttons[j],&overlap));
    }
    sdl_question_menu_set_highlight(17);
    fixture_assert(sdl_question_menu_layout(&layout));
    SDL_FRect visible;
    fixture_assert(SDL_GetRectIntersectionFloat(&layout.rows[17],&layout.entries_rect,&visible));
    inside(visible);
    fixture_assert(layout.rows[17].y>=layout.entries_rect.y-1);
    fixture_assert(g_question_menu.scroll_offset_ptr!=NULL);
    int row_delta=(int)ceilf(layout.rows[17].y+layout.rows[17].h-
        layout.entries_rect.y-layout.entries_rect.h);
    if(row_delta>0)
        fixture_assert(sdl_question_menu_scroll_offset_by(&layout,row_delta));
    fixture_assert(sdl_question_menu_layout(&layout));
    fixture_assert(layout.rows[17].y+layout.rows[17].h<=
        layout.entries_rect.y+layout.entries_rect.h+1);
    if(layout.actions_in_list) {
        inside(layout.actions_link); tap(layout.actions_link);
        fixture_assert(!ui_menu_click_has_pending());
        fixture_assert(sdl_question_menu_layout(&layout));
        fixture_assert(layout.buttons[0].y<=layout.entries_rect.y+1);
        for(int i=0;i<3;i++) {
            int delta=(int)ceilf(layout.buttons[i].y-layout.entries_rect.y);
            if(delta) (void)sdl_question_menu_scroll_offset_by(&layout,delta);
            fixture_assert(sdl_question_menu_layout(&layout));
            fixture_assert(SDL_GetRectIntersectionFloat(&layout.buttons[i],&layout.entries_rect,&visible));
            inside(visible); tap(visible);
            int actual=0,action=0; char key=0;
            fixture_assert(ui_menu_click_take_action(&actual,&action));
            fixture_assert(actual==101+i && action==UI_MENU_CLICK_PRIMARY);
            fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
        }
        SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255); SDL_RenderClear(g_state.renderer);
        sdl_question_menu_render(); capture("question-actions");
    }
    sdl_question_menu_clear(); ui_menu_click_clear();
}

static void check_character_allocation(void)
{
    SDL_strlcpy(fixture_id,"character-skills",sizeof(fixture_id));
    int base[S_MAX]={0}, gain[S_MAX]={0}, costs[S_MAX]={0};
    character_generated=character_dungeon=false;
    ui_menu_click_begin();
    sdl_character_sheet_screen_show_birth_skills(base,gain,costs,S_MEL,5000);
    fixture_assert(sdl_render_current_window_frame());
    fixture_assert(g_sdl_character_sheet_screen.last_body_px>0);
    capture("character-skills");
    fixture_assert(g_sdl_character_sheet_screen.hit_count>0);
    sdl_character_sheet_screen_show_birth_skills(base,gain,costs,S_SNG,5000);
    fixture_assert(sdl_render_current_window_frame());
    bool final_skill=false;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *h=&g_sdl_character_sheet_screen.hits[i];
        if(h->choice==S_SNG) { inside(h->rect); final_skill=true; }
    }
    fixture_assert(final_skill);
    capture("character-skills-last");
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
    character_generated=character_dungeon=true;
}

static void check_text_pages(void)
{
    SDL_strlcpy(fixture_id,"text-pages",sizeof(fixture_id));
    for(int poetry=0;poetry<2;poetry++) {
        if(poetry) {
            char body[8192]="";
            for(int i=0;i<45;i++) SDL_strlcat(body,
                "The stars above the silent forest shine beyond the mountain.\n",sizeof(body));
            sdl_poetry_screen_begin("The Long Journey",body,"","Continue");
            sdl_poetry_screen_update(true,TERM_L_BLUE,true,TERM_WHITE,false,TERM_WHITE,true);
        } else {
            fixture_assert(sdl_pause_text_screen_begin());
            for(int i=0;i<45;i++) sdl_pause_text_screen_add_line(
                "The stars above the silent forest shine beyond the mountain.",TERM_WHITE,0);
            sdl_pause_text_screen_set_visible_lines(45);
        }
        fixture_assert(sdl_render_current_window_frame());
        fixture_assert(g_sdl_standalone_pager.maximum>0);
        inside(g_sdl_standalone_pager.buttons[0]);
        inside(g_sdl_standalone_pager.buttons[1]);
        int guard=0;
        while(g_sdl_standalone_pager.offset<g_sdl_standalone_pager.maximum) {
            SDL_FRect r=g_sdl_standalone_pager.buttons[1];
            fixture_assert(sdl_standalone_screen_handle_pointer(r.x+r.w/2,r.y+r.h/2,
                UI_MENU_CLICK_PRIMARY));
            fixture_assert(sdl_render_current_window_frame());
            fixture_assert(++guard<500);
        }
        capture(poetry?"poetry-last-page":"pause-last-page");
        if(poetry) sdl_poetry_screen_hide(); else sdl_pause_text_screen_hide();
    }
}

static void check_main_menu(void)
{
    SDL_strlcpy(fixture_id,"main-menu",sizeof(fixture_id));
    character_icky=0; g_main_menu_overlay_active=true;
    for(int choice=1;choice<=MAIN_MENU_MAX;choice++) {
        g_main_menu_overlay_highlight=choice;
        main_menu_pane_layout layout;
        fixture_assert(sdl_main_menu_overlay_layout(&layout));
        inside(layout.panel); inside(layout.rows[choice]);
        fixture_assert(choice>=layout.first_choice);
        fixture_assert(choice<layout.first_choice+layout.visible_count);
    }
    SDL_SetRenderDrawColor(g_state.renderer,20,24,28,255); SDL_RenderClear(g_state.renderer);
    sdl_main_menu_pane_render(); capture("main-menu-last");
    main_menu_pane_layout layout;
    g_main_menu_overlay_highlight=1;
    g_main_menu_overlay_first_choice=1;
    fixture_assert(sdl_main_menu_overlay_layout(&layout));
    int first_before=layout.first_choice;
    int visible_before=layout.visible_count;
    SDL_Event ev={0}; ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1; ev.tfinger.fingerID=7;
    ev.tfinger.x=(layout.panel.x+layout.panel.w/2)/fixture_width;
    ev.tfinger.y=(layout.panel.y+layout.panel.h*.75f)/fixture_height;
    ev.type=SDL_EVENT_FINGER_DOWN;
    fixture_assert(sdl_main_menu_overlay_handle_event(&ev));
    ev.tfinger.y-=MIN(200.0f,layout.panel.h*.5f)/fixture_height;
    ev.type=SDL_EVENT_FINGER_MOTION;
    fixture_assert(sdl_main_menu_overlay_handle_event(&ev));
    ev.type=SDL_EVENT_FINGER_UP;
    fixture_assert(sdl_main_menu_overlay_handle_event(&ev));
    fixture_assert(g_main_menu_large_touch_dragged);
    fixture_assert(sdl_main_menu_overlay_layout(&layout));
    if(visible_before<MAIN_MENU_MAX)
        fixture_assert(layout.first_choice>first_before);
    fixture_assert(g_main_menu_overlay_active);
    char key=0; fixture_assert(Term_inkey(&key,false,true)!=0);
    sdl_main_menu_overlay_close(); character_icky=1;
}

static void check_halls(void)
{
    SDL_strlcpy(fixture_id,"halls",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_halls_screen_begin("Heroes of the Long Journey","1 / 2",true,-1);
    fixture_assert(sdl_halls_screen_page_capacity(true)==1);
    sdl_halls_screen_add_entry(1,"1","Aranwe of the Northern Forests","12345",
        "Escaped from the depths beneath the mountain after a long journey.",
        "A courageous adventurer who carried the light of the stars through the shadowed passages of Angband.",
        "Slayer of Dragons",TERM_L_BLUE,
        "Skill, treasures, discoveries, hard won victories, and a successful escape increased this hero's score.",
        "Many wounds and lost treasures reduced this hero's final score.",TERM_WHITE,true);
    sdl_halls_screen_add_action(2,"Next page",TERM_WHITE,true);
    sdl_halls_screen_add_action(3,"Previous page",TERM_WHITE,true);
    sdl_halls_screen_add_action(4,"Return",TERM_WHITE,true);
    fixture_assert(sdl_render_current_window_frame());
    fixture_assert(g_halls_text_pager.maximum>=0);
    inside(g_sdl_halls.entries[0].hit_rect);
    for(int i=0;i<g_sdl_halls.action_count;i++) inside(g_sdl_halls.actions[i].hit_rect);
    if(g_halls_text_pager.maximum>0) {
        inside(g_halls_text_pager.buttons[0]); inside(g_halls_text_pager.buttons[1]);
    }
    int guard=0;
    while(g_halls_text_pager.offset<g_halls_text_pager.maximum) {
        SDL_FRect r=g_halls_text_pager.buttons[1];
        fixture_assert(sdl_halls_pointer_press(r.x+r.w/2,r.y+r.h/2,UI_MENU_CLICK_PRIMARY));
        fixture_assert(sdl_render_current_window_frame());
        fixture_assert(++guard<500);
    }
    capture("halls-last-page");
    fixture_assert(!ui_menu_click_has_pending());
    sdl_halls_screen_hide(); ui_menu_click_clear();
}

static void check_live_sheet(void)
{
    SDL_strlcpy(fixture_id,"live-sheet",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_live(-1);
    for(int skill=0;skill<S_MAX;skill++) if(skill!=S_SPC)
        sdl_character_sheet_screen_add_live_item(skill,0,skill,0,
            skill_names_full[skill],character_sheet_skill_description(skill));
    fixture_assert(sdl_render_current_window_frame());
    capture("character-live-overview");
    static const int actions[]={'i','x','s','?',ESCAPE};
    static const cptr labels[]={"Skills","Powers","Story","Help","Back"};
    TTF_Font *font=sdl_main_menu_mono_font_for_height(g_sdl_character_sheet_screen.last_body_px);
    for(int a=0;a<5;a++) {
        bool found=false;
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
            if(hit->choice!=actions[a]) continue;
            found=true; inside(hit->rect);
            int w=0,h=0;
            fixture_assert(TTF_GetStringSize(font,labels[a],0,&w,&h));
            fixture_assert(w<=hit->rect.w+1 && h<=hit->rect.h+1);
        }
        fixture_assert(found);
    }
    for(int page=0;page<g_sdl_character_sheet_screen.debug_page_count;page++)
        if(g_debug_sheet_page_section[page]==2) {
            g_sdl_character_sheet_screen.debug_page=page;
            fixture_assert(sdl_render_current_window_frame()); capture("character-live-skills");
            float actions_top=fixture_height;
            for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
                sdl_character_sheet_hit *h=&g_sdl_character_sheet_screen.hits[i];
                if(h->choice=='i' || h->choice=='x' || h->choice=='s' || h->choice=='?')
                    actions_top=MIN(actions_top,h->rect.y);
            }
            bool skill_hit=false;
            for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
                sdl_character_sheet_hit *h=&g_sdl_character_sheet_screen.hits[i];
                if(h->choice<0 || h->choice>=S_MAX) continue;
                skill_hit=true;
                fixture_assert(h->rect.y+h->rect.h<=actions_top+1);
            }
            fixture_assert(skill_hit);
            break;
        }
    sdl_character_sheet_screen_hide(); ui_menu_click_clear();
}

static void check_large_tutorial(void)
{
    SDL_strlcpy(fixture_id,"large-tutorial",sizeof(fixture_id));
    int saved_font=config.aux_view_font_size;
    for(int size=16;size<=48;size+=32) {
        SDL_zero(fixture);
        fixture.active=true; fixture.can_continue=true;
        fixture.kind=TUTORIAL_STEP_INFO; fixture.step=1; fixture.step_count=3;
        fixture.revision=(unsigned int)size;
        SDL_strlcpy(fixture.id,"bigger-font-reading",sizeof(fixture.id));
        SDL_strlcpy(fixture.title,"Read the complete instructions",sizeof(fixture.title));
        for(int i=0;i<40;i++) SDL_strlcat(fixture.body,
            "Read each instruction carefully before choosing your next action. ",sizeof(fixture.body));
        config.aux_view_font_size=size;
        tutorial_seen_revision=fixture.revision; tutorial_was_active=true;
        tutorial_reading=false; tutorial_scroll=0;
        for(int last=0;last<2;last++) {
            drawn_count=0; drawn_content[0]='\0';
            if(last) tutorial_scroll=tutorial_max_scroll;
            sdl_gameplay_tutorial_render();
            inside(tutorial_card);
            int body_rows=0;
            for(int b=0;b<3;b++) inside(tutorial_buttons[b]);
            for(int i=0;i<drawn_count;i++) {
                inside(drawn_text[i]);
                if(drawn_button[i]) continue;
                body_rows++;
                for(int b=0;b<3;b++) {
                    if(SDL_HasRectIntersectionFloat(&drawn_text[i],&tutorial_buttons[b]))
                        printf("Tutorial font%d last%d text%d=(%.1f,%.1f,%.1f,%.1f) button%d=(%.1f,%.1f,%.1f,%.1f)\n",
                            size,last,i,drawn_text[i].x,drawn_text[i].y,drawn_text[i].w,drawn_text[i].h,
                            b,tutorial_buttons[b].x,tutorial_buttons[b].y,tutorial_buttons[b].w,tutorial_buttons[b].h);
                    fixture_assert(!SDL_HasRectIntersectionFloat(&drawn_text[i],&tutorial_buttons[b]));
                }
            }
            fixture_assert(body_rows>0);
            fixture_assert(tutorial_max_scroll>=0);
            if(fixture_width<1080 && fixture_height<1600)
                fixture_assert(tutorial_max_scroll>0);
        }
        capture(size==16?"tutorial-small-font":"tutorial-large-font");
    }
    fixture.active=false; config.aux_view_font_size=saved_font;
    sdl_gameplay_tutorial_sync();
}

static void check_welcome_cache(void)
{
    SDL_strlcpy(fixture_id,"welcome-cache",sizeof(fixture_id));
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    sdl_welcome_layout_line lines[SDL_WELCOME_MAX_LINES];
    sdl_welcome_layout_metrics metrics={0};
    fixture_assert(sdl_welcome_screen_show_intro(INTRO_STYLE_FLAME,false));
    fixture_assert(sdl_welcome_prepare_layout(&canvas,lines,N_ELEMENTS(lines),&metrics)>0);
    fixture_assert(sdl_welcome_layout_cache_matches(&canvas));
    config.bigger_font=false;
    fixture_assert(!sdl_welcome_layout_cache_matches(&canvas));
    config.bigger_font=true;
    sdl_story_font_cache_clear();
    fixture_assert(!sdl_welcome_layout_cache_matches(&canvas));
    fixture_assert(sdl_welcome_prepare_layout(&canvas,lines,N_ELEMENTS(lines),&metrics)>0);
    /* Evict the welcome fonts, then revisit without restarting the process. */
    for(int i=0;i<MAX_STORY_FONT_CACHE+1;i++)
        fixture_assert(sdl_story_font_for_height_slot(100+i,SDL_STORY_FONT_SLOT_MENU));
    fixture_assert(!sdl_welcome_layout_cache_matches(&canvas));
    int count=sdl_welcome_prepare_layout(&canvas,lines,N_ELEMENTS(lines),&metrics);
    fixture_assert(count>0);
    for(int i=0;i<count;i++) {
        bool alive=false;
        for(int f=0;f<g_state.story_font_count;f++)
            if(lines[i].font==g_state.story_fonts[f].font) alive=true;
        fixture_assert(alive);
    }
    sdl_welcome_render_intro_canvas(&canvas);
    SDL_SetRenderTarget(g_state.renderer,NULL);
    capture("welcome-revisited");
    sdl_welcome_screen_hide();
}

static void check_actual_cell_metrics(void)
{
    SDL_strlcpy(fixture_id,"actual-cell-metrics",sizeof(fixture_id));
    int widths[PANE_MAX],heights[PANE_MAX];
    SDL_Rect screen={0,0,fixture_width,fixture_height};
    for(int bigger=0;bigger<2;bigger++) {
        config.bigger_font=bigger;
        sdl_build_supporting_pane_metrics(NULL,0,widths,heights);
        sdl_view actual={0};
        fixture_assert(sdl_view_create(&actual,screen,config.monospace_font,0,
            sdl_main_view_layout_scale(),0));
        fixture_assert(widths[PANE_MAIN]==actual.cell_w);
        fixture_assert(heights[PANE_MAIN]==actual.cell_h);
        sdl_view_destroy(&actual);
        SDL_SetRenderTarget(g_state.renderer,NULL);
        for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
            int scale=sdl_max_scale_for_rect_mode(&screen,mode);
            fixture_assert(sdl_view_create(&actual,screen,config.monospace_font,0,scale,0));
            /* Tiny screens can fit fewer cells even at the minimum scale. */
            if(scale>SDL_MAIN_VIEW_MIN_SCALE) {
                fixture_assert(actual.cols>=sdl_min_terminal_cols_for_mode(mode));
                fixture_assert(actual.rows>=sdl_min_terminal_rows_for_mode(mode));
            }
            sdl_view_destroy(&actual);
            SDL_SetRenderTarget(g_state.renderer,NULL);
        }
    }
    config.bigger_font=true;
}

static void check_auto_font_metrics(void)
{
    SDL_strlcpy(fixture_id,"auto-font-density",sizeof(fixture_id));
    float saved_scale=g_state.system_scale;
    int saved_aux=config.aux_view_font_size;
    config.aux_view_font_size=0;
    static const float densities[]={1.0f,2.625f,3.0f};
    SDL_Rect screen={0,0,fixture_width,fixture_height};
    for(int density=0;density<N_ELEMENTS(densities);density++) {
        g_state.system_scale=densities[density];
        for(int bigger=0;bigger<2;bigger++) {
            config.bigger_font=bigger;
            for(int pane=PANE_MAIN+1;pane<PANE_MAX;pane++) {
                sdl_view actual={0};
                fixture_assert(sdl_view_create(&actual,screen,config.monospace_font,
                    sdl_effective_pane_font_size_for_type(pane),0,0));
                fixture_assert(actual.cell_h==sdl_effective_pane_cell_height_for_type(pane));
                sdl_view_destroy(&actual);
                SDL_SetRenderTarget(g_state.renderer,NULL);
            }
        }
    }
    config.bigger_font=true;
    config.aux_view_font_size=saved_aux;
    g_state.system_scale=saved_scale;
}

static void check(int width,int height)
{
    fixture_width=width; fixture_height=height;
    SDL_strlcpy(fixture_id,"bigger-font",sizeof(fixture_id));
    config.aux_view_font_size=24;
    g_state.window=SDL_CreateWindow("Bigger font fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    sdl_view *view=&g_views[PANE_MAIN];
    term_init(&view->t,80,24,256); Term_activate(&view->t); term_screen=&view->t;
    character_icky=1; fixture.active=false;
    check_actual_cell_metrics();
    check_auto_font_metrics();
    config.bigger_font=false;
    int pane=sdl_main_menu_pane_font_px();
    int normal=check_settings(8,"General Settings");
    config.bigger_font=true;
    fixture_assert(sdl_main_menu_pane_font_px()==(pane*3+1)/2);
    int large=check_settings(8,"General Settings");
    fixture_assert(large==(normal*3+1)/2);
    check_settings(40,"Overflow Settings");
    check_actual_general();
    check_songs(); check_questions(); check_character_allocation(); check_text_pages(); check_main_menu(); check_halls(); check_live_sheet(); check_large_tutorial(); check_welcome_cache();
    printf("%dx%d: pane %d -> %d, native settings %d -> %d; settings taps, song paging/choice, question wrapping/scroll, birth final skill, text paging, main menu drag, Halls: PASS\n",
        width,height,pane,sdl_main_menu_pane_font_px(),normal,large);
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    term_nuke(&view->t); term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}

int main(int argc,char **argv)
{
    maxima limits={0}; player_type player={0};
    player_other options={0};
    object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==6); setbuf(stdout,NULL); log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); fixture_assert(TTF_Init());
    check_config(); sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM; config.use_unsafe_area=true;
    g_state.system_scale=1; z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    player.playing=true; character_generated=character_dungeon=true;
    player.song1=player.song2=SNG_NOTHING;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_SLATE]=(SDL_Color){150,150,150,255};
    g_state.palette[TERM_WHITE]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,210,220,255};
    check(atoi(argv[2]),atoi(argv[3]));
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-screens.c.obj", "/src/sdl/ui/sdl-song-menu.c.obj",
                "/src/sdl/ui/sdl-question-menu.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj",
                "/src/sdl/ui/sdl-halls-screen.c.obj")
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
             "sdl_character_sheet_screen_commit_select"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    # Each offscreen renderer gets a fresh process: some production font
    # caches live for the application lifetime and retain font pointers.
    for width, height in [(360, 800), (800, 360), (580, 1280), (1280, 720),
                          (1080, 2400), (2400, 1080)]:
        subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(width), str(height), str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
