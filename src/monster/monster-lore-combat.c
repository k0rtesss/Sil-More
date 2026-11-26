/* File: monster-lore-combat.c */

#include "monster-internal.h"
#include "ui/ui-language.h"

void describe_monster_spells(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    int m, n;
    int msex = 0;
    int spower;
    int vn;
    cptr vp[64];
    int attack = -1;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Get spell power */
    spower = r_ptr->spell_power;

    /* Collect innate attacks */
    vn = 0;

    if (l_ptr->flags4 & (RF4_ARROW1))
    {
        vp[vn++] = "fire a shortbow";
        attack = 96 + 0;
    }

    if (l_ptr->flags4 & (RF4_ARROW2))
    {
        vp[vn++] = "fire a longbow";
        attack = 96 + 1;
    }

    if (l_ptr->flags4 & (RF4_BOULDER))
    {
        vp[vn++] = "throw boulders";
        attack = 96 + 2;
    }

    if (l_ptr->flags4 & (RF4_THROW_WEB))
    {
        vp[vn++] = "throw web";
        attack = 96 + 23;
    }

    /* Describe innate attacks */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
            {
                text_out(ui_text(" may "));
            }

            else if (n < vn - 1)
                text_out(", ");
            else if (n == 1)
                text_out(ui_text(" or "));
            else
                text_out(ui_text(", or "));

            /* Dump */
            text_out_c(TERM_L_RED, vp[n]);
        }

        /* Damage */
        if (attack > -1)
        {
            // RF4_ARROW1
            if (attack == 96 + 0)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 1d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            // RF4_ARROW2
            else if (attack == 96 + 1)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 2d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            // GF_BOULDER
            else if (attack == 96 + 2)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 6d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            else if (attack == 96 + 23)
            {
                text_out_c(TERM_UMBER, format(" (%+d)", r_ptr->spell_power));
            }
        }

        /* End */
        text_out(".  ");
    }

    /* Collect breaths */
    vn = 0;

    if (l_ptr->flags4 & (RF4_BRTH_FIRE))
        vp[vn++] = "fire";
    if (l_ptr->flags4 & (RF4_BRTH_COLD))
        vp[vn++] = "frost";
    if (l_ptr->flags4 & (RF4_BRTH_POIS))
        vp[vn++] = "poison";
    if (l_ptr->flags4 & (RF4_BRTH_DARK))
        vp[vn++] = "darkness";

    /* Describe breaths */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(ui_text(" may breathe "));
            else if (n < vn - 1)
                text_out(", ");
            else if (n == 1)
                text_out(ui_text(" or "));
            else
                text_out(ui_text(", or "));

            /* Dump */
            text_out_c(TERM_L_RED, vp[n]);
        }

        text_out(format(" (%dd%d). ", spower, 4));
    }

    /* Collect spells */
    vn = 0;

    if (l_ptr->flags4 & (RF4_EARTHQUAKE))
        vp[vn++] = "cause earthquakes";
    if (l_ptr->flags4 & (RF4_SHRIEK))
        vp[vn++] = "call for help";
    if (l_ptr->flags4 & (RF4_SCREECH))
        vp[vn++] = "cause a hideous screeching";
    if (l_ptr->flags4 & (RF4_DARKNESS))
        vp[vn++] = "create darkness";
    if (l_ptr->flags4 & (RF4_FORGET))
        vp[vn++] = "remove memories";
    if (l_ptr->flags4 & (RF4_SCARE))
        vp[vn++] = "terrify";
    if (l_ptr->flags4 & (RF4_CONF))
        vp[vn++] = "confuse";
    if (l_ptr->flags4 & (RF4_HOLD))
        vp[vn++] = "entrance";
    if (l_ptr->flags4 & (RF4_SLOW))
        vp[vn++] = "slow";
    if (l_ptr->flags4 & (RF4_DIM))
        vp[vn++] = "dim torches and lanterns";

    m = vn;

    /* Describe spells */
    if (vn)
    {
        /* Intro */
        text_out(format(ui_text("%^s may "), ui_text(wd_he[msex])));

        /* Normal spells */
        for (n = 0; n < m; n++)
        {
            if (n == 0)
                text_out(ui_text("attempt to "));
            else if (n < m - 1)
                text_out(", ");
            else if (n != 1)
                text_out(ui_text(", or "));
            else
                text_out(ui_text(" or "));

            /* Dump */
            text_out_c(TERM_ORANGE, vp[n]);
        }

        /* End this sentence */
        text_out(".  ");
    }
}

void describe_monster_drop(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    int count = l_ptr->drop_item;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Drops items */
    if (count)
    {
        bool single = count == 1;
        bool chest = (l_ptr->flags1 & RF1_DROP_CHEST) != 0;
        bool artefact = (l_ptr->flags3 & RF3_DROP_ARTEFACT) != 0;
        cptr item;

        if (r_ptr->flags2 & (RF2_TERRITORIAL))
        {
            /* Territorial monsters do not carry loot off; they keep it.
             * An artefact-bearer hoards its treasure rather than guarding
             * a scattering of lesser finds. */
            if (artefact)
                text_out(format(ui_text("%^s hoards"), ui_text(wd_he[msex])));
            else
                text_out(format(ui_text("%^s may be found with"), ui_text(wd_he[msex])));
        }
        else
        {
            /* Intro */
            text_out(format(ui_text("%^s may carry"), ui_text(wd_he[msex])));
        }

        if (count == 2)
            text_out(ui_text(" one or two"));
        else if (!single)
            text_out(format(ui_text(" up to %d"), count));

        /* Translate the article, quality and noun together. */
        if (artefact && chest)
            item = single ? " a legendary chest" : " legendary chests";
        else if (artefact)
            item = single ? " a legendary object" : " legendary objects";
        else if (chest)
            item = single ? " a chest" : " chests";
        else if (l_ptr->flags2 & RF2_DROP_SUPERB)
            item = single ? " a superb object" : " superb objects";
        else if (l_ptr->flags1 & RF1_DROP_GREAT)
            item = single ? " an exceptional object" : " exceptional objects";
        else if (l_ptr->flags1 & RF1_DROP_GOOD)
            item = single ? " a good object" : " good objects";
        else
            item = single ? " an object" : " objects";

        text_out(ui_text(item));
        if (artefact)
            text_out(ui_text(", including an artefact"));

        /* End this sentence */
        text_out(".  ");
    }
}

void describe_monster_attack(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    int m, r, n;
    cptr p, q;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Count the number of "known" attacks */
    for (n = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
    {
        /* Skip non-attacks */
        if (!r_ptr->blow[m].method)
            continue;

        /* Count known attacks */
        if ((l_ptr->blows[m]) || lore_knows_lament_stats(l_ptr)
            || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR))
            n++;
    }

    /* Examine (and count) the actual attacks */
    for (r = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
    {
        int method, effect, att, d1, d2;

        /* Skip non-attacks */
        if (!r_ptr->blow[m].method)
            continue;

        /* Skip unknown attacks */
        if (!l_ptr->blows[m] && !lore_knows_lament_stats(l_ptr))
            continue;

        /* Extract the attack info */
        method = r_ptr->blow[m].method;
        effect = r_ptr->blow[m].effect;
        att = r_ptr->blow[m].att;
        d1 = r_ptr->blow[m].dd;
        d2 = r_ptr->blow[m].ds;

        if (m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            int dd_reduction = live->blow_dd_reduction[m];
            int ds_reduction = live->blow_ds_reduction[m];

            if (d1 > 0 && dd_reduction > 0)
            {
                if (dd_reduction >= d1)
                    d1 = 1;
                else
                    d1 = MAX(1, d1 - dd_reduction);
            }
            if (d2 > 0 && ds_reduction > 0)
            {
                if (ds_reduction >= d2)
                    d2 = 1;
                else
                    d2 = MAX(1, d2 - ds_reduction);
            }

            att = total_monster_attack(live, att);
        }

        /* No method yet */
        p = NULL;

        /* Get the method */
        switch (method)
        {
        case RBM_HIT:
            p = "hit";
            break;
        case RBM_TOUCH:
            p = "touch (ignoring armour)";
            break;
        case RBM_CLAW:
            p = "claw";
            break;
        case RBM_BITE:
            p = "bite";
            break;
        case RBM_PECK:
            p = "peck";
            break;
        case RBM_STING:
            p = "sting";
            break;
        case RBM_WHIP:
            p = "whip";
            break;
        case RBM_CRUSH:
            p = "crush";
            break;
        case RBM_ENGULF:
            p = "engulf";
            break;
        case RBM_CRAWL:
            p = "crawl on you";
            break;
        case RBM_THORN:
            p = "pierce you with thorns";
            break;
        case RBM_SPORE:
            p = "release spores (ignoring evasion and armour)";
            break;
        }

        /* Default effect */
        q = NULL;

        /* Get the effect */
        switch (effect)
        {
        case RBE_HURT:
            q = "attack";
            break;
        case RBE_WOUND:
            q = "wound";
            break;
        case RBE_BATTER:
            q = "stun";
            break;
        case RBE_SHATTER:
            q = "cause earthquakes";
            break;

        case RBE_UN_BONUS:
            q = "disenchant";
            break;
        case RBE_UN_POWER:
            q = "drain charges";
            break;
        case RBE_LOSE_MANA:
            q = "drain mana";
            break;
        case RBE_SLOW:
            q = "slow";
            break;
        case RBE_EAT_ITEM:
            q = "steal items";
            break;
        case RBE_EAT_FOOD:
            q = "eat your food";
            break;
        case RBE_HUNGER:
            q = "cause hunger";
            break;

        case RBE_POISON:
            q = "poison";
            break;
        case RBE_ACID:
            q = "corrode";
            break;
        case RBE_ELEC:
            q = "shock";
            break;
        case RBE_FIRE:
            q = "burn";
            break;
        case RBE_COLD:
            q = "freeze";
            break;
        case RBE_DARK:
            q = "darken";
            break;

        case RBE_BLIND:
            q = "blind";
            break;
        case RBE_CONFUSE:
            q = "confuse";
            break;
        case RBE_TERRIFY:
            q = "terrify";
            break;
        case RBE_ENTRANCE:
            q = "entrance";
            break;
        case RBE_HALLU:
            q = "induce hallucinations";
            break;
        case RBE_DISEASE:
            q = "cause disease";
            break;

        case RBE_LOSE_STR:
            q = "reduce strength";
            break;
        case RBE_LOSE_DEX:
            q = "reduce dexterity";
            break;
        case RBE_LOSE_CON:
            q = "reduce constitution";
            break;
        case RBE_LOSE_GRA:
            q = "reduce grace";
            break;
        case RBE_LOSE_STR_CON:
            q = "reduce strength and constitution";
            break;
        case RBE_LOSE_ALL:
            q = "reduce all stats";
            break;

        case RBE_DISARM:
            q = "disarm";
            break;
        }

        /* Introduce the attack description */
        if (!r)
        {
            text_out(format(ui_text("%^s can "), ui_text(wd_he[msex])));
        }
        else if (r < n - 1)
        {
            text_out(", ");
        }
        else
        {
            text_out(ui_text(", or "));
        }

        /* Hack -- force a method */
        if (!p)
            p = "do something weird";

        /* Describe the method */
        text_out(p);

        /* Describe the effect (if any) */
        if (q)
        {
            /* Describe the attack type */
            text_out(ui_text(" to "));
            text_out_c(TERM_L_RED, q);

            /* Describe damage (if known) */
            if ((know_damage(l_ptr, m)) || lore_knows_lament_stats(l_ptr)
                || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR))
            {
                if (d1 > 0 && d2 > 0)
                {
                    text_out_c(
                        TERM_L_WHITE, format(" (%+d, %dd%d)", att, d1, d2));
                }
                else
                {
                    /* Display the attack rating */
                    text_out_c(TERM_L_WHITE, format(" (%+d)", att));
                }
            }
        }

        /* Count the attacks as printed */
        r++;
    }

    /* Finish sentence above */
    if (r)
    {
        text_out(".  ");
    }

    /* Notice lack of attacks */
    else if (l_ptr->flags1 & RF1_NEVER_BLOW)
    {
        text_out(format(ui_text("%^s has no physical attacks.  "), ui_text(wd_he[msex])));
    }

    /* Or describe the lack of knowledge */
    else
    {
        text_out(format(ui_text("Nothing is known about %s attack.  "), ui_text(wd_his[msex])));
    }
}

void describe_monster_abilities(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int n;

    int vn;
    cptr vp[64];

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Collect Abilities */
    vn = 0;
    if (r_ptr->flags2 & (RF2_ELFBANE))
        vp[vn++] = "elf-bane"; // elf-bane is obvious
    if (r_ptr->flags4 & (RF4_NOLDORBANE))
        vp[vn++] = "noldor-bane"; // noldor-bane is obvious
    if (r_ptr->flags4 & (RF4_SINDARBANE))
        vp[vn++] = "sindar-bane"; // sindar-bane is obvious
    if (r_ptr->flags4 & (RF4_DWARFBANE))
        vp[vn++] = "dwarf-bane"; // dwarf-bane is obvious
    if (r_ptr->flags4 & (RF4_EDAINBANE))
        vp[vn++] = "edain-bane"; // edain-bane is obvious
    if (l_ptr->flags2 & (RF2_CHARGE))
        vp[vn++] = "charge";
    if (l_ptr->flags2 & (RF2_KNOCK_BACK))
        vp[vn++] = "knock back";
    if (l_ptr->flags2 & (RF2_CRIPPLING))
        vp[vn++] = "crippling shot";
    if (l_ptr->flags2 & (RF2_CRUEL_BLOW))
        vp[vn++] = "cruel blow";
    if (l_ptr->flags2 & (RF2_OPPORTUNIST))
        vp[vn++] = "opportunist";
    if (l_ptr->flags2 & (RF2_ZONE_OF_CONTROL))
        vp[vn++] = "zone of control";
    if (l_ptr->flags2 & (RF2_EXCHANGE_PLACES))
        vp[vn++] = "exchange places";
    if (l_ptr->flags2 & (RF2_RIPOSTE))
        vp[vn++] = "riposte";
    if (l_ptr->flags2 & (RF2_FLANKING))
        vp[vn++] = "flanking";
    if (l_ptr->flags5 & RF5_SMITE)
        vp[vn++] = "smite";
    if (l_ptr->flags5 & RF5_SPRINTING)
        vp[vn++] = "sprinting";
    if (l_ptr->flags5 & RF5_CONCENTRATION)
        vp[vn++] = "concentration";
    if (l_ptr->flags5 & RF5_DODGING)
        vp[vn++] = "dodging";
    if (l_ptr->flags5 & RF5_BLOCKING)
        vp[vn++] = "blocking";
    if (l_ptr->flags5 & RF5_VENGEANCE)
        vp[vn++] = "vengeance";
    if (l_ptr->flags5 & RF5_BRIDGE_BUILDER)
        vp[vn++] = "bridge building";
    if (l_ptr->flags4 & (RF4_SNG_BINDING))
        vp[vn++] = "song of binding";
    if (l_ptr->flags4 & (RF4_SNG_PIERCING))
        vp[vn++] = "song of piercing";
    if (l_ptr->flags4 & (RF4_SNG_OATHS))
        vp[vn++] = "song of oaths";
    if (l_ptr->flags4 & (RF4_HATCH_SPIDER))
        vp[vn++] = "hatch spider";
    if (l_ptr->flags4 & (RF4_RALLY))
        vp[vn++] = "rally foes";

    /* Describe Abilities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if ((n == 0) && (vn > 1))
                text_out(ui_text(" has the abilities: "));
            else if (n == 0)
                text_out(ui_text(" has the ability: "));
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(ui_text(" and "));

            /* Dump */
            text_out_c(TERM_RED, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Describe the timing and counterplay of observed stateful abilities. */
    if (l_ptr->flags5 & RF5_BRIDGE_BUILDER)
        text_out(ui_text("It can repair damaged crossings and build short bridges, "
            "using limited supplies and several actions of noisy work.  "));
    if (l_ptr->flags5 & RF5_SMITE)
    {
        text_out(format(ui_text("Smite spends %d effort on an immediate melee strike "
            "with a normal hit roll and maximum damage on every die, including "
            "critical dice. The next full action is spent recovering, even "
            "after a miss; reactions are also prevented during recovery.  "),
            MON_MANA_COST));
    }
    if (l_ptr->flags5 & RF5_SPRINTING)
    {
        text_out(ui_text("Sprinting raises Fast speed to Very Fast after four moves "
            "in a continuing direction. A sharp turn, attack, alarm, wait or "
            "lost action breaks the run.  "));
    }
    if (l_ptr->flags5 & RF5_CONCENTRATION)
    {
        text_out(format(ui_text("Concentration adds +1 accuracy after each ordinary "
            "melee attack, hit or miss, up to +%d. Moving, waiting, casting or "
            "losing an action resets it. Reactions neither build the bonus "
            "nor benefit from it.  "), MAX(0, r_ptr->per / 2)));
    }
    if (l_ptr->flags5 & RF5_DODGING)
    {
        text_out(ui_text("Dodging grants +3 evasion after moving, until the next "
            "action ends.  "));
    }
    if (l_ptr->flags5 & RF5_BLOCKING)
    {
        text_out(ui_text("Blocking doubles the shield's protection dice while "
            "holding position; body armour keeps its ordinary value. Moving "
            "loses this extra shield protection until a stationary action.  "));
    }
    if (l_ptr->flags5 & RF5_VENGEANCE)
    {
        text_out(ui_text("Taking melee damage that gets through protection charges "
            "Vengeance, adding one damage die to the next melee hit. The "
            "charge does not stack; a miss preserves it, and a hit spends it "
            "even if blocked. Ranged damage does not charge Vengeance.  "));
    }

    /* Collect special abilities. */
    vn = 0;
    if (r_ptr->light > 0)
    {
        /*humaniods carry torches, others glow*/
        if (!strchr("@G", r_ptr->d_char))
            vp[vn++] = "radiate light";
        else
            vp[vn++] = "use a light source";
    }
    if (r_ptr->light < 0)
        vp[vn++] = "produce an unnatural darkness";
    if (l_ptr->flags2 & RF2_FLYING)
        vp[vn++] = "fly over chasms and water without slowing";
    if (l_ptr->flags2 & RF2_OPEN_DOOR)
        vp[vn++] = "open doors";
    if (l_ptr->flags2 & RF2_PASS_DOOR)
        vp[vn++] = "pass through doors";
    if (l_ptr->flags2 & RF2_UNLOCK_DOOR)
        vp[vn++] = "unlock doors";
    if (l_ptr->flags2 & RF2_BASH_DOOR)
        vp[vn++] = "bash down doors";
    if (l_ptr->flags2 & RF2_PASS_WALL)
        vp[vn++] = "pass through walls";
    if (l_ptr->flags2 & RF2_KILL_WALL)
        vp[vn++] = "bore through walls";
    if (l_ptr->flags2 & RF2_TUNNEL_WALL)
        vp[vn++] = "tunnel through walls";
    if (l_ptr->flags2 & RF2_KILL_BODY)
        vp[vn++] = "destroy weaker monsters";
    if (l_ptr->flags2 & RF2_TAKE_ITEM)
        vp[vn++] = "pick up objects";
    if (l_ptr->flags2 & RF2_KILL_ITEM)
        vp[vn++] = "destroy objects";

    /* Describe special abilities. */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(ui_text(" can "));
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(ui_text(" and "));

            /* Dump */
            text_out(vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Describe special abilities. */
    if (l_ptr->flags2 & RF2_INVISIBLE)
    {
        text_out(format(ui_text("%^s is very difficult to see.  "), ui_text(wd_he[msex])));
    }
    if (l_ptr->flags2 & RF2_MULTIPLY)
    {
        text_out(format(ui_text("%^s breeds explosively.  "), ui_text(wd_he[msex])));
    }
    if (l_ptr->flags2 & RF2_REGENERATE)
    {
        text_out(format(ui_text("%^s regenerates quickly.  "), ui_text(wd_he[msex])));
    }
    if (l_ptr->flags1 & RF1_NO_CRIT)
    {
        text_out(format(ui_text("%^s cannot be critically hit.  "), ui_text(wd_he[msex])));
    }
    if (l_ptr->flags1 & RF1_RES_CRIT)
    {
        text_out(format(ui_text("%^s is resistant to critical hits.  "), ui_text(wd_he[msex])));
    }

    if (l_ptr->flags2 & (RF2_CLOUD_SURROUND))
    {
        int typ = 0, dd = 0, ds = 0, rad = 0;

        /* Get type of cloud */
        cloud_surround(r_idx, &typ, &dd, &ds, &rad);

        /*hack - alter type for char-attr monster*/

        if ((r_ptr->flags4 & (RF4_BRTH_FIRE))
            && (r_ptr->flags4 & (RF4_BRTH_POIS))
            && (r_ptr->flags4 & (RF4_BRTH_DARK))
            && (r_ptr->flags4 & (RF4_BRTH_COLD)))
        {
            text_out(
                format(ui_text("%^s is surrounded by shimmering haze of ever changing "
                       "elements (%dd%d).  "),
                    ui_text(wd_he[msex]), dd, ds));
        }

        /* We emit something */
        else if (typ)
        {
            text_out(format(
                ui_text("%^s is surrounded by a shimmering haze of "), ui_text(wd_he[msex])));

            /* Describe cloud */
            if (typ == GF_FIRE)
                text_out_c(TERM_L_RED, ui_text("fire"));
            else if (typ == GF_COLD)
                text_out_c(TERM_L_RED, ui_text("frost"));
            else if (typ == GF_POIS)
                text_out_c(TERM_L_RED, ui_text("poison"));
            else if (typ == GF_DARK)
                text_out_c(TERM_L_RED, ui_text("darkness"));
            text_out(format(" (%dd%d).  ", dd, ds));
        }
    }

    /* Describe intelligence. */
    if (r_ptr->flags2 & RF2_MINDLESS)
    {
        text_out(format(ui_text("%^s is mindless.  "), ui_text(wd_he[msex])));
    }
    else if (r_ptr->flags2 & RF2_SMART)
    {
        // text_out(format("%^s is a servant of the dark.  ", wd_he[msex]));
    }
    else
    {
        text_out(format(ui_text("%^s is unintelligent.  "), ui_text(wd_he[msex])));
    }

    /* Collect susceptibilities */
    vn = 0;
    if (l_ptr->flags3 & RF3_STONE)
        vp[vn++] = "shattering";
    if (l_ptr->flags3 & RF3_HURT_LITE)
        vp[vn++] = "bright light";
    if (l_ptr->flags3 & RF3_HURT_FIRE)
        vp[vn++] = "fire";
    if (l_ptr->flags3 & RF3_HURT_COLD)
        vp[vn++] = "cold";

    /* Describe susceptibilities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(ui_text(" is vulnerable to "));
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(ui_text(" and "));

            /* Dump */
            text_out_c(TERM_L_BLUE, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Collect resistances */
    vn = 0;
    if (l_ptr->flags3 & RF3_RES_ELEC)
        vp[vn++] = "lightning";
    if (l_ptr->flags3 & RF3_RES_FIRE)
        vp[vn++] = "fire";
    if (l_ptr->flags3 & RF3_RES_COLD)
        vp[vn++] = "cold";
    if (l_ptr->flags3 & RF3_RES_POIS)
        vp[vn++] = "poison";

    /* Describe immunities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(ui_text(" resists "));
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(ui_text(" and "));

            /* Dump */
            text_out(vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Collect non-effects */
    vn = 0;
    if (l_ptr->flags3 & RF3_NO_SLOW)
        vp[vn++] = "slowed";
    if (l_ptr->flags3 & RF3_NO_STUN)
        vp[vn++] = "stunned";
    if (l_ptr->flags3 & RF3_NO_FEAR)
        vp[vn++] = "frightened";
    if (l_ptr->flags3 & RF3_NO_CONF)
        vp[vn++] = "confused";
    if (l_ptr->flags3 & RF3_NO_SLEEP)
        vp[vn++] = "put to sleep";

    /* Describe non-effects */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", ui_text(wd_he[msex])));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(ui_text(" cannot be "));
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(ui_text(" or "));

            /* Dump */
            text_out_c(TERM_YELLOW, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Describe escort */
    if (l_ptr->flags1 & RF1_ESCORT)
    {
        text_out(format(ui_text("%^s usually appears with escorts.  "), ui_text(wd_he[msex])));
    }

    /* Describe escorts */
    if (l_ptr->flags1 & RF1_ESCORTS)
    {
        text_out(
            format(ui_text("%^s usually appears with many escorts.  "), ui_text(wd_he[msex])));
    }

    /* Describe friend */
    else if (l_ptr->flags1 & RF1_FRIEND)
    {
        text_out(format(ui_text("%^s sometimes appears in groups.  "), ui_text(wd_he[msex])));
    }

    /* Describe friends */
    else if (l_ptr->flags1 & RF1_FRIENDS)
    {
        text_out(format(ui_text("%^s usually appears in groups.  "), ui_text(wd_he[msex])));
    }
}

/* Compact recall uses the already-filtered lore supplied by describe_monster.
 * Unknown traits stay unknown; this must not inspect unobserved race flags. */
typedef struct compact_monster_flag {
    u32b flag;
    cptr label;
} compact_monster_flag;

static void compact_monster_flags(cptr heading, u32b flags,
    const compact_monster_flag* entries, int count, byte color)
{
    bool found = false;
    text_out_c(TERM_L_BLUE, heading);
    for (int i = 0; i < count; i++)
    {
        if (!(flags & entries[i].flag)) continue;
        if (found) text_out(", ");
        text_out_c(color, entries[i].label);
        found = true;
    }
    if (!found) text_out_c(TERM_SLATE, ui_text("unknown"));
    text_out("\n");
}

static cptr compact_blow_method(int method)
{
    switch (method)
    {
    case RBM_HIT: return "hit";
    case RBM_TOUCH: return "touch (ignores armour)";
    case RBM_CLAW: return "claw";
    case RBM_BITE: return "bite";
    case RBM_PECK: return "peck";
    case RBM_STING: return "sting";
    case RBM_WHIP: return "whip";
    case RBM_CRUSH: return "crush";
    case RBM_ENGULF: return "engulf";
    case RBM_CRAWL: return "crawl";
    case RBM_THORN: return "thorns";
    case RBM_SPORE: return "spores (ignore evasion and armour)";
    default: return "attack";
    }
}

static cptr compact_blow_effect(int effect)
{
    switch (effect)
    {
    case RBE_HURT: return "damage";
    case RBE_WOUND: return "wound";
    case RBE_BATTER: return "stun";
    case RBE_SHATTER: return "earthquakes";
    case RBE_UN_BONUS: return "disenchant";
    case RBE_UN_POWER: return "drain charges";
    case RBE_LOSE_MANA: return "drain voice";
    case RBE_SLOW: return "slow";
    case RBE_EAT_ITEM: return "steal items";
    case RBE_EAT_FOOD: return "eat food";
    case RBE_DARK: return "darken";
    case RBE_HUNGER: return "hunger";
    case RBE_POISON: return "poison";
    case RBE_ACID: return "corrode";
    case RBE_ELEC: return "lightning";
    case RBE_FIRE: return "fire";
    case RBE_COLD: return "cold";
    case RBE_BLIND: return "blind";
    case RBE_CONFUSE: return "confuse";
    case RBE_TERRIFY: return "fear";
    case RBE_ENTRANCE: return "entrance";
    case RBE_HALLU: return "hallucination";
    case RBE_DISEASE: return "disease";
    case RBE_LOSE_STR: return "drain Strength";
    case RBE_LOSE_DEX: return "drain Dexterity";
    case RBE_LOSE_CON: return "drain Constitution";
    case RBE_LOSE_GRA: return "drain Grace";
    case RBE_LOSE_STR_CON: return "drain Strength and Constitution";
    case RBE_LOSE_ALL: return "drain attributes";
    case RBE_DISARM: return "disarm";
    default: return NULL;
    }
}

void describe_monster_compact(int r_idx, const monster_lore* lore,
    const monster_type* live)
{
    static const compact_monster_flag resistances[] = {
        {RF3_RES_ELEC, "lightning"}, {RF3_RES_FIRE, "fire"},
        {RF3_RES_COLD, "cold"}, {RF3_RES_POIS, "poison"}
    };
    static const compact_monster_flag weaknesses[] = {
        {RF3_STONE, "shattering"}, {RF3_HURT_LITE, "bright light"},
        {RF3_HURT_FIRE, "fire"}, {RF3_HURT_COLD, "cold"}
    };
    static const compact_monster_flag immunities[] = {
        {RF3_NO_SLOW, "slow"}, {RF3_NO_STUN, "stun"},
        {RF3_NO_FEAR, "fear"}, {RF3_NO_CONF, "confusion"},
        {RF3_NO_SLEEP, "sleep"}
    };
    static const compact_monster_flag ranged[] = {
        {RF4_ARROW1, "shortbow"}, {RF4_ARROW2, "longbow"},
        {RF4_BOULDER, "boulders"}, {RF4_THROW_WEB, "webs"},
        {RF4_BRTH_FIRE, "fire breath"}, {RF4_BRTH_COLD, "cold breath"},
        {RF4_BRTH_POIS, "poison breath"}, {RF4_BRTH_DARK, "dark breath"},
        {RF4_EARTHQUAKE, "earthquake"}, {RF4_SHRIEK, "calls for help"},
        {RF4_SCREECH, "stunning screech"}, {RF4_DARKNESS, "darkness"},
        {RF4_FORGET, "forgetfulness"}, {RF4_SCARE, "fear"},
        {RF4_CONF, "confusion"}, {RF4_HOLD, "entrancement"},
        {RF4_SLOW, "slowing"}, {RF4_DIM, "dims lights"},
        {RF4_HATCH_SPIDER, "hatches spiders"}, {RF4_RALLY, "rallies foes"},
        {RF4_SNG_BINDING, "Song of Binding"},
        {RF4_SNG_PIERCING, "Song of Piercing"},
        {RF4_SNG_OATHS, "Song of Oaths"}
    };
    static const compact_monster_flag abilities2[] = {
        {RF2_CHARGE, "Charge"}, {RF2_KNOCK_BACK, "Knock Back"},
        {RF2_CRIPPLING, "Crippling Shot"}, {RF2_CRUEL_BLOW, "Cruel Blow"},
        {RF2_OPPORTUNIST, "Opportunist"}, {RF2_ZONE_OF_CONTROL, "Zone of Control"},
        {RF2_EXCHANGE_PLACES, "Exchange Places"}, {RF2_RIPOSTE, "Riposte"},
        {RF2_FLANKING, "Flanking"}, {RF2_INVISIBLE, "Invisible"},
        {RF2_MULTIPLY, "Multiplies"}, {RF2_REGENERATE, "Regenerates"},
        {RF2_CLOUD_SURROUND, "Damaging cloud"},
        {RF2_TERRITORIAL, "Territorial"}, {RF2_FLYING, "Flies"},
        {RF2_OPEN_DOOR, "Opens doors"}, {RF2_UNLOCK_DOOR, "Unlocks doors"},
        {RF2_BASH_DOOR, "Bashes doors"}, {RF2_PASS_DOOR, "Passes doors"},
        {RF2_PASS_WALL, "Passes walls"}, {RF2_KILL_WALL, "Destroys walls"},
        {RF2_TUNNEL_WALL, "Tunnels"}
    };
    /* As in full recall, these bane marks are obvious by appearance. */
    static const compact_monster_flag obvious_banes[] = {
        {RF4_NOLDORBANE, "Noldor-bane"}, {RF4_SINDARBANE, "Sindar-bane"},
        {RF4_DWARFBANE, "Dwarf-bane"}, {RF4_EDAINBANE, "Edain-bane"}
    };
    static const compact_monster_flag abilities5[] = {
        {RF5_SMITE, "Smite"}, {RF5_SPRINTING, "Sprinting"},
        {RF5_CONCENTRATION, "Concentration"}, {RF5_DODGING, "Dodging"},
        {RF5_BLOCKING, "Blocking"}, {RF5_VENGEANCE, "Vengeance"},
        {RF5_BRIDGE_BUILDER, "Bridge building"}
    };
    const monster_race* race = &r_info[r_idx];
    bool found = false;
    int speed = live && live->mspeed ? live->mspeed : race->speed;
    int speed_index = MAX(0, MIN(speed, 7));

    text_out_c(TERM_L_BLUE, ui_text("Speed: "));
    if (lore->flags1 & RF1_NEVER_MOVE)
        text_out(ui_text("cannot move"));
    else if (lore->flags1 & RF1_HIDDEN_MOVE)
        text_out(ui_text("moves only when unseen"));
    else
        text_out(format(ui_text("%d (%d%% of normal)"), speed,
            (extract_energy[speed_index] * 100) / extract_energy[2]));
    if (lore->flags1 & (RF1_RAND_25 | RF1_RAND_50))
        text_out(ui_text(", erratic"));
    if (live && live->hasted) text_out(ui_text(", hasted"));
    if (live && live->slowed) text_out(ui_text(", slowed"));
    text_out("\n");

    compact_monster_flags("Resists: ", lore->flags3, resistances,
        N_ELEMENTS(resistances), TERM_WHITE);
    compact_monster_flags("Vulnerable: ", lore->flags3, weaknesses,
        N_ELEMENTS(weaknesses), TERM_L_GREEN);
    compact_monster_flags("Immune to: ", lore->flags3, immunities,
        N_ELEMENTS(immunities), TERM_WHITE);
    if (lore->flags1 & RF1_NO_CRIT)
        text_out(ui_text("Critical hits: immune\n"));
    else if (lore->flags1 & RF1_RES_CRIT)
        text_out(ui_text("Critical hits: resistant\n"));

    text_out_c(TERM_L_BLUE, ui_text("Melee: "));
    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        const monster_blow* blow = &race->blow[i];
        cptr effect;
        int attack, dice, sides;
        if (!blow->method || (!lore->blows[i] && !lore_knows_lament_stats(lore)))
            continue;
        if (found) text_out("; ");
        text_out_c(TERM_L_RED, compact_blow_method(blow->method));
        effect = compact_blow_effect(blow->effect);
        if (effect) text_out(format(": %s", effect));
        attack = blow->att; dice = blow->dd; sides = blow->ds;
        if (live)
        {
            attack = total_monster_attack((monster_type*)live, attack);
            if (dice > 0 && live->blow_dd_reduction[i] > 0)
                dice = MAX(1, dice - live->blow_dd_reduction[i]);
            if (sides > 0 && live->blow_ds_reduction[i] > 0)
                sides = MAX(1, sides - live->blow_ds_reduction[i]);
        }
        if (know_damage(lore, i) || lore_knows_lament_stats(lore))
        {
            if (dice > 0 && sides > 0)
                text_out(format(" (%+d, %dd%d)", attack, dice, sides));
            else
                text_out(format(" (%+d)", attack));
        }
        found = true;
    }
    if (!found)
        text_out_c(TERM_SLATE, (lore->flags1 & RF1_NEVER_BLOW)
            ? ui_text("no physical attacks") : ui_text("unknown"));
    text_out("\n");

    compact_monster_flags("Ranged and spells: ", lore->flags4, ranged,
        N_ELEMENTS(ranged), TERM_L_RED);
    text_out_c(TERM_L_BLUE, ui_text("Abilities: "));
    found = false;
    if (race->flags2 & RF2_ELFBANE)
    {
        text_out(ui_text("Elf-bane"));
        found = true;
    }
    for (int i = 0; i < (int)N_ELEMENTS(obvious_banes); i++)
        if (race->flags4 & obvious_banes[i].flag)
        {
            if (found) text_out(", ");
            text_out(obvious_banes[i].label); found = true;
        }
    for (int i = 0; i < (int)N_ELEMENTS(abilities2); i++)
        if (lore->flags2 & abilities2[i].flag)
        {
            if (found) text_out(", ");
            text_out(abilities2[i].label); found = true;
        }
    for (int i = 0; i < (int)N_ELEMENTS(abilities5); i++)
        if (lore->flags5 & abilities5[i].flag)
        {
            if (found) text_out(", ");
            text_out(abilities5[i].label); found = true;
        }
    if (!found) text_out_c(TERM_SLATE, ui_text("unknown"));
    text_out("\n");
}
