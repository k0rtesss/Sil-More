#include "angband.h"

#include "sdl-main-internal.h"

typedef struct sdl_information_fixed_metrics {
    int origin_x;
    int origin_y;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    TTF_Font* mono_font;
    TTF_Font* story_font;
} sdl_information_fixed_metrics;

static SDL_Color sdl_information_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static int sdl_information_measure_text_n(TTF_Font* font, cptr text, size_t len)
{
    int measured_w = 0;

    if (!font || !text || len == 0)
        return 0;

    if (!TTF_MeasureString(font, text, len, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

static int sdl_information_fixed_cell_width(int cell_h, TTF_Font* mono_font,
    TTF_Font* story_font)
{
    int cell_w;
    int mono_m_w;
    int story_m_w;

    cell_w = (int)((float)cell_h * 0.57f + 0.5f);
    mono_m_w = sdl_information_measure_text_n(mono_font, "M", 1);
    story_m_w = sdl_information_measure_text_n(story_font, "M", 1);
    if (mono_m_w > cell_w)
        cell_w = mono_m_w;
    if (story_m_w > cell_w)
        cell_w = story_m_w;
    if (cell_w < 1)
        cell_w = 1;

    return cell_w;
}

static bool sdl_information_scene_row_has_overlay_text(
    const app_information_scene* scene, int row)
{
    u16b i;

    if (!scene || row < 0)
        return false;

    for (i = 0; i < scene->op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &scene->ops[i];

        if (op->kind != APP_INFORMATION_OP_KIND_TEXT || op->row != row)
            continue;
        if (!op->text[0])
            continue;
        if (op->col >= 0 && (int)op->col < (int)APP_DUNGEON_LEFT_PANEL_COLS)
            return true;
    }

    return false;
}

static bool sdl_information_overlay_draw_text(
    const app_information_scene* scene, const app_information_op* op)
{
    if (!scene || !op || op->kind != APP_INFORMATION_OP_KIND_TEXT)
        return false;

    return sdl_information_scene_row_has_overlay_text(scene, op->row);
}

static bool sdl_information_overlay_draw_cell(
    const app_information_scene* scene, const app_information_op* op)
{
    if (!scene || !op || op->kind != APP_INFORMATION_OP_KIND_CELL)
        return false;

    return sdl_information_scene_row_has_overlay_text(scene, op->row)
        && op->col >= 0 && op->col < 2;
}

static bool sdl_information_scene_bounds(const app_information_scene* scene,
    bool overlay_only, int* out_rows, int* out_cols)
{
    int rows = 0;
    int cols = 0;
    u16b i;

    if (!scene || !out_rows || !out_cols)
        return false;

    for (i = 0; i < scene->op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &scene->ops[i];
        int width = 1;

        if (overlay_only)
        {
            if (op->kind == APP_INFORMATION_OP_KIND_TEXT
                && !sdl_information_overlay_draw_text(scene, op))
            {
                continue;
            }
            if (op->kind == APP_INFORMATION_OP_KIND_CELL
                && !sdl_information_overlay_draw_cell(scene, op))
            {
                continue;
            }
            if (op->kind == APP_INFORMATION_OP_KIND_CURSOR)
                continue;
        }

        if (op->row < 0 || op->col < 0)
            continue;

        if (op->kind == APP_INFORMATION_OP_KIND_TEXT)
        {
            width = (int)strlen(op->text);
            if (width < 1)
                width = 1;
        }
        else
        {
            width = op->width ? op->width : 1;
        }

        if (op->row + 1 > rows)
            rows = op->row + 1;
        if (op->col + width > cols)
            cols = op->col + width;
    }

    if (rows < 1)
        rows = 1;
    if (cols < 1)
        cols = 1;

    *out_rows = rows;
    *out_cols = cols;
    return true;
}

static bool sdl_information_resolve_fixed_metrics(const sdl_view* main_view,
    const app_information_scene* scene, bool overlay_only,
    sdl_information_fixed_metrics* metrics)
{
    int canvas_w;
    int canvas_h;
    int desired_px;
    int min_px;
    int pixel_height;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    TTF_Font* fallback_mono = NULL;
    TTF_Font* fallback_story = NULL;
    int fallback_cell_w = 0;
    int fallback_cell_h = 0;

    if (!main_view || !scene || !metrics)
        return false;
    if (!sdl_information_scene_bounds(scene, overlay_only, &rows, &cols))
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    desired_px = sdl_ui_scale_px(
        (float)sdl_resolve_menu_panel_font_size(config.menu_panel_font_size));
    min_px = sdl_ui_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    margin_x = sdl_ui_scale_px(2.0f);
    margin_y = 0;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        TTF_Font* mono_font = sdl_ui_font_for_height(pixel_height);
        TTF_Font* story_font = sdl_story_font_for_height(pixel_height);
        int mono_h;
        int story_h;
        int cell_h;
        int cell_w;

        if (!mono_font || !story_font)
            continue;

        mono_h = TTF_GetFontHeight(mono_font);
        story_h = TTF_GetFontHeight(story_font);
        cell_h = MAX(pixel_height, mono_h);
        if (story_h > cell_h)
            cell_h = story_h;
        cell_w = sdl_information_fixed_cell_width(cell_h, mono_font,
            story_font);

        fallback_mono = mono_font;
        fallback_story = story_font;
        fallback_cell_w = cell_w;
        fallback_cell_h = cell_h;

        if ((cols * cell_w) <= (canvas_w - margin_x * 2)
            && (rows * cell_h) <= (canvas_h - margin_y * 2))
        {
            metrics->origin_x = margin_x;
            metrics->origin_y = margin_y;
            metrics->cell_w = cell_w;
            metrics->cell_h = cell_h;
            metrics->cols = cols;
            metrics->rows = rows;
            metrics->mono_font = mono_font;
            metrics->story_font = story_font;
            return true;
        }
    }

    if (!fallback_mono || !fallback_story)
        return false;

    metrics->origin_x = margin_x;
    metrics->origin_y = margin_y;
    metrics->cell_w = fallback_cell_w;
    metrics->cell_h = fallback_cell_h;
    metrics->cols = cols;
    metrics->rows = rows;
    metrics->mono_font = fallback_mono;
    metrics->story_font = fallback_story;
    return true;
}

static bool sdl_information_layout_fixed_rect(
    const sdl_information_fixed_metrics* metrics, int col, int row,
    int width_cells, SDL_FRect* out_rect)
{
    if (!metrics || !out_rect || col < 0 || row < 0 || width_cells <= 0)
        return false;
    if (col >= metrics->cols || row >= metrics->rows)
        return false;
    if (col + width_cells > metrics->cols)
        width_cells = metrics->cols - col;
    if (width_cells <= 0)
        return false;

    *out_rect = (SDL_FRect){
        .x = (float)(metrics->origin_x + col * metrics->cell_w),
        .y = (float)(metrics->origin_y + row * metrics->cell_h),
        .w = (float)(width_cells * metrics->cell_w),
        .h = (float)metrics->cell_h
    };
    return true;
}

static bool sdl_information_layout_cell_rect(const sdl_view* view, int col,
    int row, int width_cells, SDL_FRect* out_rect)
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

static bool sdl_information_cell_is_raw(byte attr, char ch,
    byte terrain_attr, char terrain_char)
{
    unsigned char uch = (unsigned char)ch;
    unsigned char terrain_uch = (unsigned char)terrain_char;

    if ((attr & TILE_FLAG) && (uch & TILE_FLAG))
        return true;
    if (attr == 255 && uch == 0xFF)
        return true;
    if (terrain_attr || terrain_uch)
        return true;

    return false;
}

static void sdl_information_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static void sdl_information_draw_tile(const SDL_FRect* dst, byte attr, byte ch)
{
    SDL_FRect src;

    if (!dst || !g_state.tileset)
        return;
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    src.x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE);
    src.y = (float)(TILE_GET_INDEX(attr) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, dst);
}

static void sdl_information_draw_misc_icon(const SDL_FRect* dst, int icon)
{
    byte attr;
    byte ch;

    if (!dst)
        return;

    attr = misc_to_attr[icon];
    ch = (byte)misc_to_char[icon];
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    sdl_information_draw_tile(dst, attr, ch);
}

static int sdl_information_render_text_run_px(TTF_Font* font, float x_px,
    float y_px, SDL_Color color, cptr text, size_t len, int target_h,
    float max_w_px)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    SDL_Rect previous_clip;
    SDL_Rect clip_rect;
    SDL_Rect* clip_ptr = NULL;
    bool had_clip = false;
    char buf[APP_INFORMATION_TEXT_MAX + 1];
    int advance_w;
    float scale = 1.0f;
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

    advance_w = sdl_information_measure_text_n(font, buf, copy_len);
    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    if (target_h > 0 && surface->h > target_h)
    {
        scale = (float)target_h / (float)surface->h;
        dst.w *= scale;
        dst.h *= scale;
        advance_w = (int)((float)advance_w * scale + 0.5f);
    }
    if (target_h > 0 && dst.h < (float)target_h)
        dst.y += ((float)target_h - dst.h) * 0.5f;

    if (max_w_px > 0.0f)
    {
        if ((float)advance_w > max_w_px)
            advance_w = (int)max_w_px;
        if (dst.w > max_w_px)
        {
            clip_rect.x = (int)x_px;
            clip_rect.y = (int)y_px;
            clip_rect.w = (int)(max_w_px + 0.999f);
            clip_rect.h = (int)(dst.h + 0.999f);
            if (clip_rect.w > 0 && clip_rect.h > 0)
                clip_ptr = &clip_rect;
        }
    }

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (texture)
    {
        if (clip_ptr)
        {
            had_clip = SDL_GetRenderClipRect(g_state.renderer, &previous_clip);
            SDL_SetRenderClipRect(g_state.renderer, clip_ptr);
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
        if (clip_ptr)
            SDL_SetRenderClipRect(g_state.renderer,
                had_clip ? &previous_clip : NULL);
        SDL_DestroyTexture(texture);
    }

    SDL_DestroySurface(surface);
    return advance_w;
}

static float sdl_information_measure_text_run_px(TTF_Font* font, cptr text,
    size_t len, int target_h, float max_w_px)
{
    int width_px;
    int font_h;
    float scaled_w;

    if (!font || !text || len == 0)
        return 0.0f;

    width_px = sdl_information_measure_text_n(font, text, len);
    if (width_px <= 0)
        return 0.0f;

    scaled_w = (float)width_px;
    font_h = TTF_GetFontHeight(font);
    if (target_h > 0 && font_h > target_h)
        scaled_w *= (float)target_h / (float)font_h;

    if (max_w_px > 0.0f && scaled_w > max_w_px)
        scaled_w = max_w_px;

    return scaled_w;
}

static void sdl_information_render_fixed_glyph(TTF_Font* font, float x_px,
    float y_px, int cell_w, int cell_h, SDL_Color color, char ch)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    char glyph[2] = { ch ? ch : ' ', '\0' };
    float scale = 1.0f;
    float dst_w;
    float dst_h;

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

    if (surface->h > cell_h)
        scale = (float)cell_h / (float)surface->h;
    if (surface->w > 0 && ((float)surface->w * scale) > (float)cell_w)
    {
        float width_scale = (float)cell_w / (float)surface->w;

        if (width_scale < scale)
            scale = width_scale;
    }

    dst_w = (float)surface->w * scale;
    dst_h = (float)surface->h * scale;

    dst.x = x_px + ((float)cell_w - dst_w) * 0.5f;
    dst.y = y_px + ((float)cell_h - dst_h) * 0.5f;
    dst.w = dst_w;
    dst.h = dst_h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void sdl_information_render_fixed_text_cells(float x_px, float y_px,
    int cell_w, int cell_h, TTF_Font* font, SDL_Color color, cptr text,
    size_t len)
{
    size_t i;

    if (!font || !text || len == 0 || cell_w <= 0 || cell_h <= 0)
        return;

    for (i = 0; i < len; i++)
    {
        sdl_information_render_fixed_glyph(font,
            x_px + (float)(i * cell_w), y_px, cell_w, cell_h, color, text[i]);
    }
}

static void sdl_information_draw_text(const sdl_view* view, int col, int row,
    byte attr, byte story, cptr text)
{
    TTF_Font* story_font = NULL;
    bool use_story;
    bool grid_align;
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

    use_story = (story & STORY_FLAG_USE) != 0;
    grid_align = (story & STORY_FLAG_CELL_ALIGN) != 0;
    if (use_story)
        story_font = sdl_story_font_for_view((sdl_view*)view);
    if (use_story && story_font)
    {
        if (grid_align)
            sdl_render_story_text_grid((sdl_view*)view, story_font, col, row,
                (int)len, text, sdl_information_color(attr));
        else
            sdl_render_story_text_free((sdl_view*)view, story_font, col, row,
                (int)len, text, sdl_information_color(attr));
        return;
    }

    sdl_render_mono_text((sdl_view*)view, col, row, (int)len, text,
        sdl_information_color(attr));
}

static void sdl_information_draw_text_fixed(
    const sdl_information_fixed_metrics* metrics, int col, int row, byte attr,
    byte story, cptr text)
{
    SDL_FRect background;
    SDL_Color color;
    bool use_story;
    bool grid_align;
    size_t len;
    TTF_Font* run_font = NULL;
    float background_w;
    float max_run_w;
    float x_px;
    float y_px;

    if (!metrics || !text || !text[0])
        return;
    if (row < 0 || row >= metrics->rows || col >= metrics->cols)
        return;
    if (col < 0)
        col = 0;

    len = strlen(text);
    if (len == 0)
        return;
    if ((size_t)col + len > (size_t)metrics->cols)
        len = (size_t)(metrics->cols - col);
    if (len == 0)
        return;

    color = sdl_information_color(attr);
    use_story = (story & STORY_FLAG_USE) != 0;
    grid_align = (story & STORY_FLAG_CELL_ALIGN) != 0;
    x_px = (float)(metrics->origin_x + col * metrics->cell_w);
    y_px = (float)(metrics->origin_y + row * metrics->cell_h);
    max_run_w = (float)(len * metrics->cell_w);
    run_font = (use_story && metrics->story_font)
        ? metrics->story_font
        : metrics->mono_font;
    background_w = max_run_w;
    if (!grid_align && run_font)
    {
        float measured_w = sdl_information_measure_text_run_px(run_font, text,
            len, metrics->cell_h, max_run_w);

        if (measured_w > 0.0f)
            background_w = measured_w;
    }
    background = (SDL_FRect){
        .x = x_px,
        .y = y_px,
        .w = background_w,
        .h = (float)metrics->cell_h
    };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &background);

    if (use_story && metrics->story_font)
    {
        if (grid_align)
        {
            sdl_information_render_fixed_text_cells(x_px, y_px, metrics->cell_w,
                metrics->cell_h, metrics->story_font, color, text, len);
        }
        else
        {
            (void)sdl_information_render_text_run_px(metrics->story_font, x_px,
                y_px, color, text, len, metrics->cell_h, max_run_w);
        }
        return;
    }

    (void)sdl_information_render_text_run_px(metrics->mono_font, x_px, y_px,
        color, text, len, metrics->cell_h, max_run_w);
}

static void sdl_information_draw_cell(const sdl_view* view,
    const app_information_op* op)
{
    SDL_FRect dst;
    byte width;

    if (!view || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_cell_rect(view, op->col, op->row, width, &dst))
        return;

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &dst);

    if (sdl_information_cell_is_raw(op->attr, op->ch, op->terrain_attr,
            op->terrain_char))
    {
        if (g_state.use_tiles && g_state.tileset)
        {
            if ((op->terrain_attr & TILE_FLAG)
                && (((byte)op->terrain_char) & TILE_FLAG))
            {
                sdl_information_draw_tile(&dst, op->terrain_attr,
                    (byte)op->terrain_char);
            }
            if (op->attr & GRAPHICS_GLOW_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_GLOW);
            sdl_information_draw_tile(&dst, op->attr, (byte)op->ch);
            if (op->terrain_attr & GRAPHICS_SLEEP_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_SLEEPING);
            if (((byte)op->terrain_char) & GRAPHICS_SEEN_MASK)
                sdl_information_draw_misc_icon(&dst,
                    ICON_MONSTER_SEES_PLAYER);
            if (((byte)op->ch) & GRAPHICS_ALERT_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_ALERT);
        }
        return;
    }

    if (op->ch)
    {
        char glyph[2] = { op->ch, '\0' };
        sdl_information_draw_text(view, op->col, op->row, op->attr, op->story,
            glyph);
    }
}

static void sdl_information_draw_cell_fixed(
    const sdl_information_fixed_metrics* metrics, const app_information_op* op)
{
    SDL_FRect dst;
    byte width;

    if (!metrics || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_fixed_rect(metrics, op->col, op->row, width,
            &dst))
    {
        return;
    }

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &dst);

    if (sdl_information_cell_is_raw(op->attr, op->ch, op->terrain_attr,
            op->terrain_char))
    {
        if (g_state.use_tiles && g_state.tileset)
        {
            if ((op->terrain_attr & TILE_FLAG)
                && (((byte)op->terrain_char) & TILE_FLAG))
            {
                sdl_information_draw_tile(&dst, op->terrain_attr,
                    (byte)op->terrain_char);
            }
            if (op->attr & GRAPHICS_GLOW_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_GLOW);
            sdl_information_draw_tile(&dst, op->attr, (byte)op->ch);
            if (op->terrain_attr & GRAPHICS_SLEEP_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_SLEEPING);
            if (((byte)op->terrain_char) & GRAPHICS_SEEN_MASK)
                sdl_information_draw_misc_icon(&dst,
                    ICON_MONSTER_SEES_PLAYER);
            if (((byte)op->ch) & GRAPHICS_ALERT_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_ALERT);
        }
        return;
    }

    if (op->ch)
    {
        int draw_w = metrics->cell_w * width;

        sdl_information_render_fixed_glyph(metrics->mono_font, dst.x, dst.y,
            draw_w, metrics->cell_h, sdl_information_color(op->attr), op->ch);
    }
}

static void sdl_information_draw_cursor(const sdl_view* view,
    const app_information_op* op)
{
    SDL_FRect rect;
    SDL_Color color;
    byte width;

    if (!view || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_cell_rect(view, op->col, op->row, width, &rect))
        return;

    color = sdl_information_color(op->attr);
    color.a = 220;
    sdl_information_draw_rect(&rect, color);
}

static void sdl_information_draw_cursor_fixed(
    const sdl_information_fixed_metrics* metrics, const app_information_op* op)
{
    SDL_FRect rect;
    SDL_Color color;
    byte width;

    if (!metrics || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_fixed_rect(metrics, op->col, op->row, width,
            &rect))
    {
        return;
    }

    color = sdl_information_color(op->attr);
    color.a = 220;
    sdl_information_draw_rect(&rect, color);
}

static bool sdl_information_render_fixed_scene(SDL_Texture* canvas,
    const sdl_view* main_view, const app_information_snapshot* snapshot,
    bool clear_canvas, bool overlay_only)
{
    sdl_information_fixed_metrics metrics;
    u16b i;

    if (!canvas || !main_view || !snapshot)
        return false;
    if (!sdl_information_resolve_fixed_metrics(main_view, &snapshot->scene,
            overlay_only, &metrics))
    {
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, canvas);
    if (clear_canvas)
    {
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
    }

    for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &snapshot->scene.ops[i];

        if (op->kind == APP_INFORMATION_OP_KIND_TEXT)
        {
            if (!op->text[0])
                continue;
            if (overlay_only
                && !sdl_information_overlay_draw_text(&snapshot->scene, op))
                continue;
            sdl_information_draw_text_fixed(&metrics, op->col, op->row,
                op->attr, op->story, op->text);
        }
        else if (op->kind == APP_INFORMATION_OP_KIND_CELL)
        {
            if (overlay_only
                && !sdl_information_overlay_draw_cell(&snapshot->scene, op))
                continue;
            sdl_information_draw_cell_fixed(&metrics, op);
        }
    }

    for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &snapshot->scene.ops[i];

        if (op->kind == APP_INFORMATION_OP_KIND_CURSOR)
        {
            if (overlay_only)
                sdl_information_draw_cursor(main_view, op);
            else
                sdl_information_draw_cursor_fixed(&metrics, op);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}

bool sdl_scene_information_render_overlay(SDL_Texture* canvas,
    const sdl_view* main_view, const app_information_snapshot* snapshot)
{
    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_INFORMATION)
        return false;
    if (!(snapshot->scene.flags & APP_INFORMATION_SCENE_FLAG_OVERLAY_DUNGEON))
        return false;

    return sdl_information_render_fixed_scene(canvas, main_view, snapshot,
        false, true);
}

bool sdl_scene_information_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_information_snapshot* snapshot)
{
    u16b i;

    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_INFORMATION)
        return false;
    if (snapshot->snapshot.blob_count < 1)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_INFORMATION
        || snapshot->blobs[0].size < sizeof(app_information_scene))
    {
        return false;
    }

    if ((snapshot->scene.flags & APP_INFORMATION_SCENE_FLAG_TERM_MIRROR)
        && sdl_information_render_fixed_scene(canvas, main_view, snapshot,
            true, false))
    {
        return true;
    }

    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (snapshot->scene.format_version == APP_INFORMATION_FORMAT_VERSION)
    {
        for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
        {
            const app_information_op* op = &snapshot->scene.ops[i];

            if (op->kind == APP_INFORMATION_OP_KIND_TEXT)
            {
                if (!op->text[0])
                    continue;
                sdl_information_draw_text(main_view, op->col, op->row,
                    op->attr, op->story, op->text);
            }
            else if (op->kind == APP_INFORMATION_OP_KIND_CELL)
            {
                sdl_information_draw_cell(main_view, op);
            }
        }

        for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
        {
            const app_information_op* op = &snapshot->scene.ops[i];

            if (op->kind == APP_INFORMATION_OP_KIND_CURSOR)
                sdl_information_draw_cursor(main_view, op);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
