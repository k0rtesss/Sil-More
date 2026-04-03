#include "angband.h"

#include "app-ui.h"

static void app_ui_copy_text(char* dst, size_t dst_size, cptr text)
{
    if (!dst || !dst_size)
        return;

    SDL_strlcpy(dst, text ? text : "", dst_size);
}

static void app_ui_panel_add_interaction_value_line(app_ui_panel* panel,
    byte attr, const app_interaction_state* interaction)
{
    char value_buf[APP_UI_TEXT_MAX];
    size_t len;

    if (!panel || !interaction || !interaction->value[0])
        return;

    app_ui_copy_text(value_buf, sizeof(value_buf), interaction->value);
    len = strlen(value_buf);
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_CURSOR)
        && interaction->cursor_index >= 0
        && interaction->cursor_index <= (s16b)len
        && len + 1 < sizeof(value_buf))
    {
        size_t cursor = (size_t)interaction->cursor_index;

        memmove(value_buf + cursor + 1, value_buf + cursor, len - cursor + 1);
        value_buf[cursor] = '_';
    }

    (void)app_ui_panel_add_body_line(panel, attr, value_buf);
}

static cptr app_ui_confirm_label(const app_interaction_state* interaction)
{
    if (!interaction)
        return "Confirm";

    switch (interaction->kind)
    {
    case APP_INTERACTION_KIND_LIST:
        return "Select";

    case APP_INTERACTION_KIND_TEXT_ENTRY:
        return "Accept";

    case APP_INTERACTION_KIND_TARGETING:
    case APP_INTERACTION_KIND_LOOK:
        return "Confirm";

    default:
        return "Confirm";
    }
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

bool app_ui_scene_from_interaction(app_ui_scene* scene,
    const app_interaction_state* interaction)
{
    app_ui_panel* panel;
    size_t i;

    if (!scene || !interaction
        || interaction->kind == APP_INTERACTION_KIND_NONE)
    {
        return false;
    }

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->accent_attr = interaction->prompt_attr ? interaction->prompt_attr
        : TERM_L_BLUE;
    panel->min_width_px = 220;
    panel->width_cap_px = 640;

    if ((interaction->flags & APP_INTERACTION_FLAG_PLAIN_LIST) != 0)
        panel->style = APP_UI_PANEL_STYLE_PLAIN;
    if (interaction->kind == APP_INTERACTION_KIND_TARGETING
        || interaction->kind == APP_INTERACTION_KIND_LOOK)
    {
        panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED;
    }

    if (interaction->prompt[0])
        app_ui_panel_set_title(panel, interaction->prompt_attr,
            interaction->prompt);
    if (interaction->detail[0])
        app_ui_panel_set_subtitle(panel, interaction->detail_attr,
            interaction->detail);
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_VALUE) != 0
        && interaction->value[0])
    {
        app_ui_panel_add_interaction_value_line(panel,
            interaction->value_attr, interaction);
    }

    if (interaction->option_count > 0)
        panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;

    for (i = 0; i < interaction->option_count && i < APP_UI_ROW_MAX; i++)
    {
        const app_interaction_option* option = &interaction->options[i];
        bool selected = option->selected
            || ((s16b)i == interaction->selected_index);
        byte attr = option->enabled ? option->attr : TERM_L_DARK;
        char key_buf[APP_UI_KEY_MAX];

        key_buf[0] = '\0';
        if (option->key[0])
            app_ui_copy_text(key_buf, sizeof(key_buf), option->key);
        else if (option->tag)
            strnfmt(key_buf, sizeof(key_buf), "%c", option->tag);

        if (!app_ui_panel_add_row(panel, (s16b)i, attr,
                option->enabled != 0, selected, key_buf, option->label,
                option->meta))
        {
            return false;
        }
    }

    if ((interaction->flags & APP_INTERACTION_FLAG_CAN_CONFIRM) != 0)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", app_ui_confirm_label(interaction));
    }
    if ((interaction->flags & APP_INTERACTION_FLAG_CAN_CANCEL) != 0)
    {
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Cancel");
    }

    if (panel->row_count > 0 && panel->focus_area == APP_UI_FOCUS_NONE)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = (panel->selected_row >= 0) ? panel->selected_row : 0;
    }

    return true;
}
