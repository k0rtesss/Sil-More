#include "angband.h"
#include "level-generation/level-state.h"
#include "level-generation/level-stream.h"
#include "meta_state.h"

/* All positions here are local, and zero is the absent-position sentinel.
 * Do arithmetic before narrowing to the engine's byte-sized coordinates. */
static bool window_inside(int y, int x, int height, int width)
{
    return y > 0 && x > 0 && y < height - 1 && x < width - 1;
}

static bool window_byte_position(byte* y, byte* x, int dy, int dx,
    int height, int width)
{
    int ny = (int)*y - dy, nx = (int)*x - dx;
    if (!*y || !*x || !window_inside(ny, nx, height, width)) {
        *y = *x = 0;
        return false;
    }
    *y = ny; *x = nx;
    return true;
}

static bool window_short_position(s16b* y, s16b* x, int dy, int dx,
    int height, int width)
{
    int ny = (int)*y - dy, nx = (int)*x - dx;
    if (!window_inside(*y, *x, MAX_DUNGEON_HGT, MAX_DUNGEON_WID)
        || !window_inside(ny, nx, height, width)) return false;
    *y = ny; *x = nx;
    return true;
}

static int window_partition(int y, int x, int height, int width)
{
    if (!window_inside(y, x, height, width)) return -1;
    int row = (y - 1) / LEVEL_STREAM_PART_SIZE;
    int col = (x - 1) / LEVEL_STREAM_PART_SIZE;
    return row < LEVEL_STREAM_GRID && col < LEVEL_STREAM_GRID
        ? row * LEVEL_STREAM_GRID + col : -1;
}

void level_window_translate_monster(monster_type* m, int dy, int dx,
    int height, int width)
{
    if (!m || !m->r_idx) return;
    if (!window_byte_position(&m->fy, &m->fx, dy, dx, height, width)) {
        SDL_memset(m, 0, sizeof(*m));
        return;
    }
    window_byte_position(&m->target_y, &m->target_x, dy, dx, height, width);
    if (!window_byte_position(&m->ai.goal_y, &m->ai.goal_x,
            dy, dx, height, width)) m->ai.goal_age = 0;
    if (!window_byte_position(&m->ai.previous_y, &m->ai.previous_x,
            dy, dx, height, width)) m->ai.waits = 0;
    if (!window_byte_position(&m->ai.player_y, &m->ai.player_x,
            dy, dx, height, width)) m->ai.player_action_turn = 0;
    if (!window_byte_position(&m->ai.attack_y, &m->ai.attack_x,
            dy, dx, height, width)) {
        m->ai.attack_chain = 0; m->ai.attack_turn = 0;
    }
    if (!window_byte_position(&m->ai.feedback_y, &m->ai.feedback_x,
            dy, dx, height, width)) {
        m->ai.feedback_reason = m->ai.feedback_cooldown = 0;
    }
    monster_sense_state* sense = &m->ai.sense;
    bool target = window_byte_position(&sense->y, &sense->x, dy, dx, height, width);
    bool anchor = window_byte_position(&sense->anchor_y, &sense->anchor_x,
        dy, dx, height, width);
    if (!target || !anchor) SDL_memset(sense, 0, sizeof(*sense));
    else {
        byte ys[4] = {0}, xs[4] = {0};
        int count = 0, old_count = MIN(sense->recent_count, 4);
        int first = old_count == 4 ? sense->recent_next % 4 : 0;
        for (int n = 0; n < old_count; n++) {
            int slot = (first + n) % 4;
            byte y = sense->recent_y[slot], x = sense->recent_x[slot];
            if (window_byte_position(&y, &x, dy, dx, height, width)) {
                ys[count] = y; xs[count++] = x;
            }
        }
        SDL_memcpy(sense->recent_y, ys, sizeof(ys));
        SDL_memcpy(sense->recent_x, xs, sizeof(xs));
        sense->recent_count = count; sense->recent_next = count % 4;
    }
    /* Squad positions are rebuilt each processing pass. Keeping a reservation
     * for an evicted commander would be more dangerous than recomputing it. */
    SDL_memset(&m->squad, 0, sizeof(m->squad));
    monster_world_state* world = &m->world;
    if (!window_byte_position(&world->observation_y, &world->observation_x,
            dy, dx, height, width)) {
        world->observation_kind = world->observation_age = 0;
    }
    if (!window_byte_position(&world->target_y, &world->target_x,
            dy, dx, height, width)) {
        world->task = world->task_age = world->retries = 0;
    }
    if (!window_byte_position(&world->home_y, &world->home_x,
            dy, dx, height, width)) {
        world->home_y = m->fy; world->home_x = m->fx;
        world->task = world->task_age = world->retries = 0;
    }
    monster_routine_state* routine = &m->routine;
    bool home = window_byte_position(&routine->home_y, &routine->home_x,
        dy, dx, height, width);
    if (!home) {
        routine->home_y = m->fy; routine->home_x = m->fx;
        routine->territory = 0;
    } else if (routine->territory) {
        routine->territory = window_partition(routine->home_y, routine->home_x,
            height, width) + 1;
    }
    bool circuit = home && routine->count <= MON_PATROL_MAX;
    for (int i = 0; i < MIN(routine->count, MON_PATROL_MAX); i++)
        if (!window_byte_position(&routine->y[i], &routine->x[i],
                dy, dx, height, width)) circuit = false;
    /* A clipped circuit is not a circuit; do not join its separated ends. */
    if (!circuit) {
        routine->style = routine->count = routine->next = 0;
        SDL_memset(routine->y, 0, sizeof(routine->y));
        SDL_memset(routine->x, 0, sizeof(routine->x));
    }
    m->ml = false;
    m->ai.cast_checked = m->ai.cast_available = false;
}

/* An old finite map uses the pre-streaming proportional partition layout.
 * Streaming maps use exact 44-cell partitions with one outer guard cell. */
static int window_old_partition(const level_image* old, int y, int x)
{
    if (!window_inside(y, x, old->hgt, old->wid)) return -1;
    if (old->stream.enabled && old->hgt == LEVEL_STREAM_SIZE && old->wid == LEVEL_STREAM_SIZE)
        return window_partition(y, x, old->hgt, old->wid);
    if (old->partition_rows < 1 || old->partition_cols < 1) return -1;
    int row = y * old->partition_rows / old->hgt;
    int col = x * old->partition_cols / old->wid;
    int pi = row * old->partition_cols + col;
    return pi < old->partition_count && pi < PARTITION_META_MAX ? pi : -1;
}

static void window_partitions(level_image* next, const level_image* old, int dy, int dx)
{
    next->partition_rows = next->partition_cols = LEVEL_STREAM_GRID;
    next->partition_count = LEVEL_STREAM_GRID * LEVEL_STREAM_GRID;
    next->labyrinths = 0;
    SDL_memset(next->modes, 0, sizeof(next->modes));
    SDL_memset(next->big_caves, 0, sizeof(next->big_caves));
    SDL_memset(next->bridge_styles, 0, sizeof(next->bridge_styles));
    SDL_memset(next->population, 0, sizeof(next->population));
    SDL_memset(next->atmospheres, 0, sizeof(next->atmospheres));
    for (int pi = 0; pi < PARTITION_META_MAX; pi++) {
        next->densities[pi] = DENSITY_NORMAL;
        int y = 1 + (pi / LEVEL_STREAM_GRID) * LEVEL_STREAM_PART_SIZE
            + LEVEL_STREAM_PART_SIZE / 2 + dy;
        int x = 1 + (pi % LEVEL_STREAM_GRID) * LEVEL_STREAM_PART_SIZE
            + LEVEL_STREAM_PART_SIZE / 2 + dx;
        int from = window_old_partition(old, y, x);
        if (from < 0) continue;
        next->modes[pi] = old->modes[from];
        next->densities[pi] = old->densities[from];
        next->big_caves[pi] = old->big_caves[from];
        next->bridge_styles[pi] = old->bridge_styles[from];
        next->population[pi] = old->population[from];
        next->atmospheres[pi] = old->atmospheres[from];
        if (next->modes[pi] == QUAD_MODE_LABYRINTH) next->labyrinths++;
    }
}

/* Build valid lists from retained ownership, preserving the old stack order.
 * visited also bounds malformed cycles without touching any global pool. */
static void window_chain(level_image* next, const level_image* old, int head,
    s16b* destination, int holder, int y, int x, byte* linked)
{
    s16b* tail = destination;
    for (int n = 0; head > 0 && head < old->o_max && n < old->o_max; n++) {
        int index = head;
        head = old->objects[index].next_o_idx;
        object_type* object = &next->objects[index];
        if (!object->k_idx || linked[index]) continue;
        if (object->held_m_idx != holder
            || (!holder && (object->iy != y || object->ix != x))) continue;
        linked[index] = 1;
        *tail = index; tail = &object->next_o_idx;
    }
    *tail = 0;
}

static void window_actors(level_image* next, const level_image* old,
    int dy, int dx, byte* linked)
{
    next->mon_cnt = next->repro = 0;
    for (int i = 1; i < old->mon_max; i++) {
        monster_type* m = &next->monsters[i];
        if (m->r_idx && !window_inside(m->fy, m->fx, old->hgt, old->wid))
            SDL_memset(m, 0, sizeof(*m));
        level_window_translate_monster(m, dy, dx, next->hgt, next->wid);
        if (!m->r_idx) continue;
        /* Invalid/colliding old actor positions cannot create a dangling grid
         * owner. Normal images never enter this recovery path. */
        if (next->monsters_at[m->fy][m->fx]) {
            SDL_memset(m, 0, sizeof(*m)); continue;
        }
        next->monsters_at[m->fy][m->fx] = i;
        next->mon_cnt++;
        if (r_info[m->r_idx].flags2 & RF2_MULTIPLY) next->repro++;
        m->hold_o_idx = 0;
        if (m->wandering_idx < 0 || m->wandering_idx >= MAX_FLOWS
            || !next->flow_y[m->wandering_idx] || !next->flow_x[m->wandering_idx])
            m->wandering_idx = 0;
    }
    for (int i = 1; i < old->mon_max; i++) {
        monster_type* m = &next->monsters[i];
        if (!m->r_idx) continue;
        s16b* refs[] = { &m->social_rival, &m->social_focus, &m->social_ally };
        for (unsigned j = 0; j < N_ELEMENTS(refs); j++) {
            int index = *refs[j];
            if (index > 0 && (index >= old->mon_max || !next->monsters[index].r_idx))
                *refs[j] = 0;
        }
    }
    next->o_cnt = 0;
    for (int i = 1; i < old->o_max; i++) {
        object_type* object = &next->objects[i];
        if (!object->k_idx) continue;
        bool keep;
        if (object->held_m_idx) {
            int owner = object->held_m_idx;
            keep = owner > 0 && owner < old->mon_max && next->monsters[owner].r_idx;
            object->iy = object->ix = 0;
        } else keep = window_inside(object->iy, object->ix, old->hgt, old->wid)
            && window_byte_position(&object->iy, &object->ix,
                dy, dx, next->hgt, next->wid);
        if (!keep) SDL_memset(object, 0, sizeof(*object));
        else { object->next_o_idx = 0; next->o_cnt++; }
    }
    for (int y = 1; y < next->hgt - 1; y++)
        for (int x = 1; x < next->wid - 1; x++) {
            int oy = y + dy, ox = x + dx;
            if (window_inside(oy, ox, old->hgt, old->wid))
                window_chain(next, old, old->objects_at[oy][ox],
                    &next->objects_at[y][x], 0, y, x, linked);
        }
    for (int i = 1; i < old->mon_max; i++)
        if (next->monsters[i].r_idx)
            window_chain(next, old, old->monsters[i].hold_o_idx,
                &next->monsters[i].hold_o_idx, i, 0, 0, linked);
    /* Recover any live but unlinked object according to its explicit owner. */
    for (int i = 1; i < old->o_max; i++) {
        object_type* object = &next->objects[i];
        if (!object->k_idx || linked[i]) continue;
        s16b* head = object->held_m_idx
            ? &next->monsters[object->held_m_idx].hold_o_idx
            : &next->objects_at[object->iy][object->ix];
        object->next_o_idx = *head; *head = i;
    }
    if (window_inside(next->py, next->px, next->hgt, next->wid)
        && !next->monsters_at[next->py][next->px])
        next->monsters_at[next->py][next->px] = -1;
}

static void window_local_records(level_image* next, int dy, int dx)
{
    player_type* generated[] = { &next->generation_before, &next->generation_after };
    for (unsigned i = 0; i < N_ELEMENTS(generated); i++) {
        player_type* player = generated[i];
        if (player->aule_level == next->depth)
            window_byte_position(&player->aule_forge_y, &player->aule_forge_x,
                dy, dx, next->hgt, next->wid);
        if (player->mandos_level == next->depth)
            window_byte_position(&player->mandos_vault_y, &player->mandos_vault_x,
                dy, dx, next->hgt, next->wid);
    }
    int count = 0;
    byte source_map[ENV_SOURCES_MAX + 1] = {0};
    for (int i = 0; i < MIN(next->environment.source_count, ENV_SOURCES_MAX); i++) {
        environment_source source = next->sources[i];
        bool inside = window_byte_position(&source.y, &source.x, dy, dx, next->hgt, next->wid);
        int supply = 0;
        for (int y = 1; y < next->hgt - 1; y++) for (int x = 1; x < next->wid - 1; x++) {
            const environment_cell* cell = &next->cells[y][x];
            if (cell->owner != i + 1) continue;
            /* Reservoir origins are arbitrary flood-fill starting points.
             * A retained part of the same reservoir must keep its owner. */
            if (!inside) { source.y = y; source.x = x; inside = true; }
            if ((cell->flags & ENV_ADDED_LIQUID)
                || (source.feature == FEAT_LAVA && (cell->flags & ENV_DEPOSIT)
                    && cell->base_feat != FEAT_LAVA)) supply++;
        }
        if (inside) {
            source.used = MIN(source.used, supply);
            source_map[i + 1] = count + 1;
            next->sources[count++] = source;
        }
    }
    next->environment.source_count = count;
    SDL_memset(next->sources + count, 0, (ENV_SOURCES_MAX - count) * sizeof(*next->sources));
    for (int y = 1; y < next->hgt - 1; y++) for (int x = 1; x < next->wid - 1; x++) {
        environment_cell* cell = &next->cells[y][x];
        cell->owner = cell->owner <= ENV_SOURCES_MAX ? source_map[cell->owner] : 0;
    }
    for (int i = 0; i < CAVE_EVENTS_MAX; i++) {
        cave_world_event* event = &next->events[i];
        if (!window_byte_position(&event->y, &event->x, dy, dx, next->hgt, next->wid))
            SDL_memset(event, 0, sizeof(*event));
    }
    if (!window_byte_position(&next->catastrophe.y, &next->catastrophe.x,
            dy, dx, next->hgt, next->wid)) next->catastrophe.ready = false;
    count = 0;
    for (int i = 0; i < MIN(next->hint_count, 32); i++) {
        hint_message_meta meta = next->hint_metadata[i];
        bool keep = window_short_position(&meta.source_y, &meta.source_x,
            dy, dx, next->hgt, next->wid);
        if (meta.destination_count > HINT_MESSAGE_DESTINATION_MAX) keep = false;
        for (int d = 0; keep && d < meta.destination_count; d++) {
            hint_message_destination* destination = &meta.destinations[d];
            if (destination->kind && !window_short_position(&destination->y,
                    &destination->x, dy, dx, next->hgt, next->wid)) keep = false;
            else if (destination->kind == HINT_DESTINATION_PARTITION)
                destination->id = window_partition(destination->y, destination->x,
                    next->hgt, next->wid);
        }
        if (!keep) continue;
        next->hint_metadata[count] = meta;
        next->hint_lengths[count] = next->hint_lengths[i];
        SDL_memmove(next->hints[count], next->hints[i], sizeof(next->hints[count]));
        count++;
    }
    if (count < next->hint_count) {
        /* Clue selection is local to the remembered window. Forgotten notes
         * cannot exhaust whole-level clue quotas during an endless journey.
         * Searched skeletons keep their object state; only future choices are
         * made available again. */
        next->skeleton.hint_used_mask = 0;
        SDL_memset(next->skeleton.hint_use_counts, 0, sizeof(next->skeleton.hint_use_counts));
        next->skeleton.seen_count = 0;
        for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; i++) next->skeleton.seen_ids[i] = -1;
        next->skeleton.notes_shown = count;
    }
    next->hint_count = count;
    SDL_memset(next->hint_metadata + count, 0, (32 - count) * sizeof(*next->hint_metadata));
    SDL_memset(next->hint_lengths + count, 0, (32 - count) * sizeof(*next->hint_lengths));
    SDL_memset(next->hints + count, 0, (32 - count) * sizeof(*next->hints));
    next->skeleton.map_hgt = next->hgt; next->skeleton.map_wid = next->wid;
    count = 0;
    for (int i = 0; i < MIN(next->stream_site_count, LEVEL_STREAM_SITE_DEFS); i++) {
        level_stream_site_def site = next->stream_sites[i];
        int y1 = (int)site.y1 - dy, x1 = (int)site.x1 - dx;
        int y2 = (int)site.y2 - dy, x2 = (int)site.x2 - dx;
        if (y2 < 1 || x2 < 1 || y1 >= next->hgt - 1 || x1 >= next->wid - 1) continue;
        site.y1 = MAX(1, y1); site.x1 = MAX(1, x1);
        site.y2 = MIN(next->hgt - 2, y2); site.x2 = MIN(next->wid - 2, x2);
        next->stream_sites[count++] = site;
    }
    next->stream_site_count = count;
    SDL_memset(next->stream_sites + count, 0,
        (LEVEL_STREAM_SITE_DEFS - count) * sizeof(*next->stream_sites));
    count = 0;
    for (int i = 0; i < MIN(next->vertical.portal_count, CAVE_VERTICAL_PORTALS); i++) {
        cave_vertical_portal portal = next->vertical.portals[i];
        if (window_short_position(&portal.y, &portal.x, dy, dx, next->hgt, next->wid)
            && window_short_position(&portal.peer_y, &portal.peer_x,
                dy, dx, next->hgt, next->wid)) next->vertical.portals[count++] = portal;
    }
    next->vertical.portal_count = count;
    SDL_memset(next->vertical.portals + count, 0,
        (CAVE_VERTICAL_PORTALS - count) * sizeof(*next->vertical.portals));
    count = 0;
    for (int i = 0; i < MIN(next->vertical.basin_count, CAVE_VERTICAL_BASINS); i++) {
        cave_vertical_basin basin = next->vertical.basins[i];
        if (window_short_position(&basin.y, &basin.x, dy, dx, next->hgt, next->wid))
            next->vertical.basins[count++] = basin;
    }
    next->vertical.basin_count = count;
    SDL_memset(next->vertical.basins + count, 0,
        (CAVE_VERTICAL_BASINS - count) * sizeof(*next->vertical.basins));
    count = 0;
    for (int i = 0; i < MIN(next->vertical_vaults.count, VERTICAL_VAULT_MAX); i++) {
        vertical_vault_placement placement = next->vertical_vaults.placements[i];
        /* Top-left origins may leave the window while the authored module
         * still overlaps it. Keep signed coordinates for partial modules. */
        int y = (int)placement.y - dy, x = (int)placement.x - dx;
        int h = MAX_DUNGEON_HGT, w = MAX_DUNGEON_WID;
        if (v_info && placement.upper_template > 0 && placement.upper_template < z_info->v_max) {
            const vault_type* v = &v_info[placement.upper_template];
            h = (placement.transform & 4) ? v->wid : v->hgt;
            w = (placement.transform & 4) ? v->hgt : v->wid;
        }
        if (y < next->hgt - 1 && x < next->wid - 1 && y + h > 1 && x + w > 1) {
            placement.y = y; placement.x = x;
            next->vertical_vaults.placements[count++] = placement;
        }
    }
    next->vertical_vaults.count = count;
    SDL_memset(next->vertical_vaults.placements + count, 0,
        (VERTICAL_VAULT_MAX - count) * sizeof(*next->vertical_vaults.placements));
}

bool level_window_translate(level_image* image, int dy, int dx, int height, int width)
{
    if (!image || !image->objects || !z_info || height < 3 || width < 3
        || height > MAX_DUNGEON_HGT || width > MAX_DUNGEON_WID
        || image->hgt < 3 || image->wid < 3 || image->hgt > MAX_DUNGEON_HGT
        || image->wid > MAX_DUNGEON_WID || image->mon_max < 1
        || image->mon_max > MAX_MONSTERS || image->o_max < 1
        || image->o_max > z_info->o_max || dy < -MAX_DUNGEON_HGT
        || dy > MAX_DUNGEON_HGT || dx < -MAX_DUNGEON_WID
        || dx > MAX_DUNGEON_WID) return false;
    if ((dy > 0 && image->stream.y > INT64_MAX - dy)
        || (dy < 0 && image->stream.y < INT64_MIN - dy)
        || (dx > 0 && image->stream.x > INT64_MAX - dx)
        || (dx < 0 && image->stream.x < INT64_MIN - dx)) return false;
    level_image* next = SDL_malloc(sizeof(*next));
    object_type* objects = SDL_malloc(z_info->o_max * sizeof(*objects));
    byte* linked = SDL_calloc(z_info->o_max, sizeof(*linked));
    if (!next || !objects || !linked) {
        SDL_free(next); SDL_free(objects); SDL_free(linked); return false;
    }
    *next = *image;
    next->objects = objects;
    SDL_memcpy(objects, image->objects, z_info->o_max * sizeof(*objects));
    next->hgt = height; next->wid = width;
    next->py = image->py - dy; next->px = image->px - dx;
    /* Cached unvisited levels can have an evicted last player position. It is
     * not an actor: transition placement will choose its actual entry cell. */
    if (!window_inside(next->py, next->px, height, width)) next->py = next->px = 0;
    next->stream.y += dy; next->stream.x += dx;
#define CLEAR(field) SDL_memset(next->field, 0, sizeof(next->field))
    CLEAR(info); CLEAR(feat); CLEAR(color); CLEAR(natural); CLEAR(rewired);
    CLEAR(legendary); CLEAR(light); CLEAR(objects_at); CLEAR(monsters_at); CLEAR(scent);
    CLEAR(cells); CLEAR(fixtures); CLEAR(floods); CLEAR(flood_surface); CLEAR(flood_traps);
    CLEAR(water_flow); CLEAR(catastrophe_steps); CLEAR(chasm_drop); CLEAR(chasm_up);
    SDL_memset(next->wandering_cost, FLOW_MAX_DIST, sizeof(next->wandering_cost));
#undef CLEAR
    for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
        bool guard = y == 0 || x == 0 || y == height - 1 || x == width - 1;
        next->feat[y][x] = guard ? FEAT_WALL_PERM : FEAT_WALL_EXTRA;
        next->info[y][x] = CAVE_WALL;
        int primary = image->primary_style >= 0 && image->primary_style < COLOR_STYLE_SLOT_MAX
            ? image->primary_style : 0;
        next->color[y][x] = (byte)(COLOR_STYLE_BASE + primary);
        int oy = y + dy, ox = x + dx;
        if (guard || !window_inside(oy, ox, image->hgt, image->wid)) continue;
#define TILE(field) next->field[y][x] = image->field[oy][ox]
        TILE(feat); TILE(color); TILE(natural); TILE(rewired); TILE(legendary);
        TILE(scent); TILE(cells); TILE(fixtures); TILE(floods); TILE(flood_surface);
        TILE(flood_traps); TILE(water_flow); TILE(catastrophe_steps);
        TILE(chasm_drop); TILE(chasm_up);
#undef TILE
        next->info[y][x] = image->info[oy][ox] & ~(CAVE_VIEW | CAVE_SEEN
            | CAVE_TEMP | CAVE_FIRE | CAVE_OLD_VIEW | CAVE_OLD_LIT | CAVE_OLD_TORCH);
        for (int flow = 0; flow < MAX_WANDERING_GROUPS; flow++)
            next->wandering_cost[flow][y][x] = image->wandering_cost[flow][oy][ox];
    }
    for (int i = 0; i < MAX_FLOWS; i++) {
        bool center = window_byte_position(&next->flow_y[i], &next->flow_x[i],
            dy, dx, height, width);
        window_byte_position(&next->update_y[i], &next->update_x[i], dy, dx, height, width);
        if (!center && i >= FLOW_WANDERING_HEAD && i <= FLOW_WANDERING_TAIL) {
            SDL_memset(next->wandering_cost[i - FLOW_WANDERING_HEAD], FLOW_MAX_DIST,
                sizeof(next->wandering_cost[0]));
            next->wandering[i] = 0;
        }
    }
    window_partitions(next, image, dy, dx);
    window_local_records(next, dy, dx);
    window_actors(next, image, dy, dx, linked);
    level_image_repair_social(next);
    int traps = 0;
    for (int y = 1; y < height - 1; y++) for (int x = 1; x < width - 1; x++)
        if (FEAT_IS_TRAP(next->feat[y][x])) traps++;
    next->traps = MIN(traps, 255);
    /* This opaque snapshot is shared with image until the final commit. The
     * hook is allocation-free and must leave it untouched on failure. */
    if (!legendary_area_translate_runtime_snapshot(next->legendary_runtime,
            dy, dx, height, width)) {
        SDL_free(next); SDL_free(objects); SDL_free(linked); return false;
    }
    object_type* old_objects = image->objects;
    *image = *next;
    SDL_free(old_objects); SDL_free(next); SDL_free(linked);
    return true;
}
