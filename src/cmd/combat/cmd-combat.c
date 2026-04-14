/* File: cmd-combat.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-select.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "platform-frame.h"
#include "platform-time.h"

static bool valorous_oath_blocks_auto_attack(monster_type* m_ptr);

static bool polearm_is_axe(const object_type* weapon)
{
    if (!weapon)
        return false;

    if (weapon->tval != TV_POLEARM)
        return false;

    switch (weapon->sval)
    {
    case SV_BATTLE_AXE:
    case SV_GREAT_AXE:
        return true;
    default:
        return false;
    }
}

static bool sword_is_medium(const object_type* weapon)
{
    if (!weapon || weapon->tval != TV_SWORD)
        return false;

    switch (weapon->sval)
    {
    case SV_LONG_SWORD:
    case SV_BASTARD_SWORD:
        return true;
    default:
        return false;
    }
}

static bool sword_is_great(const object_type* weapon)
{
    if (!weapon || weapon->tval != TV_SWORD)
        return false;

    switch (weapon->sval)
    {
    case SV_GREAT_SWORD:
    case SV_STAR_IRON_GREAT_SWORD:
        return true;
    default:
        return false;
    }
}

static bool weapon_is_spear(const object_type* weapon)
{
    if (!weapon || weapon->tval != TV_POLEARM)
        return false;

    return (weapon->sval == SV_SPEAR || weapon->sval == SV_GREAT_SPEAR);
}

static u16b weapon_sound_message_type(const object_type* weapon, bool hit)
{
    u16b fallback = hit ? MSG_HIT : MSG_MISS;

    if (!weapon || weapon->k_idx == 0)
        return MSG_WEAPON_UNARMED;

    if (weapon->weight == 0)
        return MSG_WEAPON_UNARMED;

    switch (weapon->tval)
    {
    case TV_SWORD:
        if (sword_is_great(weapon))
            return MSG_WEAPON_SLASH_HEAVY;
        else if (sword_is_medium(weapon))
            return MSG_WEAPON_SLASH_MEDIUM;
        else
            return MSG_WEAPON_SLASH_LIGHT;
    case TV_POLEARM:
        if (weapon->sval == SV_HAND_AXE)
            return MSG_WEAPON_SLASH_LIGHT;
        else if (weapon->sval == SV_BATTLE_AXE)
            return MSG_WEAPON_SLASH_MEDIUM;
        else if (polearm_is_axe(weapon))
            return MSG_WEAPON_SLASH_HEAVY;
        else
            return MSG_WEAPON_THRUST;
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_STAFF:
    case TV_LIGHT:
    case TV_HORN:
        return MSG_WEAPON_BLUNT;
    default:
        break;
    }

    return fallback;
}

static int weapon_animation_delay(u16b weapon_sound_type)
{
    switch (weapon_sound_type)
    {
    case MSG_WEAPON_SLASH_LIGHT:
    case MSG_WEAPON_UNARMED:
        return 300;
    case MSG_WEAPON_SLASH_MEDIUM:
    case MSG_WEAPON_THRUST:
        return 350;
    case MSG_WEAPON_SLASH_HEAVY:
    case MSG_WEAPON_BLUNT:
        return 400;
    default:
        return 350; // fallback to medium
    }
}

bool graphics_are_ascii(void)
{
    return use_graphics == GRAPHICS_NONE || use_graphics == GRAPHICS_PSEUDO;
}

/*
 * Puts an item in the player's inventory.
 * If the inventory would overflow, this is handled at the start of the next
 * player turn.
 */

bool is_normal_attack(int attack_type)
{
    return (attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING)
        || (attack_type == ATT_CONTROLLED_RETREAT)
        || (attack_type == ATT_IMPALE);
}

/*
 * Search a single square for hidden things
 * (a utility function called by 'search' and 'perceive')
 */

extern bool check_hit(int power, bool display_roll)
{
    if (hit_roll(power, p_ptr->skill_use[S_EVN] + dodging_bonus(), NULL, PLAYER,
            display_roll)
        > 0)
        return (true);
    else
        return (false);
}

/*
 * Handle player hitting a real trap
 */
void hit_trap(int y, int x)
{
    int i, dam, prt, net_dam;
    int feat = cave_feat[y][x];

    cptr name = "a trap";

    /* Disturb the player */
    disturb(0, 0);

    // Store information for the combat rolls window
    combat_roll_special_char = (&f_info[feat])->d_char;
    combat_roll_special_attr = (&f_info[feat])->d_attr;

    if (p_ptr->avoid_traps && feat != FEAT_CHASM && feat != FEAT_TRAP_ROOST
        && feat != FEAT_TRAP_WEB && feat != FEAT_TRAP_PIT
        && feat != FEAT_TRAP_SPIKED_PIT)
    {
        msg_print("You carefully avoid a trap.");
        reveal_trap(y, x);
        ident_f3(TR3_AVOID_TRAPS, NULL);
        return;
    }

    /* Analyze XXX XXX XXX */
    switch (feat)
    {
        // not really a trap, but handled here due to similarities
    case FEAT_CHASM:
    {
        // give several messages so the player has a chance to see it happen
        msg_print("You fall into the darkness!");
        message_flush();
        if (p_ptr->depth >= MORGOTH_DEPTH)
        {
            msg_print("...and plunge into the abyss.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell into a chasm", p_ptr->depth);

            // chasms on the final level are fatal
            killer_mark_other(SCORE_KILLER_FALL);
            take_hit(p_ptr->chp + 1000, "falling into the abyss");
        }
        else
        {
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell into a chasm", p_ptr->depth);

            // take some damage
            falling_damage(false);

            // make a note if the player loses a greater vault
            note_lost_greater_vault();

            /* New depth */
            p_ptr->depth = MIN(p_ptr->depth + 2, MORGOTH_DEPTH);

            /* Leaving */
            p_ptr->leaving = true;
        }

        break;
    }

    case FEAT_TRAP_false_FLOOR:
    {
        // give several messages so the player has a chance to see it happen
        msg_print("The floor crumbles beneath you!");
        message_flush();
        msg_print("You fall through...");
        message_flush();
        msg_print("...and land somewhere deeper in the Iron Hells.");
        message_flush();

        // add to the notes file
        do_cmd_note("Fell through a false floor", p_ptr->depth);

        // take some damage
        falling_damage(false);

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth++;

        /* Leaving */
        p_ptr->leaving = true;

        break;
    }

    case FEAT_TRAP_PIT:
    {
        msg_print("You fall into a pit!");

        /* Falling damage */
        dam = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, name);

        /* Make some noise */
        stealth_score -= 5;

        break;
    }

    case FEAT_TRAP_SPIKED_PIT:
    {
        msg_print("You fall into a spiked pit!");

        /* Falling damage */
        dam = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, name);

        /* Extra spike damage */
        dam = damroll(4, 5);

        /* Protection */
        prt = protection_roll(GF_HURT, true);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(4, 5, dam, -1, -1, prt, 100, GF_HURT, true);

        if (net_dam > 0)
        {
            msg_print("You are impaled!");

            /* Take the damage */
            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(net_dam, name);

            (void)set_cut(p_ptr->cut + (net_dam + 1) / 2);
        }
        else
        {
            msg_print("Your armour protects you.");
        }

        /* Make some noise */
        stealth_score -= 10;

        break;
    }

    case FEAT_TRAP_DART:
    {
        if (check_hit(15, true))
        {
            dam = damroll(1, 15);
            prt = protection_roll(GF_HURT, false);

            if (dam > prt)
            {
                msg_print("A small dart hits you!");

                // do a tiny amount of damage
                killer_mark_other(SCORE_KILLER_TRAP);
                take_hit(1, name);

                update_combat_rolls2(
                    1, 15, prt + 1, -1, -1, prt, 100, GF_HURT, false);

                (void)do_dec_stat(A_STR, NULL);
            }
            else
            {
                msg_print(
                    "A small dart hits you, but is deflected by your armour.");

                update_combat_rolls2(
                    1, 15, dam, -1, -1, prt, 100, GF_HURT, false);
            }
        }
        else
        {
            msg_print("A small dart barely misses you.");
        }

        /* Make a small amount of noise */
        monster_perception(true, false, 5);

        break;
    }

    case FEAT_TRAP_FLASH:
    {
        if (!p_ptr->blind)
        {
            msg_print("There is a searing flash of light!");
            if (allow_player_blind(NULL))
            {
                (void)set_blind(p_ptr->blind + damroll(5, 4));
            }
            else
            {
                msg_print("Your vision quickly clears.");
            }
        }

        /* Make a small amount of noise */
        monster_perception(true, false, 5);

        break;
    }

    case FEAT_TRAP_GAS_CONF:
    {
        msg_print("A vapor fills the air and you feel yourself becoming "
                  "lightheaded.");
        if (allow_player_confusion(NULL))
        {
            (void)set_confused(p_ptr->confused + damroll(4, 4));
        }
        else
        {
            msg_print("You resist the effects!");
        }
        explosion(-1, 1, y, x, 3, 4, 10, GF_CONFUSION);

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_GAS_MEMORY:
    {
        msg_print("You are surrounded by a strange mist!");
        if (saving_throw(NULL, 0))
        {
            msg_print("You resist the effects!");
        }
        else
        {
            msg_print("Your memories fade away.");
            wiz_dark();
        }

        // Aesthetic explosion that does nothing
        explosion(-1, 1, y, x, 0, 0, 0, GF_NOTHING);

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_ACID:
    {
        msg_print("You are splashed with acid!");

        /* Acid damage */
        dam = damroll(4, 4);

        /* Protection */
        prt = protection_roll(GF_HURT, false);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(4, 4, dam, -1, -1, prt, 100, GF_HURT, false);

        acid_dam(dam, 4, 16, net_dam, "an acid trap");

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_IMPRISONMENT:
    {
        msg_print("Words of imprisonment echo through the halls!");
        (void)lock_doors_radius(y, x, 10, 10 + (p_ptr->depth / 2));

        break;
    }

    case FEAT_TRAP_ALARM:
    {
        if (singing(SNG_SILENCE))
        {
            msg_print("You hear the muffled toll of a bell above your head.");
        }
        else
        {
            msg_print("You hear a bell toll loudly above your head.");
        }

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }

    case FEAT_TRAP_CALTROPS:
    {
        if (skill_check(PLAYER, p_ptr->skill_use[S_PER], 10, NULL) > 0)
        {
            msg_print("You step carefully amidst a field of caltrops.");
        }
        else
        {
            msg_print("You step on a caltrop.");

            dam = damroll(1, 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(1, 4, dam, -1, -1, 0, 0, GF_HURT, true);

            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(dam, name);

            if (allow_player_slow(NULL))
            {
                msg_print("It pierces your foot.");
                set_slow(p_ptr->slow + damroll(4, 4));
            }
        }

        /* Make some noise */
        stealth_score -= 10;

        break;
    }

    case FEAT_TRAP_ROOST:
    {
        int count = 0;

        for (i = 0; i < 1000; i++)
        {
            if (count < 2)
            {
                count += summon_specific(y, x,
                    p_ptr->depth + damroll(2, 2) - damroll(2, 2),
                    SUMMON_BIRD_BAT);
            }
        }

        if (count >= 1)
        {
            msg_print("There is a flutter of wings from high above.");

            /* Forget the trap */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Remove the trap */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        break;
    }

    case FEAT_TRAP_WEB:
    {
        int count = 0;

        msg_print("You are caught in a vast black web.");

        for (i = 0; i < 1000; i++)
        {
            if (count < 1)
            {
                count += summon_specific(y, x,
                    p_ptr->depth + damroll(2, 2) - damroll(2, 2),
                    SUMMON_SPIDER);
            }
        }

        if (count >= 1)
        {
            msg_print("A spider descends from the gloom.");
        }

        break;
    }

    case FEAT_TRAP_DEADFALL:
    {
        int yy, xx;
        int sy = y; // to soothe compiler warnings
        int sx = x; // to soothe compiler warnings
        int sn = 0;

        msg_print("The ceiling collapses!");

        /* Check around the player */
        for (i = 0; i < 8; i++)
        {
            /* Get the location */
            yy = p_ptr->py + ddy_ddd[i];
            xx = p_ptr->px + ddx_ddd[i];

            /* Skip non-empty grids */
            if (!cave_empty_bold(yy, xx))
                continue;

            /* Count "safe" grids, apply the randomizer */
            if ((++sn > 1) && (rand_int(sn) != 0))
                continue;

            /* Save the safe location */
            sy = yy;
            sx = xx;
        }

        /* Hurt the player a lot */
        if (!sn)
        {
            /* Message and damage */
            msg_print("You are severely crushed!");
            dam = damroll(6, 8);

            /* Protection */
            prt = protection_roll(GF_HURT, false);

            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(6, 8, dam, -1, -1, prt, 100, GF_HURT, false);

            if (allow_player_stun(NULL))
            {
                (void)set_stun(p_ptr->stun + dam * 4);
            }
        }

        /* Destroy the grid, and push the player to safety */
        else
        {
            /* Calculate results */
            if (check_hit(20, true))
            {
                msg_print("You are struck by rubble!");
                dam = damroll(4, 8);

                /* Protection */
                prt = protection_roll(GF_HURT, false);

                update_combat_rolls2(
                    4, 8, dam, -1, -1, prt, 100, GF_HURT, false);

                net_dam = (dam - prt > 0) ? (dam - prt) : 0;

                if (allow_player_stun(NULL))
                {
                    (void)set_stun(p_ptr->stun + dam * 4);
                }
            }
            else
            {
                msg_print("You nimbly dodge the falling rock!");
                net_dam = 0;
            }

            /* Move player */
            monster_swap(p_ptr->py, p_ptr->px, sy, sx);
        }

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_TRAP);
        take_hit(net_dam, name);

        /* Forget the trap */
        cave_info[y][x] &= ~(CAVE_MARK);

        /* Replace the trap with rubble */
        cave_set_feat(y, x, FEAT_RUBBLE);

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }
    }
}

void display_hit(int y, int x, int net_dam, int dam_type, bool fatal_blow)
{
    (void)y;
    (void)x;
    (void)net_dam;
    (void)dam_type;
    (void)fatal_blow;

    // do nothing unless the appropriate option is set
    if (!display_hits)
        return;

    platform_frame_present();

    /* Delay */
    platform_frame_delay_ms((u32b)(25 * op_ptr->delay_factor));

    /* Restore the live map once the semantic animation frame has elapsed. */
    dungeon_mark_map_for_redraw();
    platform_frame_present();
    dungeon_sync_cursor_state();
}

/*
 *  Determines whether an attack is a charge attack
 */

bool valid_charge(int fy, int fx, int attack_type)
{
    int d, i;

    int deltay = fy - p_ptr->py;
    int deltax = fx - p_ptr->px;

    if (p_ptr->active_ability[S_MEL][MEL_CHARGE] && (p_ptr->pspeed > 1)
        && is_normal_attack(attack_type))
    {
        // try all three directions
        for (i = -1; i <= 1; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];

            if (p_ptr->previous_action[1] == d)
            {
                return (true);
            }
        }
    }

    return (false);
}

/*
 *  Attacks a new monster with 'follow through' if applicable
 */

void possible_follow_through(int fy, int fx, int attack_type)
{
    int d, i;

    int y, x;

    int deltay = fy - p_ptr->py;
    int deltax = fx - p_ptr->px;

    // clamp impale kills
    if (deltax > 1)
        deltax = 1;
    else if (deltax < -1)
        deltax = -1;

    if (deltay > 1)
        deltay = 1;
    else if (deltay < -1)
        deltay = -1;

    if (p_ptr->active_ability[S_MEL][MEL_FOLLOW_THROUGH] && !(p_ptr->confused)
        && (is_normal_attack(attack_type) || (attack_type == ATT_FOLLOW_THROUGH)
            || (attack_type == ATT_WHIRLWIND)))
    {
        // look through adjacent squares in an anticlockwise direction
        for (i = 1; i < 8; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];

            y = p_ptr->py + ddy[d];
            x = p_ptr->px + ddx[d];

            if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

                if (m_ptr->ml
                    && (!forgo_attacking_unwary
                        || (m_ptr->alertness >= ALERTNESS_ALERT)))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You stop your follow-through to avoid striking a fleeing foe.");
                        return;
                    }

                    msg_print("You continue your attack!");
                    py_attack_aux(y, x, ATT_FOLLOW_THROUGH);
                    return;
                }
            }
        }
    }
}

/*
 *  Determines the bonus for the ability 'concentration' and updates some
 * related variables.
 */

int concentration_bonus(int y, int x)
{
    int bonus = 0;

    // deal with 'concentration' ability
    if (p_ptr->active_ability[S_PER][PER_CONCENTRATION]
        && (p_ptr->last_attack_m_idx == cave_m_idx[y][x]))
    {
        bonus = MIN(p_ptr->consecutive_attacks, p_ptr->skill_use[S_PER] / 2);
    }

    // If the player is not engaged with this monster, reset the attack count
    // and mosnter
    if ((p_ptr->last_attack_m_idx != cave_m_idx[y][x]))
    {
        p_ptr->consecutive_attacks = 0;
        p_ptr->last_attack_m_idx = cave_m_idx[y][x];
    }

    return (bonus);
}

/*
 *  Determines the bonus for the ability 'focused attack'.
 */

int focused_attack_bonus(void)
{
    // focused attack
    if (p_ptr->focused)
    {
        p_ptr->focused = false;

        if (p_ptr->active_ability[S_PER][PER_FOCUSED_ATTACK])
        {
            return (p_ptr->skill_use[S_PER] / 2);
        }
    }

    return (0);
}

/*
 *  Determines the bonus for the ability 'master hunter'.
 */

int master_hunter_bonus(monster_type* m_ptr)
{
    // master hunter bonus
    if (p_ptr->active_ability[S_PER][PER_MASTER_HUNTER])
    {
        return (
            MIN((&l_list[m_ptr->r_idx])->pkills, p_ptr->skill_use[S_PER] / 2));
    }
    else
    {
        return (0);
    }
}

void attack_punctuation(char* punctuation, int net_dam, int crit_bonus_dice)
{
    int i;

    if (net_dam == 0)
    {
        SDL_strlcpy(punctuation, "...", sizeof(punctuation));
    }
    else if (crit_bonus_dice == 0)
    {
        SDL_strlcpy(punctuation, ".", sizeof(punctuation));
    }
    else
    {
        for (i = 0; (i < crit_bonus_dice) && (i < 20); i++)
        {
            punctuation[i] = '!';
        }
        punctuation[i] = '\0';
    }
}

bool knock_back(int y1, int x1, int y2, int x2)
{
    bool knocked = false;

    bool monster_target = false;

    int mod, d, i;
    int y3, x3; // the location to get knocked to
    int dir;

    int dy, dx;

    // default to there being no monster
    monster_type* m_ptr = NULL;

    // determine the main direction from the source to the target
    dir = rough_direction(y1, x1, y2, x2);

    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];

    // knocking a monster back...
    if (cave_m_idx[y2][x2] > 0)
    {
        monster_target = true;
        m_ptr = &mon_list[cave_m_idx[y2][x2]];
    }

    // first try to knock it straight back
    if (cave_floor_bold(y2 + dy, x2 + dx)
        && (cave_m_idx[y2 + dy][x2 + dx] == 0))
    {
        y3 = y2 + dy;
        x3 = x2 + dx;
        knocked = true;
    }

    // then try the adjacent directions
    else
    {
        // randomize clockwise or anticlockwise
        if (one_in_(2))
            mod = -1;
        else
            mod = +1;

        // try both directions
        for (i = 0; i < 2; i++)
        {
            d = cycle[chome[dir_from_delta(dy, dx)] + mod];
            y3 = y2 + ddy[d];
            x3 = x2 + ddx[d];
            if (cave_floor_bold(y3, x3) && (cave_m_idx[y3][x3] == 0))
            {
                knocked = true;
                break;
            }

            // switch direction
            mod *= -1;
        }
    }

    // make the target skip a turn
    if (knocked)
    {
        if (monster_target)
        {
            m_ptr->skip_next_turn = true;

            // actually move the monster
            monster_swap(y2, x2, y3, x3);
        }
        else
        {
            msg_print("You are knocked back.");
            p_ptr->knocked_back = true;

            p_ptr->skip_next_turn = true;

            // actually move the player
            monster_swap(y2, x2, y3, x3);

            // cannot stay in the air
            p_ptr->leaping = false;

            // make some noise when landing
            stealth_score -= 5;

            /* Set off traps */
            if (cave_trap_bold(p_ptr->py, p_ptr->px)
                || ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)))
            {
                // If it is hidden
                if (cave_info[p_ptr->py][p_ptr->px] & (CAVE_HIDDEN))
                {
                    /* Reveal the trap */
                    reveal_trap(p_ptr->py, p_ptr->px);
                }

                /* Hit the trap */
                hit_trap(p_ptr->py, p_ptr->px);
            }
        }
    }

    return (knocked);
}

bool merciless_attack(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    return (chosen_oath(OATH_MERCY) && !oath_invalid(OATH_MERCY)
        && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))));
}

bool cowardly_attack(monster_type* m_ptr)
{
    return (chosen_oath(OATH_VALOROUS) && !oath_invalid(OATH_VALOROUS)
        && m_ptr->stance == STANCE_FLEEING);  /* Monster is fleeing in terror */
}

static bool valorous_oath_blocks_auto_attack(monster_type* m_ptr)
{
    if (!valorous_oath_auto_attack_safety)
        return false;

    if (!chosen_oath(OATH_VALOROUS) || oath_invalid(OATH_VALOROUS))
        return false;

    if (!m_ptr || !m_ptr->ml)
        return false;

    return (m_ptr->stance == STANCE_FLEEING);
}

bool abort_for_mercy(monster_type* m_ptr)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return false;

    if (merciless_attack(m_ptr))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_MERCY);
        if (!prompt || !prompt[0]) prompt = "Are you sure you wish to break your oath?";
        
        if (!get_check_oath_multiline(prompt))
        {
            return true;
        }
    }

    return false;
}

bool abort_for_valorous(monster_type* m_ptr)
{
    // Unseen enemies are okay to kill  
    if (!m_ptr->ml)
        return false;

    if (cowardly_attack(m_ptr))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_VALOROUS);
        if (!prompt || !prompt[0]) prompt = "Are you sure you wish to break your oath?";
        
        if (!get_check_oath_multiline(prompt))
        {
            return true;
        }
    }

    return false;
}

/*
 * Check if an attack type is an Area of Effect (AoE) attack
 * vs a direct targeted attack
 */
bool is_aoe_attack_type(int attack_type)
{
    switch (attack_type)
    {
        case ATT_MAIN:
        case ATT_FLANKING:
        case ATT_CONTROLLED_RETREAT:
        case ATT_POLEARM:
        case ATT_RIPOSTE:
        case ATT_OPPORTUNIST:
        case ATT_ZONE_OF_CONTROL:
        case ATT_OPPORTUNITY:
        case ATT_IMPALE:
            return false;  // Direct targeted attacks
            
        case ATT_WHIRLWIND:
        case ATT_RAGE:
        case ATT_FOLLOW_THROUGH:
            return true;   // AoE attacks
            
        default:
            return false;  // Default to direct attack
    }
}

/*
 * Apply consequences when an oath is broken:
 * 1. Remove oath bonuses (recalculate stats)
 * 2. Apply a random metarun curse
 * 3. Ban the oath for the rest of this metarun
 */
void apply_oath_breaking_curse(int oath_id)
{
    cptr oath_name;
    
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return;
    
    /* Get oath name for logging - use static fallback names to avoid dangling pointer */
    static const char* fallback_oath_names[] = {"", "Mercy", "Silence", "Iron", "Smith", "Valorous", "Light"};
    if (oath_id <= z_info->oath_max && oath_info[oath_id].name) {
        oath_name = oath_name_text + oath_info[oath_id].name;
    } else if (oath_id < 7) {
        oath_name = fallback_oath_names[oath_id];
    } else {
        oath_name = "Unknown";
    }
    
    log_trace("Applying oath breaking consequences for oath %d (%s)", oath_id, oath_name);
    
    /* Disable the corresponding special ability */
    if (oath_id == OATH_MERCY) {
        p_ptr->active_ability[S_SPC][SPC_OATH_MERCY] = false;
    }
    else if (oath_id == OATH_SILENCE) {
        p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE] = false;
    }
    else if (oath_id == OATH_IRON) {
        p_ptr->active_ability[S_SPC][SPC_OATH_IRON] = false;
    }
    else if (oath_id == OATH_SMITH) {
        p_ptr->active_ability[S_SPC][SPC_OATH_SMITH] = false;
    }
    else if (oath_id == OATH_VALOROUS) {
        p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS] = false;
    }
    else if (oath_id == OATH_LIGHT) {
        p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
    }
    
    /* Remove oath bonuses by recalculating */
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATE);
    
    /* Show oath-specific curse message and let player choose curse */
    int chosen_curse = choose_oath_breaking_curse_ui(oath_id);
    
    if (chosen_curse >= 0) {
        /* Apply the chosen curse */
        add_curse_stack(chosen_curse);
        log_trace("Applied chosen curse %d for breaking oath", chosen_curse);
    } else {
        log_error("Failed to choose an oath-breaking curse; no curse applied");
    }
    
    /* Ban this oath for the rest of the metarun */
    metarun_ban_oath(oath_id);
    
    log_trace("Banned oath %d (%s) from future selection in this metarun", oath_id, oath_name);
}

void break_mercy_oath(monster_type* m_ptr, int damage)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (damage > 0
        && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))))
    {
        if (merciless_attack(m_ptr))
        {
            /* Curse message and selection handled by apply_oath_breaking_curse */
            do_cmd_note("Broke your oath", p_ptr->depth);
            
            /* Apply oath breaking consequences */
            apply_oath_breaking_curse(OATH_MERCY);
            
            /* Only mark oath as broken if player actually has it */
            p_ptr->oaths_broken |= OATH_MERCY_FLAG;
        }
    }
}

void break_valorous_oath(monster_type* m_ptr, int damage, int attack_type, int damage_source)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return;

    // Only break oath for player-caused damage  
    // damage_source: -1 = player, 0+ = monster index
    if (damage_source != -1)
        return;

    /* All player-caused attacks break Valor on hit */
    (void)attack_type;
    if (damage <= 0)
        return;

    if (!cowardly_attack(m_ptr))
        return;

    do_cmd_note("Broke your oath", p_ptr->depth);
    apply_oath_breaking_curse(OATH_VALOROUS);
    p_ptr->oaths_broken |= OATH_VALOROUS_FLAG;
}

/*
 * Attack the monster at the given location
 *
 * If no "weapon" is available, then "punch" the monster one time.
 */
void py_attack_aux(int y, int x, int attack_type)
{
    int num = 0;

    int attack_mod = 0, total_attack_mod = 0, total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int cruel_blow_multiplier = 0;
    int dam = 0, prt = 0;
    int net_dam = 0;
    int prt_percent = 100;
    int hits = 0;
    int weapon_weight;
    int total_dice;
    int blows;
    int mdd, mds;
    int stealth_bonus = 0;
    int assassination_bonus = 0;
    int monster_ripostes = 0;
    int effective_strength;
    int damage_type = GF_HURT;

    int m_idx;
    monster_type* m_ptr;
    monster_race* r_ptr;

    object_type* o_ptr;

    char m_name[80];
    char punctuation[20];

    bool abort_attack = false;
    bool do_knock_back = false;
    bool knocked = false;
    bool charge = false;
    bool rapid_attack = false;
    bool off_hand_blow = false;
    bool fatal_blow = false;
    bool smite = false;
    bool tulkas_wrath = false;
    bool huntsman_rhythm = false;

    u32b f1, f2, f3, f4; // the weapon's flags

    u32b noticed_flag = 0; // if any slay is observed and the weapon thus
                           // identified it goes here

    /* Get the monster */
    m_idx = cave_m_idx[y][x];
    m_ptr = &mon_list[m_idx];
    r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->r_idx == R_IDX_MORGOTH) {
        niena_mark_morgoth_attack();
    }

    /*possibly update the monster health bar*/
    if (p_ptr->health_who == cave_m_idx[y][x])
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Disturb the player */
    disturb(0, 0);

    /* Extract monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Auto-Recall if possible and visible */
    if (m_ptr->ml)
        monster_race_track(m_ptr->r_idx);

    /* Track a new monster */
    if (m_ptr->ml)
        health_track(cave_m_idx[y][x]);

    /* Target this monster */
    if (m_ptr->ml)
        target_set_monster(cave_m_idx[y][x]);

    if (r_ptr->flags1 & (RF1_PEACEFUL))
    {
        if (attack_type == ATT_MAIN)
        {
            /* Handle alert thrall quest interaction */
            if (is_alert_thrall(m_ptr))
            {
                handle_thrall_interaction(m_ptr);
            }
            else
            {
                msg_format("You stop before you bump into %s.", m_name);
            }
        }

        if (!player_attacked)
        {
            p_ptr->previous_action[0] = ACTION_NOTHING;
            p_ptr->energy_use = 0;
        }

        return;
    }

    /* Get the weapon */
    o_ptr = &inventory[INVEN_WIELD];
    huntsman_rhythm = p_ptr->active_ability[S_SPC][SPC_HUNTSMAN_RHYTHM];
    if (!huntsman_rhythm) {
        p_ptr->orome_spear_ready = 0;
        p_ptr->orome_bow_hit_streak = 0;
    }

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_format("You are too afraid to attack %s!", m_name);

        abort_attack = true;
    }

    // inscribing an object with "!a" produces prompts to confirm that you with
    // to attack with it idea and code from MarvinPA
    if (o_ptr->obj_note && !p_ptr->truce && m_ptr->ml)
    {
        cptr s;
        /* Find a '!' */
        s = strchr(quark_str(o_ptr->obj_note), '!');

        /* Process inscription */
        while (s)
        {
            if ((s[1] == 'a')
                && !get_check("Are you sure you wish to attack? "))
            {
                abort_attack = true;
            }

            /* Find another '!' */
            s = strchr(s + 1, '!');
        }
    }

    // Warning about breaking the truce
    if ((p_ptr->truce) && !get_check("Are you sure you wish to attack? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with fists
    if ((o_ptr->weight == 0)
        && !get_check("Are you sure you wish to attack with no weapon? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with shovel
    if ((o_ptr->tval == TV_DIGGING) && (o_ptr->sval == SV_SHOVEL)
        && !get_check("Are you sure you wish to attack with your shovel? "))
    {
        abort_attack = true;
    }

    // Don't make the player deal with Oath warnings on free attacks - pass them
    // up
    if (!is_normal_attack(attack_type) && merciless_attack(m_ptr))
    {
        abort_attack = true;
    }
    else if (abort_for_mercy(m_ptr))
    {
        abort_attack = true;
    }
    else if (!is_aoe_attack_type(attack_type) && abort_for_valorous(m_ptr))
    {
        // Only show valorous oath warning for direct attacks
        // AoE attacks will break oath immediately without warning
        abort_attack = true;
    }

    // Cancel the attack if needed
    if (abort_attack)
    {
        if (!player_attacked)
        {
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;

            // don't take a turn
            p_ptr->energy_use = 0;
        }

        /* Done */
        return;
    }

    // fighting with fists is equivalent to a 4 lb weapon for the purpose of
    // criticals
    weapon_weight = o_ptr->weight ? o_ptr->weight : 40;

    mdd = p_ptr->mdd;
    mds = p_ptr->mds;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    // determine the base for the attack_mod
    attack_mod = p_ptr->skill_use[S_MEL];

    /* Monsters might notice */
    player_attacked = true;

    // Determine the number of attacks
    blows = 1;
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        blows++;
        rapid_attack = true;
    }
    if (p_ptr->mds2 > 0 && attack_type != ATT_IMPALE)
    {
        blows++;
    }

    // Attack types that take place in the opponents' turns only allow a single
    // attack
    if (!is_normal_attack(attack_type) && attack_type != ATT_WHIRLWIND)
    {
        blows = 1;

        // undo strength adjustment to the attack (if any)
        mds = total_mds(o_ptr, 0);

        // undo the dexterity adjustment to the attack (if any)
        if (rapid_attack)
        {
            rapid_attack = false;
            attack_mod += 3;
        }
    }

    /* Attack once for each legal blow */
    while (num++ < blows)
    {
        {
            bool smite_ability = p_ptr->active_ability[S_MEL][MEL_SMITE];
            bool wrath_ability = p_ptr->active_ability[S_SPC][SPC_TULKAS_WRATH];
            smite = two_handed_melee() && (smite_ability || wrath_ability)
                && num == 1
                && (attack_type == ATT_MAIN || attack_type == ATT_FLANKING
                    || attack_type == ATT_IMPALE
                    || attack_type == ATT_FOLLOW_THROUGH
                    || attack_type == ATT_WHIRLWIND);
            tulkas_wrath = smite && wrath_ability;
        }

        do_knock_back = false;
        knocked = false;

        if (smite)
            p_ptr->skip_next_turn = true;

        // if the previous blow was a charge, undo the charge effects for later
        // blows
        if (charge)
        {
            charge = false;
            attack_mod -= 3;
            mds = p_ptr->mds;
        }

        // adjust for off-hand weapon if it is being used
        if ((num == blows) && (num != 1) && (p_ptr->mds2 > 0)
            && attack_type != ATT_IMPALE)
        {
            off_hand_blow = true;
            rapid_attack = false;

            attack_mod += p_ptr->offhand_mel_mod;
            mdd = p_ptr->mdd2;
            mds = p_ptr->mds2;
            o_ptr = &inventory[INVEN_ARM];
            weapon_weight = o_ptr->weight;
            object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        }

        if (is_normal_attack(attack_type))
        {
            assassination_bonus = stealth_melee_bonus(m_ptr, false);
        }
        else
        {
            assassination_bonus = 0;
        }

        // +3 Str/Dex on first blow when charging
        if ((num == 1) && valid_charge(y, x, attack_type))
        {
            if (!(assassination_over_charge && assassination_bonus > 0))
            {
                int str_adjustment = 3;

                if (rapid_attack)
                    str_adjustment -= 3;

                charge = true;
                attack_mod += 3;

                // undo strength adjustment to the attack (if any)
                mds = total_mds(o_ptr, str_adjustment);

                if (assassination_bonus > 0)
                {
                    msg_print(
                        "(Assassination did not apply because this was a charge attack.)");
                }
            }
        }

        // reward attacks on unaware monsters for characters with the
        // assassination ability, unless charge takes priority
        if (is_normal_attack(attack_type) && !charge)
        {
            stealth_bonus = assassination_bonus;
        }
        else
        {
            stealth_bonus = 0;
        }

        // Determine the player's attack score after all modifiers
        total_attack_mod
            = total_player_attack(m_ptr, attack_mod + stealth_bonus);

        // Determine the monster's evasion score after all modifiers
        total_evasion_mod = total_monster_evasion(m_ptr, false);

        song_disguise_note_player_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);

        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, PLAYER, m_ptr, true);

        if (hit_result <= 0 && (f3 & TR3_ACCURATE))
        {
            char m_name[80];
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

            hit_result = hit_roll(
                total_attack_mod, total_evasion_mod, PLAYER, m_ptr, true);
            if (hit_result > 0)
                msg_format("%^s tries and fails to dodge your blow.", m_name);
        }

        /* If the attack connects... */
        if (hit_result > 0)
        {
            hits++;

            /* Mark the monster as attacked */
            m_ptr->mflag |= (MFLAG_HIT_BY_MELEE);

            /* Mark the monster as charged */
            if (charge)
                m_ptr->mflag |= (MFLAG_CHARGED);

            /* Calculate the damage */
            crit_bonus_dice = crit_bonus(
                hit_result, weapon_weight, r_ptr, S_MEL, false, NULL, o_ptr);
            slay_bonus_dice = slay_bonus(o_ptr, m_ptr, &noticed_flag);

            if (f3 & TR3_CUMBERSOME)
            {
                crit_bonus_dice = 0;
            }

            total_dice = mdd + slay_bonus_dice + crit_bonus_dice;

            dam = damroll(total_dice, mds);
            if (smite)
                dam = total_dice * mds;

            /* Apply armor dice/sides curses/blessings */
            int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
            if (armor_dice_base < 0)
                armor_dice_base = 0;
            int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
            int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
            if (armor_dice < 0) armor_dice = 0;
            if (armor_sides < 1) armor_sides = 1;
            prt = damroll(armor_dice, armor_sides);
            prt_percent = prt_after_sharpness(o_ptr, &noticed_flag);

            if (prt_percent < 0)
            {
                prt_percent = 0;
            }

            prt = (prt * prt_percent) / 100;

            net_dam = dam - prt;

            /* No negative damage */
            if (net_dam < 0)
                net_dam = 0;

            if (huntsman_rhythm)
            {
                bool spear_ready = (p_ptr->orome_spear_ready != 0);
                bool spear_weapon = weapon_is_spear(o_ptr);

                if (spear_ready)
                {
                    if (spear_weapon)
                    {
                        net_dam *= 2;
                        msg_print("Your spear strike follows the rhythm of the hunt!");
                    }

                    p_ptr->orome_spear_ready = 0;
                    p_ptr->orome_bow_hit_streak = 0;
                }
                else
                {
                    /* Any melee hit breaks the bow streak even if no primed strike */
                    p_ptr->orome_bow_hit_streak = 0;
                }
            }
            else
            {
                p_ptr->orome_spear_ready = 0;
                p_ptr->orome_bow_hit_streak = 0;
            }

            break_mercy_oath(m_ptr, net_dam);
            break_valorous_oath(m_ptr, net_dam, attack_type, -1);  // -1 indicates player damage

            // Play weapon swing sound first (layered sound system)
            u16b weapon_swing_type = weapon_sound_message_type(o_ptr, false);
            sound(weapon_swing_type);

            // Delay before result sound (varies by weapon type)
            platform_delay_ms((u32b)weapon_animation_delay(weapon_swing_type));

            // Determine result sound: armor blocked, hit, or nothing
            u16b result_sound = MSG_ARMOR; // default to armor
            if (net_dam > 0)
            {
                result_sound = MSG_HIT;
            }

            // Play result sound
            sound(result_sound);

            // determine the punctuation for the attack ("...", ".", "!" etc)
            attack_punctuation(punctuation, net_dam, crit_bonus_dice);

            /* Special message for visible unalert creatures */
            if (stealth_bonus)
            {
                msg_format("You stealthily attack %s%s", m_name, punctuation);
            }
            else
            {
                /* Message */
                if (charge)
                {
                    msg_format("You charge %s%s", m_name, punctuation);
                }
                else if (smite)
                {
                    msg_format("You smite %s%s", m_name, punctuation);
                }
                else if (attack_type == ATT_IMPALE)
                {
                    msg_format("You impale %s%s", m_name, punctuation);
                }
                else
                {
                    msg_format("You hit %s%s", m_name, punctuation);
                }
            }

            // determine the player's score for knocking an opponent backwards
            // if they have the ability first calculate their strength including
            // modifiers for this attack
            effective_strength = p_ptr->stat_use[A_STR];
            if (charge)
                effective_strength += 3;
            if (rapid_attack)
                effective_strength -= 3;
            if (off_hand_blow)
                effective_strength -= 3;

            // cap the value by the weapon weight
            if (effective_strength > weapon_weight / 10)
                effective_strength = weapon_weight / 10;
            if ((effective_strength < 0)
                && (-effective_strength > weapon_weight / 10))
                effective_strength = -(weapon_weight / 10);

            // give an extra +2 bonus for using a weapon two-handed
            if (two_handed_melee())
                effective_strength += 2;

            if (tulkas_wrath)
                effective_strength *= 2;

            // check whether the effect triggers
            if (p_ptr->active_ability[S_MEL][MEL_KNOCK_BACK]
                && (attack_type != ATT_OPPORTUNIST)
                && !(r_ptr->flags1 & (RF1_NEVER_MOVE))
                && !(r_ptr->flags1 & (RF1_HIDDEN_MOVE))
                && (skill_check(PLAYER, effective_strength * 2,
                        monster_stat(m_ptr, A_CON) * 2, m_ptr)
                    > 0))
            {
                // remember this for later when the effect is applied
                do_knock_back = true;
            }

            if (singing(SNG_SLAYING) && crit_bonus_dice > 0)
            {
                int kill_threshold = ability_bonus(S_SNG, SNG_SLAYING);
                if (m_ptr->hp <= kill_threshold)
                {
                    msg_format("Your song soars as %s falls before you.", m_name);

                    /* Sort out combat rolls window */
                    total_dice = 0;
                    mds = 0;
                    dam = m_ptr->hp;
                    prt = 0;
                    prt_percent = 0;

                    /* Generate treasure */
                    monster_death(m_idx);

                    /* Auto-recall only if visible or unique */
                    if (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)))
                    {
                        monster_race_track(m_ptr->r_idx);
                    }

                    /* Delete the monster */
                    delete_monster_idx(m_idx);
                    
                    fatal_blow = true;
                }
            }

            // Take hit only if monster has not been killed by an ability already
            if (!fatal_blow)
            {
                // damage, check for death
                fatal_blow = mon_take_hit(m_idx, net_dam, NULL, -1);
                p_ptr->vengeance = 0;
            }

            update_combat_rolls2(total_dice, mds, dam, armor_dice, armor_sides,
                prt, prt_percent, damage_type, true);

            // use different colours depending on whether knock back triggered
            if (do_knock_back)
            {
                display_hit(y, x, net_dam, GF_SOUND, fatal_blow);
            }
            else
            {
                display_hit(y, x, net_dam, GF_HURT, fatal_blow);
            }

            apply_weapon_combat_effects(
                o_ptr, m_ptr, S_MEL, net_dam, fatal_blow, "blow");

            // if a slay was noticed, then identify the weapon
            if (noticed_flag)
            {
                ident_weapon_by_use(o_ptr, m_ptr, noticed_flag);
                noticed_flag = false;
            }

            // deal with killing blows
            if (fatal_blow)
            {
                // deal with 'follow_through' ability
                possible_follow_through(y, x, attack_type);

                if (p_ptr->active_ability[S_WIL][WIL_FORMIDABLE])
                {
                    int will_score = p_ptr->skill_use[S_WIL];
                    if (project_los(GF_FEAR, 0, 0, will_score, true))
                        msg_print("Your foes are daunted!");
                }

                // stop attacking
                break;
            }

            // if the monster didn't die...
            else
            {
                // deal with knock back ability if it triggered
                if (do_knock_back)
                {
                    knocked = knock_back(p_ptr->py, p_ptr->px, y, x);
                }

                // Morgoth drops his iron crown if he is hit for 10 or more net
                // damage twice
                if ((m_ptr->r_idx == R_IDX_MORGOTH)
                    && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                {
                    if (net_dam >= 10)
                    {
                        if (p_ptr->morgoth_hits == 0)
                        {
                            msg_print("The force of your blow knocks the Iron "
                                      "Crown off balance.");
                            p_ptr->morgoth_hits++;
                        }
                        else if (p_ptr->morgoth_hits == 1)
                        {
                            drop_iron_crown(m_ptr,
                                "You knock his crown from off his brow, and it "
                                "falls to the ground nearby.");
                            p_ptr->morgoth_hits++;
                        }
                    }
                }

                // Deal with cruel blow ability
                if (p_ptr->active_ability[S_STL][STL_CRUEL_BLOW]
                    && (crit_bonus_dice > 0) && (net_dam > 0)
                    && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                {
                    // Slightly magical. Function that caps out before 30
                    // but grows quickly early on, and doesn't need math.h
                    cruel_blow_multiplier = (30 - (60 / (crit_bonus_dice + 2)));
                    if (skill_check(PLAYER, cruel_blow_multiplier,
                            monster_skill(m_ptr, S_WIL), m_ptr)
                        > 0)
                    {
                        msg_format("%^s reels in pain!", m_name);

                        // confuse the monster (if possible)
                        if (!(r_ptr->flags3 & (RF3_NO_CONF)))
                        {
                            // The +1 is needed as a turn of this wears off
                            // immediately
                            m_ptr->confused += crit_bonus_dice + 1;
                        }

                        // cause a temporary morale penalty
                        scare_onlooking_friends(m_ptr, -20);
                    }
                }
            }
        }

        /* Player misses */
        else
        {
            // Play weapon swing sound first (layered sound system)
            u16b weapon_swing_type = weapon_sound_message_type(o_ptr, false);
            sound(weapon_swing_type);

            // Delay before message (shorter for misses, still weapon-aware)
            platform_delay_ms((u32b)MAX(0, weapon_animation_delay(weapon_swing_type) - 200));

            /* Message - no additional sound for miss */
            msg_format("You miss %s.", m_name);

            // Occasional warning about fighting from within a pit
            if (cave_pit_bold(p_ptr->py, p_ptr->px) && one_in_(3))
            {
                msg_print(
                    "(It is very hard to dodge or attack from within a pit.)");
            }

            // Occasional warning about fighting from within a web
            if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
                && one_in_(3))
            {
                msg_print(
                    "(It is very hard to dodge or attack from within a web.)");
            }

            if (huntsman_rhythm)
            {
                /* Misses break the streak but keep any primed strike waiting */
                p_ptr->orome_bow_hit_streak = 0;
            }
            else
            {
                p_ptr->orome_spear_ready = 0;
                p_ptr->orome_bow_hit_streak = 0;
            }

            // allow for ripostes
            // treats attack a weapon weighing 2 pounds per damage die
            if ((r_ptr->flags2 & (RF2_RIPOSTE)) && (monster_ripostes == 0)
                && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING)
                && !m_ptr->skip_this_turn && !m_ptr->skip_next_turn
                && (hit_result <= -10 - (2 * r_ptr->blow[0].dd)))
            {
                msg_format("%^s ripostes!", m_name);
                make_attack_normal(m_ptr);
                monster_ripostes++;

                if (m_ptr->ml)
                {
                    monster_lore* l_ptr = &l_list[m_ptr->r_idx];
                    l_ptr->flags2 |= (RF2_RIPOSTE);
                }
            }
        }

        // alert the monster, even if no damage was done or the player missed
        make_alert(m_ptr);

        // stop attacking if you displace the creature
        if (knocked)
            break;
    }

    // Break the truce if creatures see
    break_truce(false);
}

/*
 * Count the maximum number of continuous passable adjacent squares 
 * (not walls, not rubble, not closed doors)
 * Returns the longest sequence of adjacent passable squares
 */
int count_open_adjacent_squares(int y, int x)
{
    bool passable[8];
    int i;
    int max_continuous = 0;
    int current_continuous = 0;
    
    /* First, check which adjacent squares are passable */
    for (i = 0; i < 8; i++)
    {
        int adj_y = y + ddy_ddd[i];
        int adj_x = x + ddx_ddd[i];
        
        /* Check bounds */
        if (!in_bounds(adj_y, adj_x))
        {
            passable[i] = false;
            continue;
        }
            
        /* Check if square is passable (not wall, not rubble, not closed door) */
        if (cave_floor_bold(adj_y, adj_x) || 
            cave_feat[adj_y][adj_x] == FEAT_OPEN ||
            (cave_feat[adj_y][adj_x] >= FEAT_TRAP_HEAD && cave_feat[adj_y][adj_x] <= FEAT_TRAP_TAIL))
        {
            passable[i] = true;
        }
        else
        {
            passable[i] = false;
        }
        log_trace("Adjacent square %d: (%d,%d) feat=%d passable=%d", i, adj_y, adj_x, cave_feat[adj_y][adj_x], passable[i]);
    }
    
    /* Now find the longest continuous sequence of passable squares */
    /* We need to check sequences that are actually adjacent in the game world */
    /* Direction mapping: 0=S, 1=N, 2=E, 3=W, 4=SE, 5=SW, 6=NE, 7=NW */
    /* Clockwise order in game world: N(1), NE(6), E(2), SE(4), S(0), SW(5), W(3), NW(7) */
    int clockwise_order[8] = {1, 6, 2, 4, 0, 5, 3, 7};
    
    for (int start = 0; start < 8; start++)
    {
        current_continuous = 0;
        /* Count consecutive passable squares going clockwise from start */
        for (int offset = 0; offset < 8; offset++)
        {
            int idx = clockwise_order[(start + offset) % 8];
            if (passable[idx])
            {
                current_continuous++;
            }
            else
            {
                break; /* Stop at first non-passable square */
            }
        }
        if (current_continuous > max_continuous)
            max_continuous = current_continuous;
    }
    
    log_trace("count_open_adjacent_squares result: max_continuous=%d", max_continuous);
    return max_continuous;
}

bool whirlwind_possible(void)
{
    if (!p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK])
    {
        return (false);
    }

    return (true);
}

bool can_impale()
{
    bool has_impale_skill = p_ptr->active_ability[S_MEL][MEL_IMPALE];

    object_type* o_ptr = &inventory[INVEN_WIELD];

    return has_impale_skill && weapon_is_impale_eligible(o_ptr);
}

void py_attack(int y, int x, int attack_type)
{
    int dir, dir0, yy, xx;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    dir = dir_from_delta(y - p_ptr->py, x - p_ptr->px);
    dir0 = chome[dir];

    // Debug logging for whirlwind
    int open_squares = count_open_adjacent_squares(p_ptr->py, p_ptr->px);
    int adjacent_monsters = adj_mon_count(p_ptr->py, p_ptr->px);
    bool whirlwind_poss = whirlwind_possible();
    
    log_trace("Whirlwind debug: rage=%d, whirlwind_possible=%d, open_squares=%d, adj_monsters=%d, afraid=%d", 
              p_ptr->rage, whirlwind_poss, open_squares, adjacent_monsters, p_ptr->afraid);
    
    bool do_rage_attack = p_ptr->rage && (adjacent_monsters > 1) && !p_ptr->afraid;
    bool do_whirlwind_attack = !p_ptr->rage && whirlwind_poss
        && (open_squares >= 5) && (adjacent_monsters > 1) && !p_ptr->afraid;

    if (do_whirlwind_attack && valorous_oath_auto_attack_safety
        && chosen_oath(OATH_VALOROUS) && !oath_invalid(OATH_VALOROUS))
    {
        for (int check_dir = 1; check_dir <= 9; check_dir++)
        {
            int cy, cx;
            int m_idx;
            monster_type* m_ptr;

            if (check_dir == 5)
                continue;

            cy = p_ptr->py + ddy[check_dir];
            cx = p_ptr->px + ddx[check_dir];
            if (!in_bounds(cy, cx))
                continue;

            m_idx = cave_m_idx[cy][cx];
            if (m_idx <= 0)
                continue;

            m_ptr = &mon_list[m_idx];
            if (m_ptr->ml && (m_ptr->stance == STANCE_FLEEING))
            {
                msg_print("You hold back your whirlwind to avoid striking a fleeing foe.");
                do_whirlwind_attack = false;
                break;
            }
        }
    }

    if (do_rage_attack || do_whirlwind_attack)
    {
        int i;
        bool clockwise = one_in_(2);

        // message only for rage (too annoying otherwise)
        if (do_rage_attack)
        {
            msg_print("You strike out at everything around you!");
        }
        else
        {
            msg_print("You whirl around, striking at everything nearby!");
        }

        // attack the adjacent squares in sequence
        for (i = 0; i < 8; i++)
        {
            if (clockwise)
                dir = cycle[dir0 + i];
            else
                dir = cycle[dir0 - i];

            yy = p_ptr->py + ddy[dir];
            xx = p_ptr->px + ddx[dir];

            if (cave_m_idx[yy][xx] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[yy][xx]];

                if (do_rage_attack)
                {
                    py_attack_aux(yy, xx, ATT_RAGE);
                }
                else if ((i == 0) || !forgo_attacking_unwary
                    || (m_ptr->alertness >= ALERTNESS_ALERT))
                {
                    py_attack_aux(yy, xx, ATT_WHIRLWIND);
                }
            }
        }
    }
    else if (can_impale())
    {
        yy = y + ddy[dir];
        xx = x + ddx[dir];

        if (cave_m_idx[yy][xx] > 0)
        {
            py_attack_aux(y, x, ATT_IMPALE);
            py_attack_aux(yy, xx, ATT_IMPALE);
        }
        else
        {
            py_attack_aux(y, x, attack_type);
        }
    }
    else
    {
        py_attack_aux(y, x, attack_type);
    }
}

/*
 *  Does any flanking or controlled retreat attack necessary when player moves
 * to square y,x
 */
void flanking_or_retreat(int y, int x)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int d;
    int fy, fx;
    int start;
    monster_type* m_ptr;

    bool flanking = p_ptr->active_ability[S_EVN][EVN_FLANKING];
    bool controlled_retreat = false;
    bool moved_last_turn = false;

    if (((p_ptr->previous_action[1] >= 1) && (p_ptr->previous_action[1] <= 9)
            && (p_ptr->previous_action[1] != 5))
        || (p_ptr->previous_action[1] == ACTION_BASH))
    {
        moved_last_turn = true;
    }

    // need to have the ability, and to have not moved last round
    if (p_ptr->active_ability[S_EVN][EVN_CONTROLLED_RETREAT]
        && !moved_last_turn)
    {
        controlled_retreat = true;
    }

    if (singing(SNG_DISGUISE))
        return;

    if (!p_ptr->confused && (flanking || controlled_retreat))
    {
        fy = p_ptr->target_row;
        fx = p_ptr->target_col;

        // first see if the targetted monster is eligible and attack it if so
        if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid
            && !p_ptr->truce)
        {
            m_ptr = &mon_list[cave_m_idx[fy][fx]];

            if (!merciless_attack(m_ptr)
                && m_ptr->ml
                && (!forgo_attacking_unwary
                    || (m_ptr->alertness >= ALERTNESS_ALERT)))
            {
                // try a flanking attack
                if (flanking && (distance(py, px, fy, fx) == 1)
                    && (distance(y, x, fy, fx) == 1))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You forgo a flanking attack to avoid striking a fleeing foe.");
                        return;
                    }

                    py_attack(fy, fx, ATT_FLANKING);
                    return;
                }
                // try a controlled retreat attack
                if (controlled_retreat && (distance(py, px, fy, fx) == 1)
                    && (distance(y, x, fy, fx) > 1))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You forgo a controlled retreat attack to avoid striking a fleeing foe.");
                        return;
                    }

                    py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
                    return;
                }
            }
        }

        // otherwise we will look through the eligible monsters and choose one
        // randomly
        start = rand_int(8);

        /* Look for adjacent monsters */
        for (d = start; d < 8 + start; d++)
        {
            fy = py + ddy_ddd[d % 8];
            fx = px + ddx_ddd[d % 8];

            /* Check Bounds */
            if (!in_bounds(fy, fx))
                continue;

            if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid
                && !p_ptr->truce)
            {
                m_ptr = &mon_list[cave_m_idx[fy][fx]];

                // base conditions for an attack
                if (!merciless_attack(m_ptr)
                    && m_ptr->ml
                    && (!forgo_attacking_unwary
                        || (m_ptr->alertness >= ALERTNESS_ALERT)))
                {
                    // try a flanking attack
                    if (flanking && (distance(py, px, fy, fx) == 1)
                        && (distance(y, x, fy, fx) == 1))
                    {
                        if (valorous_oath_blocks_auto_attack(m_ptr))
                        {
                            msg_print("You forgo a flanking attack to avoid striking a fleeing foe.");
                            return;
                        }

                        py_attack(fy, fx, ATT_FLANKING);
                        return;
                    }
                    // try a controlled retreat attack
                    if (controlled_retreat && (distance(py, px, fy, fx) == 1)
                        && (distance(y, x, fy, fx) > 1))
                    {
                        if (valorous_oath_blocks_auto_attack(m_ptr))
                        {
                            msg_print("You forgo a controlled retreat attack to avoid striking a fleeing foe.");
                            return;
                        }

                        py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
                        return;
                    }
                }
            }
        }
    }
}

/*
 * Move player in the given direction, with the given "pickup" flag.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
