#!/usr/bin/env python3
"""Check race-book fit caching and phone story split equivalence offscreen.

Reuses the production SDL renderer, fonts and isolated phone layout fixture.
"""
import check_phone_books_layout as base

CHECK = r'''
/* Brute-force reference: each candidate triple is independently measured. */
static bool reference_split(cptr intro, TTF_Font* font, float width,
    int head, int tail, char* first, char* second, char* third, cptr* fourth)
{
    size_t cuts[32], best_a=0, best_b=0, best_c=0;
    int count=sdl_char_sheet_collect_intro_breaks(intro,cuts,32), best=-1;
    for(int a=0;a<count-2;a++) for(int b=a+1;b<count-1;b++)
        for(int c=b+1;c<count;c++) {
            char one[2048], two[2048], three[2048];
            cptr four=sdl_char_sheet_skip_story_ws(intro+cuts[c]);
            sdl_char_sheet_copy_story_slice(intro,cuts[a],one,sizeof(one));
            sdl_char_sheet_copy_story_slice(intro+cuts[a],cuts[b]-cuts[a],two,sizeof(two));
            sdl_char_sheet_copy_story_slice(intro+cuts[b],cuts[c]-cuts[b],three,sizeof(three));
            if(!one[0] || !two[0] || !three[0] || !four[0]) continue;
            int x=head+sdl_char_sheet_story_lines(font,one,width);
            int y=sdl_char_sheet_story_lines(font,two,width);
            int z=sdl_char_sheet_story_lines(font,three,width);
            int w=tail+sdl_char_sheet_story_lines(font,four,width);
            int total=x+y+z+w;
            int score=(MAX(MAX(x,y),MAX(z,w))-MIN(MIN(x,y),MIN(z,w)))*100
                + ABS(x*4-total)+ABS(y*4-total)+ABS(z*4-total)+ABS(w*4-total);
            if(best<0 || score<best) {best=score;best_a=cuts[a];best_b=cuts[b];best_c=cuts[c];}
        }
    if(best<0) return false;
    sdl_char_sheet_copy_story_slice(intro,best_a,first,2048);
    sdl_char_sheet_copy_story_slice(intro+best_a,best_b-best_a,second,2048);
    sdl_char_sheet_copy_story_slice(intro+best_b,best_c-best_b,third,2048);
    *fourth=sdl_char_sheet_skip_story_ws(intro+best_c);
    return first[0] && second[0] && third[0] && (*fourth)[0];
}

static void check_layout_cache(void)
{
    static cptr samples[] = {birth_intro_lore,
        "First sentence!  Second sentence? Third sentence.\n\n"
        "A longer fourth sentence with some words. Fifth. Sixth! Seventh?"};
    bool saved_bigger=config.bigger_font;
    bool saved_menus[SDL_MENU_FONT_COUNT];
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) saved_menus[menu]=config.menu_bigger_font[menu];
    config.bigger_font=false;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=false;
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_select(0,"Cache fixture");
    sdl_character_sheet_screen_set_select_intro(birth_intro_lore);
    sdl_character_sheet_screen_set_select_frame(birth_frame_top,birth_frame_bottom);
    sdl_character_sheet_screen_add_select_row(0,"First",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_row(1,"Second",TERM_WHITE,"");
    sdl_character_sheet_screen_add_select_description_candidate("The first selected description.");
    sdl_character_sheet_screen_add_select_description_candidate("The second selected description, with longer words.");
    sdl_character_sheet_screen_set_select_description("The first selected description.");
    fixture_assert(sdl_character_sheet_screen_commit_select(0));
    sdl_char_sheet_fitted_wrap_cache_clear();
    int choice=sdl_char_sheet_cached_book_body_px(900,600,20,840,32,true);
    fixture_assert(choice>0 && g_sdl_character_sheet_screen.select_book_body_px==0);
    int story=sdl_char_sheet_cached_book_body_px(900,600,20,840,32,false);
    fixture_assert(story>0 && g_sdl_character_sheet_screen.select_book_choice_body_px==choice);

    TTF_Font *a=sdl_story_font_for_height_slot(21,SDL_STORY_FONT_SLOT_CHAR_SELECT);
    TTF_Font *b=sdl_story_font_for_height_slot(23,SDL_STORY_FONT_SLOT_CHAR_SELECT);
    fixture_assert(a && b && a!=b);
    (void)sdl_char_sheet_select_description_line_count(a,"",300);
    (void)sdl_char_sheet_select_description_line_count(b,"",300);
    sdl_char_sheet_fitted_wrap_cache_clear_font(a);
    bool has_a=false,has_b=false;
    for(int i=0;i<FITTED_WRAP_CACHE_SIZE;i++) {
        has_a |= g_fitted_wrap_cache[i].font==a;
        has_b |= g_fitted_wrap_cache[i].font==b;
    }
    fixture_assert(!has_a && has_b);
    fixture_assert(g_sdl_character_sheet_screen.select_book_body_px==story);
    fixture_assert(g_sdl_character_sheet_screen.select_book_choice_body_px==choice);

    /* Force the real font cache to evict entries. Cached sizes do not borrow
     * font pointers and must survive these evictions. A full reset must clear. */
    for(int px=1;px<=MAX_STORY_FONT_CACHE+10;px++)
        fixture_assert(sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_CHAR_SELECT));
    fixture_assert(g_sdl_character_sheet_screen.select_book_body_px==story);
    fixture_assert(g_sdl_character_sheet_screen.select_book_choice_body_px==choice);
    sdl_story_font_cache_clear();
    fixture_assert(g_sdl_character_sheet_screen.select_book_body_px==0);
    fixture_assert(g_sdl_character_sheet_screen.select_book_choice_body_px==0);

    for(int text=0;text<2;text++) for(int px=14;px<=26;px+=12)
        for(int width=120;width<=480;width*=2) {
            char a1[2048],a2[2048],a3[2048],b1[2048],b2[2048],b3[2048];
            cptr a4="",b4="";
            TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_CHAR_SELECT);
            bool old=reference_split(samples[text],font,(float)width,2,3,b1,b2,b3,&b4);
            bool now=sdl_char_sheet_split_intro_four_pages_balanced(samples[text],font,(float)width,
                2,3,a1,sizeof(a1),a2,sizeof(a2),a3,sizeof(a3),&a4);
            fixture_assert(old==now);
            if(old) fixture_assert(streq(a1,b1) && streq(a2,b2) && streq(a3,b3) && streq(a4,b4));
        }
    /* Callers without candidate descriptions must also invalidate the fit
     * when the selected description itself changes. */
    sdl_character_sheet_screen_begin_select(0,"No candidate fixture");
    sdl_character_sheet_screen_set_select_intro(birth_intro_lore);
    sdl_character_sheet_screen_add_select_row(0,"First",TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_description("A short description.");
    Uint64 before=sdl_char_sheet_book_layout_hash();
    sdl_character_sheet_screen_set_select_description("A different, longer description.");
    fixture_assert(before!=sdl_char_sheet_book_layout_hash());
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
    sdl_story_font_cache_clear();
    config.bigger_font=saved_bigger;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=saved_menus[menu];
    puts("Race fit is lazy, font eviction is selective, four-page splits match brute force: PASS");
}
'''


def main():
    harness = base.HARNESS
    harness = harness.replace('static void check_books(bool narrative)',
                              CHECK + '\nstatic void check_books(bool narrative)', 1)
    harness = harness.replace('    check_books(false);check_books(true);check_hero();check_allocation();',
        '    if(fixture_width==720 && fixture_height==1600) check_layout_cache();\n'
        '    check_books(false);check_books(true);check_hero();check_allocation();', 1)
    base.HARNESS = harness
    base.OUT = base.ROOT / "scripts/output/birth-selection-layout-cache"
    base.main()


if __name__ == "__main__":
    main()
