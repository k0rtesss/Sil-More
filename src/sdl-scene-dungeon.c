#include "angband.h"

#include "sdl-main-internal.h"

typedef struct sdl_scene_layout {
    bool compact_height;
    bool hide_left_panel;
    int row_map;
    int col_map;
    int bottom_row;
    int row_name;
    int row_stat;
    int row_exp;
    int row_hp;
    int row_sp;
    int row_light;
    int row_mel;
    int row_arc;
    int row_quiver;
    int row_evn;
    int row_info;
    int row_cut;
    int row_song;
    int col_hungry;
    int col_blind;
    int col_confused;
    int col_stun;
    int col_afraid;
    int col_state;
    int col_speed;
    int col_terrain;
    int col_depth;
} sdl_scene_layout;

typedef struct sdl_scene_ui_font_cache {
    TTF_Font* font;
    int pixel_height;
    int style_signature;
    char path[1024];
} sdl_scene_ui_font_cache;

typedef struct sdl_scene_panel_metrics {
    int pixel_height;
    int cell_w;
    int cell_h;
    int panel_w;
    int panel_h;
} sdl_scene_panel_metrics;

static sdl_scene_ui_font_cache g_sdl_scene_ui_font_cache;

static void sdl_scene_draw_tile(SDL_Texture* tileset, byte attr, byte ch,
    const SDL_FRect* dst);
static void sdl_scene_render_fixed_panel_glyph(TTF_Font* font, float x_px,
    float y_px, int cell_w, int cell_h, SDL_Color color, char ch);

static SDL_Color sdl_scene_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static byte sdl_scene_alpha_byte(float alpha)
{
    if (alpha <= 0.0f)
        return 0;
    if (alpha >= 1.0f)
        return 255;
    return (byte)(alpha * 255.0f + 0.5f);
}

static float sdl_scene_progress(Uint64 now_ns, const sdl_scene_animation* anim)
{
    Uint64 elapsed_ns;

    if (!anim || !anim->active || anim->duration_ns == 0)
        return 1.0f;

    if (now_ns <= anim->started_ns)
        return 0.0f;

    elapsed_ns = now_ns - anim->started_ns;
    if (elapsed_ns >= anim->duration_ns)
        return 1.0f;

    return (float)elapsed_ns / (float)anim->duration_ns;
}

static void sdl_scene_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

static void sdl_scene_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static int sdl_scene_ui_scale_px(float logical_value)
{
    float scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;

    return (int)(logical_value * scale + 0.5f);
}

static const char* sdl_scene_ui_font_path(void)
{
    return config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/InputMono-Bold.ttf";
}

static int sdl_scene_ui_font_signature(void)
{
    int signature = 0;

    signature |= config.mono_bold ? 0x0001 : 0;
    signature |= config.mono_italic ? 0x0002 : 0;
    signature |= config.mono_underline ? 0x0004 : 0;
    signature |= config.mono_strikethrough ? 0x0008 : 0;
    signature |= (config.mono_hinting & 0xFF) << 8;
    signature |= config.mono_kerning ? 0x10000 : 0;
    signature |= (config.mono_outline & 0xFF) << 17;

    return signature;
}

static void sdl_scene_ui_apply_font_settings(TTF_Font* font)
{
    int style = TTF_STYLE_NORMAL;

    if (!font)
        return;

    if (config.mono_bold)
        style |= TTF_STYLE_BOLD;
    if (config.mono_italic)
        style |= TTF_STYLE_ITALIC;
    if (config.mono_underline)
        style |= TTF_STYLE_UNDERLINE;
    if (config.mono_strikethrough)
        style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL)
        TTF_SetFontStyle(font, style);

    TTF_SetFontHinting(font, config.mono_hinting);
    TTF_SetFontKerning(font, config.mono_kerning);
    if (config.mono_outline > 0)
        TTF_SetFontOutline(font, config.mono_outline);
}

static void sdl_scene_ui_font_clear(void)
{
    if (g_sdl_scene_ui_font_cache.font)
    {
        TTF_CloseFont(g_sdl_scene_ui_font_cache.font);
        g_sdl_scene_ui_font_cache.font = NULL;
    }

    g_sdl_scene_ui_font_cache.pixel_height = 0;
    g_sdl_scene_ui_font_cache.style_signature = 0;
    g_sdl_scene_ui_font_cache.path[0] = '\0';
}

static TTF_Font* sdl_scene_ui_font_for_height(int pixel_height)
{
    const char* font_path = sdl_scene_ui_font_path();
    int style_signature = sdl_scene_ui_font_signature();

    if (pixel_height <= 0)
        return NULL;

    if (g_sdl_scene_ui_font_cache.font
        && g_sdl_scene_ui_font_cache.pixel_height == pixel_height
        && g_sdl_scene_ui_font_cache.style_signature == style_signature
        && streq(g_sdl_scene_ui_font_cache.path, font_path))
    {
        return g_sdl_scene_ui_font_cache.font;
    }

    sdl_scene_ui_font_clear();

    g_sdl_scene_ui_font_cache.font = TTF_OpenFont(font_path, pixel_height);
    if (!g_sdl_scene_ui_font_cache.font)
    {
        log_warn("sdl scene UI font load failed for '%s': %s", font_path,
            SDL_GetError());
        return NULL;
    }

    sdl_scene_ui_apply_font_settings(g_sdl_scene_ui_font_cache.font);
    g_sdl_scene_ui_font_cache.pixel_height = pixel_height;
    g_sdl_scene_ui_font_cache.style_signature = style_signature;
    SDL_strlcpy(g_sdl_scene_ui_font_cache.path, font_path,
        sizeof(g_sdl_scene_ui_font_cache.path));
    return g_sdl_scene_ui_font_cache.font;
}

static int sdl_scene_interaction_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

static int sdl_scene_measure_ui_text(TTF_Font* font, cptr text)
{
    int measured_w = 0;

    if (!font || !text || !text[0])
        return 0;

    if (!TTF_MeasureString(font, text, 0, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

static int sdl_scene_measure_font_text_n(TTF_Font* font, cptr text, size_t len)
{
    int measured_w = 0;

    if (!font || !text || len == 0)
        return 0;

    if (!TTF_MeasureString(font, text, len, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

static void sdl_scene_render_ui_text(TTF_Font* font, float x_px, float y_px,
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

static int sdl_scene_render_text_run_px(TTF_Font* font, float x_px, float y_px,
    SDL_Color color, cptr text, size_t len, int target_h, float max_w_px)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    char buf[APP_DUNGEON_PANE_TEXT_MAX + 1];
    int advance_w;
    size_t copy_len = len;

    if (!font || !text || len == 0)
        return 0;

    if (copy_len >= sizeof(buf))
        copy_len = sizeof(buf) - 1;
    memcpy(buf, text, copy_len);
    buf[copy_len] = '\0';

    surface = TTF_RenderText_Blended(font, buf, 0, color);
    if (!surface)
        return 0;

    advance_w = sdl_scene_measure_font_text_n(font, buf, copy_len);
    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    if (target_h > 0 && surface->h > 0 && surface->h != target_h)
    {
        dst.h = (float)target_h;
    }

    if (max_w_px > 0.0f)
    {
        if (dst.w > max_w_px)
            dst.w = max_w_px;
        if ((float)advance_w > max_w_px)
            advance_w = (int)max_w_px;
    }

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (texture)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }

    SDL_DestroySurface(surface);
    return advance_w;
}

static void sdl_scene_render_fixed_panel_text_cells(float x_px, float y_px,
    int cell_w, int cell_h, TTF_Font* font, SDL_Color color, cptr text,
    size_t len)
{
    size_t i;

    if (!font || !text || len == 0 || cell_w <= 0 || cell_h <= 0)
        return;

    for (i = 0; i < len; i++)
    {
        sdl_scene_render_fixed_panel_glyph(font, x_px + (float)(i * cell_w),
            y_px, cell_w, cell_h, color, text[i]);
    }
}

static void sdl_scene_render_fixed_panel_glyph(TTF_Font* font, float x_px,
    float y_px, int cell_w, int cell_h, SDL_Color color, char ch)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    char glyph[2] = { ch ? ch : ' ', '\0' };
    float dst_w;

    if (!font || !glyph[0] || glyph[0] == ' ' || cell_w <= 0 || cell_h <= 0)
        return;

    surface = TTF_RenderText_Blended(font, glyph, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    dst_w = (float)surface->w;
    if (dst_w > (float)cell_w)
        dst_w = (float)cell_w;

    dst.x = x_px + ((float)cell_w - dst_w) * 0.5f;
    dst.y = y_px;
    dst.w = dst_w;
    dst.h = (float)cell_h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static bool sdl_scene_left_panel_metrics_for_height(const sdl_view* view,
    const sdl_scene_layout* layout, int cols, int rows, int pixel_height,
    sdl_scene_panel_metrics* metrics, TTF_Font** out_story_font,
    TTF_Font** out_mono_font)
{
    TTF_Font* story_font;
    TTF_Font* mono_font;
    int available_w;
    int available_h;
    int cell_w;
    int cell_h;

    if (!view || !layout || !metrics || !out_story_font || !out_mono_font)
        return false;
    if (cols <= 0 || rows <= 0 || pixel_height <= 0)
        return false;

    available_w = layout->col_map * view->cell_w;
    available_h = view->rows * view->cell_h;
    if (available_w <= 0 || available_h <= 0)
        return false;

    story_font = sdl_story_font_for_height(pixel_height);
    mono_font = sdl_scene_ui_font_for_height(pixel_height);
    if (!story_font || !mono_font)
        return false;

    cell_h = pixel_height;
    cell_w = MAX(1,
        (view->cell_w * pixel_height + MAX(1, view->cell_h) - 1)
            / MAX(1, view->cell_h));

    if ((cols * cell_w) > available_w || (rows * cell_h) > available_h)
        return false;

    metrics->pixel_height = pixel_height;
    metrics->cell_w = cell_w;
    metrics->cell_h = cell_h;
    metrics->panel_w = cols * cell_w;
    metrics->panel_h = rows * cell_h;
    *out_story_font = story_font;
    *out_mono_font = mono_font;
    return true;
}

static bool sdl_scene_resolve_left_panel_metrics(const sdl_view* view,
    const sdl_scene_layout* layout, int cols, int rows,
    sdl_scene_panel_metrics* metrics, TTF_Font** out_story_font,
    TTF_Font** out_mono_font)
{
    int desired_px;
    int min_px;
    int pixel_height;

    if (!view || !layout || !metrics || !out_story_font || !out_mono_font)
        return false;
    if (cols <= 0 || rows <= 0)
        return false;

    desired_px = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    min_px = sdl_scene_ui_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        if (sdl_scene_left_panel_metrics_for_height(view, layout, cols, rows,
                pixel_height, metrics, out_story_font, out_mono_font))
        {
            return true;
        }
    }

    return false;
}

static int sdl_scene_left_panel_max_reserved_cols(const sdl_view* view,
    const sdl_scene_layout* layout)
{
    int min_map_cols;
    int max_reserved_cols;

    if (!view || !layout || view->cols <= 1)
        return layout ? layout->col_map : 0;

    min_map_cols = (view->cols < 60) ? 20 : 30;
    if (view->cols <= min_map_cols)
        min_map_cols = MAX(1, view->cols / 2);

    max_reserved_cols = view->cols - min_map_cols;
    if (max_reserved_cols < layout->col_map)
        max_reserved_cols = layout->col_map;
    if (max_reserved_cols > view->cols - 1)
        max_reserved_cols = view->cols - 1;
    if (max_reserved_cols < 1)
        max_reserved_cols = 1;

    return max_reserved_cols;
}

static int sdl_scene_left_panel_reserved_cols(const sdl_view* view,
    const sdl_scene_layout* layout, const app_panes_snapshot* panes)
{
    TTF_Font* story_font = NULL;
    TTF_Font* mono_font = NULL;
    sdl_scene_layout probe_layout;
    sdl_scene_panel_metrics metrics;
    int max_reserved_cols;
    int rows;
    int cols;
    int reserved_cols;

    if (!view || !layout || !panes || view->cell_w <= 0)
        return layout ? layout->col_map : 0;
    if (layout->hide_left_panel || panes->left_panel_rows == 0
        || panes->left_panel_cols == 0)
    {
        return layout->col_map;
    }

    rows = panes->left_panel_rows;
    cols = panes->left_panel_cols;
    if (rows > view->rows)
        rows = view->rows;
    if (rows <= 0 || cols <= 0)
        return layout->col_map;

    max_reserved_cols = sdl_scene_left_panel_max_reserved_cols(view, layout);
    probe_layout = *layout;
    probe_layout.col_map = max_reserved_cols;

    if (!sdl_scene_resolve_left_panel_metrics(view, &probe_layout, cols, rows,
            &metrics, &story_font, &mono_font))
    {
        return layout->col_map;
    }

    reserved_cols = (metrics.panel_w + view->cell_w - 1) / view->cell_w;
    if (reserved_cols < 1)
        reserved_cols = 1;
    if (reserved_cols > max_reserved_cols)
        reserved_cols = max_reserved_cols;

    return reserved_cols;
}

static void sdl_scene_interaction_format_line(char* line, size_t line_size,
    const app_interaction_state* interaction,
    const app_interaction_option* option)
{
    bool plain_list;

    if (!line || !line_size)
        return;

    line[0] = '\0';
    if (!interaction || !option)
        return;

    plain_list = (interaction->flags & APP_INTERACTION_FLAG_PLAIN_LIST) != 0;
    if (plain_list)
    {
        if (option->meta[0])
            strnfmt(line, line_size, "%s %s", option->label, option->meta);
        else
            SDL_strlcpy(line, option->label, line_size);
        return;
    }

    if (option->meta[0])
    {
        if (option->tag)
            strnfmt(line, line_size, "%c %s  %s", option->tag, option->label,
                option->meta);
        else
            strnfmt(line, line_size, "%s  %s", option->label, option->meta);
    }
    else
    {
        if (option->tag)
            strnfmt(line, line_size, "%c %s", option->tag, option->label);
        else
            SDL_strlcpy(line, option->label, line_size);
    }
}

static bool sdl_scene_layout_cell_rect(const sdl_view* view, int col, int row,
    int width_cells, SDL_FRect* out_rect)
{
    if (!view || !out_rect || col < 0 || row < 0 || width_cells <= 0)
        return false;
    if (col >= view->cols || row >= view->rows)
        return false;
    if (col + width_cells > view->cols)
        width_cells = view->cols - col;
    if (width_cells <= 0)
        return false;

    *out_rect = (SDL_FRect){
        .x = (float)(col * view->cell_w),
        .y = (float)(row * view->cell_h),
        .w = (float)(width_cells * view->cell_w),
        .h = (float)view->cell_h
    };
    return true;
}

static void sdl_scene_draw_text(const sdl_view* view, int col, int row,
    byte attr, cptr text)
{
    size_t len;

    if (!view || !text || !text[0])
        return;
    if (row < 0 || row >= view->rows || col >= view->cols)
        return;
    if (col < 0)
        col = 0;

    len = strlen(text);
    if (len == 0)
        return;
    if ((size_t)col + len > (size_t)view->cols)
        len = (size_t)(view->cols - col);
    if (len == 0)
        return;

    sdl_render_mono_text((sdl_view*)view, col, row, (int)len, text,
        sdl_scene_color(attr));
}

static void sdl_scene_draw_text_snapshot(const sdl_view* view, int col, int row,
    const app_text_snapshot* text)
{
    if (!text || !text->active || !text->text[0])
        return;

    sdl_scene_draw_text(view, col, row, text->attr, text->text);
}

static bool sdl_scene_story_cell_is_text(byte attr, char ch)
{
    unsigned char uch = (unsigned char)ch;

    if ((attr & 0x80) && (uch & 0x80))
        return false;
    if (attr == 255 && uch == 0xFF)
        return false;

    return true;
}

static void sdl_scene_render_packed_story_row(const sdl_view* view,
    TTF_Font* font, int col_offset, int row, int width,
    const app_panel_cell_snapshot* cells)
{
    const float cell_h_f = (float)view->cell_h;
    int x = 0;

    if (!view || !cells || width <= 0 || row < 0 || row >= view->rows)
        return;

    while (x < width)
    {
        byte flags;
        bool use_story;
        bool grid_align;
        byte attr;
        int run_start;

        if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
        {
            SDL_FRect dst;
            byte ch = (byte)cells[x].ch;

            if (sdl_scene_layout_cell_rect(view, col_offset + x, row, 1, &dst))
            {
                sdl_scene_fill_rect(&dst, (SDL_Color){ 0, 0, 0, 255 });

                if (g_state.use_tiles && g_state.tileset
                    && (cells[x].attr & TILE_FLAG) && (ch & TILE_FLAG))
                {
                    SDL_FRect src = {
                        .x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE),
                        .y = (float)(TILE_GET_INDEX(cells[x].attr) * TILE_SIZE),
                        .w = (float)TILE_SIZE,
                        .h = (float)TILE_SIZE
                    };

                    SDL_RenderTexture(g_state.renderer, g_state.tileset, &src,
                        &dst);
                }
                else if (cells[x].ch)
                {
                    char glyph[2] = { cells[x].ch, '\0' };

                    sdl_render_mono_text((sdl_view*)view, col_offset + x, row,
                        1, glyph, sdl_scene_color(cells[x].attr));
                }
            }

            x++;
            continue;
        }

        flags = cells[x].story;
        use_story = (flags & STORY_FLAG_USE) != 0;
        grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
        attr = cells[x].attr;
        run_start = x;

        if (!use_story || !font)
        {
            char text[APP_DUNGEON_LEFT_PANEL_COLS + 1];
            int run_len = 0;
            SDL_Color color = sdl_scene_color(attr);

            while (x < width)
            {
                if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                    break;
                if (((cells[x].story & STORY_FLAG_USE) != 0 && font)
                    || cells[x].attr != attr)
                {
                    break;
                }

                if (run_len >= (int)APP_DUNGEON_LEFT_PANEL_COLS)
                    break;
                text[run_len++] = cells[x].ch ? cells[x].ch : ' ';
                x++;
            }
            text[run_len] = '\0';

            sdl_scene_fill_rect(&(SDL_FRect){
                .x = (float)((col_offset + run_start) * view->cell_w),
                .y = (float)(row * view->cell_h),
                .w = (float)(run_len * view->cell_w),
                .h = cell_h_f
            }, (SDL_Color){ 0, 0, 0, 255 });

            sdl_render_mono_text((sdl_view*)view, col_offset + run_start, row,
                run_len, text, color);
            continue;
        }

        if (grid_align)
        {
            char text[APP_DUNGEON_LEFT_PANEL_COLS + 1];
            int run_len = 0;
            SDL_Color color = sdl_scene_color(attr);

            while (x < width)
            {
                if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                    break;
                if ((cells[x].story & STORY_FLAG_USE) == 0
                    || (cells[x].story & STORY_FLAG_CELL_ALIGN) == 0
                    || cells[x].attr != attr)
                {
                    break;
                }

                if (run_len >= (int)APP_DUNGEON_LEFT_PANEL_COLS)
                    break;
                text[run_len++] = cells[x].ch ? cells[x].ch : ' ';
                x++;
            }
            text[run_len] = '\0';

            sdl_scene_fill_rect(&(SDL_FRect){
                .x = (float)((col_offset + run_start) * view->cell_w),
                .y = (float)(row * view->cell_h),
                .w = (float)(run_len * view->cell_w),
                .h = cell_h_f
            }, (SDL_Color){ 0, 0, 0, 255 });

            sdl_render_story_text_grid((sdl_view*)view, font,
                col_offset + run_start, row, run_len, text, color);
            continue;
        }

        while (x < width)
        {
            if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                break;
            if ((cells[x].story & STORY_FLAG_USE) == 0
                || (cells[x].story & STORY_FLAG_CELL_ALIGN) != 0)
            {
                break;
            }
            x++;
        }

        {
            int region_start = run_start;
            int region_end = x;
            float px_cursor = (float)((col_offset + region_start) * view->cell_w);
            float px_end = (float)((col_offset + region_end) * view->cell_w);
            int seg = region_start;

            sdl_scene_fill_rect(&(SDL_FRect){
                .x = (float)((col_offset + region_start) * view->cell_w),
                .y = (float)(row * view->cell_h),
                .w = (float)((region_end - region_start) * view->cell_w),
                .h = cell_h_f
            }, (SDL_Color){ 0, 0, 0, 255 });

            while (seg < region_end)
            {
                char text[APP_DUNGEON_LEFT_PANEL_COLS + 1];
                byte seg_attr;
                SDL_Color seg_color;
                int seg_end = seg + 1;
                int seg_len;

                if (!sdl_scene_story_cell_is_text(cells[seg].attr, cells[seg].ch))
                {
                    seg++;
                    continue;
                }

                seg_attr = cells[seg].attr;
                while (seg_end < region_end
                    && sdl_scene_story_cell_is_text(cells[seg_end].attr,
                        cells[seg_end].ch)
                    && cells[seg_end].attr == seg_attr)
                {
                    seg_end++;
                }

                seg_len = seg_end - seg;
                if (seg_len > (int)APP_DUNGEON_LEFT_PANEL_COLS)
                    seg_len = (int)APP_DUNGEON_LEFT_PANEL_COLS;
                for (int i = 0; i < seg_len; i++)
                    text[i] = cells[seg + i].ch ? cells[seg + i].ch : ' ';
                text[seg_len] = '\0';

                seg_color = sdl_scene_color(seg_attr);
                if (px_end - px_cursor <= 0.0f)
                    break;

                px_cursor += (float)sdl_render_story_text_free_px(
                    (sdl_view*)view, font, px_cursor, row, text, seg_len,
                    seg_color, px_end - px_cursor);
                seg = seg_end;
            }
        }
    }
}

static void sdl_scene_render_fixed_left_panel_row(float x_px, float y_px,
    int cols, int cell_w, int cell_h, TTF_Font* story_font,
    TTF_Font* mono_font, const app_panel_cell_snapshot* cells)
{
    int x = 0;

    if (!cells || cols <= 0 || cell_w <= 0 || cell_h <= 0)
        return;

    sdl_scene_fill_rect(&(SDL_FRect){
        .x = x_px,
        .y = y_px,
        .w = (float)(cols * cell_w),
        .h = (float)cell_h
    }, (SDL_Color){ 0, 0, 0, 255 });

    while (x < cols)
    {
        if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
        {
            SDL_FRect dst = {
                .x = x_px + (float)(x * cell_w),
                .y = y_px,
                .w = (float)cell_w,
                .h = (float)cell_h
            };
            byte ch = (byte)cells[x].ch;

            if (g_state.use_tiles && g_state.tileset
                && (cells[x].attr & TILE_FLAG) && (ch & TILE_FLAG))
            {
                sdl_scene_draw_tile(g_state.tileset, cells[x].attr, ch, &dst);
            }
            else if (cells[x].ch && cells[x].ch != ' ')
            {
                sdl_scene_render_fixed_panel_glyph(mono_font, dst.x, dst.y,
                    cell_w, cell_h, sdl_scene_color(cells[x].attr), cells[x].ch);
            }
            x++;
            continue;
        }

        {
            byte flags = cells[x].story;
            bool use_story = (flags & STORY_FLAG_USE) != 0;
            bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
            byte attr = cells[x].attr;
            int run_start = x;

            if (!use_story)
            {
                char text[APP_DUNGEON_PANE_TEXT_MAX + 1];
                int run_len;

                while (x < cols)
                {
                    if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                        break;
                    if ((cells[x].story & STORY_FLAG_USE) != 0
                        || cells[x].attr != attr)
                    {
                        break;
                    }
                    x++;
                }

                run_len = x - run_start;
                if (run_len >= (int)sizeof(text))
                    run_len = (int)sizeof(text) - 1;
                for (int i = 0; i < run_len; i++)
                    text[i] = cells[run_start + i].ch ? cells[run_start + i].ch : ' ';
                text[run_len] = '\0';

                sdl_scene_render_fixed_panel_text_cells(
                    x_px + (float)(run_start * cell_w), y_px, cell_w, cell_h,
                    mono_font, sdl_scene_color(attr), text, (size_t)run_len);
                continue;
            }

            if (grid_align)
            {
                while (x < cols)
                {
                    if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                        break;
                    if ((cells[x].story & STORY_FLAG_USE) == 0
                        || (cells[x].story & STORY_FLAG_CELL_ALIGN) == 0
                        || cells[x].attr != attr)
                    {
                        break;
                    }
                    x++;
                }

                for (int i = run_start; i < x; i++)
                {
                    if (!cells[i].ch || cells[i].ch == ' ')
                        continue;
                    sdl_scene_render_fixed_panel_glyph(story_font,
                        x_px + (float)(i * cell_w), y_px, cell_w, cell_h,
                        sdl_scene_color(cells[i].attr), cells[i].ch);
                }
                continue;
            }

            while (x < cols)
            {
                if (!sdl_scene_story_cell_is_text(cells[x].attr, cells[x].ch))
                    break;
                if ((cells[x].story & STORY_FLAG_USE) == 0
                    || (cells[x].story & STORY_FLAG_CELL_ALIGN) != 0)
                {
                    break;
                }
                x++;
            }

            {
                float px_cursor = x_px + (float)(run_start * cell_w);
                float px_end = x_px + (float)(x * cell_w);
                int seg = run_start;

                while (seg < x)
                {
                    byte seg_attr;
                    int seg_end;
                    char text[APP_DUNGEON_PANE_TEXT_MAX + 1];
                    int seg_len;

                    if (!sdl_scene_story_cell_is_text(cells[seg].attr,
                            cells[seg].ch))
                    {
                        seg++;
                        continue;
                    }

                    seg_attr = cells[seg].attr;
                    seg_end = seg + 1;
                    while (seg_end < x
                        && sdl_scene_story_cell_is_text(cells[seg_end].attr,
                            cells[seg_end].ch)
                        && cells[seg_end].attr == seg_attr)
                    {
                        seg_end++;
                    }

                    seg_len = seg_end - seg;
                    if (seg_len >= (int)sizeof(text))
                        seg_len = (int)sizeof(text) - 1;
                    for (int i = 0; i < seg_len; i++)
                        text[i] = cells[seg + i].ch ? cells[seg + i].ch : ' ';
                    text[seg_len] = '\0';

                    if (px_end - px_cursor <= 0.0f)
                        break;

                    px_cursor += (float)sdl_scene_render_text_run_px(story_font,
                        px_cursor, y_px, sdl_scene_color(seg_attr), text,
                        (size_t)seg_len, cell_h, px_end - px_cursor);
                    seg = seg_end;
                }
            }
        }
    }
}

static void sdl_scene_render_left_panel(const sdl_view* view,
    const sdl_scene_layout* layout, const app_panes_snapshot* panes)
{
    TTF_Font* font;
    TTF_Font* story_font;
    TTF_Font* mono_font;
    sdl_scene_panel_metrics metrics;
    SDL_Rect clip_rect;
    int rows;
    int cols;

    if (!view || !layout || !panes || layout->hide_left_panel)
        return;
    if (panes->left_panel_rows == 0 || panes->left_panel_cols == 0)
        return;

    rows = panes->left_panel_rows;
    cols = panes->left_panel_cols;
    if (rows > view->rows)
        rows = view->rows;
    if (cols <= 0)
        return;

    if (sdl_scene_resolve_left_panel_metrics(view, layout, cols, rows, &metrics,
            &story_font, &mono_font))
    {
        clip_rect.x = 0;
        clip_rect.y = 0;
        clip_rect.w = metrics.panel_w;
        clip_rect.h = metrics.panel_h;

        sdl_scene_fill_rect(&(SDL_FRect){
            .x = 0.0f,
            .y = 0.0f,
            .w = (float)metrics.panel_w,
            .h = (float)metrics.panel_h
        }, (SDL_Color){ 0, 0, 0, 255 });

        SDL_SetRenderClipRect(g_state.renderer, &clip_rect);
        for (int row = 0; row < rows; row++)
            sdl_scene_render_fixed_left_panel_row(0.0f,
                (float)(row * metrics.cell_h), cols, metrics.cell_w,
                metrics.cell_h, story_font, mono_font, panes->left_panel[row]);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
        return;
    }

    font = sdl_story_font_for_view(view);
    for (int row = 0; row < rows; row++)
        sdl_scene_render_packed_story_row(view, font, 0, row, cols,
            panes->left_panel[row]);
}

static sdl_scene_layout sdl_scene_make_layout(const sdl_view* view,
    u16b status_flags)
{
    sdl_scene_layout layout;

    memset(&layout, 0, sizeof(layout));
    layout.compact_height = view ? (view->rows < 24) : false;
    layout.hide_left_panel =
        (status_flags & APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL) ? true : false;
    layout.row_map = 1;
    layout.col_map = layout.hide_left_panel ? 0 : 13;
    layout.bottom_row = view ? (view->rows - 1) : 0;
    layout.row_name = 1;
    layout.row_stat = 3;
    layout.row_exp = layout.compact_height ? 7 : 8;
    layout.row_hp = layout.compact_height ? 8 : 9;
    layout.row_sp = layout.compact_height ? 9 : 10;
    layout.row_light = layout.compact_height ? 10 : 11;
    layout.row_mel = layout.compact_height ? 11 : 13;
    layout.row_arc = layout.compact_height ? 12 : 14;
    layout.row_quiver = layout.compact_height ? 13 : 15;
    layout.row_evn = layout.compact_height ? 14 : 16;
    layout.row_info = layout.compact_height ? 15 : 17;
    layout.row_cut = layout.compact_height ? 17 : 20;
    layout.row_song = layout.compact_height ? 18 : 21;
    layout.col_hungry = 0;
    layout.col_blind = 9;
    layout.col_confused = 15;
    layout.col_stun = 24;
    layout.col_afraid = 36;
    layout.col_state = 43;
    layout.col_speed = 56;
    layout.col_terrain = 61;
    layout.col_depth = 72;
    return layout;
}

static const app_snapshot_blob* sdl_scene_find_blob(
    const app_dungeon_snapshot* snapshot, u16b kind)
{
    size_t i;

    if (!snapshot)
        return NULL;

    for (i = 0; i < snapshot->snapshot.blob_count; i++)
    {
        const app_snapshot_blob* blob = &snapshot->blobs[i];

        if (blob->kind == kind && blob->data && blob->size)
            return blob;
    }

    return NULL;
}

static const app_map_snapshot* sdl_scene_map_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_MAP);

    if (!blob || blob->size < sizeof(app_map_snapshot))
        return NULL;
    return (const app_map_snapshot*)blob->data;
}

static const app_status_snapshot* sdl_scene_status_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_STATUS);

    if (!blob || blob->size < sizeof(app_status_snapshot))
        return NULL;
    return (const app_status_snapshot*)blob->data;
}

static const app_messages_snapshot* sdl_scene_messages_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_MESSAGES);

    if (!blob || blob->size < sizeof(app_messages_snapshot))
        return NULL;
    return (const app_messages_snapshot*)blob->data;
}

static const app_panes_snapshot* sdl_scene_panes_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_PANES);

    if (!blob || blob->size < sizeof(app_panes_snapshot))
        return NULL;
    return (const app_panes_snapshot*)blob->data;
}

static const app_interaction_state* sdl_scene_interaction_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_OVERLAY);

    if (!blob || blob->size < sizeof(app_interaction_state))
        return NULL;
    return (const app_interaction_state*)blob->data;
}

static void sdl_scene_render_interaction_overlay(const sdl_view* view,
    const app_interaction_state* interaction)
{
    SDL_Color background;
    SDL_Color border;
    SDL_Color selected = { 36, 74, 112, 208 };
    SDL_Rect clip_rect;
    SDL_FRect box;
    TTF_Font* font;
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;
    int line_gap;
    int pad_x;
    int pad_y;
    int outer_margin;
    int header_lines = 0;
    int option_rows = 0;
    int option_start = 0;
    int selected_index;
    int width_px = 0;
    int height_px;
    int line_step;
    int current_y;
    bool plain_list;
    char line[APP_INTERACTION_TEXT_MAX + APP_INTERACTION_META_MAX + 16];

    if (!view || !interaction || interaction->kind == APP_INTERACTION_KIND_NONE)
        return;

    canvas_w = view->cols * view->cell_w;
    canvas_h = view->rows * view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return;

    pixel_height = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    font = sdl_scene_ui_font_for_height(pixel_height);
    if (!font)
        return;

    plain_list = (interaction->flags & APP_INTERACTION_FLAG_PLAIN_LIST) != 0;
    line_h = TTF_GetFontHeight(font);
    if (line_h <= 0)
        line_h = pixel_height;
    line_gap = sdl_scene_ui_scale_px(2.0f);
    pad_x = sdl_scene_ui_scale_px(plain_list ? 16.0f : 14.0f);
    pad_y = sdl_scene_ui_scale_px(plain_list ? 10.0f : 12.0f);
    outer_margin = sdl_scene_ui_scale_px(24.0f);
    line_step = line_h + line_gap;

    if (interaction->prompt[0])
        header_lines++;
    if (interaction->detail[0])
        header_lines++;
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_VALUE)
        && interaction->value[0])
    {
        header_lines++;
    }

    if (interaction->prompt[0])
        width_px = MAX(width_px, sdl_scene_measure_ui_text(font,
            interaction->prompt));
    if (interaction->detail[0])
        width_px = MAX(width_px, sdl_scene_measure_ui_text(font,
            interaction->detail));
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_VALUE)
        && interaction->value[0])
    {
        char value_buf[APP_INTERACTION_VALUE_MAX + 2];
        size_t len = strlen(interaction->value);

        SDL_strlcpy(value_buf, interaction->value, sizeof(value_buf));
        if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_CURSOR)
            && interaction->cursor_index >= 0
            && interaction->cursor_index <= (s16b)len
            && len + 1 < sizeof(value_buf))
        {
            size_t cursor = (size_t)interaction->cursor_index;

            memmove(value_buf + cursor + 1, value_buf + cursor,
                len - cursor + 1);
            value_buf[cursor] = '_';
        }
        width_px = MAX(width_px, sdl_scene_measure_ui_text(font, value_buf));
    }

    option_rows = (interaction->kind == APP_INTERACTION_KIND_LIST)
        ? (int)interaction->option_count
        : 0;
    selected_index = interaction->selected_index;
    if (selected_index < 0)
        selected_index = 0;
    if ((u16b)selected_index >= interaction->option_count
        && interaction->option_count > 0)
    {
        selected_index = interaction->option_count - 1;
    }

    for (int i = 0; i < option_rows; i++)
    {
        const app_interaction_option* option = &interaction->options[i];

        sdl_scene_interaction_format_line(line, sizeof(line), interaction,
            option);
        width_px = MAX(width_px, sdl_scene_measure_ui_text(font, line));
    }

    width_px += pad_x * 2;
    if (width_px < sdl_scene_ui_scale_px(220.0f))
        width_px = sdl_scene_ui_scale_px(220.0f);
    if (width_px > canvas_w - outer_margin * 2)
        width_px = canvas_w - outer_margin * 2;
    if (width_px <= 0)
        return;

    if (option_rows > 0)
    {
        int header_height = 0;
        int available_h;
        int max_option_rows;

        if (header_lines > 0)
            header_height = header_lines * line_h
                + (header_lines - 1) * line_gap;

        available_h = canvas_h - outer_margin * 2 - pad_y * 2 - header_height;
        max_option_rows = (available_h + line_gap) / line_step;
        if (max_option_rows < 1)
            max_option_rows = 1;
        if (option_rows > max_option_rows)
            option_rows = max_option_rows;

        if ((int)interaction->option_count > option_rows)
        {
            option_start = selected_index - option_rows / 2;
            if (option_start < 0)
                option_start = 0;
            if (option_start + option_rows > (int)interaction->option_count)
                option_start = interaction->option_count - option_rows;
        }
    }

    height_px = pad_y * 2;
    if (header_lines > 0)
        height_px += header_lines * line_h + (header_lines - 1) * line_gap;
    if (option_rows > 0)
    {
        if (header_lines > 0)
            height_px += line_gap;
        height_px += option_rows * line_h + (option_rows - 1) * line_gap;
    }
    if (height_px < sdl_scene_ui_scale_px(72.0f))
        height_px = sdl_scene_ui_scale_px(72.0f);
    if (height_px > canvas_h - outer_margin * 2)
        height_px = canvas_h - outer_margin * 2;
    if (height_px <= 0)
        return;

    box.w = (float)width_px;
    box.h = (float)height_px;
    box.x = (float)((canvas_w - width_px) / 2);
    if (interaction->kind == APP_INTERACTION_KIND_TARGETING)
        box.y = (float)outer_margin;
    else
        box.y = (float)((canvas_h - height_px) / 2);

    if (box.x < (float)outer_margin)
        box.x = (float)outer_margin;
    if (box.y < (float)outer_margin)
        box.y = (float)outer_margin;

    background = plain_list ? (SDL_Color){ 0, 0, 0, 232 }
                            : (SDL_Color){ 10, 18, 26, 216 };
    border = plain_list ? (SDL_Color){ 0, 0, 0, 0 }
                        : (SDL_Color){ 122, 146, 170, 255 };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    sdl_scene_fill_rect(&box, background);
    if (!plain_list)
        sdl_scene_draw_rect(&box, border);

    clip_rect.x = (int)box.x + pad_x;
    clip_rect.y = (int)box.y + pad_y;
    clip_rect.w = width_px - pad_x * 2;
    clip_rect.h = height_px - pad_y * 2;
    if (clip_rect.w <= 0 || clip_rect.h <= 0)
        return;

    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    current_y = clip_rect.y;
    if (interaction->prompt[0])
    {
        sdl_scene_render_ui_text(font, (float)clip_rect.x, (float)current_y,
            sdl_scene_color(interaction->prompt_attr), interaction->prompt);
        current_y += line_step;
    }
    if (interaction->detail[0])
    {
        sdl_scene_render_ui_text(font, (float)clip_rect.x, (float)current_y,
            sdl_scene_color(interaction->detail_attr), interaction->detail);
        current_y += line_step;
    }
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_VALUE)
        && interaction->value[0])
    {
        char value_buf[APP_INTERACTION_VALUE_MAX + 2];
        size_t len = strlen(interaction->value);

        SDL_strlcpy(value_buf, interaction->value, sizeof(value_buf));
        if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_CURSOR)
            && interaction->cursor_index >= 0
            && interaction->cursor_index <= (s16b)len
            && len + 1 < sizeof(value_buf))
        {
            size_t cursor = (size_t)interaction->cursor_index;

            memmove(value_buf + cursor + 1, value_buf + cursor,
                len - cursor + 1);
            value_buf[cursor] = '_';
        }
        sdl_scene_render_ui_text(font, (float)clip_rect.x, (float)current_y,
            sdl_scene_color(interaction->value_attr), value_buf);
        current_y += line_step;
    }

    if (header_lines > 0 && option_rows > 0)
        current_y += line_gap;

    for (int i = 0; i < option_rows; i++)
    {
        const app_interaction_option* option;
        int index = option_start + i;
        byte attr;

        if ((u16b)index >= interaction->option_count)
            break;

        option = &interaction->options[index];
        attr = option->enabled ? option->attr : TERM_L_DARK;
        sdl_scene_interaction_format_line(line, sizeof(line), interaction,
            option);

        if (!plain_list && option->selected)
        {
            SDL_FRect selected_rect = {
                (float)clip_rect.x - sdl_scene_ui_scale_px(4.0f),
                (float)current_y - sdl_scene_ui_scale_px(2.0f),
                (float)clip_rect.w + sdl_scene_ui_scale_px(8.0f),
                (float)line_h + sdl_scene_ui_scale_px(4.0f)
            };

            sdl_scene_fill_rect(&selected_rect, selected);
        }

        sdl_scene_render_ui_text(font, (float)clip_rect.x, (float)current_y,
            sdl_scene_color(attr), line);
        current_y += line_step;
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

static bool sdl_scene_map_to_screen(const sdl_scene_layout* layout,
    const app_map_snapshot* map, int map_y, int map_x, int* out_row,
    int* out_col, int* out_width_cells)
{
    if (!layout || !map)
        return false;
    if (map_y < map->panel_y || map_y >= map->panel_y + map->height)
        return false;
    if (map_x < map->panel_x || map_x >= map->panel_x + map->width)
        return false;

    if (out_row)
        *out_row = layout->row_map + (map_y - map->panel_y);
    if (out_col)
        *out_col = layout->col_map
            + (map_x - map->panel_x)
                * ((use_bigtile && !graphics_are_ascii()) ? 2 : 1);
    if (out_width_cells)
        *out_width_cells = (use_bigtile && !graphics_are_ascii()) ? 2 : 1;
    return true;
}

static const app_map_cell_snapshot* sdl_scene_find_cell_by_subject(
    const app_map_snapshot* map, s32b subject)
{
    size_t i;

    if (!map)
        return NULL;

    for (i = 0; i < map->cell_count; i++)
    {
        const app_map_cell_snapshot* cell = &map->cells[i];

        if ((subject == APP_DUNGEON_PLAYER_SUBJECT
                && (cell->flags & APP_MAP_CELL_FLAG_PLAYER))
            || (subject > 0 && cell->m_idx == subject))
        {
            return cell;
        }
    }

    return NULL;
}

static void sdl_scene_draw_tile(SDL_Texture* tileset, byte attr, byte ch,
    const SDL_FRect* dst)
{
    SDL_FRect src;

    if (!tileset || !dst)
        return;
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    src.x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE);
    src.y = (float)(TILE_GET_INDEX(attr) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    SDL_RenderTexture(g_state.renderer, tileset, &src, dst);
}

static void sdl_scene_draw_misc_icon(int icon, const SDL_FRect* dst)
{
    byte attr = misc_to_attr[icon];
    byte ch = (byte)misc_to_char[icon];

    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    sdl_scene_draw_tile(g_state.tileset, attr, ch, dst);
}

static void sdl_scene_draw_map_cell(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const app_map_cell_snapshot* cell)
{
    int row;
    int col;
    int width_cells;
    SDL_FRect dst;
    char glyph[2];
    byte terrain_ch = (byte)cell->terrain_char;
    byte ch = (byte)cell->ch;

    if (!view || !layout || !map || !cell)
        return;
    if (!sdl_scene_map_to_screen(layout, map, cell->map_y, cell->map_x, &row,
            &col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_layout_cell_rect(view, col, row, width_cells, &dst))
        return;

    sdl_scene_fill_rect(&dst, (SDL_Color){ 0, 0, 0, 255 });

    if (g_state.use_tiles && g_state.tileset
        && (cell->terrain_attr & TILE_FLAG) && (terrain_ch & TILE_FLAG)
        && (cell->attr & TILE_FLAG) && (ch & TILE_FLAG))
    {
        sdl_scene_draw_tile(g_state.tileset, cell->terrain_attr, terrain_ch, &dst);
        if (cell->attr & GRAPHICS_GLOW_MASK)
            sdl_scene_draw_misc_icon(ICON_GLOW, &dst);
        sdl_scene_draw_tile(g_state.tileset, cell->attr, ch, &dst);
        if (cell->terrain_attr & GRAPHICS_SLEEP_MASK)
            sdl_scene_draw_misc_icon(ICON_SLEEPING, &dst);
        if (terrain_ch & GRAPHICS_SEEN_MASK)
            sdl_scene_draw_misc_icon(ICON_MONSTER_SEES_PLAYER, &dst);
        if (ch & GRAPHICS_ALERT_MASK)
            sdl_scene_draw_misc_icon(ICON_ALERT, &dst);
    }
    else
    {
        glyph[0] = cell->ch ? cell->ch : ' ';
        glyph[1] = '\0';
        sdl_render_mono_text((sdl_view*)view, col, row, 1, glyph,
            sdl_scene_color(cell->attr));
        if (use_bigtile && !graphics_are_ascii() && (col + 1) < view->cols)
            sdl_scene_draw_text(view, col + 1, row, TERM_WHITE, " ");
    }

    if (cell->flags & APP_MAP_CELL_FLAG_TARGET)
    {
        SDL_Color target_color = sdl_scene_color(TERM_L_RED);
        target_color.a = 220;
        sdl_scene_draw_rect(&dst, target_color);
    }

    if (cell->flags & APP_MAP_CELL_FLAG_CURSOR)
    {
        SDL_Color cursor_color = sdl_scene_color(TERM_L_BLUE);
        cursor_color.a = 220;
        sdl_scene_draw_rect(&dst, cursor_color);
    }
}

static void sdl_scene_draw_absolute_cursor(const sdl_view* view,
    const app_cursor_snapshot* cursor)
{
    SDL_FRect rect;
    SDL_Color color = sdl_scene_color(TERM_L_BLUE);

    if (!view || !cursor || !cursor->visible || cursor->relative)
        return;
    if (!sdl_scene_layout_cell_rect(view, cursor->col, cursor->row, 1, &rect))
        return;

    color.a = 220;
    sdl_scene_draw_rect(&rect, color);
}

static void sdl_scene_render_messages(const sdl_view* view,
    const app_messages_snapshot* messages)
{
    if (!view || !messages)
        return;

    if (messages->top_line_active && messages->top_line[0])
        sdl_scene_draw_text(view, 0, 0, messages->top_line_color,
            messages->top_line);
    else if (messages->line_count > 0 && messages->lines[0].text[0])
        sdl_scene_draw_text(view, 0, 0, messages->lines[0].color,
            messages->lines[0].text);
}

static void sdl_scene_render_status_panel(const sdl_view* view,
    const sdl_scene_layout* layout, const app_status_snapshot* status)
{
    char buf[64];

    if (!view || !layout || !status || layout->hide_left_panel)
        return;

    sdl_scene_draw_text(view, 0, layout->row_name, TERM_WHITE,
        status->player_name);

    strnfmt(buf, sizeof(buf), "Str %2d", status->str_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 0, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Dex %2d", status->dex_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 1, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Con %2d", status->con_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 2, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Gra %2d", status->gra_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 3, TERM_L_GREEN, buf);

    strnfmt(buf, sizeof(buf), "EXP %ld", (long)status->exp);
    sdl_scene_draw_text(view, 0, layout->row_exp, TERM_WHITE, buf);

    strnfmt(buf, sizeof(buf), "HP %d/%d", status->hp_cur, status->hp_max);
    sdl_scene_draw_text(view, 0, layout->row_hp, status->hp_attr, buf);

    strnfmt(buf, sizeof(buf), "Voice %d/%d", status->voice_cur,
        status->voice_max);
    sdl_scene_draw_text(view, 0, layout->row_sp, status->voice_attr, buf);

    sdl_scene_draw_text_snapshot(view, 0, layout->row_mel, &status->melee_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_arc, &status->archery_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_quiver, &status->quiver_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_evn, &status->evasion_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_light, &status->light_text);

    if (status->tracked_name_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info,
            &status->tracked_name_text);
    if (status->tracked_health_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info + 1,
            &status->tracked_health_text);
    if (status->tracked_alertness_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info + 2,
            &status->tracked_alertness_text);

    if (status->cut_text.active && status->poisoned_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut - 1,
            &status->cut_text);
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut,
            &status->poisoned_text);
    }
    else if (status->cut_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut, &status->cut_text);
    }
    else if (status->poisoned_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut,
            &status->poisoned_text);
    }

    sdl_scene_draw_text_snapshot(view, 0, layout->row_song, &status->song_text);
}

static void sdl_scene_render_hidden_overlay(const sdl_view* view,
    const sdl_scene_layout* layout, const app_panes_snapshot* panes)
{
    u16b i;

    if (!view || !layout || !panes || !layout->hide_left_panel)
        return;

    for (i = 0; i < panes->hidden_overlay_count
        && i < APP_DUNGEON_HIDDEN_OVERLAY_MAX; i++)
    {
        const app_hidden_overlay_line_snapshot* line = &panes->hidden_overlay[i];

        if (!line->text[0])
            continue;
        sdl_scene_draw_text(view, 0, layout->row_name + i, line->attr,
            line->text);
    }
}

static void sdl_scene_render_bottom_line(const sdl_view* view,
    const sdl_scene_layout* layout, const app_status_snapshot* status)
{
    if (!view || !layout || !status)
        return;

    sdl_scene_draw_text_snapshot(view, layout->col_hungry, layout->bottom_row,
        &status->hunger_text);
    sdl_scene_draw_text_snapshot(view, layout->col_blind, layout->bottom_row,
        &status->blind_text);
    sdl_scene_draw_text_snapshot(view, layout->col_confused, layout->bottom_row,
        &status->confused_text);
    sdl_scene_draw_text_snapshot(view, layout->col_stun, layout->bottom_row,
        &status->stun_text);
    sdl_scene_draw_text_snapshot(view, layout->col_afraid, layout->bottom_row,
        &status->afraid_text);
    sdl_scene_draw_text_snapshot(view, layout->col_state, layout->bottom_row,
        &status->state_text);
    sdl_scene_draw_text_snapshot(view, layout->col_speed, layout->bottom_row,
        &status->speed_text);
    sdl_scene_draw_text_snapshot(view, layout->col_terrain, layout->bottom_row,
        &status->terrain_text);
    sdl_scene_draw_text_snapshot(view, layout->col_depth, layout->bottom_row,
        &status->depth_text);
}

static void sdl_scene_format_combat_line(char* buf, size_t buf_size,
    const app_combat_roll_snapshot* entry)
{
    char attacker = entry->attacker_char ? entry->attacker_char : '?';
    char defender = entry->defender_char ? entry->defender_char : '?';

    strnfmt(buf, buf_size, "%c %2d[%2d] vs %c %2d[%2d]  %dd%d -> %d",
        attacker, entry->att, entry->att_roll, defender, entry->evn,
        entry->evn_roll, entry->dd, entry->ds, entry->dam);
}

static void sdl_scene_render_combat_overlay(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const app_panes_snapshot* panes)
{
    int lines;
    int i;
    char buf[80];
    int start_row;

    if (!view || !layout || !map || !panes || panes->main_combat_roll_lines <= 0)
        return;

    lines = panes->main_combat_roll_lines;
    if (lines > 3)
        lines = 3;
    if (lines > panes->combat_entry_count)
        lines = panes->combat_entry_count;

    start_row = layout->row_map + map->height;
    for (i = 0; i < lines && (start_row + i) < layout->bottom_row; i++)
    {
        sdl_scene_format_combat_line(buf, sizeof(buf), &panes->combat_entries[i]);
        sdl_scene_draw_text(view, layout->col_map, start_row + i, TERM_WHITE,
            buf);
    }
}

static void sdl_scene_draw_animation_rect(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map, int map_y,
    int map_x, SDL_Color color)
{
    int row;
    int col;
    int width_cells;
    SDL_FRect rect;

    if (!sdl_scene_map_to_screen(layout, map, map_y, map_x, &row, &col,
            &width_cells))
    {
        return;
    }
    if (!sdl_scene_layout_cell_rect(view, col, row, width_cells, &rect))
        return;

    sdl_scene_fill_rect(&rect, color);
}

static void sdl_scene_render_move_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    int width_cells;
    SDL_Color color = sdl_scene_color((anim->subject == APP_DUNGEON_PLAYER_SUBJECT)
        ? TERM_L_BLUE : TERM_L_WHITE);
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float x;
    float y;

    if (!view || !layout || !map || !anim)
        return;
    if (!sdl_scene_map_to_screen(layout, map, anim->from_y, anim->from_x,
            &from_row, &from_col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_map_to_screen(layout, map, anim->to_y, anim->to_x, &to_row,
            &to_col, NULL))
    {
        return;
    }

    start_x = (float)(from_col * view->cell_w + width_cells * view->cell_w / 2);
    start_y = (float)(from_row * view->cell_h + view->cell_h / 2);
    end_x = (float)(to_col * view->cell_w + width_cells * view->cell_w / 2);
    end_y = (float)(to_row * view->cell_h + view->cell_h / 2);
    x = start_x + (end_x - start_x) * progress;
    y = start_y + (end_y - start_y) * progress;

    color.a = 180;
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, start_x, start_y, end_x, end_y);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = x - view->cell_w * 0.20f,
        .y = y - view->cell_h * 0.20f,
        .w = view->cell_w * 0.40f,
        .h = view->cell_h * 0.40f
    });
}

static void sdl_scene_render_projectile_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    int width_cells;
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float x;
    float y;
    SDL_Color color = sdl_scene_color(TERM_YELLOW);

    if (!view || !layout || !map || !anim)
        return;
    if (!sdl_scene_map_to_screen(layout, map, anim->from_y, anim->from_x,
            &from_row, &from_col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_map_to_screen(layout, map, anim->to_y, anim->to_x, &to_row,
            &to_col, NULL))
    {
        return;
    }

    start_x = (float)(from_col * view->cell_w + width_cells * view->cell_w / 2);
    start_y = (float)(from_row * view->cell_h + view->cell_h / 2);
    end_x = (float)(to_col * view->cell_w + width_cells * view->cell_w / 2);
    end_y = (float)(to_row * view->cell_h + view->cell_h / 2);
    x = start_x + (end_x - start_x) * progress;
    y = start_y + (end_y - start_y) * progress;

    color.a = 210;
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, start_x, start_y, x, y);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = x - view->cell_w * 0.15f,
        .y = y - view->cell_h * 0.15f,
        .w = view->cell_w * 0.30f,
        .h = view->cell_h * 0.30f
    });
}

static void sdl_scene_render_damage_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    const app_map_cell_snapshot* cell = sdl_scene_find_cell_by_subject(map,
        anim->subject);
    float progress = sdl_scene_progress(now_ns, anim);
    SDL_Color color = sdl_scene_color(TERM_RED);

    if (!view || !layout || !map || !anim)
        return;

    color.a = sdl_scene_alpha_byte(0.35f * (1.0f - progress));
    if (cell)
    {
        sdl_scene_draw_animation_rect(view, layout, map, cell->map_y, cell->map_x,
            color);
    }
    else
    {
        sdl_scene_fill_rect(&(SDL_FRect){
            .x = 0.0f,
            .y = 0.0f,
            .w = (float)(view->cols * view->cell_w),
            .h = (float)(view->rows * view->cell_h)
        }, color);
    }
}

static void sdl_scene_render_object_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    SDL_Color color = sdl_scene_color(TERM_L_GREEN);

    color.a = sdl_scene_alpha_byte(0.30f * (1.0f - progress));
    sdl_scene_draw_animation_rect(view, layout, map, anim->from_y, anim->from_x,
        color);
}

static void sdl_scene_render_animations(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns)
{
    size_t i;

    if (!view || !layout || !map || !animations)
        return;

    for (i = 0; i < animation_count; i++)
    {
        const sdl_scene_animation* anim = &animations[i];

        if (!anim->active)
            continue;

        switch (anim->kind)
        {
        case SDL_SCENE_ANIMATION_ACTOR_MOVED:
            sdl_scene_render_move_animation(view, layout, map, anim, now_ns);
            break;

        case SDL_SCENE_ANIMATION_DAMAGE:
            sdl_scene_render_damage_animation(view, layout, map, anim, now_ns);
            break;

        case SDL_SCENE_ANIMATION_PROJECTILE:
            sdl_scene_render_projectile_animation(view, layout, map, anim,
                now_ns);
            break;

        case SDL_SCENE_ANIMATION_OBJECT_TRANSFER:
            sdl_scene_render_object_animation(view, layout, map, anim, now_ns);
            break;

        default:
            break;
        }
    }
}

bool sdl_scene_dungeon_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns)
{
    const app_map_snapshot* map;
    const app_status_snapshot* status;
    const app_messages_snapshot* messages;
    const app_panes_snapshot* panes;
    const app_interaction_state* interaction;
    sdl_scene_layout layout;
    size_t i;

    if (!canvas || !main_view || !snapshot)
        return false;

    map = sdl_scene_map_snapshot(snapshot);
    status = sdl_scene_status_snapshot(snapshot);
    messages = sdl_scene_messages_snapshot(snapshot);
    panes = sdl_scene_panes_snapshot(snapshot);
    interaction = sdl_scene_interaction_snapshot(snapshot);
    if (!map || !status || !messages || !panes)
        return false;

    layout = sdl_scene_make_layout(main_view, status->flags);
    if (!layout.hide_left_panel)
    {
        layout.col_map = sdl_scene_left_panel_reserved_cols(main_view, &layout,
            panes);
    }

    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (layout.hide_left_panel || panes->left_panel_cols == 0
        || panes->left_panel_rows == 0)
    {
        sdl_scene_render_status_panel(main_view, &layout, status);
    }
    else
    {
        sdl_scene_render_left_panel(main_view, &layout, panes);
    }

    for (i = 0; i < map->cell_count; i++)
        sdl_scene_draw_map_cell(main_view, &layout, map, &map->cells[i]);

    sdl_scene_render_hidden_overlay(main_view, &layout, panes);
    sdl_scene_render_combat_overlay(main_view, &layout, map, panes);
    sdl_scene_render_bottom_line(main_view, &layout, status);
    sdl_scene_render_animations(main_view, &layout, map, animations,
        animation_count, now_ns);
    sdl_scene_draw_absolute_cursor(main_view, &map->cursor);
    sdl_scene_render_messages(main_view, messages);
    sdl_scene_render_interaction_overlay(main_view, interaction);

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
