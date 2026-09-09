#ifndef INCLUDED_CAVE_FIXTURES_H
#define INCLUDED_CAVE_FIXTURES_H

#include "h-basic.h"

/* Decorations do not change collision, light propagation, or item placement. */
enum cave_fixture_kind {
    CAVE_FIXTURE_NONE,
    CAVE_FIXTURE_WALL_TORCH,
    CAVE_FIXTURE_BRAZIER
};

#define SAVEFILE_FIXTURES_MAGIC 0xF178
void cave_fixtures_clear(void);
byte cave_fixture_at(int y, int x);
void cave_fixture_set(int y, int x, byte kind);

#endif
