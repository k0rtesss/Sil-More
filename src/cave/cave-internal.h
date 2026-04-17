#ifndef INCLUDED_CAVE_INTERNAL_H
#define INCLUDED_CAVE_INTERNAL_H

#include "../angband.h"
#include "cave.h"

/*
 * Wave 0 staging internal header for future cave module extractions.
 *
 * Cave split work should share declarations here rather than creating new
 * dependencies on src/externs.h.
 */

/*
 * Lane E cave-local helpers used to keep src/cave/cave.c focused on
 * cave-state orchestration while visuals own style-derived rendering details.
 */
byte cave_visuals_get_depth_color(int depth);
void cave_visuals_set_feat_with_color(int y, int x, int feat, int color);

#endif /* INCLUDED_CAVE_INTERNAL_H */
