#ifndef INCLUDED_SDL_DESCRIPTION_H
#define INCLUDED_SDL_DESCRIPTION_H

#include "h-basic.h"

/* Borrowed terminal captures. Their owner retains both views until the
 * description is cleared; changing views never touches game objects. */
typedef struct sdl_description_content
{
    const byte* attrs;
    const char* chars;
    const byte* tattrs;
    const char* tchars;
    const byte* story;
    const byte* health;
    int width;
    int height;
    int target_cols;
} sdl_description_content;

bool sdl_description_overlay_present_views(const sdl_description_content* full,
    const sdl_description_content* comparison, bool comparison_view,
    int scroll, bool interactive, int* visible_rows, int* max_scroll);
bool sdl_description_overlay_toggle_view(void);
bool sdl_description_overlay_comparison_view(void);

#endif
