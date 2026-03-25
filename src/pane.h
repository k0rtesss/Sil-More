#pragma once

#include "pane-config.h"
#include "SDL3/SDL_rect.h"

/* Instance of a pane itself. */
struct pane {
    SDL_Rect rect;
    int index;
};

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, const int* cell_widths, const int* cell_heights,
    int margin);
