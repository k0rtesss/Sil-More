#ifndef INCLUDED_PLATFORM_STORY_FONT_H
#define INCLUDED_PLATFORM_STORY_FONT_H

#include "h-basic.h"

void sdl_story_font_enable(void);
void sdl_story_font_disable(void);
void sdl_story_font_reset(void);
bool sdl_is_story_font_enabled(void);
void sdl_story_font_set_grid(bool grid);
bool sdl_is_story_font_grid(void);
int sdl_story_font_text_width(cptr text, int len);
int sdl_get_cell_width(void);

#endif /* INCLUDED_PLATFORM_STORY_FONT_H */
