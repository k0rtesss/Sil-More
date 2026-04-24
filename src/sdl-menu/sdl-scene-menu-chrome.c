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

static const char* sdl_menu_status_rail_label_text(const app_ui_row* row)
{
    if (!row)
        return "";

    if (row->key[0])
        return row->key;

    return row->label;
}

static int sdl_menu_status_rail_gap_px(TTF_Font* mono_font)
{
    int gap_px = sdl_menu_measure_text(mono_font, " ");

    if (gap_px < 4)
        gap_px = 4;

    return gap_px;
}

static int sdl_menu_status_rail_icon_slot_px(TTF_Font* mono_font, int line_h)
{
    return sdl_menu_icon_slot_px(mono_font, line_h);
}

static int sdl_menu_status_rail_label_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_row* row, cptr text)
{
    if (!text || !text[0])
        return 0;
    if ((row->flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
        return sdl_menu_measure_text(story_font, text);

    return sdl_menu_measure_text(mono_font, text);
}

static int sdl_menu_status_rail_row_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, const app_ui_row* row)
{
    const char* label_text = sdl_menu_status_rail_label_text(row);
    int icon_slot_w;
    int gap_px;
    int label_w;
    int meta_w;
    int width;

    if (!mono_font || !row)
        return 0;

    icon_slot_w = sdl_menu_status_rail_icon_slot_px(mono_font, line_h);
    gap_px = sdl_menu_status_rail_gap_px(mono_font);
    label_w = sdl_menu_status_rail_label_width_px(mono_font, story_font, row,
        label_text);
    meta_w = sdl_menu_measure_text(mono_font, row->meta);

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

    width = label_w;
    if (row->icon_char)
    {
        width += icon_slot_w;
        if (label_w > 0)
            width += gap_px;
    }
    if (row->meta[0])
    {
        if (width > 0)
            width += gap_px;
        width += meta_w;
    }

    return width;
}

static void sdl_menu_render_status_rail_icon(TTF_Font* mono_font, float x_px,
    float y_px, int icon_slot_w, int line_h, byte icon_attr, char icon_char)
{
    sdl_menu_render_icon(mono_font, x_px, y_px, icon_slot_w, line_h,
        icon_attr, icon_char);
}

static void sdl_menu_render_status_rail_label(TTF_Font* mono_font,
    TTF_Font* story_font, float x_px, float y_px, int line_h,
    byte attr, byte flags, cptr text)
{
    if (!text || !text[0])
        return;

    if ((flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
    {
        sdl_menu_render_text(story_font, x_px, y_px, line_h,
            sdl_menu_color(attr), text);
        return;
    }

    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
        sdl_menu_color(attr), text);
}

bool sdl_menu_render_status_rail_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel)
{
    TTF_Font* mono_font = NULL;
    TTF_Font* story_font = NULL;
    SDL_FRect clear_rect;
    SDL_Rect clip_rect;
    int desired_px;
    int min_px;
    int pixel_height;
    int line_h = 0;
    int icon_slot_w = 0;
    int gap_px = 0;
    int row_top = 1;
    int panel_w_px = 0;
    int row_visible;
    int screen_rows = 0;
    int left_inset_px = 0;
    u16b i;

    if (!main_view || !panel || panel->row_count == 0)
        return false;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    desired_px = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(panel));
    min_px = sdl_menu_scale_px(10.0f);
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
        u16b row_index;

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
        icon_slot_w = sdl_menu_status_rail_icon_slot_px(mono_font, line_h);
        gap_px = sdl_menu_status_rail_gap_px(mono_font);
        left_inset_px = MAX(sdl_menu_scale_px(4.0f),
            sdl_ui_text_pair_left_padding(mono_font,
                story_font ? story_font : mono_font, line_h));

        for (row_index = 0; row_index < panel->row_count; row_index++)
        {
            candidate_w_px = MAX(candidate_w_px,
                left_inset_px + sdl_menu_status_rail_row_width_px(mono_font,
                    story_font, line_h, &panel->rows[row_index]));
        }
        if (panel->min_width_px > 0)
        {
            int min_w_px = sdl_menu_scale_px((float)panel->min_width_px);

            candidate_w_px = MAX(candidate_w_px, min_w_px);
        }
        max_w_px = panel->width_cap_px > 0
            ? sdl_menu_scale_px((float)panel->width_cap_px)
            : 0;
        if (max_w_px > 0 && candidate_w_px > max_w_px)
            candidate_w_px = max_w_px;

        screen_rows = canvas_h / line_h;
        if (candidate_w_px <= 0 || candidate_w_px > canvas_w
            || screen_rows <= row_top)
        {
            continue;
        }

        panel_w_px = candidate_w_px;
        break;
    }

    if (!mono_font || line_h <= 0 || panel_w_px <= 0 || screen_rows <= row_top)
    {
        return false;
    }

    row_visible = MIN((int)panel->row_count, screen_rows - row_top);
    if (row_visible <= 0)
        return false;

    clear_rect.x = 0.0f;
    clear_rect.y = (float)(row_top * line_h);
    clear_rect.w = (float)panel_w_px;
    clear_rect.h = (float)(row_visible * line_h);
    sdl_menu_fill_rect(&clear_rect, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = (int)clear_rect.x;
    clip_rect.y = (int)clear_rect.y;
    clip_rect.w = (int)clear_rect.w;
    clip_rect.h = (int)clear_rect.h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    for (i = 0; i < (u16b)row_visible; i++)
    {
        const app_ui_row* row = &panel->rows[i];
        float y_px = (float)((row_top + (int)i) * line_h);
        const char* label_text = sdl_menu_status_rail_label_text(row);
        byte label_attr = row->attr ? row->attr : TERM_WHITE;
        byte meta_attr = row->meta_attr ? row->meta_attr : label_attr;
        int label_w = sdl_menu_status_rail_label_width_px(mono_font,
            story_font, row, label_text);
        int meta_w = sdl_menu_measure_text(mono_font, row->meta);
        float content_x = (float)left_inset_px;

        if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, content_x,
                y_px, line_h, label_attr, row->flags,
                row->label[0] ? row->label : row->key);
            continue;
        }

        if (row->extra_icon_char)
        {
            int group_w = label_w + icon_slot_w + meta_w;
            float x_px;

            if (row->icon_char)
            {
                group_w += icon_slot_w;
                if (label_w > 0 && meta_w > 0)
                    group_w += gap_px;
                x_px = (float)panel_w_px - (float)group_w;
                if (x_px < content_x)
                    x_px = content_x;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->icon_attr, row->icon_char);
                x_px += (float)icon_slot_w;
                if (row->label[0])
                {
                    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
                        sdl_menu_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)gap_px;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->extra_icon_attr,
                    row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_menu_render_text(mono_font,
                        x_px + (float)icon_slot_w, y_px, line_h,
                        sdl_menu_color(meta_attr), row->meta);
                }
            }
            else
            {
                if (label_w > 0 && meta_w > 0)
                    group_w += gap_px;
                x_px = (float)panel_w_px - (float)group_w;
                if (x_px < content_x)
                    x_px = content_x;
                if (row->label[0])
                {
                    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
                        sdl_menu_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)gap_px;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->extra_icon_attr,
                    row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_menu_render_text(mono_font,
                        x_px + (float)icon_slot_w, y_px, line_h,
                        sdl_menu_color(meta_attr), row->meta);
                }
            }
            continue;
        }

        if (row->icon_char)
        {
            sdl_menu_render_status_rail_icon(mono_font, content_x, y_px,
                icon_slot_w, line_h, row->icon_attr, row->icon_char);
            if (row->label[0])
            {
                sdl_menu_render_status_rail_label(mono_font, story_font,
                    content_x + (float)(icon_slot_w + gap_px), y_px, line_h,
                    label_attr, row->flags, row->label);
            }
            if (row->meta[0])
            {
                float meta_x = (float)panel_w_px - (float)meta_w;
                float min_meta_x = (row->label[0]
                    ? content_x + (float)(icon_slot_w + gap_px + label_w
                        + gap_px)
                    : content_x + (float)icon_slot_w);

                if (meta_x < min_meta_x)
                    meta_x = min_meta_x;
                sdl_menu_render_text(mono_font, meta_x, y_px, line_h,
                    sdl_menu_color(meta_attr), row->meta);
            }
            continue;
        }

        if (label_text[0])
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, content_x,
                y_px, line_h, label_attr, row->flags, label_text);
        }
        if (row->meta[0])
        {
            float meta_x = (float)panel_w_px - (float)meta_w;

            if (label_text[0] && meta_x < content_x + (float)(label_w + gap_px))
                meta_x = content_x + (float)(label_w + gap_px);
            if (meta_x < content_x)
                meta_x = content_x;
            sdl_menu_render_text(mono_font, meta_x, y_px, line_h,
                sdl_menu_color(meta_attr), row->meta);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

bool sdl_menu_render_overlay_rail_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel)
{
    TTF_Font* mono_font = NULL;
    TTF_Font* story_font = NULL;
    SDL_Rect clip_rect;
    int desired_px;
    int min_px;
    int pixel_height;
    int line_h = 0;
    int icon_slot_w = 0;
    int gap_px = 0;
    int row_top = 1;
    int panel_w_px = 0;
    int row_visible;
    int screen_rows = 0;
    int left_inset_px = 0;
    u16b i;

    if (!main_view || !panel || panel->row_count == 0)
        return false;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    desired_px = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(panel));
    min_px = sdl_menu_scale_px(10.0f);
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
        u16b row_index;

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
        icon_slot_w = sdl_menu_status_rail_icon_slot_px(mono_font, line_h);
        gap_px = sdl_menu_status_rail_gap_px(mono_font);
        left_inset_px = MAX(sdl_menu_scale_px(4.0f),
            sdl_ui_text_pair_left_padding(mono_font,
                story_font ? story_font : mono_font, line_h));

        for (row_index = 0; row_index < panel->row_count; row_index++)
        {
            candidate_w_px = MAX(candidate_w_px,
                left_inset_px + sdl_menu_status_rail_row_width_px(mono_font,
                    story_font, line_h, &panel->rows[row_index]));
        }
        if (panel->min_width_px > 0)
        {
            int min_w_px = sdl_menu_scale_px((float)panel->min_width_px);

            candidate_w_px = MAX(candidate_w_px, min_w_px);
        }
        max_w_px = panel->width_cap_px > 0
            ? sdl_menu_scale_px((float)panel->width_cap_px)
            : 0;
        if (max_w_px > 0 && candidate_w_px > max_w_px)
            candidate_w_px = max_w_px;

        screen_rows = canvas_h / line_h;
        if (candidate_w_px <= 0 || candidate_w_px > canvas_w
            || screen_rows <= row_top)
        {
            continue;
        }

        panel_w_px = candidate_w_px;
        break;
    }

    if (!mono_font || line_h <= 0 || panel_w_px <= 0 || screen_rows <= row_top)
    {
        return false;
    }

    row_visible = MIN((int)panel->row_count, screen_rows - row_top);
    if (row_visible <= 0)
        return false;

    clip_rect.x = 0;
    clip_rect.y = row_top * line_h;
    clip_rect.w = panel_w_px;
    clip_rect.h = row_visible * line_h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    for (i = 0; i < (u16b)row_visible; i++)
    {
        const app_ui_row* row = &panel->rows[i];
        const char* label_text = sdl_menu_status_rail_label_text(row);
        byte label_attr = row->attr ? row->attr : TERM_WHITE;
        byte meta_attr = row->meta_attr ? row->meta_attr : label_attr;
        int label_w = sdl_menu_status_rail_label_width_px(mono_font,
            story_font, row, label_text);
        int meta_w = sdl_menu_measure_text(mono_font, row->meta);
        int row_w = sdl_menu_status_rail_row_width_px(mono_font, story_font,
            line_h, row) + left_inset_px;
        float x_px = (float)left_inset_px;
        float y_px = (float)((row_top + (int)i) * line_h);
        bool has_tail = false;

        if (row_w > 0)
        {
            sdl_menu_fill_rect(&(SDL_FRect){ 0.0f, y_px, (float)row_w,
                (float)line_h }, (SDL_Color){ 0, 0, 0, 176 });
        }

        if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, x_px,
                y_px, line_h, label_attr, row->flags,
                row->label[0] ? row->label : row->key);
            continue;
        }

        if (row->icon_char)
        {
            sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                icon_slot_w, line_h, row->icon_attr, row->icon_char);
            x_px += (float)icon_slot_w;
            if (label_text[0] || row->meta[0] || row->extra_icon_char)
                x_px += (float)gap_px;
        }

        if (label_text[0])
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, x_px,
                y_px, line_h, label_attr, row->flags, label_text);
            x_px += (float)label_w;
            has_tail = true;
        }

        if (row->extra_icon_char)
        {
            if (has_tail)
                x_px += (float)gap_px;
            sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                icon_slot_w, line_h, row->extra_icon_attr,
                row->extra_icon_char);
            x_px += (float)icon_slot_w;
            has_tail = true;
        }

        if (row->meta[0])
        {
            if (has_tail)
                x_px += (float)gap_px;
            sdl_menu_render_text(mono_font, x_px, y_px, line_h,
                sdl_menu_color(meta_attr), row->meta);
            x_px += (float)meta_w;
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

bool sdl_menu_render_strip_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel)
{
    TTF_Font* font;
    SDL_Rect clip_rect;
    int pixel_height;
    int line_h;
    int rows;
    int strip_h;
    int current_y;
    int left_inset_px;
    u16b i;
    float y_px;

    if (!main_view || !panel)
        return false;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(panel));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;

    line_h = MAX(pixel_height, TTF_GetFontHeight(font));
    rows = panel->body_line_count ? (int)panel->body_line_count : 1;
    left_inset_px = MAX(sdl_menu_scale_px(4.0f),
        sdl_ui_text_left_padding(font, line_h));
    strip_h = rows * line_h;
    if (strip_h < main_view->cell_h)
        strip_h = main_view->cell_h;
    if (strip_h > canvas_h)
        strip_h = canvas_h;

    y_px = (panel->flags & APP_UI_PANEL_FLAG_BOTTOM_ANCHORED)
        ? (float)(canvas_h - strip_h)
        : 0.0f;

    sdl_menu_fill_rect(&(SDL_FRect){
        .x = 0.0f,
        .y = y_px,
        .w = (float)canvas_w,
        .h = (float)strip_h
    }, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = 0;
    clip_rect.y = (int)y_px;
    clip_rect.w = canvas_w;
    clip_rect.h = strip_h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    current_y = (int)y_px + ((strip_h - rows * line_h) / 2);
    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];

        if (line->text[0] && line->text[0] != ' ')
        {
            sdl_menu_render_text(font, (float)left_inset_px,
                (float)current_y, line_h,
                sdl_menu_color(line->attr), line->text);
        }
        current_y += line_h;
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

static bool sdl_menu_welcome_line_blank(const app_ui_text_line* line)
{
    return line && ((line->flags & APP_UI_TEXT_FLAG_WELCOME_BLANK) != 0);
}

static int sdl_menu_welcome_line_col(const app_ui_text_line* line)
{
    return line ? (int)(line->flags & APP_UI_TEXT_FLAG_WELCOME_COL_MASK) : 0;
}

static int sdl_menu_welcome_cell_width(int line_h)
{
    int cell_w;

    if (line_h <= 0)
        return 1;

    cell_w = (int)((float)line_h * 0.57f + 0.5f);
    if (cell_w < 1)
        cell_w = 1;
    return cell_w;
}

static int sdl_menu_welcome_footer_rows(const app_ui_panel* panel)
{
    if (!panel)
        return 0;
    if (panel->footer_action_count > 0)
        return 3 + panel->detail_line_count;
    if (panel->detail_line_count > 0)
        return 1;

    return 0;
}

static bool sdl_menu_welcome_choose_layout(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel, TTF_Font** out_story,
    TTF_Font** out_mono, int* out_line_h, int* out_cell_w, int* out_base_x)
{
    static const int logical_sizes[] = { 28, 26, 24, 22, 20, 18, 16, 14 };
    const int logical_cols = 80;
    int margin_x;
    size_t i;
    TTF_Font* fallback_story = NULL;
    TTF_Font* fallback_mono = NULL;
    int fallback_line_h = 0;
    int fallback_cell_w = 0;
    int fallback_base_x = 0;

    if (!main_view || !panel || !out_story || !out_mono || !out_line_h
        || !out_cell_w || !out_base_x)
    {
        return false;
    }

    margin_x = sdl_menu_scale_px(12.0f);

    for (i = 0; i < N_ELEMENTS(logical_sizes); i++)
    {
        int pixel_height = sdl_menu_scale_px((float)logical_sizes[i]);
        TTF_Font* story_font = sdl_story_font_for_height(pixel_height);
        TTF_Font* mono_font = sdl_ui_font_for_height(pixel_height);
        int line_h;
        int cell_w;
        int design_w;
        int top_extent;
        int bottom_extent;

        if (!story_font || !mono_font)
            continue;

        line_h = MAX(pixel_height, MAX(TTF_GetFontHeight(story_font),
            TTF_GetFontHeight(mono_font)));
        cell_w = sdl_menu_welcome_cell_width(line_h);
        design_w = logical_cols * cell_w;
        top_extent = (panel->body_line_count + 1) * line_h;
        bottom_extent = sdl_menu_welcome_footer_rows(panel) * line_h;

        fallback_story = story_font;
        fallback_mono = mono_font;
        fallback_line_h = line_h;
        fallback_cell_w = cell_w;
        fallback_base_x = (canvas_w - design_w) / 2;
        if (fallback_base_x < 0)
            fallback_base_x = 0;

        if (design_w + margin_x * 2 > canvas_w)
            continue;
        if (top_extent + bottom_extent > canvas_h)
            continue;

        *out_story = story_font;
        *out_mono = mono_font;
        *out_line_h = line_h;
        *out_cell_w = cell_w;
        *out_base_x = fallback_base_x;
        return true;
    }

    if (!fallback_story || !fallback_mono)
        return false;

    *out_story = fallback_story;
    *out_mono = fallback_mono;
    *out_line_h = fallback_line_h;
    *out_cell_w = fallback_cell_w;
    *out_base_x = fallback_base_x;
    return true;
}

static void sdl_menu_welcome_format_prompt(const app_ui_panel* panel,
    char* buf, size_t buflen)
{
    u16b i;

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';
    if (!panel)
        return;

    for (i = 0; i < panel->footer_action_count; i++)
    {
        const app_ui_footer_action* action = &panel->footer_actions[i];
        char token[APP_UI_LABEL_MAX + APP_UI_KEY_MAX + 8];

        if (!action->label[0])
            continue;

        if (action->key[0])
            strnfmt(token, sizeof(token), "[%s] %s", action->key, action->label);
        else
            SDL_strlcpy(token, action->label, sizeof(token));

        if (buf[0])
            SDL_strlcat(buf, "    ", buflen);
        SDL_strlcat(buf, token, buflen);
    }
}

static bool sdl_menu_welcome_body_bounds(const app_ui_panel* panel,
    TTF_Font* story_font, TTF_Font* mono_font, int cell_w,
    int* out_min_x, int* out_max_x)
{
    bool saw_text = false;
    int min_x = 0;
    int max_x = 0;
    u16b i;

    if (!panel || !story_font || !mono_font || cell_w <= 0
        || !out_min_x || !out_max_x)
    {
        return false;
    }

    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];
        TTF_Font* font = (line->story & STORY_FLAG_USE) ? story_font : mono_font;
        int x_px;
        int text_w;

        if (sdl_menu_welcome_line_blank(line) || !line->text[0])
            continue;

        x_px = sdl_menu_welcome_line_col(line) * cell_w;
        text_w = sdl_menu_measure_text(font, line->text);
        if (text_w <= 0)
            text_w = (int)strlen(line->text) * cell_w;

        if (!saw_text)
        {
            min_x = x_px;
            max_x = x_px + text_w;
            saw_text = true;
        }
        else
        {
            min_x = MIN(min_x, x_px);
            max_x = MAX(max_x, x_px + text_w);
        }
    }

    if (!saw_text)
        return false;

    *out_min_x = min_x;
    *out_max_x = max_x;
    return true;
}

static int sdl_menu_welcome_centered_base_x(int canvas_w,
    const app_ui_panel* panel, TTF_Font* story_font, TTF_Font* mono_font,
    int cell_w, int fallback_base_x)
{
    int min_x;
    int max_x;
    int body_w;
    int base_x;

    if (!sdl_menu_welcome_body_bounds(panel, story_font, mono_font, cell_w,
            &min_x, &max_x))
    {
        return fallback_base_x;
    }

    body_w = max_x - min_x;
    base_x = (canvas_w - body_w) / 2 - min_x;

    return base_x;
}

static int sdl_menu_welcome_centered_body_y(int canvas_h,
    const app_ui_panel* panel, int line_h)
{
    int body_h;
    int body_y;
    int footer_rows;
    int footer_top_y;

    if (!panel || line_h <= 0)
        return 0;

    body_h = (int)panel->body_line_count * line_h;
    body_y = (canvas_h - body_h) / 2;
    footer_rows = sdl_menu_welcome_footer_rows(panel);
    footer_top_y = canvas_h - footer_rows * line_h;

    if (footer_rows > 0 && body_y + body_h > footer_top_y - line_h)
        body_y = footer_top_y - line_h - body_h;
    if (body_y < 0)
        body_y = 0;

    return body_y;
}

bool sdl_menu_render_welcome_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel)
{
    TTF_Font* story_font;
    TTF_Font* mono_font;
    int line_h;
    int cell_w;
    int base_x;
    int body_y;
    int intro_x;
    u16b i;

    if (!main_view || !panel)
        return false;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;
    if (!sdl_menu_welcome_choose_layout(main_view, canvas_w, canvas_h, panel,
            &story_font, &mono_font, &line_h, &cell_w, &base_x))
    {
        return false;
    }

    base_x = sdl_menu_welcome_centered_base_x(canvas_w, panel, story_font,
        mono_font, cell_w, base_x);
    body_y = sdl_menu_welcome_centered_body_y(canvas_h, panel, line_h);

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];
        TTF_Font* font = (line->story & STORY_FLAG_USE) ? story_font : mono_font;
        int x_px;
        int y_px;

        if (sdl_menu_welcome_line_blank(line) || !line->text[0])
            continue;

        x_px = base_x + sdl_menu_welcome_line_col(line) * cell_w;
        y_px = body_y + (int)i * line_h;
        sdl_menu_render_text(font, (float)x_px, (float)y_px, line_h,
            sdl_menu_color(line->attr), line->text);
    }

    intro_x = base_x + 14 * cell_w;
    if (panel->footer_action_count > 0)
    {
        char prompt_buf[APP_UI_TEXT_MAX];

        sdl_menu_welcome_format_prompt(panel, prompt_buf, sizeof(prompt_buf));
        if (prompt_buf[0])
        {
            sdl_menu_render_text(mono_font, (float)intro_x,
                (float)(canvas_h - line_h), line_h, sdl_menu_color(TERM_SLATE),
                prompt_buf);
        }

        sdl_menu_render_text(mono_font, (float)intro_x,
            (float)(canvas_h - line_h * 3), line_h,
            sdl_menu_color(TERM_L_DARK), "- - - - - - - - - - - -");

        for (i = 0; i < panel->detail_line_count; i++)
        {
            const app_ui_text_line* line = &panel->detail_lines[i];
            int y_px = canvas_h - (int)(panel->detail_line_count - i + 3) * line_h;

            if (!line->text[0])
                continue;
            sdl_menu_render_text(mono_font, (float)intro_x, (float)y_px, line_h,
                sdl_menu_color(line->attr), line->text);
        }
    }
    else if (panel->detail_line_count > 0)
    {
        const app_ui_text_line* line = &panel->detail_lines[0];
        int text_w;
        int x_px;

        if (line->text[0])
        {
            text_w = sdl_menu_measure_text(mono_font, line->text);
            x_px = (canvas_w - text_w) / 2;
            if (x_px < 0)
                x_px = 0;
            sdl_menu_render_text(mono_font, (float)x_px,
                (float)(canvas_h - line_h), line_h, sdl_menu_color(line->attr),
                line->text);
        }
    }

    return true;
}
