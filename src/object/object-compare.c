#include "angband.h"
#include "externs.h"
#include "object/object-compare.h"

typedef struct comparison_item
{
    const object_type* object;
    u32b flags[4];
    bool known;
    bool combat_visible;
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
    item->combat_visible = item->known || (object->tval != TV_RING
        && (!object_uses_smithing_difficulty(object)
            || (object->ident & IDENT_HANDLED)));
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

static void comparison_token(int* any, byte attr, cptr text)
{
    if (*any < 6)
    {
        if (*any)
            text_out("; ");
        text_out_c(attr, text);
    }
    else if (*any == 6)
        text_out_c(TERM_SLATE, "; more changes in Full view");
    (*any)++;
}

static void comparison_delta(int* any, cptr name, int delta)
{
    if (delta)
        comparison_token(any, delta > 0 ? TERM_L_GREEN : TERM_L_RED,
            format("%s %+d", name, delta));
}

static void comparison_tier(int* any, cptr name, int before, int after,
    const char** names, bool drawback)
{
    if (before != after)
        comparison_token(any, (after > before) != drawback
            ? TERM_L_GREEN : TERM_L_RED,
            format("%s %s -> %s", name, names[before], names[after]));
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
    if (!item->object)
        return 0;
    return item->object->ps + ((item->flags[3] & TR4_DEPTH_SCALE_PS)
        && item->object->ps > 0 && p_ptr ? MAX(0, p_ptr->depth) / 5 : 0);
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
            old_dd && old_ds ? before : "none",
            new_dd && new_ds ? after : "none"));
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
    const object_type* reference)
{
    comparison_item candidate, current;
    int any = 0;
    comparison_read(&candidate, selected);
    comparison_read(&current, reference);
    reference = current.object;

    if (candidate.combat_visible && current.combat_visible)
    {
        comparison_delta(&any, "Attack", selected->att
            - (reference ? reference->att : 0));
        comparison_delta(&any, "Evasion", selected->evn
            - (reference ? reference->evn : 0));
        /* Raw damage is independent of strength, abilities, handedness and
         * which inventory pointer happens to hold the item. */
        comparison_dice(&any, "Base damage", reference ? reference->dd : 0,
            reference ? reference->ds : 0, selected->dd, selected->ds);
        comparison_dice(&any, "Protection", reference ? reference->pd : 0,
            comparison_protection_sides(&current), selected->pd,
            comparison_protection_sides(&candidate));
    }

    if (candidate.known && current.known)
    {
        static const char* curse_names[] = { "none", "light", "heavy", "permanent" };
        static const char* sharpness_names[] = { "none", "half", "full" };
        static const char* stat_labels[] = { "Str", "Dex", "Con", "Gra" };
        static const u32b stat_flags[] = { TR1_STR | TR1_NEG_STR,
            TR1_DEX | TR1_NEG_DEX, TR1_CON | TR1_NEG_CON,
            TR1_GRA | TR1_NEG_GRA };
        static const u32b skill_flags[S_MAX] = { TR1_MEL, TR1_ARC, 0,
            TR1_STL, TR1_PER, TR1_WIL, TR1_SMT, TR1_SNG };
        comparison_tier(&any, "Curse", comparison_curse(current.flags[2]),
            comparison_curse(candidate.flags[2]), curse_names, true);
        comparison_tier(&any, "Sharpness", comparison_sharpness(current.flags[0]),
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
        comparison_delta(&any, "Tunneling",
            (candidate.flags[0] & TR1_TUNNEL ? selected->pval : 0)
            - (reference && (current.flags[0] & TR1_TUNNEL) ? reference->pval : 0));
        comparison_delta(&any, "Damage sides",
            (candidate.flags[0] & TR1_DAMAGE_SIDES ? selected->pval : 0)
            - (reference && (current.flags[0] & TR1_DAMAGE_SIDES) ? reference->pval : 0));
        {
            u32b handling = TR3_HAND_AND_A_HALF | TR3_TWO_HANDED;
            u32b before = current.flags[2] & handling;
            u32b after = candidate.flags[2] & handling;
            if (before != after)
                comparison_token(&any, TERM_WHITE,
                    format("Handling %s -> %s",
                        before & TR3_TWO_HANDED ? "two-handed"
                            : before ? "hand-and-a-half" : "one-handed",
                        after & TR3_TWO_HANDED ? "two-handed"
                            : after ? "hand-and-a-half" : "one-handed"));
        }
        if ((candidate.flags[3] ^ current.flags[3]) & TR4_DEPTH_SCALE_PS)
            comparison_token(&any, TERM_WHITE,
                candidate.flags[3] & TR4_DEPTH_SCALE_PS
                    ? "+protection growth with depth" : "-protection growth with depth");
        for (size_t i = 0; i < N_ELEMENTS(comparison_properties); i++)
        {
            const comparison_property* property = &comparison_properties[i];
            bool gain = (candidate.flags[property->set] & property->flag) != 0;
            bool had = (current.flags[property->set] & property->flag) != 0;
            if (gain != had)
                comparison_token(&any, gain != property->drawback
                    ? TERM_L_GREEN : TERM_L_RED,
                    format("%c%s", gain ? '+' : '-', property->name));
        }
        comparison_abilities(&any, selected, reference, true);
        comparison_abilities(&any, reference, selected, false);
    }

    {
        int delta = selected->weight - (reference ? reference->weight : 0);
        if (delta)
            comparison_token(&any, TERM_WHITE,
                format("Weight %c%d.%d lb each", delta > 0 ? '+' : '-',
                    ABS(delta) / 10, ABS(delta) % 10));
    }
    if (!any)
        text_out("No changes to compared values");
    if (!candidate.known || !current.known)
        text_out_c(TERM_SLATE, "; unidentified effects unknown");
    text_out(".\n");
}

bool object_compare_has_targets(const object_type** objects, int count)
{
    int group;
    if (!objects || count < 2 || !(group = comparison_group(objects[0])))
        return false;
    for (int i = 1; i < count; i++)
        if (objects[i] != objects[0] && (!objects[i] || !objects[i]->k_idx
            || comparison_group(objects[i]) == group))
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
            cptr heading = headings && headings[i] ? headings[i] : "compared item";
            const object_type* reference = objects[i];
            if (reference == objects[0]
                || (reference && reference->k_idx
                    && comparison_group(reference) != group))
                continue;
            for (int j = INVEN_WIELD; j < INVEN_TOTAL; j++)
                if (reference == &inventory[j])
                    slot = j;
            if ((slot >= 0) != (pass == 0))
                continue;
            if (!shown)
                text_out_c(TERM_L_BLUE, "Known item changes:\n");
            {
                char name[512];
                int heading_len = (int)strlen(heading);
                while (heading_len > 0 && (heading[heading_len - 1] == ':'
                    || heading[heading_len - 1] == ' '))
                    heading_len--;
                text_out_c(TERM_L_BLUE, format("vs %.*s%s: ", heading_len, heading,
                    slot >= 0 && reference && reference->k_idx
                        && !player_equipment_slot_counts_as_equipped(slot)
                        ? " (Harness)" : ""));
                if (reference && reference->k_idx)
                {
                    /* Names follow the same knowledge rules as the inventory;
                     * mode zero does not expose hidden combat values. */
                    object_desc_display(name, sizeof(name), reference, true, 0);
                    byte base_color = weapon_glows(reference) ? TERM_L_BLUE
                        : tval_to_attr[reference->tval % N_ELEMENTS(tval_to_attr)];
                    text_out_c(object_display_color(reference, base_color), name);
                }
                else
                    text_out("(empty slot)");
                text_out("\n  ");
            }
            comparison_pair_out(objects[0], reference);
            shown++;
        }
    }
    if (shown)
        text_out("\n");
}
