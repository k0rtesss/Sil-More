#!/usr/bin/env python3
"""Render real phone text-panel geometry at measured IME viewport heights."""
from pathlib import Path
import sys
import check_phone_short_question as base

ROOT = Path(__file__).resolve().parents[1]
prefix = base.PREFIX.replace('#include "sdl/ui/sdl-question-menu.c"', r'''
static int ime_bottom;
static SDL_Rect fixture_text_visible(SDL_Rect rect)
{
    if(ime_bottom>rect.y) rect.h=MIN(rect.h,ime_bottom-rect.y);
    return rect;
}
#define sdl_text_input_visible_rect fixture_text_visible
#include "sdl/ui/sdl-question-menu.c"
#undef sdl_text_input_visible_rect
''')
CHECK = r'''
#define FIXTURE_RANDOM_NAME 0
#define FIXTURE_COMPACT_NAME 1
static int fixture_random_name;
static void check_string_panel(void)
{
    SDL_strlcpy(fixture_id,"string-panel",sizeof(fixture_id));
    /* Actual 420dpi AVD: portrait Gboard top857/1600, landscape253/720. */
    float measured=fixture_height>fixture_width?.535625f:253.0f/720.0f;
    int heights[]={0,(int)(fixture_height*measured+.5f),
        (int)(fixture_height*.535625f),(int)(fixture_height*.60f)};
    for(int keyboard=0;keyboard<4;keyboard++) {
        ime_bottom=heights[keyboard];
        ui_menu_click_begin(); sdl_question_menu_begin(fixture_random_name?
            (FIXTURE_COMPACT_NAME?"Name:":"Artifact name"):"Inscription:");
        sdl_question_menu_set_text_input(true);
        sdl_question_menu_add_entry(2001,"","Qa_",TERM_L_BLUE);
        sdl_question_menu_add_button(2002,"Confirm",TERM_L_WHITE);
        if(fixture_random_name) sdl_question_menu_add_button(2005,
            FIXTURE_COMPACT_NAME?"Random":"Random name",TERM_L_WHITE);
        sdl_question_menu_add_button(2003,"Clear",TERM_L_WHITE);
        sdl_question_menu_add_button(2004,"Cancel",TERM_SLATE);
        sdl_question_menu_set_highlight(2001); sdl_question_menu_finish();
        sdl_question_menu_layout_info layout;
        fixture_assert(sdl_question_menu_layout(&layout));
        if(fixture_random_name && layout.actions_in_list) {
            fixture_assert(layout.entries_rect.h>=layout.row_h);
            fixture_assert(layout.rows[0].y>=layout.entries_rect.y &&
                layout.rows[0].y+layout.rows[0].h<=layout.entries_rect.y+layout.entries_rect.h+1);
            int offset=0;sdl_question_menu_set_scroll_offset_target(&offset,false);
            int expected[]={2002,2005,2003,2004};
            for(int i=0;i<4;i++) {
                fixture_assert(sdl_question_menu_layout(&layout));
                offset+=(int)(layout.buttons[i].y-layout.entries_rect.y);
                fixture_assert(sdl_question_menu_layout(&layout));
                SDL_FRect hit;
                fixture_assert(SDL_GetRectIntersectionFloat(&layout.buttons[i],&layout.entries_rect,&hit));
                fixture_assert(hit.h>=layout.row_h-1);
                fixture_assert(sdl_question_menu_handle_pointer(hit.x+hit.w/2,hit.y+hit.h/2,UI_MENU_CLICK_PRIMARY));
                int choice=-1,action=0;fixture_assert(ui_menu_click_take_action(&choice,&action));
                fixture_assert(choice==expected[i]);
            }
            sdl_question_menu_clear();ui_menu_click_clear();continue;
        }
        fixture_assert(!layout.actions_in_list && layout.button_count==g_question_menu.button_count);
        fixture_assert(layout.visible_count==1);
        float bottom=ime_bottom?ime_bottom:fixture_height;
        fixture_assert(layout.panel.y>=0 && layout.panel.y+layout.panel.h<=bottom);
        fixture_assert(layout.rows[0].h>=sdl_ui_min_tap_px());
        if(layout.rows[0].y<layout.entries_rect.y)
            fprintf(stderr,"keyboard%d bottom%d panel%.0f,%.0f %.0fx%.0f title%.0fx%.0f entry%.0f row%.0f font%d tap%d\n",
                keyboard,ime_bottom,layout.panel.x,layout.panel.y,layout.panel.w,layout.panel.h,
                layout.title_row.w,layout.title_row.h,layout.entries_rect.y,layout.rows[0].y,
                layout.font_px,sdl_ui_min_tap_px());
        fixture_assert(layout.rows[0].y>=layout.entries_rect.y);
        fixture_assert(layout.rows[0].y+layout.rows[0].h<=layout.entries_rect.y+layout.entries_rect.h+1);
        fixture_assert(!SDL_HasRectIntersectionFloat(&layout.title_row,&layout.rows[0]));
        for(int i=0;i<layout.button_count;i++) {
            fixture_assert(layout.buttons[i].h>=sdl_ui_min_tap_px());
            fixture_assert(layout.buttons[i].w>=sdl_ui_min_tap_px());
            fixture_assert(layout.buttons[i].y+layout.buttons[i].h<=bottom);
            fixture_assert(!SDL_HasRectIntersectionFloat(&layout.buttons[i],&layout.rows[0]));
            fixture_assert(!SDL_HasRectIntersectionFloat(&layout.buttons[i],&layout.title_row));
            TTF_Font *body=sdl_story_font_for_height_slot(layout.font_px,SDL_STORY_FONT_SLOT_MENU);
            fixture_assert(sdl_question_menu_wrapped_text_height(body,
                g_question_menu.buttons[i].text,layout.buttons[i].w*.84f)<=layout.buttons[i].h);
            SDL_FRect font_button=sdl_menu_font_button_bounds_avoiding(
                sdl_question_menu_header_controls(&layout));
            fixture_assert(!SDL_HasRectIntersectionFloat(&layout.buttons[i],&font_button));
            fixture_assert(sdl_question_menu_handle_pointer(layout.buttons[i].x+layout.buttons[i].w/2,
                layout.buttons[i].y+layout.buttons[i].h/2,UI_MENU_CLICK_PRIMARY));
            int choice=-1,action=0;
            fixture_assert(ui_menu_click_take_action(&choice,&action));
            int expected[]={2002,2003,2004};int random_expected[]={2002,2005,2003,2004};
            fixture_assert(choice==(fixture_random_name?random_expected[i]:expected[i]) && action==UI_MENU_CLICK_PRIMARY);
        }
        TTF_Font *title=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_TITLE),SDL_STORY_FONT_SLOT_DEFAULT);
        if(config.bigger_font && !fixture_random_name) {
            float natural=sdl_question_menu_text_width(title,"Inscription:",sdl_ui_role_font_px(SDL_UI_FONT_TITLE));
            if(natural<=layout.panel.w-36) fixture_assert(layout.title_row.w>=natural);
            fixture_assert(layout.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
        }
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        sdl_question_menu_render();
        char name[40];strnfmt(name,sizeof(name),"string-visible-%d",ime_bottom);
        capture(name);
        if(layout.inline_text) {
            cptr long_text="A long Fëanor inscription with enough characters to wrap several rows_";
            SDL_strlcpy(g_question_menu.entries[0].text,long_text,sizeof(g_question_menu.entries[0].text));
            TTF_Font *body=sdl_story_font_for_height_slot(layout.font_px,SDL_STORY_FONT_SLOT_MENU);
            cptr tail=sdl_question_menu_inline_text_tail(body,long_text,layout.rows[0].w);
            fixture_assert(tail>=long_text && tail[strlen(tail)-1]=='_');
            if(sdl_question_menu_text_width(body,long_text,layout.font_px)>layout.rows[0].w)
                fixture_assert(tail>long_text);
            fixture_assert(((unsigned char)*tail&0xc0)!=0x80);
            fixture_assert(sdl_question_menu_wrapped_text_height(body,tail,layout.rows[0].w)<=layout.row_h);
            footer_capture=true;footer_glyph_count=0;sdl_question_menu_render();footer_capture=false;
            bool field_ink=false;
            for(int i=0;i<footer_glyph_count;i++) {
                SDL_FRect glyph=footer_glyphs[i];
                if(SDL_HasRectIntersectionFloat(&glyph,&layout.rows[0])) {
                    field_ink=true;
                    fixture_assert(glyph.x>=layout.rows[0].x && glyph.y>=layout.rows[0].y);
                    fixture_assert(glyph.x+glyph.w<=layout.rows[0].x+layout.rows[0].w+1);
                    fixture_assert(glyph.y+glyph.h<=layout.rows[0].y+layout.rows[0].h+1);
                }
            }
            fixture_assert(field_ink && streq(g_question_menu.entries[0].text,long_text));
            capture("string-long-tail");
        }
        sdl_question_menu_clear();ui_menu_click_clear();
    }
}
'''

def main():
    base.OUT = ROOT/'scripts/output/mobile-string-panel-check'
    source_prefix=prefix
    if '--ignore-ime' in sys.argv:
        # Isolated negative: the unchanged bounds assertions must reject
        # a renderer that fails to use the measured keyboard-visible frame.
        source_prefix=source_prefix.replace('if(ime_bottom>rect.y)', 'if(false)')
    if '--stacked' in sys.argv or '--wrapped-field' in sys.argv:
        # Isolated production negative: retain the previous stacked layout.
        base.OUT.mkdir(parents=True,exist_ok=True)
        question=(ROOT/'src/sdl/ui/sdl-question-menu.c').read_text(encoding='utf-8-sig')
        if '--stacked' in sys.argv:
            question=question.replace('if (g_question_menu.text_input && keyboard_clipped',
                                      'if (false && g_question_menu.text_input && keyboard_clipped',1)
        else:
            question=question.replace('if (layout.inline_text)', 'if (false)',1)
        baseline=base.OUT/'stacked-question.c';baseline.write_text(question,encoding='utf-8')
        source_prefix=source_prefix.replace('#include "sdl/ui/sdl-question-menu.c"',
                                            '#include "'+baseline.as_posix()+'"')
    checks=CHECK.replace('#define FIXTURE_RANDOM_NAME 0','#define FIXTURE_RANDOM_NAME 1') if '--random-name' in sys.argv else CHECK
    if '--legacy-name-labels' in sys.argv:
        checks=checks.replace('#define FIXTURE_COMPACT_NAME 1','#define FIXTURE_COMPACT_NAME 0')
    base.HARNESS = source_prefix+checks+base.INIT+r'''
    for(int random=FIXTURE_RANDOM_NAME;random<2;random++) {
      fixture_random_name=random;
      for(int big=0;big<2;big++) {
        config.bigger_font=big;
        for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=big;
        check_string_panel();
      }
    }
    puts("String panel: actual font, visible-frame bounds, minimum targets, action IDs: PASS");
    sdl_ui_text_cache_clear();sdl_story_font_cache_clear();term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();SDL_Quit();
}
'''
    base.main(cases=[(720,1600,2),(720,1600,2.625),
                     (1080,2340,2.75),(1080,2400,2.625)])

if __name__=='__main__':main()
