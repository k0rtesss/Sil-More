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

#ifndef INCLUDED_SCORE_UI_BROWSER_H
#define INCLUDED_SCORE_UI_BROWSER_H

#include "app/app-ui.h"

void score_ui_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
app_ui_panel* score_ui_begin_browser_scene(app_ui_scene* scene,
    u16b panel_flags);

#endif /* INCLUDED_SCORE_UI_BROWSER_H */
