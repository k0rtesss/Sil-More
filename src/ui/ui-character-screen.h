#ifndef INCLUDED_UI_CHARACTER_SCREEN_H
#define INCLUDED_UI_CHARACTER_SCREEN_H

#include "../app/app-ui.h"
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
void display_player_compact_stats_skills_highlighted(int selected_skill);
void display_player_compact_stats_skills_highlighted_stat(int selected_stat);
bool build_character_sheet_ui_scene(app_ui_scene* scene, cptr prompt_text);
void display_character_tutorial(void);

#endif /* INCLUDED_UI_CHARACTER_SCREEN_H */
