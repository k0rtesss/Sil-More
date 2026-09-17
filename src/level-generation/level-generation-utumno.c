#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "cave/cave-water-flow.h"

/* These templates are selected explicitly, never by the ordinary vault lottery. */
#define UTUMNO_DOORS_VAULT 523
#define UTUMNO_FORGE_VAULT 524
#define UTUMNO_ICE_STYLE 62
#define UTUMNO_FIRE_STYLE 63

static bool utumno_build_setpiece(int index, int cy, int cx)
{
    if (index >= z_info->v_max || !v_info[index].name)
    {
        log_error("Utumno: missing required vault %d", index);
        return false;
    }
    vault_type *vault = &v_info[index];
    if (!build_vault(cy, cx, vault, false)) return false;
    int slot = dun->cent_n++;
    dun->cent[slot].y = cy;
    dun->cent[slot].x = cx;
    dun->corner[slot].y1 = cy - vault->hgt / 2;
    dun->corner[slot].x1 = cx - vault->wid / 2;
    dun->corner[slot].y2 = dun->corner[slot].y1 + vault->hgt - 1;
    dun->corner[slot].x2 = dun->corner[slot].x1 + vault->wid - 1;
    dun->kind[slot] = ROOM_KIND_GREATER_VAULT;
    mark_room_anchor_meta(slot, LAYOUT_ANCHOR_SETPIECE, false);
    mark_g_vault(cy, cx, vault->hgt, vault->wid);
    SDL_strlcpy(g_vault_name, v_name + vault->name, sizeof(g_vault_name));
    good_item_flag = true;
    for (int i = 0; i < MAX_GREATER_VAULTS; ++i)
        if (!p_ptr->greater_vaults[i])
        {
            p_ptr->greater_vaults[i] = index;
            break;
        }

    /* Preserve authored floors, predictable doors and forge identity: generic
     * vault trap rolls must not add trapdoors to this one-way branch. */
    for (int row = 0; row < vault->hgt; ++row)
        for (int col = 0; col < vault->wid; ++col)
        {
            int y = dun->corner[slot].y1 + row;
            int x = dun->corner[slot].x1 + col;
            char token = v_text[vault->text + row * vault->wid + col];
            if (token == ' ') continue;
            if (cave_feat[y][x] >= FEAT_TRAP_HEAD
                && cave_feat[y][x] <= FEAT_TRAP_TAIL)
                cave_set_feat(y, x, FEAT_FLOOR);
            cave_info[y][x] &= ~CAVE_HIDDEN;
            if (token == '+') cave_set_feat(y, x, FEAT_DOOR_HEAD);
            if (p_ptr->depth == UTUMNO_FORGE_DEPTH && token == '<')
                cave_info[y][x] &= ~CAVE_G_VAULT;
            if (token == '0')
            {
                cave_set_feat(y, x, FEAT_FORGE_UNIQUE_TAIL);
                p_ptr->unique_forge_made = true;
            }
            if (token == '~' && !cave_o_idx[y][x])
            {
                partition_population_meta meta;
                memset(&meta, 0, sizeof(meta));
                set_partition_chest_recipe(&meta, 0, 2, 0, 100, 0,
                    PARTITION_CHEST_ANCHOR_ANY);
                u16b info = cave_info[y][x];
                cave_info[y][x] &= ~CAVE_G_VAULT;
                bool placed = place_partition_chest_at(-1, y, x,
                    &meta.chest_recipes[0], QUAD_MODE_ROOMY, false);
                cave_info[y][x] = info;
                drop_clear_chest_material_weights();
                drop_set_chest_mode(0);
                if (!placed) return false;
            }
        }
    return true;
}

static bool utumno_forge_gen(void)
{
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 66;
    remember_partition_grid(1, 1, 1);
    current_partition_modes[0] = QUAD_MODE_ROOMY;
    current_partition_densities[0] = DENSITY_SPARSE;
    for (int y = 0; y < 66; ++y)
        for (int x = 0; x < 66; ++x)
            cave_set_feat_style(y, x, FEAT_WALL_PERM, UTUMNO_FIRE_STYLE);
    if (!utumno_build_setpiece(UTUMNO_FORGE_VAULT, 33, 33)) return false;
    /* A sheltered northern landing, with the guardian deeper in the workshop. */
    player_place(dun->corner[0].y1 + 2, 33);
    cave_water_flow_build();
    return true;
}

bool utumno_build_entrance(int cy, int cx)
{
    g_vault_name[0] = '\0';
    vault_type *vault = &v_info[UTUMNO_DOORS_VAULT];
    int y1 = cy - vault->hgt / 2, x1 = cx - vault->wid / 2;
    if (!solid_rock(y1 - 1, x1 - 1, y1 + vault->hgt, x1 + vault->wid))
    {
        log_debug("Utumno: central entrance overlaps an earlier guaranteed room");
        return false;
    }
    return utumno_build_setpiece(UTUMNO_DOORS_VAULT, cy, cx);
}

bool utumno_gen(void)
{
    /* Only the forge is handcrafted. The corridors use cave_gen normally. */
    terrain_history_reset();
    memset(dun, 0, sizeof(*dun));
    layout_anchor_reset();
    reset_morgoth_layout_state(false);
    reset_partition_population_metadata();
    current_labyrinth_partitions = 0;
    qv_placed_this_level = false;
    quest_lottery_winner = 0;
    p_ptr->force_forge = false;
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        current_partition_bridge_styles[i] = -1;
    }
    styles_init_for_level();
    return utumno_forge_gen();
}

/* The mandatory ladder must be reachable on foot. The general room flood can
 * cross vault interiors abstractly and permits jumps over elemental gaps;
 * neither proves that a character can actually enter these guarded doors. */
static bool utumno_ladder_reachable(int ladder_y, int ladder_x)
{
    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int head = 0, tail = 0;
    memset(reached, 0, sizeof(reached));
    reached[p_ptr->py][p_ptr->px] = 1;
    queue[tail++] = (coord){p_ptr->py, p_ptr->px};
    while (head < tail)
    {
        coord cell = queue[head++];
        if (cell.y == ladder_y && cell.x == ladder_x) return true;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                int y = cell.y + dy, x = cell.x + dx;
                if (!terrain_generation_walkable(y, x, NULL) || reached[y][x])
                    continue;
                reached[y][x] = 1;
                queue[tail++] = (coord){y, x};
            }
    }
    return false;
}

bool utumno_finalize_corridors(void)
{
    int styles[PARTITION_META_MAX];
    int ladder_y = -1, ladder_x = -1;
    for (int i = 0; i < current_partition_count; ++i)
    {
        big_cave_type_t type = current_partition_big_cave_types[i];
        styles[i] = type == BIG_CAVE_FIRE ? UTUMNO_FIRE_STYLE
            : type == BIG_CAVE_ICE ? UTUMNO_ICE_STYLE
            : styles_pick_random_from_level();
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            int feature = cave_feat[y][x];
            if (feature >= FEAT_STAIR_HEAD && feature <= FEAT_STAIR_TAIL)
            {
                if (terrain_vault_id_at(y, x) == UTUMNO_DOORS_VAULT
                    && terrain_vault_symbol_at(y, x) == '>')
                { ladder_y = y; ladder_x = x; }
                else cave_set_feat(y, x, FEAT_FLOOR);
            }
            if (feature == FEAT_POISON || feature == FEAT_CHASM)
            {
                log_debug("Utumno: forbidden terrain %d at (%d,%d), vault %d",
                    feature, y, x, terrain_vault_id_at(y, x));
                return false;
            }
            int style = styles_decode_color_style(cave_color[y][x]);
            if (style != UTUMNO_ICE_STYLE && style != UTUMNO_FIRE_STYLE)
            {
                int pi = level_partition_index_for_point(y, x);
                if (pi >= 0) cave_set_feat_style(y, x, cave_feat[y][x], styles[pi]);
            }
        }
    if (ladder_y < 0)
    {
        log_debug("Utumno: central entrance ladder missing");
        return false;
    }
    if (!utumno_ladder_reachable(ladder_y, ladder_x))
    {
        log_debug("Utumno: arrival (%d,%d) cannot reach ladder (%d,%d)",
            p_ptr->py, p_ptr->px, ladder_y, ladder_x);
        return false;
    }

    /* Add the authored unique to the ordinary depth-22 population. */
    int helcamo = monster_lookup_guid_text("7222d6d7c82f6571");
    if (helcamo > 0 && r_info[helcamo].max_num > 0)
    {
        int sy = -1, sx = -1;
        for (int pass = 0; pass < 2 && sy < 0; ++pass)
        {
            int count = 0;
            for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
                for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                {
                    if (!cave_empty_bold(y, x) || cave_feat[y][x] != FEAT_FLOOR
                        || (cave_info[y][x] & CAVE_ICKY)
                        || distance(y, x, p_ptr->py, p_ptr->px) < 12
                        || distance(y, x, ladder_y, ladder_x) < 20) continue;
                    if (!pass && level_partition_big_cave_type_for_point(y, x) != BIG_CAVE_ICE)
                        continue;
                    if (one_in_(++count)) { sy = y; sx = x; }
                }
        }
        if (sy < 0)
        {
            log_debug("Utumno: no empty outer floor for Helcamo");
            return false;
        }
        bool old_exact = current_build_vault_exact_token;
        current_build_vault_exact_token = true;
        bool placed = place_monster_one(sy, sx, helcamo, true, true, NULL);
        current_build_vault_exact_token = old_exact;
        if (!placed) return false;
    }
    return true;
}

bool utumno_place_morgoth_route(void)
{
    int candidates[PARTITION_META_MAX], count = 0, shafts = 0;
    for (int pi = 0; utumno_corridors && pi < current_partition_count; ++pi)
        if (pi != morgoth_partition_index) candidates[count++] = pi;
    for (int i = count - 1; i > 0; --i)
    {
        int j = rand_int(i + 1), swap = candidates[i];
        candidates[i] = candidates[j]; candidates[j] = swap;
    }
    for (int i = 0; i < count && shafts < 6; ++i)
    {
        int y1, y2, x1, x2, sy = -1, sx = -1, eligible = 0;
        compute_partition_bounds(candidates[i], current_partition_rows,
            current_partition_cols, &y1, &y2, &x1, &x2);
        for (int y = y1; y <= y2; ++y)
            for (int x = x1; x <= x2; ++x)
                if (cave_feat[y][x] == FEAT_FLOOR && !cave_m_idx[y][x]
                    && !cave_o_idx[y][x] && !(cave_info[y][x] & CAVE_ICKY)
                    && level_partition_index_for_point(y, x) == candidates[i]
                    && !coord_in_morgoth_region(y, x, 0)
                    && !(y == p_ptr->py && x == p_ptr->px)
                    && one_in_(++eligible)) { sy = y; sx = x; }
        if (sy < 0) continue;
        cave_set_feat(sy, sx, FEAT_MORE_SHAFT);
        ++shafts;
    }
    if (utumno_corridors && shafts != 6)
    {
        log_warn("Utumno: only %d of six outer partition shafts fit", shafts);
        return false;
    }
    if (p_ptr->utumno_forge_visited)
    {
        /* The authored throne symbol identifies the King's position even if
         * his unique has already died. South is the rear of the throne hall. */
        for (int y = 1; y < p_ptr->cur_map_hgt - 3; ++y)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                if (terrain_vault_symbol_at(y, x) == 'V')
                {
                    int py = y + 1;
                    if (cave_m_idx[py][x] || cave_o_idx[py][x]) return false;
                    cave_set_feat(py, x, FEAT_MORE);
                    cave_info[py][x] |= CAVE_ROOM | CAVE_ICKY | CAVE_G_VAULT;
                    if (p_ptr->utumno_return_to_throne)
                    {
                        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
                        player_place(py, x);
                    }
                    return true;
                }
        return false;
    }
    return true;
}
