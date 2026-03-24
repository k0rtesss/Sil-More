/* File: object-desc.h */

/*
 * Object description generation.
 * Creates textual descriptions of objects for display.
 */

#ifndef INCLUDED_OBJECT_DESC_H
#define INCLUDED_OBJECT_DESC_H

#include "h-basic.h"

typedef struct object_type object_type;

void strip_name(char* buf, int k_idx);
void object_desc(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void object_desc_floor(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void object_desc_spoil(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void identify_random_gen(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_DESC_H */
