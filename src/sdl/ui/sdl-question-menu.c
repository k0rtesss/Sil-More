/*
 * Question overlay: a titled panel with an optional wrapped description
 * block and selectable answer rows.  Local questions ("Bash the door?",
 * "Step on the trap?") anchor next to the map grid they are about; global
 * questions centre on the map view, matching the description overlay
 * chrome.  The game side fills it via sdl_question_menu_begin/add_entry/
 * finish and keeps running its own inkey loop; pointer input is fed back
 * through the ui_menu_click pending-choice mechanism, mirroring the song
 * menu.
 */

#include "angband.h"
#include "sdl/main-sdl-private.h"

#include <math.h>

#define SDL_QUESTION_MENU_MAX_COLUMNS 4

typedef struct sdl_question_menu_layout_info {
    SDL_FRect panel;
    SDL_FRect title_row;
    SDL_FRect desc_rect;
    SDL_FRect entries_rect;
    SDL_FRect info_rect;
    SDL_FRect close_rect;
    SDL_FRect suppress_rect;
    SDL_FRect scroll_track;
    SDL_FRect scroll_thumb;
    SDL_FRect rows[SDL_QUESTION_MENU_MAX_ENTRIES];
    SDL_FRect buttons[SDL_QUESTION_MENU_MAX_BUTTONS];
    float divider_y;
    float letter_w;
    float letter_gap;
    float icon_w;
    float icon_size;
    float table_column_w[SDL_QUESTION_MENU_MAX_COLUMNS];
    float table_gap;
    float divider_gap;
    float row_h;
    float help_desc_h;
    float help_content_h;
    int font_px;
    int table_column_count;
    int first_entry;
    int visible_count;
    int max_scroll_offset;
    int button_count;
    bool has_title;
    bool has_desc;
    bool has_divider;
    bool has_table;
    bool compact_table;
    bool close_button;
    bool suppress_button;
    bool info_button;
    bool scrollable;
} sdl_question_menu_layout_info;

typedef struct sdl_question_menu_touch_state {
    bool active;
    bool dragged;
    bool close_pressed;
    bool scroll_wake_sent;
    SDL_FingerID finger_id;
    int choice;
    float start_x;
    float start_y;
    float last_y;
    float accum_y;
} sdl_question_menu_touch_state;

static sdl_question_menu_touch_state g_question_menu_touch;
static bool g_question_menu_touch_scrolled = false;

/* Pixel rect of a map cell on the main view, or false when it is off the
 * current panel.  Shared with the yes/no prompt anchoring. */
bool sdl_map_grid_cell_rect(int y, int x, SDL_FRect* out)
{
    int cell_cols = use_bigtile ? 2 : 1;
    int term_row;
    int term_col;

    if (!character_generated || !character_dungeon || !p_ptr
        || !g_views[PANE_MAIN].term_ready)
    {
        return false;
    }
    if (!panel_contains(y, x))
        return false;

    term_row = ROW_MAP + (y - p_ptr->wy);
    term_col = COL_MAP + (x - p_ptr->wx) * cell_cols;
    return sdl_main_cell_rect(term_col, term_row, cell_cols, 1, out);
}

static int sdl_question_menu_visible_monster_overlaps(
    const SDL_FRect* panel)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int map_cols;
    int map_rows;
    int overlaps = 0;

    if (!panel || !p_ptr || !character_dungeon || !view->term_ready)
        return 0;

    map_cols = sdl_main_view_visual_cols(view) / (use_bigtile ? 2 : 1);
    map_rows = sdl_main_view_visual_rows(view);

    for (int y = p_ptr->wy; y < p_ptr->wy + map_rows; y++)
    {
        if (y < 0 || y >= p_ptr->cur_map_hgt)
            continue;

        for (int x = p_ptr->wx; x < p_ptr->wx + map_cols; x++)
        {
            SDL_FRect monster_cell;
            int m_idx;

            if (x < 0 || x >= p_ptr->cur_map_wid)
                continue;

            m_idx = cave_m_idx[y][x];
            if (m_idx <= 0 || !mon_list[m_idx].ml)
                continue;
            if (!sdl_map_grid_cell_rect(y, x, &monster_cell))
                continue;
            if (sdl_description_overlay_rects_intersect(panel,
                    &monster_cell))
            {
                overlaps++;
            }
        }
    }

    return overlaps;
}

static void sdl_question_menu_consider_context_hint_position(
    SDL_FRect* best, int* best_overlaps, float* best_distance,
    float candidate_x, float candidate_y, float min_x, float max_x,
    float min_y, float max_y, const SDL_FRect* anchor_cell,
    const SDL_FRect* preferred)
{
    SDL_FRect candidate;
    float dx;
    float dy;
    float distance;
    int overlaps;

    if (!best || !best_overlaps || !best_distance || !anchor_cell
        || !preferred || max_x < min_x || max_y < min_y)
    {
        return;
    }

    candidate = *best;
    candidate.x = candidate_x;
    candidate.y = candidate_y;
    if (candidate.x < min_x)
        candidate.x = min_x;
    if (candidate.x > max_x)
        candidate.x = max_x;
    if (candidate.y < min_y)
        candidate.y = min_y;
    if (candidate.y > max_y)
        candidate.y = max_y;

    if (sdl_description_overlay_rects_intersect(&candidate, anchor_cell))
        return;

    overlaps = sdl_question_menu_visible_monster_overlaps(&candidate);
    dx = candidate.x - preferred->x;
    dy = candidate.y - preferred->y;
    distance = dx * dx + dy * dy;

    if (overlaps < *best_overlaps
        || (overlaps == *best_overlaps && distance < *best_distance))
    {
        *best = candidate;
        *best_overlaps = overlaps;
        *best_distance = distance;
    }
}

/*
 * Context shortcut palettes are nonmodal, so they can move farther from their
 * anchor than an interactive question.  Prefer the normal below/above
 * placement, but move to the nearest clear part of the visible map when that
 * would hide a monster.
 */
static void sdl_question_menu_place_context_hint(
    SDL_FRect* panel, const SDL_FRect* anchor_cell, const sdl_view* view,
    float view_x, float view_y, float view_w, float view_h, float gap)
{
    SDL_FRect preferred;
    SDL_FRect best;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float best_distance = 0.0f;
    int best_overlaps;
    int map_cols;
    int map_rows;

    if (!panel || !anchor_cell || !view || !g_question_menu.context_hint)
        return;

    preferred = *panel;
    best = *panel;
    min_x = view_x + gap;
    max_x = view_x + view_w - gap - panel->w;
    min_y = view_y + gap;
    max_y = view_y + view_h - gap - panel->h;
    best_overlaps = sdl_question_menu_visible_monster_overlaps(&best);
    if (best_overlaps == 0)
        return;

#define CONTEXT_HINT_CONSIDER(x_, y_)                                        \
    sdl_question_menu_consider_context_hint_position(&best, &best_overlaps,  \
        &best_distance, (x_), (y_), min_x, max_x, min_y, max_y, anchor_cell, \
        &preferred)

    CONTEXT_HINT_CONSIDER(
        anchor_cell->x + anchor_cell->w * 0.5f - panel->w * 0.5f,
        anchor_cell->y + anchor_cell->h + gap);
    CONTEXT_HINT_CONSIDER(
        anchor_cell->x + anchor_cell->w * 0.5f - panel->w * 0.5f,
        anchor_cell->y - panel->h - gap);
    CONTEXT_HINT_CONSIDER(anchor_cell->x + anchor_cell->w + gap,
        anchor_cell->y + anchor_cell->h * 0.5f - panel->h * 0.5f);
    CONTEXT_HINT_CONSIDER(anchor_cell->x - panel->w - gap,
        anchor_cell->y + anchor_cell->h * 0.5f - panel->h * 0.5f);
    CONTEXT_HINT_CONSIDER(min_x, min_y);
    CONTEXT_HINT_CONSIDER(max_x, min_y);
    CONTEXT_HINT_CONSIDER(min_x, max_y);
    CONTEXT_HINT_CONSIDER(max_x, max_y);

    map_cols = sdl_main_view_visual_cols(view) / (use_bigtile ? 2 : 1);
    map_rows = sdl_main_view_visual_rows(view);
    for (int y = p_ptr->wy; y < p_ptr->wy + map_rows; y++)
    {
        if (y < 0 || y >= p_ptr->cur_map_hgt)
            continue;

        for (int x = p_ptr->wx; x < p_ptr->wx + map_cols; x++)
        {
            SDL_FRect monster_cell;
            int m_idx;

            if (x < 0 || x >= p_ptr->cur_map_wid)
                continue;

            m_idx = cave_m_idx[y][x];
            if (m_idx <= 0 || !mon_list[m_idx].ml
                || !sdl_map_grid_cell_rect(y, x, &monster_cell))
            {
                continue;
            }

            CONTEXT_HINT_CONSIDER(preferred.x,
                monster_cell.y - panel->h - gap);
            CONTEXT_HINT_CONSIDER(preferred.x,
                monster_cell.y + monster_cell.h + gap);
            CONTEXT_HINT_CONSIDER(monster_cell.x - panel->w - gap,
                preferred.y);
            CONTEXT_HINT_CONSIDER(monster_cell.x + monster_cell.w + gap,
                preferred.y);
        }
    }

    /*
     * A dense group can defeat the edge candidates above.  Search cell-aligned
     * positions only in that uncommon case.
     */
    if (best_overlaps > 0)
    {
        float step_x = MAX(1.0f, (float)view->cell_w);
        float step_y = MAX(1.0f, (float)view->cell_h);

        for (float y = min_y; y <= max_y; y += step_y)
        {
            for (float x = min_x; x <= max_x; x += step_x)
                CONTEXT_HINT_CONSIDER(x, y);
        }
    }

#undef CONTEXT_HINT_CONSIDER

    *panel = best;
}

static void sdl_question_menu_draw_text_aux(TTF_Font* font, cptr text,
    SDL_Color color, float x, float y, float max_w, float row_h,
    bool center, bool wrap)
{
    SDL_Texture* texture;
    SDL_FRect src;
    SDL_FRect dst;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f || row_h <= 0.0f)
        return;

    texture = wrap
        ? sdl_ui_wrapped_text_texture(font, text,
              MAX(1, (int)(max_w + 0.5f)), color, &text_w, &text_h)
        : sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return;

    if (text_h > 0 && (float)text_h > row_h * 0.94f)
        scale = (row_h * 0.94f) / (float)text_h;
    if (text_w > 0 && (float)text_w * scale > max_w)
        scale = max_w / (float)text_w;

    src = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)text_w,
        .h = (float)text_h,
    };
    dst = (SDL_FRect){
        .x = x,
        .y = y,
        .w = (float)text_w * scale,
        .h = (float)text_h * scale,
    };

    if (center && dst.w < max_w)
        dst.x = x + (max_w - dst.w) * 0.5f;
    if (wrap)
    {
        float top_pad = (row_h - dst.h) * 0.5f;

        if (top_pad < 0.0f)
            top_pad = 0.0f;
        if (top_pad > 2.0f)
            top_pad = 2.0f;
        dst.y = y + top_pad;
    }
    else
    {
        dst.y = y + (row_h - dst.h) * 0.5f;
    }

    SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
}

static void sdl_question_menu_draw_text(TTF_Font* font, cptr text,
    SDL_Color color, float x, float y, float max_w, float row_h,
    bool center)
{
    sdl_question_menu_draw_text_aux(font, text, color, x, y, max_w, row_h,
        center, false);
}

static void sdl_question_menu_draw_wrapped_text(TTF_Font* font, cptr text,
    SDL_Color color, float x, float y, float max_w, float row_h)
{
    sdl_question_menu_draw_text_aux(font, text, color, x, y, max_w, row_h,
        false, true);
}

/* A tab-separated entry is rendered as aligned columns.  Keep this private
 * to the question overlay so normal labels continue to be plain text. */
static int sdl_question_menu_split_columns(cptr text,
    char columns[][SDL_QUESTION_MENU_TEXT_LEN])
{
    size_t start = 0;
    size_t length;
    int count = 0;

    if (!columns)
        return 0;

    memset(columns, 0,
        sizeof(char) * SDL_QUESTION_MENU_MAX_COLUMNS
            * SDL_QUESTION_MENU_TEXT_LEN);
    if (!text)
        return 0;

    length = strlen(text);
    for (size_t i = 0; i <= length && count < SDL_QUESTION_MENU_MAX_COLUMNS;
        i++)
    {
        if (i == length || text[i] == '\t')
        {
            size_t column_length = i - start;

            if (column_length >= SDL_QUESTION_MENU_TEXT_LEN)
                column_length = SDL_QUESTION_MENU_TEXT_LEN - 1;
            memcpy(columns[count], text + start, column_length);
            columns[count][column_length] = '\0';
            count++;
            start = i + 1;
        }
    }

    return count;
}

/* Portrait rows keep the short role column, but let the weapon description
 * and its attack/damage summary flow together through all remaining width. */
static int sdl_question_menu_display_columns(cptr text, bool compact,
    char columns[][SDL_QUESTION_MENU_TEXT_LEN])
{
    int count = sdl_question_menu_split_columns(text, columns);

    if (!compact || count <= 2)
        return count;

    for (int column = 2; column < count; column++)
    {
        if (!columns[column][0])
            continue;
        if (columns[1][0])
        {
            SDL_strlcat(columns[1], " ",
                SDL_QUESTION_MENU_TEXT_LEN);
        }
        SDL_strlcat(columns[1], columns[column],
            SDL_QUESTION_MENU_TEXT_LEN);
    }
    return 2;
}

static float sdl_question_menu_text_width(TTF_Font* font, cptr text,
    int font_px)
{
    int width;

    if (!text || !text[0])
        return 0.0f;

    width = sdl_touch_pane_story_text_width(font, text);
    if (width > 0)
        return (float)width;

    return (float)strlen(text) * (float)font_px * 0.55f;
}

static int sdl_question_menu_wrapped_text_height(TTF_Font* font, cptr text,
    float wrap_w)
{
    int width = 0;
    int height = 0;
    int wrap_width;

    if (!font || !text || !text[0] || wrap_w <= 0.0f)
        return 0;

    wrap_width = MAX(1, (int)(wrap_w + 0.5f));
    if (!TTF_GetStringSizeWrapped(font, text, 0, wrap_width, &width,
            &height))
    {
        return 0;
    }
    return height;
}

/* Preserve the short role/stat columns and give the object-name column the
 * remaining width when a tabular question row is narrower than its ideal
 * content width.  All columns retain enough room to wrap at word boundaries
 * on a narrow panel. */
static void sdl_question_menu_fit_table_columns(float widths[], int count,
    float available_w, float table_gap, int font_px)
{
    float desired_w = 0.0f;
    float min_w[SDL_QUESTION_MENU_MAX_COLUMNS] = { 0 };
    float available_columns;
    float minimum_total = 0.0f;
    float overflow;

    if (!widths || count <= 0 || count > SDL_QUESTION_MENU_MAX_COLUMNS)
        return;

    available_columns = available_w - table_gap * (float)(count - 1);
    if (available_columns <= 1.0f)
    {
        float column_w = MAX(1.0f, available_w / (float)count);

        for (int column = 0; column < count; column++)
            widths[column] = column_w;
        return;
    }

    for (int column = 0; column < count; column++)
    {
        desired_w += widths[column];
        if (count >= 3 && column == count - 1)
            min_w[column] = widths[column];
        else
            min_w[column] = MIN(widths[column], (float)font_px * 4.0f);
        minimum_total += min_w[column];
    }

    if (desired_w <= available_columns)
        return;

    if (minimum_total >= available_columns)
    {
        float scale = available_columns / desired_w;

        for (int column = 0; column < count; column++)
            widths[column] = MAX(1.0f, widths[column] * scale);
        return;
    }

    overflow = desired_w - available_columns;
    if (count == 2)
    {
        float shrink = MIN(overflow, widths[1] - min_w[1]);

        widths[1] -= shrink;
        overflow -= shrink;
    }
    else
    {
        for (int column = 1; column < count - 1 && overflow > 0.0f;
            column++)
        {
            float shrink = MIN(overflow, widths[column] - min_w[column]);

            widths[column] -= shrink;
            overflow -= shrink;
        }
    }

    for (int column = 0; column < count && overflow > 0.0f; column++)
    {
        float shrink = MIN(overflow, widths[column] - min_w[column]);

        widths[column] -= shrink;
        overflow -= shrink;
    }
}

/* Height of the wrapped description block at the given width. */
static float sdl_question_menu_desc_height(TTF_Font* font, float wrap_w)
{
    int width = 0;
    int height = 0;
    int wrap_width;

    if (!g_question_menu.desc[0] || !font || wrap_w <= 0.0f)
        return 0.0f;

    wrap_width = MAX(1, (int)(wrap_w + 0.5f));
    if (!TTF_GetStringSizeWrapped(font, g_question_menu.desc, 0,
            wrap_width, &width, &height))
    {
        return 0.0f;
    }
    return (float)height;
}

static bool sdl_question_menu_close_button_enabled(void)
{
    return g_question_menu.active && !g_question_menu.blocking_input;
}

static bool sdl_question_menu_suppress_button_enabled(void)
{
    return g_question_menu.active && g_question_menu.context_hint;
}

static bool sdl_question_menu_layout(sdl_question_menu_layout_info* out)
{
    SDL_Rect anchor;
    SDL_FRect anchor_cell;
    TTF_Font* story_font;
    TTF_Font* mono_font;
    int font_px;
    int margin;
    float pad_x;
    float pad_y;
    float row_h;
    float entry_heights[SDL_QUESTION_MENU_MAX_ENTRIES] = { 0 };
    float entries_h = 0.0f;
    float divider_gap;
    float text_w = 0.0f;
    float title_w = 0.0f;
    float letter_w = 0.0f;
    float letter_gap;
    float icon_w = 0.0f;
    float icon_size = 0.0f;
    float table_column_w[SDL_QUESTION_MENU_MAX_COLUMNS] = { 0 };
    float table_gap = 0.0f;
    float close_reserve = 0.0f;
    float info_reserve = 0.0f;
    float suppress_reserve = 0.0f;
    float button_widths[SDL_QUESTION_MENU_MAX_BUTTONS] = { 0 };
    float button_gap = 0.0f;
    float button_total_w = 0.0f;
    float button_section_h = 0.0f;
    int button_count;
    float content_w;
    float desc_h = 0.0f;
    float help_desc_h = 0.0f;
    float help_content_h = 0.0f;
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float rows_top;
    float rows_bottom;
    float rows_h;
    float first_entry_top = 0.0f;
    int scroll_offset = 0;
    bool anchored = false;
    bool close_button;
    bool info_button;
    bool suppress_button;
    bool header_row;
    bool has_table = false;
    bool compact_table;
    int table_column_count = 0;
    bool has_icons = false;

    if (!out)
        return false;
    *out = (sdl_question_menu_layout_info){ 0 };

    if (!g_question_menu.active
        || (g_question_menu.count <= 0
            && g_question_menu.button_count <= 0))
        return false;
    if (!sdl_overlay_pane_anchor_rect(PANE_DESCRIPTION, &anchor))
        return false;
    compact_table = anchor.h > anchor.w;

    font_px = sdl_main_menu_pane_font_px();
#if SIL_SDL_MOBILE_BUILD
    font_px = (int)((float)font_px * 1.18f + 0.5f);
#endif
    if (g_question_menu.context_hint)
    {
        font_px = (int)((float)font_px * 0.78f + 0.5f);
        if (font_px < 13)
            font_px = 13;
    }
    story_font = sdl_story_font_for_height_slot(font_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!story_font)
        return false;
    mono_font = sdl_main_menu_mono_font_for_height(font_px);
    table_gap = sdl_touch_pane_clampf((float)font_px * 0.45f, 8.0f,
        16.0f);

    for (int i = 0; i < g_question_menu.count; i++)
    {
        const sdl_question_menu_entry_state* entry
            = &g_question_menu.entries[i];
        float w;

        if (strchr(entry->text, '\t'))
        {
            char columns[SDL_QUESTION_MENU_MAX_COLUMNS]
                [SDL_QUESTION_MENU_TEXT_LEN];
            int column_count = sdl_question_menu_display_columns(
                entry->text, compact_table, columns);

            has_table = true;
            if (column_count > table_column_count)
                table_column_count = column_count;
            for (int column = 0; column < column_count; column++)
            {
                w = sdl_question_menu_text_width(story_font,
                    columns[column], font_px);
                if (w > table_column_w[column])
                    table_column_w[column] = w;
            }
        }
        else
        {
            w = sdl_question_menu_text_width(story_font, entry->text,
                font_px);

            if (w > text_w)
                text_w = w;
        }
        if (entry->letter[0])
        {
            w = mono_font
                ? sdl_question_menu_text_width(mono_font, entry->letter,
                      font_px)
                : (float)strlen(entry->letter) * (float)font_px * 0.5f;
            if (w > letter_w)
                letter_w = w;
        }
        if (entry->has_icon)
            has_icons = true;
    }

    if (has_table)
    {
        float table_w = 0.0f;

        for (int column = 0; column < table_column_count; column++)
        {
            table_w += table_column_w[column];
            if (column > 0)
                table_w += table_gap;
        }
        if (table_w > text_w)
            text_w = table_w;
    }

    out->has_title = (g_question_menu.title[0] != '\0');
    if (out->has_title)
    {
        title_w = sdl_question_menu_text_width(story_font,
            g_question_menu.title, font_px);
    }

    margin = sdl_overlay_margin_px();
    pad_x = sdl_touch_pane_clampf((float)font_px * 0.68f, 10.0f, 18.0f);
    pad_y = sdl_touch_pane_clampf((float)font_px * 0.55f, 8.0f, 16.0f);
    row_h = (float)font_px * 1.24f;
    if (row_h < (float)font_px + 4.0f)
        row_h = (float)font_px + 4.0f;
    divider_gap = sdl_touch_pane_clampf((float)font_px * 0.3f, 3.0f, 8.0f);
    button_gap = sdl_touch_pane_clampf((float)font_px * 0.42f, 6.0f, 14.0f);
    letter_gap = letter_w > 0.0f
        ? sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f, 10.0f)
        : 0.0f;
    if (has_icons)
    {
        float icon_gap = sdl_touch_pane_clampf((float)font_px * 0.25f,
            4.0f, 8.0f);

        icon_size = MAX(1.0f, row_h - 4.0f);
        icon_w = row_h + icon_gap;
    }
    close_button = sdl_question_menu_close_button_enabled();
    if (close_button)
    {
        close_reserve = row_h
            + sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f,
                10.0f);
    }
    suppress_button = sdl_question_menu_suppress_button_enabled();
    if (suppress_button)
        suppress_reserve = close_reserve;
    info_button = g_question_menu.help_mode && g_question_menu.desc[0];
    if (info_button)
    {
        info_reserve = row_h
            + sdl_touch_pane_clampf((float)font_px * 0.38f, 5.0f,
                10.0f);
    }
    header_row = out->has_title || close_button || info_button
        || suppress_button;
    button_count = g_question_menu.button_count;
    if (button_count < 0)
        button_count = 0;
    if (button_count > SDL_QUESTION_MENU_MAX_BUTTONS)
        button_count = SDL_QUESTION_MENU_MAX_BUTTONS;
    out->button_count = button_count;
    if (button_count > 0)
    {
        float button_pad_x = g_question_menu.context_hint
            ? sdl_touch_pane_clampf(row_h * 0.38f, 7.0f, 14.0f)
            : sdl_touch_pane_clampf(row_h * 0.58f, 12.0f, 24.0f);
        float button_min_w = row_h
            * (g_question_menu.context_hint ? 1.65f : 2.1f);

        button_section_h = row_h + divider_gap;
        for (int i = 0; i < button_count; i++)
        {
            float w = sdl_question_menu_text_width(story_font,
                g_question_menu.buttons[i].text, font_px)
                + button_pad_x * 2.0f;

            if (w < button_min_w)
                w = button_min_w;
            button_widths[i] = w;
            button_total_w += w;
            if (i + 1 < button_count)
                button_total_w += button_gap;
        }
    }

    content_w = letter_w + letter_gap + icon_w + text_w;
    if (out->has_title)
    {
        float title_content_w = title_w + close_reserve + info_reserve
            + suppress_reserve;

        /* Context shortcut bars have room to grow across the map.  Size
         * them from the full measured title instead of shrinking long object
         * names to the old compact-title cap. */
        if (g_question_menu.context_hint)
        {
            title_content_w += sdl_touch_pane_clampf(
                (float)font_px * 0.8f, 10.0f, 22.0f);
        }
        if (title_content_w > content_w)
            content_w = title_content_w;
    }
    if (button_total_w > content_w)
        content_w = button_total_w;
    out->has_desc = !g_question_menu.help_mode
        && (g_question_menu.desc[0] != '\0');
    if (g_question_menu.desc[0])
    {
        /* Give descriptions a comfortable column without letting a short
         * option list force narrow wrapping. */
        float desc_w = sdl_question_menu_text_width(story_font,
            g_question_menu.desc, font_px);
        float desc_max = (float)font_px * 19.0f;

        if (desc_w > desc_max)
            desc_w = desc_max;
        if (desc_w > content_w)
            content_w = desc_w;
    }

    panel_w = content_w + pad_x * 2.0f;
    if (panel_w < (float)font_px
            * (g_question_menu.context_hint ? 7.0f : 9.0f))
    {
        panel_w = (float)font_px
            * (g_question_menu.context_hint ? 7.0f : 9.0f);
    }
    max_panel_w = (float)anchor.w - (float)margin * 2.0f;
    if (max_panel_w < 1.0f)
        max_panel_w = (float)anchor.w;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    /* The ideal table width may exceed the screen after panel_w is capped.
     * Fit its columns before measuring wrapped row heights, so long weapon
     * and shield names use the actual space available to them. */
    {
        float available_text_w = panel_w - pad_x * 2.0f - letter_w
            - letter_gap - icon_w;
        float row_padding = row_h
            - (float)TTF_GetFontHeight(story_font);

        if (available_text_w < 1.0f)
            available_text_w = 1.0f;
        if (row_padding < 2.0f)
            row_padding = 2.0f;
        if (has_table)
        {
            sdl_question_menu_fit_table_columns(table_column_w,
                table_column_count, available_text_w, table_gap, font_px);
        }

        for (int i = 0; i < g_question_menu.count; i++)
        {
            const sdl_question_menu_entry_state* entry
                = &g_question_menu.entries[i];
            int entry_text_h = 0;

            if (has_table && strchr(entry->text, '\t'))
            {
                char columns[SDL_QUESTION_MENU_MAX_COLUMNS]
                    [SDL_QUESTION_MENU_TEXT_LEN];
                int column_count = sdl_question_menu_display_columns(
                    entry->text, compact_table, columns);

                for (int column = 0;
                    column < table_column_count && column < column_count;
                    column++)
                {
                    int text_h = sdl_question_menu_wrapped_text_height(
                        story_font, columns[column],
                        table_column_w[column]);

                    if (text_h > entry_text_h)
                        entry_text_h = text_h;
                }
            }
            else
            {
                entry_text_h = sdl_question_menu_wrapped_text_height(
                    story_font, entry->text, available_text_w);
            }

            entry_heights[i] = row_h;
            if (entry_text_h > 0)
            {
                entry_heights[i] = MAX(entry_heights[i],
                    (float)entry_text_h + row_padding);
            }
            entries_h += entry_heights[i];
        }
    }

    max_panel_h = (float)anchor.h - (float)margin * 2.0f;
    if (max_panel_h < 1.0f)
        max_panel_h = (float)anchor.h;

    if (out->has_desc)
    {
        desc_h = sdl_question_menu_desc_height(story_font,
            panel_w - pad_x * 2.0f);

        if (desc_h > 0.0f)
        {
            int reserved_rows = g_question_menu.count;
            float reserved_entries_h = 0.0f;
            float reserved_h;

            if (reserved_rows > 3)
                reserved_rows = 3;
            for (int i = 0; i < reserved_rows; i++)
                reserved_entries_h += entry_heights[i];

            reserved_h = pad_y * 2.0f
                + reserved_entries_h
                + divider_gap
                + 4.0f;
            if (out->has_title)
                reserved_h += row_h + divider_gap;
            else if (close_button || suppress_button)
                reserved_h += row_h + divider_gap;
            reserved_h += button_section_h;

            if (max_panel_h > reserved_h)
            {
                float max_desc_h = max_panel_h - reserved_h;

                if (desc_h > max_desc_h)
                    desc_h = max_desc_h;
            }
            else
            {
                desc_h = 0.0f;
            }
        }
    }
    if (g_question_menu.help_mode)
    {
        help_desc_h = sdl_question_menu_desc_height(story_font,
            panel_w - pad_x * 2.0f);
        help_content_h = help_desc_h + entries_h;
        if (help_desc_h > 0.0f && entries_h > 0.0f)
            help_content_h += divider_gap;
    }

    panel_h = pad_y * 2.0f + entries_h;
    if (header_row)
        panel_h += row_h + divider_gap;
    if (desc_h > 0.0f)
        panel_h += desc_h + divider_gap;
    panel_h += button_section_h;
    if (g_question_menu.help_mode)
    {
        float help_panel_h = pad_y * 2.0f + help_content_h;

        if (header_row)
            help_panel_h += row_h + divider_gap;
        if (help_panel_h > panel_h)
            panel_h = help_panel_h;
    }
    if (panel_h > max_panel_h)
        panel_h = max_panel_h;

    /* Local questions sit next to the grid they are about; fall back to the
     * centred placement when the grid is off the visible map. */
    if (g_question_menu.has_anchor
        && sdl_map_grid_cell_rect(g_question_menu.anchor_y,
            g_question_menu.anchor_x, &anchor_cell))
    {
        const sdl_view* view = &g_views[PANE_MAIN];
        float view_x = (float)(view->rect.x + view->margin_x);
        float view_y = (float)(view->rect.y + view->margin_y);
        float view_w = (float)(sdl_main_view_visual_cols(view) * view->cell_w);
        float view_h = (float)(sdl_main_view_visual_rows(view) * view->cell_h);
        float gap = sdl_touch_pane_clampf(anchor_cell.h * 0.45f, 6.0f, 16.0f);

        /* Prefer below the cell, then above; centre horizontally on it. */
        out->panel.x = anchor_cell.x + anchor_cell.w * 0.5f - panel_w * 0.5f;
        out->panel.y = anchor_cell.y + anchor_cell.h + gap;
        if (out->panel.y + panel_h > view_y + view_h - gap)
            out->panel.y = anchor_cell.y - panel_h - gap;

        if (out->panel.x < view_x + gap)
            out->panel.x = view_x + gap;
        if (out->panel.x + panel_w > view_x + view_w - gap)
            out->panel.x = view_x + view_w - gap - panel_w;
        if (out->panel.y < view_y + gap)
            out->panel.y = view_y + gap;
        if (out->panel.y + panel_h > view_y + view_h - gap)
            out->panel.y = view_y + view_h - gap - panel_h;

        out->panel.w = panel_w;
        out->panel.h = panel_h;
        sdl_question_menu_place_context_hint(&out->panel, &anchor_cell, view,
            view_x, view_y, view_w, view_h, gap);
        anchored = true;
    }

    if (!anchored)
    {
        out->panel = (SDL_FRect){
            .x = (float)anchor.x + ((float)anchor.w - panel_w) * 0.5f,
            .y = (float)anchor.y + ((float)anchor.h - panel_h) * 0.5f,
            .w = panel_w,
            .h = panel_h,
        };
    }

    out->font_px = font_px;
    out->letter_w = letter_w;
    out->letter_gap = letter_gap;
    out->icon_w = icon_w;
    out->icon_size = icon_size;
    out->table_gap = table_gap;
    out->divider_gap = divider_gap;
    out->table_column_count = table_column_count;
    out->has_table = has_table;
    out->compact_table = compact_table;
    for (int column = 0; column < table_column_count; column++)
        out->table_column_w[column] = table_column_w[column];
    out->row_h = row_h;
    out->help_desc_h = help_desc_h;
    out->help_content_h = help_content_h;
    out->close_button = close_button;
    out->info_button = info_button;
    out->suppress_button = suppress_button;
    if (out->close_button)
    {
        float close_size = row_h;

        out->close_rect = (SDL_FRect){
            .x = out->panel.x + out->panel.w - pad_x - close_size,
            .y = out->panel.y + pad_y,
            .w = close_size,
            .h = close_size,
        };
    }
    if (out->suppress_button)
    {
        float suppress_size = row_h;

        out->suppress_rect = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = out->panel.y + pad_y,
            .w = suppress_size,
            .h = suppress_size,
        };
    }
    if (out->info_button)
    {
        float info_size = row_h;
        float chrome_gap = sdl_touch_pane_clampf(
            (float)font_px * 0.38f, 5.0f, 10.0f);
        float right = out->panel.x + out->panel.w - pad_x;

        if (out->close_button)
            right = out->close_rect.x - chrome_gap;
        out->info_rect = (SDL_FRect){
            .x = right - info_size,
            .y = out->panel.y + pad_y,
            .w = info_size,
            .h = info_size,
        };
    }

    rows_top = out->panel.y + pad_y;
    if (out->has_title)
    {
        out->title_row = (SDL_FRect){
            .x = out->panel.x + pad_x + suppress_reserve,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f
                - close_reserve - info_reserve - suppress_reserve,
            .h = row_h,
        };
        if (out->title_row.w < 1.0f)
            out->title_row.w = 1.0f;
        out->divider_y = rows_top + row_h + divider_gap * 0.5f;
        out->has_divider = true;
        rows_top += row_h + divider_gap;
    }
    else if (out->close_button || out->suppress_button)
    {
        out->divider_y = rows_top + row_h + divider_gap * 0.5f;
        out->has_divider = true;
        rows_top += row_h + divider_gap;
    }

    if (desc_h > 0.0f)
    {
        out->desc_rect = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f,
            .h = desc_h,
        };
        rows_top += desc_h + divider_gap;
    }
    else
    {
        out->has_desc = false;
    }

    {
        float highlight_top = 0.0f;
        float highlight_bottom = 0.0f;
        float row_y = rows_top;
        float scroll_content_h;
        int highlight_index = 0;
        int first_entry = 0;
        int visible_count = 0;
        int max_scroll_offset;

        rows_bottom = out->panel.y + out->panel.h - pad_y
            - button_section_h;
        rows_h = rows_bottom - rows_top;
        if (rows_h < 1.0f)
            rows_h = 1.0f;

        out->entries_rect = (SDL_FRect){
            .x = out->panel.x + pad_x,
            .y = rows_top,
            .w = out->panel.w - pad_x * 2.0f,
            .h = rows_h,
        };
        if (g_question_menu.help_open)
        {
            first_entry_top = help_desc_h;
            if (help_desc_h > 0.0f && entries_h > 0.0f)
                first_entry_top += divider_gap;
            scroll_content_h = help_content_h;
            max_scroll_offset = (int)ceilf(MAX(0.0f,
                help_content_h - rows_h));
            scroll_offset = g_question_menu.help_scroll_offset;
            if (scroll_offset < 0)
                scroll_offset = 0;
            if (scroll_offset > max_scroll_offset)
                scroll_offset = max_scroll_offset;
            g_question_menu.help_scroll_offset = scroll_offset;

            while (first_entry + 1 < g_question_menu.count
                && first_entry_top + entry_heights[first_entry]
                    <= (float)scroll_offset)
            {
                first_entry_top += entry_heights[first_entry];
                first_entry++;
            }

            row_y = rows_top + first_entry_top - (float)scroll_offset;
            for (int i = first_entry; i < g_question_menu.count; i++)
            {
                if (row_y >= rows_bottom)
                    break;
                row_y += entry_heights[i];
                visible_count++;
            }
        }
        else
        {
            scroll_content_h = entries_h;
            for (int i = 0; i < g_question_menu.count; i++)
            {
                if (g_question_menu.entries[i].choice
                    == g_question_menu.highlight)
                {
                    highlight_index = i;
                    break;
                }
            }
            for (int i = 0; i < highlight_index; i++)
                highlight_top += entry_heights[i];
            highlight_bottom = highlight_top
                + entry_heights[highlight_index];

            max_scroll_offset = (int)ceilf(MAX(0.0f,
                entries_h - rows_h));
            if (g_question_menu.scroll_offset_ptr)
                scroll_offset = *g_question_menu.scroll_offset_ptr;
            else
            {
                scroll_offset = (int)floorf(highlight_top
                    + entry_heights[highlight_index] * 0.5f
                    - rows_h * 0.5f);
            }
            if (scroll_offset < 0)
                scroll_offset = 0;
            if (scroll_offset > max_scroll_offset)
                scroll_offset = max_scroll_offset;

            if (g_question_menu.scroll_follow_highlight)
            {
                if (entry_heights[highlight_index] >= rows_h
                    || highlight_top < (float)scroll_offset)
                {
                    scroll_offset = (int)floorf(highlight_top);
                }
                else if (highlight_bottom > (float)scroll_offset + rows_h)
                {
                    scroll_offset = (int)ceilf(highlight_bottom - rows_h);
                }
                if (scroll_offset < 0)
                    scroll_offset = 0;
                if (scroll_offset > max_scroll_offset)
                    scroll_offset = max_scroll_offset;
            }
            if (g_question_menu.scroll_offset_ptr)
                *g_question_menu.scroll_offset_ptr = scroll_offset;

            while (first_entry + 1 < g_question_menu.count
                && first_entry_top + entry_heights[first_entry]
                    <= (float)scroll_offset)
            {
                first_entry_top += entry_heights[first_entry];
                first_entry++;
            }

            row_y = rows_top + first_entry_top - (float)scroll_offset;
            for (int i = first_entry; i < g_question_menu.count; i++)
            {
                if (row_y >= rows_bottom)
                    break;
                row_y += entry_heights[i];
                visible_count++;
            }
        }

        out->first_entry = first_entry;
        out->visible_count = visible_count;
        out->max_scroll_offset = max_scroll_offset;
        out->scrollable = max_scroll_offset > 0;
        if (out->scrollable)
        {
            float track_w = sdl_touch_pane_clampf(
                (float)font_px * 0.12f, 3.0f, 6.0f);
            float thumb_h = rows_h * rows_h
                / MAX(scroll_content_h, rows_h);
            float min_thumb_h = sdl_touch_pane_clampf(
                (float)font_px * 0.8f, 18.0f, 34.0f);

            if (thumb_h < min_thumb_h)
                thumb_h = min_thumb_h;
            if (thumb_h > rows_h)
                thumb_h = rows_h;
            out->scroll_track = (SDL_FRect){
                .x = out->panel.x + out->panel.w - track_w - 2.0f,
                .y = rows_top,
                .w = track_w,
                .h = rows_h,
            };
            out->scroll_thumb = (SDL_FRect){
                .x = out->scroll_track.x,
                .y = rows_top + (rows_h - thumb_h)
                    * ((float)scroll_offset / (float)max_scroll_offset),
                .w = track_w,
                .h = thumb_h,
            };
        }
    }

    if (button_count > 0)
    {
        float button_area_w = out->panel.w - pad_x * 2.0f;
        float y = out->panel.y + out->panel.h - pad_y - row_h;
        float total_w = button_total_w;
        float x;

        if (button_area_w < 1.0f)
            button_area_w = 1.0f;
        if (total_w > button_area_w)
        {
            float fit_w = (button_area_w
                - button_gap * (float)(button_count - 1))
                / (float)button_count;

            if (fit_w < 1.0f)
                fit_w = button_area_w / (float)button_count;
            total_w = 0.0f;
            for (int i = 0; i < button_count; i++)
            {
                button_widths[i] = fit_w;
                total_w += fit_w;
                if (i + 1 < button_count)
                    total_w += button_gap;
            }
        }

        x = out->panel.x + pad_x + (button_area_w - total_w) * 0.5f;
        if (x < out->panel.x + pad_x)
            x = out->panel.x + pad_x;

        for (int i = 0; i < button_count; i++)
        {
            out->buttons[i] = (SDL_FRect){
                .x = x,
                .y = y,
                .w = button_widths[i],
                .h = row_h,
            };
            x += button_widths[i] + button_gap;
        }
    }

    {
        float row_y = rows_top + first_entry_top - (float)scroll_offset;

        for (int i = out->first_entry;
            i < g_question_menu.count
                && i < out->first_entry + out->visible_count;
            i++)
        {
            out->rows[i] = (SDL_FRect){
                .x = out->panel.x + pad_x,
                .y = row_y,
                .w = out->panel.w - pad_x * 2.0f,
                .h = entry_heights[i],
            };
            row_y += entry_heights[i];
        }
    }

    return true;
}

void sdl_question_menu_cancel_touch(void)
{
    if (g_question_menu_touch.close_pressed && g_question_menu.close_hover)
    {
        g_question_menu.close_hover = false;
        g_state.need_present = true;
    }

    g_question_menu_touch = (sdl_question_menu_touch_state){ 0 };
    g_question_menu_touch.choice = -1;
}

static void sdl_question_menu_render_close_button(
    const sdl_question_menu_layout_info* layout, bool hover)
{
    SDL_FRect rect;
    SDL_Color icon = hover ? (SDL_Color){ 125, 185, 255, 255 }
                           : (SDL_Color){ 220, 224, 232, 235 };
    SDL_Color outline = hover ? (SDL_Color){ 125, 185, 255, 220 }
                              : (SDL_Color){ 210, 216, 226, 150 };
    float stroke;
    float pad;
    int repeats;

    if (!layout || !layout->close_button)
        return;

    rect = layout->close_rect;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, hover ? 26 : 12,
        hover ? 38 : 18, hover ? 58 : 24, hover ? 245 : 220);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderRect(g_state.renderer, &rect);

    stroke = rect.w / 11.0f;
    if (stroke < 1.0f)
        stroke = 1.0f;
    if (stroke > 4.0f)
        stroke = 4.0f;
    pad = rect.w * 0.33f;
    repeats = (int)stroke;
    if (repeats < 1)
        repeats = 1;

    SDL_SetRenderDrawColor(g_state.renderer, icon.r, icon.g, icon.b, icon.a);
    for (int i = 0; i < repeats; i++)
    {
        float offset = (float)i - ((float)repeats - 1.0f) * 0.5f;

        SDL_RenderLine(g_state.renderer, rect.x + pad,
            rect.y + pad + offset, rect.x + rect.w - pad,
            rect.y + rect.h - pad + offset);
        SDL_RenderLine(g_state.renderer, rect.x + pad,
            rect.y + rect.h - pad + offset, rect.x + rect.w - pad,
            rect.y + pad + offset);
    }
}

static void sdl_question_menu_render_info_button(
    const sdl_question_menu_layout_info* layout, TTF_Font* font, bool hover)
{
    SDL_FRect rect;
    SDL_Color outline = hover ? (SDL_Color){ 125, 185, 255, 220 }
                              : (SDL_Color){ 210, 216, 226, 150 };
    SDL_Color text = hover ? (SDL_Color){ 125, 185, 255, 255 }
                           : (SDL_Color){ 220, 224, 232, 235 };

    if (!layout || !layout->info_button || !font)
        return;
    rect = layout->info_rect;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, hover ? 26 : 12,
        hover ? 38 : 18, hover ? 58 : 24, hover ? 245 : 220);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderRect(g_state.renderer, &rect);
    sdl_question_menu_draw_text(font, "?", text, rect.x, rect.y, rect.w,
        rect.h, true);
}

static void sdl_question_menu_render_suppress_button(
    const sdl_question_menu_layout_info* layout, bool hover)
{
    SDL_FRect rect;
    SDL_Color icon = hover ? (SDL_Color){ 125, 185, 255, 255 }
                           : (SDL_Color){ 220, 224, 232, 235 };
    SDL_Color outline = hover ? (SDL_Color){ 125, 185, 255, 220 }
                              : (SDL_Color){ 210, 216, 226, 150 };
    float stroke;
    float pad;
    int repeats;

    if (!layout || !layout->suppress_button)
        return;

    rect = layout->suppress_rect;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, hover ? 26 : 12,
        hover ? 38 : 18, hover ? 58 : 24, hover ? 245 : 220);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, outline.r, outline.g,
        outline.b, outline.a);
    SDL_RenderRect(g_state.renderer, &rect);

    stroke = rect.w / 11.0f;
    if (stroke < 1.0f)
        stroke = 1.0f;
    if (stroke > 4.0f)
        stroke = 4.0f;
    pad = rect.w * 0.30f;
    repeats = (int)stroke;
    if (repeats < 1)
        repeats = 1;

    SDL_SetRenderDrawColor(g_state.renderer, icon.r, icon.g, icon.b, icon.a);
    for (int i = 0; i < repeats; i++)
    {
        float offset = (float)i - ((float)repeats - 1.0f) * 0.5f;
        float y = rect.y + rect.h * 0.5f + offset;

        SDL_RenderLine(g_state.renderer, rect.x + pad, y,
            rect.x + rect.w - pad, y);
    }
}

void sdl_question_menu_clear(void)
{
    if (g_question_menu.active || g_question_menu.count > 0)
        g_state.need_present = true;

    if (g_question_menu.context_hint)
        sdl_object_tooltip_clear();
    sdl_question_menu_cancel_touch();
    g_question_menu_touch_scrolled = false;
    memset(&g_question_menu, 0, sizeof(g_question_menu));
    g_question_menu.highlight = -1;
}

void sdl_question_menu_clear_nonblocking(void)
{
    if (g_question_menu.active && g_question_menu.nonblocking)
        sdl_question_menu_clear();
}

void sdl_question_menu_clear_context_hint(void)
{
    if (g_question_menu.active && g_question_menu.context_hint)
        sdl_question_menu_clear();
}

bool sdl_question_menu_context_hint_active(void)
{
    return g_question_menu.active && g_question_menu.context_hint;
}

void sdl_question_menu_begin(cptr title)
{
    /* The game rebuilds blocking questions after pointer-hover wakeups.  Keep
     * frontend-only chrome and help state stable until the overlay is actually
     * cleared. */
    bool close_hover = g_question_menu.active
        && g_question_menu.close_hover;
    bool suppress_hover = g_question_menu.active
        && g_question_menu.suppress_hover;
    bool help_open = g_question_menu.active
        && g_question_menu.help_mode && g_question_menu.help_open;
    bool help_button_hover = g_question_menu.active
        && g_question_menu.help_button_hover;
    int help_scroll_offset = (g_question_menu.active
            && g_question_menu.help_mode)
        ? g_question_menu.help_scroll_offset : 0;

    memset(&g_question_menu, 0, sizeof(g_question_menu));
    g_question_menu.active = true;
    g_question_menu.highlight = -1;
    g_question_menu.close_hover = close_hover;
    g_question_menu.suppress_hover = suppress_hover;
    g_question_menu.help_open = help_open;
    g_question_menu.help_button_hover = help_button_hover;
    g_question_menu.help_scroll_offset = help_scroll_offset;
    if (title)
        SDL_strlcpy(g_question_menu.title, title,
            sizeof(g_question_menu.title));
    g_state.need_present = true;
}

void sdl_question_menu_set_anchor_grid(int y, int x)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.has_anchor = true;
    g_question_menu.anchor_y = y;
    g_question_menu.anchor_x = x;
    g_state.need_present = true;
}

void sdl_question_menu_set_desc(cptr text)
{
    if (!g_question_menu.active)
        return;

    SDL_strlcpy(g_question_menu.desc, text ? text : "",
        sizeof(g_question_menu.desc));
    g_state.need_present = true;
}

static void sdl_question_menu_add_entry_aux(int choice, cptr letter,
    cptr text, byte attr, const object_type* o_ptr)
{
    sdl_question_menu_entry_state* entry;

    if (!g_question_menu.active)
        return;
    if (g_question_menu.count >= SDL_QUESTION_MENU_MAX_ENTRIES)
        return;
    if (!text || !text[0])
        return;

    entry = &g_question_menu.entries[g_question_menu.count++];
    memset(entry, 0, sizeof(*entry));
    entry->choice = choice;
    entry->text_attr = attr;
    if (o_ptr && o_ptr->k_idx)
    {
        entry->icon_attr = object_attr(o_ptr);
        entry->icon_char = object_char(o_ptr);
        entry->has_icon = true;
    }
    SDL_strlcpy(entry->letter, letter ? letter : "", sizeof(entry->letter));
    SDL_strlcpy(entry->text, text, sizeof(entry->text));
    g_state.need_present = true;
}

void sdl_question_menu_add_button(int choice, cptr text, byte attr)
{
    sdl_question_menu_button_state* button;

    if (!g_question_menu.active)
        return;
    if (g_question_menu.button_count >= SDL_QUESTION_MENU_MAX_BUTTONS)
        return;
    if (!text || !text[0])
        return;

    button = &g_question_menu.buttons[g_question_menu.button_count++];
    memset(button, 0, sizeof(*button));
    button->choice = choice;
    button->text_attr = attr;
    SDL_strlcpy(button->text, text, sizeof(button->text));
    g_state.need_present = true;
}

/* Information-only line: rendered like an entry but never hit-tested. */
void sdl_question_menu_add_text(cptr text, byte attr)
{
    sdl_question_menu_add_entry(-1, "", text, attr);
}

void sdl_question_menu_set_scroll_offset_target(int* offset,
    bool follow_highlight)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.scroll_offset_ptr = offset;
    g_question_menu.scroll_follow_highlight = follow_highlight;
}

bool sdl_question_menu_take_touch_scrolled(void)
{
    bool scrolled = g_question_menu_touch_scrolled;

    g_question_menu_touch_scrolled = false;
    return scrolled;
}

void sdl_question_menu_set_highlight(int choice)
{
    if (!g_question_menu.active)
        return;
    if (g_question_menu.highlight == choice)
        return;

    g_question_menu.highlight = choice;
    g_state.need_present = true;
}

void sdl_question_menu_finish(void)
{
    if (!g_question_menu.active
        || (g_question_menu.count <= 0
            && g_question_menu.button_count <= 0))
    {
        sdl_question_menu_clear();
        return;
    }

    g_state.need_present = true;
}

void sdl_question_menu_set_blocking_input(bool blocking)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.blocking_input = blocking;
}

bool sdl_question_menu_blocks_input(void)
{
    return g_question_menu.active && g_question_menu.blocking_input;
}

/*
 * True only for an interactive overlay such as the in-menu value picker.  A
 * blocking_input popup swallows every event (handled separately) and a
 * nonblocking one deliberately lets general input pass through, so neither
 * should make the overlay capture all pointer/touch input.  Context shortcut
 * buttons are still hit-tested in the ordinary gameplay event path.
 */
bool sdl_question_menu_captures_pointer(void)
{
    return g_question_menu.active
        && !g_question_menu.blocking_input
        && !g_question_menu.nonblocking;
}

void sdl_question_menu_set_nonblocking(bool nonblocking)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.nonblocking = nonblocking;
}

void sdl_question_menu_add_entry(int choice, cptr letter, cptr text,
    byte attr)
{
    sdl_question_menu_add_entry_aux(choice, letter, text, attr, NULL);
}

void sdl_question_menu_add_object_entry(int choice, cptr letter, cptr text,
    byte attr, const object_type* o_ptr)
{
    sdl_question_menu_add_entry_aux(choice, letter, text, attr, o_ptr);
}

void sdl_question_menu_set_context_hint(void)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.blocking_input = false;
    g_question_menu.nonblocking = true;
    g_question_menu.context_hint = true;
    sdl_object_tooltip_clear();
    g_state.need_present = true;
}

void sdl_question_menu_set_help(cptr text)
{
    if (!g_question_menu.active)
        return;

    g_question_menu.help_mode = true;
    SDL_strlcpy(g_question_menu.desc, text ? text : "",
        sizeof(g_question_menu.desc));
    g_state.need_present = true;
}

bool sdl_question_menu_toggle_help(void)
{
    if (!g_question_menu.active || !g_question_menu.help_mode
        || !g_question_menu.desc[0])
    {
        return false;
    }

    g_question_menu.help_open = !g_question_menu.help_open;
    g_question_menu.help_button_hover = false;
    sdl_question_menu_cancel_touch();
    g_state.need_present = true;
    return true;
}

void sdl_question_menu_set_timeout_ms(int ms)
{
    if (!g_question_menu.active)
        return;

    if (ms <= 0)
    {
        g_question_menu.expires_at_ns = 0;
        sdl_question_menu_clear();
        return;
    }

    g_question_menu.expires_at_ns =
        SDL_GetTicksNS() + (Uint64)ms * 1000000ULL;
}

int sdl_question_menu_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 remaining_ns;

    if (!g_question_menu.active || !g_question_menu.expires_at_ns)
        return -1;
    if (now_ns >= g_question_menu.expires_at_ns)
        return 0;

    remaining_ns = g_question_menu.expires_at_ns - now_ns;
    return (int)((remaining_ns + 999999ULL) / 1000000ULL);
}

bool sdl_question_menu_flush_expired(Uint64 now_ns)
{
    if (!g_question_menu.active || !g_question_menu.expires_at_ns
        || now_ns < g_question_menu.expires_at_ns)
        return false;

    sdl_question_menu_clear();
    return true;
}

void sdl_question_menu_render(void)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect shadow;
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    TTF_Font* story_font;
    TTF_Font* mono_font;
    SDL_Rect clip;
    SDL_Rect entries_clip;
    int hover_choice = 0;
    bool has_hover_choice;

    if (sdl_question_menu_flush_expired(SDL_GetTicksNS()))
        return;

    if (!sdl_question_menu_layout(&layout))
        return;

    story_font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!story_font)
        return;
    mono_font = sdl_main_menu_mono_font_for_height(layout.font_px);

    has_hover_choice = ui_menu_click_get_hover_choice(&hover_choice);

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    shadow = layout.panel;
    shadow.x += 3.0f;
    shadow.y += 3.0f;
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 118);
    SDL_RenderFillRect(g_state.renderer, &shadow);

    /* Match the description overlay chrome. */
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 232);
    SDL_RenderFillRect(g_state.renderer, &layout.panel);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 120);
    SDL_RenderRect(g_state.renderer, &layout.panel);

    clip = (SDL_Rect){
        .x = (int)layout.panel.x,
        .y = (int)layout.panel.y,
        .w = (int)(layout.panel.w + 1.0f),
        .h = (int)(layout.panel.h + 1.0f),
    };
    SDL_SetRenderClipRect(g_state.renderer, &clip);

    if (layout.has_title)
    {
        sdl_question_menu_draw_text(story_font,
            g_question_menu.help_open ? "Help" : g_question_menu.title,
            g_state.palette[TERM_WHITE], layout.title_row.x,
            layout.title_row.y, layout.title_row.w, layout.title_row.h,
            true);
    }
    if (layout.has_divider)
    {
        SDL_FRect divider = (SDL_FRect){
            .x = layout.panel.x,
            .y = layout.divider_y,
            .w = layout.panel.w,
            .h = 1.0f,
        };

        SDL_SetRenderDrawColor(g_state.renderer, 95, 105, 112, 104);
        SDL_RenderFillRect(g_state.renderer, &divider);
    }

    if (layout.has_desc)
    {
        int wrap_w = MAX(1, (int)(layout.desc_rect.w + 0.5f));
        int text_w = 0;
        int text_h = 0;
        SDL_Texture* texture = sdl_ui_wrapped_text_texture(story_font,
            g_question_menu.desc, wrap_w, g_state.palette[TERM_L_WHITE],
            &text_w, &text_h);

        if (texture)
        {
            SDL_FRect src = (SDL_FRect){
                .x = 0.0f,
                .y = 0.0f,
                .w = (float)text_w,
                .h = (float)text_h,
            };
            SDL_FRect dst = (SDL_FRect){
                .x = layout.desc_rect.x,
                .y = layout.desc_rect.y,
                .w = (float)text_w,
                .h = (float)text_h,
            };

            if (dst.w > layout.desc_rect.w)
                dst.w = layout.desc_rect.w;
            if (dst.h > layout.desc_rect.h)
            {
                dst.h = layout.desc_rect.h;
                src.h = layout.desc_rect.h;
            }
            SDL_RenderTexture(g_state.renderer, texture, &src, &dst);
        }
    }

    entries_clip = (SDL_Rect){
        .x = (int)floorf(layout.entries_rect.x),
        .y = (int)floorf(layout.entries_rect.y),
        .w = (int)ceilf(layout.entries_rect.w),
        .h = (int)ceilf(layout.entries_rect.h),
    };
    SDL_SetRenderClipRect(g_state.renderer, &entries_clip);

    if (g_question_menu.help_open)
    {
        SDL_FRect divider = {
            .x = layout.entries_rect.x,
            .y = layout.entries_rect.y + layout.help_desc_h
                + layout.divider_gap * 0.5f
                - (float)g_question_menu.help_scroll_offset,
            .w = layout.entries_rect.w,
            .h = 1.0f,
        };

        sdl_question_menu_draw_wrapped_text(story_font,
            g_question_menu.desc, g_state.palette[TERM_L_WHITE],
            layout.entries_rect.x,
            layout.entries_rect.y
                - (float)g_question_menu.help_scroll_offset,
            layout.entries_rect.w, layout.help_desc_h);
        if (layout.help_desc_h > 0.0f && g_question_menu.count > 0)
        {
            SDL_SetRenderDrawColor(g_state.renderer, 95, 105, 112, 104);
            SDL_RenderFillRect(g_state.renderer, &divider);
        }
    }
    {
        for (int i = layout.first_entry;
             i < g_question_menu.count
                 && i < layout.first_entry + layout.visible_count;
             i++)
        {
            const sdl_question_menu_entry_state* entry
                = &g_question_menu.entries[i];
            SDL_FRect row = layout.rows[i];
            bool selectable = (entry->choice >= 0);
            bool selected
                = selectable && entry->choice == g_question_menu.highlight;
            bool hovered = selectable && has_hover_choice
                && hover_choice == entry->choice;
            SDL_Color letter_color = g_state.palette[
                selected || hovered ? TERM_L_BLUE : TERM_SLATE];
            SDL_Color text_color = selected || hovered
                ? accent
                : g_state.palette[entry->text_attr];
            float icon_x = row.x + layout.letter_w + layout.letter_gap;
            float text_x = icon_x + layout.icon_w;
            float text_w = row.w - layout.letter_w - layout.letter_gap
                - layout.icon_w;

            if (selected || hovered)
            {
                SDL_SetRenderDrawColor(g_state.renderer, 36, 47, 62,
                    selected ? 226 : 184);
                SDL_RenderFillRect(g_state.renderer, &row);
                SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g,
                    accent.b, selected ? 225 : 168);
                SDL_RenderRect(g_state.renderer, &row);
            }

            if (entry->letter[0] && layout.letter_w > 0.0f && mono_font)
            {
                sdl_question_menu_draw_text(mono_font, entry->letter,
                    letter_color, row.x, row.y, layout.letter_w,
                    layout.row_h, false);
            }
            if (entry->has_icon && layout.icon_w > 0.0f)
            {
                if (g_state.use_tiles && g_state.tileset
                    && (entry->icon_attr & TILE_FLAG)
                    && (((byte)entry->icon_char) & TILE_FLAG))
                {
                    SDL_FRect icon_dst = (SDL_FRect){
                        .x = icon_x
                            + (layout.row_h - layout.icon_size) * 0.5f,
                        .y = row.y
                            + (layout.row_h - layout.icon_size) * 0.5f,
                        .w = layout.icon_size,
                        .h = layout.icon_size,
                    };

                    SDL_SetTextureAlphaMod(g_state.tileset, 255);
                    sdl_draw_tileset_sprite(entry->icon_attr,
                        entry->icon_char, &icon_dst, false);
                    sdl_restore_tileset_mod();
                }
                else if (mono_font)
                {
                    char symbol[2] = { entry->icon_char, '\0' };
                    byte color_attr = entry->icon_attr;

                    if (color_attr >= MAX_COLORS)
                        color_attr = entry->text_attr;
                    sdl_question_menu_draw_text(mono_font, symbol,
                        g_state.palette[color_attr], icon_x, row.y,
                        layout.icon_w, layout.row_h, false);
                }
            }
            if (layout.has_table && strchr(entry->text, '\t'))
            {
                char columns[SDL_QUESTION_MENU_MAX_COLUMNS]
                    [SDL_QUESTION_MENU_TEXT_LEN];
                int column_count = sdl_question_menu_display_columns(
                    entry->text, layout.compact_table, columns);
                float column_x = text_x;

                for (int column = 0;
                    column < layout.table_column_count
                        && column < column_count; column++)
                {
                    float column_w = layout.table_column_w[column];
                    float remaining_w = text_w - (column_x - text_x);

                    if (remaining_w <= 0.0f)
                        break;
                    if (column_w > remaining_w)
                        column_w = remaining_w;
                    sdl_question_menu_draw_wrapped_text(story_font,
                        columns[column], text_color, column_x, row.y,
                        column_w, row.h);
                    column_x += layout.table_column_w[column]
                        + layout.table_gap;
                }
            }
            else
            {
                sdl_question_menu_draw_wrapped_text(story_font, entry->text,
                    text_color, text_x, row.y, text_w, row.h);
            }
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, &clip);
    if (layout.scrollable)
    {
        SDL_SetRenderDrawColor(g_state.renderer, 120, 130, 145, 70);
        SDL_RenderFillRect(g_state.renderer, &layout.scroll_track);
        SDL_SetRenderDrawColor(g_state.renderer, 150, 190, 235, 205);
        SDL_RenderFillRect(g_state.renderer, &layout.scroll_thumb);
    }

    for (int i = 0; !g_question_menu.help_open
        && i < layout.button_count; i++)
    {
        const sdl_question_menu_button_state* button =
            &g_question_menu.buttons[i];
        SDL_FRect rect = layout.buttons[i];
        bool hovered = (g_question_menu.context_hint
                && g_question_menu.highlight == button->choice)
            || (has_hover_choice && hover_choice == button->choice);
        SDL_Color fill = hovered ? (SDL_Color){ 245, 245, 245, 255 }
                                 : (SDL_Color){ 116, 116, 116, 214 };
        SDL_Color border = hovered ? (SDL_Color){ 0, 0, 0, 255 }
                                   : (SDL_Color){ 28, 28, 28, 224 };
        SDL_Color text = hovered ? g_state.palette[TERM_DARK]
                                 : g_state.palette[button->text_attr];

        SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
            fill.a);
        SDL_RenderFillRect(g_state.renderer, &rect);
        SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
            border.b, border.a);
        SDL_RenderRect(g_state.renderer, &rect);
        sdl_question_menu_draw_text(story_font, button->text, text,
            rect.x + rect.w * 0.08f, rect.y, rect.w * 0.84f, rect.h, true);
    }

    sdl_question_menu_render_suppress_button(&layout,
        g_question_menu.suppress_hover);
    sdl_question_menu_render_info_button(&layout, story_font,
        g_question_menu.help_button_hover || g_question_menu.help_open);
    sdl_question_menu_render_close_button(&layout,
        g_question_menu.close_hover);

    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

static bool sdl_question_menu_close_button_at(float x, float y)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect hit;

    if (!sdl_question_menu_close_button_enabled())
        return false;

    if (!sdl_question_menu_layout(&layout) || !layout.close_button)
        return false;

    hit = layout.close_rect;
    hit.x -= hit.w * 0.75f;
    hit.y = layout.panel.y;
    hit.w = layout.panel.x + layout.panel.w - hit.x;
    hit.h = layout.close_rect.y + layout.close_rect.h - layout.panel.y;
    if (hit.h < layout.close_rect.h)
        hit.h = layout.close_rect.h;

    return sdl_point_in_frect(&hit, x, y);
}

static bool sdl_question_menu_info_button_at(float x, float y)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect hit;

    if (!g_question_menu.help_mode || !g_question_menu.desc[0])
        return false;
    if (!sdl_question_menu_layout(&layout) || !layout.info_button)
        return false;

    hit = layout.info_rect;
    hit.x -= hit.w * 0.2f;
    hit.y -= hit.h * 0.2f;
    hit.w *= 1.2f;
    hit.h *= 1.4f;
    return sdl_point_in_frect(&hit, x, y);
}

static bool sdl_question_menu_suppress_button_at(float x, float y)
{
    sdl_question_menu_layout_info layout;
    SDL_FRect hit;

    if (!sdl_question_menu_suppress_button_enabled())
        return false;

    if (!sdl_question_menu_layout(&layout) || !layout.suppress_button)
        return false;

    hit = layout.suppress_rect;
    hit.x = layout.panel.x;
    hit.y = layout.panel.y;
    hit.w = layout.suppress_rect.x + layout.suppress_rect.w
        + layout.suppress_rect.w * 0.75f - layout.panel.x;
    hit.h = layout.suppress_rect.y + layout.suppress_rect.h
        - layout.panel.y;
    if (hit.h < layout.suppress_rect.h)
        hit.h = layout.suppress_rect.h;

    return sdl_point_in_frect(&hit, x, y);
}

static bool sdl_question_menu_choice_at(float x, float y, int* out_choice,
    bool* out_in_panel)
{
    sdl_question_menu_layout_info layout;

    if (out_choice)
        *out_choice = -1;
    if (out_in_panel)
        *out_in_panel = false;

    if (!sdl_question_menu_layout(&layout))
        return false;
    if (!sdl_point_in_frect(&layout.panel, x, y))
        return false;

    if (out_in_panel)
        *out_in_panel = true;

    for (int i = 0; !g_question_menu.help_open
        && i < layout.button_count; i++)
    {
        if (sdl_point_in_frect(&layout.buttons[i], x, y))
        {
            if (out_choice)
                *out_choice = g_question_menu.buttons[i].choice;
            return true;
        }
    }

    if (!sdl_point_in_frect(&layout.entries_rect, x, y))
        return false;

    for (int i = layout.first_entry;
         i < g_question_menu.count
             && i < layout.first_entry + layout.visible_count;
         i++)
    {
        if (g_question_menu.entries[i].choice < 0)
            continue;
        if (sdl_point_in_frect(&layout.rows[i], x, y))
        {
            if (out_choice)
                *out_choice = g_question_menu.entries[i].choice;
            return true;
        }
    }

    return false;
}

static int sdl_question_menu_scroll_max(
    const sdl_question_menu_layout_info* layout)
{
    if (!layout)
        return 0;

    return (layout->max_scroll_offset > 0)
        ? layout->max_scroll_offset : 0;
}

static bool sdl_question_menu_scroll_offset_by(
    const sdl_question_menu_layout_info* layout, int delta)
{
    int* scroll_offset_ptr;
    int value;
    int clamped;
    int max_scroll_offset;

    scroll_offset_ptr = g_question_menu.help_open
        ? &g_question_menu.help_scroll_offset
        : g_question_menu.scroll_offset_ptr;
    if (!scroll_offset_ptr || delta == 0)
        return false;

    max_scroll_offset = sdl_question_menu_scroll_max(layout);
    value = *scroll_offset_ptr;
    clamped = value + delta;
    if (clamped < 0)
        clamped = 0;
    if (clamped > max_scroll_offset)
        clamped = max_scroll_offset;

    if (clamped == value)
        return false;

    *scroll_offset_ptr = clamped;
    if (!g_question_menu.help_open)
        g_question_menu_touch_scrolled = true;
    g_state.need_present = true;
    return true;
}

bool sdl_question_menu_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel)
{
    sdl_question_menu_layout_info layout;
    int delta;

    if (!wheel || !g_question_menu.active
        || g_question_menu.blocking_input || g_question_menu.nonblocking)
    {
        return false;
    }
    if ((!g_question_menu.help_open && !g_question_menu.scroll_offset_ptr)
        || !sdl_question_menu_layout(&layout))
    {
        return true;
    }

    delta = (int)roundf(-(float)wheel->y * layout.row_h * 3.0f);
    if (delta == 0 && wheel->y != 0.0f)
        delta = (wheel->y > 0.0f) ? -1 : 1;
    if (!g_question_menu.help_open)
        g_question_menu.scroll_follow_highlight = false;
    if (sdl_question_menu_scroll_offset_by(&layout, delta))
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_question_menu_handle_pointer(float x, float y, int action)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    if (sdl_question_menu_info_button_at(x, y))
    {
        if (action == UI_MENU_CLICK_PRIMARY)
        {
            (void)sdl_question_menu_toggle_help();
            g_question_menu.help_button_hover = true;
            if (ui_menu_click_clear_hover(&wake) && wake
                && !g_question_menu.nonblocking)
            {
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
            }
            g_state.need_present = true;
        }
        return true;
    }
    if (sdl_question_menu_close_button_at(x, y))
    {
        g_question_menu.close_hover = true;
        g_state.need_present = true;

        if (g_question_menu.nonblocking)
            sdl_question_menu_clear();
        else
            Term_keypress(ESCAPE);

        return true;
    }
    if (sdl_question_menu_suppress_button_at(x, y))
    {
        g_question_menu.suppress_hover = true;
        g_state.need_present = true;

        if (action == UI_MENU_CLICK_PRIMARY)
        {
            sdl_question_menu_clear();
            sdl_enqueue_bypassed_command(CMD_SUPPRESS_CONTEXT_POPUPS);
        }

        return true;
    }
    if (g_question_menu.context_hint)
    {
        if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
            return in_panel;
        if (action != UI_MENU_CLICK_PRIMARY)
            return true;

        sdl_question_menu_clear();
        if (choice == CMD_CONTEXT_FLOOR_ACTION)
            sdl_enqueue_bypassed_command(choice);
        else
            Term_keypress(choice);
        return true;
    }
    if (g_question_menu.nonblocking)
    {
        /*
         * Gameplay remains live around a transient roll/result popup, but its
         * opaque panel owns the pixels it covers.  This also prevents controls
         * drawn beneath it from receiving a tap through the panel.
         */
        (void)sdl_question_menu_choice_at(x, y, &choice, &in_panel);
        return in_panel;
    }
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
        return in_panel;

    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return true;

    g_state.need_present = true;
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY
        : '\r');
    (void)wake;
    return true;
}

bool sdl_question_menu_handle_touch_down(float x, float y,
    SDL_FingerID finger_id)
{
    sdl_question_menu_layout_info layout;
    int choice = -1;
    bool in_panel = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;

    sdl_question_menu_cancel_touch();

    if (sdl_question_menu_info_button_at(x, y))
    {
        (void)sdl_question_menu_toggle_help();
        return true;
    }
    if (sdl_question_menu_close_button_at(x, y))
    {
        g_question_menu_touch.active = true;
        g_question_menu_touch.close_pressed = true;
        g_question_menu_touch.finger_id = finger_id;
        g_question_menu_touch.choice = -1;
        g_question_menu_touch.start_x = x;
        g_question_menu_touch.start_y = y;
        g_question_menu_touch.last_y = y;
        g_question_menu.close_hover = true;
        g_state.need_present = true;
        return true;
    }

    if (g_question_menu.nonblocking)
        return false;
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
    {
        if (!g_question_menu.help_open
            || !sdl_question_menu_layout(&layout)
            || !sdl_point_in_frect(&layout.entries_rect, x, y))
        {
            return in_panel;
        }
        choice = -1;
    }

    g_question_menu_touch.active = true;
    g_question_menu_touch.finger_id = finger_id;
    g_question_menu_touch.choice = choice;
    g_question_menu_touch.start_x = x;
    g_question_menu_touch.start_y = y;
    g_question_menu_touch.last_y = y;
    g_question_menu_touch.accum_y = 0.0f;
    return true;
}

bool sdl_question_menu_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id)
{
    sdl_question_menu_layout_info layout;
    float dx;
    float dy;
    float total_dy;
    int pixel_delta;
    bool changed = false;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;
    if (!g_question_menu_touch.active
        || g_question_menu_touch.finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_question_menu_touch.start_x;
    if (dx < 0.0f)
        dx = -dx;
    dy = y - g_question_menu_touch.last_y;
    g_question_menu_touch.last_y = y;
    total_dy = y - g_question_menu_touch.start_y;
    if (total_dy < 0.0f)
        total_dy = -total_dy;

    if (dx > sdl_touch_swipe_threshold_px()
        || total_dy > sdl_touch_swipe_threshold_px())
    {
        g_question_menu_touch.dragged = true;
    }

    if (g_question_menu_touch.close_pressed)
    {
        bool close_hit = sdl_question_menu_close_button_at(x, y);

        if (g_question_menu.close_hover != close_hit)
        {
            g_question_menu.close_hover = close_hit;
            g_state.need_present = true;
        }
        return true;
    }

    if (!g_question_menu_touch.dragged
        || (!g_question_menu.help_open
            && !g_question_menu.scroll_offset_ptr))
    {
        return true;
    }

    if (!g_question_menu.help_open)
        g_question_menu.scroll_follow_highlight = false;
    if (!sdl_question_menu_layout(&layout))
        return true;

    /* Pointer movement is continuous in pixels; retain only the fractional
     * remainder so wrapped rows of different heights track the finger evenly. */
    g_question_menu_touch.accum_y -= dy;
    pixel_delta = (int)g_question_menu_touch.accum_y;
    if (pixel_delta != 0)
    {
        changed = sdl_question_menu_scroll_offset_by(&layout, pixel_delta);
        if (changed)
            g_question_menu_touch.accum_y -= (float)pixel_delta;
        else
            g_question_menu_touch.accum_y = 0.0f;
    }

    if (changed && !g_question_menu_touch.scroll_wake_sent)
    {
        g_question_menu_touch.scroll_wake_sent = true;
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }

    return true;
}

bool sdl_question_menu_handle_touch_up(float x, float y,
    SDL_FingerID finger_id)
{
    int press_choice;
    int release_choice = -1;
    bool in_panel = false;
    bool close_pressed;
    bool dragged;
    bool wake = false;

    if (!g_question_menu_touch.active
        || g_question_menu_touch.finger_id != finger_id)
    {
        return false;
    }

    press_choice = g_question_menu_touch.choice;
    close_pressed = g_question_menu_touch.close_pressed;
    dragged = g_question_menu_touch.dragged;
    sdl_question_menu_cancel_touch();

    if (dragged)
        return true;

    if (close_pressed)
    {
        if (sdl_question_menu_close_button_at(x, y))
            Term_keypress(ESCAPE);
        return true;
    }

    if (!sdl_question_menu_choice_at(x, y, &release_choice, &in_panel)
        || release_choice != press_choice)
    {
        return true;
    }

    if (!ui_menu_click_handle_choice_action(release_choice,
            UI_MENU_CLICK_PRIMARY, &wake))
    {
        return true;
    }

    g_state.need_present = true;
    Term_keypress('\r');
    (void)wake;
    return true;
}

bool sdl_question_menu_handle_hover_pointer(float x, float y)
{
    int choice = -1;
    bool in_panel = false;
    bool wake = false;
    bool info_hit;
    bool close_hit;
    bool suppress_hit;

    if (!g_question_menu.active)
        return false;
    if (g_question_menu.blocking_input)
        return true;

    info_hit = sdl_question_menu_info_button_at(x, y);
    if (info_hit)
    {
        if (ui_menu_click_clear_hover(&wake) && wake
            && !g_question_menu.nonblocking)
        {
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        if (!g_question_menu.help_button_hover)
        {
            g_question_menu.help_button_hover = true;
            g_state.need_present = true;
        }
        return true;
    }
    if (g_question_menu.help_button_hover)
    {
        g_question_menu.help_button_hover = false;
        g_state.need_present = true;
    }

    close_hit = sdl_question_menu_close_button_at(x, y);
    if (close_hit)
    {
        if (ui_menu_click_clear_hover(&wake) && wake
            && !g_question_menu.nonblocking)
        {
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        if (!g_question_menu.close_hover)
        {
            g_question_menu.close_hover = true;
            g_state.need_present = true;
            if (!g_question_menu.nonblocking)
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        if (g_question_menu.suppress_hover)
        {
            g_question_menu.suppress_hover = false;
            g_state.need_present = true;
        }
        if (g_question_menu.context_hint)
        {
            sdl_question_menu_layout_info layout;

            if (sdl_question_menu_layout(&layout))
            {
                (void)sdl_object_tooltip_show_text_at_rect(
                    &layout.close_rect, "Close this popup.", false);
            }
        }
        return true;
    }
    if (g_question_menu.close_hover)
    {
        g_question_menu.close_hover = false;
        g_state.need_present = true;
        if (!g_question_menu.nonblocking)
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }
    suppress_hit = sdl_question_menu_suppress_button_at(x, y);
    if (suppress_hit)
    {
        sdl_question_menu_layout_info layout;

        if (!g_question_menu.suppress_hover)
        {
            g_question_menu.suppress_hover = true;
            g_state.need_present = true;
        }
        if (sdl_question_menu_layout(&layout))
        {
            (void)sdl_object_tooltip_show_text_at_rect(
                &layout.suppress_rect,
                "Hide these popups for 10 turns.", false);
        }
        return true;
    }
    if (g_question_menu.suppress_hover)
    {
        g_question_menu.suppress_hover = false;
        g_state.need_present = true;
    }
    if (g_question_menu.context_hint)
    {
        if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
        {
            if (in_panel)
                sdl_object_tooltip_clear();
            if (g_question_menu.highlight != -1)
            {
                g_question_menu.highlight = -1;
                g_state.need_present = true;
            }
            return in_panel;
        }
        if (g_question_menu.highlight != choice)
        {
            g_question_menu.highlight = choice;
            g_state.need_present = true;
        }
        /*
         * Labeled action buttons explain themselves.  Keep their hover
         * highlight but clear the map tooltip instead of repeating labels
         * such as "Wield" in a second popup.
         */
        sdl_object_tooltip_clear();
        return true;
    }
    if (g_question_menu.nonblocking)
        return false;
    if (!sdl_question_menu_choice_at(x, y, &choice, &in_panel))
    {
        if (in_panel)
        {
            if (ui_menu_click_clear_hover(&wake) && wake)
                Term_keypress(UI_MENU_CLICK_WAKE_KEY);
            return true;
        }
        return false;
    }

    if (!ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER,
            &wake))
    {
        return true;
    }

    g_state.need_present = true;
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}
