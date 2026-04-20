/* File: ui-look-sidebar.h */
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

/*
 * Public sidebar helpers used by the unified look flow and option redraw paths.
 */

#ifndef INCLUDED_UI_LOOK_SIDEBAR_H
#define INCLUDED_UI_LOOK_SIDEBAR_H

#include "app/app-ui.h"
#include "h-basic.h"

typedef struct unified_look_state unified_look_state;

bool unified_look_build_menu_scene(unified_look_state* state, cptr title,
    app_ui_scene* scene);
int unified_look_find_cursor_selection(const unified_look_state* state,
    int cursor_y, int cursor_x);
void redraw_inven_equip_subwindows(void);
void redraw_monster_subwindows(void);

#endif /* INCLUDED_UI_LOOK_SIDEBAR_H */
