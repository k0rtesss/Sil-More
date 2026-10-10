#!/usr/bin/env python3
"""Check independent font/orientation HUD profiles and render their defaults.

Uses production mobile layout/settings code with an offscreen SDL renderer.
Character/dungeon status content and OS orientation requests are fixtures; no
player data is loaded. Output lives in scripts/output/bigger-font-profiles.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/bigger-font-profiles"
HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>

static int width, height;
static float density;
static bool large_values;
static int weapon_fixture;
void sdl_ios_request_orientation(bool portrait) {}
void sdl_ios_install_orientation_observer(SDL_Window *window) {}
bool sdl_ios_get_safe_area_insets(SDL_Window *window,
    int *left, int *right, int *top, int *bottom) { return false; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }
bool __wrap_sdl_touch_only_device_active(void) { return true; }
bool __wrap_sdl_layout_matches_supporting_pane_visibility(void) { return true; }
void __wrap_sdl_depth_menu_pane_label(char *out, size_t size) {
    SDL_strlcpy(out,"50 ft",size);
}
int __wrap_player_current_movement_energy(void) { return 100; }
int __wrap_player_current_movement_speed(void) { return 2; }
void __wrap_prt_frame_basic(void) {
    Term_putstr(0, ROW_HP, -1, TERM_WHITE,
        large_values ? "Hth  999/999" : "Health 49/61");
    Term_putstr(0, ROW_SP, -1, TERM_L_GREEN,
        large_values ? "Vce  999:999" : "Voice  49:49");
    Term_putstr(0, ROW_LIGHT, -1, TERM_YELLOW, "oo     987");
    if(weapon_fixture==2) {
        Term_putstr(0, ROW_ARC, -1, TERM_WHITE, "(+3,1d7)");
        Term_putstr(0, ROW_QUIVER, -1, TERM_WHITE, "oo 18|24/24");
    } else {
        if(weapon_fixture==1)
            Term_putstr(0, ROW_MEL-1, -1, TERM_WHITE, "(+7,2d6)");
        Term_putstr(0, ROW_MEL, -1, TERM_WHITE, "(+7,2d6)");
    }
}

static struct pane_config *find(struct sdl_pane_profile *p, enum pane_type type) {
    for(int i=0;i<p->pane_count;i++) if(p->pane_configs[i].pane==type)
        return &p->pane_configs[i];
    assert(0); return NULL;
}
static void defaults(void) {
    sdl_config_set_defaults(&config);
    config.main_view_scale=2;
    sdl_copy_pane_configs(pane_config,&pane_config_count,
        default_pane_config,default_pane_config_count);
    sdl_seed_all_pane_profiles_from_active();
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
}
static void check_default(struct sdl_pane_profile *p) {
    bool portrait=((p-g_pane_profiles)%SDL_PANE_ORIENTATION_PROFILE_COUNT)
        /SDL_MIN_TERMINAL_MODE_COUNT==SDL_PANE_ORIENTATION_PORTRAIT;
    assert(p->left_panel_compact_mode==(portrait
        ? SDL_LEFT_PANEL_COMPACT_ROW : SDL_LEFT_PANEL_COMPACT_COLUMN));
    assert(!p->left_panel_expanded_on_launch && !p->show_main_menu_button);
    assert(p->touch_top_panel_cell_count==6 && p->touch_top_panel_rows==1);
    assert(p->touch_top_panel_bindings[5]=='m');
    assert(!p->touch_top_panel_arrows_visible && p->touch_top_panel_default_open);
    struct pane_config *left=find(p,PANE_LEFT_PANEL),*log=find(p,PANE_ROLLS);
    assert(left->enabled && left->where==PLACE_TOP_LEFT);
    assert(log->enabled && log->where==(portrait ? PLACE_TOP_LEFT : PLACE_TOP_RIGHT)
        && log->rect.rows==4);
    assert(log<left);
    assert(find(p,PANE_STATUS_DEPTH)->enabled);
    assert(find(p,PANE_STATUS_DEPTH)<find(p,PANE_OVERLAY_MENU));
    assert(!find(p,PANE_LOG)->enabled);
}
static void check_persistence(void) {
    defaults();
    /* The startup tutorial reapplies the movement profile after Big Text. */
    set_sdl_bigger_font(true);
    for(int profile=0;profile<SDL_TOUCH_PROFILE_COUNT;profile++) {
        sdl_touch_apply_profile(profile);
        assert(config.touch_top_panel_cell_count==6);
        assert(config.touch_top_panel_bindings[5]=='m');
    }
    config.touch_top_panel_cell_count=10;
    sdl_touch_apply_profile(SDL_TOUCH_PROFILE_ROUND_WHEEL);
    assert(config.touch_top_panel_cell_count==10);
    defaults();
    for(int i=SDL_PANE_ORIENTATION_PROFILE_COUNT;i<SDL_PANE_PROFILE_COUNT;i++)
        check_default(&g_pane_profiles[i]);
    /* Each terminal size contains four independent font/orientation layouts. */
    for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
        set_sdl_min_terminal_mode(mode);
        for(int big=0;big<2;big++) {
            set_sdl_bigger_font(big);
            for(int portrait=0;portrait<2;portrait++) {
                set_sdl_mobile_portrait_mode(portrait);
                int index=SDL_PANE_FONT_PROFILE_INDEX(big,portrait,mode);
                config.touch_top_panel_cell_count=7+index;
                config.touch_top_panel_bindings[0]='a'+index;
                config.touch_top_panel_long_bindings[0]='A'+index;
                config.left_panel_compact_mode=index%2;
                pane_config[0].font_size=11+index;
                g_main_view_zoom_scale=7;
                sdl_store_active_pane_profile(mode);
                set_sdl_bigger_font(big); /* no-op must not discard edits */
                assert(config.touch_top_panel_cell_count==7+index);
                assert(g_main_view_zoom_scale==7);
            }
        }
    }
    assert(sdl_config_save("profiles.json",&config,g_pane_profiles,SDL_PANE_PROFILE_COUNT));
    memset(g_pane_profiles,0,sizeof(g_pane_profiles));
    assert(sdl_config_load("profiles.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
        set_sdl_min_terminal_mode(mode);
        for(int big=0;big<2;big++) {
            set_sdl_bigger_font(big);
            for(int portrait=0;portrait<2;portrait++) {
                set_sdl_mobile_portrait_mode(portrait);
                int index=SDL_PANE_FONT_PROFILE_INDEX(big,portrait,mode);
                assert(config.touch_top_panel_cell_count==7+index);
                assert(config.touch_top_panel_bindings[0]=='a'+index);
                assert(config.touch_top_panel_long_bindings[0]=='A'+index);
                assert(config.left_panel_compact_mode==index%2);
                assert(pane_config[0].font_size==11+index);
            }
        }
    }
    FILE *old=fopen("legacy.json","wb"); assert(old);
    fputs("{\"sdl\":{\"biggerFont\":true,\"leftPanelExpandedOnLaunch\":false},"
        "\"paneProfiles\":{\"landscape\":{\"NORMAL\":{\"touchTopPanelCellCount\":11,"
        "\"panes\":[{\"type\":\"LEFT_PANEL\",\"where\":\"TOP_RIGHT\",\"enabled\":true}]}},"
        "\"portrait\":{\"NORMAL\":{\"touchTopPanelCellCount\":10}}}}",old);
    fclose(old);
    defaults();
    assert(sdl_config_load("legacy.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    assert(g_pane_profiles[0].touch_top_panel_cell_count==11);
    assert(find(&g_pane_profiles[0],PANE_LEFT_PANEL)->where==PLACE_TOP_RIGHT);
    assert(g_pane_profiles[2].touch_top_panel_cell_count==10);
    sdl_normalize_unified_log_pane_profiles(true);
    sdl_ensure_default_pane_profiles_present(false);
    for(int i=SDL_PANE_ORIENTATION_PROFILE_COUNT;i<SDL_PANE_PROFILE_COUNT;i++)
        check_default(&g_pane_profiles[i]);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
    assert(config.touch_top_panel_cell_count==6);
    set_sdl_bigger_font(false); assert(config.touch_top_panel_cell_count==11);
    FILE *partial=fopen("partial.json","wb"); assert(partial);
    fputs("{\"paneProfiles\":{\"biggerFont\":{\"portrait\":{\"NORMAL\":{"
        "\"touchTopPanelCellCount\":13}}}}}",partial); fclose(partial);
    defaults();
    assert(sdl_config_load("partial.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,1,0)].touch_top_panel_cell_count==13);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,1,1)].touch_top_panel_cell_count==13);
    assert(g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,0,0)].touch_top_panel_cell_count==6);
    FILE *prior=fopen("prior-big-hud.json","wb"); assert(prior);
    fputs("{\"paneProfiles\":{\"biggerFont\":{\"landscape\":{\"NORMAL\":{"
        "\"leftPanelCompactMode\":\"ROW\",\"touchTopPanelCellCount\":9,\"panes\":["
        "{\"type\":\"LEFT_PANEL\",\"where\":\"TOP_CENTER\",\"enabled\":true,\"fontSize\":31},"
        "{\"type\":\"ROLLS\",\"where\":\"TOP_CENTER\",\"enabled\":true,\"rows\":4}]}}}}}",prior);
    fclose(prior);
    defaults();
    assert(sdl_config_load("prior-big-hud.json",&config,g_pane_profiles,
        SDL_PANE_PROFILE_COUNT,NULL)==SDL_CONFIG_LOAD_OK);
    struct sdl_pane_profile *updated=&g_pane_profiles[SDL_PANE_FONT_PROFILE_INDEX(true,0,0)];
    assert(find(updated,PANE_ROLLS)<find(updated,PANE_LEFT_PANEL));
    assert(find(updated,PANE_ROLLS)->where==PLACE_TOP_LEFT);
    assert(find(updated,PANE_LEFT_PANEL)->where==PLACE_TOP_LEFT);
    assert(find(updated,PANE_LEFT_PANEL)->font_size==31 && updated->touch_top_panel_cell_count==9);
    puts("Profiles: eight independent layouts, live toggle/rotation/terminal size, restart, legacy preservation PASS");
}

static void inside(SDL_FRect r) {
    assert(r.w>0 && r.h>0 && r.x>=-1 && r.y>=-1);
    assert(r.x+r.w<=width+1 && r.y+r.h<=height+1);
}
static void check_bottom_center_stack(void) {
    large_values=false;
    bool saved_thumb=config.touch_thumb_enabled;
    /* Isolate ordered Combat/Quick Access panes here. Their interaction with
     * Quick Touch is covered by check_quick_touch_toolbar_border.py. */
    config.touch_thumb_enabled=false;
    struct pane_config baseline[MAX_PANE_CONFIGS];
    int count=pane_config_count;
    memcpy(baseline,pane_config,sizeof(baseline));
    for(int big=1;big<2;big++)
    for(int after=0;after<2;after++)
    for(int rows=1;rows<=2;rows++)
    for(int stretch=0;stretch<2;stretch++) {
        memcpy(pane_config,baseline,sizeof(baseline));
        config.bigger_font=big;
        config.left_panel_compact_mode=SDL_LEFT_PANEL_COMPACT_COLUMN;
        config.touch_top_panel_rows=rows;
        config.touch_top_panel_size=stretch?SDL_TOUCH_TOP_PANEL_SIZE_STRETCH:3;
        int combat_index=-1,quick_index=-1;
        for(int i=0;i<count;i++) {
            if(pane_config[i].pane==PANE_COMBAT) combat_index=i;
            if(pane_config[i].pane==PANE_LEFT_PANEL) pane_config[i].font_size=0;
            /* This fixture exercises the separate bottom Combat/Quick Access
             * stack. Leave room for two minimum-size button rows on short
             * landscape displays; the default top log is checked below. */
            if(pane_config[i].pane==PANE_ROLLS) pane_config[i].enabled=false;
        }
        assert(combat_index>=0);
        struct pane_config moving=pane_config[combat_index];
        moving.where=PLACE_BOTTOM_CENTER;
        memmove(&pane_config[combat_index],&pane_config[combat_index+1],
            (count-combat_index-1)*sizeof(moving));
        for(int i=0;i<count-1;i++)
            if(pane_config[i].pane==PANE_OVERLAY_MENU) quick_index=i;
        assert(quick_index>=0);
        int insertion=quick_index+after;
        memmove(&pane_config[insertion+1],&pane_config[insertion],
            (count-1-insertion)*sizeof(moving));
        pane_config[insertion]=moving;
        sdl_left_panel_source_invalidate();
        SDL_Rect screen={0,0,width,height};
        sdl_reset_top_right_overlay_offset();
        g_touch_pane_hidden_layout_active=sdl_touch_pane_hidden_mode_active();
        g_touch_pane_proto_layout_active=sdl_touch_pane_proto_mode_active();
        sdl_place_active_panes(&screen,g_pane_rects,false,false,true);
        assert(sdl_rect_has_area(&g_pane_rects[PANE_DESCRIPTION]));
        sdl_left_panel_metrics metrics; SDL_FRect left;
        assert(sdl_left_panel_metrics_for_view(&g_views[PANE_MAIN],&metrics));
        assert(sdl_left_panel_pane_rect_for_metrics(&g_views[PANE_MAIN],&metrics,&left));
        g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){left.x,left.y,left.w,left.h};
        SDL_FRect without_combat={0};
        if(after) {
            pane_config[insertion].enabled=false;
            g_sdl_present_generation++;
            sdl_apply_top_right_overlay_offset();
            assert(sdl_touch_top_panel_compute_layout(NULL,&without_combat));
            pane_config[insertion].enabled=true;
            sdl_reset_top_right_overlay_offset();
            sdl_place_active_panes(&screen,g_pane_rects,false,false,true);
            g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){left.x,left.y,left.w,left.h};
        }
        SDL_FRect first={0};
        for(int frame=0;frame<3;frame++) {
            g_sdl_present_generation++;
            sdl_apply_top_right_overlay_offset();
            SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT],quick,contact;
            SDL_Rect combat;
            assert(sdl_touch_top_panel_compute_layout(buttons,&quick)); inside(quick);
            assert(sdl_combat_overlay_pane_current_rect(&combat));
            SDL_FRect combat_rect={combat.x,combat.y,combat.w,combat.h}; inside(combat_rect);
            if((height>width && SDL_fabsf(quick.x+quick.w/2-width/2.f)>1)
                || (after && combat.y+combat.h>quick.y+1)
                || (!after && quick.y+quick.h>combat.y+1))
                fprintf(stderr,"stack %dx%d big%d after%d rows%d stretch%d frame%d: quick %.1f,%.1f %.1fx%.1f combat %d,%d %dx%d\n",
                    width,height,big,after,rows,stretch,frame,quick.x,quick.y,
                    quick.w,quick.h,combat.x,combat.y,combat.w,combat.h);
            if(height>width) assert(SDL_fabsf(quick.x+quick.w/2-width/2.f)<=1);
            if(after) {
                assert(SDL_fabsf(quick.x-without_combat.x)<=1);
                assert(SDL_fabsf(quick.y-without_combat.y)<=1);
                assert(SDL_fabsf(quick.w-without_combat.w)<=1);
                assert(SDL_fabsf(quick.h-without_combat.h)<=1);
            }
            assert(!SDL_GetRectIntersectionFloat(&quick,&combat_rect,&contact)
                || contact.w<=1 || contact.h<=1);
            if(after) assert(combat.y+combat.h<=quick.y+1);
            else assert(quick.y+quick.h<=combat.y+1);
            if(rows==1) for(int i=1;i<6;i++) assert(buttons[i].y==buttons[0].y);
            for(int i=0;i<6;i++) {
                inside(buttons[i]);
                assert(buttons[i].w>=sdl_ui_min_tap_px() && buttons[i].h>=sdl_ui_min_tap_px());
                int slot=-1;
                assert(sdl_touch_top_panel_point_to_slot(buttons[i].x+buttons[i].w/2,
                    buttons[i].y+buttons[i].h/2,&slot) && slot==i);
            }
            if(frame) {
                assert(SDL_fabsf(first.x-quick.x)<=1 && SDL_fabsf(first.y-quick.y)<=1);
                assert(SDL_fabsf(first.w-quick.w)<=1 && SDL_fabsf(first.h-quick.h)<=1);
            } else first=quick;
            if(after && big && rows==1 && stretch && frame==2) {
                SDL_SetRenderTarget(g_state.renderer,NULL);
                SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
                assert(sdl_render_left_panel_pane_from_cells(&g_views[PANE_MAIN],&left));
                sdl_touch_round_render();
                sdl_status_depth_pane_render();
                sdl_combat_overlay_pane_render();
                sdl_touch_top_panel_render_buttons(buttons);
                char path[160]; strnfmt(path,sizeof(path),"bottom-center-3-%dx%d.png",width,height);
                SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL); assert(pixels);
                assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
            }
        }
    }
    memcpy(pane_config,baseline,sizeof(baseline));
    config.bigger_font=true;
    config.touch_thumb_enabled=saved_thumb;
    printf("Big-font Bottom Center %dx%d: Combat before/after Quick Access, 1/2 rows, fixed/Stretch, unchanged by successor and stable across frames PASS\n",width,height);
}
static void check_compact_combat_states(void) {
    sdl_view *view=&g_views[PANE_MAIN];
    SDL_Rect saved_rect=view->rect;
    int saved_cols=view->cols;
    object_kind *saved_kinds=k_info;
    static object_kind kinds[4];
    k_info=kinds;
    kinds[1].x_attr=TERM_WHITE; kinds[1].x_char=')';
    kinds[2].x_attr=TERM_L_GREEN; kinds[2].x_char='=';
    inventory[INVEN_WIELD]=(object_type){.k_idx=1,.tval=TV_SWORD};
    inventory[INVEN_LEFT]=(object_type){.k_idx=2,.tval=TV_RING};
    inventory[INVEN_RIGHT]=inventory[INVEN_LEFT];
    inventory[INVEN_NECK]=(object_type){.k_idx=3,.tval=TV_AMULET};
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=true;
    for(int i=0;i<pane_config_count;i++) if(pane_config[i].pane==PANE_LEFT_PANEL)
        pane_config[i].font_size=0;
    for(int mode=0;mode<3;mode++) for(int jewelry=0;jewelry<3;jewelry++) {
        weapon_fixture=mode;
        p_ptr->active_weapon_mode=mode==2?PLAYER_ACTIVE_WEAPON_RANGED_1:PLAYER_ACTIVE_WEAPON_MELEE;
        inventory[INVEN_ARM]=mode==1?inventory[INVEN_WIELD]:(object_type){0};
        jewelry_presets_reset();
        inventory[INVEN_LEFT].pval=0;
        if(jewelry) {
            assert(jewelry_preset_store_current(0));
            assert(jewelry_preset_set_name(0,"Jewelry"));
            if(jewelry==2) inventory[INVEN_LEFT].pval=1;
        }
        for(int narrow=0;narrow<2;narrow++) {
            view->rect.w=narrow?240:4096;
            view->cols=view->rect.w/view->cell_w;
            sdl_left_panel_source_invalidate(); g_sdl_present_generation++;
            sdl_left_panel_metrics metrics;
            assert(sdl_left_panel_metrics_for_view(view,&metrics));
            assert(metrics.compact_segment_count==(mode?5:4)+(jewelry==1));
            assert(metrics.panel_rows==(narrow?2:1));
            if(!narrow && metrics.total_w%view->cell_w==0) {
                view->rect.w=metrics.total_w; view->cols=view->rect.w/view->cell_w;
                assert(sdl_left_panel_metrics_for_view(view,&metrics));
                assert(metrics.panel_rows==1 && metrics.total_w==view->rect.w);
            }
            for(int i=3;i<metrics.compact_segment_count;i++) {
                SDL_FRect r;
                int source_row=metrics.compact_source_rows[i];
                if(!sdl_combat_overlay_cell_rect(0,source_row,12,1,&r)) {
                    fprintf(stderr,"missing combat cell: mode%d jewelry%d narrow%d source%d\n",mode,jewelry,narrow,source_row);
                    assert(0);
                }
                assert(metrics.compact_output_cols[i]+metrics.compact_widths[i]<=metrics.content_cols);
                for(int last=0;last<2;last++) {
                    int col,row;
                    float x=r.x+(last?r.w-.5f:.5f);
                    assert(sdl_combat_overlay_point_to_cell(x,r.y+r.h/2,&col,&row));
                    assert(row==source_row);
                    assert(col==(last?metrics.compact_widths[i]-1:0));
                }
            }
            SDL_FRect hp;
            assert(sdl_left_panel_source_cell_rect(0,ROW_HP,12,1,&hp));
            int col,row;
            assert(!sdl_combat_overlay_point_to_cell(hp.x+1,hp.y+1,&col,&row));
        }
        if(mode==2 && jewelry==1) {
            view->rect=saved_rect; view->cols=saved_cols;
            sdl_left_panel_source_invalidate();
            sdl_left_panel_metrics metrics; SDL_FRect panel;
            assert(sdl_left_panel_metrics_for_view(view,&metrics));
            assert(sdl_left_panel_pane_rect_for_metrics(view,&metrics,&panel));
            SDL_SetRenderTarget(g_state.renderer,NULL);
            SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
            assert(sdl_render_left_panel_pane_from_cells(view,&panel));
            SDL_Rect crop={0,0,width,(int)(panel.y+panel.h)};
            SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&crop); assert(pixels);
            char path[160]; strnfmt(path,sizeof(path),"combat-jewelry-%dx%d.png",width,height);
            assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
        }
    }
    jewelry_presets_reset();
    inventory[INVEN_LEFT]=(object_type){0}; inventory[INVEN_RIGHT]=(object_type){0};
    inventory[INVEN_NECK]=(object_type){0}; inventory[INVEN_ARM]=(object_type){0};
    inventory[INVEN_WIELD]=(object_type){0};
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=false;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    weapon_fixture=0; k_info=saved_kinds; view->rect=saved_rect; view->cols=saved_cols;
    sdl_left_panel_source_invalidate();
    puts("Compact combat: dynamic 1/2 rows, melee/offhand/ranged/quiver, matching/unmatched jewelry and first/last cell taps PASS");
}
static void check_safe_area_edges(void) {
    sdl_view *view=&g_views[PANE_MAIN];
    SDL_Rect saved_rect=view->rect, saved_safe=g_state.safe_area;
    int saved_cols=view->cols, saved_rows=view->rows;
    bool saved_unsafe=config.use_unsafe_area;
    g_state.safe_area=(SDL_Rect){11,23,width-33,height-47};
    for(int unsafe=0;unsafe<2;unsafe++) {
        config.use_unsafe_area=unsafe;
        SDL_Rect area=sdl_get_layout_screen_rect();
        view->rect=area; view->cols=area.w/view->cell_w; view->rows=area.h/view->cell_h;
        sdl_reset_top_right_overlay_offset();
        sdl_left_panel_source_invalidate(); g_sdl_present_generation++;
        sdl_place_active_panes(&area,g_pane_rects,false,false,true);
        assert(g_pane_rects[PANE_ROLLS].x==area.x && g_pane_rects[PANE_ROLLS].y==area.y);
        assert(g_pane_rects[PANE_ROLLS].h==4*sdl_effective_pane_cell_height_for_type(PANE_ROLLS));
        sdl_left_panel_metrics metrics; SDL_FRect panel, hp;
        assert(sdl_left_panel_metrics_for_view(view,&metrics));
        assert(sdl_left_panel_pane_rect_for_metrics(view,&metrics,&panel));
        assert(panel.x==area.x && panel.y==area.y+g_pane_rects[PANE_ROLLS].h);
        g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){panel.x,panel.y,panel.w,panel.h};
        assert(sdl_left_panel_source_cell_rect(0,ROW_HP,12,1,&hp));
        assert(hp.x==area.x+SDL_BIG_TEXT_SIDE_PAD_PX);
        int col,row;
        assert(sdl_main_view_point_to_cell(hp.x+1,hp.y+1,&col,&row) && row==ROW_HP);
        if(height>width) {
            SDL_Rect left,right;
            assert(sdl_mobile_portrait_control_regions(&left,&right));
            assert(left.x==area.x+SDL_BIG_TEXT_SIDE_PAD_PX);
            assert(right.x+right.w==area.x+area.w-SDL_BIG_TEXT_SIDE_PAD_PX);
        }
    }
    view->rect=saved_rect; view->cols=saved_cols; view->rows=saved_rows;
    g_state.safe_area=saved_safe; config.use_unsafe_area=saved_unsafe;
    sdl_left_panel_source_invalidate();
    puts("Big-text borders: safe/full area respected, small horizontal inset, no vertical padding, taps PASS");
}
static void check_layout(char **argv) {
    defaults(); set_sdl_bigger_font(true);
    set_sdl_mobile_portrait_mode(height>width);
    /* Exercise the optional row HUD at every size; defaults are checked above. */
    config.left_panel_compact_mode=SDL_LEFT_PANEL_COMPACT_ROW;
    for(int i=0;i<pane_config_count;i++) if(pane_config[i].pane==PANE_ROLLS)
        pane_config[i].where=PLACE_TOP_LEFT;
    config.use_unsafe_area=false;
    config.margin=6; /* Big text must ignore even a saved generous inset. */
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.system_scale=density;
    g_state.window=SDL_CreateWindow("Big-font HUD fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){11,23,width-33,height-47};
    SDL_Rect full=sdl_get_layout_screen_rect();
    assert(!memcmp(&full,&g_state.safe_area,sizeof(full)));
    config.bigger_font=false;
    SDL_Rect safe=sdl_get_layout_screen_rect();
    assert(!memcmp(&safe,&g_state.safe_area,sizeof(safe)));
    config.bigger_font=true;
    config.use_unsafe_area=true;
    full=sdl_get_layout_screen_rect();
    assert(full.x==0 && full.y==0 && full.w==width && full.h==height);
    config.use_unsafe_area=false;
    g_state.safe_area=full;
    assert(sdl_overlay_margin_px()==0 && sdl_overlay_inner_gap_px()==0);
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    g_direct_touch_present=true;
    config.touch_profile=SDL_TOUCH_PROFILE_ROUND_WHEEL;
    config.touch_round_movement_enabled=true;
    g_touch_pane_hidden_layout_active=sdl_touch_pane_hidden_mode_active();
    g_touch_pane_proto_layout_active=sdl_touch_pane_proto_mode_active();
    sdl_view *view=&g_views[PANE_MAIN];
    assert(term_init(&view->t,80,24,16)==0); view->t.data=(void *)(uintptr_t)PANE_MAIN;
    Term_activate(&view->t); term_screen=&view->t;
    view->term_ready=true; view->rect=(SDL_Rect){0,0,width,height};
    view->cell_w=8; view->cell_h=16; view->cols=width/8; view->rows=height/16;
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height); assert(view->canvas);
    for(int i=0;i<16;i++) g_state.palette[i]=(SDL_Color){230,230,230,255};
    g_state.palette[TERM_DARK]=(SDL_Color){0,0,0,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){90,235,140,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){245,210,70,255};
    for(int values=0;values<3;values++) {
        large_values=values!=0;
        int custom_font=width>height && height/density<500?18:32;
        for(int i=0;i<pane_config_count;i++) if(pane_config[i].pane==PANE_LEFT_PANEL)
            pane_config[i].font_size=values==2?custom_font:0;
        sdl_left_panel_source_invalidate();
        g_sdl_present_generation++;
        SDL_Rect screen={0,0,width,height};
        sdl_reset_top_right_overlay_offset();
        sdl_place_active_panes(&screen,g_pane_rects,false,false,true);
        SDL_Rect fitted[PANE_MAX];
        sdl_place_active_panes_fitting_main(&screen,fitted,false,false,true,NULL,NULL);
        assert(sdl_rect_has_area(&fitted[PANE_DESCRIPTION]));
        assert(!memcmp(fitted,g_pane_rects,sizeof(fitted)));
        sdl_left_panel_metrics metrics;
        assert(sdl_left_panel_metrics_for_view(view,&metrics));
        SDL_FRect panel;
        assert(sdl_left_panel_pane_rect_for_metrics(view,&metrics,&panel)); inside(panel);
        assert(metrics.compact_row && metrics.compact_segment_count==4);
        assert(metrics.separator_w==SDL_BIG_TEXT_SIDE_PAD_PX);
        if(values<2) assert(metrics.cell_h==sdl_effective_pane_cell_height_for_type(PANE_LEFT_PANEL));
        int single_cols=metrics.compact_segment_count-1;
        for(int i=0;i<metrics.compact_segment_count;i++) single_cols+=metrics.compact_widths[i];
        int natural_cell_w=sdl_effective_pane_cell_height_for_type(PANE_LEFT_PANEL)/2;
        int visual_w=sdl_main_view_visual_cols(view)*view->cell_w;
        int single_width=single_cols*natural_cell_w+2*SDL_BIG_TEXT_SIDE_PAD_PX;
        assert(metrics.panel_rows==(single_width<=visual_w?1:2));
        assert(metrics.corner_h==metrics.panel_rows*metrics.cell_h);
        assert(sdl_combat_overlay_in_compact_row());
        assert(!sdl_rect_has_area(&g_pane_rects[PANE_COMBAT]));
        for(int i=0;i<metrics.compact_segment_count;i++) {
            assert(metrics.compact_output_rows[i]<metrics.panel_rows);
            assert(metrics.compact_output_cols[i]+metrics.compact_widths[i]<=metrics.content_cols);
        }
        g_pane_rects[PANE_LEFT_PANEL]=(SDL_Rect){panel.x,panel.y,panel.w,panel.h};
        sdl_view *log=&g_views[PANE_ROLLS];
        assert(sdl_view_create(log,g_pane_rects[PANE_ROLLS],config.monospace_font,
            sdl_effective_pane_font_size_for_type(PANE_ROLLS),0,0));
        assert(log->rows==4);
        assert(log->rect.y==0 && log->rect.h==4*log->cell_h);
        assert(log->margin_x==0 && log->margin_y==0);
        assert(SDL_OVERLAY_LOG_TEXT_LEFT_PAD(log)==SDL_BIG_TEXT_SIDE_PAD_PX
            && SDL_OVERLAY_LOG_TEXT_RIGHT_PAD(log)==SDL_BIG_TEXT_SIDE_PAD_PX);
        assert(term_init(&log->t,log->cols,log->rows,16)==0);
        log->t.data=(void *)(uintptr_t)PANE_ROLLS; log->term_ready=true;
        Term_activate(&log->t);
        const char *message="A cold wind blows through the corridor. You hear footsteps in the distance.";
        int offsets[16],lengths[16];
        int lines=sdl_overlay_log_wrap(message,16,offsets,lengths); assert(lines>0);
        TTF_Font *font=sdl_story_font_for_height_slot(log->cell_h,SDL_STORY_FONT_SLOT_LOG);
        assert(font);
        for(int i=0;i<MIN(4,lines);i++)
            sdl_render_story_text_free_px(log,font,SDL_OVERLAY_LOG_TEXT_LEFT_PAD(log),i,
                message+offsets[i],lengths[i],g_state.palette[TERM_WHITE],
                log->cols*log->cell_w-SDL_OVERLAY_LOG_TEXT_LEFT_PAD(log)-SDL_OVERLAY_LOG_TEXT_RIGHT_PAD(log));
        Term_activate(&view->t);
        SDL_SetRenderTarget(g_state.renderer,NULL);
        sdl_apply_top_right_overlay_offset();
        SDL_Rect band; assert(sdl_overlay_log_pane_current_rect(&band));
        assert(sdl_overlay_log_left_margin(log->cols)==0);
        config.bigger_font=false;
        assert(sdl_overlay_log_left_margin(log->cols)==pane_log_overlay_left_margin(log->cols));
        config.bigger_font=true;
        assert(band.w>=width-log->cell_w);
        assert(panel.y>=band.y+band.h);
        assert(panel.x==screen.x && band.x==screen.x);
        assert(band.y==screen.y && panel.y==band.y+band.h);
        assert(log->rows==4 && log->rect.h>=4*log->cell_h);
        SDL_FRect log_rect={band.x,band.y,band.w,band.h}; inside(log_rect);
        SDL_SetRenderDrawColor(g_state.renderer,24,30,36,255); SDL_RenderClear(g_state.renderer);
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,210);
        SDL_RenderFillRect(g_state.renderer,&log_rect);
        SDL_FRect log_content={band.x,band.y,log->cols*log->cell_w,4*log->cell_h};
        SDL_RenderTexture(g_state.renderer,log->canvas,NULL,&log_content);
        assert(sdl_render_left_panel_pane_from_cells(view,&panel));
        for(int i=0;i<3;i++) {
            SDL_FRect cell;
            assert(sdl_left_panel_source_cell_rect(0,metrics.compact_source_rows[i],
                LEFT_PANEL_CONTENT_WID,1,&cell));
            assert(cell.h==metrics.cell_h);
            for(int part=0;part<2;part++) {
                int col,row;
                assert(sdl_main_view_point_to_cell(part?cell.x+cell.w-2:cell.x+2,
                    cell.y+.5f*metrics.cell_h,&col,&row));
                assert(g_last_main_cell_hit_left_panel && row==metrics.compact_source_rows[i]);
            }
        }
        SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT],quick;
        SDL_Rect anchor;
        enum pane_placement quick_where;
        assert(sdl_touch_top_panel_current_anchor(&screen,&anchor,&quick_where));
        assert(sdl_touch_top_panel_compute_layout(buttons,&quick)); inside(quick);
        assert(quick.x>=screen.x+SDL_BIG_TEXT_SIDE_PAD_PX-1);
        assert(quick.x+quick.w<=screen.x+screen.w-SDL_BIG_TEXT_SIDE_PAD_PX+1);
        for(int i=0;i<6;i++) {
            inside(buttons[i]);
            assert(buttons[i].w>=sdl_ui_min_tap_px() && buttons[i].h>=sdl_ui_min_tap_px());
            for(int j=0;j<i;j++) assert(!SDL_HasRectIntersectionFloat(&buttons[i],&buttons[j]));
        }
        sdl_touch_top_panel_render_buttons(buttons);
        sdl_status_depth_pane_render();
        sdl_combat_overlay_pane_render();
        status_depth_pane_layout status;
        assert(sdl_status_depth_pane_layout(&status)); inside(status.panel);
        SDL_FRect contact;
        /* Integer anchors can touch the fractional final button edge. */
        assert(!SDL_GetRectIntersectionFloat(&quick,&status.panel,&contact)
            || contact.h<=1 || contact.w<=1);
        assert(!SDL_GetRectIntersectionFloat(&quick,&log_rect,&contact)
            || contact.h<=1 || contact.w<=1);
        SDL_Rect combat;
        assert(!sdl_combat_overlay_pane_current_rect(&combat));
        SDL_FRect combat_rect;
        assert(sdl_combat_overlay_cell_rect(0,ROW_MEL,12,1,&combat_rect));
        inside(combat_rect);
        assert(combat_rect.x>=panel.x && combat_rect.x+combat_rect.w<=panel.x+panel.w);
        assert(combat_rect.y>=panel.y && combat_rect.y+combat_rect.h<=panel.y+panel.h);
        int col,row;
        assert(sdl_combat_overlay_point_to_cell(combat_rect.x+combat_rect.w/2,
            combat_rect.y+combat_rect.h/2,&col,&row) && row==ROW_MEL);
        assert(!SDL_GetRectIntersectionFloat(&status.panel,&combat_rect,&contact)
            || contact.h<=1 || contact.w<=1);
        char path[160]; strnfmt(path,sizeof(path),"hud-%dx%d-%.3f-%d.png",width,height,density,values);
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,NULL); assert(pixels);
        assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
        SDL_DestroyTexture(log->canvas); log->canvas=NULL;
        term_nuke(&log->t); log->term_ready=false;
    }
    printf("HUD %dx%d @%.3f: combined character/combat, dynamic 1/2 rows, value taps, full-width log, six reachable cells PASS\n",width,height,density);
    check_safe_area_edges();
    check_compact_combat_states();
    check_bottom_center_stack();
}
int main(int argc,char **argv) {
    assert(argc==7); setbuf(stdout,NULL); log_set_quiet(true);
    width=atoi(argv[1]); height=atoi(argv[2]); density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"); assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    assert(TTF_Init());
    static maxima limits; static player_type player; static player_other options;
    static object_type items[INVEN_TOTAL];
    static byte features[32][MAX_DUNGEON_WID];
    static s16b floor_objects[32][MAX_DUNGEON_WID];
    static s16b monsters[32][MAX_DUNGEON_WID];
    static u16b info[32][256];
    cave_feat=features; cave_o_idx=floor_objects; cave_m_idx=monsters; cave_info=info;
    z_info=&limits; p_ptr=&player; op_ptr=&options; inventory=items;
    player.playing=true; player.song1=player.song2=SNG_NOTHING;
    player.chp=player.mhp=41; player.csp=player.msp=7;
    player.cur_map_hgt=32; player.cur_map_wid=64;
    items[INVEN_LITE].k_idx=1; character_generated=character_dungeon=true;
    check_persistence(); check_layout(argv); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    mobile_sources = ["src/sdl/core/sdl-state.c", "src/sdl/core/sdl-layout.c",
                      "src/sdl/config/sdl-settings.c", "src/sdl/render/sdl-fonts.c",
                      "src/sdl-config.c", "src/sdl/input/sdl-touch-controls.c",
                      "src/sdl/ui/sdl-panes.c"]
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = tuple("/" + p + ".obj" for p in mobile_sources) + ("/src/main.c.obj",)
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects if not o.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / d) for d in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
        + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wraps = ["SDL_GetDisplayContentScale", "sdl_touch_only_device_active",
             "sdl_layout_matches_supporting_pane_visibility", "prt_frame_basic",
             "sdl_depth_menu_pane_label", "player_current_movement_energy",
             "player_current_movement_speed"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source)]
                   + [str(ROOT / p) for p in mobile_sources]
                   + ["@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                      "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    for w, h, scale in [(360,800,1), (800,360,1), (580,1280,1.5), (720,1600,2), (1600,720,2),
                         (1080,2400,2.75), (2400,1080,2.75), (1280,720,1)]:
        subprocess.run([str(exe), str(w), str(h), str(scale),
                        str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                        str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                       cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
