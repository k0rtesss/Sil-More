#include "angband.h"
#include "externs.h"
#include "init.h"
#include "log/log.h"
#include "meta_state.h"

/* The saved skill/local pair is the ability's identity. Learning and display
 * may use a different skill without moving origin grants or item/save bits. */
int ability_learning_skill(const ability_type* ability)
{
    if (!ability) return S_SPC;
    return insight_reworked_enabled() && ability->policy_kind
        ? ability->policy_skill : ability->skilltype;
}

bool ability_policy_available(const ability_type* ability)
{
    if (!ability || !ability->name) return false;
    if (!insight_reworked_enabled()) return !ability->rework_only;
    return ability->policy_kind != ABILITY_POLICY_UNSET
        && ability->policy_kind != ABILITY_POLICY_RETIRED;
}

cptr ability_display_name(const ability_type* ability)
{
    if (!ability || !b_name) return "";
    return b_name + (insight_reworked_enabled() && ability->policy_name
        ? ability->policy_name : ability->name);
}

cptr ability_effect_text(const ability_type* ability)
{
    if (!ability || !b_text) return "";
    return b_text + (insight_reworked_enabled() && ability->policy_effect
        ? ability->policy_effect : ability->effect);
}

int ability_policy_xp_price(const ability_type* ability)
{
    if (!ability || ability->policy_kind != ABILITY_POLICY_XP) return 0;
    int skill = ability->policy_skill;
    int free = current_character_profile
        && (current_character_profile->flags & RHF_FREE) ? 1 : 0;
    int price = ability->policy_cost - 50 * affinity_level(skill) - 100 * free;
    if (skill == S_SNG) price -= 100 * minstrel_level();
    price += 100 * curse_flag_delta_cur(CUR_ABILITY_COST);
    return MAX(250, price);
}

int ability_policy_insight_price(const ability_type* ability)
{
    return ability && ability->policy_kind == ABILITY_POLICY_INSIGHT
        ? ability->policy_cost : 0;
}

static bool policy_knows(int skill, int ability)
{
    return p_ptr && skill >= 0 && skill < S_MAX && ability >= 0
        && ability < ABILITIES_MAX && p_ptr->innate_ability[skill][ability];
}

bool ability_policy_prerequisites_met(const ability_type* ability)
{
    if (!ability_policy_available(ability) || !p_ptr) return false;
    /* A gifted/learned node is an anchor: missing ancestors never create debt. */
    if (policy_knows(ability->skilltype, ability->abilitynum)) return true;
    for (int i = 0; i < ability->policy_and_count; ++i)
        if (!policy_knows(ability->policy_and_skill[i],
                ability->policy_and_ability[i])) return false;
    if (ability->policy_or_count)
    {
        bool met = false;
        for (int i = 0; i < ability->policy_or_count; ++i)
            if (policy_knows(ability->policy_or_skill[i],
                    ability->policy_or_ability[i])) met = true;
        if (!met) return false;
    }
    if (ability->skilltype == S_SNG && ability->abilitynum == SNG_WOVEN_THEMES)
    {
        int songs = 0;
        for (int i = 0; b_info && z_info && i < z_info->b_max; ++i)
        {
            const ability_type* song = &b_info[i];
            if (song->name && song->skilltype == S_SNG
                && song->abilitynum != SNG_WOVEN_THEMES
                && song->abilitynum != SNG_GRA
                && song->voice_cost && ability_policy_available(song)
                && policy_knows(S_SNG, song->abilitynum)) ++songs;
        }
        if (songs < 2) return false;
    }
    return true;
}

static const ability_type* policy_find(int skill, int ability)
{
    for (int i = 0; b_info && z_info && i < z_info->b_max; ++i)
        if (b_info[i].name && b_info[i].skilltype == skill
            && b_info[i].abilitynum == ability) return &b_info[i];
    return NULL;
}

static bool policy_chain_valid(const ability_type* ability, bool path[])
{
    if (!ability || !ability->policy_kind
        || ability->policy_kind == ABILITY_POLICY_RETIRED) return false;
    int key = ability->skilltype * ABILITIES_MAX + ability->abilitynum;
    if (key < 0 || key >= ABILITY_TIMELINE_MAX || path[key]) return false;
    path[key] = true;
    for (int and = 0; and < 2; ++and)
    {
        int count = and ? ability->policy_and_count : ability->policy_or_count;
        const byte* skills = and ? ability->policy_and_skill : ability->policy_or_skill;
        const byte* abilities = and ? ability->policy_and_ability : ability->policy_or_ability;
        for (int i = 0; i < count; ++i)
        {
            const ability_type* parent = policy_find(skills[i], abilities[i]);
            if (!parent || parent->skilltype == S_SPC
                || !policy_chain_valid(parent, path)) return false;
        }
    }
    path[key] = false;
    return true;
}

errr ability_policy_validate(void)
{
    if (!b_info || !z_info) return PARSE_ERROR_GENERIC;
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* ability = &b_info[i];
        if (!ability->name || !ability->policy_kind
            || ability->policy_kind == ABILITY_POLICY_RETIRED) continue;
        bool path[ABILITY_TIMELINE_MAX] = {0};
        if (ability->policy_skill >= S_MAX
            || ability->policy_level > BASE_SKILL_MAX
            || (ability->policy_kind == ABILITY_POLICY_XP
                && (ability->policy_cost < 250 || ability->policy_and_count
                    || ability->policy_or_count))
            || (ability->policy_kind == ABILITY_POLICY_INSIGHT
                && (ability->policy_cost < 1 || ability->policy_cost > 2))
            || ((ability->policy_kind == ABILITY_POLICY_EARNED)
                && ability->policy_cost)
            || !policy_chain_valid(ability, path))
        {
            log_error("Invalid new Insight policy for ability serial %d.", i);
            return PARSE_ERROR_GENERIC;
        }
    }
    return 0;
}

void ability_policy_normalize(void)
{
    if (!p_ptr || !b_info || !z_info) return;
    for (int i = 0; i < z_info->b_max; ++i)
    {
        const ability_type* ability = &b_info[i];
        if (!ability->name || ability_policy_available(ability)) continue;
        int skill = ability->skilltype, local = ability->abilitynum;
        if (skill >= S_MAX || local >= ABILITIES_MAX) continue;
        /* Preserve raw inherited/item identities but prevent effective powers. */
        p_ptr->have_ability[skill][local] = false;
        p_ptr->active_ability[skill][local] = false;
    }
}
