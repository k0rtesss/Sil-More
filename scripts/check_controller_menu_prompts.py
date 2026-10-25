#!/usr/bin/env python3
"""Render production menus at Deck sizes with isolated fonts and controller UI.

Requires build-incremental.ps1. No user configuration or saves are loaded.
Checks complete text rasters, footer/choice separation, long memorial scrolling,
physical prompt semantics, and keyboard/nonblocking suppression. PNGs go under
scripts/output/controller-menu-prompts. --baseline-halls restores the unfitted
metadata as a negative control and must fail on the clipped memorial text.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/controller-menu-prompts"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include <stdlib.h>
#include "sdl/main-sdl-private.h"
#include "sdl/ui/sdl-halls-screen.c"
#include "sdl/ui/sdl-question-menu.c"
#include "sdl/ui/sdl-song-menu.c"
#include "sdl/ui/sdl-hint-quest-menu.c"
#include "sdl/ui/sdl-screens.c"
#include "sdl/ui/sdl-pane-menus.c"

static int width, height, cropped, paints, expected_choice;
static bool check_complete, playstation, oversized_halls_canvas;
static int nonmenu_keys;
static SDL_Texture* halls_title_texture;
static bool halls_title_seen;
void fixture_hint_buttons(void);
void fixture_check_settings(void);
char __wrap_inkey(void) { return ESCAPE; }
bool __wrap_sdl_overlay_pane_anchor_rect(enum pane_type pane, SDL_Rect* out)
{ *out = (SDL_Rect){0,0,width,height}; return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,width*(oversized_halls_canvas?2:1),height}; }
SDL_Rect __real_sdl_get_window_pixel_rect(void);
SDL_Rect __wrap_sdl_get_window_pixel_rect(void)
{
    return oversized_halls_canvas ? (SDL_Rect){0,0,width*2,height}
        : __real_sdl_get_window_pixel_rect();
}
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return 1.0f; }
SDL_GamepadType __wrap_sdl_gamepad_prompt_type(void)
{ return playstation ? SDL_GAMEPAD_TYPE_PS5 : SDL_GAMEPAD_TYPE_XBOXONE; }
bool __wrap_ui_menu_click_handle_choice_action(int choice, int action, bool* wake)
{ expected_choice = choice; if (wake) *wake = false; return true; }
errr __wrap_Term_keypress(int key) { if(key!=UI_MENU_CLICK_WAKE_KEY) nonmenu_keys++; return 0; }
bool __wrap_save_pane_config_to_json(void) { return true; }
void __wrap_sdl_apply_config(void) {}
SDL_Texture* __real_sdl_ui_text_texture(TTF_Font*,cptr,SDL_Color,int*,int*);
SDL_Texture* __wrap_sdl_ui_text_texture(TTF_Font* font,cptr text,SDL_Color color,int* w,int* h)
{
    SDL_Texture* texture=__real_sdl_ui_text_texture(font,text,color,w,h);
    if(strstr(text,"H A L L S") || !strcmp(text,"Halls of Mandos")) halls_title_texture=texture;
    return texture;
}
SDL_Texture* __real_sdl_ui_wrapped_text_texture(TTF_Font*,cptr,int,SDL_Color,int*,int*);
SDL_Texture* __wrap_sdl_ui_wrapped_text_texture(TTF_Font* font,cptr text,int width,SDL_Color color,int* w,int* h)
{
    SDL_Texture* texture=__real_sdl_ui_wrapped_text_texture(font,text,width,color,w,h);
    if(!strcmp(text,"Halls of Mandos")) halls_title_texture=texture;
    return texture;
}
void __real_sdl_ui_render_text_texture(SDL_Texture*,const SDL_FRect*,const SDL_FRect*);
void __wrap_sdl_ui_render_text_texture(SDL_Texture* texture,
    const SDL_FRect* src, const SDL_FRect* dst)
{
    float w=0,h=0;
    assert(SDL_GetTextureSize(texture,&w,&h));
    if(check_complete) {
        if(texture==halls_title_texture) {
            assert(SDL_fabsf(dst->x+dst->w*.5f-width*.5f)<=1);
            halls_title_seen=true;halls_title_texture=NULL;
        }
        paints++;
        if ((src && (src->w+1<w || src->h+1<h)) || dst->w+1<w || dst->h+1<h) {
            fprintf(stderr,"Cropped text raster %.0fx%.0f to %.0fx%.0f\n",w,h,dst->w,dst->h);
            cropped++;
        }
    }
    __real_sdl_ui_render_text_texture(texture,src,dst);
}
static bool overlap(SDL_FRect a,SDL_FRect b)
{ return a.w>0 && a.h>0 && b.w>0 && b.h>0 && a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y; }
static void inside(SDL_FRect r)
{ assert(r.w>0 && r.h>0 && r.x>=0 && r.y>=0 && r.x+r.w<=width+1 && r.y+r.h<=height+1); }
static void snapshot(cptr name)
{
    char path[160]; strnfmt(path,sizeof(path),"%dx%d-%s-%s-%s.png",width,height,
        config.bigger_font?"big":"normal",playstation?"ps":"deck",name);
    SDL_Surface* pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(pixels && IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
}
static void clear(void)
{ SDL_SetRenderClipRect(g_state.renderer,NULL);SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer); }
static void halls(bool detailed, bool long_text, int count)
{
    sdl_halls_screen_begin("Here are remembered the fates of those who entered Angband.",
        detailed?"Score (highest first) | full memorials | page 1 of 1"
                :"Score (highest first) | brief memorials | page 1 of 1",detailed,-3);
    for(int i=0;i<count;i++) {
        char name[64],rank[16];
        if(i) strnfmt(name,sizeof(name),"Maedhros the Red %d",i);
        else SDL_strlcpy(name,"Maedhros the Red",sizeof(name));
        strnfmt(rank,sizeof(rank),"#%d",i+1);
        sdl_halls_screen_add_entry(i,rank,name,"10 PTS",
            long_text?"Slain after a long pursuit through the vaults of Angband, beneath the ancient walls, surrounded by foes in the darkness of the deepest dungeon, while seeking the light of a Silmaril.":"Lives still, deep within Angband's vaults.",
            "154 turns | deepest descent 50 ft | 18 Oct 2026","Living",TERM_L_GREEN,
            long_text?"Score increases: exploration, treasure, valour, enemies defeated, discoveries in the depths, recovered artefacts, dangerous encounters, descent, and the long journey toward the throne.":"Score increases: descent +10 pts",
            "Score decreases: none | Formula: 10 base x 1.000 = 10 pts",TERM_WHITE,i==0);
    }
    sdl_halls_screen_add_action(-3,"<B> Back",TERM_WHITE,true);
    sdl_halls_screen_add_action(-7,"<View> Run History",TERM_L_BLUE,true);
    sdl_halls_screen_add_action(-1,"<Y> Order: Score",TERM_WHITE,true);
    sdl_halls_screen_add_action(-2,detailed?"<X> View: Full":"<X> View: Brief",TERM_WHITE,true);
    sdl_halls_screen_add_action(-4,"<A> Open Hero",TERM_L_BLUE,count>0);
    sdl_halls_screen_add_action(-5,"<LB> Previous",TERM_WHITE,count>1);
    sdl_halls_screen_add_action(-6,"<RB> Next",TERM_WHITE,count>1);
    check_complete=true; halls_title_seen=false;cropped=paints=0;
    sdl_halls_screen_render();
    assert(paints>8 && cropped==0 && halls_title_seen);
    assert(!sdl_halls_flow_layout());
    inside(g_halls_controller.body);
    /* Retain the centered memorial composition and its rank accent strip. */
    assert(g_halls_controller.body.w<width*.94f);
    assert(SDL_fabsf(g_halls_controller.body.x+g_halls_controller.body.w*.5f-width*.5f)<1);
    if(count) {
        SDL_Rect sample={(int)g_halls_controller.body.x+2,(int)g_halls_controller.body.y+10,1,1};
        SDL_Surface* pixel=SDL_RenderReadPixels(g_state.renderer,&sample);assert(pixel);
        Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(pixel,0,0,&r,&g,&b,&a));SDL_DestroySurface(pixel);
        assert(r==g_state.palette[TERM_WHITE].r && g==g_state.palette[TERM_WHITE].g && b==g_state.palette[TERM_WHITE].b);
    }
    for(int i=0;i<g_sdl_halls.action_count;i++) if(g_sdl_halls.actions[i].enabled) {
        SDL_FRect r=g_sdl_halls.actions[i].hit_rect;inside(r);
        assert(!overlap(r,g_halls_controller.body));
        assert(sdl_halls_hit_at(r.x+r.w/2,r.y+r.h/2)==g_sdl_halls.actions[i].choice);
        for(int j=0;j<i;j++) assert(!overlap(r,g_sdl_halls.actions[j].hit_rect));
        if(!config.bigger_font) assert(r.y==g_sdl_halls.actions[0].hit_rect.y);
    }
    if(count && !long_text) snapshot(oversized_halls_canvas
        ? (detailed?"halls-full-output-bounds":"halls-brief-output-bounds")
        : (detailed?"halls-full":"halls-brief"));
    float before=g_halls_controller.offset;
    while(sdl_halls_screen_handle_key(steamdeck_next_page_key())) {
        assert(g_halls_controller.offset>before);before=g_halls_controller.offset;
        sdl_halls_screen_render();assert(cropped==0);
    }
    assert(g_halls_controller.offset==g_halls_controller.maximum);
    assert(!sdl_halls_screen_handle_key(steamdeck_next_page_key()));
    if(count) {
        sdl_halls_entry* last=&g_sdl_halls.entries[count-1];
        SDL_Rect canvas={0,0,width,height};
        float full_h=sdl_halls_controller_card_h(&canvas,g_halls_controller.body.w,detailed,last);
        if(full_h<=g_halls_controller.body.h) inside(last->hit_rect);
    }
    while(sdl_halls_screen_handle_key(steamdeck_prev_page_key()))
        sdl_halls_screen_render();
    assert(g_halls_controller.offset==0);
    check_complete=false;sdl_halls_screen_hide();
}
static void questions(bool controller)
{
    config.input_ui_mode=controller?SDL_INPUT_UI_MODE_CONTROLLER:SDL_INPUT_UI_MODE_PLATFORM;
    sdl_question_menu_begin("Menu choices");
    int scroll=0;sdl_question_menu_set_scroll_offset_target(&scroll,true);
    sdl_question_menu_set_desc("Choose a menu entry. Long descriptions and populated lists must leave controller prompts visible.");
    for(int i=0;i<12;i++) sdl_question_menu_add_entry(i,"","A selectable menu entry with a longer wrapped explanation.",TERM_WHITE);
    sdl_question_menu_add_button(-2,"Back",TERM_WHITE);
    sdl_question_menu_add_button(-3,"An additional menu action",TERM_WHITE);
    sdl_question_menu_set_highlight(0);sdl_question_menu_finish();
    sdl_question_menu_layout_info l;assert(sdl_question_menu_layout(&l));
    if(controller) {
        if(l.max_scroll_offset>0) {
            SDL_GamepadAxisEvent axis={0};
            int keys_before=nonmenu_keys;
            g_gamepad_state.right_x=g_gamepad_state.right_y=0;
            g_gamepad_state.right_ui_dir=-1;
            axis.axis=SDL_GAMEPAD_AXIS_RIGHTY;axis.value=32767;
            sdl_gamepad_handle_axis(&axis);
            assert(scroll>0 && nonmenu_keys==keys_before);
            int scrolled=scroll;
            sdl_gamepad_handle_axis(&axis);assert(scroll==scrolled);
            axis.value=0;sdl_gamepad_handle_axis(&axis);
            scroll=0;g_question_menu.scroll_follow_highlight=true;
            assert(sdl_question_menu_layout(&l));
        }
        inside(l.controller_hint);assert(strstr(l.controller_hint_text,"<A>") && strstr(l.controller_hint_text,"<B>"));
        assert(!overlap(l.entries_rect,l.controller_hint));
        for(int i=0;i<l.button_count;i++) assert(!overlap(l.buttons[i],l.controller_hint));
        clear();sdl_question_menu_render();snapshot("questions");
        sdl_question_menu_set_help("Long help explains each option and how it affects the hero. Read the choices carefully before accepting. Long help explains each option and how it affects the hero. Read the choices carefully before accepting. Long help explains each option and how it affects the hero. Read the choices carefully before accepting. Long help explains each option and how it affects the hero. Read the choices carefully before accepting.");
        assert(sdl_question_menu_handle_help_key(steamdeck_info_key()));
        assert(g_question_menu.help_open && sdl_question_menu_layout(&l));
        assert(strstr(l.controller_hint_text,"<View>"));
        int before=g_question_menu.help_scroll_offset;
        assert(sdl_question_menu_queue_navigation(1));
        assert(!sdl_question_menu_take_navigation());
        if(l.max_scroll_offset>before) assert(g_question_menu.help_scroll_offset>before);
        assert(sdl_question_menu_handle_help_key(ESCAPE) && !g_question_menu.help_open);
        sdl_question_menu_set_nonblocking(true);assert(sdl_question_menu_layout(&l));assert(l.controller_hint.h==0);
    } else assert(l.controller_hint.h==0);
    sdl_question_menu_clear();
}
static void songs(bool controller)
{
    config.input_ui_mode=controller?SDL_INPUT_UI_MODE_CONTROLLER:SDL_INPUT_UI_MODE_PLATFORM;
    sdl_song_menu_begin("Songs");
    for(int i=0;i<8;i++) sdl_song_menu_add_entry(i,"","Song of the Trees",TERM_WHITE);
    sdl_song_menu_set_highlight(0);sdl_song_menu_finish();
    sdl_song_menu_layout_info l;assert(sdl_song_menu_layout(&l));
    if(controller) {
        inside(l.controller_hint);
        for(int i=0;i<8;i++) assert(!overlap(l.rows[i],l.controller_hint));
        assert(!overlap(l.prev,l.controller_hint));assert(!overlap(l.next,l.controller_hint));
        clear();sdl_song_menu_render();snapshot("songs");
    } else assert(l.controller_hint.h==0);
    sdl_song_menu_clear();
}
static void hints(void)
{
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS,"Hints & Quests","Level Hints",true,false,1000);
    sdl_hint_quest_menu_add_block("A hint explaining the current dungeon.",TERM_WHITE,0,1000);
    fixture_hint_buttons();sdl_hint_quest_menu_finish();
    sdl_hint_quest_layout l;assert(sdl_hint_quest_layout_compute(&l));
    inside(l.controller_hint);
    if(overlap(l.body,l.controller_hint)) fprintf(stderr,"Hints Big%d body %.0f..%.0f hint %.0f..%.0f footer %.0f..%.0f\n",config.bigger_font,l.body.y,l.body.y+l.body.h,l.controller_hint.y,l.controller_hint.y+l.controller_hint.h,l.footer.y,l.footer.y+l.footer.h);
    assert(!overlap(l.body,l.controller_hint));assert(!overlap(l.footer,l.controller_hint));
    clear();sdl_hint_quest_menu_render();snapshot("hints");
    for(int i=0;i<g_hint_quest.button_count;i++) assert(strchr(g_hint_quest.buttons[i].label,'<'));
    sdl_hint_quest_menu_hide();
}
static void preview(void)
{
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    g_sdl_character_sheet_screen.context=SDL_CHARACTER_SHEET_BIRTH_PREVIEW;
    TTF_Font* font=sdl_story_font_for_height_slot(24,SDL_STORY_FONT_SLOT_MENU);
    clear();sdl_char_sheet_draw_prompt(font,"",20,height-60,width-40,50);snapshot("preview");
}
static void button(int id)
{
    SDL_GamepadButtonEvent ev={0};ev.button=id;ev.down=true;sdl_gamepad_handle_button(&ev);
    ev.down=false;sdl_gamepad_handle_button(&ev);
}
static void panes(void)
{
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;i++) config.gamepad_button_bindings[i]='z';
    int before=nonmenu_keys;
    assert(sdl_log_pane_menu_open_at(100,100,PANE_LOG));
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];int count=0;SDL_FRect panel;
    assert(sdl_log_pane_menu_layout(entries,&count,&panel));inside(panel);
    clear();check_complete=true;cropped=0;sdl_log_pane_menu_render();assert(!cropped);check_complete=false;snapshot("log-options");
    assert(g_log_pane_menu.hover_index==0);
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(g_log_pane_menu.hover_index==1);
    g_gamepad_state.left_ui_dir=-1;config.gamepad_use_left_stick=true;
    SDL_GamepadAxisEvent axis={0};axis.axis=SDL_GAMEPAD_AXIS_LEFTY;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);assert(g_log_pane_menu.hover_index==2);
    axis.value=0;sdl_gamepad_handle_axis(&axis);
    button(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(!g_log_pane_menu.active && g_log_pane_display_pending && g_log_pane_pending_filter==LOG_HISTORY_FILTER_COMBAT);
    g_log_pane_display_pending=false;
    pane_config_count=2;
    pane_config[0].pane=PANE_INVENTORY;pane_config[0].where=PLACE_RIGHT;pane_config[0].enabled=true;
    pane_config[1].pane=PANE_WORN;pane_config[1].where=PLACE_RIGHT;pane_config[1].enabled=true;
    assert(sdl_side_pane_menu_open_at(100,100));
    clear();check_complete=true;cropped=0;sdl_side_pane_menu_render();assert(!cropped);check_complete=false;snapshot("pane-options");
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(g_side_pane_menu.hover_index==1);
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(!pane_config[1].enabled && g_side_pane_menu.active);
    button(SDL_GAMEPAD_BUTTON_EAST);assert(!g_side_pane_menu.active);
    assert(sdl_side_pane_menu_open_at(100,100));
    button(SDL_GAMEPAD_BUTTON_START);assert(!g_side_pane_menu.active);
    assert(nonmenu_keys==before);
}
int main(int argc,char** argv)
{
    assert(argc==6);setbuf(stdout,NULL);log_set_quiet(true);
    width=atoi(argv[1]);height=atoi(argv[2]);playstation=atoi(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());
    sdl_config_set_defaults(&config);config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;config.gamepad_enabled=true;
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));SDL_strlcpy(config.story_font2,argv[4],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Controller menu fixture",width,height,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);g_state.system_scale=1;
    g_startup_device_class=SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;character_generated=true;
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){220,220,220,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};g_state.palette[TERM_SLATE]=(SDL_Color){170,170,180,255};
    g_state.palette[TERM_BLUE]=(SDL_Color){50,80,120,255};g_state.palette[TERM_L_BLUE]=(SDL_Color){100,185,250,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){245,220,150,255};g_state.palette[TERM_L_GREEN]=(SDL_Color){110,230,150,255};
    for(int big=0;big<2;big++) {
        config.bigger_font=big;for(int i=0;i<SDL_MENU_FONT_COUNT;i++)config.menu_bigger_font[i]=big;
        config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
        halls(true,false,1);halls(false,false,2);halls(true,true,3);halls(false,false,0);
        /* The drawable can be narrower than the window/layout report. Both
         * page capacity and every memorial/footer must use the real output. */
        for(int full=0;full<2;full++) {
            int capacity=sdl_halls_screen_page_capacity(full);
            oversized_halls_canvas=true;
            assert(sdl_halls_screen_page_capacity(full)==capacity);
            halls(full,false,2);halls(full,true,3);halls(full,false,0);
            oversized_halls_canvas=false;
        }
        questions(true);questions(false);songs(true);songs(false);hints();preview();panes();
    }
    fixture_check_settings();
    printf("Controller menus %dx%d %s: Normal/Big memorial output bounds/text/scroll, hints, Songs, question Help, pane input, binding cancel, keyboard suppression PASS\n",width,height,playstation?"PlayStation":"Deck");
    sdl_ui_text_cache_clear();sdl_story_font_cache_clear();SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();SDL_Quit();return 0;
}
'''

MENU = r'''
#include "angband.h"
#include <assert.h>
#include "cmd/ui/cmd-ui-main-menu.c"
void fixture_hint_buttons(void)
{
    assert(hint_message_controller_tab_action('[',HINT_QUEST_PAGE_HINTS)==HINT_MESSAGE_ACTION_OPEN_THRALLS);
    assert(hint_message_controller_tab_action(']',HINT_QUEST_PAGE_HINTS)==HINT_MESSAGE_ACTION_OPEN_QUESTS);
    assert(hint_message_controller_tab_action('[',HINT_QUEST_PAGE_THRALLS)==HINT_MESSAGE_ACTION_OPEN_QUESTS);
    assert(hint_message_controller_tab_action(']',HINT_QUEST_PAGE_THRALLS)==HINT_MESSAGE_ACTION_OPEN_HINTS);
    hint_message_add_button(HINT_MESSAGE_CLICK_TOGGLE_TIPS,"All Tips",TERM_WHITE);
    hint_message_add_button(HINT_MESSAGE_CLICK_LOOK,"Look",TERM_WHITE);
    hint_message_add_button(HINT_MESSAGE_CLICK_MAP,"Map",TERM_WHITE);
    hint_message_add_button(HINT_MESSAGE_CLICK_BACK,"Back",TERM_WHITE);
}
'''

SETTINGS = r'''
#include "angband.h"
#include <assert.h>
#include "cmd/ui/cmd-ui-settings.c"
extern bool g_keyboard_capture_active, g_keyboard_capture_ready;
void fixture_check_settings(void)
{
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    assert(settings_keybind_menu_key(steamdeck_alt_action_key(),false,'p')=='r');
    assert(settings_keybind_menu_key(steamdeck_info_key(),false,'p')=='p');
    assert(settings_keybind_menu_key(steamdeck_info_key(),false,'\t')=='\t');
    assert(settings_keybind_menu_key(steamdeck_secondary_key(),false,'p')=='s');
    assert(settings_keybind_menu_key('h',true,'p')=='h');
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    assert(settings_keybind_menu_key('x',false,'p')=='x');
    assert(settings_keybind_menu_key('h',false,'p')=='h');
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    term terminal={0}; assert(term_init(&terminal,80,24,256)==0);Term_activate(&terminal);
    movement_input_binding binding;
    assert(!movement_capture_binding(&movement_settings[0],&binding));
    assert(!g_keyboard_capture_active && !g_keyboard_capture_ready);
    term_nuke(&terminal);Term=NULL;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-halls", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    harness = HARNESS
    if args.baseline_halls:
        source = (ROOT / "src/sdl/ui/sdl-halls-screen.c").read_text()
        before = """(void)sdl_halls_draw_fitted_text(fields[i], TERM_SLATE, field,
            i - 1, min_px, max_px, SDL_STORY_FONT_SLOT_NARRATIVE);"""
        after = """(void)sdl_halls_draw_text(sdl_story_font_for_height_slot(max_px,
            SDL_STORY_FONT_SLOT_NARRATIVE), fields[i], TERM_SLATE, field, i - 1);"""
        assert source.count(before) == 1
        source = source.replace(before, after, 1)
        (OUT / "baseline-halls.c").write_text(source)
        harness = harness.replace('"sdl/ui/sdl-halls-screen.c"', '"baseline-halls.c"')
    source = OUT / "check.c"
    source.write_text(harness)
    menu = OUT / "menu.c"
    menu.write_text(MENU)
    settings = OUT / "settings.c"
    settings.write_text(SETTINGS)
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-halls-screen.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj",
                "/src/sdl/ui/sdl-song-menu.c.obj", "/src/sdl/ui/sdl-hint-quest-menu.c.obj", "/src/sdl/ui/sdl-screens.c.obj",
                "/src/cmd/ui/cmd-ui-main-menu.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj",
                "/src/sdl/ui/sdl-pane-menus.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                                  "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ("sdl_overlay_pane_anchor_rect", "sdl_get_layout_screen_rect", "sdl_get_window_pixel_rect", "SDL_GetDisplayContentScale",
             "sdl_gamepad_prompt_type", "sdl_ui_render_text_texture", "ui_menu_click_handle_choice_action", "Term_keypress", "inkey",
             "save_pane_config_to_json", "sdl_apply_config")
    wraps += ("sdl_ui_text_texture", "sdl_ui_wrapped_text_texture")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(menu), str(settings), "@"+str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+wrap for wrap in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for width, height, ps in [(1280,800,0), (1280,720,0), (960,540,0), (1920,1080,0), (1280,800,1)]:
        subprocess.run([str(exe), str(width), str(height), str(ps),
                        str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf")], cwd=OUT, env=env, check=True, timeout=40)


if __name__ == "__main__":
    main()
