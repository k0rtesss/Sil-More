/* File: object-ui-select.h */

/*
 * Object item selection UI.
 */

#ifndef INCLUDED_OBJECT_UI_SELECT_H
#define INCLUDED_OBJECT_UI_SELECT_H

#include "../h-basic.h"

typedef struct object_type object_type;

extern bool item_tester_full;
extern byte item_tester_tval;
extern bool (*item_tester_hook)(const object_type*);

void toggle_inven_equip(void);
bool get_item(int* cp, cptr pmt, cptr str, int mode);

#endif /* INCLUDED_OBJECT_UI_SELECT_H */
