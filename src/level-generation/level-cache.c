#include "angband.h"
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
#include "cave/cave-water-flow.h"
#include "quest/quest-runtime.h"
#include "quest/quest-challenges.h"
#include "meta_state.h"
#include "metarun.h"
#include "cave/cave-vertical.h"
#include "cave/cave-vertical-combat.h"
#include "level-generation/level-generation-vertical-vaults.h"

/* The fourth slot is only a transient destination for a shaft, fall or wizard
 * jump. After entry the resident set is strictly depth-1, depth, depth+1. */
#define LEVEL_CACHE_SLOTS 4

typedef struct level_link_plan {
    int minimum_hgt, minimum_wid, source_depth;
    int stair_peer_count, stair_peer_depths[LEVEL_CACHE_SLOTS];
    byte feat[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte drop[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte ceiling[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
} level_link_plan;

typedef struct level_job {
    SDL_Thread* thread;
    SDL_AtomicInt done, cancel;
    int depth;
    Uint32 wake_event_type;
    bool success;
    level_image* image;
    level_link_plan links;
    cave_vertical_state inherited_vertical[2];
    vertical_vault_state inherited_vaults[2];
    int inherited_depth[2];
    byte inherited_count;
    bool allow_down;
    player_type before, player;
    player_other options;
    metarun tale;
    object_type pack[INVEN_TOTAL], smith;
    monster_race* races;
    object_kind* kinds;
    artefact_type* artefacts;
    monster_lore* lore;
    alloc_entry *race_alloc, *kind_alloc, *ego_alloc;
    bool* reserved_artefacts;
    void* drop_catalog;
    void* meta_state;
    s32b turn, playerturn, minimum_depth_counter;
    u64b seed;
} level_job;

static level_image* floors[LEVEL_CACHE_SLOTS];
static level_image* active;
static level_job* job;
static bool running;
static bool reading_floor;
static Uint32 ready_event_type;
static int requested_depth = -1, pending_depth = -1;
static int pending_y, pending_x;
static bool pending_rubble, use_pending_origin, pending_disconnected_stair;
static _Thread_local level_job* worker_job;
static void sync_vertical_regions(void);

bool level_cache_in_worker(void) { return worker_job != NULL; }
bool level_cache_reading_floor(void) { return reading_floor; }
bool level_cache_generation_cancelled(void)
{
    return worker_job && SDL_GetAtomicInt(&worker_job->cancel);
}

void level_cache_generation_preserve_quests(void)
{
    if (!worker_job) return;
    const player_type* before = &worker_job->before;
    /* Retry cleanup is allowed to undo a giver created by this job, but not
     * an encounter which still exists on another retained floor. */
    if (before->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT) {
        p_ptr->tulkas_quest = before->tulkas_quest;
        p_ptr->tulkas_target_r_idx = before->tulkas_target_r_idx;
        p_ptr->tulkas_prize_a_idx = before->tulkas_prize_a_idx;
    }
    if (before->niena_quest == NIENA_QUEST_GIVER_PRESENT) {
        p_ptr->niena_quest = before->niena_quest;
        p_ptr->niena_monsters_seen = before->niena_monsters_seen;
        p_ptr->niena_monsters_killed = before->niena_monsters_killed;
    }
    if (before->orome_quest == OROME_QUEST_GIVER_PRESENT) {
        p_ptr->orome_quest = before->orome_quest;
        p_ptr->orome_target_type = before->orome_target_type;
        p_ptr->orome_target_count = before->orome_target_count;
    }
}

static level_image* find_floor(int depth)
{
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i]->depth == depth) return floors[i];
    return NULL;
}

bool level_cache_retains(int depth)
{
    return running && active && ABS(depth - active->depth) <= 1;
}

bool level_cache_ready(int depth)
{
    return !running || find_floor(depth) != NULL;
}

static bool valid_depth(int depth)
{
    return (depth >= 0 && depth <= MORGOTH_DEPTH)
        || depth == UTUMNO_DEPTH || depth == UTUMNO_FORGE_DEPTH;
}

static void sync_vaults(void)
{
    if (!active || worker_job) return;
    vertical_vault_capture(&active->vertical_vaults);
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        level_image* floor = floors[i];
        if (!floor) continue;
        for (int p = 0; p < floor->vertical_vaults.count; p++) {
            vertical_vault_placement* part = &floor->vertical_vaults.placements[p];
            for (int j = 0; j < LEVEL_CACHE_SLOTS; j++) {
                if (!floors[j]) continue;
                for (int q = 0; q < floors[j]->vertical_vaults.count; q++) {
                    vertical_vault_placement* other = &floors[j]->vertical_vaults.placements[q];
                    if (other->identity != part->identity) continue;
                    part->reward_layers |= other->reward_layers;
                    part->discovered_layers |= other->discovered_layers;
                    other->reward_layers = part->reward_layers;
                    other->discovered_layers = part->discovered_layers;
                }
            }
        }
    }
    vertical_vault_restore(&active->vertical_vaults);
}

/* Counts include parked levels. Killing a monster on the active map therefore
 * cannot make the same unique available to an in-flight generation request. */
static void reserve_floor(const level_image* floor, int sign)
{
    for (int i = 1; i < floor->mon_max; i++) {
        int race = floor->monsters[i].r_idx;
        if (race > 0 && race < z_info->r_max && (r_info[race].flags1 & RF1_UNIQUE))
            r_info[race].cur_num = MAX(0, r_info[race].cur_num + sign);
    }
}

static void evict_floor(int slot)
{
    level_image* floor = floors[slot];
    if (!floor || floor == active) return;
    sync_vaults();
    sync_vertical_regions();
    cave_vertical_capture(&active->vertical);
    for (int i = 0; i < floor->vertical.portal_count; i++) {
        cave_vertical_portal* p = &floor->vertical.portals[i];
        level_image* receiver = find_floor(p->peer_depth);
        if (!p->pending || !(p->flags & CAVE_VERTICAL_DOWN) || !receiver) continue;
        for (int n = 0; n < receiver->vertical.portal_count; n++) {
            cave_vertical_portal* incoming = &receiver->vertical.portals[n];
            if (incoming->identity != p->identity || incoming->y != p->peer_y
                || incoming->x != p->peer_x || (incoming->flags & CAVE_VERTICAL_DOWN)) continue;
            int material = p->pending_material ? p->pending_material : p->material;
            int old_material = incoming->pending_material ? incoming->pending_material : incoming->material;
            if (incoming->pending && material != old_material
                && ((material == FEAT_WATER && old_material == FEAT_LAVA)
                    || (material == FEAT_LAVA && old_material == FEAT_WATER))) {
                int consumed = MIN(incoming->pending, p->pending);
                incoming->pending -= consumed; p->pending -= consumed;
                for (int b = 0; b < receiver->vertical.basin_count; b++)
                    if (receiver->vertical.basins[b].identity == p->identity
                        && receiver->vertical.basins[b].depth == receiver->depth) {
                        receiver->vertical.basins[b].hot = 6;
                        receiver->vertical.basins[b].flags |= CAVE_VERTICAL_CHANGED;
                    }
            }
            if (!incoming->pending) incoming->pending_material = material;
            incoming->pending += p->pending;
            incoming->due = MAX(incoming->due, p->due);
            p->pending = 0;
            p->pending_material = 0;
            break;
        }
    }
    cave_vertical_restore(&active->vertical);
    if (floor->visited) {
        quest_followup_discard_depth(floor->depth);
        for (int i = 1; i < floor->mon_max; i++) {
            int race = floor->monsters[i].r_idx;
            if (race == R_IDX_TULKAS && p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
                p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
            if (race == R_IDX_NIENA && p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
                p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
            if (race == R_IDX_OROME && p_ptr->orome_quest == OROME_QUEST_GIVER_PRESENT)
                p_ptr->orome_quest = OROME_QUEST_NOT_STARTED;
            if (race == R_IDX_VARDA && p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT)
                p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        }
        if (p_ptr->aule_level == floor->depth && p_ptr->aule_quest < AULE_QUEST_REWARDED)
            p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        if (p_ptr->mandos_level == floor->depth && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
            p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    }
    reserve_floor(floor, -1);
    /* Unseen artefacts on a forgotten map can be generated again, matching
     * wipe_o_list(). Artefacts carried away remain reserved by the player. */
    for (int i = 1; i < floor->o_max; i++) {
        const object_type* object = &floor->objects[i];
        if (object->k_idx && object->name1 && !object->marked
            && !(a_info[object->name1].seen & ART_SEEN_PHYSICAL)) {
            a_info[object->name1].cur_num = 0;
            a_info[object->name1].found_num = 0;
        }
    }
    level_image_free(floor);
    floors[slot] = NULL;
}

static void prune_cache(void)
{
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i] != active
            && ABS(floors[i]->depth - active->depth) > 1
            && floors[i]->depth != requested_depth
            && floors[i]->depth != pending_depth)
            evict_floor(i);
}

void level_cache_options_changed(void)
{
    if (!running || worker_job) return;
    if (job) SDL_SetAtomicInt(&job->cancel, 1);
    /* First-turn changes must also replace already published neighbours.
     * Keep every visited level, including the current map. */
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i] != active && !floors[i]->visited)
            evict_floor(i);
}

static void prepare_links(level_job* next)
{
    level_link_plan* links = &next->links;
    bool disconnected = birth_discon_stair || quest_challenge_active(CHALLENGE_DISCONNECTED);
    links->source_depth = active->depth;
    next->allow_down = !find_floor(next->depth + 1);
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        const level_image* source = floors[i];
        if (!source) continue;
        if (!disconnected)
            links->stair_peer_depths[links->stair_peer_count++] = source->depth;
        if (ABS(source->depth - next->depth) == 1 && next->inherited_count < 2) {
            int n = next->inherited_count++;
            next->inherited_depth[n] = source->depth;
            next->inherited_vertical[n] = source->vertical;
            next->inherited_vaults[n] = source->vertical_vaults;
            for (int p = 0; p < source->vertical.portal_count; p++) {
                const cave_vertical_portal* portal = &source->vertical.portals[p];
                if (portal->peer_depth != next->depth) continue;
                links->minimum_hgt = MAX(links->minimum_hgt, portal->peer_y + 5);
                links->minimum_wid = MAX(links->minimum_wid, portal->peer_x + 5);
            }
            for (int p = 0; p < source->vertical_vaults.count; p++) {
                const vertical_vault_placement* placement = &source->vertical_vaults.placements[p];
                const vault_type* v = &v_info[placement->upper_template];
                links->minimum_hgt = MAX(links->minimum_hgt, placement->y
                    + ((placement->transform & 4) ? v->wid : v->hgt) + 2);
                links->minimum_wid = MAX(links->minimum_wid, placement->x
                    + ((placement->transform & 4) ? v->hgt : v->wid) + 2);
            }
        }
        for (int y = 1; y < source->hgt - 1; y++)
            for (int x = 1; x < source->wid - 1; x++) {
                int feat = source->feat[y][x], inverse = 0, target = -1;
                if (feat == FEAT_LESS) { target = source->depth - 1; inverse = FEAT_MORE; }
                if (feat == FEAT_MORE) { target = source->depth + 1; inverse = FEAT_LESS; }
                if (feat == FEAT_LESS_SHAFT) { target = source->depth - 2; inverse = FEAT_MORE_SHAFT; }
                if (feat == FEAT_MORE_SHAFT) { target = source->depth + 2; inverse = FEAT_LESS_SHAFT; }
                if (!disconnected && target == next->depth) links->feat[y][x] = inverse;
                if (source->depth == next->depth - 1 && source->chasm_drop[y][x]) {
                    links->ceiling[y][x] = 1;
                    links->drop[y][x] = source->chasm_drop[y][x] - 1;
                    links->feat[y][x] = links->drop[y][x] ? FEAT_CHASM : FEAT_FLOOR;
                    /* Keep supporting crossings in the same columns. An open
                     * hole stays open; a bridge can carry another deck over
                     * the same underlying shaft on the next floor. */
                    if (links->drop[y][x] && feat != FEAT_CHASM)
                        links->feat[y][x] = feat == FEAT_BRIDGE_CHASM_V
                            ? FEAT_BRIDGE_CHASM_V : FEAT_BRIDGE_CHASM_H;
                }
                if (source->depth == next->depth + 1) {
                    if (source->chasm_up[y][x]) {
                        links->drop[y][x] = source->chasm_drop[y][x] + 1;
                        links->feat[y][x] = FEAT_CHASM;
                    }
                }
                /* A long fall must land at the same world coordinate even
                 * when its intermediate floors have not been visited. */
                if (source->chasm_drop[y][x]
                    && source->depth + source->chasm_drop[y][x] == next->depth) {
                    links->feat[y][x] = FEAT_FLOOR;
                    links->ceiling[y][x] = 1;
                }
                if (links->feat[y][x]) {
                    links->minimum_hgt = MAX(links->minimum_hgt, y + 2);
                    links->minimum_wid = MAX(links->minimum_wid, x + 2);
                }
            }
    }
}

void level_cache_generation_size(int* blocks)
{
    if (!worker_job) return;
    int extent = MAX(worker_job->links.minimum_hgt, worker_job->links.minimum_wid);
    *blocks = MAX(*blocks, (extent + PANEL_HGT - 1) / PANEL_HGT);
}

void level_cache_generation_reserve(void)
{
    cave_vertical_generation_begin(!worker_job || worker_job->allow_down);
    vertical_vault_begin(p_ptr->depth, !worker_job || worker_job->allow_down);
    if (worker_job) {
        for (int i = 0; i < worker_job->inherited_count; i++) {
            cave_vertical_generation_inherit(&worker_job->inherited_vertical[i],
                worker_job->inherited_depth[i], p_ptr->depth);
            vertical_vault_inherit(&worker_job->inherited_vaults[i], p_ptr->depth);
        }
    }
    cave_vertical_generation_reserve();
    if (!worker_job) return;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
            int feat = worker_job->links.feat[y][x];
            if (!feat) continue;
            terrain_generation_reserve(y, x);
            /* Keep the room planner's bedrock intact. The vault-fit hook
             * excludes these cells from authored rooms; finalisation applies
             * the holes and reroutes passages after the ordinary layout. */
        }
}

bool level_cache_generation_vault_fits(int y1, int x1, int y2, int x2)
{
    bool flip_v, flip_h;
    if (vertical_vault_build_transform(&flip_v, &flip_h)) return true;
    if (!vertical_vault_fits(y1, x1, y2, x2)
        || !cave_vertical_generation_fits(y1, x1, y2, x2)) return false;
    if (!worker_job || p_ptr->depth == 0 || p_ptr->depth == UTUMNO_FORGE_DEPTH) return true;
    for (int y = MAX(1, y1); y <= MIN(p_ptr->cur_map_hgt - 2, y2); y++)
        for (int x = MAX(1, x1); x <= MIN(p_ptr->cur_map_wid - 2, x2); x++)
            if (worker_job->links.feat[y][x]) return false;
    return true;
}

/* Inherited holes can sever passages which were connected before the vertical
 * geometry was applied. Reroute through bedrock while retaining the openings.
 * Authored endpoints only need new access to their paired stairs/landings. */
static bool connection_passable(int y, int x, bool endpoint)
{
    if (!player_passable(y, x, false)) return false;
    int feat = cave_feat[y][x];
    return !endpoint || !FEAT_IS_WALL(feat) || feat == FEAT_SECRET
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL);
}

static bool reconnect_regions(void)
{
    const bool authored_endpoint = p_ptr->depth == 0 || p_ptr->depth == UTUMNO_FORGE_DEPTH;
    typedef int access_map[MAX_DUNGEON_WID];
    int total = MAX_DUNGEON_HGT * MAX_DUNGEON_WID;
    access_map* access = SDL_calloc(MAX_DUNGEON_HGT, sizeof(*access));
    int* parent = SDL_malloc(total * sizeof(*parent));
    int* queue = SDL_malloc(total * sizeof(*queue));
    bool success = false;
    if (!access || !parent || !queue) goto done;
    for (int attempt = 0; attempt < 256; attempt++) {
        SDL_memset(access, 0, MAX_DUNGEON_HGT * sizeof(*access));
        if (!authored_endpoint) flood_access(p_ptr->py, p_ptr->px, access, false);
        else {
            int head = 0, tail = 0;
            access[p_ptr->py][p_ptr->px] = 1;
            queue[tail++] = p_ptr->py * MAX_DUNGEON_WID + p_ptr->px;
            while (head < tail) {
                int cell = queue[head++], y = cell / MAX_DUNGEON_WID, x = cell % MAX_DUNGEON_WID;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (!in_bounds_fully(yy, xx) || access[yy][xx]
                        || !connection_passable(yy, xx, true)) continue;
                    access[yy][xx] = 1; queue[tail++] = yy * MAX_DUNGEON_WID + xx;
                }
            }
        }
        bool disconnected = false;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !disconnected; y++)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (!access[y][x] && (!authored_endpoint || worker_job->links.feat[y][x])
                    && (!(cave_info[y][x] & CAVE_ICKY) || worker_job->links.feat[y][x])
                    && cave_floor_bold(y, x)
                    && connection_passable(y, x, authored_endpoint)) { disconnected = true; break; }
        if (!disconnected) { success = true; break; }
        SDL_memset(parent, 0xFF, total * sizeof(*parent));
        int head = 0, tail = 0, target = -1;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) if (access[y][x]) {
                int cell = y * MAX_DUNGEON_WID + x;
                parent[cell] = cell; queue[tail++] = cell;
            }
        while (head < tail && target < 0) {
            int cell = queue[head++], y = cell / MAX_DUNGEON_WID, x = cell % MAX_DUNGEON_WID;
            for (int dy = -1; dy <= 1 && target < 0; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (!in_bounds_fully(yy, xx)) continue;
                    int next = yy * MAX_DUNGEON_WID + xx;
                    if (parent[next] >= 0) continue;
                    bool passage = connection_passable(yy, xx, authored_endpoint);
                    int feat = cave_feat[yy][xx];
                    if (!passage && ((!FEAT_IS_ROCK(feat)
                                && !(authored_endpoint && cave_perma_bold(yy, xx)))
                            || (!authored_endpoint && (cave_perma_bold(yy, xx)
                                || (cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT)))))) continue;
                    parent[next] = cell;
                    if (passage && cave_floor_bold(yy, xx)
                        && (!(cave_info[yy][xx] & CAVE_ICKY) || worker_job->links.feat[yy][xx])
                        && (!authored_endpoint || worker_job->links.feat[yy][xx])) { target = next; break; }
                    queue[tail++] = next;
                }
        }
        if (target < 0) break;
        while (parent[target] != target) {
            int y = target / MAX_DUNGEON_WID, x = target % MAX_DUNGEON_WID;
            if (!connection_passable(y, x, authored_endpoint)) {
                cave_set_feat(y, x, FEAT_FLOOR);
                if (authored_endpoint) cave_info[y][x] &= ~(CAVE_ICKY | CAVE_G_VAULT);
            }
            target = parent[target];
        }
    }
done:
    SDL_free(access); SDL_free(parent); SDL_free(queue);
    return success;
}

bool level_cache_generation_links(void)
{
    if (!worker_job) {
        quest_challenge_prune_stairs();
        return true;
    }
    level_link_plan* links = &worker_job->links;
    /* Handcrafted endpoint maps can be extended with bedrock around their
     * authored area when a neighbouring stair lies beyond their old bounds. */
    int old_hgt = p_ptr->cur_map_hgt, old_wid = p_ptr->cur_map_wid;
    p_ptr->cur_map_hgt = MAX(old_hgt, links->minimum_hgt);
    p_ptr->cur_map_wid = MAX(old_wid, links->minimum_wid);
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            if (y >= old_hgt - 1 || x >= old_wid - 1)
                cave_set_feat(y, x, y == p_ptr->cur_map_hgt - 1
                    || x == p_ptr->cur_map_wid - 1 ? FEAT_WALL_PERM : FEAT_WALL_EXTRA);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
            int feat = cave_feat[y][x], target = -1;
            if (feat == FEAT_LESS) target = p_ptr->depth - 1;
            if (feat == FEAT_MORE) target = p_ptr->depth + 1;
            if (feat == FEAT_LESS_SHAFT) target = p_ptr->depth - 2;
            if (feat == FEAT_MORE_SHAFT) target = p_ptr->depth + 2;
            /* Every resident peer already owns its stair columns, including
             * the floor skipped by a shaft from the active floor. */
            for (int peer = 0; peer < links->stair_peer_count; peer++)
                if (target == links->stair_peer_depths[peer] && !links->feat[y][x]) {
                    cave_set_feat(y, x, FEAT_FLOOR);
                    break;
                }
            /* Generating upwards cannot open a new hole through the live
             * floor below. Only pre-existing vertical openings continue up. */
            if (p_ptr->depth < links->source_depth && feat == FEAT_CHASM
                && !links->drop[y][x])
                cave_set_feat(y, x, FEAT_FLOOR);
            if (!links->feat[y][x]) continue;
            if (cave_info[y][x] & CAVE_ICKY) {
                /* The paired blueprint has already built its exact endpoint.
                 * Matching inherited stairs are not a breach of protection. */
                if (feat == links->feat[y][x]) continue;
                if (p_ptr->depth != 0 && p_ptr->depth != UTUMNO_FORGE_DEPTH) return false;
                cave_info[y][x] &= ~(CAVE_ICKY | CAVE_G_VAULT);
            }
            cave_info[y][x] &= ~CAVE_WALL;
            cave_set_feat(y, x, links->feat[y][x]);
        }
    if (!player_passable(p_ptr->py, p_ptr->px, false)) {
        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
        bool placed = false;
        for (int pass = 0; pass < 2 && !placed; pass++)
            for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !placed; y++)
                for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
                    if (pass == 0 && !links->feat[y][x]) continue;
                    if (cave_m_idx[y][x] || !cave_floor_bold(y, x)
                        || !player_passable(y, x, false)) continue;
                    player_place(y, x); placed = true; break;
                }
        if (!placed) return false;
    }
    quest_challenge_prune_stairs();
    return reconnect_regions();
}

void level_cache_generation_finished(void)
{
    level_image* image = worker_job ? worker_job->image : active;
    if (!image) return;
    /* Retain inherited geometry before assigning lengths to new openings. */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
            if (worker_job) {
                image->chasm_up[y][x] = worker_job->links.ceiling[y][x];
                image->chasm_drop[y][x] = worker_job->links.drop[y][x];
            }
            for (int i = 0; i < cave_vertical_get()->portal_count; i++) {
                const cave_vertical_portal* portal = &cave_vertical_get()->portals[i];
                if (portal->kind == CAVE_VERTICAL_LAKE && portal->y == y && portal->x == x
                    && (portal->flags & CAVE_VERTICAL_DOWN)) image->chasm_drop[y][x] = 1;
                if (portal->kind == CAVE_VERTICAL_LAKE && portal->y == y && portal->x == x
                    && !(portal->flags & CAVE_VERTICAL_DOWN)) image->chasm_up[y][x] = 1;
            }
            if (cave_environment_live_underlay(y, x) != FEAT_CHASM) {
                image->chasm_drop[y][x] = 0; continue;
            }
        }
    /* One depth per connected opening gives a coherent shaft instead of a
     * different random bottom for every tile of the same chasm. */
    byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = {{0}};
    int* queue = SDL_malloc(MAX_DUNGEON_HGT * MAX_DUNGEON_WID * sizeof(*queue));
    if (!queue) return;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
            if (seen[y][x] || cave_environment_live_underlay(y, x) != FEAT_CHASM) continue;
            int head = 0, tail = 0, drop = 0;
            bool lake_landing = false;
            seen[y][x] = 1; queue[tail++] = y * MAX_DUNGEON_WID + x;
            while (head < tail) {
                int cell = queue[head++], cy = cell / MAX_DUNGEON_WID, cx = cell % MAX_DUNGEON_WID;
                if (image->chasm_drop[cy][cx]) drop = image->chasm_drop[cy][cx];
                for (int p = 0; p < cave_vertical_get()->portal_count; p++) {
                    const cave_vertical_portal* portal = &cave_vertical_get()->portals[p];
                    if (portal->kind == CAVE_VERTICAL_LAKE && portal->y == cy && portal->x == cx
                        && (portal->flags & CAVE_VERTICAL_DOWN)) lake_landing = true;
                }
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                    int yy = cy + dy, xx = cx + dx;
                    if (!in_bounds_fully(yy, xx) || seen[yy][xx]
                        || cave_environment_live_underlay(yy, xx) != FEAT_CHASM) continue;
                    seen[yy][xx] = 1; queue[tail++] = yy * MAX_DUNGEON_WID + xx;
                }
            }
            if (lake_landing) drop = 1;
            if (!drop) {
                drop = 2;
                while (p_ptr->depth + drop < MORGOTH_DEPTH && percent_chance(50 / (drop - 1))) drop++;
                drop = MIN(drop, MAX(1, MORGOTH_DEPTH - p_ptr->depth));
            }
            for (int i = 0; i < tail; i++) {
                int cy = queue[i] / MAX_DUNGEON_WID, cx = queue[i] % MAX_DUNGEON_WID;
                if (lake_landing || !image->chasm_drop[cy][cx]) image->chasm_drop[cy][cx] = drop;
            }
        }
    SDL_free(queue);
}

bool level_cache_chasm_above(int y, int x)
{
    return active && in_bounds(y, x) && active->chasm_up[y][x];
}

int level_cache_chasm_fall_depth(int y, int x)
{
    int drop = active && in_bounds(y, x) ? active->chasm_drop[y][x] : 0;
    return MIN(p_ptr->depth + (drop ? drop : 2), MORGOTH_DEPTH);
}

static int SDLCALL generate_neighbor(void* data)
{
    level_job* next = data;
    worker_job = next;
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    p_ptr = &next->player; op_ptr = &next->options;
    r_info = next->races; k_info = next->kinds; a_info = next->artefacts;
    for (int i = 1; i < z_info->r_max; i++)
        if (!(r_info[i].flags1 & RF1_UNIQUE)) r_info[i].cur_num = 0;
    l_list = next->lore; inventory = next->pack; smith_o_ptr = &next->smith;
    valar_reserved_artifacts = next->reserved_artefacts;
    metar = next->tale;
    meta_state_use_generation_copy(next->meta_state);
    drop_catalog_use_worker_copy(next->drop_catalog);
    alloc_race_table = next->race_alloc;
    alloc_kind_table = next->kind_alloc;
    alloc_ego_table = next->ego_alloc;
    level_image_bind_grids(next->image);
    cave_cost = SDL_malloc((MAX_FLOWS) * sizeof(*cave_cost));
    view_g = SDL_calloc(VIEW_MAX, sizeof(*view_g));
    temp_g = SDL_calloc(TEMP_MAX, sizeof(*temp_g));
    temp_y = SDL_calloc(TEMP_MAX, sizeof(*temp_y));
    temp_x = SDL_calloc(TEMP_MAX, sizeof(*temp_x));
    dun_data worker_dun;
    dun = &worker_dun;
    turn = next->turn; playerturn = MAX(1, next->playerturn);
    min_depth_counter = next->minimum_depth_counter;
    Rand_state_init(next->seed);
    p_ptr->depth = next->depth;
    p_ptr->create_stair = p_ptr->create_rubble = 0;
    if (cave_cost && view_g && temp_g && temp_y && temp_x && !level_cache_generation_cancelled()) {
        generate_cave();
        if (!level_cache_generation_cancelled() && character_dungeon) {
            level_cache_generation_finished();
            level_image_capture(next->image);
            next->success = true;
        }
    }
    meta_state_finish_generation();
    legendary_area_restore_runtime(NULL);
    SDL_free(cave_cost);
    SDL_free(view_g); SDL_free(temp_g); SDL_free(temp_y); SDL_free(temp_x);
    SDL_SetAtomicInt(&next->done, 1);
    if (next->wake_event_type && next->wake_event_type != (Uint32)-1) {
        SDL_Event ready = {0};
        ready.type = next->wake_event_type;
        SDL_PushEvent(&ready);
    }
    return 0;
}

static void free_job(level_job* old)
{
    SDL_free(old->races); SDL_free(old->kinds); SDL_free(old->artefacts);
    SDL_free(old->lore); SDL_free(old->race_alloc); SDL_free(old->kind_alloc);
    SDL_free(old->ego_alloc); SDL_free(old->reserved_artefacts);
    SDL_free(old->drop_catalog);
    meta_state_free_generation_copy(old->meta_state);
    level_image_free(old->image);
    SDL_free(old);
}

static void* copy_array(const void* source, size_t bytes)
{
    void* result = SDL_malloc(bytes);
    if (result && bytes) {
        if (source) SDL_memcpy(result, source, bytes);
        else SDL_memset(result, 0, bytes);
    }
    return result;
}

static void merge_generated_player(player_type* live,
    const player_type* before, const player_type* after);

static void start_job(int depth)
{
    if (job || !valid_depth(depth) || find_floor(depth)) return;
    level_job* next = SDL_calloc(1, sizeof(*next));
    if (!next) return;
    next->depth = depth;
    next->wake_event_type = ready_event_type;
    next->player = *p_ptr;
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i]->pending_player)
            merge_generated_player(&next->player,
                &floors[i]->generation_before, &floors[i]->generation_after);
    next->before = next->player;
    next->options = *op_ptr;
    next->tale = metar;
    SDL_memcpy(next->pack, inventory, sizeof(next->pack));
    next->smith = *smith_o_ptr;
    next->turn = turn; next->playerturn = playerturn;
    next->minimum_depth_counter = min_depth_counter;
    next->seed = ((u64b)Rand_div(UINT32_MAX) << 32) | Rand_div(UINT32_MAX);
    next->image = level_image_new();
    next->races = copy_array(r_info, z_info->r_max * sizeof(*r_info));
    next->kinds = copy_array(k_info, z_info->k_max * sizeof(*k_info));
    next->artefacts = copy_array(a_info, z_info->art_max * sizeof(*a_info));
    next->lore = copy_array(l_list, z_info->r_max * sizeof(*l_list));
    next->race_alloc = copy_array(alloc_race_table, alloc_race_size * sizeof(*alloc_race_table));
    next->kind_alloc = copy_array(alloc_kind_table, alloc_kind_size * sizeof(*alloc_kind_table));
    next->ego_alloc = copy_array(alloc_ego_table, alloc_ego_size * sizeof(*alloc_ego_table));
    next->reserved_artefacts = copy_array(valar_reserved_artifacts, z_info->art_max * sizeof(bool));
    next->drop_catalog = drop_catalog_copy_for_worker();
    next->meta_state = meta_state_copy_for_generation();
    if (!next->image || !next->races || !next->kinds || !next->artefacts
        || !next->lore || !next->race_alloc || !next->kind_alloc || !next->ego_alloc
        || !next->reserved_artefacts || !next->drop_catalog || !next->meta_state) { free_job(next); return; }
    level_cache_generation_finished();
    level_image_capture(active);
    sync_vaults();
    sync_vertical_regions();
    prepare_links(next);
    next->thread = SDL_CreateThread(generate_neighbor, "neighbor-level", next);
    if (!next->thread) { free_job(next); return; }
    job = next;
    log_debug("Level cache: background generation started for depth %d", depth);
}

/* Only generation-owned fields are merged. Never copy the worker's health,
 * experience, inventory, position, command state, energy or gameplay clock. */
static void merge_generated_player(player_type* live,
    const player_type* before, const player_type* after)
{
#define MERGE(field) do { if (SDL_memcmp(&before->field, &after->field, sizeof(after->field)) \
    && !SDL_memcmp(&live->field, &before->field, sizeof(before->field))) \
    SDL_memcpy(&live->field, &after->field, sizeof(after->field)); } while (0)
    MERGE(unique_forge_made);
    for (int i = 0; i < MAX_GREATER_VAULTS; i++) {
        int vault = after->greater_vaults[i];
        bool present = false, inherited = false;
        if (!vault) continue;
        for (int n = 0; n < MAX_GREATER_VAULTS; n++)
            if (before->greater_vaults[n] == vault) inherited = true;
        if (inherited) continue;
        for (int n = 0; n < MAX_GREATER_VAULTS; n++)
            if (live->greater_vaults[n] == vault) present = true;
        if (!present) for (int n = 0; n < MAX_GREATER_VAULTS; n++)
            if (!live->greater_vaults[n]) { live->greater_vaults[n] = vault; break; }
    }
    MERGE(tulkas_quest); MERGE(tulkas_target_r_idx); MERGE(tulkas_prize_a_idx);
    MERGE(aule_quest); MERGE(aule_level); MERGE(aule_forge_y); MERGE(aule_forge_x);
    MERGE(mandos_quest); MERGE(mandos_level); MERGE(mandos_vault_y); MERGE(mandos_vault_x);
    MERGE(niena_quest); MERGE(niena_monsters_seen); MERGE(niena_monsters_killed);
    MERGE(orome_quest); MERGE(orome_target_type); MERGE(orome_target_count);
    MERGE(varda_quest); MERGE(varda_level); MERGE(varda_vault_placed); MERGE(varda_vault_ready);
    for (int i = 0; i < 10; i++) {
        MERGE(quest_followup_state[i]); MERGE(quest_followup_flags[i]);
        MERGE(quest_followup_depth[i]); MERGE(quest_followup_progress[i]);
    }
    live->quest_reserved[0] = MIN(255, live->quest_reserved[0]
        + after->quest_reserved[0] - before->quest_reserved[0]);
#undef MERGE
    live->forge_count += after->forge_count - before->forge_count;
    live->quest_vault_used = MIN(255, live->quest_vault_used
        + after->quest_vault_used - before->quest_vault_used);
}

static bool generation_conflicts(const level_job* completed)
{
    if (completed->options.opt[OPT_quest_challenges_beta] != op_ptr->opt[OPT_quest_challenges_beta]
        || completed->options.opt[OPT_birth_discon_stair] != birth_discon_stair
        || completed->options.opt[OPT_birth_no_artefacts] != birth_no_artefacts
        || completed->options.opt[OPT_adult_no_artefacts] != adult_no_artefacts
        || completed->player.quest_challenge != p_ptr->quest_challenge) return true;
    for (int option = OPT_meta_artefact_memory; option <= OPT_meta_legendary_places; option++)
        if (completed->options.opt[option] != op_ptr->opt[option]) return true;
    const player_type* before = &completed->before;
    const player_type* after = &completed->player;
#define CONFLICT(field) (SDL_memcmp(&before->field, &after->field, sizeof(after->field)) \
    && SDL_memcmp(&p_ptr->field, &before->field, sizeof(before->field)))
    if (CONFLICT(tulkas_quest) || CONFLICT(aule_quest) || CONFLICT(mandos_quest)
        || CONFLICT(niena_quest) || CONFLICT(orome_quest) || CONFLICT(varda_quest)) return true;
    for (int i = 0; i < 10; i++)
        if (CONFLICT(quest_followup_state[i]) || CONFLICT(quest_followup_flags[i])) return true;
#undef CONFLICT
    for (int i = 1; i < completed->image->mon_max; i++) {
        int race = completed->image->monsters[i].r_idx;
        if (race && (r_info[race].flags1 & RF1_UNIQUE)
            && r_info[race].cur_num >= r_info[race].max_num) return true;
    }
    for (int i = 1; i < completed->image->o_max; i++) {
        int art = completed->image->objects[i].name1;
        if (art && a_info[art].cur_num) return true;
    }
    return false;
}

void level_cache_poll(void)
{
    if (!running || worker_job || !active) return;
    if (job && SDL_GetAtomicInt(&job->done)) {
        SDL_WaitThread(job->thread, NULL);
        bool conflict = job->success && (SDL_GetAtomicInt(&job->cancel) || generation_conflicts(job));
        if (conflict) log_debug("Level cache: regenerating depth %d after a run-state conflict", job->depth);
        if (job->success && !conflict && (ABS(job->depth - active->depth) <= 1
                || job->depth == requested_depth || job->depth == pending_depth)) {
            prune_cache();
            for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) if (!floors[i]) {
                floors[i] = job->image;
                job->image = NULL;
                floors[i]->generation_before = job->before;
                floors[i]->generation_after = job->player;
                floors[i]->pending_player = true;
                reserve_floor(floors[i], 1);
                for (int n = 1; n < floors[i]->o_max; n++) {
                    int art = floors[i]->objects[n].name1;
                    if (art) a_info[art].cur_num = MAX(a_info[art].cur_num, 1);
                }
                log_debug("Level cache: depth %d ready", job->depth);
                sync_vaults();
                p_ptr->redraw |= PR_MAP;
                break;
            }
        }
        free_job(job); job = NULL;
    }
    if (pending_depth >= 0 && find_floor(pending_depth) && !character_icky) {
        p_ptr->depth = pending_depth;
        p_ptr->leaving = true;
        use_pending_origin = true;
        pending_depth = -1;
        return;
    }
    if (!job) {
        if (requested_depth >= 0 && !find_floor(requested_depth)) start_job(requested_depth);
        else if (valid_depth(active->depth + 1) && !find_floor(active->depth + 1)) start_job(active->depth + 1);
        else if (valid_depth(active->depth - 1) && !find_floor(active->depth - 1)) start_job(active->depth - 1);
    }
}

bool level_cache_request(int depth)
{
    if (!running) return true;
    requested_depth = depth;
    level_cache_poll();
    return find_floor(depth) != NULL;
}

bool level_cache_transition_pending(void)
{
    return !worker_job && pending_depth >= 0;
}

void level_cache_start(void)
{
    if (!ready_event_type) ready_event_type = SDL_RegisterEvents(1);
    if (running) { level_cache_poll(); return; }
    active = level_image_new();
    if (!active) return;
    active->visited = true;
    floors[0] = active;
    running = true;
    quest_challenge_prune_stairs();
    level_image_capture(active);
    level_cache_generation_finished();
    level_cache_poll();
}

static bool ordinary_stair_destination(int feature, int source, int destination)
{
    if (source < 0 || source > MORGOTH_DEPTH
        || destination < 0 || destination > MORGOTH_DEPTH) return false;
    int step;
    switch (feature) {
    case FEAT_LESS: step = -1; break;
    case FEAT_MORE: step = 1; break;
    case FEAT_LESS_SHAFT: step = -2; break;
    case FEAT_MORE_SHAFT: step = 2; break;
    default: return false;
    }
    return destination == source + step;
}

bool level_cache_change_level(void)
{
    if (!running || !active) return false;
    int destination = p_ptr->depth;
    int arrival_y = use_pending_origin ? pending_y : p_ptr->py;
    int arrival_x = use_pending_origin ? pending_x : p_ptr->px;
    int departure_feat = cave_feat[p_ptr->py][p_ptr->px];
    bool disconnected_stair = use_pending_origin ? pending_disconnected_stair
        : (birth_discon_stair || quest_challenge_active(CHALLENGE_DISCONNECTED))
            && ordinary_stair_destination(departure_feat, active->depth, destination);
    /* Capture the outgoing map under its actual depth, not the already
     * updated destination stored by the movement command. */
    p_ptr->depth = active->depth;
    level_cache_generation_finished();
    level_image_capture(active);
    sync_vaults();
    p_ptr->depth = destination;
    if (destination == active->depth) {
        /* Explicit same-depth regeneration (wizard/cheat death) discards it. */
        level_cache_clear();
        return false;
    }
    level_image* target = find_floor(destination);
    if (!target) {
        pending_depth = requested_depth = destination;
        pending_y = arrival_y; pending_x = arrival_x;
        pending_rubble = p_ptr->create_rubble;
        pending_disconnected_stair = disconnected_stair;
        p_ptr->create_stair = 0;
        p_ptr->create_rubble = false;
        p_ptr->depth = active->depth;
        p_ptr->leaving = false;
        level_cache_poll();
        return true;
    }
    if (target->pending_player) {
        merge_generated_player(p_ptr, &target->generation_before, &target->generation_after);
        target->pending_player = false;
    }
    cave_vertical_state transit;
    cave_vertical_capture(&transit);
    level_image_restore(target);
    if (transit.submerged) {
        cave_vertical_state arriving;
        cave_vertical_capture(&arriving);
        arriving.submerged = transit.submerged;
        arriving.breath = transit.breath;
        cave_vertical_restore(&arriving);
    }
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    if (disconnected_stair) {
        /* Disconnected stairs use an independent landing on every visit.
         * Physical falls and stream travel still retain their world column. */
        int candidates = 0;
        bool niena_level = p_ptr->niena_quest == NIENA_QUEST_ACTIVE;
        for (int i = 1; i < mon_max && !niena_level; i++)
            niena_level = mon_list[i].r_idx == R_IDX_NIENA;
        byte near_down[MAX_DUNGEON_HGT][MAX_DUNGEON_WID] = {{0}};
        bool found_down = false;
        if (niena_level)
            for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
                for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
                    if (cave_feat[y][x] != FEAT_MORE && cave_feat[y][x] != FEAT_MORE_SHAFT) continue;
                    found_down = true;
                    for (int yy = MAX(1, y - 86); yy < MIN(p_ptr->cur_map_hgt - 1, y + 87); yy++)
                        for (int xx = MAX(1, x - 86); xx < MIN(p_ptr->cur_map_wid - 1, x + 87); xx++)
                            if (distance(y, x, yy, xx) < 87) near_down[yy][xx] = 1;
                }
        arrival_y = arrival_x = -1;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (cave_naked_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY)
                    && (playerturn > 0 || (cave_info[y][x] & CAVE_ROOM))
                    && player_passable(y, x, false)
                    && (!niena_level || (found_down && !near_down[y][x]))
                    && one_in_(++candidates)) {
                    arrival_y = y; arrival_x = x;
                }
    }
    if (in_bounds_fully(arrival_y, arrival_x)
        && cave_floor_bold(arrival_y, arrival_x)
        && cave_feat[arrival_y][arrival_x] != FEAT_CHASM) {
        if (cave_m_idx[arrival_y][arrival_x] > 0) {
            /* Landing never destroys a resident monster: exchange it with
             * the saved, empty entry grid. */
            int idx = cave_m_idx[arrival_y][arrival_x];
            mon_list[idx].fy = p_ptr->py; mon_list[idx].fx = p_ptr->px;
            cave_m_idx[p_ptr->py][p_ptr->px] = idx;
            cave_m_idx[arrival_y][arrival_x] = 0;
        }
        p_ptr->py = arrival_y; p_ptr->px = arrival_x;
    }
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    if (use_pending_origin) p_ptr->create_rubble = pending_rubble;
    use_pending_origin = false;
    p_ptr->create_stair = 0;
    p_ptr->skip_next_turn = false;
    if (!(p_ptr->smithing_leftover > 0 && smith_o_ptr->k_idx && smith_o_ptr->unused1 == 2))
        p_ptr->smithing_leftover = 0;
    else p_ptr->smithing = 0;
    p_ptr->leaving = false;
    active = target;
    cave_vertical_combat_reset();
    cave_vertical_refresh();
    if (!target->visited) {
        for (int i = 1; i < o_max; i++)
            if (o_list[i].k_idx) apply_autoinscription(&o_list[i]);
        if (target->depth == UTUMNO_FORGE_DEPTH) p_ptr->utumno_forge_visited = true;
        if (target->depth == MORGOTH_DEPTH) p_ptr->utumno_return_to_throne = false;
        catastrophe_prepare_level(); target->visited = true;
    }
    requested_depth = pending_depth = -1;
    use_pending_origin = pending_rubble = pending_disconnected_stair = false;
    if (job && ABS(job->depth - active->depth) > 1)
        SDL_SetAtomicInt(&job->cancel, 1);
    prune_cache();
    level_cache_poll();
    return true;
}

const level_image* level_cache_floor(int depth)
{
    return worker_job ? NULL : find_floor(depth);
}

level_image* level_cache_mutable_floor(int depth)
{
    return worker_job || !active || depth == active->depth ? NULL : find_floor(depth);
}

bool level_cache_travel_to(int depth, int y, int x)
{
    if (!valid_depth(depth) || y < 1 || x < 1 || y >= MAX_DUNGEON_HGT - 1
        || x >= MAX_DUNGEON_WID - 1) return false;
    pending_y = y; pending_x = x;
    pending_disconnected_stair = false;
    use_pending_origin = true;
    p_ptr->create_stair = p_ptr->create_rubble = 0;
    p_ptr->depth = depth;
    p_ptr->leaving = true;
    return true;
}

bool level_cache_vertical_aperture(int y, int x, int* depth, int* target_y, int* target_x)
{
    if (!p_ptr || !in_bounds_fully(y, x) || FEAT_IS_BRIDGE(cave_feat[y][x])) return false;
    const cave_vertical_state* state = cave_vertical_get();
    for (int i = 0; i < state->portal_count; i++) {
        const cave_vertical_portal* p = &state->portals[i];
        if (p->y != y || p->x != x || (p->flags & CAVE_VERTICAL_CLOSED)
            || p->kind == CAVE_VERTICAL_STREAM || p->kind == CAVE_VERTICAL_SUMP
            || p->kind == CAVE_VERTICAL_CONTROL || p->kind == CAVE_VERTICAL_RAPIDS
            || p->kind == CAVE_VERTICAL_TUBE)
            continue;
        if (depth) *depth = p->peer_depth;
        if (target_y) *target_y = p->peer_y;
        if (target_x) *target_x = p->peer_x;
        return true;
    }
    if (cave_feat[y][x] != FEAT_CHASM) return false;
    if (depth) *depth = p_ptr->depth + 1;
    if (target_y) *target_y = y;
    if (target_x) *target_x = x;
    return true;
}

bool level_cache_vertical_receive(int depth, u32b identity, int material, int amount)
{
    if (!active || worker_job) return false;
    if (depth == active->depth) {
        cave_vertical_state state;
        cave_vertical_capture(&state);
        if (!cave_vertical_receive(&state, depth, identity, material, amount)) return false;
        return cave_vertical_restore(&state);
    }
    level_image* floor = find_floor(depth);
    return floor && cave_vertical_receive(&floor->vertical, depth, identity, material, amount);
}

void level_cache_process_vertical(void)
{
    if (!active || worker_job) return;
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i] != active
            && ABS(floors[i]->depth - active->depth) == 1)
            cave_vertical_process_image(floors[i]);
    sync_vertical_regions();
}

static void sync_vertical_regions(void)
{
    if (!active || worker_job) return;
    cave_vertical_capture(&active->vertical);
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        level_image* floor = floors[i];
        if (!floor) continue;
        for (int b = 0; b < floor->vertical.basin_count; b++) {
            cave_vertical_basin* mirror = &floor->vertical.basins[b];
            level_image* owner = find_floor(mirror->depth);
            if (!owner || owner == floor) continue;
            for (int n = 0; n < owner->vertical.basin_count; n++) {
                const cave_vertical_basin* actual = &owner->vertical.basins[n];
                if (actual->identity == mirror->identity && actual->depth == mirror->depth)
                    *mirror = *actual;
            }
        }
        for (int p = 0; p < floor->vertical.portal_count; p++) {
            cave_vertical_portal* mirror = &floor->vertical.portals[p];
            level_image* owner = find_floor(mirror->peer_depth);
            if (!owner) continue;
            for (int n = 0; n < owner->vertical.portal_count; n++) {
                const cave_vertical_portal* actual = &owner->vertical.portals[n];
                if (actual->identity != mirror->identity || actual->kind != mirror->kind
                    || actual->peer_depth != floor->depth || actual->peer_y != mirror->y
                    || actual->peer_x != mirror->x) continue;
                SDL_memcpy(mirror->peer_patch, actual->local_patch, sizeof(mirror->peer_patch));
                mirror->peer_style = actual->local_style;
                if (actual->flags & CAVE_VERTICAL_DOWN)
                    mirror->material = actual->material;
                if (actual->flags & CAVE_VERTICAL_DOWN)
                    mirror->flags = (mirror->flags & ~(CAVE_VERTICAL_CLOSED | CAVE_VERTICAL_BROKEN))
                        | (actual->flags & (CAVE_VERTICAL_CLOSED | CAVE_VERTICAL_BROKEN));
            }
        }
    }
    cave_vertical_restore(&active->vertical);
}

typedef enum remote_monster_operation_kind {
    REMOTE_DAMAGE, REMOTE_POISON_BRAND, REMOTE_POISON_TICK
} remote_monster_operation_kind;

static bool remote_monster_operation(int depth, int index, int damage,
    const char* note, int who, const object_type* first, const object_type* second,
    remote_monster_operation_kind operation, bool observed)
{
    level_image* floor = level_cache_mutable_floor(depth);
    if (!floor || index <= 0 || index >= floor->mon_max
        || !floor->monsters[index].r_idx || damage < 0) return false;
    /* Resolve canonical death/quest/drop bookkeeping on the owning map. */
    level_image_capture(active);
    player_type before = *p_ptr;
    level_image_restore(floor);
    bool visible_before = mon_list[index].ml;
    if (observed) mon_list[index].ml = true;
    bool dead = false;
    if (operation == REMOTE_DAMAGE) dead = mon_take_hit(index, damage, note, who);
    else if (operation == REMOTE_POISON_BRAND) monster_poison_brand(index, first, second, damage);
    else dead = monster_poison_tick(index);
    /* Observation belongs to this shot, never to a parked map's next turn.
     * A canonical death may remove the slot or transform a special unique. */
    if (mon_list[index].r_idx) mon_list[index].ml = visible_before;
    player_type after = *p_ptr;
    level_image_capture(floor);
    level_image_restore(active);
    after.depth = before.depth; after.py = before.py; after.px = before.px;
    after.cur_map_hgt = before.cur_map_hgt; after.cur_map_wid = before.cur_map_wid;
    after.wy = before.wy; after.wx = before.wx;
    after.leaving = before.leaving; after.energy = before.energy;
    after.energy_use = before.energy_use; after.truce = before.truce;
    after.target_set = before.target_set; after.target_who = before.target_who;
    after.target_row = before.target_row; after.target_col = before.target_col;
    after.health_who = before.health_who;
    *p_ptr = after;
    p_ptr->update |= PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS;
    p_ptr->redraw |= PR_MAP;
    return dead;
}

bool level_cache_damage_monster(int depth, int index, int damage, const char* note, int who)
{
    return remote_monster_operation(depth, index, damage, note, who, NULL, NULL, REMOTE_DAMAGE, false);
}

bool level_cache_damage_monster_observed(int depth, int index, int damage,
    const char* note, int who)
{
    return remote_monster_operation(depth, index, damage, note, who, NULL, NULL, REMOTE_DAMAGE, true);
}

void level_cache_poison_monster(int depth, int index,
    const object_type* first, const object_type* second, int damage)
{
    if (damage > 0) remote_monster_operation(depth, index, damage, NULL, 0, first, second, REMOTE_POISON_BRAND, true);
}

bool level_cache_poison_tick(int depth, int index)
{
    return remote_monster_operation(depth, index, 0, NULL, 0, NULL, NULL, REMOTE_POISON_TICK, false);
}

bool level_cache_transfer_object(int depth, int index, int y, int x)
{
    level_image* floor = level_cache_mutable_floor(depth);
    if (!floor || index <= 0 || index >= o_max || !o_list[index].k_idx
        || o_list[index].held_m_idx || y < 1 || x < 1 || y >= floor->hgt - 1
        || x >= floor->wid - 1 || FEAT_IS_WALL(floor->feat[y][x])) return false;
    int slot;
    for (slot = 1; slot < floor->o_max; slot++)
        if (!floor->objects[slot].k_idx) break;
    if (slot == floor->o_max && floor->o_max >= z_info->o_max) return false;
    object_type moved = o_list[index];
    moved.iy = y; moved.ix = x;
    moved.next_o_idx = floor->objects_at[y][x];
    floor->objects[slot] = moved;
    floor->objects_at[y][x] = slot;
    floor->o_max = MAX(floor->o_max, slot + 1);
    floor->o_cnt++;
    delete_object_idx(index);
    return true;
}

void level_cache_clear(void)
{
    if (worker_job) return;
    if (job) {
        SDL_SetAtomicInt(&job->cancel, 1);
        SDL_WaitThread(job->thread, NULL);
        free_job(job); job = NULL;
    }
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        if (floors[i] && floors[i] != active) evict_floor(i);
    }
    /* Eviction may hand packets to the active recipient. Keep it alive until
     * every neighbour has relinquished its ownership. */
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        level_image_free(floors[i]); floors[i] = NULL;
    }
    active = NULL; running = false;
    requested_depth = pending_depth = -1;
    use_pending_origin = pending_rubble = pending_disconnected_stair = false;
    cave_vertical_combat_reset();
}

#include "fs/save-internal.h"
#include "fs/load-internal.h"

#define LEVEL_CACHE_SAVE_MAGIC 0x4C43

static void write_generated_player(const player_type* player)
{
#define WRITE_FIELD(field) save_wr_s32b((s32b)player->field);
    LEVEL_GENERATED_PLAYER_FIELDS(WRITE_FIELD)
#undef WRITE_FIELD
    save_wr_byte(player->quest_reserved[0]);
    for (int i = 0; i < MAX_GREATER_VAULTS; i++) save_wr_s16b(player->greater_vaults[i]);
    for (int i = 0; i < 10; i++) {
        save_wr_byte(player->quest_followup_state[i]);
        save_wr_byte(player->quest_followup_flags[i]);
        save_wr_s16b(player->quest_followup_depth[i]);
        save_wr_u16b(player->quest_followup_progress[i]);
    }
}

static bool read_generated_player(player_type* player)
{
    u32b offset = load_byte_offset;
    SDL_memset(player, 0, sizeof(*player));
    s32b value;
#define READ_FIELD(field) do { load_rd_s32b(&value); player->field = value; \
    if ((s32b)player->field != value) return false; } while (0);
    LEVEL_GENERATED_PLAYER_FIELDS(READ_FIELD)
#undef READ_FIELD
    load_rd_byte(&player->quest_reserved[0]);
    for (int i = 0; i < MAX_GREATER_VAULTS; i++) {
        load_rd_s16b(&player->greater_vaults[i]);
        if (player->greater_vaults[i] < 0 || player->greater_vaults[i] >= z_info->v_max) return false;
    }
    for (int i = 0; i < 10; i++) {
        load_rd_byte(&player->quest_followup_state[i]);
        load_rd_byte(&player->quest_followup_flags[i]);
        load_rd_s16b(&player->quest_followup_depth[i]);
        load_rd_u16b(&player->quest_followup_progress[i]);
        if (player->quest_followup_state[i] > QUEST_STATE_REWARDED
            || player->quest_followup_flags[i] > 15
            || player->quest_followup_depth[i] < 0
            || player->quest_followup_depth[i] > UTUMNO_FORGE_DEPTH) return false;
    }
#define FIELD_SIZE(field) + 4
    return load_byte_offset - offset == (0 LEVEL_GENERATED_PLAYER_FIELDS(FIELD_SIZE))
        + 1 + MAX_GREATER_VAULTS * 2 + 10 * 6;
#undef FIELD_SIZE
}

static void write_regions(const level_image* image)
{
    const cave_vertical_state* s = &image->vertical;
    save_wr_byte(s->portal_count); save_wr_byte(s->basin_count);
    save_wr_byte(s->breath); save_wr_byte(s->submerged); save_wr_s32b(s->last_turn);
    for (int i = 0; i < s->portal_count; i++) {
        const cave_vertical_portal* p = &s->portals[i];
        save_wr_u32b(p->identity);
        save_wr_s16b(p->y); save_wr_s16b(p->x); save_wr_s16b(p->peer_depth);
        save_wr_s16b(p->peer_y); save_wr_s16b(p->peer_x);
        save_wr_byte(p->kind); save_wr_byte(p->material); save_wr_byte(p->strength);
        save_wr_byte(p->direction); save_wr_byte(p->flags); save_wr_byte(p->pending_material);
        save_wr_byte(p->local_style); save_wr_byte(p->peer_style);
        save_wr_u16b(p->pending); save_wr_s32b(p->due);
        for (int y = 0; y < CAVE_VERTICAL_PATCH; y++)
            for (int x = 0; x < CAVE_VERTICAL_PATCH; x++) {
                save_wr_byte(p->local_patch[y][x]); save_wr_byte(p->peer_patch[y][x]);
            }
    }
    for (int i = 0; i < s->basin_count; i++) {
        const cave_vertical_basin* b = &s->basins[i];
        save_wr_u32b(b->identity); save_wr_s16b(b->depth);
        save_wr_s16b(b->y); save_wr_s16b(b->x);
        save_wr_u16b(b->volume); save_wr_u16b(b->capacity); save_wr_u16b(b->normal_volume);
        save_wr_byte(b->material); save_wr_byte(b->radius); save_wr_byte(b->hot); save_wr_byte(b->flags);
    }
    save_wr_byte(image->vertical_vaults.count);
    for (int i = 0; i < image->vertical_vaults.count; i++) {
        const vertical_vault_placement* p = &image->vertical_vaults.placements[i];
        save_wr_u32b(p->identity); save_wr_u16b(p->upper_template); save_wr_u16b(p->lower_template);
        save_wr_s16b(p->upper_depth); save_wr_s16b(p->y); save_wr_s16b(p->x);
        save_wr_byte(p->transform); save_wr_byte(p->discovered_layers); save_wr_byte(p->reward_layers);
    }
}

static bool read_regions(level_image* image)
{
    u32b start = load_byte_offset;
    cave_vertical_state* s = &image->vertical;
    SDL_memset(s, 0, sizeof(*s));
    load_rd_byte(&s->portal_count); load_rd_byte(&s->basin_count);
    load_rd_byte(&s->breath); load_rd_byte(&s->submerged); load_rd_s32b(&s->last_turn);
    if (s->portal_count > CAVE_VERTICAL_PORTALS || s->basin_count > CAVE_VERTICAL_BASINS)
        return false;
    for (int i = 0; i < s->portal_count; i++) {
        cave_vertical_portal* p = &s->portals[i];
        load_rd_u32b(&p->identity);
        load_rd_s16b(&p->y); load_rd_s16b(&p->x); load_rd_s16b(&p->peer_depth);
        load_rd_s16b(&p->peer_y); load_rd_s16b(&p->peer_x);
        load_rd_byte(&p->kind); load_rd_byte(&p->material); load_rd_byte(&p->strength);
        load_rd_byte(&p->direction); load_rd_byte(&p->flags); load_rd_byte(&p->pending_material);
        load_rd_byte(&p->local_style); load_rd_byte(&p->peer_style);
        load_rd_u16b(&p->pending); load_rd_s32b(&p->due);
        for (int y = 0; y < CAVE_VERTICAL_PATCH; y++)
            for (int x = 0; x < CAVE_VERTICAL_PATCH; x++) {
                load_rd_byte(&p->local_patch[y][x]); load_rd_byte(&p->peer_patch[y][x]);
                if (p->local_patch[y][x] >= z_info->f_max || p->peer_patch[y][x] >= z_info->f_max)
                    return false;
            }
    }
    for (int i = 0; i < s->basin_count; i++) {
        cave_vertical_basin* b = &s->basins[i];
        load_rd_u32b(&b->identity); load_rd_s16b(&b->depth);
        load_rd_s16b(&b->y); load_rd_s16b(&b->x);
        load_rd_u16b(&b->volume); load_rd_u16b(&b->capacity); load_rd_u16b(&b->normal_volume);
        load_rd_byte(&b->material); load_rd_byte(&b->radius); load_rd_byte(&b->hot); load_rd_byte(&b->flags);
    }
    if (!cave_vertical_restore(s)) return false;
    load_rd_byte(&image->vertical_vaults.count);
    if (image->vertical_vaults.count > VERTICAL_VAULT_MAX) return false;
    for (int i = 0; i < image->vertical_vaults.count; i++) {
        vertical_vault_placement* p = &image->vertical_vaults.placements[i];
        load_rd_u32b(&p->identity); load_rd_u16b(&p->upper_template); load_rd_u16b(&p->lower_template);
        load_rd_s16b(&p->upper_depth); load_rd_s16b(&p->y); load_rd_s16b(&p->x);
        load_rd_byte(&p->transform); load_rd_byte(&p->discovered_layers); load_rd_byte(&p->reward_layers);
        if (p->upper_template >= z_info->v_max || p->lower_template >= z_info->v_max
            || p->transform > 7 || p->discovered_layers > 3 || p->reward_layers > 3
            || !p->identity || p->upper_depth < 1 || p->upper_depth >= MORGOTH_DEPTH - 1
            || p->y < 1 || p->x < 1 || p->y >= image->hgt || p->x >= image->wid
            || (image->depth != p->upper_depth && image->depth != p->upper_depth + 1)) return false;
    }
    vertical_vault_restore(&image->vertical_vaults);
    return vertical_vault_get()->count == image->vertical_vaults.count
        && load_byte_offset - start == 9u + 126u * s->portal_count
            + 20u * s->basin_count + 17u * image->vertical_vaults.count;
}

static void write_vertical(const level_image* image)
{
    save_wr_byte(image->visited ? 1 : 0);
    save_wr_byte(image->pending_player ? 1 : 0);
    if (image->pending_player) {
        write_generated_player(&image->generation_before);
        write_generated_player(&image->generation_after);
    }
    save_wr_byte(image->hgt); save_wr_byte(image->wid);
    save_wr_s32b(image->suspended_turn);
    for (int y = 0; y < image->hgt; y++)
        for (int x = 0; x < image->wid; x++) {
            save_wr_byte(image->chasm_drop[y][x]);
            save_wr_byte(image->chasm_up[y][x]);
        }
    for (int i = 0; i < 64; i++) save_wr_byte(image->floor_choices[i]);
    const skeleton_note_state_save* skeleton = &image->skeleton;
    save_wr_s16b(skeleton->level_depth); save_wr_s16b(skeleton->note_cap);
    save_wr_s16b(skeleton->notes_shown); save_wr_s16b(skeleton->map_wid);
    save_wr_s16b(skeleton->map_hgt); save_wr_u32b(skeleton->hint_used_mask);
    for (int i = 0; i < SKEL_HINT_MAX; i++) save_wr_byte(skeleton->hint_use_counts[i]);
    save_wr_byte(skeleton->seen_count);
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; i++) save_wr_s16b(skeleton->seen_ids[i]);
    save_wr_byte(image->hint_count);
    for (int i = 0; i < image->hint_count; i++) {
        const hint_message_meta* meta = &image->hint_metadata[i];
        save_wr_byte(image->hint_lengths[i]);
        for (int line = 0; line < image->hint_lengths[i]; line++) save_wr_string(image->hints[i][line]);
        save_wr_s16b(meta->source_y); save_wr_s16b(meta->source_x);
        save_wr_byte(meta->cue_count);
        for (int n = 0; n < meta->cue_count; n++) {
            save_wr_string(meta->cue_dirs[n]); save_wr_string(meta->cue_dists[n]);
        }
        save_wr_byte(meta->destination_count);
        for (int n = 0; n < meta->destination_count; n++) {
            const hint_message_destination* dest = &meta->destinations[n];
            save_wr_byte(dest->kind); save_wr_s16b(dest->y); save_wr_s16b(dest->x);
            save_wr_s16b(dest->id); save_wr_s16b(dest->min_dist); save_wr_s16b(dest->max_dist);
        }
    }
    write_regions(image);
}

static bool read_vertical(level_image* image)
{
    byte visited = 0, pending = 0, hgt = 0, wid = 0;
    s32b suspended = 0;
    u32b offset = load_byte_offset;
    load_rd_byte(&visited); load_rd_byte(&pending);
    if (load_byte_offset - offset != 2 || visited > 1 || pending > 1
        || (visited && pending)) return false;
    image->pending_player = pending != 0;
    if (pending && (!read_generated_player(&image->generation_before)
            || !read_generated_player(&image->generation_after))) return false;
    offset = load_byte_offset;
    load_rd_byte(&hgt); load_rd_byte(&wid);
    load_rd_s32b(&suspended);
    if (load_byte_offset - offset != 6
        || hgt != image->hgt || wid != image->wid
        || suspended < 0 || suspended > turn) return false;
    image->visited = visited != 0;
    image->suspended_turn = suspended;
    for (int y = 0; y < image->hgt; y++)
        for (int x = 0; x < image->wid; x++) {
            byte drop = 0, ceiling = 0;
            offset = load_byte_offset;
            load_rd_byte(&drop); load_rd_byte(&ceiling);
            if (load_byte_offset - offset != 2 || ceiling > 1
                || drop > MAX(1, MORGOTH_DEPTH - image->depth)
                || (drop && cave_environment_live_underlay(y, x) != FEAT_CHASM)) return false;
            image->chasm_drop[y][x] = drop;
            image->chasm_up[y][x] = ceiling;
        }
    offset = load_byte_offset;
    for (int i = 0; i < 64; i++) load_rd_byte(&image->floor_choices[i]);
    if (load_byte_offset - offset != 64) return false;
    skeleton_note_state_save* skeleton = &image->skeleton;
    offset = load_byte_offset;
    load_rd_s16b(&skeleton->level_depth); load_rd_s16b(&skeleton->note_cap);
    load_rd_s16b(&skeleton->notes_shown); load_rd_s16b(&skeleton->map_wid);
    load_rd_s16b(&skeleton->map_hgt); load_rd_u32b(&skeleton->hint_used_mask);
    for (int i = 0; i < SKEL_HINT_MAX; i++) load_rd_byte(&skeleton->hint_use_counts[i]);
    load_rd_byte(&skeleton->seen_count);
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; i++) load_rd_s16b(&skeleton->seen_ids[i]);
    if (load_byte_offset - offset != 15 + SKEL_HINT_MAX + SKELETON_NOTE_SEEN_MAX * 2
        || skeleton->seen_count > SKELETON_NOTE_SEEN_MAX || skeleton->note_cap < 0
        || skeleton->notes_shown < 0 || skeleton->notes_shown > skeleton->note_cap) return false;
    offset = load_byte_offset;
    load_rd_byte(&image->hint_count);
    if (load_byte_offset - offset != 1 || image->hint_count > 32) return false;
    for (int i = 0; i < image->hint_count; i++) {
        hint_message_meta* meta = &image->hint_metadata[i];
        load_rd_byte(&image->hint_lengths[i]);
        if (!image->hint_lengths[i] || image->hint_lengths[i] > 16) return false;
        for (int line = 0; line < image->hint_lengths[i]; line++) load_rd_string(image->hints[i][line], 100);
        load_rd_s16b(&meta->source_y); load_rd_s16b(&meta->source_x);
        load_rd_byte(&meta->cue_count);
        if (meta->cue_count > HINT_MESSAGE_CUE_MAX) return false;
        for (int n = 0; n < meta->cue_count; n++) {
            load_rd_string(meta->cue_dirs[n], HINT_MESSAGE_CUE_TEXT_MAX);
            load_rd_string(meta->cue_dists[n], HINT_MESSAGE_CUE_TEXT_MAX);
        }
        load_rd_byte(&meta->destination_count);
        if (meta->destination_count > HINT_MESSAGE_DESTINATION_MAX) return false;
        for (int n = 0; n < meta->destination_count; n++) {
            hint_message_destination* dest = &meta->destinations[n];
            offset = load_byte_offset;
            load_rd_byte(&dest->kind); load_rd_s16b(&dest->y); load_rd_s16b(&dest->x);
            load_rd_s16b(&dest->id); load_rd_s16b(&dest->min_dist); load_rd_s16b(&dest->max_dist);
            if (load_byte_offset - offset != 11 || dest->kind > HINT_DESTINATION_FIXED_QUEST_SITE) return false;
        }
    }
    if (savefile_version_at_least(0, 9, 9, 6)) return read_regions(image);
    SDL_memset(&image->vertical, 0, sizeof(image->vertical));
    image->vertical.breath = 12; image->vertical.last_turn = turn;
    SDL_memset(&image->vertical_vaults, 0, sizeof(image->vertical_vaults));
    return true;
}

void level_cache_write(void)
{
    /* Saving never waits for the worker. A partially generated neighbour is
     * reconstructed in the background after loading. */
    save_wr_u16b(LEVEL_CACHE_SAVE_MAGIC);
    if (!running || !active) level_cache_start();
    if (!active) { save_wr_byte(0); return; }
    const player_type player = *p_ptr;
    level_image_capture(active);
    sync_vaults();
    sync_vertical_regions();
    save_wr_byte(1);
    save_wr_s16b(pending_depth);
    save_wr_s16b(pending_y); save_wr_s16b(pending_x);
    save_wr_byte(pending_rubble ? 1 : 0);
    write_vertical(active);
    int count = 0;
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++)
        if (floors[i] && floors[i] != active && ABS(floors[i]->depth - active->depth) == 1) count++;
    save_wr_byte(count);
    for (int i = 0; i < LEVEL_CACHE_SLOTS; i++) {
        level_image* floor = floors[i];
        if (!floor || floor == active || ABS(floor->depth - active->depth) != 1) continue;
        level_image_restore(floor);
        save_write_partition_meta();
        save_write_dungeon();
        /* Dungeon/environment timers were advanced to the current world
         * clock on restore; keep that same reference in the saved metadata. */
        s32b suspended = floor->suspended_turn;
        floor->suspended_turn = turn;
        write_vertical(floor);
        floor->suspended_turn = suspended;
    }
    level_image_restore(active);
    *p_ptr = player;
}

errr level_cache_read(void)
{
    u16b magic = 0;
    byte present = 0, count = 0;
    u32b offset = load_byte_offset;
    load_rd_u16b(&magic); load_rd_byte(&present);
    if (load_byte_offset - offset != 3 || magic != LEVEL_CACHE_SAVE_MAGIC || present > 1) return -1;
    if (!present) return 0;
    s16b saved_pending = -1, saved_y = 0, saved_x = 0;
    byte saved_rubble = 0;
    offset = load_byte_offset;
    load_rd_s16b(&saved_pending); load_rd_s16b(&saved_y); load_rd_s16b(&saved_x);
    load_rd_byte(&saved_rubble);
    if (load_byte_offset - offset != 7 || saved_rubble > 1
        || (saved_pending != -1 && (!valid_depth(saved_pending)
            || saved_pending == p_ptr->depth || !in_bounds_fully(saved_y, saved_x)))) return -1;
    level_image* current = level_image_new();
    if (!current) return -1;
    const player_type player = *p_ptr;
    level_image_capture(current);
    if (!read_vertical(current)) { level_image_free(current); return -1; }
    floors[0] = active = current;
    running = true;
    offset = load_byte_offset;
    load_rd_byte(&count);
    if (load_byte_offset - offset != 1 || count > 2) goto invalid;
    for (int i = 1; i <= count; i++) {
        character_dungeon = false;
        mon_max = o_max = 1;
        mon_cnt = o_cnt = num_repro = 0;
        SDL_memset(mon_list, 0, MAX_MONSTERS * sizeof(*mon_list));
        SDL_memset(o_list, 0, z_info->o_max * sizeof(*o_list));
        if (load_read_partition_meta()) goto invalid;
        reading_floor = true;
        errr result = load_read_dungeon();
        reading_floor = false;
        if (result) goto invalid;
        if (ABS(p_ptr->depth - current->depth) != 1 || find_floor(p_ptr->depth)) goto invalid;
        level_image* floor = level_image_new();
        if (!floor) goto invalid;
        floors[i] = floor;
        level_image_capture(floor);
        if (!read_vertical(floor)) goto invalid;
    }
    level_image_restore(current);
    *p_ptr = player;
    requested_depth = pending_depth = saved_pending;
    pending_y = saved_y; pending_x = saved_x;
    pending_rubble = saved_rubble != 0;
    /* The pending origin is already persisted. Recover stair semantics from
     * the outgoing map without extending the save lane. Explicit watercourse
     * portals retain their specified destination, even on a stair grid. */
    pending_disconnected_stair = false;
    if (saved_pending >= 0 && saved_y == p_ptr->py && saved_x == p_ptr->px
        && (birth_discon_stair || quest_challenge_active(CHALLENGE_DISCONNECTED))
        && ordinary_stair_destination(cave_feat[saved_y][saved_x], p_ptr->depth, saved_pending)) {
        pending_disconnected_stair = true;
        const cave_vertical_state* vertical = cave_vertical_get();
        for (int i = 0; i < vertical->portal_count; i++) {
            const cave_vertical_portal* portal = &vertical->portals[i];
            if (portal->y == p_ptr->py && portal->x == p_ptr->px
                && portal->peer_depth == saved_pending
                && portal->peer_y == saved_y && portal->peer_x == saved_x) {
                pending_disconnected_stair = false;
                break;
            }
        }
    }
    return 0;
invalid:
    reading_floor = false;
    level_image_restore(current);
    *p_ptr = player;
    level_cache_clear();
    return -1;
}
