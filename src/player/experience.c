#include "angband.h"
#include "quest/quest-challenges.h"
#include "quest/quest-runtime.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

bool insight_system_enabled(void)
{
    if (p_ptr && p_ptr->insight_ruleset != INSIGHT_RULESET_UNSET)
        return p_ptr->insight_ruleset == INSIGHT_RULESET_LEGACY
            || p_ptr->insight_ruleset == INSIGHT_RULESET_REWORKED;
    return op_ptr && op_ptr->opt[OPT_insight_beta];
}

bool insight_reworked_enabled(void)
{
    return p_ptr && p_ptr->insight_ruleset == INSIGHT_RULESET_REWORKED;
}

typedef struct insight_monster_type_info
{
    u32b flag;
    cptr name;
} insight_monster_type_info;

/* Insight is awarded for the first visible member of each authored monster
 * family, rather than for each individual unique monster.  These are the
 * same race categories used by monster lore and slaying flags. */
static const insight_monster_type_info insight_monster_types[] = {
    { RF3_ORC,     "an Orc" },
    { RF3_TROLL,   "a Troll" },
    { RF3_SERPENT, "a Serpent" },
    { RF3_DRAGON,  "a Dragon" },
    { RF3_RAUKO,   "a Rauko" },
    { RF3_UNDEAD,  "an Undead creature" },
    { RF3_SPIDER,  "a Spider" },
    { RF3_WOLF,    "a Wolf" },
    { RF3_MAN,     "a Man" },
    { RF3_ELF,     "an Elf" },
    { RF3_GIANT,   "a Giant" },
    { RF3_CAT,     "a Cat" },
    { RF3_HORROR,  "a Horror" },
    { RF3_VAMPIRE, "a Vampire" },
};

int insight_monster_types_possible(void)
{
    return (int)N_ELEMENTS(insight_monster_types);
}

int insight_monster_types_seen(void)
{
    int seen = 0;

    if (!p_ptr) return 0;
    for (size_t i = 0; i < N_ELEMENTS(insight_monster_types); ++i)
        if (p_ptr->insight_monster_types & insight_monster_types[i].flag)
            seen++;
    return seen;
}

void insight_award_monster_type(u32b flags3)
{
    if (!p_ptr || !insight_system_enabled()) return;

    for (size_t i = 0; i < N_ELEMENTS(insight_monster_types); ++i)
    {
        const insight_monster_type_info* type = &insight_monster_types[i];
        if (!(flags3 & type->flag)
            || (p_ptr->insight_monster_types & type->flag)) continue;

        p_ptr->insight_monster_types |= type->flag;
        char reason[96];
        strnfmt(reason, sizeof(reason), "You first see %s.", type->name);
        gain_insight_points(1, reason);
    }
}

void gain_insight_points(s32b amount, cptr reason)
{
    if (!insight_system_enabled() || amount <= 0)
        return;

    if (p_ptr->insight_points < 0)
        p_ptr->insight_points = 0;
    if (amount > PY_MAX_EXP - p_ptr->insight_points)
        p_ptr->insight_points = PY_MAX_EXP;
    else
        p_ptr->insight_points += amount;

    if (reason && reason[0])
        msg_format("%s You gain %ld insight point%s.", reason,
            (long)amount, amount == 1 ? "" : "s");
    else
        msg_format("You gain %ld insight point%s.", (long)amount,
            amount == 1 ? "" : "s");

    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->window |= PW_PLAYER_0;
}

/* Each threshold is shared by identification and smithing and claimed once
 * per hero. Keep this separate from the optional catastrophe controller. */
void insight_award_milestone(u16b milestone, cptr reason)
{
    if (!p_ptr || !insight_system_enabled() || !milestone
        || (p_ptr->insight_milestones & milestone)) return;
    p_ptr->insight_milestones |= milestone;
    gain_insight_points(1, reason);
}

void insight_artefact_milestones(int difficulty)
{
    for (int i = 0; i < 6; ++i)
    {
        char reason[96];
        int threshold = 15 + 10 * i;
        if (difficulty < threshold) break;
        strnfmt(reason, sizeof(reason),
            "You understand an artefact of difficulty %d.", threshold);
        insight_award_milestone((u16b)(1U << i), reason);
    }
}

int insight_stat_increase_cost(int stat)
{
    if (!p_ptr || !insight_system_enabled() || stat < 0 || stat >= A_MAX
        || p_ptr->stat_base[stat] >= BASE_STAT_MAX) return 0;
    int cost = birth_stat_increase_cost(p_ptr->insight_stat_invested[stat]);
    return cost > 0 && insight_reworked_enabled() ? MAX(2, cost) : cost;
}

bool insight_increase_stat(int stat)
{
    int cost = insight_stat_increase_cost(stat);
    if (!cost || cost > p_ptr->insight_points || death_spectator_active())
        return false;
    p_ptr->insight_points -= cost;
    p_ptr->insight_stat_invested[stat]++;
    p_ptr->stat_base[stat]++;
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->window |= PW_PLAYER_0;
    return true;
}

int insight_ability_upgrade_cost(int skill, int ability)
{
    if (!p_ptr || !b_info || !z_info || !insight_system_enabled()
        || insight_reworked_enabled()
        || skill < 0 || skill >= S_MAX
        || ability < 0 || ability >= ABILITIES_MAX) return 0;
    int index = ability_index(skill, ability);
    if (index < 0 || index >= z_info->b_max) return 0;
    const ability_type* entry = &b_info[index];
    if (!entry->name || entry->skilltype != skill || entry->abilitynum != ability)
        return 0;
    return entry->insight_upgrade_cost;
}

bool insight_upgrade_ability(int skill, int ability)
{
    int cost = insight_ability_upgrade_cost(skill, ability);
    if (!cost || cost > p_ptr->insight_points || death_spectator_active()
        || !p_ptr->innate_ability[skill][ability]
        || p_ptr->insight_ability_upgraded[skill][ability]) return false;
    p_ptr->insight_points -= cost;
    p_ptr->insight_ability_upgraded[skill][ability] = true;
    p_ptr->update |= PU_BONUS;
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->window |= PW_PLAYER_0;
    return true;
}

/*
 * Falling damage. 3d4 for one floor, 6d4 for two floors.
 */
void falling_damage(bool stun)
{
    int dice = 0;
    int dam;

    cptr message;

    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)
    {
        if (p_ptr->depth >= MORGOTH_DEPTH - 1)
            dice = 3; // as this means you will only fall one floor
        else
            dice = 6;
        message = "falling down a chasm";
    }
    else if (cave_stair_bold(p_ptr->py, p_ptr->px))
    {
        dice = 3;
        message = "a collapsing stair";
    }
    else
    {
        dice = 3;
        message = "a collapsing floor";
    }

    // calculate the damage
    dam = damroll(dice, 4);

    if (dice > 0)
    {
        // update the combat rolls window
        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, message);
    }

    if (stun && allow_player_stun(NULL))
    {
        set_stun(p_ptr->stun + dam * 5);
    }

    // reset staircasiness
    p_ptr->staircasiness = 0;
}

/*
 * Advance experience levels and print experience
 */
void check_experience(void)
{
    /* Hack -- lower limit */
    if (p_ptr->exp < 0)
        p_ptr->exp = 0;

    /* Hack -- lower limit */
    if (p_ptr->new_exp < 0)
        p_ptr->new_exp = 0;

    /* Hack -- upper limit */
    if (p_ptr->exp > PY_MAX_EXP)
        p_ptr->exp = PY_MAX_EXP;

    /* Hack -- upper limit */
    if (p_ptr->new_exp > PY_MAX_EXP)
        p_ptr->new_exp = PY_MAX_EXP;

    /* Hack -- maintain "max" experience */
    if (p_ptr->new_exp > p_ptr->exp)
        p_ptr->new_exp = p_ptr->exp;

    /* Redraw experience */
    p_ptr->redraw |= (PR_EXP);

    /* Redraw stuff */
    redraw_stuff();
}

/*
 * Gain experience
 */
void gain_exp(s32b amount)
{
    if (birth_fixed_exp || quest_challenge_active(CHALLENGE_FIXED_50K_XP))
    {
        return;
    }

    /* Gain some experience */
    p_ptr->exp += amount;
    p_ptr->new_exp += amount;

    /* Check Experience */
    check_experience();
}

/*
 * Lose experience
 */
void lose_exp(s32b amount)
{
    /* Never drop below zero experience */
    if (amount > p_ptr->new_exp)
        amount = p_ptr->new_exp;

    /* Lose some experience */
    p_ptr->new_exp -= amount;
    p_ptr->exp -= amount;

    /* Check Experience */
    check_experience();
}
