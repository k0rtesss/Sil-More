/* File: object-ui-enhanced.h */

/*
 * Enhanced inventory and equipment menus.
 */

#ifndef INCLUDED_OBJECT_UI_ENHANCED_H
#define INCLUDED_OBJECT_UI_ENHANCED_H

#include "../h-basic.h"

#define ENHANCED_ACTION_NONE 0
#define ENHANCED_ACTION_SWITCH 1
#define ENHANCED_ACTION_EXAMINE 2
#define ENHANCED_ACTION_USE 3
#define ENHANCED_ACTION_DROP 4
#define ENHANCED_ACTION_SUPPLIES 5

extern int enhanced_menu_action;
extern int enhanced_inventory_selected_item;
extern int enhanced_equip_action;
extern int enhanced_equipment_selected_item;
extern char current_menu_command;
extern int current_menu_state;

void describe_item_with_comparisons(int item_index, bool include_comparisons);
void run_inven_enhanced_menu(void);
void run_equip_enhanced_menu(void);

#endif /* INCLUDED_OBJECT_UI_ENHANCED_H */
