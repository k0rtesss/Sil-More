#!/usr/bin/env python3
"""Exercise pixel wrapping and raster bounds of the proportional overlay log."""
import check_menu_button_log_overlap as base

base.OUT = base.ROOT / "scripts/output/overlay-log-wrapping"
base.EXTRA_MOBILE_SOURCES = ["src/sdl/render/sdl-text.c"]
base.CHECK = r'''
static void check_wrap(void)
{
    static const char *messages[] = {
        "Character achievement available: The Oath He Should Have Sworn. "
            "As Fëanor, escape with a Silmaril and an unbroken Oath of Mercy.",
        "Character achievement available: His Father's Light. "
            "As Fëanor, use Finwë's lamp to lift the curse on Fëanor's crown."
    };
    sdl_view *log=&g_views[PANE_ROLLS];
    pane_config_count=1;
    pane_config[0]=(struct pane_config){.pane=PANE_ROLLS,
        .where=PLACE_TOP_RIGHT,.enabled=true,.rect.rows=8};
    config.bigger_font=false;
    for(int cell_h=20;cell_h<=50;cell_h+=2) {
        log->cell_h=cell_h; log->cell_w=cell_h/2; log->cols=width/log->cell_w;
        log->rows=8; assert(term_init(&log->t,log->cols,log->rows,32)==0);
        log->t.data=(void *)(uintptr_t)PANE_ROLLS; log->term_ready=true;
        Term_activate(&log->t);
        TTF_Font *font=sdl_story_font_for_view_slot(log,SDL_STORY_FONT_SLOT_LOG);
        assert(font);
        float budget=(float)(log->cols-sdl_overlay_log_left_margin(log->cols))*log->cell_w
            -SDL_OVERLAY_LOG_TEXT_LEFT_PAD(log)-SDL_OVERLAY_LOG_TEXT_RIGHT_PAD(log);
        for(int m=0;m<N_ELEMENTS(messages);m++) {
            const char *message=messages[m];
            int offsets[8], lengths[8], pos=0;
            int n=sdl_overlay_log_wrap(message,8,offsets,lengths);
            assert(n>0);
            for(int i=0;i<n;i++) {
                char segment[256]; int tw=0,th=0;
                assert(offsets[i]==pos && lengths[i]>0 && lengths[i]<96);
                assert(utf8_safe_prefix_len(message+offsets[i],lengths[i])==lengths[i]);
                memcpy(segment,message+offsets[i],lengths[i]); segment[lengths[i]]='\0';
                assert(sdl_ui_text_texture(font,segment,(SDL_Color){255,255,255,255},&tw,&th));
                assert((float)tw*cell_h/th<=budget+1.0f);
                pos=offsets[i]+lengths[i];
                while(message[pos]==' ') pos++;
            }
            assert(pos==(int)strlen(message));
        }
        term_nuke(&log->t); log->term_ready=false; Term=NULL;
    }
    puts("Overlay log: both achievement messages preserve UTF-8 and every segment fits the rendered band at 16 font sizes PASS");
}

int main(int argc,char **argv)
{
    assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)); assert(TTF_Init());
    static maxima limits; static player_type player; static player_other options;
    static object_type items[INVEN_TOTAL];
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.window=SDL_CreateWindow("Overlay log wrapping",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window); g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer);
    check_wrap();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''

if __name__ == "__main__":
    base.main()
