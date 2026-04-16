/* File: smithing.h */

/*
 * Transitional public API for the smithing split.
 *
 * Smithing logic still lives in cmd4.c on this branch. This header gives the
 * future smithing modules a stable place to own that surface.
 */

#ifndef INCLUDED_SMITHING_H
#define INCLUDED_SMITHING_H

#include "h-basic.h"

typedef struct object_type object_type;

extern object_type* smith_o_ptr;
extern int object_difficulty(object_type* o_ptr);
extern void create_smithing_item(void);
bool is_smithed_by_player(const object_type* o_ptr);

#endif /* INCLUDED_SMITHING_H */
