/* File: cmd-item.h */

/*
 * Transitional public header for the item command split.
 */

#ifndef INCLUDED_CMD_ITEM_H
#define INCLUDED_CMD_ITEM_H

#include "h-basic.h"
#include "supplies.h"

#include "cmd-identify.h"
#include "cmd-pickup.h"

typedef struct object_type object_type;

void do_cmd_fletchery(void);
void finish_fletching(int slot);
void do_cmd_inven(void);
void do_cmd_equip(void);
void do_cmd_drop(void);
void do_cmd_eat_food(object_type* default_o_ptr, int default_item);
void do_cmd_quaff_potion(object_type* default_o_ptr, int default_item);
void do_cmd_use_gem(object_type* default_o_ptr, int default_item);
void do_cmd_activate_staff(object_type* default_o_ptr, int default_item);
void do_cmd_play_instrument(object_type* default_o_ptr, int default_item);
void do_cmd_use_item_by_index(int item);
void do_cmd_use_item(void);
bool open_supplies_menu_with_context(supply_menu_action default_action,
    int default_group, bool default_focus, bool default_hotkey);
void py_pickup(void);
void do_cmd_wield(object_type* default_o_ptr, int default_item);
void do_cmd_wield_wrapper(void);
void do_cmd_takeoff(object_type* default_o_ptr, int default_item);
void do_cmd_inven_direct(void);
void do_cmd_equip_direct(void);
void do_cmd_drop_item_by_index(int item);
void do_cmd_destroy(void);
void do_cmd_observe(void);
void do_cmd_uninscribe(void);
void do_cmd_inscribe(void);
void do_cmd_activate(void);
void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item);
void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn);
void do_cmd_refuel(void);
extern bool throw_slot_menu_active;
extern bool throw_slot_enabled[];

#endif /* INCLUDED_CMD_ITEM_H */
