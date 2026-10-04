#!/usr/bin/env python3
"""Render real SDL tutorial cards offscreen using the game's fonts/text code.

No player file or configuration is loaded. Catalogue-derived views are injected
at the renderer boundary; this checks pixels/layout, not physical input.
Pass --all to check every catalogue step without saving bulk screenshots.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-render-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"
#define sdl_touch_tutorial_draw_text_line fixture_draw_text_line
#include "sdl/ui/sdl-gameplay-tutorial.c"
#undef sdl_touch_tutorial_draw_text_line
float sdl_touch_tutorial_draw_text_line(cptr,float,float,float,int,SDL_Color,bool);
static SDL_FRect drawn_text[64];
static bool drawn_button[64];
static int drawn_count;
static char drawn_hints[1024];
static char drawn_content[8192];
static bool drawn_space,drawn_enter;
static int fixture_run_count;
static char fixture_id[160];
static int fixture_width,fixture_height,fixture_font,fixture_scroll;
static bool fixture_touch_controls;
static SDL_FRect fixture_wheel, fixture_quick_access;
bool __real_sdl_touch_round_layer_controls_active(void);
bool __wrap_sdl_touch_round_layer_controls_active(void)
{ return fixture_touch_controls || __real_sdl_touch_round_layer_controls_active(); }
bool __real_sdl_touch_round_compute_layout(float *,float *,float *,float *,SDL_Rect *);
bool __wrap_sdl_touch_round_compute_layout(float *cx,float *cy,float *r,float *inner,SDL_Rect *clip)
{
    if (!fixture_touch_controls) return __real_sdl_touch_round_compute_layout(cx,cy,r,inner,clip);
    if (cx) *cx=fixture_wheel.x+fixture_wheel.w/2;
    if (cy) *cy=fixture_wheel.y+fixture_wheel.h/2;
    if (r) *r=fixture_wheel.w/2;
    if (inner) *inner=fixture_wheel.w/4;
    if (clip) *clip=(SDL_Rect){0,0,fixture_width,fixture_height};
    return true;
}
bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect *r)
{
    if (!fixture_touch_controls) return false;
    *r=fixture_quick_access; return true;
}
bool __real_sdl_map_grid_cell_rect(int,int,SDL_FRect *);
bool __wrap_sdl_map_grid_cell_rect(int y,int x,SDL_FRect *r)
{
    if (!fixture_touch_controls) return __real_sdl_map_grid_cell_rect(y,x,r);
    float size=MIN(48,fixture_width/12.0f);
    *r=(SDL_FRect){fixture_width/2.0f+(x-p_ptr->px-0.5f)*size,
        fixture_height*0.39f+(y-p_ptr->py-0.5f)*size,size,size};
    return true;
}
static void fixture_check(bool condition,const char *expression)
{
    if (!condition) {
        fprintf(stderr,"fixture %s @ %dx%d font %d scroll %d failed: %s\n",
            fixture_id,fixture_width,fixture_height,fixture_font,fixture_scroll,
            expression);
        exit(EXIT_FAILURE);
    }
}
#define fixture_assert(condition) fixture_check((condition),#condition)
float fixture_draw_text_line(cptr text,float x,float y,float width,int px,SDL_Color color,bool centered)
{
    TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_TUTORIAL);
    int w=0,h=0;
    if (text && text[0] && font && TTF_GetStringSize(font,text,0,&w,&h)) {
        SDL_strlcat(drawn_content,text,sizeof(drawn_content));
        SDL_strlcat(drawn_content,"\n",sizeof(drawn_content));
        if (!centered && color.r==182 && color.g==191 && color.b==204) {
            SDL_strlcat(drawn_hints,text,sizeof(drawn_hints));
            SDL_strlcat(drawn_hints,"\n",sizeof(drawn_hints));
        }
        if (centered && !strcmp(text,"Space")) drawn_space=true;
        if (centered && !strcmp(text,"Enter")) drawn_enter=true;
        float scale=w>width && width>0?width/w:1;
        fixture_assert(drawn_count<64);
        drawn_text[drawn_count]=(SDL_FRect){centered?x-w*scale/2:x,y,w*scale,h*scale};
        drawn_button[drawn_count++]=centered;
    }
    return sdl_touch_tutorial_draw_text_line(text,x,y,width,px,color,centered);
}
static tutorial_view fixture;
static tutorial_mode fixture_mode=TUTORIAL_MODE_EXTENDED;
static SDL_Event device_events[3];
static int device_event_count, device_event_index;
bool __real_SDL_WaitEvent(SDL_Event *event);
bool __wrap_SDL_WaitEvent(SDL_Event *event)
{
    if (!device_event_count) return __real_SDL_WaitEvent(event);
    assert(device_event_index < device_event_count);
    *event = device_events[device_event_index++];
    return true;
}
tutorial_mode __wrap_get_sdl_gameplay_tutorial_mode(void) { return fixture_mode; }
bool __wrap_tutorial_get_view(tutorial_view *out) { *out=fixture; return fixture.active; }
bool __wrap_tutorial_is_active(void) { return fixture.active; }
unsigned int __wrap_tutorial_revision(void) { return 1; }

static void field(cJSON *obj,const char *key,char *dst,size_t size)
{
    cJSON *value=cJSON_GetObjectItemCaseSensitive(obj,key);
    SDL_strlcpy(dst,cJSON_IsString(value)?value->valuestring:"",size);
}

static void paint(cJSON *entry,int width,int height,int index,int scroll,
    int font_override,bool capture)
{
    char file[80];
    SDL_Surface *pixels;
    sdl_view *view=&g_views[PANE_MAIN];
    int font_size=16;
    cJSON *font_value=cJSON_GetObjectItemCaseSensitive(entry,"font_size");
    if (cJSON_IsNumber(font_value)) font_size=font_value->valueint;
    if (font_override>0) font_size=font_override;
    field(entry,"id",fixture_id,sizeof(fixture_id));
    fixture_width=width; fixture_height=height; fixture_font=font_size;
    fixture_touch_controls=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"touch_controls"));
    float radius=MIN(160,MIN(width*0.30f,height*0.30f));
    fixture_wheel=width>height
        ?(SDL_FRect){width-2*radius-8,height-2*radius-72,2*radius,2*radius}
        :(SDL_FRect){width/2.0f-radius,height*0.77f-radius,2*radius,2*radius};
    fixture_quick_access=(SDL_FRect){8,height-64,width-16,56};
    fixture_scroll=scroll; ++fixture_run_count;
    if (capture || fixture_run_count%100==0)
        printf("fixture %s @ %dx%d font %d scroll %d%s (run %d)\n",fixture_id,
            width,height,font_size,scroll,capture?" [capture]":"",fixture_run_count);
    g_state.window=SDL_CreateWindow("Tutorial render fixture",width,height,SDL_WINDOW_HIDDEN);
    fixture_assert(g_state.window!=NULL);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    fixture_assert(g_state.renderer!=NULL);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    term_init(&view->t,width/8,height/16,256);
    Term_activate(&view->t); term_screen=&view->t;
    view->term_ready=true; view->rect=(SDL_Rect){0,0,width,height};
    view->cell_w=8; view->cell_h=16; view->cols=width/8; view->rows=height/16;
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height);
    fixture_assert(view->canvas!=NULL);
    fixture=(tutorial_view){.active=true,.step=1,.step_count=3,.revision=1};
    cJSON *step=cJSON_GetObjectItemCaseSensitive(entry,"step");
    cJSON *step_count=cJSON_GetObjectItemCaseSensitive(entry,"step_count");
    if (cJSON_IsNumber(step)) fixture.step=step->valueint;
    if (cJSON_IsNumber(step_count)) fixture.step_count=step_count->valueint;
    field(entry,"title",fixture.title,sizeof(fixture.title));
    field(entry,"body",fixture.body,sizeof(fixture.body));
    field(entry,"action",fixture.action,sizeof(fixture.action));
    field(entry,"subject",fixture.action_subject,sizeof(fixture.action_subject));
    field(entry,"anchor",fixture.anchor,sizeof(fixture.anchor));
    field(entry,"context",fixture.context.text,sizeof(fixture.context.text));
    field(entry,"subject_name",fixture.context.subject,sizeof(fixture.context.subject));
    field(entry,"subject_type",fixture.context.subject_type,sizeof(fixture.context.subject_type));
    cJSON *mode=cJSON_GetObjectItemCaseSensitive(entry,"mode");
    fixture_mode=cJSON_IsNumber(mode)?mode->valueint:TUTORIAL_MODE_EXTENDED;
    fixture.can_continue=!fixture.action[0];
    cJSON *input=cJSON_GetObjectItemCaseSensitive(entry,"input");
    tutorial_last_input=cJSON_IsNumber(input)?input->valueint:TUTORIAL_INPUT_KEYBOARD;
    config.input_ui_mode=tutorial_last_input==TUTORIAL_INPUT_CONTROLLER
        ?SDL_INPUT_UI_MODE_CONTROLLER:SDL_INPUT_UI_MODE_AUTO;
    fixture.kind=fixture.can_continue?TUTORIAL_STEP_INFO:TUTORIAL_STEP_ACTION;
    character_icky=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"menu"));
    sdl_gameplay_tutorial_set_menu_preview(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"preview")),false);
    sdl_gameplay_tutorial_set_menu_preview_control("View");
    ui_menu_click_clear();
    if (character_icky) {
        ui_menu_click_begin();
        if (!cJSON_GetObjectItemCaseSensitive(entry,"surface"))
            for (int row=2;row<MIN(view->rows-2,10);++row)
                ui_menu_click_add_full_row(row,row);
    }
    tutorial_seen_revision=1; tutorial_was_active=true;
    tutorial_focus=0; tutorial_scroll=scroll;
    tutorial_reading=scroll>0 && !fixture.can_continue;
    config.aux_view_font_size=font_size;
    SDL_SetRenderDrawColor(g_state.renderer,34,38,39,255);
    SDL_RenderClear(g_state.renderer);
    SDL_SetRenderDrawColor(g_state.renderer,56,61,57,255);
    for (int x=0;x<width;x+=32) SDL_RenderLine(g_state.renderer,x,0,x,height);
    for (int y=0;y<height;y+=32) SDL_RenderLine(g_state.renderer,0,y,width,y);
    sdl_touch_tutorial_draw_text_line("LIVE GAME / MENU FIXTURE",12,12,width-24,18,
        (SDL_Color){180,195,177,255},false);
    if (fixture_touch_controls) {
        SDL_FRect neighborhood;
        fixture_assert(tutorial_movement_neighborhood_rect(&neighborhood));
        SDL_SetRenderDrawColor(g_state.renderer,100,180,160,255);
        SDL_RenderRect(g_state.renderer,&neighborhood);
        SDL_RenderRect(g_state.renderer,&fixture_wheel);
        SDL_RenderRect(g_state.renderer,&fixture_quick_access);
        sdl_touch_round_draw_circle(fixture_wheel.x+radius,fixture_wheel.y+radius,
            radius,(SDL_Color){160,210,190,255});
    }
    drawn_count=0; drawn_hints[0]=drawn_content[0]='\0'; drawn_space=drawn_enter=false;
    cJSON *render_state=cJSON_GetObjectItemCaseSensitive(entry,"render_state");
    if (cJSON_IsString(render_state)) {
        if (!strcmp(render_state->valuestring,"pane")) {
            fixture_assert(SDL_SetRenderTarget(g_state.renderer,view->canvas));
            SDL_SetRenderDrawColor(g_state.renderer,255,0,255,255);
            SDL_RenderClear(g_state.renderer);
        }
        /* Terminal drawing and native menus can leave a pane target or a
         * narrow clip behind. The tutorial owns the complete window. */
        SDL_Rect clip={width/2,height-60,4,44};
        fixture_assert(SDL_SetRenderClipRect(g_state.renderer,&clip));
    }
    cJSON *surface=cJSON_GetObjectItemCaseSensitive(entry,"surface");
    if (cJSON_IsString(surface)) {
        if (!strcmp(surface->valuestring,"halls")) {
            sdl_halls_screen_begin("Here are remembered the fates of those who entered Angband.",
                "Score (highest first) | full memorials | page 1 of 6",true,-1);
            for (int row=0;row<3;++row)
                sdl_halls_screen_add_entry(row,"#1","The recorded hero","441 pts",
                    "Escaped the iron hells of Angband empty-handed.",
                    "52 turns | 12,850 ft | 25 May 2026","Escaped",TERM_L_BLUE,
                    "Score increases: descent +200 pts | unique kills +6 pts",
                    "Score decreases: none | Formula: 306 base x 1.440 = 441 pts",
                    TERM_GREEN,row==2);
            const char *actions[]={"Back","Run History","Order: Score","View: Full","Open Hero","Next"};
            for (int i=0;i<6;++i)
                sdl_halls_screen_add_action(-i-1,actions[i],TERM_L_WHITE,true);
        } else if (!strcmp(surface->valuestring,"hints")) {
            sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS,"Hints and Quests",
                "Hints",true,false,0);
            sdl_hint_quest_menu_add_block("Review the hints you have found.",TERM_L_WHITE,0,0);
            sdl_hint_quest_menu_add_button(-1,"Back",TERM_L_WHITE);
            sdl_hint_quest_menu_finish();
        } else if (!strcmp(surface->valuestring,"tales")) {
            fixture_assert(sdl_tale_screen_begin("The Tale So Far"));
            sdl_tale_screen_add_entry("Nienna's Mercy","You awaken in the halls of Angband.");
            sdl_tale_screen_set_prompt("Press any key to continue",true,true);
        } else {
            sdl_character_sheet_screen_begin_select(0,"Options");
            sdl_character_sheet_screen_add_select_row(0,"Input",TERM_L_WHITE,"Choose input settings.");
            sdl_character_sheet_screen_add_select_row(1,"Presentation",TERM_L_WHITE,"Choose presentation settings.");
            sdl_character_sheet_screen_add_select_row(2,"Tutorial cards",TERM_L_BLUE,"Choose tutorial settings.");
            fixture_assert(sdl_character_sheet_screen_commit_select(0));
        }
        fixture_assert(sdl_render_current_window_frame());
    } else sdl_gameplay_tutorial_render();
    cJSON *forbidden_text=cJSON_GetObjectItemCaseSensitive(entry,"forbidden_text");
    if (cJSON_IsString(forbidden_text))
        fixture_assert(!strstr(drawn_content,forbidden_text->valuestring));
    cJSON *expected_text=cJSON_GetObjectItemCaseSensitive(entry,"expected_text");
    if (cJSON_IsString(expected_text))
        fixture_assert(strstr(drawn_content,expected_text->valuestring)!=NULL);
    bool suspended=character_icky && !fixture.can_continue && !tutorial_reading && !tutorial_card.w;
    if (suspended) {
        fixture_assert(drawn_count==0);
        for (int b=0;b<3;++b) fixture_assert(!tutorial_buttons[b].w);
    } else if (character_icky && !fixture.can_continue && !tutorial_reading) {
        SDL_FRect rows;
        if (tutorial_menu_cells_rect(&rows))
            fixture_assert(!SDL_HasRectIntersectionFloat(&tutorial_card,&rows));
    }
    if (fixture_touch_controls && !tutorial_reading) {
        fixture_assert(!SDL_HasRectIntersectionFloat(&tutorial_card,&fixture_wheel));
        fixture_assert(!SDL_HasRectIntersectionFloat(&tutorial_card,&fixture_quick_access));
        if (!character_icky && (strstr(fixture.action,"move") || strstr(fixture.action,"attack"))) {
            SDL_FRect neighborhood;
            fixture_assert(tutorial_movement_neighborhood_rect(&neighborhood));
            fixture_assert(!SDL_HasRectIntersectionFloat(&tutorial_card,&neighborhood));
        }
    }
    if (!character_icky) {
        bool scrolling_hint=strstr(drawn_hints,"PgUp")
            || strstr(drawn_hints,"PgDn") || strstr(drawn_hints,"Wheel")
            || strstr(drawn_hints,"drag") || strstr(drawn_hints,"Swipe")
            || strstr(drawn_hints,"scroll") || strstr(drawn_hints,"read card")
            || strstr(drawn_hints,": read");
        fixture_assert(scrolling_hint==(tutorial_max_scroll>0));
    }
    if (tutorial_last_input==TUTORIAL_INPUT_KEYBOARD && fixture.can_continue) {
        fixture_assert(drawn_space);
        fixture_assert(!drawn_enter);
    }
    cJSON *expected_scroll=cJSON_GetObjectItemCaseSensitive(entry,"expected_scroll");
    if (cJSON_IsBool(expected_scroll))
        fixture_assert((tutorial_max_scroll>0)==cJSON_IsTrue(expected_scroll));
    fixture_assert(tutorial_card.x>=0 && tutorial_card.y>=0);
    fixture_assert(tutorial_card.x+tutorial_card.w<=width);
    fixture_assert(tutorial_card.y+tutorial_card.h<=height);
    for (int i=0;i<3;++i) {
        if (!tutorial_buttons[i].w) { fixture_assert(i==0 || suspended); continue; }
        fixture_assert(tutorial_buttons[i].h>=44);
        fixture_assert(tutorial_buttons[i].x>=tutorial_card.x);
        fixture_assert(tutorial_buttons[i].x+tutorial_buttons[i].w<=tutorial_card.x+tutorial_card.w);
        fixture_assert(tutorial_buttons[i].y>=tutorial_card.y);
        fixture_assert(tutorial_buttons[i].y+tutorial_buttons[i].h<=tutorial_card.y+tutorial_card.h);
    }
    for (int i=0;i<drawn_count;++i) {
        SDL_FRect *r=&drawn_text[i];
        fixture_assert(r->x>=tutorial_card.x-0.01f);
        fixture_assert(r->y>=tutorial_card.y-0.01f);
        fixture_assert(r->x+r->w<=tutorial_card.x+tutorial_card.w+0.01f);
        fixture_assert(r->y+r->h<=tutorial_card.y+tutorial_card.h+0.01f);
        if (drawn_button[i]) {
            bool contained=false;
            for (int b=0;b<3;++b) {
                SDL_FRect *button=&tutorial_buttons[b];
                if (r->x>=button->x && r->y>=button->y
                    && r->x+r->w<=button->x+button->w+0.01f
                    && r->y+r->h<=button->y+button->h+0.01f) contained=true;
            }
            fixture_assert(contained);
        } else {
            for (int b=0;b<3;++b)
                fixture_assert(!SDL_HasRectIntersectionFloat(r,&tutorial_buttons[b]));
        }
        for (int j=0;j<i;++j) {
            if (SDL_HasRectIntersectionFloat(r,&drawn_text[j]))
                printf("overlap fixture %d %dx%d: text %d (%.1f %.1f %.1f %.1f), %d (%.1f %.1f %.1f %.1f)\n",index,width,height,i,r->x,r->y,r->w,r->h,j,drawn_text[j].x,drawn_text[j].y,drawn_text[j].w,drawn_text[j].h);
            fixture_assert(!SDL_HasRectIntersectionFloat(r,&drawn_text[j]));
        }
    }
    if (width==2880 && height==1800 && font_size>=48) {
        if (tutorial_max_scroll!=0)
            fprintf(stderr,"fixture %s: expected desktop font %d to fit, got scroll %d/%d\n",
                fixture_id,font_size,tutorial_scroll,tutorial_max_scroll);
        fixture_assert(tutorial_max_scroll==0);
    }
    if (scroll>0 && tutorial_max_scroll>0) {
        if (tutorial_scroll!=tutorial_max_scroll)
            fprintf(stderr,"fixture %s: end scroll stopped at %d/%d\n",
                fixture_id,tutorial_scroll,tutorial_max_scroll);
        fixture_assert(tutorial_scroll==tutorial_max_scroll);
    }
    if (tutorial_card.w>0) {
        Uint8 r,g,b,a;
        SDL_Rect probe={(int)(tutorial_card.x+2),(int)(tutorial_card.y+2),1,1};
        fixture_assert(SDL_GetRenderTarget(g_state.renderer)==NULL);
        fixture_assert(!SDL_RenderClipEnabled(g_state.renderer));
        pixels=SDL_RenderReadPixels(g_state.renderer,&probe);
        fixture_assert(pixels!=NULL);
        /* Check pixels, not only the text layout: an invisible card may
         * still have valid geometry and consume all of the menu's input. */
        fixture_assert(SDL_ReadSurfacePixel(pixels,0,0,&r,&g,&b,&a));
        fixture_assert(r>=16 && r<=22 && g>=21 && g<=28 && b>=28 && b<=38);
        SDL_DestroySurface(pixels);
    }
    if (capture) {
        pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
        fixture_assert(pixels!=NULL);
        SDL_snprintf(file,sizeof(file),"card-%d-%dx%d-%s.bmp",index,width,height,scroll?"end":"start");
        fixture_assert(SDL_SaveBMP(pixels,file));
        SDL_DestroySurface(pixels);
        printf("%s: card %.0fx%.0f; scroll %d/%d\n",file,tutorial_card.w,tutorial_card.h,tutorial_scroll,tutorial_max_scroll);
    }
    if (cJSON_IsString(surface)) {
        sdl_halls_screen_hide();
        sdl_hint_quest_menu_hide();
        sdl_tale_screen_hide();
        sdl_character_sheet_screen_hide();
    }
    sdl_story_font_cache_clear();
    SDL_DestroyTexture(view->canvas); view->canvas=NULL;
    term_nuke(&view->t); view->term_ready=false; term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}

int main(int argc,char **argv)
{
    size_t length;
    char *data;
    maxima limits={0};
    player_type player={0};
    assert(argc==2);
    setbuf(stdout,NULL);
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    sdl_sync_palette();
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.use_unsafe_area=true;
    g_state.system_scale=1;
    z_info=&limits; p_ptr=&player;
    p_ptr->playing=true; p_ptr->py=7;p_ptr->px=8;
    p_ptr->cur_map_hgt=32;p_ptr->cur_map_wid=64;
    character_generated=character_dungeon=true;
    /* Exercise the real device-tutorial event loop too: holding confirm must
     * not skip a page, but a new confirm must still advance it. */
    device_events[0].type=SDL_EVENT_KEY_DOWN;
    device_events[0].key.key=SDLK_SPACE; device_events[0].key.repeat=true;
    device_events[1].type=SDL_EVENT_KEY_DOWN; device_events[1].key.key=SDLK_ESCAPE;
    device_event_count=2; device_event_index=0;
    assert(sdl_touch_tutorial_wait_action(0)==2);
    assert(device_event_index==2);
    device_events[0].key.repeat=false;
    device_event_count=1; device_event_index=0;
    assert(sdl_touch_tutorial_wait_action(0)==1);
    assert(device_event_index==1); device_event_count=0;
    data=SDL_LoadFile("fixtures.json",&length); assert(data);
    cJSON *entries=cJSON_Parse(data); assert(entries); SDL_free(data);
    for (int i=0;i<cJSON_GetArraySize(entries);++i) {
        cJSON *entry=cJSON_GetArrayItem(entries,i);
        cJSON *bulk=cJSON_GetObjectItemCaseSensitive(entry,"bulk");
        cJSON *capture=cJSON_GetObjectItemCaseSensitive(entry,"capture");
        bool save=cJSON_IsTrue(capture);
        cJSON *fixture_size=cJSON_GetObjectItemCaseSensitive(entry,"size");
        if (cJSON_IsArray(fixture_size)) {
            int width=cJSON_GetArrayItem(fixture_size,0)->valueint;
            int height=cJSON_GetArrayItem(fixture_size,1)->valueint;
            paint(entry,width,height,i,0,0,save);
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"expected_scroll")))
                paint(entry,width,height,i,1000,0,false);
            continue;
        }
        if (cJSON_IsTrue(bulk)) {
            paint(entry,2880,1800,i,0,48,false);
            paint(entry,320,480,i,0,16,false);
            paint(entry,320,240,i,0,16,false);
            paint(entry,320,240,i,1000,16,false);
            continue;
        }
        paint(entry,1280,720,i,0,0,save);
        if (cJSON_GetObjectItemCaseSensitive(entry,"font_size"))
            paint(entry,2880,1800,i,0,0,save);
        paint(entry,320,480,i,0,0,save);
        if (cJSON_GetObjectItemCaseSensitive(entry,"action")->valuestring[0])
            paint(entry,320,240,i,0,0,save);
        paint(entry,320,240,i,1000,0,save);
    }
    cJSON_Delete(entries); TTF_Quit(); SDL_Quit();
    puts("Gameplay tutorial software-render fixtures: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    catalogue = json.loads((ROOT / "lib/help/tutorials.json").read_text())
    lessons = catalogue["lessons"]
    opening = next(lesson for lesson in lessons if lesson["id"] == "opening.move")
    fleeing = next(lesson for lesson in lessons if lesson["id"] == "combat.fleeing")
    longest = max(((lesson, step) for lesson in lessons for step in lesson["steps"]),
                  key=lambda pair: len(pair[1]["text"]))
    selections = [(opening, opening["steps"][0], False, 1, False),
                  (opening, opening["steps"][-1], False, 4, False),
                  (fleeing, fleeing["steps"][0], False, 2, False),
                  (longest[0], longest[1], True, 1, False)]
    description = next(lesson for lesson in lessons if lesson["id"] == "item.first_description")
    preview_step = next(step for step in description["steps"] if step.get("action") == "examine")
    selections.extend((description, preview_step, True, device, True) for device in range(1, 5))

    def make_fixture(lesson, step, menu, device, preview, *, fixture_id=None,
                     capture=True, bulk=False, context_text="",
                     subject_name=""):
        step_number = lesson["steps"].index(step) + 1
        subject = subject_name or "The selected creature"
        detail = context_text or "Read the current requirements and costs before choosing."
        body = step["text"].replace("{subject}", subject)
        body = body.replace("{context}", detail)
        body = body.replace("{detail}", detail)
        return {"id": fixture_id or f"{lesson['id']}.step{step_number}",
                "title": lesson["title"], "body": body,
                "action": step.get("action", ""),
                "subject": step.get("subject_type", ""),
                "anchor": "inventory" if preview else "abilities" if menu else step.get("anchor", "none"),
                "menu": menu, "input": device, "preview": preview,
                "step": step_number, "step_count": len(lesson["steps"]),
                "context": context_text, "subject_name": subject_name,
                "subject_type": "monster" if subject_name else "",
                "capture": capture, "bulk": bulk}

    fixtures = []
    for lesson, step, menu, device, preview in selections:
        if lesson is fleeing:
            fixtures.append(make_fixture(
                lesson, step, menu, device, preview,
                context_text="This creature is fleeing. Morale differs from awareness; check your oath before pursuing or attacking.",
                subject_name="the Wolf"))
        else:
            fixtures.append(make_fixture(lesson, step, menu, device, preview))
    selected_count = len(fixtures)
    for mode in (0, 1):
        fixtures.append(dict(fixtures[0], id=f"{fixtures[0]['id']}.mode{mode}", mode=mode))
    fixtures.extend(dict(fixture, id=f"{fixture['id']}.font48", font_size=48)
                     for fixture in fixtures[:selected_count])
    # All introductory pages have Continue and never append lesson-wide text,
    # including stale context already captured by a displayed card.
    for lesson_id in ("opening.move", "combat.first_adjacent"):
        lesson = next(entry for entry in lessons if entry["id"] == lesson_id)
        for step in lesson["steps"]:
            for size in ([580, 1280], [360, 800]):
                fixture = make_fixture(lesson, step, False, 3, False,
                    context_text="FOOTERBUG Find a suitable weapon and armour.",
                    subject_name="the Orc")
                fixture.update(size=size, font_size=28, forbidden_text="FOOTERBUG",
                               expected_text="Continue")
                fixtures.append(fixture)
    result = next(entry for entry in lessons if entry["id"] == "combat.first_result")
    fixture = make_fixture(result, result["steps"][0], False, 3, False,
                           context_text="DETAILOK Attack 12 against Evasion 10.")
    fixture.update(size=[580, 1280], font_size=28, expected_text="DETAILOK")
    fixtures.append(fixture)
    for lesson_id, detail, expected in (
        ("combat.critical", "Your hit margin was 12 with a 3.0 lb weapon, adding 1 critical die.", "margin was 12"),
        ("status.poisoned", "Severity 15; the next damage tick is 3 Health.", "Severity 15"),
        ("status.cut", "Severity 10; the next damage tick is 2 Health.", "Severity 10"),
        ("status.stun", "Stun 60: -4 to every skill.", "Stun 60"),
        ("status.diseased", "Disease penalties: Strength -1, Dexterity +0, Constitution -1, Grace +0.", "Disease penalties"),
        ("storage.pack", "Pack space: 8/20.", "8/20"),
        ("storage.harness", "Harness space: 6/10.", "6/10"),
        ("storage.quiver", "Arrows: 15. Current free Quiver space: 30.", "Arrows: 15"),
    ):
        lesson = next(entry for entry in lessons if entry["id"] == lesson_id)
        assert lesson["steps"][0]["text"].count("{detail}") == 1
        fixture = make_fixture(lesson, lesson["steps"][0], False, 3, False,
                               context_text=detail, capture=False)
        fixture.update(size=[580, 1280], font_size=28, expected_text=expected)
        fixtures.append(fixture)
    for device in range(1, 5):
        for font_size in (16, 48):
            for case, action, body, size, expected_scroll in (
                ("info.fit", "", "A short lesson.", [1280, 720], False),
                ("info.overflow", "", "Read each line of this longer lesson carefully. " * 30,
                 [320, 240], True),
                ("compact.fit", "move", "Move.", [320, 240], False),
                ("compact.overflow", "move", "Read each line before moving. " * 20,
                 [320, 240], True),
            ):
                fixtures.append({"id": f"hints.{case}.input{device}.font{font_size}",
                                 "title": "Lesson", "body": body, "action": action,
                                 "input": device, "font_size": font_size,
                                 "size": size, "expected_scroll": expected_scroll,
                                 "capture": False})
    # Exercise the complete frame, including native full-screen early returns.
    for surface in ("halls", "settings", "hints", "tales"):
        lesson = next(entry for entry in lessons if entry["id"] == "menu." + surface)
        for size in ([1280, 720], [3840, 2160], [580, 1280]):
            fixture = make_fixture(lesson, lesson["steps"][0], True, 2, False)
            fixture.update(surface=surface, size=size, font_size=48,
                           expected_text=lesson["title"], capture=True)
            fixtures.append(fixture)
    for render_state in ("window", "pane"):
        lesson = next(entry for entry in lessons if entry["id"] == "menu.halls")
        fixture = make_fixture(lesson, lesson["steps"][0], True, 2, False)
        fixture.update(render_state=render_state, size=[1280, 720],
                       expected_text=lesson["title"], capture=False)
        fixtures.append(fixture)
    # Reproduce the portrait control geometry from the reported overlap. Check
    # every action step with both touch and mouse ownership, not only movement.
    for lesson in lessons:
        for step in lesson["steps"]:
            if not step.get("action"):
                continue
            for size, font_size in (([580, 1280], 28), ([360, 800], 20), ([800, 360], 20)):
                for device in (2, 3):
                    fixture = make_fixture(lesson, step, False, device, False,
                                           capture=lesson is opening and device == 3,
                                           context_text="Find a suitable weapon and armour. Inspect each item before using or equipping it."
                                               if lesson is opening else "")
                    fixture.update(touch_controls=True, size=size, font_size=font_size)
                    fixtures.append(fixture)
                    fixtures.append(dict(fixture, menu=True, capture=False,
                                         id=fixture["id"]+".menu"))
    if "--all" in sys.argv[1:]:
        for lesson in lessons:
            for step in lesson["steps"]:
                fixtures.append(make_fixture(
                    lesson, step, False, 1, False,
                    fixture_id=f"{lesson['id']}.step{lesson['steps'].index(step) + 1}.sweep",
                    capture=False, bulk=True))
    (OUT / "fixtures.json").write_text(json.dumps(fixtures), encoding="utf-8")
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-main-menu.c.obj", "/src/sdl-config.c.obj",
                "/src/tutorial/tutorial.c.obj")
    objects = [obj for obj in objects if not obj.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/sdl/ui/sdl-main-menu.c"), str(ROOT / "src/sdl-config.c"),
                    str(ROOT / "src/tutorial/tutorial.c"), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=tutorial_get_view,--wrap=tutorial_is_active,--wrap=tutorial_revision,--wrap=get_sdl_gameplay_tutorial_mode,--wrap=SDL_WaitEvent,--wrap=sdl_touch_round_layer_controls_active,--wrap=sdl_touch_round_compute_layout,--wrap=sdl_touch_thumb_current_bounds,--wrap=sdl_map_grid_cell_rect",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                   cwd=OUT, env=env, check=True, timeout=600 if "--all" in sys.argv[1:] else 60)


if __name__ == "__main__":
    main()
