#ifndef INCLUDED_RUNTIME_DUNGEON_H
#define INCLUDED_RUNTIME_DUNGEON_H

#include "h-basic.h"

#include <stddef.h>

void clear_active_narrative_banner(void);
bool dungeon_active_narrative_banner_animating(u64b now_ms);
bool dungeon_query_active_narrative_banner(u64b now_ms, char* text,
    size_t text_size, u64b* started_ms, u32b* hold_ms);
void death_spectator_view(void);
int p_ptr_depth_proxy(void);

#endif /* INCLUDED_RUNTIME_DUNGEON_H */
