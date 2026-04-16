#ifndef INCLUDED_UI_NARRATIVE_H
#define INCLUDED_UI_NARRATIVE_H

#include "h-basic.h"

void pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr);

extern const char entry_poetry[][100];
extern const char tutorial_leave_text[][100];
extern const char tutorial_win_text[][100];
extern const char tutorial_early_death_text[][100];
extern const char tutorial_late_death_text[][100];
extern const char throne_poetry[][100];
extern const char ultimate_bug_text[][100];

#endif /* INCLUDED_UI_NARRATIVE_H */
