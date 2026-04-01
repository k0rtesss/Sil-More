#include "angband.h"

#include "sdl-main-internal.h"

static SDL_Color sdl_menu_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static void sdl_menu_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

static void sdl_menu_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static int sdl_menu_scale_px(float logical_value)
{
    return sdl_ui_scale_px(logical_value);
}

static int sdl_menu_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

static int sdl_menu_measure_text(TTF_Font* font, cptr text)
{
    return sdl_ui_measure_text(font, text);
}

static void sdl_menu_render_text(TTF_Font* font, float x_px, float y_px,
    int line_h, SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float render_w;
    float render_h;

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
        render_h = (float)line_h;

    dst.x = x_px;
    dst.y = y_px;
    dst.w = render_w;
    dst.h = render_h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static int sdl_menu_measure_row(TTF_Font* font, const app_menu_scene* scene,
    const app_menu_row* row, int item_gap)
{
    int width = 0;

    if (!font || !scene || !row)
        return 0;

    if (!(scene->flags & APP_MENU_SCENE_FLAG_PLAIN) && row->key[0])
        width += sdl_menu_measure_text(font, row->key) + item_gap;
    if (row->label[0])
        width += sdl_menu_measure_text(font, row->label);
    if (row->meta[0])
        width += item_gap + sdl_menu_measure_text(font, row->meta);

    return width;
}

static int sdl_menu_measure_footer(TTF_Font* font,
    const app_menu_scene* scene, int pill_gap, int pill_pad_x)
{
    int width = 0;
    u16b i;

    if (!font || !scene || scene->footer_action_count == 0)
        return 0;

    for (i = 0; i < scene->footer_action_count; i++)
    {
        const app_menu_footer_action* action = &scene->footer_actions[i];
        char text[APP_MENU_KEY_MAX + APP_MENU_LABEL_MAX + 4];
        int action_w;

        if (action->key[0])
            strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
        else
            SDL_strlcpy(text, action->label, sizeof(text));

        action_w = sdl_menu_measure_text(font, text) + pill_pad_x * 2;
        if (width > 0)
            width += pill_gap;
        width += action_w;
    }

    return width;
}

static int sdl_menu_measure_tabs(TTF_Font* font, const app_menu_scene* scene,
    int pill_gap, int pill_pad_x)
{
    int width = 0;
    u16b i;

    if (!font || !scene || scene->tab_count == 0)
        return 0;

    for (i = 0; i < scene->tab_count; i++)
    {
        const app_menu_tab* tab = &scene->tabs[i];
        int tab_w = sdl_menu_measure_text(font, tab->label) + pill_pad_x * 2;

        if (width > 0)
            width += pill_gap;
        width += tab_w;
    }

    return width;
}

static void sdl_menu_render_row(TTF_Font* font, const app_menu_scene* scene,
    const app_menu_row* row, const SDL_Rect* clip_rect, int line_h,
    int item_gap, int current_y, byte accent_attr)
{
    SDL_Color color;
    SDL_Color selected_fill;
    int key_w = 0;
    int label_x = clip_rect->x;
    int meta_w = 0;
    int meta_x = clip_rect->x;

    if (!font || !scene || !row || !clip_rect)
        return;

    color = sdl_menu_color((row->flags & APP_MENU_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : row->attr);
    selected_fill = sdl_menu_color(accent_attr);
    selected_fill.a = (scene->flags & APP_MENU_SCENE_FLAG_PLAIN) ? 72 : 104;

    if ((row->flags & APP_MENU_ITEM_FLAG_SELECTED)
        && !(scene->flags & APP_MENU_SCENE_FLAG_PLAIN))
    {
        SDL_FRect selected_rect = {
            (float)(clip_rect->x - item_gap),
            (float)(current_y - sdl_menu_scale_px(3.0f)),
            (float)(clip_rect->w + item_gap * 2),
            (float)(line_h + sdl_menu_scale_px(6.0f))
        };

        sdl_menu_fill_rect(&selected_rect, selected_fill);
    }

    if (!(scene->flags & APP_MENU_SCENE_FLAG_PLAIN) && row->key[0])
    {
        key_w = sdl_menu_measure_text(font, row->key);
        sdl_menu_render_text(font, (float)clip_rect->x, (float)current_y,
            line_h, sdl_menu_color(scene->accent_attr), row->key);
        label_x += key_w + item_gap;
    }

    if (row->meta[0])
    {
        meta_w = sdl_menu_measure_text(font, row->meta);
        meta_x = clip_rect->x + clip_rect->w - meta_w;
        if (meta_x < label_x + item_gap)
            meta_x = label_x + item_gap;
    }

    if (row->label[0])
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, color, row->label);
    if (row->meta[0])
        sdl_menu_render_text(font, (float)meta_x, (float)current_y,
            line_h, color, row->meta);
}

static void sdl_menu_render_footer(TTF_Font* font,
    const app_menu_scene* scene, const SDL_Rect* clip_rect, int line_h,
    int pill_gap, int pill_pad_x, int pill_pad_y)
{
    int cursor_x;
    u16b i;

    if (!font || !scene || !clip_rect || scene->footer_action_count == 0)
        return;

    cursor_x = clip_rect->x;
    for (i = 0; i < scene->footer_action_count; i++)
    {
        const app_menu_footer_action* action = &scene->footer_actions[i];
        char text[APP_MENU_KEY_MAX + APP_MENU_LABEL_MAX + 4];
        int text_w;
        SDL_Color fill = sdl_menu_color(scene->accent_attr);
        SDL_Color border = sdl_menu_color(scene->accent_attr);
        SDL_Color text_color;
        SDL_FRect pill;

        if (action->key[0])
            strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
        else
            SDL_strlcpy(text, action->label, sizeof(text));

        text_w = sdl_menu_measure_text(font, text);
        pill.x = (float)cursor_x;
        pill.y = (float)clip_rect->y;
        pill.w = (float)(text_w + pill_pad_x * 2);
        pill.h = (float)(line_h + pill_pad_y * 2);

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        if (action->flags & APP_MENU_ITEM_FLAG_DISABLED)
        {
            fill = sdl_menu_color(TERM_L_DARK);
            fill.a = 88;
            border = sdl_menu_color(TERM_SLATE);
            border.a = 180;
            text_color = sdl_menu_color(TERM_L_DARK);
        }
        else
        {
            fill.a = 64;
            border.a = 212;
            text_color = sdl_menu_color(action->attr ? action->attr : TERM_WHITE);
        }

        sdl_menu_fill_rect(&pill, fill);
        sdl_menu_draw_rect(&pill, border);
        sdl_menu_render_text(font, pill.x + pill_pad_x, pill.y + pill_pad_y,
            line_h, text_color, text);

        cursor_x += (int)pill.w + pill_gap;
    }
}

static void sdl_menu_render_tabs(TTF_Font* font, const app_menu_scene* scene,
    const SDL_Rect* clip_rect, int line_h, int pill_gap,
    int pill_pad_x, int pill_pad_y)
{
    int cursor_x;
    u16b i;

    if (!font || !scene || !clip_rect || scene->tab_count == 0)
        return;

    cursor_x = clip_rect->x;
    for (i = 0; i < scene->tab_count; i++)
    {
        const app_menu_tab* tab = &scene->tabs[i];
        SDL_Color fill = sdl_menu_color((tab->flags & APP_MENU_ITEM_FLAG_ACTIVE)
            ? scene->accent_attr
            : TERM_SLATE);
        SDL_Color border = sdl_menu_color(scene->accent_attr);
        SDL_Color text_color = sdl_menu_color(tab->attr ? tab->attr : TERM_WHITE);
        int text_w = sdl_menu_measure_text(font, tab->label);
        SDL_FRect pill = {
            (float)cursor_x,
            (float)clip_rect->y,
            (float)(text_w + pill_pad_x * 2),
            (float)(line_h + pill_pad_y * 2)
        };

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        fill.a = (tab->flags & APP_MENU_ITEM_FLAG_ACTIVE) ? 96 : 40;
        border.a = 220;
        sdl_menu_fill_rect(&pill, fill);
        sdl_menu_draw_rect(&pill, border);
        sdl_menu_render_text(font, pill.x + pill_pad_x, pill.y + pill_pad_y,
            line_h, text_color, tab->label);

        cursor_x += (int)pill.w + pill_gap;
    }
}

static bool sdl_menu_render_scene_internal(const sdl_view* main_view,
    const app_menu_scene* scene)
{
    TTF_Font* font;
    SDL_Color panel_fill;
    SDL_Color panel_border;
    SDL_Color scrim_color = { 0, 0, 0, 112 };
    SDL_FRect panel;
    SDL_Rect left_clip;
    SDL_Rect right_clip;
    SDL_Rect footer_clip;
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;
    int line_gap;
    int section_gap;
    int item_gap;
    int pad_x;
    int pad_y;
    int outer_margin;
    int column_gap;
    int pill_gap;
    int pill_pad_x;
    int pill_pad_y;
    int top_h = 0;
    int footer_h = 0;
    int footer_space = 0;
    int column_space = 0;
    int available_column_h;
    int header_h = 0;
    int body_h = 0;
    int rows_h = 0;
    int detail_h = 0;
    int left_w = 0;
    int right_w = 0;
    int total_w;
    int max_w;
    int min_w;
    int title_w = 0;
    int subtitle_w = 0;
    int body_w = 0;
    int rows_w = 0;
    int detail_w = 0;
    int footer_w = 0;
    int tabs_w = 0;
    int tabs_h = 0;
    int row_area_gap = 0;
    int row_start = 0;
    int row_visible = 0;
    int current_y;
    bool has_detail;
    bool has_top;
    bool has_columns;
    bool has_footer;
    u16b i;

    if (!main_view || !scene)
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_scale_px((float)sdl_menu_font_size_logical(main_view));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;

    line_h = pixel_height;
    if (line_h <= 0)
        line_h = TTF_GetFontHeight(font);
    line_gap = sdl_menu_scale_px(2.0f);
    section_gap = sdl_menu_scale_px(12.0f);
    item_gap = sdl_menu_scale_px(10.0f);
    pad_x = sdl_menu_scale_px((scene->flags & APP_MENU_SCENE_FLAG_PLAIN)
        ? 16.0f : 18.0f);
    pad_y = sdl_menu_scale_px((scene->flags & APP_MENU_SCENE_FLAG_PLAIN)
        ? 12.0f : 16.0f);
    outer_margin = sdl_menu_scale_px(24.0f);
    column_gap = sdl_menu_scale_px(24.0f);
    pill_gap = sdl_menu_scale_px(10.0f);
    pill_pad_x = sdl_menu_scale_px(10.0f);
    pill_pad_y = sdl_menu_scale_px(4.0f);

    if (scene->title[0])
        title_w = sdl_menu_measure_text(font, scene->title);
    if (scene->subtitle[0])
        subtitle_w = sdl_menu_measure_text(font, scene->subtitle);
    for (i = 0; i < scene->body_line_count; i++)
        body_w = MAX(body_w, sdl_menu_measure_text(font,
            scene->body_lines[i].text));
    for (i = 0; i < scene->row_count; i++)
        rows_w = MAX(rows_w, sdl_menu_measure_row(font, scene, &scene->rows[i],
            item_gap));
    if (scene->detail_title[0])
        detail_w = sdl_menu_measure_text(font, scene->detail_title);
    for (i = 0; i < scene->detail_line_count; i++)
        detail_w = MAX(detail_w, sdl_menu_measure_text(font,
            scene->detail_lines[i].text));
    footer_w = sdl_menu_measure_footer(font, scene, pill_gap, pill_pad_x);
    tabs_w = sdl_menu_measure_tabs(font, scene, pill_gap, pill_pad_x);

    left_w = MAX(MAX(title_w, subtitle_w), MAX(body_w, rows_w));
    left_w = MAX(left_w, footer_w);
    left_w = MAX(left_w, tabs_w);
    if (left_w == 0)
        left_w = sdl_menu_scale_px(220.0f);

    has_detail = ((scene->flags & APP_MENU_SCENE_FLAG_SHOW_DETAIL) != 0)
        && (scene->detail_line_count > 0 || scene->detail_title[0]);
    if (has_detail && right_w == 0)
        right_w = MAX(detail_w, sdl_menu_scale_px(180.0f));
    else
        right_w = detail_w;

    total_w = pad_x * 2 + left_w + (has_detail ? (column_gap + right_w) : 0);
    max_w = canvas_w - outer_margin * 2;
    min_w = scene->min_width_px ? sdl_menu_scale_px((float)scene->min_width_px)
        : sdl_menu_scale_px(260.0f);
    if (scene->width_cap_px > 0)
        max_w = MIN(max_w, sdl_menu_scale_px((float)scene->width_cap_px));
    if (max_w < sdl_menu_scale_px(180.0f))
        max_w = sdl_menu_scale_px(180.0f);

    if (total_w > max_w)
    {
        if (has_detail)
        {
            int remaining = max_w - pad_x * 2 - column_gap - left_w;

            right_w = MAX(sdl_menu_scale_px(140.0f), remaining);
            total_w = pad_x * 2 + left_w + column_gap + right_w;
        }

        if (total_w > max_w)
        {
            left_w = MAX(sdl_menu_scale_px(160.0f),
                max_w - pad_x * 2 - (has_detail ? (column_gap + right_w) : 0));
            total_w = pad_x * 2 + left_w + (has_detail ? (column_gap + right_w) : 0);
        }
    }

    if (total_w < min_w)
        total_w = min_w;
    if (total_w > canvas_w)
        total_w = canvas_w;

    if (scene->tab_count > 0)
        tabs_h = line_h + pill_pad_y * 2;
    if (scene->title[0])
        header_h += line_h + line_gap;
    if (scene->subtitle[0])
        header_h += line_h + line_gap;
    if (header_h > 0)
        header_h -= line_gap;
    if (scene->body_line_count > 0)
        body_h = (scene->body_line_count * line_h)
            + ((scene->body_line_count - 1) * line_gap);
    if (has_detail)
    {
        if (scene->detail_title[0])
            detail_h += line_h + line_gap;
        if (scene->detail_line_count > 0)
            detail_h += (scene->detail_line_count * line_h)
                + ((scene->detail_line_count - 1) * line_gap);
        if (scene->detail_title[0] && scene->detail_line_count > 0)
            detail_h -= line_gap;
    }
    if (scene->footer_action_count > 0)
        footer_h = line_h + pill_pad_y * 2;

    has_top = (tabs_h > 0 || header_h > 0);
    has_footer = (footer_h > 0);
    has_columns = (body_h > 0 || scene->row_count > 0 || detail_h > 0);

    top_h = tabs_h;
    if (tabs_h > 0 && header_h > 0)
        top_h += section_gap;
    top_h += header_h;

    footer_space = (has_footer && (has_top || has_columns)) ? section_gap : 0;
    column_space = (has_columns && has_top) ? section_gap : 0;
    available_column_h = canvas_h - outer_margin * 2 - pad_y * 2 - top_h
        - column_space - footer_space - footer_h;
    if (available_column_h < line_h)
        available_column_h = line_h;

    if (scene->row_count > 0)
    {
        int row_area_available = available_column_h;

        row_area_gap = (body_h > 0) ? section_gap : 0;
        row_area_available -= body_h + row_area_gap;
        if (row_area_available < line_h)
            row_area_available = line_h;

        row_visible = (row_area_available + line_gap) / (line_h + line_gap);
        if (row_visible < 1)
            row_visible = 1;
        if (row_visible > (int)scene->row_count)
            row_visible = scene->row_count;

        row_start = scene->row_offset;
        if (row_start < 0)
            row_start = 0;
        if (scene->selected_row >= 0 && scene->selected_row < (s16b)scene->row_count)
        {
            if (scene->selected_row < row_start)
                row_start = scene->selected_row;
            if (scene->selected_row >= row_start + row_visible)
                row_start = scene->selected_row - row_visible + 1;
        }
        if (row_start + row_visible > (int)scene->row_count)
            row_start = scene->row_count - row_visible;
        if (row_start < 0)
            row_start = 0;

        rows_h = row_visible * line_h + (row_visible - 1) * line_gap;
    }

    detail_h = MIN(detail_h, available_column_h);
    column_space = has_columns && has_top ? section_gap : 0;
    column_space += has_columns ? MAX(body_h + row_area_gap + rows_h, detail_h) : 0;
    footer_space = (has_footer && (has_top || has_columns)) ? section_gap : 0;

    panel.w = (float)MIN(total_w, max_w);
    panel.h = (float)(pad_y * 2 + top_h + column_space + footer_space + footer_h);
    if (panel.h < (float)sdl_menu_scale_px(72.0f))
        panel.h = (float)sdl_menu_scale_px(72.0f);
    if (panel.h > (float)(canvas_h - outer_margin * 2))
        panel.h = (float)(canvas_h - outer_margin * 2);

    panel.x = (float)((canvas_w - (int)panel.w) / 2);
    if (scene->flags & APP_MENU_SCENE_FLAG_TOP_ANCHORED)
        panel.y = (float)outer_margin;
    else if (scene->flags & APP_MENU_SCENE_FLAG_BOTTOM_ANCHORED)
        panel.y = (float)(canvas_h - outer_margin - (int)panel.h);
    else
        panel.y = (float)((canvas_h - (int)panel.h) / 2);

    if (scene->flags & APP_MENU_SCENE_FLAG_DIM_BACKDROP)
        sdl_menu_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, (float)canvas_w, (float)canvas_h },
            scrim_color);

    panel_fill = (scene->flags & APP_MENU_SCENE_FLAG_PLAIN)
        ? (SDL_Color){ 0, 0, 0, 232 }
        : (SDL_Color){ 10, 18, 26, 224 };
    panel_border = sdl_menu_color(scene->accent_attr ? scene->accent_attr
        : TERM_L_BLUE);
    panel_border.a = 220;

    sdl_menu_fill_rect(&panel, panel_fill);
    if (!(scene->flags & APP_MENU_SCENE_FLAG_PLAIN))
        sdl_menu_draw_rect(&panel, panel_border);

    left_clip.x = (int)panel.x + pad_x;
    left_clip.y = (int)panel.y + pad_y;
    left_clip.w = left_w;
    left_clip.h = (int)panel.h - pad_y * 2;
    right_clip = left_clip;
    if (has_detail)
    {
        right_clip.x = left_clip.x + left_w + column_gap;
        right_clip.w = MAX(0, (int)panel.w - pad_x * 2 - left_w - column_gap);
    }
    footer_clip.x = (int)panel.x + pad_x;
    footer_clip.w = (int)panel.w - pad_x * 2;
    footer_clip.h = footer_h;
    footer_clip.y = (int)(panel.y + panel.h) - pad_y - footer_h;

    current_y = left_clip.y;
    if (scene->tab_count > 0)
    {
        SDL_Rect tabs_clip = {
            left_clip.x,
            current_y,
            (int)panel.w - pad_x * 2,
            tabs_h
        };

        sdl_menu_render_tabs(font, scene, &tabs_clip, line_h,
            pill_gap, pill_pad_x, pill_pad_y);
        current_y += tabs_h;
        if (header_h > 0)
            current_y += section_gap;
    }

    if (scene->title[0])
    {
        sdl_menu_render_text(font, (float)left_clip.x, (float)current_y,
            line_h, sdl_menu_color(scene->title_attr), scene->title);
        current_y += line_h + line_gap;
    }
    if (scene->subtitle[0])
    {
        sdl_menu_render_text(font, (float)left_clip.x, (float)current_y,
            line_h, sdl_menu_color(scene->subtitle_attr),
            scene->subtitle);
        current_y += line_h + line_gap;
    }
    if (has_columns)
    {
        if (scene->title[0] || scene->subtitle[0])
            current_y += section_gap - line_gap;
        else if (scene->tab_count > 0)
            current_y += section_gap;
    }

    if (body_h > 0 || rows_h > 0)
    {
        SDL_Rect column_clip = {
            left_clip.x,
            current_y,
            left_clip.w,
            footer_clip.y - current_y - ((has_footer) ? section_gap : 0)
        };

        if (column_clip.h < 0)
            column_clip.h = 0;
        SDL_SetRenderClipRect(g_state.renderer, &column_clip);
        for (i = 0; i < scene->body_line_count; i++)
        {
            sdl_menu_render_text(font, (float)left_clip.x, (float)current_y,
                line_h, sdl_menu_color(scene->body_lines[i].attr),
                scene->body_lines[i].text);
            current_y += line_h + line_gap;
        }

        if (scene->body_line_count > 0 && rows_h > 0)
            current_y += section_gap - line_gap;

        if (row_start > 0)
        {
            sdl_menu_render_text(font,
                (float)(left_clip.x + left_clip.w - sdl_menu_scale_px(10.0f)),
                (float)current_y, line_h,
                sdl_menu_color(scene->accent_attr), "^");
        }

        for (i = 0; i < (u16b)row_visible; i++)
        {
            const app_menu_row* row = &scene->rows[row_start + i];

            sdl_menu_render_row(font, scene, row, &column_clip, line_h,
                item_gap, current_y, scene->accent_attr);
            current_y += line_h + line_gap;
        }

        if (row_start + row_visible < (int)scene->row_count)
        {
            sdl_menu_render_text(font,
                (float)(left_clip.x + left_clip.w - sdl_menu_scale_px(10.0f)),
                (float)(current_y - line_gap), line_h,
                sdl_menu_color(scene->accent_attr), "v");
        }

        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (has_detail)
    {
        int detail_y = left_clip.y + top_h + (has_top ? section_gap : 0);
        SDL_Rect detail_clip_rect = {
            right_clip.x,
            detail_y,
            right_clip.w,
            footer_clip.y - detail_y - ((has_footer) ? section_gap : 0)
        };

        if (detail_clip_rect.h < 0)
            detail_clip_rect.h = 0;
        SDL_SetRenderClipRect(g_state.renderer, &detail_clip_rect);
        if (scene->detail_title[0])
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_color(scene->detail_title_attr),
                scene->detail_title);
            detail_y += line_h + line_gap;
        }
        for (i = 0; i < scene->detail_line_count; i++)
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_color(scene->detail_lines[i].attr),
                scene->detail_lines[i].text);
            detail_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (has_footer)
    {
        sdl_menu_render_footer(font, scene, &footer_clip, line_h,
            pill_gap, pill_pad_x, pill_pad_y);
    }

    return true;
}

bool sdl_scene_menu_render_overlay(const sdl_view* main_view,
    const app_menu_scene* scene)
{
    if (!main_view || !scene)
        return false;

    return sdl_menu_render_scene_internal(main_view, scene);
}

bool sdl_scene_menu_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_menu_snapshot* snapshot)
{
    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_MENU)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_MENU
        || snapshot->blobs[0].size < sizeof(app_menu_scene))
    {
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, canvas);
    if (!(snapshot->scene.flags & APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP))
    {
        SDL_SetRenderDrawColor(g_state.renderer, 6, 10, 14, 255);
        SDL_RenderClear(g_state.renderer);
    }

    if (!sdl_menu_render_scene_internal(main_view, &snapshot->scene))
    {
        SDL_SetRenderTarget(g_state.renderer, NULL);
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
