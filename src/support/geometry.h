#ifndef INCLUDED_SUPPORT_GEOMETRY_H
#define INCLUDED_SUPPORT_GEOMETRY_H

#include "h-basic.h"

extern byte get_angle_to_grid[41][41];
/* Pure angular test shared by real arc projection and monster previews. */
bool projection_arc_contains(int centerline, int dy, int dx, int degrees);
int get_angle_to_target(int y0, int x0, int y1, int x1, int dir);
void get_grid_using_angle(int angle, int y0, int x0, int* ty, int* tx);

#endif /* INCLUDED_SUPPORT_GEOMETRY_H */
