#!/usr/bin/env python3
"""Render and tap tale/intro footer labels in normal and large text."""
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
            SDL_FRect glyph;
            float next_x,next_y;
            if(big) {
                glyph=creation_find(strchr(prompts[i],'*')+3);
                SDL_FRect next=g_sdl_tale_screen.prompt_next_rect;
                next_x=next.x+next.w*.5f;
                next_y=next.y+next.h*.5f;
            } else {
                SDL_FRect full=creation_find(prompts[i]);
                sdl_tale_layout_metrics metrics; fixture_assert(sdl_tale_screen_metrics(&metrics,&canvas));
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
        "normal/large tale and intro visible skip/continue touch targets")
    fixture.main()


if __name__ == "__main__":
    main()
