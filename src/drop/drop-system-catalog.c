#include "angband.h"
#include "drop/drop-system-internal.h"
#include "mem/alloc.h"
#include "fs/path.h"
#include "fs/file.h"
#include "log/log.h"
#include <string.h>

/*
 * New drop generation system.
 *
 * Builds a catalog of all droppable items (base, ego variants, artefacts)
 * with precomputed smithing difficulties and depth metadata. The catalog is
 * cached to lib/data/drops.raw and regenerated when any relevant edit file
 * changes. All selection happens from this catalog using the difficulty-band
 * rules described in the design brief.
 */

drop_entry* g_drop_entries = NULL;
size_t g_drop_count = 0;
size_t g_drop_capacity = 0;

static int ego_s8(byte v)
{
    return (int)(int8_t)v;
}

static int smithing_step_from_ego_bonus(int bonus)
{
    if (bonus == 0)
        return 0;
    return (bonus > 0) ? 1 : -1;
}

static bool drop_kind_is_protection_amulet(const object_kind* k_ptr)
{
    return k_ptr && k_ptr->tval == TV_AMULET
        && k_ptr->sval == SV_AMULET_PROTECTION;
}

static int drop_kind_base_pd_min(const object_kind* k_ptr)
{
    return k_ptr ? k_ptr->pd : 0;
}

static int drop_kind_base_pd_max(const object_kind* k_ptr)
{
    if (drop_kind_is_protection_amulet(k_ptr))
        return 2;

    return k_ptr ? k_ptr->pd : 0;
}

static const char* DROP_RAW_FILE = "drops";
static const u32b DROP_RAW_MAGIC = 0x44525053; /* 'DRPS' */
static const u32b DROP_RAW_VERSION = 22;

typedef struct
{
    u32b magic;
    u32b version;
    u32b count;
} drop_raw_header;

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

static drop_category drop_category_for_kind(const object_kind* k_ptr)
{
    switch (k_ptr->tval)
    {
    case TV_ARROW:
        /* Base category is weapon (for ego arrows); normal arrows overridden to supply in add_drop_entry */
        return DROP_CAT_WEAPON;
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
        return DROP_CAT_WEAPON;
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
        return DROP_CAT_ARMOR;
    case TV_RING:
    case TV_AMULET:
        return DROP_CAT_JEWELRY;
    case TV_LIGHT:
        /* All non-Feanorian light sources are treated as supply (misc/torches). */
        if (k_ptr->sval == SV_LIGHT_FEANORIAN || k_ptr->sval == SV_LIGHT_SILMARIL)
            return DROP_CAT_JEWELRY;
        if (k_ptr->sval == SV_LIGHT_TORCH || k_ptr->sval == SV_LIGHT_MALLORN
            || k_ptr->sval == SV_LIGHT_LANTERN || k_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            return DROP_CAT_SUPPLY;
        return DROP_CAT_MAX;
    case TV_DIGGING:
        /* Simple shovels/mattocks go to supply (tunneling); egos stay in weapon via add_drop_entry */
        if (k_ptr->sval == SV_SHOVEL || k_ptr->sval == SV_MATTOCK)
            return DROP_CAT_SUPPLY;
        return DROP_CAT_WEAPON;
    case TV_POTION:
    case TV_STAFF:
    case TV_HORN:
    case TV_GEM:
    case TV_FOOD: /* Herbs */
    case TV_FLASK:
        return DROP_CAT_SUPPLY;
    default:
        return DROP_CAT_MAX;
    }
}

static int min_locale_depth(const object_kind* k_ptr)
{
    int min_depth = k_ptr->level;
    for (int i = 0; i < 4; i++)
    {
        if (k_ptr->locale[i] && (k_ptr->locale[i] < min_depth || min_depth == 0))
            min_depth = k_ptr->locale[i];
    }
    if (min_depth <= 0)
        min_depth = 1;
    return min_depth;
}

static int max_locale_depth(const object_kind* k_ptr)
{
    /*
     * CRITICAL: Return 0 to indicate NO max depth restriction.
     * The locale array indicates where items spawn naturally via allocation,
     * but the drop system should NOT be limited by these depths.
     */
    (void)k_ptr;
    return 0;
}

static void sort_allocations(byte* depths, byte* rarities, int count)
{
    for (int i = 1; i < count; i++)
    {
        for (int j = i; j > 0; j--)
        {
            if (depths[j] < depths[j - 1])
            {
                byte tmp_d = depths[j];
                byte tmp_r = rarities[j];
                depths[j] = depths[j - 1];
                rarities[j] = rarities[j - 1];
                depths[j - 1] = tmp_d;
                rarities[j - 1] = tmp_r;
            }
            else
            {
                break;
            }
        }
    }
}

static int collect_kind_allocations(const object_kind* k_ptr, byte* depths, byte* rarities)
{
    int count = 0;

    if (k_ptr->alloc_count > 0)
    {
        for (int i = 0; i < k_ptr->alloc_count && i < 4; i++)
        {
            depths[count] = k_ptr->alloc_depth[i];
            rarities[count] = k_ptr->alloc_prob[i];
            count++;
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            if (k_ptr->chance[i])
            {
                depths[count] = k_ptr->locale[i];
                rarities[count] = k_ptr->chance[i];
                count++;
            }
        }
    }

    sort_allocations(depths, rarities, count);
    return count;
}

static int collect_ego_allocations(const ego_item_type* e_ptr, byte* depths, byte* rarities)
{
    int count = 0;

    if (e_ptr->alloc_count > 0)
    {
        for (int i = 0; i < e_ptr->alloc_count && i < 4; i++)
        {
            depths[count] = e_ptr->alloc_depth[i];
            rarities[count] = e_ptr->alloc_prob[i];
            count++;
        }
    }

    sort_allocations(depths, rarities, count);
    return count;
}

static int schedule_min_depth(const byte* depths, int count, int fallback)
{
    if (count <= 0)
        return fallback;
    int min_depth = depths[0];
    for (int i = 1; i < count; i++)
    {
        if (depths[i] < min_depth)
            min_depth = depths[i];
    }
    return (min_depth > 0) ? min_depth : fallback;
}

static int schedule_max_depth_cap(const byte* depths, const byte* rarities, int count)
{
    int last_positive = -1;
    for (int i = 0; i < count; i++)
    {
        if (rarities[i] > 0)
            last_positive = i;
    }

    if (last_positive < 0)
        return -1; /* all zero rarities */

    /* If the schedule transitions to zero after the last positive entry,
     * treat that as an inclusive max-depth marker (i.e. cap at that depth). */
    for (int i = last_positive + 1; i < count; i++)
    {
        if (rarities[i] == 0)
            return depths[i];
    }

    return 0; /* no cap */
}

static int rarity_from_schedule(const byte* depths, const byte* rarities, int count,
    int depth, int default_rarity)
{
    if (count <= 0)
        return default_rarity;

    /* Trailing zero-rarity entries are treated as max-depth markers, not an
     * in-band rarity override at that exact depth. */
    while (count > 1 && rarities[count - 1] == 0)
        count--;

    int rarity = rarities[0];
    for (int i = 1; i < count; i++)
    {
        if (depth >= depths[i])
            rarity = rarities[i];
        else
            break;
    }
    return rarity;
}

static int combine_allocations(const byte* base_depths, const byte* base_rarities, int base_count,
    const byte* ego_depths, const byte* ego_rarities, int ego_count,
    byte* out_depths, byte* out_rarities)
{
    int base_cap = schedule_max_depth_cap(base_depths, base_rarities, base_count);
    int ego_cap = schedule_max_depth_cap(ego_depths, ego_rarities, ego_count);
    int combined_cap = 0;
    if (base_cap > 0 && ego_cap > 0)
        combined_cap = MIN(base_cap, ego_cap);
    else if (base_cap > 0)
        combined_cap = base_cap;
    else if (ego_cap > 0)
        combined_cap = ego_cap;

    byte merged[DROP_ALLOC_MAX];
    int merged_count = 0;

    for (int i = 0; i < base_count && merged_count < DROP_ALLOC_MAX; i++)
    {
        if (combined_cap > 0 && base_depths[i] > combined_cap)
            continue;
        bool exists = false;
        for (int j = 0; j < merged_count; j++)
        {
            if (merged[j] == base_depths[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            merged[merged_count++] = base_depths[i];
    }

    for (int i = 0; i < ego_count && merged_count < DROP_ALLOC_MAX; i++)
    {
        if (combined_cap > 0 && ego_depths[i] > combined_cap)
            continue;
        bool exists = false;
        for (int j = 0; j < merged_count; j++)
        {
            if (merged[j] == ego_depths[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            merged[merged_count++] = ego_depths[i];
    }

    for (int i = 0; i < merged_count; i++)
    {
        for (int j = i + 1; j < merged_count; j++)
        {
            if (merged[j] < merged[i])
            {
                byte tmp = merged[i];
                merged[i] = merged[j];
                merged[j] = tmp;
            }
        }
    }

    int out_count = 0;
    for (int i = 0; i < merged_count && out_count < DROP_ALLOC_MAX; i++)
    {
        int depth = merged[i];
        if (combined_cap > 0 && depth > combined_cap)
            continue;
        int base_r = rarity_from_schedule(base_depths, base_rarities, base_count, depth, 1);
        int ego_r = rarity_from_schedule(ego_depths, ego_rarities, ego_count, depth, 1);
        /* Allocation weights are treated as 0-100-ish rarity/weight values.
         * When combining base and ego schedules, scale back down and round up
         * so low-percentage egos don't truncate to zero (which would make
         * valid combos impossible).
         * e.g. base=15 and ego=33 yields 5 (rounded up from 4.95).
         */
        int combined = 0;
        if (base_r > 0 && ego_r > 0)
            combined = (base_r * ego_r + 99) / 100;
        if (out_count == 0 || combined != out_rarities[out_count - 1])
        {
            out_depths[out_count] = (byte)depth;
            out_rarities[out_count] = (byte)MIN(combined, 255);
            out_count++;
        }
    }

    /* Preserve inclusive max-depth markers (A:.../0) so max_depth caps apply to combined entries.
     * This intentionally allows duplicate depths (e.g. depth=6 rarity=X then depth=6 rarity=0),
     * with the trailing 0 treated as a cap marker by schedule_max_depth_cap(). */
    if (combined_cap > 0 && out_count < DROP_ALLOC_MAX)
    {
        out_depths[out_count] = (byte)combined_cap;
        out_rarities[out_count] = 0;
        out_count++;
    }

    return out_count;
}

static int more_special_rarity_bonus(int rarity_percent)
{
    if (rarity_percent <= 0)
        return 0;

    rarity_percent += 20;
    if (rarity_percent > 255)
        rarity_percent = 255;

    return rarity_percent;
}

static int less_special_rarity_penalty(int rarity_percent)
{
    if (rarity_percent <= 0)
        return 0;

    rarity_percent -= 20;
    if (rarity_percent < 0)
        rarity_percent = 0;

    return rarity_percent;
}

static byte scale_arrow_supply_rarity(byte rarity, int att_bonus)
{
    int scaled = rarity;

    while (att_bonus > 0 && scaled > 0)
    {
        scaled /= 2;
        att_bonus--;
    }

    return (byte)scaled;
}

typedef enum
{
    DROP_ALIGNMENT_STANDARD = 0,
    DROP_ALIGNMENT_NOBLE = 1,
    DROP_ALIGNMENT_EVIL = 2
} drop_alignment;

typedef enum
{
    DROP_ALIGNMENT_FILTER_ANY = 0,
    DROP_ALIGNMENT_FILTER_NOBLE = 1,
    DROP_ALIGNMENT_FILTER_EVIL = 2
} drop_alignment_filter;

static bool merge_drop_alignment_from_flags4(drop_alignment* alignment, u32b flags4)
{
    bool noble = (flags4 & TR4_NOBLE_ITEM) != 0;
    bool evil = (flags4 & TR4_EVIL_ITEM) != 0;
    drop_alignment item_alignment;

    if (noble && evil)
        return false;

    if (!noble && !evil)
        return true;

    item_alignment = noble ? DROP_ALIGNMENT_NOBLE : DROP_ALIGNMENT_EVIL;

    if (*alignment == DROP_ALIGNMENT_STANDARD || *alignment == item_alignment)
    {
        *alignment = item_alignment;
        return true;
    }

    return false;
}

static void add_drop_entry(const object_type* proto, drop_category cat,
    drop_group_kind group_kind, int group_id, int min_depth, int max_depth,
    const byte* alloc_depths, const byte* alloc_rarities, int num_allocs)
{
    object_kind* k_ptr = &k_info[proto->k_idx];

    /* Never allow INSTA_ART templates except as true artefacts */
    if ((k_ptr->flags3 & TR3_INSTA_ART) && group_kind != DROP_GROUP_ARTIFACT)
        return;

    /* Override category: simple arrows go to supply (egos go to weapon) */
    if (group_kind == DROP_GROUP_NORMAL && k_ptr->tval == TV_ARROW)
        cat = DROP_CAT_SUPPLY;

    /* Override category: ego digging tools go to weapon (normals stay in supply) */
    if (group_kind == DROP_GROUP_EGO && k_ptr->tval == TV_DIGGING)
        cat = DROP_CAT_WEAPON;

    /* Override category: artefact digging tools go to weapon (simple tools are supply-only) */
    if (group_kind == DROP_GROUP_ARTIFACT && k_ptr->tval == TV_DIGGING)
        cat = DROP_CAT_WEAPON;

    /* Special case: Lesser Jewel of Grace stays in jewelry */
    if (k_ptr->tval == TV_LIGHT && k_ptr->sval == SV_LIGHT_LESSER_JEWEL
        && object_has_ego_idx(proto, EGO_GRACE))
    {
        cat = DROP_CAT_JEWELRY;
    }

    drop_alignment alignment = DROP_ALIGNMENT_STANDARD;

    if (!merge_drop_alignment_from_flags4(&alignment, k_ptr->flags4))
        return;

    /* Check ego suffix (name2) flags4 */
    if (proto->name2 > 0 && (int)proto->name2 < z_info->e_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, e_info[(int)proto->name2].flags4))
            return;
    }

    /* Check ego prefix (unused2) flags4 */
    if (proto->unused2 > 0 && (int)proto->unused2 < z_info->e_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, e_info[(int)proto->unused2].flags4))
            return;
    }

    /* Check artefact flags4 */
    if (group_kind == DROP_GROUP_ARTIFACT
        && group_id > 0 && group_id < z_info->art_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, a_info[group_id].flags4))
            return;
    }

    if (g_drop_count + 1 > g_drop_capacity)
    {
        size_t new_cap = (g_drop_capacity == 0) ? 1024 : g_drop_capacity * 2;
        if (new_cap < g_drop_count + 1)
            new_cap = g_drop_count + 1;
        drop_entry* new_buf = mem_alloc_array(new_cap, drop_entry);
        if (g_drop_entries && g_drop_count)
            memcpy(new_buf, g_drop_entries, g_drop_count * sizeof(drop_entry));
        mem_free_null(g_drop_entries);
        g_drop_entries = new_buf;
        g_drop_capacity = new_cap;
    }

    drop_entry* entry = &g_drop_entries[g_drop_count++];
    object_copy(&entry->obj, proto);
    entry->category = cat;
    entry->group_kind = group_kind;
    entry->group_id = (s16b)group_id;
    entry->min_depth = (s16b)min_depth;
    entry->max_depth = (s16b)max_depth;
    entry->num_allocations = (byte)MIN(num_allocs, DROP_ALLOC_MAX);
    for (int i = 0; i < entry->num_allocations; i++)
    {
        entry->alloc_depth[i] = alloc_depths[i];
        entry->alloc_rarity[i] = alloc_rarities[i];
    }
    if (cat == DROP_CAT_SUPPLY)
        entry->difficulty = 0;
    else
        entry->difficulty = (s16b)smithing_difficulty_baseline(&entry->obj);
    entry->noble = (alignment == DROP_ALIGNMENT_NOBLE);
    entry->evil = (alignment == DROP_ALIGNMENT_EVIL);
}

/* Apply ego flag data (abilities and curses) without randomness */
static void apply_ego_static(object_type* o_ptr, ego_item_type* e_ptr)
{
    // abilities
    for (int i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
    {
        int idx = o_ptr->abilities;
        o_ptr->skilltype[idx] = e_ptr->skilltype[i];
        o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
        o_ptr->abilities++;
    }

    // cursed / broken flags
    if (!e_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);
    if (e_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i])
            o_ptr->stat_bonus[i] += e_ptr->stat_bonus_min[i];
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i])
            o_ptr->skill_bonus[i] += e_ptr->skill_bonus_min[i];
    }
}

typedef struct
{
    bool is_stat;
    byte index;
    s16b min_value;
    s16b max_value;
} drop_bonus_range;

static int collect_ego_bonus_ranges(const ego_item_type* first,
    const ego_item_type* second, drop_bonus_range* out, int max_out)
{
    s16b stat_min[A_MAX] = { 0 };
    s16b stat_max[A_MAX] = { 0 };
    bool stat_used[A_MAX] = { false };
    s16b skill_min[S_MAX] = { 0 };
    s16b skill_max[S_MAX] = { 0 };
    bool skill_used[S_MAX] = { false };
    const ego_item_type* egos[2] = { first, second };
    int count = 0;

    for (int ego_idx = 0; ego_idx < (int)N_ELEMENTS(egos); ego_idx++)
    {
        const ego_item_type* e_ptr = egos[ego_idx];
        if (!e_ptr)
            continue;

        for (int i = 0; i < A_MAX; i++)
        {
            if (!e_ptr->stat_bonus_set[i])
                continue;

            stat_used[i] = true;
            stat_min[i] += e_ptr->stat_bonus_min[i];
            stat_max[i] += e_ptr->stat_bonus[i];
        }

        for (int i = 0; i < S_MAX; i++)
        {
            if (!e_ptr->skill_bonus_set[i])
                continue;

            skill_used[i] = true;
            skill_min[i] += e_ptr->skill_bonus_min[i];
            skill_max[i] += e_ptr->skill_bonus[i];
        }
    }

    for (int i = 0; i < A_MAX && count < max_out; i++)
    {
        if (!stat_used[i] || stat_max[i] <= stat_min[i])
            continue;

        out[count].is_stat = true;
        out[count].index = (byte)i;
        out[count].min_value = stat_min[i];
        out[count].max_value = stat_max[i];
        count++;
    }

    for (int i = 0; i < S_MAX && count < max_out; i++)
    {
        if (!skill_used[i] || skill_max[i] <= skill_min[i])
            continue;

        out[count].is_stat = false;
        out[count].index = (byte)i;
        out[count].min_value = skill_min[i];
        out[count].max_value = skill_max[i];
        count++;
    }

    return count;
}

static void add_drop_entry_with_bonus_ranges_recursive(const object_type* proto,
    drop_category cat, drop_group_kind group_kind, int group_id, int min_depth,
    int max_depth, const byte* alloc_depths, const byte* alloc_rarities,
    int num_allocs, const drop_bonus_range* ranges, int range_count, int range_idx)
{
    if (range_idx >= range_count)
    {
        add_drop_entry(proto, cat, group_kind, group_id, min_depth, max_depth,
            alloc_depths, alloc_rarities, num_allocs);
        return;
    }

    const drop_bonus_range* range = &ranges[range_idx];
    for (int value = range->min_value; value <= range->max_value; value++)
    {
        object_type v = *proto;
        int delta = value - range->min_value;

        if (range->is_stat)
            v.stat_bonus[range->index] += (s16b)delta;
        else
            v.skill_bonus[range->index] += (s16b)delta;

        add_drop_entry_with_bonus_ranges_recursive(&v, cat, group_kind, group_id,
            min_depth, max_depth, alloc_depths, alloc_rarities, num_allocs,
            ranges, range_count, range_idx + 1);
    }
}

static void add_drop_entry_with_bonus_ranges(const object_type* proto,
    drop_category cat, drop_group_kind group_kind, int group_id, int min_depth,
    int max_depth, const byte* alloc_depths, const byte* alloc_rarities,
    int num_allocs, const ego_item_type* first, const ego_item_type* second)
{
    drop_bonus_range ranges[A_MAX + S_MAX];
    int range_count = collect_ego_bonus_ranges(first, second, ranges,
        (int)N_ELEMENTS(ranges));

    if (range_count <= 0)
    {
        add_drop_entry(proto, cat, group_kind, group_id, min_depth, max_depth,
            alloc_depths, alloc_rarities, num_allocs);
        return;
    }

    add_drop_entry_with_bonus_ranges_recursive(proto, cat, group_kind, group_id,
        min_depth, max_depth, alloc_depths, alloc_rarities, num_allocs,
        ranges, range_count, 0);
}

static bool ego_applies_to_kind(const ego_item_type* e_ptr, const object_kind* k_ptr)
{
    if (!e_ptr || !k_ptr)
        return false;

    for (int t = 0; t < EGO_TVALS_MAX; t++)
    {
        if (!e_ptr->tval[t])
            continue;
        if (k_ptr->tval != e_ptr->tval[t])
            continue;
        if (k_ptr->sval < e_ptr->min_sval[t] || k_ptr->sval > e_ptr->max_sval[t])
            continue;
        return true;
    }

    return false;
}

static int ego_combo_group_id(int prefix_idx, int suffix_idx)
{
    return ((prefix_idx & 0xFF) << 8) | (suffix_idx & 0xFF);
}

/* Build variants for a base object (normal item). */
static void build_normal_variants(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    /* Skip pure artifact templates; they should only appear via artefact entries */
    if (k_ptr->flags3 & TR3_INSTA_ART)
        return;
    if (k_ptr->flags3 & TR3_DAMAGED)
        return; /* Damaged drops come from explicit damaged-drop paths. */

    drop_category cat = drop_category_for_kind(k_ptr);
    if (cat == DROP_CAT_MAX)
        return;

    object_type base;
    object_prep(&base, k_idx);
    base.weight = k_ptr->weight;

    byte alloc_depths[DROP_ALLOC_MAX];
    byte alloc_rarities[DROP_ALLOC_MAX];
    int num_allocations = collect_kind_allocations(k_ptr, alloc_depths, alloc_rarities);
    int fallback_min = min_locale_depth(k_ptr);
    if (fallback_min <= 0)
        fallback_min = 1;
    if (num_allocations == 0)
    {
        alloc_depths[0] = (byte)fallback_min;
        alloc_rarities[0] = 1;
        num_allocations = 1;
    }

    bool has_positive_rarity = false;
    for (int i = 0; i < num_allocations; i++)
    {
        if (alloc_rarities[i] > 0)
        {
            has_positive_rarity = true;
            break;
        }
    }
    int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
    if (!has_positive_rarity && rarity_cap_depth < 0)
        return; /* never spawns */

    int min_depth = schedule_min_depth(alloc_depths, num_allocations, fallback_min);
    int max_depth = max_locale_depth(k_ptr);
    if (min_depth <= 0)
        min_depth = 1;
    if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
        max_depth = rarity_cap_depth;

    drop_group_kind group_kind = (cat == DROP_CAT_JEWELRY) ? DROP_GROUP_EGO : DROP_GROUP_NORMAL;

    /* Supply items: no smithing variants, use new allocation semantics. */
    if (cat == DROP_CAT_SUPPLY)
    {
        if (k_ptr->tval == TV_ARROW)
        {
            int att_min = k_ptr->att;
            int att_max = MAX(k_ptr->att, k_ptr->max_att);

            for (int att = att_min; att <= att_max; att++)
            {
                object_type v = base;
                byte arrow_alloc_rarities[DROP_ALLOC_MAX];
                int att_bonus = MAX(0, att - k_ptr->att);

                memcpy(arrow_alloc_rarities, alloc_rarities,
                    sizeof(arrow_alloc_rarities));
                for (int i = 0; i < num_allocations; i++)
                    arrow_alloc_rarities[i] = scale_arrow_supply_rarity(
                        alloc_rarities[i], att_bonus);

                v.att = att;
                add_drop_entry(&v, cat, DROP_GROUP_NORMAL, k_idx, min_depth,
                    max_depth, alloc_depths, arrow_alloc_rarities,
                    num_allocations);
            }
        }
        else
        {
            add_drop_entry(&base, cat, DROP_GROUP_NORMAL, k_idx, min_depth,
                max_depth, alloc_depths, alloc_rarities, num_allocations);
        }
        return;
    }

    /* Ranges from data-driven R: lines in object.txt */
    int att_min = k_ptr->att;
    int att_max = k_ptr->max_att;
    int ds_min = k_ptr->ds;
    int ds_max = k_ptr->max_ds;
    int evn_min = k_ptr->evn;
    int evn_max = k_ptr->max_evn;
    int pd_min = drop_kind_base_pd_min(k_ptr);
    int pd_max = drop_kind_base_pd_max(k_ptr);
    int ps_min = k_ptr->ps;
    int ps_max = k_ptr->max_ps;
    u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
    int pval_min = k_ptr->pval;
    int pval_max = k_ptr->max_pval;
    bool pval_allowed = kind_pval_mask != 0 || k_ptr->pval != 0;

    // Variant list (all combinations within smithing caps)
    // Use combined rarity and minimum depth for the entire item
    for (int att = att_min; att <= att_max; att++)
    {
        for (int ds = ds_min; ds <= ds_max; ds++)
        {
            for (int evn = evn_min; evn <= evn_max; evn++)
            {
                for (int pd = pd_min; pd <= pd_max; pd++)
                {
                    for (int ps = ps_min; ps <= ps_max; ps++)
                    {
                        int pval_hi = pval_allowed ? pval_max : pval_min;
                        for (int pval = pval_min; pval <= pval_hi; pval++)
                        {
                            object_type v = base;
                            int delta = pval - base.pval;
                            v.att = att;
                            v.ds = ds;
                            v.evn = evn;
                            v.pd = pd;
                            v.ps = ps;
                            v.pval = pval;

                            if (delta != 0)
                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, delta);
                            add_drop_entry(&v, cat, group_kind, k_idx,
                                min_depth, max_depth,
                                alloc_depths, alloc_rarities, num_allocations);
                        }
                    }
                }
            }
        }
    }
}

/* Build variants for ego items over applicable base kinds. */
static void build_ego_variants(int e_idx)
{
    ego_item_type* e_ptr = &e_info[e_idx];
    if (!e_ptr->tval[0])
        return;

    const char* ego_name = e_name + e_ptr->name;
    bool is_prefix = ego_name_is_prefix(ego_name);

    for (int t = 0; t < EGO_TVALS_MAX; t++)
    {
        if (!e_ptr->tval[t])
            continue;
        for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
        {
            object_kind* k_ptr = &k_info[k_idx];
            if (k_ptr->tval != e_ptr->tval[t])
                continue;
            if (k_ptr->sval < e_ptr->min_sval[t] || k_ptr->sval > e_ptr->max_sval[t])
                continue;
            if (k_ptr->flags3 & TR3_INSTA_ART)
                continue;
            if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
                && (e_ptr->flags4 & TR4_EVIL_ITEM))
                continue;

            drop_category cat = drop_category_for_kind(k_ptr);
            if (cat == DROP_CAT_MAX)
                continue;

            object_type base;
            object_prep(&base, k_idx);
            base.weight = k_ptr->weight;
            if (is_prefix)
                object_set_ego_prefix(&base, e_idx);
            else
                object_set_ego_suffix(&base, e_idx);
            apply_ego_static(&base, e_ptr);

            /* Ego items: use ego W: depth for min_depth (for difficulty penalty) */
            int ego_fallback_depth = (e_ptr->level > 0) ? e_ptr->level : 1;
            int max_depth = (e_ptr->max_level > 0) ? e_ptr->max_level
                                                   : max_locale_depth(k_ptr);

            byte base_depths[DROP_ALLOC_MAX];
            byte base_rarities[DROP_ALLOC_MAX];
            int base_allocs = collect_kind_allocations(k_ptr, base_depths, base_rarities);
            int base_fallback_depth = min_locale_depth(k_ptr);
            if (base_fallback_depth <= 0)
                base_fallback_depth = 1;
            if (base_allocs == 0)
            {
                base_depths[0] = (byte)base_fallback_depth;
                base_rarities[0] = 1;
                base_allocs = 1;
            }

            byte ego_depths[DROP_ALLOC_MAX];
            byte ego_rarities[DROP_ALLOC_MAX];
            int ego_allocs = collect_ego_allocations(e_ptr, ego_depths, ego_rarities);
            int ego_default_rarity = (e_ptr->rarity > 0) ? e_ptr->rarity : 1;
            if (ego_allocs == 0)
            {
                ego_depths[0] = (byte)ego_fallback_depth;
                ego_rarities[0] = (byte)ego_default_rarity;
                ego_allocs = 1;
            }

            /* MORE_SPECIAL: boost base item rarities by one tier (+20) so that
               e.g. Dagger (85) becomes 100, grouping with Spear/Shortsword. */
            if (k_ptr->flags3 & TR3_MORE_SPECIAL)
            {
                for (int i = 0; i < base_allocs; i++)
                    base_rarities[i] = (byte)more_special_rarity_bonus(base_rarities[i]);
            }

            /* LESS_SPECIAL: reduce base item rarities by one tier (-20), making
               ego combinations rarer. Bottoms out at 0. */
            if (k_ptr->flags4 & TR4_LESS_SPECIAL)
            {
                for (int i = 0; i < base_allocs; i++)
                    base_rarities[i] = (byte)less_special_rarity_penalty(base_rarities[i]);
            }

            byte alloc_depths[DROP_ALLOC_MAX];
            byte alloc_rarities[DROP_ALLOC_MAX];
            int num_allocations = combine_allocations(
                base_depths, base_rarities, base_allocs,
                ego_depths, ego_rarities, ego_allocs,
                alloc_depths, alloc_rarities);

            if (num_allocations == 0)
                continue;

            bool has_positive_rarity = false;
            for (int i = 0; i < num_allocations; i++)
            {
                if (alloc_rarities[i] > 0)
                {
                    has_positive_rarity = true;
                    break;
                }
            }
            if (!has_positive_rarity && num_allocations > 0
                && schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations) < 0)
            {
                continue;
            }

            int base_min_depth = schedule_min_depth(base_depths, base_allocs, base_fallback_depth);
            int min_depth = schedule_min_depth(ego_depths, ego_allocs, ego_fallback_depth);
            if (base_min_depth <= 0)
                base_min_depth = 1;
            if (min_depth < base_min_depth)
                min_depth = base_min_depth;
            int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
            if (min_depth <= 0)
                min_depth = 1;
            if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
                max_depth = rarity_cap_depth;

            /* Ranges from data-driven R: lines + ego C: line contributions */
            int ego_max_att = ego_s8(e_ptr->max_att);
            int ego_to_ds = ego_s8(e_ptr->to_ds);
            int ego_max_evn = ego_s8(e_ptr->max_evn);
            int ego_to_ps = ego_s8(e_ptr->to_ps);
            int ego_to_dd = ego_s8(e_ptr->to_dd);
            int ego_to_pd = ego_s8(e_ptr->to_pd);

            int att_min = k_ptr->att + smithing_step_from_ego_bonus(ego_max_att);
            int att_max = k_ptr->max_att + ego_max_att;
            int ds_min = k_ptr->ds + smithing_step_from_ego_bonus(ego_to_ds);
            int ds_max = k_ptr->max_ds + ego_to_ds;
            int evn_min = k_ptr->evn + smithing_step_from_ego_bonus(ego_max_evn);
            int evn_max = k_ptr->max_evn + ego_max_evn;
            int ps_min = k_ptr->ps + smithing_step_from_ego_bonus(ego_to_ps);
            int ps_max = k_ptr->max_ps + ego_to_ps;
            int dd_min = k_ptr->dd + smithing_step_from_ego_bonus(ego_to_dd);
            int dd_max = k_ptr->dd + ego_to_dd;
            int pd_min = drop_kind_base_pd_min(k_ptr)
                + smithing_step_from_ego_bonus(ego_to_pd);
            int pd_max = drop_kind_base_pd_max(k_ptr) + ego_to_pd;
            u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
            u32b ego_pval_mask = ego_item_pval_flags1(e_ptr);
            int kind_pval_min = k_ptr->pval;
            int kind_pval_max = k_ptr->max_pval;
            int ego_pval_min = (e_ptr->max_pval > 0) ? 1 : 0;
            int ego_pval_max = e_ptr->max_pval;
            bool kind_pval_allowed = (kind_pval_mask != 0)
                || (k_ptr->pval != 0) || (k_ptr->max_pval != k_ptr->pval);

            if (ds_min < 0)
                ds_min = 0;
            if (ds_max < 0)
                ds_max = 0;
            if (dd_min < 0)
                dd_min = 0;
            if (dd_max < 0)
                dd_max = 0;
            if (pd_min < 0)
                pd_min = 0;
            if (pd_max < 0)
                pd_max = 0;
            if (ps_min < 0)
                ps_min = 0;
            if (ps_max < 0)
                ps_max = 0;

            if (att_min > att_max)
                att_min = att_max;
            if (ds_min > ds_max)
                ds_min = ds_max;
            if (evn_min > evn_max)
                evn_min = evn_max;
            if (ps_min > ps_max)
                ps_min = ps_max;
            if (dd_min > dd_max)
                dd_min = dd_max;
            if (pd_min > pd_max)
                pd_min = pd_max;
            if (kind_pval_min > kind_pval_max)
                kind_pval_max = kind_pval_min;
            if (ego_pval_min > ego_pval_max)
                ego_pval_min = ego_pval_max;

            /* Generate variants using combined rarity and effective min depth */
            for (int att = att_min; att <= att_max; att++)
            {
                for (int ds = ds_min; ds <= ds_max; ds++)
                {
                    for (int evn = evn_min; evn <= evn_max; evn++)
                    {
                        for (int ps = ps_min; ps <= ps_max; ps++)
                        {
                            int kind_pval_hi = kind_pval_allowed ? kind_pval_max : kind_pval_min;
                            for (int kind_pval = kind_pval_min;
                                 kind_pval <= kind_pval_hi; kind_pval++)
                            {
                                for (int ego_pval = ego_pval_min; ego_pval <= ego_pval_max; ego_pval++)
                                {
                                    for (int dd = dd_min; dd <= dd_max; dd++)
                                    {
                                        for (int pd = pd_min; pd <= pd_max; pd++)
                                        {
                                            object_type v = base;
                                            int kind_delta = kind_pval - base.pval;
                                            /* Keep catalog prototypes aligned with object_into_special():
                                             * cursed egos remain cursed, but their pval is not inverted. */
                                            int ego_delta = ego_pval;
                                            v.att = att;
                                            v.ds = ds;
                                            v.dd = dd;
                                            v.evn = evn;
                                            v.ps = ps;
                                            v.pd = pd;
                                            v.pval = kind_pval + ego_delta;

                                            if (kind_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, kind_delta);
                                            if (ego_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, ego_pval_mask, ego_delta);

                                            add_drop_entry_with_bonus_ranges(&v, cat,
                                                DROP_GROUP_EGO, e_idx, min_depth,
                                                max_depth, alloc_depths,
                                                alloc_rarities, num_allocations,
                                                e_ptr, NULL);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void build_ego_combo_variants(int prefix_idx, int suffix_idx)
{
    if (prefix_idx <= 0 || suffix_idx <= 0)
        return;

    ego_item_type* prefix_ptr = &e_info[prefix_idx];
    ego_item_type* suffix_ptr = &e_info[suffix_idx];
    if (suffix_ptr->flags4 & TR4_NO_PREFIX)
        return;
    if (!prefix_ptr->tval[0] || !suffix_ptr->tval[0])
        return;

    const char* prefix_name = e_name + prefix_ptr->name;
    const char* suffix_name = e_name + suffix_ptr->name;
    if (!ego_name_is_prefix(prefix_name) || ego_name_is_prefix(suffix_name))
        return;

    if ((prefix_ptr->flags4 & TR4_NOBLE_ITEM) && (suffix_ptr->flags4 & TR4_EVIL_ITEM))
        return;
    if ((prefix_ptr->flags4 & TR4_EVIL_ITEM) && (suffix_ptr->flags4 & TR4_NOBLE_ITEM))
        return;

    int group_id = ego_combo_group_id(prefix_idx, suffix_idx);

    for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
    {
        object_kind* k_ptr = &k_info[k_idx];
        if (k_ptr->flags3 & TR3_INSTA_ART)
            continue;
        if (!ego_applies_to_kind(prefix_ptr, k_ptr) || !ego_applies_to_kind(suffix_ptr, k_ptr))
            continue;
        if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && ((prefix_ptr->flags4 & TR4_EVIL_ITEM)
                || (suffix_ptr->flags4 & TR4_EVIL_ITEM)))
            continue;

        drop_category cat = drop_category_for_kind(k_ptr);
        if (cat == DROP_CAT_MAX)
            continue;

        object_type base;
        object_prep(&base, k_idx);
        base.weight = k_ptr->weight;
        object_set_ego_prefix(&base, prefix_idx);
        object_set_ego_suffix(&base, suffix_idx);
        apply_ego_static(&base, prefix_ptr);
        apply_ego_static(&base, suffix_ptr);

        /* Max depth: apply the strictest max-level restriction among the two egos. */
        int max_depth = max_locale_depth(k_ptr);
        if (prefix_ptr->max_level > 0 && (max_depth == 0 || prefix_ptr->max_level < max_depth))
            max_depth = prefix_ptr->max_level;
        if (suffix_ptr->max_level > 0 && (max_depth == 0 || suffix_ptr->max_level < max_depth))
            max_depth = suffix_ptr->max_level;

        byte base_depths[DROP_ALLOC_MAX];
        byte base_rarities[DROP_ALLOC_MAX];
        int base_allocs = collect_kind_allocations(k_ptr, base_depths, base_rarities);
        int base_fallback_depth = min_locale_depth(k_ptr);
        if (base_fallback_depth <= 0)
            base_fallback_depth = 1;
        if (base_allocs == 0)
        {
            base_depths[0] = (byte)base_fallback_depth;
            base_rarities[0] = 1;
            base_allocs = 1;
        }

        int prefix_fallback_depth = (prefix_ptr->level > 0) ? prefix_ptr->level : 1;
        int suffix_fallback_depth = (suffix_ptr->level > 0) ? suffix_ptr->level : 1;

        byte prefix_depths[DROP_ALLOC_MAX];
        byte prefix_rarities[DROP_ALLOC_MAX];
        int prefix_allocs = collect_ego_allocations(prefix_ptr, prefix_depths, prefix_rarities);
        int prefix_default_rarity = (prefix_ptr->rarity > 0) ? prefix_ptr->rarity : 1;
        if (prefix_allocs == 0)
        {
            prefix_depths[0] = (byte)prefix_fallback_depth;
            prefix_rarities[0] = (byte)prefix_default_rarity;
            prefix_allocs = 1;
        }

        byte suffix_depths[DROP_ALLOC_MAX];
        byte suffix_rarities[DROP_ALLOC_MAX];
        int suffix_allocs = collect_ego_allocations(suffix_ptr, suffix_depths, suffix_rarities);
        int suffix_default_rarity = (suffix_ptr->rarity > 0) ? suffix_ptr->rarity : 1;
        if (suffix_allocs == 0)
        {
            suffix_depths[0] = (byte)suffix_fallback_depth;
            suffix_rarities[0] = (byte)suffix_default_rarity;
            suffix_allocs = 1;
        }

        byte tmp_depths[DROP_ALLOC_MAX];
        byte tmp_rarities[DROP_ALLOC_MAX];
        int tmp_allocs = combine_allocations(
            base_depths, base_rarities, base_allocs,
            prefix_depths, prefix_rarities, prefix_allocs,
            tmp_depths, tmp_rarities);

        byte alloc_depths[DROP_ALLOC_MAX];
        byte alloc_rarities[DROP_ALLOC_MAX];
        int num_allocations = combine_allocations(
            tmp_depths, tmp_rarities, tmp_allocs,
            suffix_depths, suffix_rarities, suffix_allocs,
            alloc_depths, alloc_rarities);

        if (num_allocations == 0)
            continue;

        bool has_positive_rarity = false;
        for (int i = 0; i < num_allocations; i++)
        {
            if (alloc_rarities[i] > 0)
            {
                has_positive_rarity = true;
                break;
            }
        }
        if (!has_positive_rarity && num_allocations > 0
            && schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations) < 0)
        {
            continue;
        }

        int base_min_depth = schedule_min_depth(base_depths, base_allocs, base_fallback_depth);
        int prefix_min_depth = schedule_min_depth(prefix_depths, prefix_allocs, prefix_fallback_depth);
        int suffix_min_depth = schedule_min_depth(suffix_depths, suffix_allocs, suffix_fallback_depth);

        if (base_min_depth <= 0)
            base_min_depth = 1;
        if (prefix_min_depth <= 0)
            prefix_min_depth = 1;
        if (suffix_min_depth <= 0)
            suffix_min_depth = 1;

        int min_depth = base_min_depth;
        if (prefix_min_depth > min_depth)
            min_depth = prefix_min_depth;
        if (suffix_min_depth > min_depth)
            min_depth = suffix_min_depth;

        int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
        if (min_depth <= 0)
            min_depth = 1;
        if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
            max_depth = rarity_cap_depth;

        /* Combined ego numeric contributions */
        int max_att_bonus = ego_s8(prefix_ptr->max_att) + ego_s8(suffix_ptr->max_att);
        int max_evn_bonus = ego_s8(prefix_ptr->max_evn) + ego_s8(suffix_ptr->max_evn);
        int to_dd_bonus = ego_s8(prefix_ptr->to_dd) + ego_s8(suffix_ptr->to_dd);
        int to_ds_bonus = ego_s8(prefix_ptr->to_ds) + ego_s8(suffix_ptr->to_ds);
        int to_pd_bonus = ego_s8(prefix_ptr->to_pd) + ego_s8(suffix_ptr->to_pd);
        int to_ps_bonus = ego_s8(prefix_ptr->to_ps) + ego_s8(suffix_ptr->to_ps);
        int att_min = k_ptr->att
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->max_att))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->max_att));
        int att_max = k_ptr->max_att + max_att_bonus;
        int ds_min = k_ptr->ds
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_ds))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_ds));
        int ds_max = k_ptr->max_ds + to_ds_bonus;
        int evn_min = k_ptr->evn
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->max_evn))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->max_evn));
        int evn_max = k_ptr->max_evn + max_evn_bonus;
        int ps_min = k_ptr->ps
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_ps))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_ps));
        int ps_max = k_ptr->max_ps + to_ps_bonus;
        int dd_min = k_ptr->dd
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_dd))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_dd));
        int dd_max = k_ptr->dd + to_dd_bonus;
        int pd_min = drop_kind_base_pd_min(k_ptr)
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_pd))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_pd));
        int pd_max = drop_kind_base_pd_max(k_ptr) + to_pd_bonus;
        u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
        u32b prefix_pval_mask = ego_item_pval_flags1(prefix_ptr);
        u32b suffix_pval_mask = ego_item_pval_flags1(suffix_ptr);
        int kind_pval_min = k_ptr->pval;
        int kind_pval_max = k_ptr->max_pval;
        int prefix_pval_min = (prefix_ptr->max_pval > 0) ? 1 : 0;
        int prefix_pval_max = prefix_ptr->max_pval;
        int suffix_pval_min = (suffix_ptr->max_pval > 0) ? 1 : 0;
        int suffix_pval_max = suffix_ptr->max_pval;
        bool kind_pval_allowed = (kind_pval_mask != 0)
            || (k_ptr->pval != 0) || (k_ptr->max_pval != k_ptr->pval);

        if (ds_min < 0)
            ds_min = 0;
        if (ds_max < 0)
            ds_max = 0;
        if (dd_min < 0)
            dd_min = 0;
        if (dd_max < 0)
            dd_max = 0;
        if (pd_min < 0)
            pd_min = 0;
        if (pd_max < 0)
            pd_max = 0;
        if (ps_min < 0)
            ps_min = 0;
        if (ps_max < 0)
            ps_max = 0;

        if (att_min > att_max)
            att_min = att_max;
        if (ds_min > ds_max)
            ds_min = ds_max;
        if (evn_min > evn_max)
            evn_min = evn_max;
        if (ps_min > ps_max)
            ps_min = ps_max;
        if (dd_min > dd_max)
            dd_min = dd_max;
        if (pd_min > pd_max)
            pd_min = pd_max;
        if (kind_pval_min > kind_pval_max)
            kind_pval_max = kind_pval_min;
        if (prefix_pval_min > prefix_pval_max)
            prefix_pval_min = prefix_pval_max;
        if (suffix_pval_min > suffix_pval_max)
            suffix_pval_min = suffix_pval_max;

        for (int att = att_min; att <= att_max; att++)
        {
            for (int ds = ds_min; ds <= ds_max; ds++)
            {
                for (int evn = evn_min; evn <= evn_max; evn++)
                {
                    for (int ps = ps_min; ps <= ps_max; ps++)
                    {
                        int kind_pval_hi = kind_pval_allowed ? kind_pval_max : kind_pval_min;
                        for (int kind_pval = kind_pval_min; kind_pval <= kind_pval_hi; kind_pval++)
                        {
                            for (int prefix_pval = prefix_pval_min; prefix_pval <= prefix_pval_max; prefix_pval++)
                            {
                                for (int suffix_pval = suffix_pval_min; suffix_pval <= suffix_pval_max; suffix_pval++)
                                {
                                    for (int dd = dd_min; dd <= dd_max; dd++)
                                    {
                                        for (int pd = pd_min; pd <= pd_max; pd++)
                                        {
                                            object_type v = base;
                                            int kind_delta = kind_pval - base.pval;
                                            int prefix_delta = prefix_pval;
                                            int suffix_delta = suffix_pval;
                                            v.att = att;
                                            v.ds = ds;
                                            v.dd = dd;
                                            v.evn = evn;
                                            v.ps = ps;
                                            v.pd = pd;
                                            v.pval = kind_pval + prefix_delta + suffix_delta;

                                            if (kind_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, kind_delta);
                                            if (prefix_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, prefix_pval_mask, prefix_delta);
                                            if (suffix_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, suffix_pval_mask, suffix_delta);

                                            add_drop_entry_with_bonus_ranges(&v, cat,
                                                DROP_GROUP_EGO, group_id, min_depth,
                                                max_depth, alloc_depths,
                                                alloc_rarities, num_allocations,
                                                prefix_ptr, suffix_ptr);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Build artefact entries (single variants). */
static void build_artifact_variants(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];
    if (!a_ptr->tval || !a_ptr->sval)
        return;
    int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (!k_idx)
        return;

    object_type v;
    object_prep(&v, k_idx);
    v.name1 = a_idx;

    /* Copy artefact stats */
    v.pval = a_ptr->pval;
    for (int i = 0; i < A_MAX; i++)
        v.stat_bonus[i] = a_ptr->stat_bonus[i];
    for (int i = 0; i < S_MAX; i++)
        v.skill_bonus[i] = a_ptr->skill_bonus[i];
    v.att = a_ptr->att;
    v.evn = a_ptr->evn;
    v.dd = a_ptr->dd;
    v.ds = a_ptr->ds;
    v.pd = a_ptr->pd;
    v.ps = a_ptr->ps;
    v.weight = a_ptr->weight;

    /* For stackable artefacts (arrows, throwing weapons), use spawn_num */
    {
        object_kind* ak_ptr = &k_info[k_idx];
        if ((v.tval == TV_ARROW) || (ak_ptr->flags3 & TR3_THROWING))
        {
            int desired = a_ptr->spawn_num ? (int)a_ptr->spawn_num : 1;
            int limit = object_stack_limit(&v);
            if (limit > 0 && desired > limit)
                desired = limit;
            if (desired < 1)
                desired = 1;
            v.number = (byte)desired;
        }
    }

    v.ident = 0;
    if (!a_ptr->cost)
        v.ident |= (IDENT_BROKEN);
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        v.ident |= (IDENT_CURSED);

    /* Copy artefact-granted abilities (mirrors object_into_artefact()). */
    for (int i = 0; i < a_ptr->abilities && v.abilities < (int)N_ELEMENTS(v.skilltype); i++)
    {
        int idx = v.abilities;
        v.skilltype[idx] = a_ptr->skilltype[i];
        v.abilitynum[idx] = a_ptr->abilitynum[i];
        v.bane_type[idx] = a_ptr->bane_type[i];
        v.abilities++;
    }

    object_kind* k_ptr = &k_info[k_idx];
    drop_category cat = drop_category_for_kind(k_ptr);
    if (cat == DROP_CAT_MAX)
        return;

    int rarity = (a_ptr->rarity > 0) ? a_ptr->rarity : 1;
    int level = (a_ptr->level > 0) ? a_ptr->level : 1;
    byte depth_arr[1] = {(byte)level};
    byte rarity_arr[1] = {(byte)rarity};
    add_drop_entry(&v, cat, DROP_GROUP_ARTIFACT, a_idx, level, MORGOTH_DEPTH,
        depth_arr, rarity_arr, 1);
}

static void clear_drop_entries(void)
{
    mem_free_null(g_drop_entries);
    g_drop_entries = NULL;
    g_drop_count = 0;
    g_drop_capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Raw file load/save                                                       */
/* ------------------------------------------------------------------------ */

static bool load_drop_raw(void)
{
    char path[1024];
    path_build(path, sizeof(path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));

    ang_file* fd = ang_file_open(path, "rb");
    if (!fd)
        return false;

    drop_raw_header hdr;
    if (sdl_read(fd, (char*)&hdr, sizeof(hdr)))
    {
        ang_file_close(fd);
        return false;
    }
    if (hdr.magic != DROP_RAW_MAGIC || hdr.version != DROP_RAW_VERSION)
    {
        ang_file_close(fd);
        return false;
    }

    size_t bytes = hdr.count * sizeof(drop_entry);
    drop_entry* buf = mem_alloc_array(hdr.count, drop_entry);
    if (sdl_read(fd, (char*)buf, bytes))
    {
        mem_free_null(buf);
        ang_file_close(fd);
        return false;
    }

    ang_file_close(fd);
    clear_drop_entries();
    g_drop_entries = buf;
    g_drop_count = hdr.count;
    return true;
}

static bool save_drop_raw(void)
{
    char path[1024];
    path_build(path, sizeof(path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));

    ang_file* fd = ang_file_open(path, "wb");
    if (!fd)
        return false;

    drop_raw_header hdr;
    hdr.magic = DROP_RAW_MAGIC;
    hdr.version = DROP_RAW_VERSION;
    hdr.count = (u32b)g_drop_count;

    bool ok = true;
    if (sdl_write(fd, (cptr)&hdr, sizeof(hdr)))
        ok = false;
    if (ok && sdl_write(fd, (cptr)g_drop_entries, g_drop_count * sizeof(drop_entry)))
        ok = false;

    ang_file_close(fd);
    return ok;
}

/* ------------------------------------------------------------------------ */
/* Public init                                                              */
/* ------------------------------------------------------------------------ */

void drop_system_init(void)
{
    /* Try to use cached raw if up to date */
#ifdef CHECK_MODIFICATION_TIME
    char raw_path[1024];
    char txt_path[1024];
    path_build(raw_path, sizeof(raw_path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));
    
    log_debug("drop_system_init: Checking modification times for drops.raw");
    log_debug("drop_system_init: raw_path='%s'", raw_path);
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "object.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    bool need_rebuild = check_modification_date_sdl(raw_path, txt_path) != 0;
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "special.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    need_rebuild |= (check_modification_date_sdl(raw_path, txt_path) != 0);
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "artifact.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    need_rebuild |= (check_modification_date_sdl(raw_path, txt_path) != 0);
    
    log_debug("drop_system_init: need_rebuild=%d", need_rebuild);
#else
    bool need_rebuild = true;
#endif

    if (!need_rebuild && load_drop_raw())
    {
        log_info("Loaded drop catalog from drops.raw (%zu entries)", g_drop_count);
        return;
    }

    clear_drop_entries();
    log_info("Rebuilding drop catalog...");

    /* Normal items */
    for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
        build_normal_variants(k_idx);

    /* Ego items */
    for (int e_idx = 1; e_idx < z_info->e_max; e_idx++)
        build_ego_variants(e_idx);

    /* Ego prefix+suffix combos */
    for (int p_idx = 1; p_idx < z_info->e_max; p_idx++)
    {
        ego_item_type* prefix_ptr = &e_info[p_idx];
        if (!prefix_ptr->tval[0])
            continue;
        if (!ego_name_is_prefix(e_name + prefix_ptr->name))
            continue;

        for (int s_idx = 1; s_idx < z_info->e_max; s_idx++)
        {
            ego_item_type* s_ptr = &e_info[s_idx];
            if (!s_ptr->tval[0])
                continue;
            if (ego_name_is_prefix(e_name + s_ptr->name))
                continue;

            build_ego_combo_variants(p_idx, s_idx);
        }
    }

    /* Artefacts */
    for (int a_idx = 1; a_idx < z_info->art_max; a_idx++)
        build_artifact_variants(a_idx);

    /* Log catalog size by category/group for diagnostics */
    size_t cat_counts[DROP_CAT_MAX] = { 0 };
    size_t group_kind_counts[3] = { 0 };
    for (size_t i = 0; i < g_drop_count; i++)
    {
        if (g_drop_entries[i].category < DROP_CAT_MAX)
            cat_counts[g_drop_entries[i].category]++;
        if (g_drop_entries[i].group_kind >= 0
            && g_drop_entries[i].group_kind <= 2)
            group_kind_counts[g_drop_entries[i].group_kind]++;
    }

    save_drop_raw();
    log_info("Drop catalog rebuilt: %zu entries (weapon=%zu armor=%zu jewelry=%zu supply=%zu | normal=%zu ego=%zu art=%zu)",
        g_drop_count, cat_counts[DROP_CAT_WEAPON], cat_counts[DROP_CAT_ARMOR],
        cat_counts[DROP_CAT_JEWELRY], cat_counts[DROP_CAT_SUPPLY],
        group_kind_counts[DROP_GROUP_NORMAL], group_kind_counts[DROP_GROUP_EGO],
        group_kind_counts[DROP_GROUP_ARTIFACT]);
}


/* Selection logic moved to drop-system-selection.c. */
