/* File: object/object-inventory-limits.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "object/object-internal.h"
#include "object/object-inventory-limits.h"
#include "supplies.h"

static bool carry_limit_last_failed = false;
static enum inventory_limit_group carry_limit_last_group = INV_LIMIT_NONE;
static int carry_limit_last_limit = 0;
static char carry_limit_last_label[64];
static enum inventory_limit_group pack_limit_prompt_group = INV_LIMIT_NONE;
static int grandfathered_volume[INV_LIMIT_HARNESS + 1] = { 0 };

static bool inventory_limit_is_volume_group(enum inventory_limit_group group)
{
    return group == INV_LIMIT_PACK || group == INV_LIMIT_HARNESS;
}

bool object_can_choose_pack_or_harness(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    return (f4 & TR4_HARNESS_STOWABLE) != 0;
}

void clear_inventory_limit_failure(void)
{
    carry_limit_last_failed = false;
    carry_limit_last_group = INV_LIMIT_NONE;
    carry_limit_last_limit = 0;
    carry_limit_last_label[0] = '\0';
}

bool inven_index_valid(int item, cptr context)
{
    if (item >= 0 && item < INVEN_TOTAL)
        return true;

    log_error("%s: invalid inventory slot %d",
        context ? context : "inventory", item);
    return false;
}

static enum inventory_limit_group volume_group_for_object(
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || object_effective_volume(o_ptr) <= 0)
        return INV_LIMIT_NONE;
    if (object_is_quivered_arrow(o_ptr))
        return INV_LIMIT_NONE;
    /* Arrow kinds retain their generation/equipment classification.  Their
     * carried location is a gameplay rule: loose arrows use the Pack. */
    if (o_ptr->tval == TV_ARROW)
        return INV_LIMIT_PACK;

    if (o_ptr->storage == OBJECT_STORAGE_PACK)
        return INV_LIMIT_PACK;
    if (o_ptr->storage == OBJECT_STORAGE_HARNESS)
        return INV_LIMIT_HARNESS;

    return INV_LIMIT_NONE;
}

static int volume_limit_for_group(enum inventory_limit_group group)
{
    int limit;

    if (group == INV_LIMIT_PACK)
        limit = INVENTORY_PACK_VOLUME_CAP;
    else if (group == INV_LIMIT_HARNESS)
        limit = INVENTORY_HARNESS_VOLUME_CAP;
    else
        return -1;

    if (p_ptr && p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR])
        limit += INVENTORY_HEAVY_ARMOUR_VOLUME_BONUS;

    return limit;
}

static int arrow_bundle_count(int arrows)
{
    if (arrows <= 0)
        return 0;

    return (arrows + INVENTORY_ARROW_VOLUME_BUNDLE - 1)
        / INVENTORY_ARROW_VOLUME_BUNDLE;
}

static const ability_type* learned_carriage_ability(byte target)
{
    const ability_type* best = NULL;

    if (!p_ptr || !z_info || !b_info || target == ABILITY_CARRIAGE_NONE)
        return NULL;

    for (int i = 0; i < z_info->b_max; i++)
    {
        const ability_type* b_ptr = &b_info[i];

        if (b_ptr->carriage_target != target
            || b_ptr->carriage_reduction_percent == 0)
        {
            continue;
        }
        if (b_ptr->skilltype >= S_MAX || b_ptr->abilitynum >= ABILITIES_MAX)
            continue;
        if (!p_ptr->innate_ability[b_ptr->skilltype][b_ptr->abilitynum])
            continue;
        if (!best || b_ptr->carriage_reduction_percent
                > best->carriage_reduction_percent)
        {
            best = b_ptr;
        }
    }

    return best;
}

static const ability_type* carriage_ability_for_object(
    const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
        return NULL;

    if (o_ptr->tval == TV_ARROW)
        return learned_carriage_ability(ABILITY_CARRIAGE_ARROWS);

    if (volume_group_for_object(o_ptr) != INV_LIMIT_HARNESS)
        return NULL;

    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_THROWING)
        return learned_carriage_ability(ABILITY_CARRIAGE_THROWING);

    return NULL;
}

static int carried_unit_volume(const object_type* o_ptr,
    enum inventory_limit_group group, bool apply_carriage_efficiency)
{
    const ability_type* b_ptr = NULL;
    int reduction = 0;

    (void)group;

    if (apply_carriage_efficiency)
        b_ptr = carriage_ability_for_object(o_ptr);
    if (b_ptr)
        reduction = 0 - b_ptr->carriage_reduction_percent;

    return object_effective_volume_with_reduction(o_ptr, reduction);
}

static int inventory_volume_last_slot(enum inventory_limit_group group)
{
    /* Worn Pack apparel is outside the backpack.  Harness objects remain on
     * the Harness when wielded, worn, quivered, or placed at the Belt. */
    return group == INV_LIMIT_HARNESS ? INVEN_TOTAL - 1 : INVEN_PACK;
}

static int projected_inventory_volume_usage_internal(
    enum inventory_limit_group group, const object_type* removed,
    int remove_quantity, const object_type* incoming,
    bool apply_carriage_efficiency)
{
    int usage = 0;
    int arrows = 0;
    int arrow_volume = 0;

    if (!inventory_limit_is_volume_group(group))
        return 0;

    for (int i = 0; i <= inventory_volume_last_slot(group); i++)
    {
        const object_type* o_ptr = &inventory[i];
        int number = o_ptr->number;
        int volume;

        if (o_ptr == removed)
            number = MAX(0, number - MAX(remove_quantity, 0));
        if (!o_ptr->k_idx || number <= 0)
            continue;
        if (volume_group_for_object(o_ptr) != group)
            continue;

        volume = carried_unit_volume(o_ptr, group,
            apply_carriage_efficiency);
        if (o_ptr->tval == TV_ARROW)
        {
            arrows += number;
            arrow_volume = MAX(arrow_volume, volume);
        }
        else
        {
            usage += volume * number;
        }
    }

    if (incoming && incoming->k_idx
        && volume_group_for_object(incoming) == group)
    {
        int number = MAX(incoming->number, 1);
        int volume = carried_unit_volume(incoming, group,
            apply_carriage_efficiency);

        if (incoming->tval == TV_ARROW)
        {
            arrows += number;
            arrow_volume = MAX(arrow_volume, volume);
        }
        else
        {
            usage += volume * number;
        }
    }

    usage += arrow_bundle_count(arrows) * arrow_volume;
    return usage;
}

static int projected_inventory_volume_usage(enum inventory_limit_group group,
    const object_type* removed, int remove_quantity,
    const object_type* incoming)
{
    return projected_inventory_volume_usage_internal(group, removed,
        remove_quantity, incoming, true);
}

static int inventory_volume_usage(enum inventory_limit_group group)
{
    return projected_inventory_volume_usage(group, NULL, 0, NULL);
}

static bool get_inventory_limit_info(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost)
{
    enum inventory_limit_group local_group = volume_group_for_object(o_ptr);

    if (!inventory_limit_is_volume_group(local_group))
        return false;

    if (group)
        *group = local_group;
    if (limit)
        *limit = volume_limit_for_group(local_group);
    if (cost)
        *cost = carried_unit_volume(o_ptr, local_group, true);

    return true;
}

/* Volume always follows quantity, even when an incoming object can stack. */
bool inventory_limit_is_stack_counted(const object_type* o_ptr)
{
    (void)o_ptr;
    return false;
}

int object_stack_limit(const object_type* o_ptr)
{
    if (!o_ptr)
        return MAX_STACK_SIZE - 1;

    if (o_ptr->tval == TV_RING)
        return 1;
    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_DAGGER)
        return 7;
    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_SPEAR)
        return 4;
    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_HAND_AXE)
        return 3;
    if (o_ptr->tval == TV_ARROW)
        return 48;
    if (o_ptr->tval == TV_HORN)
        return 1;

    return MAX_STACK_SIZE - 1;
}

static void fill_inventory_limit_label(enum inventory_limit_group group)
{
    switch (group)
    {
    case INV_LIMIT_PACK:
        SDL_strlcpy(carry_limit_last_label, "Pack volume",
            sizeof(carry_limit_last_label));
        break;
    case INV_LIMIT_HARNESS:
        SDL_strlcpy(carry_limit_last_label, "Harness volume",
            sizeof(carry_limit_last_label));
        break;
    case INV_LIMIT_SUPPLY_WEIGHT:
        SDL_strlcpy(carry_limit_last_label, "supply weight",
            sizeof(carry_limit_last_label));
        break;
    case INV_LIMIT_TORCHES:
        SDL_strlcpy(carry_limit_last_label, "torches",
            sizeof(carry_limit_last_label));
        break;
    case INV_LIMIT_BRASS_LAMPS:
        SDL_strlcpy(carry_limit_last_label, "oil container slots",
            sizeof(carry_limit_last_label));
        break;
    case INV_LIMIT_LESSER_JEWEL:
    case INV_LIMIT_FEANORIAN_LAMP:
        SDL_strlcpy(carry_limit_last_label,
            "lesser jewels or Feanorian lamps",
            sizeof(carry_limit_last_label));
        break;
    default:
        SDL_strlcpy(carry_limit_last_label, "items of this type",
            sizeof(carry_limit_last_label));
        break;
    }
}

static enum inventory_limit_group light_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return INV_LIMIT_NONE;

    if (player_oil_container_object(o_ptr))
        return INV_LIMIT_BRASS_LAMPS;
    if (o_ptr->tval != TV_LIGHT)
        return INV_LIMIT_NONE;

    switch (o_ptr->sval)
    {
    case SV_LIGHT_TORCH:
    case SV_LIGHT_MALLORN:
        return INV_LIMIT_TORCHES;
    case SV_LIGHT_LESSER_JEWEL:
    case SV_LIGHT_FEANORIAN:
        return INV_LIMIT_LESSER_JEWEL;
    default:
        return INV_LIMIT_NONE;
    }
}

void set_inventory_limit_failure(enum inventory_limit_group group, int limit,
    const object_type* o_ptr)
{
    (void)o_ptr;
    carry_limit_last_failed = true;
    carry_limit_last_group = group;
    carry_limit_last_limit = limit;
    fill_inventory_limit_label(group);
}

bool player_light_capacity_okay(const object_type* o_ptr, bool record_failure)
{
    int cap;
    enum inventory_limit_group group;

    if (!o_ptr || !o_ptr->k_idx)
        return true;

    cap = player_light_carry_cap(o_ptr);
    if (cap <= 0 || player_light_available_capacity(o_ptr) >= o_ptr->number)
        return true;

    if (record_failure)
    {
        group = light_limit_group(o_ptr);
        if (group != INV_LIMIT_NONE)
            set_inventory_limit_failure(group, cap, o_ptr);
    }

    return false;
}

int inventory_limit_additional_space_for_object(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int current;
    int projected;

    if (!get_inventory_limit_info(o_ptr, &group, NULL, NULL))
        return 0;

    current = inventory_volume_usage(group);
    projected = projected_inventory_volume_usage(group, NULL, 0, o_ptr);
    return MAX(projected - current, 0);
}

int inventory_limit_removal_space_for_object(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int current;
    int projected;

    if (!get_inventory_limit_info(o_ptr, &group, NULL, NULL))
        return 0;

    current = inventory_volume_usage(group);
    projected = projected_inventory_volume_usage(group, o_ptr,
        MAX(o_ptr->number, 1), NULL);
    return MAX(current - projected, 0);
}

int inventory_limit_usage_after_replacing(const object_type* incoming,
    const object_type* removed, int remove_quantity)
{
    enum inventory_limit_group group = volume_group_for_object(incoming);

    if (!inventory_limit_is_volume_group(group) || !removed
        || volume_group_for_object(removed) != group || remove_quantity <= 0)
    {
        return -1;
    }

    return projected_inventory_volume_usage(group, removed, remove_quantity,
        incoming);
}

bool inventory_type_slot_available(const object_type* o_ptr,
    bool record_failure)
{
    enum inventory_limit_group group;
    int limit;
    int allowed;
    int additional;

    if (!get_inventory_limit_info(o_ptr, &group, &limit, NULL))
        return true;

    /* A loaded legacy character may keep, rearrange, and restore possessions
     * up to the volume present at load time, but may never increase that
     * historical high-water mark. */
    allowed = MAX(limit, grandfathered_volume[group]);
    additional = inventory_limit_additional_space_for_object(o_ptr);
    if (inventory_volume_usage(group) + additional <= allowed)
        return true;

    if (record_failure)
        set_inventory_limit_failure(group, allowed, o_ptr);

    return false;
}

bool inven_carry_limit_can_replace(const object_type* o_ptr)
{
    enum inventory_limit_group group;

    if (!carry_limit_last_failed || carry_limit_last_limit <= 0 || !o_ptr)
        return false;

    if (carry_limit_last_group == INV_LIMIT_SUPPLY_WEIGHT)
    {
        return supplies_weight_counts_to_limit(o_ptr)
            && o_ptr->weight > 0 && MAX(o_ptr->number, 1) > 0;
    }

    if (o_ptr->k_idx
        && (o_ptr->tval == TV_LIGHT || o_ptr->tval == TV_FLASK))
    {
        group = light_limit_group(o_ptr);
        return group != INV_LIMIT_NONE && group == carry_limit_last_group
            && MAX(o_ptr->number, 1) > 0;
    }

    group = volume_group_for_object(o_ptr);
    return group != INV_LIMIT_NONE && group == carry_limit_last_group
        && object_effective_volume(o_ptr) > 0;
}

static bool item_tester_hook_pack_limit_group(const object_type* o_ptr)
{
    ptrdiff_t slot;

    if (volume_group_for_object(o_ptr) != pack_limit_prompt_group)
        return false;

    slot = o_ptr - inventory;
    if (slot >= INVEN_WIELD && slot < INVEN_TOTAL && cursed_p(o_ptr))
        return false;

    return true;
}

static int inventory_limit_group_last_slot(enum inventory_limit_group group)
{
    for (int item = inventory_volume_last_slot(group); item >= 0; item--)
    {
        if (inventory[item].k_idx
            && volume_group_for_object(&inventory[item]) == group
            && (item < INVEN_WIELD || !cursed_p(&inventory[item])))
        {
            return item;
        }
    }

    return -1;
}

static int inventory_limit_drop_amount(int item,
    enum inventory_limit_group group, int limit)
{
    object_type* o_ptr;
    int original_number;

    if (item < 0 || item >= INVEN_TOTAL || !inventory[item].k_idx)
        return 0;

    o_ptr = &inventory[item];
    original_number = o_ptr->number;
    for (int amount = 1; amount <= original_number; amount++)
    {
        o_ptr->number = original_number - amount;
        if (inventory_volume_usage(group) <= limit)
        {
            o_ptr->number = original_number;
            return amount;
        }
    }

    o_ptr->number = original_number;
    return original_number;
}

void inven_enforce_current_pack_limits(void)
{
    static const enum inventory_limit_group volume_groups[] = {
        INV_LIMIT_PACK, INV_LIMIT_HARNESS
    };

    if (!character_generated || character_xtra || character_icky
        || !p_ptr || p_ptr->is_dead)
    {
        return;
    }

    for (size_t i = 0; i < N_ELEMENTS(volume_groups); i++)
    {
        enum inventory_limit_group group = volume_groups[i];
        int limit = volume_limit_for_group(group);
        int usage = inventory_volume_usage(group);
        int enforce_limit = limit;
        bool warned = false;

        if (grandfathered_volume[group] > 0)
        {
            if (usage <= limit)
                grandfathered_volume[group] = 0;
            else
            {
                if (usage < grandfathered_volume[group])
                    grandfathered_volume[group] = usage;
                enforce_limit = MAX(limit, grandfathered_volume[group]);
            }
        }

        while (inventory_volume_usage(group) > enforce_limit)
        {
            int item = inventory_limit_group_last_slot(group);
            int amount;
            int old_usage = inventory_volume_usage(group);
            bool old_item_tester_full;
            byte old_item_tester_tval;
            bool (*old_item_tester_hook)(const object_type*);

            if (item < 0)
                break;

            if (!warned)
            {
                if (enforce_limit > limit)
                {
                    msg_format("Your legacy %s overage cannot increase beyond %d.%d qt; choose what to drop.",
                        group == INV_LIMIT_PACK ? "Pack" : "Harness",
                        enforce_limit / 10, enforce_limit % 10);
                }
                else
                {
                    msg_format("Your %s capacity is now %d.%d qt; choose what to drop.",
                        group == INV_LIMIT_PACK ? "Pack" : "Harness",
                        limit / 10, limit % 10);
                }
                warned = true;
            }

            old_item_tester_full = item_tester_full;
            old_item_tester_tval = item_tester_tval;
            old_item_tester_hook = item_tester_hook;
            item_tester_full = false;
            item_tester_tval = 0;
            pack_limit_prompt_group = group;
            item_tester_hook = item_tester_hook_pack_limit_group;

            if (!open_inventory_item_select_menu(USE_INVEN
                    | (group == INV_LIMIT_HARNESS ? USE_EQUIP : 0),
                    group == INV_LIMIT_PACK ? "Drop which Pack item? "
                                            : "Drop which Harness item? ",
                    "You have nothing suitable to drop.", &item))
            {
                item = inventory_limit_group_last_slot(group);
                if (item >= 0)
                    msg_print("No choice made; dropping enough of one excess item.");
            }

            pack_limit_prompt_group = INV_LIMIT_NONE;
            item_tester_hook = old_item_tester_hook;
            item_tester_tval = old_item_tester_tval;
            item_tester_full = old_item_tester_full;

            if (item < 0 || item >= INVEN_TOTAL || !inventory[item].k_idx)
                break;

            amount = inventory_limit_drop_amount(item, group, enforce_limit);
            if (amount <= 0)
                break;

            inven_drop(item, amount);
            handle_stuff();
            if (inventory_volume_usage(group) >= old_usage)
                break;
        }
    }
}

void inventory_limit_grandfather_current_overflow(void)
{
    static const enum inventory_limit_group volume_groups[] = {
        INV_LIMIT_PACK, INV_LIMIT_HARNESS
    };

    for (size_t i = 0; i < N_ELEMENTS(volume_groups); i++)
    {
        enum inventory_limit_group group = volume_groups[i];
        int used = inventory_volume_usage(group);
        int limit = volume_limit_for_group(group);

        grandfathered_volume[group] = (used > limit) ? used : 0;
    }
}

bool inven_carry_limit_failed(void)
{
    return carry_limit_last_failed;
}

enum inventory_limit_group inven_carry_limit_group(void)
{
    return carry_limit_last_group;
}

cptr inven_carry_limit_label(void)
{
    if (!carry_limit_last_failed || !carry_limit_last_label[0])
        return NULL;
    return carry_limit_last_label;
}

int inven_carry_limit_value(void)
{
    return carry_limit_last_limit;
}

bool inven_carry_limit_is_supply_weight(void)
{
    return carry_limit_last_failed
        && carry_limit_last_group == INV_LIMIT_SUPPLY_WEIGHT;
}

enum inventory_limit_group inventory_limit_group_for_object(
    const object_type* o_ptr)
{
    enum inventory_limit_group group;

    if (!o_ptr || !o_ptr->k_idx)
        return INV_LIMIT_NONE;

    if (o_ptr->tval == TV_LIGHT || o_ptr->tval == TV_FLASK
        || player_oil_container_object(o_ptr))
    {
        group = light_limit_group(o_ptr);
        if (group != INV_LIMIT_NONE)
            return group;
    }

    return volume_group_for_object(o_ptr);
}

bool inventory_limit_info_for_object(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost)
{
    enum inventory_limit_group local_group;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    local_group = inventory_limit_group_for_object(o_ptr);
    if (local_group == INV_LIMIT_NONE)
        return false;

    if (group)
        *group = local_group;

    if (inventory_limit_is_volume_group(local_group))
        return get_inventory_limit_info(o_ptr, NULL, limit, cost);

    if (local_group == INV_LIMIT_TORCHES
        || local_group == INV_LIMIT_BRASS_LAMPS
        || local_group == INV_LIMIT_LESSER_JEWEL
        || local_group == INV_LIMIT_FEANORIAN_LAMP)
    {
        if (limit)
            *limit = player_light_carry_cap(o_ptr);
        if (cost)
        {
            *cost = player_oil_container_object(o_ptr)
                ? player_oil_container_slot_cost(o_ptr) : 1;
        }
        return true;
    }

    return false;
}

int inventory_limit_usage_for_group(enum inventory_limit_group group)
{
    if (inventory_limit_is_volume_group(group))
        return inventory_volume_usage(group);
    if (group == INV_LIMIT_SUPPLY_WEIGHT)
        return supplies_limit_weight() / 10;
    if (group == INV_LIMIT_TORCHES)
        return player_carried_torch_count();
    if (group == INV_LIMIT_BRASS_LAMPS)
        return player_oil_container_slots_used();
    if (group == INV_LIMIT_LESSER_JEWEL
        || group == INV_LIMIT_FEANORIAN_LAMP)
    {
        return player_carried_light_count_for_sval(SV_LIGHT_LESSER_JEWEL)
            + player_carried_light_count_for_sval(SV_LIGHT_FEANORIAN);
    }

    return 0;
}

static int inventory_limit_space_for_object_internal(
    const object_type* o_ptr, bool apply_carriage_efficiency)
{
    enum inventory_limit_group group;
    int cost;
    int number;

    if (!inventory_limit_info_for_object(o_ptr, &group, NULL, &cost)
        || cost <= 0)
    {
        return 0;
    }

    if (inventory_limit_is_volume_group(group))
        cost = carried_unit_volume(o_ptr, group, apply_carriage_efficiency);

    number = MAX(o_ptr->number, 1);
    if (o_ptr->tval == TV_ARROW)
        return arrow_bundle_count(number) * cost;

    return cost * number;
}

int inventory_limit_space_for_object(const object_type* o_ptr)
{
    return inventory_limit_space_for_object_internal(o_ptr, true);
}

int inventory_limit_intrinsic_space_for_object(const object_type* o_ptr)
{
    return inventory_limit_space_for_object_internal(o_ptr, false);
}

int inventory_limit_carriage_savings_for_object(const object_type* o_ptr)
{
    int intrinsic = inventory_limit_intrinsic_space_for_object(o_ptr);
    int carried = inventory_limit_space_for_object(o_ptr);

    return MAX(intrinsic - carried, 0);
}

int inventory_limit_carriage_savings_for_group(
    enum inventory_limit_group group)
{
    int intrinsic;
    int carried;

    if (!inventory_limit_is_volume_group(group))
        return 0;

    intrinsic = projected_inventory_volume_usage_internal(group, NULL, 0,
        NULL, false);
    carried = inventory_volume_usage(group);
    return MAX(intrinsic - carried, 0);
}

cptr inventory_limit_carriage_ability_name_for_object(
    const object_type* o_ptr)
{
    const ability_type* b_ptr;

    if (inventory_limit_carriage_savings_for_object(o_ptr) <= 0)
        return NULL;

    b_ptr = carriage_ability_for_object(o_ptr);
    if (!b_ptr || !b_name || !b_ptr->name)
        return NULL;

    return b_name + b_ptr->name;
}

int inventory_limit_limit_for_group(enum inventory_limit_group group)
{
    if (inventory_limit_is_volume_group(group))
        return volume_limit_for_group(group);

    switch (group)
    {
    case INV_LIMIT_SUPPLY_WEIGHT:
        return supplies_current_weight_cap() / 10;
    case INV_LIMIT_TORCHES:
        return PLAYER_TORCH_CAP;
    case INV_LIMIT_BRASS_LAMPS:
        return PLAYER_OIL_CONTAINER_SLOT_CAP;
    case INV_LIMIT_LESSER_JEWEL:
    case INV_LIMIT_FEANORIAN_LAMP:
        return PLAYER_PERMANENT_LIGHT_CAP;
    default:
        return -1;
    }
}

bool inventory_limit_object_matches_group(
    enum inventory_limit_group group, const object_type* o_ptr)
{
    return group != INV_LIMIT_NONE
        && inventory_limit_group_for_object(o_ptr) == group;
}

cptr inventory_limit_group_name(enum inventory_limit_group group)
{
    switch (group)
    {
    case INV_LIMIT_PACK:
        return "Pack";
    case INV_LIMIT_HARNESS:
        return "Harness";
    case INV_LIMIT_SUPPLY_WEIGHT:
        return "supply weight";
    case INV_LIMIT_TORCHES:
        return "torches";
    case INV_LIMIT_BRASS_LAMPS:
        return "oil slots";
    case INV_LIMIT_LESSER_JEWEL:
    case INV_LIMIT_FEANORIAN_LAMP:
        return "permanent lights";
    default:
        return "";
    }
}
