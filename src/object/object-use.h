/* File: object-use.h */

#ifndef INCLUDED_OBJECT_USE_H
#define INCLUDED_OBJECT_USE_H

#include "../h-basic.h"

int consumable_healing_points(const object_type* o_ptr);
bool use_object(object_type* o_ptr, bool* ident);
bool use_sanctity_gem_on(object_type* target_o_ptr, bool* ident);

#endif /* INCLUDED_OBJECT_USE_H */
