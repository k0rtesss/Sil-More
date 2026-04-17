/* File: cmd-interact-chest-core.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-command.h"
#include "player/killer.h"
#include "cmd-interact-chest-internal.h"

void chest_apply_drop_alignment(chest_alignment_type alignment)
{
    switch (alignment)
    {
    case CHEST_ALIGNMENT_NOBLE:
        drop_allow_noble = true;
        drop_allow_evil = false;
        break;

    case CHEST_ALIGNMENT_EVIL:
        drop_allow_noble = false;
        drop_allow_evil = true;
        break;

    case CHEST_ALIGNMENT_STANDARD:
    default:
        /* Until a themed item appears, chest rolls may pick either alignment. */
        drop_allow_noble = true;
        drop_allow_evil = true;
        break;
    }
}

chest_alignment_type chest_item_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    bool noble;
    bool evil;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    noble = (f4 & TR4_NOBLE_ITEM) != 0;
    evil = (f4 & TR4_EVIL_ITEM) != 0;

    if (noble && evil)
        return CHEST_ALIGNMENT_INVALID;
    if (noble)
        return CHEST_ALIGNMENT_NOBLE;
    if (evil)
        return CHEST_ALIGNMENT_EVIL;

    return CHEST_ALIGNMENT_STANDARD;
}

/*
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 */
void chest_release_contents(object_type* o_ptr, int y, int x, int destroy_typ)
{
    int number;
    bool generated_an_item = false;
    bool dropped_an_item = false;
    chest_alignment_type chest_alignment = CHEST_ALIGNMENT_STANDARD;
    int destroyed_contents = 0;
    int old_generation_mode = object_generation_mode;
    bool old_allow_noble = drop_allow_noble;
    bool old_allow_evil = drop_allow_evil;

    object_type* i_ptr;

    object_type object_type_body;

    if (!o_ptr || o_ptr->tval != TV_CHEST)
        return;

    /* Determine how much to drop (see above) */
    number = (o_ptr->sval >= SV_CHEST_MIN_LARGE) ? 4 : rand_range(2, 3);

    /* Zero pval means empty chest */
    if (!o_ptr->pval)
        return;

    /* Opening a chest */
    object_generation_mode = OB_GEN_MODE_CHEST;
    chest_apply_drop_alignment(chest_alignment);

    /* Determine the "value" of the items */
    int base_depth = ABS(o_ptr->pval);
    if (base_depth < 1)
        base_depth = 1;

    /* Chest contents are generated at the chest's stored depth. */
    int gen_depth = base_depth;

    /* Min-depth penalties are reduced by +5 from the chest level, so items
     * appearing below their minimum depth have less penalty. */
    int penalty_depth = base_depth + 5;

    level_partition_kind part_kind = LEVEL_PART_NONE;
    if (o_ptr->xtra1 & 0x80)
        part_kind = (level_partition_kind)(o_ptr->xtra1 & 0x7F);
    if (part_kind <= LEVEL_PART_NONE || part_kind >= LEVEL_PART_MAX)
        part_kind = level_partition_kind_for_point(y, x);
    drop_profile part_profile;
    drop_profile_for_partition_kind_source(
        part_kind, PARTITION_DROP_SOURCE_CHEST, &part_profile);

    if (o_ptr->sval == SV_CHEST_PRESENT)
        number = 1;

    /* Chest-specific difficulty bonus */
    drop_quality chest_quality = DROP_QUALITY_NORMAL;
    if ((o_ptr->sval == SV_CHEST_SMALL_WOODEN)
        || (o_ptr->sval == SV_CHEST_LARGE_WOODEN))
        chest_quality = DROP_QUALITY_GOOD;
    else if ((o_ptr->sval == SV_CHEST_SMALL_STEEL)
        || (o_ptr->sval == SV_CHEST_LARGE_STEEL))
        chest_quality = DROP_QUALITY_GREAT;
    else if ((o_ptr->sval == SV_CHEST_SMALL_JEWELLED)
        || (o_ptr->sval == SV_CHEST_LARGE_JEWELLED)
        || (o_ptr->sval == SV_CHEST_PRESENT))
        chest_quality = DROP_QUALITY_SUPERB;

    /* Drop some objects (non-chests) */
    for (; number > 0; --number)
    {
        bool accepted = false;

        for (int attempt = 0; attempt < 64 && !accepted; attempt++)
        {
            /* Get local object */
            i_ptr = &object_type_body;

            /* Wipe the object */
            object_wipe(i_ptr);

            bool ok = drop_generate_object_profiled_depths(gen_depth, penalty_depth,
                chest_quality, DROP_TYPE_UNTHEMED, 0, true, &part_profile, i_ptr);

            if (!ok)
                continue;

            chest_alignment_type item_alignment = chest_item_alignment(i_ptr);

            if (item_alignment == CHEST_ALIGNMENT_INVALID)
                continue;

            if (item_alignment == CHEST_ALIGNMENT_NOBLE
                || item_alignment == CHEST_ALIGNMENT_EVIL)
            {
                if (chest_alignment == CHEST_ALIGNMENT_STANDARD)
                {
                    chest_alignment = item_alignment;
                    chest_apply_drop_alignment(chest_alignment);
                }
                else if (chest_alignment != item_alignment)
                {
                    continue;
                }
            }

            generated_an_item = true;

            if ((destroy_typ >= 0)
                && elemental_attack_destroys_object(destroy_typ, i_ptr))
            {
                destroyed_contents++;
                accepted = true;
                continue;
            }

            drop_near(i_ptr, -1, y, x);
            dropped_an_item = true;
            accepted = true;
        }
    }

    /* No longer opening a chest */
    object_generation_mode = old_generation_mode;
    drop_allow_noble = old_allow_noble;
    drop_allow_evil = old_allow_evil;

    /* Empty */
    o_ptr->pval = 0;

    /*Paranoia, delete chest theme*/
    o_ptr->xtra1 = 0;

    /* Known */
    object_known(o_ptr);

    if (!generated_an_item)
    {
        msg_print("The chest is empty.");
    }
    else if (!dropped_an_item && destroyed_contents > 0)
    {
        msg_print("The chest's contents are ruined.");
    }
    else if (destroyed_contents > 0)
    {
        msg_print("Some of the chest's contents are ruined.");
    }
}

static void chest_death(int y, int x, s16b o_idx)
{
    chest_release_contents(&o_list[o_idx], y, x, -1);
}

/*
 * Chests have traps too.
 *
 * Exploding chest destroys contents (and traps).
 * Note that the chest itself is never destroyed.
 */
static void chest_trap(int y, int x, s16b o_idx)
{
    int trap, dam;

    object_type* o_ptr = &o_list[o_idx];

    (void)x; // casting to soothe compilation warnings
    (void)y;

    /* Ignore disarmed chests */
    if (o_ptr->pval <= 0)
        return;

    /* Obtain the traps */
    trap = chest_traps[o_ptr->pval];

    // Store information for the combat rolls window
    combat_roll_special_char = object_char(o_ptr);
    combat_roll_special_attr = object_attr(o_ptr);

    /* Needle - Hallucination */
    if (trap & (CHEST_NEEDLE_HALLU))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_image(NULL))
            {
                set_image(p_ptr->image + damroll(80, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Entrancement */
    if (trap & (CHEST_NEEDLE_ENTRANCE))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_entrancement(NULL))
            {
                set_entranced(damroll(10, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Lose strength */
    if (trap & (CHEST_NEEDLE_LOSE_STR))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            (void)do_dec_stat(A_STR, NULL);
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Confusion Gas */
    if (trap & (CHEST_GAS_CONF))
    {
        msg_print("A noxious vapour escapes from the chest!");
        if (allow_player_confusion(NULL))
        {
            (void)set_confused(p_ptr->confused + damroll(4, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Acrid Smoke */
    if (trap & (CHEST_GAS_STUN))
    {
        msg_print("Acrid smoke pours from the chest!");
        if (allow_player_stun(NULL))
        {
            msg_print("It fills your lungs and your mind reels.");

            dam = damroll(3, 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(3, 4, dam, -1, -1, 0, 0, GF_HURT, false);

            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(dam, "a trapped chest");

            set_stun(p_ptr->stun + damroll(30, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Poison Gas */
    if (trap & (CHEST_GAS_POISON))
    {
        msg_print("A noxious vapour escapes from the chest!");

        update_combat_rolls1b(NULL, PLAYER, true);

        (void)pois_dam_pure(10, 4, true);
    }

    /* Flame */
    if (trap & (CHEST_FLAME))
    {
        msg_print("There is a sudden burst of flame!");

        update_combat_rolls1b(NULL, PLAYER, true);

        fire_dam_pure(10, 4, true, "a trapped chest");

        /* Make some noise */
        monster_perception(true, false, -5);
    }
}

/*
 * Attempt to open the given chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool cmd_interact_open_chest_impl(int y, int x, s16b o_idx)
{
    int score, power, difficulty;

    bool flag = true;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Attempt to unlock it */
    if (o_ptr->pval > 0)
    {
        /* Assume locked, and thus not open */
        flag = false;

        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine trap power based on the chest pval (power is 1--7)*/
        power = 1 + (o_ptr->pval / 4);

        // Base difficulty is the lock power + 5
        difficulty = power + 5;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        /* Success -- May still have traps */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            msg_print("You have picked the lock.");
            flag = true;
        }

        /* Failure -- Keep trying */
        else
        {
            /* We may continue repeating */
            more = true;
            app_command_clear_pending();
            platform_frame_flush_events();
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");
        }
    }

    /* Allowed to open */
    if (flag)
    {
        /* Apply chest traps, if any */
        chest_trap(y, x, o_idx);

        /* Let the Chest drop items */
        chest_death(y, x, o_idx);

    }

    /* Result */
    return (more);
}

/*
 * Attempt to disarm the chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool cmd_interact_disarm_chest_impl(int y, int x, s16b o_idx)
{
    int score, power, difficulty, result;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Get the score in favour (=perception) */
    score = p_ptr->skill_use[S_PER];

    /* Determine trap power based on the trap pval (power is 1--7)*/
    power = 1 + (o_ptr->pval / 4);

    // Base difficulty is the lock power
    difficulty = power;

    /* Penalize some conditions */
    if (p_ptr->blind || no_light() || p_ptr->image)
        difficulty += 5;
    if (p_ptr->confused)
        difficulty += 5;

    // perform the check
    result = skill_check(PLAYER, score, difficulty, NULL);

    /* Must find the trap first. */
    if (!object_known_p(o_ptr))
    {
        msg_print("You don't see any traps.");
    }

    /* Already disarmed/unlocked */
    else if (o_ptr->pval <= 0)
    {
        msg_print("The chest is not trapped.");
    }

    /* No traps to find. */
    else if (!chest_traps[o_ptr->pval])
    {
        msg_print("The chest is not trapped.");
    }

    /* Success (get a lot of experience) */
    else if (result > 0)
    {
        msg_print("You have disarmed the chest.");
        o_ptr->pval = (0 - o_ptr->pval);
    }

    /* Failure -- Keep trying */
    else if (result > -3)
    {
        /* We may keep trying */
        more = true;
        app_command_clear_pending();
        platform_frame_flush_events();
        msg_print("You failed to disarm the chest.");
    }

    /* Failure -- Set off the trap */
    else
    {
        msg_print("You set off a trap!");
        chest_trap(y, x, o_idx);
    }

    /* Result */
    return (more);
}
