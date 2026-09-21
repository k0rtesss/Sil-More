#ifndef INCLUDED_CAVE_EVENTS_H
#define INCLUDED_CAVE_EVENTS_H

#include "h-basic.h"

enum cave_event_kind {
    CAVE_EVENT_NONE, CAVE_EVENT_CRACK, CAVE_EVENT_COLLAPSE,
    CAVE_EVENT_FLOOD, CAVE_EVENT_BUILD, CAVE_EVENT_DIG,
    CAVE_EVENT_FREEZE, CAVE_EVENT_THAW, CAVE_EVENT_BRIDGE,
    CAVE_EVENT_VENT, CAVE_EVENT_WARNING, CAVE_EVENT_FIGHT, CAVE_EVENT_MAX
};
#define CAVE_EVENTS_MAX 32

typedef struct cave_world_event {
    u32b serial;
    s32b turn;
    byte kind, y, x, volume;
} cave_world_event;

void cave_events_reset(void);
void cave_events_player_moved(bool moved);
bool cave_events_player_is_moving(void);
void cave_events_process(void);
void cave_event_emit(int kind, int y, int x, int volume);
/* Most recent event on this level, even after its sound has faded. */
bool cave_event_latest(cave_world_event* event);
/* Events describe their physical origin, never the player's location. */
bool cave_event_for_listener(int y, int x, int perception, u32b after,
    cave_world_event* event);
int cave_sound_mask_at(int y, int x);
/* Fighting masks hearing of player sounds, including actions while stationary. */
int cave_fighting_mask_at(int y, int x);
void cave_events_terrain_changed(void);
const cave_world_event* cave_events_state(void);
u32b cave_events_next_serial(void);
void cave_events_restore(const cave_world_event* events, u32b serial);

#endif
