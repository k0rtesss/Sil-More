/* File: object-ui-display.h */

/*
 * Shared object UI display helpers.
 * Used by both the classic inventory/equipment lists and the enhanced menus.
 */

#ifndef INCLUDED_OBJECT_UI_DISPLAY_H
#define INCLUDED_OBJECT_UI_DISPLAY_H

#include "../h-basic.h"

typedef struct object_type object_type;

void display_inven(void);
void display_equip(void);
void show_inven(void);
void show_equip(void);
void show_floor(const int* floor_list, int floor_num);

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
int menu_term_width(void);
int menu_weight_col_for_width(int term_wid);
int menu_label_col_for_width(int term_wid, bool display_weights);
int menu_center_col_for_len(int term_wid, int len);
int menu_desc_limit(int text_col, int label_col, int weight_col, bool display_weights);
int menu_inventory_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);
int menu_equipment_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights);

void story_render_inventory_entry(int row, int base_col, int label_col,
    cptr desc, byte desc_attr, bool display_weights, cptr weight_text,
    byte weight_attr, cptr label_text, byte label_attr, const object_type* o_ptr,
    bool highlight, int story_term_w);
void story_render_equipment_entry(int row, int col, int slot, cptr prefix,
    byte prefix_attr, cptr desc, byte desc_attr, bool display_weights,
    cptr weight_text, byte weight_attr, cptr label_text, byte label_attr,
    const object_type* o_ptr, bool highlight, int story_term_w);
void draw_equipment_story_rows(int col, int entry_count, int* out_index,
    byte* out_color, char out_desc[][80], bool highlight_active,
    int highlight_index, bool display_weights, int story_term_w);

#endif /* INCLUDED_OBJECT_UI_DISPLAY_H */
