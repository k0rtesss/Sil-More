#ifndef INCLUDED_OBJECT_COMPARE_H
#define INCLUDED_OBJECT_COMPARE_H

#include "angband.h"

/* Print observable item stats and known-safe replacement previews. Simulation
 * restores all player/equipment state and never identifies items or uses RNG.
 * The caller opts in only for a list whose first object is the candidate. */
bool object_compare_weapon_damage(const object_type* object, int* dd, int* ds);
void object_compare_summary_out(const object_type** objects,
    const char** headings, int count);
bool object_compare_has_targets(const object_type** objects, int count);

#endif
