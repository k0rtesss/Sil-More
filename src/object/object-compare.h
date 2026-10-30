#ifndef INCLUDED_OBJECT_COMPARE_H
#define INCLUDED_OBJECT_COMPARE_H

#include "angband.h"

/* Print item deltas, without simulating equipment or changing player state.
 * The caller opts in only for a list whose first object is the candidate. */
void object_compare_summary_out(const object_type** objects,
    const char** headings, int count);
bool object_compare_has_targets(const object_type** objects, int count);

#endif
