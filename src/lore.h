#ifndef INCLUDED_LORE_H
#define INCLUDED_LORE_H

#include "h-basic.h"

/* Canonical data, IDs, saves and asset paths never pass through this layer.
 * Translate complete display strings before measuring, wrapping or clipping.
 * Returned strings are borrowed from a bounded cache. Copy text retained
 * beyond the current UI operation. Input is never changed. */
bool lore_init(void);
void lore_cleanup(void);
bool lore_set_generic(bool enabled);
cptr lore_text(cptr canonical);
void lore_copy(char* dest, size_t size, cptr canonical);
struct SDL_IOStream;
int lore_dump_printf(struct SDL_IOStream* stream, cptr fmt, ...);

#endif
