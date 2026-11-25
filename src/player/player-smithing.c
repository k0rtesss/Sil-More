#include "angband.h"
#include "externs.h"

static int smithing_affinity_stat_bonus(void)
{
    if (!rp_ptr || !current_character_profile
        || !((rp_ptr->flags | current_character_profile->flags) & RHF_DWARVEN_SMITHING))
        return 0;

    int affinity = affinity_level(S_SMT);
    return MAX(0, affinity);
}

static int smithing_learned_stat_ability(int skill, int ability)
{
    return p_ptr->innate_ability[skill][ability]
        && p_ptr->active_ability[skill][ability];
}

/* Permanent attributes include the unconditional learned stat abilities, but
 * exclude equipment, temporary effects, drain, songs and conditional bonuses. */
static int smithing_permanent_stat(int stat)
{
    int value = p_ptr->stat_base[stat];
    if (stat == A_STR)
        value += smithing_learned_stat_ability(S_MEL, MEL_STR);
    else if (stat == A_DEX)
        value += smithing_learned_stat_ability(S_ARC, ARC_DEX)
            + smithing_learned_stat_ability(S_EVN, EVN_DEX)
            + smithing_learned_stat_ability(S_STL, STL_DEX);
    else if (stat == A_GRA)
        value += smithing_learned_stat_ability(S_PER, PER_GRA)
            + smithing_learned_stat_ability(S_SMT, SMT_GRA)
            + smithing_learned_stat_ability(S_SNG, SNG_GRA);
    return MAX(BASE_STAT_MIN, MIN(BASE_STAT_MAX, value));
}

/* Add all half-point contributions before rounding the total down. */
int smithing_stat_bonus(void)
{
    const byte* learned = p_ptr->innate_ability[S_SMT];
    const byte* active = p_ptr->active_ability[S_SMT];
    int dwarven_bonus = smithing_affinity_stat_bonus();
    int dex = smithing_permanent_stat(A_DEX) + dwarven_bonus;
    int gra = smithing_permanent_stat(A_GRA) + dwarven_bonus;
    int doubled = dex + gra;

    if (learned[SMT_ARMOURSMITH] && active[SMT_ARMOURSMITH])
        doubled += smithing_permanent_stat(A_STR) + dwarven_bonus;
    if (learned[SMT_WEAPONSMITH] && active[SMT_WEAPONSMITH])
        doubled += dex;
    if (learned[SMT_JEWELLER] && active[SMT_JEWELLER])
        doubled += gra;
    if (learned[SMT_ENCHANTMENT] && active[SMT_ENCHANTMENT])
        doubled += gra;
    if (learned[SMT_EXPERTISE] && active[SMT_EXPERTISE])
        doubled += dex;
    if (learned[SMT_ARTEFACT] && active[SMT_ARTEFACT])
        doubled += gra;

    if (learned[SMT_MASTERPIECE] && active[SMT_MASTERPIECE])
        doubled += gra;

    return (doubled - (doubled < 0 && doubled % 2 != 0)) / 2;
}
