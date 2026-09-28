#ifndef INCLUDED_CAVE_LIGHT_H
#define INCLUDED_CAVE_LIGHT_H

#include "../h-basic.h"
typedef struct monster_type monster_type;

/* Physical contribution before visibility flags and the final light clamp. */
int cave_light_contribution(int light, bool glow, int sy, int sx, int y, int x);
void cave_light_remember_raw(int y, int x, int value, int darkness_delta);
void cave_light_remember_darkening(int y, int x, int darkness_delta);
bool cave_light_observed_change(const monster_type* observer, int y, int x,
    int old_contribution, int new_contribution, int* before, int* after);

#endif
