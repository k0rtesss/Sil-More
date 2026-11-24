#include "angband.h"
#include "externs.h"

/* Crafts use base attributes and learned attribute abilities. Equipment,
 * stat drain, potions, songs, and conditional bonuses do not enter here. */
static int smithing_permanent_stat(int stat)
{
    static const struct {
        int stat;
        int skill;
        int ability;
    } bonuses[] = {
        { A_STR, S_MEL, MEL_STR },
        { A_DEX, S_ARC, ARC_DEX },
        { A_DEX, S_EVN, EVN_DEX },
        { A_DEX, S_STL, STL_DEX },
        { A_GRA, S_PER, PER_GRA },
        { A_GRA, S_SMT, SMT_GRA },
        { A_GRA, S_SNG, SNG_GRA },
    };
    int value = p_ptr->stat_base[stat];

    for (size_t i = 0; i < N_ELEMENTS(bonuses); i++)
    {
        int skill = bonuses[i].skill;
        int ability = bonuses[i].ability;
        if (bonuses[i].stat == stat && p_ptr->innate_ability[skill][ability]
            && p_ptr->active_ability[skill][ability])
            value++;
    }
    return MAX(BASE_STAT_MIN, MIN(BASE_STAT_MAX, value));
}

static int smithing_affinity_stat_bonus(void)
{
    if (!rp_ptr || !current_character_profile
        || !((rp_ptr->flags | current_character_profile->flags) & RHF_DWARVEN_SMITHING))
        return 0;

    int affinity = affinity_level(S_SMT);
    return MAX(0, affinity);
}

/* Capacity is shared by all recipes. Keep half-points until the full
 * attribute and ability contribution can be rounded down together. */
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
        doubled += 2 * gra;
    if (learned[SMT_EXPERTISE] && active[SMT_EXPERTISE])
        doubled += 2 * dex;
    if (learned[SMT_ARTEFACT] && active[SMT_ARTEFACT])
        doubled += 2 * gra;

    return (doubled - (doubled < 0 && doubled % 2 != 0)) / 2;
}
