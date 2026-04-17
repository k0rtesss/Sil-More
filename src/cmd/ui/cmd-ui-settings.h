#ifndef INCLUDED_CMD_UI_SETTINGS_H
#define INCLUDED_CMD_UI_SETTINGS_H

#include "angband.h"
#include "app/app-ui.h"
#include "platform-io.h"
#include "sdl-config.h"

#define SETTINGS_SDL_GET(name) platform_ ## name
#define SETTINGS_SDL_SET(name) platform_set_ ## name

typedef bool (*settings_bool_getter_fn)(void);
typedef void (*settings_bool_setter_fn)(bool value);
typedef int (*settings_int_getter_fn)(void);
typedef void (*settings_int_setter_fn)(int value);
typedef int (*settings_indexed_int_getter_fn)(int index);
typedef void (*settings_indexed_int_setter_fn)(int index, int value);

typedef struct settings_bool_binding {
    settings_bool_getter_fn get;
    settings_bool_setter_fn set;
} settings_bool_binding;

typedef struct settings_int_binding {
    settings_int_getter_fn get;
    settings_int_setter_fn set;
} settings_int_binding;

typedef struct settings_indexed_int_binding {
    settings_indexed_int_getter_fn get;
    settings_indexed_int_setter_fn set;
} settings_indexed_int_binding;

typedef enum settings_sdl_int_config {
    SETTINGS_SDL_INT_MAIN_VIEW_SCALE = 0,
    SETTINGS_SDL_INT_MAX_SCALE,
    SETTINGS_SDL_INT_MIN_TERMINAL_MODE,
    SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE,
    SETTINGS_SDL_INT_EFFECTIVE_AUX_VIEW_FONT_SIZE,
    SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE,
    SETTINGS_SDL_INT_EFFECTIVE_MENU_PANEL_FONT_SIZE,
    SETTINGS_SDL_INT_PLAIN_MENU_FONT_SIZE,
    SETTINGS_SDL_INT_EFFECTIVE_PLAIN_MENU_FONT_SIZE,
    SETTINGS_SDL_INT_BROWSER_MENU_FONT_SIZE,
    SETTINGS_SDL_INT_EFFECTIVE_BROWSER_MENU_FONT_SIZE,
    SETTINGS_SDL_INT_CHARACTER_SHEET_FONT_SIZE,
    SETTINGS_SDL_INT_EFFECTIVE_CHARACTER_SHEET_FONT_SIZE,
    SETTINGS_SDL_INT_MARGIN,
    SETTINGS_SDL_INT_CONFIG_MAX,
} settings_sdl_int_config;

typedef enum settings_sdl_bool_config {
    SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL = 0,
    SETTINGS_SDL_BOOL_FULLSCREEN,
    SETTINGS_SDL_BOOL_TILES,
    SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED,
    SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED,
    SETTINGS_SDL_BOOL_CONFIG_MAX,
} settings_sdl_bool_config;

typedef enum settings_sdl_pane_metric {
    SETTINGS_SDL_PANE_TYPE = 0,
    SETTINGS_SDL_PANE_WHERE,
    SETTINGS_SDL_PANE_ENABLED,
    SETTINGS_SDL_PANE_ROWS,
    SETTINGS_SDL_PANE_COLS,
    SETTINGS_SDL_PANE_FONT_SIZE,
    SETTINGS_SDL_PANE_EFFECTIVE_FONT_SIZE,
    SETTINGS_SDL_PANE_CURRENT_ROWS,
    SETTINGS_SDL_PANE_CURRENT_COLS,
    SETTINGS_SDL_PANE_METRIC_MAX,
} settings_sdl_pane_metric;

typedef struct settings_ui_layout {
    bool compact;
    bool narrow;
    int list_row_budget;
    int option_line_chars;
    int prompt_line_chars;
    int inset_prompt_line_chars;
    int pane_overview_label_chars;
    int pane_overview_value_chars;
    int supporting_font_label_chars;
    int supporting_layout_type_chars;
    int supporting_layout_where_chars;
} settings_ui_layout;

typedef struct settings_choice_entry {
    int id;
    char hotkey;
    cptr label;
    bool disabled;
} settings_choice_entry;

char settings_ui_read_key(bool scan);
const struct sdl_config* settings_sdl_default_config(void);
int settings_sdl_get_int_config(settings_sdl_int_config id);
void settings_sdl_set_int_config(settings_sdl_int_config id, int value);
bool settings_sdl_get_bool_config(settings_sdl_bool_config id);
void settings_sdl_set_bool_config(settings_sdl_bool_config id, bool value);
int settings_sdl_get_pane_metric(settings_sdl_pane_metric metric, int index);
void settings_sdl_set_pane_metric(settings_sdl_pane_metric metric, int index,
    int value);
const char* settings_sdl_config_path(void);
settings_ui_layout settings_ui_read_layout(void);
int settings_ui_list_visible_rows(const settings_ui_layout* layout,
    int top_reserved_rows, int bottom_reserved_rows, int min_rows);
void settings_ui_fit_text(char* buf, size_t buflen, cptr text, int max_chars);
cptr settings_ui_pick_label(int max_chars, cptr long_label,
    cptr medium_label, cptr short_label);
bool settings_ui_prompt_string(cptr title, cptr prompt, cptr note, char* buf,
    size_t len);
app_ui_panel* settings_browser_scene_begin_ex(app_ui_scene* scene, cptr title,
    cptr subtitle, int min_width_px, int width_cap_px);
app_ui_panel* settings_browser_scene_begin(app_ui_scene* scene, cptr title,
    cptr subtitle);
bool settings_browser_add_pair_row(app_ui_panel* panel, s16b id, byte attr,
    byte meta_attr, bool enabled, bool selected, cptr label, cptr meta);
int settings_choice_find_index_by_id(const settings_choice_entry* entries,
    int entry_count, int id);
int settings_choice_menu(cptr title, const settings_choice_entry* entries,
    int entry_count, int* highlight, int cancel_id);
void do_cmd_pane_settings(void);
void do_cmd_touch_pane_button_editor(bool* settings_changed);
void do_cmd_controller_settings(void);

#endif /* INCLUDED_CMD_UI_SETTINGS_H */
