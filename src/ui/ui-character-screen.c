/* File: ui/ui-character-screen.c */

#include "angband.h"
#include "app/app-ui.h"

#include "log/log.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "ui/story_font.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-status.h"

#include <ctype.h>

#define LINEW20 20
#define TUTORIAL_BROWSER_LAYOUT_COLS 80
#define TUTORIAL_BROWSER_TOP_PADDING_LINES 2
#define TUTORIAL_BROWSER_CONTENT_LINES 20
#define TUTORIAL_BROWSER_PROMPT_LINES 2
#define TUTORIAL_BROWSER_LAYOUT_ROWS \
    (TUTORIAL_BROWSER_TOP_PADDING_LINES + TUTORIAL_BROWSER_CONTENT_LINES \
        + TUTORIAL_BROWSER_PROMPT_LINES)
#define TUTORIAL_BROWSER_MIN_WIDTH 1000
#define TUTORIAL_BROWSER_MAX_WIDTH 1800
#define TUTORIAL_BROWSER_MAX_ROW_RUNS 16

static byte format_deep_call_value(char* buf, size_t buflen, int max_width)
{
    int base_increment = 0;
    int total_increment = 0;
    int effective_total;
    char pct_buf[16];
    byte attr = TERM_L_GREEN;

    if (!buf || buflen == 0)
        return attr;

    buf[0] = '\0';
    if (max_width < 1)
        return attr;
    (void)max_width;

    min_depth_timer_status(&base_increment, NULL, &total_increment, NULL, NULL);

    effective_total = total_increment;
    if (effective_total < 0)
        effective_total = 0;

    if (base_increment > 0)
    {
        long pct = ((long)effective_total * 100L + (base_increment / 2))
            / base_increment;
        if (pct > 999L)
            pct = 999L;
        strnfmt(pct_buf, sizeof(pct_buf), "%ld%%", pct);
    }
    else if (effective_total > 0)
    {
        SDL_strlcpy(pct_buf, "INF%", sizeof(pct_buf));
    }
    else
    {
        SDL_strlcpy(pct_buf, "0%", sizeof(pct_buf));
    }

    if (base_increment <= 0)
        attr = (effective_total > 0) ? TERM_L_GREEN : TERM_YELLOW;
    else if (effective_total > base_increment)
        attr = TERM_L_GREEN;
    else if (effective_total == base_increment)
        attr = TERM_L_BLUE;
    else if (effective_total > 0)
        attr = TERM_YELLOW;
    else
        attr = TERM_L_RED;

    SDL_strlcpy(buf, pct_buf, buflen);

    return attr;
}

static bool format_min_depth_progress_bar(char* buf, size_t buflen, int line_w)
{
    int progress = 0;
    int threshold = 1;
    int bar_width;
    int filled;

    if (!buf || buflen == 0)
        return false;

    buf[0] = '\0';
    if (line_w < 12)
        return false;

    min_depth_timer_status(NULL, NULL, NULL, &progress, &threshold);
    if (threshold < 1)
        threshold = 1;
    if (progress < 0)
        progress = 0;
    if (progress > threshold)
        progress = threshold;

    bar_width = line_w - 2;
    if (bar_width > 32)
        bar_width = 32;
    if (bar_width < 8)
        return false;

    filled = (progress * bar_width) / threshold;
    if (filled < 0)
        filled = 0;
    if (filled > bar_width)
        filled = bar_width;

    if ((size_t)(bar_width + 3) > buflen)
        return false;

    buf[0] = '[';
    for (int i = 0; i < bar_width; i++)
        buf[i + 1] = (i < filled) ? '#' : '.';
    buf[bar_width + 1] = ']';
    buf[bar_width + 2] = '\0';
    return true;
}

/*
 * Display interactive character screen tutorial
 * Shows 4 stages explaining different parts of the character screen
 * with actual character data displayed
 */
static void tutorial_prompt_label(int binding, const char* fallback, char* out,
    size_t out_size)
{
    if (!out || !out_size)
        return;

    platform_gamepad_action_binding_short_label(binding, out, out_size);
    if (streq(out, "(unbound)") || streq(out, "Multiple"))
        SDL_strlcpy(out, fallback, out_size);
}

typedef struct tutorial_render_run {
    s16b col;
    byte attr;
    char text[APP_UI_TEXT_MAX];
} tutorial_render_run;

typedef struct tutorial_render_target {
    app_ui_scene* scene;
    app_ui_panel* panel;
    tutorial_render_run runs[TUTORIAL_BROWSER_MAX_ROW_RUNS];
    u16b run_count;
    int width;
    int height;
    int active_row;
    int last_emitted_row;
    bool failed;
} tutorial_render_target;

static void tutorial_render_target_init(tutorial_render_target* target,
    app_ui_scene* scene, app_ui_panel* panel, int wid, int hgt)
{
    if (!target)
        return;

    memset(target, 0, sizeof(*target));
    target->scene = scene;
    target->panel = panel;
    target->width = wid;
    target->height = hgt;
    target->active_row = -1;
    target->last_emitted_row = -1;
    if (target->width < 1)
        target->width = 1;
    if (target->height < 1)
        target->height = 1;
    if (!scene || !panel
        || !app_ui_panel_begin_rich_paragraph(scene, panel))
    {
        target->failed = true;
    }
}


static bool tutorial_scene_add_rich_run(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, const char* text, int len)
{
    char buf[APP_UI_TEXT_MAX];
    int offset = 0;

    if (!scene || !panel || !text || len <= 0)
        return true;

    while (offset < len)
    {
        int chunk = MIN(len - offset, (int)sizeof(buf) - 1);

        memcpy(buf, text + offset, (size_t)chunk);
        buf[chunk] = '\0';
        if (!app_ui_panel_add_rich_text(scene, panel, attr, buf))
            return false;
        offset += chunk;
    }

    return true;
}

static bool tutorial_scene_add_spaces(app_ui_scene* scene, app_ui_panel* panel,
    int count)
{
    char spaces[APP_UI_TEXT_MAX];

    if (!scene || !panel || count <= 0)
        return true;

    memset(spaces, ' ', sizeof(spaces) - 1u);
    spaces[sizeof(spaces) - 1u] = '\0';

    while (count > 0)
    {
        int chunk = MIN(count, (int)sizeof(spaces) - 1);

        spaces[chunk] = '\0';
        if (!app_ui_panel_add_rich_text(scene, panel, TERM_WHITE, spaces))
            return false;
        spaces[chunk] = ' ';
        count -= chunk;
    }

    return true;
}

static bool tutorial_render_target_flush_active_row(
    tutorial_render_target* target)
{
    int current_col = 0;
    int line_breaks;

    if (!target)
        return false;
    if (target->failed)
        return false;
    if (target->active_row < 0)
        return true;

    line_breaks = (target->last_emitted_row < 0)
        ? target->active_row
        : (target->active_row - target->last_emitted_row);
    while (line_breaks-- > 0)
    {
        if (!app_ui_panel_add_rich_text(target->scene, target->panel,
                TERM_WHITE, "\n"))
        {
            target->failed = true;
            return false;
        }
    }

    for (int i = 0; i < target->run_count; i++)
    {
        tutorial_render_run* run = &target->runs[i];

        if (run->col > current_col
            && !tutorial_scene_add_spaces(target->scene, target->panel,
                run->col - current_col))
        {
            target->failed = true;
            return false;
        }
        if (!tutorial_scene_add_rich_run(target->scene, target->panel,
                run->attr, run->text, (int)strlen(run->text)))
        {
            target->failed = true;
            return false;
        }
        current_col = MAX(current_col, run->col + (int)strlen(run->text));
    }

    target->last_emitted_row = target->active_row;
    target->active_row = -1;
    target->run_count = 0;
    return true;
}

static void tutorial_render_target_add_body_line(tutorial_render_target* target,
    int col, byte attr, const char* text)
{
    char buf[APP_UI_TEXT_MAX];
    int padding;

    if (!target || target->failed || !text || !text[0])
        return;
    if (!tutorial_render_target_flush_active_row(target))
        return;

    if (col < 0)
        col = 0;
    if (col >= target->width)
        return;

    padding = MIN(col, (int)sizeof(buf) - 1);
    memset(buf, ' ', (size_t)padding);
    buf[padding] = '\0';
    SDL_strlcat(buf, text, sizeof(buf));
    if (!app_ui_panel_add_body_line(target->panel, attr, buf))
        target->failed = true;
}

static void tutorial_render_text(tutorial_render_target* target, int col,
    int row, byte attr, const char* text)
{
    tutorial_render_run* run;
    int remaining;
    int body_row_start;

    if (!target || target->failed || !text || !text[0] || row < 0
        || row >= target->height || col >= target->width)
    {
        return;
    }
    if (col < 0)
        col = 0;

    body_row_start = target->height - TUTORIAL_BROWSER_PROMPT_LINES;
    if (row >= body_row_start)
    {
        tutorial_render_target_add_body_line(target, col, attr, text);
        return;
    }

    if (target->active_row != row)
    {
        if (!tutorial_render_target_flush_active_row(target))
            return;
        target->active_row = row;
    }

    remaining = target->width - col;
    if (remaining <= 0 || target->run_count >= N_ELEMENTS(target->runs))
        return;

    if (target->run_count > 0)
    {
        tutorial_render_run* last = &target->runs[target->run_count - 1];
        int last_end = last->col + (int)strlen(last->text);

        if (last->attr == attr && last_end == col)
        {
            SDL_strlcat(last->text, text, MIN(sizeof(last->text),
                (size_t)remaining + 1u));
            return;
        }
    }

    run = &target->runs[target->run_count++];
    memset(run, 0, sizeof(*run));
    run->col = (s16b)col;
    run->attr = attr;
    SDL_strlcpy(run->text, text, MIN(sizeof(run->text),
        (size_t)remaining + 1u));
}

static bool tutorial_render_target_finish(tutorial_render_target* target)
{
    if (!target)
        return false;

    return tutorial_render_target_flush_active_row(target) && !target->failed;
}

static void tutorial_put_centered(tutorial_render_target* target, int row,
    byte attr, const char* text)
{
    int len;
    int col = 0;

    if (!target || !text)
        return;

    len = (int)strlen(text);
    if (len < target->width)
        col = (target->width - len) / 2;
    if (col < 0)
        col = 0;

    tutorial_render_text(target, col, row, attr, text);
}

static int tutorial_put_trunc(tutorial_render_target* target, int col, int row,
    int max_wid, byte attr, const char* text)
{
    char buf[256];
    size_t len;
    int keep;

    if (!text || !text[0])
        return 0;

    if (max_wid < 4)
        max_wid = 4;

    len = strlen(text);
    if ((int)len <= max_wid)
    {
        tutorial_render_text(target, col, row, attr, text);
        return 1;
    }

    if (max_wid >= (int)sizeof(buf))
        max_wid = (int)sizeof(buf) - 1;

    keep = max_wid - 3;
    if (keep < 0)
        keep = 0;
    SDL_strlcpy(buf, text, (size_t)keep + 1);
    SDL_strlcat(buf, "...", sizeof(buf));
    tutorial_render_text(target, col, row, attr, buf);
    return 1;
}

static int tutorial_put_wrapped_limited(tutorial_render_target* target,
    const char* text, int start_col, int start_row, int max_width, int max_row,
    byte color)
{
    int term_width = TUTORIAL_BROWSER_LAYOUT_COLS;
    int term_height = TUTORIAL_BROWSER_LAYOUT_ROWS;
    char line_buf[512];
    int row = start_row;
    int line_pos = 0;
    const char* p = text;

    if (!text || !text[0])
        return 0;

    if (target)
    {
        if (target->width > 0)
            term_width = target->width;
        if (target->height > 0)
            term_height = target->height;
    }

    if (max_width <= 0)
        max_width = term_width - start_col - 1;
    if (max_width < 10)
        max_width = 10;

    if (max_row <= 0 || max_row > term_height)
        max_row = term_height;

    while (*p && row < max_row)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            line_buf[line_pos] = '\0';
            if (line_pos > 0)
            {
                tutorial_render_text(target, start_col, row, color, line_buf);
                row++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                line_buf[wrap_pos] = '\0';
                tutorial_render_text(target, start_col, row, color, line_buf);

                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buf[line_pos] = '\0';
                tutorial_render_text(target, start_col, row, color, line_buf);
                line_pos = 0;
            }
            row++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0 && row < max_row)
    {
        line_buf[line_pos] = '\0';
        tutorial_render_text(target, start_col, row, color, line_buf);
        row++;
    }

    return row - start_row;
}

static int tutorial_count_wrapped_lines_ex(const char* text, int max_width,
    bool preserve_empty)
{
    if (!text || !text[0])
        return 0;

    if (max_width < 10)
        max_width = 10;

    char line_buf[512];
    int line_pos = 0;
    int count = 0;
    const char* p = text;

    while (*p)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            if (line_pos > 0 || preserve_empty)
                count++;
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_pos = 0;
            }

            count++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0)
        count++;

    return count;
}

static int tutorial_put_wrapped_slice_ex(tutorial_render_target* target,
    const char* text, int start_col, int start_row, int max_width, int max_row,
    byte color, int skip_lines, int max_lines, bool preserve_empty)
{
    if (!text || !text[0])
        return 0;

    if (skip_lines < 0)
        skip_lines = 0;
    if (max_lines < 1)
        max_lines = 1;
    if (max_width < 10)
        max_width = 10;

    char line_buf[512];
    int row = start_row;
    int line_pos = 0;
    int line_index = 0;
    int drawn = 0;
    const char* p = text;

    while (*p && row < max_row)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            bool have_line = (line_pos > 0) || preserve_empty;
            if (have_line)
            {
                if (line_index >= skip_lines && drawn < max_lines)
                {
                    if (line_pos > 0)
                    {
                        line_buf[line_pos] = '\0';
                        tutorial_render_text(target, start_col, row, color,
                            line_buf);
                    }
                    row++;
                    drawn++;
                    if (drawn >= max_lines || row >= max_row)
                        break;
                }
                line_index++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (line_index >= skip_lines && drawn < max_lines)
            {
                if (wrap_pos > 0)
                    line_buf[wrap_pos] = '\0';
                else
                    line_buf[line_pos] = '\0';
                if (line_buf[0])
                    tutorial_render_text(target, start_col, row, color,
                        line_buf);
                row++;
                drawn++;
                if (drawn >= max_lines || row >= max_row)
                    break;
            }

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_pos = 0;
            }

            line_index++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0 && row < max_row && drawn < max_lines)
    {
        if (line_index >= skip_lines)
        {
            line_buf[line_pos] = '\0';
            tutorial_render_text(target, start_col, row, color, line_buf);
            row++;
            drawn++;
        }
    }

    return drawn;
}

typedef struct {
    const char* txt;
    byte col;
} tutorial_trait_line;

static int tutorial_collect_traits(tutorial_trait_line* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;

    int n = 0;
    int race = p_ptr->prace;
    int character = p_ptr->pcharacter;

    byte col_mastery = TERM_L_GREEN;
    byte col_affinity = TERM_GREEN;
    byte col_penalty = TERM_L_RED;
    byte col_gr_penalty = TERM_RED;

#define PUSH_TRAIT(text_value, color_value) \
    do { \
        if ((text_value) && n < max_out) { out[n].txt = (text_value); out[n].col = (color_value); n++; } \
    } while (0)

#define CHECK_SKILL(LABEL, AFF_FLAG, PEN_FLAG) \
    do { \
        int sc = 0; \
        if (p_info[race].flags & (AFF_FLAG)) sc++; \
        if (c_info[character].flags & (AFF_FLAG)) sc++; \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) sc--; \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) sc--; \
        sc += curse_flag_count_rhf(AFF_FLAG); \
        if (PEN_FLAG) sc -= curse_flag_count_rhf(PEN_FLAG); \
        if (sc > 2) sc = 2; \
        if (sc < -2) sc = -2; \
        if (sc == 2) PUSH_TRAIT(LABEL "++", col_mastery); \
        else if (sc == 1) PUSH_TRAIT(LABEL "+", col_affinity); \
        else if (sc == -1) PUSH_TRAIT(LABEL "-", col_penalty); \
        else if (sc == -2) PUSH_TRAIT(LABEL "--", col_gr_penalty); \
    } while (0)

#define CHECK_UNIQUE(LABEL, FLAG, COLOR) \
    do { \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH_TRAIT((LABEL), (COLOR)); \
    } while (0)

#define CHECK_UNIQUE_U(LABEL, FLAG, COLOR) \
    do { \
        if (c_info[character].flags_u & (FLAG)) \
            PUSH_TRAIT((LABEL), (COLOR)); \
    } while (0)

    CHECK_SKILL("melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    CHECK_SKILL("evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    CHECK_SKILL("stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
    CHECK_SKILL("archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    CHECK_SKILL("will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    CHECK_SKILL("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    CHECK_SKILL("smithing", RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    CHECK_SKILL("song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    CHECK_SKILL("bow", RHF_BOW_PROFICIENCY, 0);
    CHECK_SKILL("axe", RHF_AXE_PROFICIENCY, 0);

    CHECK_UNIQUE_U("Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL, TERM_VIOLET);
    CHECK_UNIQUE_U("Chosen of Ulmo", UNQ_WIL_TUOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Indomitable Will", UNQ_EARENDIL, TERM_VIOLET);
    CHECK_UNIQUE_U("Orome Himself", UNQ_WIL_FIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    CHECK_UNIQUE_U("Girdle of Melian", UNQ_SNG_MEL, TERM_VIOLET);
    CHECK_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR, TERM_VIOLET);
    CHECK_UNIQUE_U("Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    CHECK_UNIQUE_U("Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Aure entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Voice of the Girdle", UNQ_SNG_THINGOL, TERM_VIOLET);
    CHECK_UNIQUE_U("Forgotten", UNQ_MIM, TERM_VIOLET);
    CHECK_UNIQUE_U("One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    CHECK_UNIQUE_U("Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Shadow Walker", UNQ_SNG_TURGON, TERM_VIOLET);
    CHECK_UNIQUE_U("Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    CHECK_UNIQUE_U("Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);
    CHECK_UNIQUE("Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    CHECK_UNIQUE("Seafarer", RHF_FREE, TERM_VIOLET);

    CHECK_UNIQUE("Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    CHECK_UNIQUE("Treacherous", RHF_TREACHERY, TERM_UMBER);
    CHECK_UNIQUE("Doom of Mandos", RHF_CURSE, TERM_UMBER);
    CHECK_UNIQUE("Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

#undef PUSH_TRAIT
#undef CHECK_SKILL
#undef CHECK_UNIQUE
#undef CHECK_UNIQUE_U

    return n;
}

void display_character_tutorial(void)
{
    int page = 0;
    char ch;
    ui_information_scene_scope info_scope;

    if (!ui_information_scene_supported() || !ui_information_scene_enter(&info_scope))
    {
        log_warn("character tutorial: semantic scene unavailable on the snapshot renderer path");
        msg_print("Character tutorial unavailable.");
        return;
    }

    while (1)
    {
        int wid = TUTORIAL_BROWSER_LAYOUT_COLS;
        int hgt = TUTORIAL_BROWSER_LAYOUT_ROWS;

        bool steamdeck = steamdeck_controls_active();
        bool birth_context = (playerturn == 0);

        const int header_row = 0;
        const int content_top = 2;
        const int nav_row = hgt - 1;
        const int hint_row = hgt - 2;
        const int content_max_row = hint_row;

        int text_col = 2;
        int text_w = wid - text_col - 2;
        if (text_w < 20)
            text_w = 20;

        int content_rows = content_max_row - content_top;
        if (content_rows < 4)
            content_rows = 4;

        tutorial_trait_line traits[160];
        int trait_n = tutorial_collect_traits(traits, (int)N_ELEMENTS(traits));

        typedef struct { const char* key; const char* desc; } ctl_line;
        ctl_line controls[24];
        int ctl_n = 0;
        char ctl_bufs[24][24];
        char ctl_note_bufs[4][128];
        int ctl_note_n = 0;

        if (steamdeck)
        {
            char confirm_label[16];
            char use_label[16];
            char examine_label[16];
            char inven_label[16];
            char equip_label[16];
            char look_label[16];
            char char_label[16];
            char fire_label[16];
            char sing_label[16];
            char activate_label[16];
            char map_label[16];
            char bash_label[16];
            char abilities_label[16];
            char help_label[16];
            char menu_label[16];
            char shift_label[16];
            char ctrl_label[16];

            tutorial_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
            tutorial_prompt_label('u', "X", use_label, sizeof(use_label));
            tutorial_prompt_label('x', "RS Right", examine_label, sizeof(examine_label));
            tutorial_prompt_label('i', "R1", inven_label, sizeof(inven_label));
            tutorial_prompt_label('e', "L1", equip_label, sizeof(equip_label));
            tutorial_prompt_label('l', "L1+R1", look_label, sizeof(look_label));
            tutorial_prompt_label('h', "Back", char_label, sizeof(char_label));
            tutorial_prompt_label('f', "RS Down", fire_label, sizeof(fire_label));
            tutorial_prompt_label('s', "Y", sing_label, sizeof(sing_label));
            tutorial_prompt_label('a', "RS Left", activate_label, sizeof(activate_label));
            tutorial_prompt_label('M', "RS Up", map_label, sizeof(map_label));
            tutorial_prompt_label('b', "B", bash_label, sizeof(bash_label));
            tutorial_prompt_label('\t', "L5", abilities_label, sizeof(abilities_label));
            tutorial_prompt_label('?', "?", help_label, sizeof(help_label));
            tutorial_prompt_label('m', "Start", menu_label, sizeof(menu_label));
            tutorial_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            tutorial_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));

            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], "D-pad/Stick", sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Move/attack" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], confirm_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Pick up" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], use_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Use item" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], examine_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Examine" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], inven_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Inventory" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], equip_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Equipment" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], look_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Look" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], char_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Character" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], fire_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Fire" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], sing_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Sing" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], activate_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Activate" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], map_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Map" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], bash_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Bash" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], abilities_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Abilities" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], help_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Help" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], menu_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Main menu" };
                ctl_n++;
            }
        }
        else
        {
            controls[ctl_n++] = (ctl_line){ "Numpad", "Move/attack" };
            controls[ctl_n++] = (ctl_line){ "Space", "Pick up" };
            controls[ctl_n++] = (ctl_line){ "u", "Use item" };
            controls[ctl_n++] = (ctl_line){ "x", "Examine" };
            controls[ctl_n++] = (ctl_line){ "i", "Inventory" };
            controls[ctl_n++] = (ctl_line){ "e", "Equipment" };
            controls[ctl_n++] = (ctl_line){ "l", "Look" };
            controls[ctl_n++] = (ctl_line){ "Ctrl+dir", "Bash/disarm/tunnel" };
            controls[ctl_n++] = (ctl_line){ "f/F", "Fire" };
            controls[ctl_n++] = (ctl_line){ "s/S", "Sing/Stealth" };
            controls[ctl_n++] = (ctl_line){ "a", "Activate" };
            controls[ctl_n++] = (ctl_line){ "c", "Close door" };
            controls[ctl_n++] = (ctl_line){ "h", "Character" };
            controls[ctl_n++] = (ctl_line){ "m", "Main menu" };
            controls[ctl_n++] = (ctl_line){ "Tab", "Abilities" };
            controls[ctl_n++] = (ctl_line){ "?", "Help" };
            controls[ctl_n++] = (ctl_line){ NULL, "Shortcuts can be changed in Settings." };
        }

        if (steamdeck)
        {
            char shift_label[16];
            char ctrl_label[16];
            char sing_label[16];
            char fire_label[16];

            tutorial_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            tutorial_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));
            tutorial_prompt_label('s', "Y", sing_label, sizeof(sing_label));
            tutorial_prompt_label('f', "RS Down", fire_label, sizeof(fire_label));

            if (ctl_n < (int)N_ELEMENTS(controls) && ctl_note_n < (int)N_ELEMENTS(ctl_note_bufs))
            {
                strnfmt(ctl_note_bufs[ctl_note_n], sizeof(ctl_note_bufs[ctl_note_n]),
                    "Shift: %s+%s=Stealth, %s+%s=2nd quiver",
                    shift_label, sing_label, shift_label, fire_label);
                controls[ctl_n++] = (ctl_line){ NULL, ctl_note_bufs[ctl_note_n++] };
            }
            if (ctl_n < (int)N_ELEMENTS(controls) && ctl_note_n < (int)N_ELEMENTS(ctl_note_bufs))
            {
                strnfmt(ctl_note_bufs[ctl_note_n], sizeof(ctl_note_bufs[ctl_note_n]),
                    "Ctrl: %s+dir = Bash/Disarm/Tunnel", ctrl_label);
                controls[ctl_n++] = (ctl_line){ NULL, ctl_note_bufs[ctl_note_n++] };
            }
        }

        int list_rows = content_max_row - (content_top + 2);
        if (list_rows < 1)
            list_rows = 1;

        bool two_col = (wid >= 74);
        int trait_items_per_page = list_rows * (two_col ? 2 : 1);
        if (trait_items_per_page < 1)
            trait_items_per_page = 1;
        int trait_pages = (trait_n <= 0)
            ? 1
            : (trait_n + trait_items_per_page - 1) / trait_items_per_page;

        int ctl_items_per_page = list_rows * (two_col ? 2 : 1);
        if (ctl_items_per_page < 1)
            ctl_items_per_page = 1;
        int ctl_pages = (ctl_n + ctl_items_per_page - 1) / ctl_items_per_page;
        if (ctl_pages < 1)
            ctl_pages = 1;

        const char* skills_intro =
            "Total = Base + stat + equip + misc. Base affects ability purchase cost.";
        const char* skill_desc[S_MAX] = {
            "Melee chance to hit",
            "Ranged chance to hit",
            "Evade attacks",
            "Avoid detection",
            "Notice hidden",
            "Mental resistance",
            "Craft items",
            "Song power",
            ""
        };

        int skill_order[S_MAX];
        int skill_count = 0;
        for (int s = 0; s < S_MAX; s++)
        {
            if (s == S_SPC)
                continue;
            skill_order[skill_count++] = s;
        }

        int intro_lines = tutorial_count_wrapped_lines_ex(skills_intro, text_w, false);
        int skills_cap_first = content_rows - (2 + intro_lines + 1);
        int skills_cap_next = content_rows - 2;
        if (skills_cap_first < 1)
            skills_cap_first = 1;
        if (skills_cap_next < 1)
            skills_cap_next = 1;

        int skill_page_starts[32];
        int skill_page_ends[32];
        int skill_pages = 0;
        {
            int cur = 0;
            while (cur < skill_count && skill_pages < (int)N_ELEMENTS(skill_page_starts))
            {
                int cap = (skill_pages == 0) ? skills_cap_first : skills_cap_next;
                int used = 0;
                int start = cur;

                while (cur < skill_count)
                {
                    int sid = skill_order[cur];
                    int need = 1 + tutorial_count_wrapped_lines_ex(skill_desc[sid], text_w, false);
                    if (need < 1)
                        need = 1;
                    if (used + need > cap && used > 0)
                        break;
                    used += need;
                    cur++;
                }

                if (cur == start)
                    cur++;

                skill_page_starts[skill_pages] = start;
                skill_page_ends[skill_pages] = cur;
                skill_pages++;
            }
        }
        if (skill_pages < 1)
        {
            skill_pages = 1;
            skill_page_starts[0] = 0;
            skill_page_ends[0] = 0;
        }

        int history_cap = content_rows - 2;
        if (history_cap < 1)
            history_cap = 1;
        int history_lines = tutorial_count_wrapped_lines_ex(p_ptr->history, text_w, true);
        if (history_lines < 1)
            history_lines = 1;
        int history_pages = (history_lines + history_cap - 1) / history_cap;
        if (history_pages < 1)
            history_pages = 1;

        char birth_text[1200];
        int birth_pages = 0;
        if (birth_context)
        {
            size_t off = 0;
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "On smaller displays the character creation screens use a compact layout. All information is still available; it is just rearranged.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Character selection: use Up/Down to pick, and read the description/traits for the highlighted choice. On short screens, traits may be shown in a tighter, compact list.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Stats allocation: select a stat, then adjust it. The display may reuse the compact Stats+Skills sheet with the current stat highlighted.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Skills allocation works the same way (select a skill, then adjust).\n\n");

            if (steamdeck)
            {
                char next_label[16];
                char back_label[16];
                tutorial_prompt_label(steamdeck_confirm_key(), "A", next_label, sizeof(next_label));
                tutorial_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                    "Navigation: D-pad Up/Down select, Left/Right adjust  [%s] confirm  [%s] back", next_label, back_label);
            }
            else
            {
                off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                    "Navigation: Up/Down selects; Left/Right adjusts; Enter/Space confirms; ESC goes back.");
            }

            int birth_cap = content_rows - 2;
            if (birth_cap < 1)
                birth_cap = 1;
            int birth_lines = tutorial_count_wrapped_lines_ex(birth_text, text_w, true);
            if (birth_lines < 1)
                birth_lines = 1;
            birth_pages = (birth_lines + birth_cap - 1) / birth_cap;
            if (birth_pages < 1)
                birth_pages = 1;
        }

        const int page_core_1 = 0;
        const int page_core_2 = 1;
        const int page_attrs = 2;
        const int page_skills_start = 3;
        const int page_traits_legend = page_skills_start + skill_pages;
        const int page_traits_list_start = page_traits_legend + 1;
        const int page_history_start = page_traits_list_start + trait_pages;
        const int page_controls_start = page_history_start + history_pages;
        const int page_birth_start = page_controls_start + ctl_pages;
        const int total_pages = page_birth_start + (birth_context ? birth_pages : 0);

        if (page < 0)
            page = 0;
        if (page >= total_pages)
            page = total_pages - 1;

        app_ui_scene ui_scene;
        app_ui_panel* ui_panel;
        tutorial_render_target target;

        app_ui_scene_init(&ui_scene);
        ui_panel = app_ui_scene_append_panel(&ui_scene, APP_UI_LAYER_BROWSER);
        if (!ui_panel)
        {
            ui_information_scene_leave(&info_scope);
            log_warn("character tutorial: failed to allocate semantic tutorial panel");
            msg_print("Character tutorial unavailable.");
            return;
        }
        ui_panel->style = APP_UI_PANEL_STYLE_BROWSER;
        ui_panel->accent_attr = TERM_L_BLUE;
        app_ui_panel_set_title(ui_panel, TERM_L_BLUE, "Character Tutorial");
        app_ui_panel_set_widths(ui_panel, TUTORIAL_BROWSER_MIN_WIDTH,
            TUTORIAL_BROWSER_MAX_WIDTH);

        tutorial_render_target_init(&target, &ui_scene, ui_panel, wid, hgt);

        {
            char title[96];
            strnfmt(title, sizeof(title), "TUTORIAL  %d/%d", page + 1, total_pages);
            tutorial_put_centered(&target, header_row, TERM_L_BLUE, title);
        }

        int row = content_top;

        if (page == page_core_1)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "CORE STATISTICS (1/2)");
            row++;
            {
                char buf[128];
                strnfmt(buf, sizeof(buf), "Exp: %ld/%ld", (long)p_ptr->new_exp, (long)p_ptr->exp);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Awarded for depth progress, identifying items, spotting and killing monsters.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                long cur_wgt = p_ptr->total_weight / 10;
                long max_wgt = weight_limit() / 10;
                strnfmt(buf, sizeof(buf), "Burden: %ld/%ld lbs", cur_wgt, max_wgt);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Weight carried / maximum capacity.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                if (turn > 0)
                {
                    long cur_d = p_ptr->depth * 50;
                    long min_d = min_depth() * 50;
                    strnfmt(buf, sizeof(buf), "Depth: %ld/%ld", cur_d, min_d);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                    row += tutorial_put_wrapped_limited(&target,
                        "Current depth / minimum return depth (rises over time).",
                        text_col, row, text_w, content_max_row, TERM_SLATE);
                }

                comma_number(buf, playerturn);
                {
                    char line[128];
                    strnfmt(line, sizeof(line), "Turn: %s", buf);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Total game turns elapsed.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    char line[64];
                    strnfmt(line, sizeof(line), "Light: %d", p_ptr->cur_light);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Current light radius.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_core_2)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "CORE STATISTICS (2/2)");
            row++;
            {
                char buf[128];
                strnfmt(buf, sizeof(buf), "Melee: (%+d,%dd%d)", p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Main hand: (chance to hit, damage dice).",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                strnfmt(buf, sizeof(buf), "Bows:  (%+d,%dd%d)", p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Ranged: (chance to hit, damage dice).",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                strnfmt(buf, sizeof(buf), "Armor: [%+d,%d-%d]", p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "[evasion, protection] = hit-avoid chance and damage absorption.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    int cur_hp = MIN(p_ptr->chp, 999);
                    int max_hp = MIN(p_ptr->mhp, 999);
                    char line[64];
                    byte col = (p_ptr->chp >= p_ptr->mhp) ? TERM_L_GREEN : (p_ptr->chp > p_ptr->mhp / 4) ? TERM_YELLOW : TERM_RED;
                    strnfmt(line, sizeof(line), "Health: %d/%d", cur_hp, max_hp);
                    tutorial_render_text(&target, 2, row++, col, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Hit points: current / maximum.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    int cur_sp = MIN(p_ptr->csp, 999);
                    int max_sp = MIN(p_ptr->msp, 999);
                    char line[64];
                    byte col = (p_ptr->csp >= p_ptr->msp) ? TERM_L_GREEN : (p_ptr->csp > p_ptr->msp / 4) ? TERM_YELLOW : TERM_RED;
                    strnfmt(line, sizeof(line), "Voice:  %d/%d", cur_sp, max_sp);
                    tutorial_render_text(&target, 2, row++, col, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Song points: current / maximum.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_attrs)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE, "ATTRIBUTES");
            row++;
            row += tutorial_put_wrapped_limited(&target,
                "Current = Base + equip + misc - drain. Green = boosted, orange = reduced.",
                text_col, row, text_w, content_max_row, TERM_SLATE);
            row++;

            for (int stat = 0; stat < A_MAX && row < content_max_row; stat++)
            {
                char line[96];
                byte a = TERM_WHITE;
                int use = p_ptr->stat_use[stat];
                int base = p_ptr->stat_base[stat];
                if (use > base) a = TERM_L_GREEN;
                else if (use < base) a = TERM_ORANGE;
                strnfmt(line, sizeof(line), "%s: %d", stat_names[stat], use);
                tutorial_render_text(&target, 2, row++, a, line);

                const char* desc = NULL;
                if (stat == A_STR) desc = "Strength: melee dice and weight capacity.";
                else if (stat == A_DEX) desc = "Dexterity: melee/evasion/archery/stealth.";
                else if (stat == A_CON) desc = "Constitution: hit points.";
                else if (stat == A_GRA) desc = "Grace: will/perception/song/smithing and voice.";
                row += tutorial_put_wrapped_limited(&target, desc, text_col,
                    row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page >= page_skills_start && page < page_skills_start + skill_pages)
        {
            int sub = page - page_skills_start;
            char heading[64];
            if (skill_pages > 1)
                strnfmt(heading, sizeof(heading), "SKILLS (%d/%d)", sub + 1, skill_pages);
            else
                SDL_strlcpy(heading, "SKILLS", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (sub == 0)
            {
                row += tutorial_put_wrapped_limited(&target,
                    skills_intro,
                    text_col, row, text_w, content_max_row, TERM_SLATE);
                row++;
            }

            int start_idx = skill_page_starts[sub];
            int end_idx = skill_page_ends[sub];
            for (int i = start_idx; i < end_idx && row < content_max_row; i++)
            {
                int sid = skill_order[i];
                char line[128];
                strnfmt(line, sizeof(line), "%s: %d (base %d)",
                    skill_names_full[sid], p_ptr->skill_use[sid], p_ptr->skill_base[sid]);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                row += tutorial_put_wrapped_limited(&target, skill_desc[sid],
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_traits_legend)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "TRAITS LEGEND");
            row++;

            tutorial_render_text(&target, 2, row, TERM_L_GREEN, "++");
            tutorial_render_text(&target, 6, row++, TERM_SLATE, "Mastery");
            tutorial_render_text(&target, 2, row, TERM_GREEN, "+");
            tutorial_render_text(&target, 6, row++, TERM_SLATE, "Affinity");
            tutorial_render_text(&target, 2, row, TERM_RED, "--");
            tutorial_render_text(&target, 6, row++, TERM_SLATE,
                "Major penalty");
            tutorial_render_text(&target, 2, row, TERM_L_RED, "-");
            tutorial_render_text(&target, 6, row++, TERM_SLATE,
                "Minor penalty");
            tutorial_render_text(&target, 2, row, TERM_VIOLET, "UNIQUE");
            tutorial_render_text(&target, 10, row++, TERM_SLATE,
                "Special ability");
            tutorial_render_text(&target, 2, row, TERM_UMBER, "CURSE");
            tutorial_render_text(&target, 10, row++, TERM_SLATE,
                "Character curse");
            row++;

            row += tutorial_put_wrapped_limited(&target,
                "Next page shows your current traits.",
                text_col, row, text_w, content_max_row, TERM_SLATE);
        }
        else if (page >= page_traits_list_start && page < page_traits_list_start + trait_pages)
        {
            int sub = page - page_traits_list_start;
            char heading[64];
            strnfmt(heading, sizeof(heading), "YOUR TRAITS (%d/%d)", sub + 1, trait_pages);
            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (trait_n <= 0)
            {
                tutorial_render_text(&target, 2, row++, TERM_SLATE,
                    "(No special traits)");
            }
            else
            {
                int start = sub * trait_items_per_page;
                int end = start + trait_items_per_page;
                if (start < 0) start = 0;
                if (end > trait_n) end = trait_n;

                int col1 = 2;
                int gap = 2;
                int colw = wid - 4;
                int col2 = 2;
                if (two_col)
                {
                    colw = (wid - 4 - gap) / 2;
                    if (colw < 18)
                        colw = 18;
                    col2 = col1 + colw + gap;
                }

                int idx = start;
                for (int r = 0; r < list_rows && row < content_max_row; r++)
                {
                    if (idx >= end)
                        break;
                    tutorial_put_trunc(&target, col1, row, colw, traits[idx].col,
                        traits[idx].txt);
                    idx++;
                    if (two_col && idx < end)
                    {
                        tutorial_put_trunc(&target, col2, row, colw,
                            traits[idx].col, traits[idx].txt);
                        idx++;
                    }
                    row++;
                }
            }
        }
        else if (page >= page_history_start && page < page_history_start + history_pages)
        {
            int sub = page - page_history_start;
            char heading[64];
            if (history_pages > 1)
                strnfmt(heading, sizeof(heading), "HISTORY (%d/%d)", sub + 1, history_pages);
            else
                SDL_strlcpy(heading, "HISTORY", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (p_ptr->history[0])
            {
                int skip = sub * history_cap;
                tutorial_put_wrapped_slice_ex(&target,
                    p_ptr->history,
                    text_col,
                    row,
                    text_w,
                    content_max_row,
                    TERM_WHITE,
                    skip,
                    history_cap,
                    true);
            }
            else
            {
                tutorial_render_text(&target, 2, row++, TERM_SLATE,
                    "(No history)");
            }
        }
        else if (page >= page_controls_start && page < page_controls_start + ctl_pages)
        {
            int sub = page - page_controls_start;
            char heading[64];
            strnfmt(heading, sizeof(heading), "ESSENTIAL CONTROLS (%d/%d)", sub + 1, ctl_pages);
            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            int start = sub * ctl_items_per_page;
            int end = start + ctl_items_per_page;
            if (start < 0) start = 0;
            if (end > ctl_n) end = ctl_n;

            int col1 = 2;
            int gap = 2;
            int colw = wid - 4;
            int col2 = 2;
            if (two_col)
            {
                colw = (wid - 4 - gap) / 2;
                if (colw < 20)
                    colw = 20;
                col2 = col1 + colw + gap;
            }

            int idx = start;
            for (int r = 0; r < list_rows && row < content_max_row; r++)
            {
                if (idx >= end)
                    break;
                {
                    char line[128];
                    if (controls[idx].key)
                        strnfmt(line, sizeof(line), "%s - %s", controls[idx].key, controls[idx].desc);
                    else
                        SDL_strlcpy(line, controls[idx].desc, sizeof(line));
                    tutorial_put_trunc(&target, col1, row, colw, TERM_SLATE,
                        line);
                }
                idx++;

                if (two_col && idx < end)
                {
                    char line[128];
                    if (controls[idx].key)
                        strnfmt(line, sizeof(line), "%s - %s", controls[idx].key, controls[idx].desc);
                    else
                        SDL_strlcpy(line, controls[idx].desc, sizeof(line));
                    tutorial_put_trunc(&target, col2, row, colw, TERM_SLATE,
                        line);
                    idx++;
                }
                row++;
            }
        }
        else if (birth_context && page >= page_birth_start && page < page_birth_start + birth_pages)
        {
            int sub = page - page_birth_start;
            char heading[96];
            if (birth_pages > 1)
                strnfmt(heading, sizeof(heading), "CHARACTER CREATION (COMPACT) (%d/%d)", sub + 1, birth_pages);
            else
                SDL_strlcpy(heading, "CHARACTER CREATION (COMPACT SCREENS)", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            {
                int birth_cap = content_rows - 2;
                int skip = sub * birth_cap;
                tutorial_put_wrapped_slice_ex(&target,
                    birth_text,
                    text_col,
                    row,
                    text_w,
                    content_max_row,
                    TERM_SLATE,
                    skip,
                    birth_cap,
                    true);
            }
        }

        {
            if (page == total_pages - 1)
                tutorial_put_centered(&target, hint_row, TERM_L_GREEN,
                    "Tutorial complete!");
            else
                tutorial_put_centered(&target, hint_row, TERM_YELLOW,
                    steamdeck ? "D-pad left/right to navigate" : "Use left/right (or any key) to navigate");

            if (steamdeck)
            {
                char next_label[16];
                char back_label[16];
                tutorial_prompt_label(steamdeck_confirm_key(), "A", next_label, sizeof(next_label));
                tutorial_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

                char nav[160];
                if (page > 0)
                    strnfmt(nav, sizeof(nav), "(D-Left Prev)   (%s Next)   (%s Exit)", next_label, back_label);
                else
                    strnfmt(nav, sizeof(nav), "(%s Next)   (%s Exit)", next_label, back_label);
                tutorial_put_centered(&target, nav_row, TERM_SLATE, nav);
            }
            else
            {
                char nav[160];
                if (page > 0)
                    strnfmt(nav, sizeof(nav), "(4/<- Prev)   (6/-> Next)   (ESC Exit)");
                else
                    strnfmt(nav, sizeof(nav), "(6/-> Next)   (ESC Exit)");
                tutorial_put_centered(&target, nav_row, TERM_SLATE, nav);
            }
        }

        if (!tutorial_render_target_finish(&target)
            || !ui_information_scene_present_ui(&ui_scene))
        {
            ui_information_scene_leave(&info_scope);
            log_warn("character tutorial: failed to present semantic tutorial scene");
            msg_print("Character tutorial unavailable.");
            return;
        }

        ch = (char)ui_information_scene_wait_key_nonrepeat();
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if (ch == ESCAPE)
            break;
        else if (ch == '4')
        {
            if (page > 0)
                page--;
        }
        else if (ch == '6' || ch == ' ' || ch == '\r' || ch == '\n'
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (page < total_pages - 1)
                page++;
            else
                break;
        }
        else
        {
            if (page < total_pages - 1)
                page++;
            else
                break;
        }
    }

    ui_information_scene_leave(&info_scope);
}

typedef struct {
    const char *txt;
    byte col;
} compact_trait_line;

static int collect_compact_trait_lines(compact_trait_line* out, int out_max)
{
    int race = p_ptr->prace;
    int character = p_ptr->pcharacter;
    int total = 0;

    byte attr_affinity   = TERM_GREEN;
    byte attr_mastery    = TERM_L_GREEN;
    byte attr_penalty    = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    compact_trait_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)
#define HANDLE_SKILL_EX(LABEL, AFF_FLAG, PEN_FLAG)                                      \
    do {                                                                                \
        int score = 0;                                                                  \
        if (p_info[race].flags      & (AFF_FLAG)) score++;                              \
        if (c_info[character].flags & (AFF_FLAG)) score++;                              \
        if ((PEN_FLAG) && (p_info[race].flags      & (PEN_FLAG))) score--;              \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;              \
        score += curse_flag_count_rhf(AFF_FLAG);                                        \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);                        \
        if (score >  2) score =  2;                                                     \
        if (score < -2) score = -2;                                                     \
        if (score ==  2)      PUSH(ma_buf,  ma_n,  LABEL "++", attr_mastery);          \
        else if (score == 1)  PUSH(af_buf,  af_n,  LABEL "+ ", attr_affinity);         \
        else if (score == -1) PUSH(pen_buf, pen_n, LABEL "- ", attr_penalty);          \
        else if (score == -2) PUSH(pen_buf, pen_n, LABEL "--", attr_gr_penalty);       \
    } while (0)
#define HANDLE_UNIQUE(LABEL, FLAG, COLOR)                                               \
    do {                                                                                \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))        \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)
#define HANDLE_UNIQUE_U(LABEL, FLAG, COLOR)                                             \
    do {                                                                                \
        if (c_info[character].flags_u & (FLAG))                                         \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)
#define EMIT(arr, n)                                                                    \
    do {                                                                                \
        for (int _i = 0; _i < (n); ++_i) {                                              \
            if (out && total < out_max) out[total] = (arr)[_i];                        \
            total++;                                                                    \
        }                                                                               \
    } while (0)

    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U("Master Artisan",     UNQ_SMT_FEANOR,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Chosen of Ulmo",     UNQ_WIL_TUOR,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Indomitable Will",   UNQ_EARENDIL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Orome Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Songs of Power",     UNQ_SNG_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Elven Dance",        UNQ_SNG_LUT,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Old Master",         UNQ_SMT_GAMIL,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Ring Master",        UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U("Aure entuluva",      UNQ_SNG_HURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Voice of the Girdle",UNQ_SNG_THINGOL,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Forgotten",          UNQ_MIM,          TERM_VIOLET);
    HANDLE_UNIQUE_U("One Handed",         UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U("Agarwaen",           UNQ_WIL_TURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Shadow Walker",      UNQ_SNG_TURGON,   TERM_VIOLET);
    HANDLE_UNIQUE("Gift of Eru",          RHF_GIFTERU,      TERM_VIOLET);
    HANDLE_UNIQUE("Seafarer",             RHF_FREE,         TERM_VIOLET);

    HANDLE_UNIQUE("Kinslayer",            RHF_KINSLAYER,    TERM_UMBER);
    HANDLE_UNIQUE("Treacherous",          RHF_TREACHERY,    TERM_UMBER);
    HANDLE_UNIQUE("Doom of Mandos",       RHF_CURSE,        TERM_UMBER);
    HANDLE_UNIQUE("Morgoth Curse",        RHF_MOR_CURSE,    TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U
#undef HANDLE_UNIQUE
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}

static void character_sheet_build_name(char* buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    if (p_ptr->oaths_broken)
    {
        strnfmt(buf, buf_size, "%s the Oathbreaker", op_ptr->full_name);
        return;
    }

    strnfmt(buf, buf_size, "%s%s", op_ptr->full_name,
        c_name + current_character_profile->alt_name);
}

static void character_sheet_copy_trimmed(char* dst, size_t dst_size, cptr text,
    int max_chars)
{
    size_t len;

    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';
    if (!text || !text[0] || max_chars <= 0)
        return;

    len = strlen(text);
    while (len > 0 && text[len - 1] == ' ')
        len--;
    if ((int)len > max_chars)
        len = (size_t)max_chars;
    if (len >= dst_size)
        len = dst_size - 1u;

    memcpy(dst, text, len);
    dst[len] = '\0';
}

static bool character_sheet_add_metric_pair(app_ui_panel* panel, cptr label,
    cptr current, byte current_attr, char separator, cptr rhs, byte rhs_attr)
{
    return app_ui_panel_add_character_metric(panel, TERM_WHITE, label,
        current_attr, current, separator, rhs_attr, rhs);
}

static bool character_sheet_add_metric_value(app_ui_panel* panel, cptr label,
    byte value_attr, cptr value)
{
    return app_ui_panel_add_character_metric(panel, TERM_WHITE, label,
        value_attr, value, '\0', TERM_WHITE, "");
}

static bool character_sheet_build_summary(app_ui_panel* panel)
{
    char cur[32];
    char rhs[32];
    char val[64];
    char buf[160];
    byte value_attr;

    if (!panel)
        return false;

    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
    if (!character_sheet_add_metric_pair(panel, "Exp", cur, TERM_L_GREEN, '/',
            rhs, TERM_L_GREEN))
    {
        return false;
    }

    {
        long cur_b = (long)(p_ptr->total_weight / 10L);
        long max_b = (long)(weight_limit() / 10L);

        strnfmt(cur, sizeof(cur), "%ld", cur_b);
        strnfmt(rhs, sizeof(rhs), "%ld", max_b);
        if (!character_sheet_add_metric_pair(panel, "Burden", cur,
                (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW, '/',
                rhs, TERM_L_GREEN))
        {
            return false;
        }
    }

    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000)
            cur_d = 1000;
        if (min_d > 1000)
            min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);
        if (!character_sheet_add_metric_pair(panel, "Depth c/m", cur,
                (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW, '/',
                rhs, TERM_L_GREEN))
        {
            return false;
        }
        if (format_min_depth_progress_bar(buf, sizeof(buf), LINEW20)
            && !app_ui_panel_add_character_metric(panel, TERM_WHITE, "",
                TERM_L_BLUE, buf, '\0', TERM_WHITE, ""))
        {
            return false;
        }
    }

    value_attr = format_deep_call_value(val, sizeof(val), 12);
    if (!character_sheet_add_metric_value(panel, "Deep Call", value_attr, val))
        return false;

    comma_number(buf, playerturn);
    if (!character_sheet_add_metric_value(panel, "Turn", TERM_L_GREEN, buf))
        return false;

    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    if (!character_sheet_add_metric_value(panel, "Light", TERM_L_GREEN, val))
        return false;

    if (!app_ui_panel_add_character_metric(panel, TERM_WHITE, "", TERM_WHITE,
            "", '\0', TERM_WHITE, ""))
    {
        return false;
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    if (!character_sheet_add_metric_value(panel, "Melee", TERM_L_BLUE, val))
        return false;

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK]
        && !character_sheet_add_metric_value(panel, "Melee x2", TERM_L_BLUE,
            val))
    {
        return false;
    }

    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
            p_ptr->mdd2, p_ptr->mds2);
        if (!character_sheet_add_metric_value(panel, "Offhand", TERM_L_BLUE,
                val))
        {
            return false;
        }
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC],
        p_ptr->add, p_ptr->ads);
    if (!character_sheet_add_metric_value(panel, "Bows", TERM_L_BLUE, val))
        return false;

    strnfmt(val, sizeof(val), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    if (!character_sheet_add_metric_value(panel, "Armor", TERM_L_BLUE, val))
        return false;

    {
        int chp = p_ptr->chp;
        int mhp = p_ptr->mhp;

        if (chp > 999)
            chp = 999;
        if (mhp > 999)
            mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        if (!character_sheet_add_metric_pair(panel, "Health", cur, TERM_L_BLUE,
                '/', rhs, TERM_L_BLUE))
        {
            return false;
        }
    }

    {
        int csp = p_ptr->csp;
        int msp = p_ptr->msp;

        if (csp > 999)
            csp = 999;
        if (msp > 999)
            msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        if (!character_sheet_add_metric_pair(panel, "Voice", cur, TERM_L_BLUE,
                '/', rhs, TERM_L_BLUE))
        {
            return false;
        }
    }

    if (p_ptr->song1 != SNG_NOTHING)
    {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
        if (!character_sheet_add_metric_value(panel, "Song", TERM_L_BLUE, val))
            return false;
    }
    if (p_ptr->song2 != SNG_NOTHING)
    {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
        if (!character_sheet_add_metric_value(panel, "Song", TERM_L_BLUE, val))
            return false;
    }

    return true;
}

static bool character_sheet_build_traits(app_ui_panel* panel)
{
    compact_trait_line traits[96];
    int i;
    byte story = story_character_enabled() ? STORY_FLAG_USE : 0;

    if (!panel)
        return false;

    for (i = 0; i < collect_compact_trait_lines(traits, N_ELEMENTS(traits)); i++)
    {
        if (!app_ui_panel_add_detail_line_ex(panel, traits[i].col, story,
                traits[i].txt))
        {
            return false;
        }
    }

    return true;
}

static bool character_sheet_build_stats(app_ui_panel* panel)
{
    int i;

    if (!panel)
        return false;

    for (i = 0; i < A_MAX; i++)
    {
        const char* stat_label = (p_ptr->stat_drain[i] < 0)
            ? stat_names_reduced[i]
            : stat_names[i];
        char label[32];
        char value[APP_UI_KEY_MAX];
        char base[APP_UI_KEY_MAX];
        char mod1[APP_UI_KEY_MAX];
        char mod2[APP_UI_KEY_MAX];
        char mod3[APP_UI_KEY_MAX];

        character_sheet_copy_trimmed(label, sizeof(label), stat_label, 12);
        mod1[0] = '\0';
        mod2[0] = '\0';
        mod3[0] = '\0';
        cnv_stat(p_ptr->stat_use[i], value);
        base[0] = '\0';

        if (p_ptr->stat_equip_mod[i] != 0 || p_ptr->stat_drain[i] != 0
            || p_ptr->stat_misc_mod[i] != 0)
        {
            cnv_stat(p_ptr->stat_base[i], base);
            if (p_ptr->stat_equip_mod[i] != 0)
                strnfmt(mod1, sizeof(mod1), "%+3d", p_ptr->stat_equip_mod[i]);
            if (p_ptr->stat_drain[i] != 0)
                strnfmt(mod2, sizeof(mod2), "%+3d", p_ptr->stat_drain[i]);
            if (p_ptr->stat_misc_mod[i] != 0)
                strnfmt(mod3, sizeof(mod3), "%+3d", p_ptr->stat_misc_mod[i]);
        }

        if (!app_ui_panel_add_character_stat(panel, TERM_WHITE, label,
                (p_ptr->stat_drain[i] < 0) ? TERM_YELLOW : TERM_L_GREEN, value,
                TERM_SLATE, base[0] ? '=' : '\0', TERM_GREEN, base, TERM_SLATE,
                mod1, TERM_SLATE, mod2, TERM_SLATE, mod3))
        {
            return false;
        }
    }

    if (!app_ui_panel_add_character_stat(panel, TERM_WHITE, "", TERM_WHITE, "",
            TERM_WHITE, '\0', TERM_WHITE, "", TERM_WHITE, "", TERM_WHITE, "",
            TERM_WHITE, ""))
    {
        return false;
    }

    for (i = 0; i < S_MAX; i++)
    {
        char value[APP_UI_KEY_MAX];
        char base[APP_UI_KEY_MAX];
        char mod1[APP_UI_KEY_MAX];
        char mod2[APP_UI_KEY_MAX];
        char mod3[APP_UI_KEY_MAX];

        if (i == S_SPC)
            continue;

        strnfmt(value, sizeof(value), "%d", p_ptr->skill_use[i]);
        strnfmt(base, sizeof(base), "%d", p_ptr->skill_base[i]);
        mod1[0] = '\0';
        mod2[0] = '\0';
        mod3[0] = '\0';
        if (p_ptr->skill_stat_mod[i] != 0)
            strnfmt(mod1, sizeof(mod1), "%+d", p_ptr->skill_stat_mod[i]);
        if (p_ptr->skill_equip_mod[i] != 0)
            strnfmt(mod2, sizeof(mod2), "%+d", p_ptr->skill_equip_mod[i]);
        if (p_ptr->skill_misc_mod[i] != 0)
            strnfmt(mod3, sizeof(mod3), "%+d", p_ptr->skill_misc_mod[i]);

        if (!app_ui_panel_add_character_stat(panel, TERM_WHITE,
                skill_names_full[i], TERM_L_GREEN, value, TERM_SLATE, '=',
                TERM_GREEN, base, TERM_SLATE, mod1, TERM_SLATE, mod2,
                TERM_SLATE, mod3))
        {
            return false;
        }
    }

    return true;
}

static bool character_sheet_build_history(app_ui_scene* scene,
    app_ui_panel* panel)
{
    byte story = story_character_enabled() ? STORY_FLAG_USE : 0;

    if (!scene || !panel || !p_ptr->history[0])
        return true;

    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    return app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE, story,
        p_ptr->history);
}

static app_ui_panel* character_sheet_begin_ui_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    char name[64];

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_CHARACTER_SHEET;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    character_sheet_build_name(name, sizeof(name));
    app_ui_panel_set_title(panel, p_ptr->oaths_broken ? TERM_RED : TERM_L_BLUE,
        name);

    return panel;
}

static bool character_sheet_build_core(app_ui_scene* scene,
    app_ui_panel* panel)
{
    if (!scene || !panel)
        return false;

    if (!character_sheet_build_summary(panel)
        || !character_sheet_build_traits(panel)
        || !character_sheet_build_stats(panel)
        || !character_sheet_build_history(scene, panel))
    {
        return false;
    }

    return true;
}

bool build_player_subwindow_ui_scene(app_ui_scene* scene)
{
    app_ui_panel* panel = character_sheet_begin_ui_scene(scene);

    if (!panel)
        return false;

    return character_sheet_build_core(scene, panel);
}

bool build_character_sheet_ui_scene(app_ui_scene* scene, cptr prompt_text)
{
    app_ui_panel* panel = character_sheet_begin_ui_scene(scene);

    if (!panel)
        return false;
    if (!character_sheet_build_core(scene, panel))
        return false;

    if (prompt_text && prompt_text[0])
    {
        app_ui_panel* strip = app_ui_scene_append_panel(scene,
            APP_UI_LAYER_CHROME);

        if (!strip)
            return false;
        strip->style = APP_UI_PANEL_STYLE_STRIP;
        strip->flags |= APP_UI_PANEL_FLAG_BOTTOM_ANCHORED;
        if (!app_ui_panel_add_body_line(strip, TERM_L_WHITE, prompt_text))
            return false;
    }

    return true;
}

