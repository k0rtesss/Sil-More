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
    const app_ui_panel* panel)
{
    if (!main_view || !scene || !panel)
        return false;

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

    if (panel->style == APP_UI_PANEL_STYLE_BROWSER)
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
            panel))
    {
        log_warn("ui render: default panel failed (canvas=%dx%d rect=%dx%d style=%u)",
            canvas_w, canvas_h, main_view->rect.w, main_view->rect.h,
            panel->style);
        return false;
    }

    return true;
}

static bool sdl_scene_ui_render_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene, const app_ui_panel* panel)
{
    SDL_Texture* prior_target;
    SDL_Texture* fade_texture;

    if (!main_view || !scene || !panel)
        return false;
    if (panel->alpha >= 0xFFu || canvas_w <= 0 || canvas_h <= 0)
    {
        return sdl_scene_ui_render_panel_direct(main_view, canvas_w, canvas_h,
            scene, panel);
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
            scene, panel);
    }

    SDL_SetTextureBlendMode(fade_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_state.renderer, fade_texture);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_state.renderer);

    if (!sdl_scene_ui_render_panel_direct(main_view, canvas_w, canvas_h, scene,
            panel))
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
    SDL_Color scrim_color = { 0, 0, 0, 112 };
    u16b i;

    if (!main_view || !scene)
        return false;

    panel = sdl_menu_pick_ui_panel(scene);
    if (!panel)
        return false;

    if ((scene->flags & APP_UI_SCENE_FLAG_DIM_BACKDROP)
        && canvas_w > 0 && canvas_h > 0)
    {
        sdl_menu_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, (float)canvas_w,
            (float)canvas_h }, scrim_color);
    }

    for (i = 0; i < scene->panel_count; i++)
    {
        panel = &scene->panels[i];
        if (!(panel->flags & APP_UI_PANEL_FLAG_ACTIVE))
            continue;
        if (!sdl_scene_ui_render_panel(main_view, canvas_w, canvas_h, scene,
                panel))
            return false;
    }

    return true;
}

bool sdl_scene_ui_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene)
{
    if (!canvas || !main_view || !scene)
        return false;

    sdl_menu_hit_reset(main_view->rect.x, main_view->rect.y);

    SDL_SetRenderTarget(g_state.renderer, canvas);
    if ((scene->flags & APP_UI_SCENE_FLAG_USE_BACKDROP)
        && main_view->canvas && canvas_w > 0 && canvas_h > 0)
    {
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
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
        SDL_SetRenderDrawColor(g_state.renderer, 6, 10, 14, 255);
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
