#include "angband.h"
#include "achievements.h"
#include "achievements-character.h"
#include "externs.h"

static int grid_index(int y, int x)
{
    return y >= 0 && x >= 0 && y < MAX_DUNGEON_HGT && x < MAX_DUNGEON_WID
        ? y * MAX_DUNGEON_WID + x : -1;
}

void sil_achievements_discovery_reset(bool new_run)
{
    p_ptr->achievement_discovery_flags = new_run ? SIL_ACH_DISC_TRACKED
        : p_ptr->achievement_discovery_flags
            & SIL_ACH_DISC_RUN_FLAGS;
    p_ptr->achievement_water_from_y = p_ptr->achievement_water_from_x = -1;
    p_ptr->achievement_water_entry_y = p_ptr->achievement_water_entry_x = -1;
    p_ptr->achievement_poison_from_y = p_ptr->achievement_poison_from_x = -1;
    p_ptr->achievement_poison_entry_y = p_ptr->achievement_poison_entry_x = -1;
    p_ptr->achievement_labyrinth_index = -1;
    p_ptr->achievement_labyrinth_y = p_ptr->achievement_labyrinth_x = -1;
    memset(p_ptr->achievement_frozen_tiles, 0,
        sizeof(p_ptr->achievement_frozen_tiles));
}

void sil_achievements_forget_ice(int y, int x)
{
    int index = grid_index(y, x);
    if (p_ptr && index >= 0)
        p_ptr->achievement_frozen_tiles[index / 8] &= ~(1u << (index % 8));
}

void sil_achievements_on_elemental_terrain(int y, int x, int old_feat, int typ)
{
    int index = grid_index(y, x);
    if (!sil_achievements_eligible() || index < 0) return;
    if (typ == GF_COLD && cave_feat[y][x] == FEAT_ICE
        && (old_feat == FEAT_WATER || old_feat == FEAT_MELTING_ICE))
        p_ptr->achievement_frozen_tiles[index / 8] |= 1u << (index % 8);
    if (typ == GF_FIRE && FEAT_IS_ICE(old_feat)
        && !FEAT_IS_ICE(cave_feat[y][x]) && cave_m_idx[y][x] > 0
        && mon_list[cave_m_idx[y][x]].r_idx > 0
        && !(r_info[mon_list[cave_m_idx[y][x]].r_idx].flags1 & RF1_PEACEFUL))
        sil_achievements_on_event(SIL_ACH_FIRE_AND_WATER);
}

static bool water_or_ice(int feat)
{
    return feat == FEAT_WATER || feat == FEAT_DEEP_WATER || FEAT_IS_ICE(feat);
}

static bool dry_bank(int feat)
{
    return !water_or_ice(feat) && feat != FEAT_POISON && feat != FEAT_LAVA
        && feat != FEAT_CHASM;
}

/* Exiting back through the entry side is a retreat, not a crossing. */
static bool crossed(int bank_y, int bank_x, int entry_y, int entry_x,
    int last_y, int last_x, int exit_y, int exit_x)
{
    int dy = entry_y - bank_y, dx = entry_x - bank_x;
    return grid_index(bank_y, bank_x) >= 0
        && (dy * (exit_y - last_y) + dx * (exit_x - last_x) > 0)
        && (ABS(exit_y - bank_y) > 1 || ABS(exit_x - bank_x) > 1);
}

/* A crossing can contain ice, but only actual water counts towards the water
 * achievement. Record distinct cells, so pacing over two tiles cannot earn it.
 * Water and poison crossings cannot be active together; the inactive crossing's
 * existing saved coordinates hold the first two cells without a format change. */
static void record_liquid_tile(bool water, int y, int x)
{
    u32b* flags = &p_ptr->achievement_discovery_flags;
    s16b *first_y, *first_x, *second_y, *second_x;
    if (water)
    {
        first_y = &p_ptr->achievement_poison_from_y;
        first_x = &p_ptr->achievement_poison_from_x;
        second_y = &p_ptr->achievement_poison_entry_y;
        second_x = &p_ptr->achievement_poison_entry_x;
    }
    else
    {
        first_y = &p_ptr->achievement_water_from_y;
        first_x = &p_ptr->achievement_water_from_x;
        second_y = &p_ptr->achievement_water_entry_y;
        second_x = &p_ptr->achievement_water_entry_x;
    }
    if (!(*flags & SIL_ACH_DISC_LIQUID_FIRST))
    {
        *first_y = y; *first_x = x;
        *flags |= SIL_ACH_DISC_LIQUID_FIRST;
    }
    else if (y != *first_y || x != *first_x)
    {
        if (!(*flags & SIL_ACH_DISC_LIQUID_SECOND))
        {
            *second_y = y; *second_x = x;
            *flags |= SIL_ACH_DISC_LIQUID_SECOND;
        }
        else if (y != *second_y || x != *second_x)
            *flags |= SIL_ACH_DISC_LIQUID_THREE;
    }
}

/* Check that the two banks are separated by this liquid, not merely two
 * points along the same shore. The straight line between them must stay over
 * the liquid (or ice for water); the travelled route itself may wind. */
static bool opposite_liquid_banks(int bank_y, int bank_x, int entry_y, int entry_x,
    int exit_y, int exit_x, bool poison)
{
    if (grid_index(bank_y, bank_x) < 0 || !in_bounds(bank_y, bank_x)
        || !dry_bank(cave_feat[bank_y][bank_x])
        || FEAT_IS_BRIDGE(cave_feat[bank_y][bank_x])
        || FEAT_IS_BRIDGE(cave_feat[exit_y][exit_x])) return false;
    int dy = entry_y - bank_y, dx = entry_x - bank_x;
    if (dy * (exit_y - bank_y) + dx * (exit_x - bank_x) <= 0) return false;

    int x = bank_x, y = bank_y;
    int xdiff = ABS(exit_x - bank_x), ydiff = ABS(exit_y - bank_y);
    int sx = bank_x < exit_x ? 1 : -1, sy = bank_y < exit_y ? 1 : -1;
    int error = xdiff - ydiff;
    bool liquid = false;
    while (x != exit_x || y != exit_y)
    {
        int twice = 2 * error;
        if (twice > -ydiff) { error -= ydiff; x += sx; }
        if (twice < xdiff) { error += xdiff; y += sy; }
        if (x == exit_x && y == exit_y) break;
        if (!in_bounds(y, x)) return false;
        int feat = cave_feat[y][x];
        if (poison ? feat != FEAT_POISON : !water_or_ice(feat)) return false;
        liquid = true;
    }
    return liquid;
}

void sil_achievements_on_partition_entry(int y, int x)
{
    if (!sil_achievements_eligible() || !in_bounds(y, x)) return;
    level_partition_kind kind = level_partition_kind_for_point(y, x);
    unsigned bit = kind == LEVEL_PART_LABYRINTH ? 1u
        : kind == LEVEL_PART_CHASM ? 2u : 0;
    if (kind == LEVEL_PART_BIG_CAVE)
    {
        int type = level_partition_big_cave_type_for_point(y, x);
        if (type == BIG_CAVE_ICE) bit = 4;
        else if (type == BIG_CAVE_FIRE) bit = 8;
        else if (type == BIG_CAVE_POIS) bit = 16;
    }
    if (bit) sil_achievements_on_partition_discovered(bit);
}

void sil_achievements_on_terrain_seen(int y, int x)
{
    if (!p_ptr || p_ptr->depth != UTUMNO_FORGE_DEPTH
        || sil_achievement_unlocked_at(SIL_ACH_WHERE_GROND_WAS_WROUGHT)
        || !in_bounds(y, x) || !(cave_info[y][x] & CAVE_SEEN)
        || !sil_achievements_eligible()) return;
    int feature = cave_feat[y][x];
    if (feature >= FEAT_FORGE_UNIQUE_HEAD && feature <= FEAT_FORGE_UNIQUE_TAIL)
        sil_achievements_on_event(SIL_ACH_WHERE_GROND_WAS_WROUGHT);
}

void sil_achievements_on_movement(int fy, int fx, int ty, int tx)
{
    if (!sil_achievements_eligible() || !in_bounds(fy, fx) || !in_bounds(ty, tx))
        return;
    sil_achievements_on_partition_entry(ty, tx);
    u32b *flags = &p_ptr->achievement_discovery_flags;
    bool step = ABS(ty - fy) <= 1 && ABS(tx - fx) <= 1 && (fy != ty || fx != tx);
    if (!step || p_ptr->leaping)
    {
        *flags &= SIL_ACH_DISC_RUN_FLAGS;
        return;
    }
    int from = cave_feat[fy][fx], to = cave_feat[ty][tx];
    if (water_or_ice(to))
    {
        if (!water_or_ice(from) || !(*flags & SIL_ACH_DISC_CROSSING))
        {
            if (dry_bank(from))
            {
                *flags &= ~(SIL_ACH_DISC_OWN_ICE | SIL_ACH_DISC_WATER
                    | SIL_ACH_DISC_LIQUID_FLAGS);
                *flags |= SIL_ACH_DISC_CROSSING;
                p_ptr->achievement_water_from_y = fy;
                p_ptr->achievement_water_from_x = fx;
                p_ptr->achievement_water_entry_y = ty;
                p_ptr->achievement_water_entry_x = tx;
            }
        }
        if (*flags & SIL_ACH_DISC_CROSSING)
        {
            if (to == FEAT_WATER || to == FEAT_DEEP_WATER)
            {
                *flags |= SIL_ACH_DISC_WATER;
                record_liquid_tile(true, ty, tx);
            }
            int index = grid_index(ty, tx);
            if (p_ptr->achievement_frozen_tiles[index / 8] & (1u << (index % 8)))
                *flags |= SIL_ACH_DISC_OWN_ICE;
        }
    }
    else if (*flags & SIL_ACH_DISC_CROSSING)
    {
        if (dry_bank(to))
        {
            if ((*flags & SIL_ACH_DISC_LIQUID_THREE)
                && opposite_liquid_banks(p_ptr->achievement_water_from_y,
                    p_ptr->achievement_water_from_x, p_ptr->achievement_water_entry_y,
                    p_ptr->achievement_water_entry_x, ty, tx, false))
                sil_achievements_on_event(SIL_ACH_THE_FORDS_OF_SIRION);
            if ((*flags & SIL_ACH_DISC_OWN_ICE)
                && crossed(p_ptr->achievement_water_from_y, p_ptr->achievement_water_from_x,
                    p_ptr->achievement_water_entry_y, p_ptr->achievement_water_entry_x,
                    fy, fx, ty, tx))
                sil_achievements_on_event(SIL_ACH_HELCARAXE);
        }
        *flags &= ~(SIL_ACH_DISC_CROSSING | SIL_ACH_DISC_OWN_ICE
            | SIL_ACH_DISC_WATER | SIL_ACH_DISC_LIQUID_FLAGS);
    }
    if (to == FEAT_POISON && from != FEAT_POISON && dry_bank(from))
    {
        *flags &= ~SIL_ACH_DISC_LIQUID_FLAGS;
        *flags |= SIL_ACH_DISC_POISON;
        p_ptr->achievement_poison_from_y = fy;
        p_ptr->achievement_poison_from_x = fx;
        p_ptr->achievement_poison_entry_y = ty;
        p_ptr->achievement_poison_entry_x = tx;
    }
    if (to == FEAT_POISON && (*flags & SIL_ACH_DISC_POISON))
        record_liquid_tile(false, ty, tx);
    else if (to != FEAT_POISON && (*flags & SIL_ACH_DISC_POISON))
    {
        if ((*flags & SIL_ACH_DISC_POISON) && from == FEAT_POISON && dry_bank(to)
            && (*flags & SIL_ACH_DISC_LIQUID_THREE)
            && opposite_liquid_banks(p_ptr->achievement_poison_from_y, p_ptr->achievement_poison_from_x,
                p_ptr->achievement_poison_entry_y, p_ptr->achievement_poison_entry_x,
                ty, tx, true))
            sil_achievements_on_event(SIL_ACH_THROUGH_NAN_DUNGORTHEB);
        *flags &= ~(SIL_ACH_DISC_POISON | SIL_ACH_DISC_LIQUID_FLAGS);
    }
    int from_partition = level_partition_index_for_point(fy, fx);
    int to_partition = level_partition_index_for_point(ty, tx);
    if ((*flags & SIL_ACH_DISC_LABYRINTH)
        && to_partition != p_ptr->achievement_labyrinth_index)
    {
        if (from_partition == p_ptr->achievement_labyrinth_index
            && (ABS(fy - p_ptr->achievement_labyrinth_y) > 1
                || ABS(fx - p_ptr->achievement_labyrinth_x) > 1))
            sil_achievements_on_event(SIL_ACH_A_JOURNEY_IN_THE_DARK);
        *flags &= ~SIL_ACH_DISC_LABYRINTH;
    }
    if (to_partition >= 0 && to_partition != from_partition
        && level_partition_kind_for_point(ty, tx) == LEVEL_PART_LABYRINTH)
    {
        *flags |= SIL_ACH_DISC_LABYRINTH;
        p_ptr->achievement_labyrinth_index = to_partition;
        p_ptr->achievement_labyrinth_y = ty;
        p_ptr->achievement_labyrinth_x = tx;
    }
}

void sil_achievements_on_monster_seen(int m_idx)
{
    if (!sil_achievements_eligible() || m_idx <= 0 || m_idx >= mon_max
        || !(p_ptr->achievement_discovery_flags & SIL_ACH_DISC_TRACKED)
        || (p_ptr->achievement_discovery_flags & SIL_ACH_DISC_WOLF_SEEN)) return;
    /* Stable monster.txt entry 11 is the ordinary Wolf. */
    if (mon_list[m_idx].r_idx != 11) return;
    mon_list[m_idx].achievement_first_wolf = true;
    p_ptr->achievement_discovery_flags |= SIL_ACH_DISC_WOLF_SEEN;
}

void sil_achievements_on_wolf_death(bool first_wolf)
{
    if (first_wolf && !p_ptr->achievement_kills
        && (p_ptr->achievement_discovery_flags & SIL_ACH_DISC_TRACKED))
        sil_achievements_on_event(SIL_ACH_NOT_EVEN_CARCHAROTH);
}

void sil_achievements_on_skill_check(bool attacker, int skill, int difficulty,
    int skill_sides, int difficulty_sides, bool cursed, int result)
{
    if (!sil_achievements_eligible() || (attacker ? result <= 0 : result > 0)
        || sil_achievement_unlocked_at(SIL_ACH_A_FOOLS_HOPE)
        || skill_sides < 1 || difficulty_sides < 1
        || skill_sides > 100 || difficulty_sides > 100) return;
    uint64_t favourable = 0, total = 0;
    for (int a = 1; a <= skill_sides; ++a)
        for (int d = 1; d <= difficulty_sides; ++d)
        {
            uint64_t weight = cursed ? (attacker ? 2 * (skill_sides - a) + 1
                : 2 * (difficulty_sides - d) + 1) : 1;
            total += weight;
            bool success = attacker ? a + skill > d + difficulty
                : a + skill <= d + difficulty;
            if (success) favourable += weight;
        }
    if (favourable && favourable * 10 <= total)
        sil_achievements_on_event(SIL_ACH_A_FOOLS_HOPE);
}

void sil_achievements_song_begin(void)
{
    sil_achievement_character_song_begin();
}

void sil_achievements_on_song_effect(int song, int m_idx, int previous_alertness)
{
    if (sil_achievement_character_song_effect(song, m_idx, previous_alertness))
        sil_achievements_on_event(SIL_ACH_TWO_THEMES_ONE_VOICE);
}

void sil_achievements_song_end(int cost)
{
    if (sil_achievement_character_song_end(cost))
        sil_achievements_on_event(SIL_ACH_TWO_THEMES_ONE_VOICE);
}
