#include "angband.h"

#include "app-ui.h"
#include "app-scene-menu.h"

static void app_ui_copy_text(char* dst, size_t dst_size, cptr text)
{
    if (!dst || !dst_size)
        return;

    SDL_strlcpy(dst, text ? text : "", dst_size);
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

static bool app_ui_panel_copy_menu_rows(app_ui_panel* panel,
    const struct app_menu_scene* menu_scene)
{
    u16b i;

    if (!panel || !menu_scene)
        return false;

    for (i = 0; i < menu_scene->body_line_count; i++)
    {
        const app_menu_text_line* line = &menu_scene->body_lines[i];

        if (!app_ui_panel_add_body_line_ex(panel, line->attr, line->story,
                line->text))
        {
            return false;
        }
    }

    for (i = 0; i < menu_scene->row_count; i++)
    {
        const app_menu_row* row = &menu_scene->rows[i];
        bool enabled = (row->flags & APP_MENU_ITEM_FLAG_DISABLED) == 0;
        bool selected = (row->flags & APP_MENU_ITEM_FLAG_SELECTED) != 0;

        if (!app_ui_panel_add_row_ex(panel, row->id, row->attr,
                row->meta_attr, row->icon_attr, row->icon_char, enabled,
                selected, row->key, row->label, row->meta))
        {
            return false;
        }
        if (row->flags & APP_MENU_ITEM_FLAG_SECTION)
            panel->rows[i].flags |= APP_UI_ITEM_FLAG_SECTION;
        if (row->flags & APP_MENU_ITEM_FLAG_ACTIVE)
            panel->rows[i].flags |= APP_UI_ITEM_FLAG_ACTIVE;
    }

    for (i = 0; i < menu_scene->detail_line_count; i++)
    {
        const app_menu_text_line* line = &menu_scene->detail_lines[i];

        if (!app_ui_panel_add_detail_line_ex(panel, line->attr, line->story,
                line->text))
        {
            return false;
        }
    }

    for (i = 0; i < menu_scene->footer_action_count; i++)
    {
        const app_menu_footer_action* action
            = &menu_scene->footer_actions[i];
        bool enabled = (action->flags & APP_MENU_ITEM_FLAG_DISABLED) == 0;

        if (!app_ui_panel_add_footer_action(panel, action->id,
                action->attr, enabled, action->key, action->label))
        {
            return false;
        }
    }

    for (i = 0; i < menu_scene->tab_count; i++)
    {
        const app_menu_tab* tab = &menu_scene->tabs[i];
        bool active = (tab->flags & APP_MENU_ITEM_FLAG_ACTIVE) != 0;

        if (!app_ui_panel_add_tab(panel, tab->id, tab->attr, active,
                tab->label))
        {
            return false;
        }
    }

    return true;
}

bool app_ui_scene_from_menu_scene(app_ui_scene* scene,
    const struct app_menu_scene* menu_scene)
{
    app_ui_panel* panel;

    if (!scene || !menu_scene)
        return false;

    app_ui_scene_init(scene);
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP)
        scene->flags |= APP_UI_SCENE_FLAG_USE_BACKDROP;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_DIM_BACKDROP)
        scene->flags |= APP_UI_SCENE_FLAG_DIM_BACKDROP;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    if (menu_scene->flags & APP_MENU_SCENE_FLAG_TOP_ANCHORED)
        panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_BOTTOM_ANCHORED)
        panel->flags |= APP_UI_PANEL_FLAG_BOTTOM_ANCHORED;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_LEFT_ANCHORED)
        panel->flags |= APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_SHOW_DETAIL)
        panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_SCROLL_ROWS)
        panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_PLAIN)
        panel->style = APP_UI_PANEL_STYLE_PLAIN;
    if (menu_scene->flags & APP_MENU_SCENE_FLAG_LEGACY_SIDEBAR)
        panel->style = APP_UI_PANEL_STYLE_STATUS_RAIL;

    panel->focus_area = menu_scene->focus_area;
    panel->focus_id = menu_scene->focus_id;
    panel->selected_row = menu_scene->selected_row;
    panel->row_offset = menu_scene->row_offset;
    panel->min_width_px = menu_scene->min_width_px;
    panel->width_cap_px = menu_scene->width_cap_px;
    panel->title_attr = menu_scene->title_attr;
    panel->subtitle_attr = menu_scene->subtitle_attr;
    panel->detail_title_attr = menu_scene->detail_title_attr;
    panel->accent_attr = menu_scene->accent_attr;
    app_ui_copy_text(panel->title, sizeof(panel->title), menu_scene->title);
    app_ui_copy_text(panel->subtitle, sizeof(panel->subtitle),
        menu_scene->subtitle);
    app_ui_copy_text(panel->detail_title, sizeof(panel->detail_title),
        menu_scene->detail_title);

    if (!app_ui_panel_copy_menu_rows(panel, menu_scene))
        return false;

    panel->selected_row = menu_scene->selected_row;
    panel->focus_area = menu_scene->focus_area;
    panel->focus_id = menu_scene->focus_id;
    return true;
}
