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

#include "sdl-scene-menu.h"

static int sdl_menu_character_metric_group_width(TTF_Font* mono_font,
    const app_ui_character_metric* metric, int token_gap)
{
    int width = 0;

    if (!mono_font || !metric)
        return 0;

    if (metric->value[0])
        width += sdl_menu_measure_text(mono_font, metric->value);
    if (metric->separator && metric->secondary[0])
    {
        if (width > 0)
            width += token_gap;
        width += sdl_menu_measure_text(mono_font,
            (char[]){ metric->separator, '\0' });
        width += token_gap;
        width += sdl_menu_measure_text(mono_font, metric->secondary);
    }

    return width;
}

static int sdl_menu_character_metric_row_width(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_character_metric* metric, int label_gap,
    int token_gap)
{
    int label_w = 0;
    int group_w;

    if (!mono_font || !metric)
        return 0;

    if (metric->label[0])
        label_w = sdl_menu_measure_text(story_font ? story_font : mono_font,
            metric->label);
    group_w = sdl_menu_character_metric_group_width(mono_font, metric,
        token_gap);
    if (label_w > 0 && group_w > 0)
        return label_w + label_gap + group_w;

    return label_w + group_w;
}

static int sdl_menu_character_stat_group_width(TTF_Font* mono_font,
    const app_ui_character_stat* stat, int token_gap)
{
    int width = 0;
    bool first = true;
    const char* tokens[5];
    int i;

    if (!mono_font || !stat)
        return 0;

    tokens[0] = stat->value;
    tokens[1] = stat->base[0] ? (char[]){ stat->separator ? stat->separator : '=',
        '\0' } : "";
    tokens[2] = stat->base;
    tokens[3] = stat->mod1;
    tokens[4] = stat->mod2;

    for (i = 0; i < 5; i++)
    {
        if (!tokens[i][0])
            continue;
        if (!first)
            width += token_gap;
        width += sdl_menu_measure_text(mono_font, tokens[i]);
        first = false;
    }

    if (stat->mod3[0])
    {
        if (!first)
            width += token_gap;
        width += sdl_menu_measure_text(mono_font, stat->mod3);
    }

    return width;
}

static int sdl_menu_character_stat_row_width(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_character_stat* stat, int label_gap,
    int token_gap)
{
    int label_w = 0;
    int group_w;

    if (!mono_font || !stat)
        return 0;

    if (stat->label[0])
        label_w = sdl_menu_measure_text(story_font ? story_font : mono_font,
            stat->label);
    group_w = sdl_menu_character_stat_group_width(mono_font, stat, token_gap);
    if (label_w > 0 && group_w > 0)
        return label_w + label_gap + group_w;

    return label_w + group_w;
}

static void sdl_menu_render_story_or_mono(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_panel* panel, float x_px, float y_px,
    int line_h, byte attr, byte story, cptr text)
{
    TTF_Font* font;

    if (!text || !text[0])
        return;

    font = ((story & STORY_FLAG_USE) != 0 && story_font) ? story_font : mono_font;
    sdl_menu_render_text(font ? font : mono_font, x_px, y_px, line_h,
        sdl_menu_panel_color(panel, attr), text);
}

static void sdl_menu_render_character_metric_row(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_panel* panel,
    const app_ui_character_metric* metric, int x_px, int y_px, int width_px,
    int label_gap, int token_gap, int line_h)
{
    int label_w = 0;
    int group_w;
    int cursor_x;
    char sep_buf[2] = { '\0', '\0' };

    if (!mono_font || !metric)
        return;
    if (!metric->label[0] && !metric->value[0] && !metric->secondary[0])
        return;

    if (!metric->label[0])
    {
        sdl_menu_render_text(mono_font, (float)x_px, (float)y_px, line_h,
            sdl_menu_panel_color(panel, metric->value_attr), metric->value);
        return;
    }

    label_w = sdl_menu_measure_text(story_font ? story_font : mono_font,
        metric->label);
    sdl_menu_render_text(story_font ? story_font : mono_font, (float)x_px,
        (float)y_px, line_h, sdl_menu_panel_color(panel, metric->label_attr),
        metric->label);

    group_w = sdl_menu_character_metric_group_width(mono_font, metric,
        token_gap);
    cursor_x = x_px + width_px - group_w;
    if (group_w > 0 && cursor_x < x_px + label_w + label_gap)
        cursor_x = x_px + label_w + label_gap;

    if (metric->value[0])
    {
        sdl_menu_render_text(mono_font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_panel_color(panel, metric->value_attr), metric->value);
        cursor_x += sdl_menu_measure_text(mono_font, metric->value);
    }

    if (metric->separator && metric->secondary[0])
    {
        sep_buf[0] = metric->separator;
        cursor_x += token_gap;
        sdl_menu_render_text(mono_font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_panel_color(panel, TERM_WHITE), sep_buf);
        cursor_x += sdl_menu_measure_text(mono_font, sep_buf) + token_gap;
        sdl_menu_render_text(mono_font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_panel_color(panel, metric->secondary_attr),
            metric->secondary);
    }
}

static void sdl_menu_render_character_stat_row(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_panel* panel,
    const app_ui_character_stat* stat, int x_px, int y_px, int width_px,
    int label_gap, int token_gap, int line_h)
{
    int label_w = 0;
    int group_w;
    int cursor_x;
    char sep_buf[2] = { '\0', '\0' };
    const char* tokens[5];
    byte attrs[5];
    int i;

    if (!mono_font || !stat)
        return;
    if (!stat->label[0] && !stat->value[0] && !stat->base[0] && !stat->mod1[0]
        && !stat->mod2[0] && !stat->mod3[0])
    {
        return;
    }

    if (stat->label[0])
    {
        label_w = sdl_menu_measure_text(story_font ? story_font : mono_font,
            stat->label);
        sdl_menu_render_text(story_font ? story_font : mono_font, (float)x_px,
            (float)y_px, line_h, sdl_menu_panel_color(panel, stat->label_attr),
            stat->label);
    }

    group_w = sdl_menu_character_stat_group_width(mono_font, stat, token_gap);
    cursor_x = x_px + width_px - group_w;
    if (label_w > 0 && group_w > 0 && cursor_x < x_px + label_w + label_gap)
        cursor_x = x_px + label_w + label_gap;

    sep_buf[0] = stat->separator ? stat->separator : '=';
    tokens[0] = stat->value;
    tokens[1] = stat->base[0] ? sep_buf : "";
    tokens[2] = stat->base;
    tokens[3] = stat->mod1;
    tokens[4] = stat->mod2;
    attrs[0] = stat->value_attr;
    attrs[1] = stat->separator_attr;
    attrs[2] = stat->base_attr;
    attrs[3] = stat->mod1_attr;
    attrs[4] = stat->mod2_attr;

    for (i = 0; i < 5; i++)
    {
        if (!tokens[i][0])
            continue;
        sdl_menu_render_text(mono_font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_panel_color(panel, attrs[i]), tokens[i]);
        cursor_x += sdl_menu_measure_text(mono_font, tokens[i]) + token_gap;
    }

    if (stat->mod3[0])
    {
        sdl_menu_render_text(mono_font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_panel_color(panel, stat->mod3_attr), stat->mod3);
    }
}

static bool sdl_menu_panel_has_minimap(const app_ui_panel* panel)
{
    return panel && panel->minimap_cell_count > 0 && panel->minimap_width > 0
        && panel->minimap_height > 0;
}

static void sdl_menu_render_minimap_widget(const sdl_view* main_view,
    const app_ui_scene* scene, const app_ui_panel* panel, const SDL_FRect* rect)
{
    const app_ui_minimap_cell* cells;
    SDL_FRect map_rect;
    float base_cell_w;
    float base_cell_h;
    float scale;
    float cell_w;
    float cell_h;
    float map_w;
    float map_h;
    int y;
    bool bigtile_map;

    if (!main_view || !scene || !panel || !rect || rect->w <= 0.0f
        || rect->h <= 0.0f)
    {
        return;
    }
    if (!sdl_menu_panel_has_minimap(panel))
        return;
    if ((size_t)panel->minimap_cell_first + (size_t)panel->minimap_cell_count
        > (size_t)scene->minimap_cell_count)
    {
        return;
    }

    bigtile_map = use_bigtile && !graphics_are_ascii();
    base_cell_w = (float)main_view->cell_w * (bigtile_map ? 2.0f : 1.0f);
    base_cell_h = (float)main_view->cell_h;
    if (base_cell_w <= 0.0f || base_cell_h <= 0.0f)
        return;

    scale = MIN(rect->w / ((float)panel->minimap_width * base_cell_w),
        rect->h / ((float)panel->minimap_height * base_cell_h));
    scale = MIN(scale, 1.0f);
    if (scale <= 0.0f)
        return;

    cell_w = base_cell_w * scale;
    cell_h = base_cell_h * scale;
    map_w = cell_w * (float)panel->minimap_width;
    map_h = cell_h * (float)panel->minimap_height;
    map_rect.x = rect->x + (rect->w - map_w) * 0.5f;
    map_rect.y = rect->y + (rect->h - map_h) * 0.5f;
    map_rect.w = map_w;
    map_rect.h = map_h;

    sdl_menu_fill_rect(&map_rect, sdl_menu_panel_style(panel)->canvas_fill);
    if (panel->minimap_border_attr)
        sdl_menu_draw_rect(&map_rect,
            sdl_menu_panel_color(panel, panel->minimap_border_attr));

    cells = scene->minimap_cells + panel->minimap_cell_first;
    for (y = 0; y < panel->minimap_height; y++)
    {
        int x;

        for (x = 0; x < panel->minimap_width; x++)
        {
            const app_ui_minimap_cell* cell = cells
                + (size_t)y * (size_t)panel->minimap_width + (size_t)x;
            SDL_FRect dst = {
                map_rect.x + (float)x * cell_w,
                map_rect.y + (float)y * cell_h,
                cell_w,
                cell_h
            };

            sdl_menu_fill_rect(&dst, sdl_menu_panel_style(panel)->canvas_fill);

            if (sdl_menu_document_cell_is_raw(cell->attr, cell->ch,
                    cell->terrain_attr, cell->terrain_char))
            {
                if (g_state.use_tiles && g_state.tileset)
                {
                    if ((cell->terrain_attr & TILE_FLAG)
                        && (((byte)cell->terrain_char) & TILE_FLAG))
                    {
                        sdl_menu_draw_tile(cell->terrain_attr,
                            (byte)cell->terrain_char, &dst);
                    }
                    if (cell->attr & GRAPHICS_GLOW_MASK)
                        sdl_menu_draw_misc_icon(&dst, ICON_GLOW);
                    sdl_menu_draw_tile(cell->attr, (byte)cell->ch, &dst);
                    if (cell->terrain_attr & GRAPHICS_SLEEP_MASK)
                        sdl_menu_draw_misc_icon(&dst, ICON_SLEEPING);
                    if (((byte)cell->terrain_char) & GRAPHICS_SEEN_MASK)
                        sdl_menu_draw_misc_icon(&dst,
                            ICON_MONSTER_SEES_PLAYER);
                    if (((byte)cell->ch) & GRAPHICS_ALERT_MASK)
                        sdl_menu_draw_misc_icon(&dst, ICON_ALERT);
                }
                continue;
            }

            if (cell->ch && cell->ch != ' ')
            {
                SDL_FRect glyph_dst = dst;

                if (bigtile_map)
                    glyph_dst.w = cell_w * 0.5f;
                sdl_menu_draw_view_glyph(main_view, &glyph_dst,
                    sdl_menu_color(cell->attr), cell->ch);
            }
        }
    }
}

bool sdl_menu_render_minimap_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* panel)
{
    TTF_Font* font;
    SDL_FRect minimap_rect;
    int pixel_height;
    int line_h;
    int line_gap;
    int section_gap;
    int margin_x;
    int margin_y;
    int content_y;
    int prompt_h = 0;
    int prompt_y;
    u16b i;

    if (!main_view || !scene || !panel || !sdl_menu_panel_has_minimap(panel))
        return false;

    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_font_px((float)sdl_menu_font_size_logical(panel));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;

    line_h = MAX(pixel_height, TTF_GetFontHeight(font));
    line_gap = MAX(1, sdl_menu_scale_px(2.0f));
    section_gap = MAX(line_h / 2, sdl_menu_scale_px(12.0f));
    margin_x = MAX(line_h, sdl_menu_scale_px(24.0f));
    margin_y = MAX(line_h / 2, sdl_menu_scale_px(16.0f));
    content_y = margin_y;

    sdl_ui_style_draw_canvas(sdl_menu_panel_style(panel), canvas_w, canvas_h);

    if (panel->title[0])
    {
        int title_w = sdl_menu_measure_text(font, panel->title);
        int title_x = (canvas_w - title_w) / 2;

        if (title_x < margin_x)
            title_x = margin_x;
        sdl_menu_render_text(font, (float)title_x, (float)content_y, line_h,
            sdl_menu_panel_color(panel, panel->title_attr), panel->title);
        content_y += line_h + section_gap;
    }

    if (panel->body_line_count > 0)
    {
        prompt_h = panel->body_line_count * line_h
            + (panel->body_line_count - 1) * line_gap;
    }
    prompt_y = canvas_h - margin_y - prompt_h;

    minimap_rect.x = (float)margin_x;
    minimap_rect.y = (float)content_y;
    minimap_rect.w = (float)(canvas_w - margin_x * 2);
    minimap_rect.h = (float)(prompt_y - content_y
        - ((prompt_h > 0) ? section_gap : 0));
    if (minimap_rect.h <= 0.0f)
        return false;

    sdl_menu_render_minimap_widget(main_view, scene, panel, &minimap_rect);

    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];
        int text_w = sdl_menu_measure_text(font, line->text);
        int text_x = (canvas_w - text_w) / 2;

        if (text_x < margin_x)
            text_x = margin_x;
        sdl_menu_render_text(font, (float)text_x, (float)prompt_y, line_h,
            sdl_menu_panel_color(panel, line->attr), line->text);
        prompt_y += line_h + line_gap;
    }

    return true;
}

bool sdl_menu_render_character_sheet_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* panel)
{
    TTF_Font* mono_font = NULL;
    TTF_Font* story_font = NULL;
    SDL_Rect history_clip;
    SDL_FRect minimap_rect;
    bool has_minimap;
    bool fallback_fit = false;
    float minimap_aspect = 1.0f;
    int desired_px;
    int min_px;
    int pixel_height;
    int chosen_pixel_height = 0;
    int line_h = 0;
    int line_gap = 0;
    int label_gap = 0;
    int token_gap = 0;
    int layout_x = 0;
    int metrics_x = 0;
    int traits_x = 0;
    int stats_x = 0;
    int metrics_w = 0;
    int stats_w = 0;
    int title_y = 0;
    int columns_y = 0;
    int history_y = 0;
    int history_x = 0;
    int history_w = 0;
    int history_box_h = 0;
    int top_rows = 0;
    int top_h;
    int i;

    if (!main_view || !scene || !panel)
        return false;

    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    has_minimap = sdl_menu_panel_has_minimap(panel);
    if (has_minimap && panel->minimap_height > 0)
    {
        minimap_aspect = (float)panel->minimap_width
            / (float)panel->minimap_height;
    }

    desired_px = sdl_menu_font_px((float)sdl_menu_font_size_logical(panel));
    min_px = sdl_menu_scale_px(8.0f);
    if (min_px < 8)
        min_px = 8;
    if (desired_px < min_px)
        desired_px = min_px;

    minimap_rect.x = 0.0f;
    minimap_rect.y = 0.0f;
    minimap_rect.w = 0.0f;
    minimap_rect.h = 0.0f;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        int candidate_line_h;
        int candidate_line_gap;
        int candidate_section_gap;
        int candidate_label_gap;
        int candidate_token_gap;
        int candidate_column_gap;
        int candidate_margin_x;
        int candidate_margin_y;
        int candidate_bottom_reserve;
        int max_available_w;
        int available_h;
        int bottom_available_h;
        int max_metric_w = 0;
        int max_trait_w = 0;
        int max_stat_w = 0;
        int title_w = 0;
        int candidate_layout_w;
        int candidate_layout_x;
        int candidate_metrics_w;
        int candidate_traits_w;
        int candidate_stats_w;
        int candidate_metrics_x;
        int candidate_traits_x;
        int candidate_stats_x;
        int candidate_title_y;
        int candidate_columns_y;
        int candidate_history_y;
        int candidate_history_x;
        int candidate_history_w;
        int candidate_history_box_h;
        int candidate_history_h = 0;
        SDL_FRect candidate_minimap_rect = { 0 };

        mono_font = sdl_ui_font_for_height(pixel_height);
        story_font = sdl_story_font_for_height(pixel_height);
        if (!mono_font)
            continue;
        if (!story_font)
            story_font = mono_font;

        candidate_line_h = MAX(pixel_height, MAX(TTF_GetFontHeight(mono_font),
            TTF_GetFontHeight(story_font)));
        candidate_line_gap = MAX(1, candidate_line_h / 6);
        candidate_section_gap = MAX(candidate_line_h / 2, candidate_line_gap * 3);
        candidate_label_gap = MAX(candidate_line_h / 2, candidate_line_gap * 3);
        candidate_token_gap = MAX(4, candidate_line_h / 4);
        candidate_column_gap = MAX(candidate_section_gap, candidate_label_gap);
        candidate_margin_x = MAX(candidate_line_h, sdl_menu_scale_px(24.0f));
        candidate_margin_y = MAX(candidate_line_gap * 2,
            sdl_menu_scale_px(12.0f));
        candidate_bottom_reserve = candidate_line_h + candidate_section_gap;
        max_available_w = canvas_w - candidate_margin_x * 2;
        if (max_available_w <= 0)
            continue;

        if (panel->title[0])
            title_w = sdl_menu_measure_text(story_font, panel->title);

        for (i = 0; i < panel->character_metric_count; i++)
        {
            max_metric_w = MAX(max_metric_w,
                sdl_menu_character_metric_row_width(mono_font, story_font,
                    &panel->character_metrics[i], candidate_label_gap,
                    candidate_token_gap));
        }

        for (i = 0; i < panel->detail_line_count; i++)
        {
            TTF_Font* font = ((panel->detail_lines[i].story & STORY_FLAG_USE) != 0
                && story_font) ? story_font : mono_font;

            max_trait_w = MAX(max_trait_w,
                sdl_menu_measure_text(font, panel->detail_lines[i].text));
        }

        for (i = 0; i < panel->character_stat_count; i++)
        {
            max_stat_w = MAX(max_stat_w,
                sdl_menu_character_stat_row_width(mono_font, story_font,
                    &panel->character_stats[i], candidate_label_gap,
                    candidate_token_gap));
        }

        candidate_metrics_w = max_metric_w + candidate_token_gap * 2;
        candidate_traits_w = max_trait_w + candidate_token_gap * 2;
        candidate_stats_w = max_stat_w + candidate_token_gap * 2;
        candidate_layout_w = candidate_metrics_w + candidate_traits_w
            + candidate_stats_w + candidate_column_gap * 2;
        candidate_layout_w = MAX(candidate_layout_w, title_w);
        if (candidate_layout_w > max_available_w)
            continue;

        candidate_layout_x = (canvas_w - candidate_layout_w) / 2;
        candidate_metrics_x = candidate_layout_x;
        candidate_traits_x = candidate_metrics_x + candidate_metrics_w
            + candidate_column_gap;
        candidate_stats_x = candidate_traits_x + candidate_traits_w
            + candidate_column_gap;

        top_rows = panel->character_metric_count;
        if ((int)panel->detail_line_count > top_rows)
            top_rows = panel->detail_line_count;
        if ((int)panel->character_stat_count > top_rows)
            top_rows = panel->character_stat_count;
        if (top_rows < 1)
            top_rows = 1;

        top_h = top_rows * candidate_line_h + (top_rows - 1) * candidate_line_gap;
        candidate_title_y = candidate_margin_y;
        candidate_columns_y = candidate_title_y + candidate_line_h
            + candidate_section_gap;
        available_h = canvas_h - candidate_margin_y - candidate_bottom_reserve
            - candidate_columns_y;
        if (available_h < top_h)
            continue;

        candidate_history_y = candidate_columns_y + top_h;
        bottom_available_h = available_h - top_h;
        if ((panel->rich_paragraph_count > 0 || has_minimap)
            && bottom_available_h > candidate_section_gap)
        {
            candidate_history_y += candidate_section_gap;
            bottom_available_h -= candidate_section_gap;
        }
        if (bottom_available_h < 0)
            bottom_available_h = 0;

        candidate_history_x = candidate_layout_x;
        candidate_history_w = candidate_layout_w;
        candidate_history_box_h = bottom_available_h;

        if (has_minimap && bottom_available_h > 0)
        {
            int min_map_w = sdl_menu_scale_px(96.0f);
            int preferred_map_w = (int)((float)bottom_available_h
                * minimap_aspect + 0.5f);
            int max_map_w = candidate_layout_w / 3;
            int history_min_w = (panel->rich_paragraph_count > 0)
                ? sdl_menu_scale_px(240.0f)
                : 0;

            if (max_map_w < min_map_w)
                max_map_w = min_map_w;
            if (preferred_map_w > max_map_w)
                preferred_map_w = max_map_w;
            if (preferred_map_w < min_map_w)
                preferred_map_w = min_map_w;

            if (panel->rich_paragraph_count > 0)
            {
                while (preferred_map_w > min_map_w
                    && (candidate_layout_w - preferred_map_w
                        - candidate_column_gap) < history_min_w)
                {
                    preferred_map_w -= candidate_line_h;
                }

                if ((candidate_layout_w - preferred_map_w - candidate_column_gap)
                    >= history_min_w)
                {
                    candidate_history_w = candidate_layout_w - preferred_map_w
                        - candidate_column_gap;
                    candidate_minimap_rect.x = (float)(candidate_layout_x
                        + candidate_history_w + candidate_column_gap);
                }
                else
                {
                    preferred_map_w = 0;
                    candidate_history_w = candidate_layout_w;
                }
            }
            else
            {
                if (preferred_map_w > candidate_layout_w)
                    preferred_map_w = candidate_layout_w;
                candidate_minimap_rect.x = (float)(candidate_layout_x
                    + (candidate_layout_w - preferred_map_w) / 2);
            }

            if (preferred_map_w > 0)
            {
                candidate_minimap_rect.y = (float)candidate_history_y;
                candidate_minimap_rect.w = (float)preferred_map_w;
                candidate_minimap_rect.h = (float)bottom_available_h;
            }
        }

        if (panel->rich_paragraph_count > 0 && candidate_history_w > 0
            && candidate_history_box_h > 0)
        {
            candidate_history_h = sdl_menu_measure_rich_text_height(mono_font,
                story_font, candidate_line_h, candidate_line_gap,
                candidate_line_h, candidate_history_w, scene, panel);
        }

        chosen_pixel_height = pixel_height;
        line_h = candidate_line_h;
        line_gap = candidate_line_gap;
        label_gap = candidate_label_gap;
        token_gap = candidate_token_gap;
        layout_x = candidate_layout_x;
        metrics_x = candidate_metrics_x;
        traits_x = candidate_traits_x;
        stats_x = candidate_stats_x;
        metrics_w = candidate_metrics_w;
        stats_w = candidate_stats_w;
        title_y = candidate_title_y;
        columns_y = candidate_columns_y;
        history_y = candidate_history_y;
        history_x = candidate_history_x;
        history_w = candidate_history_w;
        history_box_h = candidate_history_box_h;
        minimap_rect = candidate_minimap_rect;
        fallback_fit = true;

        if (panel->rich_paragraph_count > 0 && candidate_history_h > 0
            && candidate_history_h > candidate_history_box_h)
        {
            continue;
        }

        break;
    }

    if (!fallback_fit || chosen_pixel_height <= 0)
        return false;

    mono_font = sdl_ui_font_for_height(chosen_pixel_height);
    story_font = sdl_story_font_for_height(chosen_pixel_height);
    if (!mono_font)
        return false;
    if (!story_font)
        story_font = mono_font;

    sdl_ui_style_draw_canvas(sdl_menu_panel_style(panel), canvas_w, canvas_h);

    {
        const sdl_ui_style* style = sdl_menu_panel_style(panel);
        SDL_Color fill = style->panel_fill_alt;
        SDL_Color border = style->panel_border_soft;
        int layout_w = (stats_x + stats_w) - layout_x;
        int column_h = top_rows * line_h + MAX(0, top_rows - 1) * line_gap;

        fill.a = MIN(fill.a, 58);
        border.a = MIN(border.a, 112);
        if (layout_w > 0 && column_h > 0)
        {
            SDL_FRect top_rect = {
                (float)layout_x,
                (float)(columns_y - line_gap),
                (float)layout_w,
                (float)(column_h + line_gap * 2)
            };

            sdl_menu_fill_rect(&top_rect, fill);
            sdl_menu_draw_rect(&top_rect, border);
        }
        if (history_w > 0 && history_box_h > 0)
        {
            SDL_FRect history_rect = {
                (float)history_x,
                (float)(history_y - line_gap),
                (float)history_w,
                (float)(history_box_h + line_gap)
            };

            sdl_menu_fill_rect(&history_rect, fill);
            sdl_menu_draw_rect(&history_rect, border);
        }
        if (minimap_rect.w > 0.0f && minimap_rect.h > 0.0f)
        {
            SDL_FRect map_frame = minimap_rect;

            map_frame.x -= (float)line_gap;
            map_frame.y -= (float)line_gap;
            map_frame.w += (float)line_gap * 2.0f;
            map_frame.h += (float)line_gap * 2.0f;
            sdl_menu_fill_rect(&map_frame, fill);
            sdl_menu_draw_rect(&map_frame, border);
        }
    }

    if (panel->title[0])
    {
        int title_w = sdl_menu_measure_text(story_font, panel->title);
        int title_x = (canvas_w - title_w) / 2;

        if (title_x < layout_x)
            title_x = layout_x;
        sdl_menu_render_text(story_font, (float)title_x, (float)title_y,
            line_h, sdl_menu_panel_color(panel, panel->title_attr),
            panel->title);
    }

    for (i = 0; i < panel->character_metric_count; i++)
    {
        const app_ui_character_metric* metric = &panel->character_metrics[i];
        int y_px = columns_y + i * (line_h + line_gap);

        sdl_menu_render_character_metric_row(mono_font, story_font, panel,
            metric, metrics_x, y_px, metrics_w, label_gap, token_gap, line_h);
    }

    for (i = 0; i < panel->detail_line_count; i++)
    {
        int y_px = columns_y + i * (line_h + line_gap);

        sdl_menu_render_story_or_mono(mono_font, story_font, panel,
            (float)traits_x, (float)y_px, line_h, panel->detail_lines[i].attr,
            panel->detail_lines[i].story, panel->detail_lines[i].text);
    }

    for (i = 0; i < panel->character_stat_count; i++)
    {
        const app_ui_character_stat* stat = &panel->character_stats[i];
        int y_px = columns_y + i * (line_h + line_gap);

        sdl_menu_render_character_stat_row(mono_font, story_font, panel, stat,
            stats_x, y_px, stats_w, label_gap, token_gap, line_h);
    }

    if (panel->rich_paragraph_count > 0 && history_w > 0 && history_box_h > 0)
    {
        history_clip.x = history_x;
        history_clip.y = history_y;
        history_clip.w = history_w;
        history_clip.h = history_box_h;
        SDL_SetRenderClipRect(g_state.renderer, &history_clip);
        (void)sdl_menu_render_rich_text(scene, panel, mono_font, story_font,
            &history_clip, line_h, line_gap, line_h, history_y);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (minimap_rect.w > 0.0f && minimap_rect.h > 0.0f)
        sdl_menu_render_minimap_widget(main_view, scene, panel, &minimap_rect);
    return true;
}
