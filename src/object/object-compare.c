#include "angband.h"
#include "ui/ui-language.h"
#include "externs.h"
#include "object/object-compare.h"
#include "object/object-info.h"
#include "object/object-flags.h"
#include "player/player-upkeep-internal.h"
#include "metarun.h"

typedef struct comparison_item
{
    const object_type* object;
    u32b flags[4];
    bool known;
} comparison_item;

typedef struct comparison_property
{
    int set;
    u32b flag;
    cptr name;
    bool drawback;
} comparison_property;

/* Mechanical properties only: identity, alignment, generation and smithing
 * flags are deliberately absent. Signs describe the item's contribution,
 * rather than promising a change to the player's final derived statistics. */
static const comparison_property comparison_properties[] = {
    { 0, TR1_VAMPIRIC, "vampirism", false },
    { 0, TR1_SLAY_MAN_OR_ELF, "slay humans/elves", false },
    { 0, TR1_SLAY_ORC, "slay orcs", false },
    { 0, TR1_SLAY_TROLL, "slay trolls", false },
    { 0, TR1_SLAY_WOLF, "slay wolves", false },
    { 0, TR1_SLAY_SPIDER, "slay spiders", false },
    { 0, TR1_SLAY_UNDEAD, "slay undead", false },
    { 0, TR1_SLAY_RAUKO, "slay raukar", false },
    { 0, TR1_SLAY_DRAGON, "slay dragons", false },
    { 0, TR1_BRAND_COLD, "cold brand", false },
    { 0, TR1_BRAND_FIRE, "fire brand", false },
    { 0, TR1_BRAND_ELEC, "lightning brand", false },
    { 0, TR1_BRAND_POIS, "poison brand", false },
    { 1, TR2_SUST_STR, "sustain Str", false },
    { 1, TR2_SUST_DEX, "sustain Dex", false },
    { 1, TR2_SUST_CON, "sustain Con", false },
    { 1, TR2_SUST_GRA, "sustain Gra", false },
    { 1, TR2_RES_COLD, "cold resistance", false },
    { 1, TR2_RES_FIRE, "fire resistance", false },
    { 1, TR2_RES_ELEC, "lightning resistance", false },
    { 1, TR2_RES_POIS, "poison resistance", false },
    { 1, TR2_RES_BLEED, "bleeding resistance", false },
    { 1, TR2_RES_FEAR, "fear resistance", false },
    { 1, TR2_RES_BLIND, "blindness resistance", false },
    { 1, TR2_RES_CONFU, "confusion resistance", false },
    { 1, TR2_RES_STUN, "stun resistance", false },
    { 1, TR2_RES_HALLU, "hallucination resistance", false },
    { 1, TR2_RADIANCE, "radiance", false },
    { 1, TR2_SLOW_DIGEST, "slow digestion", false },
    { 1, TR2_LIGHT, "light", false },
    { 1, TR2_REGEN, "regeneration", false },
    { 1, TR2_SEE_INVIS, "see invisible", false },
    { 1, TR2_FREE_ACT, "free action", false },
    { 1, TR2_SPEED, "speed", false },
    { 1, TR2_TRAITOR, "betrayal", true },
    { 1, TR2_FEAR, "fear on injury", true },
    { 1, TR2_HUNGER, "hunger", true },
    { 1, TR2_DARKNESS, "darkness", true },
    { 1, TR2_SLOWNESS, "slowness", true },
    { 1, TR2_DANGER, "danger", true },
    { 1, TR2_AGGRAVATE, "aggravation", true },
    { 1, TR2_HAUNTED, "haunting", true },
    { 1, TR2_VUL_COLD, "cold vulnerability", true },
    { 1, TR2_VUL_FIRE, "fire vulnerability", true },
    { 1, TR2_VUL_POIS, "poison vulnerability", true },
    { 2, TR3_CHEAT_DEATH, "cheat death", false },
    { 2, TR3_STAND_FAST, "stand fast", false },
    { 2, TR3_ACCURATE, "accuracy rerolls", false },
    { 2, TR3_CUMBERSOME, "no critical hits", true },
    { 2, TR3_AVOID_TRAPS, "avoid traps", false },
    { 2, TR3_MEDIC, "medic", false },
    { 2, TR3_WILL_DRAIN, "Will drain", false },
    { 3, TR4_UNLIGHT, "unlight", true },
    { 3, TR4_ARMOR_SHATTER, "armor shatter", false },
    { 3, TR4_PAIRED, "paired weapon", false },
    { 3, TR4_SUBTLETY_THROW, "throwing Subtlety", false },
    { 3, TR4_BREAKS_PERMA_CURSE, "break permanent curses", false },
    { 3, TR4_SLAY_SERPENT, "slay serpents", false },
    { 3, TR4_SLAY_VAMPIRE, "slay vampires", false },
    { 3, TR4_SLAY_HORROR, "slay horrors", false },
    { 3, TR4_SLAY_CAT, "slay cats", false },
    { 3, TR4_SLAY_GIANT, "slay giants", false },
    { 3, TR4_DEEP_CALL, "Deep Call", true },
    { 3, TR4_PROT_FIRE, "protection against fire", false },
    { 3, TR4_PROT_COLD, "protection against cold", false },
    { 3, TR4_PROT_POIS, "protection against poison", false },
    { 3, TR4_PROT_DARK, "protection against darkness", false },
};

static void comparison_read(comparison_item* item, const object_type* object)
{
    memset(item, 0, sizeof(*item));
    if (object && !object->k_idx)
        object = NULL;
    item->object = object;
    item->known = !object || object_known_p(object)
        || (object->ident & IDENT_SPOIL);
    if (object)
        object_flags_known4(object, &item->flags[0], &item->flags[1],
            &item->flags[2], &item->flags[3]);
}

static int comparison_group(const object_type* object)
{
    if (!object || !object->k_idx)
        return 0;
    switch (object->tval)
    {
    case TV_SWORD: case TV_HAFTED: case TV_POLEARM: case TV_DIGGING:
    case TV_SHIELD:
        return INVEN_ARM; /* Off-hand can replace a shield with a weapon. */
    case TV_HELM: case TV_CROWN:
        return INVEN_HEAD;
    case TV_MAIL: case TV_SOFT_ARMOR:
        return INVEN_BODY;
    case TV_RING:
        return INVEN_LEFT;
    case TV_ARROW:
        return INVEN_QUIVER1;
    case TV_BOW: case TV_AMULET: case TV_LIGHT: case TV_CLOAK:
    case TV_GLOVES: case TV_BOOTS:
        return wield_slot(object);
    default:
        return 0;
    }
}

typedef struct comparison_loadout
{
    int attack, evasion, dd, ds, armor_min, armor_max;
    int offhand_attack, offhand_dd, offhand_ds;
    bool offhand;
    bool ranged;
    bool throwing;
} comparison_loadout;

static bool comparison_melee(const object_type* object)
{
    return object && (object->tval == TV_SWORD || object->tval == TV_HAFTED
        || object->tval == TV_POLEARM || object->tval == TV_DIGGING);
}

static int comparison_slot(const object_type* selected,
    const object_type* reference)
{
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        if (reference == &inventory[i] && i != INVEN_BELT)
            return i;
    /* A worn ring compared to an empty alternative moves to the other hand.
     * Comparing against its own slot would incorrectly remove it first. */
    if (!reference && selected->tval == TV_RING)
    {
        if (selected == &inventory[INVEN_LEFT] && !inventory[INVEN_RIGHT].k_idx)
            return INVEN_RIGHT;
        if (selected == &inventory[INVEN_RIGHT] && !inventory[INVEN_LEFT].k_idx)
            return INVEN_LEFT;
    }
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        if (selected == &inventory[i] && i != INVEN_BELT)
            return i;
    return comparison_melee(selected) ? INVEN_WIELD : wield_slot(selected);
}

static bool comparison_target_matches(const object_type* selected,
    const object_type* reference)
{
    if (!reference || !reference->k_idx)
        return true;
    if (comparison_group(selected) != comparison_group(reference))
        return false;
    if (reference == &inventory[INVEN_ARM] && comparison_melee(selected)
        && !player_offhand_weapon_allowed(&inventory[INVEN_WIELD], selected))
        return false;
    if (selected->tval == TV_SHIELD && comparison_melee(reference))
        return reference == &inventory[INVEN_ARM];
    if (comparison_melee(selected) && reference->tval == TV_SHIELD)
        return reference == &inventory[INVEN_ARM]
            && player_offhand_weapon_allowed(&inventory[INVEN_WIELD], selected);
    return true;
}

static void comparison_mask_unknown(object_type* object)
{
    if (!object->k_idx || object_known_p(object) || (object->ident & IDENT_SPOIL))
        return;
    /* Combat numbers and weight are observable even before handling or ID.
     * Identity, grants and magical bonuses must not affect the known preview. */
    object->ps = object_effective_protection_sides(object);
    object->name1 = 0;
    object_set_ego_prefix(object, 0);
    object_set_ego_suffix(object, 0);
    object->pval = 0;
    object->abilities = 0;
    memset(object->stat_bonus, 0, sizeof(object->stat_bonus));
    memset(object->skill_bonus, 0, sizeof(object->skill_bonus));
}

static void comparison_activate_grants(const object_type* object)
{
    for (int i = 0; i < object->abilities; i++)
    {
        int skill = object->skilltype[i], ability = object->abilitynum[i];
        if (skill < 0 || skill >= S_MAX || ability < 0
            || ability >= ABILITIES_MAX || player_ability_restriction(skill, ability))
            continue;
        if (!p_ptr->have_ability[skill][ability])
            p_ptr->have_ability[skill][ability]
                = p_ptr->active_ability[skill][ability] = true;
    }
}

static void comparison_stow(const object_type* object)
{
    if (!object->k_idx)
        return;
    for (int i = 0; i <= INVEN_PACK; i++)
        if (!inventory[i].k_idx)
        {
            inventory[i] = *object;
            return;
        }
}

static bool comparison_preview(const object_type* object, int slot,
    comparison_loadout* result)
{
    player_type player_snapshot;
    object_type inventory_snapshot[INVEN_TOTAL];
    object_type candidate = { 0 }, displaced;
    bool old_flags_preview;
    bool unknown_equipped = false;
    bool carried_oath_negated;
    int source = -1;

    if (!p_ptr || !inventory || !rp_ptr || !current_character_profile
        || !oath_info || slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return false;
    if (object && object->k_idx)
        candidate = *object;
    player_snapshot = *p_ptr;
    memcpy(inventory_snapshot, inventory, sizeof(inventory_snapshot));
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        if (object == &inventory[i])
            source = i;
        if (inventory[i].k_idx && player_equipment_slot_counts_as_equipped(i)
            && !object_known_p(&inventory[i]) && !(inventory[i].ident & IDENT_SPOIL))
            unknown_equipped = true;
        comparison_mask_unknown(&inventory[i]);
    }
    comparison_mask_unknown(&candidate);
    old_flags_preview = object_flags_known_preview(true);
    carried_oath_negated = player_has_inventory_flag3(TR3_OATH_NEGATE);
    /* Remove stale active grants from unknown equipment before any pass can
     * use them (Parry, for example, is consulted early during upkeep). */
    calc_bonuses_for_preview();
    calc_bonuses_for_preview();
    if (unknown_equipped)
    {
        /* Current attribute totals are already visible. Keep their otherwise
         * unexplained contribution constant in both known-effects previews;
         * never infer or disclose which unidentified item supplied it. */
        for (int i = 0; i < A_MAX; i++)
            p_ptr->stat_base[i] += player_snapshot.stat_use[i] - p_ptr->stat_use[i];
    }
    displaced = inventory[slot];
    if (source >= 0 && source != slot)
    {
        if (inventory[source].number > 1)
            inventory[source].number--;
        else
            object_wipe(&inventory[source]);
    }
    inventory[slot] = candidate;
    if (slot != INVEN_QUIVER1 && candidate.k_idx)
        inventory[slot].number = 1;
    if (source != slot)
        comparison_stow(&displaced);

    /* Harness alternatives are compared as active weapons in the same role. */
    if (slot == INVEN_WIELD || slot == INVEN_ARM)
        p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    else if (slot == INVEN_BOW || slot == INVEN_QUIVER1)
    {
        p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
        inventory[INVEN_WIELD].pickup_slot = -1;
    }
    if (slot != INVEN_QUIVER1)
        comparison_activate_grants(&inventory[slot]);
    if (carried_oath_negated)
    {
        /* Displaced possessions remain carried, even when every legacy pack
         * slot is full and the temporary stow cannot represent them there. */
        for (int oath = OATH_MERCY; oath <= OATH_LIGHT; oath++)
        {
            int ability = oath_special_ability_from_oath_num(oath);
            if (ability >= 0)
                p_ptr->active_ability[S_SPC][ability] = false;
        }
    }
    /* Establish grants for the requested role before resolving handedness. */
    calc_bonuses_for_preview();
    if (player_active_weapon_is_melee())
    {
        u32b f1, f2, f3;
        object_type* offhand = &inventory[INVEN_ARM];
        object_flags(&inventory[INVEN_WIELD], &f1, &f2, &f3);
        if ((f3 & TR3_TWO_HANDED)
            || (offhand->k_idx && offhand->tval != TV_SHIELD
                && !player_offhand_weapon_allowed(&inventory[INVEN_WIELD], offhand)))
        {
            displaced = *offhand;
            object_wipe(offhand);
            comparison_stow(&displaced);
        }
    }
    /* A removed item's grants may affect earlier steps in the same pass. */
    calc_bonuses_for_preview();
    calc_bonuses_for_preview();
    memset(result, 0, sizeof(*result));
    result->throwing = player_active_weapon_is_ranged()
        && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_THROWING;
    result->ranged = player_active_weapon_is_ranged() && !result->throwing;
    result->attack = p_ptr->skill_use[result->ranged ? S_ARC : S_MEL];
    result->dd = result->ranged ? p_ptr->add : p_ptr->mdd;
    result->ds = result->ranged ? p_ptr->ads : p_ptr->mds;
    if (result->throwing)
    {
        const object_type* thrown = &inventory[INVEN_WIELD];
        result->attack = p_ptr->skill_use[S_MEL] + thrown->att
            + axe_bonus(thrown) + polearm_bonus(thrown);
        if (p_ptr->active_ability[S_MEL][MEL_THROWING]
            || object_grants_ability(thrown, S_MEL, MEL_THROWING))
            result->attack++;
        result->dd = total_mdd(thrown);
        result->ds = total_mds_for_weapon_mode(thrown, 0,
            PLAYER_ACTIVE_WEAPON_RANGED_1);
        if (result->ds > 0)
            result->ds = MAX(1, result->ds - curse_flag_delta_cur(CUR_MDS_SHIFT));
    }
    if (result->ranged)
    {
        const object_type* arrow = slot == INVEN_QUIVER1 ? &inventory[slot]
            : player_quiver_arrow_object(player_quiver_selected_arrow_slot());
        if (arrow && arrow->k_idx)
            result->attack += arrow->att;
    }
    result->evasion = p_ptr->skill_use[S_EVN];
    result->armor_min = p_min(GF_HURT, true);
    result->armor_max = p_max(GF_HURT, true);
    result->offhand = !result->ranged && !result->throwing && inventory[INVEN_ARM].k_idx
        && player_offhand_weapon_allowed(&inventory[INVEN_WIELD], &inventory[INVEN_ARM]);
    if (result->offhand)
    {
        result->offhand_attack = p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod;
        result->offhand_dd = p_ptr->mdd2;
        result->offhand_ds = p_ptr->mds2;
    }
    *p_ptr = player_snapshot;
    memcpy(inventory, inventory_snapshot, sizeof(inventory_snapshot));
    object_flags_known_preview(old_flags_preview);
    return true;
}

bool object_compare_weapon_damage(const object_type* object, int* dd, int* ds)
{
    comparison_loadout preview;
    if (!object || (!comparison_melee(object) && object->tval != TV_BOW)
        || !comparison_preview(object, comparison_slot(object, NULL), &preview))
        return false;
    bool offhand = comparison_slot(object, NULL) == INVEN_ARM;
    *dd = offhand ? preview.offhand_dd : preview.dd;
    *ds = offhand ? preview.offhand_ds : preview.ds;
    return true;
}

static void comparison_token(int* any, byte attr, cptr text)
{
    if (*any < 6)
    {
        if (*any)
            text_out("; ");
        text_out_c(attr, text);
    }
    else if (*any == 6)
        text_out_c(TERM_SLATE, ui_text("; more changes in Full view"));
    (*any)++;
}

static void comparison_delta(int* any, cptr name, int delta)
{
    if (delta)
        comparison_token(any, delta > 0 ? TERM_L_GREEN : TERM_L_RED,
            format("%s %+d", ui_text(name), delta));
}

static void comparison_tier(int* any, cptr name, int before, int after,
    const char** names, bool drawback)
{
    if (before != after)
        comparison_token(any, (after > before) != drawback
            ? TERM_L_GREEN : TERM_L_RED,
            format("%s %s -> %s", name, ui_text(names[before]),
                ui_text(names[after])));
}

static int comparison_curse(u32b flags)
{
    return flags & TR3_PERMA_CURSE ? 3
        : flags & TR3_HEAVY_CURSE ? 2 : flags & TR3_LIGHT_CURSE ? 1 : 0;
}

static int comparison_sharpness(u32b flags)
{
    return flags & TR1_SHARPNESS2 ? 2 : flags & TR1_SHARPNESS ? 1 : 0;
}

static int comparison_protection_sides(const comparison_item* item)
{
    return object_effective_protection_sides(item->object);
}

static void comparison_combat_number(cptr label, int before, int after)
{
    int delta = after - before;
    text_out_c(delta > 0 ? TERM_L_GREEN : delta < 0 ? TERM_L_RED : TERM_WHITE,
        format("%s %+d (%+d -> %+d)", label, delta, before, after));
}

static void comparison_dice(int* any, cptr name, int old_dd, int old_ds,
    int new_dd, int new_ds);

static void comparison_combat_out(const object_type* selected,
    const object_type* reference, int slot)
{
    comparison_loadout before, after;
    if (!comparison_preview(reference, slot, &before)
        || !comparison_preview(selected, slot, &after))
        return;
    if (slot == INVEN_ARM && comparison_melee(selected))
    {
        comparison_combat_number(ui_text("Off-hand attack"), before.offhand_attack,
            after.offhand_attack);
        before.dd = before.offhand_dd; before.ds = before.offhand_ds;
        after.dd = after.offhand_dd; after.ds = after.offhand_ds;
    }
    else
        comparison_combat_number(after.ranged ? ui_text("Archery attack")
                : after.throwing ? ui_text("Throwing attack") : ui_text("Attack"),
            before.attack, after.attack);
    text_out("; ");
    comparison_combat_number(ui_text("Evasion"), before.evasion, after.evasion);
    text_out("; ");
    int any = 0;
    comparison_dice(&any, ui_text("Damage"), before.dd, before.ds, after.dd, after.ds);
    if (!any)
        text_out(format(ui_text("Damage %dd%d (unchanged)"), after.dd, after.ds));
    if (selected->pd || (reference && reference->pd)
        || before.armor_min != after.armor_min || before.armor_max != after.armor_max)
    {
        text_out("; ");
        byte attr = TERM_WHITE;
        if (after.armor_min >= before.armor_min && after.armor_max >= before.armor_max
            && (after.armor_min != before.armor_min || after.armor_max != before.armor_max))
            attr = TERM_L_GREEN;
        if (after.armor_min <= before.armor_min && after.armor_max <= before.armor_max
            && (after.armor_min != before.armor_min || after.armor_max != before.armor_max))
            attr = TERM_L_RED;
        text_out_c(attr, format(ui_text("Armor %d-%d -> %d-%d"), before.armor_min,
            before.armor_max, after.armor_min, after.armor_max));
    }
    if ((slot != INVEN_ARM || !comparison_melee(selected))
        && (before.offhand || after.offhand))
    {
        text_out("; ");
        comparison_combat_number(ui_text("Off-hand attack"), before.offhand_attack,
            after.offhand_attack);
        text_out("; ");
        any = 0;
        comparison_dice(&any, ui_text("Off-hand damage"), before.offhand_dd,
            before.offhand_ds, after.offhand_dd, after.offhand_ds);
        if (!any)
            text_out(format(ui_text("Off-hand damage %dd%d (unchanged)"),
                after.offhand_dd, after.offhand_ds));
    }
    text_out(".\n  ");
}

static void comparison_dice(int* any, cptr name, int old_dd, int old_ds,
    int new_dd, int new_ds)
{
    if (old_dd != new_dd || old_ds != new_ds)
    {
        char before[24], after[24];
        byte attr = TERM_WHITE;
        /* Use a directional color only when neither dice nor sides trades
         * against the other. Mixed changes retain their neutral comparison. */
        if (old_dd > 0 && old_ds > 0 && new_dd > 0 && new_ds > 0)
        {
            if (new_dd >= old_dd && new_ds >= old_ds)
                attr = TERM_L_GREEN;
            else if (new_dd <= old_dd && new_ds <= old_ds)
                attr = TERM_L_RED;
        }
        strnfmt(before, sizeof(before), "%dd%d", old_dd, old_ds);
        strnfmt(after, sizeof(after), "%dd%d", new_dd, new_ds);
        comparison_token(any, attr, format("%s %s -> %s", name,
            old_dd && old_ds ? before : ui_text("none"),
            new_dd && new_ds ? after : ui_text("none")));
    }
}

static bool comparison_has_ability(const object_type* object,
    const object_type* source, int index)
{
    if (!object)
        return false;
    for (int i = 0; i < object->abilities; i++)
    {
        if (object->skilltype[i] == source->skilltype[index]
            && object->abilitynum[i] == source->abilitynum[index]
            && object->bane_type[i] == source->bane_type[index])
            return true;
    }
    return false;
}

static void comparison_abilities(int* any, const object_type* object,
    const object_type* other, bool gain)
{
    if (!object)
        return;
    for (int i = 0; i < object->abilities; i++)
    {
        int index;
        cptr name;
        char label[128];
        if (comparison_has_ability(other, object, i))
            continue;
        index = ability_index(object->skilltype[i], object->abilitynum[i]);
        if (index < 0 || index >= z_info->b_max || !b_info[index].name)
            continue;
        name = b_name + b_info[index].name;
        if (object->skilltype[i] == S_PER
            && object->abilitynum[i] == PER_BANE
            && object->bane_type[i] > 0 && object->bane_type[i] < 9)
            strnfmt(label, sizeof(label), "%c%s-%s", gain ? '+' : '-',
                bane_name[object->bane_type[i]], name);
        else
            strnfmt(label, sizeof(label), "%c%s", gain ? '+' : '-', name);
        comparison_token(any, gain ? TERM_L_GREEN : TERM_L_RED, label);
    }
}

static void comparison_pair_out(const object_type* selected,
    const object_type* reference, int slot)
{
    comparison_item candidate, current;
    int any = 0;
    comparison_read(&candidate, selected);
    comparison_read(&current, reference);
    reference = current.object;

    comparison_combat_out(selected, reference, slot);
    comparison_dice(&any, ui_text("Protection"), reference ? reference->pd : 0,
        comparison_protection_sides(&current), selected->pd,
        comparison_protection_sides(&candidate));
    /* Essential combat values never consume the limited property budget. */
    if (any)
        text_out(".\n  ");
    any = 0;

    if (candidate.known && current.known)
    {
        static const char* curse_names[] = { "none", "light curse", "heavy curse", "permanent curse" };
        static const char* sharpness_names[] = { "none", "half", "full" };
        static const char* stat_labels[] = { "Str", "Dex", "Con", "Gra" };
        static const u32b stat_flags[] = { TR1_STR | TR1_NEG_STR,
            TR1_DEX | TR1_NEG_DEX, TR1_CON | TR1_NEG_CON,
            TR1_GRA | TR1_NEG_GRA };
        static const u32b skill_flags[S_MAX] = { TR1_MEL, TR1_ARC, 0,
            TR1_STL, TR1_PER, TR1_WIL, TR1_SMT, TR1_SNG };
        comparison_tier(&any, ui_text("Curse"), comparison_curse(current.flags[2]),
            comparison_curse(candidate.flags[2]), curse_names, true);
        comparison_tier(&any, ui_text("Sharpness"), comparison_sharpness(current.flags[0]),
            comparison_sharpness(candidate.flags[0]), sharpness_names, false);
        for (int i = 0; i < A_MAX; i++)
            comparison_delta(&any, stat_labels[i],
                (candidate.flags[0] & stat_flags[i] ? selected->stat_bonus[i] : 0)
                - (reference && (current.flags[0] & stat_flags[i])
                    ? reference->stat_bonus[i] : 0));
        for (int i = 0; i < S_MAX; i++)
            comparison_delta(&any, skill_names_full[i],
                (candidate.flags[0] & skill_flags[i] ? selected->skill_bonus[i] : 0)
                - (reference && (current.flags[0] & skill_flags[i])
                    ? reference->skill_bonus[i] : 0));
        comparison_delta(&any, ui_text("Tunneling"),
            (candidate.flags[0] & TR1_TUNNEL ? selected->pval : 0)
            - (reference && (current.flags[0] & TR1_TUNNEL) ? reference->pval : 0));
        comparison_delta(&any, ui_text("Damage sides"),
            (candidate.flags[0] & TR1_DAMAGE_SIDES ? selected->pval : 0)
            - (reference && (current.flags[0] & TR1_DAMAGE_SIDES) ? reference->pval : 0));
        {
            u32b handling = TR3_HAND_AND_A_HALF | TR3_TWO_HANDED;
            u32b before = current.flags[2] & handling;
            u32b after = candidate.flags[2] & handling;
            if (before != after)
                comparison_token(&any, TERM_WHITE,
                    format(ui_text("Handling %s -> %s"),
                        before & TR3_TWO_HANDED ? ui_text("two-handed")
                            : before ? ui_text("hand-and-a-half") : ui_text("one-handed"),
                        after & TR3_TWO_HANDED ? ui_text("two-handed")
                            : after ? ui_text("hand-and-a-half") : ui_text("one-handed")));
        }
        if ((candidate.flags[3] ^ current.flags[3]) & TR4_DEPTH_SCALE_PS)
            comparison_token(&any, TERM_WHITE,
                candidate.flags[3] & TR4_DEPTH_SCALE_PS
                    ? ui_text("+protection growth with depth") : ui_text("-protection growth with depth"));
        for (size_t i = 0; i < N_ELEMENTS(comparison_properties); i++)
        {
            const comparison_property* property = &comparison_properties[i];
            bool gain = (candidate.flags[property->set] & property->flag) != 0;
            bool had = (current.flags[property->set] & property->flag) != 0;
            if (gain != had)
                comparison_token(&any, gain != property->drawback
                    ? TERM_L_GREEN : TERM_L_RED,
                    format("%c%s", gain ? '+' : '-', ui_text(property->name)));
        }
        comparison_abilities(&any, selected, reference, true);
        comparison_abilities(&any, reference, selected, false);
    }

    {
        int delta = selected->weight - (reference ? reference->weight : 0);
        if (delta)
            comparison_token(&any, TERM_WHITE,
                format(ui_text("Weight %c%d.%d lb each"), delta > 0 ? '+' : '-',
                    ABS(delta) / 10, ABS(delta) % 10));
    }
    if (!any)
        text_out(ui_text("No other known changes"));
    if (!candidate.known || !current.known)
        text_out_c(TERM_SLATE, ui_text("; unidentified effects unknown"));
    text_out(".\n");
}

bool object_compare_has_targets(const object_type** objects, int count)
{
    int group;
    if (!objects || count < 2 || !(group = comparison_group(objects[0])))
        return false;
    for (int i = 1; i < count; i++)
        if (objects[i] != objects[0]
            && comparison_target_matches(objects[0], objects[i]))
            return true;
    return false;
}

void object_compare_summary_out(const object_type** objects,
    const char** headings, int count)
{
    int shown = 0;
    int group;
    if (!objects || count < 2 || !(group = comparison_group(objects[0])))
        return;

    /* Prefer references in equipment slots, then carried/floor alternatives.
     * Two summaries cover both ring choices without filling a small viewport.
     * The Full view retains all existing descriptions. */
    for (int pass = 0; pass < 2 && shown < 2; pass++)
    {
        for (int i = 1; i < count && shown < 2; i++)
        {
            int slot = -1;
            cptr heading = headings && headings[i] ? headings[i] : ui_text("compared item");
            const object_type* reference = objects[i];
            if (reference == objects[0]
                || !comparison_target_matches(objects[0], reference))
                continue;
            for (int j = INVEN_WIELD; j < INVEN_TOTAL; j++)
                if (reference == &inventory[j])
                    slot = j;
            if ((slot >= 0) != (pass == 0))
                continue;
            if (!shown)
                text_out_c(TERM_L_BLUE,
                    ui_text("Known item changes (with your strength and abilities):\n"));
            {
                int heading_len = (int)strlen(heading);
                while (heading_len > 0 && (heading[heading_len - 1] == ':'
                    || heading[heading_len - 1] == ' '))
                    heading_len--;
                text_out_c(TERM_L_BLUE, format(ui_text("vs %.*s%s: "), heading_len, heading,
                    slot >= 0 && reference && reference->k_idx
                        && !player_equipment_slot_counts_as_equipped(slot)
                        ? ui_text(" (Harness)") : ""));
                if (reference && reference->k_idx)
                {
                    object_info_item_header_out(reference);
                }
                else
                    text_out(ui_text("(empty slot)"));
                text_out("\n  ");
            }
            comparison_pair_out(objects[0], reference,
                comparison_slot(objects[0], reference));
            shown++;
        }
    }
    if (shown)
        text_out("\n");
}
