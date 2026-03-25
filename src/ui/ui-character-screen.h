#ifndef INCLUDED_UI_CHARACTER_SCREEN_H
#define INCLUDED_UI_CHARACTER_SCREEN_H

#include "../h-basic.h"

enum {
    DISPLAY_PLAYER_MODE_STANDARD = 0,
    DISPLAY_PLAYER_MODE_FLAGS = 1,
    DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS = 100,
    DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS = 101,
    DISPLAY_PLAYER_MODE_COMPACT_SKILLS = 102,
    DISPLAY_PLAYER_MODE_COMPACT_HISTORY = 103,
};

void display_player(int mode);
void display_player_compact_set_scroll(int scroll);
int display_player_compact_get_max_scroll(void);
void display_player_compact_stats_skills_highlighted(int selected_skill);
void display_player_compact_stats_skills_highlighted_stat(int selected_stat);
void display_character_tutorial(void);

#endif /* INCLUDED_UI_CHARACTER_SCREEN_H */
