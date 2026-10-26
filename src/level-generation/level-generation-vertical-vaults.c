#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-vertical-vaults.h"
#include "level-generation/level-cache.h"

/* Only neighbor-owned blueprints survive eviction. This is not a floor
 * archive: no monsters, objects or ordinary terrain are retained here. */
static _Thread_local vertical_vault_state current;
static _Thread_local int current_depth;
static _Thread_local bool new_down_allowed;
static _Thread_local int building = -1;
static _Thread_local bool suppress_rewards;

static bool pair_valid(int upper, int lower)
{
    if (upper <= 0 || lower <= 0 || upper >= z_info->v_max
        || lower >= z_info->v_max) return false;
    const vault_type *a = &v_info[upper], *b = &v_info[lower];
    if (!a->name || !b->name || a->typ != 12 || b->typ != 12
        || a->vertical_companion != lower || b->vertical_companion != upper
        || a->vertical_layer != 0 || b->vertical_layer != 1
        || a->hgt != b->hgt || a->wid != b->wid || !a->hgt || !a->wid)
        return false;
    for (int layer = 0; layer < 2; ++layer)
    {
        const vault_type *v = layer ? b : a;
        bool stair = false;
        int approaches = 0;
        int drain_rewards = 0;
        for (int i = 0; i < v->vertical_marker_count; ++i)
        {
            int y = v->vertical_marker_y[i], x = v->vertical_marker_x[i];
            if (y >= v->hgt || x >= v->wid) return false;
            if (v->vertical_marker_role[i] == VERTICAL_VAULT_STAIR)
            {
                if (v_text[a->text + y * a->wid + x] != '>'
                    || v_text[b->text + y * b->wid + x] != '<') return false;
                stair = true;
            }
            if (v->vertical_marker_role[i] == VERTICAL_VAULT_APPROACH)
                ++approaches;
            if (v->vertical_marker_role[i] == VERTICAL_VAULT_DRAIN_REWARD)
            {
                char token = v_text[v->text + y * v->wid + x];
                if (++drain_rewards > 1 || (token != '_' && token != '"')) return false;
            }
        }
        if (!stair || approaches < 2) return false;
        /* One layer bit owns either ordinary authored loot or a single
         * delayed award. Mixing them would remint ordinary loot while wet. */
        if (drain_rewards)
            for (int i = 0; i < v->hgt * v->wid; ++i)
                if (strchr("*&!~?She5689=", v_text[v->text + i])) return false;
    }
    return true;
}

static int layer_of(const vertical_vault_placement *p, int depth)
{
    return depth == p->upper_depth ? 0 : depth == p->upper_depth + 1 ? 1 : -1;
}

static void bounds(const vertical_vault_placement *p, int *h, int *w)
{
    const vault_type *v = &v_info[p->upper_template];
    *h = (p->transform & 4) ? v->wid : v->hgt;
    *w = (p->transform & 4) ? v->hgt : v->wid;
}

static void transform_point(const vertical_vault_placement *p,
    int row, int col, int *y, int *x)
{
    const vault_type *v = &v_info[p->upper_template];
    if (p->transform & 1) row = v->hgt - 1 - row;
    if (p->transform & 2) col = v->wid - 1 - col;
    *y = p->y + ((p->transform & 4) ? col : row);
    *x = p->x + ((p->transform & 4) ? row : col);
}

void vertical_vault_reset(void)
{
    memset(&current, 0, sizeof(current));
    current_depth = 0;
    new_down_allowed = false;
    building = -1;
    suppress_rewards = false;
}

void vertical_vault_begin(int depth, bool allow_down)
{
    vertical_vault_reset();
    current_depth = depth;
    new_down_allowed = allow_down && depth >= 1 && depth < MORGOTH_DEPTH - 1;
}

const vertical_vault_state *vertical_vault_get(void) { return &current; }
void vertical_vault_capture(vertical_vault_state *out) { if (out) *out = current; }

void vertical_vault_restore(const vertical_vault_state *state)
{
    vertical_vault_reset();
    current_depth = p_ptr->depth;
    if (state) vertical_vault_inherit(state, current_depth);
}

void vertical_vault_inherit(const vertical_vault_state *neighbor, int target_depth)
{
    if (!neighbor) return;
    for (int i = 0; i < MIN(neighbor->count, VERTICAL_VAULT_MAX); ++i)
    {
        const vertical_vault_placement *p = &neighbor->placements[i];
        if (!p->identity || p->transform > 7 || p->y <= -MAX_DUNGEON_HGT || p->x <= -MAX_DUNGEON_WID
            || p->upper_depth < 1 || p->upper_depth >= MORGOTH_DEPTH - 1
            || layer_of(p, target_depth) < 0
            || !pair_valid(p->upper_template, p->lower_template)) continue;
        int h, w; bounds(p, &h, &w);
        if (p->y + h <= 1 || p->x + w <= 1
            || p->y >= p_ptr->cur_map_hgt - 1 || p->x >= p_ptr->cur_map_wid - 1) continue;
        int n;
        for (n = 0; n < current.count; ++n)
            if (current.placements[n].identity == p->identity) break;
        if (n < current.count)
        {
            current.placements[n].discovered_layers |= p->discovered_layers & 3;
            current.placements[n].reward_layers |= p->reward_layers & 3;
        }
        else if (current.count < VERTICAL_VAULT_MAX)
        {
            current.placements[current.count++] = *p;
        }
    }
}

bool vertical_vault_fits(int y1, int x1, int y2, int x2)
{
    for (int i = 0; i < current.count; ++i)
    {
        if (i == building) continue;
        int h, w;
        bounds(&current.placements[i], &h, &w);
        const vertical_vault_placement *p = &current.placements[i];
        if (y1 <= p->y + h && y2 >= p->y - 1
            && x1 <= p->x + w && x2 >= p->x - 1) return false;
    }
    return true;
}

bool vertical_vault_build_transform(bool *flip_v, bool *flip_h)
{
    if (building < 0) return false;
    *flip_v = (current.placements[building].transform & 1) != 0;
    *flip_h = (current.placements[building].transform & 2) != 0;
    return true;
}

bool vertical_vault_suppress_rewards(void) { return suppress_rewards; }

static int drain_reward_marker(const vault_type *v)
{
    for (int i = 0; i < v->vertical_marker_count; ++i)
        if (v->vertical_marker_role[i] == VERTICAL_VAULT_DRAIN_REWARD) return i;
    return -1;
}

void vertical_vault_release_rewards(void)
{
    if (current_depth != p_ptr->depth) return;
    for (int i = 0; i < current.count; ++i)
    {
        vertical_vault_placement *p = &current.placements[i];
        int layer = layer_of(p, current_depth);
        if (layer < 0 || (p->reward_layers & (1 << layer))) continue;
        vault_type *v = &v_info[layer ? p->lower_template : p->upper_template];
        int marker = drain_reward_marker(v), y, x;
        if (marker < 0) continue;
        transform_point(p, v->vertical_marker_y[marker], v->vertical_marker_x[marker], &y, &x);
        if (!in_bounds_fully(y, x) || cave_feat[y][x] != FEAT_FLOOR || cave_o_idx[y][x]) continue;
        bool space = o_max < z_info->o_max;
        for (int n = 1; !space && n < o_max; ++n) space = !o_list[n].k_idx;
        if (!space) continue;
        /* Match the former '*' token's frequency, depth and partition policy.
         * A deliberate no-loot roll consumes the award, as ordinary builds do. */
        if (!vault_drop_passes(VDG_NORMAL))
        {
            p->reward_layers |= 1 << layer;
            continue;
        }
        int depth = player_generation_depth();
        partition_drop_profile profile = partition_drop_profile_for_mode_source_cfg(
            drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
        place_object_with_profile_params(y, x, depth, depth + dieroll(5),
            DROP_QUALITY_NORMAL, DROP_TYPE_NOT_DAMAGED, false, 1, 0, &profile);
        if (!cave_o_idx[y][x]) continue;
        p->reward_layers |= 1 << layer;
        if (!level_cache_in_worker() && character_dungeon && !p_ptr->blind
            && !p_ptr->image && (cave_info[y][x] & CAVE_SEEN))
            msg_print("The draining cistern reveals treasure beneath the water.");
    }
}

int vertical_vault_liquid_profile(const vertical_vault_state *state, int depth,
    u32b identity, int y, int x)
{
    if (!state || !identity) return 0;
    for (int i = 0; i < MIN(state->count, VERTICAL_VAULT_MAX); ++i)
    {
        const vertical_vault_placement *p = &state->placements[i];
        int layer = layer_of(p, depth);
        if (p->identity != identity || layer < 0 || p->transform > 7
            || !pair_valid(p->upper_template, p->lower_template)) continue;
        const vault_type *v = &v_info[layer ? p->lower_template : p->upper_template];
        int row = (p->transform & 4) ? x - p->x : y - p->y;
        int col = (p->transform & 4) ? y - p->y : x - p->x;
        if (row < 0 || col < 0 || row >= v->hgt || col >= v->wid) continue;
        if (p->transform & 1) row = v->hgt - 1 - row;
        if (p->transform & 2) col = v->wid - 1 - col;
        cptr data = v_text + v->text;
        char token = data[row * v->wid + col];
        if (token == '"') return 3;
        if (token == '_') return 2;
        if (token != '.') return 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                int yy = row + dy, xx = col + dx;
                if (yy < 0 || xx < 0 || yy >= v->hgt || xx >= v->wid) continue;
                token = data[yy * v->wid + xx];
                if (token == '_' || token == '"') return 1;
            }
        return 0;
    }
    return 0;
}

static bool build_pair_layer(int index)
{
    vertical_vault_placement *p = &current.placements[index];
    int layer = layer_of(p, current_depth), h, w;
    if (layer < 0 || !pair_valid(p->upper_template, p->lower_template)) return false;
    bounds(p, &h, &w);
    if (p->y + h >= p_ptr->cur_map_hgt - 1
        || p->x + w >= p_ptr->cur_map_wid - 1
        || dun->cent_n >= room_capacity_limit()) return false;
    vault_type *v = &v_info[layer ? p->lower_template : p->upper_template];
    building = index;
    suppress_rewards = (p->reward_layers & (1 << layer)) != 0;
    bool result = build_vault(p->y + h / 2, p->x + w / 2, v,
        (p->transform & 4) != 0);
    building = -1;
    suppress_rewards = false;
    if (!result) return false;
    if (drain_reward_marker(v) < 0) p->reward_layers |= 1 << layer;
    int slot = dun->cent_n++;
    dun->cent[slot].y = p->y + h / 2;
    dun->cent[slot].x = p->x + w / 2;
    /* The circulation centre must be walkable even when blueprint centre is
     * water or a pillar. The mandatory stair is connected to both approaches. */
    for (int i = 0; i < v->vertical_marker_count; ++i)
        if (v->vertical_marker_role[i] == VERTICAL_VAULT_STAIR)
        {
            int y, x;
            transform_point(p, v->vertical_marker_y[i], v->vertical_marker_x[i], &y, &x);
            dun->cent[slot].y = y;
            dun->cent[slot].x = x;
        }
    dun->corner[slot].y1 = p->y;
    dun->corner[slot].x1 = p->x;
    dun->corner[slot].y2 = p->y + h - 1;
    dun->corner[slot].x2 = p->x + w - 1;
    dun->kind[slot] = ROOM_KIND_GREATER_VAULT;
    mark_room_anchor_meta(slot, LAYOUT_ANCHOR_SETPIECE, false);
    return true;
}

bool vertical_vault_place(void)
{
    for (int i = 0; i < current.count; ++i)
        if (!build_pair_layer(i)) return false;
    if (!new_down_allowed || current.count >= VERTICAL_VAULT_MAX) return true;
    /* Do not attach a second copy of a pair to this same edge. */
    for (int i = 0; i < current.count; ++i)
        if (current.placements[i].upper_depth == current_depth) return true;
    for (int id = 1; id < z_info->v_max; ++id)
    {
        const vault_type *v = &v_info[id];
        if (!v->vertical_companion || v->vertical_layer || v->depth > current_depth
            || (v->max_depth && v->max_depth < current_depth)
            || !pair_valid(id, v->vertical_companion)
            || !one_in_(MAX(1, v->rarity))) continue;
        vertical_vault_placement p;
        memset(&p, 0, sizeof(p));
        p.upper_template = id;
        p.lower_template = v->vertical_companion;
        p.upper_depth = current_depth;
        p.transform = (v->flags & VLT_NO_ROTATION) ? 0 : rand_int(8);
        int h, w;
        bounds(&p, &h, &w);
        /* Keep a new blueprint inside the smallest ordinary (66x66) map so a
         * later first visit to its companion can inherit the complete frame. */
        int max_y = MIN(p_ptr->cur_map_hgt, 66) - h - 3;
        int max_x = MIN(p_ptr->cur_map_wid, 66) - w - 3;
        if (max_y < 3 || max_x < 3) continue;
        for (int attempt = 0; attempt < 64; ++attempt)
        {
            p.y = rand_range(3, max_y);
            p.x = rand_range(3, max_x);
            if (!vertical_vault_fits(p.y - 1, p.x - 1, p.y + h, p.x + w)
                || !level_cache_generation_vault_fits(p.y - 1, p.x - 1,
                    p.y + h, p.x + w)
                || !solid_rock(p.y - 1, p.x - 1, p.y + h, p.x + w)) continue;
            /* Stable identity is local to the depth edge and transformed frame. */
            p.identity = 0x80000000U | ((u32b)current_depth << 24)
                | ((u32b)id << 12) | ((u32b)p.y << 6) | p.x;
            int n = current.count++;
            current.placements[n] = p;
            if (build_pair_layer(n)) return true;
            --current.count;
        }
    }
    return true;
}

bool vertical_vault_marker_at(int index, vertical_vault_marker *out)
{
    if (index < 0 || !out) return false;
    for (int i = 0; i < current.count; ++i)
    {
        const vertical_vault_placement *p = &current.placements[i];
        int layer = layer_of(p, current_depth);
        if (layer < 0) continue;
        const vault_type *v = &v_info[layer ? p->lower_template : p->upper_template];
        if (index >= v->vertical_marker_count) { index -= v->vertical_marker_count; continue; }
        memset(out, 0, sizeof(*out));
        out->role = v->vertical_marker_role[index];
        int y, x;
        transform_point(p, v->vertical_marker_y[index], v->vertical_marker_x[index], &y, &x);
        out->y = out->target_y = y;
        out->x = out->target_x = x;
        out->target_depth = p->upper_depth + (layer ? 0 : 1);
        out->identity = p->identity;
        return true;
    }
    return false;
}

bool vertical_vault_peer_marker_at(int index, vertical_vault_marker* out)
{
    if (index < 0 || !out) return false;
    for (int i = 0; i < current.count; i++) {
        const vertical_vault_placement* p = &current.placements[i];
        int layer = layer_of(p, current_depth);
        if (layer < 0) continue;
        const vault_type* v = &v_info[layer ? p->upper_template : p->lower_template];
        if (index >= v->vertical_marker_count) { index -= v->vertical_marker_count; continue; }
        memset(out, 0, sizeof(*out));
        out->role = v->vertical_marker_role[index];
        int y, x;
        transform_point(p, v->vertical_marker_y[index], v->vertical_marker_x[index], &y, &x);
        out->y = out->target_y = y; out->x = out->target_x = x;
        out->target_depth = p->upper_depth + (layer ? 0 : 1);
        out->identity = p->identity;
        return true;
    }
    return false;
}

int vertical_vault_visit(int y, int x)
{
    int first = 0;
    for (int i = 0; i < current.count; ++i)
    {
        vertical_vault_placement *p = &current.placements[i];
        int layer = layer_of(p, current_depth), h, w;
        if (layer < 0) continue;
        bounds(p, &h, &w);
        if (y >= p->y && y < p->y + h && x >= p->x && x < p->x + w) {
            if (!p->discovered_layers) first = layer ? p->lower_template : p->upper_template;
            p->discovered_layers |= 1 << layer;
        }
    }
    return first;
}
