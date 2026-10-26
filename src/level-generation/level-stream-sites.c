#include "angband.h"
#include "level-generation/level-state.h"
#include "level-generation/level-stream.h"
#include "level-generation/level-cache.h"
#include "supplies.h"
#include "fs/save-internal.h"
#include "fs/load-internal.h"

/* Only authored places accumulate.  Ordinary visits have no archive entry.
 * Tokens survive list compaction, pickup, floor transfers and save/load. */
#define STREAM_SITES_MAX 128
#define STREAM_SITE_MAGIC 0x5753
typedef struct site_cell {
    u16b info, legendary;
    byte feat, color, natural, rewired, fixture, flood, surface, trap, flow;
    byte chasm_down, chasm_up;
    environment_cell environment;
    u32b catastrophe;
} site_cell;
typedef struct site_actor {
    monster_type value;
    s16b depth;
    s64b y, x, frame_y, frame_x;
    u64b rival, focus, ally;
} site_actor;
typedef struct site_object {
    object_type value;
    s16b depth;
    s64b y, x;
    u64b holder;
} site_object;
typedef struct site_source {
    environment_source value;
    s64b y, x;
} site_source;
typedef struct stream_site {
    u32b id, next_token;
    s16b depth, height, width;
    s64b y, x;
    byte kind;
    bool discovered, awarded;
    char name[80];
    s32b suspended;
    site_cell* cells;
    site_actor* actors;
    site_object* objects;
    u16b actor_count, object_count;
    byte source_count;
    site_source sources[ENV_SOURCES_MAX];
    cave_vertical_state vertical;
    vertical_vault_state vaults;
    bool aule, mandos;
    s64b aule_y, aule_x, mandos_y, mandos_x;
} stream_site;
static stream_site sites[STREAM_SITES_MAX];
static u16b site_count;
static stream_site* site_at(const level_image* image, int y, int x);

static bool local(const level_image* image, s64b wy, s64b wx, int* y, int* x)
{
    s64b yy = wy - image->stream.y + 1, xx = wx - image->stream.x + 1;
    if (yy < 1 || xx < 1 || yy >= image->hgt - 1 || xx >= image->wid - 1) return false;
    if (y) *y = (int)yy;
    if (x) *x = (int)xx;
    return true;
}
static bool inside(const stream_site* site, s64b y, s64b x)
{ return y >= site->y && y < site->y + site->height && x >= site->x && x < site->x + site->width; }
static stream_site* owner(u64b token)
{
    u32b id = (u32b)(token >> 32);
    return id && id <= site_count ? &sites[id - 1] : NULL;
}
static u64b new_token(stream_site* site)
{
    if (site->next_token == UINT32_MAX) quit("Permanent site identity space exhausted");
    return ((u64b)site->id << 32) | ++site->next_token;
}
static u64b actor_token(const level_image* image, int index)
{ return index > 0 && index < image->mon_max ? image->monsters[index].site_token : 0; }
static bool carried(u64b token)
{
    if (!token) return false;
    for (int i = 0; i < INVEN_TOTAL; i++)
        if (inventory[i].k_idx && inventory[i].site_token == token) return true;
    for (int i = 0; i < player_carried_extra_entry_count(); i++) {
        const object_type* o = player_carried_extra_entry_at(i);
        if (o && o->k_idx && o->site_token == token) return true;
    }
    for (int i = 0; i < player_quiver_store_entry_count(); i++) {
        const object_type* o = player_quiver_store_entry_at(i);
        if (o && o->k_idx && o->site_token == token) return true;
    }
    for (int i = 0; i < supplies_entry_count(); i++) {
        const object_type* o = supplies_entry_at(i);
        if (o && o->k_idx && o->site_token == token) return true;
    }
    return false;
}

void level_stream_normalize_carried(void)
{
    int maximum = INVEN_TOTAL + player_carried_extra_entry_count()
        + player_quiver_store_entry_count() + supplies_entry_count();
    object_type** entries = SDL_calloc(MAX(1, maximum), sizeof(*entries));
    if (!entries) quit("Out of memory assigning carried site identities");
    int count = 0;
    for (int i = 0; i < INVEN_TOTAL; i++) entries[count++] = &inventory[i];
    for (int i = 0; i < player_carried_extra_entry_count(); i++) entries[count++] = player_carried_extra_entry_at(i);
    for (int i = 0; i < player_quiver_store_entry_count(); i++) entries[count++] = player_quiver_store_entry_at(i);
    for (int i = 0; i < supplies_entry_count(); i++) entries[count++] = supplies_entry_at(i);
    for (int i = 0; i < count; i++) {
        object_type* object = entries[i];
        if (!object || !object->k_idx || !owner(object->site_token)) continue;
        for (int j = 0; j < i; j++) if (entries[j] && entries[j]->k_idx
            && entries[j]->site_token == object->site_token) {
            object->site_token = new_token(owner(object->site_token)); break;
        }
    }
    SDL_free(entries);
}

void level_stream_clear(void)
{
    for (int i = 0; i < site_count; i++) {
        SDL_free(sites[i].cells); SDL_free(sites[i].actors); SDL_free(sites[i].objects);
    }
    SDL_memset(sites, 0, sizeof(sites)); site_count = 0;
    level_stream_set_frame((level_stream_frame){0});
}
bool level_stream_has_sites(int depth)
{
    for (int i = 0; i < site_count; i++) if (sites[i].depth == depth) return true;
    return false;
}
void level_stream_site_mask(int depth, level_stream_frame frame, void* storage)
{
    byte (*mask)[MAX_DUNGEON_WID] = storage;
    SDL_memset(mask, 0, MAX_DUNGEON_HGT * sizeof(*mask));
    for (int i = 0; i < site_count; i++) {
        const stream_site* s = &sites[i];
        if (s->depth != depth) continue;
        s64b y1 = MAX(1, s->y - frame.y + 1), x1 = MAX(1, s->x - frame.x + 1);
        s64b y2 = MIN(MAX_DUNGEON_HGT - 2, s->y + s->height - frame.y);
        s64b x2 = MIN(MAX_DUNGEON_WID - 2, s->x + s->width - frame.x);
        for (s64b y = y1; y <= y2; y++) for (s64b x = x1; x <= x2; x++) mask[y][x] = 1;
    }
}
bool level_stream_site_monster(int depth, u64b token)
{
    (void)depth;
    stream_site* site = owner(token);
    return site != NULL;
}
bool level_stream_site_artefact(int artefact)
{
    if (artefact <= 0) return false;
    for (int i = 0; i < site_count; i++)
        for (int j = 0; j < sites[i].object_count; j++)
            if (sites[i].objects[j].value.name1 == artefact) return true;
    return false;
}

static void register_definitions(level_image* image)
{
    for (int i = 0; i < image->stream_site_count; i++) {
        const level_stream_site_def* d = &image->stream_sites[i];
        s64b wy = image->stream.y + d->y1 - 1, wx = image->stream.x + d->x1 - 1;
        int h = d->y2 - d->y1 + 1, w = d->x2 - d->x1 + 1;
        bool present = false;
        for (int s = 0; s < site_count; s++) {
            stream_site* site = &sites[s];
            if (site->depth == image->depth && wy < site->y + site->height
                && wy + h > site->y && wx < site->x + site->width && wx + w > site->x) {
                present = true; break;
            }
        }
        if (present) continue;
        if (site_count == STREAM_SITES_MAX) quit("Too many permanent dungeon sites");
        stream_site* site = &sites[site_count++];
        site->id = site_count; site->depth = image->depth;
        site->y = wy; site->x = wx; site->height = h; site->width = w;
        site->kind = d->kind; SDL_strlcpy(site->name, d->name, sizeof(site->name));
        site->cells = SDL_calloc((size_t)h * w, sizeof(*site->cells));
        if (!site->cells) quit("Out of memory preserving an authored dungeon site");
    }
}

static void infer_definition(level_image* image, int y, int x, int kind, const char* name)
{
    if (image->stream_site_count >= LEVEL_STREAM_SITE_DEFS || y < 1 || x < 1
        || y >= image->hgt - 1 || x >= image->wid - 1) return;
    int y1 = y, y2 = y, x1 = x, x2 = x;
    /* Authored rectangles flag their walls as well as their floors. Follow
     * that physical footprint rather than guessing the old partition size. */
    byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = {{0}};
    int* queue = SDL_malloc(MAX_DUNGEON_HGT * MAX_DUNGEON_WID * sizeof(*queue));
    if (!queue) quit("Out of memory importing a persistent site");
    int head = 0, tail = 1; queue[0] = y * MAX_DUNGEON_WID + x; seen[y][x] = 1;
    while (head < tail) {
        int p = queue[head++], yy = p / MAX_DUNGEON_WID, xx = p % MAX_DUNGEON_WID;
        y1 = MIN(y1, yy); y2 = MAX(y2, yy); x1 = MIN(x1, xx); x2 = MAX(x2, xx);
        const int dy[4] = {-1, 0, 1, 0}, dx[4] = {0, 1, 0, -1};
        for (int d = 0; d < 4; d++) {
            int ny = yy + dy[d], nx = xx + dx[d];
            if (ny < 1 || nx < 1 || ny >= image->hgt - 1 || nx >= image->wid - 1 || seen[ny][nx]
                || !(image->info[ny][nx] & (kind == 1 ? CAVE_G_VAULT : CAVE_ICKY))) continue;
            seen[ny][nx] = 1; queue[tail++] = ny * MAX_DUNGEON_WID + nx;
        }
    }
    SDL_free(queue);
    if (tail == 1 && kind == 2) { y1 -= 8; x1 -= 8; y2 += 8; x2 += 8; }
    y1 = MAX(1, y1 - 2); x1 = MAX(1, x1 - 2);
    y2 = MIN(image->hgt - 2, y2 + 2); x2 = MIN(image->wid - 2, x2 + 2);
    for (int i = 0; i < image->stream_site_count; i++) {
        const level_stream_site_def* old = &image->stream_sites[i];
        if (y >= old->y1 && y <= old->y2 && x >= old->x1 && x <= old->x2) return;
    }
    level_stream_site_def* d = &image->stream_sites[image->stream_site_count++];
    d->y1 = y1; d->x1 = x1; d->y2 = y2; d->x2 = x2; d->kind = kind;
    SDL_strlcpy(d->name, name && name[0] ? name : kind == 1 ? "Great vault" : "Quest site", sizeof(d->name));
}

static void infer_loaded_sites(level_image* image)
{
    /* Also catches a giver created during play, after initial generation. */
    if (!image->stream_site_count && !level_stream_has_sites(image->depth)
        && (image->depth == 0 || image->depth == UTUMNO_DEPTH || image->depth == UTUMNO_FORGE_DEPTH)) {
        level_stream_site_def* d = &image->stream_sites[image->stream_site_count++];
        d->y1 = d->x1 = 1; d->y2 = image->hgt - 2; d->x2 = image->wid - 2; d->kind = 3;
        SDL_strlcpy(d->name, image->depth ? "Utumno" : "Gates of Angband", sizeof(d->name));
    }
    for (int y = 1; y < image->hgt - 1; y++) for (int x = 1; x < image->wid - 1; x++) {
        if (!(image->info[y][x] & CAVE_G_VAULT) || site_at(image, y, x)) continue;
        infer_definition(image, y, x, 1, image->vault_name);
        register_definitions(image);
    }
    for (int m = 1; m < image->mon_max; m++) {
        const monster_type* a = &image->monsters[m];
        if (a->site_token || (a->r_idx != R_IDX_TULKAS && a->r_idx != R_IDX_AULE && a->r_idx != R_IDX_MANDOS && a->r_idx != R_IDX_NIENA
            && a->r_idx != R_IDX_OROME && a->r_idx != R_IDX_VARDA)) continue;
        if (!site_at(image, a->fy, a->fx)) infer_definition(image, a->fy, a->fx, 2, "Quest site");
    }
    const player_type* player = image->pending_player ? &image->generation_after : p_ptr;
    if (player->aule_level == image->depth && player->aule_quest != AULE_QUEST_NOT_STARTED)
        infer_definition(image, player->aule_forge_y, player->aule_forge_x, 2, "Aule's forge");
    if (player->mandos_level == image->depth && player->mandos_quest != MANDOS_QUEST_NOT_STARTED)
        infer_definition(image, player->mandos_vault_y, player->mandos_vault_x, 2, "Mandos' tomb");
}
static stream_site* site_at(const level_image* image, int y, int x)
{
    s64b wy = image->stream.y + y - 1, wx = image->stream.x + x - 1;
    for (int i = 0; i < site_count; i++)
        if (sites[i].depth == image->depth && inside(&sites[i], wy, wx)) return &sites[i];
    return NULL;
}

void level_stream_capture_sites(level_image* image)
{
    if (!image || !image->stream.enabled || level_cache_in_worker()) return;
    infer_loaded_sites(image);
    register_definitions(image);
    /* First establish ownership for every actor. Held loot then belongs to
     * the actor even when it has walked out of the authored footprint. */
    for (int m = 1; m < image->mon_max; m++) {
        monster_type* actor = &image->monsters[m];
        if (!actor->r_idx || actor->site_token) continue;
        stream_site* site = site_at(image, actor->fy, actor->fx);
        if (site) actor->site_token = new_token(site);
    }
    for (int o = 1; o < image->o_max; o++) {
        object_type* obj = &image->objects[o];
        if (!obj->k_idx) continue;
        stream_site* site = owner(obj->site_token);
        if (!site && obj->held_m_idx > 0) site = owner(actor_token(image, obj->held_m_idx));
        if (!site && !obj->held_m_idx) site = site_at(image, obj->iy, obj->ix);
        bool duplicate = carried(obj->site_token);
        for (int n = 1; n < o && !duplicate && obj->site_token; n++)
            duplicate = image->objects[n].k_idx && image->objects[n].site_token == obj->site_token;
        if (site && (!obj->site_token || duplicate)) obj->site_token = new_token(site);
    }
    for (int s = 0; s < site_count; s++) {
        stream_site* site = &sites[s];
        bool geometry = site->depth == image->depth;
        bool touches = geometry && site->y < image->stream.y + image->hgt - 2
            && site->y + site->height > image->stream.y
            && site->x < image->stream.x + image->wid - 2 && site->x + site->width > image->stream.x;
        s32b elapsed = MAX(0, turn - site->suspended);
        s32b image_elapsed = MAX(0, turn - image->suspended_turn);
        byte source_map[ENV_SOURCES_MAX + 1] = {0};
        for (int n = 0; touches && n < site->source_count; n++)
            if (site->sources[n].value.next_turn) site->sources[n].value.next_turn += elapsed;
        for (int n = 0; touches && n < image->environment.source_count; n++) {
            const environment_source* source = &image->sources[n];
            s64b wy = image->stream.y + source->y - 1, wx = image->stream.x + source->x - 1;
            int slot = -1; bool belongs = inside(site, wy, wx);
            for (int j = 0; j < site->source_count; j++)
                if (site->sources[j].y == wy && site->sources[j].x == wx
                    && site->sources[j].value.feature == source->feature) slot = j;
            for (int y = 0; y < site->height && slot < 0; y++) for (int x = 0; x < site->width; x++) {
                int ly, lx;
                if (!local(image, site->y + y, site->x + x, &ly, &lx)
                    || image->cells[ly][lx].owner != n + 1) continue;
                belongs = true;
                int previous = site->cells[y * site->width + x].environment.owner;
                if (previous > 0 && previous <= site->source_count
                    && site->sources[previous - 1].value.feature == source->feature) { slot = previous - 1; break; }
            }
            if (!belongs) continue;
            if (slot < 0) {
                if (site->source_count == ENV_SOURCES_MAX) quit("Too many reservoirs in a permanent site");
                slot = site->source_count++;
            }
            site->sources[slot].value = *source;
            site->sources[slot].value.next_turn += image_elapsed;
            site->sources[slot].y = wy; site->sources[slot].x = wx;
            source_map[n + 1] = slot + 1;
        }
        for (int y = 0; geometry && y < site->height; y++) for (int x = 0; x < site->width; x++) {
            int ly, lx;
            site_cell* c = &site->cells[y * site->width + x];
            if (!local(image, site->y + y, site->x + x, &ly, &lx)) {
                if (touches && c->environment.due) c->environment.due += elapsed;
                continue;
            }
            c->feat = image->feat[ly][lx]; c->info = image->info[ly][lx];
            c->color = image->color[ly][lx]; c->natural = image->natural[ly][lx];
            c->rewired = image->rewired[ly][lx]; c->legendary = image->legendary[ly][lx];
            c->fixture = image->fixtures[ly][lx]; c->flood = image->floods[ly][lx];
            c->surface = image->flood_surface[ly][lx]; c->trap = image->flood_traps[ly][lx];
            c->flow = image->water_flow[ly][lx]; c->environment = image->cells[ly][lx];
            c->environment.owner = source_map[c->environment.owner];
            if (c->environment.due) c->environment.due += image_elapsed;
            c->catastrophe = image->catastrophe_steps[ly][lx];
            c->chasm_down = image->chasm_drop[ly][lx]; c->chasm_up = image->chasm_up[ly][lx];
            if ((c->info & CAVE_MARK) && (c->info & (CAVE_ICKY | CAVE_G_VAULT))) site->discovered = true;
        }
        /* Remove records whose last known position was in this resident
         * window, including killed actors and looted/destroyed floor stacks. */
        int count = 0;
        for (int n = 0; n < site->actor_count; n++)
            if (site->actors[n].depth != image->depth
                || !local(image, site->actors[n].y, site->actors[n].x, NULL, NULL))
                site->actors[count++] = site->actors[n];
        site->actor_count = count;
        for (int m = 1; m < image->mon_max; m++) {
            const monster_type* actor = &image->monsters[m];
            if (!actor->r_idx || owner(actor->site_token) != site) continue;
            for (int n = 0; n < site->actor_count; n++)
                if (site->actors[n].value.site_token == actor->site_token)
                    site->actors[n] = site->actors[--site->actor_count];
            site_actor* grown = SDL_realloc(site->actors, (site->actor_count + 1) * sizeof(*grown));
            if (!grown) quit("Out of memory preserving site occupants");
            site->actors = grown;
            site_actor* a = &site->actors[site->actor_count++];
            SDL_memset(a, 0, sizeof(*a)); a->value = *actor;
            a->depth = image->depth;
            a->y = image->stream.y + actor->fy - 1; a->x = image->stream.x + actor->fx - 1;
            a->frame_y = image->stream.y; a->frame_x = image->stream.x;
            a->rival = actor_token(image, actor->social_rival);
            a->focus = actor_token(image, actor->social_focus);
            a->ally = actor_token(image, actor->social_ally);
            a->value.hold_o_idx = 0;
            if (actor->encountered && (actor->r_idx == R_IDX_TULKAS || actor->r_idx == R_IDX_AULE
                || actor->r_idx == R_IDX_MANDOS || actor->r_idx == R_IDX_NIENA
                || actor->r_idx == R_IDX_OROME || actor->r_idx == R_IDX_VARDA)) site->discovered = true;
        }
        count = 0;
        for (int n = 0; n < site->object_count; n++) {
            site_object* obj = &site->objects[n];
            if (!carried(obj->value.site_token)
                && (obj->depth != image->depth || !local(image, obj->y, obj->x, NULL, NULL))) site->objects[count++] = *obj;
        }
        site->object_count = count;
        for (int o = 1; o < image->o_max; o++) {
            const object_type* obj = &image->objects[o];
            if (!obj->k_idx || owner(obj->site_token) != site) continue;
            for (int n = 0; n < site->object_count; n++)
                if (site->objects[n].value.site_token == obj->site_token)
                    site->objects[n] = site->objects[--site->object_count];
            site_object* grown = SDL_realloc(site->objects, (site->object_count + 1) * sizeof(*grown));
            if (!grown) quit("Out of memory preserving site objects");
            site->objects = grown;
            site_object* saved = &site->objects[site->object_count++];
            SDL_memset(saved, 0, sizeof(*saved)); saved->value = *obj;
            saved->depth = image->depth;
            saved->holder = actor_token(image, obj->held_m_idx);
            int y = obj->iy, x = obj->ix;
            if (obj->held_m_idx > 0 && obj->held_m_idx < image->mon_max) {
                y = image->monsters[obj->held_m_idx].fy; x = image->monsters[obj->held_m_idx].fx;
            }
            saved->y = image->stream.y + y - 1; saved->x = image->stream.x + x - 1;
            saved->value.next_o_idx = saved->value.held_m_idx = 0;
        }
        if (touches) {
        int retained = 0;
        for (int n = 0; n < site->vertical.portal_count; n++) {
            cave_vertical_portal p = site->vertical.portals[n];
            if (!local(image, site->y + p.y - 1, site->x + p.x - 1, NULL, NULL)) {
                if (p.due) p.due += elapsed;
                site->vertical.portals[retained++] = p;
            }
        }
        site->vertical.portal_count = retained;
        for (int n = 0; n < image->vertical.portal_count && site->vertical.portal_count < CAVE_VERTICAL_PORTALS; n++) {
            cave_vertical_portal p = image->vertical.portals[n];
            if (!inside(site, image->stream.y + p.y - 1, image->stream.x + p.x - 1)) continue;
            p.y += image->stream.y - site->y; p.x += image->stream.x - site->x;
            p.peer_y += image->stream.y - site->y; p.peer_x += image->stream.x - site->x;
            if (p.due) p.due += image_elapsed;
            site->vertical.portals[site->vertical.portal_count++] = p;
        }
        retained = 0;
        for (int n = 0; n < site->vertical.basin_count; n++) {
            cave_vertical_basin b = site->vertical.basins[n];
            if (!local(image, site->y + b.y - 1, site->x + b.x - 1, NULL, NULL)) site->vertical.basins[retained++] = b;
        }
        site->vertical.basin_count = retained;
        for (int n = 0; n < image->vertical.basin_count && site->vertical.basin_count < CAVE_VERTICAL_BASINS; n++) {
            cave_vertical_basin b = image->vertical.basins[n];
            if (!inside(site, image->stream.y + b.y - 1, image->stream.x + b.x - 1)) continue;
            b.y += image->stream.y - site->y; b.x += image->stream.x - site->x;
            site->vertical.basins[site->vertical.basin_count++] = b;
        }
        for (int n = 0; n < image->vertical_vaults.count; n++) {
            vertical_vault_placement v = image->vertical_vaults.placements[n];
            int previous = -1;
            for (int j = 0; j < site->vaults.count; j++) if (site->vaults.placements[j].identity == v.identity) previous = j;
            if (previous < 0 && !inside(site, image->stream.y + MAX(1, v.y) - 1,
                image->stream.x + MAX(1, v.x) - 1)) continue;
            v.y += image->stream.y - site->y; v.x += image->stream.x - site->x;
            if (previous >= 0) site->vaults.placements[previous] = v;
            else if (site->vaults.count < VERTICAL_VAULT_MAX) site->vaults.placements[site->vaults.count++] = v;
        }
        const player_type* player = image->pending_player ? &image->generation_after : p_ptr;
        if (player->aule_level == image->depth && player->aule_forge_y && player->aule_forge_x
            && inside(site, image->stream.y + player->aule_forge_y - 1,
                image->stream.x + player->aule_forge_x - 1)) {
            site->aule = true; site->aule_y = image->stream.y + player->aule_forge_y - 1;
            site->aule_x = image->stream.x + player->aule_forge_x - 1;
        }
        if (player->mandos_level == image->depth && player->mandos_vault_y && player->mandos_vault_x
            && inside(site, image->stream.y + player->mandos_vault_y - 1,
                image->stream.x + player->mandos_vault_x - 1)) {
            site->mandos = true; site->mandos_y = image->stream.y + player->mandos_vault_y - 1;
            site->mandos_x = image->stream.x + player->mandos_vault_x - 1;
        }
        site->suspended = turn;
        }
    }
}

static int monster_slot(level_image* image, u64b token)
{
    if (!token) return 0;
    for (int i = 1; i < image->mon_max; i++)
        if (image->monsters[i].r_idx && image->monsters[i].site_token == token) return i;
    return 0;
}
static int free_monster(level_image* image)
{
    for (int i = 1; i < MAX_MONSTERS; i++) if (!image->monsters[i].r_idx) {
        image->mon_max = MAX(image->mon_max, i + 1); return i;
    }
    return 0;
}
static int free_object(level_image* image)
{
    for (int i = 1; i < z_info->o_max; i++) if (!image->objects[i].k_idx) {
        image->o_max = MAX(image->o_max, i + 1); return i;
    }
    return 0;
}

void level_stream_restore_sites(level_image* image)
{
    if (!image || !image->stream.enabled) return;
    bool inserted[MAX_MONSTERS] = {0};
    for (int s = 0; s < site_count; s++) {
        const stream_site* site = &sites[s];
        bool geometry = site->depth == image->depth;
        s32b elapsed = MAX(0, turn - site->suspended);
        byte source_map[ENV_SOURCES_MAX + 1] = {0};
        for (int n = 0; geometry && n < site->source_count; n++) {
            const site_source* saved = &site->sources[n];
            int y = 0, x = 0; bool visible = false;
            for (int sy = 0; sy < site->height && !visible; sy++) for (int sx = 0; sx < site->width; sx++)
                if (site->cells[sy * site->width + sx].environment.owner == n + 1
                    && local(image, site->y + sy, site->x + sx, &y, &x)) { visible = true; break; }
            if (!visible) continue;
            int slot = -1;
            for (int j = 0; j < image->environment.source_count; j++)
                if (image->sources[j].feature == saved->value.feature
                    && image->stream.y + image->sources[j].y - 1 == saved->y
                    && image->stream.x + image->sources[j].x - 1 == saved->x) slot = j;
            int existing = image->cells[y][x].owner;
            if (slot < 0 && existing > 0 && existing <= image->environment.source_count
                && image->sources[existing - 1].feature == saved->value.feature) slot = existing - 1;
            if (slot < 0) {
                if (image->environment.source_count == ENV_SOURCES_MAX)
                    quit("No reservoir slot available for a permanent site");
                slot = image->environment.source_count++;
            }
            environment_source value = saved->value;
            int ay, ax;
            if (local(image, saved->y, saved->x, &ay, &ax)) { value.y = ay; value.x = ax; }
            else { value.y = y; value.x = x; }
            if (value.next_turn) value.next_turn += elapsed;
            image->sources[slot] = value;
            source_map[n + 1] = slot + 1;
        }
        for (int y = 0; geometry && y < site->height; y++) for (int x = 0; x < site->width; x++) {
            int ly, lx;
            if (!local(image, site->y + y, site->x + x, &ly, &lx)) continue;
            const site_cell* c = &site->cells[y * site->width + x];
            if (!c->feat) continue;
            image->feat[ly][lx] = c->feat; image->info[ly][lx] = c->info & ~(CAVE_SEEN | CAVE_VIEW);
            image->color[ly][lx] = c->color; image->natural[ly][lx] = c->natural;
            image->rewired[ly][lx] = c->rewired; image->legendary[ly][lx] = c->legendary;
            image->fixtures[ly][lx] = c->fixture; image->floods[ly][lx] = c->flood;
            image->flood_surface[ly][lx] = c->surface; image->flood_traps[ly][lx] = c->trap;
            image->water_flow[ly][lx] = c->flow; image->cells[ly][lx] = c->environment;
            image->cells[ly][lx].owner = source_map[c->environment.owner];
            if (image->cells[ly][lx].due) image->cells[ly][lx].due += elapsed;
            image->catastrophe_steps[ly][lx] = c->catastrophe;
            image->chasm_drop[ly][lx] = c->chasm_down; image->chasm_up[ly][lx] = c->chasm_up;
            if (site->kind == 1 && site->name[0]) SDL_strlcpy(image->vault_name, site->name, sizeof(image->vault_name));
        }
        for (int n = 0; n < site->actor_count; n++) {
            const site_actor* actor = &site->actors[n];
            int y, x;
            if (actor->depth != image->depth || !local(image, actor->y, actor->x, &y, &x)
                || monster_slot(image, actor->value.site_token)) continue;
            if (image->monsters_at[y][x]) continue;
            int slot = free_monster(image);
            if (!slot) quit("No room to restore permanent site occupants");
            monster_type value = actor->value;
            level_window_translate_monster(&value, (int)(image->stream.y - actor->frame_y),
                (int)(image->stream.x - actor->frame_x), image->hgt, image->wid);
            value.fy = y; value.fx = x; value.hold_o_idx = 0; value.ml = false;
            value.social_rival = value.social_focus = value.social_ally = 0;
            image->monsters[slot] = value; image->monsters_at[y][x] = slot; image->mon_cnt++;
            inserted[slot] = true;
            if (r_info[value.r_idx].flags2 & RF2_MULTIPLY) image->repro++;
        }
        for (int n = 0; n < site->object_count; n++) {
            const site_object* object = &site->objects[n];
            if (object->depth != image->depth) continue;
            int y, x, holder = 0; bool exists = carried(object->value.site_token);
            for (int j = 1; j < image->o_max && !exists; j++)
                exists = image->objects[j].k_idx && image->objects[j].site_token == object->value.site_token;
            if (exists) continue;
            if (object->holder) {
                holder = monster_slot(image, object->holder);
                if (!holder) continue;
                y = image->monsters[holder].fy; x = image->monsters[holder].fx;
            } else if (!local(image, object->y, object->x, &y, &x)) continue;
            int slot = free_object(image);
            if (!slot) quit("No room to restore permanent site objects");
            object_type value = object->value;
            value.held_m_idx = holder; value.iy = holder ? 0 : y; value.ix = holder ? 0 : x;
            if (holder) {
                value.next_o_idx = image->monsters[holder].hold_o_idx;
                image->monsters[holder].hold_o_idx = slot;
            } else {
                value.next_o_idx = image->objects_at[y][x]; image->objects_at[y][x] = slot;
            }
            image->objects[slot] = value; image->o_cnt++;
        }
        for (int n = 0; geometry && n < site->vertical.portal_count; n++) {
            cave_vertical_portal p = site->vertical.portals[n]; int y, x;
            if (!local(image, site->y + p.y - 1, site->x + p.x - 1, &y, &x)) continue;
            bool exists = false;
            for (int j = 0; j < image->vertical.portal_count; j++)
                if (image->vertical.portals[j].identity == p.identity && image->vertical.portals[j].y == y
                    && image->vertical.portals[j].x == x) exists = true;
            if (exists || image->vertical.portal_count == CAVE_VERTICAL_PORTALS) continue;
            s64b py = site->y + p.peer_y - image->stream.y;
            s64b px = site->x + p.peer_x - image->stream.x;
            if (py < 1 || px < 1 || py >= image->hgt - 1 || px >= image->wid - 1) continue;
            p.y = y; p.x = x; p.peer_y = py; p.peer_x = px;
            if (p.due) p.due += elapsed;
            image->vertical.portals[image->vertical.portal_count++] = p;
        }
        for (int n = 0; geometry && n < site->vertical.basin_count; n++) {
            cave_vertical_basin b = site->vertical.basins[n]; int y, x;
            if (!local(image, site->y + b.y - 1, site->x + b.x - 1, &y, &x)) continue;
            bool exists = false;
            for (int j = 0; j < image->vertical.basin_count; j++)
                if (image->vertical.basins[j].identity == b.identity) exists = true;
            if (exists || image->vertical.basin_count == CAVE_VERTICAL_BASINS) continue;
            b.y = y; b.x = x; image->vertical.basins[image->vertical.basin_count++] = b;
        }
        for (int n = 0; geometry && n < site->vaults.count; n++) {
            vertical_vault_placement v = site->vaults.placements[n]; bool exists = false;
            for (int j = 0; j < image->vertical_vaults.count; j++)
                if (image->vertical_vaults.placements[j].identity == v.identity) exists = true;
            if (exists || image->vertical_vaults.count == VERTICAL_VAULT_MAX) continue;
            s64b y = site->y + v.y - image->stream.y, x = site->x + v.x - image->stream.x;
            if (y <= -MAX_DUNGEON_HGT || x <= -MAX_DUNGEON_WID || y >= image->hgt || x >= image->wid) continue;
            v.y = y; v.x = x; image->vertical_vaults.placements[image->vertical_vaults.count++] = v;
        }
    }
    /* Resolve archived links only for actors actually restored in this call.
     * Existing nearby actors retain their live feuds, including ordinary
     * counterparts which deliberately have no persistent site token. */
    for (int s = 0; s < site_count; s++) for (int n = 0; n < sites[s].actor_count; n++) {
        const site_actor* a = &sites[s].actors[n];
        int slot = monster_slot(image, a->value.site_token);
        if (!slot || !inserted[slot]) continue;
        image->monsters[slot].social_rival = monster_slot(image, a->rival);
        image->monsters[slot].social_focus = monster_slot(image, a->focus);
        image->monsters[slot].social_ally = monster_slot(image, a->ally);
    }
    level_image_repair_social(image);
}

void level_stream_project_quests(void)
{
    level_stream_frame f = level_stream_get_frame();
    for (int s = 0; s < site_count; s++) {
        const stream_site* site = &sites[s];
        if (site->aule) {
            s64b y = site->aule_y - f.y + 1, x = site->aule_x - f.x + 1;
            p_ptr->aule_forge_y = y > 0 && y < p_ptr->cur_map_hgt - 1 ? y : 0;
            p_ptr->aule_forge_x = x > 0 && x < p_ptr->cur_map_wid - 1 ? x : 0;
        }
        if (site->mandos) {
            s64b y = site->mandos_y - f.y + 1, x = site->mandos_x - f.x + 1;
            p_ptr->mandos_vault_y = y > 0 && y < p_ptr->cur_map_hgt - 1 ? y : 0;
            p_ptr->mandos_vault_x = x > 0 && x < p_ptr->cur_map_wid - 1 ? x : 0;
        }
    }
}

bool level_stream_site_discovery_awarded(int y, int x)
{
    level_stream_frame f = level_stream_get_frame();
    for (int i = 0; i < site_count; i++)
        if (sites[i].depth == p_ptr->depth && inside(&sites[i], f.y + y - 1, f.x + x - 1)) return sites[i].awarded;
    return false;
}
void level_stream_note_site_discovery(int y, int x)
{
    level_stream_frame f = level_stream_get_frame();
    for (int i = 0; i < site_count; i++) if (sites[i].depth == p_ptr->depth
        && inside(&sites[i], f.y + y - 1, f.x + x - 1)) sites[i].discovered = sites[i].awarded = true;
}
int level_stream_bookmark_count(void)
{
    int count = 0;
    for (int i = 0; i < site_count; i++) if (sites[i].discovered) count++;
    return count;
}
bool level_stream_bookmark(int index, char* text, size_t length)
{
    level_stream_frame f = level_stream_get_frame();
    for (int i = 0; i < site_count; i++) {
        const stream_site* site = &sites[i];
        if (!site->discovered || index-- != 0) continue;
        s64b dy = site->y + site->height / 2 - (f.y + p_ptr->py - 1);
        s64b dx = site->x + site->width / 2 - (f.x + p_ptr->px - 1);
        SDL_snprintf(text, length, "%s (%d ft): %lld %s, %lld %s", site->name, site->depth * 50,
            (long long)(dy < 0 ? -dy : dy), dy < 0 ? "north" : "south",
            (long long)(dx < 0 ? -dx : dx), dx < 0 ? "west" : "east");
        return true;
    }
    return false;
}

static void wr64(u64b value)
{ save_wr_u32b((u32b)value); save_wr_u32b((u32b)(value >> 32)); }
static u64b rd64(void)
{ u32b lo = 0, hi = 0; load_rd_u32b(&lo); load_rd_u32b(&hi); return ((u64b)hi << 32) | lo; }
#define CELL_BYTES(X) X(feat) X(color) X(natural) X(rewired) X(fixture) \
    X(flood) X(surface) X(trap) X(flow) X(chasm_down) X(chasm_up)
#define ENV_BYTES(X) X(flags) X(base_feat) X(known_feat) X(bridge_feat) X(underlay) \
    X(material) X(integrity) X(work) X(pending_feat) X(owner) X(known_underlay) X(known_material)
#define WORLD_BYTES(X) X(initialized) X(observation_kind) X(observation_y) X(observation_x) \
    X(observation_age) X(task) X(target_y) X(target_x) X(task_age) X(retries) X(cooldown) \
    X(home_y) X(home_x) X(supplies)

static void write_cell(const site_cell* c)
{
    save_wr_u16b(c->info); save_wr_u16b(c->legendary);
#define B(f) save_wr_byte(c->f);
    CELL_BYTES(B)
#undef B
#define B(f) save_wr_byte(c->environment.f);
    ENV_BYTES(B)
#undef B
    save_wr_s16b(c->environment.heat); save_wr_s32b(c->environment.due);
    save_wr_u32b(c->catastrophe);
}
static bool read_cell(site_cell* c)
{
    load_rd_u16b(&c->info); load_rd_u16b(&c->legendary);
#define B(f) load_rd_byte(&c->f);
    CELL_BYTES(B)
#undef B
#define B(f) load_rd_byte(&c->environment.f);
    ENV_BYTES(B)
#undef B
    load_rd_s16b(&c->environment.heat); load_rd_s32b(&c->environment.due);
    load_rd_u32b(&c->catastrophe);
    return c->feat < z_info->f_max && (c->flow & ~15) == 0 && c->environment.owner <= ENV_SOURCES_MAX;
}

static void write_regions(const stream_site* site)
{
    save_wr_byte(site->vertical.portal_count); save_wr_byte(site->vertical.basin_count);
    for (int i = 0; i < site->vertical.portal_count; i++) {
        const cave_vertical_portal* p = &site->vertical.portals[i];
        save_wr_u32b(p->identity);
        save_wr_s16b(p->y); save_wr_s16b(p->x); save_wr_s16b(p->peer_depth);
        save_wr_s16b(p->peer_y); save_wr_s16b(p->peer_x);
#define B(f) save_wr_byte(p->f);
        B(kind) B(material) B(strength) B(direction) B(flags) B(pending_material) B(local_style) B(peer_style)
#undef B
        save_wr_u16b(p->pending); save_wr_s32b(p->due);
        for (int y = 0; y < CAVE_VERTICAL_PATCH; y++) for (int x = 0; x < CAVE_VERTICAL_PATCH; x++) {
            save_wr_byte(p->local_patch[y][x]); save_wr_byte(p->peer_patch[y][x]);
        }
    }
    for (int i = 0; i < site->vertical.basin_count; i++) {
        const cave_vertical_basin* b = &site->vertical.basins[i];
        save_wr_u32b(b->identity); save_wr_s16b(b->depth);
        save_wr_s16b(b->y); save_wr_s16b(b->x);
        save_wr_u16b(b->volume); save_wr_u16b(b->capacity); save_wr_u16b(b->normal_volume);
        save_wr_byte(b->material); save_wr_byte(b->radius); save_wr_byte(b->hot); save_wr_byte(b->flags);
    }
    save_wr_byte(site->vaults.count);
    for (int i = 0; i < site->vaults.count; i++) {
        const vertical_vault_placement* v = &site->vaults.placements[i];
        save_wr_u32b(v->identity); save_wr_u16b(v->upper_template); save_wr_u16b(v->lower_template);
        save_wr_s16b(v->upper_depth); save_wr_s16b(v->y); save_wr_s16b(v->x);
        save_wr_byte(v->transform); save_wr_byte(v->discovered_layers); save_wr_byte(v->reward_layers);
    }
}
static bool read_regions(stream_site* site)
{
    load_rd_byte(&site->vertical.portal_count); load_rd_byte(&site->vertical.basin_count);
    if (site->vertical.portal_count > CAVE_VERTICAL_PORTALS || site->vertical.basin_count > CAVE_VERTICAL_BASINS) return false;
    for (int i = 0; i < site->vertical.portal_count; i++) {
        cave_vertical_portal* p = &site->vertical.portals[i];
        load_rd_u32b(&p->identity);
        load_rd_s16b(&p->y); load_rd_s16b(&p->x); load_rd_s16b(&p->peer_depth);
        load_rd_s16b(&p->peer_y); load_rd_s16b(&p->peer_x);
#define B(f) load_rd_byte(&p->f);
        B(kind) B(material) B(strength) B(direction) B(flags) B(pending_material) B(local_style) B(peer_style)
#undef B
        load_rd_u16b(&p->pending); load_rd_s32b(&p->due);
        for (int y = 0; y < CAVE_VERTICAL_PATCH; y++) for (int x = 0; x < CAVE_VERTICAL_PATCH; x++) {
            load_rd_byte(&p->local_patch[y][x]); load_rd_byte(&p->peer_patch[y][x]);
        }
        if (!p->identity || p->kind < 1 || p->kind > CAVE_VERTICAL_TUBE
            || p->peer_depth < 0 || p->peer_depth > UTUMNO_FORGE_DEPTH
            || p->y < 1 || p->x < 1 || p->y > site->height || p->x > site->width) return false;
    }
    for (int i = 0; i < site->vertical.basin_count; i++) {
        cave_vertical_basin* b = &site->vertical.basins[i];
        load_rd_u32b(&b->identity); load_rd_s16b(&b->depth);
        load_rd_s16b(&b->y); load_rd_s16b(&b->x);
        load_rd_u16b(&b->volume); load_rd_u16b(&b->capacity); load_rd_u16b(&b->normal_volume);
        load_rd_byte(&b->material); load_rd_byte(&b->radius); load_rd_byte(&b->hot); load_rd_byte(&b->flags);
        if (!b->identity || b->y < 1 || b->x < 1 || b->y > site->height || b->x > site->width
            || !b->capacity || b->volume > b->capacity) return false;
    }
    load_rd_byte(&site->vaults.count);
    if (site->vaults.count > VERTICAL_VAULT_MAX) return false;
    for (int i = 0; i < site->vaults.count; i++) {
        vertical_vault_placement* v = &site->vaults.placements[i];
        load_rd_u32b(&v->identity); load_rd_u16b(&v->upper_template); load_rd_u16b(&v->lower_template);
        load_rd_s16b(&v->upper_depth); load_rd_s16b(&v->y); load_rd_s16b(&v->x);
        load_rd_byte(&v->transform); load_rd_byte(&v->discovered_layers); load_rd_byte(&v->reward_layers);
        if (!v->identity || !v->upper_template || v->upper_template >= z_info->v_max
            || !v->lower_template || v->lower_template >= z_info->v_max || v->transform > 7
            || (v->discovered_layers & ~3) || (v->reward_layers & ~3)) return false;
    }
    return true;
}

void level_stream_write(void)
{
    level_cache_stream_capture_sites();
    save_wr_u16b(STREAM_SITE_MAGIC); save_wr_u16b(site_count);
    for (int i = 0; i < site_count; i++) {
        const stream_site* s = &sites[i];
        save_wr_u32b(s->id); save_wr_u32b(s->next_token); save_wr_s16b(s->depth);
        save_wr_s16b(s->height); save_wr_s16b(s->width); wr64(s->y); wr64(s->x);
        save_wr_byte(s->kind); save_wr_byte(s->discovered); save_wr_byte(s->awarded);
        save_wr_string(s->name); save_wr_s32b(s->suspended);
        save_wr_byte(s->aule); save_wr_byte(s->mandos);
        wr64(s->aule_y); wr64(s->aule_x); wr64(s->mandos_y); wr64(s->mandos_x);
        save_wr_byte(s->source_count);
        for (int n = 0; n < s->source_count; n++) {
            const site_source* src = &s->sources[n]; const environment_source* e = &src->value;
            wr64(src->y); wr64(src->x);
            save_wr_byte(e->feature); save_wr_byte(e->capacity); save_wr_byte(e->used); save_wr_byte(e->phase);
            save_wr_s32b(e->next_turn);
        }
        for (int n = 0; n < s->height * s->width; n++) write_cell(&s->cells[n]);
        save_wr_u16b(s->actor_count);
        for (int n = 0; n < s->actor_count; n++) {
            const site_actor* a = &s->actors[n]; const monster_type* m = &a->value;
            save_wr_s16b(a->depth); wr64(a->y); wr64(a->x); wr64(a->frame_y); wr64(a->frame_x);
            wr64(a->rival); wr64(a->focus); wr64(a->ally); save_wr_monster(m);
            save_wr_byte(m->social_group); save_wr_byte(m->social_memory); save_wr_byte(m->social_cooldown);
            save_wr_byte(m->social_state); save_wr_byte(m->social_timer); save_wr_byte(m->social_player_threat);
            save_wr_u32b(m->world.last_event);
#define B(f) save_wr_byte(m->world.f);
            WORLD_BYTES(B)
#undef B
            const monster_routine_state* r = &m->routine;
            save_wr_byte(r->home_y); save_wr_byte(r->home_x); save_wr_byte(r->territory);
            save_wr_byte(r->style); save_wr_byte(r->count); save_wr_byte(r->next);
            for (int j = 0; j < r->count; j++) { save_wr_byte(r->y[j]); save_wr_byte(r->x[j]); }
        }
        save_wr_u16b(s->object_count);
        for (int n = 0; n < s->object_count; n++) {
            const site_object* o = &s->objects[n];
            save_wr_s16b(o->depth); wr64(o->y); wr64(o->x); wr64(o->holder); save_wr_item(&o->value);
        }
        write_regions(s);
    }
    save_wr_u32b(0xC35AE791);
}

errr level_stream_read(void)
{
    u16b magic = 0, count = 0;
    load_rd_u16b(&magic); load_rd_u16b(&count);
    if (magic != STREAM_SITE_MAGIC || count > STREAM_SITES_MAX || site_count) return -1;
    size_t total_cells = 0;
    for (int i = 0; i < count; i++) {
        stream_site* s = &sites[site_count++];
        byte discovered = 0, awarded = 0, aule = 0, mandos = 0;
        load_rd_u32b(&s->id); load_rd_u32b(&s->next_token); load_rd_s16b(&s->depth);
        load_rd_s16b(&s->height); load_rd_s16b(&s->width); s->y = (s64b)rd64(); s->x = (s64b)rd64();
        load_rd_byte(&s->kind); load_rd_byte(&discovered); load_rd_byte(&awarded);
        load_rd_string(s->name, sizeof(s->name)); load_rd_s32b(&s->suspended);
        load_rd_byte(&aule); load_rd_byte(&mandos);
        s->aule_y = (s64b)rd64(); s->aule_x = (s64b)rd64();
        s->mandos_y = (s64b)rd64(); s->mandos_x = (s64b)rd64();
        load_rd_byte(&s->source_count);
        if (s->source_count > ENV_SOURCES_MAX) goto invalid;
        for (int n = 0; n < s->source_count; n++) {
            site_source* src = &s->sources[n]; environment_source* e = &src->value;
            src->y = (s64b)rd64(); src->x = (s64b)rd64();
            load_rd_byte(&e->feature); load_rd_byte(&e->capacity); load_rd_byte(&e->used); load_rd_byte(&e->phase);
            load_rd_s32b(&e->next_turn);
            if (!e->feature || e->feature >= z_info->f_max || src->y < INT64_MIN / 2 || src->y > INT64_MAX / 2
                || src->x < INT64_MIN / 2 || src->x > INT64_MAX / 2) goto invalid;
        }
        if (s->id != site_count || s->depth < 0 || s->depth > UTUMNO_FORGE_DEPTH
            || s->height < 1 || s->width < 1 || s->height > MAX_DUNGEON_HGT || s->width > MAX_DUNGEON_WID
            || s->kind < 1 || s->kind > 3 || discovered > 1 || awarded > 1 || aule > 1 || mandos > 1
            || s->y < INT64_MIN / 2 || s->y > INT64_MAX / 2 || s->x < INT64_MIN / 2 || s->x > INT64_MAX / 2)
            goto invalid;
        s->discovered = discovered; s->awarded = awarded; s->aule = aule; s->mandos = mandos;
        total_cells += (size_t)s->height * s->width;
        if (total_cells > 1048576) goto invalid;
        s->cells = SDL_calloc((size_t)s->height * s->width, sizeof(*s->cells));
        if (!s->cells) goto invalid;
        for (int n = 0; n < s->height * s->width; n++)
            if (!read_cell(&s->cells[n]) || s->cells[n].environment.owner > s->source_count) goto invalid;
        load_rd_u16b(&s->actor_count);
        if (s->actor_count >= MAX_MONSTERS) goto invalid;
        s->actors = SDL_calloc(MAX(1, s->actor_count), sizeof(*s->actors));
        if (!s->actors) goto invalid;
        for (int n = 0; n < s->actor_count; n++) {
            site_actor* a = &s->actors[n]; monster_type* m = &a->value;
            load_rd_s16b(&a->depth);
            a->y = (s64b)rd64(); a->x = (s64b)rd64(); a->frame_y = (s64b)rd64(); a->frame_x = (s64b)rd64();
            a->rival = rd64(); a->focus = rd64(); a->ally = rd64(); load_rd_monster(m);
            load_rd_byte(&m->social_group); load_rd_byte(&m->social_memory); load_rd_byte(&m->social_cooldown);
            load_rd_byte(&m->social_state); load_rd_byte(&m->social_timer); load_rd_byte(&m->social_player_threat);
            load_rd_u32b(&m->world.last_event);
#define B(f) load_rd_byte(&m->world.f);
            WORLD_BYTES(B)
#undef B
            monster_routine_state* r = &m->routine;
            load_rd_byte(&r->home_y); load_rd_byte(&r->home_x); load_rd_byte(&r->territory);
            load_rd_byte(&r->style); load_rd_byte(&r->count); load_rd_byte(&r->next);
            if (r->count > MON_PATROL_MAX || (r->count && r->next >= r->count)) goto invalid;
            for (int j = 0; j < r->count; j++) { load_rd_byte(&r->y[j]); load_rd_byte(&r->x[j]); }
            if (a->depth < 0 || a->depth > UTUMNO_FORGE_DEPTH || m->r_idx < 1 || m->r_idx >= z_info->r_max || (m->site_token >> 32) != s->id
                || !(u32b)m->site_token || (u32b)m->site_token > s->next_token
                || a->frame_y < INT64_MIN / 2 || a->frame_y > INT64_MAX / 2
                || a->frame_x < INT64_MIN / 2 || a->frame_x > INT64_MAX / 2
                || a->y - a->frame_y < 0 || a->y - a->frame_y >= MAX_DUNGEON_HGT
                || a->x - a->frame_x < 0 || a->x - a->frame_x >= MAX_DUNGEON_WID) goto invalid;
            for (int j = 0; j < n; j++) if (s->actors[j].value.site_token == m->site_token) goto invalid;
        }
        load_rd_u16b(&s->object_count);
        if (s->object_count >= z_info->o_max) goto invalid;
        s->objects = SDL_calloc(MAX(1, s->object_count), sizeof(*s->objects));
        if (!s->objects) goto invalid;
        for (int n = 0; n < s->object_count; n++) {
            site_object* o = &s->objects[n];
            load_rd_s16b(&o->depth);
            o->y = (s64b)rd64(); o->x = (s64b)rd64(); o->holder = rd64();
            if (o->depth < 0 || o->depth > UTUMNO_FORGE_DEPTH || load_rd_item(&o->value) || !o->value.k_idx || (o->value.site_token >> 32) != s->id
                || !(u32b)o->value.site_token || (u32b)o->value.site_token > s->next_token
                || o->y < INT64_MIN / 2 || o->y > INT64_MAX / 2 || o->x < INT64_MIN / 2 || o->x > INT64_MAX / 2) goto invalid;
            if (o->holder) {
                bool found = false;
                for (int j = 0; j < s->actor_count; j++) if (s->actors[j].value.site_token == o->holder) found = true;
                if (!found) goto invalid;
            }
            for (int j = 0; j < n; j++) if (s->objects[j].value.site_token == o->value.site_token) goto invalid;
        }
        if (!read_regions(s)) goto invalid;
    }
    u32b end = 0; load_rd_u32b(&end);
    if (end != 0xC35AE791) goto invalid;
    level_stream_project_quests();
    for (int i = 0; i < site_count; i++) for (int n = 0; n < sites[i].actor_count; n++) {
        const monster_type* m = &sites[i].actors[n].value;
        if ((r_info[m->r_idx].flags1 & RF1_UNIQUE) && !level_cache_stream_actor_present(m->site_token))
            r_info[m->r_idx].cur_num = MAX(1, r_info[m->r_idx].cur_num);
    }
    return 0;
invalid:
    level_stream_clear();
    return -1;
}
