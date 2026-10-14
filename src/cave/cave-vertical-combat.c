#include "angband.h"
#include "cave/cave-vertical-combat.h"
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
#include "monster/monster-abilities.h"
#include "quest/quest-challenges.h"
#include "quest/quest-rewards-beta.h"
#include "spell/spell-damage.h"
#include "tutorial/tutorial-game.h"
#include "ui/question.h"

/* A floor separation costs three squares of projectile range. Targets must
 * be close to a real opening; this is not targeting through arbitrary floors. */
#define VERTICAL_RANGE 3
#define APERTURE_REACH 4
#define VERTICAL_CHOICES 24

typedef struct vertical_line {
    int y, x, depth, peer_y, peer_x;
} vertical_line;

static s32b combat_turn = -1;
static int combat_depth = -1;

static bool cached_bounds(const level_image* floor, int y, int x)
{
    return floor && y > 0 && x > 0 && y < floor->hgt - 1
        && x < floor->wid - 1;
}

static bool cached_open(const level_image* floor, int y, int x)
{
    return cached_bounds(floor, y, x) && !(floor->info[y][x] & CAVE_WALL);
}

/* Supercover the ray. In particular, diagonal corner gaps between two walls
 * do not transmit an arrow or disclose a monster behind those walls. */
static bool cached_line(const level_image* floor, int y, int x, int ty, int tx)
{
    int nx = ABS(tx - x), ny = ABS(ty - y), ix = 0, iy = 0;
    int sx = tx > x ? 1 : -1, sy = ty > y ? 1 : -1;
    if (!cached_open(floor, y, x) || !cached_open(floor, ty, tx)) return false;
    if ((y != ty || x != tx) && floor->monsters_at[y][x] > 0) return false;
    while (ix < nx || iy < ny)
    {
        int decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;
        if (decision == 0)
        {
            if (!cached_open(floor, y, x + sx)
                || !cached_open(floor, y + sy, x)) return false;
            x += sx; y += sy; ix++; iy++;
        }
        else if (decision < 0) { x += sx; ix++; }
        else { y += sy; iy++; }
        if (!cached_open(floor, y, x)) return false;
        if ((y != ty || x != tx) && floor->monsters_at[y][x] > 0)
            return false;
    }
    return true;
}

static bool opening_line(int y, int x, vertical_line* line)
{
    int depth, py, px;
    if (!p_ptr || !in_bounds(y, x)
        || distance(p_ptr->py, p_ptr->px, y, x) > APERTURE_REACH
        || cave_m_idx[y][x] > 0
        || ((p_ptr->py != y || p_ptr->px != x)
            && projectable(p_ptr->py, p_ptr->px, y, x, PROJECT_CHCK) != PROJECT_CLEAR)
        || !level_cache_vertical_aperture(y, x, &depth, &py, &px)
        || ABS(depth - p_ptr->depth) != 1) return false;
    line->y = y; line->x = x; line->depth = depth;
    line->peer_y = py; line->peer_x = px;
    return true;
}

static int line_range(const vertical_line* line, const monster_type* monster)
{
    return VERTICAL_RANGE + distance(p_ptr->py, p_ptr->px, line->y, line->x)
        + distance(line->peer_y, line->peer_x, monster->fy, monster->fx);
}

static bool visible_target(const level_image* floor, const vertical_line* line,
    const monster_type* monster)
{
    const monster_race* race;
    if (p_ptr->blind || p_ptr->image || !player_can_see_bold(line->y, line->x)
        || !monster->r_idx || monster->hp <= 0
        || !cached_bounds(floor, monster->fy, monster->fx)
        || distance(line->peer_y, line->peer_x, monster->fy, monster->fx)
            > APERTURE_REACH
        || !cached_line(floor, line->peer_y, line->peer_x,
            monster->fy, monster->fx)) return false;
    race = &r_info[monster->r_idx];
    if ((race->flags1 & RF1_PEACEFUL)
        || ((race->flags2 & RF2_INVISIBLE) && !p_ptr->see_inv)) return false;
    /* Torchlight reaches the landing, never the whole resident floor. */
    return floor->light[monster->fy][monster->fx] > 0
        || (p_ptr->cur_light > 0 && line_range(line, monster) <= p_ptr->cur_light);
}

bool cave_vertical_combat_available(int y, int x)
{
    vertical_line line;
    const level_image* floor;
    if (!opening_line(y, x, &line)) return false;
    floor = level_cache_floor(line.depth);
    if (!floor) return false;
    for (int i = 1; i < floor->mon_max; i++)
        if (visible_target(floor, &line, &floor->monsters[i])) return true;
    return false;
}

static int cached_light_penalty(const level_image* floor,
    const monster_type* monster)
{
    return (r_info[monster->r_idx].flags3 & RF3_HURT_LITE)
        ? MAX(0, floor->light[monster->fy][monster->fx] - 2) : 0;
}

static bool cached_sees_player(monster_type* monster, const vertical_line* line)
{
    const monster_race* race = &r_info[monster->r_idx];
    int range = line_range(line, monster);
    if (monster->alertness < ALERTNESS_UNWARY || range > MAX_SIGHT
        || ((race->flags2 & RF2_SHORT_SIGHTED) && range > 2)) return false;
    if (visual_recognition && (race->flags2 & RF2_SMART)
        && !monster_race_is_vala(monster->r_idx))
    {
        int divisor = p_ptr->active_ability[S_STL][STL_DISGUISE] ? 4 : 2;
        int vision = monster_skill(monster, S_PER) / divisor + p_ptr->cur_light
            + ((cave_info[p_ptr->py][p_ptr->px] & CAVE_GLOW) ? 2 : 0);
        if (vision < range) return false;
    }
    return true;
}

static int cached_evasion(const level_image* floor, monster_type* monster)
{
    const monster_race* race = &r_info[monster->r_idx];
    int evasion = race->evn + monster_dodging_bonus_preview(monster)
        - monster->song_evasion_penalty - (monster->stunned ? 2 : 0)
        - cached_light_penalty(floor, monster);
    if (FEAT_IS_ICE(floor->feat[monster->fy][monster->fx])
        && !(race->flags2 & RF2_FLYING)) evasion -= ICE_EVASION_PENALTY;
    evasion += elf_bane_bonus(monster) + dwarf_bane_bonus(monster)
        + edain_bane_bonus(monster) - unique_bane_bonus(monster);
    if (evasion > 0 && (monster->alertness < ALERTNESS_ALERT
        || (race->light > 0 && strchr("@G", race->d_char)
            && cave_light[p_ptr->py][p_ptr->px] <= 0))) evasion /= 2;
    if (evasion > 0) evasion /= 2;
    return monster->alertness < ALERTNESS_UNWARY ? -5 : evasion;
}

static void land_arrow(level_image* floor, const object_type* arrow, int y, int x)
{
    int index;
    /* Same ordinary-arrow floor breakage as horizontal shots (10%). */
    if (percent_chance(10)) return;
    for (index = 1; index < floor->o_max; index++)
        if (!floor->objects[index].k_idx) break;
    if (index >= z_info->o_max) return;
    if (index == floor->o_max) floor->o_max++;
    object_copy(&floor->objects[index], arrow);
    floor->objects[index].number = 1;
    floor->objects[index].iy = y; floor->objects[index].ix = x;
    floor->objects[index].held_m_idx = 0;
    floor->objects[index].next_o_idx = floor->objects_at[y][x];
    floor->objects[index].pickup = true;
    floor->objects[index].pickup_slot = INVEN_QUIVER1;
    floor->objects_at[y][x] = index;
    floor->o_cnt++;
}

static int player_attack(const vertical_line* line, monster_type* target,
    const object_type* arrow)
{
    monster_type local = *target;
    int base = p_ptr->skill_use[S_ARC] + arrow->att
        - line_range(line, target) / 5;
    if (p_ptr->active_ability[S_ARC][ARC_ROUT] && target->stance == STANCE_FLEEING)
        base += 5;
    /* The shared helper's planar distance is inapplicable across floors. */
    local.fy = p_ptr->py; local.fx = p_ptr->px;
    return total_player_attack_ex(&local, base, false, true);
}

bool cave_vertical_combat_interact(int y, int x)
{
    vertical_line line;
    level_image* floor;
    ui_question_option options[VERTICAL_CHOICES];
    char labels[VERTICAL_CHOICES][100];
    int indices[VERTICAL_CHOICES], count = 0, choice, slot, hit, damage = 0;
    int critical_dice = 0;
    object_type arrow, *ammo, *bow = &inventory[INVEN_BOW];
    monster_type target;
    bool deadly_hail;
    u32b af1, af2, af3, af4, bf1, bf2, bf3, bf4;
    u32b arrow_noticed = 0, bow_noticed = 0;
    if (!opening_line(y, x, &line)) return false;
    floor = level_cache_mutable_floor(line.depth);
    if (!floor) return false;
    for (int i = 1; i < floor->mon_max && count < VERTICAL_CHOICES; i++)
    {
        char name[80];
        if (!visible_target(floor, &line, &floor->monsters[i])) continue;
        target = floor->monsters[i]; target.ml = true;
        monster_desc(name, sizeof(name), &target, 0);
        strnfmt(labels[count], sizeof(labels[count]), "Shoot %s %s", name,
            line.depth > p_ptr->depth ? "below" : "above");
        indices[count] = i;
        options[count] = (ui_question_option){ (char)('a' + count),
            labels[count], TERM_WHITE, false };
        count++;
    }
    if (!count) return false;
    choice = ui_question_ask("Through the opening", "Aim at the neighbouring floor.",
        options, count, y, x, 0);
    if (choice < 0) return true;
    if (quest_challenge_forbid_ranged()) return true;
    if (player_active_weapon_kind() != PLAYER_ACTIVE_WEAPON_KIND_BOW
        || !bow->tval || !p_ptr->ammo_tval)
    {
        msg_print("Ready a bow to shoot through the opening."); return true;
    }
    slot = player_quiver_selected_arrow_slot();
    ammo = slot < 0 ? NULL : player_quiver_arrow_object(slot);
    if (!ammo || !ammo->number)
    {
        msg_print("You have no arrows in your quiver."); return true;
    }
    if (p_ptr->afraid || p_ptr->confused || player_submerged_in_deep_water())
    {
        msg_print("You cannot aim through the opening now."); return true;
    }
    target = floor->monsters[indices[choice]]; target.ml = true;
    if (!visible_target(floor, &line, &target)
        || line_range(&line, &target) > archery_range(bow))
    {
        msg_print("That target is beyond your bow's reach."); return true;
    }
    if (!tutorial_game_action_allowed("fire", bow)
        || abort_for_mercy(&target) || abort_for_valorous(&target)) return true;
    object_copy(&arrow, ammo); arrow.number = 1;
    object_flags4(&arrow, &af1, &af2, &af3, &af4);
    object_flags4(bow, &bf1, &bf2, &bf3, &bf4);
    /* The once-per-action accounting matches ordinary archery. */
    p_ptr->energy_use = p_ptr->active_ability[S_ARC][ARC_SKIRMISHING]
        && wearing_only_light_armour() && p_ptr->previous_action[1] >= 1
        && p_ptr->previous_action[1] <= 9 && p_ptr->previous_action[1] != 5 ? 50 : 100;
    p_ptr->previous_action[0] = ACTION_ARCHERY;
    player_attacked = true;
    tutorial_game_action_done("fire", bow);
    deadly_hail = p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL]
        && p_ptr->killed_enemy_with_arrow;
    p_ptr->killed_enemy_with_arrow = false;
    sound(MSG_SHOOT);
    hit = hit_roll(player_attack(&line, &target, &arrow),
        cached_evasion(floor, &target), PLAYER, &target, true);
    if (hit <= 0 && ((af3 | bf3) & TR3_ACCURATE))
        hit = hit_roll(player_attack(&line, &target, &arrow),
            cached_evasion(floor, &target), PLAYER, &target, true);
    if (hit > 0)
    {
        monster_race* race = &r_info[target.r_idx];
        int critical = ((af3 | bf3) & TR3_CUMBERSOME) ? 0
            : crit_bonus(hit, bow->weight, race, S_ARC, false, NULL, NULL);
        int dice, sides, armour_dice, armour_sides, protection, percent;
        if (p_ptr->active_ability[S_ARC][ARC_AMBUSH]
            && target.alertness < ALERTNESS_ALERT) critical++;
        critical_dice = critical;
        dice = bow->dd + critical + slay_bonus(&arrow, &target, &arrow_noticed);
        dice += slay_bonus(bow, &target, &bow_noticed);
        if (deadly_hail) dice *= 2;
        sides = MAX(0, total_ads(bow));
        damage = damroll(dice, sides);
        armour_dice = MAX(0, race->pd - target.song_armor_dice_penalty
            + monster_blocking_bonus_dice_preview(&target)
            + curse_flag_delta_cur(CUR_MON_ARM_DICE));
        armour_sides = MAX(1, monster_base_armour_sides(&target)
            + curse_flag_delta_cur(CUR_MON_ARM_SIDE));
        percent = prt_after_sharpness(&arrow, &arrow_noticed);
        protection = damroll(armour_dice, armour_sides) * percent / 100;
        if (protection >= damage && p_ptr->active_ability[S_ARC][ARC_PUNCTURE])
        { damage = 5; protection = 0; }
        update_combat_rolls2(dice, sides, damage, armour_dice, armour_sides,
            protection, percent, GF_HURT, false);
        damage = MAX(0, damage - protection);
        break_mercy_oath(&target, damage);
        break_valorous_oath(&target, damage, ATT_MAIN, -1);
        quest_beta_bow_hit(damage);
        msg_format("Your arrow hits through the opening for %d damage.", damage);
        if (arrow_noticed || bow_noticed)
            ident_bow_arrow_by_use(bow, &arrow, ammo, &target,
                bow_noticed, arrow_noticed);
    }
    else
    {
        quest_beta_bow_miss();
        msg_print("Your arrow misses through the opening.");
    }
    if (hit > 0 && damage < target.hp)
    {
        apply_weapon_combat_effects(&arrow, &floor->monsters[indices[choice]],
            S_ARC, damage, false, "arrow");
        apply_weapon_combat_effects(bow, &floor->monsters[indices[choice]],
            S_ARC, damage, false, "arrow");
        if (p_ptr->active_ability[S_ARC][ARC_CRIPPLING] && damage > 0
            && critical_dice > 0 && !(r_info[target.r_idx].flags1 & RF1_RES_CRIT)
            && !monster_race_is_vala(target.r_idx)
            && skill_check(PLAYER, 30 - 60 / (critical_dice + 2),
                monster_skill(&target, S_WIL), &target) > 0)
        {
            monster_type* wounded = &floor->monsters[indices[choice]];
            if (!wounded->slowed) wounded->mspeed = MAX(1, wounded->mspeed - 1);
            wounded->slowed = MIN(200, wounded->slowed + critical_dice + 1);
        }
    }
    player_quiver_remove_arrows(slot, 1);
    p_ptr->redraw |= PR_QUIVER;
    floor->monsters[indices[choice]].mflag |= MFLAG_HIT_BY_RANGED | MFLAG_ACTV;
    floor->monsters[indices[choice]].alertness = MAX(ALERTNESS_ALERT,
        floor->monsters[indices[choice]].alertness);
    level_cache_damage_monster(line.depth, indices[choice], damage, " dies.", -1);
    floor = level_cache_mutable_floor(line.depth);
    if (floor)
    {
        p_ptr->killed_enemy_with_arrow = !floor->monsters[indices[choice]].r_idx;
        land_arrow(floor, &arrow, target.fy, target.fx);
    }
    player_attack_audible = true;
    break_truce(false);
    /* A vertical target cannot neutralise a nearby melee opportunity. */
    attacks_of_opportunity(0, 0);
    return true;
}

static bool retaliate(level_image* floor, monster_type* monster,
    const vertical_line* line)
{
    const monster_race* race = &r_info[monster->r_idx];
    int bit, dd, sides, hit, critical, damage, protection, attack, evasion;
    bool boulder;
    char killer[100];
    monster_type visible = *monster;
    if ((race->flags1 & RF1_PEACEFUL)
        || p_ptr->truce || monster->confused || monster->stance == STANCE_FLEEING
        || !cached_sees_player(monster, line)
        || monster->smite_recovery || monster->skip_this_turn
        || monster->skip_next_turn
        || (floor->feat[monster->fy][monster->fx] == FEAT_DEEP_WATER
            && !(race->flags2 & RF2_FLYING))) return false;
    if ((race->flags4 & RF4_ARROW2) && line_range(line, monster) <= 16) bit = 1;
    else if ((race->flags4 & RF4_ARROW1) && line_range(line, monster) <= 10) bit = 0;
    else if ((race->flags4 & RF4_BOULDER) && line_range(line, monster) <= 5) bit = 2;
    else return false;
    if (monster->mana < spell_info_RF4[bit][COL_SPELL_MANA_COST]) return false;
    monster->mana -= spell_info_RF4[bit][COL_SPELL_MANA_COST];
    boulder = bit == 2;
    sides = get_sides(96 + bit); dd = boulder ? 6 : bit + 1;
    visible.ml = visible_target(floor, line, monster);
    attack = race->spell_power - (monster->stunned ? 2 : 0)
        - cached_light_penalty(floor, monster) - line_range(line, monster) / 5
        + elf_bane_bonus(monster) + dwarf_bane_bonus(monster)
        + edain_bane_bonus(monster) - unique_bane_bonus(monster);
    if (attack > 0 && race->light > 0 && strchr("@G", race->d_char)
        && cave_light[p_ptr->py][p_ptr->px] <= 0) attack /= 2;
    evasion = total_player_evasion(&visible, false);
    if (!boulder) evasion /= 2;
    disturb(1, 0);
    msg_print(boulder ? "A rock hurtles through the opening!"
        : "An arrow flies through the opening!");
    hit = hit_roll(attack, evasion, &visible, PLAYER, true);
    if (hit <= 0) return true;
    critical = crit_bonus(hit, boulder ? 100 : sides >= 11 ? 30 : 20,
        &r_info[0], S_ARC, boulder, &visible, NULL);
    damage = damroll(dd + critical, sides);
    protection = protection_roll(GF_HURT, false);
    update_combat_rolls2(dd + critical, sides, damage, -1, -1,
        protection, 100, GF_HURT, false);
    damage = MAX(0, damage - protection);
    monster_desc(killer, sizeof(killer), &visible, 0x88);
    take_hit(damage, killer);
    if (!boulder && damage > 0 && critical > 0 && (race->flags2 & RF2_CRIPPLING)
        && skill_check(&visible, critical * 4,
            p_ptr->skill_use[S_WIL] + p_ptr->free_act * 10, PLAYER) > 0)
        set_slow(p_ptr->slow + critical);
    return true;
}

/* Move one flying creature, never the whole resident map. Preflight both
 * allocation pools so o_pop cannot compact unrelated active-floor objects. */
static bool fly_through(level_image* floor, int index, const vertical_line* line)
{
    monster_type* source = &floor->monsters[index];
    monster_type* arrived;
    int y = -1, x = -1, held = 0, capacity, m_index, object_index;
    if (!(r_info[source->r_idx].flags2 & RF2_FLYING)
        || (r_info[source->r_idx].flags1 & RF1_PEACEFUL)
        || source->stance == STANCE_FLEEING
        || source->fy != line->peer_y || source->fx != line->peer_x)
        return false;
    for (int dy = -1; dy <= 1 && y < 0; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = line->y + dy, xx = line->x + dx;
            if (in_bounds(yy, xx) && cave_floor_bold(yy, xx)
                && cave_m_idx[yy][xx] == 0
                && (yy != p_ptr->py || xx != p_ptr->px)
                && (!dx || !dy || (cave_floor_bold(line->y, xx)
                    && cave_floor_bold(yy, line->x))))
            { y = yy; x = xx; break; }
        }
    if (y < 0) return false;
    capacity = MAX_MONSTERS - mon_max;
    for (int i = 1; !capacity && i < mon_max; i++)
        if (!mon_list[i].r_idx) capacity++;
    if (!capacity) return false;
    for (int i = source->hold_o_idx; i; i = floor->objects[i].next_o_idx)
    {
        if (i <= 0 || i >= floor->o_max || !floor->objects[i].k_idx
            || ++held >= floor->o_max) return false;
    }
    capacity = z_info->o_max - o_max;
    for (int i = 1; i < o_max; i++) if (!o_list[i].k_idx) capacity++;
    if (held > capacity) return false;
    m_index = mon_pop();
    if (!m_index) return false;
    arrived = &mon_list[m_index];
    *arrived = *source;
    arrived->fy = y; arrived->fx = x; arrived->hold_o_idx = 0;
    arrived->ml = false; arrived->cdis = 0;
    arrived->ability_in_action = arrived->ability_melee = arrived->ability_displaced = false;
    memset(arrived->previous_action, ACTION_MISC, sizeof(arrived->previous_action));
    arrived->consecutive_attacks = 0;
    arrived->target_y = arrived->target_x = 0;
    arrived->wandering_idx = 0;
    arrived->social_rival = arrived->social_focus = arrived->social_ally = 0;
    arrived->social_state = arrived->social_timer = arrived->social_memory = 0;
    memset(&arrived->ai, 0, sizeof(arrived->ai));
    memset(&arrived->squad, 0, sizeof(arrived->squad));
    memset(&arrived->routine, 0, sizeof(arrived->routine));
    byte world_initialized = arrived->world.initialized;
    byte supplies = arrived->world.supplies;
    memset(&arrived->world, 0, sizeof(arrived->world));
    arrived->world.initialized = world_initialized;
    arrived->world.supplies = supplies;
    arrived->world.home_y = arrived->fy; arrived->world.home_x = arrived->fx;
    object_index = source->hold_o_idx;
    while (object_index)
    {
        int next = floor->objects[object_index].next_o_idx;
        int destination = o_pop(); /* Guaranteed space by the preflight. */
        o_list[destination] = floor->objects[object_index];
        o_list[destination].held_m_idx = m_index;
        o_list[destination].iy = o_list[destination].ix = 0;
        o_list[destination].next_o_idx = arrived->hold_o_idx;
        arrived->hold_o_idx = destination;
        memset(&floor->objects[object_index], 0, sizeof(object_type));
        floor->o_cnt--; object_index = next;
    }
    cave_m_idx[y][x] = m_index;
    floor->monsters_at[source->fy][source->fx] = 0;
    if (r_info[source->r_idx].flags2 & RF2_MULTIPLY)
    { floor->repro--; num_repro++; }
    if (r_info[source->r_idx].flags1 & RF1_ATTR_MULTI) shimmer_monsters = true;
    /* Uniques are reserved across all resident floors. Ordinary race counts
     * deliberately describe only the active floor (level_image_restore). */
    if (!(r_info[source->r_idx].flags1 & RF1_UNIQUE)) r_info[source->r_idx].cur_num++;
    memset(source, 0, sizeof(*source)); floor->mon_cnt--;
    for (int i = 1; i < floor->mon_max; i++)
    {
        monster_type* other = &floor->monsters[i];
        if (other->social_rival == index || other->social_focus == index
            || other->social_ally == index)
        {
            other->social_rival = other->social_focus = other->social_ally = 0;
            other->social_state = other->social_timer = other->social_memory = 0;
        }
    }
    /* Unique reservations stay unchanged across the transfer. */
    update_mon(m_index, true);
    lite_spot(y, x);
    msg_print("A creature flies through the opening!");
    return true;
}

static void cached_actor_action(level_image* floor, int index,
    const vertical_line* line)
{
    monster_type* monster = &floor->monsters[index];
    int old_y = monster->fy, old_x = monster->fx;
    bool skipped = false;
    monster_abilities_begin_action(monster);
    if (monster->smite_recovery == 1) { skipped = true; goto finished; }
    if (monster->skip_next_turn || monster->skip_this_turn)
    {
        monster->skip_next_turn = monster->skip_this_turn = 0;
        skipped = true; goto finished;
    }
    if (monster->stunned) monster->stunned--;
    if (monster->confused)
    { monster->confused--; skipped = true; goto finished; }
    if (fly_through(floor, index, line)) return;
    if (retaliate(floor, monster, line)) goto finished;
    if (!(r_info[monster->r_idx].flags1 & RF1_NEVER_MOVE))
    {
        int sign = monster->stance == STANCE_FLEEING ? -1 : 1;
        int dy = (line->peer_y > monster->fy) - (line->peer_y < monster->fy);
        int dx = (line->peer_x > monster->fx) - (line->peer_x < monster->fx);
        int y = monster->fy + sign * dy, x = monster->fx + sign * dx;
        if ((dy || dx) && cached_open(floor, y, x)
            && floor->monsters_at[y][x] == 0
            && (floor->feat[y][x] != FEAT_CHASM
                || (r_info[monster->r_idx].flags2 & RF2_FLYING))
            && (!dx || !dy || (cached_open(floor, monster->fy, x)
                && cached_open(floor, y, monster->fx))))
        {
            floor->monsters_at[monster->fy][monster->fx] = 0;
            monster->fy = y; monster->fx = x;
            floor->monsters_at[y][x] = index;
        }
    }
finished:
    monster_abilities_end_action(monster, old_y, old_x, skipped);
    if (monster->hasted) monster->hasted--;
    if (monster->slowed) monster->slowed--;
    monster->mspeed = MIN(7, MAX(1, r_info[monster->r_idx].speed
        + (monster->hasted ? 1 : 0) - (monster->slowed ? 1 : 0)
        + (monster_sprinting_preview(monster) ? 1 : 0)));
}

void cave_vertical_combat_reset(void)
{
    combat_turn = -1; combat_depth = -1;
}

void cave_vertical_combat_process(void)
{
    vertical_line lines[VERTICAL_CHOICES];
    int count = 0, elapsed;
    if (!p_ptr || p_ptr->is_dead || p_ptr->leaving) return;
    if (combat_turn < 0 || combat_depth != p_ptr->depth || turn < combat_turn)
    { combat_turn = turn; combat_depth = p_ptr->depth; return; }
    elapsed = MIN(100, turn - combat_turn);
    combat_turn = turn;
    if (elapsed <= 0) return;
    for (int y = p_ptr->py - APERTURE_REACH;
        y <= p_ptr->py + APERTURE_REACH && count < VERTICAL_CHOICES; y++)
        for (int x = p_ptr->px - APERTURE_REACH;
            x <= p_ptr->px + APERTURE_REACH && count < VERTICAL_CHOICES; x++)
            if (opening_line(y, x, &lines[count])) count++;
    for (int depth = p_ptr->depth - 1; depth <= p_ptr->depth + 1; depth += 2)
    {
        level_image* floor = level_cache_mutable_floor(depth);
        if (!floor) continue;
        for (int index = 1; index < floor->mon_max && !p_ptr->is_dead; index++)
        {
            monster_type* monster = &floor->monsters[index];
            const vertical_line* line = NULL;
            int nearest = MAX_SIGHT;
            if (!monster->r_idx || !cached_bounds(floor, monster->fy, monster->fx))
                continue;
            for (int i = 0; i < count; i++)
            {
                int d;
                if (lines[i].depth != depth) continue;
                d = distance(lines[i].peer_y, lines[i].peer_x,
                    monster->fy, monster->fx);
                if (d <= APERTURE_REACH && d < nearest
                    && cached_line(floor, lines[i].peer_y, lines[i].peer_x,
                        monster->fy, monster->fx)) { line = &lines[i]; nearest = d; }
            }
            if (!line || (!cached_sees_player(monster, line)
                && !(monster->mflag & MFLAG_HIT_BY_RANGED))) continue;
            monster->energy = MIN(399, monster->energy
                + elapsed * extract_energy[monster->mspeed]);
            /* Line of sight through an open landing wakes an unwary creature;
             * asleep creatures still need a real attack to wake them. */
            if (monster->alertness >= ALERTNESS_UNWARY)
                monster->alertness = MAX(monster->alertness, ALERTNESS_ALERT);
            /* process_world runs every ten game ticks: fast actors can owe
             * several actions. Keep the normal energy remainder, bounded to
             * four decisions so a resumed floor never replays a long backlog. */
            while (monster->r_idx && monster->energy >= 100
                && monster->alertness >= ALERTNESS_ALERT && !p_ptr->is_dead)
            {
                if (distance(line->peer_y, line->peer_x, monster->fy, monster->fx)
                        > APERTURE_REACH
                    || !cached_line(floor, line->peer_y, line->peer_x,
                        monster->fy, monster->fx))
                { monster->energy = MIN(99, monster->energy); break; }
                monster->energy -= 100;
                cached_actor_action(floor, index, line);
            }
        }
    }
}
