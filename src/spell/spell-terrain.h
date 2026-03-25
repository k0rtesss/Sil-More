/* File: spell/spell-terrain.h */

/*
 * Terrain manipulation, area destruction, and lighting effects.
 */

#ifndef INCLUDED_SPELL_TERRAIN_H
#define INCLUDED_SPELL_TERRAIN_H

#include "../h-basic.h"

void destroy_area(int y1, int x1, int r, bool full);
void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who);
bool close_chasm(int y, int x, int power);
bool close_chasms(int power);
void light_room(int y1, int x1);
void darken_room(int y1, int x1);
bool light_area(int dd, int ds, int rad);
bool darken_area(int dd, int ds, int rad);

#endif /* INCLUDED_SPELL_TERRAIN_H */
