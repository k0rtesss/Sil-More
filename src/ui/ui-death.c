/* File: ui/ui-death.c */

#include "angband.h"

#include "blitz.h"
#include "log/log.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "player/killer.h"
#include "player/player-oaths.h"
#include "score/score_ui.h"
#include "supplies.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-death.h"
#include "ui/ui-information-scene.h"

static bool death_present_ui_page(const app_ui_scene* scene, bool* out_escape)
{
    int key;

    if (out_escape)
        *out_escape = false;
    if (!scene)
        return false;
    if (!ui_information_scene_present_ui(scene))
        return false;

    key = ui_information_scene_wait_key_nonrepeat();
    if (out_escape)
        *out_escape = (key == ESCAPE);
    return true;
}

static bool death_add_body_line(app_ui_panel* panel, byte attr, cptr text)
{
    if (!panel || !text || !text[0])
        return true;

    return app_ui_panel_add_body_line(panel, attr, text);
}

static app_ui_panel* death_begin_item_list_scene(app_ui_scene* scene,
    cptr title)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    if (title && title[0])
        app_ui_panel_set_title(panel, TERM_WHITE, title);

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "Any key", "Continue");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Exit");

    return panel;
}

static byte death_item_row_attr(const object_type* o_ptr, bool empty_slot)
{
    if (!o_ptr || empty_slot || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr, object_default_text_color(o_ptr));
}

static void death_format_weight(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    int weight = 0;

    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!show_weights || !o_ptr || !o_ptr->k_idx)
        return;

    weight = o_ptr->weight * o_ptr->number;
    strnfmt(buf, buf_size, "%2d.%1d lb", weight / 10, weight % 10);
}

static void death_format_total_weight(char* buf, size_t buf_size,
    int total_weight)
{
    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!show_weights)
        return;

    strnfmt(buf, buf_size, "%2d.%1d lb", total_weight / 10,
        total_weight % 10);
}

static void death_format_meta_with_tag(char* buf, size_t buf_size,
    cptr weight_text, cptr tag_text)
{
    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (weight_text && weight_text[0] && tag_text && tag_text[0])
    {
        strnfmt(buf, buf_size, "%s  %s", weight_text, tag_text);
    }
    else if (weight_text && weight_text[0])
    {
        SDL_strlcpy(buf, weight_text, buf_size);
    }
    else if (tag_text && tag_text[0])
    {
        SDL_strlcpy(buf, tag_text, buf_size);
    }
}

static bool death_append_armour_total_rows(app_ui_panel* panel,
    int armour_weight)
{
    char buf[APP_UI_LABEL_MAX];

    if (!panel || armour_weight <= 0)
        return true;

    if (!app_ui_panel_add_row(panel, (s16b)-4001, TERM_L_DARK, true, false, "",
            "--------", ""))
    {
        return false;
    }

    strnfmt(buf, sizeof(buf), "armour: %3d.%1d lb",
        armour_weight / 10, armour_weight % 10);
    return app_ui_panel_add_row(panel, (s16b)-4002, TERM_SLATE, true, false,
        "", buf, "");
}

static byte death_status_title_attr(void)
{
    if (p_ptr->morgoth_slain && !p_ptr->escaped)
        return TERM_YELLOW;

    return TERM_L_BLUE;
}

static cptr death_status_title_text(void)
{
    if (p_ptr->escaped)
    {
        if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
            return "You have escaped and kept your oath";

        return "You have escaped";
    }
    if (p_ptr->morgoth_slain)
        return "You are acclaimed as the Slayer of Morgoth";

    return "You have been slain";
}

static void death_format_score_day(const high_score* score, char* out,
    size_t out_size)
{
    const char* when;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    when = score->day;
    while (*when && isspace((unsigned char)*when))
        when++;

    if ((*when == '@') && strlen(when) == 9)
    {
        char month_num[3];
        char month[4];

        SDL_strlcpy(month_num, when + 5, sizeof(month_num));
        atomonth(atoi(month_num), month);
        if (*(when + 7) == '0')
            strnfmt(out, out_size, "%.1s %.3s %.4s", when + 8, month,
                when + 1);
        else
            strnfmt(out, out_size, "%.2s %.3s %.4s", when + 7, month,
                when + 1);
        return;
    }

    SDL_strlcpy(out, when, out_size);
}

static void death_build_name_line(const high_score* score, char* out,
    size_t out_size)
{
    int ph;
    char score_commas[16];
    const char* suffix = "";

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    ph = atoi(score->p_h);
    if (ph >= 0 && ph < z_info->c_max)
        suffix = c_name + c_info[ph].alt_name;

    comma_number(score_commas, score_points(score));
    strnfmt(out, out_size, "%s%s  [%s pts]", score->who, suffix, score_commas);
}

static bool death_build_curse_line(const high_score* score, char* out,
    size_t out_size, byte* out_attr)
{
    int curses;

    if (out_attr)
        *out_attr = TERM_WHITE;
    if (!out || !out_size)
        return false;

    out[0] = '\0';
    if (!score || !scores_version_has_curses(score_file_global_ctx()))
        return false;

    curses = parse_score_int(score->pts, sizeof(score->pts), 0);
    if (curses > 0)
    {
        strnfmt(out, out_size, "%d curse%s endured", curses,
            (curses == 1) ? "" : "s");
        if (out_attr)
            *out_attr = TERM_L_RED;
        return true;
    }
    if (curses < 0)
    {
        strnfmt(out, out_size, "%d blessing%s claimed", -curses,
            (curses == -1) ? "" : "s");
        if (out_attr)
            *out_attr = TERM_L_GREEN;
        return true;
    }

    return false;
}

static void death_build_outcome_line(const high_score* score, char* out,
    size_t out_size)
{
    int silmarils;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    silmarils = atoi(score->silmarils);
    if (score->escaped[0] == 't')
    {
        SDL_strlcpy(out, "Escaped the iron hells", out_size);
        if ((score->morgoth_slain[0] == 't') || (silmarils > 0))
            SDL_strlcat(out, " and brought back the light of Valinor",
                out_size);
        else
            SDL_strlcat(out, " empty-handed", out_size);
        return;
    }

    if (score->morgoth_slain[0] == 't')
    {
        strnfmt(out, out_size, "Victorious over Morgoth's illusion (%s)",
            score->how);
        return;
    }

    strnfmt(out, out_size, "Slain by %s", score->how);
    if (silmarils > 0)
        SDL_strlcat(out, " during a daring escape", out_size);
}

static void death_build_run_line(const high_score* score, char* out,
    size_t out_size)
{
    char turns_commas[16];
    char depth_commas[16];
    char day[32];
    int turns;
    int depth;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    turns = atoi(score->turns);
    depth = atoi(score->cur_dun) * 50;
    comma_number(turns_commas, turns);
    comma_number(depth_commas, depth);
    death_format_score_day(score, day, sizeof(day));

    if (day[0])
    {
        strnfmt(out, out_size, "Depth %s ft, after %s turns. (%s)",
            depth_commas, turns_commas, day);
    }
    else
    {
        strnfmt(out, out_size, "Depth %s ft, after %s turns.",
            depth_commas, turns_commas);
    }
}

static bool death_build_trophy_line(const high_score* score, char* out,
    size_t out_size)
{
    int silmarils;

    if (!out || !out_size)
        return false;

    out[0] = '\0';
    if (!score)
        return false;

    silmarils = atoi(score->silmarils);
    if (score->morgoth_slain[0] == 't')
        SDL_strlcat(out, "V", out_size);
    if (silmarils >= 1)
        SDL_strlcat(out, out[0] ? "  *" : "*", out_size);
    if (silmarils >= 2)
        SDL_strlcat(out, " *", out_size);
    if (silmarils >= 3)
        SDL_strlcat(out, " *", out_size);

    if (!out[0])
        return false;

    {
        char decorated[APP_UI_TEXT_MAX];

        strnfmt(decorated, sizeof(decorated), "Trophies: %s", out);
        SDL_strlcpy(out, decorated, out_size);
    }
    return true;
}

static bool death_build_inventory_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    int i;
    int z = 0;
    bool include_supplies;

    panel = death_begin_item_list_scene(scene, "You are carrying");
    if (!panel)
        return false;

    include_supplies = !inventory_menu_get_include_equip()
        && supplies_visible_for_current_filter();

    for (i = 0; i < INVEN_PACK; i++)
    {
        if (inventory[i].k_idx)
            z = i + 1;
    }

    if (include_supplies)
    {
        char label_buf[APP_UI_LABEL_MAX];
        char meta_buf[APP_UI_META_MAX];
        char weight_buf[16];
        char tag_buf[8];
        char label = supplies_label_char();
        int slot = supplies_virtual_slot();

        format_supply_summary(label_buf, sizeof(label_buf));
        if (!label && slot >= 0)
            label = index_to_label(slot);
        if (label)
            strnfmt(tag_buf, sizeof(tag_buf), "(%c)", label);
        else
            tag_buf[0] = '\0';

        death_format_total_weight(weight_buf, sizeof(weight_buf),
            supplies_total_weight());
        death_format_meta_with_tag(meta_buf, sizeof(meta_buf), weight_buf,
            tag_buf);

        if (!app_ui_panel_add_row_ex(panel, (s16b)SUPPLIES_INDEX, TERM_L_WHITE,
                TERM_L_WHITE, 0, '\0', true, false, "", label_buf, meta_buf))
        {
            return false;
        }
    }

    for (i = 0; i < z; i++)
    {
        object_type* o_ptr = &inventory[i];
        char label_buf[APP_UI_LABEL_MAX];
        char meta_buf[APP_UI_META_MAX];
        char weight_buf[16];
        char tag_buf[8];
        byte attr;

        if (!item_tester_okay(o_ptr))
            continue;

        object_desc(label_buf, sizeof(label_buf), o_ptr, true, 3);
        death_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
        strnfmt(tag_buf, sizeof(tag_buf), "(%c)", index_to_label(i));
        death_format_meta_with_tag(meta_buf, sizeof(meta_buf), weight_buf,
            tag_buf);
        attr = death_item_row_attr(o_ptr, false);

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, attr,
                object_attr(o_ptr), object_char(o_ptr), true, false, "",
                label_buf, meta_buf))
        {
            return false;
        }
    }

    if (panel->row_count == 0)
    {
        return app_ui_panel_add_row(panel, (s16b)-1, TERM_SLATE, true, false,
            "", "Nothing carried.", "");
    }

    return true;
}

static bool death_build_equipment_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    int i;
    int armour_weight = 0;

    panel = death_begin_item_list_scene(scene, "You are using");
    if (!panel)
        return false;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        bool empty_slot = !o_ptr->k_idx;
        char desc_buf[96];
        char label_buf[APP_UI_LABEL_MAX];
        char meta_buf[APP_UI_META_MAX];
        char weight_buf[16];
        char tag_buf[8];
        byte attr;
        byte meta_attr;
        byte icon_attr = 0;
        char icon_char = '\0';

        if (!item_tester_okay(o_ptr))
            continue;

        if (empty_slot)
        {
            SDL_strlcpy(desc_buf, describe_empty_slot(i), sizeof(desc_buf));
            if (i == INVEN_QUIVER2)
            {
                SDL_strlcat(desc_buf, " (keeps passive bonuses)",
                    sizeof(desc_buf));
            }
        }
        else
        {
            object_desc(desc_buf, sizeof(desc_buf), o_ptr, true, 3);
            icon_attr = object_attr(o_ptr);
            icon_char = object_char(o_ptr);
        }

        strnfmt(label_buf, sizeof(label_buf), "%s: %s", mention_use(i),
            desc_buf);
        strnfmt(tag_buf, sizeof(tag_buf), "(%c)", index_to_label(i));
        death_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
        death_format_meta_with_tag(meta_buf, sizeof(meta_buf),
            empty_slot ? "" : weight_buf, tag_buf);

        attr = death_item_row_attr(o_ptr, empty_slot);
        meta_attr = attr;
        if (!empty_slot && show_weights && o_ptr->weight
            && i >= INVEN_BODY && i <= INVEN_FEET)
        {
            meta_attr = TERM_SLATE;
            armour_weight += o_ptr->weight * o_ptr->number;
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, meta_attr,
                icon_attr, icon_char, true, false, "", label_buf, meta_buf))
        {
            return false;
        }
    }

    if (panel->row_count == 0)
    {
        return app_ui_panel_add_row(panel, (s16b)-1, TERM_SLATE, true, false,
            "", "Nothing equipped.", "");
    }

    return death_append_armour_total_rows(panel, armour_weight);
}

static bool death_build_final_menu_scene(app_ui_scene* scene,
    const high_score* score, int highlight, bool morgoth_victory)
{
    app_ui_panel* panel;
    char summary[APP_UI_TEXT_MAX];
    char outcome[APP_UI_TEXT_MAX];
    char run_line[APP_UI_TEXT_MAX];
    char trophy_line[APP_UI_TEXT_MAX];
    char curse_line[APP_UI_TEXT_MAX];
    byte curse_attr = TERM_WHITE;
    const char* option_a = morgoth_victory ? "a) Review the Valar's record"
                                           : "a) View scores";
    const char* option_b = morgoth_victory ? "b) Survey Angband one last time"
                                           : "b) Final look";
    const char* option_c = morgoth_victory ? "c) Rehear the proclamations"
                                           : "c) View final messages";
    const char* option_d = morgoth_victory ? "d) Review your legend"
                                           : "d) View character sheet";
    const char* option_e = morgoth_victory ? "e) Append to the annals"
                                           : "e) Add comment to notes";
    const char* option_f = morgoth_victory ? "f) Archive your legend"
                                           : "f) Save character sheet";
    const char* option_g = "g) Exit";

    if (!scene || !score)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1360);
    app_ui_panel_set_title(panel, death_status_title_attr(),
        death_status_title_text());

    death_build_name_line(score, summary, sizeof(summary));
    if (summary[0])
        app_ui_panel_set_subtitle(panel, TERM_WHITE, summary);

    if (death_build_curse_line(score, curse_line, sizeof(curse_line),
            &curse_attr))
    {
        if (!death_add_body_line(panel, curse_attr, curse_line))
            return false;
    }

    death_build_outcome_line(score, outcome, sizeof(outcome));
    death_build_run_line(score, run_line, sizeof(run_line));

    if (!death_add_body_line(panel, TERM_WHITE, outcome)
        || !death_add_body_line(panel, TERM_SLATE, run_line)
        || !death_add_body_line(panel, TERM_L_DARK,
            "________________________________________"))
        return false;

    if (death_build_trophy_line(score, trophy_line, sizeof(trophy_line))
        && !death_add_body_line(panel, TERM_YELLOW, trophy_line))
    {
        return false;
    }

    if (!death_add_body_line(panel,
            (highlight == 1) ? TERM_L_BLUE : TERM_WHITE, option_a)
        || !death_add_body_line(panel,
            (highlight == 2) ? TERM_L_BLUE : TERM_WHITE, option_b)
        || !death_add_body_line(panel,
            (highlight == 3) ? TERM_L_BLUE : TERM_WHITE, option_c)
        || !death_add_body_line(panel,
            (highlight == 4) ? TERM_L_BLUE : TERM_WHITE, option_d)
        || !death_add_body_line(panel,
            (highlight == 5) ? TERM_L_BLUE : TERM_WHITE, option_e)
        || !death_add_body_line(panel,
            (highlight == 6) ? TERM_L_BLUE : TERM_WHITE, option_f)
        || !death_add_body_line(panel,
            (highlight == 7) ? TERM_L_BLUE : TERM_WHITE, option_g))
    {
        return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Choose");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "a-g", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Exit");

    return true;
}

void do_cmd_morgoth_victory(void)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[160];

    p_ptr->morgoth_slain = true;
    input_clear_pending();
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    p_ptr->escaped = false;

    (void)strftime(long_day, sizeof(long_day), "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    strnfmt(buf, sizeof(buf),
        "On %s you broke the illusion binding Morgoth to his throne.",
        long_day);
    do_cmd_note(buf, p_ptr->depth);

    do_cmd_note(
        "The Valar hail your impossible triumph and pour out their blessing.",
        p_ptr->depth);

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    SDL_strlcpy(p_ptr->died_from, "Morgoth's illusory defeat",
        sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_OTHER);
    killer_commit(p_ptr->died_from);

    if (run_mode_is_blitz())
        blitz_show_end_summary(3);
}

void ui_death_show_character_info(void)
{
    app_ui_scene character_scene;
    app_ui_scene equipment_scene;
    app_ui_scene inventory_scene;
    bool have_equipment = (p_ptr->equip_cnt > 0);
    bool have_inventory = (p_ptr->inven_cnt > 0);
    bool have_character_scene;
    bool have_equipment_scene = true;
    bool have_inventory_scene = true;
    ui_information_scene_scope scope;
    bool escaped = false;

    have_character_scene = build_character_sheet_ui_scene(&character_scene,
        "(press any key)");

    if (have_equipment)
    {
        bool saved_item_tester_full = item_tester_full;

        item_tester_full = true;
        have_equipment_scene = death_build_equipment_scene(&equipment_scene);
        item_tester_full = saved_item_tester_full;
    }

    if (have_inventory)
    {
        bool saved_item_tester_full = item_tester_full;

        item_tester_full = true;
        have_inventory_scene = death_build_inventory_scene(&inventory_scene);
        item_tester_full = saved_item_tester_full;
    }

    if (!have_character_scene || !have_equipment_scene || !have_inventory_scene)
    {
        log_warn("death character info: semantic scenes required");
        return;
    }

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("death character info: semantic scene entry required");
        return;
    }

        if (!death_present_ui_page(&character_scene, &escaped))
        {
            ui_information_scene_leave(&scope);
            log_warn("death character info: failed to present character sheet scene");
            return;
        }

    if (escaped)
    {
        ui_information_scene_leave(&scope);
        return;
    }

    if (have_equipment)
    {
        if (!death_present_ui_page(&equipment_scene, &escaped))
        {
            ui_information_scene_leave(&scope);
            log_warn("death character info: failed to present equipment scene");
            return;
        }
        if (escaped)
        {
            ui_information_scene_leave(&scope);
            return;
        }
    }

    if (have_inventory)
    {
        if (!death_present_ui_page(&inventory_scene, &escaped))
        {
            ui_information_scene_leave(&scope);
            log_warn("death character info: failed to present inventory scene");
            return;
        }
        if (escaped)
        {
            ui_information_scene_leave(&scope);
            return;
        }
    }

    ui_information_scene_leave(&scope);
    if (escaped)
        return;

    do_cmd_knowledge_notes();
}

int ui_death_final_menu(const high_score* score, int* highlight)
{
    char ch;
    bool morgoth_victory = (p_ptr->morgoth_slain && !p_ptr->escaped);
    app_ui_scene scene;
    ui_information_scene_scope scope = { 0 };

    if (!death_build_final_menu_scene(&scene, score, *highlight,
            morgoth_victory)
        || !ui_information_scene_enter(&scope)
        || !ui_information_scene_present_ui(&scene))
    {
        if (scope.active)
            ui_information_scene_leave(&scope);
        log_warn("death final menu: semantic presentation required");
        return 7;
    }

    inkey_set_cursor_hidden(true);
    ch = (char)ui_information_scene_wait_key();
    inkey_set_cursor_hidden(false);
    ui_information_scene_leave(&scope);

    if (ch == 'a')
    {
        *highlight = 1;
        return 1;
    }
    if (ch == 'b')
    {
        *highlight = 2;
        return 2;
    }
    if (ch == 'c')
    {
        *highlight = 3;
        return 3;
    }
    if (ch == 'd')
    {
        *highlight = 4;
        return 4;
    }
    if (ch == 'e')
    {
        *highlight = 5;
        return 5;
    }
    if (ch == 'f')
    {
        *highlight = 6;
        return 6;
    }
    if ((ch == 'g') || (ch == 'q') || (ch == 'Q'))
    {
        *highlight = 7;
        return 7;
    }
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        return *highlight;
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = 7;
    }
    if (ch == '2')
    {
        if (*highlight < 7)
            (*highlight)++;
        else if (*highlight == 7)
            *highlight = 1;
    }

    return 0;
}
