/* File: cmd-ui-settings-panes.c */

#include "angband.h"
#include "app/app-command.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "sdl-config.h"
#include "sdl-main-internal.h"
#include "sound-config.h"
#include "cmd-ui-settings-internal.h"
#include "cmd-ui.h"
#include "cmd-ui-settings.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
void do_cmd_touch_pane_button_editor(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);
const char* settings_sdl_config_path(void);
static const char* settings_sdl_touch_slot_name(int idx);
static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen);
static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen);
static int settings_sdl_touch_binding(int panel, int slot);
static void settings_sdl_set_touch_binding(int panel, int slot, int binding);

const char* settings_sdl_config_path(void)
{
    return SETTINGS_SDL_GET(config_path)();
}

typedef struct settings_sdl_pane_overview {
    int main_view_scale;
    int max_scale;
    int min_terminal_mode;
    int aux_view_font_size;
    int effective_aux_view_font_size;
    int menu_panel_font_size;
    int effective_menu_panel_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    bool right_panes_enabled;
    bool bottom_panes_enabled;
    cptr config_path;
} settings_sdl_pane_overview;

static void settings_sdl_read_pane_overview(
    settings_sdl_pane_overview* overview)
{
    if (!overview)
        return;

    memset(overview, 0, sizeof(*overview));
    overview->main_view_scale = settings_sdl_get_int_config(
        SETTINGS_SDL_INT_MAIN_VIEW_SCALE);
    overview->max_scale = settings_sdl_get_int_config(SETTINGS_SDL_INT_MAX_SCALE);
    overview->min_terminal_mode = settings_sdl_get_int_config(
        SETTINGS_SDL_INT_MIN_TERMINAL_MODE);
    overview->aux_view_font_size = settings_sdl_get_int_config(
        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE);
    overview->effective_aux_view_font_size
        = settings_sdl_get_int_config(SETTINGS_SDL_INT_EFFECTIVE_AUX_VIEW_FONT_SIZE);
    overview->menu_panel_font_size = settings_sdl_get_int_config(
        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE);
    overview->effective_menu_panel_font_size
        = settings_sdl_get_int_config(SETTINGS_SDL_INT_EFFECTIVE_MENU_PANEL_FONT_SIZE);
    overview->margin = settings_sdl_get_int_config(SETTINGS_SDL_INT_MARGIN);
    overview->fullscreen = settings_sdl_get_bool_config(
        SETTINGS_SDL_BOOL_FULLSCREEN);
    overview->tiles = settings_sdl_get_bool_config(SETTINGS_SDL_BOOL_TILES);
    overview->right_panes_enabled = settings_sdl_get_bool_config(
        SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED);
    overview->bottom_panes_enabled = settings_sdl_get_bool_config(
        SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED);
    overview->config_path = settings_sdl_config_path();
}

static void format_font_size_value(char* buf, size_t buflen, int raw, int effective,
    int max_chars)
{
    char long_buf[24];
    char medium_buf[24];
    char short_buf[16];

    if (!buf || !buflen)
        return;

    if (raw > 0)
    {
        strnfmt(long_buf, sizeof(long_buf), "%d", raw);
        settings_ui_fit_text(buf, buflen, long_buf, max_chars);
        return;
    }

    strnfmt(long_buf, sizeof(long_buf), "auto (%d)", effective);
    strnfmt(medium_buf, sizeof(medium_buf), "auto %d", effective);
    strnfmt(short_buf, sizeof(short_buf), "a%d", effective);
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, long_buf, medium_buf, short_buf),
        max_chars);
}

static bool settings_browser_add_label_row(app_ui_panel* panel, s16b id,
    byte attr, bool enabled, bool selected, cptr label)
{
    return app_ui_panel_add_row_ex(panel, id, attr, attr, 0, '\0', enabled,
        selected, "", label ? label : "", "");
}

static void settings_ui_format_field(char* buf, size_t buflen, cptr text,
    bool selected)
{
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (selected)
        strnfmt(buf, buflen, "[%s]", text);
    else
        SDL_strlcpy(buf, text, buflen);
}

static void settings_ui_format_auto_value(char* buf, size_t buflen, int value,
    int max_chars)
{
    char raw_buf[16];
    char auto_long[16];
    char auto_short[8];

    if (!buf || !buflen)
        return;

    if (value > 0)
    {
        strnfmt(raw_buf, sizeof(raw_buf), "%d", value);
        settings_ui_fit_text(buf, buflen, raw_buf, max_chars);
        return;
    }

    SDL_strlcpy(auto_long, "auto", sizeof(auto_long));
    SDL_strlcpy(auto_short, "a", sizeof(auto_short));
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, auto_long, auto_long, auto_short),
        max_chars);
}

static const char* sdl_min_terminal_mode_label(int mode)
{
    return (mode == 1) ? "compact (50x18)" : "normal (80x24)";
}

static bool pane_settings_present_ui_scene(int k, bool settings_changed,
    const settings_sdl_pane_overview* overview)
{
    settings_ui_layout layout = settings_ui_read_layout();
    app_ui_scene scene;
    app_ui_panel* panel;
    char value_buf[32];
    char font_value[24];
    cptr config_label;
    int label_hint;

    if (!overview)
        return false;

    config_label = (overview->config_path && overview->config_path[0])
        ? overview->config_path
        : "sil_sdl.json";
    label_hint = layout.pane_overview_label_chars;

    panel = settings_browser_scene_begin_ex(&scene, "SDL Pane Settings",
        config_label, 1120, 2200);
    if (!panel)
        return false;

    if (k > 4)
        app_ui_panel_set_row_offset(panel, (s16b)(k - 4));

    strnfmt(value_buf, sizeof(value_buf), "%d", overview->main_view_scale);
    if (!settings_browser_add_pair_row(panel, 0, (k == 0) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 0,
            settings_ui_pick_label(label_hint,
                "Main View Scale (1-max) [Alt++/-]",
                "Main View Scale [Alt++/-]",
                "View Scale"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 1, (k == 1) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 1,
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(overview->min_terminal_mode)))
    {
        return false;
    }

    format_font_size_value(font_value, sizeof(font_value),
        overview->aux_view_font_size,
        overview->effective_aux_view_font_size,
        layout.pane_overview_value_chars);
    if (!settings_browser_add_pair_row(panel, 2, (k == 2) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 2,
            settings_ui_pick_label(label_hint,
                "Default Aux Font Size (0=auto, 8-48)",
                "Default Aux Font (0=auto)",
                "Aux Font"), font_value))
    {
        return false;
    }

    format_font_size_value(font_value, sizeof(font_value),
        overview->menu_panel_font_size,
        overview->effective_menu_panel_font_size,
        layout.pane_overview_value_chars);
    if (!settings_browser_add_pair_row(panel, 3, (k == 3) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 3,
            settings_ui_pick_label(label_hint,
                "Menu + Left Panel Font (0=auto, 8-64)",
                "Menu + Left Panel Font",
                "Menu Font"), font_value))
    {
        return false;
    }

    strnfmt(value_buf, sizeof(value_buf), "%d", overview->margin);
    if (!settings_browser_add_pair_row(panel, 4, (k == 4) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 4,
            settings_ui_pick_label(label_hint,
                "Margin (0-20)",
                "Margin",
                "Margin"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 5, (k == 5) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 5, "Fullscreen",
            overview->fullscreen ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 6, (k == 6) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 6, "Tiles",
            overview->tiles ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 7, (k == 7) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 7,
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            overview->right_panes_enabled ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 8, (k == 8) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 8,
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            overview->bottom_panes_enabled ? "yes" : "no"))
    {
        return false;
    }

    strnfmt(value_buf, sizeof(value_buf), "%d", get_supporting_pane_config_count());
    if (!settings_browser_add_pair_row(panel, 9, (k == 9) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 9,
            settings_ui_pick_label(label_hint,
                "View Pane Configuration",
                "Pane Configuration",
                "Pane Layout"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_label_row(panel, 10, (k == 10) ? TERM_L_BLUE
            : TERM_WHITE, true, k == 10,
            settings_ui_pick_label(label_hint,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts")))
    {
        return false;
    }

    if (!settings_browser_add_label_row(panel, 11, (k == 11) ? TERM_L_BLUE
            : TERM_WHITE, true, k == 11, settings_changed
            ? "Save Changes and Return"
            : "Return to Options Menu"))
    {
        return false;
    }

    if (settings_changed)
    {
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW,
            "Settings changed. Changes take effect immediately.");
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW,
            "Changes will be saved to the SDL config file on exit.");
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "4/6", "Set");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
        (k == 2) || (k == 3), "0", "Auto");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Enter", (k == 9 || k == 10) ? "Open" : "Accept");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    return ui_information_scene_present_ui(&scene);
}

void do_cmd_pane_settings(void)
{
    int k = 0;
    int n = 12; /* Total number of options */
    bool done = false;
    bool settings_changed = false;
    int dir;

    while (!done)
    {
        settings_sdl_pane_overview overview;
        cptr config_label;

        settings_sdl_read_pane_overview(&overview);
        config_label = (overview.config_path && overview.config_path[0])
            ? overview.config_path
            : "sil_sdl.json";

        if (!pane_settings_present_ui_scene(k, settings_changed, &overview))
        {
            done = true;
            continue;
        }

        /* Get key */
        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_key(false);
        inkey_set_cursor_hidden(false);
        
        /* Try to translate the key into a direction */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);
        
        /* Process input */
        switch (ch)
        {
        case ESCAPE:
        {
            /* Exit without needing to navigate to the bottom */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Enter activates the current option for actions; otherwise accept/exit. */
            if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }

            /* Save if changed, then exit */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }
        
        case '-':
        case '8':
        {
            /* Move up */
            k = (n + k - 1) % n;
            break;
        }
        
        case '2':
        {
            /* Move down */
            k = (k + 1) % n;
            break;
        }

        case '0':
        {
            if (k == 2)
            {
                if (overview.aux_view_font_size != 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE, 0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3)
            {
                if (overview.menu_panel_font_size != 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE, 0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else
            {
                bell("0 sets the selected font to auto");
            }
            break;
        }
        
        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == 1) /* Minimum Terminal Size */
            {
                settings_sdl_set_int_config(SETTINGS_SDL_INT_MIN_TERMINAL_MODE,
                    overview.min_terminal_mode == 0 ? 1 : 0);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 5) /* Fullscreen */
            {
                settings_sdl_set_bool_config(SETTINGS_SDL_BOOL_FULLSCREEN,
                    !overview.fullscreen);
                settings_changed = true;
            }
            else if (k == 6) /* Tiles */
            {
                settings_sdl_set_bool_config(SETTINGS_SDL_BOOL_TILES,
                    !overview.tiles);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 7) /* Enable Side Panes */
            {
                settings_sdl_set_bool_config(
                    SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED,
                    !overview.right_panes_enabled);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                settings_sdl_set_bool_config(
                    SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED,
                    !overview.bottom_panes_enabled);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == 11) /* Save/Return */
            {
                if (settings_changed)
                {
                    if (save_pane_config_to_json())
                    {
                        msg_format("Settings saved to %s", config_label);
                    }
                }
                done = true;
            }
            break;
        }
        
        case 'y':
        case '6':
        {
            /* Increase value or set to yes */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = overview.main_view_scale;
                if (val < overview.max_scale)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MAIN_VIEW_SCALE, val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (overview.min_terminal_mode != 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MIN_TERMINAL_MODE, 0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = overview.aux_view_font_size;
                if (val == 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE,
                        overview.effective_aux_view_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 48)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE, val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = overview.menu_panel_font_size;
                if (val == 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE,
                        overview.effective_menu_panel_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 64)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE, val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = overview.margin;
                if (val < 20)
                {
                    settings_sdl_set_int_config(SETTINGS_SDL_INT_MARGIN,
                        val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                if (!overview.fullscreen)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_FULLSCREEN, true);
                    settings_changed = true;
                }
            }
            else if (k == 6) /* Tiles */
            {
                if (!overview.tiles)
                {
                    settings_sdl_set_bool_config(SETTINGS_SDL_BOOL_TILES, true);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 7) /* Enable Side Panes */
            {
                if (!overview.right_panes_enabled)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED, true);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                if (!overview.bottom_panes_enabled)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED, true);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }
        
        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = overview.main_view_scale;
                if (val > 1)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MAIN_VIEW_SCALE, val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (overview.min_terminal_mode != 1)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MIN_TERMINAL_MODE, 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = overview.aux_view_font_size;
                if (val == 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE,
                        overview.effective_aux_view_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE, val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = overview.menu_panel_font_size;
                if (val == 0)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE,
                        overview.effective_menu_panel_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    settings_sdl_set_int_config(
                        SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE, val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = overview.margin;
                if (val > 0)
                {
                    settings_sdl_set_int_config(SETTINGS_SDL_INT_MARGIN,
                        val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                if (overview.fullscreen)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_FULLSCREEN, false);
                    settings_changed = true;
                }
            }
            else if (k == 6) /* Tiles */
            {
                if (overview.tiles)
                {
                    settings_sdl_set_bool_config(SETTINGS_SDL_BOOL_TILES,
                        false);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 7) /* Enable Side Panes */
            {
                if (overview.right_panes_enabled)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED, false);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                if (overview.bottom_panes_enabled)
                {
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED, false);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }
        
        default:
        {
            bell("Illegal command for pane settings!");
            break;
        }
        }
    }
}


typedef struct settings_sdl_pane_state {
    enum pane_type type;
    enum pane_placement where;
    bool enabled;
    int rows;
    int cols;
    int font_size;
    int effective_font_size;
    int current_rows;
    int current_cols;
} settings_sdl_pane_state;

static void settings_sdl_read_pane_state(int idx,
    settings_sdl_pane_state* pane_state);

static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_get_pane_metric(
            SETTINGS_SDL_PANE_TYPE, i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    if (pane_count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin(&scene,
            "Supporting Pane Fonts", "");

        if (panel)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "No supporting panes are configured.");
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                true, "Esc", "Back");
            (void)ui_information_scene_present_ui(&scene);
            (void)settings_ui_read_key(false);
        }
        return;
    }

    {
        int sel = 0;
        bool done = false;
        bool changed = false;
        int dir;

        while (!done)
        {
            settings_ui_layout layout = settings_ui_read_layout();
            app_ui_scene scene;
            app_ui_panel* panel = settings_browser_scene_begin(&scene,
                "Supporting Pane Fonts", "");

            if (!panel)
            {
                done = true;
            }
            else
            {
                if (sel > 4)
                    app_ui_panel_set_row_offset(panel, (s16b)(sel - 4));
                for (int i = 0; i < pane_count; i++)
                {
                    int idx = pane_indices[i];
                    settings_sdl_pane_state pane_state;
                    enum pane_type type;
                    bool enabled;
                    int raw_font;
                    int effective_font;
                    byte a;
                    char label_buf[48];
                    char font_value[24];
                    const char* type_label;

                    settings_sdl_read_pane_state(idx, &pane_state);
                    type = pane_state.type;
                    enabled = pane_state.enabled;
                    raw_font = pane_state.font_size;
                    effective_font = pane_state.effective_font_size;
                    a = (i == sel) ? TERM_L_BLUE
                        : (enabled ? TERM_WHITE : TERM_SLATE);
                    type_label = settings_ui_pick_label(
                        layout.supporting_font_label_chars,
                        pane_type_name(type), pane_type_name(type),
                        pane_type_short_name(type));

                    format_font_size_value(font_value, sizeof(font_value),
                        raw_font, effective_font,
                        layout.pane_overview_value_chars);
                    strnfmt(label_buf, sizeof(label_buf), "%s %s",
                        type_label, enabled ? "on" : "off");
                    if (!settings_browser_add_pair_row(panel, (s16b)i, a,
                            TERM_SLATE, true, i == sel, label_buf,
                            font_value))
                    {
                        done = true;
                        break;
                    }
                }

                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Changes apply immediately.");
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                    true, "8/2", "Move");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE,
                    true, "4/6", "Set");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
                    true, "0", "Auto");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE,
                    true, "Esc", "Back");
                if (!done && !ui_information_scene_present_ui(&scene))
                    done = true;
            }

            inkey_set_cursor_hidden(true);
            char ch = settings_ui_read_key(false);
            inkey_set_cursor_hidden(false);

            dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);

            switch (ch)
            {
            case ESCAPE:
            case '\n':
            case '\r':
                done = true;
                break;

            case '-':
            case '8':
                sel = (pane_count + sel - 1) % pane_count;
                break;

            case '2':
                sel = (sel + 1) % pane_count;
                break;

            case '0':
            {
                int idx = pane_indices[sel];
                if (settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_FONT_SIZE,
                        idx) != 0)
                {
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_FONT_SIZE,
                        idx, 0);
                    changed = true;
                    sdl_apply_config();
                }
                break;
            }

            case 'n':
            case '4':
            case 'y':
            case '6':
            {
                int idx = pane_indices[sel];
                int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
                int value = settings_sdl_get_pane_metric(
                    SETTINGS_SDL_PANE_FONT_SIZE, idx);

                if (value == 0)
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_FONT_SIZE,
                        idx, settings_sdl_get_pane_metric(
                                 SETTINGS_SDL_PANE_EFFECTIVE_FONT_SIZE, idx));
                else
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_FONT_SIZE,
                        idx, value + delta);

                changed = true;
                sdl_apply_config();
                break;
            }

            default:
                bell("Illegal command for pane font editor!");
                break;
            }
        }

        if (changed && settings_changed)
            *settings_changed = true;
    }
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
    case PLACE_DOUBLE_RIGHT: return "DR";
    case PLACE_DOUBLE_LEFT: return "DL";
    case PLACE_BOTTOM: return "BOT";
    default: return "?";
    }
}

static void settings_sdl_read_pane_state(int idx,
    settings_sdl_pane_state* pane_state)
{
    if (!pane_state)
        return;

    memset(pane_state, 0, sizeof(*pane_state));
    pane_state->type = (enum pane_type)settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_TYPE, idx);
    pane_state->where = (enum pane_placement)settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_WHERE, idx);
    pane_state->enabled = settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_ENABLED, idx) != 0;
    pane_state->rows = settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_ROWS,
        idx);
    pane_state->cols = settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_COLS,
        idx);
    pane_state->font_size = settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_FONT_SIZE, idx);
    pane_state->effective_font_size = settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_EFFECTIVE_FONT_SIZE, idx);
    pane_state->current_rows = settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_CURRENT_ROWS, idx);
    pane_state->current_cols = settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_CURRENT_COLS, idx);
}

static int get_supporting_pane_config_count(void)
{
    int count = 0;
    int total = get_pane_config_count();
    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_get_pane_metric(
            SETTINGS_SDL_PANE_TYPE, i);
        if (type != PANE_MAIN)
            count++;
    }
    return count;
}

static int supporting_pane_master_idx(const int* pane_indices, int pane_count,
    enum pane_placement where)
{
    int first_idx = -1;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        if ((enum pane_placement)settings_sdl_get_pane_metric(
                SETTINGS_SDL_PANE_WHERE, idx)
            != where)
            continue;
        if (first_idx < 0)
            first_idx = idx;
        if (settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_ENABLED, idx) != 0)
            return idx;
    }

    return first_idx;
}

static bool supporting_pane_rows_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_WHERE, idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (where == PLACE_BOTTOM && idx != master_idx);
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)settings_sdl_get_pane_metric(
        SETTINGS_SDL_PANE_WHERE, idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
        || (*field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx)))
    {
        *field = (*field + 1) % 4;
    }
}

static bool supporting_pane_normalize_shared_sizes(const int* pane_indices, int pane_count)
{
    bool changed = false;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        enum pane_placement where = (enum pane_placement)settings_sdl_get_pane_metric(
            SETTINGS_SDL_PANE_WHERE, idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

        if (where == PLACE_BOTTOM && idx != master_idx
            && settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_ROWS, idx) != 0)
        {
            settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_ROWS, idx, 0);
            changed = true;
        }
        else if (pane_placement_is_side(where) && idx != master_idx
            && settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_COLS, idx) != 0)
        {
            settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_COLS, idx, 0);
            changed = true;
        }
    }

    return changed;
}

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_get_pane_metric(
            SETTINGS_SDL_PANE_TYPE, i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    int sel = 0;
    int field = 0; /* 0 = enabled, 1 = where, 2 = rows, 3 = cols */
    bool done = false;
    bool changed = false;
    int dir;

    if (pane_count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin(&scene,
            "Supporting Pane Layout", "");

        if (panel)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "No supporting panes are configured.");
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                true, "Esc", "Back");
            (void)ui_information_scene_present_ui(&scene);
            (void)settings_ui_read_key(false);
        }
        return;
    }

    if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
    {
        changed = true;
        sdl_apply_config();
    }
    supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            "Supporting Pane Layout", "", 1180, 2200);

        if (!panel)
        {
            done = true;
        }
        else
        {
                if (sel > 4)
                    app_ui_panel_set_row_offset(panel, (s16b)(sel - 4));
                for (int i = 0; i < pane_count; i++)
                {
                    int idx = pane_indices[i];
                    settings_sdl_pane_state pane_state;
                    enum pane_type type;
                    enum pane_placement where;
                    int master_idx;
                    bool enabled;
                    bool rows_locked = supporting_pane_rows_locked(pane_indices,
                        pane_count, idx);
                    bool cols_locked = supporting_pane_cols_locked(pane_indices,
                        pane_count, idx);
                    int rows;
                    int cols;
                    byte a;
                    char type_buf[24];
                    char enabled_field[12];
                    char where_field[24];
                    char rows_value[16];
                    char rows_field[20];
                    char cols_value[16];
                    char cols_field[20];
                    char line_buf[128];
                    const char* type_label;
                    const char* where_label;

                    settings_sdl_read_pane_state(idx, &pane_state);
                    type = pane_state.type;
                    where = pane_state.where;
                    enabled = pane_state.enabled;
                    rows = pane_state.rows;
                    cols = pane_state.cols;
                    master_idx = supporting_pane_master_idx(pane_indices,
                        pane_count, where);
                    a = (i == sel) ? TERM_L_BLUE
                        : (enabled ? TERM_WHITE : TERM_SLATE);
                    type_label = settings_ui_pick_label(
                        layout.supporting_layout_type_chars,
                        pane_type_name(type), pane_type_name(type),
                        pane_type_short_name(type));
                    where_label = settings_ui_pick_label(
                        layout.supporting_layout_where_chars,
                        pane_placement_name(where), pane_placement_name(where),
                        pane_where_short_name(where));

                    settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                        layout.supporting_layout_type_chars);
                    settings_ui_format_field(enabled_field,
                        sizeof(enabled_field), enabled ? "on" : "off",
                        i == sel && field == 0);
                    settings_ui_format_field(where_field, sizeof(where_field),
                        where_label, i == sel && field == 1);

                    if (rows_locked)
                    {
                        int shared_rows = (master_idx >= 0)
                            ? settings_sdl_get_pane_metric(
                                  SETTINGS_SDL_PANE_ROWS, master_idx)
                            : rows;
                        settings_ui_format_auto_value(rows_value,
                            sizeof(rows_value), shared_rows, 4);
                    }
                    else
                    {
                        settings_ui_format_auto_value(rows_value,
                            sizeof(rows_value), rows, 4);
                    }
                    settings_ui_format_field(rows_field, sizeof(rows_field),
                        rows_value, !rows_locked && i == sel && field == 2);

                    if (cols_locked)
                    {
                        int shared_cols = (master_idx >= 0)
                            ? settings_sdl_get_pane_metric(
                                  SETTINGS_SDL_PANE_COLS, master_idx)
                            : cols;
                        settings_ui_format_auto_value(cols_value,
                            sizeof(cols_value), shared_cols, 4);
                    }
                    else
                    {
                        settings_ui_format_auto_value(cols_value,
                            sizeof(cols_value), cols, 4);
                    }
                    settings_ui_format_field(cols_field, sizeof(cols_field),
                        cols_value, !cols_locked && i == sel && field == 3);

                    strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s",
                        type_buf, where_field, enabled_field, rows_field,
                        cols_field);
                    if (!settings_browser_add_label_row(panel, (s16b)i, a,
                            true, i == sel, line_buf))
                    {
                        done = true;
                        break;
                    }
                }

                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Each side slot shares cols with its first pane.");
                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Bottom panes share rows.");
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                    true, "8/2", "Move");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE,
                    true, "Space", "Field");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
                    true, "4/6", "Set");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE,
                    true, "0", "Auto");
                (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE,
                    true, "Esc", "Back");
                if (!done && !ui_information_scene_present_ui(&scene))
                    done = true;
        }
        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_key(false);
        inkey_set_cursor_hidden(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case ' ':
        case 't':
        case '5':
            field = (field + 1) % 4;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '-':
        case '8':
            sel = (pane_count + sel - 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '2':
            sel = (sel + 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '0':
        {
            int idx = pane_indices[sel];
            if (field == 0 || field == 1)
            {
                bell("Use 4/6 to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared for bottom panes");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side slot");
                break;
            }

            if (field == 2)
                settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_ROWS, idx, 0);
            else
                settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_COLS, idx, 0);

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        case 'n':
        case '4':
        case 'y':
        case '6':
        {
            int idx = pane_indices[sel];
            int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
            enum pane_type type = (enum pane_type)settings_sdl_get_pane_metric(
                SETTINGS_SDL_PANE_TYPE, idx);
            enum pane_placement where = (enum pane_placement)settings_sdl_get_pane_metric(
                SETTINGS_SDL_PANE_WHERE, idx);

            if (field == 0)
            {
                settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_ENABLED, idx,
                    delta > 0 ? 1 : 0);
            }
            else if (field == 1)
            {
                settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_WHERE, idx,
                    pane_next_allowed_placement(type, where, delta));
            }
            else if (field == 2)
            {
                int rows = settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_ROWS,
                    idx);

                if (supporting_pane_rows_locked(pane_indices, pane_count, idx))
                {
                    bell("Rows are shared for bottom panes");
                    break;
                }
                if (rows == 0)
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_ROWS, idx,
                        settings_sdl_get_pane_metric(
                            SETTINGS_SDL_PANE_CURRENT_ROWS, idx));
                else
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_ROWS, idx,
                        rows + delta);
            }
            else
            {
                int cols = settings_sdl_get_pane_metric(SETTINGS_SDL_PANE_COLS,
                    idx);

                if (supporting_pane_cols_locked(pane_indices, pane_count, idx))
                {
                    bell("Cols are shared within each side slot");
                    break;
                }
                if (cols == 0)
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_COLS, idx,
                        settings_sdl_get_pane_metric(
                            SETTINGS_SDL_PANE_CURRENT_COLS, idx));
                else
                    settings_sdl_set_pane_metric(SETTINGS_SDL_PANE_COLS, idx,
                        cols + delta);
            }

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        default:
            bell("Illegal command for pane layout editor!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        settings_sdl_touch_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    binding_action_label(binding, buf, buflen);
}

static const char* settings_sdl_touch_slot_name(int idx)
{
    return SETTINGS_SDL_GET(touch_pane_slot_name)(idx);
}

static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen)
{
    SETTINGS_SDL_GET(touch_pane_panel_name)(panel, buf, buflen);
}

static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen)
{
    SETTINGS_SDL_GET(touch_pane_button_label_for_panel)(panel, slot, buf,
        buflen);
}

static int settings_sdl_touch_binding(int panel, int slot)
{
    return SETTINGS_SDL_GET(touch_pane_binding_for_panel)(panel, slot);
}

static void settings_sdl_set_touch_binding(int panel, int slot, int binding)
{
    SETTINGS_SDL_SET(touch_pane_binding_for_panel)(panel, slot, binding);
}

static void settings_sdl_set_touch_button_label(int panel, int slot,
    cptr label)
{
    SETTINGS_SDL_SET(touch_pane_button_label_for_panel)(panel, slot, label);
}

static void settings_sdl_set_touch_panel_name(int panel, cptr name)
{
    SETTINGS_SDL_SET(touch_pane_panel_name)(panel, name);
}

static int settings_sdl_touch_default_binding(int panel, int slot)
{
    const struct sdl_config* defaults = settings_sdl_default_config();

    if (panel < 0 || panel >= SDL_TOUCH_PANE_PANEL_COUNT
        || slot < 0 || slot >= SDL_TOUCH_PANE_BUTTON_COUNT)
    {
        return GAMEPAD_BIND_NONE;
    }

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? defaults->touch_pane_second_bindings[slot]
        : defaults->touch_pane_bindings[slot];
}

void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    const int list_start_row = 5;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 5);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= SDL_TOUCH_PANE_BUTTON_COUNT)
            highlight = SDL_TOUCH_PANE_BUTTON_COUNT - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;

        app_ui_scene scene;
        app_ui_panel* ui_panel = settings_browser_scene_begin_ex(&scene,
            "Touch Settings", "", 1100, 2200);

        if (!ui_panel)
        {
            done = true;
        }
        else
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            settings_sdl_touch_panel_name(panel, panel_name,
                sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name,
                (panel == SDL_TOUCH_PANE_PANEL_SECOND)
                    ? " (empty = main panel)"
                    : "");
            app_ui_panel_set_subtitle(ui_panel, TERM_SLATE, info_buf);
            if (top > 0)
                app_ui_panel_set_row_offset(ui_panel, (s16b)top);

            for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++)
            {
                char action_buf[80];
                char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
                char left_buf[64];
                byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

                settings_sdl_touch_button_label(panel, i,
                    label_buf, sizeof(label_buf));
                touch_pane_action_label_for_panel(panel,
                    settings_sdl_touch_binding(panel, i),
                    action_buf, sizeof(action_buf));

                if (label_buf[0])
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s %s",
                        settings_sdl_touch_slot_name(i), label_buf);
                }
                else
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s",
                        settings_sdl_touch_slot_name(i));
                }

                if (!settings_browser_add_pair_row(ui_panel, (s16b)i, a,
                        TERM_SLATE, true, i == highlight, left_buf,
                        action_buf))
                {
                    done = true;
                    break;
                }
            }

            (void)app_ui_panel_add_footer_action(ui_panel, 1, TERM_WHITE,
                true, "8/2", "Move");
            (void)app_ui_panel_add_footer_action(ui_panel, 2, TERM_WHITE,
                true, "4/6", "Action");
            (void)app_ui_panel_add_footer_action(ui_panel, 3, TERM_WHITE,
                true, "Tab", "Panel");
            (void)app_ui_panel_add_footer_action(ui_panel, 4, TERM_WHITE,
                true, "l/p", "Rename");
            (void)app_ui_panel_add_footer_action(ui_panel, 5, TERM_WHITE,
                true, "r", "Reset");
            (void)app_ui_panel_add_footer_action(ui_panel, 6, TERM_WHITE,
                true, "R", "Reset all");
            (void)app_ui_panel_add_footer_action(ui_panel, 7, TERM_WHITE,
                true, "Esc", "Back");
            if (!done && !ui_information_scene_present_ui(&scene))
                done = true;
        }

        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_key(false);
        inkey_set_cursor_hidden(false);

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (SDL_TOUCH_PANE_BUTTON_COUNT + highlight - 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case '2':
            highlight = (highlight + 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case 'n':
        case '4':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, settings_sdl_touch_binding(panel, highlight));
            idx = (choice_count + idx - 1) % choice_count;
            settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, settings_sdl_touch_binding(panel, highlight));
            idx = (idx + 1) % choice_count;
            settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'l':
        case 'L':
        {
            char prompt[96];
            char prompt_long[96];
            char prompt_medium[96];
            char prompt_short[64];
            char current_label[SDL_TOUCH_PANE_LABEL_LEN];
            char new_label[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            settings_sdl_touch_button_label(panel, highlight, current_label, sizeof(current_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "New label for %s (blank = use key label): ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "New label for %s (blank = default): ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(layout.prompt_line_chars,
                    prompt_long, prompt_medium, prompt_short));
            strnfmt(current_buf, sizeof(current_buf), "Current label: %s", current_label);
            new_label[0] = '\0';
            if (settings_ui_prompt_string("Touch Settings", prompt,
                    current_buf, new_label, sizeof(new_label)))
            {
                settings_sdl_set_touch_button_label(panel, highlight,
                    new_label);
                changed = true;
            }
            break;
        }

        case '\t':
            panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                ? SDL_TOUCH_PANE_PANEL_SECOND
                : SDL_TOUCH_PANE_PANEL_MAIN;
            break;

        case 'p':
        case 'P':
        {
            char prompt[96];
            char current_name[SDL_TOUCH_PANE_LABEL_LEN];
            char new_name[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            settings_sdl_touch_panel_name(panel, current_name, sizeof(current_name));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(layout.prompt_line_chars,
                    "Name for current panel (blank = default): ",
                    "Panel name (blank = default): ",
                    "Panel name: "));
            strnfmt(current_buf, sizeof(current_buf), "Current panel name: %s", current_name);
            new_name[0] = '\0';
            if (settings_ui_prompt_string("Touch Settings", prompt,
                    current_buf, new_name, sizeof(new_name)))
            {
                settings_sdl_set_touch_panel_name(panel, new_name);
                changed = true;
            }
            break;
        }

        case 'r':
            settings_sdl_set_touch_binding(panel, highlight,
                settings_sdl_touch_default_binding(panel, highlight));
            clear_sdl_touch_pane_button_label_for_panel(panel, highlight);
            changed = true;
            break;

        case 'R':
            sdl_touch_pane_reset_bindings_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }
}

