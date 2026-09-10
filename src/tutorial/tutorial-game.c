#include "angband.h"
#include "externs.h"
#include "tutorial-game.h"
#include "tutorial-world.h"
#include "blitz.h"
#include "metarun.h"
#include "supplies.h"
#include "sdl-config.h"
#include "log/log.h"
#include "ui/question.h"
#include "ui/menu-click.h"
#include "support/movement-input.h"
#include <stddef.h>

/* Observations are made at complete player/UI transactions, never from a
 * redraw callback. The snapshot is transient and never alters a save format. */
static bool started;
static bool opening_pending;
static bool waiting;
static bool managing;
static bool ui_checkpoint_requested;
static char visible_description_type[48];
static int authorized_action_depth;
static bool reached_kinds[65536];
static const char *item_action_ids[] = {
    "item.armour.equip", "item.staff.ready", "item.staff.use",
    "item.horn.ready", "item.horn.use", "item.bow.ready", "item.bow.active",
    "item.bow.use", "item.throwing.ready", "item.throwing.active",
    "item.throwing.use", "item.arrows.active"
};
static bool offered_item_actions[N_ELEMENTS(item_action_ids)];
static u32b observed_tale;
static int previous_y, previous_x, previous_depth, previous_hp, previous_voice;
static int previous_mode, previous_min_depth;
static s16b previous_drain[A_MAX];
static byte previous_abilities[S_MAX][ABILITIES_MAX];

typedef struct condition_lesson {
    const char *id;
    const char *name;
    size_t offset;
    const char *effect;
} condition_lesson;

#define CONDITION(FIELD, NAME, EFFECT) \
    { "status." #FIELD, NAME, offsetof(player_type, FIELD), EFFECT }
static const condition_lesson conditions[] = {
    CONDITION(diseased, "Diseased", "Disease lowers Constitution by 1 on infection and a random attribute by 1 every 50 player turns. Rest does not cure it."),
    CONDITION(poisoned, "Poisoned", "Poison prevents ordinary Health regeneration."),
    CONDITION(cut, "Bleeding", "Bleeding prevents ordinary Health regeneration. Healing halves bleeding; it does not always stop it."),
    CONDITION(stun, "Stunned", "Stun penalizes every skill. More than 100 stun prevents acting."),
    CONDITION(afraid, "Afraid", "Fear prevents normal melee attacks and proper ranged aiming."),
    CONDITION(confused, "Confused", "Confusion interferes with directions and aiming. Inspect your options before acting."),
    CONDITION(blind, "Blind", "You cannot rely on normal sight. Remembered map squares are not current observations."),
    CONDITION(image, "Hallucinating", "Apparent creature and item identities are unreliable while hallucinating."),
    CONDITION(entranced, "Entranced", "You cannot act until the entrancement ends. Continue resumes normal game time; it does not cure you."),
    CONDITION(slow, "Slow", "Slowing changes action speed. Quickness can offset speed without removing the slow condition."),
    CONDITION(fast, "Fast", "Increased speed changes how often you act relative to enemies."),
    CONDITION(rage, "Rage", "Rage gives +1 Strength and Constitution, -1 Dexterity and Grace, restricts awareness, and prevents stealth."),
    CONDITION(darkened, "Darkened", "Your light is dimmed. This is different from empty fuel or blindness."),
    CONDITION(tmp_str, "Strong", "Temporary Strength increases Strength and sustains it. Check the live attribute breakdown."),
    CONDITION(tmp_dex, "Agile", "Temporary Dexterity improves its associated skills and sustains Dexterity."),
    CONDITION(tmp_con, "Resilient", "Temporary Constitution changes maximum Health and sustains Constitution."),
    CONDITION(tmp_gra, "Grace", "Temporary Grace changes Grace-based skills and maximum Voice, and sustains Grace."),
    CONDITION(tmp_per, "Perceptive", "Temporary Perception improves your current Perception skill."),
    CONDITION(tim_invis, "True sight", "Your temporary sight improves detection of invisible creatures and grants its sight protections."),
    CONDITION(oppose_fire, "Resist fire", "An additional temporary resistance layer reduces fire damage."),
    CONDITION(oppose_cold, "Resist cold", "An additional temporary resistance layer reduces cold damage."),
    CONDITION(oppose_pois, "Resist poison", "An additional temporary resistance layer reduces poison damage."),
    CONDITION(song_challenge_effect, "Challenge echo", "A song has left a temporary effect. Read your current skill breakdown."),
    CONDITION(song_elbereth_effect, "Elbereth echo", "A song has left a temporary effect. Read your current skill breakdown.")
};
#undef CONDITION
static int previous_conditions[N_ELEMENTS(conditions)];

static void observe_extra_states(bool seed)
{
    static const char *ids[] = {
        "status.heavy_stun", "status.knocked_out", "status.full",
        "status.song_lockout_timer", "status.song_contest_player_stacks",
        "status.climbing", "status.leaping", "status.knocked_back",
        "status.skip_next_turn", "status.was_entranced", "status.vengeance",
        "status.focused", "status.concentration", "status.power_throw",
        "status.singing", "status.in_pit", "status.in_web", "status.sunlight",
        "status.cursed", "status.running", "status.smithing", "status.fletching",
        "status.resting", "status.repeat", "status.mortal_wound", "status.on_the_run"
    };
    static bool before[N_ELEMENTS(ids)];
    bool grid = in_bounds(p_ptr->py, p_ptr->px);
    bool now[] = {
        p_ptr->stun >= 50 && p_ptr->stun <= 100, p_ptr->stun > 100,
        p_ptr->food >= PY_FOOD_FULL, p_ptr->song_lockout_timer > 0,
        p_ptr->song_contest_player_stacks > 0, p_ptr->climbing, p_ptr->leaping,
        p_ptr->knocked_back, p_ptr->skip_next_turn, p_ptr->was_entranced,
        p_ptr->vengeance > 0 && p_ptr->active_ability[S_WIL][WIL_VENGEANCE],
        p_ptr->focused && p_ptr->active_ability[S_PER][PER_FOCUSED_ATTACK],
        p_ptr->consecutive_attacks > 0 && p_ptr->active_ability[S_PER][PER_CONCENTRATION],
        player_power_throw_ready(), p_ptr->song1 != SNG_NOTHING,
        grid && cave_pit_bold(p_ptr->py, p_ptr->px) && !p_ptr->leaping,
        grid && cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB,
        grid && cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT,
        p_ptr->cursed, p_ptr->running > 0, p_ptr->smithing > 0,
        p_ptr->fletching > 0, p_ptr->resting != 0, p_ptr->command_rep > 0,
        p_ptr->cut > 100, p_ptr->on_the_run
    };
    for (int i = 0; i < (int)N_ELEMENTS(ids); ++i) {
        tutorial_status status = tutorial_lesson_status(ids[i]);
        if (!seed && now[i] && (!before[i] || status == TUTORIAL_UNSEEN
            || status == TUTORIAL_IN_PROGRESS)) {
            tutorial_context context = {0};
            SDL_strlcpy(context.subject_type, ids[i] + 7, sizeof(context.subject_type));
            SDL_strlcpy(context.subject, ids[i] + 7, sizeof(context.subject));
            tutorial_observe(ids[i], &context);
        }
        before[i] = now[i];
    }
}

static bool gameplay_available(void)
{
    return !managing && character_generated && p_ptr && started
        && !run_mode_is_blitz() && !p_ptr->tutorial_deferred
        && !p_ptr->is_dead && !death_spectator_active();
}

static void observe(const char *id, const char *type, const char *subject,
    const char *detail)
{
    tutorial_context context = {0};
    if (!gameplay_available() || !tutorial_enabled()) return;
    SDL_strlcpy(context.subject_type, type ? type : "", sizeof(context.subject_type));
    SDL_strlcpy(context.subject, subject ? subject : "", sizeof(context.subject));
    SDL_strlcpy(context.text, detail ? detail : "", sizeof(context.text));
    tutorial_observe(id, &context);
}

static const char *item_type(const object_type *item)
{
    if (!item || !item->k_idx) return "item";
    switch (item->tval) {
    case TV_STAFF: return "staff";
    case TV_HORN: return "horn";
    case TV_BOW: return "bow";
    case TV_ARROW: return "arrows";
    case TV_RING: return "ring";
    case TV_AMULET: return "amulet";
    case TV_POTION: return "potion";
    case TV_FOOD: return "food";
    case TV_LIGHT: return "light";
    case TV_FLASK: return "oil";
    case TV_GEM: return "gem";
    case TV_DIGGING: return "digging";
    case TV_METAL: return "metal";
    case TV_CHEST: return "chest";
    case TV_SKELETON: return "skeleton";
    case TV_SHIELD: return "shield";
    case TV_BOOTS: case TV_GLOVES: case TV_HELM: case TV_CROWN:
    case TV_CLOAK: case TV_SOFT_ARMOR: case TV_MAIL: return "armour";
    case TV_SWORD: case TV_HAFTED: case TV_POLEARM:
        return player_can_treat_as_throwing(item) ? "throwing" : "weapon";
    default: return "item";
    }
}

static bool item_is_remedy(const object_type *item, const char *condition)
{
    if (!item || !item->k_idx || !object_aware_p(item)) return false;
    if (item->tval == TV_POTION) {
        if (!strcmp(condition, "drain")) {
            static const int potions[A_MAX] = { SV_POTION_STR, SV_POTION_DEX,
                SV_POTION_CON, SV_POTION_GRA };
            for (int stat = 0; stat < A_MAX; ++stat)
                if (item->sval == potions[stat] && p_ptr->stat_drain[stat] <= -3)
                    return true;
            return false;
        }
        if (!strcmp(condition, "diseased"))
            return item->sval == SV_POTION_HEALING || item->sval == SV_POTION_MIRUVOR;
        if (item->sval == SV_POTION_MIRUVOR)
            return strstr("poisoned cut stun afraid confused blind image health voice", condition) != NULL;
        if (!strcmp(condition, "poisoned")) return item->sval == SV_POTION_ANTIDOTE;
        if (!strcmp(condition, "cut") || !strcmp(condition, "health"))
            return item->sval == SV_POTION_HEALING;
        if (!strcmp(condition, "voice")) return item->sval == SV_POTION_VOICE
            || (item->sval == SV_POTION_ESGALDUIN && p_ptr->msp >= 4);
        if (!strcmp(condition, "stun") || !strcmp(condition, "confused")
            || !strcmp(condition, "rage")) return item->sval == SV_POTION_CLARITY;
        if (!strcmp(condition, "blind")) return item->sval == SV_POTION_true_SIGHT;
        if (!strcmp(condition, "image")) return item->sval == SV_POTION_CLARITY
            || item->sval == SV_POTION_true_SIGHT;
    }
    if (item->tval == TV_FOOD) {
        if (!strcmp(condition, "cut") || !strcmp(condition, "health"))
            return item->sval == SV_FOOD_HEALING;
        if (!strcmp(condition, "drain")) return item->sval == SV_FOOD_RESTORATION
            || (item->sval == SV_FOOD_LEMBAS && p_ptr->stat_drain[A_GRA] < 0);
        if (!strcmp(condition, "hunger")) return item->sval >= SV_FOOD_MIN_FOOD
            || item->sval == SV_FOOD_SUSTENANCE;
    }
    return false;
}

static bool available_remedy(const char *condition)
{
    if (p_ptr->entranced || p_ptr->stun > 100) return false;
    for (int i = 0; i < INVEN_TOTAL; ++i)
        if (item_is_remedy(&inventory[i], condition)) return true;
    for (int i = 0; i < player_carried_extra_entry_count(); ++i)
        if (item_is_remedy(player_carried_extra_entry_at(i), condition)) return true;
    for (int i = 0; i < supplies_entry_count(); ++i)
        if (item_is_remedy(supplies_entry_at(i), condition)) return true;
    return false;
}

static void offer_remedy(const char *condition, const char *name)
{
    char id[80];
    strnfmt(id, sizeof(id), "status.%s.remedy", condition);
    if (!available_remedy(condition)) {
        tutorial_forget_observation(id);
        return;
    }
    observe(id, condition, name,
        "Choose a known remedy in Supplies. Read its effect before using it. Pack access retains its normal time cost.");
}

void tutorial_game_item(const object_type *item)
{
    char name[160], id[80], detail[384];
    const char *type;
    if (!gameplay_available() || !item || !item->k_idx || p_ptr->image) return;
    if (item->tval == TV_SKELETON || item->tval == TV_CHEST) {
        object_desc(name, sizeof(name), item, true, 3);
        observe(item->tval == TV_SKELETON ? "world.skeleton" : "world.chest",
            item->tval == TV_SKELETON ? "skeleton" : "chest", name,
            "Use this feature's own interaction and inspect the available choices before committing.");
        return;
    }
    type = item_type(item);
    reached_kinds[(u16b)item->k_idx] = true;
    object_desc(name, sizeof(name), item, true, 3);
    strnfmt(detail, sizeof(detail), "Examine %s to read its known properties and handling requirements.", name);
    observe("item.first_description", type, name, detail);
    if (tutorial_lesson_status("item.first_description") == TUTORIAL_UNSEEN
        || tutorial_lesson_status("item.first_description") == TUTORIAL_IN_PROGRESS) return;
    strnfmt(id, sizeof(id), "item.%s", type);
    observe(id, type, name, detail);
    if (tutorial_lesson_status(id) == TUTORIAL_UNSEEN
        || tutorial_lesson_status(id) == TUTORIAL_IN_PROGRESS) return;
    /* Effect cards use aware kinds only; no hidden subtype is exposed. */
    if (object_aware_p(item) && (item->tval == TV_POTION || item->tval == TV_FOOD
        || item->tval == TV_GEM || item->tval == TV_STAFF || item->tval == TV_HORN)) {
        strnfmt(id, sizeof(id), "effect.%d", item->k_idx);
        observe(id, type, name, detail);
    }
    if (object_known_p(item) && item->name1)
        observe("item.artefact", type, name, "Read the entire known description, including drawbacks and granted abilities.");
}

void tutorial_game_item_described(const object_type *item)
{
    char name[160], detail[384];
    if (!item || !item->k_idx || !gameplay_available() || p_ptr->image) return;
    if (item->tval == TV_SKELETON || item->tval == TV_CHEST) {
        tutorial_game_item_description_closed();
        tutorial_game_item(item);
        return;
    }
    SDL_strlcpy(visible_description_type, item_type(item), sizeof(visible_description_type));
    reached_kinds[(u16b)item->k_idx] = true;
    object_desc(name, sizeof(name), item, true, 3);
    strnfmt(detail, sizeof(detail), "%s. Weight: %.1f lb. Only revealed properties belong in this description.", name, item->weight / 10.0);
    tutorial_game_action_done("examine", item);
    observe("item.description", item_type(item), name, detail);
    ui_checkpoint_requested = true;
}

void tutorial_game_item_description_closed(void)
{
    visible_description_type[0] = '\0';
}

void tutorial_game_identified(const object_type *item, const char *reason)
{
    char name[160], id[80];
    if (!gameplay_available() || !item || !item->k_idx || p_ptr->image) return;
    if (item->tval == TV_SKELETON || item->tval == TV_CHEST) return;
    reached_kinds[(u16b)item->k_idx] = true;
    object_desc(name, sizeof(name), item, true, 3);
    observe(reason ? reason : "identification.item", item_type(item), name,
        "The game has revealed new information about this item. Open its updated description to see what you learned.");
    /* Capture the revealed effect before the last consumable disappears.
     * Queue information only; the next safe checkpoint displays the card. */
    if (object_aware_p(item) && (item->tval == TV_POTION || item->tval == TV_FOOD
        || item->tval == TV_GEM || item->tval == TV_STAFF || item->tval == TV_HORN)) {
        strnfmt(id, sizeof(id), "effect.%d", item->k_idx);
        observe(id, item_type(item), name, "This item's effect has been revealed.");
    }
}

void tutorial_game_explain(const char *id, const char *subject, const char *detail)
{
    observe(id, "", subject, detail);
}

void tutorial_game_explain_now(const char *id, const char *subject, const char *detail)
{
    observe(id, "", subject, detail);
    ui_checkpoint_requested = true;
    tutorial_game_wait();
}

void tutorial_game_menu(const char *id, const char *description)
{
    char lesson[80];
    if (!gameplay_available() || waiting) return;
    tutorial_menu_opened(id);
    strnfmt(lesson, sizeof(lesson), "menu.%s", id);
    observe(lesson, id, id, description);
    ui_checkpoint_requested = true;
}

void tutorial_game_ability(int skill, int ability, bool before_purchase)
{
    char id[80];
    ability_type *entry;
    int index;
    if (!gameplay_available() || skill < 0 || skill >= S_MAX
        || ability < 0 || ability >= ABILITIES_MAX) return;
    index = ability_index(skill, ability);
    if (index < 0 || index >= z_info->b_max) return;
    entry = &b_info[index];
    strnfmt(id, sizeof(id), "ability.%d.preview", index);
    observe(id, "ability", b_name + entry->name,
        before_purchase ? "Read the live requirements, effect and XP cost. Continue returns to your purchase decision; it does not buy the ability."
        : "This ability is now available. Read its effect and current activation requirements.");
    if (before_purchase) ui_checkpoint_requested = true;
}

/* Pump the frontend, without entering the actor scheduler or consuming the
 * input which belongs to the owning game menu. SDL handles card controls. */
void tutorial_game_wait(void)
{
    tutorial_view view;
    if (waiting || managing || !gameplay_available()) return;
    waiting = true;
    if (ui_checkpoint_requested) {
        ui_checkpoint_requested = false;
        tutorial_checkpoint(true);
    }
    while (tutorial_get_view(&view)) {
        if (view.kind == TUTORIAL_STEP_ACTION) {
            /* A preview can already be open when its explanatory card yields
             * to Examine. The displayed description satisfies this read-only
             * step; do not require toggling it closed and opening it again. */
            if (!strcmp(view.action, "examine") && visible_description_type[0]
                && !p_ptr->image && (!view.action_subject[0]
                    || !strcmp(view.action_subject, visible_description_type))) {
                tutorial_action_finished("examine", visible_description_type, true);
                tutorial_checkpoint(true);
                continue;
            }
            /* Do not tell an already-stealthy player to toggle stealth off.
             * The introductory explanation has been read; its desired state
             * is satisfied without adding a fake action or game turn. */
            if (!strcmp(view.action, "stealth") && p_ptr->stealth_mode) {
                tutorial_action_finished("stealth", "", true);
                tutorial_checkpoint(true);
                continue;
            }
            break;
        }
        Term_fresh();
        Term_xtra(TERM_XTRA_EVENT, 1);
        if (!p_ptr->playing || p_ptr->leaving || p_ptr->is_dead) {
            tutorial_invalidate_context();
            break;
        }
    }
    waiting = false;
}

static bool target_can_be_attacked(const monster_type *monster)
{
    if (!monster || !monster->r_idx || !monster->ml || p_ptr->afraid
        || p_ptr->confused || p_ptr->image || p_ptr->truce) return false;
    if (r_info[monster->r_idx].flags1 & RF1_PEACEFUL) return false;
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE) return false;
    if (merciless_attack((monster_type *)monster) || cowardly_attack((monster_type *)monster)) return false;
    return true;
}

static bool tutorial_monster_observable(const monster_type *monster)
{
    return monster && monster->r_idx && monster->ml
        && player_has_los_bold(monster->fy, monster->fx)
        && !(r_info[monster->r_idx].flags1 & RF1_PEACEFUL) && !p_ptr->image;
}

/* Shared by the live observation and the first-map generation check. */
static bool tutorial_first_monster_target(const monster_type *monster)
{
    return !p_ptr->rage && !p_ptr->entranced && p_ptr->stun <= 100
        && tutorial_monster_observable(monster) && target_can_be_attacked(monster);
}

bool tutorial_game_first_monster_triggered(void)
{
    for (int i = 1; i < mon_max; ++i)
        if (tutorial_first_monster_target(&mon_list[i])) return true;
    return false;
}

bool tutorial_game_target_allowed(int y, int x)
{
    tutorial_view view;
    if (!tutorial_peek_view(&view) || !tutorial_action_waiting()
        || (strcmp(view.id, "item.bow.use") && strcmp(view.id, "item.throwing.use")
            && strcmp(view.id, "item.horn.use"))) return true;
    if (!in_bounds(y, x) || cave_m_idx[y][x] <= 0) return false;
    return target_can_be_attacked(&mon_list[cave_m_idx[y][x]]);
}

/* All target facts below are visible or recorded lore. Never reveal a hidden
 * monster property merely because a lesson happens to be available. */
static bool useful_instrument(const object_type *item)
{
    u32b immunity = 0;
    if (!item || !item->k_idx || !object_aware_p(item)
        || object_has_broken_prefix(item) || p_ptr->truce
        || p_ptr->entranced || p_ptr->stun > 100 || p_ptr->confused || p_ptr->image) return false;
    if (item->tval == TV_STAFF) {
        if (item->pval < CHANNELING_CHARGE_MULTIPLIER) return false;
        switch (item->sval) {
        case SV_STAFF_SLUMBER: immunity = RF3_NO_SLEEP; break;
        case SV_STAFF_MAJESTY: immunity = RF3_NO_FEAR; break;
        case SV_STAFF_DISMAY: immunity = RF3_NO_CONF; break;
        case SV_STAFF_SELF_KNOWLEDGE: return true;
        case SV_STAFF_SANCTITY:
            for (int i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
                if (inventory[i].k_idx && player_equipment_slot_counts_as_equipped(i)
                    && object_known_p(&inventory[i]) && cursed_p(&inventory[i])) return true;
            return false;
        case SV_STAFF_WARDING: return cave_clean_bold(p_ptr->py, p_ptr->px);
        default: return false;
        }
    } else if (item->tval == TV_HORN) {
        int cost = p_ptr->active_ability[S_WIL][WIL_CHANNELING] ? 10 : 20;
        if (p_ptr->csp < cost) return false;
        switch (item->sval) {
        case SV_HORN_TERROR: immunity = RF3_NO_FEAR; break;
        case SV_HORN_THUNDER: case SV_HORN_FORCE: break;
        default: return false;
        }
    } else return false;
    bool target = false;
    for (int i = 1; i < mon_max; ++i) {
        const monster_type *monster = &mon_list[i];
        if (!monster->r_idx || !monster->ml
            || !player_has_los_bold(monster->fy, monster->fx)) continue;
        if (item->tval == TV_HORN && (abs(monster->fy - p_ptr->py) > 3
            || abs(monster->fx - p_ptr->px) > 3)) continue;
        /* Area effects must not teach breaking a truce, oath or peace. */
        if (!target_can_be_attacked(monster)) return false;
        if (l_list[monster->r_idx].flags3 & immunity) continue;
        if (item->tval == TV_STAFF && item->sval == SV_STAFF_SLUMBER
            && monster->alertness < ALERTNESS_UNWARY) continue;
        if ((immunity == RF3_NO_FEAR) && monster->stance == STANCE_FLEEING) continue;
        target = true;
    }
    return target;
}

void tutorial_game_attack(const monster_type *monster)
{
    if (!gameplay_available()) return;
    tutorial_action_finished("attack", "monster", true);
    observe("combat.first_result", "monster", "Attack result",
        "Attack + d20 must exceed Evasion + d20; ties miss. On a hit, Protection is rolled and subtracted from damage.");
    (void)monster;
}

void tutorial_game_combat_roll(const combat_roll *roll)
{
    char detail[384];
    if (!gameplay_available() || !roll || !roll->is_attacker_player) return;
    if (roll->no_damage)
        strnfmt(detail, sizeof(detail), "Attack %d + roll %d = %d; Evasion %d + roll %d = %d. No damage roll followed this contest.",
            roll->att, roll->att_roll, roll->att + roll->att_roll,
            roll->evn, roll->evn_roll, roll->evn + roll->evn_roll);
    else strnfmt(detail, sizeof(detail), "Attack total %d; Evasion total %d. Damage rolled %d; Protection rolled %d (%d%% applies). The combat history keeps the complete result.",
        roll->att + roll->att_roll, roll->evn + roll->evn_roll, roll->dam, roll->prot, roll->prt_percent);
    observe("combat.first_result", "combat", "Your combat roll", detail);
}

bool tutorial_game_action_allowed(const char *action, const object_type *item)
{
    tutorial_view view;
    if (managing || authorized_action_depth || !tutorial_get_view(&view)) return true;
    if (!tutorial_action_permitted(action)) return false;
    if (!strcmp(action, "open-menu") || !strcmp(action, "close-menu")) return true;
    if (item && !strncmp(view.id, "status.", 7) && strstr(view.id, ".remedy"))
        return item_is_remedy(item, view.context.subject_type);
    if (item && !strcmp(action, "use-item")
        && (!strcmp(view.id, "item.staff.use") || !strcmp(view.id, "item.horn.use"))
        && !useful_instrument(item)) return false;
    if (item && !strncmp(view.id, "item.", 5)
        && strcmp(view.id, "item.first_description") && strcmp(view.id, "item.description")
        && view.context.subject_type[0]) {
        const char *type = item_type(item);
        if (strcmp(type, view.context.subject_type)
            && !(strcmp(view.context.subject_type, "armour") == 0 && strcmp(type, "shield") == 0))
            return false;
    }
    if (item && (!strcmp(action, "equip") || !strcmp(action, "change-active"))
        && smith_oath_forbids_object(item)) return false;
    return true;
}

void tutorial_game_action_done(const char *action, const object_type *item)
{
    tutorial_view view;
    const char *type = item_type(item);
    /* A permitted Change Active setup can finish its paid Pack access through
     * the wield callback. Actual equipping also readies that chosen item. */
    if (item && !strcmp(action, "equip") && tutorial_action_waiting()
        && !strcmp(tutorial_current_action(), "ready")) action = "ready";
    if (tutorial_peek_view(&view) && !strncmp(view.id, "status.", 7)
        && strstr(view.id, ".remedy") && item_is_remedy(item, view.context.subject_type))
        type = view.context.subject_type;
    tutorial_action_finished(action, type, true);
}

static void observe_item_action(const char *id, const char *type,
    const char *name, const char *detail)
{
    for (int i = 0; i < (int)N_ELEMENTS(item_action_ids); ++i)
        if (!strcmp(id, item_action_ids[i])) offered_item_actions[i] = true;
    observe(id, type, name, detail);
}

static void offer_item_actions(const object_type *item, bool hostile_target)
{
    char id[80], name[160];
    const char *type;
    bool ready;
    if (!item || !item->k_idx) return;
    type = item_type(item);
    if (!strcmp(type, "armour") && object_known_p(item)
        && !cursed_p(item) && !smith_oath_forbids_object(item)
        && (tutorial_lesson_status("item.armour") == TUTORIAL_COMPLETED
            || tutorial_lesson_status("item.armour") == TUTORIAL_SKIPPED)) {
        int slot = wield_slot(item);
        if (slot >= INVEN_WIELD && slot < INVEN_TOTAL && !inventory[slot].k_idx) {
            object_desc(name, sizeof(name), item, true, 3);
            observe_item_action("item.armour.equip", "armour", name, "This known armour fits an empty slot. Inspect the listed changes, then equip it or skip to keep your current setup.");
        }
        return;
    }
    if (strcmp(type, "staff") && strcmp(type, "horn")
        && strcmp(type, "bow") && strcmp(type, "throwing")) return;
    strnfmt(id, sizeof(id), "item.%s", type);
    if (tutorial_lesson_status(id) == TUTORIAL_UNSEEN
        || tutorial_lesson_status(id) == TUTORIAL_IN_PROGRESS) return;
    object_desc(name, sizeof(name), item, true, 3);
    ready = inventory_limit_group_for_object(item) == INV_LIMIT_HARNESS
        || player_inventory_handle_is_equipped(player_inventory_handle_for_object(item));
    if (!ready) {
        object_type moving = *item;
        moving.storage = OBJECT_STORAGE_HARNESS;
        if (!object_can_choose_pack_or_harness(item)
            || !inventory_type_slot_available(&moving, false)) return;
        strnfmt(id, sizeof(id), "item.%s.ready", type);
        observe_item_action(id, type, name, "Open Inventory, select this item and choose Ready to move it to the Harness. Reaching into the Pack takes three turns.");
        return;
    }
    strnfmt(id, sizeof(id), "item.%s.ready", type);
    if (!strcmp(type, "staff")) {
        if (useful_instrument(item))
            observe_item_action("item.staff.use", type, name, "Use this known staff from the Harness once. It spends a charge; effects on enemies may be resisted.");
    } else if (!strcmp(type, "horn")) {
        int cost = p_ptr->active_ability[S_WIL][WIL_CHANNELING] ? 10 : 20;
        if (useful_instrument(item)) {
            char detail[200];
            strnfmt(detail, sizeof(detail), "Sound this known horn toward a hostile target. It costs %d Voice and makes a loud noise.", cost);
            observe_item_action("item.horn.use", type, name, detail);
        }
    } else {
        if (smith_oath_forbids_object(item)) return;
        bool active = !strcmp(type, "bow")
            ? player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW
            : player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_THROWING;
        if (!active) {
            strnfmt(id, sizeof(id), "item.%s.active", type);
            observe_item_action(id, type, name, "Open Change Active and choose this type of weapon. Only ready gear is offered; the normal setup cost still applies.");
        } else if (hostile_target && (!strcmp(type, "throwing") || player_quiver_selected_arrow_slot() >= 0)) {
            strnfmt(id, sizeof(id), "item.%s.use", type);
            observe_item_action(id, type, name, "Choose a visible hostile target and make one ranged attack. The normal time, ammunition and reaction rules apply.");
        }
    }
}

bool tutorial_game_begin_action(const char *action, const object_type *item)
{
    if (!tutorial_game_action_allowed(action, item)) return false;
    ++authorized_action_depth;
    return true;
}

void tutorial_game_end_action(void)
{
    if (authorized_action_depth > 0) --authorized_action_depth;
}

void tutorial_game_item_used(const object_type *item)
{
    tutorial_game_action_done("use-item", item);
}

bool tutorial_game_command_allowed(int command, int direction)
{
    const char *action = "other";
    if (!tutorial_is_active() || managing) return true;
    if (command == ESCAPE) { tutorial_skip(); return false; }
    switch (command) {
    case 'S': action = "stealth"; break;
    case ';': case '/':
        if (!direction) { action = "direction"; break; }
        if (direction < 1 || direction > 9 || direction == 5) return false;
        if (direction >= 1 && direction <= 9 && direction != 5
            && in_bounds(p_ptr->py + ddy[direction], p_ptr->px + ddx[direction])) {
            int y = p_ptr->py + ddy[direction], x = p_ptr->px + ddx[direction];
            int monster = cave_m_idx[y][x];
            if (monster > 0) {
                if (!target_can_be_attacked(&mon_list[monster])) return false;
                action = "attack";
            } else if (command == ';' && cave_floor_bold(y, x)
                && !cave_pit_bold(y, x) && cave_feat[y][x] != FEAT_CHASM
                && !(cave_info[y][x] & CAVE_MARK && cave_feat[y][x] >= FEAT_TRAP_HEAD
                    && cave_feat[y][x] <= FEAT_TRAP_TAIL)) action = "move";
        } else return false;
        break;
    case 'x': action = "examine"; break;
    case 'w': action = "equip"; break;
    case 'u': case 'a': case 'p': case 'q': case 'E': action = "use-item"; break;
    case '\t': case CMD_ACTIVE_WEAPON_MODE: case KTRL('F'): action = "change-active"; break;
    case 's': action = "song"; break;
    case 'f': case 'F': action = player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_THROWING
        ? "throw" : "fire"; break;
    case 't': case KTRL('T'): action = "throw"; break;
    case 'g': action = "pickup"; break;
    case 'z': action = "wait"; break;
    case 'i': case 'e': case 'r': case 'j': case 'm': case 'h': case '@':
    case 'y': case 'H': case 'O': case '?': case 'l': case 'L': case 'M':
    case '[': case ']': case '~': case KTRL('P'): case KTRL('Q'):
        action = "open-menu"; break;
    case KTRL('S'): case KTRL('X'): case KTRL('C'):
        tutorial_invalidate_context(); return true;
    }
    if (!strcmp(action, "direction")) {
        const char *expected = tutorial_current_action();
        return !strcmp(expected, "move") || !strcmp(expected, "attack");
    }
    return tutorial_game_action_allowed(action, NULL);
}

static void tutorial_game_upgrade_notice(void)
{
    metarun *tale = metarun_current_mutable();
    if (!tale || !tale->tutorial_upgrade_pending || run_mode_is_blitz()) return;
    bool was_managing = managing;
    managing = true;
    const char *text = p_ptr->tutorial_deferred
        ? "You have updated the game to version 0.9.8. This version adds gameplay tutorials. For consistency, they will be enabled for your next new character, after this one dies. This character will continue without gameplay tutorials."
        : "You have updated the game to version 0.9.8. This version adds gameplay tutorials. You are starting a new character, so tutorials are available now.";
    char message[700];
    strnfmt(message, sizeof(message), "%s %s", text,
        get_sdl_gameplay_tutorial_mode() == TUTORIAL_MODE_DISABLED
        ? "Your saved tutorial setting is Disabled. They will stay off until you choose Normal or Extended in Options."
        : "Extended is the default. You can choose Disabled, Normal or Extended in Options; changing this setting will not enable tutorials for an older character.");
    ui_question_option options[] = {{'c', "Continue", TERM_WHITE, false}};
    int choice = ui_question_ask_overlay("Updated to 0.9.8", message,
        options, 1, UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 0);
    if (choice == 0) {
        tale->tutorial_upgrade_pending = 0;
        metar.tutorial_upgrade_pending = 0;
        if (save_metaruns() != 0) {
            tale->tutorial_upgrade_pending = 1;
            metar.tutorial_upgrade_pending = 1;
            log_warn("Could not persist the tutorial upgrade notice for Tale %u", (unsigned)tale->id);
        }
    }
    managing = was_managing;
}

void tutorial_game_start(void)
{
    const metarun *tale = metarun_current();
    /* dungeon() calls this on every level. Preserve comparison snapshots on
     * ordinary travel, so the next safe checkpoint can observe the transition. */
    if (started && observed_tale == (tale ? tale->id : 0)
        && !p_ptr->restoring && playerturn > 1) return;
    tutorial_game_item_description_closed();
    tutorial_invalidate_context();
    tutorial_set_mode(get_sdl_gameplay_tutorial_mode());
    tutorial_set_character_blocked(p_ptr->tutorial_deferred);
    tutorial_sync_tale();
    tutorial_game_upgrade_notice();
    if (!started || playerturn <= 1 || observed_tale != (tale ? tale->id : 0))
        memset(reached_kinds, 0, sizeof(reached_kinds));
    started = true;
    tutorial_world_start();
    observed_tale = tale ? tale->id : 0;
    previous_y = p_ptr->py; previous_x = p_ptr->px;
    previous_depth = p_ptr->depth; previous_hp = p_ptr->chp;
    previous_voice = p_ptr->csp; previous_mode = player_active_weapon_mode();
    previous_min_depth = min_depth();
    memcpy(previous_drain, p_ptr->stat_drain, sizeof(previous_drain));
    memcpy(previous_abilities, p_ptr->have_ability, sizeof(previous_abilities));
    for (int i = 0; i < (int)N_ELEMENTS(conditions); ++i)
        previous_conditions[i] = *(const s16b *)((const char *)p_ptr + conditions[i].offset);
    observe_extra_states(true);
    opening_pending = (!p_ptr->restoring && playerturn <= 1)
        || tutorial_lesson_status("opening.move") == TUTORIAL_IN_PROGRESS;
    if (opening_pending)
        observe("opening.move", "", "Your first steps",
            "Find a suitable weapon and armour. If your oath restricts found equipment, follow its rules and prepare your own gear.");
}

void tutorial_game_checkpoint(void)
{
    char detail[384], id[80];
    const metarun *tale = metarun_current();
    if (!started || (tale && tale->id != observed_tale)) tutorial_game_start();
    if (!gameplay_available()) { tutorial_invalidate_context(); return; }
    if (p_ptr->depth != previous_depth) {
        tutorial_invalidate_context();
        observe("world.depth", "depth", "A new level", "Stairs generate a new level. Your current and minimum depth govern the available routes.");
        previous_depth = p_ptr->depth;
    }
    if (opening_pending) {
        observe("opening.move", "", "Your first steps",
            "Find a suitable weapon and armour. If your oath restricts found equipment, follow its rules and prepare your own gear.");
        tutorial_status status = tutorial_lesson_status("opening.move");
        if (status == TUTORIAL_COMPLETED || status == TUTORIAL_SKIPPED) opening_pending = false;
    }
    if (p_ptr->py != previous_y || p_ptr->px != previous_x) {
        tutorial_action_finished("move", "", true);
        previous_y = p_ptr->py; previous_x = p_ptr->px;
    }
    if (p_ptr->stealth_mode) tutorial_action_finished("stealth", "", true);
    if (player_active_weapon_mode() != previous_mode) {
        tutorial_action_finished("change-active", "", true);
        previous_mode = player_active_weapon_mode();
    }
    for (int i = 0; i < (int)N_ELEMENTS(conditions); ++i) {
        const condition_lesson *condition = &conditions[i];
        int value = *(const s16b *)((const char *)p_ptr + condition->offset);
        tutorial_status status = tutorial_lesson_status(condition->id);
        if (value && (!previous_conditions[i] || status == TUTORIAL_UNSEEN
            || status == TUTORIAL_IN_PROGRESS)) {
            if (!strcmp(condition->id, "status.poisoned") || !strcmp(condition->id, "status.cut"))
                strnfmt(detail, sizeof(detail), "Severity %d; at this value the next damage tick is %d Health. %s", value, (value + 4) / 5, condition->effect);
            else if (!strcmp(condition->id, "status.diseased"))
                strnfmt(detail, sizeof(detail), "Disease penalties: Strength %+d, Dexterity %+d, Constitution %+d, Grace %+d. %s",
                    p_ptr->stat_disease[A_STR], p_ptr->stat_disease[A_DEX],
                    p_ptr->stat_disease[A_CON], p_ptr->stat_disease[A_GRA], condition->effect);
            else if (!strcmp(condition->id, "status.stun"))
                strnfmt(detail, sizeof(detail), "Stun %d: %+d to every skill. %s", value, value >= 50 ? -4 : -2, condition->effect);
            else strnfmt(detail, sizeof(detail), "%s %d. %s", condition->name, value, condition->effect);
            observe(condition->id, condition->id + 7, condition->name, detail);
        }
        if (value) offer_remedy(condition->id + 7, condition->name);
        else {
            strnfmt(id, sizeof(id), "%s.remedy", condition->id);
            tutorial_forget_observation(id);
        }
        previous_conditions[i] = value;
    }
    bool drained = false;
    for (int stat = 0; stat < A_MAX; ++stat) {
        drained = drained || p_ptr->stat_drain[stat] < 0;
        strnfmt(id, sizeof(id), "status.drain%d", stat);
        tutorial_status status = tutorial_lesson_status(id);
        if (p_ptr->stat_drain[stat] < previous_drain[stat]
            || (p_ptr->stat_drain[stat] < 0 && (status == TUTORIAL_UNSEEN
                || status == TUTORIAL_IN_PROGRESS))) {
            static const char *names[] = { "Strength", "Dexterity", "Constitution", "Grace" };
            strnfmt(id, sizeof(id), "status.drain%d", stat);
            strnfmt(detail, sizeof(detail), "%s drain changed from %d to %d. Check the attribute breakdown; drain differs from equipment penalties or an expiring buff.", names[stat], previous_drain[stat], p_ptr->stat_drain[stat]);
            observe(id, "drain", names[stat], detail);
        }
        previous_drain[stat] = p_ptr->stat_drain[stat];
    }
    if (drained) offer_remedy("drain", "Drained attributes");
    else tutorial_forget_observation("status.drain.remedy");
    if (p_ptr->chp < previous_hp) {
        strnfmt(detail, sizeof(detail), "Health: %d/%d. Read the last attack and its effects before acting again.", p_ptr->chp, p_ptr->mhp);
        observe("combat.first_damage", "health", "Health lost", detail);
    }
    if (p_ptr->chp <= p_ptr->mhp * op_ptr->hitpoint_warn / 10) {
        observe("status.health", "health", "Low Health", "Your Health is at or below your warning threshold. Inspect known healing options and the escape route.");
        offer_remedy("health", "Low Health");
    } else tutorial_forget_observation("status.health.remedy");
    previous_hp = p_ptr->chp;
    if (p_ptr->csp < previous_voice && p_ptr->csp <= p_ptr->msp / 4)
        observe("status.voice", "voice", "Low Voice", "Voice fuels songs and horns. It does not regenerate while singing; stop singing to recover it.");
    if (p_ptr->msp > 0 && p_ptr->csp <= p_ptr->msp / 4)
        offer_remedy("voice", "Low Voice");
    else tutorial_forget_observation("status.voice.remedy");
    previous_voice = p_ptr->csp;
    if (p_ptr->food < PY_FOOD_ALERT) {
        observe(p_ptr->food < PY_FOOD_STARVE ? "status.starving"
            : p_ptr->food < PY_FOOD_WEAK ? "status.weak" : "status.hungry",
            "hunger", "Hunger", "Food is consumed as the game advances. Weakness reduces Strength; starvation damages you and prevents Health regeneration.");
        offer_remedy("hunger", "Hunger");
    } else tutorial_forget_observation("status.hunger.remedy");
    if (min_depth() != previous_min_depth) {
        strnfmt(detail, sizeof(detail), "Minimum depth is now %d ft. Game actions advance this pressure; time spent reading does not.", min_depth() * 50);
        observe("world.minimum_depth", "depth", "Minimum depth", detail);
        previous_min_depth = min_depth();
    }
    int adjacent = 0;
    bool legal_hostile = false, legal_adjacent = false;
    for (int i = 1; i < mon_max; ++i) {
        monster_type *monster = &mon_list[i];
        if (!tutorial_monster_observable(monster)) continue;
        char name[160];
        monster_desc(name, sizeof(name), monster, 0);
        if (target_can_be_attacked(monster)) legal_hostile = true;
        if (tutorial_first_monster_target(monster))
            observe("combat.first_monster", "monster", name,
                "Awareness and morale are different. Stealth helps avoid notice but slows movement; it does not guarantee that an alert enemy loses you.");
        if (abs(monster->fy - p_ptr->py) <= 1 && abs(monster->fx - p_ptr->px) <= 1) {
            ++adjacent;
            if (target_can_be_attacked(monster) && player_active_weapon_is_melee()
                && !p_ptr->entranced && p_ptr->stun <= 100) {
                legal_adjacent = true;
                observe("combat.first_adjacent", "monster", name, "Moving toward an adjacent hostile attacks it. Attack once, or skip this lesson to choose another tactic.");
            }
        }
        if (monster->stance == STANCE_FLEEING)
            observe("combat.fleeing", "monster", name, "This creature is fleeing. Morale differs from awareness; check your oath before pursuing or attacking.");
    }
    if (!legal_adjacent) tutorial_forget_observation("combat.first_adjacent");
    if (!legal_hostile || p_ptr->rage) tutorial_forget_observation("combat.first_monster");
    if (adjacent >= 2) observe("combat.surrounded", "monster", "Surrounded",
        "Adjacent foes improve each other's accuracy. Doors and narrow passages can reduce the number attacking together.");
    /* Reached means on this square or visibly adjacent, not distant discovery. */
    for (int y = MAX(0, p_ptr->py - 1); y <= MIN(p_ptr->cur_map_hgt - 1, p_ptr->py + 1); ++y)
        for (int x = MAX(0, p_ptr->px - 1); x <= MIN(p_ptr->cur_map_wid - 1, p_ptr->px + 1); ++x) {
            if (!(cave_info[y][x] & CAVE_SEEN) && (y != p_ptr->py || x != p_ptr->px)) continue;
            for (int object = cave_o_idx[y][x]; object; object = o_list[object].next_o_idx)
                if (o_list[object].marked) tutorial_game_item(&o_list[object]);
            int feat = cave_feat[y][x];
            if ((cave_info[y][x] & CAVE_MARK) && feat != FEAT_FLOOR
                && feat != FEAT_SECRET && feat != FEAT_WALL_EXTRA
                && feat != FEAT_WALL_INNER && feat != FEAT_WALL_OUTER && feat != FEAT_WALL_SOLID) {
                strnfmt(id, sizeof(id), "terrain.%d", feat);
                observe(id, "terrain", "Nearby terrain", "Inspect this known feature before stepping onto it or choosing an interaction.");
                if (cave_forge_bold(y, x))
                    observe("world.forge", "terrain", "A forge", "Inspect the forge and its remaining uses. Open Smithing to compare requirements before committing resources.");
                if (cave_trap_bold(y, x))
                    observe("world.trap", "terrain", "A revealed trap", "Inspect the trap before moving. Choose a route around it or check the applicable interaction and its risks.");
            }
        }
    memset(offered_item_actions, 0, sizeof(offered_item_actions));
    if (!p_ptr->entranced && p_ptr->stun <= 100 && !p_ptr->confused && !p_ptr->image) {
        for (int i = 0; i < INVEN_TOTAL; ++i) {
            if (inventory[i].k_idx && reached_kinds[(u16b)inventory[i].k_idx]) tutorial_game_item(&inventory[i]);
            offer_item_actions(&inventory[i], legal_hostile);
        }
        for (int i = 0; i < player_carried_extra_entry_count(); ++i) {
            object_type *item = player_carried_extra_entry_at(i);
            if (item && item->k_idx && reached_kinds[(u16b)item->k_idx]) tutorial_game_item(item);
            offer_item_actions(item, legal_hostile);
        }
        for (int i = 0; i < supplies_entry_count(); ++i) {
            object_type *item = supplies_entry_at(i);
            if (item && item->k_idx && reached_kinds[(u16b)item->k_idx]) tutorial_game_item(item);
        }
    }
    if (player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW
        && tutorial_lesson_status("item.arrows") == TUTORIAL_COMPLETED) {
        int arrows[2];
        if (player_quiver_arrow_slots(arrows, 2) >= 2)
            observe_item_action("item.arrows.active", "arrows", "Active arrows",
                "Choose a different arrow stack while keeping your current bow and shield. Changing only arrows is free.");
    }
    /* Context can expire while another lesson is being read, or during normal
     * Pack access. Keep paid transactions alive until their owning callback. */
    if (!player_pack_action_pending())
        for (int i = 0; i < (int)N_ELEMENTS(item_action_ids); ++i)
            if (!offered_item_actions[i]) tutorial_forget_observation(item_action_ids[i]);
    for (int skill = 0; skill < S_MAX; ++skill)
        for (int ability = 0; ability < ABILITIES_MAX; ++ability) {
            if (p_ptr->have_ability[skill][ability] && !previous_abilities[skill][ability])
                tutorial_game_ability(skill, ability, false);
            previous_abilities[skill][ability] = p_ptr->have_ability[skill][ability];
        }
    tutorial_world_checkpoint();
    observe_extra_states(false);
    tutorial_checkpoint(!(player_pack_action_pending() && tutorial_action_waiting()));
    if (tutorial_is_active()) {
        /* Freeze only automation. Do not cancel an in-flight Pack transaction
         * or a mandatory recovery action merely because a card is displayed. */
        p_ptr->running = 0; p_ptr->resting = 0; p_ptr->command_rep = 0;
        sdl_mouse_path_cancel();
    }
    tutorial_game_wait();
}

#define TUTORIAL_ARCHIVE_PAGE_SIZE 10

/* Stable semantic ID families keep new catalogue cards in their topic without
 * changing saved lesson IDs. Unknown families remain readable under Other. */
static const struct {
    const char *label;
    const char *prefixes[4];
} tutorial_archive_topics[] = {
    {"Getting started & menus", {"opening.", "menu.", NULL}},
    {"Combat", {"combat.", NULL}},
    {"Items & equipment", {"item.", "identification.", NULL}},
    {"Carrying & storage", {"storage.", NULL}},
    {"Skills & abilities", {"ability.", "advancement.", NULL}},
    {"Exploration", {"world.", NULL}},
    {"Terrain & traps", {"terrain.", NULL}},
    {"Monsters", {"monster.", NULL}},
    {"Conditions", {"status.", NULL}},
    {"Item effects", {"effect.", NULL}},
    {"Quests & Tales", {"quest.", "tale.", NULL}},
    {"Other", {NULL}}
};

typedef struct tutorial_archive_card {
    char id[80];
    char title[160];
    tutorial_status status;
    int topic;
} tutorial_archive_card;

static int tutorial_archive_topic_for_id(const char *id)
{
    for (int i = 0; i < (int)N_ELEMENTS(tutorial_archive_topics); ++i)
        for (int j = 0; tutorial_archive_topics[i].prefixes[j]; ++j) {
            const char *prefix = tutorial_archive_topics[i].prefixes[j];
            if (!strncmp(id, prefix, strlen(prefix))) return i;
        }
    return (int)N_ELEMENTS(tutorial_archive_topics) - 1;
}

static int tutorial_archive_compare(const void *left, const void *right)
{
    const tutorial_archive_card *a = left, *b = right;
    int order = SDL_strcasecmp(a->title, b->title);
    return order ? order : strcmp(a->id, b->id);
}

void tutorial_game_archive(void)
{
    int offset = 0, topic = -1, card_count = 0, topic_selection = 0;
    int card_selection = 0;
    int topic_counts[N_ELEMENTS(tutorial_archive_topics)] = {0};
    bool old_managing = managing;
    managing = true;
    tutorial_archive_begin();
    int capacity = tutorial_archive_count();
    tutorial_archive_card *cards = capacity > 0
        ? calloc((size_t)capacity, sizeof(*cards)) : NULL;
    if (capacity > 0 && !cards) {
        msg_print("Unable to open tutorial cards.");
        goto cleanup;
    }
    for (int i = 0; i < capacity; ++i) {
        tutorial_view view;
        tutorial_status status;
        if (!tutorial_archive_entry(i, &view, &status)
            || status == TUTORIAL_UNSEEN) continue;
        tutorial_archive_card *card = &cards[card_count++];
        SDL_strlcpy(card->id, view.id, sizeof(card->id));
        SDL_strlcpy(card->title, view.title, sizeof(card->title));
        card->status = status;
        card->topic = tutorial_archive_topic_for_id(view.id);
        ++topic_counts[card->topic];
    }
    if (card_count > 1)
        qsort(cards, (size_t)card_count, sizeof(*cards), tutorial_archive_compare);
    while (true) {
        if (topic < 0) {
            ui_question_option options[N_ELEMENTS(tutorial_archive_topics) + 1];
            char labels[N_ELEMENTS(tutorial_archive_topics)][96];
            int topics[N_ELEMENTS(tutorial_archive_topics)];
            int count = 0;
            char description[192];
            for (int i = 0; i < (int)N_ELEMENTS(tutorial_archive_topics); ++i) {
                if (!topic_counts[i]) continue;
                strnfmt(labels[count], sizeof(labels[count]), "%s (%d)",
                    tutorial_archive_topics[i].label, topic_counts[i]);
                topics[count] = i;
                options[count] = (ui_question_option){(char)('a' + count),
                    labels[count], TERM_WHITE, false};
                ++count;
            }
            strnfmt(description, sizeof(description), card_count
                ? "%d revealed tutorial cards in this Tale. Choose a topic to read them again. Reading is free."
                : "No tutorial cards have been revealed in this Tale yet. Cards appear here when you encounter their lessons.",
                card_count);
            options[count] = (ui_question_option){'x', "Back", TERM_WHITE, false};
            int choice = ui_question_ask_overlay("Tutorial cards", description,
                options, count + 1, -1, -1, topic_selection);
            if (choice < 0 || choice >= count) break;
            topic_selection = choice;
            topic = topics[choice];
            offset = 0;
            card_selection = 0;
        }
        ui_question_option options[TUTORIAL_ARCHIVE_PAGE_SIZE + 3];
        char labels[TUTORIAL_ARCHIVE_PAGE_SIZE][200];
        int indices[TUTORIAL_ARCHIVE_PAGE_SIZE];
        char title[128], description[160];
        int count = 0, seen = 0;
        for (int i = 0; i < card_count; ++i) {
            const tutorial_archive_card *card = &cards[i];
            if (card->topic != topic) continue;
            if (seen++ < offset) continue;
            if (count >= TUTORIAL_ARCHIVE_PAGE_SIZE) break;
            strnfmt(labels[count], sizeof(labels[count]), "%s%s", card->title,
                card->status == TUTORIAL_SKIPPED ? " (skipped)"
                : card->status == TUTORIAL_IN_PROGRESS ? " (in progress)" : "");
            indices[count] = i;
            options[count] = (ui_question_option){(char)('a' + count), labels[count], TERM_WHITE, false};
            ++count;
        }
        options[count] = (ui_question_option){'n', "Next page", TERM_L_BLUE,
            offset + count >= topic_counts[topic]};
        options[count + 1] = (ui_question_option){'p', "Previous page", TERM_L_BLUE, offset == 0};
        options[count + 2] = (ui_question_option){'x', "Back to topics", TERM_WHITE, false};
        strnfmt(title, sizeof(title), "Tutorial cards: %s", tutorial_archive_topics[topic].label);
        strnfmt(description, sizeof(description),
            "Page %d of %d. Select a card to read it again. Escape returns to topics.",
            offset / TUTORIAL_ARCHIVE_PAGE_SIZE + 1,
            (topic_counts[topic] + TUTORIAL_ARCHIVE_PAGE_SIZE - 1) / TUTORIAL_ARCHIVE_PAGE_SIZE);
        int choice = ui_question_ask_overlay(title, description, options,
            count + 3, -1, -1, card_selection);
        if (choice < 0 || choice >= count + 2) { topic = -1; continue; }
        if (choice == count) {
            if (offset + count < topic_counts[topic]) offset += TUTORIAL_ARCHIVE_PAGE_SIZE;
            card_selection = 0;
            continue;
        }
        if (choice == count + 1) {
            offset = MAX(0, offset - TUTORIAL_ARCHIVE_PAGE_SIZE);
            card_selection = 0;
            continue;
        }
        card_selection = choice;
        if (tutorial_replay(cards[indices[choice]].id)) {
            tutorial_checkpoint(true);
            tutorial_view view;
            waiting = true;
            while (tutorial_get_view(&view)) {
                Term_fresh();
                Term_xtra(TERM_XTRA_EVENT, 1);
            }
            waiting = false;
        }
    }
cleanup:
    free(cards);
    managing = old_managing;
    tutorial_archive_end();
    if (managing) tutorial_checkpoint(false);
    else tutorial_game_wait();
}

void tutorial_game_lifecycle(const char *id)
{
    tutorial_context context = {0};
    tutorial_view view;
    if (!started || !p_ptr || p_ptr->tutorial_deferred
        || run_mode_is_blitz() || !tutorial_enabled()) return;
    tutorial_invalidate_context();
    SDL_strlcpy(context.subject, p_ptr->escaped ? "Escape" : "Death", sizeof(context.subject));
    SDL_strlcpy(context.text, p_ptr->escaped
        ? "This hero's escape has ended the expedition. Review the recovered Silmarils and the Tale's remaining objective."
        : "This hero's run has ended. Review the cause of death and combat history before choosing the next hero in this Tale.", sizeof(context.text));
    tutorial_observe(id, &context);
    tutorial_checkpoint(true);
    waiting = true;
    while (tutorial_get_view(&view)) {
        if (view.kind == TUTORIAL_STEP_ACTION) { tutorial_skip(); break; }
        Term_fresh();
        Term_xtra(TERM_XTRA_EVENT, 1);
    }
    waiting = false;
    tutorial_invalidate_context();
}

void tutorial_game_settings(void)
{
    bool old_managing = managing;
    managing = true;
    tutorial_checkpoint(false);
    while (true) {
        char mode_label[96];
        strnfmt(mode_label, sizeof(mode_label), "Gameplay tutorials: %s",
            tutorial_mode_name(get_sdl_gameplay_tutorial_mode()));
        ui_question_option options[] = {
            {'e', mode_label, TERM_L_BLUE, false},
            {'r', "Reset tutorials for this Tale", TERM_ORANGE, run_mode_is_blitz()},
            {'l', "Tutorial cards", TERM_WHITE, false},
            {'b', "Back", TERM_WHITE, false}
        };
        int choice = ui_question_ask_overlay("Gameplay tutorials",
            p_ptr && p_ptr->tutorial_deferred
            ? "Tutorials are deferred for this older character. Mode changes and reset apply to your next new character. Cycle Disabled, Normal and Extended; Extended includes all lessons. Revealed tutorial cards remain available to read."
            : "Cycle Disabled, Normal and Extended. Normal teaches core controls and survival; Extended adds detailed mechanics. Lessons are remembered across heroes in this Tale. Reading is free; guided actions keep their normal costs.", options, N_ELEMENTS(options), -1, -1, 0);
        if (choice < 0 || choice == 3) break;
        if (choice == 0) cycle_sdl_gameplay_tutorial_mode();
        else if (choice == 1) {
            ui_question_option confirm[] = {{'r', "Reset this Tale's lesson history", TERM_ORANGE, false}, {'c', "Cancel", TERM_WHITE, false}};
            if (ui_question_ask_overlay("Reset tutorials", "Only tutorial history for the current Tale will be cleared. The game and device controls do not reset.", confirm, 2, -1, -1, 1) == 0) {
                tutorial_reset_tale();
                opening_pending = true;
            }
        } else if (choice == 2) tutorial_game_archive();
    }
    managing = old_managing;
    if (!managing) {
        tutorial_checkpoint(true);
        tutorial_game_wait();
    }
}
