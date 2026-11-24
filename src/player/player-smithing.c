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

/* All recipes use current attributes. Add the ability contributions before
 * rounding the total down so half-points can combine. */
int smithing_stat_bonus(void)
{
    const byte* learned = p_ptr->innate_ability[S_SMT];
    const byte* active = p_ptr->active_ability[S_SMT];
    int dwarven_bonus = smithing_affinity_stat_bonus();
    int dex = p_ptr->stat_use[A_DEX] + dwarven_bonus;
    int gra = p_ptr->stat_use[A_GRA] + dwarven_bonus;
    int doubled = dex + gra;

    if (learned[SMT_ARMOURSMITH] && active[SMT_ARMOURSMITH])
        doubled += p_ptr->stat_use[A_STR] + dwarven_bonus;
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
