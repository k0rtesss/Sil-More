/* File: object-ui-enhanced.h */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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
void show_inven_enhanced(void);
void show_equip_enhanced(void);
void run_inven_enhanced_menu(void);
void run_equip_enhanced_menu(void);

#endif /* INCLUDED_OBJECT_UI_ENHANCED_H */
