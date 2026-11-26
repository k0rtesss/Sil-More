#!/usr/bin/env python3
"""Exercise pixel wrapping and raster bounds of the proportional overlay log."""
import check_menu_button_log_overlap as base

base.OUT = base.ROOT / "scripts/output/overlay-log-wrapping"
base.EXTRA_MOBILE_SOURCES = ["src/sdl/render/sdl-text.c"]
base.EXTRA_WRAPS = ["TTF_MeasureString", "pane_log_combat_row_tokens"]
base.CHECK = r'''
static int measures;
static bool test_roll;
static const combat_roll_token roll[] = {
    {.text="+17", .attr=TERM_WHITE}, {.text=" vs ", .attr=TERM_SLATE},
    {.text="12", .attr=TERM_L_RED}
};
bool __real_TTF_MeasureString(TTF_Font*, const char*, size_t, int, int*, size_t*);
bool __wrap_TTF_MeasureString(TTF_Font* font, const char* text, size_t length,
    int maximum, int* width, size_t* fit)
{
    ++measures;
    return __real_TTF_MeasureString(font, text, length, maximum, width, fit);
}
int __real_pane_log_combat_row_tokens(int, const combat_roll_token**);
int __wrap_pane_log_combat_row_tokens(int row, const combat_roll_token** tokens)
{
    if (!test_roll || row != 0)
        return __real_pane_log_combat_row_tokens(row, tokens);
    if (tokens) *tokens = roll;
    return N_ELEMENTS(roll);
}

static void check_roll_redraw(void)
{
    sdl_view *log = &g_views[PANE_ROLLS];
    log->cols = 48; log->rows = 2; log->cell_w = 10; log->cell_h = 20;
    log->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, log->cols * log->cell_w, log->rows * log->cell_h);
    assert(log->canvas && !sdl_view_link_term(log, PANE_ROLLS));
    Term_activate(&log->t);
    sdl_set_present_suppressed(true);
    test_roll = true;
    Term->story_font_active = true;
    Term->story_pixel_pack = true;
    for (int col = 0; col < 6; ++col)
        Term_putstr(col, 0, 1, col % 2 ? TERM_SLATE : TERM_WHITE, "X");
    measures = 0;
    Term_fresh();
    assert(measures == N_ELEMENTS(roll));
    assert(!memcmp(Term->old->a[0], Term->scr->a[0], Term->wid));
    assert(!memcmp(Term->old->story[0], Term->scr->story[0], Term->wid));

    SDL_Rect row_rect={0,0,log->canvas->w,log->cell_h};
    assert(SDL_SetRenderTarget(g_state.renderer,log->canvas));
    SDL_Surface* first=SDL_RenderReadPixels(g_state.renderer,&row_rect);
    measures=0;
    assert(sdl_render_overlay_log_row(0) && measures==0);
    SDL_Surface* second=SDL_RenderReadPixels(g_state.renderer,&row_rect);
    assert(first && second && first->format==second->format && first->pitch==second->pitch);
    assert(!memcmp(first->pixels,second->pixels,(size_t)first->h*first->pitch));
    SDL_DestroySurface(first);
    SDL_DestroySurface(second);

    /* A face can change size without its pointer changing. The advance of
     * an unwrapped token must be measured again at the new generation. */
    TTF_Font* roll_font=sdl_story_font_for_view_slot(log,STORY_FONT_SLOT_SECONDARY);
    float size=TTF_GetFontSize(roll_font);
    assert(TTF_SetFontSize(roll_font,size+2));
    measures=0;
    assert(sdl_render_overlay_log_row(0) && measures==N_ELEMENTS(roll));
    assert(TTF_SetFontSize(roll_font,size));
    sdl_ui_text_cache_clear_font(roll_font);

    /* An ordinary row can reuse the same y while the side buffer exists. */
    Term->story_font_active = Term->story_pixel_pack = false;
    Term_clear();
    Term_putstr(0, 0, -1, TERM_WHITE, "ordinary");
    measures = 0;
    Term_fresh();
    assert(measures == 0 && !memcmp(Term->old->c[0], "ordinary", 8));
    test_roll = false;
    sdl_set_present_suppressed(false);
    term_nuke(&log->t); log->term_ready = false; Term = NULL;
    puts("Combat roll: six fallback color runs render and measure three tokens once PASS");
}

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
            sdl_ui_text_cache_clear_font(font);
            measures=0;
            int n=sdl_overlay_log_wrap(message,8,offsets,lengths);
            assert(n>0 && measures==n);
            int hot_offsets[8],hot_lengths[8];
            measures=0;
            assert(sdl_overlay_log_wrap(message,8,hot_offsets,hot_lengths)==n);
            assert(!memcmp(offsets,hot_offsets,n*sizeof(int)));
            assert(!memcmp(lengths,hot_lengths,n*sizeof(int)));
            /* Hash collisions may cause a miss but must not change the fit. */
            assert(measures<n);
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
        char mutable[]="iiiiiiii iiiiiiii iiiiiiii iiiiiiii";
        int off[8],len[8],expected_off[8],expected_len[8];
        sdl_overlay_log_wrap(mutable,8,off,len);
        for (size_t i=0; mutable[i]; i++) if (mutable[i]=='i') mutable[i]='W';
        int n=sdl_overlay_log_wrap(mutable,8,off,len);
        sdl_ui_text_cache_clear_font(font);
        assert(sdl_overlay_log_wrap(mutable,8,expected_off,expected_len)==n);
        assert(!memcmp(off,expected_off,n*sizeof(int)) && !memcmp(len,expected_len,n*sizeof(int)));
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
    check_roll_redraw();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''

if __name__ == "__main__":
    base.main()
