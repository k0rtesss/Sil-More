#include "angband.h"

#include "app/app-interaction.h"
#include "app/app-scene-dungeon.h"
#include "app/app-wire.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WINDOWS
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static bool ensure_directory_tree(const char* path)
{
    char buffer[1024];
    size_t i;

    if (!path || !path[0])
        return false;

    SDL_strlcpy(buffer, path, sizeof(buffer));
    for (i = 0; buffer[i]; i++)
    {
        if (buffer[i] != '/' && buffer[i] != '\\')
            continue;
        if (i == 0)
            continue;

        buffer[i] = '\0';
#ifdef WINDOWS
        if (_mkdir(buffer) != 0 && errno != EEXIST)
            return false;
#else
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST)
            return false;
#endif
        buffer[i] = PATH_SEP[0];
    }

#ifdef WINDOWS
    if (_mkdir(buffer) != 0 && errno != EEXIST)
        return false;
#else
    if (mkdir(buffer, 0755) != 0 && errno != EEXIST)
        return false;
#endif
    return true;
}

static bool write_binary_file(const char* path, const void* data, size_t size)
{
    FILE* file;

    if (!path || !data)
        return false;

    file = fopen(path, "wb");
    if (!file)
        return false;

    if (size && fwrite(data, 1, size, file) != size)
    {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

static byte* build_map_blob(size_t* out_size)
{
    static const char* rows[] = {
        "#########",
        "#.......#",
        "#..@o...#",
        "#...m...#",
        "#....x..#",
        "#########"
    };
    const size_t width = 9;
    const size_t height = N_ELEMENTS(rows);
    const size_t cell_count = width * height;
    const size_t size = sizeof(app_map_snapshot)
        + (cell_count * sizeof(app_map_cell_snapshot));
    app_map_snapshot* map = calloc(1, size);
    size_t index = 0;
    size_t y;
    size_t x;

    if (!map)
        return NULL;

    map->format_version = APP_DUNGEON_MAP_FORMAT_VERSION;
    map->flags = APP_DUNGEON_SNAPSHOT_FLAG_WAITING;
    map->width = (u16b)width;
    map->height = (u16b)height;
    map->panel_y = 0;
    map->panel_x = 0;
    map->player_y = 2;
    map->player_x = 3;
    map->cursor.visible = 1;
    map->cursor.relative = 1;
    map->cursor.map_y = 4;
    map->cursor.map_x = 5;
    map->target.active = 1;
    map->target.who = 17;
    map->target.map_y = 3;
    map->target.map_x = 4;
    map->cell_count = (u32b)cell_count;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++, index++)
        {
            app_map_cell_snapshot* cell = &map->cells[index];
            char ch = rows[y][x];

            memset(cell, 0, sizeof(*cell));
            cell->map_y = (s16b)y;
            cell->map_x = (s16b)x;
            cell->terrain_char = (ch == '#') ? '#' : '.';
            cell->terrain_attr = (ch == '#') ? TERM_SLATE : TERM_L_DARK;
            cell->attr = cell->terrain_attr;
            cell->ch = ch;

            if (ch == '#')
                cell->flags |= APP_MAP_CELL_FLAG_WALL;
            if (ch == '@')
            {
                cell->attr = TERM_WHITE;
                cell->m_idx = -1;
                cell->flags |= APP_MAP_CELL_FLAG_PLAYER;
            }
            else if (ch == 'o')
            {
                cell->attr = TERM_YELLOW;
                cell->o_idx = 3;
                cell->flags |= APP_MAP_CELL_FLAG_OBJECT;
            }
            else if (ch == 'm')
            {
                cell->attr = TERM_L_RED;
                cell->m_idx = 17;
                cell->flags |= APP_MAP_CELL_FLAG_MONSTER
                    | APP_MAP_CELL_FLAG_TARGET;
            }
            else if (ch == 'x')
            {
                cell->attr = TERM_L_BLUE;
                cell->ch = '.';
                cell->flags |= APP_MAP_CELL_FLAG_CURSOR;
            }
        }
    }

    *out_size = size;
    return (byte*)map;
}

static byte* build_panes_blob(bool resolved, size_t* out_size)
{
    app_panes_snapshot* panes = calloc(1, sizeof(*panes));

    if (!panes)
        return NULL;

    panes->format_version = APP_DUNGEON_PANES_FORMAT_VERSION;
    panes->main_combat_roll_lines = resolved ? 0 : 1;

    if (!resolved)
    {
        app_combat_roll_snapshot* entry = &panes->combat_entries[0];

        panes->combat_entry_count = 1;
        entry->round = 1;
        entry->index = 0;
        entry->att_type = 12;
        entry->dam_type = 4;
        entry->attacker_char = '@';
        entry->attacker_attr = TERM_WHITE;
        entry->defender_char = 'o';
        entry->defender_attr = TERM_L_RED;
        entry->is_attacker_player = 1;
        entry->att = 12;
        entry->att_roll = 9;
        entry->evn = 8;
        entry->evn_roll = 4;
        entry->dd = 1;
        entry->ds = 8;
        entry->dam = 5;
        entry->pd = 1;
        entry->ps = 4;
        entry->prot = 1;
        entry->prt_percent = 10;
        entry->melee = 1;
    }

    *out_size = sizeof(*panes);
    return (byte*)panes;
}

static void build_demo_interaction(app_interaction_state* interaction,
    bool resolved)
{
    if (!interaction)
        return;

    memset(interaction, 0, sizeof(*interaction));
    interaction->format_version = APP_INTERACTION_FORMAT_VERSION;
    interaction->selected_index = -1;

    if (resolved)
        return;

    interaction->kind = APP_INTERACTION_KIND_LIST;
    interaction->reason = APP_WAIT_REASON_LIST_SELECTION;
    interaction->flags = APP_INTERACTION_FLAG_CAN_CONFIRM
        | APP_INTERACTION_FLAG_CAN_CANCEL
        | APP_INTERACTION_FLAG_SHOW_OPTIONS
        | APP_INTERACTION_FLAG_SHOW_VALUE;
    interaction->prompt_attr = TERM_WHITE;
    interaction->detail_attr = TERM_SLATE;
    interaction->value_attr = TERM_YELLOW;
    interaction->selected_index = 1;
    interaction->cursor_index = 1;
    interaction->option_count = 3;
    SDL_strlcpy(interaction->prompt, "Choose ammunition",
        sizeof(interaction->prompt));
    SDL_strlcpy(interaction->detail,
        "Enter confirms. Esc cancels. Press a, b, or c.",
        sizeof(interaction->detail));
    SDL_strlcpy(interaction->value, "2/7",
        sizeof(interaction->value));

    interaction->options[0].attr = TERM_WHITE;
    interaction->options[0].tag = 'a';
    interaction->options[0].enabled = 1;
    SDL_strlcpy(interaction->options[0].key, "a",
        sizeof(interaction->options[0].key));
    SDL_strlcpy(interaction->options[0].label, "Ashen Arrow",
        sizeof(interaction->options[0].label));
    SDL_strlcpy(interaction->options[0].meta, "1.0 lb",
        sizeof(interaction->options[0].meta));

    interaction->options[1].attr = TERM_L_BLUE;
    interaction->options[1].tag = 'b';
    interaction->options[1].enabled = 1;
    interaction->options[1].selected = 1;
    interaction->options[1].flags = APP_INTERACTION_ENTRY_FLAG_SELECTED;
    SDL_strlcpy(interaction->options[1].key, "b",
        sizeof(interaction->options[1].key));
    SDL_strlcpy(interaction->options[1].label, "Broadhead Arrow",
        sizeof(interaction->options[1].label));
    SDL_strlcpy(interaction->options[1].meta, "1.3 lb",
        sizeof(interaction->options[1].meta));

    interaction->options[2].attr = TERM_SLATE;
    interaction->options[2].tag = 'c';
    interaction->options[2].enabled = 1;
    SDL_strlcpy(interaction->options[2].key, "c",
        sizeof(interaction->options[2].key));
    SDL_strlcpy(interaction->options[2].label, "Cancel",
        sizeof(interaction->options[2].label));
    SDL_strlcpy(interaction->options[2].meta, "",
        sizeof(interaction->options[2].meta));
}

static bool build_demo_chrome_scene(app_ui_scene* scene, const char* topline,
    const char** lines, size_t line_count)
{
    app_ui_panel* panel;
    size_t i;

    if (!scene)
        return false;

    app_ui_scene_init(scene);

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_CHROME);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_STRIP;
    panel->flags = APP_UI_PANEL_FLAG_ACTIVE | APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    if (topline && topline[0]
        && !app_ui_panel_add_body_line(panel, TERM_YELLOW, topline))
    {
        return false;
    }

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_CHROME);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->flags = APP_UI_PANEL_FLAG_ACTIVE | APP_UI_PANEL_FLAG_LEFT_ANCHORED;

    for (i = 0; i < line_count; i++)
    {
        if (!lines[i] || !lines[i][0])
            continue;
        if (!app_ui_panel_add_body_line(panel,
                (i == 0) ? TERM_WHITE : TERM_SLATE, lines[i]))
        {
            return false;
        }
    }

    return true;
}

static byte* build_overlay_blob(bool resolved, const char* topline,
    const char** lines, size_t line_count, size_t* out_size)
{
    app_dungeon_overlay_snapshot* overlay = calloc(1, sizeof(*overlay));

    if (!overlay)
        return NULL;

    overlay->format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    build_demo_interaction(&overlay->interaction, resolved);
    app_ui_scene_init(&overlay->transient_scene);
    if (!build_demo_chrome_scene(&overlay->chrome_scene, topline, lines,
            line_count))
    {
        free(overlay);
        return NULL;
    }

    *out_size = sizeof(*overlay);
    return (byte*)overlay;
}

static byte* build_events_blob(bool resolved, size_t* out_size)
{
    app_event_record records[2];
    app_event_span span;
    size_t packet_size;
    byte* packet = NULL;

    memset(records, 0, sizeof(records));
    records[0].kind = APP_EVENT_KIND_WAIT_STATE;
    records[0].scope = APP_EVENT_SCOPE_SESSION;
    records[0].payload_size = sizeof(records[0]);
    records[0].sequence = 1;
    records[0].subject = resolved ? APP_WAIT_REASON_NONE
        : APP_WAIT_REASON_LIST_SELECTION;
    records[0].arg0 = resolved ? APP_SESSION_STATE_RUNNING
        : APP_SESSION_STATE_WAITING;
    records[1].kind = resolved ? APP_EVENT_KIND_MESSAGE
        : APP_EVENT_KIND_PROJECTILE;
    records[1].scope = APP_EVENT_SCOPE_SCENE;
    records[1].payload_size = sizeof(records[1]);
    records[1].sequence = 2;
    records[1].subject = resolved ? MSG_GENERIC : APP_DUNGEON_PLAYER_SUBJECT;
    records[1].arg0 = resolved ? 1 : APP_PACK_COORD(2, 3);
    records[1].arg1 = resolved ? 0 : APP_PACK_COORD(3, 4);

    span.records = records;
    span.count = N_ELEMENTS(records);
    span.dropped_count = 0;

    packet_size = app_wire_event_packet_size(&span);
    packet = calloc(1, packet_size);
    if (!packet)
        return NULL;

    if (!app_wire_serialize_event_packet(&span, packet, packet_size, out_size))
    {
        free(packet);
        return NULL;
    }

    return packet;
}

static byte* build_session_blob(bool resolved, size_t* out_size)
{
    app_session_export state;
    size_t packet_size;
    byte* packet = NULL;

    memset(&state, 0, sizeof(state));
    state.api_version = APP_SESSION_API_VERSION;
    state.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
        | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
        | APP_SESSION_FLAG_EXTERNAL_DRIVE;
    state.state = resolved ? APP_SESSION_STATE_RUNNING
        : APP_SESSION_STATE_WAITING;
    state.wait_reason = resolved ? APP_WAIT_REASON_NONE
        : APP_WAIT_REASON_LIST_SELECTION;
    state.snapshot_scene = APP_SCENE_KIND_DUNGEON;
    state.snapshot_flags = resolved ? APP_SNAPSHOT_FLAG_PARTIAL
        : (APP_SNAPSHOT_FLAG_PARTIAL | APP_SNAPSHOT_FLAG_WAITING);
    state.snapshot_blob_count = 3;
    state.pending_event_count = 2;
    state.snapshot_revision = resolved ? 8 : 7;
    state.counters.emitted_events = 2;

    packet_size = app_wire_session_packet_size();
    packet = calloc(1, packet_size);
    if (!packet)
        return NULL;

    if (!app_wire_serialize_session_packet(&state, packet, packet_size,
            out_size))
    {
        free(packet);
        return NULL;
    }

    return packet;
}

static byte* build_snapshot_blob(bool resolved, size_t* out_size)
{
    static const char* open_lines[] = {
        "You enter a ruined corridor.",
        "An orc scout steps into view.",
        "Select the arrow to loose."
    };
    static const char* resolved_lines[] = {
        "You enter a ruined corridor.",
        "You ready a broadhead arrow.",
        "The orc scout is still in sight."
    };
    app_snapshot snapshot;
    app_snapshot_blob blobs[3];
    byte* map_data = NULL;
    byte* panes_data = NULL;
    byte* overlay_data = NULL;
    size_t map_size = 0;
    size_t panes_size = 0;
    size_t overlay_size = 0;
    size_t packet_size;
    byte* packet = NULL;
    const char* topline = resolved ? "You ready a broadhead arrow."
        : "Choose an arrow to fire.";
    const char** lines = resolved ? resolved_lines : open_lines;

    memset(&snapshot, 0, sizeof(snapshot));
    memset(blobs, 0, sizeof(blobs));

    map_data = build_map_blob(&map_size);
    panes_data = build_panes_blob(resolved, &panes_size);
    overlay_data = build_overlay_blob(resolved, topline, lines, 3,
        &overlay_size);
    if (!map_data || !panes_data || !overlay_data)
        goto cleanup;

    blobs[0].kind = APP_SNAPSHOT_BLOB_MAP;
    blobs[0].format_version = APP_DUNGEON_MAP_FORMAT_VERSION;
    blobs[0].data = map_data;
    blobs[0].size = map_size;
    blobs[1].kind = APP_SNAPSHOT_BLOB_PANES;
    blobs[1].format_version = APP_DUNGEON_PANES_FORMAT_VERSION;
    blobs[1].data = panes_data;
    blobs[1].size = panes_size;
    blobs[2].kind = APP_SNAPSHOT_BLOB_OVERLAY;
    blobs[2].format_version = APP_DUNGEON_OVERLAY_FORMAT_VERSION;
    blobs[2].data = overlay_data;
    blobs[2].size = overlay_size;

    snapshot.revision = resolved ? 8 : 7;
    snapshot.scene = APP_SCENE_KIND_DUNGEON;
    snapshot.flags = APP_SNAPSHOT_FLAG_PARTIAL;
    if (!resolved)
        snapshot.flags |= APP_SNAPSHOT_FLAG_WAITING;
    snapshot.blobs = blobs;
    snapshot.blob_count = N_ELEMENTS(blobs);

    packet_size = app_wire_snapshot_packet_size(&snapshot);
    packet = calloc(1, packet_size);
    if (!packet)
        goto cleanup;

    if (!app_wire_serialize_snapshot_packet(&snapshot, packet, packet_size,
            out_size))
    {
        free(packet);
        packet = NULL;
    }

cleanup:
    free(map_data);
    free(panes_data);
    free(overlay_data);
    return packet;
}

static bool emit_layout_json(const char* path)
{
    FILE* file = fopen(path, "w");

    if (!file)
        return false;

    fprintf(file, "{\n");
    fprintf(file,
        "  \"map\": {\n"
        "    \"size\": %u,\n"
        "    \"cellsOffset\": %u,\n"
        "    \"width\": %u,\n"
        "    \"height\": %u,\n"
        "    \"cursor\": {\n"
        "      \"visible\": %u,\n"
        "      \"relative\": %u,\n"
        "      \"mapY\": %u,\n"
        "      \"mapX\": %u\n"
        "    },\n"
        "    \"target\": {\n"
        "      \"active\": %u,\n"
        "      \"mapY\": %u,\n"
        "      \"mapX\": %u\n"
        "    }\n"
        "  },\n",
        (unsigned)sizeof(app_map_snapshot),
        (unsigned)offsetof(app_map_snapshot, cells),
        (unsigned)offsetof(app_map_snapshot, width),
        (unsigned)offsetof(app_map_snapshot, height),
        (unsigned)(offsetof(app_map_snapshot, cursor)
            + offsetof(app_cursor_snapshot, visible)),
        (unsigned)(offsetof(app_map_snapshot, cursor)
            + offsetof(app_cursor_snapshot, relative)),
        (unsigned)(offsetof(app_map_snapshot, cursor)
            + offsetof(app_cursor_snapshot, map_y)),
        (unsigned)(offsetof(app_map_snapshot, cursor)
            + offsetof(app_cursor_snapshot, map_x)),
        (unsigned)(offsetof(app_map_snapshot, target)
            + offsetof(app_target_snapshot, active)),
        (unsigned)(offsetof(app_map_snapshot, target)
            + offsetof(app_target_snapshot, map_y)),
        (unsigned)(offsetof(app_map_snapshot, target)
            + offsetof(app_target_snapshot, map_x)));
    fprintf(file,
        "  \"mapCell\": {\n"
        "    \"size\": %u,\n"
        "    \"mapY\": %u,\n"
        "    \"mapX\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"attr\": %u,\n"
        "    \"ch\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_map_cell_snapshot),
        (unsigned)offsetof(app_map_cell_snapshot, map_y),
        (unsigned)offsetof(app_map_cell_snapshot, map_x),
        (unsigned)offsetof(app_map_cell_snapshot, flags),
        (unsigned)offsetof(app_map_cell_snapshot, attr),
        (unsigned)offsetof(app_map_cell_snapshot, ch));
    fprintf(file,
        "  \"panes\": {\n"
        "    \"size\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"combatEntryCount\": %u,\n"
        "    \"mainCombatRollLines\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_panes_snapshot),
        (unsigned)offsetof(app_panes_snapshot, flags),
        (unsigned)offsetof(app_panes_snapshot, combat_entry_count),
        (unsigned)offsetof(app_panes_snapshot, main_combat_roll_lines));
    fprintf(file,
        "  \"overlay\": {\n"
        "    \"size\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"interactionOffset\": %u,\n"
        "    \"chromeSceneOffset\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_dungeon_overlay_snapshot),
        (unsigned)offsetof(app_dungeon_overlay_snapshot, flags),
        (unsigned)offsetof(app_dungeon_overlay_snapshot, interaction),
        (unsigned)offsetof(app_dungeon_overlay_snapshot, chrome_scene));
    fprintf(file,
        "  \"interaction\": {\n"
        "    \"size\": %u,\n"
        "    \"kind\": %u,\n"
        "    \"reason\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"selectedIndex\": %u,\n"
        "    \"cursorIndex\": %u,\n"
        "    \"optionCount\": %u,\n"
        "    \"prompt\": %u,\n"
        "    \"detail\": %u,\n"
        "    \"value\": %u,\n"
        "    \"options\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_interaction_state),
        (unsigned)offsetof(app_interaction_state, kind),
        (unsigned)offsetof(app_interaction_state, reason),
        (unsigned)offsetof(app_interaction_state, flags),
        (unsigned)offsetof(app_interaction_state, selected_index),
        (unsigned)offsetof(app_interaction_state, cursor_index),
        (unsigned)offsetof(app_interaction_state, option_count),
        (unsigned)offsetof(app_interaction_state, prompt),
        (unsigned)offsetof(app_interaction_state, detail),
        (unsigned)offsetof(app_interaction_state, value),
        (unsigned)offsetof(app_interaction_state, options));
    fprintf(file,
        "  \"interactionOption\": {\n"
        "    \"size\": %u,\n"
        "    \"attr\": %u,\n"
        "    \"tag\": %u,\n"
        "    \"enabled\": %u,\n"
        "    \"selected\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"key\": %u,\n"
        "    \"label\": %u,\n"
        "    \"meta\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_interaction_option),
        (unsigned)offsetof(app_interaction_option, attr),
        (unsigned)offsetof(app_interaction_option, tag),
        (unsigned)offsetof(app_interaction_option, enabled),
        (unsigned)offsetof(app_interaction_option, selected),
        (unsigned)offsetof(app_interaction_option, flags),
        (unsigned)offsetof(app_interaction_option, key),
        (unsigned)offsetof(app_interaction_option, label),
        (unsigned)offsetof(app_interaction_option, meta));
    fprintf(file,
        "  \"uiScene\": {\n"
        "    \"size\": %u,\n"
        "    \"panelCount\": %u,\n"
        "    \"panels\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_ui_scene),
        (unsigned)offsetof(app_ui_scene, panel_count),
        (unsigned)offsetof(app_ui_scene, panels));
    fprintf(file,
        "  \"uiPanel\": {\n"
        "    \"size\": %u,\n"
        "    \"style\": %u,\n"
        "    \"flags\": %u,\n"
        "    \"bodyLineCount\": %u,\n"
        "    \"bodyLines\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_ui_panel),
        (unsigned)offsetof(app_ui_panel, style),
        (unsigned)offsetof(app_ui_panel, flags),
        (unsigned)offsetof(app_ui_panel, body_line_count),
        (unsigned)offsetof(app_ui_panel, body_lines));
    fprintf(file,
        "  \"uiTextLine\": {\n"
        "    \"size\": %u,\n"
        "    \"attr\": %u,\n"
        "    \"text\": %u\n"
        "  },\n",
        (unsigned)sizeof(app_ui_text_line),
        (unsigned)offsetof(app_ui_text_line, attr),
        (unsigned)offsetof(app_ui_text_line, text));
    fprintf(file,
        "  \"constants\": {\n"
        "    \"blobKinds\": {\n"
        "      \"map\": %u,\n"
        "      \"panes\": %u,\n"
        "      \"overlay\": %u\n"
        "    },\n"
        "    \"mapFlags\": {\n"
        "      \"player\": %u,\n"
        "      \"monster\": %u,\n"
        "      \"object\": %u,\n"
        "      \"target\": %u,\n"
        "      \"cursor\": %u\n"
        "    }\n"
        "  }\n"
        "}\n",
        (unsigned)APP_SNAPSHOT_BLOB_MAP,
        (unsigned)APP_SNAPSHOT_BLOB_PANES,
        (unsigned)APP_SNAPSHOT_BLOB_OVERLAY,
        (unsigned)APP_MAP_CELL_FLAG_PLAYER,
        (unsigned)APP_MAP_CELL_FLAG_MONSTER,
        (unsigned)APP_MAP_CELL_FLAG_OBJECT,
        (unsigned)APP_MAP_CELL_FLAG_TARGET,
        (unsigned)APP_MAP_CELL_FLAG_CURSOR);

    fclose(file);
    return true;
}

static bool emit_state_files(const char* output_dir, const char* stem,
    bool resolved)
{
    char path[1024];
    byte* snapshot_packet = NULL;
    byte* event_packet = NULL;
    byte* session_packet = NULL;
    size_t snapshot_size = 0;
    size_t event_size = 0;
    size_t session_size = 0;
    bool ok = false;

    snapshot_packet = build_snapshot_blob(resolved, &snapshot_size);
    event_packet = build_events_blob(resolved, &event_size);
    session_packet = build_session_blob(resolved, &session_size);
    if (!snapshot_packet || !event_packet || !session_packet)
        goto cleanup;

    strnfmt(path, sizeof(path), "%s/%s.snapshot.bin", output_dir, stem);
    if (!write_binary_file(path, snapshot_packet, snapshot_size))
        goto cleanup;
    strnfmt(path, sizeof(path), "%s/%s.events.bin", output_dir, stem);
    if (!write_binary_file(path, event_packet, event_size))
        goto cleanup;
    strnfmt(path, sizeof(path), "%s/%s.session.bin", output_dir, stem);
    if (!write_binary_file(path, session_packet, session_size))
        goto cleanup;

    ok = true;

cleanup:
    free(snapshot_packet);
    free(event_packet);
    free(session_packet);
    return ok;
}

int main(int argc, char* argv[])
{
    const char* output_dir = (argc > 1) ? argv[1] : "web/ui8-demo/samples";
    char layout_path[1024];

    if (!ensure_directory_tree(output_dir))
    {
        fprintf(stderr, "Unable to create output directory: %s\n", output_dir);
        return EXIT_FAILURE;
    }

    if (!emit_state_files(output_dir, "choice-open", false)
        || !emit_state_files(output_dir, "choice-picked", true))
    {
        fprintf(stderr, "Unable to emit demo packet files.\n");
        return EXIT_FAILURE;
    }

    strnfmt(layout_path, sizeof(layout_path), "%s/layout.json", output_dir);
    if (!emit_layout_json(layout_path))
    {
        fprintf(stderr, "Unable to emit layout file: %s\n", layout_path);
        return EXIT_FAILURE;
    }

    printf("UI8 demo packets emitted to %s\n", output_dir);
    return EXIT_SUCCESS;
}
