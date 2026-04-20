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

#ifndef INCLUDED_UI_SEMANTIC_SCENE_H
#define INCLUDED_UI_SEMANTIC_SCENE_H

#include "app/app-ui.h"
#include "ui/ui-information-scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_semantic_panel_config {
    u16b scene_flags;
    u16b layer;
    u16b style;
    u16b panel_flags;
    byte title_attr;
    byte subtitle_attr;
    byte accent_attr;
    u16b min_width_px;
    u16b width_cap_px;
    cptr title;
    cptr subtitle;
} ui_semantic_panel_config;

void ui_semantic_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
app_ui_panel* ui_semantic_scene_append_panel(app_ui_scene* scene,
    const ui_semantic_panel_config* config);
app_ui_panel* ui_semantic_scene_begin_panel(app_ui_scene* scene,
    const ui_semantic_panel_config* config);
app_ui_panel* ui_semantic_scene_begin_browser(app_ui_scene* scene,
    byte title_attr, cptr title, byte subtitle_attr, cptr subtitle,
    byte accent_attr, u16b panel_flags, u16b min_width_px,
    u16b width_cap_px);
app_ui_panel* ui_semantic_scene_begin_plain(app_ui_scene* scene,
    u16b scene_flags, u16b layer, byte title_attr, cptr title,
    byte subtitle_attr, cptr subtitle, byte accent_attr,
    u16b min_width_px, u16b width_cap_px);
bool ui_semantic_scene_present_and_wait_key(const app_ui_scene* scene,
    bool nonrepeat, bool hidden_cursor, u16b wait_reason, int* out_key);
void ui_semantic_scene_clear_pending_input(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_UI_SEMANTIC_SCENE_H */
