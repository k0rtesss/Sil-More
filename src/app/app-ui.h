#ifndef INCLUDED_APP_UI_H
#define INCLUDED_APP_UI_H

#include "app-interaction.h"
#include "app-scene-information.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_UI_FORMAT_VERSION 6u
#define APP_UI_TITLE_MAX 80u
#define APP_UI_TEXT_MAX 160u
#define APP_UI_LABEL_MAX 96u
#define APP_UI_META_MAX 64u
#define APP_UI_KEY_MAX 12u
#define APP_UI_BODY_LINE_MAX 24u
#define APP_UI_ROW_MAX 192u
#define APP_UI_DETAIL_LINE_MAX 24u
#define APP_UI_FOOTER_ACTION_MAX 8u
#define APP_UI_TAB_MAX 8u
#define APP_UI_PANEL_MAX 4u
#define APP_UI_DOCUMENT_OP_MAX 256u
#define APP_UI_RICH_PARAGRAPH_MAX 96u
#define APP_UI_RICH_RUN_MAX 512u

typedef enum app_ui_scene_flag {
    APP_UI_SCENE_FLAG_NONE = 0x0000u,
    APP_UI_SCENE_FLAG_USE_BACKDROP = 0x0001u,
    APP_UI_SCENE_FLAG_DIM_BACKDROP = 0x0002u
} app_ui_scene_flag;

typedef enum app_ui_layer_kind {
    APP_UI_LAYER_NONE = 0,
    APP_UI_LAYER_CHROME = 1,
    APP_UI_LAYER_TRANSIENT = 2,
    APP_UI_LAYER_MODAL = 3,
    APP_UI_LAYER_BROWSER = 4
} app_ui_layer_kind;

typedef enum app_ui_panel_style {
    APP_UI_PANEL_STYLE_DEFAULT = 0,
    APP_UI_PANEL_STYLE_PLAIN = 1,
    APP_UI_PANEL_STYLE_STATUS_RAIL = 2,
    APP_UI_PANEL_STYLE_STRIP = 3,
    APP_UI_PANEL_STYLE_DOCUMENT = 4,
    APP_UI_PANEL_STYLE_BROWSER = 5
} app_ui_panel_style;

typedef enum app_ui_panel_flag {
    APP_UI_PANEL_FLAG_NONE = 0x0000u,
    APP_UI_PANEL_FLAG_ACTIVE = 0x0001u,
    APP_UI_PANEL_FLAG_TOP_ANCHORED = 0x0002u,
    APP_UI_PANEL_FLAG_BOTTOM_ANCHORED = 0x0004u,
    APP_UI_PANEL_FLAG_LEFT_ANCHORED = 0x0008u,
    APP_UI_PANEL_FLAG_SHOW_DETAIL = 0x0010u,
    APP_UI_PANEL_FLAG_SCROLL_ROWS = 0x0020u,
    APP_UI_PANEL_FLAG_DETAIL_LEADING = 0x0040u
} app_ui_panel_flag;

typedef enum app_ui_focus_area {
    APP_UI_FOCUS_NONE = 0,
    APP_UI_FOCUS_TABS = 1,
    APP_UI_FOCUS_ROWS = 2,
    APP_UI_FOCUS_DETAIL = 3,
    APP_UI_FOCUS_FOOTER = 4
} app_ui_focus_area;

typedef enum app_ui_item_flag {
    APP_UI_ITEM_FLAG_NONE = 0x00u,
    APP_UI_ITEM_FLAG_DISABLED = 0x01u,
    APP_UI_ITEM_FLAG_SELECTED = 0x02u,
    APP_UI_ITEM_FLAG_ACTIVE = 0x04u,
    APP_UI_ITEM_FLAG_SECTION = 0x08u,
    APP_UI_ITEM_FLAG_STORY_LABEL = 0x10u
} app_ui_item_flag;

typedef struct app_ui_text_line {
    byte attr;
    byte story;
    u16b flags;
    char text[APP_UI_TEXT_MAX];
} app_ui_text_line;

typedef struct app_ui_row {
    s16b id;
    byte attr;
    byte meta_attr;
    byte flags;
    byte icon_attr;
    byte extra_icon_attr;
    char icon_char;
    char extra_icon_char;
    char key[APP_UI_KEY_MAX];
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} app_ui_row;

typedef struct app_ui_footer_action {
    s16b id;
    byte attr;
    byte flags;
    char key[APP_UI_KEY_MAX];
    char label[APP_UI_LABEL_MAX];
} app_ui_footer_action;

typedef struct app_ui_tab {
    s16b id;
    byte attr;
    byte flags;
    char label[APP_UI_LABEL_MAX];
} app_ui_tab;

typedef enum app_ui_document_op_kind {
    APP_UI_DOCUMENT_OP_NONE = 0,
    APP_UI_DOCUMENT_OP_TEXT = 1,
    APP_UI_DOCUMENT_OP_CELL = 2,
    APP_UI_DOCUMENT_OP_CURSOR = 3
} app_ui_document_op_kind;

typedef struct app_ui_document_op {
    byte kind;
    byte attr;
    byte story;
    byte width;
    s16b row;
    s16b col;
    byte terrain_attr;
    char ch;
    char terrain_char;
    byte reserved;
    char text[APP_UI_TEXT_MAX];
} app_ui_document_op;

typedef struct app_ui_rich_paragraph {
    u16b run_first;
    u16b run_count;
} app_ui_rich_paragraph;

typedef struct app_ui_rich_run {
    byte attr;
    byte story;
    u16b reserved;
    char text[APP_UI_TEXT_MAX];
} app_ui_rich_run;

typedef struct app_ui_panel {
    u16b layer;
    u16b flags;
    u16b style;
    u16b focus_area;
    s16b focus_id;
    s16b selected_row;
    s16b row_offset;
    u16b min_width_px;
    u16b width_cap_px;
    u16b body_line_count;
    u16b row_count;
    u16b detail_line_count;
    u16b footer_action_count;
    u16b tab_count;
    u16b document_op_first;
    u16b document_op_count;
    u16b document_rows;
    u16b document_cols;
    u16b rich_paragraph_first;
    u16b rich_paragraph_count;
    byte title_attr;
    byte subtitle_attr;
    byte detail_title_attr;
    byte accent_attr;
    byte icon_attr;
    char icon_char;
    u16b reserved;
    char title[APP_UI_TITLE_MAX];
    char subtitle[APP_UI_TEXT_MAX];
    char detail_title[APP_UI_TITLE_MAX];
    app_ui_text_line body_lines[APP_UI_BODY_LINE_MAX];
    app_ui_row rows[APP_UI_ROW_MAX];
    app_ui_text_line detail_lines[APP_UI_DETAIL_LINE_MAX];
    app_ui_footer_action footer_actions[APP_UI_FOOTER_ACTION_MAX];
    app_ui_tab tabs[APP_UI_TAB_MAX];
} app_ui_panel;

typedef struct app_ui_scene {
    u16b format_version;
    u16b flags;
    u16b panel_count;
    u16b document_op_count;
    u16b rich_paragraph_count;
    u16b rich_run_count;
    app_ui_panel panels[APP_UI_PANEL_MAX];
    app_ui_document_op document_ops[APP_UI_DOCUMENT_OP_MAX];
    app_ui_rich_paragraph rich_paragraphs[APP_UI_RICH_PARAGRAPH_MAX];
    app_ui_rich_run rich_runs[APP_UI_RICH_RUN_MAX];
} app_ui_scene;

void app_ui_panel_init(app_ui_panel* panel, u16b layer);
void app_ui_scene_init(app_ui_scene* scene);
app_ui_panel* app_ui_scene_append_panel(app_ui_scene* scene, u16b layer);
void app_ui_panel_set_icon(app_ui_panel* panel, byte attr, char ch);
void app_ui_panel_set_title(app_ui_panel* panel, byte attr, cptr text);
void app_ui_panel_set_subtitle(app_ui_panel* panel, byte attr, cptr text);
void app_ui_panel_set_detail_title(app_ui_panel* panel, byte attr, cptr text);
void app_ui_panel_set_widths(app_ui_panel* panel, u16b min_width_px,
    u16b width_cap_px);
void app_ui_panel_set_row_offset(app_ui_panel* panel, s16b row_offset);
bool app_ui_panel_add_body_line_ex(app_ui_panel* panel, byte attr,
    byte story, cptr text);
bool app_ui_panel_add_body_line(app_ui_panel* panel, byte attr, cptr text);
bool app_ui_panel_add_row_ex(app_ui_panel* panel, s16b id, byte attr,
    byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr key, cptr label, cptr meta);
bool app_ui_panel_add_row(app_ui_panel* panel, s16b id, byte attr,
    bool enabled, bool selected, cptr key, cptr label, cptr meta);
bool app_ui_panel_add_detail_line_ex(app_ui_panel* panel, byte attr,
    byte story, cptr text);
bool app_ui_panel_add_detail_line(app_ui_panel* panel, byte attr, cptr text);
bool app_ui_panel_add_footer_action(app_ui_panel* panel, s16b id, byte attr,
    bool enabled, cptr key, cptr label);
bool app_ui_panel_add_tab(app_ui_panel* panel, s16b id, byte attr,
    bool active, cptr label);
bool app_ui_panel_add_document_text_ex(app_ui_scene* scene,
    app_ui_panel* panel, s16b row, s16b col, byte attr, byte story, cptr text);
bool app_ui_panel_add_document_text(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, byte attr, cptr text);
bool app_ui_panel_add_document_cell_ex(app_ui_scene* scene,
    app_ui_panel* panel, s16b row, s16b col, byte attr, char ch,
    byte terrain_attr, char terrain_char, byte story, byte width);
bool app_ui_panel_add_document_cursor(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, byte attr, byte width);
bool app_ui_panel_begin_rich_paragraph(app_ui_scene* scene,
    app_ui_panel* panel);
bool app_ui_panel_add_rich_text_ex(app_ui_scene* scene, app_ui_panel* panel,
    byte attr, byte story, cptr text);
bool app_ui_panel_add_rich_text(app_ui_scene* scene, app_ui_panel* panel,
    byte attr, cptr text);
bool app_ui_scene_from_interaction(app_ui_scene* scene,
    const app_interaction_state* interaction);
bool app_ui_scene_from_information_document(app_ui_scene* scene,
    const app_information_scene* information_scene);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_UI_H */
