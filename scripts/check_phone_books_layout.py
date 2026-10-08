#!/usr/bin/env python3
"""Privately render production birth books/hero details at real phone density.

No user data, emulator, or shared build is touched. Actual race prose is read
from birth-selection.c; page/scroll rendering and hit geometry are production.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
from check_phone_aux_layout import PREFIX

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/phone-books-check"

HARNESS = PREFIX + r'''
static float fixture_density;
bool sdl_mobile_lifecycle_handle_event(const SDL_Event *event) { return false; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id) { return fixture_density; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
bool __wrap_sdl_mobile_portrait_layout_active(void) { return fixture_width<fixture_height; }
bool __wrap_sdl_touch_tutorial_device_available(void) { return true; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){0,0,fixture_width,fixture_height}; }
static bool recording,footer_recording;
static void record_text(TTF_Font*,cptr,SDL_FRect,float);
/* SCREENS SOURCE */
/* RACE CONSTANTS */
static int picker_result,picker_calls;
int __wrap_ui_question_ask_overlay(cptr title,cptr desc,const ui_question_option *options,
    int count,int anchor_y,int anchor_x,int default_index)
{
    fixture_assert(!strcmp(title,"Chapters") && !strcmp(desc,"Choose a chapter."));
    fixture_assert(count==3 && anchor_y==UI_QUESTION_GLOBAL && anchor_x==UI_QUESTION_GLOBAL && default_index==0);
    for(int i=0;i<count;i++) {
        fixture_assert(!options[i].disabled && options[i].key==0);
        fixture_assert(!strcmp(options[i].label,g_sdl_character_sheet_screen.narrative_contents_label[i]));
    }
    ++picker_calls;return picker_result;
}
typedef struct { char text[256]; SDL_FRect ink,box; SDL_Rect clip; bool clipped,footer; float scale,font_px; } text_record;
static text_record records[1024];
static int record_count,issues;
static char reached[32768];
static void expect(bool condition,cptr reason)
{
    if(!condition) { ++issues; fprintf(stderr,"%s %dx%d density%.3f Bigger%d: %s\n",fixture_id,fixture_width,fixture_height,fixture_density,config.bigger_font,reason); }
}
static bool inside(SDL_FRect r)
{ return r.x>=-1 && r.y>=-1 && r.x+r.w<=fixture_width+1 && r.y+r.h<=fixture_height+1; }
static bool overlap(SDL_FRect a,SDL_FRect b)
{ return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h; }
static void record_text(TTF_Font *font,cptr text,SDL_FRect box,float wrap_width)
{
    if(!recording || !font || !text || !text[0] || !box.w || !box.h) return;
    fixture_assert(record_count<1024);
    text_record *rec=&records[record_count++];
    *rec=(text_record){.box=box,.footer=footer_recording};
    SDL_strlcpy(rec->text,text,sizeof(rec->text));
    rec->clipped=SDL_RenderClipEnabled(g_state.renderer);
    if(rec->clipped) SDL_GetRenderClipRect(g_state.renderer,&rec->clip);
    rec->font_px=TTF_GetFontSize(font);
    SDL_Surface *surface=TTF_RenderText_Blended_Wrapped(font,text,0,
        (SDL_Color){255,255,255,255},MAX(1,(int)wrap_width));
    fixture_assert(surface);
    rec->scale=box.h/surface->h;
    int left=surface->w,right=-1,top=surface->h,bottom=-1;
    for(int y=0;y<surface->h;y++) for(int x=0;x<surface->w;x++) {
        Uint8 r,g,b,a;
        fixture_assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
        if(a) { left=MIN(left,x);right=MAX(right,x);top=MIN(top,y);bottom=MAX(bottom,y); }
    }
    rec->ink=(SDL_FRect){box.x+left*box.w/surface->w,box.y+top*box.h/surface->h,
        MAX(0,right-left+1)*box.w/surface->w,MAX(0,bottom-top+1)*box.h/surface->h};
    SDL_DestroySurface(surface);
}
static void capture(cptr name)
{
    char path[160];
    strnfmt(path,sizeof(path),"%s-%dx%d-%.3f-%s.png",name,fixture_width,fixture_height,fixture_density,config.bigger_font?"on":"off");
    SDL_Surface *surface=SDL_RenderReadPixels(g_state.renderer,NULL);
    fixture_assert(surface && IMG_SavePNG(surface,path)); SDL_DestroySurface(surface);
}
static void frame(void)
{
    record_count=0; recording=true;
    fixture_assert(sdl_render_current_window_frame()); recording=false;
    float footer_top=fixture_height;
    float body_top=fixture_height;
    for(int i=0;i<record_count;i++) if(records[i].clipped && !records[i].footer)
        body_top=MIN(body_top,records[i].clip.y);
    if(sdl_character_sheet_screen_mobile_carousel_active()) {
        text_record *power=NULL,*counter=NULL;
        for(int i=0;i<record_count;i++) {
            if(!strcmp(records[i].text," *** mighty")) power=&records[i];
            if(!strcmp(records[i].text,"1/3") || !strcmp(records[i].text,"2/3")) counter=&records[i];
        }
        expect(power && counter,"hero META suffix/counter missing");
        if(power && counter) {
            int meta_px=sdl_ui_role_font_px(SDL_UI_FONT_META);
            expect(power->font_px==meta_px && counter->font_px==meta_px,"hero suffix/counter differs from fixed META font role");
            expect(power->scale>=.99f && counter->scale>=.99f,"hero META suffix/counter shrinks below resolved size");
            expect(inside(power->ink) && inside(counter->ink),"hero META suffix/counter ink outside screen");
            expect(!overlap(power->ink,counter->ink),"hero suffix and counter ink intersect");
            expect(power->ink.y+power->ink.h<=body_top+1 && counter->ink.y+counter->ink.h<=body_top+1,"hero META ink overlaps body viewport");
            for(int i=0;i<record_count;i++) {
                text_record *name=&records[i];
                if(name==power || name==counter || name->clipped || name->footer || name->box.y>=power->box.y) continue;
                expect(name->ink.y+name->ink.h<=MIN(power->ink.y,counter->ink.y)+1,"hero name ink overlaps suffix/counter line");
            }
        }
    }
    int footer_count=0;
    for(int i=0;i<record_count;i++) {
        text_record *r=&records[i];
        if(!strcmp(r->text,"Chapters")) {
            expect(inside(r->ink),"Chapters glyph ink outside screen");
            expect(r->scale>=.99f,"Chapters label shrinks below CONTROL role");
        }
        if(!r->footer) continue;
        footer_count++;
        expect(r->scale>=.99f,"footer text shrinks below resolved font size");
        footer_top=MIN(footer_top,r->box.y);
        if(!inside(r->ink)) { fprintf(stderr,"footer [%s] ink%.1f %.1f %.1f %.1f\n",r->text,r->ink.x,r->ink.y,r->ink.w,r->ink.h); expect(false,"footer glyph ink outside screen"); }
        if(r->clipped) expect(r->ink.y>=r->clip.y-1 && r->ink.y+r->ink.h<=r->clip.y+r->clip.h+1,"footer glyph ink clipped");
    }
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *h=&g_sdl_character_sheet_screen.hits[i];
        expect(inside(h->rect),"hit outside physical screen");
        if(h->choice>=0) for(int j=0;j<g_sdl_character_sheet_screen.hit_count;j++) {
            sdl_character_sheet_hit *other=&g_sdl_character_sheet_screen.hits[j];
            if(other->choice<0 && other->rect.y>fixture_height*.5f)
                expect(!overlap(h->rect,other->rect),"body hit target intersects footer");
        }
        if((h->choice>=5001 && h->choice<=5003) || h->choice==SDL_CHAR_SHEET_CONTENTS_PICKER) {
            if(h->rect.w+1<sdl_ui_min_tap_px() || h->rect.h+1<sdl_ui_min_tap_px())
                fprintf(stderr,"Contents hit%d %.1fx%.1f min%d\n",h->choice,h->rect.w,h->rect.h,sdl_ui_min_tap_px());
            expect(h->rect.w+1>=sdl_ui_min_tap_px() && h->rect.h+1>=sdl_ui_min_tap_px(),"Contents target below48dp");
        }
        if(h->choice<0 && h->rect.y>fixture_height*.5f) {
            expect(h->rect.w+1>=sdl_ui_min_tap_px() && h->rect.h+1>=sdl_ui_min_tap_px(),"footer target below48dp");
            footer_top=MIN(footer_top,h->rect.y);
            for(int j=i+1;j<g_sdl_character_sheet_screen.hit_count;j++)
                if(g_sdl_character_sheet_screen.hits[j].choice<0 && g_sdl_character_sheet_screen.hits[j].rect.y>fixture_height*.5f)
                    expect(!overlap(h->rect,g_sdl_character_sheet_screen.hits[j].rect),"footer hit targets intersect");
        }
    }
    for(int i=0;i<record_count;i++) {
        text_record *r=&records[i];
        if(r->footer) continue;
        bool visible=inside(r->ink);
        if(!r->clipped && body_top<fixture_height && r->ink.y<body_top)
            expect(r->ink.y+r->ink.h<=body_top+1,"header glyph ink overlaps body viewport");
        if(r->clipped) {
            expect(r->clip.y+r->clip.h<=footer_top+1,"body clip intersects footer");
            visible &= r->ink.y>=r->clip.y-1 && r->ink.y+r->ink.h<=r->clip.y+r->clip.h+1;
        } else if(r->ink.y+r->ink.h>footer_top+1) {
            fprintf(stderr,"unclipped text [%s] ink bottom%.1f footer top%.1f\n",r->text,r->ink.y+r->ink.h,footer_top);
            expect(false,"body glyph ink intersects footer");
        }
        if(visible && !strstr(reached,r->text)) { SDL_strlcat(reached,r->text,sizeof(reached)); SDL_strlcat(reached,"\n",sizeof(reached)); }
    }
    expect(footer_count>0,"no rendered footer text");
}
static void check_picker(void)
{
    SDL_FRect button={0};
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        if(g_sdl_character_sheet_screen.hits[i].choice==SDL_CHAR_SHEET_CONTENTS_PICKER)
            button=g_sdl_character_sheet_screen.hits[i].rect;
    fixture_assert(button.w>0 && button.h>0);
    for(int test=0;test<3;test++) {
        picker_result=test==0?-1:test==1?0:2;
        int calls=picker_calls,page=g_sdl_character_sheet_screen.select_page;
        s32b before_turn=turn;int before_energy=p_ptr->energy_use;
        expect(sdl_character_sheet_screen_handle_pointer_button(button.x+button.w/2,
            button.y+button.h/2,UI_MENU_CLICK_PRIMARY),"Chapters tap unhandled");
        expect(picker_calls==calls+1,"Chapters picker did not open");
        expect(g_sdl_character_sheet_screen.select_page==page && turn==before_turn && p_ptr->energy_use==before_energy,"picker unexpectedly advances page or gameplay");
        int choice=0,action=0;char key=0;
        if(picker_result<0) {
            expect(!ui_menu_click_take_action(&choice,&action),"cancel leaves pending engine action");
            expect(Term_inkey(&key,false,true)!=0,"cancel leaks a command key");
        } else {
            expect(ui_menu_click_take_action(&choice,&action),"chosen chapter has no pending engine action");
            expect(choice==g_sdl_character_sheet_screen.narrative_contents_choice[picker_result] && action==UI_MENU_CLICK_PRIMARY,"chosen chapter routes wrong engine ID");
            expect(Term_inkey(&key,false,true)==0 && key=='\r',"chosen chapter does not wake engine with Return");
            expect(Term_inkey(&key,false,true)!=0,"chosen chapter leaks extra keys");
        }
        frame();
    }
}
static void scroll_to_end(cptr name)
{
    frame(); int maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
    int guard=0;
    while(sdl_character_sheet_screen_scroll_book(1)) {
        fixture_assert(++guard<500); frame();
        expect(g_sdl_character_sheet_screen.sheet_scroll_max==maximum,"body scroll maximum changed within page");
    }
    expect(g_sdl_character_sheet_screen.sheet_scroll==maximum,"body end unreachable");
    capture(name);
}
static void check_books(bool narrative)
{
    reached[0]=0;
    SDL_strlcpy(fixture_id,narrative?"intro-narrative":"race-book",sizeof(fixture_id));
    ui_menu_click_begin();
    if(narrative) {
        sdl_character_sheet_screen_begin_book("The War of the Jewels");
        sdl_character_sheet_screen_add_book_contents("The Tale",5001,0);
        sdl_character_sheet_screen_add_book_contents("The War",5002,1);
        sdl_character_sheet_screen_add_book_contents("The Peoples",5003,2);
        sdl_character_sheet_screen_set_book_target_page_count(3);
        sdl_character_sheet_screen_add_book_paragraph(birth_frame_top);
        char lore[4096];SDL_strlcpy(lore,birth_intro_lore,sizeof(lore));
        char *paragraph=lore;
        while(paragraph && paragraph[0]) {
            char *next=strstr(paragraph,"\n\n");
            if(next) { *next=0;next+=2; }
            sdl_character_sheet_screen_add_book_paragraph(paragraph);paragraph=next;
        }
        sdl_character_sheet_screen_add_book_paragraph(birth_frame_bottom);
        sdl_character_sheet_screen_set_book_close_button(true);
        sdl_character_sheet_screen_set_book_close_label("Begin your journey");
        sdl_character_sheet_screen_commit_book();
    } else {
        sdl_character_sheet_screen_begin_select(0,"Choose your people");
        sdl_character_sheet_screen_set_select_intro(birth_intro_lore);
        sdl_character_sheet_screen_set_select_frame(birth_frame_top,birth_frame_bottom);
        sdl_character_sheet_screen_add_select_row(0,"Noldor",TERM_WHITE,"High Elves of the West");
        sdl_character_sheet_screen_add_select_row(1,"Sindar",TERM_WHITE,"Grey Elves of Beleriand");
        sdl_character_sheet_screen_add_select_row(2,"Naugrim",TERM_WHITE,"");
        sdl_character_sheet_screen_add_select_row(3,"Edain",TERM_WHITE,"");
        sdl_character_sheet_screen_add_select_row(4,"Fingolfinrim",TERM_WHITE,"");
        sdl_character_sheet_screen_add_select_row(5,"Finarfinrim",TERM_WHITE,"");
        sdl_character_sheet_screen_set_select_description("The Noldor are the High Elves of the West, deep in lore and craft, who dwelt in the light of Valinor. For love of the stolen Silmarils they returned to Middle-earth in exile to make war upon Morgoth. Proud and gifted, they raised shining kingdoms and forged wondrous things, yet a doom of sorrow shadows their valour. ENDCHOICELORE");
        fixture_assert(sdl_character_sheet_screen_commit_select(0));
    }
    frame(); capture(narrative?"narrative-first":"race-first");
    int body=narrative?g_sdl_character_sheet_screen.narrative_body_px:g_sdl_character_sheet_screen.select_book_body_px;
    if(body!=sdl_ui_role_font_px(SDL_UI_FONT_BODY)) fprintf(stderr,"book body %d expected role %d\n",body,sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    expect(body==sdl_ui_role_font_px(SDL_UI_FONT_BODY),"book body differs from resolved BODY role");
    if(narrative) {
        if(g_sdl_character_sheet_screen.narrative_contents_body_px!=sdl_ui_role_font_px(SDL_UI_FONT_BODY))
            fprintf(stderr,"Contents px%d expected%d\n",g_sdl_character_sheet_screen.narrative_contents_body_px,sdl_ui_role_font_px(SDL_UI_FONT_BODY));
        expect(g_sdl_character_sheet_screen.narrative_contents_body_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY),"Contents differs from resolved BODY role");
        if(fixture_width>fixture_height) check_picker();
    }
    int count=g_sdl_character_sheet_screen.select_page_count;
    for(int page=0;page<count;page++) {
        g_sdl_character_sheet_screen.select_page=page;
        sdl_character_sheet_set_scroll(0);
        scroll_to_end(narrative?"narrative-end":"race-end");
    }
    expect(strstr(reached,"Thangorodrim")!=NULL,"Thangorodrim prose unreachable");
    expect(strstr(reached,"Mandos")!=NULL,"middle race prose unreachable");
    expect(strstr(reached,"renown")!=NULL,"last race prose unreachable");
    if(!narrative) expect(strstr(reached,"ENDCHOICELORE")!=NULL,"final choice lore unreachable");
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
static void hero(int selected)
{
    static cptr names[]={"Feanor","Maedhros","Celebrimbor"};
    sdl_character_sheet_screen_begin_select(selected,"Choose your hero");
    for(int i=0;i<3;i++) sdl_character_sheet_screen_add_select_row(i,names[i],TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_title_detail(selected==0?"Celebrimbor, Last of the House of Feanor":names[selected]," *** mighty",TERM_GREEN);
    sdl_character_sheet_screen_set_select_description("A mighty spirit, maker of Silmarils, you carry this tale into darkness. Through fire and grief the long history unfolds. ENDLORE");
    sdl_character_sheet_screen_set_select_detail_size_hint(4,2,9);
    sdl_character_sheet_screen_set_select_ability_rows(2);
    static cptr stats[]={"STR\t+2","DEX\t+3","CON\t+3","GRA\t+4"};
    for(int i=0;i<4;i++) sdl_character_sheet_screen_add_select_detail(stats[i],TERM_L_BLUE,"Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("Artifice",TERM_RED,"Starting ability");
    sdl_character_sheet_screen_add_select_detail("Jeweler",TERM_RED,"Starting ability");
    for(int i=0;i<9;i++) { char text[40];strnfmt(text,sizeof(text),"Trait %d Affinity",i+1);sdl_character_sheet_screen_add_select_detail(text,TERM_GREEN,"Trait bonus"); }
    fixture_assert(sdl_character_sheet_screen_commit_select(selected));
}
static void check_hero(void)
{
    SDL_strlcpy(fixture_id,"hero-carousel",sizeof(fixture_id));reached[0]=0;
    ui_menu_click_begin();hero(0);
    expect(sdl_character_sheet_screen_mobile_carousel_active(),"hero carousel inactive");
    scroll_to_end("hero-end");
    for(int i=0;i<4;i++) expect(strstr(reached,((cptr[]){"STR","DEX","CON","GRA"})[i])!=NULL,"hero stat unreachable");
    expect(strstr(reached,"Artifice") && strstr(reached,"Jeweler"),"hero abilities unreachable");
    expect(strstr(reached,"Trait 9 Affinity")!=NULL,"last hero trait unreachable");
    expect(strstr(reached,"ENDLORE")!=NULL,"last hero lore unreachable");
    /* Production carousel key path changes the current hero without leaking
     * the vertical detail scroll to the new selection. */
    sdl_character_sheet_birth_swipe_begin(fixture_width*.8f,fixture_height*.5f,7);
    expect(sdl_character_sheet_birth_swipe_motion(fixture_width*.2f,fixture_height*.5f,7),"hero horizontal swipe unhandled");
    char key=0;expect(Term_inkey(&key,false,true)==0 && key=='6',"hero swipe did not emit next key");
    int next=1;
    hero(next);frame();expect(g_sdl_character_sheet_screen.sheet_scroll==0,"hero inherits previous vertical scroll");
    capture("hero-next");sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
static void check_allocation(void)
{
    int base[S_MAX]={0},gain[S_MAX]={0},cost[S_MAX]={0};
    SDL_strlcpy(fixture_id,"allocation",sizeof(fixture_id));
    reached[0]=0;
    sdl_character_sheet_screen_show_birth_stats(base,cost,A_GRA,5000);frame();capture("stats");
    expect(strstr(reached,"Gra")!=NULL,"selected Grace glyph ink is clipped");
    sdl_character_sheet_screen_hide();
    sdl_character_sheet_screen_show_birth_skills(base,gain,cost,S_SNG,5000);frame();capture("skills");
    expect(strstr(reached,"Song")!=NULL,"selected Song glyph ink is clipped");
    if(config.bigger_font && g_sdl_character_sheet_screen.sheet_scroll_max==0) {
        /* Wide high-density phones naturally fit zero-valued allocation
         * rows. Long numeric values force real production wrapping/overflow
         * without replacing the measured viewport or hit rectangles. */
        for(int skill=0;skill<S_MAX;skill++) {
            p_ptr->skill_use[skill]=9999;p_ptr->skill_base[skill]=9999;
            p_ptr->skill_stat_mod[skill]=999;p_ptr->skill_equip_mod[skill]=999;
            p_ptr->skill_misc_mod[skill]=999;base[skill]=9999;
        }
        sdl_character_sheet_screen_show_birth_skills(base,gain,cost,S_SNG,5000);
        frame();capture("skills-overflow");
        expect(g_sdl_character_sheet_screen.sheet_scroll_max>0,"Big Skills stress fails to exercise overflow");
    }
    SDL_FRect song={0};
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
        if(g_sdl_character_sheet_screen.hits[i].choice==S_SNG) song=g_sdl_character_sheet_screen.hits[i].rect;
    fixture_assert(song.w>0 && song.h>0);
    int maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
    if(maximum>0) for(int i=0;i<record_count;i++) if(!strncmp(records[i].text,"Song",4) && records[i].clipped) {
        SDL_Rect clip=records[i].clip;SDL_FRect drag=g_sdl_character_sheet_screen.select_scroll_rect;
        bool same=drag.x>=clip.x-1 && drag.x<=clip.x+1 && drag.y>=clip.y-1 && drag.y<=clip.y+1
            && drag.w>=clip.w-1 && drag.w<=clip.w+1 && drag.h>=clip.h-1 && drag.h<=clip.h+1;
        if(!same)fprintf(stderr,"Skills real clip%d,%d,%d,%d differs drag viewport%.1f,%.1f,%.1f,%.1f\n",clip.x,clip.y,clip.w,clip.h,drag.x,drag.y,drag.w,drag.h);
        expect(same,"Skills drag viewport differs from actual body clip");
    }
    if(maximum>0) expect(g_sdl_character_sheet_screen.select_scroll_rect.y<=song.y
        && g_sdl_character_sheet_screen.select_scroll_rect.y+g_sdl_character_sheet_screen.select_scroll_rect.h>=song.y+song.h,
        "overflowing Skills row outside published drag viewport");
    float x=song.x+song.w/2,y=song.y+song.h/2;
    for(int held=0;held<2;held++) {
        ui_menu_click_begin();
        SDL_Event ev={0};ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
        ev.tfinger.touchID=1;ev.tfinger.fingerID=7;
        ev.tfinger.x=x/fixture_width;ev.tfinger.y=y/fixture_height;
        ev.tfinger.timestamp=SDL_GetTicksNS();ev.type=SDL_EVENT_FINGER_DOWN;
        expect(sdl_character_sheet_screen_handle_event(&ev),"Skills finger down unhandled");
        if(maximum>0) expect(g_sdl_character_sheet_screen.select_scroll_drag.active,"overflow Skills finger down not owned by drag viewport");
        expect(g_sdl_character_sheet_screen.touch_press.active,"Skills finger down does not arm purchase/refund press");
        if(held)g_sdl_character_sheet_screen.touch_press.start_time-=600000000ULL;
        ev.type=SDL_EVENT_FINGER_UP;ev.tfinger.timestamp=SDL_GetTicksNS();
        expect(sdl_character_sheet_screen_handle_event(&ev),"Skills finger up unhandled");
        int choice=0,action=0;char key=0;
        expect(ui_menu_click_take_action(&choice,&action),"Skills tap has no pending engine action");
        expect(choice==S_SNG && action==(held?UI_MENU_CLICK_SECONDARY:UI_MENU_CLICK_PRIMARY),"Skills tap emits wrong purchase/refund action");
        expect(Term_inkey(&key,false,true)==0 && key=='\r',"Skills tap does not emit one Return");
        expect(Term_inkey(&key,false,true)!=0,"Skills tap leaks extra keys");
    }
    if(maximum>0) {
        ui_menu_click_begin();
        SDL_Event ev={0};ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
        ev.tfinger.touchID=1;ev.tfinger.fingerID=7;ev.tfinger.timestamp=SDL_GetTicksNS();
        ev.tfinger.x=x/fixture_width;ev.tfinger.y=y/fixture_height;ev.type=SDL_EVENT_FINGER_DOWN;
        sdl_character_sheet_screen_handle_event(&ev);
        int before_scroll=g_sdl_character_sheet_screen.sheet_scroll;
        ev.tfinger.y=(y+MAX(100.0f,sdl_ui_density_scale()*60))/fixture_height;
        ev.type=SDL_EVENT_FINGER_MOTION;sdl_character_sheet_screen_handle_event(&ev);
        ev.type=SDL_EVENT_FINGER_UP;sdl_character_sheet_screen_handle_event(&ev);
        int choice=0,action=0;char key=0;
        expect(g_sdl_character_sheet_screen.sheet_scroll!=before_scroll,"Skills drag fails to scroll overflowing rows");
        expect(!ui_menu_click_take_action(&choice,&action),"Skills drag emits purchase/refund selection");
        expect(Term_inkey(&key,false,true)!=0,"Skills drag leaks command key");
        expect(g_sdl_character_sheet_screen.selected_index==S_SNG,"Skills drag changes selected skill");
    }
    printf("Skills gesture case: scroll maximum%d (primary/secondary%s)\n",maximum,maximum>0?"/drag":"");
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
int main(int argc,char **argv)
{
    maxima limits={0};player_type player={0};player_other options={0};object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==8);setbuf(stdout,NULL);log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);config.bigger_font=atoi(argv[7]);
    fixture_density=atof(argv[6]);g_state.system_scale=fixture_density;
    fixture_width=atoi(argv[2]);fixture_height=atoi(argv[3]);
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;config.use_unsafe_area=true;
    z_info=&limits;p_ptr=&player;op_ptr=&options;inventory=items;
    g_state.window=SDL_CreateWindow("Phone books fixture",fixture_width,fixture_height,SDL_WINDOW_HIDDEN);fixture_assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");fixture_assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
    sdl_view *view=&g_views[PANE_MAIN];term_init(&view->t,80,24,256);Term_activate(&view->t);term_screen=&view->t;character_icky=1;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){angband_color_table[i][1],angband_color_table[i][2],angband_color_table[i][3],255};
    check_books(false);check_books(true);check_hero();check_allocation();
    printf("%dx%d density%.3f Bigger%d: books/hero/allocation %s (%d issues)\n",fixture_width,fixture_height,fixture_density,config.bigger_font,issues?"FAIL":"PASS",issues);
    return issues?1:0;
}
'''


def brace_end(source, start):
    position = source.index("{", start)
    depth = 1
    position += 1
    while depth:
        depth += (source[position] == "{") - (source[position] == "}")
        position += 1
    return position


def main():
    screens = (ROOT / "src/sdl/ui/sdl-screens.c").read_text()
    for signature in ["static SDL_FRect sdl_char_sheet_draw_text_aligned(",
                      "static SDL_FRect sdl_char_sheet_draw_text_alpha("]:
        start = screens.index(signature)
        end = brace_end(screens, start)
        drawing = screens[start:end].replace("SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);",
            "record_text(font,text,dst,max_w);\n    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);")
        screens = screens[:start] + drawing + screens[end:]
    signature = "static void sdl_char_sheet_draw_book_page_controls("
    start = screens.index(signature)
    end = brace_end(screens, start)
    actual = screens[start:end].replace("sdl_char_sheet_draw_book_page_controls(", "actual_book_controls(", 1)
    wrapper = r'''
static void sdl_char_sheet_draw_book_page_controls(TTF_Font *font,float x,float w,float y,float h,int page,int count)
{ footer_recording=true;actual_book_controls(font,x,w,y,h,page,count);footer_recording=false; }
'''
    screens = screens[:start] + actual + wrapper + screens[end:]
    signature = "void sdl_char_sheet_draw_prompt("
    start = screens.index(signature)
    end = brace_end(screens, start)
    actual = screens[start:end].replace("sdl_char_sheet_draw_prompt(", "actual_draw_prompt(", 1)
    wrapper = r'''
void sdl_char_sheet_draw_prompt(TTF_Font *font,cptr prompt,float x,float y,float w,float h)
{ bool old=footer_recording;footer_recording=true;actual_draw_prompt(font,prompt,x,y,w,h);footer_recording=old; }
'''
    screens = screens[:start] + actual + wrapper + screens[end:]
    birth = (ROOT / "src/birth/birth-selection.c").read_text()
    constants = "\n".join(re.search(r"static const char " + name + r'\[\] =.*?";', birth, re.S).group()
                          for name in ["birth_frame_top", "birth_intro_lore", "birth_frame_bottom"])
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("/* SCREENS SOURCE */", screens).replace("/* RACE CONSTANTS */", constants), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj", "/src/sdl/ui/sdl-screens.c.obj",
                "/src/sdl/render/sdl-fonts.c.obj", "/src/sdl/input/sdl-touch-tutorial.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
                                  + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision", "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout", "sdl_touch_thumb_current_bounds",
             "sdl_map_grid_cell_rect", "sdl_touch_only_device_active", "sdl_get_layout_screen_rect", "SDL_GetDisplayContentScale",
             "sdl_touch_tutorial_device_available", "sdl_mobile_portrait_layout_active", "ui_question_ask_overlay"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    failures = []
    for width, height, density in [(720, 1600, 2), (1080, 2340, 2.75), (1080, 2400, 2.625)]:
        for w, h in [(width, height), (height, width)]:
            for bigger in [1]:  # Normal designs are covered by check_normal_ui_compat.py.
                result = subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(w), str(h),
                    str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"), str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                    str(density), str(bigger)], cwd=OUT, env=env, timeout=60)
                if result.returncode: failures.append(f"{w}x{h}@{density} Bigger{bigger}")
    if failures: raise SystemExit("Failed: " + ", ".join(failures))


if __name__ == "__main__":
    main()
