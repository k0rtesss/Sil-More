#include "angband.h"
#include "sdl-main-internal.h"

static const char* const sdl_story_fallback_font = "lib/xtra/font/MarcellusSC-Regular.ttf";

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
        font = TTF_OpenFont(font_path, font_size);
        if (font) {
            sdl_apply_font_settings(font, true);
            return font;
        }
        log_warn("Failed to load custom font '%s': %s", font_path, SDL_GetError());
    }

    font = TTF_OpenFont(fallback_path, font_size);
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

    const char* font_path = (config.story_font[0] != '\0') ? config.story_font : NULL;
    TTF_Font* font = sdl_load_font_with_fallback(font_path, pixel_height, sdl_story_fallback_font);
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
    if (Term)
        Term->story_font_active = false;
}

void sdl_apply_story_font_state(bool active)
{
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (g_views[i].term_ready)
            g_views[i].t.story_font_active = active;
    }
    if (Term)
        Term->story_font_active = active;
}

void sdl_apply_story_grid_state(bool grid)
{
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (g_views[i].term_ready)
            g_views[i].t.story_font_grid = grid;
    }
    if (Term)
        Term->story_font_grid = grid;
}

void sdl_story_font_reset_state(void)
{
    g_state.story_font_depth = 0;
    sdl_apply_story_font_state(false);
    g_state.story_font_grid = false;
    sdl_apply_story_grid_state(false);
    if (Term)
        Term->story_chunk_active = false;
}

void sdl_story_font_enable(void)
{
    g_state.story_font_depth++;
    if (g_state.story_font_depth == 1)
        sdl_apply_story_font_state(true);
}

void sdl_story_font_disable(void)
{
    if (g_state.story_font_depth > 0)
        g_state.story_font_depth--;
    bool active = (g_state.story_font_depth > 0);
    sdl_apply_story_font_state(active);
    if (!active)
        sdl_story_font_set_grid(false);
}

bool sdl_is_story_font_enabled(void)
{
    return (Term && Term->story_font_active);
}

void sdl_story_font_set_grid(bool grid)
{
    if (g_state.story_font_grid == grid)
        return;
    g_state.story_font_grid = grid;
    sdl_apply_story_grid_state(grid);
}

bool sdl_is_story_font_grid(void)
{
    return (Term && Term->story_font_grid);
}

void sdl_story_font_reset(void)
{
    sdl_story_font_reset_state();
}

int sdl_story_font_text_width(cptr text, int len)
{
    if (!text)
        return 0;

    sdl_view* d = NULL;
    if (Term)
        d = sdl_view_from_term(Term);
    if (!d || !d->term_ready) {
        if (g_views[0].term_ready)
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

int sdl_get_cell_width(void)
{
    if (g_views[0].term_ready)
        return g_views[0].cell_w;
    return 8;
}

void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || n <= 0)
        return;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    memcpy(text_buf, s, len);
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return;

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)text_surface->h;
        float scale = (surf_h_f > cell_h_f && surf_h_f > 0.0f)
            ? (cell_h_f / surf_h_f) : 1.0f;
        SDL_FRect dst = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(text_surface->w) * scale,
            (float)(text_surface->h) * scale
        };
        float max_w = (float)(n * d->cell_w);
        SDL_Rect previous_clip;
        SDL_Rect clip = {
            x * d->cell_w,
            y * d->cell_h,
            (int)(max_w + 0.999f),
            (int)(dst.h + 0.999f)
        };
        bool had_clip = false;

        if (clip.w > 0 && clip.h > 0)
        {
            had_clip = SDL_GetRenderClipRect(g_state.renderer, &previous_clip);
            SDL_SetRenderClipRect(g_state.renderer, &clip);
        }
        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        if (clip.w > 0 && clip.h > 0)
            SDL_SetRenderClipRect(g_state.renderer,
                had_clip ? &previous_clip : NULL);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
}

int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col, float max_w_px)
{
    if (!d || !font || !s || n <= 0)
        return 0;

    char text_buf[256];
    int len = (n < 255) ? n : 255;
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        text_buf[i] = (ch ? (char)ch : ' ');
    }
    text_buf[len] = '\0';

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_buf, 0, col);
    if (!text_surface)
        return 0;

    int adv_w_unscaled = 0;
    TTF_MeasureString(font, text_buf, len, 0, &adv_w_unscaled, NULL);

    float cell_h_f = (float)d->cell_h;
    float surf_h_f = (float)text_surface->h;
    float scale = (surf_h_f > cell_h_f && surf_h_f > 0.0f)
        ? (cell_h_f / surf_h_f) : 1.0f;
    float advance_w = (float)adv_w_unscaled * scale;
    float render_w = (float)text_surface->w * scale;
    float render_h = (float)text_surface->h * scale;
    SDL_Rect clip = { 0, 0, 0, 0 };
    SDL_Rect previous_clip;

    if (max_w_px > 0.0f && advance_w > max_w_px)
        advance_w = max_w_px;
    if (max_w_px > 0.0f && render_w > max_w_px) {
        clip.x = (int)x_px;
        clip.y = y * d->cell_h;
        clip.w = (int)(max_w_px + 0.999f);
        clip.h = (int)(render_h + 0.999f);
    }

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(g_state.renderer, text_surface);
    if (text_texture) {
        bool had_clip = false;
        SDL_FRect dst = {
            x_px,
            (float)(y * d->cell_h),
            render_w,
            render_h
        };
        if (clip.w > 0 && clip.h > 0)
        {
            had_clip = SDL_GetRenderClipRect(g_state.renderer, &previous_clip);
            SDL_SetRenderClipRect(g_state.renderer, &clip);
        }
        SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, text_texture, NULL, &dst);
        if (clip.w > 0 && clip.h > 0)
            SDL_SetRenderClipRect(g_state.renderer,
                had_clip ? &previous_clip : NULL);
        SDL_DestroyTexture(text_texture);
    }

    SDL_DestroySurface(text_surface);
    return (int)advance_w;
}

static bool sdl_story_cell_is_text(byte a, char c)
{
    unsigned char uc = (unsigned char)c;

    if ((a & 0x80) && (uc & 0x80))
        return false;
    if (a == 255 && uc == 0xFF)
        return false;

    return true;
}

void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr)
{
    if (!d || !font || !Term || !story_row || !row_chars || !row_attr)
        return;

    const int wid = Term->wid;
    const float cell_w_f = (float)d->cell_w;
    const float cell_h_f = (float)d->cell_h;

    int x = 0;
    while (x < wid) {
        if (!sdl_story_cell_is_text(row_attr[x], row_chars[x])) {
            x++;
            continue;
        }

        byte flags = story_row[x];
        bool use_story = (flags & STORY_FLAG_USE) != 0;
        bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
        byte attr = row_attr[x];
        int run_start = x;

        if (!use_story) {
            while (x < wid) {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) != 0 || row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_FRect clear_rect = { run_start * cell_w_f, y * cell_h_f, run_len * cell_w_f, cell_h_f };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);

            SDL_Color col = {
                angband_color_table[attr][1],
                angband_color_table[attr][2],
                angband_color_table[attr][3],
                255
            };
            sdl_render_mono_text(d, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        if (grid_align) {
            while (x < wid) {
                if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                    break;
                byte f = story_row[x];
                if ((f & STORY_FLAG_USE) == 0 || (f & STORY_FLAG_CELL_ALIGN) == 0 || row_attr[x] != attr)
                    break;
                x++;
            }

            int run_len = x - run_start;
            SDL_FRect clear_rect = { run_start * cell_w_f, y * cell_h_f, run_len * cell_w_f, cell_h_f };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);

            SDL_Color col = {
                angband_color_table[attr][1],
                angband_color_table[attr][2],
                angband_color_table[attr][3],
                255
            };
            sdl_render_story_text_grid(d, font, run_start, y, run_len, row_chars + run_start, col);
            continue;
        }

        while (x < wid) {
            if (!sdl_story_cell_is_text(row_attr[x], row_chars[x]))
                break;
            byte f = story_row[x];
            if ((f & STORY_FLAG_USE) == 0 || (f & STORY_FLAG_CELL_ALIGN) != 0)
                break;
            x++;
        }

        int region_start = run_start;
        int region_end = x;
        if (region_end <= region_start)
            continue;

        SDL_FRect clear_rect = {
            region_start * cell_w_f, y * cell_h_f, (region_end - region_start) * cell_w_f, cell_h_f
        };
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &clear_rect);

        float px_cursor = region_start * cell_w_f;
        float px_end = region_end * cell_w_f;
        int seg = region_start;
        while (seg < region_end) {
            if (!sdl_story_cell_is_text(row_attr[seg], row_chars[seg])) {
                seg++;
                continue;
            }

            byte seg_attr = row_attr[seg];
            int seg_end = seg + 1;
            while (seg_end < region_end && sdl_story_cell_is_text(row_attr[seg_end], row_chars[seg_end])
                && row_attr[seg_end] == seg_attr) {
                seg_end++;
            }

            int seg_len = seg_end - seg;
            SDL_Color seg_col = {
                angband_color_table[seg_attr][1],
                angband_color_table[seg_attr][2],
                angband_color_table[seg_attr][3],
                255
            };

            float remaining = px_end - px_cursor;
            if (remaining <= 0.0f)
                break;

            int consumed = sdl_render_story_text_free_px(d, font, px_cursor, y, row_chars + seg, seg_len, seg_col,
                remaining);
            if (consumed <= 0)
                break;

            px_cursor += (float)consumed;
            seg = seg_end;
        }
    }
}

void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col)
{
    if (!d || !font || n <= 0)
        return;

    float cell_w_f = (float)d->cell_w;
    float cell_h_f = (float)d->cell_h;

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (!ch || ch == ' ')
            continue;

        char glyph_text[2] = { (char)ch, '\0' };
        SDL_Surface* glyph_surface = TTF_RenderText_Blended(font, glyph_text, 0, col);
        if (!glyph_surface)
            continue;

        SDL_Texture* glyph_texture = SDL_CreateTextureFromSurface(g_state.renderer, glyph_surface);
        if (glyph_texture) {
            float surf_w = (float)glyph_surface->w;
            float surf_h = (float)glyph_surface->h;
            float scale = (surf_h > 0.0f) ? (cell_h_f / surf_h) : 1.0f;
            float scaled_w = surf_w * scale;
            float dst_w = scaled_w;
            float offset_x = 0.0f;

            if (scaled_w > cell_w_f) {
                dst_w = cell_w_f;
                scale = (surf_w > 0.0f) ? (dst_w / surf_w) : 1.0f;
            } else {
                offset_x = (cell_w_f - scaled_w) * 0.5f;
            }

            SDL_FRect dst = {
                (float)((x + i) * d->cell_w) + offset_x,
                (float)(y * d->cell_h),
                dst_w,
                cell_h_f
            };

            SDL_SetTextureBlendMode(glyph_texture, SDL_BLENDMODE_BLEND);
            SDL_RenderTexture(g_state.renderer, glyph_texture, NULL, &dst);
            SDL_DestroyTexture(glyph_texture);
        }

        SDL_DestroySurface(glyph_surface);
    }
}
