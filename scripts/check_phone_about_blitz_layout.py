#!/usr/bin/env python3
"""Render actual semantic About/Blitz builders using production native SDL UI.

Reuses the private real-density book harness (including glyph/hit/footer
measurement), with unique outputs. Never modifies the shared build or a save.
"""
from pathlib import Path
import os
import re
import subprocess
import check_phone_books_layout as books

ROOT=Path(__file__).resolve().parents[1]
def function(source,marker):
 start=source.rindex(marker);return source[start:books.brace_end(source,start)]

def main():
 menu=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
 blitz=(ROOT/'src/birth/birth-blitz.c').read_text()
 declarations='#include "blitz.h"\n'
 declarations+=re.search(r'typedef struct main_menu_about_line.*?main_menu_about_line;',menu,re.S).group()
 declarations+=re.search(r'static const main_menu_about_line about_lines\[\] = \{.*?\n    \};',menu,re.S).group()
 declarations+=function(menu,'static void main_menu_about_semantic_attrs(')
 declarations+=function(menu,'static void main_menu_about_semantic_build(')
 declarations+=function(menu,'static void main_menu_blitz_intro_mobile_build(')
 for marker in ['static cptr blitz_character_mode_name(', 'static cptr blitz_effect_mode_name(', 'static void blitz_setup_draw_mobile(']:
  declarations+=function(blitz,marker)
 checks=r'''
static void check_book_exit(void)
{
    ui_menu_click_begin();ui_menu_click_set_hover_enabled(true);
    bool found=false;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice!=SDL_SELECT_CLICK_CLOSE)continue;
        expect(sdl_character_sheet_screen_handle_pointer_button(hit->rect.x+hit->rect.w/2,
            hit->rect.y+hit->rect.h/2,UI_MENU_CLICK_PRIMARY),"book exit tap unhandled");
        int choice=0,action=0;char key=0;
        expect(ui_menu_click_take_action(&choice,&action)&&choice==SDL_SELECT_CLICK_CLOSE&&action==UI_MENU_CLICK_PRIMARY,"book exit dispatch wrong");
        expect(Term_inkey(&key,false,true)==0,"book exit fails to wake caller");found=true;break;
    }
    expect(found,"book exit hit missing");
}
static int about_big_body_px;
static void check_actual_about_blitz(void)
{
    reached[0]=0;SDL_strlcpy(fixture_id,"actual-about",sizeof(fixture_id));
    main_menu_about_semantic_build(about_lines);
    frame();capture("about-first");
    expect(g_sdl_character_sheet_screen.narrative_full_width,
        "About does not request the full-width semantic page");
    expect(g_sdl_character_sheet_screen.narrative_terminal_style,
        "About does not preserve the terminal font style");
    expect(g_sdl_character_sheet_screen.font_menu==SDL_MENU_FONT_ABOUT,
        "About loses its independent semantic font identity");
    expect(g_sdl_character_sheet_screen.narrative_para_count==21,
        "About loses authored blank lines");
    expect(g_sdl_character_sheet_screen.narrative_para_char_attr[0][0]
            ==TERM_VIOLET
        && g_sdl_character_sheet_screen.narrative_para_char_attr[0][28]
            ==TERM_L_BLUE,
        "About loses Sil-More/SilQ colours");
    expect(g_sdl_character_sheet_screen.narrative_para_char_attr[3][0]
            ==TERM_YELLOW
        && g_sdl_character_sheet_screen.narrative_para_char_attr[3][11]
            ==TERM_WHITE,
        "About label colour leaks into its value");
    expect(sdl_char_sheet_narrative_font_for_height(
            g_sdl_character_sheet_screen.narrative_body_px)
        ==sdl_main_menu_mono_font_for_height(
            g_sdl_character_sheet_screen.narrative_body_px),
        "About body does not use the configured terminal font");
    float probe_w=(float)fixture_width*0.91f;
    float framed_w=sdl_char_sheet_book_width(
        g_sdl_character_sheet_screen.narrative_body_px,probe_w)
        +2.0f*sdl_char_sheet_book_margin_h(0.0f,probe_w);
    float frame_diff=framed_w-probe_w;if(frame_diff<0.0f)frame_diff=-frame_diff;
    expect(frame_diff<1.0f,
        "About bordered page does not fill the content width");
    if(get_sdl_menu_bigger_font()) {
        about_big_body_px=g_sdl_character_sheet_screen.narrative_body_px;
        expect(about_big_body_px>0,"Big About has no semantic body font");
    } else {
        expect(g_sdl_character_sheet_screen.narrative_body_px>0,
            "Normal About has no semantic body font");
        expect(about_big_body_px>=g_sdl_character_sheet_screen.narrative_body_px,
            "Big About is smaller than Normal About");
    }
    int count=g_sdl_character_sheet_screen.select_page_count;
    for(int page=0;page<count;page++) {
        g_sdl_character_sheet_screen.select_page=page;
        sdl_character_sheet_set_scroll(0);scroll_to_end("about-end");
    }
    expect(strstr(reached,"Tolkien") && strstr(reached,"timeless") && strstr(reached,"creations."),"full Tolkien tail unreachable");
    expect(strstr(reached,"k0rtess") && strstr(reached,"West") && strstr(reached,"Wind") && strstr(reached,"Ninjikin"),"credits unreachable");
    bool back=false;for(int i=0;i<record_count;i++)if(!strcmp(records[i].text,"Back"))back=true;
    expect(back,"About footer loses Back label");
    check_book_exit();
    sdl_character_sheet_screen_hide();
    if(!get_sdl_menu_bigger_font())return;
    reached[0]=0;SDL_strlcpy(fixture_id,"actual-blitz-intro",sizeof(fixture_id));
    main_menu_blitz_intro_mobile_build();frame();capture("blitz-intro-first");
    bool exit_label_fits=false;
    for(int i=0;i<record_count;i++) {
        text_record *r=&records[i];
        if(!r->footer || (strcmp(r->text,"Continue") && strcmp(r->text,"Go on"))) continue;
        for(int j=0;j<g_sdl_character_sheet_screen.hit_count;j++) {
            sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[j];
            if(hit->choice!=SDL_SELECT_CLICK_CLOSE) continue;
            TTF_Font *font=sdl_story_font_for_height_slot((int)r->font_px,SDL_STORY_FONT_SLOT_DEFAULT);
            expect(sdl_char_sheet_text_width(font,r->text)<=hit->rect.w,
                "intro exit label splits a word to fit its button");
            expect(r->ink.x>=hit->rect.x-1 && r->ink.y>=hit->rect.y-1
                && r->ink.x+r->ink.w<=hit->rect.x+hit->rect.w+1
                && r->ink.y+r->ink.h<=hit->rect.y+hit->rect.h+1,
                "intro exit glyphs spill outside their button");
            exit_label_fits=true;
        }
    }
    expect(exit_label_fits,"intro exit label missing");
    count=g_sdl_character_sheet_screen.select_page_count;
    for(int page=0;page<count;page++) {
        g_sdl_character_sheet_screen.select_page=page;sdl_character_sheet_set_scroll(0);scroll_to_end("blitz-intro-end");
    }
    expect(strstr(reached,"self-contained")&&strstr(reached,"metaprogress")&&strstr(reached,"living")&&strstr(reached,"save")&&strstr(reached,"current")&&strstr(reached,"first."),"Blitz intro paragraphs unreachable");
    check_book_exit();
    sdl_character_sheet_screen_hide();
    SDL_strlcpy(fixture_id,"actual-blitz-setup",sizeof(fixture_id));
    blitz_setup setup={.character_mode=BLITZ_CHARACTER_RANDOM_STATS,.oaths_enabled=true,
        .blessing_count=9,.curse_count=9,.effect_mode=BLITZ_EFFECT_SELECTED_DESCR};
    reached[0]=0;
    for(int selected=0;selected<5;selected++) {
        ui_menu_click_begin();blitz_setup_draw_mobile(&setup,selected);frame();capture("blitz-setup");
        expect(g_sdl_character_sheet_screen.select_row_count==6
            && g_sdl_character_sheet_screen.select_rows[0].is_heading,
            "setup loses an option or its static instructions");
        expect(!strcmp(g_sdl_character_sheet_screen.select_confirm_label,"Begin"),"setup footer starts run without Begin label");
        const sdl_character_sheet_select_row *row=&g_sdl_character_sheet_screen.select_rows[selected+1];
        expect(strstr(reached,row->label)!=NULL,"selected setup full value unreachable");
        bool begin_glyph=false;for(int i=0;i<record_count;i++)if(!strcmp(records[i].text,"Begin"))begin_glyph=true;
        expect(begin_glyph,"actual Begin glyph missing");
    }
    expect(strstr(reached,"Random with stats") && strstr(reached,"Selected + descriptions"),"full setup values unreachable");
    expect(strstr(reached,"Left/right") && strstr(reached,"again") && strstr(reached,"Begin"),
        "setup value-change instructions are not visible");
    bool clicked_begin=false;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice!=-2)continue;
        s32b before_turn=turn;int before_energy=p_ptr->energy_use;
        expect(sdl_character_sheet_screen_handle_pointer_button(hit->rect.x+hit->rect.w/2,
            hit->rect.y+hit->rect.h/2,UI_MENU_CLICK_PRIMARY),"Begin tap unhandled");
        int choice=0,action=0;char key=0;
        expect(ui_menu_click_take_action(&choice,&action)&&choice==-2&&action==UI_MENU_CLICK_PRIMARY,"Begin wrong engine dispatch");
        expect(Term_inkey(&key,false,true)==0&&key=='\r',"Begin fails to wake caller");
        expect(turn==before_turn&&p_ptr->energy_use==before_energy,"Begin dispatch consumes gameplay turn");
        clicked_begin=true;break;
    }
    expect(clicked_begin,"Begin hit target missing");
    sdl_character_sheet_screen_hide();
    sdl_character_sheet_screen_begin_select(0,"Ordinary menu");
    expect(!g_sdl_character_sheet_screen.select_confirm_label[0],"Begin override leaks into next menu");
    sdl_character_sheet_screen_set_select_menu_style(true);
    sdl_character_sheet_screen_add_select_row(0,"Ordinary choice",TERM_WHITE,"");
    sdl_character_sheet_screen_commit_select(0);frame();
    bool choose=false;for(int i=0;i<record_count;i++)if(!strcmp(records[i].text,"Choose"))choose=true;
    expect(choose,"ordinary menu fails to restore Choose glyph");
    sdl_character_sheet_screen_hide();
}
'''
 # Existing production native glyph measurement records every drawn word,
 # body viewport, footer and hit target; only fixtures are replaced here.
 books.HARNESS=books.HARNESS.replace('int main(int argc,char **argv)',declarations+checks+'\nint main(int argc,char **argv)')
 books.HARNESS=books.HARNESS.replace('check_books(false);check_books(true);check_hero();check_allocation();','check_actual_about_blitz();')
 books.HARNESS=books.HARNESS.replace(
  'expect(r->scale>=.99f,"footer text shrinks below resolved font size");',
  'if(!g_sdl_character_sheet_screen.narrative_terminal_style)'
  'expect(r->scale>=.99f,"footer text shrinks below resolved font size");')
 books.HARNESS=books.HARNESS.replace('check_actual_about_blitz();',
  'check_actual_about_blitz();config.bigger_font=false;'
  'for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++)config.menu_bigger_font[menu]=false;'
  'check_actual_about_blitz();')
 books.HARNESS=books.HARNESS.replace('books/hero/allocation','About/Blitz')
 books.OUT=ROOT/'scripts/output/phone-about-blitz-check'
 books.main()
 # Also exercise the Steam Deck / desktop-density canvas where About used to
 # be constrained by the 16-row terminal grid. The generated harness runs the
 # semantic surface once with Big Font and once with Normal Font.
 env=os.environ.copy()
 env['PATH']=os.pathsep.join([str(books.BUILD/'_deps'/dep)
  for dep in ['SDL','SDL_ttf','SDL_image','SDL_mixer']]
  +['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
 exe=books.OUT/'check.exe'
 for width,height in [(1280,800),(800,1280)]:
  subprocess.run([str(exe),str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf'),
   str(width),str(height),str(ROOT/'lib/xtra/font/VictorMono-Medium.ttf'),
   str(ROOT/'lib/xtra/font/Cinzel-Medium.ttf'),'1','1'],cwd=books.OUT,
   env=env,check=True,timeout=60)

if __name__=='__main__':main()
