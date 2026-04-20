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

#ifndef INCLUDED_PLATFORM_STORY_FONT_H
#define INCLUDED_PLATFORM_STORY_FONT_H

#include "h-basic.h"

void platform_story_font_enable(void);
void platform_story_font_disable(void);
void platform_story_font_reset(void);
bool platform_story_font_enabled(void);
void platform_story_font_set_cell_align(bool enabled);
bool platform_story_font_cell_align_enabled(void);
int platform_story_font_text_width(cptr text, int len);
int platform_story_font_cell_width(void);

#endif /* INCLUDED_PLATFORM_STORY_FONT_H */
