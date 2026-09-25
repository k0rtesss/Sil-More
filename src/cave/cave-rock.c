#include "angband.h"
#include "externs.h"
#include "log/log.h"

/* A small success fractures intact stone; a margin of five collapses it.
 * Cracked veins keep their mineral identity until they actually break. */
int cave_rock_damage_feature(int feat, int margin)
{
    if (!FEAT_IS_ROCK(feat) || margin <= 0) return feat;
    if (feat == FEAT_DAMAGED_WALL || feat == FEAT_CRACKED_QUARTZ || margin >= 5)
        return FEAT_RUBBLE;
    return feat == FEAT_QUARTZ ? FEAT_CRACKED_QUARTZ : FEAT_DAMAGED_WALL;
}

bool cave_quartz_natural_site(int y, int x)
{
    if (!in_bounds_fully(y, x)
        || (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)))
        return false;
    level_partition_kind kind = level_partition_kind_for_point(y, x);
    return kind == LEVEL_PART_CAVEY || kind == LEVEL_PART_BIG_CAVE
        || kind == LEVEL_PART_CHASM;
}

/* The chasm is an impact crater: its deposits are the only mining source of
 * star iron. A chasm flag on a different partition must not grant this ore. */
int cave_quartz_metal_kind(int y, int x, int depth)
{
    if (level_partition_kind_for_point(y, x) == LEVEL_PART_CHASM
        && (cave_info[y][x] & CAVE_CHASM_AREA))
        return depth >= 10 ? SV_METAL_STAR_IRON : -1;
    return depth >= MITHRIL_VEIN_MIN_DEPTH ? SV_METAL_MITHRIL : -1;
}

/* Called by the terrain setter after a vein becomes rubble/open ground.
 * Digging, blasting, miners and erosion therefore release its contents once.
 * ENV_MINERAL_SPENT prevents regrowth; it does not mean loot was collected. */
void cave_quartz_release(int y, int x)
{
    object_type gem;
    drop_profile profile;
    int depth = p_ptr->depth;
    object_wipe(&gem);
    drop_profile_default(&profile);
    profile.weight_weapon = profile.weight_armor = profile.weight_jewelry = 0;
    profile.weight_supply = 100;
    profile.supply_potion = profile.supply_herb = profile.supply_staff = 0;
    profile.supply_light = profile.supply_arrows = profile.supply_tunneling = 0;
    profile.supply_gem = 100;
    if (!drop_generate_object_profiled(depth, DROP_QUALITY_NORMAL,
            DROP_TYPE_STAFF, 0, false, &profile, &gem) || gem.tval != TV_GEM)
    {
        /* Even shallow veins yield a usable gem when the depth pool is empty. */
        int kind = lookup_kind(TV_GEM, SV_GEM_LIGHT);
        if (kind <= 0) {
            log_error("Quartz reward: missing Light gem template");
            return;
        }
        object_prep(&gem, kind);
    }
    gem.number = 1;
    if (player_can_see_bold(y, x)) object_aware(&gem);
    drop_near(&gem, -1, y, x);
    if (player_can_see_bold(y, x)) msg_print("A gem glitters in the broken quartz!");

    int metal = cave_quartz_metal_kind(y, x, depth);
    /* Preserve the old deep-cave mithril rate; metals now supplement the gem.
     * Star iron retains the crater's 20-25% chance from 500 ft onward. */
    int chance = MIN(25, 10 + depth);
    if (metal >= 0 && rand_int(100) < chance
        && (metal == SV_METAL_STAR_IRON || rand_int(100) < 45))
    {
        object_type ore;
        int kind = lookup_kind(TV_METAL, metal);
        if (kind <= 0) return;
        object_prep(&ore, kind);
        drop_near(&ore, -1, y, x);
        if (player_can_see_bold(y, x))
            msg_print(metal == SV_METAL_STAR_IRON
                ? "A jagged shard of star iron lies in the rubble!"
                : "A gleaming piece of mithril lies in the rubble!");
    }
}
