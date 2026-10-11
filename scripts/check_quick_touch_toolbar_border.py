#!/usr/bin/env python3
"""Exercise live Quick Touch/Quick Access bounds, rendering and pointer routes.

Uses the production mobile SDL layout with the isolated character fixture from
check_bigger_font_profiles. No user configuration or save is opened.
"""
from pathlib import Path

import check_bigger_font_profiles as base

CHECK = r'''
static void check_quick_touch_border(char **argv) {
    defaults();
    config.use_unsafe_area=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    config.mobile_portrait_mode=height>width;
    config.touch_thumb_enabled=true;
    config.touch_round_movement_enabled=true;
    config.touch_profile=SDL_TOUCH_PROFILE_ROUND_WHEEL;
    config.touch_top_panel_cell_count=6;
    config.touch_top_panel_arrows_visible=false;
    config.show_main_menu_button=false;
    g_touch_top_panel_open=true; g_direct_touch_present=true;
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    g_left_panel_pane_expanded=false; inkey_flag=true;
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.system_scale=density;
    g_state.window=SDL_CreateWindow("Quick Touch border fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); assert(g_state.renderer);
    sdl_view *view=&g_views[PANE_MAIN];
    assert(term_init(&view->t,80,24,16)==0); Term_activate(&view->t); term_screen=&view->t;
    view->t.data=(void *)(uintptr_t)PANE_MAIN; view->term_ready=true;
    view->rect=(SDL_Rect){0,0,width,height}; view->cell_w=8; view->cell_h=16;
    view->cols=width/8; view->rows=height/16;
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height); assert(view->canvas);
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    p_ptr->food=PY_FOOD_FULL-1;
    const enum pane_placement positions[]={PLACE_BOTTOM_LEFT,PLACE_BOTTOM_CENTER,PLACE_BOTTOM_RIGHT};
    int cases=0;
    for(int big=0;big<2;big++) for(int row_mode=0;row_mode<2;row_mode++)
    for(int side=0;side<2;side++) for(int position=0;position<3;position++)
    for(int rows=1;rows<=2;rows++) for(int stretch=0;stretch<2;stretch++)
    for(int description=0;description<2;description++) {
        /* Four contextual actions in both control groups plus the wheel need
         * a taller logical viewport than the compact gameplay case. */
        if(description && height<width && height/density<500) continue;
        config.bigger_font=big;
        config.left_panel_compact_mode=row_mode;
        config.quick_touch_buttons_on_left=side;
        config.touch_top_panel_rows=rows;
        config.touch_top_panel_size=stretch?SDL_TOUCH_TOP_PANEL_SIZE_STRETCH:3;
        enum pane_placement left_where=side?PLACE_TOP_RIGHT:PLACE_TOP_LEFT;
        pane_config_count=4;
        pane_config[0]=(struct pane_config){.pane=PANE_ROLLS,.where=left_where,
            .enabled=big,.rect.rows=4};
        pane_config[1]=(struct pane_config){.pane=PANE_LEFT_PANEL,.where=left_where,.enabled=true};
        pane_config[2]=(struct pane_config){.pane=PANE_STATUS_DEPTH,.where=PLACE_BOTTOM_CENTER,
            .enabled=true,.rect.rows=1,.rect.cols=24};
        pane_config[3]=(struct pane_config){.pane=PANE_OVERLAY_MENU,.where=positions[position],
            .enabled=true,.rect.rows=1,.rect.cols=4};
        g_description_overlay.active=description;
        g_description_overlay.interactive=description;
        g_description_overlay.footer_action_count=4;
        const int actions[]={'g','d','x','i'};
        const char *labels[]={"g) Pick Up","d) Drop","x) Inspect","i) Inventory"};
        const int bindings[]={'j','i','y','h',TOUCH_BIND_TOGGLE_TILES,'m'};
        for(int i=0;i<4;i++) {
            g_description_overlay.footer_actions[i].key=actions[i];
            SDL_strlcpy(g_description_overlay.footer_actions[i].token,labels[i],
                sizeof(g_description_overlay.footer_actions[i].token));
        }
        for(int i=0;i<6;i++) config.touch_top_panel_bindings[i]=description?actions[i%4]:bindings[i];
        sdl_touch_target_layout_end();
        sdl_reset_top_right_overlay_offset();
        sdl_left_panel_source_invalidate(); g_sdl_present_generation++;
        sdl_place_active_panes(&view->rect,g_pane_rects,false,false,true);
        sdl_left_panel_metrics metrics; SDL_FRect left;
        assert(sdl_left_panel_metrics_for_view(view,&metrics));
        assert(sdl_left_panel_pane_rect_for_metrics(view,&metrics,&left));
        g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){left.x,left.y,left.w,left.h};
        SDL_FRect first_quick={0},first_thumb={0};
        for(int frame=0;frame<2;frame++) {
            g_sdl_present_generation++;
            sdl_apply_top_right_overlay_offset();
            SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT],quick,thumb,contact;
            bool quick_ok=sdl_touch_top_panel_compute_layout(buttons,&quick);
            bool thumb_ok=sdl_touch_thumb_current_bounds(&thumb);
            if(!quick_ok || !thumb_ok) {
                fprintf(stderr,"missing controls %dx%d big%d rowmode%d side%d position%d rows%d stretch%d description%d: quick%d thumb%d\n",
                    width,height,big,row_mode,side,position,rows,stretch,description,quick_ok,thumb_ok);
                assert(0);
            }
            inside(quick); inside(thumb);
            if(SDL_GetRectIntersectionFloat(&quick,&thumb,&contact) && contact.w>1 && contact.h>1) {
                fprintf(stderr,"overlap %dx%d big%d rowmode%d side%d position%d rows%d stretch%d description%d: quick %.1f,%.1f %.1fx%.1f thumb %.1f,%.1f %.1fx%.1f\n",
                    width,height,big,row_mode,side,position,rows,stretch,description,
                    quick.x,quick.y,quick.w,quick.h,thumb.x,thumb.y,thumb.w,thumb.h);
                assert(0);
            }
            int visible=0;
            for(int i=0;i<SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;i++) if(buttons[i].w>0) {
                inside(buttons[i]);
                int slot=-1;
                assert(sdl_touch_top_panel_point_to_slot(buttons[i].x+buttons[i].w/2,
                    buttons[i].y+buttons[i].h/2,&slot) && slot==i);
                visible++;
            }
            assert(visible>=4);
            int slot=-1;
            assert(sdl_touch_thumb_point_to_button(thumb.x+2,thumb.y+2,&slot) && slot>=0);
            assert(!sdl_touch_top_panel_point_to_slot(thumb.x+2,thumb.y+2,&slot));
            if(frame) {
                assert(SDL_fabsf(first_quick.x-quick.x)<=1 && SDL_fabsf(first_quick.y-quick.y)<=1);
                assert(SDL_fabsf(first_quick.w-quick.w)<=1 && SDL_fabsf(first_quick.h-quick.h)<=1);
                assert(SDL_fabsf(first_thumb.x-thumb.x)<=1 && SDL_fabsf(first_thumb.y-thumb.y)<=1);
            } else { first_quick=quick; first_thumb=thumb; }
            if(big && row_mode && !side && position==1 && rows==1 && stretch && !description && frame==1) {
                SDL_SetRenderTarget(g_state.renderer,NULL);
                SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
                assert(sdl_render_left_panel_pane_from_cells(view,&left));
                sdl_touch_round_render(); sdl_touch_thumb_render();
                sdl_status_depth_pane_render(); sdl_touch_top_panel_render_buttons(buttons);
                SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL); assert(pixels);
                char path[128]; strnfmt(path,sizeof(path),"quick-touch-border-%dx%d.png",width,height);
                assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
            }
        }
        cases++;
    }
    printf("Quick Touch border %dx%d: %d normal/big, row/column, left/right, bottom slots, fixed/Stretch, 1/2 toolbar rows and contextual cases; non-overlap, both pointer routes and stable frames PASS\n",width,height,cases);
}
'''


if __name__ == "__main__":
    base.OUT = Path(__file__).resolve().parent / "output/quick-touch-toolbar-border"
    base.HARNESS = base.HARNESS.replace("int main(int argc,char **argv)", CHECK + "\nint main(int argc,char **argv)")
    base.HARNESS = base.HARNESS.replace("check_persistence(); check_layout(argv); return 0;",
                                      "check_quick_touch_border(argv); return 0;")
    base.main()
