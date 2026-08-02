#include "angband.h"
#include "externs.h"

#define PACK_ACTION_TURN_COST 3

typedef struct player_pack_action_state
{
    player_pack_action_kind kind;
    int item;
    int arg;
    bool flag;
    int turns_left;
    bool completing;
    object_type object;
} player_pack_action_state;

static player_pack_action_state pack_action;

static object_type* pack_action_object(int item)
{
    if (item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
        return player_quiver_arrow_object(item);
    if (item >= 0 && item < INVEN_TOTAL)
        return &inventory[item];
    if (item < 0 && 0 - item > 0 && 0 - item < o_max)
        return &o_list[0 - item];

    return NULL;
}

static bool pack_action_object_matches(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return o_ptr->k_idx == pack_action.object.k_idx
        && o_ptr->tval == pack_action.object.tval
        && o_ptr->sval == pack_action.object.sval
        && o_ptr->name1 == pack_action.object.name1
        && o_ptr->name2 == pack_action.object.name2;
}

void player_pack_action_reset(void)
{
    object_wipe(&pack_action.object);
    pack_action.kind = PLAYER_PACK_ACTION_NONE;
    pack_action.item = 0;
    pack_action.arg = 0;
    pack_action.flag = false;
    pack_action.turns_left = 0;
    pack_action.completing = false;
}

bool player_pack_action_pending(void)
{
    return pack_action.kind != PLAYER_PACK_ACTION_NONE;
}

int player_pack_action_turns_left(void)
{
    return pack_action.turns_left;
}

bool player_pack_action_completing(player_pack_action_kind kind)
{
    return pack_action.completing && pack_action.kind == kind;
}

int player_pack_action_completion_arg(void)
{
    return pack_action.arg;
}

/* Compatibility for browser paths that used to preflight the combat rule.
 * Pack access is never rejected now; the shared command sinks defer it. */
bool player_pack_item_action_blocked(const object_type* o_ptr)
{
    (void)o_ptr;
    return false;
}

cptr player_pack_item_action_restriction_message(void)
{
    return "Pack actions take three turns and attacks interrupt them.";
}

static bool player_pack_action_start_internal(player_pack_action_kind kind,
    int item, int arg, bool flag, const object_type* o_ptr, bool force_pack)
{
    char o_name[80];

    if (pack_action.completing || kind == PLAYER_PACK_ACTION_NONE
        || !o_ptr || !o_ptr->k_idx
        || (!force_pack
            && inventory_limit_group_for_object(o_ptr) != INV_LIMIT_PACK))
    {
        return false;
    }

    if (player_pack_action_pending())
        return true;

    pack_action.kind = kind;
    pack_action.item = item;
    pack_action.arg = arg;
    pack_action.flag = flag;
    pack_action.turns_left = PACK_ACTION_TURN_COST - 1;
    object_copy(&pack_action.object, o_ptr);

    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
    msg_format("You begin searching your Pack for %s.", o_name);

    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
    p_ptr->redraw |= PR_STATE;
    return true;
}

bool player_pack_action_start(player_pack_action_kind kind, int item, int arg,
    bool flag, const object_type* o_ptr)
{
    return player_pack_action_start_internal(kind, item, arg, flag, o_ptr,
        false);
}

bool player_pack_action_start_forced(player_pack_action_kind kind, int item,
    int arg, bool flag, const object_type* o_ptr)
{
    return player_pack_action_start_internal(kind, item, arg, flag, o_ptr,
        true);
}

void player_pack_action_cancel(void)
{
    if (!player_pack_action_pending())
        return;

    player_pack_action_reset();
    p_ptr->redraw |= PR_STATE;
}

void player_pack_action_interrupt(void)
{
    if (!player_pack_action_pending())
        return;

    player_pack_action_cancel();
    msg_print("Your Pack action is interrupted!");
}

static void player_pack_action_complete(player_pack_action_kind kind, int item,
    int arg, bool flag, object_type* o_ptr)
{
    switch (kind)
    {
    case PLAYER_PACK_ACTION_USE_ITEM:
        do_cmd_use_item_by_index(item);
        break;
    case PLAYER_PACK_ACTION_WIELD:
        if (arg >= INVEN_WIELD && arg < INVEN_TOTAL)
        {
            if (flag)
                (void)do_cmd_wield_stack_to_slot(o_ptr, item, arg);
            else
                (void)do_cmd_wield_to_slot(o_ptr, item, arg);
        }
        else
        {
            do_cmd_wield(o_ptr, item);
        }
        break;
    case PLAYER_PACK_ACTION_TAKEOFF:
        do_cmd_takeoff(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_DROP:
        (void)do_cmd_drop_item_by_index_confirm(item, false);
        break;
    case PLAYER_PACK_ACTION_DELETE:
        (void)do_cmd_delete_item_by_index(item);
        break;
    case PLAYER_PACK_ACTION_REFUEL_LAMP:
        do_cmd_refuel_lamp(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_REFUEL_TORCH:
        do_cmd_refuel_torch(o_ptr, item, flag);
        break;
    case PLAYER_PACK_ACTION_EAT:
        do_cmd_eat_food(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_QUAFF:
        do_cmd_quaff_potion(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_PLAY:
        do_cmd_play_instrument(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_ACTIVATE_STAFF:
        do_cmd_activate_staff(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_USE_GEM:
        do_cmd_use_gem(o_ptr, item);
        break;
    case PLAYER_PACK_ACTION_ACTIVATE:
        do_cmd_activate_by_index(item);
        break;
    case PLAYER_PACK_ACTION_PICKUP:
        py_pickup_aux(0 - item);
        break;
    case PLAYER_PACK_ACTION_JEWELRY_PRESET:
        (void)do_cmd_jewelry_preset_apply(arg);
        break;
    case PLAYER_PACK_ACTION_NONE:
    default:
        break;
    }
}

void player_pack_action_process(void)
{
    player_pack_action_kind kind;
    object_type* o_ptr;
    int item;
    int arg;
    bool flag;

    if (!player_pack_action_pending())
        return;

    if (pack_action.turns_left > 1)
    {
        pack_action.turns_left--;
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        p_ptr->redraw |= PR_STATE;
        return;
    }

    o_ptr = pack_action_object(pack_action.item);
    if (!pack_action_object_matches(o_ptr))
    {
        player_pack_action_cancel();
        msg_print("You can no longer find that item.");
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        return;
    }

    kind = pack_action.kind;
    item = pack_action.item;
    arg = pack_action.arg;
    flag = pack_action.flag;
    pack_action.turns_left = 0;
    pack_action.completing = true;
    p_ptr->redraw |= PR_STATE;

    player_pack_action_complete(kind, item, arg, flag, o_ptr);
    pack_action.completing = false;
    pack_action.kind = PLAYER_PACK_ACTION_NONE;
    object_wipe(&pack_action.object);

    /* The third Pack turn is spent even if final validation now fails. */
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
}
