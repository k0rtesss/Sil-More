#ifndef INCLUDED_SDL_BRIDGE_H
#define INCLUDED_SDL_BRIDGE_H

#include <SDL3/SDL.h>

/* Draw the dry deck over an already rendered material underlay. */
void sdl_draw_bridge_deck(int y, int x, const SDL_FRect* dst);

#endif
