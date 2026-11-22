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

int smithing_common_stat_bonus(void)
{
    return smithing_permanent_stat(A_DEX) + smithing_permanent_stat(A_GRA);
}

enum craft_category { CRAFT_HEAVY, CRAFT_JEWELLERY, CRAFT_LIGHT };

static enum craft_category smithing_stat_category(const object_kind* kind)
{
    /* Classify the base kind so affixes, alloys, and weight edits cannot
     * change which attribute the craft uses. */
    switch (kind->tval)
    {
    case TV_RING: case TV_AMULET: case TV_HORN: case TV_LIGHT:
        return CRAFT_JEWELLERY;
    case TV_MAIL: case TV_BOW: case TV_SOFT_ARMOR: case TV_CLOAK:
        return CRAFT_LIGHT;
    case TV_SHIELD:
        return kind->sval == SV_MITHRIL_SHIELD ? CRAFT_HEAVY : CRAFT_LIGHT;
    case TV_HAFTED:
        return kind->sval == SV_QUARTERSTAFF ? CRAFT_LIGHT : CRAFT_HEAVY;
    case TV_BOOTS:
        return (kind->sval == SV_PAIR_OF_LEATHER_BOOTS
                || kind->sval == SV_PAIR_OF_SHABBY_BOOTS)
            ? CRAFT_LIGHT : CRAFT_HEAVY;
    case TV_GLOVES:
        return kind->sval == SV_SET_OF_LEATHER_GLOVES ? CRAFT_LIGHT : CRAFT_HEAVY;
    default:
        return CRAFT_HEAVY;
    }
}

int smithing_item_skill(const object_type* object)
{
    int skill = p_ptr->skill_use[S_SMT];
    if (!object || !object->k_idx)
        return skill;

    switch (smithing_stat_category(&k_info[object->k_idx]))
    {
    case CRAFT_HEAVY: {
        int sum = smithing_permanent_stat(A_STR) + smithing_permanent_stat(A_DEX);
        /* The shared bonus is an integer. Floor the half-point term even
         * when the combined attributes are negative. */
        return skill + (sum - (sum < 0 && sum % 2 != 0)) / 2;
    }
    case CRAFT_JEWELLERY:
        return skill + smithing_permanent_stat(A_GRA);
    case CRAFT_LIGHT:
        return skill + smithing_permanent_stat(A_DEX);
    }
    return skill;
}
