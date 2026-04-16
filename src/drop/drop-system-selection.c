#include "angband.h"
#include "drop/drop-system-internal.h"
#include "externs.h"
#include "gen-log.h"
#include "mem/alloc.h"

static const int DROP_MIN_DIFFICULTY = -15;

bool drop_allow_noble = false;
bool drop_allow_evil = false;
bool drop_allow_noble_from_quality = true;

typedef enum
{
    DROP_ALIGNMENT_FILTER_ANY = 0,
    DROP_ALIGNMENT_FILTER_NOBLE = 1,
    DROP_ALIGNMENT_FILTER_EVIL = 2
} drop_alignment_filter;

static bool drop_object_is_damaged(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags(o_ptr, &f1, &f2, &f3);
    return (f3 & TR3_DAMAGED) != 0;
}

static const char* drop_quality_name(drop_quality quality)
{
    switch (quality)
    {
    case DROP_QUALITY_ARTEFACT:
        return "artefact";
    case DROP_QUALITY_GOOD:
        return "good";
    case DROP_QUALITY_GREAT:
        return "great";
    case DROP_QUALITY_SUPERB:
        return "superb";
    case DROP_QUALITY_NORMAL:
    default:
        return "normal";
    }
}

static int drop_quality_bonus(drop_quality quality)
{
    switch (quality)
    {
    case DROP_QUALITY_ARTEFACT:
        return DROP_BONUS_ARTEFACT;
    case DROP_QUALITY_GOOD:
        return DROP_BONUS_GOOD;
    case DROP_QUALITY_GREAT:
        return DROP_BONUS_GREAT;
    case DROP_QUALITY_SUPERB:
        return DROP_BONUS_SUPERB;
    case DROP_QUALITY_NORMAL:
    default:
        return 0;
    }
}

/* Restore runtime quantities (fuel, charges, stacks) that were handled by apply_magic previously. */
static void drop_apply_spawn_quantities(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    switch (o_ptr->tval)
    {
    case TV_LIGHT:
    {
        if (o_ptr->timeout <= 0)
        {
            if (o_ptr->sval == SV_LIGHT_TORCH)
            {
                int spawn_fuel = 1000;
                int min_fuel = 250;
                o_ptr->timeout = one_in_(3) ? rand_range(min_fuel, spawn_fuel) : spawn_fuel;
            }
            else if (o_ptr->sval == SV_LIGHT_LANTERN)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(500, 3000) : 3000;
            }
            else if (o_ptr->sval == SV_LIGHT_MALLORN)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(30, 100) : 100;
            }
        }
        break;
    }
    case TV_STAFF:
    {
        int mult = CHANNELING_CHARGE_MULTIPLIER;
        switch (o_ptr->sval)
        {
        case SV_STAFF_SECRETS:
        case SV_STAFF_IMPRISONMENT:
        case SV_STAFF_FREEDOM:
        case SV_STAFF_LIGHT:
        case SV_STAFF_REVELATIONS:
        case SV_STAFF_FOES:
        case SV_STAFF_SLUMBER:
        case SV_STAFF_MAJESTY:
            o_ptr->pval = mult * damroll(4, 2);
            break;
        case SV_STAFF_SANCTITY:
        case SV_STAFF_UNDERSTANDING:
        case SV_STAFF_TREASURES:
        case SV_STAFF_SELF_KNOWLEDGE:
        case SV_STAFF_DISMAY:
        case SV_STAFF_RECHARGING:
            o_ptr->pval = mult * damroll(2, 2);
            break;
        case SV_STAFF_SUMMONING:
            o_ptr->pval = mult * damroll(6, 2);
            break;
        default:
            o_ptr->pval = mult * damroll(2, 2);
            break;
        }
        break;
    }
    case TV_GEM:
    {
        int charges = 0;
        switch (o_ptr->sval)
        {
        case SV_GEM_FREEDOM:
        case SV_GEM_LIGHT:
        case SV_GEM_REVELATIONS:
        case SV_GEM_FOES:
            charges = damroll(4, 2);
            break;
        case SV_GEM_SANCTITY:
        case SV_GEM_UNDERSTANDING:
        case SV_GEM_TREASURES:
        case SV_GEM_SELF_KNOWLEDGE:
        case SV_GEM_RECHARGING:
        case SV_GEM_SHADOWS:
            charges = damroll(2, 2);
            break;
        default:
            charges = damroll(2, 2);
            break;
        }
        o_ptr->number = charges;
        o_ptr->pval = 0;
        break;
    }
    default:
        break;
    }

    if ((k_ptr->flags3 & TR3_THROWING) && o_ptr->tval != TV_ARROW && !o_ptr->name1)
    {
        if (one_in_(2))
        {
            int stack_limit = object_stack_limit(o_ptr);
            int max_spawn = (stack_limit < 5) ? stack_limit : 5;
            int min_spawn = (max_spawn < 2) ? 1 : 2;
            o_ptr->number = rand_range(min_spawn, max_spawn);
        }
    }
}

typedef enum
{
    DROP_SUPPLY_POTION = 0,
    DROP_SUPPLY_HERB = 1,
    DROP_SUPPLY_GEM = 2,
    DROP_SUPPLY_STAFF = 3,
    DROP_SUPPLY_LIGHT = 4,
    DROP_SUPPLY_ARROWS = 5,
    DROP_SUPPLY_TUNNELING = 6,
    DROP_SUPPLY_GROUP_MAX = 7
} drop_supply_group_id;

typedef struct
{
    drop_category cat;
    u32b cat_mask;
    drop_quality quality;
    int depth;
    int legal_depth;
    int min_depth_penalty_depth;
    int difficulty_bonus;
    bool is_supply;
    int droptype;
    int base_roll;
    int lower;
    int upper;
    bool allow_artefacts;
    bool artefacts_only;
    bool allow_noble;
    bool allow_evil;
    bool allow_noble_from_quality;
    drop_alignment_filter alignment_filter;
    bool allow_damaged;
    int artefact_weight_multiplier;
    int noble_rarity_bonus;
    int cat_weights[DROP_CAT_MAX];
    int supply_weights[DROP_SUPPLY_GROUP_MAX];
} drop_request;

typedef struct
{
    s16b entry_indices[4096];
    int entry_count;
    drop_group_kind kind;
    s16b group_id;
    int rarity;
} drop_group;

static void drop_request_set_default_weights(drop_request* req)
{
    req->artefact_weight_multiplier = 1;
    req->noble_rarity_bonus = 0;
    req->allow_damaged = false;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
        req->cat_weights[i] = DROP_DEFAULT_CAT_WEIGHT;
    req->supply_weights[DROP_SUPPLY_POTION] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_HERB] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_GEM] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_STAFF] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_LIGHT] = DROP_DEFAULT_SUPPLY_WEIGHT;
    req->supply_weights[DROP_SUPPLY_ARROWS] = DROP_DEFAULT_SUPPLY_WEIGHT;
    req->supply_weights[DROP_SUPPLY_TUNNELING] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
}

static void drop_request_apply_profile(
    drop_request* req, const drop_profile* profile)
{
    drop_request_set_default_weights(req);
    if (!profile)
        return;

    req->cat_weights[DROP_CAT_WEAPON] = MAX(0, profile->weight_weapon);
    req->cat_weights[DROP_CAT_ARMOR] = MAX(0, profile->weight_armor);
    req->cat_weights[DROP_CAT_JEWELRY] = MAX(0, profile->weight_jewelry);
    req->cat_weights[DROP_CAT_SUPPLY] = MAX(0, profile->weight_supply);

    req->supply_weights[DROP_SUPPLY_POTION] = MAX(0, profile->supply_potion);
    req->supply_weights[DROP_SUPPLY_HERB] = MAX(0, profile->supply_herb);
    req->supply_weights[DROP_SUPPLY_GEM] = MAX(0, profile->supply_gem);
    req->supply_weights[DROP_SUPPLY_STAFF] = MAX(0, profile->supply_staff);
    req->supply_weights[DROP_SUPPLY_LIGHT] = MAX(0, profile->supply_light);
    req->supply_weights[DROP_SUPPLY_ARROWS] = MAX(0, profile->supply_arrows);
    req->supply_weights[DROP_SUPPLY_TUNNELING] = MAX(0, profile->supply_tunneling);
    req->allow_damaged = profile->allow_damaged;
}

static drop_supply_group_id supply_group_for_entry(const drop_entry* e)
{
    switch (e->obj.tval)
    {
    case TV_POTION:
        return DROP_SUPPLY_POTION;
    case TV_FOOD:
        return DROP_SUPPLY_HERB;
    case TV_GEM:
        return DROP_SUPPLY_GEM;
    case TV_STAFF:
        return DROP_SUPPLY_STAFF;
    case TV_LIGHT:
    case TV_FLASK:
        return DROP_SUPPLY_LIGHT;
    case TV_DIGGING:
        return DROP_SUPPLY_TUNNELING;
    case TV_ARROW:
        return DROP_SUPPLY_ARROWS;
    default:
        return DROP_SUPPLY_ARROWS;
    }
}

static int supply_entry_weight(const drop_entry* e, int depth)
{
    int diff = e->min_depth - depth;
    if (diff <= 0)
        return 10;
    int w = 10 - diff;
    if (w < 1)
        w = 1;
    return w;
}

static int drop_entry_pick_weight(const drop_entry* e, int base_rarity,
    int group_size, const drop_request* req)
{
    int weight = base_rarity;

    if (weight <= 0)
        return 0;

    if (e && req && e->noble && req->noble_rarity_bonus > 0 && group_size > 0)
        weight += group_size * req->noble_rarity_bonus;

    return weight;
}

static int drop_group_pick_bonus(const drop_group* grp, drop_entry* entries,
    const drop_request* req)
{
    int noble_count = 0;

    if (!grp || grp->entry_count <= 0)
        return 0;
    if (!req || req->noble_rarity_bonus <= 0)
        return 0;

    for (int i = 0; i < grp->entry_count; i++)
    {
        if (entries[grp->entry_indices[i]].noble)
            noble_count++;
    }

    return noble_count * req->noble_rarity_bonus;
}

static int drop_entry_rarity_at_depth(const drop_entry* e, int depth);
static int group_rarity_at_depth(const drop_entry* e, int depth);
static const drop_entry* find_drop_entry_for_object(const object_type* o_ptr,
    byte ego_prefix, byte ego_suffix);

static const drop_entry* find_drop_entry_for_object(const object_type* o_ptr,
    byte ego_prefix, byte ego_suffix)
{
    if (!o_ptr || !o_ptr->k_idx)
        return NULL;

    for (size_t i = 0; i < g_drop_count; i++)
    {
        const drop_entry* e = &g_drop_entries[i];
        if (e->obj.k_idx != o_ptr->k_idx)
            continue;
        if (e->obj.name1 != o_ptr->name1)
            continue;
        if (object_ego_prefix(&e->obj) != ego_prefix)
            continue;
        if (object_ego_suffix(&e->obj) != ego_suffix)
            continue;

        return e;
    }

    return NULL;
}

int object_weight_rarity(const object_type* o_ptr, int depth)
{
    if (!o_ptr || !o_ptr->k_idx)
        return 0;
    if (depth < 1)
        depth = 1;
    if (!g_drop_entries || g_drop_count == 0)
        return 0;

    byte ego_prefix = object_ego_prefix(o_ptr);
    byte ego_suffix = object_ego_suffix(o_ptr);
    const drop_entry* match = find_drop_entry_for_object(o_ptr, ego_prefix, ego_suffix);

    if (match)
        return group_rarity_at_depth(match, depth);

    return 0;
}

static const char* drop_category_name(drop_category cat)
{
    switch (cat)
    {
    case DROP_CAT_WEAPON:
        return "weapon";
    case DROP_CAT_ARMOR:
        return "armor";
    case DROP_CAT_JEWELRY:
        return "jewelry";
    case DROP_CAT_SUPPLY:
        return "supply";
    default:
        return "unknown";
    }
}

static drop_category roll_category(const drop_request* req)
{
    int weights[DROP_CAT_MAX];
    int total = 0;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
    {
        weights[i] = DROP_DEFAULT_CAT_WEIGHT;
        if (req)
            weights[i] = req->cat_weights[i];
        if (weights[i] < 0)
            weights[i] = 0;
        total += weights[i];
    }

    if (total <= 0)
    {
        for (int i = 0; i < DROP_CAT_MAX; ++i)
            weights[i] = DROP_DEFAULT_CAT_WEIGHT;
        total = DROP_DEFAULT_CAT_WEIGHT * DROP_CAT_MAX;
    }

    int roll = rand_int(total);
    int accum = 0;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
    {
        accum += weights[i];
        if (roll < accum)
            return (drop_category)i;
    }

    return DROP_CAT_SUPPLY;
}

static bool droptype_matches(const drop_request* req, const drop_entry* e)
{
    bool damaged = drop_object_is_damaged(&e->obj);

    if (damaged && req->droptype != DROP_TYPE_DAMAGED && !req->allow_damaged)
        return false;

    switch (req->droptype)
    {
    case DROP_TYPE_NOT_DAMAGED:
        return !damaged;
    case DROP_TYPE_DAMAGED:
        return damaged;
    case DROP_TYPE_EDGED:
        return e->obj.tval == TV_SWORD;
    case DROP_TYPE_POLEARM:
        return e->obj.tval == TV_POLEARM;
    case DROP_TYPE_BOW:
        return e->obj.tval == TV_BOW;
    case DROP_TYPE_DIGGING:
        return e->obj.tval == TV_DIGGING;
    case DROP_TYPE_SHIELD:
        return e->obj.tval == TV_SHIELD;
    case DROP_TYPE_ARMOR:
        return (e->obj.tval == TV_MAIL || e->obj.tval == TV_SOFT_ARMOR);
    case DROP_TYPE_BOOTS:
        return e->obj.tval == TV_BOOTS;
    case DROP_TYPE_CLOAK:
        return e->obj.tval == TV_CLOAK;
    case DROP_TYPE_GLOVES:
        return e->obj.tval == TV_GLOVES;
    case DROP_TYPE_HEADGEAR:
        return (e->obj.tval == TV_HELM || e->obj.tval == TV_CROWN);
    case DROP_TYPE_JEWELRY:
        return (e->obj.tval == TV_RING || e->obj.tval == TV_AMULET
            || e->obj.tval == TV_LIGHT);
    case DROP_TYPE_POTION:
        return e->obj.tval == TV_POTION;
    case DROP_TYPE_STAFF:
        return (e->obj.tval == TV_STAFF || e->obj.tval == TV_HORN
            || e->obj.tval == TV_GEM);
    case DROP_TYPE_SIMPLE_LIGHTS:
        return e->group_kind == DROP_GROUP_NORMAL
            && e->obj.tval == TV_LIGHT
            && (e->obj.sval == SV_LIGHT_TORCH || e->obj.sval == SV_LIGHT_MALLORN
                || e->obj.sval == SV_LIGHT_LANTERN);
    case DROP_TYPE_TORCHES:
        return e->obj.tval == TV_LIGHT
            && (e->obj.sval == SV_LIGHT_TORCH || e->obj.sval == SV_LIGHT_MALLORN
                || e->obj.sval == SV_LIGHT_LANTERN || e->obj.sval == SV_LIGHT_LESSER_JEWEL);
    default:
        return true;
    }
}

static bool collect_candidate_entries(
    const drop_request* req, bool relaxed, drop_entry** out, size_t* out_count)
{
    if (!g_drop_entries || g_drop_count == 0)
        return false;

    drop_entry* buf = mem_alloc_array(g_drop_count, drop_entry);
    size_t count = 0;
    int gen_depth = req->depth;
    int depth = req->legal_depth;
    int penalty_depth = req->min_depth_penalty_depth;

    int filter_artifact = 0, filter_droptype = 0, filter_category = 0;
    int filter_maxdepth = 0, filter_difficulty = 0, filter_total = 0;

    for (size_t i = 0; i < g_drop_count; i++)
    {
        drop_entry e = g_drop_entries[i];
        filter_total++;

        if (req->artefacts_only && e.group_kind != DROP_GROUP_ARTIFACT)
            continue;

        if (e.group_kind == DROP_GROUP_ARTIFACT)
        {
            if (!req->allow_artefacts || req->quality < DROP_QUALITY_GOOD)
            {
                filter_artifact++;
                continue;
            }

            artefact_type* a_ptr = &a_info[e.group_id];
            if (a_ptr->cur_num || (a_ptr->seen & ART_SEEN_PHYSICAL))
            {
                filter_artifact++;
                continue;
            }
            if ((a_ptr->flags3 & TR3_INSTA_ART) && e.group_id >= 20)
            {
                filter_artifact++;
                continue;
            }
        }

        if (req->alignment_filter == DROP_ALIGNMENT_FILTER_NOBLE && !e.noble)
            continue;

        if (req->alignment_filter == DROP_ALIGNMENT_FILTER_EVIL && !e.evil)
            continue;

        if (e.noble)
        {
            bool noble_allowed = req->allow_noble
                || ((object_generation_mode != OB_GEN_MODE_CHEST)
                    && req->allow_noble_from_quality
                    && req->quality >= DROP_QUALITY_GOOD);
            if (!noble_allowed)
                continue;
        }

        if (e.evil)
        {
            bool evil_allowed = req->allow_evil
                || ((object_generation_mode != OB_GEN_MODE_CHEST)
                    && req->quality >= DROP_QUALITY_GOOD);
            if (!evil_allowed)
                continue;
        }

        if (!droptype_matches(req, &e))
        {
            filter_droptype++;
            continue;
        }

        if ((req->cat_mask & (1U << e.category)) == 0)
        {
            filter_category++;
            continue;
        }

        if (e.max_depth > 0 && depth > e.max_depth)
        {
            filter_maxdepth++;
            if (filter_maxdepth <= 3 && gen_log_initialized && gen_depth >= 19)
            {
                gen_log_write("DROP_MAXDEPTH_REJECT",
                    "depth=%d item_maxdepth=%d k_idx=%d group_kind=%d",
                    gen_depth, e.max_depth, e.obj.k_idx, e.group_kind);
            }
            continue;
        }

        int rarity_weight = group_rarity_at_depth(&e, depth);
        if (rarity_weight <= 0)
            continue;

        int effective_dif = e.difficulty;
        if (penalty_depth < e.min_depth)
            effective_dif += 2 * (e.min_depth - penalty_depth);

        if (req->is_supply)
        {
            buf[count++] = e;
            continue;
        }

        if (!relaxed && (effective_dif < req->lower || effective_dif > req->upper))
        {
            filter_difficulty++;
            continue;
        }

        buf[count++] = e;
    }

    if (gen_log_initialized && gen_depth >= 19)
    {
        gen_log_write("DROP_FILTER",
            "depth=%d cat=%s relaxed=%s total=%d artifact_used=%d droptype=%d "
            "category=%d maxdepth=%d difficulty=%d passed=%zu",
            gen_depth, drop_category_name(req->cat), relaxed ? "yes" : "no",
            filter_total, filter_artifact, filter_droptype, filter_category,
            filter_maxdepth, filter_difficulty, count);
    }

    *out = buf;
    *out_count = count;

    if (gen_log_initialized && count > 0)
    {
        int samples = (count < 5) ? (int)count : 5;
        for (int i = 0; i < samples; i++)
        {
            drop_entry* e = &buf[i];
            int effective_dif = e->difficulty;
            if (penalty_depth < e->min_depth)
                effective_dif += 2 * (e->min_depth - penalty_depth);
            int rarity_at_depth = drop_entry_rarity_at_depth(e, depth);
            int weight_at_depth = group_rarity_at_depth(e, depth);

            gen_log_write("DROP_CANDIDATE",
                "relaxed=%s k_idx=%d cat=%s group_kind=%d group_id=%d "
                "base_dif=%d eff_dif=%d min_depth=%d max_depth=%d rarity_at_depth=%d weight=%d",
                relaxed ? "yes" : "no", e->obj.k_idx,
                drop_category_name(e->category), e->group_kind, e->group_id,
                e->difficulty, effective_dif, e->min_depth, e->max_depth,
                rarity_at_depth, weight_at_depth);
        }
    }

    return (count > 0);
}

static int drop_entry_rarity_at_depth(const drop_entry* e, int depth)
{
    if (!e || e->num_allocations == 0)
        return 1;

    int count = e->num_allocations;
    while (count > 1 && e->alloc_rarity[count - 1] == 0)
        count--;

    int rarity = e->alloc_rarity[0];
    for (int i = 1; i < count; i++)
    {
        if (depth >= e->alloc_depth[i])
            rarity = e->alloc_rarity[i];
        else
            break;
    }
    return rarity;
}

static int group_rarity_at_depth(const drop_entry* e, int depth)
{
    int rarity = drop_entry_rarity_at_depth(e, depth);
    if (rarity <= 0)
        return 0;
    return rarity;
}

static bool build_groups(drop_entry* entries, size_t count, drop_group* groups,
    int* group_count)
{
    int gcount = 0;
    for (size_t i = 0; i < count; i++)
    {
        drop_entry* e = &entries[i];
        bool found = false;
        for (int g = 0; g < gcount; g++)
        {
            drop_group* grp = &groups[g];
            if (grp->kind == e->group_kind && grp->group_id == e->group_id)
            {
                if (grp->entry_count
                    < (int)(sizeof(grp->entry_indices) / sizeof(grp->entry_indices[0])))
                    grp->entry_indices[grp->entry_count++] = (s16b)i;
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (gcount >= *group_count)
                break;
            drop_group* grp = &groups[gcount++];
            grp->kind = e->group_kind;
            grp->group_id = e->group_id;
            grp->entry_count = 0;
            grp->entry_indices[grp->entry_count++] = (s16b)i;
        }
    }
    *group_count = gcount;
    return (gcount > 0);
}

static drop_group* choose_group(drop_group* groups, int group_count,
    drop_entry* entries, int depth, const drop_request* req)
{
    if (group_count <= 0)
        return NULL;

    int* weights = mem_alloc_array(group_count, int);
    int total = 0;
    for (int i = 0; i < group_count; i++)
    {
        int entry_idx = groups[i].entry_indices[0];
        int w = group_rarity_at_depth(&entries[entry_idx], depth);
        int group_bonus = drop_group_pick_bonus(&groups[i], entries, req);
        if (w > 0)
            w += group_bonus;
        if (req && groups[i].kind == DROP_GROUP_ARTIFACT
            && req->artefact_weight_multiplier > 1)
        {
            w *= req->artefact_weight_multiplier;
        }
        weights[i] = w;
        total += w;
    }
    if (total <= 0)
    {
        mem_free_null(weights);
        return NULL;
    }
    int pick = rand_int(total);
    int accum = 0;
    int chosen = group_count - 1;
    for (int i = 0; i < group_count; i++)
    {
        accum += weights[i];
        if (pick < accum)
        {
            chosen = i;
            break;
        }
    }

    if (gen_log_initialized)
    {
        int samples = (group_count < 10) ? group_count : 10;
        for (int i = 0; i < samples; i++)
        {
            int entry_idx = groups[i].entry_indices[0];
            int weight = group_rarity_at_depth(&entries[entry_idx], depth);
            int group_bonus = drop_group_pick_bonus(&groups[i], entries, req);
            if (weight > 0)
                weight += group_bonus;
            if (req && groups[i].kind == DROP_GROUP_ARTIFACT
                && req->artefact_weight_multiplier > 1)
            {
                weight *= req->artefact_weight_multiplier;
            }
            gen_log_write("DROP_GROUP",
                "idx=%d kind=%d group_id=%d weight=%d total=%d "
                "entries=%d noble_bonus=%d chosen=%s",
                i, groups[i].kind, groups[i].group_id,
                weight, total,
                groups[i].entry_count, group_bonus,
                (i == chosen) ? "YES" : "no");
        }
        gen_log_write("DROP_GROUP_PICK",
            "pick=%d total=%d chosen_idx=%d", pick, total, chosen);
    }

    mem_free_null(weights);
    return &groups[chosen];
}

static drop_entry* choose_entry_from_group(drop_entry* entries,
    const drop_group* grp, int depth, const drop_request* req)
{
    int base_rarity;
    if (grp->entry_count <= 0)
        return NULL;

    base_rarity = group_rarity_at_depth(&entries[grp->entry_indices[0]], depth);

    int* weights = mem_alloc_array(grp->entry_count, int);
    int total = 0;
    int chosen_slot = grp->entry_count - 1;

    for (int i = 0; i < grp->entry_count; i++)
    {
        int entry_idx = grp->entry_indices[i];
        int weight = drop_entry_pick_weight(&entries[entry_idx], base_rarity,
            grp->entry_count, req);
        weights[i] = weight;
        total += weight;
    }

    if (total <= 0)
    {
        mem_free_null(weights);
        return &entries[grp->entry_indices[0]];
    }

    int pick = rand_int(total);
    for (int i = 0, acc = 0; i < grp->entry_count; i++)
    {
        acc += weights[i];
        if (pick < acc)
        {
            chosen_slot = i;
            break;
        }
    }
    drop_entry* chosen = &entries[grp->entry_indices[chosen_slot]];

    if (gen_log_initialized)
    {
        gen_log_write("DROP_ITEM_SELECT",
            "group_kind=%d group_id=%d entry_count=%d pick=%d total=%d "
            "chosen_slot=%d chosen_weight=%d noble=%s k_idx=%d att=%d ds=%d evn=%d ps=%d",
            grp->kind, grp->group_id, grp->entry_count, pick, total,
            chosen_slot, weights[chosen_slot], chosen->noble ? "yes" : "no",
            chosen->obj.k_idx, chosen->obj.att, chosen->obj.ds,
            chosen->obj.evn, chosen->obj.ps);
    }

    mem_free_null(weights);
    return chosen;
}

static drop_entry* choose_supply_entry(drop_entry* entries, size_t count,
    int depth, const drop_request* req)
{
    typedef struct
    {
        drop_entry** items;
        int count;
        int cap;
    } supply_bucket;

    supply_bucket buckets[DROP_SUPPLY_GROUP_MAX];
    int bucket_weights[DROP_SUPPLY_GROUP_MAX] = { 0 };
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        buckets[gid].items = NULL;
        buckets[gid].count = 0;
        buckets[gid].cap = 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        drop_entry* e = &entries[i];
        drop_supply_group_id gid = supply_group_for_entry(e);
        supply_bucket* b = &buckets[gid];
        if (b->count + 1 > b->cap)
        {
            int new_cap = (b->cap == 0) ? 64 : b->cap * 2;
            if (new_cap < b->count + 1)
                new_cap = b->count + 1;
            drop_entry** new_items
                = (drop_entry**)realloc(b->items, new_cap * sizeof(*new_items));
            if (!new_items)
            {
                for (int j = 0; j < DROP_SUPPLY_GROUP_MAX; j++)
                    mem_free_null(buckets[j].items);
                return NULL;
            }
            b->items = new_items;
            b->cap = new_cap;
        }
        b->items[b->count++] = e;
    }

    int total_group_weight = 0;
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        if (buckets[gid].count == 0)
            continue;
        int w = (req) ? req->supply_weights[gid] : DROP_DEFAULT_SUPPLY_WEIGHT;
        if (w <= 0)
            continue;
        bucket_weights[gid] = w;
        total_group_weight += w;
    }
    if (total_group_weight == 0)
    {
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int pick_group = rand_int(total_group_weight);
    int chosen_gid = DROP_SUPPLY_GROUP_MAX - 1;
    for (int gid = 0, acc = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        if (buckets[gid].count == 0)
            continue;
        acc += bucket_weights[gid];
        if (pick_group < acc)
        {
            chosen_gid = gid;
            break;
        }
    }

    supply_bucket* chosen_bucket = &buckets[chosen_gid];
    int* item_weights = mem_alloc_array(chosen_bucket->count, int);
    if (!item_weights)
    {
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int total_item_weight = 0;
    for (int i = 0; i < chosen_bucket->count; i++)
    {
        drop_entry* e = chosen_bucket->items[i];
        int rarity_weight = group_rarity_at_depth(e, depth);
        if (rarity_weight <= 0)
        {
            item_weights[i] = 0;
            continue;
        }
        int w = rarity_weight * supply_entry_weight(e, depth);
        item_weights[i] = w;
        total_item_weight += w;
    }
    if (total_item_weight <= 0)
    {
        mem_free_null(item_weights);
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int pick_item = rand_int(total_item_weight);
    drop_entry* chosen = chosen_bucket->items[chosen_bucket->count - 1];
    for (int i = 0, acc = 0; i < chosen_bucket->count; i++)
    {
        acc += item_weights[i];
        if (pick_item < acc)
        {
            chosen = chosen_bucket->items[i];
            break;
        }
    }

    mem_free_null(item_weights);
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
        mem_free_null(buckets[gid].items);
    return chosen;
}

static void log_drop_attempt(const drop_request* req, size_t strict_count,
    size_t relaxed_count, const drop_entry* chosen, bool used_relaxed,
    bool fallback)
{
    if (!gen_log_initialized)
        return;

    int effective_dif = -1;
    int a_idx = -1;
    int e_idx = -1;
    int group_kind = -1;
    if (chosen)
    {
        if (req->min_depth_penalty_depth < chosen->min_depth)
            effective_dif = chosen->difficulty
                + 2 * (chosen->min_depth - req->min_depth_penalty_depth);
        else
            effective_dif = chosen->difficulty;

        if (chosen->group_kind == DROP_GROUP_ARTIFACT)
            a_idx = chosen->group_id;
        else if (chosen->group_kind == DROP_GROUP_EGO)
            e_idx = chosen->group_id;
        group_kind = chosen->group_kind;
    }

    gen_log_write("DROP",
        "depth=%d cat=%s droptype=%d supply=%s target=%d band=%d..%d bonus=%d "
        "legal_depth=%d penalty_depth=%d cat_mask=0x%x "
        "strict=%zu relaxed=%zu used_relaxed=%s fallback=%s "
        "chosen_k=%d a_idx=%d e_idx=%d base_dif=%d eff_dif=%d min_depth=%d "
        "max_depth=%d rarity_at_depth=%d group_kind=%d",
        req->depth,
        chosen ? drop_category_name(chosen->category) : drop_category_name(req->cat),
        req->droptype,
        req->is_supply ? "yes" : "no", req->base_roll, req->lower, req->upper,
        req->difficulty_bonus, req->legal_depth, req->min_depth_penalty_depth,
        (unsigned)req->cat_mask, strict_count, relaxed_count,
        used_relaxed ? "yes" : "no", fallback ? "yes" : "no",
        chosen ? chosen->obj.k_idx : -1, a_idx, e_idx,
        chosen ? chosen->difficulty : -1, effective_dif,
        chosen ? chosen->min_depth : -1, chosen ? chosen->max_depth : -1,
        chosen ? group_rarity_at_depth(chosen, req->legal_depth) : 0, group_kind);
}

static int g_chest_vault_type = 0;
static int g_chest_mode = 0;
static int g_chest_material_wood_pct = -1;
static int g_chest_material_steel_pct = -1;
static int g_chest_material_jewel_pct = -1;

static void reset_chest_generation_context(void)
{
    g_chest_vault_type = 0;
    g_chest_mode = 0;
    g_chest_material_wood_pct = -1;
    g_chest_material_steel_pct = -1;
    g_chest_material_jewel_pct = -1;
}

static bool chest_has_custom_material_weights(void)
{
    return g_chest_material_wood_pct >= 0
        && g_chest_material_steel_pct >= 0
        && g_chest_material_jewel_pct >= 0;
}

static void normalize_chest_material_weights(
    int* wooden_pct, int* steel_pct, int* jewelled_pct)
{
    int total;

    if (!wooden_pct || !steel_pct || !jewelled_pct)
        return;

    if (*wooden_pct < 0)
        *wooden_pct = 0;
    if (*steel_pct < 0)
        *steel_pct = 0;
    if (*jewelled_pct < 0)
        *jewelled_pct = 0;

    total = *wooden_pct + *steel_pct + *jewelled_pct;
    if (total <= 0)
    {
        *wooden_pct = 50;
        *steel_pct = 35;
        *jewelled_pct = 15;
        return;
    }

    if (total > 100)
    {
        int overflow = total - 100;

        if (*jewelled_pct >= overflow)
            *jewelled_pct -= overflow;
        else if (*steel_pct >= overflow)
            *steel_pct -= overflow;
        else
            *wooden_pct = MAX(0, *wooden_pct - overflow);
    }
    else if (total < 100)
    {
        *jewelled_pct += (100 - total);
    }
}

static drop_quality chest_material_quality_for_index(int material_index)
{
    if (material_index <= 0)
        return DROP_QUALITY_GOOD;
    if (material_index == 1)
        return DROP_QUALITY_GREAT;
    return DROP_QUALITY_SUPERB;
}

static bool generate_chest(int depth, const drop_profile* profile, object_type* out)
{
    bool is_large;
    bool upgraded = false;
    if (g_chest_mode == 1)
        is_large = false;
    else if (g_chest_mode == 2)
        is_large = true;
    else
        is_large = one_in_(2);

    const int small_svals[] = {
        SV_CHEST_SMALL_WOODEN, SV_CHEST_SMALL_STEEL, SV_CHEST_SMALL_JEWELLED};
    const int large_svals[] = {
        SV_CHEST_LARGE_WOODEN, SV_CHEST_LARGE_STEEL, SV_CHEST_LARGE_JEWELLED};

    int material_roll = rand_int(100);
    int material_index;
    drop_quality material_quality;
    bool force_steel = p_ptr && (p_ptr->depth == 0);

    if (force_steel)
    {
        material_index = 1;
        material_quality = DROP_QUALITY_GREAT;
    }
    else if (chest_has_custom_material_weights())
    {
        int wooden_pct = g_chest_material_wood_pct;
        int steel_pct = g_chest_material_steel_pct;
        int jewelled_pct = g_chest_material_jewel_pct;

        normalize_chest_material_weights(&wooden_pct, &steel_pct, &jewelled_pct);

        if (material_roll < wooden_pct)
            material_index = 0;
        else if (material_roll < wooden_pct + steel_pct)
            material_index = 1;
        else
            material_index = 2;

        material_quality = chest_material_quality_for_index(material_index);
    }
    else if (g_chest_vault_type == -1)
    {
        material_index = 2;
        material_quality = DROP_QUALITY_SUPERB;
    }
    else
    {
        int wooden_pct, steel_pct;

        if (g_chest_vault_type == 6)
        {
            wooden_pct = 65;
            steel_pct = 35;
        }
        else if (g_chest_vault_type == 7)
        {
            wooden_pct = 35;
            steel_pct = 65;
        }
        else if (g_chest_vault_type == 8)
        {
            wooden_pct = 0;
            steel_pct = 0;
        }
        else if (g_chest_vault_type == 9)
        {
            wooden_pct = 0;
            steel_pct = 0;
        }
        else
        {
            wooden_pct = 50;
            steel_pct = 35;
        }

        int jewelled_pct = 100 - wooden_pct - steel_pct;
        int chest_delta = curse_flag_delta_cur(CUR_CHEST_WOOD);
        if (chest_delta != 0)
        {
            int shift = chest_delta * 20;
            int half = shift / 2;
            wooden_pct += shift;
            steel_pct -= half;
            jewelled_pct -= (shift - half);

            if (wooden_pct < 0)
                wooden_pct = 0;
            if (wooden_pct > 100)
                wooden_pct = 100;
            if (steel_pct < 0)
                steel_pct = 0;
            if (jewelled_pct < 0)
                jewelled_pct = 0;

            int total = wooden_pct + steel_pct + jewelled_pct;
            if (total > 100)
            {
                wooden_pct -= (total - 100);
                if (wooden_pct < 0)
                    wooden_pct = 0;
            }
            else if (total < 100)
            {
                jewelled_pct += (100 - total);
            }
        }

        if (material_roll < wooden_pct)
            material_index = 0;
        else if (material_roll < wooden_pct + steel_pct)
            material_index = 1;
        else
            material_index = 2;

        material_quality = chest_material_quality_for_index(material_index);
    }

    if (rand_int(100) < 10)
    {
        if (material_index < 2)
        {
            material_index++;
            upgraded = true;
        }
        else if (!is_large)
        {
            is_large = true;
            upgraded = true;
        }

        material_quality = chest_material_quality_for_index(material_index);
    }

    int chest_sval = is_large ? large_svals[material_index]
        : small_svals[material_index];
    int difficulty_bonus = drop_quality_bonus(material_quality);
    int k_idx = lookup_kind(TV_CHEST, chest_sval);
    if (!k_idx)
    {
        if (gen_log_initialized)
            gen_log_write("CHEST_ERROR", "Failed to find chest k_idx for sval=%d", chest_sval);
        reset_chest_generation_context();
        return false;
    }

    object_prep(out, k_idx);

    out->pval = depth;
    if (out->pval > 25)
        out->pval = 25;
    if (out->pval < 1)
        out->pval = 1;

    (void)profile;
    out->xtra1 = 0;

    if (gen_log_initialized)
    {
        gen_log_write("CHEST_GENERATED",
            "depth=%d vault_type=%d mode=%d size=%s material=%s quality=%s difficulty_bonus=%d chest_level=%d sval=%d upgraded=%s",
            depth, g_chest_vault_type, g_chest_mode, is_large ? "large" : "small",
            material_index == 0 ? "wooden" : (material_index == 1 ? "steel" : "jewelled"),
            drop_quality_name(material_quality), difficulty_bonus, out->pval,
            chest_sval, upgraded ? "yes" : "no");
    }

    reset_chest_generation_context();

    return true;
}

void drop_set_chest_vault_type(int vault_type)
{
    g_chest_vault_type = vault_type;
}

void drop_set_chest_mode(int mode)
{
    g_chest_mode = mode;
}

void drop_set_chest_material_weights(int wooden_pct, int steel_pct, int jewelled_pct)
{
    g_chest_material_wood_pct = wooden_pct;
    g_chest_material_steel_pct = steel_pct;
    g_chest_material_jewel_pct = jewelled_pct;
}

void drop_clear_chest_material_weights(void)
{
    g_chest_material_wood_pct = -1;
    g_chest_material_steel_pct = -1;
    g_chest_material_jewel_pct = -1;
}

bool drop_generate_object(int depth, drop_quality quality, int droptype,
    bool allow_artefacts, object_type* out)
{
    return drop_generate_object_profiled(
        depth, quality, droptype, 0, allow_artefacts, NULL, out);
}

static drop_entry* drop_try_pick(drop_request* req, int legal_depth,
    drop_entry** candidates, size_t* cand_count, size_t* strict_count,
    size_t* relaxed_count, bool relaxed, bool fallback)
{
    mem_free_null(*candidates);
    *candidates = NULL;
    *cand_count = 0;
    if (!relaxed)
        *strict_count = 0;
    if (relaxed_count)
        *relaxed_count = 0;

    if (!collect_candidate_entries(req, relaxed, candidates, cand_count))
    {
        if (relaxed)
        {
            if (relaxed_count)
                *relaxed_count = *cand_count;
        }
        else
        {
            *strict_count = *cand_count;
        }

        log_drop_attempt(req, *strict_count,
            relaxed_count ? *relaxed_count : 0, NULL, relaxed, fallback);
        return NULL;
    }

    if (relaxed)
    {
        if (relaxed_count)
            *relaxed_count = *cand_count;
    }
    else
    {
        *strict_count = *cand_count;
    }

    drop_entry* chosen = NULL;
    if (*cand_count > 0)
    {
        if (req->is_supply)
        {
            chosen = choose_supply_entry(*candidates, *cand_count, legal_depth, req);
        }
        else
        {
            drop_group* groups = mem_alloc_array(*cand_count, drop_group);
            int group_cap = (int)(*cand_count);
            int group_count = group_cap;
            if (build_groups(*candidates, *cand_count, groups, &group_count))
            {
                drop_group* grp = choose_group(groups, group_count, *candidates, legal_depth, req);
                if (grp)
                    chosen = choose_entry_from_group(*candidates, grp, legal_depth, req);
            }
            mem_free_null(groups);
        }
    }

    log_drop_attempt(req, *strict_count,
        relaxed_count ? *relaxed_count : 0, chosen, relaxed, fallback);
    return chosen;
}

static void drop_apply_chosen_entry(const drop_entry* chosen, int depth,
    object_type* out)
{
    object_wipe(out);
    object_copy(out, &chosen->obj);

    object_refresh_weight(out);

    drop_apply_spawn_quantities(out);
    if (drop_object_is_damaged(out))
    {
        object_aware(out);
        object_known(out);
    }

    if (chosen->group_kind == DROP_GROUP_ARTIFACT)
    {
        artefact_type* a_ptr = &a_info[chosen->group_id];
        if (!a_ptr->cur_num)
            a_ptr->cur_num = 1;
    }
    if (out->tval == TV_ARROW && !artefact_p(out))
    {
        int depth_adjust = MORGOTH_DEPTH - depth;
        out->number = 20 + damroll(1, 10 + MAX(0, depth_adjust));
        if (out->number > 48)
            out->number = 48;
    }
}

static bool drop_generate_object_internal(int depth, drop_quality quality,
    int min_depth_penalty_depth, int droptype, int extra_bonus, bool allow_artefacts,
    int artefact_weight_multiplier, bool artefacts_only,
    const drop_profile* profile, drop_alignment_filter alignment_filter,
    object_type* out)
{
    if (min_depth_penalty_depth < 1)
        min_depth_penalty_depth = 1;

    if (droptype == DROP_TYPE_CHEST)
        return generate_chest(depth, profile, out);

    drop_request req = { 0 };
    drop_request_apply_profile(&req, profile);
    int gen_depth = depth;
    int legal_depth = gen_depth;
    if (p_ptr)
    {
        int current_depth = player_generation_depth();
        if (legal_depth > current_depth)
            legal_depth = current_depth;
    }

    req.depth = gen_depth;
    req.quality = quality;
    req.legal_depth = legal_depth;
    req.min_depth_penalty_depth = min_depth_penalty_depth;
    req.difficulty_bonus = extra_bonus + drop_quality_bonus(quality);
    req.is_supply = false;
    req.droptype = droptype;
    req.allow_artefacts = allow_artefacts;
    req.artefacts_only = artefacts_only;
    req.allow_noble = drop_allow_noble;
    req.allow_evil = drop_allow_evil;
    req.allow_noble_from_quality = drop_allow_noble_from_quality;
    req.alignment_filter = alignment_filter;
    req.artefact_weight_multiplier
        = (allow_artefacts && artefact_weight_multiplier > 1)
        ? artefact_weight_multiplier
        : 1;
    if (req.artefact_weight_multiplier > 100)
        req.artefact_weight_multiplier = 100;
    req.noble_rarity_bonus = 0;
    if (object_generation_mode == OB_GEN_MODE_CHEST && req.allow_noble)
        req.noble_rarity_bonus = DROP_CHEST_NOBLE_RARITY_BONUS;

    bool chest_profile_allows_supply = (object_generation_mode == OB_GEN_MODE_CHEST)
        && profile && (profile->weight_supply > 0);
    bool disallow_supply = (quality > DROP_QUALITY_NORMAL)
        || (object_generation_mode == OB_GEN_MODE_CHEST);
    if (chest_profile_allows_supply)
        disallow_supply = false;
    if (disallow_supply)
    {
        req.cat_weights[DROP_CAT_SUPPLY] = 0;
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 0;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 0;

        if (req.cat_weights[DROP_CAT_WEAPON] <= 0
            && req.cat_weights[DROP_CAT_ARMOR] <= 0
            && req.cat_weights[DROP_CAT_JEWELRY] <= 0)
        {
            req.cat_weights[DROP_CAT_WEAPON] = DROP_DEFAULT_CAT_WEIGHT;
            req.cat_weights[DROP_CAT_ARMOR] = DROP_DEFAULT_CAT_WEIGHT;
            req.cat_weights[DROP_CAT_JEWELRY] = DROP_DEFAULT_CAT_WEIGHT;
        }
    }

    int sides = 25 + (3 * legal_depth) / 4;
    if (sides < 1)
        sides = 1;
    int roll1 = dieroll(sides);
    int roll2 = dieroll(sides);
    int min_roll = MIN(roll1, roll2);
    int base_calc = (int)(1.25 * legal_depth) - 19 + min_roll;
    req.base_roll = base_calc + req.difficulty_bonus;
    req.lower = req.base_roll - 2;
    req.upper = req.base_roll + 2;
    req.cat_mask = 0;
    bool negative_target = (req.base_roll < 0);

    if (gen_log_initialized)
    {
        gen_log_write("DROP_TARGET",
            "depth=%d legal_depth=%d min_penalty_depth=%d quality=%s bonus=%d art_mult=%d noble_bonus=%d sides=%d roll1=%d roll2=%d min=%d "
            "base_calc=%d target=%d band=%d..%d",
            depth, legal_depth, min_depth_penalty_depth, drop_quality_name(quality),
            req.difficulty_bonus, req.artefact_weight_multiplier,
            req.noble_rarity_bonus, sides, roll1, roll2, min_roll,
            base_calc, req.base_roll, req.lower, req.upper);
    }

    switch (droptype)
    {
    case DROP_TYPE_WEAPON:
    case DROP_TYPE_EDGED:
    case DROP_TYPE_POLEARM:
    case DROP_TYPE_BOW:
        req.cat = DROP_CAT_WEAPON;
        break;
    case DROP_TYPE_DIGGING:
        if (disallow_supply)
            return false;
        req.cat = DROP_CAT_SUPPLY;
        req.is_supply = true;
        break;
    case DROP_TYPE_ARMOR:
    case DROP_TYPE_SHIELD:
    case DROP_TYPE_BOOTS:
    case DROP_TYPE_CLOAK:
    case DROP_TYPE_GLOVES:
    case DROP_TYPE_HEADGEAR:
        req.cat = DROP_CAT_ARMOR;
        break;
    case DROP_TYPE_JEWELRY:
        req.cat = DROP_CAT_JEWELRY;
        break;
    case DROP_TYPE_POTION:
    case DROP_TYPE_STAFF:
    case DROP_TYPE_SIMPLE_LIGHTS:
    case DROP_TYPE_TORCHES:
        if (disallow_supply)
            return false;
        req.cat = DROP_CAT_SUPPLY;
        req.is_supply = true;
        break;
    case DROP_TYPE_DAMAGED:
        req.cat = DROP_CAT_ARMOR;
        req.cat_mask = (1U << DROP_CAT_WEAPON) | (1U << DROP_CAT_ARMOR);
        break;
    default:
        req.cat = roll_category(&req);
        req.cat_mask = (1U << req.cat);
        break;
    }
    if (req.cat == DROP_CAT_SUPPLY)
        req.is_supply = true;
    if (req.cat_mask == 0)
        req.cat_mask = (1U << req.cat);

    if (droptype == DROP_TYPE_TORCHES || droptype == DROP_TYPE_SIMPLE_LIGHTS)
    {
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 100;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 0;
    }

    if (droptype == DROP_TYPE_DIGGING)
    {
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 0;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 100;
    }

    if (!req.is_supply && req.upper < DROP_MIN_DIFFICULTY
        && droptype != DROP_TYPE_DAMAGED)
    {
        if (gen_log_initialized)
        {
            gen_log_write("DROP_SKIP",
                "depth=%d droptype=%d target=%d band=%d..%d (upper<%d)",
                depth, droptype, req.base_roll, req.lower, req.upper,
                DROP_MIN_DIFFICULTY);
        }
        return false;
    }

    drop_entry* candidates = NULL;
    size_t cand_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    drop_entry* chosen = NULL;

    bool partition_driven_cat = false;
    switch (droptype)
    {
    case DROP_TYPE_WEAPON:
    case DROP_TYPE_EDGED:
    case DROP_TYPE_POLEARM:
    case DROP_TYPE_BOW:
    case DROP_TYPE_DIGGING:
    case DROP_TYPE_ARMOR:
    case DROP_TYPE_SHIELD:
    case DROP_TYPE_BOOTS:
    case DROP_TYPE_CLOAK:
    case DROP_TYPE_GLOVES:
    case DROP_TYPE_HEADGEAR:
    case DROP_TYPE_JEWELRY:
    case DROP_TYPE_POTION:
    case DROP_TYPE_STAFF:
    case DROP_TYPE_SIMPLE_LIGHTS:
    case DROP_TYPE_TORCHES:
    case DROP_TYPE_NOT_DAMAGED:
    case DROP_TYPE_DAMAGED:
        partition_driven_cat = false;
        break;
    default:
        partition_driven_cat = true;
        break;
    }

    chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
        &strict_count, &relaxed_count, false, false);

    if (!negative_target && !chosen && !req.is_supply && partition_driven_cat
        && (req.cat == DROP_CAT_WEAPON || req.cat == DROP_CAT_ARMOR
            || req.cat == DROP_CAT_JEWELRY))
    {
        drop_category cats[3] = { DROP_CAT_WEAPON, DROP_CAT_ARMOR, DROP_CAT_JEWELRY };
        for (int i = 0; i < 3; i++)
        {
            for (int j = i + 1; j < 3; j++)
            {
                int wi = MAX(0, req.cat_weights[cats[i]]);
                int wj = MAX(0, req.cat_weights[cats[j]]);
                if (wj > wi)
                {
                    drop_category tmp = cats[i];
                    cats[i] = cats[j];
                    cats[j] = tmp;
                }
            }
        }

        for (int i = 0; i < 3 && !chosen; i++)
        {
            drop_category cat = cats[i];
            int w = MAX(0, req.cat_weights[cat]);
            if (w <= 0)
                continue;
            if (req.cat_mask & (1U << cat))
                continue;

            req.cat_mask |= (1U << cat);
            chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
                &strict_count, &relaxed_count, false, true);
        }
    }

    while (!negative_target && !chosen && !req.is_supply
        && req.lower > DROP_MIN_DIFFICULTY)
    {
        req.lower--;
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, false, true);
    }

    if (req.artefacts_only && !chosen && !req.is_supply)
    {
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, true, true);
    }

    bool ok = (chosen != NULL);

    if (!ok && gen_log_initialized)
    {
        gen_log_write("DROP_FAILED",
            "depth=%d cat=%d droptype=%d target=%d - no valid items after retries",
            depth, req.cat, droptype, req.base_roll);
    }

    if (ok)
        drop_apply_chosen_entry(chosen, depth, out);

    mem_free_null(candidates);
    return ok;
}

bool drop_generate_object_with_bonus(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, depth, droptype, extra_bonus, allow_artefacts, 1, false,
        NULL, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, depth, droptype, extra_bonus, allow_artefacts, 1, false,
        profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_with_bonus_depths(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, 1, false, NULL,
        DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled_depths(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, const drop_profile* profile,
    object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, 1, false, profile,
        DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled_depths_biased(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, int artefact_weight_multiplier,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, artefact_weight_multiplier,
        false, profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_guaranteed_artefact(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, 0, true, 1, true, profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_chasm_sanctum_object(int depth, object_type* out)
{
    drop_request req = { 0 };
    drop_entry* candidates = NULL;
    size_t cand_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    drop_entry* chosen = NULL;
    int legal_depth;
    int penalty_depth;
    bool allow_artefacts = !(adult_no_artefacts || birth_no_artefacts);

    if (!out)
        return false;

    if (depth < 1)
        depth = 1;

    legal_depth = depth;
    penalty_depth = depth + 5;

    if (p_ptr)
    {
        int current_depth = player_generation_depth();
        if (legal_depth > current_depth)
            legal_depth = current_depth;
    }

    drop_request_apply_profile(&req, NULL);
    req.depth = depth;
    req.quality = DROP_QUALITY_SUPERB;
    req.cat = DROP_CAT_WEAPON;
    req.cat_mask = (1U << DROP_CAT_WEAPON)
        | (1U << DROP_CAT_ARMOR)
        | (1U << DROP_CAT_JEWELRY);
    req.legal_depth = legal_depth;
    req.min_depth_penalty_depth = penalty_depth;
    req.difficulty_bonus = DROP_BONUS_SUPERB;
    req.is_supply = false;
    req.droptype = DROP_TYPE_UNTHEMED;
    req.allow_artefacts = allow_artefacts;
    req.artefacts_only = true;
    req.allow_noble = false;
    req.allow_evil = true;
    req.allow_noble_from_quality = false;
    req.alignment_filter = DROP_ALIGNMENT_FILTER_EVIL;
    req.artefact_weight_multiplier = 1;
    req.noble_rarity_bonus = 0;

    {
        int sides = 25 + (3 * legal_depth) / 4;
        int roll1;
        int roll2;
        int min_roll;
        int base_calc;

        if (sides < 1)
            sides = 1;
        roll1 = dieroll(sides);
        roll2 = dieroll(sides);
        min_roll = MIN(roll1, roll2);
        base_calc = (int)(1.25 * legal_depth) - 19 + min_roll;
        req.base_roll = base_calc + req.difficulty_bonus;
        req.lower = req.base_roll - 5;
        req.upper = req.base_roll + 5;

        if (gen_log_initialized)
        {
            gen_log_write("DROP_SANCTUM",
                "artefact_pass depth=%d legal_depth=%d target=%d band=%d..%d sides=%d roll1=%d roll2=%d min=%d",
                depth, legal_depth, req.base_roll, req.lower, req.upper,
                sides, roll1, roll2, min_roll);
        }
    }

    if (allow_artefacts && req.upper >= DROP_MIN_DIFFICULTY)
    {
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, false, false);
    }

    if (chosen)
    {
        drop_apply_chosen_entry(chosen, depth, out);
        mem_free_null(candidates);
        return true;
    }

    mem_free_null(candidates);

    if (gen_log_initialized)
    {
        gen_log_write("DROP_SANCTUM",
            "fallback_pass depth=%d legal_depth=%d penalty_depth=%d quality=%s alignment=evil_only",
            depth, legal_depth, penalty_depth,
            drop_quality_name(DROP_QUALITY_SUPERB));
    }

    return drop_generate_object_internal(depth, DROP_QUALITY_SUPERB, penalty_depth,
        DROP_TYPE_UNTHEMED, 0, allow_artefacts, 1, false, NULL,
        DROP_ALIGNMENT_FILTER_EVIL, out);
}
