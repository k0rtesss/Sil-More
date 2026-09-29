#include "angband.h"
#include "externs.h"
#include "cave/cave-atmosphere.h"
#include "cave/cave-bridge.h"
#include "cave/cave-water-flow.h"
#include "tutorial/tutorial-game.h"

/* An atmosphere belongs to the existing partition, but only its matching
 * natural caves / built rooms / ruined floors receive the effect. Corridors
 * and protected vaults retain their normal rules. */
static byte style_ground[COLOR_STYLE_SLOT_MAX];
static byte partition_atmospheres[PARTITION_META_MAX];
static u32b narrated_partitions;
static cave_atmosphere_kind last_atmosphere;

void cave_atmosphere_ground_rules_clear(void)
{
    memset(style_ground, 0, sizeof(style_ground));
}

bool cave_atmosphere_set_ground_rule(int style, int ground)
{
    if (style < 0 || style >= COLOR_STYLE_SLOT_MAX
        || !z_info || style >= z_info->style_max
        || ground < 0 || ground >= ATMOSPHERE_GROUND_MAX)
        return false;
    style_ground[style] = (byte)ground;
    return true;
}

void cave_atmosphere_reset(void)
{
    memset(partition_atmospheres, 0, sizeof(partition_atmospheres));
    narrated_partitions = 0;
    last_atmosphere = CAVE_ATMOSPHERE_NONE;
}

void cave_atmosphere_get_partitions(byte* out)
{
    memcpy(out, partition_atmospheres, sizeof(partition_atmospheres));
}

void cave_atmosphere_set_partitions(const byte* in, int count)
{
    cave_atmosphere_reset();
    if (!in) return;
    for (int i = 0; i < MIN(count, PARTITION_META_MAX); i++)
        if (in[i] < CAVE_ATMOSPHERE_MAX)
            partition_atmospheres[i] = in[i];
}

static cave_atmosphere_kind atmosphere_candidate(int y, int x)
{
    if (!p_ptr || !in_bounds_fully(y, x) || !cave_floor_bold(y, x)
        || (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)))
        return CAVE_ATMOSPHERE_NONE;

    switch (level_partition_kind_for_point(y, x))
    {
    case LEVEL_PART_CAVEY:
        return cave_natural[y][x] ? CAVE_ATMOSPHERE_HUSHED : CAVE_ATMOSPHERE_NONE;
    case LEVEL_PART_ROOMY:
        return (cave_info[y][x] & CAVE_ROOM) && !cave_natural[y][x]
            ? CAVE_ATMOSPHERE_ECHOING : CAVE_ATMOSPHERE_NONE;
    case LEVEL_PART_RUINED:
        return (cave_info[y][x] & CAVE_ROOM)
            ? CAVE_ATMOSPHERE_DRAUGHTY : CAVE_ATMOSPHERE_NONE;
    default:
        return CAVE_ATMOSPHERE_NONE;
    }
}

static int atmosphere_ground_at(int y, int x)
{
    /* The actual surface overrides a snow/moss style beneath water or ice.
     * Bridges also have their own hard deck, regardless of the bank's style. */
    int feat = cave_feat[y][x];
    if (feat != FEAT_FLOOR && feat != FEAT_RAGE_FLOOR)
        return ATMOSPHERE_GROUND_STONE;
    int style = z_info && style_info ? styles_decode_color_style(cave_color[y][x]) : -1;
    return style >= 0 && style < COLOR_STYLE_SLOT_MAX
        ? style_ground[style] : ATMOSPHERE_GROUND_STONE;
}

static int atmosphere_tile_chance(int y, int x, cave_atmosphere_kind kind)
{
    int ground = atmosphere_ground_at(y, x);
    bool ice = false, water = false, rushing = false;
    bool lava = false, chasm = false, broken_door = false;

    /* Sample actual generation terrain, never remembered/visible artwork.
     * Solid walls block influence; terrain in protected cells is ignored. */
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if (!in_bounds_fully(yy, xx)
                || (cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                || !los(y, x, yy, xx))
                continue;
            int feat = cave_bridge_underlay(cave_feat[yy][xx]);
            ice |= FEAT_IS_ICE(feat);
            lava |= feat == FEAT_LAVA;
            chasm |= feat == FEAT_CHASM;
            broken_door |= feat == FEAT_BROKEN;
            if (feat == FEAT_WATER || feat == FEAT_DEEP_WATER || feat == FEAT_POISON)
            {
                water = true;
                rushing |= cave_water_flow_direction(yy, xx) != CAVE_WATER_FLOW_CALM;
            }
        }

    int chance;
    if (kind == CAVE_ATMOSPHERE_HUSHED)
    {
        chance = ground == ATMOSPHERE_GROUND_SNOW ? 60
            : ground == ATMOSPHERE_GROUND_SOFT ? 50 : 15;
        if (ice && ground == ATMOSPHERE_GROUND_STONE) chance -= 10;
        if (rushing) chance -= 20;
        if (lava) chance -= 20;
    }
    else if (kind == CAVE_ATMOSPHERE_ECHOING)
    {
        chance = ground == ATMOSPHERE_GROUND_STONE ? 35
            : ground == ATMOSPHERE_GROUND_SOFT ? 10 : 5;
        if (ice && ground == ATMOSPHERE_GROUND_STONE) chance += 15;
        if (rushing) chance -= 20;
        else if (water) chance -= 5;
        if (lava) chance -= 15;
    }
    else
    {
        chance = 15;
        if (lava) chance += 30;
        if (chasm) chance += 30;
        if (broken_door) chance += 10;
    }
    return MAX(5, MIN(65, chance));
}

static void atmosphere_generation_profile(byte* chances, byte* kinds)
{
    int scores[PARTITION_META_MAX] = {0};
    int floors[PARTITION_META_MAX] = {0};
    memset(chances, 0, PARTITION_META_MAX);
    memset(kinds, 0, PARTITION_META_MAX);
    if (!p_ptr) return;

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            cave_atmosphere_kind kind = atmosphere_candidate(y, x);
            if (kind == CAVE_ATMOSPHERE_NONE) continue;
            int pi = level_partition_index_for_point(y, x);
            if (pi < 0 || pi >= PARTITION_META_MAX) continue;
            kinds[pi] = (byte)kind;
            scores[pi] += atmosphere_tile_chance(y, x, kind);
            floors[pi]++;
        }
    for (int pi = 0; pi < PARTITION_META_MAX; pi++)
        if (floors[pi])
            chances[pi] = (byte)((scores[pi] + floors[pi] / 2) / floors[pi]);
}

void cave_atmosphere_generation_chances(byte* out)
{
    byte kinds[PARTITION_META_MAX];
    atmosphere_generation_profile(out, kinds);
}

void cave_atmosphere_generate(void)
{
    byte chances[PARTITION_META_MAX], kinds[PARTITION_META_MAX];
    cave_atmosphere_reset();
    /* Handcrafted final/surface/tutorial maps do not acquire random rules. */
    if (!p_ptr || p_ptr->depth <= 0
        || (p_ptr->depth >= MORGOTH_DEPTH && p_ptr->depth != UTUMNO_DEPTH)
        || tutorial_game_start_needs_clear_area())
        return;
    atmosphere_generation_profile(chances, kinds);
    for (int pi = 0; pi < PARTITION_META_MAX; pi++)
        if (chances[pi] && percent_chance(chances[pi]))
            partition_atmospheres[pi] = kinds[pi];
}

cave_atmosphere_kind cave_atmosphere_at(int y, int x)
{
    cave_atmosphere_kind kind = atmosphere_candidate(y, x);
    if (kind == CAVE_ATMOSPHERE_NONE) return kind;
    int pi = level_partition_index_for_point(y, x);
    return pi >= 0 && pi < PARTITION_META_MAX && partition_atmospheres[pi] == kind
        ? kind : CAVE_ATMOSPHERE_NONE;
}

int cave_atmosphere_skill_modifier(int y, int x, int skill)
{
    int song = 0;
    switch (cave_atmosphere_at(y, x))
    {
    case CAVE_ATMOSPHERE_HUSHED: song = -3; break;
    case CAVE_ATMOSPHERE_ECHOING: song = 3; break;
    default: break;
    }
    return skill == S_SNG ? song : (skill == S_STL ? -song : 0);
}

bool cave_atmosphere_changed(int old_y, int old_x, int new_y, int new_x)
{
    cave_atmosphere_kind before = cave_atmosphere_at(old_y, old_x);
    cave_atmosphere_kind after = cave_atmosphere_at(new_y, new_x);
    return before != after || (after == CAVE_ATMOSPHERE_DRAUGHTY
        && level_partition_index_for_point(old_y, old_x)
            != level_partition_index_for_point(new_y, new_x));
}

int cave_atmosphere_flame_penalty(int y, int x, bool lantern)
{
    if (cave_atmosphere_at(y, x) != CAVE_ATMOSPHERE_DRAUGHTY) return 0;
    /* A gust lasts one player turn. Repainting, inspecting equipment, moving
     * within the same ruin, and saving/loading cannot reroll it or use RNG. */
    u32b tick = (u32b)MAX(0, playerturn);
    u32b partition = (u32b)level_partition_index_for_point(y, x);
    /* Two calm turns separate gust opportunities, so a radius-one torch
     * cannot suffer a randomly extended blackout while staying in the ruin. */
    if (tick % 3U != partition % 3U) return 0;
    u32b phase = tick / 3U;
    phase ^= (partition + 1U) * 0x9E3779B9U;
    phase ^= (u32b)p_ptr->depth * 0x85EBCA6BU;
    phase ^= phase >> 16;
    phase *= 0x7FEB352DU;
    phase ^= phase >> 15;
    return phase % (lantern ? 4U : 2U) == 0 ? 1 : 0;
}

cptr cave_atmosphere_name(cave_atmosphere_kind kind)
{
    switch (kind)
    {
    case CAVE_ATMOSPHERE_HUSHED: return "Hushed";
    case CAVE_ATMOSPHERE_ECHOING: return "Echoing";
    case CAVE_ATMOSPHERE_DRAUGHTY: return "Draughty";
    default: return "";
    }
}

cptr cave_atmosphere_description(cave_atmosphere_kind kind)
{
    switch (kind)
    {
    case CAVE_ATMOSPHERE_HUSHED:
        return "Your footsteps fall softly, and your voice finds little answer. Stealth +3; Song -3 while here.";
    case CAVE_ATMOSPHERE_ECHOING:
        return "The stone carries every footstep and lends strength to your voice. Song +3; Stealth -3 while here.";
    case CAVE_ATMOSPHERE_DRAUGHTY:
        return "Gusts stir these ruins. Fuelled lights occasionally lose one radius for a turn; torches can dim to radius 0, lanterns flicker less often. Fuel is not lost. Jewels and Feanorian lamps are unaffected.";
    default: return "";
    }
}

void cave_atmosphere_note_player_position(void)
{
    if (!p_ptr || p_ptr->is_dead) return;
    cave_atmosphere_kind kind = cave_atmosphere_at(p_ptr->py, p_ptr->px);
    if (kind != last_atmosphere)
    {
        last_atmosphere = kind;
        p_ptr->update |= PU_BONUS | PU_TORCH;
        p_ptr->redraw |= PR_EXTRA | PR_MAP;
    }
    if (kind == CAVE_ATMOSPHERE_NONE) return;
    int pi = level_partition_index_for_point(p_ptr->py, p_ptr->px);
    if (pi < 0 || pi >= PARTITION_META_MAX) return;
    u32b bit = 1U << pi;
    if (!(narrated_partitions & bit))
    {
        narrated_partitions |= bit;
        if (!p_ptr->restoring)
            msg_print(cave_atmosphere_description(kind));
    }
}
