/* File: object/object-inventory.h */

#ifndef INCLUDED_OBJECT_INVENTORY_H
#define INCLUDED_OBJECT_INVENTORY_H

#include "angband.h"

void inven_item_charges(int item);
void inven_item_describe(int item);
void inven_item_increase(int item, int num);
void inven_item_optimize(int item);
void floor_item_charges(int item);
void floor_item_describe(int item);
void floor_item_increase(int item, int num);
void floor_item_optimize(int item);
void check_pack_overflow(void);
void player_carried_extra_reset_store(void);
int player_carried_extra_entry_count(void);
object_type* player_carried_extra_entry_at(int index);
bool player_carried_extra_load(const object_type* o_ptr);
bool player_carried_extra_handle_valid(int item);
object_type* player_inventory_object(int item);
int player_inventory_handle_for_object(const object_type* o_ptr);
bool player_inventory_handle_valid(int item);
bool player_inventory_handle_is_carried(int item);
bool player_inventory_handle_is_equipped(int item);
int player_pack_entry_count(void);
int player_pack_entry_handle_at(int ordinal);
object_type* player_pack_entry_at(int ordinal);
char player_inventory_label(int item);
bool inven_carry_okay(const object_type* o_ptr);
bool inven_carry_okay_after_removing(const object_type* o_ptr,
    int remove_item, int remove_amt);
int inven_carry(object_type* o_ptr, bool combine_ammo);
int inven_takeoff(int item, int amt);
void inven_drop(int item, int amt);
void combine_pack(void);
void reorder_pack(bool display_message);
void check_artifact_visibility(void);

#endif /* INCLUDED_OBJECT_INVENTORY_H */
