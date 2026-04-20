/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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

bool build_player_subwindow_ui_scene(app_ui_scene* scene);
bool build_character_sheet_ui_scene(app_ui_scene* scene, cptr prompt_text);
void display_character_tutorial(void);
errr file_character(cptr name, bool full);

#endif /* INCLUDED_UI_CHARACTER_SCREEN_H */
