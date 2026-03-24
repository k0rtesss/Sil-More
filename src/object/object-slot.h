/* File: object-slot.h */

/*
 * Object slot and index utilities.
 * Functions for converting between item indices, labels, and slots.
 */

#ifndef INCLUDED_OBJECT_SLOT_H
#define INCLUDED_OBJECT_SLOT_H

#include "h-basic.h"

typedef struct object_type object_type;

char index_to_label(int i);
s16b label_to_inven(int c);
s16b label_to_equip(int c);
s16b wield_slot(const object_type* o_ptr);
cptr describe_empty_slot(int i);
cptr mention_use(int i);
cptr describe_use(int i);
bool item_tester_okay(const object_type* o_ptr);
int scan_floor(int* items, int size, int y, int x, int mode);

#endif /* INCLUDED_OBJECT_SLOT_H */
