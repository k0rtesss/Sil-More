#ifndef INCLUDED_APP_SCENE_MENU_H
#define INCLUDED_APP_SCENE_MENU_H

#include "app-interaction.h"
#include "app-snapshot.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MENU_FORMAT_VERSION 2u
#define APP_MENU_TITLE_MAX 80u
#define APP_MENU_TEXT_MAX 160u
#define APP_MENU_LABEL_MAX 96u
#define APP_MENU_META_MAX 64u
#define APP_MENU_KEY_MAX 12u
#define APP_MENU_BODY_LINE_MAX 24u
#define APP_MENU_ROW_MAX 64u
#define APP_MENU_DETAIL_LINE_MAX 24u
#define APP_MENU_FOOTER_ACTION_MAX 8u
#define APP_MENU_TAB_MAX 8u

typedef enum app_menu_scene_flag {
    APP_MENU_SCENE_FLAG_NONE = 0x0000u,
    APP_MENU_SCENE_FLAG_TOP_ANCHORED = 0x0001u,
    APP_MENU_SCENE_FLAG_BOTTOM_ANCHORED = 0x0002u,
    APP_MENU_SCENE_FLAG_PLAIN = 0x0004u,
    APP_MENU_SCENE_FLAG_SHOW_DETAIL = 0x0008u,
    APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP = 0x0010u,
    APP_MENU_SCENE_FLAG_DIM_BACKDROP = 0x0020u,
    APP_MENU_SCENE_FLAG_SCROLL_ROWS = 0x0040u,
    APP_MENU_SCENE_FLAG_LEFT_ANCHORED = 0x0080u,
    APP_MENU_SCENE_FLAG_LEGACY_SIDEBAR = 0x0100u
} app_menu_scene_flag;

typedef enum app_menu_focus_area {
    APP_MENU_FOCUS_NONE = 0,
    APP_MENU_FOCUS_TABS = 1,
    APP_MENU_FOCUS_ROWS = 2,
    APP_MENU_FOCUS_DETAIL = 3,
    APP_MENU_FOCUS_FOOTER = 4
} app_menu_focus_area;

typedef enum app_menu_item_flag {
    APP_MENU_ITEM_FLAG_NONE = 0x00u,
    APP_MENU_ITEM_FLAG_DISABLED = 0x01u,
    APP_MENU_ITEM_FLAG_SELECTED = 0x02u,
    APP_MENU_ITEM_FLAG_ACTIVE = 0x04u,
    APP_MENU_ITEM_FLAG_SECTION = 0x08u
} app_menu_item_flag;

typedef struct app_menu_text_line {
    byte attr;
    byte story;
    u16b flags;
    char text[APP_MENU_TEXT_MAX];
} app_menu_text_line;

typedef struct app_menu_row {
    s16b id;
    byte attr;
    byte meta_attr;
    byte flags;
    byte icon_attr;
    byte reserved0;
    char icon_char;
    char key[APP_MENU_KEY_MAX];
    char label[APP_MENU_LABEL_MAX];
    char meta[APP_MENU_META_MAX];
} app_menu_row;

typedef struct app_menu_footer_action {
    s16b id;
    byte attr;
    byte flags;
    char key[APP_MENU_KEY_MAX];
    char label[APP_MENU_LABEL_MAX];
} app_menu_footer_action;

typedef struct app_menu_tab {
    s16b id;
    byte attr;
    byte flags;
    char label[APP_MENU_LABEL_MAX];
} app_menu_tab;

typedef struct app_menu_scene {
    u16b format_version;
    u16b flags;
    u16b focus_area;
    s16b focus_id;
    u16b min_width_px;
    u16b width_cap_px;
    u16b body_line_count;
    u16b row_count;
    u16b detail_line_count;
    u16b footer_action_count;
    u16b tab_count;
    s16b selected_row;
    s16b row_offset;
    byte title_attr;
    byte subtitle_attr;
    byte detail_title_attr;
    byte accent_attr;
    char title[APP_MENU_TITLE_MAX];
    char subtitle[APP_MENU_TEXT_MAX];
    char detail_title[APP_MENU_TITLE_MAX];
    app_menu_text_line body_lines[APP_MENU_BODY_LINE_MAX];
    app_menu_row rows[APP_MENU_ROW_MAX];
    app_menu_text_line detail_lines[APP_MENU_DETAIL_LINE_MAX];
    app_menu_footer_action footer_actions[APP_MENU_FOOTER_ACTION_MAX];
    app_menu_tab tabs[APP_MENU_TAB_MAX];
} app_menu_scene;

typedef struct app_menu_snapshot {
    app_snapshot snapshot;
    app_snapshot_blob blobs[1];
    app_menu_scene scene;
} app_menu_snapshot;

void app_menu_scene_init(app_menu_scene* scene);
void app_menu_snapshot_init(app_menu_snapshot* snapshot);
void app_menu_scene_set_title(app_menu_scene* scene, byte attr, cptr text);
void app_menu_scene_set_subtitle(app_menu_scene* scene, byte attr, cptr text);
void app_menu_scene_set_detail_title(app_menu_scene* scene, byte attr,
    cptr text);
void app_menu_scene_set_widths(app_menu_scene* scene, u16b min_width_px,
    u16b width_cap_px);
void app_menu_scene_set_row_offset(app_menu_scene* scene, s16b row_offset);
bool app_menu_scene_add_body_line_ex(app_menu_scene* scene, byte attr,
    byte story, cptr text);
bool app_menu_scene_add_body_line(app_menu_scene* scene, byte attr, cptr text);
bool app_menu_scene_add_row_ex(app_menu_scene* scene, s16b id, byte attr,
    byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr key, cptr label, cptr meta);
bool app_menu_scene_add_row(app_menu_scene* scene, s16b id, byte attr,
    bool enabled, bool selected, cptr key, cptr label, cptr meta);
bool app_menu_scene_add_detail_line_ex(app_menu_scene* scene, byte attr,
    byte story, cptr text);
bool app_menu_scene_add_detail_line(app_menu_scene* scene, byte attr,
    cptr text);
bool app_menu_scene_add_footer_action(app_menu_scene* scene, s16b id,
    byte attr, bool enabled, cptr key, cptr label);
bool app_menu_scene_add_tab(app_menu_scene* scene, s16b id, byte attr,
    bool active, cptr label);
bool app_menu_scene_from_interaction(app_menu_scene* scene,
    const app_interaction_state* interaction);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_MENU_H */
