#!/usr/bin/env python3
"""Measure populated log text against the production native Exit action.

Uses real phone density, BODY mono cells, the actual log wrapping/layout and
native action renderer. Actual command scrolling is tested by the C caller test.
"""
from pathlib import Path
import check_phone_books_layout as books
from check_phone_about_blitz_layout import function

ROOT=Path(__file__).resolve().parents[1]
def main():
 source=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
 helpers=function(source,'static bool log_history_wrap_next(')+function(source,'static void log_history_layout(')
 checks=r'''
static void check_log_footer(void)
{
    SDL_strlcpy(fixture_id,"populated-log-footer",sizeof(fixture_id));
    sdl_view *view=&g_views[PANE_MAIN];
    int px=sdl_ui_role_font_px(SDL_UI_FONT_BODY);
    view->term_ready=true;view->cell_h=px;view->cell_w=MAX(1,px/2);
    view->cols=fixture_width/view->cell_w;view->rows=fixture_height/view->cell_h;
    view->rect=(SDL_Rect){0,0,fixture_width,fixture_height};
    Term_resize(view->cols,view->rows);
    view->font_atlas=sdl_load_ttf_font_cells(config.monospace_font,view->cell_w,view->cell_h,NULL);
    fixture_assert(view->font_atlas);view->font_atlas_cell_w=view->cell_w;view->font_atlas_cell_h=view->cell_h;
    int top,bottom,prompt;log_history_layout(view->rows,&top,&bottom,&prompt);
    expect(top<=bottom && bottom<prompt,"log body does not leave hint row");
    ui_menu_click_begin();ui_menu_click_set_touch_exit_button(true);
    sdl_touch_menu_button_layout_entry buttons[SDL_TOUCH_MENU_BUTTON_MAX];
    int count=sdl_touch_menu_button_layout(buttons,N_ELEMENTS(buttons));fixture_assert(count==1);
    SDL_FRect exit=buttons[0].rect;
    expect(inside(exit),"Exit outside physical screen");
    expect(exit.w+1>=sdl_ui_min_tap_px() && exit.h+1>=sdl_ui_min_tap_px(),"Exit below48dp");
    expect((prompt+1)*view->cell_h<=exit.y+1,"hint row intersects Exit");
    const char*entries[]={"FIRST A very long recorded message with all words and numbers 12345 retained. A very long recorded message with all words and numbers 12345 retained. A very long recorded message with all words and numbers 12345 retained. LASTTAIL",
        "Combat @ to o att +14 hit 7 evn +8 damage (2d7+1d5) net 12 protection 3 COMBATTAIL"};
    for(int entry=0;entry<2;entry++) {
    cptr text=entries[entry];
    char lines[64][256];int total=0;while(log_history_wrap_next(&text,view->cols,lines[total],sizeof(lines[0])))total++;
    int capacity=bottom-top+1;char reached[8192]="";
    for(int start=0;start<=MAX(0,total-capacity);start++) {
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        int rows=MIN(capacity,total-start);
        for(int i=0;i<rows;i++) {
            int row=bottom-rows+1+i;
            expect((row+1)*view->cell_h<=prompt*view->cell_h,"populated mono glyph cells intersect hint/footer");
            sdl_render_mono_text(view,0,row,strlen(lines[start+i]),lines[start+i],(SDL_Color){255,255,255,255});
            SDL_strlcat(reached,lines[start+i],sizeof(reached));
        }
        cptr hint="Swipe tabs, drag to scroll";
        if(strlen(hint)>view->cols)hint="Tap a tab to filter";
        sdl_render_mono_text(view,0,prompt,strlen(hint),hint,(SDL_Color){150,150,150,255});
        expect((prompt+1)*view->cell_h<=exit.y+1,"hint mono glyph cells intersect Exit");
        sdl_touch_exit_button_render();
        if(!start)capture(entry?"combat-first":"log-first");
        if(start==MAX(0,total-capacity))capture(entry?"combat-tail":"log-tail");
    }
    expect(entry?strstr(reached,"Combat")&&strstr(reached,"COMBATTAIL"):
        strstr(reached,"FIRST")&&strstr(reached,"LASTTAIL"),"wrapped entry tail unreachable");
    }
    s32b before_turn=turn;int before_energy=p_ptr->energy_use;
    expect(sdl_touch_exit_button_handle_pointer(exit.x+exit.w/2,exit.y+exit.h/2),"Exit tap unhandled");
    char key=0;expect(Term_inkey(&key,false,true)==0&&key==ESCAPE,"Exit fails to dispatch Escape");
    expect(turn==before_turn&&p_ptr->energy_use==before_energy,"Exit consumes turn");
    ui_menu_click_clear();
    SDL_DestroyTexture(view->font_atlas);view->font_atlas=NULL;
}
'''
 books.HARNESS=books.HARNESS.replace('int main(int argc,char **argv)',helpers+checks+'\nint main(int argc,char **argv)')
 books.HARNESS=books.HARNESS.replace('check_books(false);check_books(true);check_hero();check_allocation();','check_log_footer();')
 books.HARNESS=books.HARNESS.replace('books/hero/allocation','Log footer')
 books.OUT=ROOT/'scripts/output/phone-log-footer-check'
 books.main()

if __name__=='__main__':main()
