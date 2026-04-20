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

#include "sdl-main-internal.h"

static int sdl_bootstrap_scale_px(float logical_value)
{
    float scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;

    return (int)(logical_value * scale + 0.5f);
}

static SDL_Color sdl_bootstrap_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static int sdl_bootstrap_measure_text(TTF_Font* font, cptr text)
{
    int measured_w = 0;

    if (!font || !text || !text[0])
        return 0;

    if (!TTF_MeasureString(font, text, 0, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

static void sdl_bootstrap_render_text(TTF_Font* font, float x_px, float y_px,
    SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;

    if (!font || !text || !text[0])
        return;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static int sdl_bootstrap_cell_width(int line_h)
{
    int cell_w;

    if (line_h <= 0)
        return 1;

    cell_w = (int)(line_h * 0.57f + 0.5f);
    if (cell_w < 1)
        cell_w = 1;
    return cell_w;
}

static bool sdl_bootstrap_choose_layout(const sdl_view* main_view,
    const app_bootstrap_scene* scene, TTF_Font** out_font, int* out_line_h,
    int* out_cell_w, int* out_base_x)
{
    static const int logical_sizes[] = { 28, 26, 24, 22, 20, 18, 16, 14 };
    int canvas_w;
    int canvas_h;
    int logical_cols;
    int margin_x;
    size_t i;
    TTF_Font* fallback_font = NULL;
    int fallback_line_h = 0;
    int fallback_cell_w = 0;
    int fallback_base_x = 0;

    if (!main_view || !scene || !out_font || !out_line_h || !out_cell_w
        || !out_base_x)
    {
        return false;
    }

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    logical_cols = scene->logical_cols > 0 ? scene->logical_cols
        : APP_BOOTSTRAP_LOGICAL_COLS;
    margin_x = sdl_bootstrap_scale_px(12.0f);

    for (i = 0; i < N_ELEMENTS(logical_sizes); i++)
    {
        int pixel_height = sdl_bootstrap_scale_px((float)logical_sizes[i]);
        TTF_Font* font = sdl_story_font_for_height(pixel_height);
        int line_h;
        int cell_w;
        int design_w;
        int max_right = 0;
        int top_extent = 0;
        int bottom_extent = 0;
        u16b op_index;

        if (!font)
            continue;

        line_h = TTF_GetFontHeight(font);
        if (line_h <= 0)
            line_h = pixel_height;
        cell_w = sdl_bootstrap_cell_width(line_h);
        design_w = logical_cols * cell_w;

        for (op_index = 0; op_index < scene->op_count; op_index++)
        {
            const app_bootstrap_op* op = &scene->ops[op_index];
            int line_right;

            if (!op->text[0])
                continue;

            line_right = op->col * cell_w + sdl_bootstrap_measure_text(font,
                op->text);
            if (line_right > max_right)
                max_right = line_right;

            if (op->flags & APP_BOOTSTRAP_OP_FLAG_BOTTOM_ANCHORED)
            {
                int extent = (op->row + 1) * line_h;

                if (extent > bottom_extent)
                    bottom_extent = extent;
            }
            else
            {
                int extent = (op->row + 1) * line_h;

                if (extent > top_extent)
                    top_extent = extent;
            }
        }

        design_w = MAX(design_w, max_right);
        fallback_font = font;
        fallback_line_h = line_h;
        fallback_cell_w = cell_w;
        fallback_base_x = (canvas_w - design_w) / 2;

        if (design_w + margin_x * 2 > canvas_w)
            continue;
        if (top_extent + bottom_extent > canvas_h)
            continue;

        *out_font = font;
        *out_line_h = line_h;
        *out_cell_w = cell_w;
        *out_base_x = fallback_base_x;
        return true;
    }

    if (!fallback_font)
        return false;

    *out_font = fallback_font;
    *out_line_h = fallback_line_h;
    *out_cell_w = fallback_cell_w;
    *out_base_x = fallback_base_x;
    return true;
}

bool sdl_scene_bootstrap_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_bootstrap_snapshot* snapshot)
{
    TTF_Font* font;
    int canvas_h;
    int line_h;
    int cell_w;
    int base_x;
    u16b i;

    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_BOOTSTRAP)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_BOOTSTRAP
        || snapshot->blobs[0].size < sizeof(app_bootstrap_scene))
    {
        return false;
    }

    if (!sdl_bootstrap_choose_layout(main_view, &snapshot->scene, &font,
            &line_h, &cell_w, &base_x))
    {
        return false;
    }

    canvas_h = main_view->rows * main_view->cell_h;
    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (i = 0; i < snapshot->scene.op_count; i++)
    {
        const app_bootstrap_op* op = &snapshot->scene.ops[i];
        int y_px = (op->flags & APP_BOOTSTRAP_OP_FLAG_BOTTOM_ANCHORED)
            ? (canvas_h - ((op->row + 1) * line_h))
            : (op->row * line_h);
        int x_px = base_x + (op->col * cell_w);

        if (!op->text[0])
            continue;
        sdl_bootstrap_render_text(font, (float)x_px, (float)y_px,
            sdl_bootstrap_color(op->attr), op->text);
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
