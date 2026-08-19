#include "angband.h"
#include "externs.h"
#include "cmd/world/cmd-interact-chest.h"
#include "log/log.h"
#include "metarun.h"
#include "object/object-ui-select.h"
#include "sdl-config.h"
#include "ui/question.h"

static void prise_silmaril(void);

/*
 * Helper function to determine the equip sound based on item type
 */
static int get_equip_sound(const object_type* o_ptr)
{
    /* Fuel-burning light sources */
    if (o_ptr->tval == TV_LIGHT)
    {
        if (((o_ptr->sval == SV_LIGHT_TORCH)
                || (o_ptr->sval == SV_LIGHT_MALLORN))
            && player_light_has_fuel(o_ptr))
        {
            return MSG_TORCH_LIGHT;
        }

        if ((o_ptr->sval == SV_LIGHT_LANTERN)
            && (object_ego_prefix(o_ptr) != EGO_BROKEN_BRASS_LANTERN)
            && player_light_has_fuel(o_ptr))
        {
            return MSG_TORCH_LIGHT;
        }
    }

    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_EQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_EQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_EQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_EQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_EQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_EQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_EQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * Helper function to determine the unequip sound based on item type
 */
static int get_unequip_sound(const object_type* o_ptr)
{
    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_UNEQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_UNEQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_UNEQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_UNEQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_UNEQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_UNEQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_UNEQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * The "wearable" tester
 */
static bool item_tester_hook_wear(const object_type* o_ptr)
{
    /* Staves and horns are used directly from the Harness, never equipped. */
    if (o_ptr && (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN))
        return (false);

    /* Literal (broken) items cannot be equipped until repaired. */
    if (object_has_broken_prefix(o_ptr))
        return (false);

    // Despite being a crown, the Iron Crown cannot be worn
    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
        return (false);

    /* Check for a usable slot */
    if (wield_slot(o_ptr) >= INVEN_WIELD)
        return (true);

    /* Assume not wearable */
    return (false);
}

/* Give the player an explicit choice before knowingly binding a cursed item
 * to their equipment.  Pack actions call do_cmd_wield() again when their
 * delayed action completes, so do not repeat the prompt for that completion
 * call after the player has already accepted it. */
static bool cursed_item_known_to_player(const object_type* o_ptr)
{
    if (!o_ptr || !cursed_p(o_ptr))
        return false;

    if (object_known_p(o_ptr))
        return true;

    /* Sanctity and other sensing paths can reveal the curse without fully
     * identifying the item.  These are the same visible feelings rendered as
     * "cursed" by object_desc(). */
    return o_ptr->discount == INSCRIP_CURSED
        || o_ptr->discount == INSCRIP_TERRIBLE
        || o_ptr->discount == INSCRIP_WORTHLESS;
}

static bool confirm_known_cursed_wield(const object_type* o_ptr, int item)
{
    ui_question_option options[2] = {
        { 'e', "Equip anyway", TERM_ORANGE, false },
        { 'c', "Cancel", TERM_SLATE, false }
    };
    char o_name[120];
    char desc[260];

    if (!o_ptr || !o_ptr->k_idx || !cursed_item_known_to_player(o_ptr)
        || player_pack_action_completing(PLAYER_PACK_ACTION_WIELD))
    {
        return true;
    }

    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, false, 0);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    strnfmt(desc, sizeof(desc),
        "You know %s is cursed. Once equipped, it may be impossible to "
        "remove.", o_name);

    return ui_question_ask("Known cursed item", desc, options, 2,
        UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 1) == 0;
}

static bool item_tester_hook_ring_slots(const object_type* o_ptr)
{
    return (o_ptr == &inventory[INVEN_LEFT]) || (o_ptr == &inventory[INVEN_RIGHT]);
}

bool throw_slot_menu_active = false;
bool throw_slot_enabled[INVEN_TOTAL];
static int forced_wield_slot = -1;
static bool forced_wield_full_stack = false;
static bool wield_command_succeeded = false;

static bool forced_wield_slot_accepts_object(const object_type* o_ptr,
    int forced_slot)
{
    int natural_slot;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (forced_slot < INVEN_WIELD || forced_slot >= INVEN_TOTAL)
        return false;

    natural_slot = wield_slot(o_ptr);
    if (natural_slot == forced_slot)
        return true;

    switch (forced_slot)
    {
    case INVEN_WIELD:
        return player_can_treat_as_throwing(o_ptr);
    case INVEN_LEFT:
    case INVEN_RIGHT:
        return o_ptr->tval == TV_RING;
    case INVEN_NECK:
        return o_ptr->tval == TV_AMULET;
    case INVEN_QUIVER1:
        return o_ptr->tval == TV_ARROW;
    case INVEN_BELT:
        return object_is_belt_weapon(o_ptr);
    default:
        return false;
    }
}

static int first_floor_item_under_player(void)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    floor_num = scan_floor(
        floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        const object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx)
            continue;

        if (object_is_searched_skeleton(o_ptr))
            continue;

        return 0 - o_idx;
    }

    return 0;
}

/*
 * Return the first floor object under the player whose primary action is an
 * interaction rather than pickup.  Space normally expands to the "/5"
 * interact-here keymap, but touch shortcuts resolve Space contextually before
 * request_command() sees it.  Keep skeletons and unopened chests on the
 * interaction path instead of rewriting that shortcut to 'g'.
 */
static int first_floor_interaction_under_player(void)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    floor_num = scan_floor(
        floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        const object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx)
            continue;
        if (o_ptr->tval == TV_SKELETON
            && !object_is_searched_skeleton(o_ptr))
        {
            return 0 - o_idx;
        }
        if (o_ptr->tval == TV_CHEST && o_ptr->pval != 0)
            return 0 - o_idx;
    }

    return 0;
}

cptr item_use_action_name(const object_type* o_ptr, int item)
{
    if (!o_ptr)
        return "Use";

    if (o_ptr->name1 >= ART_MORGOTH_1 && o_ptr->name1 <= ART_MORGOTH_3)
        return "Prise";

    if (o_ptr->tval == TV_SKELETON)
        return "Search";

    if (o_ptr->tval == TV_CHEST)
    {
        if (chest_trap_minigame && o_ptr->pval != 0)
            return "Handle";
        if (o_ptr->pval > 0 && object_chest_trap_flags(o_ptr)
            && object_known_p(o_ptr))
        {
            return "Disarm";
        }
        return "Open";
    }

    if (item < INVEN_WIELD
        && inventory[INVEN_LITE].tval == TV_LIGHT
        && inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN
        && (o_ptr->tval == TV_FLASK
            || (o_ptr->tval == TV_LIGHT
                && o_ptr->sval == SV_LIGHT_LANTERN)))
    {
        return "Refuel";
    }

    switch (o_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    case TV_ARROW:
        return (item >= INVEN_WIELD && item < INVEN_TOTAL) ? "Take Off"
                                                           : "Wield";
    case TV_FLASK:
        return "Use";
    case TV_NOTE:
        return "Read";
    case TV_STAFF:
        return (item < 0) ? "Pick up" : "Activate";
    case TV_HORN:
        return (item < 0) ? "Pick up" : "Play";
    case TV_POTION:
        return "Quaff";
    case TV_FOOD:
        return "Eat";
    default:
        return "Use";
    }
}

static bool floor_context_add_action(floor_context_action actions[],
    int capacity, int* count, floor_context_action_kind kind, int key,
    cptr label, byte attr)
{
    floor_context_action* action;

    if (!actions || !count || *count < 0 || *count >= capacity
        || !label || !label[0])
    {
        return false;
    }

    action = &actions[(*count)++];
    action->kind = kind;
    action->key = key;
    action->attr = attr;
    SDL_strlcpy(action->label, label, sizeof(action->label));
    if (key == ESCAPE)
        strnfmt(action->token, sizeof(action->token), "Esc %s", label);
    else if (key == ' ')
        strnfmt(action->token, sizeof(action->token), "Space %s", label);
    else if (key > 0 && key < 128 && SDL_isprint(key))
        strnfmt(action->token, sizeof(action->token), "%c %s", key, label);
    else
        SDL_strlcpy(action->token, label, sizeof(action->token));
    return true;
}

static cptr floor_context_use_label(const object_type* o_ptr, int floor_item,
    char* label, size_t label_len)
{
    cptr action_name;

    if (!o_ptr || !o_ptr->k_idx || !label || label_len == 0)
        return NULL;

    action_name = item_use_action_name(o_ptr, floor_item);
    if (streq(action_name, "Pick up") || streq(action_name, "Pick Up"))
        return NULL;

    if (streq(action_name, "Wield"))
    {
        if (o_ptr->tval == TV_ARROW)
            return NULL;
        if (player_can_treat_as_throwing(o_ptr))
        {
            SDL_strlcpy(label,
                object_is_belt_weapon(o_ptr) ? "Equip..." : "Wield",
                label_len);
        }
        else if (o_ptr->tval == TV_DIGGING || o_ptr->tval == TV_HAFTED
            || o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_SWORD)
        {
            SDL_strlcpy(label, "Wield", label_len);
        }
        else
        {
            SDL_strlcpy(label,
                o_ptr->tval == TV_RING && inventory[INVEN_LEFT].k_idx
                    && inventory[INVEN_RIGHT].k_idx
                    ? "Equip..." : "Equip",
                label_len);
        }
        return label;
    }

    SDL_strlcpy(label, action_name, label_len);
    return label;
}

static int floor_context_first_item_and_count(int* first_item)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    int count = 0;

    if (first_item)
        *first_item = 0;
    if (!p_ptr)
        return 0;

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK,
        p_ptr->py, p_ptr->px, 0x00);
    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        const object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;
        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx || object_is_searched_skeleton(o_ptr))
            continue;
        if (count == 0 && first_item)
            *first_item = 0 - o_idx;
        count++;
    }
    return count;
}

int floor_context_collect_item_actions(int floor_item, bool include_details,
    bool include_close, floor_context_action actions[], int capacity)
{
    object_type* o_ptr;
    bool interaction_only;
    char use_label[32];
    int count = 0;

    if (!p_ptr || !actions || capacity <= 0 || floor_item >= 0)
        return 0;
    if (0 - floor_item <= 0 || 0 - floor_item >= o_max)
        return 0;

    o_ptr = &o_list[0 - floor_item];
    if (!o_ptr->k_idx || o_ptr->iy != p_ptr->py || o_ptr->ix != p_ptr->px)
        return 0;

    interaction_only = o_ptr->tval == TV_SKELETON
        || o_ptr->tval == TV_CHEST;

    if (floor_context_use_label(o_ptr, floor_item, use_label,
            sizeof(use_label)))
    {
        floor_context_add_action(actions, capacity, &count,
            FLOOR_CONTEXT_ACTION_USE, 'u', use_label, TERM_L_WHITE);
    }

    if (!interaction_only)
    {
        if (o_ptr->tval == TV_ARROW)
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_QUIVER, 'v', "Quiver", TERM_L_BLUE);
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_PACK, 'g', "Pack", TERM_L_BLUE);
        }
        else if (supplies_is_supply_object(o_ptr))
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_SUPPLIES, 'j', "Supplies", TERM_L_BLUE);
        }
        else if (object_can_choose_pack_or_harness(o_ptr))
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_HARNESS, 'h', "Harness", TERM_L_BLUE);
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_PACK, 'g', "Pack", TERM_L_BLUE);
        }
        else if (o_ptr->storage == OBJECT_STORAGE_JEWELRY)
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_JEWELRY, 'j', "Jewelry", TERM_L_BLUE);
        }
        else if (o_ptr->storage == OBJECT_STORAGE_PACK)
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_PACK, 'g', "Pack", TERM_L_BLUE);
        }
        else if (o_ptr->storage == OBJECT_STORAGE_HARNESS)
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_HARNESS, 'h', "Harness", TERM_L_BLUE);
        }
        else
        {
            floor_context_add_action(actions, capacity, &count,
                FLOOR_CONTEXT_ACTION_PICKUP, ' ', "Pick Up", TERM_L_WHITE);
        }
    }

    if (include_details && !interaction_only)
    {
        floor_context_add_action(actions, capacity, &count,
            FLOOR_CONTEXT_ACTION_DETAILS, 'x', "Details", TERM_L_BLUE);
    }
    if (include_close)
    {
        floor_context_add_action(actions, capacity, &count,
            FLOOR_CONTEXT_ACTION_CLOSE, ESCAPE, "Close", TERM_SLATE);
    }

    return count;
}

int floor_context_collect_square_actions(bool include_details,
    floor_context_action actions[], int capacity)
{
    int first_item = 0;
    int item_count = floor_context_first_item_and_count(&first_item);
    int count = 0;

    if (!actions || capacity <= 0 || item_count <= 0)
        return 0;
    if (item_count == 1)
    {
        return floor_context_collect_item_actions(first_item, include_details,
            false, actions, capacity);
    }

    {
        char label[32];

        strnfmt(label, sizeof(label), "Items (%d)...", item_count);
        floor_context_add_action(actions, capacity, &count,
            FLOOR_CONTEXT_ACTION_ITEMS, 'i', label, TERM_L_WHITE);
    }
    return count;
}

bool floor_context_action_for_key(int floor_item, int key,
    floor_context_action_kind* kind)
{
    floor_context_action actions[FLOOR_CONTEXT_MAX_ACTIONS];
    int count = floor_context_collect_item_actions(floor_item, false, true,
        actions, (int)N_ELEMENTS(actions));

    if (kind)
        *kind = FLOOR_CONTEXT_ACTION_NONE;
    for (int i = 0; i < count; i++)
    {
        if (actions[i].key != key)
            continue;
        if (kind)
            *kind = actions[i].kind;
        return true;
    }
    return false;
}

static bool floor_context_preferred_pickup_action(int floor_item,
    floor_context_action* selected)
{
    floor_context_action actions[FLOOR_CONTEXT_MAX_ACTIONS];
    int count = floor_context_collect_item_actions(floor_item, false, false,
        actions, (int)N_ELEMENTS(actions));
    static const floor_context_action_kind preference[] = {
        FLOOR_CONTEXT_ACTION_HARNESS,
        FLOOR_CONTEXT_ACTION_PACK,
        FLOOR_CONTEXT_ACTION_SUPPLIES,
        FLOOR_CONTEXT_ACTION_JEWELRY,
        FLOOR_CONTEXT_ACTION_PICKUP
    };

    for (int p = 0; p < (int)N_ELEMENTS(preference); p++)
    {
        for (int i = 0; i < count; i++)
        {
            if (actions[i].kind != preference[p])
                continue;
            if (selected)
                *selected = actions[i];
            return true;
        }
    }
    return false;
}

static cptr floor_context_space_pickup_label(int floor_item)
{
    floor_context_action selected;

    if (floor_context_preferred_pickup_action(floor_item, &selected))
    {
        switch (selected.kind)
        {
        case FLOOR_CONTEXT_ACTION_HARNESS:
            return "Harness";
        case FLOOR_CONTEXT_ACTION_PACK:
            return "Pack";
        case FLOOR_CONTEXT_ACTION_SUPPLIES:
            return "Supplies";
        case FLOOR_CONTEXT_ACTION_JEWELRY:
            return "Jewelry";
        case FLOOR_CONTEXT_ACTION_PICKUP:
            return "Pick Up";
        default:
            break;
        }
    }
    return "Pick Up";
}

static bool floor_context_primary_popup_action(int floor_item,
    floor_context_action* selected)
{
    floor_context_action actions[FLOOR_CONTEXT_MAX_ACTIONS];
    int count = floor_context_collect_item_actions(floor_item, false, false,
        actions, (int)N_ELEMENTS(actions));

    if (count <= 0)
        return false;
    if (selected)
        *selected = actions[0];
    return true;
}

static int choose_arrow_move_quantity(const object_type* o_ptr,
    cptr destination, int maximum, bool partial_fit)
{
    char o_name[80];
    char prompt[192];

    if (!o_ptr || !o_ptr->k_idx || o_ptr->number <= 0 || maximum <= 0)
        return 0;

    maximum = MIN(maximum, o_ptr->number);
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
    if (partial_fit)
    {
        strnfmt(prompt, sizeof(prompt),
            "Your %s can only hold %d of %s. Move how many? (0-%d): ",
            destination, maximum, o_name, maximum);
        return get_quantity_touch_category_force_prompt_action(prompt, "Move",
            maximum, SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
    }

    strnfmt(prompt, sizeof(prompt),
        "How many do you want to move from %s to your %s? ", o_name,
        destination);
    return get_quantity_action(prompt, "Move", maximum);
}

static bool choose_quiver_arrow_replacement(const object_type* incoming,
    int needed, int* replacement_item)
{
    object_choice_entry entries[QUIVER_ARROW_CAPACITY + 1];
    int handles[QUIVER_ARROW_CAPACITY + 1];
    int count;
    int selected = -1;
    char desc[320];

    if (replacement_item)
        *replacement_item = -1;
    if (!incoming || !incoming->k_idx || needed <= 0 || !replacement_item)
        return false;

    count = player_quiver_arrow_slots(handles, (int)N_ELEMENTS(handles));
    for (int i = 0; i < count; i++)
    {
        object_type* candidate = player_quiver_arrow_object(handles[i]);
        char label[OBJECT_CHOICE_LABEL_LEN];

        if (!candidate || !candidate->k_idx)
            continue;
        strnfmt(label, sizeof(label), "%d)", i + 1);
        object_choice_entry_make(&entries[i], handles[i], candidate, label,
            handles[i] == player_quiver_selected_arrow_slot()
                ? "Active" : "Quiver");
    }
    if (count <= 0)
        return false;

    strnfmt(desc, sizeof(desc),
        "Your quiver has %d/%d arrows. Moving the selected arrows requires "
        "replacing %d quivered arrow%s.",
        player_quiver_arrow_count(), QUIVER_ARROW_CAPACITY, needed,
        needed == 1 ? "" : "s");
    if (!object_choice_overlay("What to replace?", desc, entries, count, 0,
            &selected)
        || selected < 0 || selected >= count)
    {
        return false;
    }

    *replacement_item = entries[selected].item;
    return true;
}

static int drop_quiver_replacement_arrows(int item, int needed)
{
    object_type* o_ptr = player_quiver_arrow_object(item);
    object_type dropped;
    char o_name[120];
    int amount;

    if (!o_ptr || !o_ptr->k_idx || needed <= 0)
        return 0;

    amount = MIN(needed, o_ptr->number);
    object_copy(&dropped, o_ptr);
    dropped.number = amount;
    dropped.pickup = false;
    dropped.pickup_slot = -1;
    dropped.storage = OBJECT_STORAGE_NONE;
    dropped.next_o_idx = 0;
    dropped.held_m_idx = 0;
    dropped.iy = dropped.ix = 0;
    object_desc(o_name, sizeof(o_name), &dropped, true, 3);

    player_quiver_remove_arrows(item, amount);
    drop_near(&dropped, 0, p_ptr->py, p_ptr->px);
    msg_format("You replace %s from your quiver.", o_name);
    p_ptr->redraw |= PR_MAP | PR_QUIVER;
    return amount;
}

static void do_cmd_quiver_arrows(object_type* o_ptr, int item)
{
    object_type moving;
    int available;
    int requested;
    int placed;

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_ARROW)
        return;
    if ((item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
        || (item >= 0 && inventory_slot_is_quivered_arrow(item)))
    {
        msg_print("Those arrows are already in your quiver.");
        return;
    }

    available = player_quiver_arrow_space();
    if (available > 0)
    {
        int maximum = MIN(available, o_ptr->number);

        requested = choose_arrow_move_quantity(o_ptr, "Quiver", maximum,
            maximum < o_ptr->number);
    }
    else
    {
        requested = choose_arrow_move_quantity(o_ptr, "Quiver",
            MIN(o_ptr->number, QUIVER_ARROW_CAPACITY), false);
        while (requested > player_quiver_arrow_space())
        {
            int replacement_item;
            int needed = requested - player_quiver_arrow_space();

            if (!choose_quiver_arrow_replacement(o_ptr, needed,
                    &replacement_item))
            {
                return;
            }
            if (drop_quiver_replacement_arrows(replacement_item, needed) <= 0)
                return;
        }
    }
    if (requested <= 0)
        return;

    object_copy(&moving, o_ptr);
    moving.number = requested;
    moving.pickup = false;
    moving.pickup_slot = INVEN_QUIVER1;
    placed = player_quiver_absorb_arrow(&moving);
    if (placed <= 0)
    {
        msg_print("You have no free place for another type of arrow.");
        return;
    }

    if (item >= 0)
    {
        inven_item_increase(item, 0 - placed);
        inven_item_optimize(item);
    }
    else
    {
        floor_item_increase(0 - item, 0 - placed);
        floor_item_optimize(0 - item);
    }

    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
    p_ptr->update |= PU_BONUS;
    p_ptr->notice |= PN_COMBINE | PN_REORDER;
    p_ptr->redraw |= PR_QUIVER;
    p_ptr->window |= PW_INVEN | PW_EQUIP | PW_PLAYER_0;
    msg_format("You place %d arrow%s in your quiver (%d/%d).", placed,
        placed == 1 ? "" : "s", player_quiver_arrow_count(),
        QUIVER_ARROW_CAPACITY);
    sound(MSG_EQUIP_BOW);
    wield_command_succeeded = true;
}

enum {
    CONTEXT_SQUARE_POPUP_SUPPRESS_TURNS = 10
};

static s64b context_square_popup_suppressed_at = -1;
static s64b context_square_popup_suppressed_until = -1;
static int pending_floor_context_item = 0;
static floor_context_action_kind pending_floor_context_action
    = FLOOR_CONTEXT_ACTION_NONE;

static bool context_square_popup_is_suppressed(void)
{
    s64b current_turn = (s64b)playerturn;

    if (context_square_popup_suppressed_at < 0)
        return false;

    /*
     * A loaded or newly created character can move the full-turn counter
     * backwards.  Temporary UI suppression belongs only to the run in which
     * it was requested.
     */
    if (current_turn < context_square_popup_suppressed_at
        || current_turn >= context_square_popup_suppressed_until)
    {
        context_square_popup_suppressed_at = -1;
        context_square_popup_suppressed_until = -1;
        return false;
    }

    return true;
}

/*
 * Show the desktop counterpart of Quick Touch after the player enters a
 * context-sensitive square.  This is a compact, nonblocking shortcut palette:
 * its buttons perform the same actions as the Quick Touch controls.
 *
 * Return true when a hint was shown.
 */
bool do_cmd_context_square_action_popup(void)
{
    char context_label[32];
    int floor_item = first_floor_item_under_player();
    char title[80];

    if (!get_sdl_show_context_square_popups()
        || context_square_popup_is_suppressed())
    {
        return false;
    }

    if (!touch_shortcut_context_action(' ', false, NULL,
            context_label, sizeof(context_label))
        || strcmp(context_label, "Confirm") == 0)
    {
        return false;
    }

    if (cave_stair_bold(p_ptr->py, p_ptr->px)
        || cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        cptr feature_name = f_name + f_info[
            cave_feat[p_ptr->py][p_ptr->px]].name;

        strnfmt(title, sizeof(title), "%^s", feature_name);
    }
    else if (floor_item)
    {
        object_type* o_ptr = &o_list[0 - floor_item];

        if (!o_ptr->k_idx)
            return false;
        if (o_ptr->tval == TV_CHEST && o_ptr->pval == 0)
            return false;

        object_desc(title, sizeof(title), o_ptr, true, 3);
    }
    else
    {
        return false;
    }

    sdl_question_menu_begin(title);
    sdl_question_menu_set_anchor_grid(p_ptr->py, p_ptr->px);

    if (floor_item && !cave_stair_bold(p_ptr->py, p_ptr->px)
        && !cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        object_type* o_ptr = &o_list[0 - floor_item];
        bool action_only = o_ptr->tval == TV_SKELETON
            || o_ptr->tval == TV_CHEST;
        char action_label[32];
        cptr action_name = floor_context_use_label(o_ptr, floor_item,
            action_label, sizeof(action_label));
        bool action_is_pickup = !action_name;

        if (!action_only)
            sdl_question_menu_add_button('x', "Description", TERM_L_BLUE);
        if (!action_is_pickup)
            sdl_question_menu_add_button(CMD_CONTEXT_FLOOR_ACTION,
                action_name, TERM_L_WHITE);
        if (!action_only)
        {
            cptr pickup_name = object_can_store_directly_in_pack(o_ptr)
                ? "Pack" : "Pick Up";

            sdl_question_menu_add_button('g', pickup_name,
                streq(pickup_name, "Pack") ? TERM_L_BLUE : TERM_L_WHITE);
            if (object_can_choose_pack_or_harness(o_ptr))
                sdl_question_menu_add_button(' ', "Harness", TERM_L_BLUE);
        }
    }
    else
    {
        sdl_question_menu_add_button(' ', context_label, TERM_L_WHITE);
    }

    sdl_question_menu_finish();
    sdl_question_menu_set_context_hint();
    Term_fresh();
    return true;
}

void do_cmd_suppress_context_square_popups(void)
{
    context_square_popup_suppressed_at = (s64b)playerturn;
    context_square_popup_suppressed_until =
        (s64b)playerturn + CONTEXT_SQUARE_POPUP_SUPPRESS_TURNS;
    sdl_question_menu_clear_context_hint();
}

/*
 * Perform the action promised by the desktop square-context popup without
 * reopening the generic inventory browser.  The popup is cleared whenever the
 * player enters another command, so its floor item is still on this square.
 */
void do_cmd_context_floor_item_action(void)
{
    int floor_item;
    floor_context_action_kind action;

    if (pending_floor_context_action != FLOOR_CONTEXT_ACTION_NONE)
    {
        floor_item = pending_floor_context_item;
        action = pending_floor_context_action;
        pending_floor_context_item = 0;
        pending_floor_context_action = FLOOR_CONTEXT_ACTION_NONE;
        (void)floor_context_perform_action(floor_item, action);
        return;
    }

    {
        int item_count = floor_context_first_item_and_count(&floor_item);

        if (item_count > 1)
            (void)floor_context_perform_action(0,
                FLOOR_CONTEXT_ACTION_ITEMS);
        else if (floor_item)
            (void)floor_context_perform_action(floor_item,
                FLOOR_CONTEXT_ACTION_USE);
    }
}

void do_cmd_queue_floor_context_action(floor_context_action_kind kind)
{
    int first_item = 0;
    int item_count;

    if (kind <= FLOOR_CONTEXT_ACTION_NONE
        || kind >= FLOOR_CONTEXT_ACTION_CLOSE)
    {
        return;
    }

    item_count = floor_context_first_item_and_count(&first_item);
    if (kind == FLOOR_CONTEXT_ACTION_ITEMS)
    {
        if (item_count <= 1)
            return;
        first_item = 0;
    }
    else if (kind == FLOOR_CONTEXT_ACTION_PICKUP_CONTEXT)
    {
        if (item_count <= 0)
            return;
        if (item_count > 1)
            first_item = 0;
    }
    else if (item_count != 1)
    {
        return;
    }

    pending_floor_context_item = first_item;
    pending_floor_context_action = kind;
    Term_keypress(CMD_CONTEXT_FLOOR_ACTION);
}

/*
 * Perform the targeted floor interaction promised by the unified Use action.
 * This is deliberately limited to the player's square: carried chests and
 * skeletons still have to be put down before they can be opened or searched.
 */
static bool use_floor_interaction_by_index(int item)
{
    int o_idx;
    object_type* o_ptr;

    if (item >= 0)
        return false;

    o_idx = 0 - item;
    if (o_idx <= 0 || o_idx >= o_max)
        return false;

    o_ptr = &o_list[o_idx];
    if (!o_ptr->k_idx || o_ptr->iy != p_ptr->py || o_ptr->ix != p_ptr->px)
        return false;

    if (o_ptr->tval == TV_SKELETON
        && !object_is_searched_skeleton(o_ptr))
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        do_cmd_search_skeleton(p_ptr->py, p_ptr->px, o_idx);
        return true;
    }

    if (o_ptr->tval == TV_CHEST && o_ptr->pval != 0)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;

        if (chest_trap_minigame)
        {
            (void)do_cmd_open_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        else if (o_ptr->pval > 0 && object_chest_trap_flags(o_ptr)
            && object_known_p(o_ptr))
        {
            (void)do_cmd_disarm_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        else
        {
            (void)do_cmd_open_chest(p_ptr->py, p_ptr->px, o_idx);
        }
        return true;
    }

    return false;
}

/*
 * Context-sensitive interpretation of touch shortcut bindings, so the
 * on-screen button shows (and performs) the action that fits the player's
 * current situation:
 *
 *   Confirm: in an open description -> the shown item's contextual pickup
 *          footer action, preserving Space so destination choosers can open;
 *          otherwise on stairs -> interact-here with confirmation, on a forge
 *          -> smith, on a pile -> choose an item, standing on one item -> its
 *          pickup destination, else -> confirm.
 *   '<'/'>' : on the matching staircase -> interact-here with confirmation.
 *   'u'/'x': outside a description, retain Use/Description.  Inside one, both
 *          fixed controls submit the first explicit footer action, preserving
 *          their old "perform the primary action" role.
 *
 * `description_open` is the caller's "an interactive item description popup is
 * showing" state.  Returns true (filling *out_key and label) for contextual
 * bindings while in the dungeon; false otherwise, so the caller keeps the
 * binding's static label/key.
 */
bool touch_shortcut_context_action(int binding, bool description_open,
    int* out_key, char* label, size_t label_len)
{
    int key = binding;
    int floor_interaction = 0;
    const char* name = NULL;
    char context_name[32];

    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return false;

    if (binding == ' ') {
        if (description_open) {
            int floor_item = first_floor_item_under_player();
            floor_context_action selected;

            key = ' ';
            if (!floor_item)
                name = "Confirm";
            else if (floor_context_preferred_pickup_action(floor_item,
                    &selected))
            {
                /* The description footer owns contextual pickup.  Preserve
                 * Space so multiple destinations can open their chooser
                 * instead of bypassing it through one raw destination key. */
                key = ' ';
                name = selected.label;
            }
            else
                name = "Confirm";
        } else if (cave_down_stairs_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which asks before changing levels. */
            key = ' ';
            name = "Go Down";
        } else if (cave_up_stairs_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which asks before changing levels. */
            key = ' ';
            name = "Go Up";
        } else if (cave_forge_bold(p_ptr->py, p_ptr->px)) {
            /* Space runs interact-here, which opens the smithing screen. */
            key = ' ';
            name = "Smith";
        } else {
            int first_item = 0;
            int item_count = floor_context_first_item_and_count(&first_item);

            if (item_count > 1)
            {
                key = CMD_CONTEXT_FLOOR_ACTION;
                strnfmt(context_name, sizeof(context_name), "Items (%d)...",
                    item_count);
                name = context_name;
            }
            else if ((floor_interaction =
                first_floor_interaction_under_player()) != 0)
            {
                /* Leave Space intact so request_command() applies its normal
                 * "/5" interact-here keymap. */
                key = ' ';
                name = item_use_action_name(&o_list[-floor_interaction],
                    floor_interaction);
            }
            else if (first_item != 0)
            {
                key = ' ';
                name = floor_context_space_pickup_label(first_item);
            }
            else
            {
                key = ' ';
                name = "Confirm";
            }
        }
    } else if (binding == '<') {
        if (!cave_up_stairs_bold(p_ptr->py, p_ptr->px))
            return false;
        /* Keep Space so the interact-here command supplies the confirmation. */
        key = ' ';
        name = "Go Up";
    } else if (binding == '>') {
        if (!cave_down_stairs_bold(p_ptr->py, p_ptr->px))
            return false;
        /* Keep Space so the interact-here command supplies the confirmation. */
        key = ' ';
        name = "Go Down";
    } else if (binding == 'u') {
        int floor_item = first_floor_item_under_player();
        floor_context_action selected;

        if (description_open && floor_item != 0
            && floor_context_primary_popup_action(floor_item, &selected))
        {
            key = selected.key;
            name = selected.label;
        }
        else
        {
            key = 'u';
            name = (floor_item != 0)
                ? item_use_action_name(&o_list[-floor_item], floor_item)
                : "Use";
        }
    } else if (binding == 'x') {
        key = 'x';
        if (description_open) {
            int floor_item = first_floor_item_under_player();
            floor_context_action selected;

            if (floor_item != 0
                && floor_context_primary_popup_action(floor_item, &selected))
            {
                key = selected.key;
                name = selected.label;
            }
            else
            {
                name = "Use";
            }
        } else {
            name = "Description";
        }
    } else {
        return false;
    }

    if (out_key)
        *out_key = key;
    if (label && label_len)
        strnfmt(label, label_len, "%s", name);
    return true;
}

static bool smith_oath_takeoff_hits_pack(const object_type* o_ptr, int source_item)
{
    if (!smith_oath_forbids_object(o_ptr))
        return false;

    if (player_inventory_handle_is_carried(source_item))
        return inven_carry_okay_after_removing(o_ptr, source_item, 1);

    return inven_carry_okay(o_ptr);
}

bool open_supplies_menu_with_context(supply_menu_action default_action, int default_group, bool default_focus, bool default_hotkey)
{
    supply_menu_request request = {0};
    supply_menu_action action = default_action;
    bool hotkey = default_hotkey;
    bool focus = default_focus;
    int group = default_group;

    if (supplies_has_pending_action())
    {
        supply_menu_action pending = supplies_pending_action();
        if (pending != SUPPLY_MENU_ACTION_NONE)
            action = pending;
        hotkey = supplies_pending_hotkey();
        int pending_group = supplies_pending_group();
        if (pending_group >= 0 && pending_group < SUPPLY_GROUP_MAX)
        {
            focus = true;
            group = pending_group;
        }
        supplies_clear_pending_action();
    }

    request.action = action;
    request.hotkey_mode = hotkey;
    if (focus && group >= 0 && group < SUPPLY_GROUP_MAX)
    {
        request.focus_group = true;
        request.group = group;
    }

    return do_cmd_knowledge_supplies(&request);
}

bool open_inventory_menu_page(supply_menu_page page)
{
    supply_menu_request request = {0};

    request.focus_page = true;
    request.page = page;

    return do_cmd_knowledge_supplies(&request);
}

bool open_inventory_menu_category(inventory_menu_group group)
{
    supply_menu_request request = {0};

    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_inventory_group = true;
    request.inventory_group = group;

    return do_cmd_knowledge_supplies(&request);
}

static bool replacement_choice_type_matches(const object_type* incoming,
    const object_type* candidate)
{
    enum inventory_limit_group incoming_group;

    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    incoming_group = inventory_limit_group_for_object(incoming);
    if (incoming_group == INV_LIMIT_PACK
        || incoming_group == INV_LIMIT_HARNESS)
    {
        return inventory_limit_group_for_object(candidate) == incoming_group;
    }

    if (player_oil_container_object(incoming)
        && player_oil_container_object(candidate))
    {
        return true;
    }

    if (incoming->tval == candidate->tval)
        return true;

    {
        int incoming_slot = wield_slot(incoming);

        if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
        {
            int candidate_slot = wield_slot(candidate);

            if (candidate_slot == incoming_slot)
                return true;
        }
    }

    return false;
}

static bool replacement_choice_allowed(const object_type* incoming,
    const object_type* candidate, bool equipped, bool include_equip)
{
    if (!incoming || !incoming->k_idx || !candidate || !candidate->k_idx)
        return false;

    if (equipped)
    {
        if (!include_equip)
            return false;
        if (cursed_p(candidate))
            return false;
    }

    /* Oil flasks are expendable lamp capacity, not a player-facing choice. */
    if (incoming->tval == TV_LIGHT && incoming->sval == SV_LIGHT_LANTERN
        && candidate->tval == TV_FLASK)
    {
        return false;
    }

    if (!inven_carry_limit_can_replace(candidate))
        return false;

    return replacement_choice_type_matches(incoming, candidate);
}

bool open_inventory_replacement_menu(inventory_menu_group group,
    const object_type* incoming, bool include_equip, bool include_supplies,
    cptr reason, int* replacement_item)
{
    object_choice_entry* entries;
    int capacity;
    int count = 0;
    int selected = -1;
    char desc[480];
    char incoming_name[120];
    char incoming_summary[160];

    if (replacement_item)
        *replacement_item = -1;

    if (!incoming || !incoming->k_idx || !replacement_item)
        return false;

    /* Volume replacement is a pool decision, so use the inventory browser's
     * Pack/Harness view rather than a type-specific flat picker.  Supply and
     * light replacement retain their specialized behavior below. */
    if (group == INVENTORY_MENU_GROUP_PACK
        || group == INVENTORY_MENU_GROUP_HARNESS)
    {
        supply_menu_request request = {0};

        request.focus_page = true;
        request.page = SUPPLY_MENU_PAGE_INVENTORY;
        request.focus_inventory_group = true;
        request.inventory_group = group;
        request.preview_inventory_description = true;
        request.replacement_mode = true;
        request.replacement_incoming = incoming;
        request.replacement_include_equip = include_equip;
        request.replacement_include_supplies = include_supplies;
        request.replacement_reason = reason;
        request.replacement_item_out = replacement_item;
        return do_cmd_knowledge_supplies(&request);
    }

    capacity = player_pack_entry_count();
    if (include_equip)
        capacity += INVEN_TOTAL - INVEN_WIELD;
    if (include_supplies)
        capacity += supplies_entry_count();
    entries = mem_alloc_array(MAX(capacity, 1), object_choice_entry);

    if (include_equip)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (!replacement_choice_allowed(incoming, &inventory[i], true,
                    include_equip))
            {
                continue;
            }

            object_choice_entry_make(&entries[count], i, &inventory[i],
                NULL, NULL);
            count++;
        }
    }

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!replacement_choice_allowed(incoming, o_ptr, false,
                include_equip))
        {
            continue;
        }

        object_choice_entry_make(&entries[count], item, o_ptr, NULL, NULL);
        count++;
    }

    if (include_supplies)
    {
        for (int i = 0; i < supplies_entry_count(); i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (!replacement_choice_allowed(incoming, o_ptr, false,
                    include_equip))
            {
                continue;
            }

            object_choice_entry_make(&entries[count], SUPPLIES_INDEX + i,
                o_ptr, NULL, NULL);
            count++;
        }
    }

    if (count <= 0)
    {
        entries = mem_free(entries);
        return false;
    }

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    if (show_weights)
    {
        int incoming_weight = incoming->weight * MAX(incoming->number, 1);

        strnfmt(incoming_summary, sizeof(incoming_summary), "%s  %2d.%1d lb",
            incoming_name, incoming_weight / 10, incoming_weight % 10);
    }
    else
    {
        SDL_strlcpy(incoming_summary, incoming_name,
            sizeof(incoming_summary));
    }
    if (reason && reason[0])
    {
        strnfmt(desc, sizeof(desc), "%s\nPicking up: %s", reason,
            incoming_summary);
    }
    else
    {
        strnfmt(desc, sizeof(desc), "Picking up: %s", incoming_summary);
    }

    if (!object_choice_overlay("What to replace?", desc, entries, count, 0,
            &selected))
    {
        entries = mem_free(entries);
        return false;
    }

    if (selected < 0 || selected >= count)
    {
        entries = mem_free(entries);
        return false;
    }

    *replacement_item = entries[selected].item;
    entries = mem_free(entries);
    return true;
}

bool open_inventory_slot_pick_menu(const object_type* incoming,
    const bool* enabled, cptr reason, int* slot_out)
{
    supply_menu_request request = {0};

    if (slot_out)
        *slot_out = -1;

    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.preview_inventory_description = true;
    request.slot_pick_mode = true;
    request.slot_pick_incoming = incoming;
    request.slot_pick_enabled = enabled;
    request.slot_pick_reason = reason;
    request.slot_pick_item_out = slot_out;

    return do_cmd_knowledge_supplies(&request);
}

bool open_inventory_item_select_menu(int mode, cptr reason, cptr none_msg,
    int* item_out)
{
    bool selected;

    if (!item_out)
        return false;

    *item_out = -1;

    p_ptr->get_item_mode = mode;
    selected = object_item_select_overlay(mode, reason, none_msg, item_out);

    p_ptr->get_item_mode = 0;
    item_tester_tval = 0;
    item_tester_hook = NULL;
    p_ptr->command_wrk = 0;
    p_ptr->command_see = false;

    return selected;
}

static inventory_menu_group inventory_browser_group_for_object(
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return INVENTORY_MENU_GROUP_ALL;

    return inventory_menu_group_for_limit_group(
        inventory_limit_group_for_object(o_ptr));
}

static int supply_browser_group_for_object(const object_type* o_ptr)
{
    if (supplies_group_matches_object(SUPPLY_GROUP_HERBS, o_ptr))
        return SUPPLY_GROUP_HERBS;
    if (supplies_group_matches_object(SUPPLY_GROUP_FOOD, o_ptr))
        return SUPPLY_GROUP_FOOD;
    if (supplies_group_matches_object(SUPPLY_GROUP_POTIONS, o_ptr))
        return SUPPLY_GROUP_POTIONS;
    if (supplies_group_matches_object(SUPPLY_GROUP_GEMS, o_ptr))
        return SUPPLY_GROUP_GEMS;
    if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
        return SUPPLY_GROUP_LIGHTS;

    return SUPPLY_GROUP_SUPPLY;
}

static bool open_inventory_menu_focused_on_floor(int floor_item,
    bool use_type_group, supply_floor_action floor_action)
{
    supply_menu_request request = {0};
    int floor_o_idx = 0 - floor_item;
    object_type* o_ptr;

    if (floor_o_idx <= 0 || floor_o_idx >= o_max)
        return open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);

    o_ptr = &o_list[floor_o_idx];
    if (!o_ptr->k_idx)
        return open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);

    request.focus_floor_item = true;
    request.floor_o_idx = floor_o_idx;
    request.floor_action = floor_action;

    if (supplies_is_supply_object(o_ptr))
    {
        request.focus_group = true;
        request.group = supply_browser_group_for_object(o_ptr);
    }
    else
    {
        request.focus_page = true;
        request.page = SUPPLY_MENU_PAGE_INVENTORY;
        request.focus_inventory_group = true;
        request.inventory_group = use_type_group
            ? inventory_browser_group_for_object(o_ptr)
            : INVENTORY_MENU_GROUP_ALL;
    }

    return do_cmd_knowledge_supplies(&request);
}

static bool handle_iron_crown_silmaril_action(object_type* o_ptr, int item)
{
    object_type* w_ptr;

    if (!o_ptr)
        return false;

    if ((o_ptr->name1 < ART_MORGOTH_1) || (o_ptr->name1 > ART_MORGOTH_3))
        return false;

    if (item >= 0)
    {
        msg_print("You would have to put it down first.");
        return true;
    }

    w_ptr = &inventory[INVEN_WIELD];
    if (!w_ptr->k_idx)
    {
        msg_print(
            "To prise a Silmaril from the crown, you would need to wield a "
            "weapon.");
        return true;
    }

    if (!get_check("Will you try to prise a Silmaril from the Iron Crown? "))
        return true;

    prise_silmaril();

    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;

    return true;
}

/*
 * Use an item by index, helper for enhanced menus
 */
static bool replace_pack_item_for_arrow_move(const object_type* incoming)
{
    int replacement_item = -1;
    object_type* candidate;
    int used;
    int limit;
    int needed;
    int left;
    int remove_amount;
    char reason[280];

    if (!incoming || !incoming->k_idx || incoming->number <= 0)
        return false;

    used = inventory_limit_usage_for_group(INV_LIMIT_PACK);
    limit = inventory_limit_limit_for_group(INV_LIMIT_PACK);
    if (inven_carry_limit_failed()
        && inven_carry_limit_group() == INV_LIMIT_PACK)
    {
        limit = MAX(limit, inven_carry_limit_value());
    }
    needed = inventory_limit_additional_space_for_object(incoming);
    left = MAX(limit - used, 0);
    if (inventory_limit_space_for_object(incoming) > limit)
    {
        msg_format("Those arrows need %d.%d qt, more than your Pack's %d.%d "
                   "qt maximum.",
            inventory_limit_space_for_object(incoming) / 10,
            ABS(inventory_limit_space_for_object(incoming) % 10), limit / 10,
            ABS(limit % 10));
        return false;
    }
    strnfmt(reason, sizeof(reason),
        "No room in Pack: %d.%d/%d.%d qt used; %d.%d qt left, %d.%d qt "
        "needed. Choose an item to drop.",
        used / 10, ABS(used % 10), limit / 10, ABS(limit % 10), left / 10,
        ABS(left % 10), needed / 10, ABS(needed % 10));

    if (!open_inventory_replacement_menu(INVENTORY_MENU_GROUP_PACK, incoming,
            false, false, reason, &replacement_item))
    {
        return false;
    }
    if (!player_inventory_handle_is_carried(replacement_item))
        return false;

    candidate = player_inventory_object(replacement_item);
    if (!candidate || !candidate->k_idx
        || inventory_limit_group_for_object(candidate) != INV_LIMIT_PACK)
    {
        return false;
    }

    remove_amount = candidate->number;
    for (int amount = 1; amount <= candidate->number; amount++)
    {
        int projected = inventory_limit_usage_after_replacing(incoming,
            candidate, amount);

        if (projected >= 0 && projected <= limit)
        {
            remove_amount = amount;
            break;
        }
    }

    inven_drop(replacement_item, remove_amount);
    p_ptr->notice |= PN_COMBINE | PN_REORDER;
    notice_stuff();
    return true;
}

static void do_cmd_unquiver_pack_arrow(int item)
{
    object_type packed;
    object_type* o_ptr;
    int max_quantity;
    int original_number;
    int placed;

    o_ptr = player_quiver_arrow_object(item);
    if (!o_ptr || !o_ptr->k_idx)
    {
        return;
    }

    object_copy(&packed, o_ptr);
    packed.pickup = false;
    packed.pickup_slot = -1;
    /* Loose arrows always use the Pack by type.  Keep their storage marker
     * neutral so they combine with arrows obtained through ordinary pickup. */
    packed.storage = OBJECT_STORAGE_NONE;
    if (player_pack_action_start(PLAYER_PACK_ACTION_MOVE_STORAGE, item,
            OBJECT_STORAGE_PACK, false, &packed))
        return;

    max_quantity = inventory_limit_max_carryable_quantity(&packed);
    if (max_quantity > 0)
    {
        int maximum = MIN(max_quantity, o_ptr->number);

        packed.number = choose_arrow_move_quantity(o_ptr, "Pack", maximum,
            maximum < o_ptr->number);
    }
    else
    {
        packed.number = choose_arrow_move_quantity(o_ptr, "Pack",
            o_ptr->number, false);
    }
    if (packed.number <= 0)
        return;

    while (!inventory_type_slot_available(&packed, true))
    {
        if (!inven_carry_limit_failed()
            || inven_carry_limit_group() != INV_LIMIT_PACK
            || !replace_pack_item_for_arrow_move(&packed))
        {
            return;
        }
    }

    original_number = packed.number;
    (void)inven_carry(&packed, false);
    placed = original_number - packed.number;
    if (placed <= 0)
    {
        msg_print("There is not enough room in your Pack for those arrows.");
        return;
    }
    player_quiver_remove_arrows(item, placed);
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
    p_ptr->update |= PU_BONUS;
    p_ptr->notice |= PN_COMBINE | PN_REORDER;
    p_ptr->redraw |= PR_QUIVER;
    p_ptr->window |= PW_INVEN | PW_EQUIP | PW_PLAYER_0;
    msg_format("You move %d arrow%s from your quiver to your Pack.", placed,
        placed == 1 ? "" : "s");
}

static bool item_storage_destination_available(const object_type* o_ptr,
    byte target_storage, int quantity, bool report)
{
    object_type moving;

    if (!o_ptr || !o_ptr->k_idx
        || (target_storage != OBJECT_STORAGE_PACK
            && target_storage != OBJECT_STORAGE_HARNESS))
    {
        return false;
    }

    object_copy(&moving, o_ptr);
    moving.storage = target_storage;
    moving.number = MAX(1, MIN(quantity, o_ptr->number));
    if (inventory_type_slot_available(&moving, report))
        return true;

    if (report)
    {
        enum inventory_limit_group group =
            target_storage == OBJECT_STORAGE_PACK
            ? INV_LIMIT_PACK : INV_LIMIT_HARNESS;
        int used = inventory_limit_usage_for_group(group);
        int limit = inven_carry_limit_value();
        int needed = inventory_limit_additional_space_for_object(&moving);
        int left;

        if (limit < 0)
            limit = inventory_limit_limit_for_group(group);
        left = MAX(limit - used, 0);
        msg_format("No room in %s: %d.%d/%d.%d qt used (%d.%d qt left); "
                   "this move needs %d.%d qt.",
            inventory_limit_group_name(group), used / 10, ABS(used % 10),
            limit / 10, ABS(limit % 10), left / 10, ABS(left % 10),
            needed / 10, ABS(needed % 10));
    }

    return false;
}

bool do_cmd_move_item_to_storage(int item, byte target_storage)
{
    object_type* o_ptr;
    char o_name[80];

    if (target_storage == OBJECT_STORAGE_PACK
        && ((item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
            || (item >= 0 && inventory_slot_is_quivered_arrow(item))))
    {
        do_cmd_unquiver_pack_arrow(item);
        return true;
    }

    if (!player_inventory_handle_is_carried(item))
        return false;

    o_ptr = player_inventory_object(item);
    if (!object_can_choose_pack_or_harness(o_ptr)
        || (target_storage != OBJECT_STORAGE_PACK
            && target_storage != OBJECT_STORAGE_HARNESS)
        || o_ptr->storage == target_storage)
    {
        return false;
    }

    if (!item_storage_destination_available(o_ptr, target_storage,
            o_ptr->number, true))
    {
        return false;
    }

    /* Reaching into the Pack, or opening it to store something, follows the
     * same interruptible three-turn rule as every other Pack action. */
    if (player_pack_action_start_forced(PLAYER_PACK_ACTION_MOVE_STORAGE,
            item, target_storage, false, o_ptr))
    {
        return true;
    }

    o_ptr->storage = target_storage;
    if (target_storage == OBJECT_STORAGE_HARNESS)
        player_active_weapon_assign_harness_color(o_ptr);
    if (target_storage == OBJECT_STORAGE_PACK)
    {
        /* Stored copies are no longer assigned to an auto-recovery sheath. */
        o_ptr->pickup = false;
        o_ptr->pickup_slot = -1;
    }
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    if (target_storage == OBJECT_STORAGE_HARNESS)
        msg_format("You ready %s on your Harness.", o_name);
    else
        msg_format("You store %s in your Pack.", o_name);

    p_ptr->notice |= PN_COMBINE | PN_REORDER;
    p_ptr->update |= PU_BONUS;
    p_ptr->redraw |= PR_BASIC | PR_MEL | PR_ARC | PR_QUIVER | PR_MAP;
    p_ptr->window |= PW_INVEN | PW_EQUIP | PW_PLAYER_0;
    return true;
}

void do_cmd_use_item_by_index(int item)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, -1, false, true);
        return;
    }

    object_type* o_ptr;

    if (item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
    {
        do_cmd_unquiver_pack_arrow(item);
        return;
    }

    /* Get the item (in the pack) */
    if (player_inventory_handle_valid(item))
    {
        o_ptr = player_inventory_object(item);
        log_debug("do_cmd_use_item_by_index: Using item from inventory, index=%d", item);
    }

    /* Get the item (on the floor) */
    else if (item < 0)
    {
        o_ptr = &o_list[0 - item];
        log_debug("do_cmd_use_item_by_index: Using item from floor, index=%d, o_list index=%d", item, 0 - item);
    }
    else
    {
        return;
    }

    if (item >= 0 && item < INVEN_PACK
        && inventory_slot_is_quivered_arrow(item))
    {
        do_cmd_unquiver_pack_arrow(item);
        return;
    }

    if (o_ptr->name1 == ART_MORGOTH_0)
    {
        msg_print("There are no Silmarils left in the Iron Crown.");
        return;
    }

    if (handle_iron_crown_silmaril_action(o_ptr, item))
        return;

    if (!((item < 0) && o_ptr->tval == TV_ARROW)
        && player_pack_action_start(PLAYER_PACK_ACTION_USE_ITEM, item, 0,
            false, o_ptr))
        return;

    if (use_floor_interaction_by_index(item))
        return;

    // determine the action based on the item type
    switch (o_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    case TV_ARROW:
    case TV_FLASK:
    {
        /* Floor handles are negative, while expandable carried handles are
         * above the fixed inventory range.  Both are wield sources; only
         * actual equipment handles may be routed to takeoff. */
        if (item < 0 || player_inventory_handle_is_carried(item))
        {
            object_type* l_ptr = &inventory[INVEN_LITE];
            bool try_to_wield = true;

            // possibly refuel a light
            if ((o_ptr->tval == TV_FLASK)
                || ((l_ptr->tval == o_ptr->tval) && (l_ptr->sval == o_ptr->sval)
                    && (o_ptr->sval == SV_LIGHT_LANTERN)))
            {
                if (l_ptr->sval == SV_LIGHT_LANTERN)
                {
                    do_cmd_refuel_lamp(o_ptr, item);
                    try_to_wield = false;
                }
            }

            if (o_ptr->tval == TV_FLASK && try_to_wield)
            {
                if ((l_ptr->tval != TV_LIGHT)
                    || (l_ptr->sval != SV_LIGHT_LANTERN))
                {
                    msg_print("You are not wielding a lantern.");
                }
                try_to_wield = false;
            }

            if (try_to_wield)
            {
                log_debug("do_cmd_use_item_by_index: Calling do_cmd_wield with item=%d (o_ptr tval=%d)", item, o_ptr->tval);
                /* Handle arrows and throwing weapons */
                if (o_ptr->tval == TV_ARROW)
                {
                    do_cmd_wield(o_ptr, item);
                }
                else
                {
                    do_cmd_wield(o_ptr, item);
                }
            }
        }
        else if (player_inventory_handle_is_equipped(item))
        {
            /* Handle equipped arrows specially */
            if (o_ptr->tval == TV_ARROW)
            {
                do_cmd_takeoff(o_ptr, item);
            }
            else
            {
                do_cmd_takeoff(o_ptr, item);
            }
        }
        else
        {
            log_warn("do_cmd_use_item_by_index: refusing invalid item handle %d",
                item);
        }
        break;
    }
    case TV_NOTE:
    {
        note_info_screen(o_ptr);
        break;
    }
    case TV_METAL:
    {
        msg_print("To smith with mithril or star-iron, take them to a forge and "
                  "type (,).");
        break;
    }
    case TV_CHEST:
    {
        msg_print("You would need to put it down to open it.");
        break;
    }
    case TV_SKELETON:
    {
        msg_print("You would need to put it down to search it.");
        break;
    }
    case TV_STAFF:
    {
        if (item < 0)
            py_pickup_aux(0 - item);
        else
            do_cmd_activate_staff(o_ptr, item);
        break;
    }
    case TV_GEM:
    {
        do_cmd_use_gem(o_ptr, item);
        break;
    }
    case TV_HORN:
    {
        if (item < 0)
            py_pickup_aux(0 - item);
        else
            do_cmd_play_instrument(o_ptr, item);
        break;
    }

    case TV_POTION:
    {
        do_cmd_quaff_potion(o_ptr, item);
        break;
    }
    case TV_FOOD:
    {
        do_cmd_eat_food(o_ptr, item);
        break;
    }
    default:
    {
        msg_print("It has no use.");
        break;
    }
    }
}

/*
 * Use an item, a unified 'use' command.
 */
void do_cmd_use_item(void)
{
    int floor_item = first_floor_item_under_player();

    if (floor_item)
    {
        log_debug(
            "do_cmd_use_item: Opening browser on floor item under player, item=%d",
            floor_item);
        (void)open_inventory_menu_focused_on_floor(floor_item, true,
            SUPPLY_FLOOR_ACTION_USE);
        return;
    }

    {
        extern char current_menu_command;
        extern int current_menu_state;

        current_menu_command = 0;
        current_menu_state = 0;
    }

    log_debug("do_cmd_use_item: No floor item, opening all inventory browser");
    (void)open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);
}

/*
 * Wrapper for wear/wield command with enhanced menu support
 */
void do_cmd_wield_wrapper(void)
{
    int floor_item = first_floor_item_under_player();

    log_debug("do_cmd_wield_wrapper: Opening all inventory browser");

    if (floor_item)
    {
        (void)open_inventory_menu_focused_on_floor(floor_item, false,
            SUPPLY_FLOOR_ACTION_WIELD);
        return;
    }

    (void)open_inventory_menu_category(INVENTORY_MENU_GROUP_ALL);
}

/*
 * Enhanced wear/wield command that supports cycling between inventory/equipment
 */
void do_cmd_wield_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_wield_enhanced: Starting enhanced wear/wield cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }
    
    /* Set the filter to only show wearable items */
    item_tester_hook = item_tester_hook_wear;
    log_debug("do_cmd_wield_enhanced: Set item_tester_hook to item_tester_hook_wear (%p)", (void*)item_tester_hook);
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the filter */
    item_tester_hook = NULL;
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Enhanced use item command that supports cycling between inventory/equipment
 */
void do_cmd_use_item_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_use_item_enhanced: Starting enhanced use item cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Direct access inventory with cycling support
 */
void do_cmd_inven_direct(void)
{
    log_debug("do_cmd_inven_direct: Opening inventory browser page");
    (void)open_inventory_menu_page(SUPPLY_MENU_PAGE_INVENTORY);
}

/*
 * Direct access equipment with cycling support
 */
void do_cmd_equip_direct(void)
{
    log_debug("do_cmd_equip_direct: Opening equipped browser page");
    (void)open_inventory_menu_page(SUPPLY_MENU_PAGE_EQUIPPED);
}

/*
 * Display inventory
 */
void do_cmd_inven(void)
{
    do_cmd_inven_direct();
}

/*
 * Display equipment
 */
void do_cmd_equip(void)
{
    do_cmd_equip_direct();
}

/*
 * Wield or wear a single item from the pack or floor
 */
void do_cmd_wield(object_type* default_o_ptr, int default_item)
{
    int item, slot;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    cptr act;

    cptr q, s;

    int i, quantity, original_quantity;

    bool weapon_less_effective = false;

    bool grants_two_weapon = false;

    char o_name[80];

    bool combine = false;
    bool is_throwing = false;
    bool free_active_weapon_change = false;
    int supply_index = supplies_current_action();
    bool from_supplies = false;
    int oil_swap_drop_idx = 0;
    int lamp_flasks_to_replace = 0;
    int lamp_flask_oil = 0;
    int lamp_replacement_item = -1;
    bool lamp_flask_replacement_planned = false;
    bool move_pack_weapon_to_harness = false;

    u32b f1, f2, f3, f4;

    log_debug("do_cmd_wield: Called with default_o_ptr=%p, default_item=%d", (void*)default_o_ptr, default_item);

    wield_command_succeeded = false;

    /* Ensure throw_slot_menu_active is false at start */
    throw_slot_menu_active = false;
    for (i = 0; i < INVEN_TOTAL; i++)
        throw_slot_enabled[i] = false;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
        from_supplies = (item == SUPPLIES_INDEX) && (supply_index >= 0);
        log_debug("do_cmd_wield: Using default item, tval=%d, sval=%d, k_idx=%d", 
            o_ptr->tval, o_ptr->sval, o_ptr->k_idx);
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_hook_wear;

        /* Get an item */
        q = "Wear/Wield which item? ";
        s = "You have nothing you can wear or wield.";
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
            return;

        if (item == SUPPLIES_INDEX)
        {
            open_supplies_menu_with_context(
                SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_LIGHTS, true, true);
            return;
        }

        /* Get the item (in the pack) */
        if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
        }
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    if (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN)
    {
        msg_print("Staves and horns are kept in your Harness and used from there.");
        return;
    }

    if (object_has_broken_prefix(o_ptr))
    {
        msg_print("Broken items must be repaired before they can be equipped.");
        return;
    }

    if (!confirm_known_cursed_wield(o_ptr, item))
        return;

    if (!((item < 0) && o_ptr->tval == TV_ARROW)
        && player_pack_action_start(PLAYER_PACK_ACTION_WIELD, item,
            forced_wield_slot, forced_wield_full_stack, o_ptr))
    {
        wield_command_succeeded = true;
        return;
    }

    // remember how many there were
    original_quantity = o_ptr->number;

    // Check whether it would be too heavy
    if ((item < 0)
        && (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2))
    {
        /* Describe it */
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

        log_debug("do_cmd_wield: Floor item too heavy - total=%d + item=%d > limit=%d", 
            p_ptr->total_weight, o_ptr->weight, weight_limit() * 3 / 2);

        if (o_ptr->k_idx)
            msg_format("You cannot lift %s.", o_name);
        else
            log_debug("do_cmd_wield: WARNING - o_ptr->k_idx is 0, no message shown to user!");

        /* Abort */
        return;
    }
    
    log_debug("do_cmd_wield: Weight check passed or inventory item (item=%d)", item);

    if (o_ptr->tval == TV_ARROW)
    {
        do_cmd_quiver_arrows(o_ptr, item);
        return;
    }

    /* Check the slot */
    slot = wield_slot(o_ptr);
    if (forced_wield_slot >= INVEN_WIELD && forced_wield_slot < INVEN_TOTAL)
    {
        if (!forced_wield_slot_accepts_object(o_ptr, forced_wield_slot))
        {
            if (item < 0)
                object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            else
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            msg_format("You cannot put %s there.", o_name);
            return;
        }

        slot = forced_wield_slot;
    }
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        if (item < 0)
            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        else
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        msg_format("You cannot wear or wield %s.", o_name);
        return;
    }

    /* Equipping directly from the floor is still an acquisition.  Harness
     * items continue to consume Harness volume after being equipped, so this
     * path must not bypass the same limit enforced by normal pickup. */
    if (item < 0
        && inventory_limit_group_for_object(o_ptr) == INV_LIMIT_HARNESS
        && !inventory_type_slot_available(o_ptr, true))
    {
        enum inventory_limit_group group = inven_carry_limit_group();

        if (group == INV_LIMIT_PACK || group == INV_LIMIT_HARNESS)
        {
            int used = inventory_limit_usage_for_group(group);
            int limit = inven_carry_limit_value();
            int needed = inventory_limit_additional_space_for_object(o_ptr);
            int left = MAX(limit - used, 0);

            msg_format("No room in %s: %d.%d/%d.%d qt used (%d.%d qt left); "
                       "this item needs %d.%d qt.",
                inventory_limit_group_name(group), used / 10, used % 10,
                limit / 10, limit % 10, left / 10, left % 10,
                needed / 10, needed % 10);
        }
        return;
    }

    if ((item < 0) && player_light_carry_cap(o_ptr) > 0
        && !(o_ptr->tval == TV_LIGHT
            && o_ptr->sval == SV_LIGHT_LANTERN))
    {
        object_type* equipped_ptr = &inventory[slot];
        bool replacing_same_group = equipped_ptr->k_idx
            && player_light_share_carry_group(o_ptr, equipped_ptr);

        if (!replacing_same_group && player_light_available_capacity(o_ptr) <= 0)
        {
            inventory_menu_group menu_group =
                inventory_menu_group_for_limit_group(
                    inventory_limit_group_for_object(o_ptr));

            if (player_oil_container_object(o_ptr))
                msg_print("You have no free lamp/flask slots.");
            else
                msg_print("You cannot carry any more of those.");
            if (menu_group != INVENTORY_MENU_GROUP_ALL)
                (void)open_inventory_menu_category(menu_group);
            return;
        }
    }

    /* Ask for ring to replace */
    if ((forced_wield_slot < 0) && (o_ptr->tval == TV_RING) && inventory[INVEN_LEFT].k_idx
        && inventory[INVEN_RIGHT].k_idx)
    {
        item_tester_tval = TV_RING;
        item_tester_hook = item_tester_hook_ring_slots;
        item_tester_full = false;

        q = "Replace which ring? ";
        s = "Oops.";
        if (!open_inventory_item_select_menu(USE_EQUIP, q, s, &slot))
        {
            item_tester_tval = 0;
            item_tester_hook = NULL;
            return;
        }

        item_tester_tval = 0;
        item_tester_hook = NULL;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    is_throwing = player_can_treat_as_throwing_flags(o_ptr, f3);

    log_debug("do_cmd_wield: item=%d, is_throwing=%d, slot=%d", item, is_throwing, slot);

    if (is_throwing)
    {
        if ((forced_wield_slot == INVEN_WIELD
                || forced_wield_slot == INVEN_BELT)
            && forced_wield_slot_accepts_object(o_ptr, forced_wield_slot))
        {
            slot = forced_wield_slot;
        }
        else
        {
            bool any_throw_dest = false;
            int slot_choice;

            log_debug("do_cmd_wield: Throwing weapon detected, showing slot menu");
            throw_slot_menu_active = true;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;

            {
                object_type* wield_ptr = &inventory[INVEN_WIELD];
                bool allow_wield = true;

                if (wield_ptr->k_idx && cursed_p(wield_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_wield = false;
                }

                if (allow_wield)
                {
                    throw_slot_enabled[INVEN_WIELD] = true;
                    any_throw_dest = true;
                }
            }

            {
                object_type* belt_ptr = &inventory[INVEN_BELT];
                bool allow_belt = object_is_belt_weapon(o_ptr);

                if (belt_ptr->k_idx && cursed_p(belt_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_belt = false;
                }

                if (allow_belt)
                {
                    throw_slot_enabled[INVEN_BELT] = true;
                    any_throw_dest = true;
                }
            }

            if (!any_throw_dest)
            {
                log_debug("do_cmd_wield: No available slot for throwing weapon, returning");
                msg_print("You have no available slot for that throwing weapon.");
                throw_slot_menu_active = false;
                return;
            }

            slot_choice = slot;

            if (!throw_slot_enabled[slot_choice])
            {
                if (throw_slot_enabled[INVEN_BELT])
                    slot_choice = INVEN_BELT;
                else
                    slot_choice = INVEN_WIELD;
            }

            /* Count the available destinations to decide if a choice is needed. */
            int throw_dest_count = 0;
            for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
                if (throw_slot_enabled[i])
                    throw_dest_count++;

            bool slot_selected;

            if (throw_dest_count <= 1)
            {
                /* Only one place it can go - no need to ask. */
                slot_selected = true;
            }
            else
            {
                /* Throwing weapons use the hand or the optional belt. */
                int chosen_slot = -1;

                slot_selected = open_inventory_slot_pick_menu(o_ptr,
                    throw_slot_enabled,
                    "Place this throwing weapon: the hand wields it, "
                    "and the belt holds one dagger or hand axe. Other "
                    "throwing weapons stay on the Harness.",
                    &chosen_slot);

                if (slot_selected && chosen_slot >= INVEN_WIELD
                    && chosen_slot < INVEN_TOTAL && throw_slot_enabled[chosen_slot])
                    slot_choice = chosen_slot;
                else
                    slot_selected = false;
            }

            if (!slot_selected)
            {
                log_debug("do_cmd_wield: User cancelled slot selection, cleaning up and returning");
                item_tester_hook = NULL;
                item_tester_full = false;

                for (i = 0; i < INVEN_TOTAL; i++)
                    throw_slot_enabled[i] = false;

                throw_slot_menu_active = false;
                return;
            }

            log_debug("do_cmd_wield: User selected slot %d for throwing weapon", slot_choice);

            item_tester_hook = NULL;
            item_tester_full = false;
            throw_slot_menu_active = false;

            slot = slot_choice;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;
        }
    }

    move_pack_weapon_to_harness = player_inventory_handle_is_carried(item)
        && object_can_choose_pack_or_harness(o_ptr)
        && o_ptr->storage == OBJECT_STORAGE_PACK;
    if (move_pack_weapon_to_harness)
    {
        int moving_quantity = (forced_wield_full_stack && is_throwing
                && slot == INVEN_WIELD)
            ? MIN(o_ptr->number, object_stack_limit(o_ptr)) : 1;

        if (!item_storage_destination_available(o_ptr,
                OBJECT_STORAGE_HARNESS, moving_quantity, true))
        {
            return;
        }
    }
    // Check for paired weapons (e.g., Glamdring + Orcrist)
    // Paired weapons can be wielded together without Two Weapon Fighting
    bool paired_weapon_prompt = false;
    if ((forced_wield_slot < 0) && o_ptr->name1
        && inventory[INVEN_WIELD].k_idx)
    {
        int paired_idx = get_paired_artefact(o_ptr->name1);
        if (paired_idx && inventory[INVEN_WIELD].name1 == paired_idx)
        {
            // The weapon we're trying to wield is paired with our main hand weapon
            if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
                && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
            {
                if (get_check("Wield alongside its mate in your off-hand? "))
                {
                    slot = INVEN_ARM;
                    paired_weapon_prompt = true;
                }
            }
        }
    }

    // Ask about two weapon fighting if necessary
    for (i = 0; i < o_ptr->abilities; i++)
    {
        if ((o_ptr->skilltype[i] == S_MEL)
            && (o_ptr->abilitynum[i] == MEL_TWO_WEAPON)
            && object_known_p(o_ptr))
        {
            grants_two_weapon = true;
        }
    }
    if ((forced_wield_slot < 0) && !paired_weapon_prompt
        && (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] || grants_two_weapon)
        && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
            || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)))
    {
        if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
            && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
        {
            if (get_check("Do you wish to wield it in your off-hand? "))
            {
                slot = INVEN_ARM;
            }
        }
    }

    if ((item >= INVEN_WIELD) && (item < INVEN_TOTAL) && (item != slot)
        && cursed_p(o_ptr))
    {
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
        msg_format("You cannot bear to move the %s you are %s.", o_name,
            describe_use(item));
        return;
    }

    /* Prevent wielding into a cursed slot */
    if (cursed_p(&inventory[slot]))
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[slot], false, 0);

        /* Message */
        msg_format("You cannot bear to give up the %s you are %s.", o_name,
            describe_use(slot));

        /* Cancel the command */
        return;
    }

    /* Check if Maedhros character is trying to wield a two-handed weapon */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from wielding two-handed weapons.");
        return;
    }

    /* Check if Maedhros character is trying to wield a shield */
    if ((o_ptr->tval == TV_SHIELD)
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from using shields.");
        return;
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        if (cursed_p(&inventory[INVEN_ARM]))
        {
            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                msg_print("You would need to remove your shield, but cannot "
                          "bear to part "
                          "with it.");
            }
            else
            {
                msg_print("You would need to remove your off-hand weapon, but "
                          "cannot bear to "
                          "part with it.");
            }

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            flush();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "shield. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print("This would require removing (and dropping) your "
                          "off-hand weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        if (cursed_p(&inventory[INVEN_WIELD]))
        {
            msg_print("You would need to put down your weapon, but cannot bear "
                      "to part "
                      "with it.");

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            flush();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "weapon. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print(
                    "This would require removing (and dropping) your weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a
     * hand and a half weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (!inventory[INVEN_ARM].k_idx))
    {
        weapon_less_effective = true;
    }

    if (smith_oath_forbids_object(o_ptr) && !smith_oath_confirm_break())
        return;

    if (inventory[slot].k_idx && !combine
        && smith_oath_takeoff_hits_pack(&inventory[slot], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && inventory[INVEN_ARM].k_idx
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_ARM], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((slot == INVEN_ARM)
        && inventory[INVEN_WIELD].k_idx
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED))
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_WIELD], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if (item < 0 && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN)
    {
        bool replacement_aborted = false;

        lamp_flask_replacement_planned =
            prepare_brass_lamp_flask_replacement(o_ptr,
                &lamp_flasks_to_replace, &lamp_flask_oil,
                &replacement_aborted);
        if (replacement_aborted)
            return;

        if (!lamp_flask_replacement_planned
            && player_light_available_capacity(o_ptr) <= 0)
        {
            inventory_menu_group menu_group =
                inventory_menu_group_for_limit_group(
                    inventory_limit_group_for_object(o_ptr));

            (void)inven_carry_okay(o_ptr);
            msg_print("You have no free lamp/flask slots.");
            if (menu_group == INVENTORY_MENU_GROUP_ALL
                || !open_inventory_replacement_menu(menu_group, o_ptr, true,
                    true, "Replace a brass lantern to make room.",
                    &lamp_replacement_item))
            {
                return;
            }
        }
    }

    if (!from_supplies
        && o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
        && o_ptr->timeout > 0 && !lamp_flask_replacement_planned
        && lamp_replacement_item < 0
        && player_lamp_oil_would_overflow_with_bonus(o_ptr->timeout,
            (item < 0) ? 1 : 0)
        && !get_check("Taking this lamp will waste some oil. Proceed? "))
    {
        return;
    }
    
    /* Oath of Light: warn before equipping light-dimming items */
    if (chosen_oath(OATH_LIGHT) && !oath_invalid(OATH_LIGHT))
    {
        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        if ((f2 & TR2_DARKNESS) || (f4 & TR4_UNLIGHT))
        {
            char* prompt = oath_confirmation_prompt(OATH_LIGHT);
            if (!prompt || !prompt[0]) {
                prompt = "This item will dim your light. Break the Oath of Light?";
            }
            
            if (!get_check_oath_multiline(prompt))
            {
                log_trace("do_cmd_wield: Player declined to break Oath of Light for item (tval=%d, sval=%d)", o_ptr->tval, o_ptr->sval);
                return;
            }
            
            p_ptr->oaths_broken |= OATH_LIGHT_FLAG;
            p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
            apply_oath_breaking_curse(OATH_LIGHT);
            metarun_ban_oath(OATH_LIGHT);
            log_trace("do_cmd_wield: Oath of Light broken by equipping light-dimming item");
        }
    }

    if (lamp_flask_replacement_planned)
    {
        if (!commit_brass_lamp_flask_replacement(lamp_flasks_to_replace,
                lamp_flask_oil))
        {
            msg_print("The oil flask could not be replaced.");
            return;
        }
    }
    else if (lamp_replacement_item >= 0
        && lamp_replacement_item != slot)
    {
        if (lamp_replacement_item >= SUPPLIES_INDEX)
        {
            if (!supplies_drop_amount(
                    lamp_replacement_item - SUPPLIES_INDEX, 1))
            {
                msg_print("The brass lantern could not be replaced.");
                return;
            }
        }
        else if (player_inventory_handle_is_carried(lamp_replacement_item))
        {
            inven_drop(lamp_replacement_item, 1);
        }
        else
        {
            msg_print("That lantern cannot be replaced here.");
            return;
        }
    }

    if (!from_supplies && item >= 0)
    {
        free_active_weapon_change =
            player_active_weapon_wield_change_is_free(slot, o_ptr, combine);
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain local object */
    object_copy(i_ptr, o_ptr);
    if (move_pack_weapon_to_harness)
    {
        i_ptr->storage = OBJECT_STORAGE_HARNESS;
    }
    if (i_ptr->storage == OBJECT_STORAGE_HARNESS)
        player_active_weapon_assign_harness_color(i_ptr);

    if (!from_supplies && i_ptr->tval == TV_LIGHT
        && i_ptr->sval == SV_LIGHT_LANTERN)
    {
        player_gain_lamp_oil_with_bonus(i_ptr->timeout, true,
            (item < 0) ? 1 : 0);
        i_ptr->timeout = 0;
    }

    bool target_is_throw_slot = (slot == INVEN_BELT);
    bool ready_full_throwing_stack = forced_wield_full_stack && is_throwing
        && slot == INVEN_WIELD;

    /* A belt is a single sheath/loop.  Active Throwing, unlike ordinary
     * melee wielding, readies the entire source stack in the active hand. */
    if (slot == INVEN_BELT)
    {
        quantity = 1;
    }
    else if ((i_ptr->tval == TV_ARROW) || ready_full_throwing_stack)
    {
        if (combine)
        {
            int stack_limit = object_stack_limit(&inventory[slot]);
            quantity = MIN(o_ptr->number,
                stack_limit - (&inventory[slot])->number);
        }
        else
        {
            int stack_limit = object_stack_limit(i_ptr);
            quantity = MIN(o_ptr->number, stack_limit);
        }
    }
    else
    {
        quantity = 1;
    }

    /* Modify quantity */
    i_ptr->number = quantity;

    /* Decrease the item (from the pack) */
    if (from_supplies)
    {
        supplies_consume_quantity(supply_index, quantity);
    }
    else if (player_inventory_handle_valid(item))
    {
        object_type* carried = player_inventory_object(item);

        log_debug(
            "do_cmd_wield: Before decrease - item=%d, k_idx=%d, ego_pfx=%d, ego_sfx=%d, number=%d",
            item, carried->k_idx, object_ego_prefix(carried),
            object_ego_suffix(carried), carried->number);
        inven_item_increase(item, -quantity);
        inven_item_optimize(item);
        carried = player_inventory_object(item);
        log_debug("do_cmd_wield: After optimize - item=%d, k_idx=%d",
            item, carried ? carried->k_idx : 0);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -quantity);
        floor_item_optimize(0 - item);
    }

    /* Get the wield slot */
    o_ptr = &inventory[slot];
    
    log_debug("do_cmd_wield: Wield slot %d - has k_idx=%d, ego_pfx=%d, ego_sfx=%d",
        slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));

    /* Take off existing item */
    if (o_ptr->k_idx && !combine)
    {
        bool refill_oil_pool_from_takeoff = (item < 0)
            && player_oil_container_object(i_ptr)
            && player_oil_container_object(o_ptr);
        int takeoff_result;

        /*
         * Lights coming from the floor are not counted yet, so reserve them
         * during the takeoff even when swapping within the same carry group.
         * Pack/supplies lights are already counted and only need reservation
         * when the swap crosses carry groups.
         */
        if (slot == INVEN_LITE && player_light_carry_cap(i_ptr) > 0)
        {
            if ((item < 0) || !player_light_share_carry_group(i_ptr, o_ptr))
                player_light_reserve_incoming(i_ptr, i_ptr->number);
            else
                player_light_clear_incoming_reservation();
        }

        log_debug(
            "do_cmd_wield: Taking off existing item from slot %d - k_idx=%d, ego_pfx=%d, ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
        /* Take off existing item */
        takeoff_result = inven_takeoff(slot, 255);
        if (refill_oil_pool_from_takeoff && takeoff_result < 0)
            oil_swap_drop_idx = 0 - takeoff_result;
        player_light_clear_incoming_reservation();
        
        /* Refresh pointer after takeoff */
        o_ptr = &inventory[slot];
        log_debug("do_cmd_wield: After takeoff, slot %d now has k_idx=%d", 
                  slot, o_ptr->k_idx);
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[i_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        /* Take off shield */
        check_pack_overflow();
        (void)inven_takeoff(INVEN_ARM, 255);
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        /* Stop wielding two handed weapon */
        (void)inven_takeoff(INVEN_WIELD, 255);
    }

    /* Combine the new stuff into the equipment */
    if (combine)
    {
        log_debug(
            "do_cmd_wield: Combining - slot %d has k_idx=%d ego_pfx=%d ego_sfx=%d, adding k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
            i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        msg_print(
            "You combine them with some that are already in your quiver.");
        object_absorb(o_ptr, i_ptr);
    }
    /* Wear the new stuff */
    else
    {
        log_debug(
            "do_cmd_wield: Copying to slot %d - source k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        object_copy(o_ptr, i_ptr);
        log_debug(
            "do_cmd_wield: After copy, slot %d now has k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
    }

    if (oil_swap_drop_idx > 0 && oil_swap_drop_idx < o_max
        && o_list[oil_swap_drop_idx].k_idx)
    {
        if (player_refill_lamp_oil_from_container(&o_list[oil_swap_drop_idx])
            > 0)
        {
            p_ptr->redraw |= (PR_LIGHT);
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
        }
    }

    /* Once the player has equipped an item, remember its combat stats forever. */
    o_ptr->ident |= (IDENT_HANDLED);

    /* Increment the equip counter by hand */
    if (!combine)
        p_ptr->equip_cnt++;

    /* Attempt identification immediately upon equipping (before printing message) */
    {
        bool slot_is_quiver1 = (slot == INVEN_QUIVER1);
        bool slot_is_belt = (slot == INVEN_BELT);
        bool belt_grants_bonuses = slot_is_belt && is_throwing;
        bool apply_wield_effects
            = !slot_is_quiver1 && (!slot_is_belt || belt_grants_bonuses);

        if (apply_wield_effects)
        {
            ident_on_wield(o_ptr);

            // activate all of its new abilities
            for (i = 0; i < o_ptr->abilities; i++)
            {
                if (!p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]])
                {
                    p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                        = true;
                    p_ptr->active_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                        = true;
                }
            }
        }
    }

    /* Where is the item now */
    if ((slot == INVEN_WIELD)
        || ((slot == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)))
    {
        act = "You are wielding";
    }
    else if (slot == INVEN_BOW)
    {
        act = "You are shooting with";
    }
    else if (slot == INVEN_LITE)
    {
        act = "Your light source is";
    }
    else if (slot == INVEN_HORN)
    {
        act = "You are carrying";
    }
    else if (slot == INVEN_QUIVER1)
    {
        act = "In your quiver you have";
    }
    else if (slot == INVEN_BELT)
    {
        act = "At your belt you have";
    }
    else
    {
        act = "You are wearing";
    }

    /* Describe the result */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("%s %s (%c).", act, o_name, index_to_label(slot));

    /* Play equip sound */
    {
        int equip_sound = get_equip_sound(o_ptr);
        if (equip_sound >= 0)
            sound(equip_sound);
    }

    // Deal with wielding from the floor
    if (item < 0)
    {
        if (target_is_throw_slot && (quantity < original_quantity)
            && ((i_ptr->tval == TV_ARROW) || is_throwing))
        {
            int floor_idx = 0 - item;
            object_type* floor_ptr = &o_list[floor_idx];

            if (floor_ptr->k_idx && floor_ptr->number > 0)
                py_pickup_aux(floor_idx);
        }

        /* Forget monster */
        o_ptr->held_m_idx = 0;

        /* Forget location */
        o_ptr->iy = o_ptr->ix = 0;

        // Break the truce if picking up an item from the floor
        break_truce(false);

        // Special effects when picking up all the items from the floor
        if (i_ptr->number == original_quantity)
        {
            /* No longer marked */
            o_ptr->marked = false;
        }
    }

    /* Cursed! */
    if (cursed_p(o_ptr))
    {
        /* Warn the player */
        msg_print("You have a bad feeling about this...");

        /* Remove special inscription, if any */
        if (o_ptr->discount >= INSCRIP_NULL)
            o_ptr->discount = 0;

        /* Sense the object if allowed */
        if (o_ptr->discount == 0)
            o_ptr->discount = INSCRIP_CURSED;

        /* The object has been "sensed" */
        o_ptr->ident |= (IDENT_SENSE);
    }

    /* Items with BREAKS_PERMA_CURSE can break the Oath of Fëanor on all equipped items */
    {
        u32b o_f1, o_f2, o_f3, o_f4;
        object_flags4(o_ptr, &o_f1, &o_f2, &o_f3, &o_f4);

        if (o_f4 & TR4_BREAKS_PERMA_CURSE)
        {
            int j;
            bool oath_broken = false;

            /* Check all equipped items for the Oath of Fëanor (perma-curse) */
            for (j = INVEN_WIELD; j < INVEN_TOTAL; j++)
            {
                object_type *eq_ptr = &inventory[j];
                u32b eq_f1, eq_f2, eq_f3;

                if (!eq_ptr->k_idx) continue;
                if (!player_equipment_slot_counts_as_equipped(j)) continue;

                object_flags(eq_ptr, &eq_f1, &eq_f2, &eq_f3);

                if ((eq_f3 & TR3_PERMA_CURSE) && cursed_p(eq_ptr))
                {
                    /* Break the curse - the holy light overcomes the oath */
                    eq_ptr->ident &= ~IDENT_CURSED;
                    oath_broken = true;
                }
            }

            if (oath_broken)
            {
                msg_print("The holy light breaks the Oath of Fëanor!");
            }
        }
    }

    if (weapon_less_effective)
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format(
            "You are no longer able to wield your %s as effectively.", o_name);
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Recalculate mana */
    p_ptr->update |= (PU_MANA);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when wielding a light source */
    if (slot == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    wield_command_succeeded = true;
    if (free_active_weapon_change)
    {
        p_ptr->energy_use = 0;
        p_ptr->previous_action[0] = ACTION_NOTHING;
        player_active_weapon_free_change_commit();
    }

    /* Force immediate sidebar update */
    handle_stuff();
    inven_update_current_pack_limits();

    /*
     * Smithing identification checks depend on the player's current effective
     * skills, so retry now that equipped bonuses have been applied.
     */
    if (player_try_identify_smithing_object(o_ptr, true, 0))
    {
        /* Ensure the newly-identified item (and any resulting bonuses) display immediately. */
        handle_stuff();
    }

    sdl_quick_access_suggest_equipped_item(o_ptr->tval);
}

bool do_cmd_wield_to_slot(
    object_type* default_o_ptr, int default_item, int forced_slot)
{
    int old_forced_slot = forced_wield_slot;
    bool old_forced_full_stack = forced_wield_full_stack;
    bool succeeded;

    forced_wield_slot = forced_slot;
    forced_wield_full_stack = false;
    do_cmd_wield(default_o_ptr, default_item);
    succeeded = wield_command_succeeded;
    forced_wield_slot = old_forced_slot;
    forced_wield_full_stack = old_forced_full_stack;
    return succeeded;
}

bool do_cmd_wield_stack_to_slot(
    object_type* default_o_ptr, int default_item, int forced_slot)
{
    int old_forced_slot = forced_wield_slot;
    bool old_forced_full_stack = forced_wield_full_stack;
    bool succeeded;

    forced_wield_slot = forced_slot;
    forced_wield_full_stack = true;
    do_cmd_wield(default_o_ptr, default_item);
    succeeded = wield_command_succeeded;
    forced_wield_slot = old_forced_slot;
    forced_wield_full_stack = old_forced_full_stack;
    return succeeded;
}

static int jewelry_preset_inventory_slot(int preset_slot)
{
    switch (preset_slot)
    {
    case JEWELRY_PRESET_SLOT_LEFT:
        return INVEN_LEFT;
    case JEWELRY_PRESET_SLOT_RIGHT:
        return INVEN_RIGHT;
    case JEWELRY_PRESET_SLOT_NECK:
        return INVEN_NECK;
    default:
        return -1;
    }
}

static int jewelry_preset_slot_for_inventory(int inventory_slot)
{
    switch (inventory_slot)
    {
    case INVEN_LEFT:
        return JEWELRY_PRESET_SLOT_LEFT;
    case INVEN_RIGHT:
        return JEWELRY_PRESET_SLOT_RIGHT;
    case INVEN_NECK:
        return JEWELRY_PRESET_SLOT_NECK;
    default:
        return -1;
    }
}

static bool jewelry_preset_current_slot_is_settled(int inventory_slot,
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    int preset_slot = jewelry_preset_slot_for_inventory(inventory_slot);

    if (preset_slot < 0 || !target_present[preset_slot])
        return false;

    return jewelry_preset_objects_match(&inventory[inventory_slot],
        targets[preset_slot]);
}

static bool jewelry_preset_targets_available(
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    bool used[INVEN_TOTAL];
    int pack_count = player_pack_entry_count();
    bool* used_pack = pack_count > 0
        ? mem_alloc_array(pack_count, bool) : NULL;
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };

    memset(used, 0, sizeof(used));
    if (used_pack)
        memset(used_pack, 0, (size_t)pack_count * sizeof(*used_pack));

    for (int preset_slot = 0; preset_slot < JEWELRY_PRESET_SLOT_MAX;
         preset_slot++)
    {
        const object_type* target = targets[preset_slot];
        int preferred = jewelry_preset_inventory_slot(preset_slot);
        bool found = false;

        if (!target_present[preset_slot])
            continue;

        if (preferred >= 0 && !used[preferred]
            && jewelry_preset_objects_match(&inventory[preferred], target))
        {
            used[preferred] = true;
            continue;
        }

        for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX && !found; i++)
        {
            int item = jewelry_slots[i];

            if (used[item])
                continue;
            if (jewelry_preset_objects_match(&inventory[item], target))
            {
                used[item] = true;
                found = true;
            }
        }

        for (int ordinal = 0; ordinal < pack_count && !found; ordinal++)
        {
            object_type* o_ptr;

            if (used_pack[ordinal])
                continue;
            o_ptr = player_pack_entry_at(ordinal);
            if (jewelry_preset_objects_match(o_ptr, target))
            {
                used_pack[ordinal] = true;
                found = true;
            }
        }

        if (!found)
        {
            used_pack = mem_free(used_pack);
            return false;
        }
    }

    used_pack = mem_free(used_pack);
    return true;
}

static int jewelry_preset_find_source_for_target(int preset_slot,
    const object_type* target,
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX])
{
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };
    int dest = jewelry_preset_inventory_slot(preset_slot);

    if (dest >= 0 && jewelry_preset_objects_match(&inventory[dest], target))
        return dest;

    for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX; i++)
    {
        int item = jewelry_slots[i];

        if (item == dest)
            continue;
        if (jewelry_preset_current_slot_is_settled(item, targets,
                target_present))
            continue;
        if (jewelry_preset_objects_match(&inventory[item], target))
            return item;
    }

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        object_type* o_ptr = player_pack_entry_at(ordinal);

        if (jewelry_preset_objects_match(o_ptr, target))
            return player_pack_entry_handle_at(ordinal);
    }

    return -1;
}

static bool jewelry_preset_current_jewelry_can_move(
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX],
    const bool target_present[JEWELRY_PRESET_SLOT_MAX], bool report)
{
    int jewelry_slots[JEWELRY_PRESET_SLOT_MAX] = {
        INVEN_LEFT, INVEN_RIGHT, INVEN_NECK
    };

    for (int i = 0; i < JEWELRY_PRESET_SLOT_MAX; i++)
    {
        int item = jewelry_slots[i];
        object_type* o_ptr = &inventory[item];
        if (!o_ptr->k_idx)
            continue;
        if (jewelry_preset_current_slot_is_settled(item, targets,
                target_present))
            continue;
        if (!cursed_p(o_ptr))
            continue;

        if (report)
        {
            char o_name[80];

            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
            msg_format("You cannot bear to give up the %s you are %s.",
                o_name, describe_use(item));
        }
        return false;
    }

    return true;
}

static bool jewelry_preset_can_apply_now(int preset)
{
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX];
    bool target_present[JEWELRY_PRESET_SLOT_MAX];

    if (death_spectator_active() || preset < 0
        || preset >= JEWELRY_PRESET_MAX || !jewelry_preset_is_set(preset))
    {
        return false;
    }

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        targets[slot] = jewelry_preset_object(preset, slot);
        target_present[slot] = targets[slot] && targets[slot]->k_idx;
        if (!target_present[slot])
            return false;
    }

    return jewelry_preset_targets_available(targets, target_present)
        && jewelry_preset_current_jewelry_can_move(
            targets, target_present, false);
}

static void jewelry_preset_display_name(int preset, char* buf, size_t buflen)
{
    const char* name = jewelry_preset_name(preset);

    if (!buf || buflen == 0)
        return;
    if (name && name[0])
        SDL_strlcpy(buf, name, buflen);
    else
        strnfmt(buf, buflen, "Jewelry set %d", preset + 1);
}

bool do_cmd_jewelry_preset_apply(int preset)
{
    const object_type* targets[JEWELRY_PRESET_SLOT_MAX];
    bool target_present[JEWELRY_PRESET_SLOT_MAX];
    bool changed = false;
    char preset_name[JEWELRY_PRESET_NAME_MAX + 20];

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (preset < 0 || preset >= JEWELRY_PRESET_MAX
        || !jewelry_preset_is_set(preset))
    {
        jewelry_preset_display_name(preset, preset_name, sizeof(preset_name));
        msg_format("%s is empty.", preset_name);
        return false;
    }
    jewelry_preset_display_name(preset, preset_name, sizeof(preset_name));

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        targets[slot] = jewelry_preset_object(preset, slot);
        target_present[slot] = targets[slot] && targets[slot]->k_idx;
    }

    if (!target_present[JEWELRY_PRESET_SLOT_LEFT]
        || !target_present[JEWELRY_PRESET_SLOT_RIGHT]
        || !target_present[JEWELRY_PRESET_SLOT_NECK])
    {
        msg_format("%s is incomplete.", preset_name);
        return false;
    }

    if (!jewelry_preset_targets_available(targets, target_present))
    {
        msg_format("You no longer have all the items for %s.", preset_name);
        return false;
    }

    if (!jewelry_preset_current_jewelry_can_move(
            targets, target_present, true))
        return false;

    if (!player_pack_action_completing(PLAYER_PACK_ACTION_JEWELRY_PRESET))
    {
        for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
        {
            int dest = jewelry_preset_inventory_slot(slot);
            int source;

            if (dest < 0 || !target_present[slot]
                || jewelry_preset_objects_match(&inventory[dest],
                    targets[slot]))
            {
                continue;
            }

            source = jewelry_preset_find_source_for_target(slot,
                targets[slot], targets, target_present);
            if (source >= 0
                && player_pack_action_start(
                    PLAYER_PACK_ACTION_JEWELRY_PRESET, source, preset, false,
                    &inventory[source]))
            {
                return true;
            }
        }
    }

    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        int dest = jewelry_preset_inventory_slot(slot);
        int source;

        if (dest < 0 || !target_present[slot])
            continue;

        if (jewelry_preset_objects_match(&inventory[dest], targets[slot]))
            continue;

        source = jewelry_preset_find_source_for_target(slot, targets[slot],
            targets, target_present);
        if (source < 0)
        {
            msg_format("You no longer have all the items for %s.", preset_name);
            return changed;
        }

        do_cmd_wield_to_slot(&inventory[source], source, dest);

        if (!jewelry_preset_objects_match(&inventory[dest], targets[slot]))
        {
            msg_format("%s could not be completed.", preset_name);
            return changed;
        }

        changed = true;
    }

    if (changed)
        msg_format("%s equipped.", preset_name);
    else
        msg_format("%s is already equipped.", preset_name);

    return true;
}

bool do_cmd_jewelry_preset_store(int preset)
{
    char preset_name[JEWELRY_PRESET_NAME_MAX + 20];

    if (preset < 0 || preset >= JEWELRY_PRESET_MAX)
        return false;

    jewelry_preset_display_name(preset, preset_name, sizeof(preset_name));

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (jewelry_preset_is_set(preset)
        && !get_check(format("Replace %s? ", preset_name)))
    {
        return false;
    }

    if (!jewelry_preset_store_current(preset))
    {
        msg_print("Wear two rings and an amulet before saving a jewelry set.");
        return false;
    }

    msg_format("%s saved.", preset_name);
    return true;
}

bool do_cmd_jewelry_preset_clear(int preset)
{
    char preset_name[JEWELRY_PRESET_NAME_MAX + 20];

    if (preset < 0 || preset >= JEWELRY_PRESET_MAX)
        return false;

    jewelry_preset_display_name(preset, preset_name, sizeof(preset_name));

    if (!jewelry_preset_is_set(preset))
    {
        msg_format("%s is already empty.", preset_name);
        return false;
    }

    if (!get_check(format("Clear %s? ", preset_name)))
        return false;

    jewelry_preset_clear(preset);
    msg_format("%s cleared.", preset_name);
    return true;
}

void do_cmd_jewelry_preset_shortcut(void)
{
    ui_question_option options[JEWELRY_PRESET_MAX];
    char labels[JEWELRY_PRESET_MAX][JEWELRY_PRESET_NAME_MAX + 24];
    int choice;

    for (int i = 0; i < JEWELRY_PRESET_MAX; i++)
    {
        bool available = jewelry_preset_can_apply_now(i);

        char preset_name[JEWELRY_PRESET_NAME_MAX + 20];

        jewelry_preset_display_name(i, preset_name, sizeof(preset_name));
        strnfmt(labels[i], sizeof(labels[i]), "%s%s", preset_name,
            jewelry_preset_is_set(i)
                ? (available ? "" : " (unavailable)") : " (empty)");
        options[i].key = (char)('1' + i);
        options[i].label = labels[i];
        options[i].attr = TERM_L_WHITE;
        options[i].disabled = !available;
    }

    choice = ui_question_ask("Wear which jewelry set?",
        "Grey choices cannot be worn right now.", options,
        JEWELRY_PRESET_MAX, UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 0);
    if (choice < 0)
        return;

    (void)do_cmd_jewelry_preset_apply(choice);
}

/*
 * Take off an item
 */
void do_cmd_takeoff(object_type* default_o_ptr, int default_item)
{
    int item;
    bool can_break_curse;

    object_type* o_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        item = default_item;
    }
    /* Get an item */
    else
    {
        q = "Remove which item? ";
        s = "You are not wearing anything to remove.";
        if (!open_inventory_item_select_menu(USE_EQUIP, q, s, &item))
            return;
    }

    if (!player_inventory_handle_is_equipped(item))
    {
        log_warn("do_cmd_takeoff: refusing non-equipment item handle %d", item);
        return;
    }

    o_ptr = player_inventory_object(item);
    if (!o_ptr || !o_ptr->k_idx || o_ptr->number <= 0)
    {
        log_warn("do_cmd_takeoff: refusing empty or unavailable slot %d", item);
        return;
    }
    if (default_o_ptr && default_o_ptr != o_ptr)
    {
        log_warn("do_cmd_takeoff: refusing stale object pointer for slot %d",
            item);
        return;
    }

    if (player_pack_action_start(PLAYER_PACK_ACTION_TAKEOFF, item, 0, false,
            o_ptr))
        return;

    can_break_curse = p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING];

    if (((item == INVEN_QUIVER1) || (item == INVEN_BELT)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }
    else if (cursed_p(o_ptr) && can_break_curse)
    {
        {
            object_type carry_preview;
            object_copy(&carry_preview, o_ptr);
            carry_preview.ident &= ~(IDENT_CURSED);
            carry_preview.ident |= IDENT_UNCURSED;

            if (carry_preview.discount >= INSCRIP_NULL)
                carry_preview.discount = 0;

            if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(&carry_preview)
                && !smith_oath_confirm_break())
            {
                return;
            }

            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
    }
    else if (cursed_p(o_ptr))
    {
        /* Oops */
        msg_print("You cannot bear to part with it.");

        /* Nope */
        return;
    }
    else if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(o_ptr)
        && !smith_oath_confirm_break())
    {
        return;
    }


    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get unequip sound before taking off (since o_ptr may be modified) */
    int unequip_sound = get_unequip_sound(o_ptr);

    /* Take off the item */
    (void)inven_takeoff(item, 255);

    /* Play unequip sound */
    if (unequip_sound >= 0)
        sound(unequip_sound);

    /* Deal with wielding of shield when already wielding a hand and a half
     * weapon
     */
    if ((item == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3
            & (TR3_HAND_AND_A_HALF)))
    {
        char o_name[80];

        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format("You can now wield your %s more effectively.", o_name);
    }

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when removing a light source */
    if (item == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    /* Force immediate sidebar update */
    handle_stuff();
    inven_update_current_pack_limits();
}

static bool confirm_drop_item_amount(object_type* o_ptr, int amt)
{
    object_type prompt_obj;
    char prompt_name[80];
    char prompt[120];

    if (!o_ptr || !o_ptr->k_idx || amt <= 0)
        return false;

    object_copy(&prompt_obj, o_ptr);
    prompt_obj.number = amt;
    object_desc(prompt_name, sizeof(prompt_name), &prompt_obj, false, 0);
    strnfmt(prompt, sizeof(prompt), "Drop %s? ", prompt_name);
    return get_check(prompt);
}

/*
 * Drop an item by index (for enhanced menus).  Returns false if the player
 * cancels quantity or confirmation prompts before anything is dropped.
 */
bool do_cmd_drop_item_by_index_confirm(int item, bool confirm)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return true;
    }

    int amt;
    object_type* o_ptr;
    char o_name[80];
    char quantity_prompt[160];

    if (item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
    {
        object_type dropped;

        o_ptr = player_quiver_arrow_object(item);
        if (!o_ptr || !o_ptr->k_idx)
            return false;
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
        strnfmt(quantity_prompt, sizeof(quantity_prompt),
            "Drop how many %s? ", o_name);
        amt = get_quantity_action(quantity_prompt, "Drop", o_ptr->number);
        if (amt <= 0)
            return false;
        if (confirm && !confirm_drop_item_amount(o_ptr, amt))
            return false;

        object_copy(&dropped, o_ptr);
        dropped.number = amt;
        dropped.pickup = false;
        dropped.pickup_slot = -1;
        object_desc(o_name, sizeof(o_name), &dropped, true, 3);
        player_quiver_remove_arrows(item, amt);
        msg_format("You drop %s (quiver).", o_name);
        drop_near(&dropped, 0, p_ptr->py, p_ptr->px);
        p_ptr->energy_use = 50;
        p_ptr->redraw |= PR_QUIVER;
        return true;
    }

    /* Paranoia */
    if (!player_inventory_handle_valid(item))
        return false;

    /* Get the item */
    o_ptr = player_inventory_object(item);

    /* Nothing there */
    if (!o_ptr->k_idx)
        return false;

    /* Get a quantity */
    if (player_pack_action_completing(PLAYER_PACK_ACTION_DROP))
    {
        amt = player_pack_action_completion_arg();
    }
    else
    {
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
        strnfmt(quantity_prompt, sizeof(quantity_prompt), "Drop how many %s? ",
            o_name);
        amt = get_quantity_action(quantity_prompt, "Drop", o_ptr->number);
    }

    /* Allow user abort */
    if (amt <= 0)
        return false;

    if (((item == INVEN_QUIVER1) || (item == INVEN_BELT)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return false;
    }

    if (!player_pack_action_completing(PLAYER_PACK_ACTION_DROP)
        && confirm && !confirm_drop_item_amount(o_ptr, amt))
        return false;

    if (player_inventory_handle_is_equipped(item) && cursed_p(o_ptr)
        && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
    {
        msg_print("You cannot bear to part with it.");
        return false;
    }

    if (player_pack_action_start(PLAYER_PACK_ACTION_DROP, item, amt, false,
            o_ptr))
    {
        return true;
    }

    /* Hack -- Cannot remove cursed items */
    if (player_inventory_handle_is_equipped(item) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return false;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 50;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    return true;
}

/*
 * Drop an item by index (for enhanced menus)
 */
void do_cmd_drop_item_by_index(int item)
{
    (void)do_cmd_drop_item_by_index_confirm(item, false);
}

/*
 * Drop an item
 */
void do_cmd_drop(void)
{
    int item;

    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
            "Drop which item?", "You have nothing to drop.", &item))
    {
        return;
    }

    do_cmd_drop_item_by_index(item);
}

/*
 * An "item_tester_hook" for destroying objects
 */
static bool item_tester_hook_destroy(const object_type* o_ptr)
{
    if (o_ptr) { } // suppresses warnings about this function

    return (true);
}

/*
 *  Shatter the player's wielded weapon.
 */
void shatter_weapon(int silnum)
{
    int i;
    object_type* w_ptr = &inventory[INVEN_WIELD];
    char w_name[80];
    int anger_level;

    log_debug("shatter_weapon: called for silmaril #%d", silnum);
    
    /* Set the appropriate shatter flag for this silmaril */
    if (silnum == 2)
    {
        p_ptr->crown_shatter_sil2 = true;
        log_debug("shatter_weapon: set crown_shatter_sil2 = true");
    }
    else if (silnum == 3)
    {
        p_ptr->crown_shatter_sil3 = true;
        log_debug("shatter_weapon: set crown_shatter_sil3 = true");
    }

    /* Get the basic name of the object */
    object_desc(w_name, sizeof(w_name), w_ptr, false, 0);

    if (silnum == 2)
        msg_print(
            "You strive to free a second Silmaril, but it is not fated to be.");
    else
        msg_print(
            "You strive to free a third Silmaril, but it is not fated to be.");

    msg_format(
        "As you strike the crown, your %s shatters into innumerable pieces.",
        w_name);

    // make more noise
    stealth_score -= 5;

    inven_item_increase(INVEN_WIELD, -1);
    inven_item_optimize(INVEN_WIELD);

    /* Determine anger level based on which Silmaril (2nd = state 3, 3rd = state 4) */
    anger_level = (silnum == 2) ? 3 : 4;

    log_debug("shatter_weapon: anger_level=%d for silmaril #%d", anger_level, silnum);

    /* Process monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* If Morgoth, then anger him */
        if (m_ptr->r_idx == R_IDX_MORGOTH)
        {
            log_debug("shatter_weapon: found Morgoth at (%d,%d), cdis=%d, alertness=%d",
                     m_ptr->fy, m_ptr->fx, m_ptr->cdis, m_ptr->alertness);
            
            if ((m_ptr->cdis <= 5)
                && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
            {
                log_debug("shatter_weapon: Morgoth sees shard strike, calling anger_morgoth(%d)", 
                         anger_level);
                msg_print("A shard strikes Morgoth upon his cheek.");
                set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
                anger_morgoth(anger_level);
            }
            else
            {
                log_debug("shatter_weapon: Morgoth doesn't see/is too far");
            }
        }
    }
}

static void prise_silmaril(void)
{
    object_type* o_ptr;
    object_type* w_ptr;
    artefact_type* a_ptr;

    object_type object_type_body;

    cptr freed_msg = NULL; // default to soothe compiler warnings

    bool freed = false;

    int slot = 0;

    int dam = 0;
    int prt = 0;
    int net_dam = 0;
    int prt_percent = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0;
    int pd = 10;
    int noise = 0;
    u32b dummy_noticed_flag;

    int mds = p_ptr->mds;
    int attack_mod = p_ptr->skill_use[S_MEL];

    char o_name[80];

    // the Crown is on the ground
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

    log_debug("prise_silmaril: attempting to prise silmaril from crown artifact %d", 
             o_ptr->name1);
    log_debug("prise_silmaril: current morgoth_state=%d, silmarils_possessed=%d",
             p_ptr->morgoth_state, silmarils_possessed());

    switch (o_ptr->name1)
    {
    case ART_MORGOTH_3:
    {
        noise = 5;
        freed_msg = "You have freed a Silmaril!";
        break;
    }
    case ART_MORGOTH_2:
    {
        noise = 10;

        if (p_ptr->crown_shatter)
            freed_msg = "The fates be damned! You free a second Silmaril.";
        else
            freed_msg = "You free a second Silmaril.";

        msg_print(
            "As you reach for the second jewel, you feel the weight of "
            "Morgoth's wrath pressing upon you.");
        msg_print(
            "To take another Silmaril will kindle a fury beyond measure.");
        if (!get_check("Will you dare to claim it? "))
            return;

        break;
    }
    case ART_MORGOTH_1:
    {
        noise = 15;

        freed_msg
            = "You free the final Silmaril. You have a very bad feeling about "
              "this.";

        msg_print(
            "Looking into the hallowed light of the final Silmaril, you are "
            "filled with a strange dread.");
        if (!get_check("Are you sure you wish to proceed? "))
            return;

        break;
    }
    }

    /* Get the weapon */
    w_ptr = &inventory[INVEN_WIELD];

    // undo rapid attack penalties
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        // undo strength adjustment to the attack
        mds = total_mds(w_ptr, 0);

        // undo the dexterity adjustment to the attack
        attack_mod += 3;
    }

    /* Test for hit */
    hit_result = hit_roll(attack_mod, 0, PLAYER, NULL, true);

    /* Make some noise */
    stealth_score -= noise;

    // Determine damage
    if (hit_result > 0)
    {
        crit_bonus_dice = crit_bonus(hit_result, w_ptr->weight,
            &r_info[R_IDX_MORGOTH], S_MEL, false, NULL, w_ptr);

        dam = damroll(p_ptr->mdd + crit_bonus_dice, mds);
        prt = damroll(pd, 4);

        prt_percent = prt_after_sharpness(w_ptr, &dummy_noticed_flag);

        if (prt_percent < 0)
        {
            prt_percent = 0;
        }

        prt = (prt * prt_percent) / 100;
        net_dam = dam - prt;

        /* No negative damage */
        if (net_dam < 0)
            net_dam = 0;

        // update_combat_rolls1b(PLAYER, true);
        update_combat_rolls2(p_ptr->mdd + crit_bonus_dice, mds, dam, pd, 4, prt,
            prt_percent, GF_HURT, true);
    }

    // if you succeed in prising out a Silmaril...
    if (net_dam > 0)
    {
        freed = true;

        switch (o_ptr->name1)
        {
        case ART_MORGOTH_3:
        {
            /* Process monsters - anger Morgoth when 1st Silmaril is taken */
            for (int i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr = &mon_list[i];

                /* If Morgoth, then anger him to state 2 for 1st Silmaril */
                if (m_ptr->r_idx == R_IDX_MORGOTH
                    && m_ptr->alertness >= ALERTNESS_ALERT)
                {
                    log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                             m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                    
                    if ((m_ptr->cdis <= 5)
                        && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                    {
                        log_debug("prise_silmaril: Morgoth sees 1st silmaril taken, calling anger_morgoth(2)");
                        msg_print("Morgoth roars in fury!");
                        anger_morgoth(2);
                    }
                    else
                    {
                        log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_2:
        {
            /* 50% chance to shatter if not already shattered on 2nd silmaril */
            if (!p_ptr->crown_shatter_sil2 && one_in_(2))
            {
                log_debug("prise_silmaril: 2nd silmaril shatter check failed (50%%), calling shatter_weapon(2)");
                shatter_weapon(2);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 2nd silmaril - no shatter (already_shattered=%d)", 
                         p_ptr->crown_shatter_sil2);
                
                /* Process monsters */
                for (int i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr = &mon_list[i];

                    /* If Morgoth, then anger him to state 3 for 2nd Silmaril */
                    if (m_ptr->r_idx == R_IDX_MORGOTH
                        && m_ptr->alertness >= ALERTNESS_ALERT)
                    {
                        log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                                 m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                        
                        if ((m_ptr->cdis <= 5)
                            && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                        {
                            log_debug("prise_silmaril: Morgoth sees 2nd silmaril taken, calling anger_morgoth(3)");
                            msg_print("Morgoth howls with rage!");
                            anger_morgoth(3);
                        }
                        else
                        {
                            log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                        }
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_1:
        {
            /* 100% shatter on 3rd silmaril if not already shattered on 3rd */
            if (!p_ptr->crown_shatter_sil3)
            {
                log_debug("prise_silmaril: 3rd silmaril shatter check (100%%), calling shatter_weapon(3)");
                shatter_weapon(3);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 3rd silmaril - no shatter (already_shattered=%d), but cursed!",
                         p_ptr->crown_shatter_sil3);
                p_ptr->cursed = true;
            }
            break;
        }
        }

        if (freed)
        {
            // change its type to that of the crown with one less silmaril
            o_ptr->name1--;

            // get the details of this new crown
            a_ptr = &a_info[o_ptr->name1];

            // modify the existing crown
            object_into_artefact(o_ptr, a_ptr);

            // report success
            msg_print(freed_msg);

            // Get new local object
            o_ptr = &object_type_body;

            // Make Silmaril
            object_prep(o_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_SILMARIL));

            // Get it
            slot = inven_carry(o_ptr, false);

            if (slot == SUPPLIES_INDEX)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                char label = supplies_label_char();
                if (!label)
                    label = 'a';
                msg_format("You add %s to your supplies (%c).", o_name, label);
            }
            else if (slot >= 0)
            {
                /* Get the object again */
                o_ptr = player_inventory_object(slot);
                if (!o_ptr || !o_ptr->k_idx)
                {
                    log_warn("Silmaril pickup returned an invalid inventory handle: %d",
                        slot);
                    return;
                }

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", o_name,
                    player_inventory_label(slot));
            }
            else
            {
                /* Inventory full - find best adjacent square for Silmaril */
                int dy, dx;
                int best_y = p_ptr->py;
                int best_x = p_ptr->px;
                int backup_y = -1;
                int backup_x = -1;
                bool found_ideal = false;
                bool found_backup = false;
                
                /* First pass: try to find square with no items AND no monsters */
                for (dy = -1; dy <= 1; dy++)
                {
                    for (dx = -1; dx <= 1; dx++)
                    {
                        int ty = p_ptr->py + dy;
                        int tx = p_ptr->px + dx;
                        
                        /* Skip center */
                        if (dy == 0 && dx == 0) continue;
                        
                        /* Check if square is valid, empty floor, no objects, no monsters */
                        if (in_bounds_fully(ty, tx) && 
                            cave_clean_bold(ty, tx) && 
                            cave_m_idx[ty][tx] == 0)
                        {
                            best_y = ty;
                            best_x = tx;
                            found_ideal = true;
                            break;
                        }
                        /* Backup: empty floor with no objects (but monster might be there) */
                        else if (!found_backup && in_bounds_fully(ty, tx) && cave_clean_bold(ty, tx))
                        {
                            backup_y = ty;
                            backup_x = tx;
                            found_backup = true;
                        }
                    }
                    if (found_ideal) break;
                }
                
                /* Use backup square if no ideal square found */
                if (!found_ideal && found_backup)
                {
                    best_y = backup_y;
                    best_x = backup_x;
                    log_debug("prise_silmaril: no monster-free square, using backup at (%d,%d)", best_y, best_x);
                }
                
                /* Drop the Silmaril */
                if (found_ideal)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (no items, no monsters)", 
                             best_y, best_x);
                }
                else if (found_backup)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (WARNING: monster may be present)", 
                             best_y, best_x);
                }
                else
                {
                    log_debug("prise_silmaril: inventory full, no adjacent empty square, using drop_near fallback");
                }
                
                drop_near(o_ptr, 0, best_y, best_x);
                
                /* Describe what we dropped */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                msg_format("You have no room, so %s drops to the floor.", o_name);
            }

            // Break the truce (always)
            break_truce(true);

            // add a note to the notes file
            do_cmd_note("Cut a Silmaril from Morgoth's crown", p_ptr->depth);
        }
    }

    // if you fail to prise out a Silmaril...
    else
    {
        msg_print("Try though you might, you were unable to free a Silmaril.");

        // Break the truce if creatures see
        break_truce(false);
    }

    // check for taking of final Silmaril
    if (o_ptr->name1 == ART_MORGOTH_0)
    {
        log_debug("prise_silmaril: final silmaril taken! Calling anger_morgoth(4)");
        msg_print("You hear a cry of vengeance echo through the iron hells.");
        msg_print("You feel your doom awaiting you.");
        wake_all_monsters(0);
        anger_morgoth(4);  // Final Silmaril pushes Morgoth to desperate state
    }
    
    log_debug("prise_silmaril: complete, freed=%s, final morgoth_state=%d", 
             freed ? "true" : "false", p_ptr->morgoth_state);
}

bool do_cmd_delete_item_by_index(int item)
{
    int amt;
    int old_number;
    int old_charges = 0;

    object_type* o_ptr;

    char o_name[80];
    char prompt_name[80];
    char quantity_name[80];
    char quantity_prompt[160];
    char prompt[160];

    /* Get the item (in the pack) */
    if (player_inventory_handle_valid(item))
    {
        o_ptr = player_inventory_object(item);
    }

    /* Get the item (on the floor) */
    else
    {
        int o_idx = 0 - item;

        if (o_idx <= 0 || o_idx >= o_max)
            return false;
        o_ptr = &o_list[o_idx];
    }

    if (!o_ptr->k_idx)
        return false;

    if (handle_iron_crown_silmaril_action(o_ptr, item))
        return true;

    /* Get a quantity */
    if (player_pack_action_completing(PLAYER_PACK_ACTION_DELETE))
    {
        amt = player_pack_action_completion_arg();
    }
    else
    {
        if (item < 0)
            object_desc_floor(quantity_name, sizeof(quantity_name), o_ptr,
                false, 0);
        else
            object_desc(quantity_name, sizeof(quantity_name), o_ptr, false, 0);
        strnfmt(quantity_prompt, sizeof(quantity_prompt),
            "Delete how many %s? ", quantity_name);
        amt = get_quantity_action(quantity_prompt, "Delete", o_ptr->number);
    }

    /* Allow user abort */
    if (amt <= 0)
        return false;

    /* Describe the object */
    old_number = o_ptr->number;

    /* Hack, state the correct number of charges to be destroyed if staff*/
    if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
    {
        /*save the number of charges*/
        old_charges = o_ptr->pval;

        /*distribute the charges*/
        o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

        o_ptr->pval = old_charges - o_ptr->pval;
    }

    /*hack -  make sure we get the right amount displayed*/
    o_ptr->number = amt;

    /*now describe with correct amount*/
    if (item < 0)
    {
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        object_desc_floor(prompt_name, sizeof(prompt_name), o_ptr, false, 0);
    }
    else
    {
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        object_desc(prompt_name, sizeof(prompt_name), o_ptr, false, 0);
    }

    /*reverse the hack*/
    o_ptr->number = old_number;

    strnfmt(prompt, sizeof(prompt), "Do you really want to DELETE %s? ",
        prompt_name);
    if (!player_pack_action_completing(PLAYER_PACK_ACTION_DELETE)
        && !get_check(prompt))
    {
        if (old_charges)
            o_ptr->pval = old_charges;
        return false;
    }

    if (old_charges)
        o_ptr->pval = old_charges;

    /* Deleting an item already on the floor is not Pack management. */
    if (item >= 0
        && player_pack_action_start(PLAYER_PACK_ACTION_DELETE, item, amt,
            false, o_ptr))
    {
        return true;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Message */
    msg_format("You delete %s.", o_name);

    /* Eliminate the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -amt);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Eliminate the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -amt);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    return true;
}

/*
 * Destroy an item
 */
void do_cmd_destroy(void)
{
    int item;
    object_type* o_ptr;

    item_tester_hook = item_tester_hook_destroy;

    // Special case for prising Silmarils from the Iron Crown of Morgoth
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    if (handle_iron_crown_silmaril_action(o_ptr, -1))
        return;

    if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR,
            "Delete which item?",
            "You have nothing to delete.", &item))
    {
        return;
    }

    (void)do_cmd_delete_item_by_index(item);
}

static bool floor_context_show_details(int floor_item)
{
    extern char current_menu_command;
    extern int current_menu_state;
    floor_context_action_kind kind;
    char key;
    int o_idx = 0 - floor_item;

    if (floor_item >= 0 || o_idx <= 0 || o_idx >= o_max
        || !o_list[o_idx].k_idx || o_list[o_idx].iy != p_ptr->py
        || o_list[o_idx].ix != p_ptr->px)
    {
        return false;
    }

    current_menu_command = 'x';
    current_menu_state = 0;
    key = describe_item_with_floor_actions(floor_item, true);
    current_menu_command = 0;
    current_menu_state = 0;

    if (!key || key == ESCAPE)
        return true;
    if (key == 'x')
        kind = FLOOR_CONTEXT_ACTION_USE;
    else if (key == ' ')
        kind = FLOOR_CONTEXT_ACTION_PICKUP_CONTEXT;
    else if (!floor_context_action_for_key(floor_item, key, &kind))
        return false;
    if (kind == FLOOR_CONTEXT_ACTION_CLOSE)
        return true;
    return floor_context_perform_action(floor_item, kind);
}

static bool item_tester_hook_floor_context(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && !object_is_searched_skeleton(o_ptr);
}

static bool item_tester_hook_floor_context_pickup(const object_type* o_ptr)
{
    return item_tester_hook_floor_context(o_ptr)
        && o_ptr->tval != TV_SKELETON && o_ptr->tval != TV_CHEST;
}

static bool floor_context_select_item(int* floor_item, bool pickup_only)
{
    bool old_item_tester_full = item_tester_full;

    if (!floor_item)
        return false;
    *floor_item = 0;

    item_tester_tval = 0;
    item_tester_hook = pickup_only
        ? item_tester_hook_floor_context_pickup
        : item_tester_hook_floor_context;
    item_tester_full = false;
    if (!open_inventory_item_select_menu(USE_FLOOR,
            "Choose a floor item: ", "There is nothing here.", floor_item))
    {
        item_tester_full = old_item_tester_full;
        return false;
    }
    item_tester_full = old_item_tester_full;

    return true;
}

static bool floor_context_choose_item(void)
{
    int floor_item;

    if (!floor_context_select_item(&floor_item, false))
        return false;
    return floor_context_show_details(floor_item);
}

static bool floor_context_is_pickup_destination(
    floor_context_action_kind kind)
{
    return kind == FLOOR_CONTEXT_ACTION_PACK
        || kind == FLOOR_CONTEXT_ACTION_HARNESS
        || kind == FLOOR_CONTEXT_ACTION_SUPPLIES
        || kind == FLOOR_CONTEXT_ACTION_QUIVER
        || kind == FLOOR_CONTEXT_ACTION_JEWELRY
        || kind == FLOOR_CONTEXT_ACTION_PICKUP;
}

static bool floor_context_pickup(int floor_item)
{
    floor_context_action actions[FLOOR_CONTEXT_MAX_ACTIONS];
    floor_context_action destinations[FLOOR_CONTEXT_MAX_ACTIONS];
    ui_question_option options[FLOOR_CONTEXT_MAX_ACTIONS];
    object_type* o_ptr;
    char o_name[120];
    char desc[180];
    int destination_count = 0;
    int default_index = 0;
    int count;
    int choice;

    if (floor_item == 0)
    {
        int first_item = 0;
        int item_count = floor_context_first_item_and_count(&first_item);

        if (item_count == 1)
            floor_item = first_item;
        else if (item_count <= 0
            || !floor_context_select_item(&floor_item, true))
        {
            return false;
        }
    }
    if (floor_item >= 0 || 0 - floor_item <= 0 || 0 - floor_item >= o_max)
        return false;

    o_ptr = &o_list[0 - floor_item];
    if (!o_ptr->k_idx || o_ptr->iy != p_ptr->py || o_ptr->ix != p_ptr->px)
        return false;

    count = floor_context_collect_item_actions(floor_item, false, false,
        actions, (int)N_ELEMENTS(actions));
    for (int i = 0; i < count; i++)
    {
        if (!floor_context_is_pickup_destination(actions[i].kind))
            continue;
        destinations[destination_count] = actions[i];
        options[destination_count] = (ui_question_option){
            (char)actions[i].key, destinations[destination_count].label,
            actions[i].attr, false
        };
        if (actions[i].kind == FLOOR_CONTEXT_ACTION_PACK)
            default_index = destination_count;
        destination_count++;
    }

    if (destination_count <= 0)
        return false;
    if (destination_count == 1)
    {
        return floor_context_perform_action(floor_item,
            destinations[0].kind);
    }

    object_desc_floor(o_name, sizeof(o_name), o_ptr, false, 0);
    strnfmt(desc, sizeof(desc), "Choose where to put %s.", o_name);
    choice = ui_question_ask("Pick up where?", desc, options,
        destination_count, p_ptr->py, p_ptr->px, default_index);
    if (choice < 0 || choice >= destination_count)
        return false;

    return floor_context_perform_action(floor_item,
        destinations[choice].kind);
}

/* Equipping a Belt weapon from the floor is distinct from merely picking it
 * up.  The active destination must use the active-throwing transition so the
 * full stack is readied and the weapon mode changes; the Belt remains a
 * one-item equipped destination. */
static bool floor_context_equip_belt_weapon(object_type* o_ptr,
    int floor_item)
{
    bool enabled[INVEN_TOTAL] = { false };
    int destination_count = 0;
    int slot = -1;

    if (!o_ptr || !o_ptr->k_idx || !object_is_belt_weapon(o_ptr)
        || floor_item >= 0)
    {
        return false;
    }

    enabled[INVEN_WIELD] = true;
    enabled[INVEN_BELT] = true;
    destination_count = 2;

    if (inventory[INVEN_WIELD].k_idx && cursed_p(&inventory[INVEN_WIELD])
        && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
    {
        enabled[INVEN_WIELD] = false;
        destination_count--;
    }
    if (inventory[INVEN_BELT].k_idx && cursed_p(&inventory[INVEN_BELT])
        && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
    {
        enabled[INVEN_BELT] = false;
        destination_count--;
    }

    if (destination_count <= 0)
    {
        msg_print("You have no available slot for that throwing weapon.");
        return true;
    }
    if (destination_count == 1)
        slot = enabled[INVEN_WIELD] ? INVEN_WIELD : INVEN_BELT;
    else if (!open_inventory_slot_pick_menu(o_ptr, enabled,
            "Equip this throwing weapon: the active Harness weapon is "
            "readied to throw, while the belt holds one dagger or hand axe.",
            &slot))
    {
        return true;
    }

    if (slot == INVEN_WIELD)
        return player_ready_throwing_weapon(o_ptr, floor_item);
    if (slot == INVEN_BELT)
        return do_cmd_wield_to_slot(o_ptr, floor_item, INVEN_BELT);
    return false;
}

bool floor_context_perform_action(int floor_item,
    floor_context_action_kind kind)
{
    object_type* o_ptr;
    int o_idx;

    if (kind == FLOOR_CONTEXT_ACTION_ITEMS)
        return floor_context_choose_item();
    if (kind == FLOOR_CONTEXT_ACTION_PICKUP_CONTEXT)
        return floor_context_pickup(floor_item);
    if (kind == FLOOR_CONTEXT_ACTION_CLOSE)
        return true;

    o_idx = 0 - floor_item;
    if (floor_item >= 0 || o_idx <= 0 || o_idx >= o_max)
        return false;
    o_ptr = &o_list[o_idx];
    if (!o_ptr->k_idx || o_ptr->iy != p_ptr->py || o_ptr->ix != p_ptr->px)
        return false;

    switch (kind)
    {
    case FLOOR_CONTEXT_ACTION_DETAILS:
        return floor_context_show_details(floor_item);
    case FLOOR_CONTEXT_ACTION_USE:
    {
        extern char current_menu_command;
        extern int current_menu_state;

        if (object_is_belt_weapon(o_ptr))
            return floor_context_equip_belt_weapon(o_ptr, floor_item);

        current_menu_command = 'u';
        current_menu_state = 0;
        do_cmd_use_item_by_index(floor_item);
        current_menu_command = 0;
        current_menu_state = 0;
        return true;
    }
    case FLOOR_CONTEXT_ACTION_READY_THROW:
        return player_ready_throwing_weapon(o_ptr, floor_item);
    case FLOOR_CONTEXT_ACTION_QUIVER:
        return do_cmd_wield_to_slot(o_ptr, floor_item, INVEN_QUIVER1);
    case FLOOR_CONTEXT_ACTION_PACK:
    case FLOOR_CONTEXT_ACTION_HARNESS:
    case FLOOR_CONTEXT_ACTION_SUPPLIES:
    case FLOOR_CONTEXT_ACTION_JEWELRY:
    case FLOOR_CONTEXT_ACTION_PICKUP:
        p_ptr->previous_action[0] = ACTION_MISC;
        p_ptr->energy_use = 100;
        if (kind == FLOOR_CONTEXT_ACTION_PACK)
            py_pickup_aux_to_pack(o_idx);
        else if (kind == FLOOR_CONTEXT_ACTION_HARNESS)
            py_pickup_aux_to_harness(o_idx);
        else
            py_pickup_aux(o_idx);
        return true;
    case FLOOR_CONTEXT_ACTION_NONE:
    case FLOOR_CONTEXT_ACTION_PICKUP_CONTEXT:
    case FLOOR_CONTEXT_ACTION_ITEMS:
    case FLOOR_CONTEXT_ACTION_CLOSE:
    default:
        return false;
    }
}

/*
 * Observe an item, displaying what is known about it
 */
void do_cmd_observe(void)
{
    supply_menu_request request = {0};
    int floor_item = first_floor_item_under_player();

    {
        extern char current_menu_command;
        extern int current_menu_state;

        current_menu_command = 0;
        current_menu_state = 0;
    }

    /* Shortcut: if standing on an item, expose its complete item-specific
     * action set instead of opening the general inventory browser. */
    if (floor_item != 0)
    {
        log_debug(
            "do_cmd_observe: Examining floor item under player, item=%d",
            floor_item);
        (void)floor_context_show_details(floor_item);
        return;
    }

    log_debug("do_cmd_observe: Opening inventory browser preview");
    request.focus_page = true;
    request.page = SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_inventory_group = true;
    request.inventory_group = INVENTORY_MENU_GROUP_ALL;
    request.preview_inventory_description = true;
    (void)do_cmd_knowledge_supplies(&request);
}

/*
 * Enhanced observe command that supports cycling between inventory/equipment
 */
void do_cmd_observe_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_observe_enhanced: Starting enhanced observe cycle");
    
    /* Clear any active banner before starting enhanced menu cycle */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        log_trace("do_cmd_observe_enhanced: Loop iteration, current_menu_state=%d", current_menu_state);
        
        if (current_menu_state == 0) {
            log_trace("do_cmd_observe_enhanced: Displaying inventory");
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            log_trace("do_cmd_observe_enhanced: After inventory, enhanced_menu_action=%d", enhanced_menu_action);
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_observe_enhanced: Switching to equipment");
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (inven action=%d)", enhanced_menu_action);
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_observe_enhanced: Displaying equipment, current_menu_state=%d", current_menu_state);
            do_cmd_equip();
            log_trace("do_cmd_observe_enhanced: Returned from equipment, current_menu_state=%d", current_menu_state);
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            log_trace("do_cmd_observe_enhanced: After equipment, enhanced_equip_action=%d", enhanced_equip_action);
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_observe_enhanced: Switching to inventory");
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (equip action=%d)", enhanced_equip_action);
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Helper function which actually removes the inscription
 */
void uninscribe(object_type* o_ptr)
{
    /* Remove the inscription */
    o_ptr->obj_note = 0;

    /*The object kind has an autoinscription*/
    // Sil-y: removed restriction to known items (through 'object_aware')
    if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART))
        && (get_autoinscription_index(o_ptr->k_idx) != -1))
    {
        char tmp_val[160];
        char o_name2[80];

        /*make a fake object so we can give a proper message*/
        object_type* i_ptr;
        object_type object_type_body;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Create the object */
        object_prep(i_ptr, o_ptr->k_idx);

        /*make it plural*/
        i_ptr->number = 2;

        /*now describe with correct amount*/
        object_desc(o_name2, sizeof(o_name2), i_ptr, false, 0);

        /* Prompt */
        strnfmt(tmp_val, sizeof(tmp_val),
            "Remove automatic inscription for %s? ", o_name2);

        /* Auto-Inscribe if they want that */
        if (get_check(tmp_val))
            obliterate_autoinscription(o_ptr->k_idx);
    }

    /* Message */
    msg_print("Inscription removed.");

    /* Combine the pack */
    p_ptr->notice |= (PN_COMBINE);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Remove the inscription from an object
 * XXX Mention item (when done)?
 */
void do_cmd_uninscribe(void)
{
    int item;

    object_type* o_ptr;

    cptr q, s;

    /* Get an item */
    q = "Un-inscribe which item? ";
    s = "You have nothing to un-inscribe.";
    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN | USE_FLOOR,
            q, s, &item))
        return;

    /* Get the item (in the pack) */
    if (player_inventory_handle_valid(item))
    {
        o_ptr = player_inventory_object(item);
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Nothing to remove */
    if (!o_ptr->obj_note)
    {
        msg_print("That item had no inscription to remove.");
        return;
    }

    // Do the work
    uninscribe(o_ptr);
}

/*
 * Inscribe an object with a comment
 */
void do_cmd_inscribe(void)
{
    int item;

    object_type* o_ptr;

    char o_name[80];

    char tmp[80];

    cptr q, s;

    /* Get an item */
    q = "Inscribe which item? ";
    s = "You have nothing to inscribe.";
    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN | USE_FLOOR,
            q, s, &item))
        return;

    /* Get the item (in the pack) */
    if (player_inventory_handle_valid(item))
    {
        o_ptr = player_inventory_object(item);
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Describe the activity */
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("Inscribing %s.", o_name);
    message_flush();

    /* Start with nothing */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Use old inscription */
    if (o_ptr->obj_note)
    {
        /* Start with the old inscription */
        strnfmt(tmp, sizeof(tmp), "%s", quark_str(o_ptr->obj_note));
    }

    /* Get a new inscription (possibly empty) */
    if (term_get_string("Inscription: ", tmp, sizeof(tmp)))
    {
        char tmp_val[160];
        char o_name2[80];

        /*make a fake object so we can give a proper message*/
        object_type* i_ptr;
        object_type object_type_body;

        // if given an empty inscription, then uninscribe instead
        if (strlen(tmp) == 0)
        {
            uninscribe(o_ptr);
            return;
        }

        /* Save the inscription */
        o_ptr->obj_note = quark_add(tmp);

        /* Add an autoinscription? */
        // Sil-y: removed restriction to known items (through 'object_aware')
        if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART)))
        {
            /* Get local object */
            i_ptr = &object_type_body;

            /* Wipe the object */
            object_wipe(i_ptr);

            /* Create the object */
            object_prep(i_ptr, o_ptr->k_idx);

            /*make it plural*/
            i_ptr->number = 2;

            /*now describe with correct amount*/
            object_desc(o_name2, sizeof(o_name2), i_ptr, false, 0);

            /* Prompt */
            strnfmt(tmp_val, sizeof(tmp_val),
                "Automatically inscribe all %s with '%s'? ", o_name2, tmp);

            /* Auto-Inscribe if they want that */
            if (get_check(tmp_val))
                add_autoinscription(o_ptr->k_idx, tmp);
        }

        /* Combine the pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }
}

/*
 * An "item_tester_hook" for refueling lanterns
 */
static bool item_tester_refuel_lantern(const object_type* o_ptr)
{
    if (object_has_broken_prefix(o_ptr))
        return (false);

    /* Flasks of oil are okay */
    if (o_ptr->tval == TV_FLASK)
        return (true);

    /* Non-empty lanterns are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && (o_ptr->timeout > 0))
    {
        return (true);
    }

    /* Assume not okay */
    return (false);
}

/*
 * Refill the player's lamp (from the pack or floor)
 */
void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item)
{
    int item;

    object_type* o_ptr = NULL;
    object_type* j_ptr;
    int supply_index = supplies_current_action();
    bool from_supplies = false;
    int source_oil = 0;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
        from_supplies = (item == SUPPLIES_INDEX) && (supply_index >= 0);
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_refuel_lantern;

        /* Get an item */
        q = "Refill with which source of oil? ";
        s = "You have no sources of oil.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE,
            SUPPLY_GROUP_LIGHTS, true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE,
                SUPPLY_GROUP_LIGHTS, true, true);
            return;
        }

        supplies_clear_pending_action();

        /* Get the item (in the pack) */
        if (item >= SUPPLIES_INDEX)
        {
            supply_index = item - SUPPLIES_INDEX;
            o_ptr = supplies_entry_at(supply_index);
            from_supplies = true;
        }
        else if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    if (!o_ptr)
        return;

    if (player_pack_action_start(PLAYER_PACK_ACTION_REFUEL_LAMP, item, 0,
            false, o_ptr))
        return;

    if (object_has_broken_prefix(o_ptr))
    {
        msg_print("Broken items must be repaired before they can be used.");
        return;
    }

    source_oil = (o_ptr->tval == TV_FLASK) ? o_ptr->pval : o_ptr->timeout;

    if (from_supplies)
    {
        if (source_oil > 0)
        {
            player_gain_lamp_oil(source_oil, true);
            player_oil_container_set_fuel(o_ptr, 0);
            supplies_refresh_entry(supply_index);
            msg_print("You add the oil to your lamp stores.");
        }
        else
        {
            msg_print("That oil is already in your lamp stores.");
        }

        p_ptr->redraw |= (PR_LIGHT);
        handle_stuff();
        return;
    }

    /* Get the lantern */
    j_ptr = &inventory[INVEN_LITE];

    if ((j_ptr->tval != TV_LIGHT) || (j_ptr->sval != SV_LIGHT_LANTERN))
    {
        msg_print("You are not wielding a lantern.");
        return;
    }

    if (object_has_broken_prefix(j_ptr))
    {
        msg_print("Broken items must be repaired before they can be used.");
        return;
    }

    if (source_oil <= 0)
    {
        msg_print("There is no oil left in that.");
        return;
    }

    if (source_oil + player_light_fuel(j_ptr) > player_light_max_fuel(j_ptr)
        && !get_check("Refueling this lamp will waste some oil. Proceed? "))
    {
        return;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Refuel from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        player_light_add_fuel(j_ptr, source_oil);
    }
    /* Refuel from a flask */
    else
    {
        player_light_add_fuel(j_ptr, source_oil);
    }

    /* Message */
    msg_print("You fuel your lamp.");

    /* Comment */
    if (player_light_fuel(j_ptr) >= player_light_max_fuel(j_ptr))
    {
        player_light_set_fuel(j_ptr, player_light_max_fuel(j_ptr));
        msg_print("Your lamp is full.");
    }

    /* Refilled from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        /* Unstack if necessary */
        if (o_ptr->number > 1)
        {
            object_type* i_ptr;
            object_type object_type_body;

            /* Get local object */
            i_ptr = &object_type_body;

            /* Obtain a local object */
            object_copy(i_ptr, o_ptr);

            /* Modify quantity */
            i_ptr->number = 1;

            /* Remove fuel */
            i_ptr->timeout = 0;

            /* Unstack the used item */
            o_ptr->number--;

            /* Carry or drop */
            if (item >= 0)
            {
                item = inven_carry(i_ptr, false);
                if (item < 0)
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
            }
            else
                drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
        }

        /* Empty a single latern */
        else
        {
            /* No more fuel */
            o_ptr->timeout = 0;
        }

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /* Refilled from a flask */
    else
    {
        /* Decrease the item (from the pack) */
        if (item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_describe(item);
            inven_item_optimize(item);
        }

        /* Decrease the item (from the floor) */
        else
        {
            floor_item_increase(0 - item, -1);
            floor_item_describe(0 - item);
            floor_item_optimize(0 - item);
        }
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the lamp
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_torch(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_TORCH))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_mallorn(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_MALLORN))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * Refuel the player's torch (from the pack or floor)
 */
void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn)
{
    int item;

    object_type* o_ptr;
    object_type* j_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = is_mallorn ? item_tester_refuel_mallorn
                                      : item_tester_refuel_torch;

        /* Get an item */
        q = "Refuel with which torch? ";
        s = "You have no extra torches.";
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
            return;

        /* Get the item (in the pack) */
        if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    if (player_pack_action_start(PLAYER_PACK_ACTION_REFUEL_TORCH, item, 0,
            is_mallorn, o_ptr))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get the primary torch */
    j_ptr = &inventory[INVEN_LITE];
    
    log_debug("do_cmd_refuel_torch: BEFORE refuel - j_ptr (INVEN_LITE) k_idx=%d timeout=%d",
              j_ptr->k_idx, j_ptr->timeout);
    log_debug("do_cmd_refuel_torch: BEFORE refuel - o_ptr (item=%d) k_idx=%d timeout=%d",
              item, o_ptr->k_idx, o_ptr->timeout);

    /* Refuel */
    j_ptr->timeout += o_ptr->timeout + 5;
    
    log_debug("do_cmd_refuel_torch: AFTER refuel - j_ptr timeout=%d", j_ptr->timeout);

    /* Message */
    msg_print("You combine the torches.");

    /* Over-fuel message */
    int max_fuel = is_mallorn ? FUEL_MALLORN : FUEL_TORCH;
    if (j_ptr->timeout >= max_fuel)
    {
        j_ptr->timeout = max_fuel;
        msg_print("Your torch is fully fueled.");
    }

    /* Refuel message */
    else
    {
        msg_print("Your torch glows more brightly.");
    }

    /* Decrease the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the torch
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * Refuel the player's lamp or torch
 */
void do_cmd_refuel(void)
{
    object_type* o_ptr;

    /* Get the light */
    o_ptr = &inventory[INVEN_LITE];

    /* It is nothing */
    if (o_ptr->tval != TV_LIGHT)
    {
        msg_print("You are not wielding a light.");
    }

    /* It's a lamp */
    else if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        do_cmd_refuel_lamp(NULL, 0);
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_TORCH)
    {
        msg_print("You can no longer combine torches.");
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_MALLORN)
    {
        msg_print("You can no longer combine torches.");
    }

    /* No torch to refuel */
    else
    {
        msg_print("Your light cannot be refueled.");
    }
}
