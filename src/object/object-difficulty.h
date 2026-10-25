#ifndef INCLUDED_OBJECT_DIFFICULTY_H
#define INCLUDED_OBJECT_DIFFICULTY_H

#include "angband.h"

/* One intrinsic valuation for drop entries, descriptions, and smithing.
 * Adjustments are percentage points, combined before the single division.
 * Character-specific smithing discounts never change the neutral item value. */
int object_difficulty_base(const object_type* object);
int object_difficulty_adjusted(const object_type* object, int percent_adjustment);

#endif
