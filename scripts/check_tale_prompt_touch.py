#!/usr/bin/env python3
"""Check complete tale/intro captions and footer taps in normal and large text."""
from pathlib import Path
import check_big_text_creation as fixture

CHECK = r'''
static void creation_intro_skip(void)
{
    static cptr prompts[]={"[Tap to finish passage]  *  [Skip intro]",
        "[Tap to turn the page]  *  [Skip intro]",
        "[Tap to continue]  *  [Back] fast forward"};
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    for(int big=0;big<2;big++) for(int manuscript=0;manuscript<2;manuscript++) {
        config.bigger_font=big;
        for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=big;
        fixture_assert(sdl_tale_screen_begin("THE NAMELESS CHRONICLE"));
        sdl_tale_screen_set_manuscript(manuscript);
        sdl_tale_screen_add_entry("","You awaken in darkness. No name. No memory.");
        sdl_tale_screen_set_active_entry(0,255);
        for(int i=0;i<(int)N_ELEMENTS(prompts);i++) {
            sdl_tale_screen_set_prompt(prompts[i],true,false);
            creation_begin("intro-skip-target"); sdl_tale_screen_render_canvas(&canvas); stop_capture();
            sdl_tale_layout_metrics metrics; fixture_assert(sdl_tale_screen_metrics(&metrics,&canvas));
            SDL_FRect title=creation_find("THE NAMELESS CHRONICLE");
            int title_w=0,title_h=0;
            fixture_assert(TTF_GetStringSizeWrapped(metrics.title_font,
                "THE NAMELESS CHRONICLE",0,0,&title_w,&title_h));
            if(big || title_w>metrics.column_w) {
                TTF_HorizontalAlignment previous=TTF_GetFontWrapAlignment(metrics.title_font);
                TTF_SetFontWrapAlignment(metrics.title_font,manuscript
                    ?TTF_HORIZONTAL_ALIGN_CENTER:TTF_HORIZONTAL_ALIGN_LEFT);
                fixture_assert(TTF_GetStringSizeWrapped(metrics.title_font,
                    "THE NAMELESS CHRONICLE",0,(int)metrics.column_w,&title_w,&title_h));
                TTF_SetFontWrapAlignment(metrics.title_font,previous);
            }
            capture(manuscript?"intro-caption":"tale-caption");
            /* Compare with the full shaped texture: a clipped source rectangle
             * can stay inside the panel while silently dropping the last letters. */
            if(SDL_fabsf(title.w-title_w)>1 || SDL_fabsf(title.h-title_h)>1)
                fprintf(stderr,"caption manuscript=%d big=%d: drawn %.1fx%.1f, expected %dx%d, column %.1f\n",
                    manuscript,big,title.w,title.h,title_w,title_h,metrics.column_w);
            fixture_assert(SDL_fabsf(title.w-title_w)<=1 && SDL_fabsf(title.h-title_h)<=1);
            inside(title);
            fixture_assert(title.x>=metrics.column_x-1
                && title.x+title.w<=metrics.column_x+metrics.column_w+1);
            fixture_assert(title.y+title.h<=metrics.body_y);
            SDL_FRect glyph;
            float next_x,next_y;
            if(big) {
                glyph=creation_find(strchr(prompts[i],'*')+3);
                SDL_FRect next=g_sdl_tale_screen.prompt_next_rect;
                next_x=next.x+next.w*.5f;
                next_y=next.y+next.h*.5f;
            } else {
                SDL_FRect full=creation_find(prompts[i]);
                int total_w=0,prefix_w=0;
                fixture_assert(TTF_GetStringSize(metrics.prompt_font,prompts[i],0,&total_w,NULL));
                fixture_assert(TTF_GetStringSize(metrics.prompt_font,prompts[i],
                    (size_t)(strchr(prompts[i],'*')-prompts[i])+1,&prefix_w,NULL));
                float prefix=full.w*(float)prefix_w/(float)total_w;
                glyph=(SDL_FRect){full.x+prefix,full.y,full.w-prefix,full.h};
                next_x=full.x+prefix*.5f;
                next_y=full.y+full.h*.5f;
            }
            inside(glyph);
            char key=0;
            /* Tap each part of the visibly drawn skip label, including the
             * left edge that falls outside an unscaled prefix measurement. */
            for(int tap=0;tap<3;tap++) {
                fixture_assert(sdl_tale_screen_handle_pointer(glyph.x+glyph.w*(.2f+.3f*tap),glyph.y+glyph.h*.5f));
                fixture_assert(Term_inkey(&key,false,true)==0 && key==ESCAPE);
            }
            fixture_assert(sdl_tale_screen_handle_pointer(next_x,next_y));
            fixture_assert(Term_inkey(&key,false,true)==0 && key=='\r');
            capture("intro-skip-target");
        }
        sdl_tale_screen_hide();
    }
    config.bigger_font=true;
    for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=true;
}
'''


def main():
    fixture.OUT = Path(__file__).resolve().parents[1] / "scripts/output/tale-prompt-touch-check"
    fixture.HARNESS = fixture.HARNESS.replace(
        "static void creation_hero_drag(void)",
        CHECK + "\nstatic void creation_hero_drag(void)")
    fixture.HARNESS = fixture.HARNESS.replace(
        "creation_characters(); creation_skills(); creation_lamp(); creation_tale(); creation_hero_drag();",
        "creation_intro_skip();")
    fixture.HARNESS = fixture.HARNESS.replace(
        "compact stats, aligned calculations, lamp/footer containment, tale actions, hero touch/mouse drag",
        "complete normal/large tale and intro captions, visible skip/continue touch targets")
    fixture.main()


if __name__ == "__main__":
    main()
