#include "angband.h"

#include "sdl-main-internal.h"

typedef struct sdl_scene_layout {
    bool hide_left_panel;
    int canvas_w;
    int canvas_h;
    int top_strip_h_px;
    int bottom_strip_h_px;
    int left_panel_w_px;
    int map_origin_x_px;
    int map_origin_y_px;
    int map_width_px;
    int map_height_px;
    int content_bottom_px;
} sdl_scene_layout;

typedef struct sdl_scene_strip_metrics {
    TTF_Font* font;
    int line_h;
    int row_count;
    int strip_h;
    int left_inset_px;
} sdl_scene_strip_metrics;

typedef struct sdl_scene_status_rail_metrics {
    TTF_Font* mono_font;
    TTF_Font* story_font;
    int line_h;
    int icon_slot_w;
    int gap_px;
    int left_inset_px;
    int panel_w_px;
    int row_visible;
} sdl_scene_status_rail_metrics;

static void sdl_scene_draw_tile(SDL_Texture* tileset, byte attr, byte ch,
    const SDL_FRect* dst);
static void sdl_scene_render_look_prompt(const sdl_view* view,
    const sdl_scene_layout* layout, const app_interaction_state* interaction);
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
    return sdl_ui_scale_px(logical_value);
}

static int sdl_scene_interaction_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

static int sdl_scene_measure_ui_text(TTF_Font* font, cptr text)
{
    return sdl_ui_measure_text(font, text);
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
    sdl_ui_render_text(font, x_px, y_px, color, text);
}

static void sdl_scene_render_ui_text_line(TTF_Font* font, float x_px,
    float y_px, int line_h, SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float render_w;
    float render_h;
    float scale = 1.0f;

    if (!font || !text || !text[0] || line_h <= 0)
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

    render_w = (float)surface->w;
    render_h = (float)surface->h;
    if (render_h > (float)line_h && render_h > 0.0f)
    {
        scale = (float)line_h / render_h;
        render_w *= scale;
        render_h *= scale;
    }

    dst.x = x_px;
    dst.y = y_px;
    dst.w = render_w;
    dst.h = render_h;

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
    SDL_Rect previous_clip;
    SDL_Rect clip_rect;
    SDL_Rect* clip_ptr = NULL;
    bool had_clip = false;
    char buf[APP_DUNGEON_PANE_TEXT_MAX + 1];
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

    advance_w = sdl_scene_measure_font_text_n(font, buf, copy_len);
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

static const app_ui_panel* sdl_scene_find_status_rail_panel(
    const app_ui_scene* scene)
{
    u16b i;

    if (!scene)
        return NULL;

    for (i = 0; i < scene->panel_count; i++)
    {
        const app_ui_panel* panel = &scene->panels[i];

        if ((panel->flags & APP_UI_PANEL_FLAG_ACTIVE)
            && (panel->flags & APP_UI_PANEL_FLAG_LEFT_ANCHORED)
            && panel->style == APP_UI_PANEL_STYLE_STATUS_RAIL)
        {
            return panel;
        }
    }

    return NULL;
}

static const app_ui_panel* sdl_scene_find_strip_panel(const app_ui_scene* scene,
    u16b anchor_flag)
{
    u16b i;

    if (!scene)
        return NULL;

    for (i = 0; i < scene->panel_count; i++)
    {
        const app_ui_panel* panel = &scene->panels[i];

        if ((panel->flags & APP_UI_PANEL_FLAG_ACTIVE)
            && panel->style == APP_UI_PANEL_STYLE_STRIP
            && (panel->flags & anchor_flag))
        {
            return panel;
        }
    }

    return NULL;
}

static const char* sdl_scene_ui_status_label_text(const app_ui_row* row)
{
    if (!row)
        return "";
    if (row->key[0])
        return row->key;
    return row->label;
}

static int sdl_scene_ui_status_gap_px(TTF_Font* mono_font)
{
    int gap_px = sdl_scene_measure_ui_text(mono_font, " ");

    if (gap_px < 4)
        gap_px = 4;

    return gap_px;
}

static int sdl_scene_ui_status_icon_slot_px(TTF_Font* mono_font, int line_h)
{
    int icon_slot_w = sdl_scene_measure_ui_text(mono_font, "MM");

    if (icon_slot_w < line_h)
        icon_slot_w = line_h;
    if (icon_slot_w < 1)
        icon_slot_w = 1;

    return icon_slot_w;
}

static int sdl_scene_ui_status_label_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_row* row, cptr text)
{
    if (!text || !text[0])
        return 0;
    if ((row->flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
        return sdl_scene_measure_ui_text(story_font, text);

    return sdl_scene_measure_ui_text(mono_font, text);
}

static int sdl_scene_ui_status_row_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, const app_ui_row* row)
{
    const char* label_text = sdl_scene_ui_status_label_text(row);
    int icon_slot_w;
    int gap_px;
    int label_w;
    int meta_w;
    int width = 0;

    if (!mono_font || !row)
        return 0;

    icon_slot_w = sdl_scene_ui_status_icon_slot_px(mono_font, line_h);
    gap_px = sdl_scene_ui_status_gap_px(mono_font);
    label_w = sdl_scene_ui_status_label_width_px(mono_font, story_font, row,
        label_text);
    meta_w = sdl_scene_measure_ui_text(mono_font, row->meta);

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        return label_w;

    if (row->extra_icon_char)
    {
        width = label_w + icon_slot_w + meta_w;
        if (row->icon_char)
            width += icon_slot_w;
        if (label_w > 0 && meta_w > 0)
            width += gap_px;
        return width;
    }

    if (row->icon_char)
    {
        width += icon_slot_w;
        if (label_w > 0)
            width += gap_px;
    }
    width += label_w;
    if (row->meta[0])
    {
        if (width > 0)
            width += gap_px;
        width += meta_w;
    }

    return width;
}

static bool sdl_scene_ui_measure_strip(const sdl_view* view,
    const app_ui_panel* panel, int canvas_h, sdl_scene_strip_metrics* out_metrics)
{
    int pixel_height;

    if (out_metrics)
        memset(out_metrics, 0, sizeof(*out_metrics));
    if (!view || !panel || !out_metrics || canvas_h <= 0)
        return false;

    pixel_height = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    out_metrics->font = sdl_ui_font_for_height(pixel_height);
    if (!out_metrics->font)
        return false;

    out_metrics->line_h = MAX(pixel_height, TTF_GetFontHeight(out_metrics->font));
    out_metrics->row_count = panel->body_line_count ? (int)panel->body_line_count
        : 1;
    out_metrics->strip_h = out_metrics->row_count * out_metrics->line_h;
    if (out_metrics->strip_h < view->cell_h)
        out_metrics->strip_h = view->cell_h;
    if (out_metrics->strip_h > canvas_h)
        out_metrics->strip_h = canvas_h;
    out_metrics->left_inset_px = MAX(sdl_scene_ui_scale_px(4.0f),
        sdl_ui_text_left_padding(out_metrics->font, out_metrics->line_h));
    return true;
}

static int sdl_scene_ui_strip_height_px(const sdl_view* view,
    const app_ui_panel* panel, int canvas_h)
{
    sdl_scene_strip_metrics metrics;

    if (!sdl_scene_ui_measure_strip(view, panel, canvas_h, &metrics))
        return 0;

    return metrics.strip_h;
}

static bool sdl_scene_ui_measure_status_rail(const sdl_view* view,
    const app_ui_panel* panel, int canvas_w, int available_h_px,
    sdl_scene_status_rail_metrics* out_metrics)
{
    int desired_px;
    int min_px;
    int pixel_height;

    if (out_metrics)
        memset(out_metrics, 0, sizeof(*out_metrics));
    if (!view || !panel || !out_metrics || panel->row_count == 0
        || canvas_w <= 0 || available_h_px <= 0)
    {
        return false;
    }

    desired_px = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    min_px = sdl_scene_ui_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        int mono_h;
        int story_h = 0;
        int candidate_w_px = 0;
        int max_w_px;
        int line_h;
        int row_visible;
        int left_inset_px;
        u16b row_index;
        TTF_Font* mono_font;
        TTF_Font* story_font;

        mono_font = sdl_ui_font_for_height(pixel_height);
        story_font = sdl_story_font_for_height(pixel_height);
        if (!mono_font)
            continue;

        mono_h = TTF_GetFontHeight(mono_font);
        if (story_font)
            story_h = TTF_GetFontHeight(story_font);
        line_h = MAX(pixel_height, MAX(mono_h, story_h));
        if (line_h < 1)
            line_h = 1;
        left_inset_px = MAX(sdl_scene_ui_scale_px(4.0f),
            sdl_ui_text_pair_left_padding(mono_font,
                story_font ? story_font : mono_font, line_h));

        for (row_index = 0; row_index < panel->row_count; row_index++)
        {
            candidate_w_px = MAX(candidate_w_px,
                left_inset_px + sdl_scene_ui_status_row_width_px(mono_font,
                    story_font, line_h, &panel->rows[row_index]));
        }
        if (panel->min_width_px > 0)
        {
            int min_w_px = sdl_scene_ui_scale_px((float)panel->min_width_px);

            candidate_w_px = MAX(candidate_w_px, min_w_px);
        }
        max_w_px = panel->width_cap_px > 0
            ? sdl_scene_ui_scale_px((float)panel->width_cap_px)
            : 0;
        if (max_w_px > 0 && candidate_w_px > max_w_px)
            candidate_w_px = max_w_px;

        row_visible = available_h_px / line_h;
        if (candidate_w_px > canvas_w - view->cell_w)
            candidate_w_px = MAX(0, canvas_w - view->cell_w);
        if (candidate_w_px <= 0 || candidate_w_px > canvas_w || row_visible < 1)
        {
            continue;
        }

        out_metrics->mono_font = mono_font;
        out_metrics->story_font = story_font;
        out_metrics->line_h = line_h;
        out_metrics->icon_slot_w = sdl_scene_ui_status_icon_slot_px(mono_font,
            line_h);
        out_metrics->gap_px = sdl_scene_ui_status_gap_px(mono_font);
        out_metrics->left_inset_px = left_inset_px;
        out_metrics->panel_w_px = candidate_w_px;
        out_metrics->row_visible = MIN((int)panel->row_count, row_visible);
        return out_metrics->row_visible > 0;
    }

    return false;
}

static int sdl_scene_ui_left_reserved_px(const sdl_view* view,
    const app_ui_panel* panel, int canvas_w, int available_h_px)
{
    sdl_scene_status_rail_metrics metrics;

    if (!sdl_scene_ui_measure_status_rail(view, panel, canvas_w, available_h_px,
            &metrics))
    {
        return 0;
    }

    return metrics.panel_w_px;
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

static int sdl_scene_map_cell_width_px(const sdl_view* view)
{
    if (!view || view->cell_w <= 0)
        return 0;

    return view->cell_w * ((use_bigtile && !graphics_are_ascii()) ? 2 : 1);
}

static bool sdl_scene_map_cell_rect(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map, int map_y,
    int map_x, SDL_FRect* out_rect)
{
    int cell_w_px;
    int cell_x;
    int cell_y;

    if (!view || !layout || !map || !out_rect)
        return false;
    if (map_y < map->panel_y || map_y >= map->panel_y + map->height)
        return false;
    if (map_x < map->panel_x || map_x >= map->panel_x + map->width)
        return false;

    cell_w_px = sdl_scene_map_cell_width_px(view);
    if (cell_w_px <= 0 || view->cell_h <= 0)
        return false;

    cell_x = map_x - map->panel_x;
    cell_y = map_y - map->panel_y;
    *out_rect = (SDL_FRect){
        .x = (float)(layout->map_origin_x_px + cell_x * cell_w_px),
        .y = (float)(layout->map_origin_y_px + cell_y * view->cell_h),
        .w = (float)cell_w_px,
        .h = (float)view->cell_h
    };
    return true;
}

static bool sdl_scene_layout_map_clip_rect(const sdl_scene_layout* layout,
    SDL_Rect* out_rect)
{
    int clip_w;
    int clip_h;

    if (!layout || !out_rect)
        return false;
    if (layout->map_width_px <= 0 || layout->map_height_px <= 0)
        return false;
    if (layout->canvas_w <= 0 || layout->canvas_h <= 0)
        return false;

    clip_w = layout->map_width_px;
    clip_h = layout->map_height_px;
    if (layout->map_origin_x_px + clip_w > layout->canvas_w)
        clip_w = layout->canvas_w - layout->map_origin_x_px;
    if (layout->map_origin_y_px + clip_h > layout->content_bottom_px)
        clip_h = layout->content_bottom_px - layout->map_origin_y_px;
    if (clip_w <= 0 || clip_h <= 0)
        return false;

    *out_rect = (SDL_Rect){
        layout->map_origin_x_px,
        layout->map_origin_y_px,
        clip_w,
        clip_h
    };
    return true;
}

static void sdl_scene_draw_mono_glyph_px(const sdl_view* view, float x_px,
    float y_px, char ch, SDL_Color color)
{
    SDL_FRect src;
    SDL_FRect dst;
    unsigned char glyph;

    if (!view || !view->font_atlas)
        return;

    glyph = (unsigned char)ch;
    SDL_SetTextureColorMod(view->font_atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(view->font_atlas, 255);

    src = (SDL_FRect){
        (float)((glyph & 15) * view->cell_w),
        (float)((glyph >> 4) * view->cell_h),
        (float)view->cell_w,
        (float)view->cell_h
    };
    dst = (SDL_FRect){ x_px, y_px, (float)view->cell_w, (float)view->cell_h };
    if (use_graphics == GRAPHICS_PSEUDO
        && solid_walls
        && (glyph == '#' || glyph == '%'))
    {
        sdl_scene_fill_rect(&dst, color);
    }
    SDL_RenderTexture(g_state.renderer, view->font_atlas, &src, &dst);
}

static void sdl_scene_draw_mono_text_px(const sdl_view* view, float x_px,
    float y_px, byte attr, cptr text)
{
    SDL_Color color;
    float current_x;
    float canvas_w;
    size_t i;

    if (!view || !text || !text[0])
        return;
    if (view->cell_w <= 0 || view->cell_h <= 0)
        return;
    canvas_w = (float)(view->cols * view->cell_w);
    if (canvas_w <= 0.0f || y_px >= (float)(view->rows * view->cell_h))
        return;
    if (x_px < 0.0f)
        x_px = 0.0f;

    color = sdl_scene_color(attr);
    current_x = x_px;
    for (i = 0; text[i]; i++)
    {
        if (current_x >= canvas_w)
            break;
        sdl_scene_draw_mono_glyph_px(view, current_x, y_px, text[i], color);
        current_x += (float)view->cell_w;
    }
}

static sdl_scene_layout sdl_scene_make_layout(const sdl_view* view,
    const app_map_snapshot* map, u16b status_flags,
    const app_ui_scene* chrome_scene)
{
    sdl_scene_layout layout;
    const app_ui_panel* top_strip;
    const app_ui_panel* bottom_strip;
    const app_ui_panel* status_rail;
    int chrome_content_h;
    int max_map_x_px;
    int max_map_y_px;
    int map_cell_w_px;

    memset(&layout, 0, sizeof(layout));
    layout.hide_left_panel =
        (status_flags & APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL) ? true : false;
    if (!view)
        return layout;

    layout.canvas_w = view->cols * view->cell_w;
    layout.canvas_h = view->rows * view->cell_h;
    if (!map)
        return layout;

    map_cell_w_px = sdl_scene_map_cell_width_px(view);
    layout.map_width_px = map->width * map_cell_w_px;
    layout.map_height_px = map->height * view->cell_h;

    if (chrome_scene)
    {
        top_strip = sdl_scene_find_strip_panel(chrome_scene,
            APP_UI_PANEL_FLAG_TOP_ANCHORED);
        bottom_strip = sdl_scene_find_strip_panel(chrome_scene,
            APP_UI_PANEL_FLAG_BOTTOM_ANCHORED);
        status_rail = sdl_scene_find_status_rail_panel(chrome_scene);
        layout.top_strip_h_px = sdl_scene_ui_strip_height_px(view, top_strip,
            layout.canvas_h);
        layout.bottom_strip_h_px = sdl_scene_ui_strip_height_px(view, bottom_strip,
            layout.canvas_h);
        chrome_content_h = layout.canvas_h - layout.top_strip_h_px
            - layout.bottom_strip_h_px;
        if (chrome_content_h < 0)
            chrome_content_h = 0;
        layout.left_panel_w_px = sdl_scene_ui_left_reserved_px(view, status_rail,
            layout.canvas_w, chrome_content_h);
        if (!layout.hide_left_panel)
        {
            layout.map_origin_x_px = layout.left_panel_w_px;
        }
    }

    layout.content_bottom_px = layout.canvas_h - layout.bottom_strip_h_px;
    if (layout.content_bottom_px < 0)
        layout.content_bottom_px = 0;
    layout.map_origin_y_px = layout.top_strip_h_px;

    max_map_x_px = layout.canvas_w - layout.map_width_px;
    max_map_y_px = layout.content_bottom_px - layout.map_height_px;
    if (max_map_x_px < 0)
        max_map_x_px = 0;
    if (max_map_y_px < 0)
        max_map_y_px = 0;
    if (layout.map_origin_x_px > max_map_x_px)
        layout.map_origin_x_px = max_map_x_px;
    if (layout.map_origin_y_px > max_map_y_px)
        layout.map_origin_y_px = max_map_y_px;

    return layout;
}

static void sdl_scene_render_icon(TTF_Font* font, float x_px, float y_px,
    int icon_slot_w, int line_h, byte icon_attr, char icon_char)
{
    SDL_FRect tile_dst;
    byte ch = (byte)icon_char;

    if (!font || !icon_char || icon_char == ' ')
        return;

    if (g_state.use_tiles && g_state.tileset
        && (icon_attr & TILE_FLAG) && (ch & TILE_FLAG))
    {
        float tile_size = (float)MIN(line_h, icon_slot_w);

        tile_dst.x = x_px + ((float)icon_slot_w - tile_size) * 0.5f;
        tile_dst.y = y_px + ((float)line_h - tile_size) * 0.5f;
        tile_dst.w = tile_size;
        tile_dst.h = tile_size;
        sdl_scene_draw_tile(g_state.tileset, icon_attr, ch, &tile_dst);
        return;
    }

    {
        char glyph[2] = { icon_char, '\0' };
        int glyph_w = sdl_scene_measure_ui_text(font, glyph);
        float text_x = x_px;

        if (glyph_w < icon_slot_w)
            text_x += ((float)icon_slot_w - (float)glyph_w) * 0.5f;
        sdl_scene_render_ui_text_line(font, text_x, y_px, line_h,
            sdl_scene_color(icon_attr ? icon_attr : TERM_WHITE), glyph);
    }
}

static void sdl_scene_render_status_rail_label(TTF_Font* mono_font,
    TTF_Font* story_font, float x_px, float y_px, int line_h, byte attr,
    byte flags, cptr text)
{
    if (!text || !text[0])
        return;

    if ((flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
    {
        sdl_scene_render_ui_text_line(story_font, x_px, y_px, line_h,
            sdl_scene_color(attr), text);
        return;
    }

    sdl_scene_render_ui_text_line(mono_font, x_px, y_px, line_h,
        sdl_scene_color(attr), text);
}

static bool sdl_scene_render_chrome_strip_panel(const sdl_view* view,
    const sdl_scene_layout* layout, const app_ui_panel* panel)
{
    sdl_scene_strip_metrics metrics;
    SDL_Rect clip_rect;
    float y_px;
    int strip_h;
    int current_y;
    u16b i;

    if (!view || !layout || !panel)
        return false;
    if (!sdl_scene_ui_measure_strip(view, panel, layout->canvas_h, &metrics))
        return false;

    strip_h = (panel->flags & APP_UI_PANEL_FLAG_BOTTOM_ANCHORED)
        ? layout->bottom_strip_h_px
        : layout->top_strip_h_px;
    if (strip_h <= 0)
        strip_h = metrics.strip_h;
    if (strip_h <= 0 || layout->canvas_w <= 0)
        return false;

    y_px = (panel->flags & APP_UI_PANEL_FLAG_BOTTOM_ANCHORED)
        ? (float)(layout->canvas_h - strip_h)
        : 0.0f;
    sdl_scene_fill_rect(&(SDL_FRect){
        .x = 0.0f,
        .y = y_px,
        .w = (float)layout->canvas_w,
        .h = (float)strip_h
    }, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = 0;
    clip_rect.y = (int)y_px;
    clip_rect.w = layout->canvas_w;
    clip_rect.h = strip_h;
    if (clip_rect.w <= 0 || clip_rect.h <= 0)
        return false;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    current_y = (int)y_px + ((strip_h - metrics.row_count * metrics.line_h) / 2);
    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];

        if (line->text[0] && line->text[0] != ' ')
        {
            sdl_scene_render_ui_text_line(metrics.font,
                (float)metrics.left_inset_px, (float)current_y, metrics.line_h,
                sdl_scene_color(line->attr), line->text);
        }
        current_y += metrics.line_h;
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

static bool sdl_scene_render_chrome_status_rail_panel(const sdl_view* view,
    const sdl_scene_layout* layout, const app_ui_panel* panel)
{
    sdl_scene_status_rail_metrics metrics;
    SDL_FRect clear_rect;
    SDL_Rect clip_rect;
    int available_h_px;
    u16b i;

    if (!view || !layout || !panel || panel->row_count == 0
        || layout->left_panel_w_px <= 0)
    {
        return false;
    }

    available_h_px = layout->content_bottom_px - layout->top_strip_h_px;
    if (!sdl_scene_ui_measure_status_rail(view, panel, layout->canvas_w,
            available_h_px, &metrics))
    {
        return false;
    }
    metrics.panel_w_px = layout->left_panel_w_px;

    clear_rect.x = 0.0f;
    clear_rect.y = (float)layout->top_strip_h_px;
    clear_rect.w = (float)metrics.panel_w_px;
    clear_rect.h = (float)(metrics.row_visible * metrics.line_h);
    if (clear_rect.w <= 0.0f || clear_rect.h <= 0.0f)
        return false;
    sdl_scene_fill_rect(&clear_rect, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = 0;
    clip_rect.y = (int)clear_rect.y;
    clip_rect.w = metrics.panel_w_px;
    clip_rect.h = (int)clear_rect.h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    for (i = 0; i < (u16b)metrics.row_visible; i++)
    {
        const app_ui_row* row = &panel->rows[i];
        float y_px = clear_rect.y + (float)((int)i * metrics.line_h);
        const char* label_text = sdl_scene_ui_status_label_text(row);
        byte label_attr = row->attr ? row->attr : TERM_WHITE;
        byte meta_attr = row->meta_attr ? row->meta_attr : label_attr;
        int label_w = sdl_scene_ui_status_label_width_px(metrics.mono_font,
            metrics.story_font, row, label_text);
        int meta_w = sdl_scene_measure_ui_text(metrics.mono_font, row->meta);
        float content_x = (float)metrics.left_inset_px;

        if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        {
            sdl_scene_render_status_rail_label(metrics.mono_font,
                metrics.story_font, content_x, y_px, metrics.line_h, label_attr,
                row->flags, row->label[0] ? row->label : row->key);
            continue;
        }

        if (row->extra_icon_char)
        {
            int group_w = label_w + metrics.icon_slot_w + meta_w;
            float x_px;

            if (row->icon_char)
            {
                group_w += metrics.icon_slot_w;
                if (label_w > 0 && meta_w > 0)
                    group_w += metrics.gap_px;
                x_px = (float)metrics.panel_w_px - (float)group_w;
                if (x_px < content_x)
                    x_px = content_x;
                sdl_scene_render_icon(metrics.mono_font, x_px, y_px,
                    metrics.icon_slot_w, metrics.line_h, row->icon_attr,
                    row->icon_char);
                x_px += (float)metrics.icon_slot_w;
                if (row->label[0])
                {
                    sdl_scene_render_ui_text_line(metrics.mono_font, x_px, y_px,
                        metrics.line_h, sdl_scene_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)metrics.gap_px;
                sdl_scene_render_icon(metrics.mono_font, x_px, y_px,
                    metrics.icon_slot_w, metrics.line_h,
                    row->extra_icon_attr, row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_scene_render_ui_text_line(metrics.mono_font,
                        x_px + (float)metrics.icon_slot_w, y_px,
                        metrics.line_h, sdl_scene_color(meta_attr), row->meta);
                }
            }
            else
            {
                if (label_w > 0 && meta_w > 0)
                    group_w += metrics.gap_px;
                x_px = (float)metrics.panel_w_px - (float)group_w;
                if (x_px < content_x)
                    x_px = content_x;
                if (row->label[0])
                {
                    sdl_scene_render_ui_text_line(metrics.mono_font, x_px, y_px,
                        metrics.line_h, sdl_scene_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)metrics.gap_px;
                sdl_scene_render_icon(metrics.mono_font, x_px, y_px,
                    metrics.icon_slot_w, metrics.line_h,
                    row->extra_icon_attr, row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_scene_render_ui_text_line(metrics.mono_font,
                        x_px + (float)metrics.icon_slot_w, y_px,
                        metrics.line_h, sdl_scene_color(meta_attr), row->meta);
                }
            }
            continue;
        }

        if (row->icon_char)
        {
            sdl_scene_render_icon(metrics.mono_font, content_x, y_px,
                metrics.icon_slot_w, metrics.line_h, row->icon_attr,
                row->icon_char);
            if (row->label[0])
            {
                sdl_scene_render_status_rail_label(metrics.mono_font,
                    metrics.story_font,
                    content_x + (float)(metrics.icon_slot_w + metrics.gap_px),
                    y_px, metrics.line_h, label_attr, row->flags, row->label);
            }
            if (row->meta[0])
            {
                float meta_x = (float)metrics.panel_w_px - (float)meta_w;
                float min_meta_x = (row->label[0]
                    ? content_x + (float)(metrics.icon_slot_w + metrics.gap_px
                        + label_w + metrics.gap_px)
                    : content_x + (float)metrics.icon_slot_w);

                if (meta_x < min_meta_x)
                    meta_x = min_meta_x;
                sdl_scene_render_ui_text_line(metrics.mono_font, meta_x, y_px,
                    metrics.line_h, sdl_scene_color(meta_attr), row->meta);
            }
            continue;
        }

        if (label_text[0])
        {
            sdl_scene_render_status_rail_label(metrics.mono_font,
                metrics.story_font, content_x, y_px, metrics.line_h, label_attr,
                row->flags, label_text);
        }
        if (row->meta[0])
        {
            float meta_x = (float)metrics.panel_w_px - (float)meta_w;

            if (label_text[0]
                && meta_x < content_x + (float)(label_w + metrics.gap_px))
            {
                meta_x = content_x + (float)(label_w + metrics.gap_px);
            }
            if (meta_x < content_x)
                meta_x = content_x;
            sdl_scene_render_ui_text_line(metrics.mono_font, meta_x, y_px,
                metrics.line_h, sdl_scene_color(meta_attr), row->meta);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

static bool sdl_scene_render_chrome_scene(const sdl_view* view,
    const sdl_scene_layout* layout, const app_ui_scene* scene)
{
    u16b i;

    if (!view || !layout || !scene)
        return false;

    /* Keep dungeon chrome geometry and rendering coupled in this renderer. */
    for (i = 0; i < scene->panel_count; i++)
    {
        const app_ui_panel* panel = &scene->panels[i];

        if (!(panel->flags & APP_UI_PANEL_FLAG_ACTIVE))
            continue;

        if (panel->style == APP_UI_PANEL_STYLE_STRIP)
        {
            if (!sdl_scene_render_chrome_strip_panel(view, layout, panel))
                return false;
            continue;
        }

        if (panel->style == APP_UI_PANEL_STYLE_STATUS_RAIL)
        {
            if (!sdl_scene_render_chrome_status_rail_panel(view, layout, panel))
                return false;
            continue;
        }

        log_warn("dungeon chrome render: unsupported panel style %u",
            (unsigned)panel->style);
        return false;
    }

    return true;
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

static const app_panes_snapshot* sdl_scene_panes_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_PANES);

    if (!blob || blob->size < sizeof(app_panes_snapshot))
        return NULL;
    return (const app_panes_snapshot*)blob->data;
}

static const app_dungeon_overlay_snapshot* sdl_scene_overlay_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_OVERLAY);

    if (!blob || blob->size < sizeof(app_dungeon_overlay_snapshot))
        return NULL;
    return (const app_dungeon_overlay_snapshot*)blob->data;
}

static const app_interaction_state* sdl_scene_overlay_interaction(
    const app_dungeon_overlay_snapshot* overlay)
{
    if (!overlay)
        return NULL;

    return &overlay->interaction;
}

static const app_ui_scene* sdl_scene_overlay_transient_scene(
    const app_dungeon_overlay_snapshot* overlay)
{
    if (!overlay)
        return NULL;
    if (!(overlay->flags & APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_TRANSIENT_MENU))
        return NULL;
    if (overlay->transient_scene.panel_count == 0)
        return NULL;

    return &overlay->transient_scene;
}

static const app_ui_scene* sdl_scene_overlay_chrome_scene(
    const app_dungeon_overlay_snapshot* overlay)
{
    if (!overlay || overlay->chrome_scene.panel_count == 0)
        return NULL;

    return &overlay->chrome_scene;
}

static void sdl_scene_render_interaction_overlay(const sdl_view* view,
    const sdl_scene_layout* layout, const app_interaction_state* interaction)
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

    if (interaction->kind == APP_INTERACTION_KIND_LOOK && interaction->prompt[0])
    {
        sdl_scene_render_look_prompt(view, layout, interaction);
        return;
    }
    if (interaction->kind == APP_INTERACTION_KIND_LOOK)
        return;

    canvas_w = view->cols * view->cell_w;
    canvas_h = view->rows * view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return;

    pixel_height = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    font = sdl_ui_font_for_height(pixel_height);
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
    SDL_FRect dst;
    byte terrain_ch = (byte)cell->terrain_char;
    byte ch = (byte)cell->ch;

    if (!view || !layout || !map || !cell)
        return;
    if (!sdl_scene_map_cell_rect(view, layout, map, cell->map_y, cell->map_x,
            &dst))
    {
        return;
    }

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
        sdl_scene_draw_mono_glyph_px(view, dst.x, dst.y,
            cell->ch ? cell->ch : ' ', sdl_scene_color(cell->attr));
        if (use_bigtile && !graphics_are_ascii())
        {
            sdl_scene_draw_mono_glyph_px(view, dst.x + view->cell_w, dst.y,
                ' ', sdl_scene_color(TERM_WHITE));
        }
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

static void sdl_scene_render_look_prompt(const sdl_view* view,
    const sdl_scene_layout* layout, const app_interaction_state* interaction)
{
    TTF_Font* font;
    const char* text;
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;

    if (!view || !layout || !interaction)
        return;
    (void)layout;

    text = interaction->prompt[0] ? interaction->prompt : interaction->detail;
    if (!text || !text[0])
        return;

    canvas_w = view->cols * view->cell_w;
    canvas_h = view->rows * view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return;

    pixel_height = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
    {
        if (view->cell_h > 0)
        {
            sdl_scene_fill_rect(&(SDL_FRect){
                .x = 0.0f,
                .y = 0.0f,
                .w = (float)canvas_w,
                .h = (float)view->cell_h
            }, (SDL_Color){ 0, 0, 0, 255 });
        }
        sdl_scene_draw_mono_text_px(view, 0.0f, 0.0f,
            interaction->prompt_attr ? interaction->prompt_attr : TERM_WHITE,
            text);
        return;
    }

    line_h = MAX(pixel_height, TTF_GetFontHeight(font));
    if (line_h < 1)
        line_h = pixel_height;
    if (line_h > canvas_h)
        line_h = canvas_h;
    if (line_h > 0)
    {
        sdl_scene_fill_rect(&(SDL_FRect){
            .x = 0.0f,
            .y = 0.0f,
            .w = (float)canvas_w,
            .h = (float)line_h
        }, (SDL_Color){ 0, 0, 0, 255 });
    }

    sdl_scene_render_ui_text(font, 0.0f, 0.0f,
        sdl_scene_color(interaction->prompt_attr
            ? interaction->prompt_attr
            : TERM_WHITE),
        text);
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
    TTF_Font* font = NULL;
    int lines;
    int available_h_px;
    int desired_px;
    int min_px;
    int line_h = 0;
    float current_y;
    float max_w_px;
    int pixel_height;

    if (!view || !layout || !map || !panes || panes->main_combat_roll_lines <= 0)
        return;

    lines = panes->main_combat_roll_lines;
    if (lines > 3)
        lines = 3;
    if (lines > panes->combat_entry_count)
        lines = panes->combat_entry_count;
    if (lines <= 0)
        return;

    available_h_px = layout->content_bottom_px
        - (layout->map_origin_y_px + layout->map_height_px);
    if (available_h_px <= 0)
        return;

    desired_px = sdl_scene_ui_scale_px(
        (float)sdl_scene_interaction_font_size_logical(view));
    min_px = sdl_scene_ui_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        int candidate_h;

        font = sdl_ui_font_for_height(pixel_height);
        if (!font)
            continue;

        candidate_h = MAX(pixel_height, TTF_GetFontHeight(font));
        if (candidate_h < 1)
            continue;
        if (candidate_h * lines <= available_h_px)
        {
            line_h = candidate_h;
            break;
        }
    }
    if (!font || line_h <= 0)
        return;

    current_y = (float)(layout->map_origin_y_px + layout->map_height_px);
    max_w_px = (float)(layout->canvas_w - layout->map_origin_x_px);
    for (int i = 0; i < lines; i++)
    {
        char buf[80];

        sdl_scene_format_combat_line(buf, sizeof(buf), &panes->combat_entries[i]);
        (void)sdl_scene_render_text_run_px(font,
            (float)layout->map_origin_x_px, current_y, sdl_scene_color(TERM_WHITE),
            buf, strlen(buf), line_h, max_w_px);
        current_y += (float)line_h;
    }
}

static void sdl_scene_draw_animation_rect(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map, int map_y,
    int map_x, SDL_Color color)
{
    SDL_FRect rect;

    if (!sdl_scene_map_cell_rect(view, layout, map, map_y, map_x, &rect))
        return;

    sdl_scene_fill_rect(&rect, color);
}

static void sdl_scene_render_move_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    SDL_FRect from_rect;
    SDL_FRect to_rect;
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
    if (!sdl_scene_map_cell_rect(view, layout, map, anim->from_y, anim->from_x,
            &from_rect))
    {
        return;
    }
    if (!sdl_scene_map_cell_rect(view, layout, map, anim->to_y, anim->to_x,
            &to_rect))
    {
        return;
    }

    start_x = from_rect.x + from_rect.w * 0.5f;
    start_y = from_rect.y + from_rect.h * 0.5f;
    end_x = to_rect.x + to_rect.w * 0.5f;
    end_y = to_rect.y + to_rect.h * 0.5f;
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
    SDL_FRect from_rect;
    SDL_FRect to_rect;
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float x;
    float y;
    SDL_Color color = sdl_scene_color(TERM_YELLOW);

    if (!view || !layout || !map || !anim)
        return;
    if (!sdl_scene_map_cell_rect(view, layout, map, anim->from_y, anim->from_x,
            &from_rect))
    {
        return;
    }
    if (!sdl_scene_map_cell_rect(view, layout, map, anim->to_y, anim->to_x,
            &to_rect))
    {
        return;
    }

    start_x = from_rect.x + from_rect.w * 0.5f;
    start_y = from_rect.y + from_rect.h * 0.5f;
    end_x = to_rect.x + to_rect.w * 0.5f;
    end_y = to_rect.y + to_rect.h * 0.5f;
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
            .x = (float)layout->map_origin_x_px,
            .y = (float)layout->map_origin_y_px,
            .w = (float)layout->map_width_px,
            .h = (float)layout->map_height_px
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
    const app_panes_snapshot* panes;
    const app_dungeon_overlay_snapshot* overlay;
    const app_interaction_state* interaction;
    const app_ui_scene* transient_scene;
    const app_ui_scene* chrome_scene;
    sdl_scene_layout layout;
    SDL_Rect map_clip_rect;
    bool have_map_clip;
    size_t i;

    if (!canvas || !main_view || !snapshot)
        return false;

    map = sdl_scene_map_snapshot(snapshot);
    status = sdl_scene_status_snapshot(snapshot);
    panes = sdl_scene_panes_snapshot(snapshot);
    overlay = sdl_scene_overlay_snapshot(snapshot);
    interaction = sdl_scene_overlay_interaction(overlay);
    transient_scene = sdl_scene_overlay_transient_scene(overlay);
    chrome_scene = sdl_scene_overlay_chrome_scene(overlay);
    if (!map || !status || !panes || !overlay)
        return false;

    layout = sdl_scene_make_layout(main_view, map, status->flags, chrome_scene);
    have_map_clip = sdl_scene_layout_map_clip_rect(&layout, &map_clip_rect);

    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (have_map_clip)
        SDL_SetRenderClipRect(g_state.renderer, &map_clip_rect);
    for (i = 0; i < map->cell_count; i++)
        sdl_scene_draw_map_cell(main_view, &layout, map, &map->cells[i]);
    sdl_scene_render_animations(main_view, &layout, map, animations,
        animation_count, now_ns);
    if (have_map_clip)
        SDL_SetRenderClipRect(g_state.renderer, NULL);

    sdl_scene_render_combat_overlay(main_view, &layout, map, panes);
    if (chrome_scene && !sdl_scene_render_chrome_scene(main_view, &layout,
            chrome_scene))
    {
        SDL_SetRenderTarget(g_state.renderer, NULL);
        return false;
    }
    if (transient_scene)
        (void)sdl_scene_ui_render_overlay(main_view,
            layout.canvas_w, layout.canvas_h, transient_scene);
    sdl_scene_render_interaction_overlay(main_view, &layout, interaction);

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
