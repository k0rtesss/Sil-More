/* File: object-ui-display.h */

/*
 * Shared object UI display helpers.
 * Semantic item scenes use the row/layout helpers here; the legacy
 * subwindow path still uses display_inven() / display_equip().
 */

#ifndef INCLUDED_OBJECT_UI_DISPLAY_H
#define INCLUDED_OBJECT_UI_DISPLAY_H

#include "../h-basic.h"

typedef struct object_type object_type;

/* Legacy term-grid subwindow renderers for PW_INVEN / PW_EQUIP. */
void display_inven(void);
void display_equip(void);

void story_print_equipment_prefix(int row, int col, byte attr, cptr prefix);
void story_prepare_equipment_desc(char* dest, size_t dest_size, cptr src,
    int slot, bool has_object, int max_cols);

bool supplies_visible_for_current_filter(void);
void format_supply_summary(char* buf, size_t len);

bool get_story_inventory_list_active(void);
void set_story_inventory_list_active(bool active);
bool get_story_equipment_list_active(void);
void set_story_equipment_list_active(bool active);

int draw_item_tile(int x, int y, object_type* o_ptr);
int menu_weight_col_for_width(int term_wid);
int menu_label_col_for_width(int term_wid, bool display_weights);
int menu_overlay_clear_col(int col);
int menu_desc_limit(int text_col, int label_col, int weight_col, bool display_weights);
int menu_inventory_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);
int menu_equipment_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);

#endif /* INCLUDED_OBJECT_UI_DISPLAY_H */
