/* ui/story_font.c - Story font rendering helpers */

#include "../angband.h"
#include "../externs.h"
#include "../platform-story-font.h"
#include "story_font.h"

/*
 * Count how many lines text will occupy when using story font with pixel-based wrapping.
 * Similar to count_wrapped_lines but accounts for proportional font width.
 */
int count_wrapped_lines_story(cptr str, int wrap_cols, int indent)
{
    if (!str || wrap_cols <= 0)
        return 1;

    /* Convert column-based wrap to pixel width */
    int cell_width = sdl_get_cell_width();
    int wrap_pixels = wrap_cols * cell_width;
    int indent_pixels = indent * cell_width;
    int space_pixels = sdl_story_font_text_width(" ", 1);
    if (space_pixels <= 0)
        space_pixels = cell_width;

    int lines = 1;
    int x_pixels = indent_pixels;
    cptr s = str;

    while (*s)
    {
        /* Handle newlines */
        if (*s == '\n')
        {
            x_pixels = indent_pixels;
            lines++;
            s++;
            continue;
        }

        /* Skip leading spaces */
        while (*s == ' ')
        {
            x_pixels += space_pixels;
            if (x_pixels >= wrap_pixels)
            {
                x_pixels = indent_pixels;
                lines++;
            }
            s++;
        }

        if (!*s)
            break;

        /* Find the end of the current word */
        cptr word_start = s;
        int word_chars = 0;
        while (s[word_chars] && s[word_chars] != ' ' && s[word_chars] != '\n')
            word_chars++;

        if (word_chars == 0)
            continue;

        /* Measure the word in pixels */
        int word_pixels = sdl_story_font_text_width(word_start, word_chars);
        /* Check if word fits on current line */
        if (x_pixels > indent_pixels && (x_pixels + word_pixels) > wrap_pixels)
        {
            x_pixels = indent_pixels;
            lines++;
        }

        /* Advance by the word's pixel width */
        x_pixels += word_pixels;

        /* Move past the word */
        s += word_chars;
    }

    return lines;
}

static bool story_term_is_main(void)
{
    return platform_frame_active_view_is_main();
}

bool story_inventory_enabled(void)
{
    return story_term_is_main() ? story_inventory_lists : story_inventory_lists_pane;
}

bool story_equipment_enabled(void)
{
    return story_term_is_main() ? story_equipment_lists : story_equipment_lists_pane;
}

bool story_look_enabled(void) { return story_display_lists; }
bool story_character_enabled(void) { return story_character_sheet; }

bool story_monster_desc_enabled(void)
{
    return story_term_is_main() ? story_monster_desc_main : story_monster_desc_pane;
}

void story_font_term_push(bool active, bool grid, story_font_term_state* prev)
{
    if (!prev)
        return;

    prev->active = sdl_is_story_font_enabled();
    prev->grid = sdl_is_story_font_grid();

    if (active && !prev->active)
        sdl_story_font_enable();
    else if (!active && prev->active)
        sdl_story_font_disable();

    sdl_story_font_set_grid(grid);
}

void story_font_term_pop(story_font_term_state* prev)
{
    if (!prev)
        return;

    if (prev->active && !sdl_is_story_font_enabled())
        sdl_story_font_enable();
    else if (!prev->active && sdl_is_story_font_enabled())
        sdl_story_font_disable();

    sdl_story_font_set_grid(prev->grid);
}

void text_out_to_screen_story(byte a, cptr str)
{
    bool was_enabled = sdl_is_story_font_enabled();
    bool was_grid = sdl_is_story_font_grid();

    if (was_enabled)
        sdl_story_font_disable();
    if (was_grid)
        sdl_story_font_set_grid(false);

    text_out_to_screen(a, str);

    if (was_enabled)
        sdl_story_font_enable();
    if (was_grid)
        sdl_story_font_set_grid(true);
}

static void story_print_text_internal(int row, int col, int max_cols, byte attr, cptr text, bool force_grid)
{
    story_font_term_state prev;

    if (!text)
        text = "";

    if (max_cols > 0)
        c_prt(attr, "", row, col);

    story_font_term_push(sdl_is_story_font_enabled(), force_grid, &prev);
    c_put_str(attr, text, row, col);
    story_font_term_pop(&prev);
}

void story_print_text(int row, int col, int max_cols, byte attr, cptr text)
{
    story_print_text_internal(row, col, max_cols, attr, text, false);
}

void story_print_text_grid(int row, int col, int max_cols, byte attr, cptr text)
{
    story_print_text_internal(row, col, max_cols, attr, text, true);
}

void story_print_mono(int row, int col, byte attr, cptr text)
{
    if (!text)
        text = "";

    story_font_term_state prev;
    story_font_term_push(false, sdl_is_story_font_grid(), &prev);
    c_put_str(attr, text, row, col);
    story_font_term_pop(&prev);
}

void story_fill_rect(int row, int col, int width_cols, byte attr)
{
    char spaces[APP_UI_TEXT_MAX];
    int count;

    if (width_cols <= 0)
        return;

    count = MIN(width_cols, (int)sizeof(spaces) - 1);
    memset(spaces, ' ', (size_t)count);
    spaces[count] = '\0';
    c_put_str(attr, spaces, row, col);
}
