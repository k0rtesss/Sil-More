/* File: ui-look-sidebar.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "ui-look-sidebar.h"
#include "ui-status.h"
#include <ctype.h>

static int unified_sidebar_object_group(const object_type* o_ptr)
{
    if (!o_ptr)
        return LOOK_GROUP_OTHER;

    if (artefact_p(o_ptr))
        return LOOK_GROUP_ARTIFACT;

    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOW:
    case TV_DIGGING:
    case TV_ARROW:
        return LOOK_GROUP_WEAPON;

    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return LOOK_GROUP_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
    case TV_STAFF:
        return LOOK_GROUP_JEWELRY;

    case TV_EASTER:
        return LOOK_GROUP_HERBS;

    case TV_POTION:
        return LOOK_GROUP_POTIONS;

    case TV_GEM:
        return LOOK_GROUP_GEMS;

    case TV_FOOD:
        if (o_ptr->sval < SV_FOOD_MIN_FOOD)
            return LOOK_GROUP_CONSUMABLE;
        break;
    }

    return LOOK_GROUP_OTHER;
}

typedef struct unified_sidebar_sorted_object {
    int o_idx;
    int y, x;
    object_type* o_ptr;
    bool is_artifact;
    int difficulty;
    int level;
    int group;
    int distance;
    int original_index;
} unified_sidebar_sorted_object;

static bool unified_sidebar_object_should_swap(
    const unified_sidebar_sorted_object* a,
    const unified_sidebar_sorted_object* b)
{
    bool sort_by_difficulty_only = look_objects_sort_by_difficulty ? true : false;
    bool a_known = object_known_p(a->o_ptr) ? true : false;
    bool b_known = object_known_p(b->o_ptr) ? true : false;

    if (!sort_by_difficulty_only && a->group != b->group)
        return (b->group < a->group);

    /* Unidentified items stay at the top of the section/list. */
    if (a_known != b_known)
        return (!b_known && a_known);

    if (!a_known)
    {
        if (b->distance < a->distance)
            return true;
        if ((b->distance == a->distance) && (b->original_index < a->original_index))
            return true;
        return false;
    }

    if (b->difficulty > a->difficulty)
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance < a->distance))
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance == a->distance)
        && (b->original_index < a->original_index))
        return true;

    return false;
}

static bool unified_look_sidebar_in_radius(const unified_look_state* state, int y,
    int x)
{
    if (!state || !state->nearby_filter)
        return true;

    return distance(p_ptr->py, p_ptr->px, y, x) <= UNIFIED_LOOK_NEAR_RADIUS;
}

static int unified_sidebar_collect_sorted_objects(const unified_look_state* state,
    unified_sidebar_sorted_object objects[], int max_objects)
{
    int i;
    int valid_objects = 0;

    if (!state || !objects || (max_objects <= 0))
        return 0;

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; (i < temp_n) && (valid_objects < max_objects); i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr;
        unified_sidebar_sorted_object* entry;

        if (!o_idx)
            continue;

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;
        if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
            continue;

        o_ptr = &o_list[o_idx];

        /* Only show marked (memorized) objects that the player has actually seen. */
        if (!o_ptr->marked)
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        entry = &objects[valid_objects];
        entry->o_idx = o_idx;
        entry->y = temp_y[i];
        entry->x = temp_x[i];
        entry->o_ptr = o_ptr;
        entry->is_artifact = artefact_p(o_ptr) ? true : false;
        entry->difficulty = object_difficulty(o_ptr);
        entry->level = k_info[o_ptr->k_idx].level;
        entry->group = unified_sidebar_object_group(o_ptr);
        if ((state->object_group_filter >= 0)
            && (entry->group != state->object_group_filter))
            continue;
        entry->distance = distance(p_ptr->py, p_ptr->px, entry->y, entry->x);
        entry->original_index = i;

        valid_objects++;
    }

    for (i = 0; i < valid_objects - 1; i++) {
        for (int j = i + 1; j < valid_objects; j++) {
            if (unified_sidebar_object_should_swap(&objects[i], &objects[j]))
            {
                unified_sidebar_sorted_object temp = objects[i];
                objects[i] = objects[j];
                objects[j] = temp;
            }
        }
    }

    return valid_objects;
}

int unified_look_find_cursor_selection(const unified_look_state* state, int cursor_y,
    int cursor_x)
{
    int i;
    int entity_index = 0;

    if (!state)
        return -1;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx)
                continue;
            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;
            if (!mon_list[m_idx].ml)
                continue;
            if (!unified_look_sidebar_in_radius(state, temp_y[i], temp_x[i]))
                continue;

            if ((temp_y[i] == cursor_y) && (temp_x[i] == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    if (state->show_objects)
    {
        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        for (i = 0; i < valid_objects; i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];

            if (state->limit_objects_top_five
                && (group_display_counts[entry->group] >= 5))
                continue;

            group_display_counts[entry->group]++;

            if ((entry->y == cursor_y) && (entry->x == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    return -1;
}

void redraw_inven_equip_subwindows(void)
{
    ui_status_refresh_window_mask(PW_INVEN | PW_EQUIP);
}

void redraw_monster_subwindows(void)
{
    ui_status_refresh_window_mask(PW_MONSTER);
}

static void sidebar_trim_spaces(char* s)
{
    if (!s) return;

    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static int sidebar_find_stats_pos(const char* s)
{
    if (!s) return -1;
    
    /* Stats section typically appears after the item name, preceded by a space.
     * Format: "Item Name (dice) [bonus] <pval> {inscription}"
     * We search for the first space-delimited bracket that looks like stats.
     */
    
    int first_stats_pos = -1;
    
    /* Look for the first bracket that follows a space or starts the string */
    for (int i = 0; s[i]; ++i)
    {
        char c = s[i];
        
        /* Found a potential stats delimiter */
        if (c == '(' || c == '[' || c == '<' || c == '{')
        {
            /* Check if this is preceded by a space (or is at start) */
            if (i == 0 || s[i-1] == ' ')
            {
                /* This looks like the start of stats section */
                first_stats_pos = i;
                break;
            }
            /* If preceded by a letter/digit, it might be part of the name */
            /* Keep searching */
        }
    }
    
    /* If we found stats position at start (i==0), that means NO base name!
     * This shouldn't happen with properly formatted object_desc output.
     * If it does, we should treat the whole thing as base name, not stats.
     */
    if (first_stats_pos == 0)
    {
        log_trace("sidebar_find_stats_pos: stats at position 0 for '%s' - treating as name", s);
        return -1;
    }
    
    return first_stats_pos;
}

static void sidebar_compact_name(const char* src, int max_len, char* dest, size_t dest_sz)
{
    if (!dest_sz) return;
    dest[0] = 0;

    if (!src) return;

    int src_len = (int)strlen(src);
    if (max_len < 1)
    {
        log_trace("sidebar_compact_name: max_len < 1 for src='%s'", src);
        return;
    }

    if (src_len <= max_len)
    {
        strnfmt(dest, dest_sz, "%s", src);
        log_trace("sidebar_compact_name: no shortening needed src='%s' len=%d max=%d", src, src_len, max_len);
        return;
    }

    int stats_pos = sidebar_find_stats_pos(src);
    log_trace("sidebar_compact_name: shortening src='%s' len=%d max=%d stats_pos=%d", src, src_len, max_len, stats_pos);

    if (stats_pos < 0)
    {
        strnfmt(dest, dest_sz, "%.*s", max_len, src);
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: no stats segment, result='%s'", dest);
        return;
    }

    int stats_len = src_len - stats_pos;
    
    /* If stats are very long and would fill the whole space,
     * prioritize showing at least SOME of the base name rather than stats-only.
     */
    if (stats_len >= max_len)
    {
        /* Try to show at least a portion of the base name, even if truncated */
        int base_space = max_len / 2; /* Give half space to name */
        if (base_space < 3) base_space = 3; /* Minimum name chars */
        if (base_space > stats_pos) base_space = stats_pos; /* Don't exceed available name */
        
        int stats_space = max_len - base_space;
        if (stats_space < 3) stats_space = 3; /* Minimum stats chars */
        
        /* Extract truncated base name */
        char base_truncated[64];
        strnfmt(base_truncated, sizeof(base_truncated), "%.*s", base_space, src);
        sidebar_trim_spaces(base_truncated);
        
        /* Extract beginning of stats */
        char stats_truncated[64];
        strnfmt(stats_truncated, sizeof(stats_truncated), "%.*s", stats_space, src + stats_pos);
        
        /* Combine them */
        if (base_truncated[0])
        {
            strnfmt(dest, dest_sz, "%s %s", base_truncated, stats_truncated);
        }
        else
        {
            strnfmt(dest, dest_sz, "%s", stats_truncated);
        }
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: long stats, showing truncated name+stats result='%s'", dest);
        return;
    }

    int base_space = max_len - stats_len;
    if (base_space < 0) base_space = 0;

    char base_full[128];
    char base_compact[128];
    base_full[0] = 0;
    base_compact[0] = 0;

    if (stats_pos > 0)
    {
        strnfmt(base_full, sizeof(base_full), "%.*s", stats_pos, src);
        sidebar_trim_spaces(base_full);
    }

    if (base_space > 0 && base_full[0])
    {
        int base_full_len = (int)strlen(base_full);
        if (base_full_len <= base_space)
        {
            SDL_strlcpy(base_compact, base_full, sizeof(base_compact));
        }
        else
        {
            const char* word_start[16];
            int word_len[16];
            int word_count = 0;
            const char* p = base_full;

            while (*p && word_count < 16)
            {
                while (*p && isspace((unsigned char)*p))
                    ++p;
                if (!*p)
                    break;

                word_start[word_count] = p;
                const char* q = p;
                while (*q && !isspace((unsigned char)*q))
                    ++q;
                word_len[word_count] = (int)(q - p);
                ++word_count;
                p = q;
            }

            int remaining = base_space;
            bool first_word = true;

            for (int i = 0; i < word_count && remaining > 0; ++i)
            {
                int needed_space = first_word ? 0 : 1;
                if (remaining <= needed_space)
                    break;

                if (!first_word)
                {
                    SDL_strlcat(base_compact, " ", sizeof(base_compact));
                    --remaining;
                }

                int take = word_len[i];
                if (take > remaining)
                {
                    if (first_word)
                    {
                        take = remaining;
                        if (take > 0)
                        {
                            char temp[64];
                            strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                            SDL_strlcat(base_compact, temp, sizeof(base_compact));
                            remaining -= take;
                        }
                    }
                    else if (remaining > 1)
                    {
                        char temp[64];
                        int partial = remaining;
                        strnfmt(temp, sizeof(temp), "%.*s", partial, word_start[i]);
                        SDL_strlcat(base_compact, temp, sizeof(base_compact));
                        remaining = 0;
                    }
                    else
                    {
                        size_t len = strlen(base_compact);
                        if (len && base_compact[len - 1] == ' ')
                            base_compact[len - 1] = '\0';
                        break;
                    }
                }
                else
                {
                    char temp[64];
                    strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                    SDL_strlcat(base_compact, temp, sizeof(base_compact));
                    remaining -= take;
                }

                first_word = false;
            }

            sidebar_trim_spaces(base_compact);

            if (!base_compact[0] && base_space > 0)
            {
                int take = (base_space < base_full_len) ? base_space : base_full_len;
                strnfmt(base_compact, sizeof(base_compact), "%.*s", take, base_full);
                sidebar_trim_spaces(base_compact);
            }
        }
    }

    dest[0] = 0;
    if (base_compact[0])
    {
        SDL_strlcpy(dest, base_compact, dest_sz);
        size_t len = strlen(dest);
        if (len && dest[len - 1] != ' ')
            SDL_strlcat(dest, " ", dest_sz);
    }

    SDL_strlcat(dest, src + stats_pos, dest_sz);
    sidebar_trim_spaces(dest);
    log_trace("sidebar_compact_name: combined result='%s'", dest);
}

static cptr unified_look_object_filter_tag(const unified_look_state* state)
{
    int filter = state ? state->object_group_filter : -1;

    switch (filter)
    {
    case LOOK_GROUP_ARTIFACT:
        return "ART";
    case LOOK_GROUP_WEAPON:
        return "WEAP";
    case LOOK_GROUP_ARMOUR:
        return "ARM";
    case LOOK_GROUP_JEWELRY:
        return "JEWL";
    case LOOK_GROUP_HERBS:
        return "HERB";
    case LOOK_GROUP_POTIONS:
        return "POT";
    case LOOK_GROUP_GEMS:
        return "GEM";
    case LOOK_GROUP_CONSUMABLE:
        return "CONS";
    case LOOK_GROUP_OTHER:
        return "OTHER";
    default:
        return "ALL";
    }
}

#define UNIFIED_LOOK_MONSTER_NAME_CHARS 30
#define UNIFIED_LOOK_OBJECT_NAME_CHARS 44
#define UNIFIED_LOOK_OVERLAY_MIN_WIDTH_PX 360
#define UNIFIED_LOOK_OVERLAY_WIDTH_CAP_PX 820

static void unified_sidebar_pad_bigtile_pair(char* primary,
    size_t primary_size, char* secondary, size_t secondary_size)
{
    int primary_len;
    int secondary_len;
    int total_span;
    int pad_needed;

    if (!use_bigtile || !primary || !secondary)
        return;

    primary_len = (int)strlen(primary);
    secondary_len = (int)strlen(secondary);
    total_span = primary_len + secondary_len;

    if (total_span < 13)
    {
        pad_needed = 13 - total_span;

        while (pad_needed > 0 && primary_len + 1 < (int)primary_size)
        {
            primary[primary_len++] = ' ';
            pad_needed--;
        }
        primary[primary_len] = '\0';
        total_span = primary_len + secondary_len;

        while (pad_needed > 0 && secondary_len + 1 < (int)secondary_size)
        {
            secondary[secondary_len++] = ' ';
            pad_needed--;
        }
        secondary[secondary_len] = '\0';
        total_span = primary_len + secondary_len;
    }

    if ((total_span % 2) == 0)
    {
        if (secondary_len + 1 < (int)secondary_size)
        {
            secondary[secondary_len++] = ' ';
            secondary[secondary_len] = '\0';
        }
        else if (primary_len + 1 < (int)primary_size)
        {
            primary[primary_len++] = ' ';
            primary[primary_len] = '\0';
        }
    }
}

static void unified_sidebar_pad_bigtile_single(char* text, size_t text_size)
{
    int text_len;
    int pad_needed;

    if (!use_bigtile || !text)
        return;

    text_len = (int)strlen(text);
    if (text_len < 13)
    {
        pad_needed = 13 - text_len;
        while (pad_needed > 0 && text_len + 1 < (int)text_size)
        {
            text[text_len++] = ' ';
            pad_needed--;
        }
        text[text_len] = '\0';
    }

    text_len = (int)strlen(text);
    if ((text_len % 2) == 0 && text_len + 1 < (int)text_size)
    {
        text[text_len++] = ' ';
        text[text_len] = '\0';
    }
}

static void unified_sidebar_format_monster_row(const monster_type* m_ptr,
    const monster_race* r_ptr, int max_name_chars, char* display_name,
    size_t display_name_size, char* morale_display,
    size_t morale_display_size, byte* morale_attr, int* display_name_len)
{
    char monster_name[80];
    char truncated_name[80];
    char hp_bar[10];
    char hp_display[12];
    int hp_len = 0;
    int available_width;
    int max_name_len;
    int morale_num = 0;
    byte meta_attr = TERM_WHITE;

    if (!m_ptr || !r_ptr || !display_name || !morale_display)
        return;

    monster_desc_race(monster_name, sizeof(monster_name), m_ptr->r_idx);
    if (m_ptr->maxhp > 0)
        hp_len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;

    if (m_ptr->confused && m_ptr->stunned)
        strncpy(hp_bar, "cscscscs", hp_len);
    else if (m_ptr->confused)
        strncpy(hp_bar, "cccccccc", hp_len);
    else if (m_ptr->stunned)
        strncpy(hp_bar, "ssssssss", hp_len);
    else
        strncpy(hp_bar, "********", hp_len);
    hp_bar[hp_len] = '\0';

    if (m_ptr->alertness < ALERTNESS_UNWARY)
    {
        meta_attr = TERM_BLUE;
        morale_num = m_ptr->alertness;
    }
    else if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        meta_attr = TERM_L_BLUE;
        morale_num = m_ptr->alertness;
    }
    else if (m_ptr->morale >= 0)
    {
        morale_num = (m_ptr->morale + 9) / 10;
    }
    else
    {
        morale_num = m_ptr->morale / 10;
    }

    hp_display[0] = '\0';
    if (hp_bar[0])
        strnfmt(hp_display, sizeof(hp_display), " %s", hp_bar);

    strnfmt(morale_display, morale_display_size, " %d", morale_num);

    available_width = max_name_chars;
    if (available_width < 8)
        available_width = 8;

    max_name_len = available_width - (int)strlen(hp_display);
    if (max_name_len < 4)
        max_name_len = 4;
    if (max_name_len > (int)sizeof(truncated_name) - 1)
        max_name_len = (int)sizeof(truncated_name) - 1;

    memset(truncated_name, 0, sizeof(truncated_name));
    SDL_strlcpy(truncated_name, monster_name, sizeof(truncated_name));
    if ((int)strlen(truncated_name) > max_name_len)
        truncated_name[max_name_len] = '\0';

    strnfmt(display_name, display_name_size, "%s%s", truncated_name,
        hp_display);
    unified_sidebar_pad_bigtile_pair(display_name, display_name_size,
        morale_display, morale_display_size);

    if (morale_attr)
        *morale_attr = meta_attr;
    if (display_name_len)
        *display_name_len = (int)strlen(display_name);
}

static void unified_sidebar_format_object_row(
    const unified_sidebar_sorted_object* entry, int max_name_chars,
    char* display_name, size_t display_name_size, byte* base_color,
    int* display_name_len)
{
    object_type* o_ptr;
    char object_name[60];
    char name_source[80];
    char weight_buf[16];
    char smith_buf[16];
    int available_name_width;
    int weight_len;
    int smith_len;
    int max_name_len;

    if (!entry || !display_name || display_name_size == 0)
        return;

    o_ptr = entry->o_ptr;
    if (!o_ptr)
        return;

    object_desc_floor(object_name, sizeof(object_name), o_ptr, false, 4);
    SDL_strlcpy(name_source, object_name, sizeof(name_source));
    if (entry->is_artifact && object_known_p(o_ptr))
    {
        size_t len = strlen(name_source);

        if (len + 1 < sizeof(name_source))
        {
            memmove(name_source + 1, name_source, len + 1);
            name_source[0] = '*';
        }
    }

    if (base_color)
    {
        *base_color = weapon_glows(o_ptr)
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
    }

    strnfmt(weight_buf, sizeof(weight_buf), " %d.%1d",
        (o_ptr->weight * o_ptr->number) / 10,
        (o_ptr->weight * o_ptr->number) % 10);
    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look]
        && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);

        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
    }

    available_name_width = max_name_chars;
    if (available_name_width < 10)
        available_name_width = 10;

    weight_len = (int)strlen(weight_buf);
    smith_len = (int)strlen(smith_buf);
    max_name_len = available_name_width - weight_len - smith_len - 1;
    if (max_name_len < 4)
        max_name_len = 4;
    if (max_name_len > (int)display_name_size - weight_len - 1)
        max_name_len = (int)display_name_size - weight_len - 1;

    sidebar_compact_name(name_source, max_name_len, display_name,
        display_name_size);
    SDL_strlcat(display_name, weight_buf, display_name_size);
    if (smith_buf[0])
        SDL_strlcat(display_name, smith_buf, display_name_size);
    unified_sidebar_pad_bigtile_single(display_name, display_name_size);

    if (display_name_len)
        *display_name_len = (int)strlen(display_name);
}

static bool unified_look_menu_add_section(app_ui_panel* panel, byte attr,
    cptr label)
{
    u16b row_index;

    if (!panel || !label || !label[0] || panel->row_count >= APP_UI_ROW_MAX)
        return false;

    row_index = panel->row_count;
    if (!app_ui_panel_add_row(panel, -1, attr, true, false, "", label, ""))
        return false;

    panel->rows[row_index].flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

static void unified_look_clear_highlight_state(unified_look_state* state)
{
    if (!state)
        return;

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
        highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
            false);

    state->highlighted_y = -1;
    state->highlighted_x = -1;
    state->highlighted_entity_type = 0;
}

static void unified_look_select_highlight(unified_look_state* state, int y,
    int x, int entity_type)
{
    if (!state)
        return;

    state->highlighted_y = y;
    state->highlighted_x = x;
    state->highlighted_entity_type = entity_type;
    state->cursor_y = y;
    state->cursor_x = x;
    highlight_entity_on_map_type(y, x, true, entity_type);
}

bool unified_look_build_menu_scene(unified_look_state* state, cptr title,
    app_ui_scene* scene)
{
    app_ui_panel* panel;
    bool has_sidebar_selection;
    bool selected_row_found = false;
    int monster_count = 0;
    int object_count = 0;
    int object_start = 0;

    if (!state || !scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_TRANSIENT);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_OVERLAY_RAIL;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, UNIFIED_LOOK_OVERLAY_MIN_WIDTH_PX,
        UNIFIED_LOOK_OVERLAY_WIDTH_CAP_PX);
    (void)title;

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));
    unified_look_clear_highlight_state(state);

    if (state->show_monsters && panel->row_count < APP_UI_ROW_MAX)
    {
        int i;

        (void)unified_look_menu_add_section(panel, TERM_WHITE, "MONSTERS:");
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n && panel->row_count < APP_UI_ROW_MAX; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
            monster_type* m_ptr;
            monster_race* r_ptr;
            byte label_attr;
            byte meta_attr = TERM_WHITE;
            char label[APP_UI_LABEL_MAX];
            char meta[APP_UI_META_MAX];
            bool selected;

            if (!m_idx)
                continue;
            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;

            m_ptr = &mon_list[m_idx];
            r_ptr = &r_info[m_ptr->r_idx];
            if (!m_ptr->ml)
                continue;

            selected = has_sidebar_selection
                && (state->selected_entity == monster_count);
            unified_sidebar_format_monster_row(m_ptr, r_ptr,
                UNIFIED_LOOK_MONSTER_NAME_CHARS, label, sizeof(label), meta,
                sizeof(meta),
                &meta_attr, NULL);
            label_attr = selected ? TERM_L_BLUE : TERM_WHITE;
            if (!app_ui_panel_add_row_ex(panel, monster_count, label_attr,
                    selected ? TERM_L_BLUE : meta_attr, monster_attr(r_ptr),
                    monster_char(r_ptr), true, selected, "", label, meta))
            {
                break;
            }

            if (selected)
            {
                selected_row_found = true;
                unified_look_select_highlight(state, temp_y[i], temp_x[i], 1);
            }

            monster_count++;
        }
    }

    object_start = monster_count;
    if (state->show_objects && panel->row_count < APP_UI_ROW_MAX)
    {
        int i;
        int group_display_counts[LOOK_GROUP_COUNT] = { 0 };
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects;
        char header_buf[32];

        strnfmt(header_buf, sizeof(header_buf), "OBJECTS: %s",
            unified_look_object_filter_tag(state));
        (void)unified_look_menu_add_section(panel, TERM_WHITE, header_buf);

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        for (i = 0; i < valid_objects && panel->row_count < APP_UI_ROW_MAX;
             i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];
            object_type* o_ptr = entry->o_ptr;
            char label[APP_UI_LABEL_MAX];
            byte row_attr = TERM_WHITE;
            bool selected;

            if (state->limit_objects_top_five
                && group_display_counts[entry->group] >= 5)
            {
                continue;
            }

            group_display_counts[entry->group]++;
            selected = has_sidebar_selection
                && (state->selected_entity == (object_start + object_count));
            unified_sidebar_format_object_row(entry,
                UNIFIED_LOOK_OBJECT_NAME_CHARS, label, sizeof(label),
                &row_attr, NULL);
            if (!app_ui_panel_add_row_ex(panel,
                    object_start + object_count,
                    selected ? TERM_L_BLUE : row_attr,
                    selected ? TERM_L_BLUE : row_attr,
                    object_attr(o_ptr), object_char(o_ptr), true, selected,
                    "", label, ""))
            {
                break;
            }

            if (selected)
            {
                selected_row_found = true;
                unified_look_select_highlight(state, entry->y, entry->x, 2);
            }

            object_count++;
        }
    }

    if (has_sidebar_selection && !selected_row_found)
        unified_look_clear_highlight_state(state);

    return panel->row_count > 0;
}
