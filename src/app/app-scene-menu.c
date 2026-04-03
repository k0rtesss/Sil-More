#include "angband.h"

#include "app-scene-menu.h"

static void app_menu_copy_text(char* dst, size_t dst_size, cptr text)
{
    if (!dst || !dst_size)
        return;

    SDL_strlcpy(dst, text ? text : "", dst_size);
}

static void app_menu_set_value_line(app_menu_scene* scene, byte attr,
    const app_interaction_state* interaction)
{
    char value_buf[APP_MENU_TEXT_MAX];
    size_t len;

    if (!scene || !interaction || !interaction->value[0])
        return;

    app_menu_copy_text(value_buf, sizeof(value_buf), interaction->value);
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

    (void)app_menu_scene_add_body_line(scene, attr, value_buf);
}

static cptr app_menu_confirm_label(const app_interaction_state* interaction)
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

void app_menu_scene_init(app_menu_scene* scene)
{
    if (!scene)
        return;

    memset(scene, 0, sizeof(*scene));
    scene->format_version = APP_MENU_FORMAT_VERSION;
    scene->focus_id = -1;
    scene->selected_row = -1;
    scene->row_offset = 0;
    scene->min_width_px = 260;
    scene->width_cap_px = 720;
    scene->title_attr = TERM_L_BLUE;
    scene->subtitle_attr = TERM_WHITE;
    scene->detail_title_attr = TERM_L_BLUE;
    scene->accent_attr = TERM_L_BLUE;
}

void app_menu_snapshot_init(app_menu_snapshot* snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
    app_ui_scene_init(&snapshot->scene);
    snapshot->snapshot.scene = APP_SCENE_KIND_MENU;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
    snapshot->blobs[0].kind = APP_SNAPSHOT_BLOB_MENU;
    snapshot->blobs[0].format_version = APP_UI_FORMAT_VERSION;
    snapshot->blobs[0].data = (const byte*)&snapshot->scene;
    snapshot->blobs[0].size = sizeof(snapshot->scene);
}

void app_menu_scene_set_title(app_menu_scene* scene, byte attr, cptr text)
{
    if (!scene)
        return;

    scene->title_attr = attr;
    app_menu_copy_text(scene->title, sizeof(scene->title), text);
}

void app_menu_scene_set_subtitle(app_menu_scene* scene, byte attr, cptr text)
{
    if (!scene)
        return;

    scene->subtitle_attr = attr;
    app_menu_copy_text(scene->subtitle, sizeof(scene->subtitle), text);
}

void app_menu_scene_set_detail_title(app_menu_scene* scene, byte attr,
    cptr text)
{
    if (!scene)
        return;

    scene->detail_title_attr = attr;
    app_menu_copy_text(scene->detail_title, sizeof(scene->detail_title), text);
}

void app_menu_scene_set_widths(app_menu_scene* scene, u16b min_width_px,
    u16b width_cap_px)
{
    if (!scene)
        return;

    scene->min_width_px = min_width_px;
    scene->width_cap_px = width_cap_px;
}

void app_menu_scene_set_row_offset(app_menu_scene* scene, s16b row_offset)
{
    if (!scene)
        return;

    scene->row_offset = row_offset;
}

bool app_menu_scene_add_body_line_ex(app_menu_scene* scene, byte attr,
    byte story, cptr text)
{
    app_menu_text_line* line;

    if (!scene || !text || !text[0]
        || scene->body_line_count >= APP_MENU_BODY_LINE_MAX)
    {
        return false;
    }

    line = &scene->body_lines[scene->body_line_count++];
    memset(line, 0, sizeof(*line));
    line->attr = attr;
    line->story = story;
    app_menu_copy_text(line->text, sizeof(line->text), text);
    return true;
}

bool app_menu_scene_add_body_line(app_menu_scene* scene, byte attr, cptr text)
{
    return app_menu_scene_add_body_line_ex(scene, attr, 0, text);
}

bool app_menu_scene_add_row_ex(app_menu_scene* scene, s16b id, byte attr,
    byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr key, cptr label, cptr meta)
{
    app_menu_row* row;

    if (!scene || !label || !label[0] || scene->row_count >= APP_MENU_ROW_MAX)
        return false;

    row = &scene->rows[scene->row_count];
    memset(row, 0, sizeof(*row));
    row->id = id;
    row->attr = attr;
    row->meta_attr = meta_attr;
    row->flags = APP_MENU_ITEM_FLAG_NONE;
    row->icon_attr = icon_attr;
    row->icon_char = icon_char;
    if (!enabled)
        row->flags |= APP_MENU_ITEM_FLAG_DISABLED;
    if (selected)
        row->flags |= APP_MENU_ITEM_FLAG_SELECTED;
    app_menu_copy_text(row->key, sizeof(row->key), key);
    app_menu_copy_text(row->label, sizeof(row->label), label);
    app_menu_copy_text(row->meta, sizeof(row->meta), meta);

    if (selected)
    {
        scene->selected_row = (s16b)scene->row_count;
        scene->focus_area = APP_MENU_FOCUS_ROWS;
        scene->focus_id = id;
    }

    scene->row_count++;
    return true;
}

bool app_menu_scene_add_row(app_menu_scene* scene, s16b id, byte attr,
    bool enabled, bool selected, cptr key, cptr label, cptr meta)
{
    return app_menu_scene_add_row_ex(scene, id, attr, attr, 0, '\0', enabled,
        selected, key, label, meta);
}

bool app_menu_scene_add_detail_line_ex(app_menu_scene* scene, byte attr,
    byte story, cptr text)
{
    app_menu_text_line* line;

    if (!scene || !text || !text[0]
        || scene->detail_line_count >= APP_MENU_DETAIL_LINE_MAX)
    {
        return false;
    }

    line = &scene->detail_lines[scene->detail_line_count++];
    memset(line, 0, sizeof(*line));
    line->attr = attr;
    line->story = story;
    app_menu_copy_text(line->text, sizeof(line->text), text);
    scene->flags |= APP_MENU_SCENE_FLAG_SHOW_DETAIL;
    return true;
}

bool app_menu_scene_add_detail_line(app_menu_scene* scene, byte attr,
    cptr text)
{
    return app_menu_scene_add_detail_line_ex(scene, attr, 0, text);
}

bool app_menu_scene_add_footer_action(app_menu_scene* scene, s16b id,
    byte attr, bool enabled, cptr key, cptr label)
{
    app_menu_footer_action* action;

    if (!scene || !label || !label[0]
        || scene->footer_action_count >= APP_MENU_FOOTER_ACTION_MAX)
    {
        return false;
    }

    action = &scene->footer_actions[scene->footer_action_count++];
    memset(action, 0, sizeof(*action));
    action->id = id;
    action->attr = attr;
    action->flags = enabled ? APP_MENU_ITEM_FLAG_NONE
        : APP_MENU_ITEM_FLAG_DISABLED;
    app_menu_copy_text(action->key, sizeof(action->key), key);
    app_menu_copy_text(action->label, sizeof(action->label), label);
    if (scene->focus_area == APP_MENU_FOCUS_NONE)
    {
        scene->focus_area = APP_MENU_FOCUS_FOOTER;
        scene->focus_id = id;
    }
    return true;
}

bool app_menu_scene_add_tab(app_menu_scene* scene, s16b id, byte attr,
    bool active, cptr label)
{
    app_menu_tab* tab;

    if (!scene || !label || !label[0] || scene->tab_count >= APP_MENU_TAB_MAX)
        return false;

    tab = &scene->tabs[scene->tab_count++];
    memset(tab, 0, sizeof(*tab));
    tab->id = id;
    tab->attr = attr;
    tab->flags = active ? APP_MENU_ITEM_FLAG_ACTIVE : APP_MENU_ITEM_FLAG_NONE;
    app_menu_copy_text(tab->label, sizeof(tab->label), label);
    if (active)
    {
        scene->focus_area = APP_MENU_FOCUS_TABS;
        scene->focus_id = id;
    }
    return true;
}

bool app_menu_scene_from_interaction(app_menu_scene* scene,
    const app_interaction_state* interaction)
{
    size_t i;

    if (!scene || !interaction
        || interaction->kind == APP_INTERACTION_KIND_NONE)
    {
        return false;
    }

    app_menu_scene_init(scene);
    scene->accent_attr = interaction->prompt_attr ? interaction->prompt_attr
        : TERM_L_BLUE;
    scene->min_width_px = 220;
    scene->width_cap_px = 640;

    if ((interaction->flags & APP_INTERACTION_FLAG_PLAIN_LIST) != 0)
        scene->flags |= APP_MENU_SCENE_FLAG_PLAIN;
    if (interaction->kind == APP_INTERACTION_KIND_TARGETING
        || interaction->kind == APP_INTERACTION_KIND_LOOK)
    {
        scene->flags |= APP_MENU_SCENE_FLAG_TOP_ANCHORED;
    }

    if (interaction->prompt[0])
        app_menu_scene_set_title(scene, interaction->prompt_attr,
            interaction->prompt);
    if (interaction->detail[0])
        app_menu_scene_set_subtitle(scene, interaction->detail_attr,
            interaction->detail);
    if ((interaction->flags & APP_INTERACTION_FLAG_SHOW_VALUE) != 0
        && interaction->value[0])
    {
        app_menu_set_value_line(scene, interaction->value_attr, interaction);
    }

    if (interaction->option_count > 0)
        scene->flags |= APP_MENU_SCENE_FLAG_SCROLL_ROWS;

    for (i = 0; i < interaction->option_count && i < APP_MENU_ROW_MAX; i++)
    {
        const app_interaction_option* option = &interaction->options[i];
        bool selected = option->selected
            || ((s16b)i == interaction->selected_index);
        byte attr = option->enabled ? option->attr : TERM_L_DARK;
        char key_buf[APP_MENU_KEY_MAX];

        key_buf[0] = '\0';
        if (option->key[0])
            app_menu_copy_text(key_buf, sizeof(key_buf), option->key);
        else if (option->tag)
            strnfmt(key_buf, sizeof(key_buf), "%c", option->tag);

        if (!app_menu_scene_add_row(scene, (s16b)i, attr, option->enabled != 0,
                selected, key_buf, option->label, option->meta))
        {
            return false;
        }
    }

    if ((interaction->flags & APP_INTERACTION_FLAG_CAN_CONFIRM) != 0)
    {
        (void)app_menu_scene_add_footer_action(scene, 1, TERM_L_BLUE, true,
            "Enter", app_menu_confirm_label(interaction));
    }
    if ((interaction->flags & APP_INTERACTION_FLAG_CAN_CANCEL) != 0)
    {
        (void)app_menu_scene_add_footer_action(scene, 2, TERM_WHITE, true,
            "Esc", "Cancel");
    }

    if (scene->row_count > 0 && scene->focus_area == APP_MENU_FOCUS_NONE)
    {
        scene->focus_area = APP_MENU_FOCUS_ROWS;
        scene->focus_id = (scene->selected_row >= 0) ? scene->selected_row : 0;
    }

    return true;
}
