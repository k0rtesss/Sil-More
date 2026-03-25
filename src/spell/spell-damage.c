/* File: spell-damage.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "spell/spell-damage.h"

/*
 * Return a color to use for the bolt/ball spells
 */
static byte spell_color(int type)
{
    /* Analyze */
    switch (type)
    {
    case GF_ARROW:
        return (TERM_L_UMBER);
    case GF_BOULDER:
        return (TERM_SLATE);
    case GF_ACID:
        return (TERM_SLATE);
    case GF_ELEC:
        return (TERM_BLUE);
    case GF_FIRE:
        return (TERM_RED);
    case GF_COLD:
        return (TERM_WHITE);
    case GF_POIS:
        return (TERM_GREEN);
    case GF_CONFUSION:
        return (TERM_L_UMBER);
    case GF_SOUND:
        return (TERM_L_WHITE);
    case GF_LIGHT:
        return (TERM_WHITE);
    case GF_DARK_WEAK:
        return (TERM_L_DARK);
    case GF_DARK:
        return (TERM_L_DARK);
    case GF_IDENTIFY:
        return (TERM_WHITE);
    case GF_EARTHQUAKE:
        return (TERM_SLATE);
    case GF_WEB:
        return (TERM_L_UMBER);
    }

    /* Standard "color" */
    return (TERM_L_WHITE);
}

/*
 * Find the attr/char pair to use for a spell effect
 *
 * It is moving (or has moved) from (x,y) to (nx,ny).
 *
 * If the distance is not "one", we (may) return "*".
 */
u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
    int base;

    byte k;

    byte a;
    char c;

    /* No motion (*) */
    if ((ny == y) && (nx == x))
        base = 0x30;

    /* Vertical (|) */
    else if (nx == x)
        base = 0x40;

    /* Horizontal (-) */
    else if (ny == y)
        base = 0x50;

    /* Diagonal (/) */
    else if ((ny - y) == (x - nx))
        base = 0x60;

    /* Diagonal (\) */
    else if ((ny - y) == (nx - x))
        base = 0x70;

    /* Weird (*) */
    else
        base = 0x30;

    if (typ == GF_LIGHT && use_graphics == GRAPHICS_MICROCHASM)
    {
        a = misc_to_attr[ICON_GLOW];
        c = misc_to_char[ICON_GLOW];
    }
    else
    {
        /* Basic spell color */
        k = spell_color(typ);

        /* Obtain attr/char */
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];
    }

    /* Create pict */
    return (PICT(a, c));
}

/*
 * Allows items that have the CHEAT_DEATH flag to save the player
 */
void attempt_to_cheat_death(void)
{
    char o_name[80];

    /* Scan the equipment */
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b f1, f2, f3;

        object_type* o_ptr = &inventory[i];
        object_flags(o_ptr, &f1, &f2, &f3);

        /* If player is dead, save them at the cost of the item */
        if (f3 & TR3_CHEAT_DEATH && p_ptr->chp <= 0)
        {
            p_ptr->chp = 1;
            p_ptr->energy += 100;
            set_blind(0);
            set_confused(0);
            set_poisoned(0);
            set_afraid(0);
            set_entranced(0);
            set_image(0);
            set_stun(0);
            set_cut(0);
            set_slow(0);

            /* Get a description */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            msg_format("Your %s breaks into two pieces!", o_name);
            ident_f3(TR3_CHEAT_DEATH, o_ptr);

            inven_item_increase(i, -1);
            inven_item_optimize(i);
        }
    }
}

/*
 * Decreases players hit points and sets death flag if necessary
 *
 * Invulnerability needs to be changed into a "shield" XXX XXX XXX
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(int dam, cptr kb_str)
{
    int old_chp = p_ptr->chp;

    int warning = (p_ptr->mhp * op_ptr->hitpoint_warn / 10);

    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    /* Paranoia */
    if (p_ptr->is_dead)
        return;

    /* Disturb */
    disturb(1, 0);

    /* Hurt the player */
    p_ptr->chp -= dam;

    attempt_to_cheat_death();

    /* Display the hitpoints */
    p_ptr->redraw |= (PR_HP);

    /* Window stuff */
    p_ptr->window |= (PW_PLAYER_0);

    if (p_ptr->chp <= 0)
    {
        /* Hack -- Note death */
        message(MSG_DEATH, 0, "You die.");
        message_flush();

        /* Note cause of death */
        if (p_ptr->image == 0)
        {
            SDL_strlcpy(p_ptr->died_from, kb_str, sizeof(p_ptr->died_from));
        }
        else
        {
            strnfmt(p_ptr->died_from, sizeof(p_ptr->died_from),
                "%s (while hallucinating)", kb_str);
        }

        killer_commit(kb_str);

        /* Note death */
        p_ptr->is_dead = true;

        /* Leaving */
        p_ptr->leaving = true;

        /* Write a note */

        /* Get time */
        (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

        /* Add note */
        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /*killed by */
        sprintf(buf, "Slain by %s.", p_ptr->died_from);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        /* date and time*/
        sprintf(buf, "Died on %s.", long_day);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /* Dead */
        return;
    }

    /* Hitpoint warning */
    if (p_ptr->chp < warning)
    {
        /* Hack -- bell on first notice */
        if (old_chp > warning)
        {
            bell("Low hitpoint warning!");
        }

        /* Message */
        message(MSG_HITPOINT_WARN, 0, "*** LOW HITPOINT WARNING! ***");
        message_flush();
    }

    // Cancel entrancement
    set_entranced(0);
}

/*
 * Does a given class of objects (usually) hate acid?
 * Note that acid can either melt or corrode something.
 */
bool hates_acid(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable items */
    case TV_ARROW:
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    /* Staffs are wood */
    case TV_STAFF:
    {
        return (true);
    }

    /* Ouch */
    case TV_CHEST:
    {
        return (true);
    }

    /* Skeleton */
    case TV_SKELETON:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate electricity?
 */
bool hates_elec(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_RING:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate fire?
 * Hafted/Polearm weapons have wooden shafts.
 * Arrows/Bows are mostly wooden.
 */
bool hates_fire(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }

    /* Chests */
    case TV_CHEST:
    {
        return (true);
    }

    /* Torches */
    case TV_LIGHT:
    {
        if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
            return (true);
        else
            return (false);
    }

    /* Notes burn */
    case TV_NOTE:
    {
        return (true);
    }

    /* Staffs burn */
    case TV_STAFF:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate cold?
 */
bool hates_cold(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_POTION:
    case TV_GEM:
    case TV_FLASK:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Melt something
 */
static int set_acid_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_acid(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ACID))
        return (false);
    return (true);
}

/*
 * Electrical damage
 */
static int set_elec_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_elec(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ELEC))
        return (false);
    return (true);
}

/*
 * Burn something
 */
static int set_fire_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_fire(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_FIRE))
        return (false);
    return (true);
}

/*
 * Freeze things
 */
static int set_cold_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_cold(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_COLD))
        return (false);
    return (true);
}

/*
 * This seems like a pretty standard "typedef"
 */
typedef int (*inven_func)(const object_type*);

/*
 * Destroys a type of item on a given percent chance
 * Note that missiles are no longer necessarily all destroyed
 *
 * Returns number of items destroyed.
 */
static int inven_damage(inven_func typ, int perc, int resistance)
{
    int i, j, k, amt;

    object_type* o_ptr;

    char o_name[80];

    /* Count the casualties */
    k = 0;

    /* Scan through the slots backwards */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Hack -- for now, skip artefacts */
        if (artefact_p(o_ptr))
            continue;

        /* Give this item slot a shot at death */
        if ((*typ)(o_ptr))
        {
            /* Count the casualties */
            for (amt = j = 0; j < o_ptr->number; ++j)
            {
                if (percent_chance(perc)
                    && ((resistance < 0) || one_in_(resistance)))
                    amt++;
            }

            /* Some casualities */
            if (amt)
            {
                int old_charges = 0;

                /*hack, make sure the proper number of charges is displayed in
                 * the message*/
                if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
                    && (amt < o_ptr->number))
                {
                    /*save the number of charges*/
                    old_charges = o_ptr->pval;

                    /*distribute the charges*/
                    o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

                    o_ptr->pval = old_charges - o_ptr->pval;
                }

                /* Get a description */
                object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

                /* Message */
                msg_format("%sour %s (%c) %s destroyed!",
                    ((o_ptr->number > 1) ? ((amt == o_ptr->number)
                             ? "All of y"
                             : (amt > 1 ? "Some of y" : "One of y"))
                                         : "Y"),
                    o_name, index_to_label(i), ((amt > 1) ? "were" : "was"));

                /*hack, restore the proper number of charges after the messages
                 * have printed so the proper number of charges are destroyed*/
                if (old_charges)
                    o_ptr->pval = old_charges;

                /* Hack -- If staffs are destroyed, the total maximum
                 * timeout or charges of the stack needs to be reduced,
                 * unless all the items are being destroyed. -LM-
                 */
                if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
                {
                    o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;
                }

                /* Destroy "amt" items */
                inven_item_increase(i, -amt);
                inven_item_optimize(i);

                /* Count the casualties */
                k += amt;
            }
        }
    }

    /* Return the casualty count */
    return (k);
}

/*
 * Acid has hit the player, attempt to affect some armor.
 */
static int damage_armour(void)
{
    object_type* o_ptr = NULL;

    u32b f1, f2, f3;

    char o_name[80];

    int item = INVEN_BODY; // a default value to soothe compilation warnings

    /* Pick a (possibly empty) inventory slot */
    switch (dieroll(6))
    {
    case 1:
        item = INVEN_BODY;
        break;
    case 2:
        item = INVEN_ARM;
        break;
    case 3:
        item = INVEN_OUTER;
        break;
    case 4:
        item = INVEN_HANDS;
        break;
    case 5:
        item = INVEN_HEAD;
        break;
    case 6:
        item = INVEN_FEET;
        break;
    }

    o_ptr = &inventory[item];

    /* Nothing to damage */
    if (!o_ptr->k_idx || ((item == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)))
        return (false);

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Describe */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Object resists */
    if (f3 & (TR3_IGNORE_ACID))
    {
        msg_format("Your %s is unaffected!", o_name);

        return (true);
    }

    /* No damage left to be done */
    if ((o_ptr->ps <= 0) && (o_ptr->evn <= 0))
    {
        /* Destroy the item */
        inven_item_increase(item, -1);
        inven_item_optimize(item);

        /* Message */
        msg_format("Your %s is destroyed!", o_name);
    }
    else if (o_ptr->evn >= 0)
    {
        /* Damage the item */
        o_ptr->evn--;

        /* Message */
        msg_format("Your %s is damaged!", o_name);
    }
    else
    {
        /* Damage the item */
        o_ptr->ps--;

        /* Message */
        msg_format("Your %s is damaged!", o_name);
    }

    /* Calculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

    /* Item was damaged */
    return (true);
}

/*
 * Hurt the player with Acid
 */
void acid_dam(int dam, cptr kb_str)
{
    int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Damage armour */
    damage_armour();

    /* Take damage */
    take_hit(dam, kb_str);

    /* Inventory damage */
    inven_damage(set_acid_destroy, inv, 1);

    /* Supply damage */
    supplies_damage(set_acid_destroy, inv, 1);
}

/*
 * Hurt the player with electricity
 */
void elec_dam(int dam, cptr kb_str)
{
    int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);

    /* Inventory damage */
    inven_damage(set_elec_destroy, inv, 1);

    /* Supply damage */
    supplies_damage(set_elec_destroy, inv, 1);
}

/*
 * The player's fire resistance depends on equipment and temporary effects
 */
extern int resist_fire(void)
{
    int res = p_ptr->resist_fire;

    if (p_ptr->oppose_fire)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's cold resistance depends on equipment and temporary effects
 */
extern int resist_cold(void)
{
    int res = p_ptr->resist_cold;

    if (p_ptr->oppose_cold)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's poison resistance depends on equipment and temporary effects
 */
extern int resist_pois(void)
{
    int res = p_ptr->resist_pois;

    if (p_ptr->oppose_pois)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's dark resistance is strictly dependent
 * on the brightness of their square
 */
extern int resist_dark(void)
{
    int res = cave_light[p_ptr->py][p_ptr->px];

    if (res < 1)
        res = 1;

    return (res);
}

static void log_elemental_damage_context(const char* tag, cptr kb_str, int dam,
    int prt, int resistance, int net_dam)
{
    bool should_log = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px)
        != BIG_CAVE_NONE;

    if (!should_log)
    {
        should_log = (cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
    }

    if (!should_log)
        return;

    log_partition_debug_for_point(tag, p_ptr->py, p_ptr->px);
    log_debug(
        "%s: killer=%s raw=%d prt=%d net=%d base_fire=%d base_cold=%d base_pois=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d effective_resistance=%d",
        tag, kb_str ? kb_str : "(none)", dam, prt, net_dam,
        p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
        p_ptr->oppose_fire, p_ptr->oppose_cold, p_ptr->oppose_pois,
        resistance);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_mixed(int dam, cptr kb_str)
{
    int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);

    /* Inventory damage */
    inven_damage(set_fire_destroy, inv, resist_fire());

    /* Supply damage */
    supplies_damage(set_fire_destroy, inv, resist_fire());

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_FIRE, false);
    int inv;
    int resistance = resist_fire();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_FIRE, false);
    }

    log_elemental_damage_context("fire_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Inventory damage */
    inven_damage(set_fire_destroy, inv, resistance);

    /* Supply damage */
    supplies_damage(set_fire_destroy, inv, resistance);

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_mixed(int dam, cptr kb_str)
{
    int inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);

    /* Inventory damage */
    inven_damage(set_cold_destroy, inv, resist_cold());

    /* Supply damage */
    supplies_damage(set_cold_destroy, inv, resist_cold());

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_COLD, false);
    int inv;
    int resistance = resist_cold();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    inv = (dam < 10) ? 1 : (dam < 20) ? 2 : 3;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_COLD, false);
    }

    log_elemental_damage_context("cold_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Inventory damage */
    inven_damage(set_cold_destroy, inv, resistance);

    /* Supply damage */
    supplies_damage(set_cold_destroy, inv, resistance);

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Darkness from melee
 */
void dark_dam_mixed(int dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);
}

/*
 * Hurt the player with Darkness from breaths
 */
void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_DARK, false);
    int resistance = resist_dark();

    net_dam = dam / resistance;
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_DARK, false);
    }

    // 'pure' darkness attacks can also blind
    if (one_in_(resistance) && allow_player_blind(NULL))
    {
        (void)set_blind(p_ptr->blind + damroll(2, 4));
    }

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);
}

/*
 * Poison the player from melee
 */
void pois_dam_mixed(int dam)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Poison the player from breaths etc
 */
void pois_dam_pure(int dd, int ds, bool update_rolls)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_POIS, false);
    int resistance = resist_pois();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_POIS, false);
    }

    log_elemental_damage_context("pois_dam_pure", "poison", dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Increase a stat by one randomized level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 */
bool inc_stat(int stat)
{
    /* Cannot go above BASE_STAT_MAX */
    if (p_ptr->stat_base[stat] < BASE_STAT_MAX)
    {
        p_ptr->stat_base[stat]++;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to gain */
    return (false);
}

/*
 * Decreases a stat by a number of points.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.
 */
bool dec_stat(int stat, int amount, bool permanent)
{
    int result = false;

    /* Temporary damage */
    if (!permanent)
    {
        p_ptr->stat_drain[stat] -= amount;
        result = true;
    }

    /* Permanent damage */
    if (permanent && (p_ptr->stat_base[stat] > 0))
    {
        if (amount > p_ptr->stat_base[stat])
            p_ptr->stat_base[stat] = 0;
        else
            p_ptr->stat_base[stat] -= amount;

        result = true;
    }

    /* Apply changes */
    if (result)
    {
        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);
    }

    /* Done */
    return (result);
}

/*
 * Restore a stat by the number of points.
 * Return true only if this actually makes a difference.
 */
bool res_stat(int stat, int points)
{
    /* Restore if needed */
    if (p_ptr->stat_drain[stat] < 0)
    {
        /* Restore */
        p_ptr->stat_drain[stat] += points;

        if (p_ptr->stat_drain[stat] > 0)
            p_ptr->stat_drain[stat] = 0;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to restore */
    return (false);
}

/*
 * Inflict disease on the character.
 */
void disease(int* damage)
{
    int con, attempts;
    int i;

    /* Get current constitution */
    con = p_ptr->stat_use[A_CON];

    /* Adjust damage and choose message based on constitution */
    if (con < -2)
    {
        msg_print("You feel deathly ill.");
        *damage *= 2;
    }

    else if (con < 0)
    {
        msg_print("You feel seriously ill.");
    }

    else if (con < 2)
    {
        msg_print("You feel quite ill.");
        *damage = *damage * 2 / 3;
    }

    else if (con < 5)
    {
        msg_print("You feel ill.");
        *damage /= 2;
    }

    else if (con < 7)
    {
        msg_print("You feel sick.");
        *damage /= 3;
    }

    else
    {
        msg_print("You feel a bit sick.");
        *damage /= 4;
    }

    /* Infect the character (fully cumulative) */
    set_poisoned(p_ptr->poisoned + *damage + 1);

    /* Determine # of stat-reduction attempts */
    attempts = (5 + *damage) / 5;

    /* Attack stats */
    for (i = 0; i < attempts; i++)
    {
        /* Each attempt has a 10% chance of success */
        if (one_in_(10))
        {
            /* Damage a random stat */
            (void)do_dec_stat(rand_int(A_MAX), NULL);
        }
    }
}

/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "true" if the player notices anything.
 *
 * Sil-y: this presently brings att, evn, dd, ds, pd, ds down towards their base
 * values by one point each
 */
bool apply_disenchant(int mode)
{
    int t = 0;

    object_type* o_ptr;

    object_kind* k_ptr;

    char o_name[80];

    /* Unused parameter */
    (void)mode;

    /* Pick a random slot */
    switch (dieroll(8))
    {
    case 1:
        t = INVEN_WIELD;
        break;
    case 2:
        t = INVEN_BOW;
        break;
    case 3:
        t = INVEN_BODY;
        break;
    case 4:
        t = INVEN_OUTER;
        break;
    case 5:
        t = INVEN_ARM;
        break;
    case 6:
        t = INVEN_HEAD;
        break;
    case 7:
        t = INVEN_HANDS;
        break;
    case 8:
        t = INVEN_FEET;
        break;
    }

    /* Get the item */
    o_ptr = &inventory[t];

    k_ptr = &k_info[o_ptr->k_idx];

    /* No item, nothing happens */
    if (!o_ptr->k_idx)
        return (false);

    /* Check to see if it is disenchantable */

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Artefacts have 60% chance to resist */
    if (artefact_p(o_ptr) && percent_chance(60))
    {
        /* Message */
        msg_format("Your %s (%c) resist%s disenchantment!", o_name,
            index_to_label(t), ((o_ptr->number != 1) ? "" : "s"));

        /* Notice */
        return (true);
    }

    /* Do the disenchanting */
    if (o_ptr->att > k_ptr->att)
        o_ptr->att--;
    if (o_ptr->evn > k_ptr->evn)
        o_ptr->evn--;
    if (o_ptr->ds > k_ptr->ds)
        o_ptr->ds--;
    if (o_ptr->dd > k_ptr->dd)
        o_ptr->dd--;
    if (o_ptr->ps > k_ptr->ps)
        o_ptr->ps--;
    if (o_ptr->pd > k_ptr->pd)
        o_ptr->pd--;

    msg_format("Your %s (%c) %s disenchanted!", o_name, index_to_label(t),
        ((o_ptr->number != 1) ? "were" : "was"));

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

    /* Notice */
    return (true);
}
