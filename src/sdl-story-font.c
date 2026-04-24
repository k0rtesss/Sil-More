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
#define ANGBAND_NO_IO_COMPAT
#include "fs/io_sdl.h"
#undef ANGBAND_NO_IO_COMPAT
#include "fs/resource.h"
#include "platform-story-font.h"
#include "sdl-main-internal.h"

static const char* const sdl_story_fallback_font = "font/MarcellusSC-Regular.ttf";

static void sdl_apply_font_settings(TTF_Font* font, bool is_story_font)
{
    bool bold = is_story_font ? config.story_bold : config.mono_bold;
    bool italic = is_story_font ? config.story_italic : config.mono_italic;
    bool underline = is_story_font ? config.story_underline : config.mono_underline;
    bool strikethrough = is_story_font ? config.story_strikethrough : config.mono_strikethrough;
    int hinting = is_story_font ? config.story_hinting : config.mono_hinting;
    bool kerning = is_story_font ? config.story_kerning : config.mono_kerning;
    int outline = is_story_font ? config.story_outline : config.mono_outline;
    int style = TTF_STYLE_NORMAL;

    if (bold)
        style |= TTF_STYLE_BOLD;
    if (italic)
        style |= TTF_STYLE_ITALIC;
    if (underline)
        style |= TTF_STYLE_UNDERLINE;
    if (strikethrough)
        style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL)
        TTF_SetFontStyle(font, style);

    TTF_SetFontHinting(font, hinting);
    TTF_SetFontKerning(font, kerning);
    if (outline > 0)
        TTF_SetFontOutline(font, outline);
}

static TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path)
{
    TTF_Font* font = NULL;

    if (font_path && font_path[0] != '\0') {
        ang_file* stream = sdl_fopen(font_path, "rb");
        if (stream)
            font = TTF_OpenFontIO(stream, true, (float)font_size);
        if (font) {
            sdl_apply_font_settings(font, true);
            return font;
        }
        log_warn("Failed to load custom font '%s': %s", font_path, SDL_GetError());
    }

    {
        ang_file* stream = sdl_fopen(fallback_path, "rb");
        if (stream)
            font = TTF_OpenFontIO(stream, true, (float)font_size);
    }
    if (font) {
        sdl_apply_font_settings(font, true);
    } else {
        log_error("Failed to load fallback font '%s': %s", fallback_path, SDL_GetError());
    }

    return font;
}

void sdl_story_font_cache_clear(void)
{
    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].font) {
            TTF_CloseFont(g_state.story_fonts[i].font);
            g_state.story_fonts[i].font = NULL;
        }
        g_state.story_fonts[i].pixel_height = 0;
    }
    g_state.story_font_count = 0;
}

TTF_Font* sdl_story_font_for_height(int pixel_height)
{
    if (pixel_height <= 0)
        return NULL;

    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].pixel_height == pixel_height)
            return g_state.story_fonts[i].font;
    }

    if (g_state.story_font_count >= MAX_STORY_FONT_CACHE) {
        log_warn("Story font cache full; reusing size %d", g_state.story_fonts[0].pixel_height);
        return g_state.story_fonts[0].font;
    }

    char font_path_buf[1024];
    char fallback_path_buf[1024];
    const char* font_path = NULL;
    TTF_Font* font;

    if (!resource_resolve_xtra_path(fallback_path_buf, sizeof(fallback_path_buf),
            NULL, sdl_story_fallback_font))
    {
        log_error("Failed to resolve story font fallback path");
        return NULL;
    }

    if (config.story_font[0] != '\0'
        && resource_resolve_xtra_path(font_path_buf, sizeof(font_path_buf),
            config.story_font, NULL))
    {
        font_path = font_path_buf;
    }

    font = sdl_load_font_with_fallback(font_path, pixel_height,
        fallback_path_buf);
    if (!font)
        return NULL;

    g_state.story_fonts[g_state.story_font_count].pixel_height = pixel_height;
    g_state.story_fonts[g_state.story_font_count].font = font;
    g_state.story_font_count++;
    return font;
}

TTF_Font* sdl_story_font_for_view(const sdl_view* d)
{
    if (!d)
        return NULL;
    return sdl_story_font_for_height(d->cell_h);
}

void sdl_load_story_fonts(void)
{
    int main_cell_h = config.main_view_scale * TILE_SIZE;
    int pane_cell_widths[PANE_MAX] = { 0 };
    int pane_cell_heights[PANE_MAX] = { 0 };

    sdl_story_font_cache_clear();
    (void)sdl_story_font_for_height(main_cell_h);
    sdl_build_supporting_pane_metrics(pane_config, pane_config_count,
        pane_cell_widths, pane_cell_heights);
    for (int i = 1; i < PANE_MAX; i++) {
        if (pane_cell_heights[i] > 0)
            (void)sdl_story_font_for_height(pane_cell_heights[i]);
    }

    g_state.story_font_depth = 0;
}

void platform_story_font_enable(void)
{
    g_state.story_font_depth++;
}

void platform_story_font_disable(void)
{
    if (g_state.story_font_depth > 0)
        g_state.story_font_depth--;
}

bool platform_story_font_enabled(void)
{
    return g_state.story_font_depth > 0;
}

void platform_story_font_reset(void)
{
    g_state.story_font_depth = 0;
}

int platform_story_font_text_width(cptr text, int len)
{
    if (!text)
        return 0;

    sdl_view* d = NULL;
    if (platform_frame_view_ready(sdl_active_view_index()))
        d = &g_views[sdl_active_view_index()];
    if (!d || !d->ready) {
        if (g_views[0].ready)
            d = &g_views[0];
    }
    if (!d)
        return 0;

    TTF_Font* font = sdl_story_font_for_view(d);
    if (!font)
        return 0;

    int w = 0;
    TTF_MeasureString(font, text, len, 0, &w, NULL);

    int font_h = TTF_GetFontHeight(font);
    if (font_h > d->cell_h) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)font_h;
        float scale = cell_h_f / surf_h_f;
        w = (int)((float)w * scale);
    }

    return w;
}

int platform_story_font_cell_width(void)
{
    if (g_views[0].ready)
        return g_views[0].cell_w;
    return 8;
}
