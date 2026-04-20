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

/* ui/story_font.h - Story font helpers and rendering utilities */

#ifndef INCLUDED_UI_STORY_FONT_H
#define INCLUDED_UI_STORY_FONT_H

#include "../h-basic.h"

typedef struct story_font_term_state
{
    bool active;
    bool grid;
} story_font_term_state;

int count_wrapped_lines_story(cptr str, int wrap_cols, int indent);

bool story_inventory_enabled(void);
bool story_equipment_enabled(void);
bool story_look_enabled(void);
bool story_character_enabled(void);
bool story_monster_desc_enabled(void);

void story_font_term_push(bool active, bool grid, story_font_term_state* prev);
void story_font_term_pop(story_font_term_state* prev);

#endif /* INCLUDED_UI_STORY_FONT_H */
