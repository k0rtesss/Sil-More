#include "angband.h"
#include "externs.h"
#include "fs/load-internal.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "monster/monster-world.h"
#include "level-generation/level-generation-terrain-vaults.h"

static environment_cell read_cell(void)
{
    environment_cell c = {0};
    rd_byte(&c.flags); rd_byte(&c.base_feat); rd_byte(&c.known_feat);
    rd_byte(&c.bridge_feat); rd_byte(&c.underlay); rd_byte(&c.material);
    rd_byte(&c.integrity); rd_byte(&c.work); rd_byte(&c.pending_feat);
    rd_byte(&c.owner); rd_byte(&c.known_underlay); rd_byte(&c.known_material);
    rd_s16b(&c.heat); rd_s32b(&c.due);
    return c;
}

static errr invalid_environment(void)
{
    cave_environment_reset();
    note("Invalid living dungeon state.");
    return -1;
}

errr load_read_environment(void)
{
    /* Never carry jobs, old map knowledge or events from a prior game. */
    for (int i = 1; i < mon_max; i++) memset(&mon_list[i].world, 0, sizeof(mon_list[i].world));
    if (!savefile_version_at_least(0,9,8,17)) {
        /* Generation scratch metadata may describe a different game. Legacy
         * authored rooms are conservatively protected by their saved flags. */
        terrain_vault_reset();
        cave_environment_seed();
        return 0;
    }
    cave_environment_reset();
    u32b start = load_byte_offset;
    u16b magic = 0;
    environment_state s = {0};
    rd_u16b(&magic); rd_bool(&s.ready); rd_u32b(&s.random); rd_s32b(&s.last_turn);
    rd_byte(&s.source_count); rd_byte(&s.mineral_budget);
    if (magic != 0xEC17 || load_byte_offset-start != 13 || !cave_environment_restore_state(s))
        return invalid_environment();
    for (int i = 0; i < s.source_count; i++) {
        environment_source source = {0}; start = load_byte_offset;
        rd_byte(&source.y); rd_byte(&source.x); rd_byte(&source.feature);
        rd_byte(&source.capacity); rd_byte(&source.used); rd_byte(&source.phase);
        rd_s32b(&source.next_turn);
        if (load_byte_offset-start != 10 || !cave_environment_restore_source(i,source))
            return invalid_environment();
    }
    int total = p_ptr->cur_map_hgt*p_ptr->cur_map_wid, pos = 0;
    while (pos < total) {
        u16b run = 0; start = load_byte_offset;
        rd_u16b(&run); environment_cell cell = read_cell();
        if (load_byte_offset-start != 20 || !run || run > total-pos) return invalid_environment();
        for (int i = 0; i < run; i++, pos++)
            if (!cave_environment_restore_cell(pos/p_ptr->cur_map_wid, pos%p_ptr->cur_map_wid, cell))
                return invalid_environment();
    }
    cave_world_event events[CAVE_EVENTS_MAX] = {{0}};
    u32b serial = 0; start = load_byte_offset;
    rd_u32b(&serial);
    for (int i = 0; i < CAVE_EVENTS_MAX; i++) {
        cave_world_event* e = &events[i];
        rd_u32b(&e->serial); rd_s32b(&e->turn); rd_byte(&e->kind);
        rd_byte(&e->y); rd_byte(&e->x); rd_byte(&e->volume);
        if (e->kind >= CAVE_EVENT_MAX || e->turn < 0 || e->turn > turn
            || e->volume > 64 || (e->kind && (!in_bounds_fully(e->y,e->x) || !e->serial
                || e->serial > serial))) return invalid_environment();
        for (int j = 0; j < i; j++)
            if (e->serial && events[j].serial == e->serial) return invalid_environment();
    }
    if (load_byte_offset-start != 4+12*CAVE_EVENTS_MAX) return invalid_environment();
    cave_events_restore(events,serial);
    cave_events_player_moved(p_ptr->previous_action[0] >= 1
        && p_ptr->previous_action[0] <= 9 && p_ptr->previous_action[0] != 5);
    u16b count = 0; rd_u16b(&count);
    if (count != mon_max) return invalid_environment();
    for (int i = 1; i < mon_max; i++) {
        monster_world_state* w = &mon_list[i].world; start = load_byte_offset;
        rd_u32b(&w->last_event); rd_byte(&w->initialized);
        rd_byte(&w->observation_kind); rd_byte(&w->observation_y);
        rd_byte(&w->observation_x); rd_byte(&w->observation_age);
        rd_byte(&w->task); rd_byte(&w->target_y); rd_byte(&w->target_x);
        rd_byte(&w->task_age); rd_byte(&w->retries); rd_byte(&w->cooldown);
        rd_byte(&w->home_y); rd_byte(&w->home_x); rd_byte(&w->supplies);
        if (load_byte_offset-start != 18 || w->initialized > 1 || w->task > MON_WORLD_BRIDGE
            || w->observation_kind >= CAVE_EVENT_MAX || w->observation_age > 48
            || w->supplies > 24 || w->task_age > 49 || w->retries > 3 || w->cooldown > 16
            || (w->initialized && !in_bounds_fully(w->home_y,w->home_x))
            || (w->task && !in_bounds_fully(w->target_y,w->target_x))
            || (w->observation_age && !in_bounds_fully(w->observation_y,w->observation_x)))
            return invalid_environment();
    }
    /* Version 19 appends monster diplomacy after the environment block. */
    return savefile_version_at_least(0, 9, 8, 19)
        || load_only_checksums_remain() ? 0 : invalid_environment();
}
