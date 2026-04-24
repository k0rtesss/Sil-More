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

#include "angband.h"
#include "app/app-ui.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-narrative.h"

const char entry_poetry[][100] = { { "Into the vast and echoing gloom," },
    { "more dread than many-tunnelled tomb" },
    { "  down awful corridors that wind" },
    { "    down to a menace dark enshrined;" },
    { "      down to the mountain's roots profound," },
    { "devoured, tormented, bored and ground" },
    { "by seething vermin spawned of stone;" },
    { "  down to the depths they went alone..." },

    { "" } };

const char tutorial_leave_text[][100] = {
    { "You have finished the first half of the tutorial and are ready" },
    { "to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "Congratulations. You have survived a fire-drake (usually found" },
    { "at 900 ft!), and have finished the tutorial in fine form." },
    { "You are more than ready to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "A key feature of Sil (and all Roguelike games) is that you cannot" },
    { "use savepoints: if you die, that's it!" },
    { "It is thus a challenging game where you need to really *think*." },
    { " " },
    { "However, it is a bit frustrating to die before the end of the" },
    { "tutorial, so we evidentally made it a bit too deadly." }, { " " },
    { "Just restart the tutorial and you should be back to where you" },
    { "were in a couple of minutes. Remember that if combat is not going" },
    { "your way, you can try to escape and heal, then either come back" },
    { "and again to defeat your adversary, or simply ignore it." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "Congratulations: you have finished the tutorial." }, { " " },
    { "You have also just been through a rite of passage: dying." },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },
    { " " },
    { "You are now more than ready to create a character and start playing." },
    { " " }, { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." },

    { "" }
};

const char throne_poetry[][100] = { { "Loud rose a din of laughter hoarse," },
    { "  self-loathing yet without remorse;" },
    { "    loud came a singing harsh and fierce" },
    { "      like swords of terror souls to pierce." },
    { "Red was the glare through open doors" },
    { "  of firelight mirrored on brazen floors," },
    { "    and up the arches towering clomb" },
    { "      to glooms unguessed, to vaulted dome" },
    { "        swathed in wavering smokes and steams" },
    { "          stabbed with flickering lightning-gleams." },

    { "" } };

const char ultimate_bug_text[][100]
    = { { "Against all hope, you defeated the Dark Enemy," },
          { "  and destroyed his physical form." },
          { "    For the rest of this age at least," },
          { "      Arda shall be free from the tyrant's shadow." },
          { "But there will be time later for reflection" },
          { "  on this great change to Arda's fate." },
          { "    You are buried still in Angband's vaults" },
          { "      -- make quick your bold escape!" },

          { "" } };

static bool pause_with_text_line_is_blank(cptr text)
{
    while (text && *text)
    {
        if (!isspace((unsigned char)*text))
            return false;
        text++;
    }

    return true;
}

static bool pause_with_text_append_line_block(app_ui_scene* scene,
    app_ui_panel* panel, const char lines[][100], byte attr)
{
    bool paragraph_open = false;
    bool line_open = false;
    bool wrote_any = false;

    if (!scene || !panel || !lines)
        return false;

    for (int i = 0; lines[i][0]; i++)
    {
        if (pause_with_text_line_is_blank(lines[i]))
        {
            paragraph_open = false;
            line_open = false;
            continue;
        }

        if (!paragraph_open)
        {
            if (!app_ui_panel_begin_rich_paragraph(scene, panel))
                return false;
            paragraph_open = true;
            line_open = false;
        }
        else if (line_open
            && !app_ui_panel_add_rich_text_ex(scene, panel, attr,
                STORY_FLAG_USE, "\n"))
        {
            return false;
        }

        if (!app_ui_panel_add_rich_text_ex(scene, panel, attr, STORY_FLAG_USE,
                lines[i]))
        {
            return false;
        }

        line_open = true;
        wrote_any = true;
    }

    return wrote_any;
}

static bool pause_with_text_build_ui_scene(app_ui_scene* scene, int row, int col,
    const char desc[][100], const char extra[][100], byte extra_attr,
    bool overlay_dungeon)
{
    app_ui_panel* panel;

    if (!scene)
        return false;
    (void)row;
    (void)col;

    app_ui_scene_init(scene);
    if (overlay_dungeon)
        scene->flags |= APP_UI_SCENE_FLAG_USE_BACKDROP;
    panel = app_ui_scene_append_panel(scene,
        overlay_dungeon ? APP_UI_LAYER_TRANSIENT : APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = extra_attr ? extra_attr : TERM_SLATE;
    app_ui_panel_set_widths(panel,
        overlay_dungeon ? 1100 : 980,
        overlay_dungeon ? 1900 : 1700);

    if (extra && extra[0][0]
        && !pause_with_text_append_line_block(scene, panel, extra, extra_attr))
    {
        return false;
    }

    if (!pause_with_text_append_line_block(scene, panel, desc, TERM_WHITE))
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        if (!app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
                STORY_FLAG_USE, " "))
        {
            return false;
        }
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "Space", "Continue");
}

static bool pause_with_text_scene_enter(ui_information_scene_scope* scope,
    bool* overlay_dungeon)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (overlay_dungeon)
        *overlay_dungeon = false;
    if (!scope || !session)
        return false;
    if (!ui_information_scene_enter(scope))
        return false;

    snapshot = app_session_snapshot(session);
    if (overlay_dungeon && snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON)
        *overlay_dungeon = true;
    return true;
}

static bool pause_with_text_scene_present(const app_ui_scene* scene)
{
    return scene ? ui_information_scene_present_ui(scene) : false;
}

static void pause_with_text_wait_continue(void)
{
    ui_information_scene_event event;

    while (ui_information_scene_wait_event(&event, APP_INPUT_FLAG_REPEAT))
    {
        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        if (event.command.kind == APP_UI_COMMAND_KIND_CANCEL
            || event.command.kind == APP_UI_COMMAND_KIND_ACTIVATE
            || event.command.kind == APP_UI_COMMAND_KIND_SELECT)
        {
            return;
        }
    }
}

void pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    bool overlay_dungeon = false;

    if (!pause_with_text_scene_enter(&scope, &overlay_dungeon))
    {
        log_error("pause_with_text: semantic scene unavailable");
        quit("Narrative pause requires the snapshot UI renderer.");
        return;
    }

    if (!pause_with_text_build_ui_scene(&scene, row, col, desc, extra,
            extra_attr, overlay_dungeon)
        || !pause_with_text_scene_present(&scene))
    {
        ui_information_scene_leave(&scope);
        log_error("pause_with_text: semantic scene presentation failed");
        quit("Narrative pause could not be displayed.");
        return;
    }

    {
        app_session* session = app_session_current();

        pause_with_text_wait_continue();
        if (session)
            app_session_clear_inputs(session);
    }
    ui_information_scene_leave(&scope);
}
