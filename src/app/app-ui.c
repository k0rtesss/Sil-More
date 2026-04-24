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

#include "app-ui.h"
#include "support/utf8.h"

static void app_ui_copy_text(char* dst, size_t dst_size, cptr text)
{
    if (!dst || !dst_size)
        return;

    (void)utf8_strlcpy(dst, text ? text : "", dst_size);
}

static app_ui_rich_paragraph* app_ui_panel_append_rich_paragraph(
    app_ui_scene* scene, app_ui_panel* panel)
{
    app_ui_rich_paragraph* paragraph;

    if (!scene || !panel)
        return NULL;
    if (scene->rich_paragraph_count >= APP_UI_RICH_PARAGRAPH_MAX)
        return NULL;

    if (panel->rich_paragraph_count == 0)
        panel->rich_paragraph_first = scene->rich_paragraph_count;
    else if ((u16b)(panel->rich_paragraph_first + panel->rich_paragraph_count)
        != scene->rich_paragraph_count)
    {
        return NULL;
    }

    paragraph = &scene->rich_paragraphs[scene->rich_paragraph_count++];
    memset(paragraph, 0, sizeof(*paragraph));
    panel->rich_paragraph_count++;
    return paragraph;
}

static app_ui_rich_paragraph* app_ui_panel_current_rich_paragraph(
    app_ui_scene* scene, app_ui_panel* panel)
{
    if (!scene || !panel || panel->rich_paragraph_count == 0)
        return NULL;

    return &scene->rich_paragraphs[(u16b)(panel->rich_paragraph_first
        + panel->rich_paragraph_count - 1)];
}

static app_ui_rich_run* app_ui_panel_append_rich_run(app_ui_scene* scene,
    app_ui_panel* panel)
{
    app_ui_rich_paragraph* paragraph;
    app_ui_rich_run* run;

    if (!scene || !panel)
        return NULL;
    if (scene->rich_run_count >= APP_UI_RICH_RUN_MAX)
        return NULL;

    paragraph = app_ui_panel_current_rich_paragraph(scene, panel);
    if (!paragraph)
        paragraph = app_ui_panel_append_rich_paragraph(scene, panel);
    if (!paragraph)
        return NULL;

    if (paragraph->run_count == 0)
        paragraph->run_first = scene->rich_run_count;
    else if ((u16b)(paragraph->run_first + paragraph->run_count)
        != scene->rich_run_count)
    {
        return NULL;
    }

    run = &scene->rich_runs[scene->rich_run_count++];
    memset(run, 0, sizeof(*run));
    paragraph->run_count++;
    return run;
}

void app_ui_panel_init(app_ui_panel* panel, u16b layer)
{
    if (!panel)
        return;

    memset(panel, 0, sizeof(*panel));
    panel->layer = layer;
    panel->flags = APP_UI_PANEL_FLAG_ACTIVE;
    panel->style = APP_UI_PANEL_STYLE_DEFAULT;
    panel->focus_area = APP_UI_FOCUS_NONE;
    panel->focus_id = -1;
    panel->selected_row = -1;
    panel->row_offset = 0;
    panel->min_width_px = 260;
    panel->width_cap_px = 720;
    panel->title_attr = TERM_L_BLUE;
    panel->subtitle_attr = TERM_WHITE;
    panel->detail_title_attr = TERM_L_BLUE;
    panel->accent_attr = TERM_L_BLUE;
    panel->alpha = 0xFFu;
}

void app_ui_scene_init(app_ui_scene* scene)
{
    if (!scene)
        return;

    memset(scene, 0, sizeof(*scene));
    scene->format_version = APP_UI_FORMAT_VERSION;
}

app_ui_panel* app_ui_scene_append_panel(app_ui_scene* scene, u16b layer)
{
    app_ui_panel* panel;

    if (!scene || scene->panel_count >= APP_UI_PANEL_MAX)
        return NULL;

    panel = &scene->panels[scene->panel_count++];
    app_ui_panel_init(panel, layer);
    return panel;
}

void app_ui_panel_set_icon(app_ui_panel* panel, byte attr, char ch)
{
    if (!panel)
        return;

    panel->icon_attr = attr;
    panel->icon_char = ch;
}

void app_ui_panel_set_title(app_ui_panel* panel, byte attr, cptr text)
{
    if (!panel)
        return;

    panel->title_attr = attr;
    app_ui_copy_text(panel->title, sizeof(panel->title), text);
}

void app_ui_panel_set_subtitle(app_ui_panel* panel, byte attr, cptr text)
{
    if (!panel)
        return;

    panel->subtitle_attr = attr;
    app_ui_copy_text(panel->subtitle, sizeof(panel->subtitle), text);
}

void app_ui_panel_set_detail_title(app_ui_panel* panel, byte attr, cptr text)
{
    if (!panel)
        return;

    panel->detail_title_attr = attr;
    app_ui_copy_text(panel->detail_title, sizeof(panel->detail_title), text);
}

void app_ui_panel_set_widths(app_ui_panel* panel, u16b min_width_px,
    u16b width_cap_px)
{
    if (!panel)
        return;

    panel->min_width_px = min_width_px;
    panel->width_cap_px = width_cap_px;
}

void app_ui_panel_set_row_offset(app_ui_panel* panel, s16b row_offset)
{
    if (!panel)
        return;

    panel->row_offset = row_offset;
}

bool app_ui_panel_add_body_line_ex(app_ui_panel* panel, byte attr,
    byte story, cptr text)
{
    app_ui_text_line* line;

    if (!panel || !text || !text[0]
        || panel->body_line_count >= APP_UI_BODY_LINE_MAX)
    {
        return false;
    }

    line = &panel->body_lines[panel->body_line_count++];
    memset(line, 0, sizeof(*line));
    line->attr = attr;
    line->story = story;
    app_ui_copy_text(line->text, sizeof(line->text), text);
    return true;
}

bool app_ui_panel_add_body_line(app_ui_panel* panel, byte attr, cptr text)
{
    return app_ui_panel_add_body_line_ex(panel, attr, 0, text);
}

bool app_ui_panel_add_row_ex(app_ui_panel* panel, s16b id, byte attr,
    byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr key, cptr label, cptr meta)
{
    app_ui_row* row;

    if (!panel || panel->row_count >= APP_UI_ROW_MAX)
        return false;
    if ((!key || !key[0]) && (!label || !label[0]) && (!meta || !meta[0])
        && (!icon_char || icon_char == ' '))
    {
        return false;
    }

    row = &panel->rows[panel->row_count];
    memset(row, 0, sizeof(*row));
    row->id = id;
    row->attr = attr;
    row->meta_attr = meta_attr;
    row->flags = APP_UI_ITEM_FLAG_NONE;
    row->icon_attr = icon_attr;
    row->icon_char = icon_char;
    if (!enabled)
        row->flags |= APP_UI_ITEM_FLAG_DISABLED;
    if (selected)
        row->flags |= APP_UI_ITEM_FLAG_SELECTED;
    app_ui_copy_text(row->key, sizeof(row->key), key);
    app_ui_copy_text(row->label, sizeof(row->label), label);
    app_ui_copy_text(row->meta, sizeof(row->meta), meta);

    if (selected)
    {
        panel->selected_row = (s16b)panel->row_count;
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = id;
    }

    panel->row_count++;
    return true;
}

bool app_ui_panel_add_row(app_ui_panel* panel, s16b id, byte attr,
    bool enabled, bool selected, cptr key, cptr label, cptr meta)
{
    return app_ui_panel_add_row_ex(panel, id, attr, attr, 0, '\0', enabled,
        selected, key, label, meta);
}

bool app_ui_panel_add_detail_line_ex(app_ui_panel* panel, byte attr,
    byte story, cptr text)
{
    app_ui_text_line* line;

    if (!panel || !text || !text[0]
        || panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
    {
        return false;
    }

    line = &panel->detail_lines[panel->detail_line_count++];
    memset(line, 0, sizeof(*line));
    line->attr = attr;
    line->story = story;
    app_ui_copy_text(line->text, sizeof(line->text), text);
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
    return true;
}

bool app_ui_panel_add_detail_line(app_ui_panel* panel, byte attr, cptr text)
{
    return app_ui_panel_add_detail_line_ex(panel, attr, 0, text);
}

bool app_ui_panel_add_footer_action(app_ui_panel* panel, s16b id, byte attr,
    bool enabled, cptr key, cptr label)
{
    app_ui_footer_action* action;

    if (!panel || !label || !label[0]
        || panel->footer_action_count >= APP_UI_FOOTER_ACTION_MAX)
    {
        return false;
    }

    action = &panel->footer_actions[panel->footer_action_count++];
    memset(action, 0, sizeof(*action));
    action->id = id;
    action->attr = attr;
    action->flags = enabled ? APP_UI_ITEM_FLAG_NONE : APP_UI_ITEM_FLAG_DISABLED;
    app_ui_copy_text(action->key, sizeof(action->key), key);
    app_ui_copy_text(action->label, sizeof(action->label), label);
    if (panel->focus_area == APP_UI_FOCUS_NONE)
    {
        panel->focus_area = APP_UI_FOCUS_FOOTER;
        panel->focus_id = id;
    }
    return true;
}

bool app_ui_panel_add_tab(app_ui_panel* panel, s16b id, byte attr,
    bool active, cptr label)
{
    app_ui_tab* tab;

    if (!panel || !label || !label[0] || panel->tab_count >= APP_UI_TAB_MAX)
        return false;

    tab = &panel->tabs[panel->tab_count++];
    memset(tab, 0, sizeof(*tab));
    tab->id = id;
    tab->attr = attr;
    tab->flags = active ? APP_UI_ITEM_FLAG_ACTIVE : APP_UI_ITEM_FLAG_NONE;
    app_ui_copy_text(tab->label, sizeof(tab->label), label);
    if (active)
    {
        panel->focus_area = APP_UI_FOCUS_TABS;
        panel->focus_id = id;
    }
    return true;
}

bool app_ui_panel_begin_rich_paragraph(app_ui_scene* scene, app_ui_panel* panel)
{
    if (!scene || !panel)
        return false;

    return app_ui_panel_append_rich_paragraph(scene, panel) != NULL;
}

bool app_ui_panel_add_rich_text_alpha_ex(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, byte story, byte alpha, cptr text)
{
    app_ui_rich_paragraph* paragraph;
    bool wrote_any = false;
    cptr cursor;

    if (!scene || !panel || !text || !text[0])
        return false;

    cursor = text;
    while (*cursor)
    {
        app_ui_rich_run* run;
        size_t len;

        paragraph = app_ui_panel_current_rich_paragraph(scene, panel);
        if (!paragraph && !app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        paragraph = app_ui_panel_current_rich_paragraph(scene, panel);
        if (!paragraph)
            return false;

        if (paragraph->run_count > 0)
        {
            run = &scene->rich_runs[(u16b)(paragraph->run_first
                + paragraph->run_count - 1)];
            if (run->attr == attr && run->story == story
                && run->alpha == alpha)
            {
                size_t current_len = strlen(run->text);
                size_t available = APP_UI_TEXT_MAX - 1u - current_len;

                if (available > 0)
                {
                    len = utf8_clip_bytes(cursor, available);
                    if (len > available)
                        len = available;
                    if (len == 0)
                        break;
                    memcpy(run->text + current_len, cursor, len);
                    run->text[current_len + len] = '\0';
                    cursor += len;
                    wrote_any = true;
                    continue;
                }
            }
        }

        run = app_ui_panel_append_rich_run(scene, panel);
        if (!run)
            return false;

        len = utf8_clip_bytes(cursor, APP_UI_TEXT_MAX - 1u);
        if (len == 0)
            break;

        run->attr = attr;
        run->story = story;
        run->alpha = alpha;
        memcpy(run->text, cursor, len);
        run->text[len] = '\0';
        cursor += len;
        wrote_any = true;
    }

    return wrote_any;
}

bool app_ui_panel_add_rich_text_ex(app_ui_scene* scene, app_ui_panel* panel,
    byte attr, byte story, cptr text)
{
    return app_ui_panel_add_rich_text_alpha_ex(scene, panel, attr, story,
        0xFFu, text);
}

bool app_ui_panel_add_rich_text_alpha(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, byte alpha, cptr text)
{
    return app_ui_panel_add_rich_text_alpha_ex(scene, panel, attr, 0,
        alpha, text);
}

bool app_ui_panel_add_rich_text(app_ui_scene* scene, app_ui_panel* panel,
    byte attr, cptr text)
{
    return app_ui_panel_add_rich_text_alpha(scene, panel, attr, 0xFFu, text);
}

bool app_ui_panel_set_minimap(app_ui_scene* scene, app_ui_panel* panel,
    u16b width, u16b height, s16b player_x, s16b player_y,
    byte border_attr, byte player_attr, const app_ui_minimap_cell* cells)
{
    size_t cell_count;
    u16b first;

    if (!scene || !panel || !cells || width == 0 || height == 0)
        return false;

    cell_count = (size_t)width * (size_t)height;
    if (cell_count == 0 || cell_count > APP_UI_MINIMAP_CELL_MAX)
        return false;
    if ((size_t)scene->minimap_cell_count + cell_count > APP_UI_MINIMAP_CELL_MAX)
        return false;

    first = scene->minimap_cell_count;
    memcpy(scene->minimap_cells + first, cells,
        cell_count * sizeof(scene->minimap_cells[0]));
    scene->minimap_cell_count = (u16b)(scene->minimap_cell_count + cell_count);

    panel->minimap_cell_first = first;
    panel->minimap_cell_count = (u16b)cell_count;
    panel->minimap_width = width;
    panel->minimap_height = height;
    panel->minimap_player_x = player_x;
    panel->minimap_player_y = player_y;
    panel->minimap_border_attr = border_attr;
    panel->minimap_player_attr = player_attr;
    return true;
}

bool app_ui_panel_add_character_metric(app_ui_panel* panel, byte label_attr,
    cptr label, byte value_attr, cptr value, char separator,
    byte secondary_attr, cptr secondary)
{
    app_ui_character_metric* metric;

    if (!panel
        || panel->character_metric_count >= APP_UI_CHARACTER_METRIC_MAX)
    {
        return false;
    }

    metric = &panel->character_metrics[panel->character_metric_count++];
    memset(metric, 0, sizeof(*metric));
    metric->label_attr = label_attr;
    metric->value_attr = value_attr;
    metric->secondary_attr = secondary_attr;
    metric->separator = separator;
    app_ui_copy_text(metric->label, sizeof(metric->label), label);
    app_ui_copy_text(metric->value, sizeof(metric->value), value);
    app_ui_copy_text(metric->secondary, sizeof(metric->secondary), secondary);
    return true;
}

bool app_ui_panel_add_character_stat(app_ui_panel* panel, byte label_attr,
    cptr label, byte value_attr, cptr value, byte separator_attr,
    char separator, byte base_attr, cptr base, byte mod1_attr, cptr mod1,
    byte mod2_attr, cptr mod2, byte mod3_attr, cptr mod3)
{
    app_ui_character_stat* stat;

    if (!panel
        || panel->character_stat_count >= APP_UI_CHARACTER_STAT_MAX)
    {
        return false;
    }

    stat = &panel->character_stats[panel->character_stat_count++];
    memset(stat, 0, sizeof(*stat));
    stat->label_attr = label_attr;
    stat->value_attr = value_attr;
    stat->separator_attr = separator_attr;
    stat->base_attr = base_attr;
    stat->mod1_attr = mod1_attr;
    stat->mod2_attr = mod2_attr;
    stat->mod3_attr = mod3_attr;
    stat->separator = separator;
    app_ui_copy_text(stat->label, sizeof(stat->label), label);
    app_ui_copy_text(stat->value, sizeof(stat->value), value);
    app_ui_copy_text(stat->base, sizeof(stat->base), base);
    app_ui_copy_text(stat->mod1, sizeof(stat->mod1), mod1);
    app_ui_copy_text(stat->mod2, sizeof(stat->mod2), mod2);
    app_ui_copy_text(stat->mod3, sizeof(stat->mod3), mod3);
    return true;
}
