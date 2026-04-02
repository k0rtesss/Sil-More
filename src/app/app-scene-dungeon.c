#include "angband.h"

#include "app-scene-dungeon.h"
#include "app-session.h"
#include "externs.h"
#include "melee/melee-combat-display.h"
#include "ui/ui-status.h"

typedef struct app_hidden_overlay_line_local {
    char text[APP_DUNGEON_PANE_TEXT_MAX];
    byte attr;
} app_hidden_overlay_line_local;

typedef struct app_dungeon_overlay_layout_local {
    int row_name;
    int row_stat;
    int row_exp;
    int row_hp;
    int row_sp;
    int row_light;
    int row_mel;
    int row_arc;
    int row_quiver;
    int row_evn;
    int row_info;
    int row_cut;
    int row_song;
    int col_hungry;
    int col_blind;
    int col_confused;
    int col_stun;
    int col_afraid;
    int col_state;
    int col_speed;
    int col_terrain;
    int col_depth;
} app_dungeon_overlay_layout_local;

static bool app_dungeon_buffer_reserve(byte** data, size_t* capacity,
    size_t required)
{
    byte* next;

    if (!data || !capacity)
        return false;

    if (required <= *capacity)
        return true;

    next = realloc(*data, required);
    if (!next)
        return false;

    *data = next;
    *capacity = required;
    return true;
}

static void app_interaction_snapshot_clear(app_interaction_state* interaction)
{
    app_interaction_clear(interaction);
}

static void app_dungeon_overlay_layout_init(
    app_dungeon_overlay_layout_local* layout, bool compact_height)
{
    if (!layout)
        return;

    memset(layout, 0, sizeof(*layout));
    layout->row_name = 1;
    layout->row_stat = 3;
    layout->row_exp = compact_height ? 7 : 8;
    layout->row_hp = compact_height ? 8 : 9;
    layout->row_sp = compact_height ? 9 : 10;
    layout->row_light = compact_height ? 10 : 11;
    layout->row_mel = compact_height ? 11 : 13;
    layout->row_arc = compact_height ? 12 : 14;
    layout->row_quiver = compact_height ? 13 : 15;
    layout->row_evn = compact_height ? 14 : 16;
    layout->row_info = compact_height ? 15 : 17;
    layout->row_cut = compact_height ? 17 : 20;
    layout->row_song = compact_height ? 18 : 21;
    layout->col_hungry = 0;
    layout->col_blind = 9;
    layout->col_confused = 15;
    layout->col_stun = 24;
    layout->col_afraid = 36;
    layout->col_state = 43;
    layout->col_speed = 56;
    layout->col_terrain = 61;
    layout->col_depth = 72;
}

static void app_dungeon_overlay_snapshot_clear(
    app_dungeon_overlay_snapshot* overlay)
{
    if (!overlay)
        return;

    memset(overlay, 0, sizeof(*overlay));
    overlay->format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    app_interaction_clear(&overlay->interaction);
    app_menu_scene_init(&overlay->transient_menu);
    app_ui_scene_init(&overlay->chrome_scene);
}

static void app_dungeon_ui_line_set(char* out_line, size_t out_line_size,
    cptr text)
{
    if (!out_line || out_line_size == 0)
        return;

    SDL_strlcpy(out_line, text ? text : "", out_line_size);
}

static void app_dungeon_ui_line_write_at(char* out_line, size_t out_line_size,
    size_t col, const app_text_snapshot* text)
{
    size_t len;
    size_t text_len;

    if (!out_line || out_line_size == 0 || !text || !text->active
        || !text->text[0])
    {
        return;
    }
    if (out_line_size < 2)
        return;

    len = strlen(out_line);
    if (col >= out_line_size - 1)
        col = out_line_size - 2;
    if (len < col)
    {
        memset(out_line + len, ' ', col - len);
        out_line[col] = '\0';
        len = col;
    }
    else if (len > 0 && len < out_line_size - 1)
    {
        out_line[len++] = ' ';
        out_line[len] = '\0';
    }

    text_len = strlen(text->text);
    if (text_len >= out_line_size - len)
        text_len = out_line_size - len - 1;
    memcpy(out_line + len, text->text, text_len);
    out_line[len + text_len] = '\0';
}

static bool app_dungeon_ui_append_body_or_blank(app_ui_panel* panel,
    byte attr, cptr text)
{
    if (!panel)
        return false;

    if (text && text[0])
        return app_ui_panel_add_body_line(panel, attr, text);

    return app_ui_panel_add_body_line(panel, attr, " ");
}

static bool app_dungeon_ui_append_status_row_or_blank(app_ui_panel* panel,
    byte attr, cptr text)
{
    s16b row_id;

    if (!panel)
        return false;

    row_id = (s16b)panel->row_count;
    if (!app_ui_panel_add_row(panel, row_id, attr, true, false, NULL,
            (text && text[0]) ? text : " ", NULL))
    {
        return false;
    }

    panel->rows[panel->row_count - 1].flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

static app_ui_panel* app_dungeon_ui_append_chrome_panel(app_ui_scene* scene,
    u16b style, u16b flags)
{
    app_ui_panel* panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_CHROME);

    if (!panel)
        return NULL;

    panel->style = style;
    panel->flags |= flags;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    return panel;
}

static void app_text_snapshot_clear(app_text_snapshot* text)
{
    if (!text)
        return;

    memset(text, 0, sizeof(*text));
}

static void app_text_snapshot_set(app_text_snapshot* text, byte attr,
    cptr value)
{
    app_text_snapshot_clear(text);
    if (!text || !value || !value[0])
        return;

    text->attr = attr;
    text->active = 1;
    SDL_strlcpy(text->text, value, sizeof(text->text));
}

static bool app_dungeon_compact_width(void)
{
    return (Term && (Term->wid < 80));
}

static bool app_dungeon_compact_height(void)
{
    return SIL_UI_COMPACT_HEIGHT ? true : false;
}

static byte app_status_depth_attr(void)
{
    byte attr = TERM_WHITE;

    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4 || feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6 || feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    return attr;
}

static bool app_status_state_text(char* out_long, size_t out_long_sz,
    char* out_short, size_t out_short_sz, byte* out_attr)
{
    if (!p_ptr)
        return false;

    if (out_long && out_long_sz)
        out_long[0] = '\0';
    if (out_short && out_short_sz)
        out_short[0] = '\0';
    if (out_attr)
        *out_attr = TERM_WHITE;

    if (p_ptr->entranced)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, "Entranced", out_long_sz);
        SDL_strlcpy(out_short, "En", out_short_sz);
        return true;
    }

    if (p_ptr->smithing)
    {
        SDL_strlcpy(out_long, "Smithing", out_long_sz);
        SDL_strlcpy(out_short, "Sm", out_short_sz);
        return true;
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(out_long, "Fletching", out_long_sz);
        SDL_strlcpy(out_short, "Fl", out_short_sz);
        return true;
    }

    if (p_ptr->rage)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, "Rage", out_long_sz);
        SDL_strlcpy(out_short, "Rg", out_short_sz);
        return true;
    }

    if (p_ptr->resting)
    {
        int n = p_ptr->resting;

        if (n == -1)
        {
            SDL_strlcpy(out_long, "Rest*", out_long_sz);
            SDL_strlcpy(out_short, "R*", out_short_sz);
        }
        else if (n == -2)
        {
            SDL_strlcpy(out_long, "Rest&", out_long_sz);
            SDL_strlcpy(out_short, "R&", out_short_sz);
        }
        else if (n >= 1000)
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%dk", n / 1000);
        }
        else
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%d", n);
        }
        return true;
    }

    if (p_ptr->command_rep)
    {
        strnfmt(out_long, out_long_sz, "Repeat %d", p_ptr->command_rep);
        strnfmt(out_short, out_short_sz, "Rp%d", p_ptr->command_rep);
        return true;
    }

    if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(out_long, "Stealth", out_long_sz);
        SDL_strlcpy(out_short, "St", out_short_sz);
        return true;
    }

    return false;
}

static cptr app_partition_abbrev_for_point(int y, int x)
{
    switch (level_partition_kind_for_point(y, x))
    {
    case LEVEL_PART_ROOMY:
        return "Room";
    case LEVEL_PART_RUINED:
        return "Ruin";
    case LEVEL_PART_CAVEY:
        return "Cave";
    case LEVEL_PART_BIG_CAVE:
        return "BigCa";
    case LEVEL_PART_LABYRINTH:
        return "Labir";
    case LEVEL_PART_CHASM:
        return "Chasm";
    default:
        return "";
    }
}

static void app_hidden_overlay_add_line(app_hidden_overlay_line_local* lines,
    int* count, int max_lines, byte attr, cptr text)
{
    if (!lines || !count || !text || !text[0] || (*count >= max_lines))
        return;

    SDL_strlcpy(lines[*count].text, text, sizeof(lines[*count].text));
    lines[*count].attr = attr;
    (*count)++;
}

static int app_hidden_overlay_build_lines(
    app_hidden_overlay_line_local* lines, int max_lines)
{
    int count = 0;
    char buf[32];
    byte hp_color;
    byte voice_color;

    if (!lines || !p_ptr || max_lines <= 0)
        return 0;

    hp_color = health_attr(p_ptr->chp, p_ptr->mhp);
    if (p_ptr->csp >= p_ptr->msp)
        voice_color = TERM_L_GREEN;
    else if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
        voice_color = TERM_YELLOW;
    else
        voice_color = TERM_RED;

    strnfmt(buf, sizeof(buf), "HP %3d", MIN(p_ptr->chp, 999));
    app_hidden_overlay_add_line(lines, &count, max_lines, hp_color, buf);

    strnfmt(buf, sizeof(buf), "VC %3d", MIN(p_ptr->csp, 999));
    app_hidden_overlay_add_line(lines, &count, max_lines, voice_color, buf);

    if (p_ptr->cut > 100)
    {
        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_RED,
            "MW !!!");
    }
    else if (p_ptr->cut > 20)
    {
        strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_RED, buf);
    }
    else if (p_ptr->cut > 0)
    {
        strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_L_RED, buf);
    }

    if (p_ptr->poisoned > 20)
    {
        strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_L_GREEN,
            buf);
    }
    else if (p_ptr->poisoned > 0)
    {
        strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_GREEN, buf);
    }

    if ((p_ptr->song1 != SNG_NOTHING) || (p_ptr->song2 != SNG_NOTHING))
    {
        cptr song1_name = (p_ptr->song1 != SNG_NOTHING)
            ? (b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name)
            : NULL;
        cptr song2_name = (p_ptr->song2 != SNG_NOTHING)
            ? (b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name)
            : NULL;

        buf[0] = '\0';
        if (song1_name && song2_name)
            strnfmt(buf, sizeof(buf), "%s+%s", song1_name + 8, song2_name + 8);
        else if (song1_name)
            SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
        else if (song2_name)
            SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

        app_hidden_overlay_add_line(lines, &count, max_lines, TERM_L_BLUE,
            buf);
    }

    if (p_ptr->health_who
        && mon_list[p_ptr->health_who].ml
        && !p_ptr->image
        && (mon_list[p_ptr->health_who].hp > 0))
    {
        monster_type* m_ptr = &mon_list[p_ptr->health_who];
        int len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
        int i;

        if (len < 0)
            len = 0;
        if (len > 8)
            len = 8;

        for (i = 0; i < len; i++)
            buf[i] = '*';
        buf[len] = '\0';

        app_hidden_overlay_add_line(lines, &count, max_lines,
            health_attr(m_ptr->hp, m_ptr->maxhp), buf);
    }

    return count;
}

static int app_collect_combat_entries(app_combat_roll_snapshot* out_entries,
    int max_entries)
{
    int count = 0;
    int round;

    for (round = 0; round < 2 && count < max_entries; round++)
    {
        int combat_num_for_round = (round == 0) ? combat_number
            : combat_number_old;
        int idx;
        int player_indices[MAX_COMBAT_ROLLS];
        int monster_indices[MAX_COMBAT_ROLLS];
        int player_count = 0;
        int monster_count = 0;
        int i;

        if (combat_num_for_round <= 0)
            continue;

        for (idx = combat_num_for_round - 1; idx >= 0; idx--)
        {
            if (combat_rolls[round][idx].att_type == COMBAT_ROLL_NONE)
                continue;

            if (combat_rolls[round][idx].is_attacker_player)
                player_indices[player_count++] = idx;
            else
                monster_indices[monster_count++] = idx;
        }

        for (i = 0; (i < player_count) && (count < max_entries); i++)
        {
            combat_roll* roll = &combat_rolls[round][player_indices[i]];
            app_combat_roll_snapshot* entry = &out_entries[count++];

            memset(entry, 0, sizeof(*entry));
            entry->round = (s16b)round;
            entry->index = (s16b)player_indices[i];
            entry->att_type = (s16b)roll->att_type;
            entry->dam_type = (s16b)roll->dam_type;
            entry->attacker_char = roll->attacker_char;
            entry->attacker_attr = roll->attacker_attr;
            entry->defender_char = roll->defender_char;
            entry->defender_attr = roll->defender_attr;
            entry->is_attacker_player = roll->is_attacker_player ? 1 : 0;
            entry->is_defender_player = roll->is_defender_player ? 1 : 0;
            entry->att = (s16b)roll->att;
            entry->att_roll = (s16b)roll->att_roll;
            entry->evn = (s16b)roll->evn;
            entry->evn_roll = (s16b)roll->evn_roll;
            entry->dd = (s16b)roll->dd;
            entry->ds = (s16b)roll->ds;
            entry->dam = (s16b)roll->dam;
            entry->pd = (s16b)roll->pd;
            entry->ps = (s16b)roll->ps;
            entry->prot = (s16b)roll->prot;
            entry->prt_percent = (s16b)roll->prt_percent;
            entry->melee = roll->melee ? 1 : 0;
        }

        for (i = 0; (i < monster_count) && (count < max_entries); i++)
        {
            combat_roll* roll = &combat_rolls[round][monster_indices[i]];
            app_combat_roll_snapshot* entry = &out_entries[count++];

            memset(entry, 0, sizeof(*entry));
            entry->round = (s16b)round;
            entry->index = (s16b)monster_indices[i];
            entry->att_type = (s16b)roll->att_type;
            entry->dam_type = (s16b)roll->dam_type;
            entry->attacker_char = roll->attacker_char;
            entry->attacker_attr = roll->attacker_attr;
            entry->defender_char = roll->defender_char;
            entry->defender_attr = roll->defender_attr;
            entry->is_attacker_player = roll->is_attacker_player ? 1 : 0;
            entry->is_defender_player = roll->is_defender_player ? 1 : 0;
            entry->att = (s16b)roll->att;
            entry->att_roll = (s16b)roll->att_roll;
            entry->evn = (s16b)roll->evn;
            entry->evn_roll = (s16b)roll->evn_roll;
            entry->dd = (s16b)roll->dd;
            entry->ds = (s16b)roll->ds;
            entry->dam = (s16b)roll->dam;
            entry->pd = (s16b)roll->pd;
            entry->ps = (s16b)roll->ps;
            entry->prot = (s16b)roll->prot;
            entry->prt_percent = (s16b)roll->prt_percent;
            entry->melee = roll->melee ? 1 : 0;
        }
    }

    return count;
}

static bool app_build_map_blob(app_dungeon_snapshot* snapshot,
    const app_wait_state* wait_state)
{
    size_t cell_count;
    size_t required;
    app_map_snapshot* map;
    bool suppress_saved_target;
    int y;
    int x;
    size_t cell_index = 0;

    cell_count = (size_t)SCREEN_HGT * (size_t)SCREEN_WID;
    required = sizeof(*map)
        + (cell_count * sizeof(app_map_cell_snapshot));

    if (!app_dungeon_buffer_reserve(&snapshot->map_data,
            &snapshot->map_capacity, required))
    {
        return false;
    }

    map = (app_map_snapshot*)snapshot->map_data;
    memset(map, 0, required);

    map->format_version = APP_DUNGEON_MAP_FORMAT_VERSION;
    map->flags = (wait_state && (wait_state->reason != APP_WAIT_REASON_NONE))
        ? APP_DUNGEON_SNAPSHOT_FLAG_WAITING : 0;
    map->width = (u16b)SCREEN_WID;
    map->height = (u16b)SCREEN_HGT;
    map->panel_y = p_ptr->wy;
    map->panel_x = p_ptr->wx;
    map->player_y = p_ptr->py;
    map->player_x = p_ptr->px;
    map->cursor = snapshot->cursor_state;
    suppress_saved_target = (wait_state
        && wait_state->reason == APP_WAIT_REASON_TARGETING
        && snapshot->cursor_state.visible
        && snapshot->cursor_state.relative);
    map->target.active = (!suppress_saved_target && p_ptr->target_set) ? 1 : 0;
    map->target.who = p_ptr->target_who;
    map->target.map_y = p_ptr->target_row;
    map->target.map_x = p_ptr->target_col;
    map->cell_count = (u32b)cell_count;

    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            app_map_cell_snapshot* cell = &map->cells[cell_index++];
            byte attr = TERM_DARK;
            char ch = ' ';
            byte terrain_attr = TERM_DARK;
            char terrain_char = ' ';
            u16b flags = 0;

            map_info(y, x, &attr, &ch, &terrain_attr, &terrain_char);

            memset(cell, 0, sizeof(*cell));
            cell->map_y = (s16b)y;
            cell->map_x = (s16b)x;
            cell->feat = cave_feat[y][x];
            cell->light = cave_light[y][x];
            cell->m_idx = cave_m_idx[y][x];
            cell->o_idx = cave_o_idx[y][x];
            cell->cave_info = cave_info[y][x];
            cell->terrain_attr = terrain_attr;
            cell->terrain_char = terrain_char;
            cell->attr = attr;
            cell->ch = ch;

            if (cave_info[y][x] & CAVE_SEEN)
                flags |= APP_MAP_CELL_FLAG_SEEN;
            if (cave_info[y][x] & CAVE_MARK)
                flags |= APP_MAP_CELL_FLAG_MARK;
            if (cave_info[y][x] & CAVE_VIEW)
                flags |= APP_MAP_CELL_FLAG_VIEW;
            if (cave_info[y][x] & CAVE_WALL)
                flags |= APP_MAP_CELL_FLAG_WALL;
            if (cell->m_idx < 0)
                flags |= APP_MAP_CELL_FLAG_PLAYER;
            else if (cell->m_idx > 0)
                flags |= APP_MAP_CELL_FLAG_MONSTER;
            if (cell->o_idx > 0)
                flags |= APP_MAP_CELL_FLAG_OBJECT;
            if (!suppress_saved_target && p_ptr->target_set
                && (p_ptr->target_row == y)
                && (p_ptr->target_col == x))
            {
                flags |= APP_MAP_CELL_FLAG_TARGET;
            }
            if (snapshot->cursor_state.visible && snapshot->cursor_state.relative
                && (snapshot->cursor_state.map_y == y)
                && (snapshot->cursor_state.map_x == x))
            {
                flags |= APP_MAP_CELL_FLAG_CURSOR;
            }

            cell->flags = flags;
        }
    }

    snapshot->map_size = required;
    return true;
}

static void app_build_hunger_text(app_text_snapshot* out_text)
{
    if (p_ptr->food < PY_FOOD_STARVE)
        app_text_snapshot_set(out_text, TERM_RED, "Starving");
    else if (p_ptr->food < PY_FOOD_WEAK)
        app_text_snapshot_set(out_text, TERM_ORANGE, "Weak");
    else if (p_ptr->food < PY_FOOD_ALERT)
        app_text_snapshot_set(out_text, TERM_YELLOW, "Hungry");
    else if (p_ptr->food >= PY_FOOD_FULL)
        app_text_snapshot_set(out_text, TERM_L_GREEN, "Full");
}

static void app_build_condition_text(app_text_snapshot* out_text, bool active,
    byte attr, cptr text)
{
    if (active)
        app_text_snapshot_set(out_text, attr, text);
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_cut_text(app_text_snapshot* out_text)
{
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    if (p_ptr->cut > 100)
        app_text_snapshot_set(out_text, TERM_RED, "Mortal wound");
    else if (p_ptr->cut > 20)
    {
        strnfmt(buf, sizeof(buf), "Bleeding %d", p_ptr->cut);
        app_text_snapshot_set(out_text, TERM_RED, buf);
    }
    else if (p_ptr->cut > 0)
    {
        strnfmt(buf, sizeof(buf), "Bleeding %d", p_ptr->cut);
        app_text_snapshot_set(out_text, TERM_L_RED, buf);
    }
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_poison_text(app_text_snapshot* out_text)
{
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    if (p_ptr->poisoned > 20)
    {
        strnfmt(buf, sizeof(buf), "Poisoned %d", p_ptr->poisoned);
        app_text_snapshot_set(out_text, TERM_L_GREEN, buf);
    }
    else if (p_ptr->poisoned > 0)
    {
        strnfmt(buf, sizeof(buf), "Poisoned %d", p_ptr->poisoned);
        app_text_snapshot_set(out_text, TERM_GREEN, buf);
    }
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_stun_text(app_text_snapshot* out_text)
{
    if (p_ptr->stun > 100)
        app_text_snapshot_set(out_text, TERM_RED, "Knocked out");
    else if (p_ptr->stun > 50)
        app_text_snapshot_set(out_text, TERM_ORANGE, "Heavy stun");
    else if (p_ptr->stun > 0)
        app_text_snapshot_set(out_text, TERM_ORANGE, "Stun");
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_speed_text(app_text_snapshot* out_text)
{
    if (p_ptr->pspeed > 2)
        app_text_snapshot_set(out_text, TERM_L_GREEN, "Fast");
    else if (p_ptr->pspeed < 2)
        app_text_snapshot_set(out_text, TERM_ORANGE, "Slow");
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_terrain_text(app_text_snapshot* out_text)
{
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
        app_text_snapshot_set(out_text, TERM_ORANGE, "Pit");
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
        app_text_snapshot_set(out_text, TERM_ORANGE, "Web");
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
        app_text_snapshot_set(out_text, TERM_YELLOW, "Sun");
    else
        app_text_snapshot_clear(out_text);
}

static void app_build_depth_text(app_text_snapshot* out_text)
{
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    if (!p_ptr->depth)
        SDL_strlcpy(buf, "Surface", sizeof(buf));
    else
        strnfmt(buf, sizeof(buf), "%d ft", p_ptr->depth * 50);

    app_text_snapshot_set(out_text, app_status_depth_attr(), buf);
}

static void app_build_song_text(app_text_snapshot* out_text)
{
    cptr song1_name = (p_ptr->song1 != SNG_NOTHING)
        ? (b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name)
        : NULL;
    cptr song2_name = (p_ptr->song2 != SNG_NOTHING)
        ? (b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name)
        : NULL;
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    buf[0] = '\0';
    if (song1_name && song2_name)
        strnfmt(buf, sizeof(buf), "%s + %s", song1_name + 8, song2_name + 8);
    else if (song1_name)
        SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
    else if (song2_name)
        SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

    app_text_snapshot_set(out_text, TERM_L_BLUE, buf);
}

static void app_build_quiver_text(app_text_snapshot* out_text)
{
    object_type* q1_ptr = &inventory[INVEN_QUIVER1];
    object_type* q2_ptr = &inventory[INVEN_QUIVER2];
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    buf[0] = '\0';
    if (q1_ptr->k_idx)
    {
        strnfmt(buf, sizeof(buf), "Q1 %d/%d", q1_ptr->number,
            object_stack_limit(q1_ptr));
    }
    if (q2_ptr->k_idx)
    {
        size_t len = strlen(buf);
        strnfmt(buf + len, sizeof(buf) - len, "%sQ2 %d/%d",
            len ? " " : "", q2_ptr->number, object_stack_limit(q2_ptr));
    }

    app_text_snapshot_set(out_text, TERM_L_WHITE, buf);
}

static void app_build_light_text(app_text_snapshot* out_text)
{
    object_type* o_ptr = &inventory[INVEN_LITE];
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_L_WHITE;
    bool infinite = false;
    long fuel = 0;

    if (!o_ptr->k_idx)
    {
        app_text_snapshot_clear(out_text);
        return;
    }

    if (o_ptr->tval == TV_LIGHT)
    {
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_LANTERN:
        case SV_LIGHT_MALLORN:
            fuel = o_ptr->timeout;
            break;
        default:
            infinite = true;
            break;
        }
    }
    else
    {
        u32b f1;
        u32b f2;
        u32b f3;

        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & TR2_LIGHT)
            infinite = true;
    }

    if (infinite)
    {
        SDL_strlcpy(buf, "Light inf", sizeof(buf));
        attr = TERM_L_GREEN;
    }
    else
    {
        if (fuel < 0)
            fuel = 0;

        if (fuel == 0)
            attr = TERM_RED;
        else if (fuel <= 100)
            attr = TERM_ORANGE;

        strnfmt(buf, sizeof(buf), "Light %ld", fuel);
    }

    app_text_snapshot_set(out_text, attr, buf);
}

static void app_build_primary_stat_text(app_text_snapshot* out_text, int skill,
    int value_a, int value_b)
{
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", skill, value_a, value_b);
    app_text_snapshot_set(out_text, TERM_L_WHITE, buf);
}

static void app_build_armor_text(app_text_snapshot* out_text)
{
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];
    bool block = p_ptr->active_ability[S_EVN][EVN_BLOCKING];

    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = false;
    strnfmt(buf, sizeof(buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = block;

    app_text_snapshot_set(out_text, TERM_SLATE, buf);
}

static void app_build_tracked_monster_texts(app_status_snapshot* status)
{
    monster_type* m_ptr;
    char name[APP_DUNGEON_STATUS_TEXT_MAX];
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    int alertness_attr = TERM_WHITE;

    app_text_snapshot_clear(&status->tracked_name_text);
    app_text_snapshot_clear(&status->tracked_health_text);
    app_text_snapshot_clear(&status->tracked_alertness_text);
    status->tracked_visible = 0;
    status->tracked_m_idx = 0;
    status->tracked_hp_cur = 0;
    status->tracked_hp_max = 0;
    status->tracked_hp_attr = TERM_WHITE;

    if (!p_ptr->health_who || p_ptr->image)
        return;

    m_ptr = &mon_list[p_ptr->health_who];
    if (!m_ptr->r_idx || !m_ptr->ml || (m_ptr->hp <= 0))
        return;

    status->tracked_visible = 1;
    status->tracked_m_idx = p_ptr->health_who;
    status->tracked_hp_cur = m_ptr->hp;
    status->tracked_hp_max = m_ptr->maxhp;
    status->tracked_hp_attr = health_attr(m_ptr->hp, m_ptr->maxhp);

    monster_desc(name, sizeof(name), m_ptr, 0);
    app_text_snapshot_set(&status->tracked_name_text, TERM_L_BLUE, name);

    strnfmt(text, sizeof(text), "%d/%d", m_ptr->hp, m_ptr->maxhp);
    app_text_snapshot_set(&status->tracked_health_text,
        status->tracked_hp_attr, text);

    if (get_alertness_text(m_ptr, sizeof(text), text, &alertness_attr))
        app_text_snapshot_set(&status->tracked_alertness_text,
            (byte)alertness_attr, text);
}

static bool app_build_status_blob(app_dungeon_snapshot* snapshot,
    const app_wait_state* wait_state)
{
    app_status_snapshot* status;
    char state_long[APP_DUNGEON_STATUS_TEXT_MAX];
    char state_short[12];
    byte state_attr = TERM_WHITE;

    if (!app_dungeon_buffer_reserve(&snapshot->status_data,
            &snapshot->status_capacity, sizeof(*status)))
    {
        return false;
    }

    status = (app_status_snapshot*)snapshot->status_data;
    memset(status, 0, sizeof(*status));

    status->format_version = APP_DUNGEON_STATUS_FORMAT_VERSION;
    if (app_dungeon_compact_width())
        status->flags |= APP_DUNGEON_SNAPSHOT_FLAG_COMPACT_WIDTH;
    if (app_dungeon_compact_height())
        status->flags |= APP_DUNGEON_SNAPSHOT_FLAG_COMPACT_HEIGHT;
    if (g_hide_left_panel)
        status->flags |= APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL;
    if (wait_state && (wait_state->reason != APP_WAIT_REASON_NONE))
        status->flags |= APP_DUNGEON_SNAPSHOT_FLAG_WAITING;

    status->exp = p_ptr->new_exp;
    status->depth = p_ptr->depth;
    status->hp_cur = p_ptr->chp;
    status->hp_max = p_ptr->mhp;
    status->voice_cur = p_ptr->csp;
    status->voice_max = p_ptr->msp;
    status->str_use = p_ptr->stat_use[A_STR];
    status->dex_use = p_ptr->stat_use[A_DEX];
    status->con_use = p_ptr->stat_use[A_CON];
    status->gra_use = p_ptr->stat_use[A_GRA];
    status->melee_skill = p_ptr->skill_use[S_MEL];
    status->archery_skill = p_ptr->skill_use[S_ARC];
    status->evasion_skill = p_ptr->skill_use[S_EVN];
    status->hp_attr = health_attr(p_ptr->chp, p_ptr->mhp);
    if (p_ptr->csp >= p_ptr->msp)
        status->voice_attr = TERM_L_GREEN;
    else if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
        status->voice_attr = TERM_YELLOW;
    else
        status->voice_attr = TERM_RED;

    SDL_strlcpy(status->player_name, op_ptr->full_name,
        sizeof(status->player_name));

    app_build_primary_stat_text(&status->melee_text, p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    app_build_primary_stat_text(&status->archery_text,
        p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    app_build_armor_text(&status->evasion_text);
    app_build_quiver_text(&status->quiver_text);
    app_build_light_text(&status->light_text);
    app_build_depth_text(&status->depth_text);
    app_build_terrain_text(&status->terrain_text);
    app_build_hunger_text(&status->hunger_text);
    app_build_condition_text(&status->blind_text, p_ptr->blind, TERM_ORANGE,
        "Blind");
    app_build_condition_text(&status->confused_text, p_ptr->confused,
        TERM_ORANGE, "Confused");
    app_build_condition_text(&status->afraid_text, p_ptr->afraid,
        TERM_ORANGE, "Afraid");
    app_build_cut_text(&status->cut_text);
    app_build_poison_text(&status->poisoned_text);
    app_build_stun_text(&status->stun_text);
    app_build_speed_text(&status->speed_text);
    app_build_song_text(&status->song_text);

    state_long[0] = '\0';
    state_short[0] = '\0';
    if (app_status_state_text(state_long, sizeof(state_long), state_short,
            sizeof(state_short), &state_attr))
    {
        app_text_snapshot_set(&status->state_text, state_attr, state_long);
    }

    if (app_partition_abbrev_for_point(p_ptr->py, p_ptr->px)[0]
        && status->terrain_text.active)
    {
        char buf[APP_DUNGEON_STATUS_TEXT_MAX];

        strnfmt(buf, sizeof(buf), "%s %s",
            app_partition_abbrev_for_point(p_ptr->py, p_ptr->px),
            status->terrain_text.text);
        app_text_snapshot_set(&status->terrain_text, status->terrain_text.attr,
            buf);
    }
    else if (app_partition_abbrev_for_point(p_ptr->py, p_ptr->px)[0]
        && !status->terrain_text.active)
    {
        app_text_snapshot_set(&status->terrain_text, TERM_WHITE,
            app_partition_abbrev_for_point(p_ptr->py, p_ptr->px));
    }

    app_build_tracked_monster_texts(status);

    snapshot->status_size = sizeof(*status);
    return true;
}

static bool app_build_messages_blob(app_dungeon_snapshot* snapshot)
{
    size_t line_count = 0;
    size_t required;
    app_messages_snapshot* messages;
    char top_line[APP_DUNGEON_MESSAGE_TEXT_MAX];
    byte top_line_color = TERM_WHITE;
    u16b top_line_type = MSG_GENERIC;
    bool more_pending = false;
    size_t i;

    if (message_num() > 0)
        line_count = (size_t)message_num();
    if (line_count > APP_DUNGEON_MESSAGE_LIMIT)
        line_count = APP_DUNGEON_MESSAGE_LIMIT;

    required = sizeof(*messages)
        + (line_count * sizeof(app_message_line_snapshot));

    if (!app_dungeon_buffer_reserve(&snapshot->messages_data,
            &snapshot->messages_capacity, required))
    {
        return false;
    }

    messages = (app_messages_snapshot*)snapshot->messages_data;
    memset(messages, 0, required);

    messages->format_version = APP_DUNGEON_MESSAGES_FORMAT_VERSION;
    messages->line_count = (u16b)line_count;

    top_line[0] = '\0';
    if (message_topline_snapshot(top_line, sizeof(top_line), &top_line_color,
            &top_line_type, &more_pending))
    {
        messages->top_line_active = 1;
        messages->top_line_color = top_line_color;
        messages->top_line_type = top_line_type;
        messages->more_pending = more_pending ? 1 : 0;
        SDL_strlcpy(messages->top_line, top_line, sizeof(messages->top_line));
    }

    for (i = 0; i < line_count; i++)
    {
        app_message_line_snapshot* line = &messages->lines[i];

        memset(line, 0, sizeof(*line));
        line->type = message_type((s16b)i);
        line->color = message_color((s16b)i);
        line->age = (s16b)i;
        SDL_strlcpy(line->text, message_str((s16b)i), sizeof(line->text));
    }

    snapshot->messages_size = required;
    return true;
}

static bool app_build_panes_blob(app_dungeon_snapshot* snapshot)
{
    app_panes_snapshot* panes;
    int combat_count;

    if (!app_dungeon_buffer_reserve(&snapshot->panes_data,
            &snapshot->panes_capacity, sizeof(*panes)))
    {
        return false;
    }

    panes = (app_panes_snapshot*)snapshot->panes_data;
    memset(panes, 0, sizeof(*panes));

    panes->format_version = APP_DUNGEON_PANES_FORMAT_VERSION;
    panes->main_combat_roll_lines = op_ptr->main_combat_rolls;

    if (g_hide_left_panel)
        panes->flags |= APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL;

    combat_count = app_collect_combat_entries(panes->combat_entries,
        APP_DUNGEON_COMBAT_ENTRY_MAX);
    panes->combat_entry_count = (u16b)combat_count;
    snapshot->panes_size = sizeof(*panes);
    return true;
}

static void app_build_top_strip_ui_panel(app_ui_scene* scene,
    const app_messages_snapshot* messages)
{
    app_ui_panel* panel;
    byte attr = TERM_WHITE;
    cptr text = " ";

    if (!scene)
        return;

    panel = app_dungeon_ui_append_chrome_panel(scene, APP_UI_PANEL_STYLE_STRIP,
        APP_UI_PANEL_FLAG_TOP_ANCHORED | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    if (messages)
    {
        if (messages->top_line_active && messages->top_line[0])
        {
            attr = messages->top_line_color;
            text = messages->top_line;
        }
        else if (messages->line_count > 0 && messages->lines[0].text[0])
        {
            attr = messages->lines[0].color;
            text = messages->lines[0].text;
        }
    }

    (void)app_ui_panel_add_body_line(panel, attr, text);
}

static void app_build_left_rail_ui_panel(app_ui_scene* scene,
    const app_status_snapshot* status)
{
    app_ui_panel* panel;
    app_dungeon_overlay_layout_local layout;
    bool compact_height;
    int max_row;
    int row;
    char lines[APP_DUNGEON_LEFT_PANEL_ROWS_MAX][APP_UI_TEXT_MAX];
    byte attrs[APP_DUNGEON_LEFT_PANEL_ROWS_MAX];

    if (!scene || !status)
        return;

    panel = app_dungeon_ui_append_chrome_panel(scene,
        APP_UI_PANEL_STYLE_STATUS_RAIL, APP_UI_PANEL_FLAG_TOP_ANCHORED
            | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    panel->min_width_px = 120;
    panel->width_cap_px = 260;

    if (status->flags & APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL)
    {
        app_hidden_overlay_line_local
            hidden_lines[APP_DUNGEON_HIDDEN_OVERLAY_MAX];
        int hidden_count = app_hidden_overlay_build_lines(hidden_lines,
            APP_DUNGEON_HIDDEN_OVERLAY_MAX);
        int i;

        for (i = 0; i < hidden_count; i++)
        {
            (void)app_dungeon_ui_append_status_row_or_blank(panel,
                hidden_lines[i].attr, hidden_lines[i].text);
        }
        if (panel->row_count == 0)
            (void)app_dungeon_ui_append_status_row_or_blank(panel,
                TERM_WHITE, " ");
        return;
    }

    compact_height = (status->flags & APP_DUNGEON_SNAPSHOT_FLAG_COMPACT_HEIGHT)
        ? true : false;
    app_dungeon_overlay_layout_init(&layout, compact_height);
    max_row = layout.row_song;
    if (max_row >= (int)APP_DUNGEON_LEFT_PANEL_ROWS_MAX)
        max_row = (int)APP_DUNGEON_LEFT_PANEL_ROWS_MAX - 1;

    for (row = 0; row <= max_row; row++)
    {
        lines[row][0] = '\0';
        attrs[row] = TERM_WHITE;
    }

    app_dungeon_ui_line_set(lines[layout.row_name],
        sizeof(lines[layout.row_name]), status->player_name);
    attrs[layout.row_name] = TERM_WHITE;

    strnfmt(lines[layout.row_stat + 0], sizeof(lines[layout.row_stat + 0]),
        "Str %2d", status->str_use);
    attrs[layout.row_stat + 0] = TERM_L_GREEN;
    strnfmt(lines[layout.row_stat + 1], sizeof(lines[layout.row_stat + 1]),
        "Dex %2d", status->dex_use);
    attrs[layout.row_stat + 1] = TERM_L_GREEN;
    strnfmt(lines[layout.row_stat + 2], sizeof(lines[layout.row_stat + 2]),
        "Con %2d", status->con_use);
    attrs[layout.row_stat + 2] = TERM_L_GREEN;
    strnfmt(lines[layout.row_stat + 3], sizeof(lines[layout.row_stat + 3]),
        "Gra %2d", status->gra_use);
    attrs[layout.row_stat + 3] = TERM_L_GREEN;

    strnfmt(lines[layout.row_exp], sizeof(lines[layout.row_exp]), "EXP %ld",
        (long)status->exp);
    attrs[layout.row_exp] = TERM_WHITE;

    strnfmt(lines[layout.row_hp], sizeof(lines[layout.row_hp]), "HP %d/%d",
        status->hp_cur, status->hp_max);
    attrs[layout.row_hp] = status->hp_attr;

    strnfmt(lines[layout.row_sp], sizeof(lines[layout.row_sp]),
        "Voice %d/%d", status->voice_cur, status->voice_max);
    attrs[layout.row_sp] = status->voice_attr;

    if (status->light_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_light],
            sizeof(lines[layout.row_light]), status->light_text.text);
        attrs[layout.row_light] = status->light_text.attr;
    }
    if (status->melee_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_mel],
            sizeof(lines[layout.row_mel]), status->melee_text.text);
        attrs[layout.row_mel] = status->melee_text.attr;
    }
    if (status->archery_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_arc],
            sizeof(lines[layout.row_arc]), status->archery_text.text);
        attrs[layout.row_arc] = status->archery_text.attr;
    }
    if (status->quiver_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_quiver],
            sizeof(lines[layout.row_quiver]), status->quiver_text.text);
        attrs[layout.row_quiver] = status->quiver_text.attr;
    }
    if (status->evasion_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_evn],
            sizeof(lines[layout.row_evn]), status->evasion_text.text);
        attrs[layout.row_evn] = status->evasion_text.attr;
    }
    if (status->tracked_name_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_info],
            sizeof(lines[layout.row_info]), status->tracked_name_text.text);
        attrs[layout.row_info] = status->tracked_name_text.attr;
    }
    if (status->tracked_health_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_info + 1],
            sizeof(lines[layout.row_info + 1]),
            status->tracked_health_text.text);
        attrs[layout.row_info + 1] = status->tracked_health_text.attr;
    }
    if (status->tracked_alertness_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_info + 2],
            sizeof(lines[layout.row_info + 2]),
            status->tracked_alertness_text.text);
        attrs[layout.row_info + 2] = status->tracked_alertness_text.attr;
    }

    if (status->cut_text.active && status->poisoned_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_cut - 1],
            sizeof(lines[layout.row_cut - 1]), status->cut_text.text);
        attrs[layout.row_cut - 1] = status->cut_text.attr;
        app_dungeon_ui_line_set(lines[layout.row_cut],
            sizeof(lines[layout.row_cut]), status->poisoned_text.text);
        attrs[layout.row_cut] = status->poisoned_text.attr;
    }
    else if (status->cut_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_cut],
            sizeof(lines[layout.row_cut]), status->cut_text.text);
        attrs[layout.row_cut] = status->cut_text.attr;
    }
    else if (status->poisoned_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_cut],
            sizeof(lines[layout.row_cut]), status->poisoned_text.text);
        attrs[layout.row_cut] = status->poisoned_text.attr;
    }

    if (status->song_text.active)
    {
        app_dungeon_ui_line_set(lines[layout.row_song],
            sizeof(lines[layout.row_song]), status->song_text.text);
        attrs[layout.row_song] = status->song_text.attr;
    }

    for (row = 0; row <= max_row; row++)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, attrs[row],
            lines[row]);
    }
}

static void app_build_bottom_strip_ui_panel(app_ui_scene* scene,
    const app_status_snapshot* status)
{
    app_ui_panel* panel;
    app_dungeon_overlay_layout_local layout;
    bool compact_height;
    char line[APP_UI_TEXT_MAX];

    if (!scene || !status)
        return;

    compact_height = (status->flags & APP_DUNGEON_SNAPSHOT_FLAG_COMPACT_HEIGHT)
        ? true : false;
    app_dungeon_overlay_layout_init(&layout, compact_height);

    panel = app_dungeon_ui_append_chrome_panel(scene, APP_UI_PANEL_STYLE_STRIP,
        APP_UI_PANEL_FLAG_BOTTOM_ANCHORED | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    line[0] = '\0';
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_hungry,
        &status->hunger_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_blind,
        &status->blind_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_confused,
        &status->confused_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_stun,
        &status->stun_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_afraid,
        &status->afraid_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_state,
        &status->state_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_speed,
        &status->speed_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_terrain,
        &status->terrain_text);
    app_dungeon_ui_line_write_at(line, sizeof(line), layout.col_depth,
        &status->depth_text);

    (void)app_dungeon_ui_append_body_or_blank(panel, TERM_WHITE, line);
}

static void app_build_chrome_ui_scene(app_dungeon_overlay_snapshot* overlay,
    const app_status_snapshot* status, const app_messages_snapshot* messages)
{
    if (!overlay)
        return;

    app_ui_scene_init(&overlay->chrome_scene);
    app_build_top_strip_ui_panel(&overlay->chrome_scene, messages);
    app_build_left_rail_ui_panel(&overlay->chrome_scene, status);
    app_build_bottom_strip_ui_panel(&overlay->chrome_scene, status);
}

static bool app_build_overlay_blob(app_dungeon_snapshot* snapshot,
    const app_status_snapshot* status, const app_messages_snapshot* messages,
    const app_interaction_state* interaction,
    const app_menu_scene* transient_menu)
{
    app_dungeon_overlay_snapshot* overlay;

    if (!snapshot)
        return false;

    if (!app_dungeon_buffer_reserve(&snapshot->overlay_data,
            &snapshot->overlay_capacity, sizeof(*overlay)))
    {
        return false;
    }

    overlay = (app_dungeon_overlay_snapshot*)snapshot->overlay_data;
    app_dungeon_overlay_snapshot_clear(overlay);
    app_build_chrome_ui_scene(overlay, status, messages);

    if (interaction)
        memcpy(&overlay->interaction, interaction, sizeof(overlay->interaction));
    else
        app_interaction_snapshot_clear(&overlay->interaction);

    if (transient_menu)
    {
        overlay->flags |= APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_TRANSIENT_MENU;
        overlay->transient_menu = *transient_menu;
    }

    snapshot->overlay_size = sizeof(*overlay);
    return true;
}

void app_dungeon_snapshot_init(app_dungeon_snapshot* snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot->blobs[0].kind = APP_SNAPSHOT_BLOB_MAP;
    snapshot->blobs[0].format_version = APP_DUNGEON_MAP_FORMAT_VERSION;
    snapshot->blobs[1].kind = APP_SNAPSHOT_BLOB_STATUS;
    snapshot->blobs[1].format_version = APP_DUNGEON_STATUS_FORMAT_VERSION;
    snapshot->blobs[2].kind = APP_SNAPSHOT_BLOB_MESSAGES;
    snapshot->blobs[2].format_version = APP_DUNGEON_MESSAGES_FORMAT_VERSION;
    snapshot->blobs[3].kind = APP_SNAPSHOT_BLOB_PANES;
    snapshot->blobs[3].format_version = APP_DUNGEON_PANES_FORMAT_VERSION;
    snapshot->blobs[4].kind = APP_SNAPSHOT_BLOB_OVERLAY;
    snapshot->blobs[4].format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
}

void app_dungeon_snapshot_destroy(app_dungeon_snapshot* snapshot)
{
    if (!snapshot)
        return;

    mem_free_null(snapshot->map_data);
    mem_free_null(snapshot->status_data);
    mem_free_null(snapshot->messages_data);
    mem_free_null(snapshot->panes_data);
    mem_free_null(snapshot->overlay_data);
    memset(snapshot, 0, sizeof(*snapshot));
}

bool app_build_dungeon_snapshot(app_dungeon_snapshot* snapshot,
    u64b revision, const app_wait_state* wait_state,
    const app_interaction_state* interaction,
    const app_menu_scene* transient_menu, u32b update_mask, u32b redraw_mask,
    u32b window_mask)
{
    u16b snapshot_flags = 0;
    const app_status_snapshot* status;
    const app_messages_snapshot* messages;

    if (!snapshot || !Term || !p_ptr || !op_ptr || !character_generated)
        return false;

    if (!app_build_map_blob(snapshot, wait_state)
        || !app_build_status_blob(snapshot, wait_state)
        || !app_build_messages_blob(snapshot)
        || !app_build_panes_blob(snapshot))
    {
        return false;
    }

    status = (const app_status_snapshot*)snapshot->status_data;
    messages = (const app_messages_snapshot*)snapshot->messages_data;
    if (!app_build_overlay_blob(snapshot, status, messages, interaction,
            transient_menu))
    {
        return false;
    }

    if (wait_state && (wait_state->reason != APP_WAIT_REASON_NONE))
        snapshot_flags |= APP_SNAPSHOT_FLAG_WAITING;
    if (update_mask || redraw_mask || window_mask)
        snapshot_flags |= APP_SNAPSHOT_FLAG_DIRTY;

    snapshot->snapshot.revision = revision;
    snapshot->snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot->snapshot.flags = snapshot_flags;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);

    snapshot->blobs[0].data = snapshot->map_data;
    snapshot->blobs[0].size = snapshot->map_size;
    snapshot->blobs[1].data = snapshot->status_data;
    snapshot->blobs[1].size = snapshot->status_size;
    snapshot->blobs[2].data = snapshot->messages_data;
    snapshot->blobs[2].size = snapshot->messages_size;
    snapshot->blobs[3].data = snapshot->panes_data;
    snapshot->blobs[3].size = snapshot->panes_size;
    snapshot->blobs[4].data = snapshot->overlay_data;
    snapshot->blobs[4].size = snapshot->overlay_size;

    return true;
}

u32b app_snapshot_invalidation_from_masks(u32b update_mask, u32b redraw_mask,
    u32b window_mask)
{
    u32b mask = APP_SNAPSHOT_INVALIDATE_NONE;

    if (update_mask & (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE
            | PU_MONSTERS | PU_PANEL))
    {
        mask |= APP_SNAPSHOT_INVALIDATE_MAP
            | APP_SNAPSHOT_INVALIDATE_CURSOR
            | APP_SNAPSHOT_INVALIDATE_TARGET;
    }

    if (redraw_mask & PR_MAP)
        mask |= APP_SNAPSHOT_INVALIDATE_MAP;

    if (redraw_mask & (PR_BASIC | PR_MISC | PR_EXP | PR_STATS | PR_MEL
            | PR_ARC | PR_QUIVER | PR_ARMOR | PR_HP | PR_VOICE | PR_SONG
            | PR_DEPTH | PR_HEALTHBAR | PR_EXTRA | PR_CUT | PR_STUN
            | PR_HUNGER | PR_BLIND | PR_CONFUSED | PR_AFRAID | PR_POISONED
            | PR_STATE | PR_SPEED | PR_TERRAIN | PR_RESIST))
    {
        mask |= APP_SNAPSHOT_INVALIDATE_STATUS | APP_SNAPSHOT_INVALIDATE_PANES;
    }

    if (window_mask & PW_MESSAGE)
        mask |= APP_SNAPSHOT_INVALIDATE_MESSAGES;

    if (window_mask & (PW_INVEN | PW_EQUIP | PW_PLAYER_0 | PW_COMBAT_ROLLS
            | PW_MONSTER | PW_MONLIST))
    {
        mask |= APP_SNAPSHOT_INVALIDATE_PANES | APP_SNAPSHOT_INVALIDATE_STATUS;
    }

    return mask;
}

bool app_dump_dungeon_snapshot_text(const app_dungeon_snapshot* snapshot,
    char* buf, size_t buf_size)
{
    const app_map_snapshot* map;
    size_t pos = 0;

    if (!snapshot || !buf || !buf_size || !snapshot->map_data)
        return false;

    map = (const app_map_snapshot*)snapshot->map_data;
    buf[0] = '\0';

    for (u16b row = 0; row < map->height; row++)
    {
        for (u16b col = 0; col < map->width; col++)
        {
            size_t index = ((size_t)row * map->width) + col;
            char cell_ch = map->cells[index].ch ? map->cells[index].ch : ' ';

            if ((pos + 2) >= buf_size)
                return false;

            buf[pos++] = cell_ch;
        }

        if ((pos + 1) >= buf_size)
            return false;

        buf[pos++] = '\n';
    }

    buf[pos] = '\0';
    return true;
}
