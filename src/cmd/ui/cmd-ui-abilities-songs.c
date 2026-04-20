/* File: cmd-ui-abilities-songs.c */
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
 * Lane-local song selection helpers split from cmd-ui-abilities-scenes.c.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "cmd-ui-abilities.h"
#include "cmd-ui-abilities-scenes.h"
#include "cmd-ui.h"
#include "log/log.h"
#include "platform-input.h"
#include "player/player-abilities.h"
#include "player/player-oaths.h"
#include "ui/ui-information-scene.h"

#include <ctype.h>

/* ------------------------------------------------------------------
 * add_random_curse()
 *   Marks the item cursed.
 *   Gives it random negative modifiers.
 * ------------------------------------------------------------------ */
void add_random_curse(object_type* o_ptr)
{
    int old_pval;
    int pval_delta;

    /* Make it show up as cursed right away. */
    o_ptr->ident |= IDENT_CURSED;

    /* Negative pval / attack / evasion. */
    old_pval = o_ptr->pval;
    if (o_ptr->pval > 0)
        o_ptr->pval = -(rand_int(3) + 1);
    pval_delta = o_ptr->pval - old_pval;
    if (pval_delta != 0)
    {
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr),
            pval_delta);
    }
    if (o_ptr->att > 0)
        o_ptr->att = -(rand_int(3) + 1);
    if (o_ptr->evn > 0)
        o_ptr->evn = -(rand_int(3) + 1);

    /* Very small chance to damage dice on weapons / armour. */
    if (one_in_(8))
    {
        if (o_ptr->dd)
            o_ptr->dd = MAX(1, o_ptr->dd - 1);
        if (o_ptr->pd)
            o_ptr->pd = MAX(1, o_ptr->pd - 1);
    }
}

static char song_menu_letter(int song_index)
{
    char letter = (char)('a' + song_index);

    if (letter >= 's')
        letter++;

    return letter;
}

typedef struct song_semantic_entry {
    int song;
    char key;
    byte attr;
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} song_semantic_entry;

static void song_semantic_song_name(int song, char* out, size_t outsz)
{
    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (song == SNG_NOTHING)
    {
        SDL_strlcpy(out, "Stop singing", outsz);
        return;
    }
    if (song == SNG_EXCHANGE_THEMES)
    {
        SDL_strlcpy(out, "Exchange themes", outsz);
        return;
    }
    if (song >= 0 && song < SNG_MAX)
    {
        SDL_strlcpy(out,
            b_name + (&b_info[ability_index(S_SNG, song)])->name, outsz);
        return;
    }

    SDL_strlcpy(out, "<unknown song>", outsz);
}

static void song_semantic_current_summary(char* out, size_t outsz)
{
    char song1[APP_UI_LABEL_MAX];
    char song2[APP_UI_LABEL_MAX];

    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (p_ptr->song1 == SNG_NOTHING && p_ptr->song2 == SNG_NOTHING)
    {
        SDL_strlcpy(out, "Current: silence", outsz);
        return;
    }

    song1[0] = '\0';
    song2[0] = '\0';
    if (p_ptr->song1 != SNG_NOTHING)
        song_semantic_song_name(p_ptr->song1, song1, sizeof(song1));
    if (p_ptr->song2 != SNG_NOTHING)
        song_semantic_song_name(p_ptr->song2, song2, sizeof(song2));

    if (song1[0] && song2[0])
    {
        strnfmt(out, outsz, "Current: %s + %s", song1, song2);
    }
    else if (song1[0])
    {
        strnfmt(out, outsz, "Current: %s", song1);
    }
    else if (song2[0])
    {
        strnfmt(out, outsz, "Current: %s", song2);
    }
    else
    {
        SDL_strlcpy(out, "Current: silence", outsz);
    }
}

static int song_semantic_collect_entries(song_semantic_entry* out, int max_count)
{
    int count = 0;
    int i;

    if (!out || max_count <= 0)
        return 0;

    memset(&out[count], 0, sizeof(out[count]));
    out[count].song = SNG_NOTHING;
    out[count].key = 's';
    out[count].attr =
        (p_ptr->song1 == SNG_NOTHING && p_ptr->song2 == SNG_NOTHING)
            ? TERM_L_BLUE
            : TERM_SLATE;
    SDL_strlcpy(out[count].label, "Stop singing", sizeof(out[count].label));
    if (p_ptr->song1 == SNG_NOTHING && p_ptr->song2 == SNG_NOTHING)
        SDL_strlcpy(out[count].meta, "Current", sizeof(out[count].meta));
    count++;

    for (i = 0; i < SNG_MAX && count < max_count; i++)
    {
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;
        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        memset(&out[count], 0, sizeof(out[count]));
        out[count].song = i;
        out[count].key = song_menu_letter(i);
        out[count].attr = TERM_WHITE;
        song_semantic_song_name(i, out[count].label,
            sizeof(out[count].label));

        if (p_ptr->song1 == i)
        {
            out[count].attr = TERM_L_BLUE;
            SDL_strlcpy(out[count].meta, "Main", sizeof(out[count].meta));
        }
        else if (p_ptr->song2 == i)
        {
            out[count].attr = TERM_BLUE;
            SDL_strlcpy(out[count].meta, "Minor", sizeof(out[count].meta));
        }

        count++;
    }

    if (p_ptr->song2 != SNG_NOTHING && count < max_count)
    {
        memset(&out[count], 0, sizeof(out[count]));
        out[count].song = SNG_EXCHANGE_THEMES;
        out[count].key = 'x';
        out[count].attr = TERM_L_BLUE;
        SDL_strlcpy(out[count].label, "Exchange themes",
            sizeof(out[count].label));
        SDL_strlcpy(out[count].meta, "Swap", sizeof(out[count].meta));
        count++;
    }

    return count;
}

static int song_semantic_default_index(const song_semantic_entry* entries,
    int count)
{
    int i;

    if (!entries || count <= 0)
        return 0;

    for (i = 0; i < count; i++)
    {
        if (entries[i].song == p_ptr->song1)
            return i;
    }

    return 0;
}

static int song_semantic_find_index_for_key(const song_semantic_entry* entries,
    int count, int ch)
{
    int i;
    char key = (char)tolower((unsigned char)ch);

    if (!entries)
        return -1;

    for (i = 0; i < count; i++)
    {
        if (entries[i].key == key)
            return i;
    }

    return -1;
}

static void song_semantic_add_detail(app_ui_panel* panel,
    const song_semantic_entry* entry)
{
    ability_type* b_ptr;
    char title[APP_UI_TITLE_MAX];

    if (!panel || !entry)
        return;

    song_semantic_song_name(entry->song, title, sizeof(title));
    app_ui_panel_set_detail_title(panel, TERM_YELLOW, title);

    if (entry->song == SNG_NOTHING)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "End your current song or minor theme.");
        return;
    }

    if (entry->song == SNG_EXCHANGE_THEMES)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Swap the order of your current major and minor themes.");
        return;
    }

    if (p_ptr->song1 == entry->song)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
            "Currently your main song.");
        if (p_ptr->song2 != SNG_NOTHING)
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
                "Choosing it again will end the current minor theme.");
        }
    }
    else if (p_ptr->song2 == entry->song)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_BLUE,
            "Currently your minor theme.");
    }

    b_ptr = &b_info[ability_index(S_SNG, entry->song)];
    ability_semantic_add_description_lines(panel, S_SNG, b_ptr);
}

static bool song_semantic_build_scene(app_ui_scene* scene,
    const song_semantic_entry* entries, int count, int selected,
    byte status_attr, cptr status)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    int i;

    if (!scene || !entries || count <= 0)
        return false;

    if (selected < 0)
        selected = 0;
    if (selected >= count)
        selected = count - 1;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 1700);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Songs of Power");
    song_semantic_current_summary(subtitle, sizeof(subtitle));
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    if (status && status[0])
    {
        (void)app_ui_panel_add_body_line(panel, status_attr, status);
    }
    else
    {
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
            "Choose a song, stop singing, or swap your woven themes.");
    }

    for (i = 0; i < count; i++)
    {
        char keybuf[APP_UI_KEY_MAX];

        strnfmt(keybuf, sizeof(keybuf), "%c", entries[i].key);
        (void)app_ui_panel_add_row_ex(panel, (s16b)entries[i].song,
            entries[i].attr, TERM_SLATE, 0, '\0', true, i == selected, keybuf,
            entries[i].label, entries[i].meta);
    }

    song_semantic_add_detail(panel, &entries[selected]);

    if (steamdeck_controls_active())
    {
        char confirm_label[APP_UI_KEY_MAX];
        char back_label[APP_UI_KEY_MAX];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
            confirm_label, "Select");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            back_label, "Back");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
            "Enter", "Select");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "Esc", "Back");
    }

    panel->focus_area = APP_UI_FOCUS_ROWS;
    return true;
}

void do_cmd_change_song(void)
{
    ui_information_scene_scope scope;
    song_semantic_entry entries[32];
    char status[APP_UI_TEXT_MAX];
    byte status_attr = TERM_SLATE;
    int entry_count;
    int selected;
    int song_choice = -1;
    bool done = false;

    log_debug("Player opening song selection menu");

    if (p_ptr->song_lockout_timer > 0)
    {
        msg_format("You cannot sing for %d more turn%s.",
            p_ptr->song_lockout_timer,
            (p_ptr->song_lockout_timer == 1) ? "" : "s");
        return;
    }

    entry_count = song_semantic_collect_entries(entries, N_ELEMENTS(entries));
    if (entry_count <= 1)
    {
        log_trace("No songs available - player knows no songs of power");
        msg_print("You do not know any songs of power.");
        return;
    }

    if (!ui_information_scene_enter(&scope))
        return;

    status[0] = '\0';
    selected = song_semantic_default_index(entries, entry_count);

    while (!done)
    {
        app_ui_scene scene;
        int ch;
        int key_index;

        if (!song_semantic_build_scene(&scene, entries, entry_count, selected,
                status_attr, status)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            log_warn("song selection: semantic scene presentation failed");
            msg_print("Song selection unavailable.");
            return;
        }

        ch = ui_information_scene_wait_key();
        if (steamdeck_controls_active())
        {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
        }

        status[0] = '\0';
        key_index = song_semantic_find_index_for_key(entries, entry_count, ch);
        if (key_index >= 0)
        {
            song_choice = entries[key_index].song;
            done = true;
            continue;
        }

        if ((ch == ESCAPE) || (ch == 'q') || (ch == 'Q') || (ch == '4'))
        {
            done = true;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            song_choice = entries[selected].song;
            done = true;
            continue;
        }

        if (ch == '8'
#ifdef ARROW_UP
            || ch == ARROW_UP
#endif
        )
        {
            selected = (selected + entry_count - 1) % entry_count;
            continue;
        }

        if (ch == '2'
#ifdef ARROW_DOWN
            || ch == ARROW_DOWN
#endif
        )
        {
            selected = (selected + 1) % entry_count;
            continue;
        }

        bell("Illegal song choice.");
        status_attr = TERM_RED;
        SDL_strlcpy(status, "Illegal song choice.", sizeof(status));
    }

    ui_information_scene_leave(&scope);

    if (song_choice < 0)
    {
        log_trace("Song selection cancelled by player");
        return;
    }

    if (song_choice != SNG_NOTHING
        && chosen_oath(OATH_SILENCE) && !oath_invalid(OATH_SILENCE))
    {
        char* prompt = oath_confirmation_prompt(OATH_SILENCE);

        if (!prompt || !prompt[0])
            prompt =
                "Are you certain you wish to break your Oath of Silence?";

        if (get_check_oath_multiline(prompt))
        {
            log_info("Player broke oath of silence to sing");
            do_cmd_note("Broke your oath", p_ptr->depth);
            apply_oath_breaking_curse(OATH_SILENCE);
            p_ptr->oaths_broken |= OATH_SILENCE_FLAG;
        }
        else
        {
            log_debug("Player cancelled song due to oath of silence");
            return;
        }
    }

    log_info("Player changed song to %s",
        song_choice == SNG_NOTHING ? "silence"
                                   : song_choice == SNG_EXCHANGE_THEMES
                                       ? "exchange themes"
                                       : "new song");
    change_song(song_choice);
}
