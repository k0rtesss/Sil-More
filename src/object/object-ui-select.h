/* File: object-ui-select.h */

/*
 * Object item selection UI.
 */

#ifndef INCLUDED_OBJECT_UI_SELECT_H
#define INCLUDED_OBJECT_UI_SELECT_H

#include "../h-basic.h"

void toggle_inven_equip(void);
bool get_item(int* cp, cptr pmt, cptr str, int mode);

#endif /* INCLUDED_OBJECT_UI_SELECT_H */
