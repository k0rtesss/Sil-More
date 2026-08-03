/* File: object/object-inventory-limits.h */

#ifndef INCLUDED_OBJECT_INVENTORY_LIMITS_H
#define INCLUDED_OBJECT_INVENTORY_LIMITS_H

#include "angband.h"

#ifndef INVENTORY_LIMIT_GROUP_DEFINED
#define INVENTORY_LIMIT_GROUP_DEFINED
enum inventory_limit_group
{
    INV_LIMIT_NONE = 0,
    INV_LIMIT_PACK,
    INV_LIMIT_HARNESS,
    INV_LIMIT_JEWELRY,
    INV_LIMIT_SUPPLY_WEIGHT,
    INV_LIMIT_TORCHES,
    INV_LIMIT_BRASS_LAMPS,
    INV_LIMIT_LESSER_JEWEL,
    INV_LIMIT_FEANORIAN_LAMP
};
#endif

/* Pack and Harness capacity is stored in tenths of an imperial quart. */
#define INVENTORY_PACK_VOLUME_CAP 260
#define INVENTORY_HARNESS_VOLUME_CAP 210
#define INVENTORY_HEAVY_ARMOUR_VOLUME_BONUS 60
#define INVENTORY_ARROW_VOLUME_BUNDLE 12

bool inven_carry_limit_failed(void);
enum inventory_limit_group inven_carry_limit_group(void);
cptr inven_carry_limit_label(void);
int inven_carry_limit_value(void);
bool inven_carry_limit_is_supply_weight(void);
bool inven_carry_limit_can_replace(const object_type* o_ptr);
enum inventory_limit_group inventory_limit_group_for_object(
    const object_type* o_ptr);
bool object_can_choose_pack_or_harness(const object_type* o_ptr);
bool object_can_store_directly_in_pack(const object_type* o_ptr);
bool inventory_limit_info_for_object(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost);
int inventory_limit_usage_for_group(enum inventory_limit_group group);
int inventory_limit_limit_for_group(enum inventory_limit_group group);
int inventory_limit_space_for_object(const object_type* o_ptr);
int inventory_limit_intrinsic_space_for_object(const object_type* o_ptr);
int inventory_limit_carriage_savings_for_object(const object_type* o_ptr);
int inventory_limit_carriage_savings_for_group(
    enum inventory_limit_group group);
cptr inventory_limit_carriage_ability_name_for_object(
    const object_type* o_ptr);
int inventory_limit_additional_space_for_object(const object_type* o_ptr);
int inventory_limit_max_carryable_quantity(const object_type* o_ptr);
int inventory_limit_removal_space_for_object(const object_type* o_ptr);
int inventory_limit_usage_after_replacing(const object_type* incoming,
    const object_type* removed, int remove_quantity);
bool inventory_limit_object_matches_group(enum inventory_limit_group group,
    const object_type* o_ptr);
cptr inventory_limit_group_name(enum inventory_limit_group group);
void inven_enforce_current_pack_limits(void);
void inventory_limit_grandfather_current_overflow(void);
int object_stack_limit(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_INVENTORY_LIMITS_H */
