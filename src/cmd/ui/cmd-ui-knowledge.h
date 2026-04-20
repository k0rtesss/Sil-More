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

#ifndef INCLUDED_CMD_UI_KNOWLEDGE_H
#define INCLUDED_CMD_UI_KNOWLEDGE_H

/*
 * Temporary shared surface for the split knowledge implementation files.
 *
 * Wave 1A keeps the browser, supplies, and root-menu helpers local to this
 * command family until the broader informational-scene helpers land in
 * Wave 2.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "ui/ui-information-scene.h"
#include "cmd-ui.h"

#define KNOWLEDGE_BROWSER_ROWS 16

typedef struct monster_list_entry
{
    s16b r_idx;
    byte amount;
} monster_list_entry;

typedef struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval;
    int sval;
} object_list_entry;

typedef struct supply_list_entry
{
    int item_idx;   /* First inventory slot containing this kind */
    int k_idx;      /* Object kind index */
    int total;      /* Total quantity across the pack */
    int supply_idx; /* Index inside the supply cache (-1 if not present) */
} supply_list_entry;

typedef struct knowledge_browser_state
{
    int column[4];
    int group_cur[4];
    int group_top[4];
    int entry_cur[4];
    int entry_top[4];
    bool tabs_focus;
} knowledge_browser_state;

bool knowledge_pause_information_scene(ui_information_scene_scope* scope);
bool knowledge_resume_information_scene(ui_information_scene_scope* scope);
bool knowledge_enter_information_scene_or_report(
    ui_information_scene_scope* scope, cptr log_name, cptr unavailable_message);
bool knowledge_present_ui_scene_or_abort(
    ui_information_scene_scope* scope, bool build_ok, app_ui_scene* scene,
    cptr scene_name, cptr user_message);

void knowledge_set_last_page(int page);
void knowledge_browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows);
int knowledge_normalize_page(int page);
bool knowledge_handle_page_input(char ch, int* page);
bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs);
bool knowledge_is_recall_input(int ch);

app_ui_panel* knowledge_scene_begin(app_ui_scene* scene, int page,
    bool tabs_focus, cptr status);
void knowledge_scene_set_focus(app_ui_panel* panel, bool tabs_focus);

void knowledge_desc_obj_fake(int k_idx);

#endif /* INCLUDED_CMD_UI_KNOWLEDGE_H */
