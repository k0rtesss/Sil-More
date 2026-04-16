/* File: cmd-ui-query.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cmd-ui.h"
#include "log/log.h"
#include "platform-input.h"
#include "ui/ui-information-scene.h"

/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[]
    = { " :A dark grid", "!:A potion (or oil)", "\":An amulet", "#:A wall",
          /* "$:unused", */
          "%:A quartz vein", "&:A plant", "':An open door", "(:Soft armour",
          "):A shield", "*:A gem (or unseen monster)", "+:A closed door",
          ",:Food", "-:Arrows", ".:Floor", "/:An axe or polearm", "0:A forge",
          /* "1:unused", */
          /* "2:unused", */
          /* "3:unused", */
          /* "4:unused", */
          /* "5:unused", */
          /* "6:unused", */
          /* "7:unused", */
          /* "8:unused", */
          /* "9:unused", */
          "::Rubble", ";:A glyph of warding", "<:A staircase up", "=:A ring",
          ">:A staircase down", "?:An instrument", "@:Elf, Dwarf, or Man",
          /* "A:unused", */
          /* "B:unused", */
          "C:Canine", "D:Dragon",
          /* "E:unused", */
          /* "F:unused", */
          "G:Giant", "H:Horror", "I:Insect",
          /* "J:unused", */
          /* "K:unused", */
          /* "L:unused", */
          "M:Spider", "N:Nameless Thing",
          /* "O:unused", */
          "P:Giant",
          /* "Q:unused", */
          "R:Rauko", "S:Ancient Serpent", "T:Troll",
          /* "U:unused", */
          "V:Valar", "W:Wight/Wraith",
          /* "X:unused", */
          /* "Y:unused", */
          /* "Z:unused", */
          "[:Mail", "\\:A blunt weapon (or digger)", "]:Misc. armour",
          "^:A trap", "_:A staff",
          /* "`:unused", */
          /* "a:unused", */
          "b:Bat/Bird",
          /* "c:unused", */
          "d:Dragon",
          /* "e:unused", */
          "f:Feline",
          /* "g:unused", */
          /* "h:unused", */
          /* "i:unused", */
          /* "j:unused", */
          /* "k:unused", */
          /* "l:unused", */
          "m:Young Spider",
          /* "n:unused", */
          "o:Orc",
          /* "p:unused", */
          /* "q:unused", */
          /* "r:unused", */
          "s:Serpent",
          /* "t:unused", */
          /* "u:unused", */
          "v:Vampire", "w:Creeping Shadow",
          /* "x:unused", */
          /* "y:unused", */
          /* "z:unused", */
          /* "{:unused", */
          "|:An edged weapon (sword/dagger/etc)", "}:A bow",
          "~:A tool (or miscellaneous item)", NULL };

/*
 * Sorting hook -- Comp function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform on "u".
 */
bool ang_sort_comp_hook(const void* u, const void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b* why = (u16b*)(v);

    int w1 = who[a];
    int w2 = who[b];

    int z1, z2;

    /* Sort by player kills */
    if (*why >= 4)
    {
        /* Extract player kills */
        z1 = l_list[w1].pkills;
        z2 = l_list[w2].pkills;

        /* Compare player kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by total kills */
    if (*why >= 3)
    {
        /* Extract total kills */
        z1 = l_list[w1].tkills;
        z2 = l_list[w2].tkills;

        /* Compare total kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster level */
    if (*why >= 2)
    {
        /* Extract levels */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare levels */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster depth */
    if (*why >= 1)
    {
        /* Extract experience */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare experience */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Compare indexes */
    return (w1 <= w2);
}

/*
 * Sorting hook -- Swap function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform.
 */
void ang_sort_swap_hook(void* u, void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b holder;

    /* Unused parameter */
    (void)v;

    /* Swap */
    holder = who[a];
    who[a] = who[b];
    who[b] = holder;
}

static void query_symbol_build_snapshot_prompt(char* buf, size_t buflen,
    cptr summary, int index, int total)
{
    cptr text = summary ? summary : "";

    if (!buf || buflen == 0)
        return;

    if (total > 1)
    {
        strnfmt(buf, buflen, "%s [%d/%d]   - prev   any key next   Esc back",
            text, index + 1, total);
    }
    else
    {
        strnfmt(buf, buflen, "%s [Esc back]", text);
    }
}

typedef struct query_symbol_choice_row {
    char key;
    cptr label;
    cptr meta;
} query_symbol_choice_row;

static const query_symbol_choice_row query_symbol_choice_rows[] = {
    { 'y', "Recall details", "Current order" },
    { 'k', "Sort by kills", "Player kills, then total kills" },
    { 'p', "Sort by level", "Monster level" },
    { 'n', "Back", "Cancel" }
};

static bool query_symbol_build_choice_scene(app_ui_scene* scene, cptr summary,
    int match_count, int selected)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];
    bool steamdeck = steamdeck_controls_active();
    int i;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1500);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Query symbol");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Monster recall");

    if (summary && summary[0])
        (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, summary);

    strnfmt(status, sizeof(status), "%d matching monster%s.", match_count,
        (match_count == 1) ? "" : "s");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE, status);
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Choose recall order.");

    for (i = 0; i < (int)N_ELEMENTS(query_symbol_choice_rows); i++)
    {
        const query_symbol_choice_row* row = &query_symbol_choice_rows[i];

        if (!app_ui_panel_add_row(panel, (s16b)i, TERM_WHITE, true,
                i == selected, format("%c", row->key), row->label, row->meta))
        {
            return false;
        }
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            confirm_label, "Select");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            back_label, "Back");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Enter", "Select");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "y/k/p/n", "Shortcut");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "Esc", "Back");
    }

    return true;
}

static bool query_symbol_snapshot_choose_mode(cptr summary, int match_count,
    char* out_choice)
{
    ui_information_scene_scope scope;
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();

    if (out_choice)
        *out_choice = 'n';

    if (!ui_information_scene_enter(&scope))
        return false;

    while (1)
    {
        app_ui_scene scene;
        int ch;
        int d;

        if (!query_symbol_build_choice_scene(&scene, summary, match_count,
                selected)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            log_warn("query symbol: failed to present semantic choice scene");
            msg_print("Monster recall options unavailable.");
            return true;
        }

        ch = ui_information_scene_wait_key_nonrepeat();
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            if (out_choice)
                *out_choice = 'n';
            break;
        }

        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (out_choice)
                *out_choice = query_symbol_choice_rows[selected].key;
            break;
        }

        ch = (unsigned char)ch;
        if (ch == 'y' || ch == 'Y' || ch == 'k' || ch == 'K' || ch == 'p'
            || ch == 'P' || ch == 'n' || ch == 'N')
        {
            if (out_choice)
                *out_choice = (char)tolower(ch);
            break;
        }

        d = target_dir((char)ch);
        if (d)
        {
            if (ddy[d] > 0 && selected < (int)N_ELEMENTS(query_symbol_choice_rows) - 1)
                selected++;
            else if (ddy[d] < 0 && selected > 0)
                selected--;
        }
    }

    ui_information_scene_leave(&scope);
    return true;
}

static bool query_symbol_run_snapshot_recall_loop(u16b* who, int n,
    cptr summary)
{
    int i;
    bool steamdeck;

    if (!who || n <= 0)
        return true;

    i = n - 1;
    steamdeck = steamdeck_controls_active();

    while (1)
    {
        int r_idx = who[i];
        int key = ESCAPE;
        char prompt[192];

        monster_race_track(r_idx);
        handle_stuff();

        query_symbol_build_snapshot_prompt(prompt, sizeof(prompt), summary, i,
            n);
        if (!ui_information_scene_show_monster_recall(r_idx, NULL, prompt,
                true, &key))
        {
            log_warn("query symbol: semantic monster recall unavailable on the snapshot renderer path");
            bell("Monster recall screen unavailable.");
            msg_print("Monster recall screen unavailable.");
            return false;
        }

        if (steamdeck && key == steamdeck_back_key())
            key = ESCAPE;

        if (key == ESCAPE || key == 'q' || key == 'Q')
            break;

        if (key == 'r' || key == 'R')
            continue;

        if (key == '-' || key == '4')
        {
            if (++i == n)
                i = 0;
        }
        else
        {
            if (i-- == 0)
                i = n - 1;
        }
    }

    return true;
}

/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "multiple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 *
 * The responses may be sorted in several ways, see below.
 *
 *
 */
void do_cmd_query_symbol(void)
{
    int i, n;
    char sym, query;
    char buf[128];

    bool all = false;
    bool uniq = false;
    bool norm = false;

    u16b why = 0;
    u16b* who;

    /* Get a character, or abort */
    if (!get_com("Enter character to be identified: ", &sym))
        return;

    /* Find that character info, and describe it */
    for (i = 0; ident_info[i]; ++i)
    {
        if (sym == ident_info[i][0])
            break;
    }

    /* Describe */
    if (sym == KTRL('A'))
    {
        all = true;
        SDL_strlcpy(buf, "Full monster list.", sizeof(buf));
    }
    else if (sym == KTRL('U'))
    {
        all = uniq = true;
        SDL_strlcpy(buf, "Unique monster list.", sizeof(buf));
    }
    else if (sym == KTRL('N'))
    {
        all = norm = true;
        SDL_strlcpy(buf, "Non-unique monster list.", sizeof(buf));
    }
    else if (ident_info[i])
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, ident_info[i] + 2);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, "Unknown Symbol");
    }

    /* Display the result */
    message_topline_override(TERM_WHITE, buf);

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Nothing to recall */
        if (!cheat_know && !l_ptr->tsights && !know_monster_info)
            continue;

        /* Require non-unique monsters if needed */
        if (norm && (r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        /* Require unique monsters if needed */
        if (uniq && !(r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Collect "appropriate" monsters */
        if (all || (r_ptr->d_char == sym))
            who[n++] = i;
    }

    /* Nothing to recall */
    if (!n)
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    if (!query_symbol_snapshot_choose_mode(buf, n, &query))
    {
        log_warn("query symbol: semantic recall mode chooser unavailable");
        msg_print("Monster recall unavailable.");
        who = mem_free(who);
        return;
    }

    if (query == 'k')
        why = 4;
    else if (query == 'p')
        why = 2;

    if (query != 'y' && query != 'k' && query != 'p')
    {
        who = mem_free(who);
        return;
    }

    if (why)
    {
        ang_sort_comp = ang_sort_comp_hook;
        ang_sort_swap = ang_sort_swap_hook;
        ang_sort(who, &why, n);
    }

    if (!query_symbol_run_snapshot_recall_loop(who, n, buf))
    {
        log_warn("query symbol: semantic recall loop unavailable");
        msg_print("Monster recall unavailable.");
    }

    who = mem_free(who);
}
