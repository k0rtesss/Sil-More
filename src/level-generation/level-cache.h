#ifndef INCLUDED_LEVEL_CACHE_H
#define INCLUDED_LEVEL_CACHE_H

#include "h-basic.h"

/* Only the gameplay thread owns the cache. The generator receives a private
 * copy of its inputs and publishes a completed level through an atomic flag. */
bool level_cache_in_worker(void);
void level_cache_start(void);
void level_cache_poll(void);
void level_cache_clear(void);
bool level_cache_retains(int depth);
bool level_cache_ready(int depth);
bool level_cache_request(int depth);
bool level_cache_change_level(void);

/* Called by the generator before population and final access validation. */
void level_cache_generation_size(int* blocks);
void level_cache_generation_reserve(void);
bool level_cache_generation_vault_fits(int y1, int x1, int y2, int x2);
bool level_cache_generation_links(void);
void level_cache_generation_finished(void);
bool level_cache_generation_cancelled(void);
void level_cache_generation_preserve_quests(void);

/* Vertical openings are separate from the terrain: a landing can have an
 * opening overhead without itself being a hole in the floor. */
bool level_cache_chasm_above(int y, int x);
int level_cache_chasm_fall_depth(int y, int x);

/* Versioned savefile lane, after the current dungeon. */
void level_cache_write(void);
errr level_cache_read(void);
bool level_cache_reading_floor(void);

#endif
