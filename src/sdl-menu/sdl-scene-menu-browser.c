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

static void sdl_menu_browser_action_text(const app_ui_footer_action* action,
    char* text, size_t text_size)
{
    char prompt[32];
    cptr key_text;

    if (!text || !text_size)
        return;

    text[0] = '\0';
    if (!action || !action->label[0])
        return;

    prompt[0] = '\0';
    if (portable_controls_active())
    {
        platform_input_prompt_for_ui_action(APP_INPUT_DEVICE_GAMEPAD,
            action->interaction.action, action->interaction.action_key,
            prompt, sizeof(prompt));
    }

    key_text = prompt[0] ? prompt : action->key;
    if (key_text && key_text[0])
        strnfmt(text, text_size, "%s %s", key_text, action->label);
    else
        SDL_strlcpy(text, action->label, text_size);
}

static int sdl_menu_browser_action_width(TTF_Font* font,
    const app_ui_footer_action* action)
{
    char text[APP_UI_LABEL_MAX + 40];
    int pad_x;

    if (!font || !action || !action->label[0])
        return 0;

    sdl_menu_browser_action_text(action, text, sizeof(text));
    pad_x = sdl_menu_scale_px(8.0f);
    return sdl_menu_measure_text(font, text) + pad_x * 2;
}

static int sdl_menu_browser_footer_lines(TTF_Font* font,
    const app_ui_panel* panel, int max_w, int item_gap)
{
    int cursor_w = 0;
    int lines = 0;
    u16b i;

    if (!font || !panel || panel->footer_action_count == 0 || max_w <= 0)
        return 0;

    lines = 1;
    for (i = 0; i < panel->footer_action_count; i++)
    {
        int token_w = sdl_menu_browser_action_width(font,
            &panel->footer_actions[i]);

        if (token_w <= 0)
            continue;
        if (cursor_w > 0 && cursor_w + item_gap + token_w > max_w)
        {
            lines++;
            cursor_w = token_w;
        }
        else
        {
            if (cursor_w > 0)
                cursor_w += item_gap;
            cursor_w += token_w;
        }
    }

    return lines;
}

static void sdl_menu_render_browser_tabs(TTF_Font* font,
    const app_ui_panel* panel, int x_px, int y_px, int line_h, int item_gap)
{
    int cursor_x = x_px;
    int pad_x = sdl_menu_scale_px(10.0f);
    int pad_y = sdl_menu_scale_px(3.0f);
    u16b i;

    if (!font || !panel || panel->tab_count == 0)
        return;

    for (i = 0; i < panel->tab_count; i++)
    {
        const app_ui_tab* tab = &panel->tabs[i];
        int text_w = sdl_menu_measure_text(font, tab->label);
        int tab_w = text_w + pad_x * 2;
        SDL_FRect pill = {
            (float)cursor_x,
            (float)y_px,
            (float)tab_w,
            (float)(line_h + pad_y * 2)
        };
        SDL_Color fill = (tab->flags & APP_UI_ITEM_FLAG_ACTIVE)
            ? sdl_menu_panel_style(panel)->selected_fill
            : sdl_menu_panel_style(panel)->panel_fill_alt;
        SDL_Color border = sdl_menu_panel_accent(panel, panel->accent_attr);

        if (i > 0)
            cursor_x += item_gap;
        pill.x = (float)cursor_x;
        fill.a = (tab->flags & APP_UI_ITEM_FLAG_ACTIVE) ? 132 : 70;
        border.a = 212;
        sdl_menu_fill_rect(&pill, fill);
        sdl_menu_draw_rect(&pill, border);
        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_TAB, tab->id,
            tab->interaction.action_key, tab->interaction.role,
            tab->interaction.action, tab->interaction.flags, tab->flags, -1,
            0, 0,
            &(SDL_FRect){ (float)cursor_x, (float)y_px, (float)tab_w,
                (float)MAX(line_h + pad_y * 2, sdl_menu_scale_px(24.0f)) },
            tab->label, tab->interaction.tooltip);
        sdl_menu_render_text(font, (float)(cursor_x + pad_x),
            (float)(y_px + pad_y), line_h,
            sdl_menu_panel_color(panel, tab->attr ? tab->attr : TERM_SLATE),
            tab->label);
        cursor_x += tab_w;
    }
}

static void sdl_menu_render_browser_footer(TTF_Font* font,
    const app_ui_panel* panel, int x_px, int y_px, int max_w, int line_h,
    int line_gap, int item_gap)
{
    int cursor_x = x_px;
    int cursor_y = y_px;
    u16b i;

    if (!font || !panel || panel->footer_action_count == 0 || max_w <= 0)
        return;

    for (i = 0; i < panel->footer_action_count; i++)
    {
        const app_ui_footer_action* action = &panel->footer_actions[i];
        char text[APP_UI_LABEL_MAX + 40];
        int token_w;
        int text_w;
        int pad_x = sdl_menu_scale_px(8.0f);
        int pad_y = sdl_menu_scale_px(2.0f);
        byte attr;
        SDL_Color fill;
        SDL_Color border;

        if (!action->label[0])
            continue;
        sdl_menu_browser_action_text(action, text, sizeof(text));

        text_w = sdl_menu_measure_text(font, text);
        token_w = text_w + pad_x * 2;
        if (cursor_x > x_px && cursor_x + token_w > x_px + max_w)
        {
            cursor_x = x_px;
            cursor_y += line_h + line_gap;
        }

        attr = (action->flags & APP_UI_ITEM_FLAG_DISABLED)
            ? TERM_L_DARK
            : (action->attr ? action->attr : TERM_SLATE);
        fill = (action->flags & APP_UI_ITEM_FLAG_DISABLED)
            ? sdl_menu_panel_style(panel)->disabled_fill
            : sdl_menu_panel_style(panel)->panel_fill_alt;
        border = (action->flags & APP_UI_ITEM_FLAG_DISABLED)
            ? sdl_menu_panel_style(panel)->panel_border_soft
            : sdl_menu_panel_accent(panel, panel->accent_attr);
        fill.a = (action->flags & APP_UI_ITEM_FLAG_DISABLED) ? 96 : 76;
        border.a = (action->flags & APP_UI_ITEM_FLAG_DISABLED) ? 150 : 196;
        sdl_menu_fill_rect(&(SDL_FRect){ (float)cursor_x, (float)cursor_y,
            (float)token_w, (float)(line_h + pad_y * 2) }, fill);
        sdl_menu_draw_rect(&(SDL_FRect){ (float)cursor_x, (float)cursor_y,
            (float)token_w, (float)(line_h + pad_y * 2) }, border);
        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_FOOTER_ACTION,
            action->id, action->interaction.action_key,
            action->interaction.role, action->interaction.action,
            action->interaction.flags, action->flags, -1, 0, 0,
            &(SDL_FRect){ (float)cursor_x, (float)cursor_y,
                (float)token_w, (float)MAX(line_h + pad_y * 2,
                    sdl_menu_scale_px(24.0f)) },
            action->label, action->interaction.tooltip);
        sdl_menu_render_text(font, (float)(cursor_x + pad_x),
            (float)(cursor_y + pad_y), line_h,
            sdl_menu_panel_color(panel, attr), text);
        cursor_x += token_w + item_gap;
    }
}

static void sdl_menu_render_browser_row(TTF_Font* font,
    const app_ui_panel* panel, const app_ui_row* row, s16b row_index,
    const SDL_Rect* clip_rect, int line_h, int item_gap, int current_y)
{
    SDL_Color color;
    SDL_Color meta_color;
    int icon_slot_w = 0;
    int key_w = 0;
    int label_x;
    int meta_w = 0;
    int meta_x;

    if (!font || !panel || !row || !clip_rect)
        return;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
    {
        if (row->label[0])
        {
            sdl_menu_render_text(font, (float)clip_rect->x, (float)current_y,
                line_h, sdl_menu_panel_color(panel,
                    row->attr ? row->attr : TERM_WHITE), row->label);
        }
        return;
    }

    color = sdl_menu_panel_color(panel,
        (row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : ((row->flags & APP_UI_ITEM_FLAG_SELECTED)
            ? panel->accent_attr
            : row->attr));
    meta_color = sdl_menu_panel_color(panel,
        (row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : ((row->flags & APP_UI_ITEM_FLAG_SELECTED)
            ? panel->accent_attr
            : (row->meta_attr ? row->meta_attr : row->attr)));
    label_x = clip_rect->x;
    meta_x = clip_rect->x;

    {
        SDL_FRect hit_rect = {
            (float)clip_rect->x,
            (float)(current_y - sdl_menu_scale_px(2.0f)),
            (float)clip_rect->w,
            (float)MAX(line_h + sdl_menu_scale_px(4.0f),
                sdl_menu_scale_px(24.0f))
        };

        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_ROW, row->id,
            row->interaction.action_key, row->interaction.role,
            row->interaction.action, row->interaction.flags, row->flags, -1,
            row_index, panel->selected_row, &hit_rect, row->label,
            row->interaction.tooltip);
        if (row->flags & APP_UI_ITEM_FLAG_SELECTED)
        {
            SDL_Color fill = sdl_menu_panel_style(panel)->selected_fill;

            sdl_menu_fill_rect(&hit_rect, fill);
        }
    }

    if (row->icon_char)
    {
        icon_slot_w = sdl_menu_icon_slot_px(font, line_h);
        sdl_menu_render_icon(font, (float)label_x, (float)current_y,
            icon_slot_w, line_h, row->icon_attr, row->icon_char);
        label_x += icon_slot_w;
        if (row->key[0] || row->label[0] || row->meta[0])
            label_x += item_gap;
    }

    if (row->key[0])
    {
        key_w = sdl_menu_measure_text(font, row->key);
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, sdl_menu_panel_accent(panel, panel->accent_attr),
            row->key);
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
            line_h, meta_color, row->meta);
}

bool sdl_menu_render_browser_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* ui_panel)
{
    TTF_Font* font;
    TTF_Font* story_font = NULL;
    const sdl_ui_style* style;
    int pixel_height;
    int line_h;
    int line_gap;
    int section_gap;
    int item_gap;
    int margin_x;
    int margin_y;
    int column_gap;
    int detail_w = 0;
    int detail_measured_w = 0;
    int detail_x;
    int rows_x;
    int header_y;
    int divider_y;
    int content_top;
    int footer_lines;
    int footer_h = 0;
    int footer_y;
    int status_h = 0;
    int status_y;
    int content_bottom;
    int available_rows_h;
    int content_w;
    int rich_h = 0;
    int rich_visible_h = 0;
    int rich_scroll_px = 0;
    int rich_max_scroll_px = 0;
    int row_visible = 0;
    int row_start = 0;
    int row_area_gap = 0;
    int subheader_y = 0;
    bool has_detail;
    bool has_header;
    bool rich_scrollable = false;
    bool detail_leading;
    u16b i;

    if (!main_view || !scene || !ui_panel)
        return false;

    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    style = sdl_menu_panel_style(ui_panel);
    pixel_height = sdl_menu_scale_px((float)sdl_menu_font_size_logical(ui_panel));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;
    story_font = sdl_story_font_for_height(pixel_height);
    if (!story_font)
        story_font = font;

    line_h = pixel_height;
    if (line_h <= 0)
        line_h = TTF_GetFontHeight(font);
    line_gap = MAX(1, sdl_menu_scale_px(style->line_gap));
    section_gap = MAX(line_gap, sdl_menu_scale_px(style->section_gap));
    item_gap = MAX(1, sdl_menu_scale_px(style->item_gap));
    margin_x = MAX(0, sdl_menu_scale_px(style->margin_x));
    margin_y = MAX(0, sdl_menu_scale_px(style->margin_y));
    column_gap = MAX(1, sdl_menu_scale_px(style->column_gap));

    sdl_ui_style_draw_canvas(style, canvas_w, canvas_h);

    footer_lines = sdl_menu_browser_footer_lines(font, ui_panel,
        MAX(1, canvas_w - margin_x * 2), item_gap);
    if (footer_lines > 0)
        footer_h = footer_lines * line_h + (footer_lines - 1) * line_gap;
    if (ui_panel->body_line_count > 0)
        status_h = ui_panel->body_line_count * line_h
            + (ui_panel->body_line_count - 1) * line_gap;

    has_detail = ((ui_panel->flags & APP_UI_PANEL_FLAG_SHOW_DETAIL) != 0)
        && (ui_panel->detail_line_count > 0 || ui_panel->detail_title[0]);
    detail_leading = has_detail
        && ((ui_panel->flags & APP_UI_PANEL_FLAG_DETAIL_LEADING) != 0);
    if (has_detail)
    {
        if (ui_panel->detail_title[0])
            detail_measured_w = sdl_menu_measure_text(font,
                ui_panel->detail_title);
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            detail_measured_w = MAX(detail_measured_w, sdl_menu_measure_text(font,
                ui_panel->detail_lines[i].text));
        }
        detail_w = detail_measured_w + sdl_menu_scale_px(10.0f);
        if (detail_w < sdl_menu_scale_px(150.0f))
            detail_w = sdl_menu_scale_px(150.0f);
        if (detail_w > canvas_w / 3)
            detail_w = canvas_w / 3;
    }

    if (has_detail && detail_leading)
    {
        detail_x = margin_x;
        rows_x = margin_x + detail_w + column_gap;
    }
    else
    {
        rows_x = margin_x;
        detail_x = canvas_w - margin_x - detail_w;
    }

    content_w = has_detail
        ? (canvas_w - rows_x - margin_x)
        : (canvas_w - margin_x * 2);
    if (has_detail && !detail_leading)
        content_w = detail_x - column_gap - rows_x;
    if (content_w < 1)
        content_w = 1;

    if (ui_panel->rich_paragraph_count > 0)
    {
        rich_h = sdl_menu_measure_rich_text_height(font, story_font, line_h,
            line_gap, line_h + line_gap, content_w, scene, ui_panel);
    }

    header_y = margin_y;
    has_header = false;
    if (ui_panel->title[0])
    {
        sdl_menu_render_text(font, (float)margin_x, (float)header_y, line_h,
            sdl_menu_panel_color(ui_panel, ui_panel->title_attr),
            ui_panel->title);
        header_y += line_h + line_gap;
        has_header = true;
    }
    if (ui_panel->tab_count > 0)
    {
        sdl_menu_render_browser_tabs(font, ui_panel, margin_x, header_y, line_h,
            item_gap);
        header_y += line_h + section_gap;
        has_header = true;
    }
    subheader_y = header_y;
    if (ui_panel->detail_title[0])
    {
        sdl_menu_render_text(font, (float)detail_x, (float)subheader_y, line_h,
            sdl_menu_panel_color(ui_panel, ui_panel->detail_title_attr),
            ui_panel->detail_title);
        has_header = true;
    }
    if (ui_panel->subtitle[0])
    {
        sdl_menu_render_text(font, (float)rows_x, (float)subheader_y, line_h,
            sdl_menu_panel_color(ui_panel, ui_panel->subtitle_attr),
            ui_panel->subtitle);
        has_header = true;
    }

    if (ui_panel->detail_title[0] || ui_panel->subtitle[0])
        header_y = subheader_y + line_h + line_gap;

    if (has_header)
    {
        divider_y = header_y;
        sdl_ui_style_draw_rule(style, &(SDL_FRect){
            (float)margin_x, (float)divider_y,
            (float)(canvas_w - margin_x * 2), 1.0f
        });
        if (has_detail)
        {
            float rule_x = detail_leading
                ? (float)(rows_x - column_gap / 2)
                : (float)(detail_x - column_gap / 2);

            sdl_ui_style_draw_rule(style, &(SDL_FRect){
                rule_x, (float)(divider_y + line_gap),
                1.0f, (float)(canvas_h - divider_y - margin_y - footer_h
                    - status_h - section_gap * 2)
            });
        }
        content_top = divider_y + line_gap + section_gap;
    }
    else
    {
        content_top = margin_y;
    }

    footer_y = canvas_h - margin_y - footer_h;
    status_y = footer_y;
    if (status_h > 0)
        status_y -= status_h + section_gap;
    content_bottom = (status_h > 0) ? (status_y - line_gap) : (footer_y - line_gap);
    available_rows_h = content_bottom - content_top;
    if (available_rows_h < line_h)
        available_rows_h = line_h;
    rich_visible_h = MAX(0, content_bottom - content_top);
    if (ui_panel->row_count == 0 && rich_h > rich_visible_h
        && (ui_panel->flags & APP_UI_PANEL_FLAG_SCROLL_ROWS))
    {
        rich_scrollable = true;
        rich_max_scroll_px = MAX(0, rich_h - rich_visible_h);
        rich_scroll_px = MAX(0, ui_panel->row_offset) * (line_h + line_gap);
        if (rich_scroll_px > rich_max_scroll_px)
            rich_scroll_px = rich_max_scroll_px;
    }

    if (ui_panel->row_count > 0)
    {
        row_area_gap = (rich_h > 0) ? section_gap : 0;
        available_rows_h -= rich_h + row_area_gap;
        if (available_rows_h < line_h)
            available_rows_h = line_h;

        row_visible = (available_rows_h + line_gap) / (line_h + line_gap);
        if (row_visible < 1)
            row_visible = 1;
        if (row_visible > (int)ui_panel->row_count)
            row_visible = ui_panel->row_count;

        row_start = ui_panel->row_offset;
        if (row_start < 0)
            row_start = 0;
        if (ui_panel->selected_row >= 0
            && ui_panel->selected_row < (s16b)ui_panel->row_count)
        {
            if (ui_panel->selected_row < row_start)
                row_start = ui_panel->selected_row;
            if (ui_panel->selected_row >= row_start + row_visible)
                row_start = ui_panel->selected_row - row_visible + 1;
        }
        if (row_start + row_visible > (int)ui_panel->row_count)
            row_start = ui_panel->row_count - row_visible;
        if (row_start < 0)
            row_start = 0;
    }

    if (has_detail)
    {
        SDL_Rect detail_clip = {
            detail_x,
            content_top,
            detail_w,
            MAX(0, content_bottom - content_top)
        };
        int detail_y = content_top;

        SDL_SetRenderClipRect(g_state.renderer, &detail_clip);
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            sdl_menu_render_text(font, (float)detail_x, (float)detail_y, line_h,
                sdl_menu_panel_color(ui_panel, ui_panel->detail_lines[i].attr),
                ui_panel->detail_lines[i].text);
            detail_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (ui_panel->row_count > 0)
    {
        SDL_Rect row_clip;
        int row_y = content_top;

        if (rich_h > 0)
        {
            SDL_Rect rich_clip = {
                rows_x,
                content_top,
                content_w,
                MAX(0, content_bottom - content_top)
            };

            SDL_SetRenderClipRect(g_state.renderer, &rich_clip);
            row_y += sdl_menu_render_rich_text(scene, ui_panel, font,
                story_font, &rich_clip, line_h, line_gap, line_h + line_gap,
                content_top);
            SDL_SetRenderClipRect(g_state.renderer, NULL);
            row_y += row_area_gap;
        }

        row_clip.x = rows_x;
        row_clip.y = row_y;
        row_clip.w = content_w;
        row_clip.h = MAX(0, content_bottom - row_y);

        if ((ui_panel->flags & APP_UI_PANEL_FLAG_SCROLL_ROWS)
            && row_clip.w > 0 && row_clip.h > 0)
        {
            SDL_FRect scroll_rect = {
                (float)row_clip.x,
                (float)row_clip.y,
                (float)row_clip.w,
                (float)row_clip.h
            };

            (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_PANEL, -1, 0,
                APP_UI_WIDGET_ROLE_SCROLL_REGION,
                APP_UI_WIDGET_ACTION_SCROLL,
                APP_UI_INTERACTION_FLAG_POINTER_ENABLED
                    | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
                APP_UI_ITEM_FLAG_NONE, -1, row_start, row_visible,
                &scroll_rect, ui_panel->title, "");
        }
        SDL_SetRenderClipRect(g_state.renderer, &row_clip);
        for (i = 0; i < (u16b)row_visible; i++)
        {
            const app_ui_row* row = &ui_panel->rows[row_start + i];

            sdl_menu_render_browser_row(font, ui_panel, row,
                (s16b)(row_start + i), &row_clip, line_h,
                sdl_menu_scale_px(10.0f), row_y);
            row_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }
    else if (rich_h > 0)
    {
        SDL_Rect rich_clip = {
            rows_x,
            content_top,
            content_w,
            MAX(0, content_bottom - content_top)
        };
        int rich_start_y = content_top - rich_scroll_px;

        if ((ui_panel->flags & APP_UI_PANEL_FLAG_SCROLL_ROWS)
            && rich_clip.w > 0 && rich_clip.h > 0)
        {
            SDL_FRect scroll_rect = {
                (float)rich_clip.x,
                (float)rich_clip.y,
                (float)rich_clip.w,
                (float)rich_clip.h
            };

            (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_PANEL, -1, 0,
                APP_UI_WIDGET_ROLE_SCROLL_REGION,
                APP_UI_WIDGET_ACTION_SCROLL,
                APP_UI_INTERACTION_FLAG_POINTER_ENABLED
                    | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
                APP_UI_ITEM_FLAG_NONE, -1, ui_panel->row_offset, 0,
                &scroll_rect, ui_panel->title, "");
        }
        SDL_SetRenderClipRect(g_state.renderer, &rich_clip);
        (void)sdl_menu_render_rich_text(scene, ui_panel, font, story_font,
            &rich_clip, line_h, line_gap, line_h + line_gap, rich_start_y);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
        if (rich_scrollable && rich_scroll_px > 0)
        {
            sdl_menu_render_text(font,
                (float)(rows_x + content_w - sdl_menu_scale_px(10.0f)),
                (float)content_top, line_h,
                sdl_menu_panel_accent(ui_panel, ui_panel->accent_attr), "^");
        }
        if (rich_scrollable && rich_scroll_px < rich_max_scroll_px)
        {
            sdl_menu_render_text(font,
                (float)(rows_x + content_w - sdl_menu_scale_px(10.0f)),
                (float)(content_bottom - line_h),
                line_h, sdl_menu_panel_accent(ui_panel, ui_panel->accent_attr),
                "v");
        }
    }

    if (status_h > 0)
    {
        int y = status_y;

        for (i = 0; i < ui_panel->body_line_count; i++)
        {
            sdl_menu_render_text(font, (float)margin_x, (float)y, line_h,
                sdl_menu_panel_color(ui_panel, ui_panel->body_lines[i].attr),
                ui_panel->body_lines[i].text);
            y += line_h + line_gap;
        }
    }

    if (footer_h > 0)
    {
        sdl_menu_render_browser_footer(font, ui_panel, margin_x, footer_y,
            MAX(1, canvas_w - margin_x * 2), line_h, line_gap, item_gap);
    }

    return true;
}
