/* File: object-ui-identify.h */

/*
 * Unified identify menu helpers.
 */

#ifndef INCLUDED_OBJECT_UI_IDENTIFY_H
#define INCLUDED_OBJECT_UI_IDENTIFY_H

#include "../h-basic.h"

typedef struct object_type object_type;

bool display_unified_identify_menu(bool include_floor, int* out_item,
    object_type** out_object);

#endif /* INCLUDED_OBJECT_UI_IDENTIFY_H */
