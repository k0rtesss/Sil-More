/* File: object-ui-display.h */

/*
 * Shared object UI display helpers.
 * Semantic item scenes use the shared row/layout helpers here.
 */

#ifndef INCLUDED_OBJECT_UI_DISPLAY_H
#define INCLUDED_OBJECT_UI_DISPLAY_H

#include "../app/app-ui.h"
#include "../h-basic.h"

typedef struct object_type object_type;

bool build_inventory_subwindow_ui_scene(app_ui_scene* scene);
bool build_equipment_subwindow_ui_scene(app_ui_scene* scene);

bool supplies_visible_for_current_filter(void);
void format_supply_summary(char* buf, size_t len);

bool get_story_inventory_list_active(void);
void set_story_inventory_list_active(bool active);
bool get_story_equipment_list_active(void);
void set_story_equipment_list_active(bool active);
int menu_weight_col_for_width(int term_wid);
int menu_label_col_for_width(int term_wid, bool display_weights);
int menu_overlay_clear_col(int col);
int menu_desc_limit(int text_col, int label_col, int weight_col, bool display_weights);
int menu_inventory_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);
int menu_equipment_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);

#endif /* INCLUDED_OBJECT_UI_DISPLAY_H */
