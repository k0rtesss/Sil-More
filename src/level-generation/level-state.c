#include "angband.h"
#include "level-generation/level-state.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
#include "meta_state.h"

level_image* level_image_new(void)
{
    level_image* image = SDL_calloc(1, sizeof(*image));
    if (!image) return NULL;
    image->objects = SDL_calloc(z_info->o_max, sizeof(*image->objects));
    if (!image->objects) { SDL_free(image); return NULL; }
    return image;
}

void level_image_free(level_image* image)
{
    if (!image) return;
    legendary_area_free_runtime_snapshot(image->legendary_runtime);
    SDL_free(image->objects);
    SDL_free(image);
}

level_image* level_image_clone(const level_image* source)
{
    level_image* copy = level_image_new();
    if (!copy) return NULL;
    object_type* objects = copy->objects;
    *copy = *source;
    copy->objects = objects;
    copy->legendary_runtime = legendary_area_clone_runtime_snapshot(source->legendary_runtime);
    if (source->legendary_runtime && !copy->legendary_runtime) {
        level_image_free(copy); return NULL;
    }
    SDL_memcpy(copy->objects, source->objects, z_info->o_max * sizeof(*objects));
    return copy;
}

void level_image_bind_grids(level_image* image)
{
    level_stream_restore_frame(image);
    cave_info = image->info;
    cave_feat = image->feat;
    cave_color = image->color;
    cave_natural = image->natural;
    cave_rewired = image->rewired;
    legendary_area_id = image->legendary;
    cave_light = image->light;
    cave_o_idx = image->objects_at;
    cave_m_idx = image->monsters_at;
    cave_when = image->scent;
    mon_list = image->monsters;
    o_list = image->objects;
}

void level_image_repair_social(level_image* image)
{
    monster_type* saved_list = mon_list;
    s16b saved_max = mon_max;
    mon_list = image->monsters; mon_max = image->mon_max;
    bool changed;
    do {
        changed = false;
        for (int i = 1; i < mon_max; i++) {
            monster_type* m = &mon_list[i];
            if (!m->r_idx) continue;
            bool valid = monster_social_valid(m);
            if (valid && m->social_rival)
                valid = mon_list[m->social_rival].social_rival == i
                    && mon_list[m->social_rival].social_state == m->social_state;
            if (valid && m->social_state == MON_SOCIAL_HELP)
                valid = mon_list[m->social_ally].social_rival == m->social_focus
                    && mon_list[m->social_ally].social_state == MON_SOCIAL_FIGHT;
            if (valid) continue;
            m->social_rival = m->social_focus = m->social_ally = 0;
            m->social_state = m->social_timer = m->social_memory = 0;
            m->social_cooldown = MON_SOCIAL_COOLDOWN;
            changed = true;
        }
    } while (changed);
    mon_list = saved_list; mon_max = saved_max;
}

#define COPY_TO(field, global) SDL_memmove(image->field, global, sizeof(image->field))
#define COPY_FROM(global, field) SDL_memmove(global, image->field, sizeof(image->field))

void level_image_capture(level_image* image)
{
    level_stream_capture_frame(image);
    image->depth = p_ptr->depth;
    image->hgt = p_ptr->cur_map_hgt; image->wid = p_ptr->cur_map_wid;
    image->py = p_ptr->py; image->px = p_ptr->px;
    image->suspended_turn = turn;
    cave_vertical_capture(&image->vertical);
    vertical_vault_capture(&image->vertical_vaults);
    legendary_area_free_runtime_snapshot(image->legendary_runtime);
    image->legendary_runtime = legendary_area_capture_runtime();
    image->feeling = feeling; image->do_feeling = do_feeling;
    image->rating = rating; image->good_item = good_item_flag;
    image->traps = num_trap_on_level; image->truce = p_ptr->truce;
    image->mon_max = mon_max; image->mon_cnt = mon_cnt;
    image->o_max = o_max; image->o_cnt = o_cnt; image->repro = num_repro;
    COPY_TO(vault_name, g_vault_name);
    COPY_TO(info, cave_info); COPY_TO(feat, cave_feat);
    COPY_TO(color, cave_color); COPY_TO(natural, cave_natural);
    COPY_TO(rewired, cave_rewired);
    if (legendary_area_id) COPY_TO(legendary, legendary_area_id);
    else SDL_memset(image->legendary, 0, sizeof(image->legendary));
    COPY_TO(light, cave_light); COPY_TO(objects_at, cave_o_idx);
    COPY_TO(monsters_at, cave_m_idx); COPY_TO(scent, cave_when);
    COPY_TO(wandering_cost, &cave_cost[FLOW_WANDERING_HEAD]); COPY_TO(flow_y, flow_center_y);
    COPY_TO(flow_x, flow_center_x); COPY_TO(update_y, update_center_y);
    COPY_TO(update_x, update_center_x); COPY_TO(wandering, wandering_pause);
    image->scent_when = scent_when;
    COPY_TO(monsters, mon_list);
    SDL_memmove(image->objects, o_list, z_info->o_max * sizeof(*o_list));
    image->partition_rows = current_partition_rows;
    image->partition_cols = current_partition_cols;
    image->partition_count = current_partition_count;
    image->labyrinths = current_labyrinth_partitions;
    COPY_TO(modes, current_partition_modes);
    COPY_TO(densities, current_partition_densities);
    COPY_TO(big_caves, current_partition_big_cave_types);
    COPY_TO(bridge_styles, current_partition_bridge_styles);
    COPY_TO(population, current_partition_population_meta);
    cave_atmosphere_get_partitions(image->atmospheres);
    styles_copy_level_floor_choices(image->floor_choices, 64);
    styles_copy_level_door_choices(image->door_choices, 64);
    image->primary_style = styles_get_level_primary_style();
    image->environment = cave_environment_get_state();
    for (int i = 0; i < image->environment.source_count; i++)
        image->sources[i] = *cave_environment_source_at(i);
    COPY_TO(events, cave_events_state());
    image->next_event = cave_events_next_serial();
    image->catastrophe = catastrophe_get_state();
    skeleton_note_get_state(&image->skeleton);
    image->hint_count = MIN(32, hint_messages_count_for_save());
    for (int i = 0; i < image->hint_count; i++) {
        image->hint_lengths[i] = MIN(16, hint_messages_message_line_count(i));
        hint_messages_message_meta(i, &image->hint_metadata[i]);
        for (int line = 0; line < image->hint_lengths[i]; line++)
            SDL_strlcpy(image->hints[i][line], hint_messages_message_line(i, line), 100);
    }
    for (int a = 0; a < MON_GROUP_MAX; a++)
        for (int b = 0; b < MON_GROUP_MAX; b++)
            image->relations[a][b] = monster_group_relation(a, b);
    for (int y = 0; y < image->hgt; y++)
        for (int x = 0; x < image->wid; x++) {
            image->cells[y][x] = *cave_environment_cell_at(y, x);
            image->fixtures[y][x] = cave_fixture_at(y, x);
            image->floods[y][x] = cave_flood_stage_at(y, x);
            image->flood_surface[y][x] = cave_flood_surface_kind_at(y, x);
            image->flood_traps[y][x] = cave_flood_trap_kind_at(y, x);
            image->water_flow[y][x] = cave_water_flow_encoded_at(y, x);
            image->catastrophe_steps[y][x] = catastrophe_cell_step(y, x);
        }
}

void level_image_restore(const level_image* image)
{
    level_stream_restore_frame(image);
    s32b elapsed = MAX(0, turn - image->suspended_turn);
    p_ptr->depth = image->depth;
    p_ptr->cur_map_hgt = image->hgt; p_ptr->cur_map_wid = image->wid;
    p_ptr->py = image->py; p_ptr->px = image->px;
    cave_vertical_restore(&image->vertical);
    vertical_vault_restore(&image->vertical_vaults);
    p_ptr->truce = image->truce;
    feeling = image->feeling; do_feeling = image->do_feeling;
    rating = image->rating; good_item_flag = image->good_item;
    num_trap_on_level = image->traps;
    mon_max = image->mon_max; mon_cnt = image->mon_cnt;
    o_max = image->o_max; o_cnt = image->o_cnt; num_repro = image->repro;
    COPY_FROM(g_vault_name, vault_name);
    COPY_FROM(cave_info, info); COPY_FROM(cave_feat, feat);
    COPY_FROM(cave_color, color); COPY_FROM(cave_natural, natural);
    COPY_FROM(cave_rewired, rewired);
    legendary_area_map_ensure();
    COPY_FROM(legendary_area_id, legendary);
    legendary_area_restore_runtime(image->legendary_runtime);
    COPY_FROM(cave_light, light); COPY_FROM(cave_o_idx, objects_at);
    COPY_FROM(cave_m_idx, monsters_at); COPY_FROM(cave_when, scent);
    SDL_memset(cave_cost, FLOW_MAX_DIST, (MAX_FLOWS) * sizeof(*cave_cost));
    COPY_FROM(&cave_cost[FLOW_WANDERING_HEAD], wandering_cost);
    COPY_FROM(flow_center_y, flow_y);
    COPY_FROM(flow_center_x, flow_x); COPY_FROM(update_center_y, update_y);
    COPY_FROM(update_center_x, update_x); COPY_FROM(wandering_pause, wandering);
    /* Individual pursuit/noise flows are cheap derived caches. Force lazy
     * rebuilds, instead of retaining 1,500 whole-map arrays per parked floor. */
    SDL_memset(flow_center_y, 0, FLOW_WANDERING_HEAD * sizeof(*flow_center_y));
    SDL_memset(flow_center_x, 0, FLOW_WANDERING_HEAD * sizeof(*flow_center_x));
    SDL_memset(update_center_y, 0, FLOW_WANDERING_HEAD * sizeof(*update_center_y));
    SDL_memset(update_center_x, 0, FLOW_WANDERING_HEAD * sizeof(*update_center_x));
    for (int i = FLOW_PLAYER_NOISE; i < MAX_FLOWS; i++) {
        flow_center_y[i] = flow_center_x[i] = update_center_y[i] = update_center_x[i] = 0;
    }
    scent_when = image->scent_when;
    COPY_FROM(mon_list, monsters);
    for (int i = 1; i < z_info->r_max; i++)
        if (!(r_info[i].flags1 & RF1_UNIQUE)) r_info[i].cur_num = 0;
    for (int i = 1; i < mon_max; i++)
        if (mon_list[i].r_idx && !(r_info[mon_list[i].r_idx].flags1 & RF1_UNIQUE))
            r_info[mon_list[i].r_idx].cur_num++;
    SDL_memcpy(o_list, image->objects, z_info->o_max * sizeof(*o_list));
    current_partition_rows = image->partition_rows;
    current_partition_cols = image->partition_cols;
    current_partition_count = image->partition_count;
    current_labyrinth_partitions = image->labyrinths;
    COPY_FROM(current_partition_modes, modes);
    COPY_FROM(current_partition_densities, densities);
    COPY_FROM(current_partition_big_cave_types, big_caves);
    COPY_FROM(current_partition_bridge_styles, bridge_styles);
    COPY_FROM(current_partition_population_meta, population);
    cave_atmosphere_set_partitions(image->atmospheres, PARTITION_META_MAX);
    styles_load_level_floor_choices(image->floor_choices, 64);
    styles_load_level_door_choices(image->door_choices, 64);
    styles_set_loaded_level_primary(image->primary_style);
    cave_environment_reset(); cave_fixtures_clear(); cave_flood_clear();
    environment_state env = image->environment;
    env.last_turn += elapsed;
    cave_environment_restore_state(env);
    cave_water_flow_restore_begin();
    for (int y = 0; y < image->hgt; y++)
        for (int x = 0; x < image->wid; x++) {
            environment_cell cell = image->cells[y][x];
            if (cell.due > 0) cell.due += elapsed;
            cave_environment_restore_cell(y, x, cell);
            cave_fixture_set(y, x, image->fixtures[y][x]);
            if (image->floods[y][x]) cave_flood_restore(y, x, image->floods[y][x]);
            if (image->flood_surface[y][x])
                cave_flood_restore_surface(y, x, image->flood_surface[y][x]);
            if (image->flood_traps[y][x])
                cave_flood_restore_trap_kind(y, x, image->flood_traps[y][x]);
            cave_water_flow_restore_cell(y, x, image->water_flow[y][x]);
        }
    for (int i = 0; i < env.source_count; i++) {
        environment_source source = image->sources[i];
        source.next_turn += elapsed;
        cave_environment_restore_source(i, source);
    }
    cave_water_flow_restore_finish();
    cave_world_event events[CAVE_EVENTS_MAX];
    SDL_memcpy(events, image->events, sizeof(events));
    for (int i = 0; i < CAVE_EVENTS_MAX; i++) events[i].turn += elapsed;
    cave_events_restore(events, image->next_event);
    monster_social_restore_relations(image->relations);
    catastrophe_state run = catastrophe_get_state();
    catastrophe_state local = image->catastrophe;
    local.chance = run.chance;
    local.smith_milestones = run.smith_milestones;
    local.find_milestones = run.find_milestones;
    local.last_stage = run.last_stage;
    local.craft_difficulty = run.craft_difficulty;
    local.timer_step = run.timer_step;
    SDL_memcpy(local.pending, run.pending, sizeof(local.pending));
    catastrophe_restore_state(local);
    for (int y = 0; y < image->hgt; y++)
        for (int x = 0; x < image->wid; x++)
            catastrophe_restore_cell(y, x, image->catastrophe_steps[y][x]);
    skeleton_note_set_state(&image->skeleton);
    hint_messages_clear_for_load(image->depth, image->wid, image->hgt);
    for (int i = 0; i < image->hint_count; i++)
        hint_messages_add_for_load(image->hints[i], image->hint_lengths[i], &image->hint_metadata[i]);
    view_n = temp_n = 0;
    character_dungeon = true;
}
