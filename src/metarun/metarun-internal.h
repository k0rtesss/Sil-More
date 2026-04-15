#ifndef INCLUDED_METARUN_INTERNAL_H
#define INCLUDED_METARUN_INTERNAL_H

#include "../metarun.h"

/*
 * Wave 0 staging internal header for future metarun module extractions.
 *
 * New metarun-internal declarations should live here so the later
 * metarun implementation split does not need to grow src/externs.h.
 */

/*
 * Lane B transitional note:
 * these implementation fragments are still text-included by src/metarun.c so
 * Wave 1A can split ownership without taking root CMake changes in the same
 * lane.
 */

#define METARUN_HISTORY_PAGE_SIZE 48

static bool sync_current_metarun_slot(bool stamp_time);

#endif /* INCLUDED_METARUN_INTERNAL_H */
