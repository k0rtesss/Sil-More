#include "angband.h"

#include "app-scene-dungeon.h"
#include "app-session.h"
#include "externs.h"
#include "log/log.h"
#include "melee/melee-combat-display.h"
#include "ui/ui-status.h"

typedef struct app_text_snapshot {
    byte attr;
    byte active;
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
} app_text_snapshot;

typedef struct app_status_build_local {
    s32b exp;
    s16b hp_cur;
    s16b hp_max;
    s16b voice_cur;
    s16b voice_max;
    s16b str_use;
    s16b dex_use;
    s16b con_use;
    s16b gra_use;
    s16b tracked_hp_cur;
    s16b tracked_hp_max;
    byte hp_attr;
    byte voice_attr;
    byte tracked_hp_attr;
    byte tracked_visible;
    char player_name[APP_DUNGEON_NAME_TEXT_MAX];
    app_text_snapshot melee_text;
    app_text_snapshot archery_text;
    app_text_snapshot evasion_text;
    app_text_snapshot quiver_text;
    app_text_snapshot light_text;
    app_text_snapshot depth_text;
    app_text_snapshot terrain_text;
    app_text_snapshot hunger_text;
    app_text_snapshot blind_text;
    app_text_snapshot confused_text;
    app_text_snapshot afraid_text;
    app_text_snapshot cut_text;
    app_text_snapshot poisoned_text;
    app_text_snapshot stun_text;
    app_text_snapshot state_text;
    app_text_snapshot speed_text;
    app_text_snapshot song_text;
    app_text_snapshot tracked_name_text;
    app_text_snapshot tracked_health_text;
    app_text_snapshot tracked_alertness_text;
} app_status_build_local;

typedef struct app_footer_item_snapshot {
    byte attr;
    byte active;
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
} app_footer_item_snapshot;

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

static void app_dungeon_overlay_snapshot_clear(
    app_dungeon_overlay_snapshot* overlay)
{
    if (!overlay)
        return;

    memset(overlay, 0, sizeof(*overlay));
    overlay->format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    app_interaction_clear(&overlay->interaction);
    app_ui_scene_init(&overlay->transient_scene);
    app_ui_scene_init(&overlay->chrome_scene);
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
    const app_ui_row* source)
{
    app_ui_row* row;

    if (!panel || panel->row_count >= APP_UI_ROW_MAX)
        return false;

    row = &panel->rows[panel->row_count];
    memset(row, 0, sizeof(*row));
    row->id = (s16b)panel->row_count;
    row->attr = TERM_WHITE;
    row->meta_attr = TERM_WHITE;

    if (source)
        *row = *source;

    if (!row->key[0] && !row->label[0] && !row->meta[0]
        && !row->icon_char && !row->extra_icon_char)
    {
        SDL_strlcpy(row->label, " ", sizeof(row->label));
    }

    row->id = (s16b)panel->row_count;
    panel->row_count++;
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

static void app_status_compact_line_clear(app_status_compact_line* compact)
{
    if (!compact)
        return;

    memset(compact, 0, sizeof(*compact));
}

static bool app_status_compact_line_append(app_status_compact_line* compact,
    byte attr, bool required, cptr long_text, cptr short_text)
{
    app_status_compact_segment* segment;

    if (!compact || !long_text || !long_text[0]
        || compact->segment_count >= APP_DUNGEON_COMPACT_SEGMENT_MAX)
    {
        return false;
    }

    segment = &compact->segments[compact->segment_count++];
    memset(segment, 0, sizeof(*segment));
    segment->attr = attr;
    segment->required = required ? 1 : 0;
    SDL_strlcpy(segment->long_text, long_text, sizeof(segment->long_text));
    SDL_strlcpy(segment->short_text,
        (short_text && short_text[0]) ? short_text : long_text,
        sizeof(segment->short_text));
    return true;
}

static bool app_status_text_snapshot_copy(const app_text_snapshot* text,
    char* out_text, size_t out_text_sz, byte* out_attr)
{
    if (out_text && out_text_sz)
        out_text[0] = '\0';
    if (out_attr)
        *out_attr = TERM_WHITE;
    if (!text || !text->active || !text->text[0])
        return false;

    if (out_text && out_text_sz)
        SDL_strlcpy(out_text, text->text, out_text_sz);
    if (out_attr)
        *out_attr = text->attr;
    return true;
}

static void app_build_light_text(app_text_snapshot* out_text);
static void app_build_armor_text(app_text_snapshot* out_text);

byte app_status_depth_attr_live(void)
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

bool app_status_state_text_live(char* out_long, size_t out_long_sz,
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

static cptr app_partition_short_label(cptr long_label)
{
    if (!long_label || !long_label[0])
        return "";
    if (!strcmp(long_label, "Room"))
        return "Rm";
    if (!strcmp(long_label, "Ruin"))
        return "Ru";
    if (!strcmp(long_label, "Cave"))
        return "Cv";
    if (!strcmp(long_label, "BigCa"))
        return "BC";
    if (!strcmp(long_label, "Labir"))
        return "Lb";
    if (!strcmp(long_label, "Chasm"))
        return "Ch";
    return long_label;
}

static void app_status_row_clear(app_ui_row* row)
{
    if (!row)
        return;

    memset(row, 0, sizeof(*row));
    row->id = -1;
    row->attr = TERM_WHITE;
    row->meta_attr = TERM_WHITE;
}

static void app_status_row_set_section(app_ui_row* row, byte attr, cptr label,
    bool story_label)
{
    if (!row)
        return;

    app_status_row_clear(row);
    row->attr = attr;
    row->meta_attr = attr;
    row->flags = APP_UI_ITEM_FLAG_SECTION;
    if (story_label)
        row->flags |= APP_UI_ITEM_FLAG_STORY_LABEL;
    SDL_strlcpy(row->label, label ? label : "", sizeof(row->label));
}

static void app_status_row_set_key_meta(app_ui_row* row, byte key_attr,
    cptr key, byte meta_attr, cptr meta, bool story_label)
{
    if (!row)
        return;

    app_status_row_clear(row);
    row->attr = key_attr;
    row->meta_attr = meta_attr;
    if (story_label)
        row->flags |= APP_UI_ITEM_FLAG_STORY_LABEL;
    SDL_strlcpy(row->key, key ? key : "", sizeof(row->key));
    SDL_strlcpy(row->meta, meta ? meta : "", sizeof(row->meta));
}

static void app_status_row_set_value(app_ui_row* row, byte attr, cptr value)
{
    if (!row)
        return;

    app_status_row_clear(row);
    row->attr = attr;
    row->meta_attr = attr;
    SDL_strlcpy(row->meta, value ? value : "", sizeof(row->meta));
}

static void app_status_row_set_bar(app_ui_row* row, byte attr, cptr bar)
{
    if (!row)
        return;

    app_status_row_clear(row);
    row->attr = attr;
    row->meta_attr = attr;
    SDL_strlcpy(row->label, bar ? bar : "", sizeof(row->label));
}

static bool app_footer_snapshot_append(app_footer_item_snapshot* items,
    size_t item_capacity, size_t* item_count, const app_text_snapshot* text)
{
    app_footer_item_snapshot* item;

    if (!items || !item_count || !text || !text->active || !text->text[0]
        || *item_count >= item_capacity)
    {
        return false;
    }

    item = &items[*item_count];
    memset(item, 0, sizeof(*item));
    item->attr = text->attr;
    item->active = 1;
    SDL_strlcpy(item->text, text->text, sizeof(item->text));
    (*item_count)++;
    return true;
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
    else if (p_ptr->food >= PY_FOOD_MAX)
        app_text_snapshot_set(out_text, TERM_GREEN, "Full");
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

    app_text_snapshot_set(out_text, app_status_depth_attr_live(), buf);
}

static bool app_status_song_name_live(int song, char* out_text,
    size_t out_text_sz)
{
    cptr song_name;

    if (!out_text || !out_text_sz || !p_ptr || song == SNG_NOTHING)
        return false;

    song_name = b_name + (&b_info[ability_index(S_SNG, song)])->name;
    SDL_strlcpy(out_text, song_name + 8, out_text_sz);
    return out_text[0] != '\0';
}

bool app_status_song_lines_live(char* out_primary, size_t out_primary_sz,
    byte* out_primary_attr, char* out_secondary, size_t out_secondary_sz,
    byte* out_secondary_attr)
{
    bool primary_active = false;
    bool secondary_active = false;

    if (out_primary && out_primary_sz)
        out_primary[0] = '\0';
    if (out_secondary && out_secondary_sz)
        out_secondary[0] = '\0';
    if (out_primary_attr)
        *out_primary_attr = TERM_L_BLUE;
    if (out_secondary_attr)
        *out_secondary_attr = TERM_BLUE;
    if (!p_ptr)
        return false;

    primary_active = app_status_song_name_live(p_ptr->song1, out_primary,
        out_primary_sz);
    secondary_active = app_status_song_name_live(p_ptr->song2, out_secondary,
        out_secondary_sz);
    return primary_active || secondary_active;
}

static void app_build_song_text(app_text_snapshot* out_text)
{
    char primary[APP_DUNGEON_STATUS_TEXT_MAX];
    char secondary[APP_DUNGEON_STATUS_TEXT_MAX];

    primary[0] = '\0';
    secondary[0] = '\0';
    if (!app_status_song_lines_live(primary, sizeof(primary), NULL, secondary,
            sizeof(secondary), NULL))
    {
        app_text_snapshot_clear(out_text);
        return;
    }

    if (primary[0] && secondary[0])
    {
        char buf[APP_DUNGEON_STATUS_TEXT_MAX];

        strnfmt(buf, sizeof(buf), "%s + %s", primary, secondary);
        app_text_snapshot_set(out_text, TERM_L_BLUE, buf);
    }
    else if (primary[0])
    {
        app_text_snapshot_set(out_text, TERM_L_BLUE, primary);
    }
    else
    {
        app_text_snapshot_set(out_text, TERM_BLUE, secondary);
    }
}

bool app_status_text_live(app_status_text_kind kind, char* out_text,
    size_t out_text_sz, byte* out_attr)
{
    app_text_snapshot text;
    cptr partition_label = "";

    app_text_snapshot_clear(&text);
    if (!p_ptr)
        return app_status_text_snapshot_copy(&text, out_text, out_text_sz,
            out_attr);

    switch (kind)
    {
    case APP_STATUS_TEXT_HUNGER:
        app_build_hunger_text(&text);
        break;

    case APP_STATUS_TEXT_BLIND:
        app_build_condition_text(&text, p_ptr->blind, TERM_ORANGE, "Blind");
        break;

    case APP_STATUS_TEXT_CONFUSED:
        app_build_condition_text(&text, p_ptr->confused, TERM_ORANGE,
            "Confused");
        break;

    case APP_STATUS_TEXT_AFRAID:
        app_build_condition_text(&text, p_ptr->afraid, TERM_ORANGE, "Afraid");
        break;

    case APP_STATUS_TEXT_CUT:
        app_build_cut_text(&text);
        break;

    case APP_STATUS_TEXT_POISONED:
        app_build_poison_text(&text);
        break;

    case APP_STATUS_TEXT_STUN:
        app_build_stun_text(&text);
        break;

    case APP_STATUS_TEXT_SPEED:
        app_build_speed_text(&text);
        break;

    case APP_STATUS_TEXT_TERRAIN:
        app_build_terrain_text(&text);
        break;

    case APP_STATUS_TEXT_DEPTH:
        app_build_depth_text(&text);
        break;

    case APP_STATUS_TEXT_PARTITION:
        partition_label = app_partition_abbrev_for_point(p_ptr->py, p_ptr->px);
        if (partition_label[0])
            app_text_snapshot_set(&text, TERM_WHITE, partition_label);
        break;

    case APP_STATUS_TEXT_STATE:
    {
        char short_text[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];
        byte attr = TERM_WHITE;

        short_text[0] = '\0';
        if (app_status_state_text_live(out_text, out_text_sz, short_text,
                sizeof(short_text), &attr))
        {
            if (out_attr)
                *out_attr = attr;
            return true;
        }
        return false;
    }

    case APP_STATUS_TEXT_SONG:
        app_build_song_text(&text);
        break;

    case APP_STATUS_TEXT_LIGHT:
        app_build_light_text(&text);
        break;

    case APP_STATUS_TEXT_EVASION:
        app_build_armor_text(&text);
        break;

    default:
        return false;
    }

    return app_status_text_snapshot_copy(&text, out_text, out_text_sz, out_attr);
}

bool app_status_compact_line_build_live(app_status_compact_line* compact,
    bool include_song, bool include_wounds)
{
    char long_text[APP_DUNGEON_STATUS_TEXT_MAX];
    char state_long[APP_DUNGEON_STATUS_TEXT_MAX];
    char state_short[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];
    byte state_attr = TERM_WHITE;
    byte attr = TERM_WHITE;

    if (!compact || !p_ptr)
        return false;

    app_status_compact_line_clear(compact);

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_HUNGER, long_text,
            sizeof(long_text), &attr))
    {
        cptr hunger_short = "Hu";

        if (!strcmp(long_text, "Starving"))
            hunger_short = "St";
        else if (!strcmp(long_text, "Weak"))
            hunger_short = "Wk";
        else if (!strcmp(long_text, "Full"))
            hunger_short = "Fu";

        (void)app_status_compact_line_append(compact, attr, true, long_text,
            hunger_short);
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_BLIND, long_text,
            sizeof(long_text), &attr))
    {
        (void)app_status_compact_line_append(compact, attr, true, long_text,
            "Bl");
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_CONFUSED, long_text,
            sizeof(long_text), &attr))
    {
        (void)app_status_compact_line_append(compact, attr, true, long_text,
            "Cn");
    }

    if (include_wounds)
    {
        char cut_short[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];

        cut_short[0] = '\0';
        long_text[0] = '\0';
        if (app_status_text_live(APP_STATUS_TEXT_CUT, long_text,
                sizeof(long_text), &attr))
        {
            if (!strcmp(long_text, "Mortal wound"))
                SDL_strlcpy(cut_short, "MW", sizeof(cut_short));
            else
                strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            (void)app_status_compact_line_append(compact, attr, true, long_text,
                cut_short);
        }

        cut_short[0] = '\0';
        long_text[0] = '\0';
        if (app_status_text_live(APP_STATUS_TEXT_POISONED, long_text,
                sizeof(long_text), &attr))
        {
            strnfmt(cut_short, sizeof(cut_short), "P%d", p_ptr->poisoned);
            (void)app_status_compact_line_append(compact, attr, true, long_text,
                cut_short);
        }
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_STUN, long_text,
            sizeof(long_text), &attr))
    {
        cptr stun_short = "St";

        if (!strcmp(long_text, "Knocked out"))
            stun_short = "KO";
        else if (!strcmp(long_text, "Heavy stun"))
            stun_short = "HS";

        (void)app_status_compact_line_append(compact, attr, true, long_text,
            stun_short);
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_AFRAID, long_text,
            sizeof(long_text), &attr))
    {
        (void)app_status_compact_line_append(compact, attr, true, long_text,
            "Af");
    }

    if (include_song && app_status_text_live(APP_STATUS_TEXT_SONG, long_text,
            sizeof(long_text), &attr))
    {
        char song_short[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];

        song_short[0] = '\0';
        strnfmt(song_short, sizeof(song_short), "S:%.*s", 6, long_text);
        (void)app_status_compact_line_append(compact, attr, false, long_text,
            song_short);
    }

    state_long[0] = '\0';
    state_short[0] = '\0';
    if (app_status_state_text_live(state_long, sizeof(state_long), state_short,
            sizeof(state_short), &state_attr))
    {
        (void)app_status_compact_line_append(compact, state_attr, false,
            state_long, state_short);
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_SPEED, long_text,
            sizeof(long_text), &attr))
    {
        cptr speed_short = !strcmp(long_text, "Fast") ? "Fa" : "Sl";

        (void)app_status_compact_line_append(compact, attr, false, long_text,
            speed_short);
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_TERRAIN, long_text,
            sizeof(long_text), &attr))
    {
        cptr terrain_short = "Sn";

        if (!strcmp(long_text, "Pit"))
            terrain_short = "Pt";
        else if (!strcmp(long_text, "Web"))
            terrain_short = "Wb";

        (void)app_status_compact_line_append(compact, attr, false, long_text,
            terrain_short);
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_PARTITION, long_text,
            sizeof(long_text), &attr))
    {
        (void)app_status_compact_line_append(compact, attr, false, long_text,
            app_partition_short_label(long_text));
    }

    long_text[0] = '\0';
    if (app_status_text_live(APP_STATUS_TEXT_DEPTH, long_text, sizeof(long_text),
            &attr))
    {
        char depth_short[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];

        if (!p_ptr->depth)
            SDL_strlcpy(depth_short, "0'", sizeof(depth_short));
        else
            strnfmt(depth_short, sizeof(depth_short), "%d'", p_ptr->depth * 50);
        (void)app_status_compact_line_append(compact, attr, true, long_text,
            depth_short);
    }

    return true;
}

static void app_build_quiver_text(app_text_snapshot* out_text)
{
    app_status_quiver_live quiver;
    char buf[APP_DUNGEON_STATUS_TEXT_MAX];

    buf[0] = '\0';
    if (!app_status_quiver_live_build(&quiver))
    {
        app_text_snapshot_clear(out_text);
        return;
    }

    if (quiver.q1_active)
    {
        strnfmt(buf, sizeof(buf), "Q1 %d/%d", quiver.q1_current,
            quiver.q1_max);
    }
    if (quiver.q2_active)
    {
        size_t len = strlen(buf);
        strnfmt(buf + len, sizeof(buf) - len, "%sQ2 %d/%d",
            len ? " " : "", quiver.q2_current, quiver.q2_max);
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
        SDL_strlcpy(buf, "inf", sizeof(buf));
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

        strnfmt(buf, sizeof(buf), "%ld", fuel);
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

bool app_status_tracked_monster_live_build(
    app_status_tracked_monster_live* tracked)
{
    monster_type* m_ptr;
    int alertness_attr = TERM_WHITE;

    if (!tracked)
        return false;

    memset(tracked, 0, sizeof(*tracked));
    tracked->hp_attr = TERM_WHITE;
    tracked->alertness_attr = TERM_WHITE;

    if (!p_ptr->health_who || p_ptr->image)
        return false;

    m_ptr = &mon_list[p_ptr->health_who];
    if (!m_ptr->r_idx || !m_ptr->ml || (m_ptr->hp <= 0))
        return false;

    tracked->visible = 1;
    tracked->hp_cur = m_ptr->hp;
    tracked->hp_max = m_ptr->maxhp;
    tracked->hp_attr = health_attr(m_ptr->hp, m_ptr->maxhp);
    tracked->confused = m_ptr->confused ? 1 : 0;
    tracked->stunned = m_ptr->stunned ? 1 : 0;

    monster_desc(tracked->name, sizeof(tracked->name), m_ptr, 0);
    strnfmt(tracked->health, sizeof(tracked->health), "%d/%d", m_ptr->hp,
        m_ptr->maxhp);

    if (get_alertness_text(m_ptr, sizeof(tracked->alertness),
            tracked->alertness, &alertness_attr))
    {
        tracked->alertness_attr = (byte)alertness_attr;
    }

    return true;
}

bool app_status_quiver_live_build(app_status_quiver_live* quiver)
{
    object_type* q1_ptr;
    object_type* q2_ptr;

    if (!quiver)
        return false;

    memset(quiver, 0, sizeof(*quiver));
    q1_ptr = &inventory[INVEN_QUIVER1];
    q2_ptr = &inventory[INVEN_QUIVER2];

    if (q1_ptr->k_idx)
    {
        quiver->q1_active = 1;
        quiver->q1_attr = object_attr(q1_ptr);
        quiver->q1_char = object_char(q1_ptr);
        quiver->q1_current = q1_ptr->number;
        quiver->q1_max = object_stack_limit(q1_ptr);
    }

    if (q2_ptr->k_idx)
    {
        quiver->q2_active = 1;
        quiver->q2_attr = object_attr(q2_ptr);
        quiver->q2_char = object_char(q2_ptr);
        quiver->q2_current = q2_ptr->number;
        quiver->q2_max = object_stack_limit(q2_ptr);
    }

    if (quiver->q1_active && quiver->q2_active
        && q1_ptr->tval == q2_ptr->tval
        && q1_ptr->sval == q2_ptr->sval)
    {
        quiver->same_type = 1;
    }

    return quiver->q1_active || quiver->q2_active;
}

static void app_build_tracked_monster_texts(app_status_build_local* live)
{
    app_status_tracked_monster_live tracked;

    app_text_snapshot_clear(&live->tracked_name_text);
    app_text_snapshot_clear(&live->tracked_health_text);
    app_text_snapshot_clear(&live->tracked_alertness_text);
    live->tracked_visible = 0;
    live->tracked_hp_cur = 0;
    live->tracked_hp_max = 0;
    live->tracked_hp_attr = TERM_WHITE;

    if (!app_status_tracked_monster_live_build(&tracked))
        return;

    live->tracked_visible = tracked.visible;
    live->tracked_hp_cur = tracked.hp_cur;
    live->tracked_hp_max = tracked.hp_max;
    live->tracked_hp_attr = tracked.hp_attr;
    app_text_snapshot_set(&live->tracked_name_text, TERM_L_BLUE, tracked.name);
    app_text_snapshot_set(&live->tracked_health_text, tracked.hp_attr,
        tracked.health);

    if (tracked.alertness[0])
    {
        app_text_snapshot_set(&live->tracked_alertness_text,
            tracked.alertness_attr, tracked.alertness);
    }
}

static void app_build_hidden_left_panel_rows(app_ui_panel* panel,
    const app_status_build_local* live)
{
    app_ui_row row;
    char buf[32];

    if (!panel || !live)
        return;

    panel->min_width_px = 72;
    panel->width_cap_px = 160;

    strnfmt(buf, sizeof(buf), "HP %3d", MIN(live->hp_cur, 999));
    app_status_row_set_bar(&row, live->hp_attr, buf);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    strnfmt(buf, sizeof(buf), "VC %3d", MIN(live->voice_cur, 999));
    app_status_row_set_bar(&row, live->voice_attr, buf);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    if (p_ptr->cut > 100)
    {
        app_status_row_set_bar(&row, TERM_RED, "MW !!!");
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }
    else if (p_ptr->cut > 0)
    {
        strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
        app_status_row_set_bar(&row,
            (p_ptr->cut > 20) ? TERM_RED : TERM_L_RED, buf);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }

    if (p_ptr->poisoned > 0)
    {
        strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
        app_status_row_set_bar(&row,
            (p_ptr->poisoned > 20) ? TERM_L_GREEN : TERM_GREEN, buf);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }

    if (live->song_text.active)
    {
        app_status_row_set_bar(&row, live->song_text.attr,
            live->song_text.text);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }

    if (live->tracked_visible && live->tracked_hp_max > 0)
    {
        int filled = (8 * live->tracked_hp_cur + live->tracked_hp_max - 1)
            / live->tracked_hp_max;
        int i;

        if (filled < 0)
            filled = 0;
        if (filled > 8)
            filled = 8;

        for (i = 0; i < filled; i++)
            buf[i] = '*';
        buf[filled] = '\0';

        app_status_row_set_bar(&row, live->tracked_hp_attr, buf);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }
}

static void app_build_left_panel_rows(app_ui_panel* panel,
    const app_status_build_local* live, bool hide_left_panel)
{
    app_ui_row row;
    char value_buf[APP_UI_META_MAX];
    char bar[13];
    bool need_gap;
    int filled;
    int i;

    if (!panel || !live)
        return;

    if (hide_left_panel)
    {
        app_build_hidden_left_panel_rows(panel, live);
        return;
    }

    panel->min_width_px = 120;
    panel->width_cap_px = 260;

    app_status_row_set_section(&row, TERM_L_BLUE, live->player_name, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    filled = 0;
    if (live->hp_max > 0)
        filled = (12 * live->hp_cur + live->hp_max - 1) / live->hp_max;
    if (filled < 0)
        filled = 0;
    if (filled > 12)
        filled = 12;
    for (i = 0; i < filled; i++)
        bar[i] = 'x';
    for (i = filled; i < 12; i++)
        bar[i] = ' ';
    bar[12] = '\0';
    app_status_row_set_bar(&row, live->hp_attr, bar);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);

    cnv_stat(live->str_use, value_buf);
    app_status_row_set_key_meta(&row, TERM_WHITE, "Str",
        (p_ptr->stat_drain[A_STR] < 0) ? TERM_YELLOW : TERM_L_GREEN,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    cnv_stat(live->dex_use, value_buf);
    app_status_row_set_key_meta(&row, TERM_WHITE, "Dex",
        (p_ptr->stat_drain[A_DEX] < 0) ? TERM_YELLOW : TERM_L_GREEN,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    cnv_stat(live->con_use, value_buf);
    app_status_row_set_key_meta(&row, TERM_WHITE, "Con",
        (p_ptr->stat_drain[A_CON] < 0) ? TERM_YELLOW : TERM_L_GREEN,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    cnv_stat(live->gra_use, value_buf);
    app_status_row_set_key_meta(&row, TERM_WHITE, "Gra",
        (p_ptr->stat_drain[A_GRA] < 0) ? TERM_YELLOW : TERM_L_GREEN,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);

    comma_number(value_buf, live->exp);
    app_status_row_set_key_meta(&row, TERM_WHITE, "Exp", TERM_L_GREEN,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    strnfmt(value_buf, sizeof(value_buf), "%d/%d", live->hp_cur,
        live->hp_max);
    app_status_row_set_key_meta(&row, TERM_WHITE,
        (live->hp_max >= 100) ? "Hth" : "Health", live->hp_attr,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    strnfmt(value_buf, sizeof(value_buf), "%d:%d", live->voice_cur,
        live->voice_max);
    app_status_row_set_key_meta(&row, TERM_WHITE,
        (live->voice_max >= 100) ? "Vce" : "Voice", live->voice_attr,
        value_buf, true);
    (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);

    if (live->light_text.active)
    {
        object_type* light_ptr = &inventory[INVEN_LITE];

        app_status_row_set_key_meta(&row, TERM_WHITE, "", live->light_text.attr,
            live->light_text.text, false);
        if (light_ptr->k_idx)
        {
            row.icon_attr = object_attr(light_ptr);
            row.icon_char = object_char(light_ptr);
        }
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }

    need_gap = live->melee_text.active || live->archery_text.active
        || live->quiver_text.active;
    if (need_gap)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);

        if (live->melee_text.active)
        {
            app_status_row_set_value(&row, live->melee_text.attr,
                live->melee_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
        if (live->archery_text.active)
        {
            app_status_row_set_value(&row, live->archery_text.attr,
                live->archery_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
        if (live->quiver_text.active)
        {
            app_status_quiver_live quiver;

            app_status_row_clear(&row);
            row.attr = live->quiver_text.attr;
            row.meta_attr = live->quiver_text.attr;
            (void)app_status_quiver_live_build(&quiver);

            if (quiver.same_type)
            {
                strnfmt(row.label, sizeof(row.label), "%d/%d",
                    quiver.q1_current, quiver.q1_max);
                strnfmt(row.meta, sizeof(row.meta), "%d/%d",
                    quiver.q2_current, quiver.q2_max);
                row.extra_icon_attr = quiver.q1_attr;
                row.extra_icon_char = quiver.q1_char;
            }
            else
            {
                if (quiver.q1_active)
                {
                    row.icon_attr = quiver.q1_attr;
                    row.icon_char = quiver.q1_char;
                    strnfmt(row.label, sizeof(row.label), "%d/%d",
                        quiver.q1_current, quiver.q1_max);
                }
                if (quiver.q2_active)
                {
                    row.extra_icon_attr = quiver.q2_attr;
                    row.extra_icon_char = quiver.q2_char;
                    strnfmt(row.meta, sizeof(row.meta), "%d/%d",
                        quiver.q2_current, quiver.q2_max);
                }
            }

            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
    }

    if (live->evasion_text.active)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);
        app_status_row_set_value(&row, live->evasion_text.attr,
            live->evasion_text.text);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }

    need_gap = live->tracked_name_text.active || live->tracked_health_text.active
        || live->tracked_alertness_text.active;
    if (need_gap)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);
        if (live->tracked_name_text.active)
        {
            app_status_row_set_section(&row, live->tracked_name_text.attr,
                live->tracked_name_text.text, true);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
        if (live->tracked_health_text.active)
        {
            app_status_row_set_value(&row, live->tracked_health_text.attr,
                live->tracked_health_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
        if (live->tracked_alertness_text.active)
        {
            app_status_row_set_value(&row, live->tracked_alertness_text.attr,
                live->tracked_alertness_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
    }

    need_gap = live->cut_text.active || live->poisoned_text.active;
    if (need_gap)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);
        if (live->cut_text.active)
        {
            app_status_row_set_value(&row, live->cut_text.attr,
                live->cut_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
        if (live->poisoned_text.active)
        {
            app_status_row_set_value(&row, live->poisoned_text.attr,
                live->poisoned_text.text);
            (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
        }
    }

    if (live->song_text.active)
    {
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);
        app_status_row_set_section(&row, live->song_text.attr,
            live->song_text.text, true);
        (void)app_dungeon_ui_append_status_row_or_blank(panel, &row);
    }
}

static void app_build_footer_items(app_footer_item_snapshot* items,
    size_t item_capacity, size_t* item_count,
    const app_status_build_local* live)
{
    if (!items || !item_count || !live)
        return;

    memset(items, 0, sizeof(*items) * item_capacity);
    *item_count = 0;

    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->hunger_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->blind_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->confused_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->stun_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->afraid_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->state_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->speed_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->terrain_text);
    (void)app_footer_snapshot_append(items, item_capacity, item_count,
        &live->depth_text);
}

static bool app_status_build_live(app_status_build_local* live)
{
    char state_long[APP_DUNGEON_STATUS_TEXT_MAX];
    char state_short[12];
    byte state_attr = TERM_WHITE;
    cptr partition_label;

    if (!live || !p_ptr || !op_ptr)
        return false;

    memset(live, 0, sizeof(*live));

    live->exp = p_ptr->new_exp;
    live->hp_cur = p_ptr->chp;
    live->hp_max = p_ptr->mhp;
    live->voice_cur = p_ptr->csp;
    live->voice_max = p_ptr->msp;
    live->str_use = p_ptr->stat_use[A_STR];
    live->dex_use = p_ptr->stat_use[A_DEX];
    live->con_use = p_ptr->stat_use[A_CON];
    live->gra_use = p_ptr->stat_use[A_GRA];
    live->hp_attr = health_attr(p_ptr->chp, p_ptr->mhp);
    if (p_ptr->csp >= p_ptr->msp)
        live->voice_attr = TERM_L_GREEN;
    else if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
        live->voice_attr = TERM_YELLOW;
    else
        live->voice_attr = TERM_RED;

    SDL_strlcpy(live->player_name, op_ptr->full_name,
        sizeof(live->player_name));

    app_build_primary_stat_text(&live->melee_text, p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    app_build_primary_stat_text(&live->archery_text,
        p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    app_build_armor_text(&live->evasion_text);
    app_build_quiver_text(&live->quiver_text);
    app_build_light_text(&live->light_text);
    app_build_depth_text(&live->depth_text);
    app_build_terrain_text(&live->terrain_text);
    app_build_hunger_text(&live->hunger_text);
    app_build_condition_text(&live->blind_text, p_ptr->blind, TERM_ORANGE,
        "Blind");
    app_build_condition_text(&live->confused_text, p_ptr->confused,
        TERM_ORANGE, "Confused");
    app_build_condition_text(&live->afraid_text, p_ptr->afraid,
        TERM_ORANGE, "Afraid");
    app_build_cut_text(&live->cut_text);
    app_build_poison_text(&live->poisoned_text);
    app_build_stun_text(&live->stun_text);
    app_build_speed_text(&live->speed_text);
    app_build_song_text(&live->song_text);

    state_long[0] = '\0';
    state_short[0] = '\0';
    if (app_status_state_text_live(state_long, sizeof(state_long), state_short,
            sizeof(state_short), &state_attr))
    {
        app_text_snapshot_set(&live->state_text, state_attr, state_long);
    }

    partition_label = app_partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    if (partition_label[0] && live->terrain_text.active)
    {
        char buf[APP_DUNGEON_STATUS_TEXT_MAX];

        strnfmt(buf, sizeof(buf), "%s %s", partition_label,
            live->terrain_text.text);
        app_text_snapshot_set(&live->terrain_text, live->terrain_text.attr,
            buf);
    }
    else if (partition_label[0] && !live->terrain_text.active)
    {
        app_text_snapshot_set(&live->terrain_text, TERM_WHITE,
            partition_label);
    }

    app_build_tracked_monster_texts(live);
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

    combat_count = app_collect_combat_entries(panes->combat_entries,
        APP_DUNGEON_COMBAT_ENTRY_MAX);
    panes->combat_entry_count = (u16b)combat_count;
    snapshot->panes_size = sizeof(*panes);
    return true;
}

static void app_build_top_strip_ui_panel(app_ui_scene* scene)
{
    app_ui_panel* panel;
    char top_line[APP_DUNGEON_MESSAGE_TEXT_MAX];
    byte attr = TERM_WHITE;
    u16b top_line_type = MSG_GENERIC;
    bool more_pending = false;
    cptr text = " ";

    if (!scene)
        return;

    panel = app_dungeon_ui_append_chrome_panel(scene, APP_UI_PANEL_STYLE_STRIP,
        APP_UI_PANEL_FLAG_TOP_ANCHORED | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    top_line[0] = '\0';
    if (message_topline_snapshot(top_line, sizeof(top_line), &attr,
            &top_line_type, &more_pending)
        && top_line[0])
    {
        text = top_line;
    }
    else if (message_num() > 0 && message_str(0)[0])
    {
        attr = message_color(0);
        text = message_str(0);
    }

    (void)top_line_type;
    (void)more_pending;
    (void)app_ui_panel_add_body_line(panel, attr, text);
}

static void app_build_left_rail_ui_panel(app_ui_scene* scene,
    const app_status_build_local* live, bool hide_left_panel)
{
    app_ui_panel* panel;
    u16b style;

    if (!scene || !live)
        return;

    /* Hidden-left-panel mode is an overlay rail rather than reserved chrome. */
    style = hide_left_panel
        ? APP_UI_PANEL_STYLE_OVERLAY_RAIL
        : APP_UI_PANEL_STYLE_STATUS_RAIL;
    panel = app_dungeon_ui_append_chrome_panel(scene, style,
        APP_UI_PANEL_FLAG_TOP_ANCHORED | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    app_build_left_panel_rows(panel, live, hide_left_panel);
    if (panel->row_count == 0)
        (void)app_dungeon_ui_append_status_row_or_blank(panel, NULL);
}

static void app_build_bottom_strip_ui_panel(app_ui_scene* scene,
    const app_status_build_local* live)
{
    app_footer_item_snapshot items[9];
    size_t item_count = 0;
    app_ui_panel* panel;
    char line[APP_UI_TEXT_MAX];

    if (!scene || !live)
        return;

    panel = app_dungeon_ui_append_chrome_panel(scene, APP_UI_PANEL_STYLE_STRIP,
        APP_UI_PANEL_FLAG_BOTTOM_ANCHORED | APP_UI_PANEL_FLAG_LEFT_ANCHORED);
    if (!panel)
        return;

    app_build_footer_items(items, N_ELEMENTS(items), &item_count, live);

    line[0] = '\0';
    for (size_t i = 0; i < item_count; i++)
    {
        const app_footer_item_snapshot* item = &items[i];

        if (!item->active || !item->text[0])
            continue;
        if (line[0])
            SDL_strlcat(line, "  ", sizeof(line));
        SDL_strlcat(line, item->text, sizeof(line));
    }

    (void)app_dungeon_ui_append_body_or_blank(panel, TERM_WHITE, line);
}

static bool app_build_chrome_ui_scene(app_dungeon_overlay_snapshot* overlay)
{
    app_status_build_local live;

    if (!overlay)
        return false;
    if (!app_status_build_live(&live))
        return false;

    app_ui_scene_init(&overlay->chrome_scene);
    app_build_top_strip_ui_panel(&overlay->chrome_scene);
    app_build_left_rail_ui_panel(&overlay->chrome_scene, &live,
        ui_left_panel_hidden());
    app_build_bottom_strip_ui_panel(&overlay->chrome_scene, &live);
    return true;
}

static bool app_build_overlay_blob(app_dungeon_snapshot* snapshot,
    const app_interaction_state* interaction,
    const app_ui_scene* transient_scene)
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
    if (!app_build_chrome_ui_scene(overlay))
        return false;

    if (interaction)
        memcpy(&overlay->interaction, interaction, sizeof(overlay->interaction));
    else
        app_interaction_snapshot_clear(&overlay->interaction);

    if (transient_scene)
        overlay->transient_scene = *transient_scene;

    if (overlay->transient_scene.panel_count > 0)
        overlay->flags |= APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_TRANSIENT_MENU;

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
    snapshot->blobs[1].kind = APP_SNAPSHOT_BLOB_PANES;
    snapshot->blobs[1].format_version = APP_DUNGEON_PANES_FORMAT_VERSION;
    snapshot->blobs[2].kind = APP_SNAPSHOT_BLOB_OVERLAY;
    snapshot->blobs[2].format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
}

void app_dungeon_snapshot_destroy(app_dungeon_snapshot* snapshot)
{
    if (!snapshot)
        return;

    mem_free_null(snapshot->map_data);
    mem_free_null(snapshot->panes_data);
    mem_free_null(snapshot->overlay_data);
    memset(snapshot, 0, sizeof(*snapshot));
}

bool app_build_dungeon_snapshot(app_dungeon_snapshot* snapshot,
    u64b revision, const app_wait_state* wait_state,
    const app_interaction_state* interaction,
    const app_ui_scene* transient_scene, u32b update_mask, u32b redraw_mask,
    u32b window_mask)
{
    u16b snapshot_flags = 0;

    if (!snapshot || !platform_frame_main_view_ready() || !p_ptr || !op_ptr
        || !character_generated)
        return false;

    if (!app_build_map_blob(snapshot, wait_state)
        || !app_build_panes_blob(snapshot))
    {
        return false;
    }

    if (!app_build_overlay_blob(snapshot, interaction, transient_scene))
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
    snapshot->blobs[1].data = snapshot->panes_data;
    snapshot->blobs[1].size = snapshot->panes_size;
    snapshot->blobs[2].data = snapshot->overlay_data;
    snapshot->blobs[2].size = snapshot->overlay_size;

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
        mask |= APP_SNAPSHOT_INVALIDATE_OVERLAY;
    }

    (void)window_mask;

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
