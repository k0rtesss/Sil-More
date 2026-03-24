/* File: object-internal.h */

/*
 * Internal scaffolding for the object subsystem split.
 * Shared object helper declarations move here as object1.c/object2.c shrink.
 */

#ifndef INCLUDED_OBJECT_INTERNAL_H
#define INCLUDED_OBJECT_INTERNAL_H

#include "h-basic.h"

typedef struct object_type object_type;

#define OBJECT_FLAGS_FULL 1
#define OBJECT_FLAGS_KNOWN 2

#define ENHANCED_MAX_LIST 80

#endif /* INCLUDED_OBJECT_INTERNAL_H */
