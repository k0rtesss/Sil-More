/* File: game-event.h */

/*
 * Narrative text display, dungeon utility functions, and shared event helpers.
 */

#ifndef INCLUDED_GAME_EVENT_H
#define INCLUDED_GAME_EVENT_H

#include "h-basic.h"

bool random_stair_location(int* sy, int* sx);
void break_truce(bool obvious);
bool similar_monsters(int m1y, int m1x, int m2y, int m2x);

void pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr);

extern const char entry_poetry[][100];
extern const char tutorial_leave_text[][100];
extern const char tutorial_win_text[][100];
extern const char tutorial_early_death_text[][100];
extern const char tutorial_late_death_text[][100];
extern const char throne_poetry[][100];
extern const char ultimate_bug_text[][100];

#endif /* INCLUDED_GAME_EVENT_H */
