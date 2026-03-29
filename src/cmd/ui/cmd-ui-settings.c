/* File: cmd-ui-settings.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-information-scene.h"

#define COLOR_SAMPLE "###"

/*
 * Settings scene bridge: when an information scene scope is active, mirror
 * the current Term contents into the scene and wait for input through the
 * information scene channel.  Falls back to inkey() in legacy mode.
 */
static int settings_wait_key(void)
{
    if (ui_information_scene_is_active())
    {
        (void)ui_information_scene_present_term();
        return ui_information_scene_wait_key();
    }
    return inkey();
}

static cptr dump_seperator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";

static void dump_visual_pair(
    SDL_IOStream* fff, const char* tag, int index, byte attr, byte chr)
{
    bool attr_tile = (attr & TILE_FLAG) != 0;
    bool char_tile = (chr & TILE_FLAG) != 0;

    SDL_IOprintf(fff, "%s:%d:", tag, index);
    if (attr_tile)
        SDL_IOprintf(fff, "R%d", TILE_GET_INDEX(attr));
    else
        SDL_IOprintf(fff, "0x%02X", attr);

    SDL_WriteU8(fff, ':');

    if (char_tile)
        SDL_IOprintf(fff, "C%d", TILE_GET_INDEX(chr));
    else
        SDL_IOprintf(fff, "0x%02X", (byte)chr);

SDL_WriteU8(fff, '\n');
SDL_WriteU8(fff, '\n');
}

/*
 * Remove old lines from pref files
 */
static void remove_old_dump(cptr orig_file, cptr mark)
{
    SDL_IOStream* tmp_fff, *orig_fff;

    char tmp_file[1024];
    char buf[1024];
    bool between_marks = false;
    bool changed = false;
    char expected_line[1024];

    /* Open an old dump file in read-only mode */
    orig_fff = sdl_fopen(orig_file, "r");

    /* If original file does not exist, nothing to do */
    if (!orig_fff)
        return;

    /* Open a new temporary file */
    tmp_fff = sdl_fopen_temp(tmp_file, sizeof(tmp_file));

    if (!tmp_fff)
    {
        msg_format("Failed to create temporary file %s.", tmp_file);
        msg_print(NULL);
        return;
    }

    strnfmt(expected_line, sizeof(expected_line), "%s begin %s", dump_seperator,
        mark);

    /* Loop for every line */
    while (true)
    {
        /* Read a line */
        if (sdl_fgets(orig_fff, buf, sizeof(buf)))
        {
            /* End of file but no end marker */
            if (between_marks)
                changed = false;

            break;
        }

        /* Is this line a header/footer? */
        if (strncmp(buf, dump_seperator, strlen(dump_seperator)) == 0)
        {
            /* Found the expected line? */
            if (strcmp(buf, expected_line) == 0)
            {
                if (!between_marks)
                {
                    /* Expect the footer next */
                    strnfmt(expected_line, sizeof(expected_line), "%s end %s",
                        dump_seperator, mark);

                    between_marks = true;

                    /* There are some changes */
                    changed = true;
                }
                else
                {
                    /* Expect a header next - XXX shouldn't happen */
                    strnfmt(expected_line, sizeof(expected_line), "%s begin %s",
                        dump_seperator, mark);

                    between_marks = false;

                    /* Next line */
                    continue;
                }
            }
            /* Found a different line */
            else
            {
                /* Expected a footer and got something different? */
                if (between_marks)
                {
                    /* Abort */
                    changed = false;
                    break;
                }
            }
        }

        if (!between_marks)
        {
            /* Copy orginal line */
            SDL_IOprintf(tmp_fff, "%s\n", buf);
        }
    }

    /* Close files */
    sdl_fclose(orig_fff);
    sdl_fclose(tmp_fff);

    /* If there are changes, overwrite the original file with the new one */
    if (changed)
    {
        /* Copy contents of temporary file */
        tmp_fff = sdl_fopen(tmp_file, "r");
        orig_fff = sdl_fopen(orig_file, "w");

        while (!sdl_fgets(tmp_fff, buf, sizeof(buf)))
        {
            SDL_IOprintf(orig_fff, "%s\n", buf);
        }

        sdl_fclose(orig_fff);
        sdl_fclose(tmp_fff);
    }

    /* Kill the temporary file */
    fd_kill(tmp_file);
}

/*
 * Output the header of a pref-file dump
 */
static void pref_header(SDL_IOStream* fff, cptr mark)
{
    /* Start of dump */
    SDL_IOprintf(fff, "%s begin %s\n", dump_seperator, mark);

    SDL_IOprintf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");
}

/*
 * Output the footer of a pref-file dump
 */
static void pref_footer(SDL_IOStream* fff, cptr mark)
{
    SDL_IOprintf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");

    /* End of dump */
    SDL_IOprintf(fff, "%s end %s\n", dump_seperator, mark);
}

/*
 * Ask for a "user pref line" and process it
 */
void do_cmd_pref(void)
{
    char tmp[80];

    /* Default */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Ask for a "user pref command" */
    if (!term_get_string("Pref: ", tmp, sizeof(tmp)))
        return;

    /* Process that pref command */
    (void)process_pref_file_command(tmp);
}

/*
 * Ask for a "user pref file" and process it.
 *
 * This function should only be used by standard interaction commands,
 * in which a standard "Command:" prompt is present on the given row.
 *
 * Allow absolute file names?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(int row)
{
    char ftmp[80];

    /* Prompt */
    Term_putstr(2, row + 2, -1, TERM_SLATE, "(Escape to cancel)");

    /* Prompt */
    prt("File: ", row, 2);

    /* Default filename */
    strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

    /* Ask for a file (or cancel) */
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;

    /* Process the given filename */
    if (process_pref_file(ftmp))
    {
        /* Mention failure */
        msg_format("Failed to load '%s'!", ftmp);
    }
    else
    {
        /* Mention success */
        msg_format("Loaded '%s'.", ftmp);
    }
}

void clear_skills_and_abilities()
{
    int i, j;

    /* Clear the base values of the skills */
    for (i = 0; i < A_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    ability_log_reset();

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

/*
 * Interact with some options
 */
struct option_group_marker
{
    int before_index;
    cptr label;
};

static const struct option_group_marker interface_option_groups[] = {
    { 0, "Messages" },
    { 3, "Input" },
    { 7, "Look" },
    { 8, "Layout" },
    { 9, "Warnings" },
    { 10, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker text_option_groups[] = {
    { 0, "Look and Lore" },
    { 3, "Inventory and Equipment" },
    { 7, "Character" },
    { -1, NULL }
};

static const struct option_group_marker gameplay_option_groups[] = {
    { 0, "Combat Behavior" },
    { 3, "Information" },
    { 6, "World Generation" },
    { -1, NULL }
};

static const struct option_group_marker efficiency_option_groups[] = {
    { 0, "Animation" },
    { 2, "Camera" },
    { -1, NULL }
};

static const struct option_group_marker visual_option_groups[] = {
    { 0, "Lists and Overlays" },
    { 3, "Map and Highlights" },
    { 12, "Narrative" },
    { 16, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker challenge_option_groups[] = {
    { 0, "Traversal" },
    { 2, "Content" },
    { -1, NULL }
};

static const struct option_group_marker debug_option_groups[] = {
    { 0, "Generation" },
    { 4, "Knowledge" },
    { 10, "Survival" },
    { -1, NULL }
};

static const struct option_group_marker sound_option_groups[] = {
    { 0, "Effects" },
    { 5, "Effect Volume" },
    { 10, "Music" },
    { 12, "Music Volume" },
    { -1, NULL }
};

static const struct option_group_marker* get_option_groups_for_page(int page)
{
    switch (page)
    {
    case INTERFACE_PAGE: return interface_option_groups;
    case TEXT_PAGE: return text_option_groups;
    case GAMEPLAY_PAGE: return gameplay_option_groups;
    case EFFICIENCY_PAGE: return efficiency_option_groups;
    case VISUAL_PAGE: return visual_option_groups;
    case CHALLENGE_PAGE: return challenge_option_groups;
    case DEBUG_PAGE: return debug_option_groups;
    case SOUND_PAGE: return sound_option_groups;
    default: return NULL;
    }
}

static int option_group_count_before(const struct option_group_marker* groups,
    int option_index)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++) {
        if (groups[i].before_index <= option_index)
            count++;
    }

    return count;
}

static int option_group_total_rows(const struct option_group_marker* groups)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++)
        count++;

    return count;
}

static bool option_page_uses_app_config(int page)
{
    return (page == INTERFACE_PAGE) || (page == TEXT_PAGE)
        || (page == EFFICIENCY_PAGE) || (page == VISUAL_PAGE);
}

static int settings_ui_term_wid(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return wid;
}

static int settings_ui_line_width(int col)
{
    int width = settings_ui_term_wid() - col;

    if (width < 1)
        width = 1;

    return width;
}

static void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    if (!buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if ((int)strlen(text) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        strnfmt(buf, buflen, "%.*s", max_chars, text);
    }
    else
    {
        strnfmt(buf, buflen, "%.*s...", max_chars - 3, text);
    }
}

static cptr settings_ui_pick_label(int max_chars, cptr long_label,
    cptr medium_label, cptr short_label)
{
    cptr labels[3] = { long_label, medium_label, short_label };

    for (int i = 0; i < 3; i++)
    {
        if (labels[i] && labels[i][0] && (int)strlen(labels[i]) <= max_chars)
            return labels[i];
    }

    if (short_label && short_label[0])
        return short_label;
    if (medium_label && medium_label[0])
        return medium_label;
    if (long_label && long_label[0])
        return long_label;

    return "";
}

static void settings_ui_format_pair_line(char* buf, size_t buflen, cptr label,
    cptr value, int max_chars, int min_value_chars)
{
    char label_buf[128];
    char value_buf[96];
    int desired_value;
    int value_budget;
    int label_budget;

    if (!buflen)
        return;

    if (!label)
        label = "";
    if (!value)
        value = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (!value[0])
    {
        settings_ui_fit_text(buf, buflen, label, max_chars);
        return;
    }

    desired_value = (int)strlen(value);
    value_budget = MIN(max_chars - 4,
        MAX(min_value_chars, MIN(desired_value, (max_chars * 3) / 5)));

    if (value_budget < 1)
        value_budget = MIN(max_chars, MAX(1, max_chars / 2));

    settings_ui_fit_text(value_buf, sizeof(value_buf), value, value_budget);
    label_budget = max_chars - (int)strlen(value_buf) - 2;

    if (label_budget < 4)
    {
        settings_ui_fit_text(buf, buflen, value, max_chars);
        return;
    }

    settings_ui_fit_text(label_buf, sizeof(label_buf), label, label_budget);
    strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
}

static void settings_ui_put_fitted(int row, int col, byte attr, cptr text)
{
    char buf[160];
    int width = settings_ui_line_width(col);

    settings_ui_fit_text(buf, sizeof(buf), text, width);
    Term_putstr(col, row, width, attr, buf);
}

static void settings_ui_format_field(char* buf, size_t buflen, cptr text,
    bool selected)
{
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (selected)
        strnfmt(buf, buflen, "[%s]", text);
    else
        SDL_strlcpy(buf, text, buflen);
}

static void settings_ui_format_auto_value(char* buf, size_t buflen, int value,
    int max_chars)
{
    char raw_buf[16];
    char auto_long[16];
    char auto_short[8];

    if (!buf || !buflen)
        return;

    if (value > 0)
    {
        strnfmt(raw_buf, sizeof(raw_buf), "%d", value);
        settings_ui_fit_text(buf, buflen, raw_buf, max_chars);
        return;
    }

    SDL_strlcpy(auto_long, "auto", sizeof(auto_long));
    SDL_strlcpy(auto_short, "a", sizeof(auto_short));
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, auto_long, auto_long, auto_short),
        max_chars);
}

static bool option_menu_use_compact_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 60);
}

static bool option_menu_use_narrow_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 50);
}

static int option_menu_max_line_chars(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    /* Options start at column 4; keep one cell free for the cursor. */
    wid -= 5;

    if (wid < 8)
        wid = 8;

    return wid;
}

static void option_menu_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    settings_ui_fit_text(buf, buflen, text, max_chars);
}

static cptr sound_option_label(int index)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    if (compact)
    {
        switch (index)
        {
        case 0: return narrow ? "Sounds" : "Game sounds";
        case 1: return narrow ? "Combat sfx" : "Combat sounds";
        case 2: return narrow ? "Inv sfx" : "Inventory sounds";
        case 3: return narrow ? "Walk sfx" : "Walk sounds";
        case 4: return narrow ? "Door sfx" : "Door sounds";
        case 5: return narrow ? "Combat vol" : "Combat volume";
        case 6: return narrow ? "Inv vol" : "Inventory volume";
        case 7: return narrow ? "Walk vol" : "Walk volume";
        case 8: return narrow ? "Door vol" : "Door volume";
        case 9: return narrow ? "Other vol" : "Other volume";
        case 10: return "Menu music";
        case 11: return "Ambient music";
        case 12: return narrow ? "Menu vol" : "Menu music volume";
        case 13: return narrow ? "Ambient vol" : "Ambient music volume";
        default: return "(unknown sound option)";
        }
    }

    switch (index)
    {
    case 0: return "Enable game sounds";
    case 1: return "Enable combat sounds";
    case 2: return "Enable inventory sounds";
    case 3: return "Enable walk sounds";
    case 4: return "Enable door sounds";
    case 5: return "Combat sounds volume";
    case 6: return "Inventory sounds volume";
    case 7: return "Walk sounds volume";
    case 8: return "Door sounds volume";
    case 9: return "Other sounds volume";
    case 10: return "Enable main menu music";
    case 11: return "Enable ambient dungeon music";
    case 12: return "Main menu music volume";
    case 13: return "Ambient music volume";
    default: return "(unknown sound option)";
    }
}

static cptr option_menu_label(int opt)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    switch (opt)
    {
    case OPT_delay_factor:
        return compact ? (narrow ? "Anim delay" : "Animation delay")
                       : "Delay factor for animation (0 to 9)";
    case OPT_hitpoint_warning:
        return compact ? (narrow ? "HP warn" : "HP warning")
                       : "Hitpoint warning threshold (0% to 90%)";
    case OPT_main_combat_rolls:
        return compact ? (narrow ? "Combat lines" : "Combat roll lines")
                       : "Main terminal combat roll lines (0=off, 1-4=lines)";
    case OPT_hide_left_panel:
        return compact ? (narrow ? "Compact panel" : "Compact left panel")
                       : "Hide Left Panel [Alt+P]";
    case OPT_show_level_entry_banner:
        return compact ? (narrow ? "Entry text" : "Entry narrative")
                       : "Level entry narrative";
    case OPT_show_partition_narrative:
        return compact ? (narrow ? "Partition text" : "Partition narrative")
                       : "Partition transition narrative";
    case OPT_ability_desc_mode:
        return compact ? (narrow ? "Ability text" : "Ability descriptions")
                       : "Ability descriptions (0=lore+effect, 1=effect+lore, 2=effect)";
    case OPT_vault_drop_frequency:
        return compact ? "Vault drops" : "Vault drop frequency";
    case OPT_noble_item_spawn_mode:
        return compact ? (narrow ? "Noble items" : "Noble item sources")
                       : "Noble item spawns";
    case OPT_look_objects_sort_by_difficulty:
        return compact ? (narrow ? "Look diff sort" : "Look sort by diff")
                       : "Sort look (L) objects by difficulty only";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
    case OPT_banner_message_stairs:
        return compact ? "Banner layout" : "Banner message layout";
    case OPT_unlock_blitz_mode:
        return compact ? (narrow ? "Blitz unlocked" : "Unlock Blitz Mode")
                       : "Unlock Blitz Mode";
    default:
        break;
    }

    if (compact)
    {
        switch (opt)
        {
        case OPT_system_beep: return narrow ? "Beep" : "Error beep";
        case OPT_quick_messages: return narrow ? "Quick prompts" : "Quick prompts";
        case OPT_auto_more: return narrow ? "Auto more" : "Auto -more-";
        case OPT_easy_main_menu: return narrow ? "Esc menu" : "Esc main menu";
        case OPT_hjkl_movement: return narrow ? "hjkl move" : "hjkl movement";
        case OPT_angband_keyset: return narrow ? "Angband keys" : "Angband keyset";
        case OPT_space_acts_as_comma: return narrow ? "Space = comma" : "Space acts as comma";
        case OPT_story_lists: return narrow ? "Story look" : "Story font: look/target";
        case OPT_story_lists_inven: return narrow ? "Story inv" : "Story font: inv menu";
        case OPT_story_lists_equip: return narrow ? "Story equip" : "Story font: equip menu";
        case OPT_story_character_sheet: return narrow ? "Story sheet" : "Story font: char sheet";
        case OPT_story_lists_inven_pane: return narrow ? "Story inv pane" : "Story font: inv pane";
        case OPT_story_lists_equip_pane: return narrow ? "Story eq pane" : "Story font: equip pane";
        case OPT_story_monster_desc: return narrow ? "Story mon desc" : "Story font: monster desc";
        case OPT_story_monster_desc_pane: return narrow ? "Story mon pane" : "Story font: monster pane";
        case OPT_valorous_oath_auto_attack_safety: return narrow ? "Valorous safety" : "Valorous oath safety";
        case OPT_forgo_attacking_unwary: return narrow ? "Skip unwary hits" : "Forgo unwary attacks";
        case OPT_assassination_over_charge: return narrow ? "Stealth over charge" : "Assassination over Charge";
        case OPT_stop_singing_on_rest: return narrow ? "Stop song on rest" : "Stop singing on rest";
        case OPT_know_monster_info: return narrow ? "Know monsters" : "Know monster info";
        case OPT_visual_recognition: return narrow ? "Need light to spot" : "Need light to spot";
        case OPT_disable_skeleton_note_tutorial: return narrow ? "Hide skeleton tips" : "Hide skeleton tutorials";
        case OPT_smaller_level_size: return narrow ? "Smaller levels" : "Smaller level size";
        case OPT_more_stairs: return narrow ? "More stairs" : "Extra stairs";
        case OPT_instant_run: return narrow ? "Fast running" : "Faster running";
        case OPT_center_player: return narrow ? "Center map" : "Center map";
        case OPT_run_avoid_center: return narrow ? "No center on run" : "Avoid centering on run";
        case OPT_auto_display_lists: return narrow ? "Auto lists" : "Auto display lists";
        case OPT_artifact_unique_color: return narrow ? "Yellow artefacts" : "Yellow unique artefacts";
        case OPT_hilite_player: return narrow ? "Cursor on player" : "Highlight player";
        case OPT_hilite_target: return narrow ? "Cursor on target" : "Highlight target";
        case OPT_hilite_unwary: return narrow ? "Mark unwary" : "Highlight unwary";
        case OPT_solid_walls: return narrow ? "Solid walls" : "Solid walls";
        case OPT_hybrid_walls: return narrow ? "Hybrid walls" : "Hybrid walls";
        case OPT_unidentified_items_slate: return narrow ? "Slate unknown items" : "Slate unidentified items";
        case OPT_stealth_vision: return narrow ? "Stealth vision" : "Stealth vision";
        case OPT_sleep_icon: return narrow ? "Sleep icon" : "Sleep icon";
        case OPT_show_smithing_difficulty: return narrow ? "Smith dbg items" : "Debug smithing in items";
        case OPT_show_smithing_difficulty_look: return narrow ? "Smith dbg look" : "Debug smithing in look";
        case OPT_show_level_generation_debug: return narrow ? "Dbg lvl screen" : "Debug level screen";
        case OPT_birth_discon_stair: return narrow ? "Disc. stairs" : "Disconnected stairs";
        case OPT_birth_ironman: return narrow ? "Straight down" : "Straight down";
        case OPT_birth_no_artefacts: return narrow ? "No artefacts" : "No artefacts";
        case OPT_birth_fixed_exp: return narrow ? "Fixed XP" : "Fixed experience";
        case OPT_cheat_peek: return narrow ? "Debug obj gen" : "Debug object gen";
        case OPT_cheat_hear: return narrow ? "Debug mon gen" : "Debug monster gen";
        case OPT_cheat_room: return narrow ? "Debug room gen" : "Debug dungeon gen";
        case OPT_cheat_xtra: return narrow ? "Debug extra" : "Debug extra";
        case OPT_cheat_know: return narrow ? "Debug know mons" : "Debug know monsters";
        case OPT_cheat_monsters: return narrow ? "Debug show mons" : "Debug show monsters";
        case OPT_cheat_noise: return narrow ? "Debug noise" : "Debug noise";
        case OPT_cheat_scent: return narrow ? "Debug scent" : "Debug scent";
        case OPT_cheat_light: return narrow ? "Debug light" : "Debug light";
        case OPT_cheat_skill_rolls: return narrow ? "Debug skill rolls" : "Debug skill rolls";
        case OPT_cheat_live: return narrow ? "Debug no death" : "Debug avoid death";
        case OPT_cheat_timestop: return narrow ? "Debug time stop" : "Debug time stop";
        default:
            break;
        }
    }

    if (option_desc[opt])
        return option_desc[opt];
    if (option_text[opt])
        return option_text[opt];
    return "(unknown option)";
}

static void option_menu_format_line(char* buf, size_t buflen, cptr label,
    cptr value)
{
    if (!option_menu_use_compact_layout())
    {
        strnfmt(buf, buflen, "%-48s: %s", label, value);
    }
    else
    {
        char label_buf[96];
        char value_buf[48];
        int max_chars = option_menu_max_line_chars();
        int value_len;
        int label_budget;

        option_menu_fit_text(value_buf, sizeof(value_buf), value, max_chars);
        value_len = (int)strlen(value_buf);

        if (value_len <= 0)
        {
            option_menu_fit_text(buf, buflen, label, max_chars);
            return;
        }

        label_budget = max_chars - value_len - 2;
        if (label_budget <= 0)
        {
            option_menu_fit_text(buf, buflen, value_buf, max_chars);
            return;
        }

        option_menu_fit_text(label_buf, sizeof(label_buf), label, label_budget);
        strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
    }
}

static void option_apply_side_effects(int opt)
{
    if (opt == OPT_story_lists_inven_pane || opt == OPT_story_lists_equip_pane)
        redraw_inven_equip_subwindows();
    if (opt == OPT_story_monster_desc_pane)
        redraw_monster_subwindows();
    if (opt == OPT_stealth_vision || opt == OPT_visual_recognition
        || opt == OPT_sleep_icon)
        p_ptr->redraw |= (PR_MAP);
}

extern void do_cmd_options_aux(int page, cptr info)
{
    char ch;

    int i, k = 0, n = 0;
    int scroll = 0;

    int opt[OPT_PAGE_PER];

    char buf[160];

    int dir;
    
    bool is_sound_page = (page == SOUND_PAGE);
    bool app_page = option_page_uses_app_config(page);
    bool metarun_page = !app_page && !is_sound_page;
    bool app_settings_dirty = false;
    bool metarun_settings_dirty = false;
    bool sound_settings_dirty = false;
    const struct option_group_marker* groups = get_option_groups_for_page(page);
    struct sound_config* sound_cfg = sdl_sound_get_config();

    /* Scan the options */
    for (i = 0; i < OPT_PAGE_PER; i++)
    {
        /* Collect options on this "page" */
        if (option_page[page][i] != OPT_NONE)
        {
            opt[n++] = option_page[page][i];
        }
    }
    
    /* Special case: Sound page uses custom display instead of standard options */
    if (is_sound_page)
    {
        n = 14; /* 5 enable flags + 5 volume controls + 2 music enable + 2 music volume */
    }

    /* Interact with the player */
    while (true)
    {
        int first_row = 3;
        int footer_rows = (page == CHALLENGE_PAGE) ? 4 : 2;
        int visible_rows = Term->hgt - footer_rows - first_row;
        int total_rows = n + option_group_total_rows(groups);
        int selected_display_row = k + option_group_count_before(groups, k);
        int group_index = 0;
        int display_row = 0;
        int max_scroll;

        if (visible_rows < 1)
            visible_rows = 1;

        max_scroll = total_rows - visible_rows;
        if (max_scroll < 0)
            max_scroll = 0;

        if (selected_display_row < scroll)
            scroll = selected_display_row;
        else if (selected_display_row >= scroll + visible_rows)
            scroll = selected_display_row - visible_rows + 1;
        if (scroll > max_scroll)
            scroll = max_scroll;

        Term_clear();

        /* Prompt XXX XXX XXX */
        strnfmt(buf, sizeof(buf), "%s", info);
        settings_ui_put_fitted(1, 2, TERM_WHITE, buf);

        /* Display the options */
        for (i = 0; i < n; i++)
        {
            byte a = TERM_WHITE;
            int row;

            while (groups && groups[group_index].before_index == i)
            {
                row = first_row + display_row - scroll;
                if (row >= first_row && row < first_row + visible_rows)
                    Term_putstr(2, row, -1, TERM_SLATE, groups[group_index].label);
                display_row++;
                group_index++;
            }

            /* Color current option */
            if (i == k)
                a = TERM_L_BLUE;

            /* Display the option text */
            buf[0] = '\0';
            if (is_sound_page)
            {
                char value_str[32];

                if (i == 0)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enabled ? "yes" : "no ");
                }
                else if (i == 1)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_combat ? "yes" : "no ");
                }
                else if (i == 2)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_inventory ? "yes" : "no ");
                }
                else if (i == 3)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_walk ? "yes" : "no ");
                }
                else if (i == 4)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_doors ? "yes" : "no ");
                }
                else if (i == 5)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_combat * 100.0f);
                }
                else if (i == 6)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_inventory * 100.0f);
                }
                else if (i == 7)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_walk * 100.0f);
                }
                else if (i == 8)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_doors * 100.0f);
                }
                else if (i == 9)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_other * 100.0f);
                }
                else if (i == 10)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_main_enabled ? "yes" : "no ");
                }
                else if (i == 11)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_ambient_enabled ? "yes" : "no ");
                }
                else if (i == 12)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_main_volume * 100.0f);
                }
                else if (i == 13)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_ambient_volume * 100.0f);
                }

                option_menu_format_line(buf, sizeof(buf), sound_option_label(i),
                    value_str);
            }
            else if (opt[i] == OPT_delay_factor)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d", op_ptr->delay_factor);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hitpoint_warning)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d%%",
                    op_ptr->hitpoint_warn * 10);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hide_left_panel)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    get_sdl_hide_left_panel() ? "yes" : "no ");
            }
            else if (opt[i] == OPT_main_combat_rolls)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d",
                    op_ptr->main_combat_rolls);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_show_level_entry_banner)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->level_entry_narrative_mode)
                {
                case LEVEL_ENTRY_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case LEVEL_ENTRY_NARRATIVE_MESSAGE: mode_str = "Message"; break;
                case LEVEL_ENTRY_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:
                    mode_str = compact ? "Banner delay" : "Banner with delay";
                    break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_show_partition_narrative)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->partition_narrative_mode)
                {
                case PARTITION_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case PARTITION_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:                          mode_str = "Message"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_ability_desc_mode)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->ability_desc_mode)
                {
                case 1:  mode_str = compact ? "1 effect+lore" : "1 (effect+lore)"; break;
                case 2:  mode_str = compact ? "2 effect only" : "2 (effect only)"; break;
                default: mode_str = compact ? "0 lore+effect" : "0 (lore+effect)"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_vault_drop_frequency)
            {
                const char *vdf_names[] = { "Normal", "Modest", "Scarce", "Meager", "Plentiful" };
                char value_str[32];
                byte mode = op_ptr->vault_drop_frequency;
                if (mode > VDF_PLENTIFUL)
                    mode = VDF_NORMAL;
                strnfmt(value_str, sizeof(value_str), "%s (%d)", vdf_names[mode],
                    mode);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_noble_item_spawn_mode)
            {
                const char *mode_str
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                    ? (option_menu_use_compact_layout() ? "1 with vaults" : "1 (also &/! vault drops)")
                    : (option_menu_use_compact_layout() ? "0 restricted" : "0 (good+/chests/human+elf skeletons)");
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_intro_style)
            {
                const char *is_names[] = {
                    "Flame Imperishable", "Oath of Feanor",
                    "Twilight of Valinor", "Song of Luthien",
                    "Words of Hurin", "Random"
                };
                byte m = op_ptr->intro_style;
                if (m > INTRO_STYLE_RANDOM) m = INTRO_STYLE_FLAME;
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    is_names[m]);
            }
            else if (opt[i] == OPT_banner_message_stairs)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "Stair" : "Straight");
            }
            else
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "yes" : "no ");
            }

            row = first_row + display_row - scroll;
            if (row >= first_row && row < first_row + visible_rows)
                c_prt(a, buf, row, 4);
            display_row++;
        }

        if (total_rows > visible_rows)
        {
            strnfmt(buf, sizeof(buf), "(scroll: rows %d-%d of %d)",
                scroll + 1, MIN(scroll + visible_rows, total_rows), total_rows);
            settings_ui_put_fitted(Term->hgt - 2, 2, TERM_SLATE, buf);
        }

        if (page == CHALLENGE_PAGE)
        {
            settings_ui_put_fitted(Term->hgt - 4, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Challenge options can only be changed during character creation",
                    "Challenge options only change during character creation",
                    "Challenge options only change at birth"));
            settings_ui_put_fitted(Term->hgt - 3, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "or on the very first turn",
                    "or on the first turn",
                    "or on turn 1"));

            if (playerturn == 0)
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(direction keys to set, Return/Escape to accept)",
                        "(direction keys to set, Enter/Esc to accept)",
                        "(arrows set, Enter/Esc accept)"));
            }
            else
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(press Return to go back)",
                        "(press Enter to go back)",
                        "(Enter goes back)"));
            }
        }
        else
        {
            settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "(direction keys to set, Return/Escape to accept)",
                    "(direction keys to set, Enter/Esc to accept)",
                    "(arrows set, Enter/Esc accept)"));
        }

        /* Hilite current option */
        move_cursor(first_row + selected_display_row - scroll,
            MIN(54, Term->wid - 1));

        /* Get a key */
        inkey_set_cursor_hidden(true);
        ch = settings_wait_key();
        inkey_set_cursor_hidden(false);

        /*
         * HACK - Try to translate the key into a direction
         * to allow using the roguelike keys for navigation.
         */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        /* Analyze */
        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
        {
            /* Hack -- Notice use of any "cheat" options */
            for (i = OPT_CHEAT; i < OPT_ADULT; i++)
            {
                if (op_ptr->opt[i])
                {
                    /* Set score option */
                    if (!op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)])
                        metarun_settings_dirty = true;
                    op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = true;
                }
            }

            if (sound_settings_dirty)
            {
                sdl_sound_save_config();
                sdl_sound_reload();
            }

            if (app_settings_dirty)
                save_pane_config_to_json();

            if (metarun_settings_dirty)
                metarun_save_persistent_settings();

            return;
        }

        case '-':
        case '8':
        {
            k = (n + k - 1) % n;
            break;
        }

        case '2':
        {
            k = (k + 1) % n;
            break;
        }

        case 't':
        case '5':
        case ' ':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = !sound_cfg->enabled;
                        use_sound = sound_cfg->enabled;
                    }
                    else if (k == 1) sound_cfg->enable_combat = !sound_cfg->enable_combat;
                    else if (k == 2) sound_cfg->enable_inventory = !sound_cfg->enable_inventory;
                    else if (k == 3) sound_cfg->enable_walk = !sound_cfg->enable_walk;
                    else if (k == 4) sound_cfg->enable_doors = !sound_cfg->enable_doors;
                    else if (k == 10) sound_cfg->music_main_enabled = !sound_cfg->music_main_enabled;
                    else if (k == 11) sound_cfg->music_ambient_enabled = !sound_cfg->music_ambient_enabled;
                    /* Volume controls (5-9, 12-13) don't toggle */
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 0;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(!get_sdl_hide_left_panel());
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 0;

                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    /* Toggle cycles forward */
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_FLAME;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 0;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? NOBLE_ITEM_SPAWN_INCLUDE_VAULTS
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else
                {
                    op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case 'y':
        case '6':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = true;
                        use_sound = true;
                    }
                    else if (k == 1) sound_cfg->enable_combat = true;
                    else if (k == 2) sound_cfg->enable_inventory = true;
                    else if (k == 3) sound_cfg->enable_walk = true;
                    else if (k == 4) sound_cfg->enable_doors = true;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat < 1.0f) ? sound_cfg->volume_combat + 0.1f : 1.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory < 1.0f) ? sound_cfg->volume_inventory + 0.1f : 1.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk < 1.0f) ? sound_cfg->volume_walk + 0.1f : 1.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors < 1.0f) ? sound_cfg->volume_doors + 0.1f : 1.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other < 1.0f) ? sound_cfg->volume_other + 0.1f : 1.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = true;
                    else if (k == 11) sound_cfg->music_ambient_enabled = true;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume < 1.0f) ? sound_cfg->music_main_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 9;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 9;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(true);
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 4;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 2;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_PLENTIFUL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode < NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                        ? op_ptr->noble_item_spawn_mode + 1
                        : NOBLE_ITEM_SPAWN_INCLUDE_VAULTS;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_RANDOM;
                }
                else
                {
                    op_ptr->opt[opt[k]] = true;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case 'n':
        case '4':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = false;
                        use_sound = false;
                    }
                    else if (k == 1) sound_cfg->enable_combat = false;
                    else if (k == 2) sound_cfg->enable_inventory = false;
                    else if (k == 3) sound_cfg->enable_walk = false;
                    else if (k == 4) sound_cfg->enable_doors = false;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat > 0.0f) ? sound_cfg->volume_combat - 0.1f : 0.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory > 0.0f) ? sound_cfg->volume_inventory - 0.1f : 0.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk > 0.0f) ? sound_cfg->volume_walk - 0.1f : 0.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors > 0.0f) ? sound_cfg->volume_doors - 0.1f : 0.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other > 0.0f) ? sound_cfg->volume_other - 0.1f : 0.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = false;
                    else if (k == 11) sound_cfg->music_ambient_enabled = false;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume > 0.0f) ? sound_cfg->music_main_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor > 0)
                        ? op_ptr->delay_factor - 1
                        : 0;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn > 0)
                        ? op_ptr->hitpoint_warn - 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    set_sdl_hide_left_panel(false);
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls > 0)
                        ? op_ptr->main_combat_rolls - 1
                        : 0;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
                        ? op_ptr->level_entry_narrative_mode - 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_BANNER)
                        ? op_ptr->partition_narrative_mode - 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode > 0)
                        ? op_ptr->ability_desc_mode - 1
                        : 0;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency > VDF_NORMAL)
                        ? op_ptr->vault_drop_frequency - 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? op_ptr->noble_item_spawn_mode - 1
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style > INTRO_STYLE_FLAME)
                        ? op_ptr->intro_style - 1
                        : INTRO_STYLE_FLAME;
                }
                else
                {
                    op_ptr->opt[opt[k]] = false;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        default:
        {
            bell("Illegal command for normal options!");
            break;
        }
        }

        if (birth_fixed_exp && playerturn == 0 && p_ptr->exp != PY_FIXED_EXP)
        {
            int total_exp = PY_FIXED_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
        else if (!birth_fixed_exp && playerturn == 0
            && p_ptr->exp >= PY_FIXED_EXP)
        {
            int total_exp = PY_START_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
    }
}

/*
 * Write all current options to the given preference file in the
 * lib/user directory. Modified from KAmband 1.8.
 */
static errr option_dump(cptr fname)
{
    static cptr mark = "Options Dump";

    int i, j;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("option_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old options */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic option dump\n\n");

    /* Dump options (skip cheat, adult, score) */
    for (i = 0; i < OPT_CHEAT; i++)
    {
        /* Require a real option */
        if (!option_text[i])
            continue;

        /* Comment */
        SDL_IOprintf(fff, "# Option '%s'\n", option_desc[i]);

        /* Dump the option */
        if (op_ptr->opt[i])
        {
            SDL_IOprintf(fff, "Y:%s\n", option_text[i]);
        }
        else
        {
            SDL_IOprintf(fff, "X:%s\n", option_text[i]);
        }

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Dump window flags */
    for (i = 1; i < ANGBAND_TERM_MAX; i++)
    {
        /* Require a real window */
        if (!angband_term[i])
            continue;

        /* Check each flag */
        for (j = 0; j < 32; j++)
        {
            /* Require a real flag */
            if (!window_flag_desc[j])
                continue;

            /* Comment */
            SDL_IOprintf(fff, "# Window '%s', Flag '%s'\n", angband_term_name[i],
                window_flag_desc[j]);

            /* Dump the flag */
            if (op_ptr->window_flag[i] & (1L << j))
            {
                SDL_IOprintf(fff, "W:%d:%d:1\n", i, j);
            }
            else
            {
                SDL_IOprintf(fff, "W:%d:%d:0\n", i, j);
            }

            /* Skip a line */
            SDL_IOprintf(fff, "\n");
        }
    }

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
static void do_cmd_touch_pane_button_editor(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);
static void format_font_size_value(char* buf, size_t buflen, int raw, int effective,
    int max_chars)
{
    char long_buf[24];
    char medium_buf[24];
    char short_buf[16];

    if (!buf || !buflen)
        return;

    if (raw > 0)
    {
        strnfmt(long_buf, sizeof(long_buf), "%d", raw);
        settings_ui_fit_text(buf, buflen, long_buf, max_chars);
        return;
    }

    strnfmt(long_buf, sizeof(long_buf), "auto (%d)", effective);
    strnfmt(medium_buf, sizeof(medium_buf), "auto %d", effective);
    strnfmt(short_buf, sizeof(short_buf), "a%d", effective);
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, long_buf, medium_buf, short_buf),
        max_chars);
}

static const char* sdl_min_terminal_mode_label(int mode)
{
    return (mode == 1) ? "compact (50x18)" : "normal (80x24)";
}

void do_cmd_pane_settings(void)
{
    int k = 0;
    int n = 12; /* Total number of options */
    bool done = false;
    bool settings_changed = false;
    int dir;
    const char* config_path = get_sdl_config_path();
    const char* config_label = (config_path && config_path[0]) ? config_path : "sil_sdl.json";
    
    /* Save screen */
    screen_save();
    
    while (!done)
    {
        int row_width;
        int label_hint;

        /* Clear screen */
        Term_clear();

        /* Display title */
        settings_ui_put_fitted(1, 2, TERM_WHITE, "SDL Pane Settings");

        /* Display current settings */
        char buf[96];
        char value_buf[32];
        int y0 = 3;
        byte a;
        char font_value[24];
        row_width = settings_ui_line_width(2);
        label_hint = MAX(10, row_width - 12);

        /* Option 0: Main View Scale */
        a = (k == 0) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_main_view_scale());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Main View Scale (1-max) [Alt++/-]",
                "Main View Scale [Alt++/-]",
                "View Scale"),
            value_buf, row_width, 3);
        c_prt(a, buf, y0 + 0, 2);

        /* Option 1: Minimum Terminal Size */
        a = (k == 1) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(get_sdl_min_terminal_mode()),
            row_width, 10);
        c_prt(a, buf, y0 + 1, 2);

        /* Option 2: Aux View Font Size */
        a = (k == 2) ? TERM_L_BLUE : TERM_WHITE;
        format_font_size_value(font_value, sizeof(font_value),
            get_sdl_aux_view_font_size(), get_sdl_effective_aux_view_font_size(),
            MAX(6, MIN(14, row_width / 2)));
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Default Aux Font Size (0=auto, 8-48)",
                "Default Aux Font (0=auto)",
                "Aux Font"),
            font_value, row_width, 6);
        c_prt(a, buf, y0 + 2, 2);

        /* Option 3: Menu + Left Panel Font Size */
        a = (k == 3) ? TERM_L_BLUE : TERM_WHITE;
        format_font_size_value(font_value, sizeof(font_value),
            get_sdl_menu_panel_font_size(),
            get_sdl_effective_menu_panel_font_size(),
            MAX(6, MIN(14, row_width / 2)));
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Menu + Left Panel Font (0=auto, 8-64)",
                "Menu + Left Panel Font",
                "Menu Font"),
            font_value, row_width, 6);
        c_prt(a, buf, y0 + 3, 2);

        /* Option 4: Margin */
        a = (k == 4) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_margin());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Margin (0-20)",
                "Margin",
                "Margin"),
            value_buf, row_width, 3);
        c_prt(a, buf, y0 + 4, 2);

        /* Option 5: Fullscreen */
        a = (k == 5) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Fullscreen",
            get_sdl_fullscreen() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 5, 2);

        /* Option 6: Tiles */
        a = (k == 6) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Tiles",
            get_sdl_tiles() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 6, 2);

        /* Option 7: Enable Side Panes */
        a = (k == 7) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            get_sdl_enable_right_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 7, 2);

        /* Option 8: Enable Bottom Panes */
        a = (k == 8) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            get_sdl_enable_bottom_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 8, 2);

        /* Option 9: View Pane Configuration (supporting panes only) */
        a = (k == 9) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%s (%d)",
            settings_ui_pick_label(row_width,
                "View Pane Configuration",
                "Pane Configuration",
                "Pane Layout"),
            get_supporting_pane_config_count());
        {
            char fitted_buf[96];
            settings_ui_fit_text(fitted_buf, sizeof(fitted_buf), buf, row_width);
            SDL_strlcpy(buf, fitted_buf, sizeof(buf));
        }
        c_prt(a, buf, y0 + 9, 2);

        /* Option 10: Pane Font Sizes */
        a = (k == 10) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_ui_pick_label(row_width,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts"),
            row_width);
        c_prt(a, buf, y0 + 10, 2);

        /* Option 11: Save/Return */
        a = (k == 11) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_changed ? "Save Changes and Return"
                             : "Return to Options Menu",
            row_width);
        c_prt(a, buf, y0 + 11, 2);

        /* Display help */
        int y = Term->hgt - 3;
        if (settings_changed)
        {
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Settings changed - changes take effect immediately.",
                    "Settings changed - active immediately.",
                    "Changes apply immediately."));
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Will be saved to your SDL config file on exit.",
                    "Saved to your SDL config on exit.",
                    "Saved on exit."));
        }
        settings_ui_put_fitted(y++, 2, TERM_SLATE,
            settings_ui_pick_label(settings_ui_line_width(2),
                "(direction keys to set, 0 = auto font, Return/Escape to accept)",
                "(arrows move, 4/6 or y/n set, 0 auto, Enter/Esc exit)",
                "(arrows move, 4/6 set, 0 auto, Enter/Esc)"));

        /* Get key */
        inkey_set_cursor_hidden(true);
        char ch = settings_wait_key();
        inkey_set_cursor_hidden(false);
        
        /* Try to translate the key into a direction */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);
        
        /* Process input */
        switch (ch)
        {
        case ESCAPE:
        {
            /* Exit without needing to navigate to the bottom */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Enter activates the current option for actions; otherwise accept/exit. */
            if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }

            /* Save if changed, then exit */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }
        
        case '-':
        case '8':
        {
            /* Move up */
            k = (n + k - 1) % n;
            break;
        }
        
        case '2':
        {
            /* Move down */
            k = (k + 1) % n;
            break;
        }

        case '0':
        {
            if (k == 2)
            {
                if (get_sdl_aux_view_font_size() != 0)
                {
                    set_sdl_aux_view_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3)
            {
                if (get_sdl_menu_panel_font_size() != 0)
                {
                    set_sdl_menu_panel_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else
            {
                bell("0 sets the selected font to auto");
            }
            break;
        }
        
        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == 1) /* Minimum Terminal Size */
            {
                set_sdl_min_terminal_mode(get_sdl_min_terminal_mode() == 0 ? 1 : 0);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 5) /* Fullscreen */
            {
                set_sdl_fullscreen(!get_sdl_fullscreen());
                settings_changed = true;
            }
            else if (k == 6) /* Tiles */
            {
                set_sdl_tiles(!get_sdl_tiles());
                settings_changed = true;
            }
            else if (k == 7) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(!get_sdl_enable_right_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(!get_sdl_enable_bottom_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == 11) /* Save/Return */
            {
                if (settings_changed)
                {
                    if (save_pane_config_to_json())
                    {
                        msg_format("Settings saved to %s", config_label);
                    }
                }
                done = true;
            }
            break;
        }
        
        case 'y':
        case '6':
        {
            /* Increase value or set to yes */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                int max_scale = get_sdl_max_scale();
                if (val < max_scale)
                {
                    set_sdl_main_view_scale(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 0)
                {
                    set_sdl_min_terminal_mode(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 48)
                {
                    set_sdl_aux_view_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = get_sdl_menu_panel_font_size();
                if (val == 0)
                {
                    set_sdl_menu_panel_font_size(
                        get_sdl_effective_menu_panel_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 64)
                {
                    set_sdl_menu_panel_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = get_sdl_margin();
                if (val < 20)
                {
                    set_sdl_margin(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                set_sdl_fullscreen(true);
                settings_changed = true;
            }
            else if (k == 6) /* Tiles */
            {
                set_sdl_tiles(true);
                settings_changed = true;
            }
            else if (k == 7) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            break;
        }
        
        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                if (val > 1)
                {
                    set_sdl_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 1)
                {
                    set_sdl_min_terminal_mode(1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    set_sdl_aux_view_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = get_sdl_menu_panel_font_size();
                if (val == 0)
                {
                    set_sdl_menu_panel_font_size(
                        get_sdl_effective_menu_panel_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    set_sdl_menu_panel_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = get_sdl_margin();
                if (val > 0)
                {
                    set_sdl_margin(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                set_sdl_fullscreen(false);
                settings_changed = true;
            }
            else if (k == 6) /* Tiles */
            {
                set_sdl_tiles(false);
                settings_changed = true;
            }
            else if (k == 7) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            break;
        }
        
        default:
        {
            bell("Illegal command for pane settings!");
            break;
        }
        }
    }
    
    /* Restore screen */
    screen_load();
}


static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    if (pane_count <= 0)
    {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Fonts");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)settings_wait_key();
        screen_load();
        return;
    }

    {
        int sel = 0;
        bool done = false;
        bool changed = false;
        int dir;

        while (!done)
        {
            int y0 = 4;
            int row_width;
            int term_wid;

            Term_clear();
            term_wid = settings_ui_term_wid();
            row_width = settings_ui_line_width(2);
            settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Fonts");
            settings_ui_put_fitted(2, 2, TERM_WHITE, "=====================");

            for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
            {
                int idx = pane_indices[i];
                enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
                bool enabled = get_sdl_pane_enabled(idx);
                int raw_font = get_sdl_pane_font_size(idx);
                int effective_font = get_sdl_pane_effective_font_size(idx);
                byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
                char line_buf[96];
                char label_buf[48];
                char font_value[24];
                char font_field[28];
                const char* type_label = settings_ui_pick_label(MAX(8, row_width / 2),
                    pane_type_name(type), pane_type_name(type),
                    pane_type_short_name(type));

                format_font_size_value(font_value, sizeof(font_value), raw_font,
                    effective_font, MAX(6, MIN(14, row_width / 2)));
                settings_ui_format_field(font_field, sizeof(font_field), font_value,
                    i == sel);
                strnfmt(label_buf, sizeof(label_buf), "%s %s", type_label,
                    enabled ? "on" : "off");
                settings_ui_format_pair_line(line_buf, sizeof(line_buf), label_buf,
                    font_field, row_width, 6);
                c_prt(a, line_buf, y0 + i, 2);
            }

            {
                int y = Term->hgt - 4;
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Up/Down: select pane   4/6 (or n/y): change font size",
                        "Up/Down select pane   4/6 set font size",
                        "Up/Down select   4/6 set"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "0: auto (uses default aux font / auto main-based size)",
                        "0: auto font size",
                        "0 auto font"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Changes apply immediately",
                        "Changes apply immediately",
                        "Changes apply now"));
            }

            Term_fresh();

            inkey_set_cursor_hidden(true);
            char ch = settings_wait_key();
            inkey_set_cursor_hidden(false);

            dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);

            switch (ch)
            {
            case ESCAPE:
            case '\n':
            case '\r':
                done = true;
                break;

            case '-':
            case '8':
                sel = (pane_count + sel - 1) % pane_count;
                break;

            case '2':
                sel = (sel + 1) % pane_count;
                break;

            case '0':
            {
                int idx = pane_indices[sel];
                if (get_sdl_pane_font_size(idx) != 0)
                {
                    set_sdl_pane_font_size(idx, 0);
                    changed = true;
                    sdl_apply_config();
                }
                break;
            }

            case 'n':
            case '4':
            case 'y':
            case '6':
            {
                int idx = pane_indices[sel];
                int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
                int value = get_sdl_pane_font_size(idx);

                if (value == 0)
                    set_sdl_pane_font_size(idx, get_sdl_pane_effective_font_size(idx));
                else
                    set_sdl_pane_font_size(idx, value + delta);

                changed = true;
                sdl_apply_config();
                break;
            }

            default:
                bell("Illegal command for pane font editor!");
                break;
            }
        }

        if (changed && settings_changed)
            *settings_changed = true;
    }

    screen_load();
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
    case PLACE_DOUBLE_RIGHT: return "DR";
    case PLACE_DOUBLE_LEFT: return "DL";
    case PLACE_BOTTOM: return "BOT";
    default: return "?";
    }
}

static int get_supporting_pane_config_count(void)
{
    int count = 0;
    int total = get_pane_config_count();
    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type != PANE_MAIN)
            count++;
    }
    return count;
}

static int supporting_pane_master_idx(const int* pane_indices, int pane_count,
    enum pane_placement where)
{
    int fallback = -1;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        if ((enum pane_placement)get_sdl_pane_where(idx) != where)
            continue;
        if (fallback < 0)
            fallback = idx;
        if (get_sdl_pane_enabled(idx))
            return idx;
    }

    return fallback;
}

static bool supporting_pane_rows_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (where == PLACE_BOTTOM && idx != master_idx);
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
        || (*field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx)))
    {
        *field = (*field + 1) % 4;
    }
}

static bool supporting_pane_normalize_shared_sizes(const int* pane_indices, int pane_count)
{
    bool changed = false;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

        if (where == PLACE_BOTTOM && idx != master_idx && get_sdl_pane_rows(idx) != 0)
        {
            set_sdl_pane_rows(idx, 0);
            changed = true;
        }
        else if (pane_placement_is_side(where) && idx != master_idx
            && get_sdl_pane_cols(idx) != 0)
        {
            set_sdl_pane_cols(idx, 0);
            changed = true;
        }
    }

    return changed;
}

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    int sel = 0;
    int field = 0; /* 0 = enabled, 1 = where, 2 = rows, 3 = cols */
    bool done = false;
    bool changed = false;
    int dir;

    if (pane_count <= 0)
    {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Layout");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)settings_wait_key();
        screen_load();
        return;
    }

    if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
    {
        changed = true;
        sdl_apply_config();
    }
    supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);

    while (!done)
    {
        int y0 = 4;
        int term_wid;
        int row_width;

        Term_clear();
        term_wid = settings_ui_term_wid();
        row_width = settings_ui_line_width(2);
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Layout");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "======================");

        for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
        {
            int idx = pane_indices[i];
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
            int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);
            bool enabled = get_sdl_pane_enabled(idx);
            bool rows_locked = supporting_pane_rows_locked(pane_indices, pane_count, idx);
            bool cols_locked = supporting_pane_cols_locked(pane_indices, pane_count, idx);
            int rows = get_sdl_pane_rows(idx);
            int cols = get_sdl_pane_cols(idx);
            byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
            char type_buf[24];
            char enabled_field[12];
            char where_field[24];
            char rows_value[16];
            char rows_field[20];
            char cols_value[16];
            char cols_field[20];
            char line_buf[128];
            const char* type_label = settings_ui_pick_label(MAX(8, row_width / 3),
                pane_type_name(type), pane_type_name(type), pane_type_short_name(type));
            const char* where_label = settings_ui_pick_label(MAX(4, row_width / 4),
                pane_placement_name(where), pane_placement_name(where),
                pane_where_short_name(where));

            settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                MAX(4, row_width / 3));
            settings_ui_format_field(enabled_field, sizeof(enabled_field),
                enabled ? "on" : "off", i == sel && field == 0);
            settings_ui_format_field(where_field, sizeof(where_field), where_label,
                i == sel && field == 1);

            if (rows_locked)
            {
                int shared_rows = (master_idx >= 0) ? get_sdl_pane_rows(master_idx) : rows;
                settings_ui_format_auto_value(rows_value, sizeof(rows_value),
                    shared_rows, 4);
            }
            else
                settings_ui_format_auto_value(rows_value, sizeof(rows_value), rows, 4);
            settings_ui_format_field(rows_field, sizeof(rows_field), rows_value,
                !rows_locked && i == sel && field == 2);

            if (cols_locked)
            {
                int shared_cols = (master_idx >= 0) ? get_sdl_pane_cols(master_idx) : cols;
                settings_ui_format_auto_value(cols_value, sizeof(cols_value),
                    shared_cols, 4);
            }
            else
                settings_ui_format_auto_value(cols_value, sizeof(cols_value), cols, 4);
            settings_ui_format_field(cols_field, sizeof(cols_field), cols_value,
                !cols_locked && i == sel && field == 3);

            strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s", type_buf,
                where_field, enabled_field, rows_field, cols_field);
            settings_ui_put_fitted(y0 + i, 2, a, line_buf);
        }

        {
            int y = Term->hgt - 4;
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Up/Down: select pane   Space: choose on/off, where, rows, cols",
                    "Up/Down select pane   Space switch field",
                    "Up/Down select   Space field"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "4/6 (or n/y): toggle, cycle, or +/- value   0: set rows/cols to auto",
                    "4/6 or y/n: toggle, cycle, or +/- value   0: auto",
                    "4/6 cycle/set   0 auto"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Each side slot shares cols with its first pane; bottom panes share rows",
                    "Side slots share cols; bottom panes share rows",
                    "Side slots share cols; bottom shares rows"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "ESC/Enter: return (changes apply immediately)",
                    "ESC/Enter: return",
                    "Esc/Enter return"));
        }

        Term_fresh();

        inkey_set_cursor_hidden(true);
        char ch = settings_wait_key();
        inkey_set_cursor_hidden(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case ' ':
        case 't':
        case '5':
            field = (field + 1) % 4;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '-':
        case '8':
            sel = (pane_count + sel - 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '2':
            sel = (sel + 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '0':
        {
            int idx = pane_indices[sel];
            if (field == 0 || field == 1)
            {
                bell("Use 4/6 to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared for bottom panes");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side slot");
                break;
            }

            if (field == 2)
                set_sdl_pane_rows(idx, 0);
            else
                set_sdl_pane_cols(idx, 0);

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        case 'n':
        case '4':
        case 'y':
        case '6':
        {
            int idx = pane_indices[sel];
            int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);

            if (field == 0)
            {
                set_sdl_pane_enabled(idx, (delta > 0));
            }
            else if (field == 1)
            {
                set_sdl_pane_where(idx, pane_next_allowed_placement(type, where, delta));
            }
            else if (field == 2)
            {
                int rows = get_sdl_pane_rows(idx);

                if (supporting_pane_rows_locked(pane_indices, pane_count, idx))
                {
                    bell("Rows are shared for bottom panes");
                    break;
                }
                if (rows == 0)
                    set_sdl_pane_rows(idx, get_sdl_pane_current_rows(idx));
                else
                    set_sdl_pane_rows(idx, rows + delta);
            }
            else
            {
                int cols = get_sdl_pane_cols(idx);

                if (supporting_pane_cols_locked(pane_indices, pane_count, idx))
                {
                    bell("Cols are shared within each side slot");
                    break;
                }
                if (cols == 0)
                    set_sdl_pane_cols(idx, get_sdl_pane_current_cols(idx));
                else
                    set_sdl_pane_cols(idx, cols + delta);
            }

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        default:
            bell("Illegal command for pane layout editor!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    screen_load();
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        get_sdl_touch_pane_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    binding_action_label(binding, buf, buflen);
}

static void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    int term_w, term_h;
    const int list_start_row = 5;

    screen_save();

    while (!done)
    {
        int row;
        int visible_rows;
        int row_width;

        Term_get_size(&term_w, &term_h);
        row_width = settings_ui_line_width(2);
        visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;

        if (highlight < 0)
            highlight = 0;
        if (highlight >= SDL_TOUCH_PANE_BUTTON_COUNT)
            highlight = SDL_TOUCH_PANE_BUTTON_COUNT - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;

        Term_clear();
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Touch Settings");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "==============");

        row = list_start_row;
        for (int i = top; i < SDL_TOUCH_PANE_BUTTON_COUNT && i < top + visible_rows; i++)
        {
            char action_buf[80];
            char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
            char left_buf[64];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            get_sdl_touch_pane_button_label_for_panel(panel, i, label_buf, sizeof(label_buf));
            touch_pane_action_label_for_panel(panel,
                get_sdl_touch_pane_binding_for_panel(panel, i), action_buf, sizeof(action_buf));

            if (label_buf[0])
                strnfmt(left_buf, sizeof(left_buf), "%s %s",
                    get_sdl_touch_pane_slot_name(i), label_buf);
            else
                strnfmt(left_buf, sizeof(left_buf), "%s",
                    get_sdl_touch_pane_slot_name(i));

            settings_ui_format_pair_line(line_buf, sizeof(line_buf), left_buf,
                action_buf, row_width, 14);
            c_prt(a, line_buf, row++, 2);
        }

        row = list_start_row + visible_rows + 1;
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            get_sdl_touch_pane_panel_name(panel, panel_name, sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name, (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? " (empty = main panel)" : "");
            settings_ui_put_fitted(3, 2, TERM_SLATE, info_buf);
        }
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Up/Down: select button   4/6: previous/next action   l: rename slot",
                "Up/Down select   4/6 action   l rename slot",
                "Up/Down select   4/6 action"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Tab: switch panel   p: rename panel   r: reset selected   R: reset all",
                "Tab switch panel   p rename panel   r/R reset",
                "Tab switch   p rename   r/R reset"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "ESC/Enter: return",
                "Esc/Enter: return",
                "Esc/Enter return"));

        Term_fresh();

        inkey_set_cursor_hidden(true);
        char ch = settings_wait_key();
        inkey_set_cursor_hidden(false);

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (SDL_TOUCH_PANE_BUTTON_COUNT + highlight - 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case '2':
            highlight = (highlight + 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case 'n':
        case '4':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, get_sdl_touch_pane_binding_for_panel(panel, highlight));
            idx = (choice_count + idx - 1) % choice_count;
            set_sdl_touch_pane_binding_for_panel(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, get_sdl_touch_pane_binding_for_panel(panel, highlight));
            idx = (idx + 1) % choice_count;
            set_sdl_touch_pane_binding_for_panel(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'l':
        case 'L':
        {
            char prompt[96];
            char prompt_long[96];
            char prompt_medium[96];
            char prompt_short[64];
            char current_label[SDL_TOUCH_PANE_LABEL_LEN];
            char new_label[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            get_sdl_touch_pane_button_label_for_panel(panel, highlight, current_label, sizeof(current_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "New label for %s (blank = use key label): ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "New label for %s (blank = default): ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    prompt_long, prompt_medium, prompt_short));
            strnfmt(current_buf, sizeof(current_buf), "Current label: %s", current_label);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_label[0] = '\0';
            if (term_get_string(prompt, new_label, sizeof(new_label)))
            {
                set_sdl_touch_pane_button_label_for_panel(panel, highlight, new_label);
                changed = true;
            }
            break;
        }

        case '\t':
            panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                ? SDL_TOUCH_PANE_PANEL_SECOND
                : SDL_TOUCH_PANE_PANEL_MAIN;
            break;

        case 'p':
        case 'P':
        {
            char prompt[96];
            char current_name[SDL_TOUCH_PANE_LABEL_LEN];
            char new_name[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            get_sdl_touch_pane_panel_name(panel, current_name, sizeof(current_name));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    "Name for current panel (blank = default): ",
                    "Panel name (blank = default): ",
                    "Panel name: "));
            strnfmt(current_buf, sizeof(current_buf), "Current panel name: %s", current_name);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_name[0] = '\0';
            if (term_get_string(prompt, new_name, sizeof(new_name)))
            {
                set_sdl_touch_pane_panel_name(panel, new_name);
                changed = true;
            }
            break;
        }

        case 'r':
            set_sdl_touch_pane_binding_for_panel(panel, highlight,
                get_sdl_touch_pane_default_binding_for_panel(panel, highlight));
            clear_sdl_touch_pane_button_label_for_panel(panel, highlight);
            changed = true;
            break;

        case 'R':
            sdl_touch_pane_reset_bindings_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }

    screen_load();
}


void do_cmd_controller_settings(void);

int options_menu(int* highlight)
{
    int ch;
    int options = 16;
    int term_wid = 80;
    int term_hgt = 24;
    int title_row = 1;
    int row;
    bool allow_debug_menu = false;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    if (allow_debug_menu && p_ptr->noscore)
        options++;

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 20)
        title_row = 0;

    row = title_row + 2;

    Term_putstr(2, title_row, -1, TERM_WHITE, "Options and misc");

    Term_putstr(2, row++, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "a) Set Keybinds");
    Term_putstr(2, row++, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "b) Controller Settings");
    Term_putstr(2, row++, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "c) Touch Settings");
    Term_putstr(2, row++, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "d) Pane Settings");
    Term_putstr(2, row++, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "e) Interface Options");
    Term_putstr(2, row++, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "f) Efficiency Options");
    Term_putstr(2, row++, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "g) Visual Options");
    Term_putstr(2, row++, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "t) Text Options");
    Term_putstr(2, row++, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "h) Gameplay Options");
    Term_putstr(2, row++, -1, (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "i) Sound Options");
    Term_putstr(2, row++, -1, (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "j) Load a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
        "k) Append Options to a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 13) ? TERM_L_BLUE : TERM_WHITE,
        "l) Set Macros");
    Term_putstr(2, row++, -1, (*highlight == 14) ? TERM_L_BLUE : TERM_WHITE,
        "m) Set Colours");
    Term_putstr(2, row++, -1, (*highlight == 15) ? TERM_L_BLUE : TERM_WHITE,
        "n) Write a note");
    Term_putstr(2, row++, -1, (*highlight == 16) ? TERM_L_BLUE : TERM_WHITE,
        "o) Return to Game");

    if (allow_debug_menu && p_ptr->noscore)
    {
        Term_putstr(2, row++, -1, (*highlight == 17) ? TERM_L_BLUE : TERM_WHITE,
            "p) Debugging Options");
    }

    /* Show product name and version on the bottom of the menu */
    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        if (row < term_hgt)
            Term_putstr(2, row, term_wid - 2, TERM_SLATE, verbuf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, title_row + 1 + *highlight);

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = settings_wait_key();
    inkey_set_cursor_hidden(false);

    if ((ch == 'a') || (ch == 'A'))
    {
        *highlight = 1;
        return (1);
    }

    if ((ch == 'b') || (ch == 'B'))
    {
        *highlight = 2;
        return (2);
    }

    if ((ch == 'c') || (ch == 'C'))
    {
        *highlight = 3;
        return (3);
    }

    if ((ch == 'd') || (ch == 'D'))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == 'e') || (ch == 'E'))
    {
        *highlight = 5;
        return (5);
    }

    if ((ch == 'f') || (ch == 'F'))
    {
        *highlight = 6;
        return (6);
    }

    if ((ch == 'g') || (ch == 'G'))
    {
        *highlight = 7;
        return (7);
    }

    if ((ch == 't') || (ch == 'T'))
    {
        *highlight = 8;
        return (8);
    }

    if ((ch == 'h') || (ch == 'H'))
    {
        *highlight = 9;
        return (9);
    }

    if ((ch == 'i') || (ch == 'I'))
    {
        *highlight = 10;
        return (10);
    }

    if ((ch == 'j') || (ch == 'J'))
    {
        *highlight = 11;
        return (11);
    }

    if ((ch == 'k') || (ch == 'K'))
    {
        *highlight = 12;
        return (12);
    }

    if ((ch == 'l') || (ch == 'L'))
    {
        *highlight = 13;
        return (13);
    }

    if ((ch == 'm') || (ch == 'M'))
    {
        *highlight = 14;
        return (14);
    }

    if ((ch == 'n') || (ch == 'N'))
    {
        *highlight = 15;
        return (15);
    }

    if ((ch == 'o') || (ch == 'O') || (ch == ESCAPE) || (ch == 'q'))
    {
        *highlight = 16;
        return (16);
    }

    if (allow_debug_menu && p_ptr->noscore && ((ch == 'p') || (ch == 'P')))
    {
        *highlight = 17;
        return (17);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

/*
 * Set or unset various options.
 *
 * After using this command, a complete redraw should be performed,
 * in case any visual options have been changed.
 */
void do_cmd_options(void)
{
    int choice = 0;
    int highlight = 1;

    char ftmp[80];

    bool return_to_game = false;
    ui_information_scene_scope settings_scope;
    bool settings_scene = ui_information_scene_enter(&settings_scope);

    /* Clear any active banner before opening options */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    if (!settings_scene)
        screen_save();

    /* Clear screen */
    Term_clear();

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        choice = options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_keybinds();
            Term_clear();
            break;
        }
        case 2:
        {
            do_cmd_controller_settings();
            Term_clear();
            break;
        }
        case 3:
        {
            do_cmd_touch_pane_button_editor(NULL);
            Term_clear();
            break;
        }
        case 4:
        {
            do_cmd_pane_settings();
            Term_clear();
            break;
        }
        case 5:
        {
            do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
            Term_clear();
            break;
        }
        case 6:
        {
            do_cmd_options_aux(EFFICIENCY_PAGE, "Efficiency Options");
            Term_clear();
            break;
        }
        case 7:
        {
            do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
            Term_clear();
            break;
        }
        case 8:
        {
            do_cmd_options_aux(TEXT_PAGE, "Text Options");
            Term_clear();
            break;
        }
        case 9:
        {
            do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
            Term_clear();
            break;
        }
        case 10:
        {
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            Term_clear();
            break;
        }
        case 11:
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(12);
            Term_clear();
            break;
        }
        case 12:
        {
            /* Prompt */
            Term_putstr(2, 14, -1, TERM_SLATE, "(Escape to cancel)");

            /* Prompt */
            prt("File: ", 12, 2);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
            {
                Term_clear();
                continue;
            }

            /* Dump the options */
            if (option_dump(ftmp))
            {
                /* Failure */
                msg_print("Failed!");
            }
            else
            {
                /* Success */
                msg_print("Done.");
            }

            Term_clear();
            break;
        }
        case 13:
        {
            do_cmd_macros();
            Term_clear();
            break;
        }
        case 14:
        {
            do_cmd_colors();
            Term_clear();
            break;
        }
        case 15:
        {
            do_cmd_note("", p_ptr->depth);
            Term_clear();
            break;
        }
        case 16:
        {
            /* Return to Game */
            return_to_game = true;
            Term_clear();
            break;
        }
        case 17:
        {
            /* Debugging Options (only reachable when p_ptr->noscore) */
            do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
            Term_clear();
            break;
        }
        }

    }

    /* Flush messages */
    message_flush();

    /* Clean up */
    if (settings_scene)
        ui_information_scene_leave(&settings_scope);
    else
        screen_load();
}

#ifdef ALLOW_MACROS
/* Forward declaration */
static errr keymap_dump(cptr fname);
#endif

/*
 * Helper to turn a single keycode into printable text for the keybind UI.
 */
static void describe_keycode(byte keycode, char* buf, size_t buflen)
{
    char raw[2];

    if (!buf || !buflen)
        return;

    raw[0] = (char)keycode;
    raw[1] = '\0';

    ascii_to_text(buf, buflen, raw);
}

struct keybind_entry
{
    byte key_code;
    cptr extra_default_keys;
    cptr key_name;
    cptr action;
    bool requires_keymap;
};

static bool key_matches_default(const struct keybind_entry* entry, byte key)
{
    if (key == entry->key_code)
        return true;
    if (entry->extra_default_keys && strchr(entry->extra_default_keys, key))
        return true;
    return false;
}

static bool key_provides_action(int mode, byte key, cptr action, bool requires_keymap)
{
    cptr mapping = keymap_act[mode][key];

    if (requires_keymap)
        return (mapping && streq(mapping, action));

    if (!mapping)
        return true;

    return streq(mapping, action);
}

static bool entry_has_binding(int mode, const struct keybind_entry* entry)
{
    int key;

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
        return true;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                return true;
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        return true;
    }

    return false;
}

/*
 * Build a comma-separated list of keys that trigger the supplied action.
 */
static void describe_action_bindings(int mode, const struct keybind_entry* entry, char* buf,
    size_t buflen)
{
    int key;
    bool found = false;
    size_t current_len = 0;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!entry->action)
    {
        SDL_strlcpy(buf, "(none)", buflen);
        return;
    }

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
    {
        char key_label[16];
        describe_keycode(entry->key_code, key_label, sizeof(key_label));
        SDL_strlcpy(buf, key_label, buflen);
        current_len = strlen(buf);
        found = true;
    }

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
            {
                char key_label[16];
                describe_keycode((byte)*extra, key_label, sizeof(key_label));
                if (found)
                    strnfcat(buf, buflen, &current_len, ", %s", key_label);
                else
                {
                    SDL_strlcpy(buf, key_label, buflen);
                    current_len = strlen(buf);
                    found = true;
                }
            }
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        {
            char key_label[16];
            describe_keycode((byte)key, key_label, sizeof(key_label));
            if (found)
                strnfcat(buf, buflen, &current_len, ", %s", key_label);
            else
            {
                SDL_strlcpy(buf, key_label, buflen);
                current_len = strlen(buf);
                found = true;
            }
        }
    }

    if (!found)
        SDL_strlcpy(buf, "(none)", buflen);
}

/*
 * Remove all key bindings that trigger the specified action.
 */
static void unbind_action(int mode, cptr action)
{
    int key;

    if (!action)
        return;

    for (key = 0; key < 256; key++)
    {
        if (keymap_act[mode][key] && streq(keymap_act[mode][key], action))
        {
            keymap_act[mode][key] = str_free(keymap_act[mode][key]);
        }
    }
}

static bool list_missing_primary_bindings(int mode, const struct keybind_entry* entries,
    int count, char* buffer, size_t buflen)
{
    int i;
    bool ok = true;
    size_t cur = 0;

    if (!buffer || !buflen)
        return true;

    buffer[0] = '\0';

    for (i = 0; i < count; i++)
    {
        if (entry_has_binding(mode, &entries[i]))
            continue;

        if (!ok)
            strnfcat(buffer, buflen, &cur, ", ");
        strnfcat(buffer, buflen, &cur, "%s", entries[i].key_name);
        ok = false;
    }

    return ok;
}

/*
 * Keybind configuration menu
 * Allows rebinding of movement commands for players without a numpad
 */
void do_cmd_keybinds(void)
{
    int mode;
    bool done = false;
    bool dirty = false;
    char ch;
    bool showing_primary = true;
    int highlight_primary = 0;
    int highlight_secondary = 0;
    int top_primary = 0;
    int top_secondary = 0;
    const char* default_file = "user.prf";
    const int list_start_row = 5;
    int term_w, term_h;
    int visible_rows;
    static const struct keybind_entry primary_keybinds[] = {
        {'1', NULL, "Move SW (numpad 1)", ";1", true},
        {'2', NULL, "Move S (numpad 2)", ";2", true},
        {'3', NULL, "Move SE (numpad 3)", ";3", true},
        {'4', NULL, "Move W (numpad 4)", ";4", true},
        {'6', NULL, "Move E (numpad 6)", ";6", true},
        {'7', NULL, "Move NW (numpad 7)", ";7", true},
        {'8', NULL, "Move N (numpad 8)", ";8", true},
        {'9', NULL, "Move NE (numpad 9)", ";9", true},
        {'z', NULL, "Wait (z / numpad 5)", "z", false},
        {'i', NULL, "Inventory", "i", false},
        {'e', NULL, "Equipment", "e", false},
        {'u', NULL, "Use item", "u", false},
        {'x', NULL, "Examine item", "x", false},
        {'s', NULL, "Sing / change song", "s", false},
        {'S', NULL, "Toggle stealth", "S", false},
        {'h', "H@", "Character sheet (h / H / @)", "h", false},
        {'f', NULL, "Fire (primary quiver)", "f", false},
        {'F', NULL, "Fire (secondary quiver)", "F", false},
        {'l', NULL, "Look around", "l", false},
        {'T', NULL, "Tunnel / dig", "T", false},
        {'b', NULL, "Bash door", "b", false},
    };
    
    static const struct keybind_entry secondary_keybinds[] = {
        {'j', NULL, "Supplies overview", "j", false},
        {'.', NULL, "Run (also shift)", ".", false},
        {'/', NULL, "Alt action (also ctrl)", "/", false},
        {'w', NULL, "Wear / wield equipment", "w", false},
        {'r', NULL, "Remove equipment", "r", false},
        {'d', NULL, "Drop item", "d", false},
        {'k', NULL, "Destroy item", "k", false},
        {'g', NULL, "Pick up items", "g", false},
        {'Z', NULL, "Rest", "Z", false},
        {'o', NULL, "Open door / chest", "o", false},
        {'c', NULL, "Close door", "c", false},
        {'D', NULL, "Disarm trap / chest", "D", false},
        {'X', NULL, "Exchange places", "X", false},
        {'-', NULL, "Fletch arrows", "-", false},
        {'{', NULL, "Inscribe item", "{", false},
        {'a', NULL, "Activate staff", "a", false},
        {'E', NULL, "Eat food", "E", false},
        {'t', NULL, "Throw item", "t", false},
        {'p', NULL, "Blow horn", "p", false},
        {'q', NULL, "Quaff potion", "q", false},
        {'M', NULL, "View map", "M", false},
        {'L', NULL, "Pan", "L", false},
        {'0', NULL, "Smithing screen", "0", false},
        {'<', NULL, "Go upstairs", "<", false},
        {'>', NULL, "Go downstairs", ">", false},
        {'m', NULL, "Main menu", "m", false},
        {'?', NULL, "Help", "?", false},
        {'@', NULL, "Character sheet (alternate)", "@", false},
        {'O', NULL, "Options menu", "O", false},
        {':', NULL, "Take notes", ":", false},
        {'~', NULL, "Knowledge browser", "~", false},
        {'[', NULL, "Monster list", "[", false},
        {']', NULL, "Object list", "]", false},
    };
    
    Term_get_size(&term_w, &term_h);
    visible_rows = term_h - list_start_row - 6;
    if (visible_rows < 5)
        visible_rows = 5;
    
    int primary_count = (int)N_ELEMENTS(primary_keybinds);
    int secondary_count = (int)N_ELEMENTS(secondary_keybinds);
    
    /* Determine the keyset mode */
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;
    
    screen_save();
    
    while (!done)
    {
        const struct keybind_entry* keybinds;
        int num_keybinds;
        int* highlight_ptr;
        int* top_ptr;
        int highlight;
        int display_end;
        int row;
        int i;
        bool compact_width;
        char binding_buf[80];
        char line_buf[128];
        int row_width;

        Term_get_size(&term_w, &term_h);
        visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;
        compact_width = (term_w < 70);
        row_width = settings_ui_line_width(2);
        
        if (showing_primary)
        {
            keybinds = primary_keybinds;
            num_keybinds = primary_count;
            highlight_ptr = &highlight_primary;
            top_ptr = &top_primary;
        }
        else
        {
            keybinds = secondary_keybinds;
            num_keybinds = secondary_count;
            highlight_ptr = &highlight_secondary;
            top_ptr = &top_secondary;
        }
        
        if (*highlight_ptr >= num_keybinds)
            *highlight_ptr = num_keybinds - 1;
        if (*highlight_ptr < 0)
            *highlight_ptr = 0;
        
        if (*top_ptr > *highlight_ptr)
            *top_ptr = *highlight_ptr;
        if (*top_ptr + visible_rows <= *highlight_ptr)
            *top_ptr = *highlight_ptr - visible_rows + 1;
        if (*top_ptr < 0)
            *top_ptr = 0;
        if (num_keybinds > visible_rows)
        {
            int max_top = num_keybinds - visible_rows;
            if (*top_ptr > max_top)
                *top_ptr = max_top;
        }
        else
        {
            *top_ptr = 0;
        }
        
        highlight = *highlight_ptr;
        
        /* Clear screen */
        Term_clear();

        /* Title */
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Keybind Configuration");
        if (compact_width)
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "8/2 move  Enter bind  Tab switch  Esc return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary commands" : "Supplementary commands");
        }
        else
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "Arrow to navigate, Enter to bind, Tab to switch groups, Escape to return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary Commands: Essential for the gameplay"
                                : "Supplementary Commands");
        }
        
        /* List visible keybinds */
        display_end = *top_ptr + visible_rows;
        if (display_end > num_keybinds)
            display_end = num_keybinds;
        for (i = *top_ptr; i < display_end; i++)
        {
            int entry_row = list_start_row + (i - *top_ptr);
            describe_action_bindings(mode, &keybinds[i], binding_buf, sizeof(binding_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                keybinds[i].key_name, binding_buf, row_width, 12);

            /* Display the keybind */
            if (i == highlight)
            {
                /* Highlighted */
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            }
            else
            {
                /* Normal */
                prt(line_buf, entry_row, 2);
            }
        }
        
        /* Clear any leftover rows */
        for (i = display_end; i < *top_ptr + visible_rows; i++)
        {
            row = list_start_row + (i - *top_ptr);
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }
        
        /* Instructions at bottom */
        if (compact_width)
        {
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                "s: save keybinds");
            settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
                "r: reset selected");
        }
        else
        {
            strnfmt(line_buf, sizeof(line_buf), "Press 's' to save keybinds to %s",
                default_file);
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                line_buf);
            settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
                "Press 'r' to reset selected keybind to default");
        }
        if (dirty)
            c_prt(TERM_YELLOW, "Unsaved changes", list_start_row + visible_rows + 3, 2);
        else
            Term_erase(2, list_start_row + visible_rows + 3,
                term_w > 2 ? term_w - 2 : 0);
        
        /* Get input */
        ch = settings_wait_key();
        
        /* Handle input */
        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            char missing[256];
            if (!list_missing_primary_bindings(mode, primary_keybinds, primary_count, missing,
                    sizeof(missing)))
            {
                char prompt[512];
                strnfmt(prompt, sizeof(prompt),
                    "Essential commands are unbound (%s). Exit anyway? ", missing);
                if (!get_check(prompt))
                    continue;
            }
            done = true;
        }
        else if (ch == '\t')
        {
            showing_primary = !showing_primary;
            continue;
        }
        else if (ch == '8')
        {
            /* Move up */
            if (num_keybinds > 0)
            {
                highlight = (highlight + num_keybinds - 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '2')
        {
            /* Move down */
            if (num_keybinds > 0)
            {
                highlight = (highlight + 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            /* Rebind the selected key */
            cptr action = keybinds[highlight].action;
            char key_label[32];
            char prompt[80];
            char prompt_long[96];
            char prompt_short[80];
            int entry_row = list_start_row + (highlight - *top_ptr);

            /* Clear the action area */
            Term_erase(2, entry_row, 255);
            
            /* Prompt for new binding */
            strnfmt(prompt_long, sizeof(prompt_long),
                "Press key to use for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            strnfmt(prompt_short, sizeof(prompt_short),
                "Bind %s (Esc cancels):", keybinds[highlight].key_name);
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(row_width, prompt_long, prompt_short,
                    prompt_short));
            settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
            Term_fresh();
            
            /* Get the key to bind */
            flush();
            char bind_key = inkey();
            
            if (bind_key != ESCAPE && bind_key != 0)
            {
                byte new_key = (byte)bind_key;
                
                /* Clear any existing action on the chosen key */
                keymap_act[mode][new_key] = str_free(keymap_act[mode][new_key]);
                keymap_act[mode][new_key] = str_dup(action);
                dirty = true;
                
                describe_keycode(new_key, key_label, sizeof(key_label));
                msg_format("Key %s now performs %s", key_label, keybinds[highlight].key_name);
                message_flush();
            }
        }
        else if (ch == 'r' || ch == 'R')
        {
            /* Reset to default */
            byte target_key = keybinds[highlight].key_code;
            char key_label[32];
            cptr action = keybinds[highlight].action;

            /* Remove the action from any custom keys */
            unbind_action(mode, action);
            
            /* Restore default action */
            keymap_act[mode][target_key] = str_free(keymap_act[mode][target_key]);
            if (keybinds[highlight].requires_keymap)
                keymap_act[mode][target_key] = str_dup(action);
            
            dirty = true;
            
            describe_keycode(target_key, key_label, sizeof(key_label));
            msg_format("Reset %s to default key %s", keybinds[highlight].key_name, key_label);
            message_flush();
        }
        else if (ch == 's' || ch == 'S')
        {
#ifdef ALLOW_MACROS
            /* Save keybinds to file */
            char ftmp[80];
            
            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s", default_file);
            
            /* Clear prompt area */
            prt("                                                              ", list_start_row + visible_rows + 1, 2);
            prt("File: ", list_start_row + visible_rows + 1, 2);
            
            /* Ask for a file */
            if (askfor_aux(ftmp, sizeof(ftmp)))
            {
                /* Dump the keymaps */
                if (keymap_dump(ftmp) == 0)
                {
                    msg_format("Keybinds saved to %s.", ftmp);
                    dirty = false;
                }
                else
                {
                    msg_print("Failed to save keybinds.");
                }
                message_flush();
            }
#else
            msg_print("Saving keybinds is not available in this build.");
            message_flush();
#endif
        }
        
        /* Store updated highlight for the active group */
        *highlight_ptr = highlight;
    }
    
    /* Load screen */
    screen_load();

    if (dirty)
    {
        char prompt[80];
        strnfmt(prompt, sizeof(prompt), "Save keybinds to %s? ", default_file);
        if (get_check(prompt))
        {
            if (keymap_dump(default_file) == 0)
            {
                msg_format("Keybinds saved to %s.", default_file);
                message_flush();
            }
            else
            {
                msg_print("Failed to save keybinds.");
                message_flush();
            }
        }
    }
}

typedef enum controller_entry_type {
    CONTROLLER_ENTRY_TOGGLE = 0,
    CONTROLLER_ENTRY_ACTION,
} controller_entry_type;

typedef enum controller_toggle_id {
    CONTROLLER_TOGGLE_ENABLED = 0,
    CONTROLLER_TOGGLE_AUTO_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_MODE,
    CONTROLLER_TOGGLE_DPAD,
    CONTROLLER_TOGGLE_LEFT_STICK,
} controller_toggle_id;

typedef struct controller_entry {
    controller_entry_type type;
    int id;
    const char* label;
} controller_entry;

static const char* controller_gamepad_button_label(int button)
{
    switch (button) {
    case GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case GAMEPAD_BUTTON_EAST: return "B (East)";
    case GAMEPAD_BUTTON_WEST: return "X (West)";
    case GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case GAMEPAD_BUTTON_START: return "Start (Menu)";
    case GAMEPAD_BUTTON_BACK: return "Back (View)";
    case GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case GAMEPAD_BUTTON_MISC1: return "Misc1";
    case GAMEPAD_BUTTON_MISC2: return "Misc2";
    case GAMEPAD_BUTTON_MISC3: return "Misc3";
    case GAMEPAD_BUTTON_MISC4: return "Misc4";
    case GAMEPAD_BUTTON_MISC5: return "Misc5";
    case GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* controller_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* controller_gamepad_stick_dir_label(int type, int dir)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* dir_label = NULL;

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; break;
    default: dir_label = "Unknown"; break;
    }

    return format("%s %s", stick, dir_label);
}

static const char* controller_gamepad_combo_label(void)
{
    return "L1+R1 Combo";
}

static void controller_binding_label(int type, int id, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, controller_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, controller_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, controller_gamepad_stick_dir_label(type, id), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, controller_gamepad_combo_label(), buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static int controller_action_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_button_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_trigger_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_left_stick_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_right_stick_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (get_sdl_gamepad_shoulder_combo_binding() == binding) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

static void controller_action_binding_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = controller_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        controller_binding_label(type, id, buf, buflen);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

static bool controller_binding_matches_action(int binding, int type, int id)
{
    if (type == GAMEPAD_CAPTURE_BUTTON)
        return get_sdl_gamepad_button_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_TRIGGER)
        return get_sdl_gamepad_trigger_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_LEFT_STICK)
        return get_sdl_gamepad_left_stick_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_RIGHT_STICK)
        return get_sdl_gamepad_right_stick_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO)
        return get_sdl_gamepad_shoulder_combo_binding() == binding;
    return false;
}

void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple")) {
        SDL_strlcpy(buf, fallback, buflen);
    }
}

static void controller_entry_value(const controller_entry* entry, char* buf, size_t buflen)
{
    if (!entry || !buf || !buflen)
        return;

    switch (entry->type) {
    case CONTROLLER_ENTRY_TOGGLE:
        switch (entry->id) {
        case CONTROLLER_TOGGLE_ENABLED:
            SDL_strlcpy(buf, get_sdl_gamepad_enabled() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_AUTO_MODE:
            SDL_strlcpy(buf, get_sdl_gamepad_auto_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_STEAMDECK_MODE:
            SDL_strlcpy(buf, get_sdl_steamdeck_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_DPAD:
            SDL_strlcpy(buf, get_sdl_gamepad_use_dpad() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_LEFT_STICK:
            SDL_strlcpy(buf, get_sdl_gamepad_use_left_stick() ? "On" : "Off", buflen);
            break;
        default:
            SDL_strlcpy(buf, "(unknown)", buflen);
            break;
        }
        break;
    case CONTROLLER_ENTRY_ACTION:
        controller_action_binding_label(entry->id, buf, buflen);
        break;
    default:
        SDL_strlcpy(buf, "(unknown)", buflen);
        break;
    }
}

static void controller_set_toggle(int toggle_id, bool value)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        set_sdl_gamepad_enabled(value);
        break;
    case CONTROLLER_TOGGLE_AUTO_MODE:
        set_sdl_gamepad_auto_mode(value);
        break;
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        set_sdl_steamdeck_mode(value);
        break;
    case CONTROLLER_TOGGLE_DPAD:
        set_sdl_gamepad_use_dpad(value);
        break;
    case CONTROLLER_TOGGLE_LEFT_STICK:
        set_sdl_gamepad_use_left_stick(value);
        break;
    default:
        break;
    }
}

static void controller_clear_action_bindings(int binding, int skip_type, int skip_id)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_button_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_BUTTON && skip_id == i)
                continue;
            set_sdl_gamepad_button_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_trigger_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_TRIGGER && skip_id == i)
                continue;
            set_sdl_gamepad_trigger_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_left_stick_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_LEFT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_left_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_right_stick_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_RIGHT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_right_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    if (get_sdl_gamepad_shoulder_combo_binding() == binding) {
        if (!(skip_type == GAMEPAD_CAPTURE_SHOULDER_COMBO))
            set_sdl_gamepad_shoulder_combo_binding(GAMEPAD_BIND_NONE);
    }
}

static void controller_assign_action_binding(int binding, int type, int id)
{
    controller_clear_action_bindings(binding, type, id);

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        set_sdl_gamepad_button_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        set_sdl_gamepad_trigger_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK) {
        set_sdl_gamepad_left_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        set_sdl_gamepad_right_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        set_sdl_gamepad_shoulder_combo_binding(binding);
    }
}

static bool controller_action_default_binding(int binding, int* out_type, int* out_id)
{
    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_default_button_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_BUTTON;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_default_trigger_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_default_left_stick_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_default_right_stick_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    if (get_sdl_gamepad_default_shoulder_combo_binding() == binding) {
        if (out_type)
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
        if (out_id)
            *out_id = 0;
        return true;
    }

    return false;
}

void do_cmd_controller_settings(void)
{
    bool done = false;
    int highlight = 0;
    int top = 0;
    int term_w, term_h;
    const int list_start_row = 5;

    static const controller_entry entries[] = {
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_ENABLED, "Controller Input" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_AUTO_MODE, "Auto Controller Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_MODE, "Steam Deck UI Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_DPAD, "D-pad Movement" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_LEFT_STICK, "Left Stick Movement" },
        { CONTROLLER_ENTRY_ACTION, ' ', "Confirm (Space)" },
        { CONTROLLER_ENTRY_ACTION, '\r', "Enter" },
        { CONTROLLER_ENTRY_ACTION, ESCAPE, "Escape" },
        { CONTROLLER_ENTRY_ACTION, '\t', "Abilities (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire (primary)" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire (secondary)" },
        { CONTROLLER_ENTRY_ACTION, 'l', "Look around" },
        { CONTROLLER_ENTRY_ACTION, 'T', "Tunnel / dig" },
        { CONTROLLER_ENTRY_ACTION, 'b', "Bash door" },
        { CONTROLLER_ENTRY_ACTION, 'z', "Wait" },
        { CONTROLLER_ENTRY_ACTION, 'j', "Supplies overview" },
        { CONTROLLER_ENTRY_ACTION, '.', "Run" },
        { CONTROLLER_ENTRY_ACTION, '/', "Alt action" },
        { CONTROLLER_ENTRY_ACTION, 'w', "Wear / wield" },
        { CONTROLLER_ENTRY_ACTION, 'r', "Remove equipment" },
        { CONTROLLER_ENTRY_ACTION, 'd', "Drop item" },
        { CONTROLLER_ENTRY_ACTION, 'k', "Destroy item" },
        { CONTROLLER_ENTRY_ACTION, 'g', "Pick up items" },
        { CONTROLLER_ENTRY_ACTION, 'Z', "Rest" },
        { CONTROLLER_ENTRY_ACTION, 'o', "Open door / chest" },
        { CONTROLLER_ENTRY_ACTION, 'c', "Close door" },
        { CONTROLLER_ENTRY_ACTION, 'D', "Disarm trap / chest" },
        { CONTROLLER_ENTRY_ACTION, 'X', "Exchange places" },
        { CONTROLLER_ENTRY_ACTION, '-', "Fletch arrows" },
        { CONTROLLER_ENTRY_ACTION, '{', "Inscribe item" },
        { CONTROLLER_ENTRY_ACTION, 'a', "Activate staff" },
        { CONTROLLER_ENTRY_ACTION, 'E', "Eat food" },
        { CONTROLLER_ENTRY_ACTION, 't', "Throw item" },
        { CONTROLLER_ENTRY_ACTION, 'p', "Blow horn" },
        { CONTROLLER_ENTRY_ACTION, 'q', "Quaff potion" },
        { CONTROLLER_ENTRY_ACTION, 'M', "View map" },
        { CONTROLLER_ENTRY_ACTION, 'L', "Pan view" },
        { CONTROLLER_ENTRY_ACTION, '0', "Smithing screen" },
        { CONTROLLER_ENTRY_ACTION, '<', "Go upstairs" },
        { CONTROLLER_ENTRY_ACTION, '>', "Go downstairs" },
        { CONTROLLER_ENTRY_ACTION, 'm', "Main menu" },
        { CONTROLLER_ENTRY_ACTION, '?', "Help" },
        { CONTROLLER_ENTRY_ACTION, 'O', "Options menu" },
        { CONTROLLER_ENTRY_ACTION, ':', "Take notes" },
        { CONTROLLER_ENTRY_ACTION, '~', "Knowledge browser" },
        { CONTROLLER_ENTRY_ACTION, '[', "Monster list" },
        { CONTROLLER_ENTRY_ACTION, ']', "Object list" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_SHIFT, "Shift modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_CTRL, "Ctrl modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_ALT, "Alt modifier" },
    };

    int entry_count = (int)N_ELEMENTS(entries);

    screen_save();

    while (!done) {
        char value_buf[64];
        char line_buf[128];
        int row;
        bool steamdeck = steamdeck_controls_active();
        bool compact_width;
        int row_width;

        Term_get_size(&term_w, &term_h);
        row_width = settings_ui_line_width(2);
        int visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;
        compact_width = (term_w < 70);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;
        if (entry_count > visible_rows) {
            int max_top = entry_count - visible_rows;
            if (top > max_top)
                top = max_top;
        } else {
            top = 0;
        }

        Term_clear();
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Controller Settings");
        if (steamdeck) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: A=bind, B=back */
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "D-pad %s bind  %s back"
                              : "D-pad navigate  %s bind  %s back",
                confirm_label, back_label);
            settings_ui_put_fitted(2, 0, TERM_WHITE, prompt_buf);
        } else {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                compact_width ? "8/2 move  Enter bind  Esc return"
                              : "Arrow to navigate, Enter to bind, Escape to return");
        }

        for (int i = top; i < entry_count && i < top + visible_rows; i++) {
            int entry_row = list_start_row + (i - top);
            controller_entry_value(&entries[i], value_buf, sizeof(value_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf), entries[i].label,
                value_buf, row_width, 12);

            if (i == highlight) {
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            } else {
                prt(line_buf, entry_row, 2);
            }
        }

        for (row = list_start_row + (entry_count - top); row < list_start_row + visible_rows; row++) {
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }

        if (steamdeck) {
            char reset_label[16];
            char reset_all_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: X=reset selected, Y=reset all */
            controller_prompt_label(steamdeck_alt_action_key(), "X", reset_label, sizeof(reset_label));
            controller_prompt_label(steamdeck_secondary_key(), "Y", reset_all_label, sizeof(reset_all_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "[%s] reset  [%s] reset all"
                              : "Reset: [%s] selected, [%s] all",
                reset_label, reset_all_label);
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                prompt_buf);
        } else {
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                compact_width ? "r: reset selected  R: reset all"
                              : "Press 'r' to reset selected binding, 'R' to reset all bindings");
        }
        settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
            compact_width ? "Saves on exit." : "Changes are saved on exit.");

        char ch = settings_wait_key();

        if (ch == ESCAPE || ch == 'q' || ch == 'Q' || (steamdeck && ch == steamdeck_back_key())) {
            done = true;
        } else if (ch == '8') {
            highlight = (highlight + entry_count - 1) % entry_count;
        } else if (ch == '2') {
            highlight = (highlight + 1) % entry_count;
        } else if (ch == 'r' || (steamdeck && ch == steamdeck_alt_action_key())) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                int binding_type = 0;
                int binding_id = 0;
                if (controller_action_default_binding(entries[highlight].id, &binding_type, &binding_id)) {
                    controller_assign_action_binding(entries[highlight].id, binding_type, binding_id);
                    msg_print("Binding reset to default.");
                } else {
                    controller_clear_action_bindings(entries[highlight].id, -1, -1);
                    msg_print("No default binding for action.");
                }
                message_flush();
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            sdl_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];
            int entry_row = list_start_row + (highlight - top);

            if (entry->type == CONTROLLER_ENTRY_TOGGLE) {
                char cur[16];
                controller_entry_value(entry, cur, sizeof(cur));
                controller_set_toggle(entry->id, streq(cur, "Off"));
            } else {
                char prompt[80];
                char prompt_long[96];
                char prompt_medium[80];
                char prompt_short[64];
                int cap_type = 0;
                int cap_id = 0;
                Term_erase(2, entry_row, 255);
                if (steamdeck) {
                    char cancel_label[16];
                    controller_prompt_label(steamdeck_back_key(), "B", cancel_label, sizeof(cancel_label));
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Press button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Bind %s  (%s cancel)", entry->label, cancel_label);
                } else {
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s (Esc=cancel, Backspace=clear)",
                        entry->label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Bind %s (Esc=cancel, Bksp=clear)", entry->label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s (Esc cancel, Bksp clear)", entry->label);
                }
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                        prompt_short));
                settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
                Term_fresh();

                flush();
                if (!sdl_gamepad_capture_begin()) {
                    msg_print("No controller detected.");
                    message_flush();
                    continue;
                }

                bool waiting = true;
                while (waiting) {
                    if (sdl_gamepad_capture_poll(&cap_type, &cap_id)) {
                        if (controller_binding_matches_action(ESCAPE, cap_type, cap_id)) {
                            sdl_gamepad_capture_cancel();
                            waiting = false;
                            break;
                        }
                        controller_assign_action_binding(entry->id, cap_type, cap_id);
                        waiting = false;
                        break;
                    }

                    inkey_set_scan(true);
                    char choice = inkey();
                    if (choice == ESCAPE) {
                        sdl_gamepad_capture_cancel();
                        waiting = false;
                    } else if (choice == '\b' || choice == 127) {
                        sdl_gamepad_capture_cancel();
                        controller_clear_action_bindings(entry->id, -1, -1);
                        waiting = false;
                    } else if (choice == 0) {
                        Term_xtra(TERM_XTRA_DELAY, 10);
                    }
                }
            }
        }
    }

    screen_load();
}

#ifdef ALLOW_MACROS

/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
    static cptr mark = "Macro Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("macro_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old macros */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic macro dump\n\n");

    /* Dump them */
    for (i = 0; i < macro__num; i++)
    {
        /* Start the macro */
        SDL_IOprintf(fff, "# Macro '%d'\n\n", i);

        /* Extract the macro action */
        ascii_to_text(buf, sizeof(buf), macro__act[i]);

        /* Dump the macro action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Extract the macro pattern */
        ascii_to_text(buf, sizeof(buf), macro__pat[i]);

        /* Dump the macro pattern */
        SDL_IOprintf(fff, "P:%s\n", buf);

        /* End the macro */
        SDL_IOprintf(fff, "\n\n");
    }

    /* Start dumping */
    SDL_IOprintf(fff, "\n\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Note the complex use of the "inkey()" function from "util.c".
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux(char* buf)
{
    char ch;

    int n = 0;

    char tmp[1024];

    /* Flush */
    flush();

    /* Do not process macros */
    inkey_set_base(true);

    /* First key */
    ch = inkey();

    /* Read the pattern */
    while (ch != '\0')
    {
        /* Save the key */
        buf[n++] = ch;

        /* Do not process macros */
        inkey_set_base(true);

        /* Do not wait for keys */
        inkey_set_scan(true);

        /* Attempt to read a key */
        ch = inkey();
    }

    /* Terminate */
    buf[n] = '\0';

    /* Flush */
    flush();

    /* Convert the trigger */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);
}

/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux_keymap(char* buf)
{
    char tmp[1024];

    /* Flush */
    flush();

    /* Get a key */
    buf[0] = inkey();
    buf[1] = '\0';

    /* Convert to ascii */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);

    /* Flush */
    flush();
}

/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
    static cptr mark = "Keymap Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("keymap_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old keymaps */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic keymap dump\n\n");

    /* Dump them */
    for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
    {
        char key[2] = "?";

        cptr act;

        /* Loop up the keymap */
        act = keymap_act[mode][i];

        /* Skip empty keymaps */
        if (!act)
            continue;

        /* Encode the action */
        ascii_to_text(buf, sizeof(buf), act);

        /* Dump the keymap action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Convert the key into a string */
        key[0] = i;

        /* Encode the key */
        ascii_to_text(buf, sizeof(buf), key);

        /* Dump the keymap pattern */
        SDL_IOprintf(fff, "C:%d:%s\n", mode, buf);

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

#endif

/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
    char ch;

    char tmp[1024];

    char pat[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening macros menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        int term_wid = 80;
        int term_hgt = 24;
        int title_row = 1;
        int menu_row = 3;
        int action_label_row;
        int action_row;
        int command_row;
        int input_row;

        Term_get_size(&term_wid, &term_hgt);
        action_label_row = MAX(menu_row + 11, term_hgt - 4);
        action_row = MIN(term_hgt - 2, action_label_row + 1);
        command_row = MAX(action_row + 1, term_hgt - 2);
        input_row = MAX(command_row + 1, term_hgt - 1);

        /* Clear screen */
        Term_clear();

        /* Describe */
        prt("Interact with Macros", title_row, 0);

        /* Describe that action */
        prt("Current action:", action_label_row, 0);

        /* Analyze the current action */
        ascii_to_text(tmp, sizeof(tmp), macro_buffer);

        /* Display the current action */
        Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

        /* Selections */
        prt("(1) Load a user pref file", menu_row, 5);
#ifdef ALLOW_MACROS
        prt("(2) Append macros to a file", menu_row + 1, 5);
        prt("(3) Query a macro", menu_row + 2, 5);
        prt("(4) Create a macro", menu_row + 3, 5);
        prt("(5) Remove a macro", menu_row + 4, 5);
        prt("(6) Append keymaps to a file", menu_row + 5, 5);
        prt("(7) Query a keymap", menu_row + 6, 5);
        prt("(8) Create a keymap", menu_row + 7, 5);
        prt("(9) Remove a keymap", menu_row + 8, 5);
        prt("(0) Enter a new action", menu_row + 9, 5);
#endif /* ALLOW_MACROS */

        /* Prompt */
        prt("Command: ", command_row, 0);

        /* Get a command */
        ch = settings_wait_key();

        /* Leave */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(command_row);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (ch == '2')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append macros to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)macro_dump(ftmp);

            /* Prompt */
            msg_print("Appended macros.");
        }

        /* Query a macro */
        else if (ch == '3')
        {
            int k;

            /* Prompt */
            prt("Command: Query a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Get the action */
            k = macro_find_exact(pat);

            /* Nothing found */
            if (k < 0)
            {
                /* Prompt */
                msg_print("Found no macro.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a macro.");
            }
        }

        /* Create a macro */
        else if (ch == '4')
        {
            /* Prompt */
            prt("Command: Create a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Link the macro */
                macro_add(pat, macro_buffer);

                /* Prompt */
                msg_print("Added a macro.");
            }
        }

        /* Remove a macro */
        else if (ch == '5')
        {
            /* Prompt */
            prt("Command: Remove a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Link the macro */
            macro_add(pat, pat);

            /* Prompt */
            msg_print("Removed a macro.");
        }

        /* Save keymaps */
        else if (ch == '6')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append keymaps to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)keymap_dump(ftmp);

            /* Prompt */
            msg_print("Appended keymaps.");
        }

        /* Query a keymap */
        else if (ch == '7')
        {
            cptr act;

            /* Prompt */
            prt("Command: Query a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Look up the keymap */
            act = keymap_act[mode][(byte)(pat[0])];

            /* Nothing found */
            if (!act)
            {
                /* Prompt */
                msg_print("Found no keymap.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, act, sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a keymap.");
            }
        }

        /* Create a keymap */
        else if (ch == '8')
        {
            /* Prompt */
            prt("Command: Create a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Free old keymap */
                str_free(keymap_act[mode][(byte)(pat[0])]);

                /* Make new keymap */
                keymap_act[mode][(byte)(pat[0])] = str_dup(macro_buffer);

                /* Prompt */
                msg_print("Added a keymap.");
            }
        }

        /* Remove a keymap */
        else if (ch == '9')
        {
            /* Prompt */
            prt("Command: Remove a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Free old keymap */
            str_free(keymap_act[mode][(byte)(pat[0])]);

            /* Make new keymap */
            keymap_act[mode][(byte)(pat[0])] = NULL;

            /* Prompt */
            msg_print("Removed a keymap.");
        }

        /* Enter a new action */
        else if (ch == '0')
        {
            /* Prompt */
            prt("Command: Enter a new action", command_row, 0);

            /* Go to the correct location */
            Term_gotoxy(0, action_row);

            /* Analyze the current action */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Extract an action */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
            }
        }

#endif /* ALLOW_MACROS */

        /* Oops */
        else
        {
            /* Oops */
            bell("Illegal command for macros!");
        }

        /* Flush messages */
        message_flush();
    }

    screen_load();
}

/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * It erases the given line.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte* attr, int y)
{
    byte base, shade, temp;
    bool changed = false;
    char *msg, *pos;
    int ch;

    /* Start with the given base color */
    base = GET_BASE_COLOR(*attr);

    /* 1. Query for base color */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Format the query */
        msg = format("1. Choose base color (use arrows) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            color_names[base], base);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(base, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current base color */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous color if possible */
        if ((ch == '4') && (base > 0))
        {
            --base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }

        /* Move to the next color if possible */
        if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
        {
            ++base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }
    }

    /* The player selected a different base color, start from shade 0 */
    if (changed)
        shade = 0;
    /* We assume that the player is editing the current shade, go there */
    else
        shade = GET_SHADE(*attr);

    /* 2. Query for specific shade */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Create the real color */
        temp = MAKE_EXTENDED_COLOR(base, shade);

        /* Format the message */
        msg = format("2. Choose shade (use arrows) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            get_ext_color_name(temp), temp);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(temp, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current shade */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous shade if possible */
        if ((ch == '4') && (shade > 0))
        {
            --shade;
            continue;
        }

        /* Move to the next shade if possible */
        if ((ch == '6') && (shade < MAX_SHADES - 1))
        {
            ++shade;
            continue;
        }
    }

    /* Assign the selected shade */
    *attr = temp;

    /* Clear the line. It is needed to fit in the current UI */
    Term_erase(0, y, 255);
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
    int ch;
    int cx;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Visuals", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_VISUALS
        prt("(2) Dump monster attr/chars", 5, 5);
        prt("(3) Dump object attr/chars", 6, 5);
        prt("(4) Dump feature attr/chars", 7, 5);
        prt("(5) Dump flavor attr/chars", 8, 5);
        prt("(6) Change monster attr/chars", 9, 5);
        prt("(7) Change object attr/chars", 10, 5);
        prt("(8) Change feature attr/chars", 11, 5);
        prt("(9) Change flavor attr/chars", 12, 5);
#endif
        prt("(0) Reset visuals", 13, 5);

        /* Prompt */
        prt("Command: ", 15, 0);

        /* Prompt */
        ch = settings_wait_key();

        /* Done */
        if (ch == ESCAPE)
            break;

        if ((ch >= '6') && (ch <= '9'))
        {
            int term_wid = 80;
            int term_hgt = 24;

            Term_get_size(&term_wid, &term_hgt);
            if ((term_wid < 60) || (term_hgt < 21))
            {
                msg_print("The attr/char editor requires a larger window than compact mode.");
                message_flush();
                continue;
            }
        }

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (ch == '2')
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump monster attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_monsters: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Monster attr/char definitions\n\n");

            /* Dump monsters */
            for (i = 0; i < z_info->r_max; i++)
            {
                monster_race* r_ptr = &r_info[i];

                /* Skip non-entries */
                if (!r_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (r_name + r_ptr->name));

                /* Dump the monster attr/char info */
                dump_visual_pair(fff, "R", i, r_ptr->x_attr, (byte)r_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped monster attr/chars.");
        }

        /* Dump object attr/chars */
        else if (ch == '3')
        {
            static cptr mark = "Object attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump object attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_objects: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Object attr/char definitions\n\n");

            /* Dump objects */
            for (i = 0; i < z_info->k_max; i++)
            {
                object_kind* k_ptr = &k_info[i];

                /* Skip non-entries */
                if (!k_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (k_name + k_ptr->name));

                /* Dump the object attr/char info */
                dump_visual_pair(
                    fff, "K", i, k_ptr->x_attr, (byte)k_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped object attr/chars.");
        }

        /* Dump feature attr/chars */
        else if (ch == '4')
        {
            static cptr mark = "Feature attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump feature attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_features: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Feature attr/char definitions\n\n");

            /* Dump features */
            for (i = 0; i < z_info->f_max; i++)
            {
                feature_type* f_ptr = &f_info[i];

                /* Skip non-entries */
                if (!f_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (f_name + f_ptr->name));

                /* Dump the feature attr/char info */
                dump_visual_pair(
                    fff, "F", i, f_ptr->x_attr, (byte)f_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped feature attr/chars.");
        }

        /* Dump flavor attr/chars */
        else if (ch == '5')
        {
            static cptr mark = "Flavor attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump flavor attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_flavors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Flavor attr/char definitions\n\n");

            /* Dump flavors */
            for (i = 0; i < z_info->flavor_max; i++)
            {
                flavor_type* flavor_ptr = &flavor_info[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (flavor_text + flavor_ptr->text));

                /* Dump the flavor attr/char info */
                dump_visual_pair(
                    fff, "L", i, flavor_ptr->x_attr, (byte)flavor_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped flavor attr/chars.");
        }

        /* Modify monster attr/chars */
        else if (ch == '6')
        {
            static int r = 0;

            /* Prompt */
            prt("Command: Change monster attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                monster_race* r_ptr = &r_info[r];

                byte da = (byte)(r_ptr->d_attr);
                byte dc = (byte)(r_ptr->d_char);
                byte ca = (byte)(r_ptr->x_attr);
                byte cc = (byte)(r_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Monster = %d, Name = %-40.40s", r,
                        (r_name + r_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3u / %3u", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3u / %3u", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = settings_wait_key();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    r = (r + z_info->r_max + 1) % z_info->r_max;
                if (cx == 'N')
                    r = (r + z_info->r_max - 1) % z_info->r_max;
                if (cx == 'a')
                    r_ptr->x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    r_ptr->x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    r_ptr->x_char = (byte)(cc + 1);
                if (cx == 'C')
                    r_ptr->x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&r_ptr->x_attr, 22);
                }
            }
        }

        /* Modify object attr/chars */
        else if (ch == '7')
        {
            static int k = 0;

            /* Prompt */
            prt("Command: Change object attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                object_kind* k_ptr = &k_info[k];

                byte da = (byte)(k_ptr->d_attr);
                byte dc = (byte)(k_ptr->d_char);
                byte ca = (byte)(k_ptr->x_attr);
                byte cc = (byte)(k_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Object = %d, Name = %-40.40s", k,
                        (k_name + k_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = settings_wait_key();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    k = (k + z_info->k_max + 1) % z_info->k_max;
                if (cx == 'N')
                    k = (k + z_info->k_max - 1) % z_info->k_max;
                if (cx == 'a')
                    k_info[k].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    k_info[k].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    k_info[k].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    k_info[k].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&k_info[k].x_attr, 22);
                }
            }
        }

        /* Modify feature attr/chars */
        else if (ch == '8')
        {
            static int f = 0;

            /* Prompt */
            prt("Command: Change feature attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                feature_type* f_ptr = &f_info[f];

                byte da = (byte)(f_ptr->d_attr);
                byte dc = (byte)(f_ptr->d_char);
                byte ca = (byte)(f_ptr->x_attr);
                byte cc = (byte)(f_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Terrain = %d, Name = %-40.40s", f,
                        (f_name + f_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = settings_wait_key();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->f_max + 1) % z_info->f_max;
                if (cx == 'N')
                    f = (f + z_info->f_max - 1) % z_info->f_max;
                if (cx == 'a')
                    f_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    f_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    f_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    f_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&f_info[f].x_attr, 22);
                }
            }
        }

        /* Modify flavor attr/chars */
        else if (ch == '9')
        {
            static int f = 0;

            /* Prompt */
            prt("Command: Change flavor attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                flavor_type* flavor_ptr = &flavor_info[f];

                byte da = (byte)(flavor_ptr->d_attr);
                byte dc = (byte)(flavor_ptr->d_char);
                byte ca = (byte)(flavor_ptr->x_attr);
                byte cc = (byte)(flavor_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Flavor = %d, Text = %-40.40s", f,
                        (flavor_text + flavor_ptr->text)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = settings_wait_key();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
                if (cx == 'N')
                    f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
                if (cx == 'a')
                    flavor_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    flavor_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    flavor_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    flavor_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&flavor_info[f].x_attr, 22);
                }
            }
        }

#endif /* ALLOW_VISUALS */

        /* Reset visuals */
        else if (ch == '0')
        {
            /* Reset */
            reset_visuals(true);

            /* Message */
            msg_print("Visual attr/char tables reset.");
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for visuals!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Asks to the user for specific color values.
 * Returns true if the color was modified.
 */
static bool askfor_color_values(int idx)
{
    char str[10];

    int k, r, g, b;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][1]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Red (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    r = atoi(str);

    /* Check bounds */
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][2]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Green (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    g = atoi(str);

    /* Check bounds */
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][3]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Blue (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    b = atoi(str);

    /* Check bounds */
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][0]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Extra (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    k = atoi(str);

    /* Check bounds */
    if (k < 0)
        k = 0;
    if (k > 255)
        k = 255;

    /* Do nothing if the color is not modified */
    if ((k == angband_color_table[idx][0]) && (r == angband_color_table[idx][1])
        && (g == angband_color_table[idx][2])
        && (b == angband_color_table[idx][3]))
        return false;

    /* Modify the color table */
    angband_color_table[idx][0] = k;
    angband_color_table[idx][1] = r;
    angband_color_table[idx][2] = g;
    angband_color_table[idx][3] = b;

    /* Notify the changes */
    return true;
}

/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx)                                                          \
    ((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2]       \
            + (int)angband_color_table[idx][3]                                 \
        == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
    int x, y, idx, old_idx;
    char ch;
    char msg[100];

    /* Flags */
    bool do_move, do_update;

    /* Clear the screen */
    Term_clear();

    /* Draw the color table */
    for (idx = 0; idx < MAX_COLORS; idx++)
    {
        /* Get coordinates, the x value is adjusted to show a fake cursor */
        x = COLOR_X(idx) + 1;
        y = COLOR_Y(idx);

        /* Show a sample of the color */
        if (IS_BLACK(idx))
            c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
        else
            c_put_str(idx, COLOR_SAMPLE, y, x);
    }

    /* Show screen commands and help */
    y = 2;
    x = 42;
    c_put_str(TERM_WHITE, "Commands:", y, x);
    c_put_str(TERM_WHITE, "ESC: Return", y + 2, x);
    c_put_str(TERM_WHITE, "Arrows: Move to color", y + 3, x);
    c_put_str(TERM_WHITE, "k,K: Incr,Decr extra value", y + 4, x);
    c_put_str(TERM_WHITE, "r,R: Incr,Decr red value", y + 5, x);
    c_put_str(TERM_WHITE, "g,G: Incr,Decr green value", y + 6, x);
    c_put_str(TERM_WHITE, "b,B: Incr,Decr blue value", y + 7, x);
    c_put_str(TERM_WHITE, "c: Copy from color", y + 8, x);
    c_put_str(TERM_WHITE, "v: Set specific values", y + 9, x);
    c_put_str(TERM_WHITE, "First column: base colors", y + 11, x);
    c_put_str(TERM_WHITE, "Second column: first shade, etc.", y + 12, x);

    c_put_str(
        TERM_WHITE, "Shades look like base colors in 16 color ports.", 23, 0);

    /* Hack - We want to show the fake cursor */
    do_move = true;
    do_update = true;

    /* Start with the first color */
    idx = 0;

    /* Used to erase the old position of the fake cursor */
    old_idx = -1;

    while (1)
    {
        /* Movement request */
        if (do_move)
        {
            /* Erase the old fake cursor */
            if (old_idx >= 0)
            {
                /* Get coordinates */
                x = COLOR_X(old_idx);
                y = COLOR_Y(old_idx);

                /* Draw spaces */
                c_put_str(TERM_WHITE, " ", y, x);
                c_put_str(TERM_WHITE, " ", y, x + 4);
            }

            /* Show the current fake cursor */
            /* Get coordinates */
            x = COLOR_X(idx);
            y = COLOR_Y(idx);

            /* Draw the cursor */
            c_put_str(TERM_WHITE, ">", y, x);
            c_put_str(TERM_WHITE, "<", y, x + 4);

            /* Format the name of the color */
            SDL_strlcpy(msg,
                format("Color = %d (0x%02X), Name = %s", idx, idx,
                    get_ext_color_name(idx)),
                sizeof(msg));

            /* Show the name and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 2, 0);
        }

        /* Color update request */
        if (do_update)
        {
            /* Get coordinates, adjust x */
            x = COLOR_X(idx) + 1;
            y = COLOR_Y(idx);

            /* Hack - Redraw the sample if needed */
            if (IS_BLACK(idx))
                c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
            else
                c_put_str(idx, COLOR_SAMPLE, y, x);

            /* Notify the changes in the color table to the terminal */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* The user is playing with white, redraw all */
            if (idx == TERM_WHITE)
                Term_redraw();

            /* Or reduce flickering by redrawing the changes only */
            else
                Term_redraw_section(x, y, x + 2, y);
        }

        /* Common code, show the values in the color table */
        if (do_move || do_update)
        {
            /* Format the view of the color values */
            SDL_strlcpy(msg,
                format("K = %d / R,G,B = %d, %d, %d",
                    angband_color_table[idx][0], angband_color_table[idx][1],
                    angband_color_table[idx][2], angband_color_table[idx][3]),
                sizeof(msg));

            /* Show color values and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 4, 0);
        }

        /* Reset flags */
        do_move = false;
        do_update = false;
        old_idx = -1;

        /* Get a command */
        if (!get_com("Command: Modify colors ", &ch))
            break;

        switch (ch)
        {
        /* Down */
        case '2':
        {
            /* Check bounds */
            if (idx + 1 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            ++idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Up */
        case '8':
        {
            /* Check bounds */
            if (idx - 1 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            --idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Left */
        case '4':
        {
            /* Check bounds */
            if (idx - 16 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx -= 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Right */
        case '6':
        {
            /* Check bounds */
            if (idx + 16 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx += 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Copy from color */
        case 'c':
        {
            char str[10];
            int src;

            /* Get the default value, the base color */
            sprintf(str, "%d", GET_BASE_COLOR(idx));

            /* Query, check for ESCAPE */
            if (!term_get_string(format("Copy from color (0-%d, def. base) ",
                                     MAX_COLORS - 1),
                    str, sizeof(str)))
                break;

            /* Convert to number */
            src = atoi(str);

            /* Check bounds */
            if (src < 0)
                src = 0;
            if (src >= MAX_COLORS)
                src = MAX_COLORS - 1;

            /* Do nothing if the colors are the same */
            if (src == idx)
                break;

            /* Modify the color table */
            angband_color_table[idx][0] = angband_color_table[src][0];
            angband_color_table[idx][1] = angband_color_table[src][1];
            angband_color_table[idx][2] = angband_color_table[src][2];
            angband_color_table[idx][3] = angband_color_table[src][3];

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the extra value */
        case 'k':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the extra value */
        case 'K':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the red value */
        case 'r':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the red value */
        case 'R':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the green value */
        case 'g':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Decrease the green value */
        case 'G':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the blue value */
        case 'b':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the blue value */
        case 'B':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Ask for specific values */
        case 'v':
        {
            do_update = askfor_color_values(idx);
            break;
        }
        }
    }
}

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int ch;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening colors menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Colors", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_COLORS
        prt("(2) Dump colors", 5, 5);
        prt("(3) Modify colors", 6, 5);
#endif /* ALLOW_COLORS */

        /* Prompt */
        prt("Command: ", 8, 0);

        /* Prompt */
        ch = settings_wait_key();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(8);

            /* Could skip the following if loading cancelled XXX XXX XXX */

            /* Mega-Hack -- React to color changes */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* Mega-Hack -- Redraw physical windows */
            Term_redraw();
        }

#ifdef ALLOW_COLORS

        /* Dump colors */
        else if (ch == '2')
        {
            static cptr mark = "Colors";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump colors", 8, 0);

            /* Prompt */
            prt("File: ", 10, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_colors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old colors */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Color redefinitions\n\n");

            /* Dump colors */
            for (i = 0; i < 256; i++)
            {
                int kv = angband_color_table[i][0];
                int rv = angband_color_table[i][1];
                int gv = angband_color_table[i][2];
                int bv = angband_color_table[i][3];

                cptr name = "unknown";

                /* Skip non-entries */
                if (!kv && !rv && !gv && !bv)
                    continue;

                /* Extract the color name */
                if (i < 16)
                    name = color_names[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# Color '%s'\n", name);

                /* Dump the monster attr/char info */
                SDL_IOprintf(fff, "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n", i, kv, rv,
                    gv, bv);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped color redefinitions.");
        }

        /* Edit colors */
        else if (ch == '3')
        {
            modify_colors();
        }

#endif /* ALLOW_COLORS */

        /* Unknown option */
        else
        {
            bell("Illegal command for colors!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

