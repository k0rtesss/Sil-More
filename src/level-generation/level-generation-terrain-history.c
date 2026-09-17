#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain.h"
#include "cave/cave-bridge.h"

/* Historical layers are a generation plan, not live fluid simulation. */
static int history_count, history_phase;
static int history_epochs[TERRAIN_HISTORY_SYSTEMS];
static coord history_banks[256];
static int history_bank_count;
static bool history_started;
static int history_ancient_feature(int y, int x);

void terrain_history_reset(void)
{
    history_count = history_phase = 0;
    history_bank_count = 0;
    history_started = false;
    memset(history_epochs, 0, sizeof(history_epochs));
    terrain_landmark_plans_reset();
}
bool terrain_history_active(void) { return history_count > 0; }
bool terrain_history_started(void) { return history_started; }
int terrain_history_count(void) { return history_count; }
int terrain_history_epoch(int system)
{ return system >= 0 && system < history_count ? history_epochs[system] : -1; }

void terrain_history_begin(void)
{
    bool utumno = (p_ptr->depth == UTUMNO_DEPTH);
    if (morgoth_level_active || p_ptr->depth < 1
        || (p_ptr->depth > MORGOTH_DEPTH && !utumno)) return;
    history_started = true;
    const terrain_theme_profile* theme = terrain_theme_for_depth(p_ptr->depth);
    const terrain_history_profile* profile = terrain_history_for_depth(p_ptr->depth);
    if (!percent_chance(terrain_landmark_for_depth(p_ptr->depth)->chance)) return;
    int target = utumno ? 3 : 1 + percent_chance(profile->second_system_chance);
    int previous = -1;
    unsigned used = 0;
    for (int s = 0; s < target; s++)
    {
        int total = 0;
        for (int m = 0; m < 5; m++)
            if (m != previous && (!utumno || !(used & (1u << m)))) total += theme->weights[m];
        /* Two separate histories need distinct materials; a one-material
         * profile already supports multiple pools within its one network. */
        if (!total) break;
        int roll = rand_int(total), material = 0;
        for (; material < 5; material++)
        {
            if (material == previous || (utumno && (used & (1u << material)))) continue;
            if (roll < theme->weights[material]) break;
            roll -= theme->weights[material];
        }
        int epoch_total = profile->ancient + profile->disaster + profile->overflow;
        if (!epoch_total || material == 5) break;
        roll = rand_int(epoch_total);
        int epoch = roll < profile->ancient ? TERRAIN_HISTORY_ANCIENT
            : roll < profile->ancient + profile->disaster ? TERRAIN_HISTORY_DISASTER : TERRAIN_HISTORY_OVERFLOW;
        if (!terrain_landmark_plan_system(history_count, material)) continue;
        history_epochs[history_count++] = epoch; previous = material;
        used |= 1u << material;
    }
    /* Both systems have their routes before the first constructed room. */
    for (int s = 0; s < history_count; s++)
        if (history_epochs[s] != TERRAIN_HISTORY_DISASTER) terrain_landmark_foundation(s);
    int seen = 0;
    for (int y = 6; y < p_ptr->cur_map_hgt - 6; y++) for (int x = 6; x < p_ptr->cur_map_wid - 6; x++)
    {
        if (!history_ancient_feature(y, x)) continue;
        for (int d = 0; d < 4; d++)
        {
            int dy = d == 0 ? -1 : d == 2 ? 1 : 0;
            int dx = d == 1 ? 1 : d == 3 ? -1 : 0;
            int yy = y + dy * 8, xx = x + dx * 8;
            if (!in_bounds_fully(yy, xx) || cave_feat[yy][xx] != FEAT_WALL_EXTRA) continue;
            int slot = seen < 256 ? seen : rand_int(seen + 1);
            seen++;
            if (slot < 256) history_banks[slot] = (coord){yy, xx};
        }
    }
    history_bank_count = MIN(seen, 256);
    history_phase = history_count ? 1 : 0;
    log_debug("Terrain history planned: %d systems, epochs=%d/%d before construction",
        history_count, history_count ? history_epochs[0] : -1, history_count > 1 ? history_epochs[1] : -1);
}

void terrain_history_start_tunnels(void) { if (history_count) history_phase = 2; }

void terrain_history_pick_bank_site(int* y, int* x, int y1, int x1, int y2, int x2)
{
    if (history_phase != 1 || !history_bank_count || !one_in_(3)) return;
    for (int i = 0; i < 24; i++)
    {
        coord site = history_banks[rand_int(history_bank_count)];
        if (site.y < y1 || site.y > y2 || site.x < x1 || site.x > x2
            || cave_feat[site.y][site.x] != FEAT_WALL_EXTRA
            || (cave_info[site.y][site.x] & (CAVE_ROOM | CAVE_ICKY))) continue;
        *y = site.y; *x = site.x; return;
    }
}

static int history_ancient_feature(int y, int x)
{
    for (int s = 0; s < history_count; s++)
        if (history_epochs[s] != TERRAIN_HISTORY_DISASTER)
        {
            int feat = terrain_landmark_planned_feature(s, y, x);
            if (feat) return feat;
        }
    return 0;
}

bool terrain_history_reserved(int y, int x)
{
    if (!history_phase || !in_bounds_fully(y, x)) return false;
    if (history_ancient_feature(y, x)) return true;
    for (int s = 0; s < history_count; s++)
        if (history_epochs[s] != TERRAIN_HISTORY_DISASTER && terrain_landmark_planned_cap(s, y, x)) return true;
    return false;
}
bool terrain_history_vault_fits(int y1, int x1, int y2, int x2)
{
    if (!history_phase) return true;
    for (int y = MAX(1, y1); y <= MIN(p_ptr->cur_map_hgt - 2, y2); y++)
        for (int x = MAX(1, x1); x <= MIN(p_ptr->cur_map_wid - 2, x2); x++)
            if (terrain_history_reserved(y, x)) return false;
    return true;
}
int terrain_history_material_in_bounds(int y1, int x1, int y2, int x2)
{
    int material = 0;
    for (int y = MAX(1, y1); y <= MIN(p_ptr->cur_map_hgt - 2, y2); y++)
        for (int x = MAX(1, x1); x <= MIN(p_ptr->cur_map_wid - 2, x2); x++)
        {
            int f = history_ancient_feature(y, x);
            if (!f || f == FEAT_CHASM) continue;
            if (material && material != f) return -1;
            material = f;
        }
    return material;
}

int terrain_history_construction_feature(int y, int x, int feature)
{
    if (!history_phase || !in_bounds_fully(y, x)) return feature;
    int original = history_ancient_feature(y, x);
    if (original)
    {
        if (cave_feat_is_bridge(cave_feat[y][x])) return cave_feat[y][x];
        if (history_phase == 2 && feature == FEAT_FLOOR)
        {
            int vertical = 1, horizontal = 1;
            for (int d = -1; d <= 1; d += 2) for (int i = 1; i < 24; i++)
            { if (history_ancient_feature(y + i * d, x) != original) break; vertical++; }
            for (int d = -1; d <= 1; d += 2) for (int i = 1; i < 24; i++)
            { if (history_ancient_feature(y, x + i * d) != original) break; horizontal++; }
            return cave_bridge_feature(original, vertical < horizontal);
        }
        return original;
    }
    for (int s = 0; s < history_count; s++)
        if (history_epochs[s] != TERRAIN_HISTORY_DISASTER)
        {
            int cap = terrain_landmark_planned_cap(s, y, x);
            if (cap) return cap;
        }
    return feature;
}

bool terrain_history_finish(void)
{
    history_phase = 0;
    if (history_count) terrain_generation_reset();
    /* Commit old systems first, then later disasters. An overflow reuses the
     * same basin/channel graph instead of rolling an unrelated second river. */
    for (int late = 0; late < 2; late++) for (int s = 0; s < history_count; s++)
    {
        if ((history_epochs[s] == TERRAIN_HISTORY_DISASTER) != (late != 0)) continue;
        if (!terrain_landmark_realize(s, history_epochs[s]))
        { log_debug("Terrain history: system %d epoch %d could not realize its plan", s, history_epochs[s]); return false; }
        if (history_epochs[s] != TERRAIN_HISTORY_DISASTER)
            for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (terrain_landmark_planned_cap(s, y, x)) terrain_generation_reserve(y, x);
    }
    return true;
}
