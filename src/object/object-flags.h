/* File: object-flags.h */

/*
 * Object flag accessor functions.
 * Provides access to an item's flags from base kind, artefact, and ego sources.
 */

#ifndef INCLUDED_OBJECT_FLAGS_H
#define INCLUDED_OBJECT_FLAGS_H

#include "h-basic.h"

typedef struct object_type object_type;

void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
void object_flags4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
void object_flags_known4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
bool object_grants_ability(const object_type* o_ptr, int skilltype, int abilitynum);

#endif /* INCLUDED_OBJECT_FLAGS_H */
