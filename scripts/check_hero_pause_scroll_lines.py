#!/usr/bin/env python3
"""Exercise production phone hero and pause-text scroll boundaries.

Captures rendered text and its SDL clip while scrolling both viewports at
physical phone sizes. Verifies complete glyphs, readable first/last content,
page-navigation coverage, and retained hero detail hit targets.
"""
from pathlib import Path

import check_phone_books_layout as fixture

ROOT = Path(__file__).resolve().parents[1]
fixture.OUT = ROOT / "scripts/output/hero-pause-scroll-lines"


def once(source, before, after):
    assert source.count(before) == 1, f"fixture anchor changed: {before[:80]}"
    return source.replace(before, after, 1)


def harness():
    source = fixture.HARNESS
    source = once(source,
        'sdl_character_sheet_screen_set_select_description("A mighty spirit, maker of Silmarils, you carry this tale into darkness. Through fire and grief the long history unfolds. ENDLORE");',
        'sdl_character_sheet_screen_set_select_description("You rise aflame, spirit of fire, maker of Silmarils, whose fëa kindled wrath undying.\\n\\nA mighty spirit carries this tale into darkness. Through fire and grief the long history unfolds. ENDLORE");')
    source = once(source,
        '        if(r->clipped) {\n            expect(r->clip.y+r->clip.h<=footer_top+1,"body clip intersects footer");',
        r'''        if(r->clipped) {
            if(sdl_character_sheet_screen_mobile_carousel_active()) {
                bool intersects=r->ink.y<r->clip.y+r->clip.h && r->ink.y+r->ink.h>r->clip.y;
                if(intersects && (r->ink.y<r->clip.y-.51f || r->ink.y+r->ink.h>r->clip.y+r->clip.h+.51f))
                    fprintf(stderr,"clipped hero [%s]: ink %.1f..%.1f, viewport %d..%d\n",
                        r->text,r->ink.y,r->ink.y+r->ink.h,r->clip.y,r->clip.y+r->clip.h);
                expect(!intersects || (r->ink.y>=r->clip.y-.51f && r->ink.y+r->ink.h<=r->clip.y+r->clip.h+.51f),
                    "hero glyph crosses scroll boundary");
            }
            expect(r->clip.y+r->clip.h<=footer_top+1,"body clip intersects footer");''')
    source = once(source, 'int main(int argc,char **argv)\n{', r'''
static void hero_scroll_lines(void)
{
    SDL_strlcpy(fixture_id,"hero-scroll-lines",sizeof(fixture_id));reached[0]=0;
    ui_menu_click_begin();hero(0);frame();
    int maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
    expect(maximum>0,"hero detail did not overflow");
    capture("hero-first");
    int step=MAX(1,(int)(g_sdl_character_sheet_screen.last_body_line_h/3));
    for(int offset=1;offset<maximum;offset+=step) {
        g_sdl_character_sheet_screen.sheet_scroll=offset;frame();
        if(offset<step*2) capture("hero-partial-scroll");
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            SDL_FRect r=g_sdl_character_sheet_screen.hits[i].rect;
            SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
            if(g_sdl_character_sheet_screen.hits[i].choice>=SDL_CHAR_SHEET_INFO_CHOICE_BASE)
                expect(r.y>=view.y-.01f && r.y+r.h<=view.y+view.h+.01f,
                    "hero detail hit leaves viewport");
        }
    }
    g_sdl_character_sheet_screen.sheet_scroll=maximum;frame();capture("hero-last");
    expect(strstr(reached,"Artifice") && strstr(reached,"Jeweler"),"hero abilities unreachable");
    expect(strstr(reached,"Trait 9 Affinity") && strstr(reached,"ENDLORE"),"hero final traits or lore unreachable");
    sdl_character_sheet_screen_hide();ui_menu_click_clear();

    /* A long selectable trait spans multiple wrapped lines.  When its first
     * line is above the viewport, a complete continuation must remain tappable. */
    ui_menu_click_begin();hero(0);
    sdl_character_sheet_screen_add_select_detail(
        "Beginningalpha is followed by a long sequence of distinct words describing an unusually elaborate hero trait with many aspects and consequences for the adventure, including lore, choices, strengths, weaknesses, and a final identifiable conclusion.",
        TERM_GREEN,"Wrapped trait details");
    sdl_character_sheet_screen_set_select_description(
        "A hero awakens.\n\nThe ancient history of the kingdoms stretches across many ages and distant lands. "
        "Their makers journeyed through exile, hardship and conflict, carrying their hopes into the darkness. "
        "Their descendants remembered the cities and the people they loved. "
        "The ancient history of the kingdoms stretches across many ages and distant lands. "
        "Their makers journeyed through exile, hardship and conflict, carrying their hopes into the darkness. "
        "Their descendants remembered the cities and the people they loved.");
    fixture_assert(sdl_character_sheet_screen_commit_select(0));frame();
    maximum=g_sdl_character_sheet_screen.sheet_scroll_max;
    bool continuation_checked=false;
    for(int offset=0;offset<=maximum;offset+=MAX(1,step/2)) {
        g_sdl_character_sheet_screen.sheet_scroll=offset;frame();
        for(int i=0;i<g_debug_sheet_row_count;i++) {
            const sdl_debug_sheet_row *first=&g_debug_sheet_rows[i];
            if(first->choice<0 || first->group_rows<2 || !strstr(first->text,"Beginningalpha")) continue;
            bool first_visible=false, continuation_visible=false;
            for(int r=0;r<record_count;r++) {
                if(!strcmp(records[r].text,first->text)) first_visible=true;
                for(int j=1;j<first->group_rows && i+j<g_debug_sheet_row_count;j++)
                    if(!strcmp(records[r].text,g_debug_sheet_rows[i+j].text)) continuation_visible=true;
            }
            if(!first_visible && continuation_visible) {
                bool hit=false;
                for(int h=0;h<g_sdl_character_sheet_screen.hit_count;h++)
                    if(g_sdl_character_sheet_screen.hits[h].choice==first->choice) hit=true;
                expect(hit,"visible wrapped continuation lost its tap target");
                continuation_checked=true;
            }
        }
    }
    expect(continuation_checked,"wrapped continuation test did not reach the boundary");
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
static const char* pause_lines[] = {
    "Verse 01", "Verse 02", "Verse 03", "Verse 04", "Verse 05", "Verse 06",
    "Verse 07", "Verse 08", "Verse 09", "Verse 10", "Verse 11", "Verse 12",
    "Verse 13", "Verse 14", "Verse 15", "Verse 16", "Verse 17", "Verse 18",
    "Verse 19", "Verse 20", "Verse 21", "Verse 22", "Verse 23", "Verse 24",
    "Verse 25", "Verse 26", "Verse 27", "Verse 28", "Verse 29", "Verse 30",
    "Verse 31", "Verse 32"
};
static bool pause_seen[N_ELEMENTS(pause_lines)];
static void pause_frame(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    record_count=0;recording=true;
    sdl_pause_text_screen_render_canvas(&canvas);recording=false;
    for(int i=0;i<record_count;i++) {
        text_record *r=&records[i];
        if(!r->clipped) continue;
        bool intersects=r->ink.y<r->clip.y+r->clip.h && r->ink.y+r->ink.h>r->clip.y;
        if(intersects && (r->ink.y<r->clip.y-.51f || r->ink.y+r->ink.h>r->clip.y+r->clip.h+.51f))
            fprintf(stderr,"clipped pause [%s]: ink %.1f..%.1f, viewport %d..%d\n",
                r->text,r->ink.y,r->ink.y+r->ink.h,r->clip.y,r->clip.y+r->clip.h);
        expect(!intersects || (r->ink.y>=r->clip.y-.51f && r->ink.y+r->ink.h<=r->clip.y+r->clip.h+.51f),
            "pause glyph crosses scroll boundary");
        for(int j=0;j<(int)N_ELEMENTS(pause_lines);j++)
            if(!strcmp(r->text,pause_lines[j])
                && r->ink.y>=r->clip.y-.51f && r->ink.y+r->ink.h<=r->clip.y+r->clip.h+.51f)
                pause_seen[j]=true;
    }
}
static void pause_scroll_lines(void)
{
    SDL_strlcpy(fixture_id,"pause-scroll-lines",sizeof(fixture_id));
    fixture_assert(sdl_pause_text_screen_begin());
    for(int i=0;i<(int)N_ELEMENTS(pause_lines);i++)
        sdl_pause_text_screen_add_line(pause_lines[i],TERM_WHITE,0);
    sdl_pause_text_screen_set_visible_lines(N_ELEMENTS(pause_lines));
    memset(pause_seen,0,sizeof(pause_seen));pause_frame();capture("pause-first");
    float maximum=g_sdl_standalone_pager.maximum;
    expect(maximum>0,"pause text did not overflow");
    for(int offset=1;offset<(int)maximum;offset+=13) {
        g_sdl_standalone_pager.offset=offset;pause_frame();
        if(offset==1) capture("pause-partial-scroll");
    }
    g_sdl_standalone_pager.offset=maximum;pause_frame();capture("pause-last");
    for(int i=0;i<(int)N_ELEMENTS(pause_lines);i++)
        expect(pause_seen[i],"pause line unreachable while dragging");
    memset(pause_seen,0,sizeof(pause_seen));
    g_sdl_standalone_pager.offset=0;pause_frame();
    SDL_FRect next=g_sdl_standalone_pager.buttons[1];
    expect(inside(next) && next.w>=sdl_ui_min_tap_px() && next.h>=sdl_ui_min_tap_px(),
        "pause Next target is outside the screen or too small");
    expect(sdl_standalone_screen_handle_pointer(next.x+next.w*.5f,next.y+next.h*.5f,
        UI_MENU_CLICK_PRIMARY) && g_sdl_standalone_pager.offset>0,
        "pause Next tap did not advance");
    g_sdl_standalone_pager.offset=0;pause_frame();
    int pages=0;
    while(g_sdl_standalone_pager.offset<g_sdl_standalone_pager.maximum-.01f) {
        float old=g_sdl_standalone_pager.offset;
        expect(sdl_standalone_screen_handle_key(']'),"pause Next unhandled");pause_frame();
        expect(g_sdl_standalone_pager.offset>old,"pause Next did not advance");
        fixture_assert(++pages<100);
    }
    for(int i=0;i<(int)N_ELEMENTS(pause_lines);i++) {
        if(!pause_seen[i]) fprintf(stderr,"page navigation skipped [%s]\n",pause_lines[i]);
        expect(pause_seen[i],"pause line skipped by Next");
    }
    sdl_pause_text_screen_hide();
}
int main(int argc,char **argv)
{''')
    source = once(source,
        '    check_books(false);check_books(true);check_hero();check_allocation();',
        '    hero_scroll_lines();pause_scroll_lines();')
    source = once(source, 'books/hero/allocation %s', 'hero/pause scroll %s')
    return source


def main():
    fixture.HARNESS = harness()
    fixture.main()


if __name__ == "__main__":
    main()
