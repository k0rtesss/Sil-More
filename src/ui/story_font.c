/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

/* ui/story_font.c - Story font rendering helpers */

#include "../angband.h"
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
    int cell_width = platform_story_font_cell_width();
    int wrap_pixels = wrap_cols * cell_width;
    int indent_pixels = indent * cell_width;
    int space_pixels = platform_story_font_text_width(" ", 1);
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
        int word_pixels = platform_story_font_text_width(word_start, word_chars);
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

    prev->active = platform_story_font_enabled();
    prev->grid = platform_story_font_cell_align_enabled();

    if (active && !prev->active)
        platform_story_font_enable();
    else if (!active && prev->active)
        platform_story_font_disable();

    platform_story_font_set_cell_align(grid);
}

void story_font_term_pop(story_font_term_state* prev)
{
    if (!prev)
        return;

    if (prev->active && !platform_story_font_enabled())
        platform_story_font_enable();
    else if (!prev->active && platform_story_font_enabled())
        platform_story_font_disable();

    platform_story_font_set_cell_align(prev->grid);
}
