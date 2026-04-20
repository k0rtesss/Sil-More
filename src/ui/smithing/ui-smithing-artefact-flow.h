/* File: ui-smithing-artefact-flow.h */
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

/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static void smith_ui_artefact_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;
    bool leave_menu = false;
    char buf[36];

    log_info("Player opened artifact creation menu");
    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            smith2_o_ptr->pd = 1;
        }
    }

    if (strlen(smith2_a_ptr->name) == 0)
    {
        sprintf(buf, "of %s", op_ptr->full_name);
        SDL_strlcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
    }

    prepare_artefact();
    smith_ui_artefact_backup_current_state();

    while (!leave_menu)
    {
        app_ui_scene scene;
        int count = smith_ui_artefact_root_entry_count();
        int choice;
        char ch;

        prepare_artefact();
        if (highlight < 1 || highlight > count)
            highlight = 1;
        if (!smith_ui_artefact_root_build_scene(&scene, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot artefact menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = count;
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < count)
                highlight++;
            else
                highlight = 1;
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            leave_menu = true;
            continue;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if (highlight == count)
        {
            rename_artefact();
            smith_ui_artefact_backup_current_state();
        }
        else if (highlight <= MAX_CATS)
        {
            smith_ui_snapshot_begin_nested_transition();
            smith_ui_snapshot_scene_close(&scope);
            smith_ui_artefact_flag_snapshot_menu(highlight);
            smith_ui_snapshot_end_nested_transition();
            if (!smith_ui_snapshot_scene_enter(&scope))
                return;
        }
        else
        {
            int skill = smith_ui_artefact_root_skill(highlight);

            if (skill >= 0)
            {
                smith_ui_snapshot_begin_nested_transition();
                smith_ui_snapshot_scene_close(&scope);
                smith_ui_artefact_ability_snapshot_menu(skill);
                smith_ui_snapshot_end_nested_transition();
                if (!smith_ui_snapshot_scene_enter(&scope))
                    return;
            }
        }
    }

    prepare_artefact();
    smith_ui_snapshot_scene_close(&scope);
}
