#include "angband.h"
#include "externs.h"

/* Crafting and learning use the character's own lasting attributes. Neither
 * equipment, drain, potions, songs nor conditional combat bonuses enter here. */
int player_permanent_stat(int stat)
{
    static const int skill[] = { S_MEL, S_ARC, S_EVN, S_STL, S_PER,
        S_WIL, S_SMT, S_SNG };
    static const int ability[] = { MEL_STR, ARC_DEX, EVN_DEX, STL_DEX,
        PER_GRA, WIL_CON, SMT_GRA, SNG_GRA };
    static const int attribute[] = { A_STR, A_DEX, A_DEX, A_DEX,
        A_GRA, A_CON, A_GRA, A_GRA };
    int value;

    if (!p_ptr || stat < 0 || stat >= A_MAX) return 0;
    value = p_ptr->stat_base[stat];
    for (size_t i = 0; i < N_ELEMENTS(skill); ++i)
    {
        if (attribute[i] == stat && p_ptr->innate_ability[skill[i]][ability[i]]
            && p_ptr->active_ability[skill[i]][ability[i]])
            ++value;
    }
    return MAX(BASE_STAT_MIN, MIN(BASE_STAT_MAX, value));
}

int smithing_affinity_stat_bonus(void)
{
    u32b flags;

    if (!rp_ptr || !current_character_profile) return 0;
    flags = rp_ptr->flags | current_character_profile->flags;
    if (!(flags & RHF_DWARVEN_SMITHING)) return 0;
    return MAX(0, affinity_level(S_SMT));
}

int smithing_effective_stat(int stat)
{
    if (stat < 0 || stat >= A_MAX) return 0;
    return player_permanent_stat(stat) + smithing_affinity_stat_bonus();
}

int ability_required_skill(const ability_type* ability, int skill)
{
    if (!ability || skill < 0 || skill >= S_MAX) return 0;
    return MAX(ability->skill_req[skill],
        skill == ability->skilltype
            && !(insight_system_enabled() && ability->insight_branch)
            ? ability->level : 0);
}

bool ability_skill_requirements_met(const ability_type* ability)
{
    if (!ability || !p_ptr) return false;
    for (int skill = 0; skill < S_MAX; ++skill)
    {
        int need = ability_required_skill(ability, skill);
        if (need > 0 && p_ptr->skill_base[skill] < need)
            return false;
    }
    return true;
}

bool ability_stat_requirements_met(const ability_type* ability)
{
    if (!ability) return false;
    for (int i = 0; i < A_MAX; ++i)
        if (ability->stat_req[i] > 0
            && (ability->skilltype == S_SMT ? smithing_effective_stat(i)
                                            : player_permanent_stat(i))
                < ability->stat_req[i])
            return false;
    return true;
}

/* Fixed point coefficients copied from develop's S: format. Keep hundredths
 * until the final sum so fractional and negative contributions round once. */
int ability_stat_score_scaled(const ability_type* ability, bool permanent)
{
    int scaled = 0;
    if (!ability || !p_ptr) return 0;
    for (int i = 0; i < A_MAX; ++i)
        if (ability->stat_score_weight_set[i])
            scaled += ability->stat_score_weight[i]
                * (permanent
                    ? (ability->skilltype == S_SMT ? smithing_effective_stat(i)
                                                   : player_permanent_stat(i))
                    : p_ptr->stat_use[i]);
    return scaled;
}

static int score_floor(int scaled)
{
    return scaled >= 0 ? scaled / 100 : -((-scaled + 99) / 100);
}

int ability_score(int skilltype, int abilitynum)
{
    const ability_type* ability;
    int scaled;
    if (!p_ptr || skilltype < 0 || skilltype >= S_MAX
        || abilitynum < 0 || abilitynum >= ABILITIES_MAX) return 0;
    if (!z_info || !b_info) return p_ptr->skill_use[skilltype];
    ability = &b_info[ability_index(skilltype, abilitynum)];
    if (!ability->name || ability->skilltype != skilltype
        || ability->abilitynum != abilitynum || !ability->score_weights_set)
        return p_ptr->skill_use[skilltype];
    scaled = ability_stat_score_scaled(ability, skilltype == S_SMT);
    for (int i = 0; i < S_MAX; ++i)
        if (ability->skill_score_weight_set[i])
            scaled += ability->skill_score_weight[i]
                * (p_ptr->skill_base[i] + p_ptr->skill_equip_mod[i]
                    + p_ptr->skill_misc_mod[i]);
    return score_floor(scaled);
}

enum craft_category { CRAFT_HEAVY, CRAFT_MAIL, CRAFT_JEWELLERY, CRAFT_LIGHT };

static int smithing_stat_category(const object_type* object)
{
    int tval, sval;
    if (!object || !object->k_idx) return CRAFT_LIGHT;
    /* Use immutable base kind, never affix flags or the current weight. */
    tval = k_info[object->k_idx].tval;
    sval = k_info[object->k_idx].sval;
    switch (tval)
    {
    case TV_MAIL: return CRAFT_MAIL;
    case TV_RING: case TV_AMULET: case TV_HORN: case TV_LIGHT:
        return CRAFT_JEWELLERY;
    case TV_BOW: case TV_SOFT_ARMOR: case TV_CLOAK:
        return CRAFT_LIGHT;
    case TV_SHIELD:
        return sval == SV_MITHRIL_SHIELD ? CRAFT_HEAVY : CRAFT_LIGHT;
    case TV_HAFTED:
        return sval == SV_QUARTERSTAFF ? CRAFT_LIGHT : CRAFT_HEAVY;
    case TV_BOOTS:
        return (sval == SV_PAIR_OF_LEATHER_BOOTS || sval == SV_PAIR_OF_SHABBY_BOOTS)
            ? CRAFT_LIGHT : CRAFT_HEAVY;
    case TV_GLOVES:
        return sval == SV_SET_OF_LEATHER_GLOVES ? CRAFT_LIGHT : CRAFT_HEAVY;
    default: return CRAFT_HEAVY;
    }
}

cptr smithing_stat_category_name(const object_type* object)
{
    static cptr names[] = { "Heavy metal", "Mail", "Jewellery", "Light craft" };
    return names[smithing_stat_category(object)];
}

int smithing_mastery_stat_bonus_scaled(int abilitynum)
{
    if (!p_ptr || !b_info || !z_info || abilitynum < 0 || abilitynum >= ABILITIES_MAX
        || !p_ptr->active_ability[S_SMT][abilitynum]) return 0;
    if (insight_ability_upgrade_cost(S_SMT, abilitynum)
        && !p_ptr->insight_ability_upgraded[S_SMT][abilitynum]) return 0;
    return ability_stat_score_scaled(&b_info[ability_index(S_SMT, abilitynum)], true);
}

int smithing_common_stat_bonus_scaled(void)
{
    static const int masteries[] = { SMT_EXPERTISE, SMT_ENCHANTMENT, SMT_ARTEFACT };
    int scaled = 100 * (smithing_effective_stat(A_DEX) + smithing_effective_stat(A_GRA));
    for (size_t i = 0; i < N_ELEMENTS(masteries); ++i)
        scaled += smithing_mastery_stat_bonus_scaled(masteries[i]);
    return scaled;
}

int smithing_common_stat_bonus(void)
{
    return score_floor(smithing_common_stat_bonus_scaled());
}

int smithing_category_stat_bonus_scaled(const object_type* object)
{
    if (!object || !object->k_idx) return 0;
    switch (smithing_stat_category(object))
    {
    case CRAFT_HEAVY:
        return 50 * (smithing_effective_stat(A_STR) + smithing_effective_stat(A_DEX));
    case CRAFT_JEWELLERY: return 100 * smithing_effective_stat(A_GRA);
    default: return 100 * smithing_effective_stat(A_DEX);
    }
}

cptr smithing_category_stat_formula(const object_type* object)
{
    switch (smithing_stat_category(object))
    {
    case CRAFT_HEAVY: return "0.5 STR + 0.5 DEX";
    case CRAFT_JEWELLERY: return "GRA";
    default: return "DEX";
    }
}

int smithing_stat_bonus(const object_type* object)
{
    /* The sheet shows the common portion; crafting rounds the whole sum once. */
    return score_floor(smithing_common_stat_bonus_scaled()
        + smithing_category_stat_bonus_scaled(object));
}

int smithing_effective_skill(const object_type* object)
{
    return p_ptr->skill_base[S_SMT] + p_ptr->skill_equip_mod[S_SMT]
        + p_ptr->skill_misc_mod[S_SMT] + smithing_stat_bonus(object)
        + forge_bonus(p_ptr->py, p_ptr->px);
}
