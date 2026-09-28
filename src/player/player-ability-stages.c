#include "angband.h"
#include "externs.h"
#include "init.h"
#include "log/log.h"

/* A stage keeps its original skill/ability identity, so combat, equipment and
 * saved learned abilities need no remapping. B: only changes Insight
 * progression. A stage may name several alternative parents; learning any
 * complete parent path is enough to unlock it. */
static int stage_parent_count_raw(const ability_type* ability)
{
    if (!ability || !ability->stage_cost)
        return 0;
    return ability->stage_parent_count;
}

static const ability_type* stage_parent_raw_at(const ability_type* ability,
    int parent_index)
{
    int count = stage_parent_count_raw(ability);

    if (parent_index < 0 || parent_index >= count || !b_info || !z_info)
        return NULL;
    for (int i = 0; i < z_info->b_max; ++i)
        if (b_info[i].name
            && b_info[i].skilltype == ability->stage_parent_skill[parent_index]
            && b_info[i].abilitynum == ability->stage_parent_ability[parent_index])
            return &b_info[i];
    return NULL;
}

static bool stage_chain_valid(const ability_type* node,
    const ability_type* seen[], int depth)
{
    if (!node || depth >= ABILITIES_MAX)
        return false;
    for (int i = 0; i < depth; ++i)
        if (seen[i] == node)
            return false;
    if (!node->stage_cost)
        return true;

    seen[depth] = node;
    int count = stage_parent_count_raw(node);
    if (count <= 0)
        return false;
    for (int i = 0; i < count; ++i)
    {
        const ability_type* parent = stage_parent_raw_at(node, i);
        if (!parent || parent->skilltype == S_SPC
            || !stage_chain_valid(parent, seen, depth + 1))
            return false;
    }
    return true;
}

errr ability_stages_validate(void)
{
    if (!b_info || !z_info) return PARSE_ERROR_GENERIC;
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* entry = &b_info[i];
        const ability_type* seen[ABILITIES_MAX] = {0};
        if (!entry->name || !entry->stage_cost) continue;
        if (!stage_chain_valid(entry, seen, 0))
        {
            log_error("Invalid ability stage chain for serial %d (missing parent, special-skill link or cycle).", i);
            return PARSE_ERROR_GENERIC;
        }
    }
    return 0;
}

bool ability_is_stage(const ability_type* ability)
{
    return insight_system_enabled() && ability && ability->stage_cost > 0;
}

int ability_stage_parent_count(const ability_type* ability)
{
    return ability_is_stage(ability) ? stage_parent_count_raw(ability) : 0;
}

const ability_type* ability_stage_parent_at(const ability_type* ability,
    int parent_index)
{
    return ability_is_stage(ability)
        ? stage_parent_raw_at(ability, parent_index) : NULL;
}

const ability_type* ability_stage_parent(const ability_type* ability)
{
    return ability_stage_parent_at(ability, 0);
}

bool ability_stage_has_parent(const ability_type* ability,
    const ability_type* parent)
{
    if (!ability_is_stage(ability) || !parent)
        return false;
    for (int i = 0; i < ability_stage_parent_count(ability); ++i)
        if (ability_stage_parent_at(ability, i) == parent)
            return true;
    return false;
}

bool ability_stage_shares_parent(const ability_type* a,
    const ability_type* b)
{
    if (!ability_is_stage(a) || !ability_is_stage(b))
        return false;
    for (int i = 0; i < ability_stage_parent_count(a); ++i)
        for (int j = 0; j < ability_stage_parent_count(b); ++j)
            if (ability_stage_parent_at(a, i) == ability_stage_parent_at(b, j))
                return true;
    return false;
}

static bool stage_path_met(const ability_type* node, int depth)
{
    if (!node || depth >= ABILITIES_MAX || !p_ptr)
        return false;
    if (!p_ptr->innate_ability[node->skilltype][node->abilitynum])
        return false;
    if (!node->stage_cost)
        return true;
    for (int i = 0; i < stage_parent_count_raw(node); ++i)
        if (stage_path_met(stage_parent_raw_at(node, i), depth + 1))
            return true;
    return false;
}

static const ability_type* stage_first_missing(const ability_type* node,
    int depth)
{
    if (!node || depth >= ABILITIES_MAX || !p_ptr)
        return node;
    if (!p_ptr->innate_ability[node->skilltype][node->abilitynum])
        return node;
    if (!node->stage_cost)
        return NULL;
    for (int i = 0; i < stage_parent_count_raw(node); ++i)
        if (stage_path_met(stage_parent_raw_at(node, i), depth + 1))
            return NULL;
    for (int i = 0; i < stage_parent_count_raw(node); ++i)
    {
        const ability_type* missing = stage_first_missing(
            stage_parent_raw_at(node, i), depth + 1);
        if (missing) return missing;
    }
    return node;
}

bool ability_stage_parent_path_met(const ability_type* ability,
    int parent_index)
{
    if (!ability_is_stage(ability))
        return false;
    return stage_path_met(stage_parent_raw_at(ability, parent_index), 0);
}

static int stage_depth_raw(const ability_type* ability, int depth)
{
    if (!ability || !ability->stage_cost)
        return 0;
    if (depth >= ABILITIES_MAX)
        return ABILITIES_MAX;
    int best = ABILITIES_MAX;
    for (int i = 0; i < stage_parent_count_raw(ability); ++i)
    {
        const ability_type* parent = stage_parent_raw_at(ability, i);
        int parent_depth = parent ? stage_depth_raw(parent, depth + 1) : ABILITIES_MAX;
        if (parent && parent_depth < best) best = parent_depth;
    }
    return best < ABILITIES_MAX ? best + 1 : 0;
}

int ability_stage_depth(const ability_type* ability)
{
    return ability_is_stage(ability) ? stage_depth_raw(ability, 0) : 0;
}

const ability_type* ability_stage_missing_parent(const ability_type* ability)
{
    if (!ability_is_stage(ability))
        return NULL;
    for (int i = 0; i < stage_parent_count_raw(ability); ++i)
        if (stage_path_met(stage_parent_raw_at(ability, i), 0))
            return NULL;
    return stage_first_missing(stage_parent_raw_at(ability, 0), 0);
}

static void stage_collect_ancestors(const ability_type* node,
    const ability_type* out[], int* count, int depth)
{
    if (!node || !node->stage_cost || depth >= ABILITIES_MAX)
        return;
    for (int i = 0; i < *count; ++i)
        if (out[i] == node)
            return;
    if (*count >= ABILITIES_MAX)
        return;
    out[(*count)++] = node;
    for (int i = 0; i < stage_parent_count_raw(node); ++i)
        stage_collect_ancestors(stage_parent_raw_at(node, i), out,
            count, depth + 1);
}

bool ability_stages_exclusive(const ability_type* a, const ability_type* b)
{
    const ability_type* a_nodes[ABILITIES_MAX] = {0};
    const ability_type* b_nodes[ABILITIES_MAX] = {0};
    int a_count = 0, b_count = 0;

    stage_collect_ancestors(a, a_nodes, &a_count, 0);
    stage_collect_ancestors(b, b_nodes, &b_count, 0);
    for (int i = 0; i < a_count; ++i)
        for (int j = 0; j < b_count; ++j)
            if (a_nodes[i] != b_nodes[j]
                && a_nodes[i]->stage_choice_group
                && a_nodes[i]->stage_choice_group == b_nodes[j]->stage_choice_group
                && ability_stage_shares_parent(a_nodes[i], b_nodes[j]))
                return true;
    return false;
}

const ability_type* ability_stage_conflict(const ability_type* ability)
{
    if (!ability_is_stage(ability) || !p_ptr || !b_info || !z_info) return NULL;
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* other = &b_info[i];
        if (other->name && p_ptr->innate_ability[other->skilltype][other->abilitynum]
            && ability_stages_exclusive(ability, other)) return other;
    }
    return NULL;
}

void ability_stage_activate(const ability_type* ability)
{
    if (!ability_is_stage(ability)) return;
    for (int i = 0; i < z_info->b_max; ++i)
        if (b_info[i].name && ability_stages_exclusive(ability, &b_info[i]))
            p_ptr->active_ability[b_info[i].skilltype][b_info[i].abilitynum] = false;
}

void ability_stages_normalize(void)
{
    if (!insight_system_enabled() || !b_info || !z_info || !p_ptr) return;
    /* An item cannot supply the alternative to an already learned choice. */
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* a = &b_info[i];
        if (!a->name || !a->stage_cost
            || !p_ptr->active_ability[a->skilltype][a->abilitynum]) continue;
        if (!p_ptr->innate_ability[a->skilltype][a->abilitynum]
            && ability_stage_conflict(a))
        {
            p_ptr->active_ability[a->skilltype][a->abilitynum] = false;
            continue;
        }
    }
    /* Preserve old learned abilities without allowing both exclusive effects.
     * If an old save/Insight-off character has both, keep the first active
     * stage; explicitly toggling the other on selects that line instead. */
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* a = &b_info[i];
        if (!a->name || !a->stage_cost
            || !p_ptr->active_ability[a->skilltype][a->abilitynum]) continue;
        for (int j = i + 1; j < z_info->b_max; ++j)
        {
            const ability_type* b = &b_info[j];
            if (b->name && b->stage_cost && ability_stages_exclusive(a, b))
                p_ptr->active_ability[b->skilltype][b->abilitynum] = false;
        }
    }
}
