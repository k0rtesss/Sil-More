/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"

#include "sdl-menu/sdl-scene-menu.h"

static const app_ui_panel* sdl_menu_pick_ui_panel(const app_ui_scene* scene)
{
    u16b i;

    if (!scene)
        return NULL;

    for (i = 0; i < scene->panel_count; i++)
    {
        const app_ui_panel* panel = &scene->panels[i];

        if (panel->flags & APP_UI_PANEL_FLAG_ACTIVE)
            return panel;
    }

    return NULL;
}

static bool sdl_scene_ui_render_panel_direct(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* panel, u16b panel_index)
{
    u16b scene_kind;

    if (!main_view || !scene || !panel)
        return false;

    scene_kind = sdl_menu_hit_scene_kind();
    if (scene_kind == APP_SCENE_KIND_NONE)
        scene_kind = APP_SCENE_KIND_MENU;

    if (panel->style == APP_UI_PANEL_STYLE_STRIP)
    {
        if (!sdl_menu_render_strip_panel(main_view, canvas_w, canvas_h,
                panel))
        {
            log_warn("ui render: strip panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_STATUS_RAIL)
    {
        if (!sdl_menu_render_status_rail_panel(main_view, canvas_w,
                canvas_h, panel))
        {
            log_warn("ui render: status rail panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_OVERLAY_RAIL)
    {
        if (!sdl_menu_render_overlay_rail_panel(main_view, canvas_w,
                canvas_h, panel))
        {
            log_warn("ui render: overlay rail panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_WELCOME)
    {
        if (!sdl_menu_render_welcome_panel(main_view, canvas_w, canvas_h,
                panel))
        {
            log_warn("ui render: welcome panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_CHARACTER_SHEET)
    {
        if (!sdl_menu_render_character_sheet_panel(main_view, canvas_w,
                canvas_h, scene, panel))
        {
            log_warn("ui render: character sheet panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_MINIMAP)
    {
        if (!sdl_menu_render_minimap_panel(main_view, canvas_w, canvas_h,
                scene, panel))
        {
            log_warn("ui render: minimap panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (panel->style == APP_UI_PANEL_STYLE_BROWSER
        || (panel->style == APP_UI_PANEL_STYLE_ITEM_BROWSER
            && panel->layer != APP_UI_LAYER_TRANSIENT)
        || panel->style == APP_UI_PANEL_STYLE_CRAFTING
        || panel->style == APP_UI_PANEL_STYLE_MAP_RECALL)
    {
        if (!sdl_menu_render_browser_panel(main_view, canvas_w, canvas_h,
                scene, panel))
        {
            log_warn("ui render: browser panel failed (canvas=%dx%d rect=%dx%d)",
                canvas_w, canvas_h, main_view->rect.w, main_view->rect.h);
            return false;
        }
        return true;
    }

    if (!sdl_menu_render_panel_internal(main_view, canvas_w, canvas_h, scene,
            panel, scene_kind, panel_index))
    {
        log_warn("ui render: default panel failed (canvas=%dx%d rect=%dx%d style=%u)",
            canvas_w, canvas_h, main_view->rect.w, main_view->rect.h,
            panel->style);
        return false;
    }

    return true;
}

static bool sdl_scene_ui_render_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene, const app_ui_panel* panel,
    u16b panel_index)
{
    SDL_Texture* prior_target;
    SDL_Texture* fade_texture;

    if (!main_view || !scene || !panel)
        return false;
    if (panel->alpha >= 0xFFu || canvas_w <= 0 || canvas_h <= 0)
    {
        return sdl_scene_ui_render_panel_direct(main_view, canvas_w, canvas_h,
            scene, panel, panel_index);
    }
    if (panel->alpha == 0)
        return true;

    prior_target = SDL_GetRenderTarget(g_state.renderer);
    fade_texture = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, canvas_w, canvas_h);
    if (!fade_texture)
    {
        log_warn("ui render: fade texture create failed, falling back to direct render (%s)",
            SDL_GetError());
        return sdl_scene_ui_render_panel_direct(main_view, canvas_w, canvas_h,
            scene, panel, panel_index);
    }

    SDL_SetTextureBlendMode(fade_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_state.renderer, fade_texture);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_state.renderer);

    if (!sdl_scene_ui_render_panel_direct(main_view, canvas_w, canvas_h, scene,
            panel, panel_index))
    {
        SDL_SetRenderTarget(g_state.renderer, prior_target);
        SDL_DestroyTexture(fade_texture);
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, prior_target);
    SDL_SetTextureAlphaMod(fade_texture, panel->alpha);
    SDL_RenderTexture(g_state.renderer, fade_texture, NULL, &(SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)canvas_w,
        .h = (float)canvas_h
    });
    SDL_DestroyTexture(fade_texture);
    return true;
}

bool sdl_scene_ui_render_overlay(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene)
{
    const app_ui_panel* panel;
    u16b i;

    if (!main_view || !scene)
        return false;

    panel = sdl_menu_pick_ui_panel(scene);
    if (!panel)
        return false;

    if ((scene->flags & APP_UI_SCENE_FLAG_DIM_BACKDROP)
        && canvas_w > 0 && canvas_h > 0)
    {
        const sdl_ui_style* style = sdl_menu_panel_style(panel);
        SDL_Color scrim_color = style->shadow;

        scrim_color.a = 112;
        sdl_menu_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, (float)canvas_w,
            (float)canvas_h }, scrim_color);
    }

    for (i = 0; i < scene->panel_count; i++)
    {
        panel = &scene->panels[i];
        if (!(panel->flags & APP_UI_PANEL_FLAG_ACTIVE))
            continue;
        sdl_menu_hit_begin_panel(i, panel);
        if (!sdl_scene_ui_render_panel(main_view, canvas_w, canvas_h, scene,
                panel, i))
        {
            sdl_menu_hit_end_panel();
            return false;
        }
        sdl_menu_hit_end_panel();
    }

    sdl_menu_pointer_render_tooltip(canvas_w, canvas_h);
    return true;
}

bool sdl_scene_ui_render_at(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, int hit_origin_x, int hit_origin_y,
    u16b hit_view_index, const app_ui_scene* scene)
{
    if (!canvas || !main_view || !scene)
        return false;

    sdl_menu_hit_reset_for_view(hit_origin_x, hit_origin_y,
        hit_view_index);
    sdl_menu_hit_set_scene(APP_SCENE_KIND_MENU);

    SDL_SetRenderTarget(g_state.renderer, canvas);
    if ((scene->flags & APP_UI_SCENE_FLAG_USE_BACKDROP)
        && main_view->canvas && canvas_w > 0 && canvas_h > 0)
    {
        const sdl_ui_style* style = sdl_ui_style_for_panel(
            scene->panel_count > 0 ? scene->panels[0].style
                                   : APP_UI_PANEL_STYLE_DEFAULT);

        SDL_SetRenderDrawColor(g_state.renderer, style->canvas_fill.r,
            style->canvas_fill.g, style->canvas_fill.b, style->canvas_fill.a);
        SDL_RenderClear(g_state.renderer);
        SDL_RenderTexture(g_state.renderer, main_view->canvas, NULL,
            &(SDL_FRect){
                .x = (float)main_view->margin_x,
                .y = (float)main_view->margin_y,
                .w = (float)(main_view->cols * main_view->cell_w),
                .h = (float)(main_view->rows * main_view->cell_h)
            });
    }
    else
    {
        const sdl_ui_style* style = sdl_ui_style_for_panel(
            scene->panel_count > 0 ? scene->panels[0].style
                                   : APP_UI_PANEL_STYLE_DEFAULT);

        SDL_SetRenderDrawColor(g_state.renderer, style->canvas_fill.r,
            style->canvas_fill.g, style->canvas_fill.b, style->canvas_fill.a);
        SDL_RenderClear(g_state.renderer);
    }

    if (!sdl_scene_ui_render_overlay(main_view, canvas_w, canvas_h, scene))
    {
        SDL_SetRenderTarget(g_state.renderer, NULL);
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}

bool sdl_scene_ui_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene)
{
    if (!main_view)
        return false;

    return sdl_scene_ui_render_at(canvas, main_view, canvas_w, canvas_h,
        main_view->rect.x, main_view->rect.y, 0, scene);
}

bool sdl_scene_menu_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_menu_snapshot* snapshot)
{
    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_MENU)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_MENU
        || snapshot->blobs[0].size < sizeof(app_ui_scene))
    {
        return false;
    }

    return sdl_scene_ui_render(canvas, main_view, canvas_w, canvas_h,
        &snapshot->scene);
}
