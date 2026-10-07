#!/usr/bin/env python3
"""Render the short Blitz confirmation after the separately paged introduction."""
from pathlib import Path
import check_phone_short_question as question
from check_phone_about_blitz_layout import function

ROOT=Path(__file__).resolve().parents[1]
def main():
 source=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
 caller=function(source,'static bool main_menu_blitz_confirm_mobile(')
 caller=caller.replace('ui_question_ask_overlay(', 'fixture_question(')
 caller=caller.replace('main_menu_blitz_intro_mobile_build();', '').replace('if (!main_menu_mobile_book_wait()) return false;', '')
 checks=r'''
#include "ui/question.h"
static int fixture_question(cptr title,cptr desc,const ui_question_option *options,
    int count,int y,int x,int selected)
{
    fixture_assert(count==2 && selected==1);
    fixture_assert(!strcmp(desc,"Save your story game and switch to Blitz now?"));
    ui_menu_click_begin();sdl_question_menu_begin(title);sdl_question_menu_set_desc(desc);
    for(int i=0;i<count;i++)sdl_question_menu_add_entry(i,"",options[i].label,options[i].attr);
    sdl_question_menu_set_highlight(selected);sdl_question_menu_finish();
    sdl_question_menu_layout_info layout;fixture_assert(sdl_question_menu_layout(&layout));
    inside(layout.panel);fixture_assert(layout.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    inventory_background();sdl_question_menu_render();capture("blitz-intro-first");
    if(layout.max_scroll_offset>0) {
        g_question_menu.scroll_follow_highlight=false;
        fixture_assert(sdl_question_menu_scroll_offset_by(&layout,100000));
        fixture_assert(sdl_question_menu_layout(&layout));
    }
    inventory_background();sdl_question_menu_render();capture("blitz-intro-end");
    /* Both full answer labels remain reachable after the full introduction. */
    for(int i=0;i<count;i++) {
        int offset=(int)(layout.rows[i].y-layout.entries_rect.y);
        if(offset>0) {
            g_question_menu.scroll_follow_highlight=false;
            sdl_question_menu_scroll_offset_by(&layout,offset);
            fixture_assert(sdl_question_menu_layout(&layout));
        }
        SDL_FRect hit;fixture_assert(SDL_GetRectIntersectionFloat(&layout.rows[i],&layout.entries_rect,&hit));
        fixture_assert(hit.h>=sdl_ui_min_tap_px());
        fixture_assert(sdl_question_menu_handle_pointer(hit.x+hit.w/2,hit.y+hit.h/2,UI_MENU_CLICK_PRIMARY));
        int choice=-1,action=0;fixture_assert(ui_menu_click_take_action(&choice,&action));
        fixture_assert(choice==i && action==UI_MENU_CLICK_PRIMARY);
    }
    sdl_question_menu_clear();ui_menu_click_clear();return 1;
}
'''
 checks+=caller+'\nstatic void actual_intro(void){fixture_assert(!main_menu_blitz_confirm_mobile());}\n'
 question.HARNESS=question.HARNESS.replace('int main(int argc,char **argv)',checks+'\nint main(int argc,char **argv)')
 question.HARNESS=question.HARNESS.replace('short_question();','actual_intro();')
 question.HARNESS=question.HARNESS.replace('Short question','Blitz intro question')
 question.HARNESS=question.HARNESS.replace('Off/On natural height, Help paging, IDs','Off/On short confirmation, answer reachability, IDs')
 question.OUT=ROOT/'scripts/output/phone-blitz-intro-check'
 question.main()

if __name__=='__main__':main()
