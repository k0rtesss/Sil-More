#!/usr/bin/env python3
"""Check real description capture wrapping against the modal thumb reservation.

Uses the production SDL layout and terminal text writer at phone sizes. The
thumb stub models its runtime predicate: modal controls appear only after the
interactive overlay becomes active. No player files are loaded.
"""
import inspect
import check_phone_overlays_layout as overlays

HARNESS = overlays.HARNESS[:overlays.HARNESS.index('static void inside(')]
HARNESS = HARNESS.replace('#include "sdl/input/sdl-touch-controls.c"', '')
HARNESS = HARNESS.replace(
    'bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect *r){return false;}',
    '''bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect *r){
    if(!g_description_overlay.active || !g_description_overlay.interactive
        || !g_description_overlay.footer_action_count)
        return false;
    if(width<height) *r=(SDL_FRect){40,height-150*density,width-80,100*density};
    else *r=(SDL_FRect){40,130,182,440};
    return true;
}''')
HARNESS += r'''
bool __wrap_sdl_mobile_portrait_layout_active(void) { return width<height; }
static void check_capture(bool interactive) {
    term scratch;
    term *previous=Term;
    char paragraph[2048];
    paragraph[0]='\0';
    for(int i=0;i<12;i++) SDL_strlcat(paragraph,
        "   A simple arrow of ash or birch, iron-headed and fletched for useful service. "
        "The waybread of the Elves is wrapped in leaves of mallorn. ",sizeof(paragraph));
    character_icky=1;
    g_description_overlay=(description_overlay_state){0};
    sdl_description_overlay_set_footer("Space Pick Up  Esc Close",true);
    sdl_description_overlay_add_footer_action(' ',"Space Pick Up");
    sdl_description_overlay_add_footer_action(ESCAPE,"Esc Close");
    int cols=sdl_description_overlay_capture_cols(100,interactive);
    assert(!g_description_overlay.active && !g_description_overlay.interactive);
    assert(g_description_overlay.footer_action_count==2);
    assert(term_init(&scratch,cols,255,16)==0);
    Term_activate(&scratch); text_out_wrap=text_out_indent=0;
    text_out_to_screen(TERM_WHITE,paragraph);
    int rows=scratch.scr->cy+1;
    byte *attrs=calloc(cols*rows,1);
    char *chars=calloc(cols*rows,1);
    assert(attrs&&chars);
    for(int y=0;y<rows;y++)for(int x=0;x<cols;x++) {
        attrs[y*cols+x]=scratch.scr->a[y][x];
        chars[y*cols+x]=scratch.scr->c[y][x];
    }
    g_description_overlay=(description_overlay_state){.active=true,
        .interactive=interactive,.attrs=attrs,.chars=chars,
        .width=cols,.height=rows,.target_cols=cols,.footer_action_count=2};
    description_overlay_layout layout;
    assert(sdl_description_overlay_layout(&layout));
    if(cols>layout.visible_cols) fprintf(stderr,
        "capture %d cols exceeds displayed %d at %dx%d big=%d interactive=%d\n",
        cols,layout.visible_cols,width,height,config.bigger_font,interactive);
    assert(cols<=layout.visible_cols);
    for(int y=0;y<rows;y++)for(int x=layout.visible_cols;x<cols;x++)
        assert(chars[y*cols+x]==' ');
    assert(layout.max_scroll>=0);
    g_description_overlay=(description_overlay_state){0};
    character_icky=0;
    Term_activate(previous);term_nuke(&scratch);free(attrs);free(chars);
}
int main(int argc,char **argv){
    assert(argc==6);setbuf(stdout,NULL);log_set_quiet(true);
    width=atoi(argv[1]);height=atoi(argv[2]);density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());
    sdl_config_set_defaults(&config);
    config.main_view_scale=2;
    config.mobile_portrait_mode=width<height;
    SDL_strlcpy(config.story_font2,argv[4],sizeof(config.story_font2));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    g_state.system_scale=density;
    term base;assert(term_init(&base,100,24,16)==0);Term_activate(&base);
    for(int big=0;big<2;big++) {
        config.bigger_font=big;
        check_capture(true);check_capture(false);
    }
    Term_activate(NULL);term_nuke(&base);
    printf("Description %dx%d @%.3f: normal/big modal/inline wrapping and restored state PASS\n",
        width,height,density);
    return 0;
}
'''


def main():
    driver = overlays.runner
    driver.HARNESS = HARNESS
    driver.OUT = driver.ROOT / 'scripts/output/description-capture-check'
    code = inspect.getsource(driver.main)
    code = code.replace('"/src/sdl/ui/sdl-halls-screen.c.obj",',
        '"/src/sdl/ui/sdl-menus.c.obj", "/src/sdl/input/sdl-tooltips.c.obj", '
        '"/src/sdl/ui/sdl-hint-quest-menu.c.obj", "/src/sdl/ui/sdl-pane-menus.c.obj",')
    code = code.replace('--wrap=Term_keypress",',
        '--wrap=Term_keypress,--wrap=sdl_overlay_pane_anchor_rect,--wrap=sdl_touch_thumb_current_bounds,--wrap=sdl_mobile_portrait_layout_active,--wrap=sdl_apply_config,--wrap=save_pane_config_to_json,--wrap=sdl_touch_only_device_active,--wrap=player_active_weapon_is_ranged,--wrap=player_quick_throw_available,--wrap=sdl_gamepad_send_key,--wrap=do_cmd_queue_floor_context_action,--wrap=floor_context_collect_square_actions,--wrap=touch_shortcut_context_action",')
    scope = driver.__dict__.copy()
    exec(code, scope)
    scope['main']()


if __name__ == '__main__':
    main()
