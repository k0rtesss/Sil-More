#include "angband.h"
#include "externs.h"
#include "fs/save-internal.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"

static void write_cell(environment_cell c)
{
    wr_byte(c.flags); wr_byte(c.base_feat); wr_byte(c.known_feat);
    wr_byte(c.bridge_feat); wr_byte(c.underlay); wr_byte(c.material);
    wr_byte(c.integrity); wr_byte(c.work); wr_byte(c.pending_feat);
    wr_byte(c.owner); wr_byte(c.known_underlay); wr_byte(c.known_material);
    wr_s16b(c.heat); wr_s32b(c.due);
}

void save_write_environment(void)
{
    environment_state s = cave_environment_get_state();
    wr_u16b(0xEC17);
    wr_byte(s.ready ? 1 : 0); wr_u32b(s.random); wr_s32b(s.last_turn);
    wr_byte(s.source_count); wr_byte(s.mineral_budget);
    for (int i = 0; i < s.source_count; i++) {
        const environment_source* source = cave_environment_source_at(i);
        wr_byte(source->y); wr_byte(source->x); wr_byte(source->feature);
        wr_byte(source->capacity); wr_byte(source->used); wr_byte(source->phase);
        wr_s32b(source->next_turn);
    }
    /* Typed RLE: no struct padding, host byte order, or native bool is written. */
    environment_cell previous = {0};
    u16b run = 0;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++) for (int x = 0; x < p_ptr->cur_map_wid; x++) {
        environment_cell cell = *cave_environment_cell_at(y,x);
        if (run && (memcmp(&previous, &cell, sizeof(cell)) || run == 65535)) {
            wr_u16b(run); write_cell(previous); run = 0;
        }
        previous = cell; run++;
    }
    if (run) { wr_u16b(run); write_cell(previous); }
    wr_u32b(cave_events_next_serial());
    const cave_world_event* events = cave_events_state();
    for (int i = 0; i < CAVE_EVENTS_MAX; i++) {
        wr_u32b(events[i].serial); wr_s32b(events[i].turn);
        wr_byte(events[i].kind); wr_byte(events[i].y);
        wr_byte(events[i].x); wr_byte(events[i].volume);
    }
    /* Called after dungeon compaction: indices match the just-written actors. */
    wr_u16b(mon_max);
    for (int i = 1; i < mon_max; i++) {
        monster_world_state* w = &mon_list[i].world;
        wr_u32b(w->last_event); wr_byte(w->initialized);
        wr_byte(w->observation_kind); wr_byte(w->observation_y);
        wr_byte(w->observation_x); wr_byte(w->observation_age);
        wr_byte(w->task); wr_byte(w->target_y); wr_byte(w->target_x);
        wr_byte(w->task_age); wr_byte(w->retries); wr_byte(w->cooldown);
        wr_byte(w->home_y); wr_byte(w->home_x); wr_byte(w->supplies);
    }
}
