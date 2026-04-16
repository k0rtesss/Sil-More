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
