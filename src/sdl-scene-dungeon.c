#include "angband.h"

#include "sdl-main-internal.h"

typedef struct sdl_scene_layout {
    bool compact_height;
    bool hide_left_panel;
    int row_map;
    int col_map;
    int bottom_row;
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
} sdl_scene_layout;

static SDL_Color sdl_scene_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static byte sdl_scene_alpha_byte(float alpha)
{
    if (alpha <= 0.0f)
        return 0;
    if (alpha >= 1.0f)
        return 255;
    return (byte)(alpha * 255.0f + 0.5f);
}

static float sdl_scene_progress(Uint64 now_ns, const sdl_scene_animation* anim)
{
    Uint64 elapsed_ns;

    if (!anim || !anim->active || anim->duration_ns == 0)
        return 1.0f;

    if (now_ns <= anim->started_ns)
        return 0.0f;

    elapsed_ns = now_ns - anim->started_ns;
    if (elapsed_ns >= anim->duration_ns)
        return 1.0f;

    return (float)elapsed_ns / (float)anim->duration_ns;
}

static void sdl_scene_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

static void sdl_scene_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static bool sdl_scene_layout_cell_rect(const sdl_view* view, int col, int row,
    int width_cells, SDL_FRect* out_rect)
{
    if (!view || !out_rect || col < 0 || row < 0 || width_cells <= 0)
        return false;
    if (col >= view->cols || row >= view->rows)
        return false;
    if (col + width_cells > view->cols)
        width_cells = view->cols - col;
    if (width_cells <= 0)
        return false;

    *out_rect = (SDL_FRect){
        .x = (float)(col * view->cell_w),
        .y = (float)(row * view->cell_h),
        .w = (float)(width_cells * view->cell_w),
        .h = (float)view->cell_h
    };
    return true;
}

static void sdl_scene_copy_legacy_left_panel(SDL_Texture* canvas,
    const sdl_view* view, const sdl_scene_layout* layout)
{
    SDL_FRect dst;
    SDL_FRect src;
    float width_px;
    float height_px;

    if (!canvas || !view || !layout || layout->hide_left_panel)
        return;
    if (!view->canvas || view->canvas == canvas)
        return;
    if (layout->col_map <= 0 || view->cell_w <= 0 || view->cell_h <= 0)
        return;

    width_px = (float)(layout->col_map * view->cell_w);
    height_px = (float)(view->rows * view->cell_h);
    if (width_px <= 0.0f || height_px <= 0.0f)
        return;

    src = (SDL_FRect){
        .x = 0.0f,
        .y = 0.0f,
        .w = width_px,
        .h = height_px
    };
    dst = src;

    /*
     * The snapshot renderer currently synthesizes a simplified left status area.
     * Reuse the legacy main-term strip instead so the visible panel matches the
     * classic develop output while the scene renderer continues to own the map.
     */
    SDL_RenderTexture(g_state.renderer, view->canvas, &src, &dst);
}

static void sdl_scene_draw_text(const sdl_view* view, int col, int row,
    byte attr, cptr text)
{
    size_t len;

    if (!view || !text || !text[0])
        return;
    if (row < 0 || row >= view->rows || col >= view->cols)
        return;
    if (col < 0)
        col = 0;

    len = strlen(text);
    if (len == 0)
        return;
    if ((size_t)col + len > (size_t)view->cols)
        len = (size_t)(view->cols - col);
    if (len == 0)
        return;

    sdl_render_mono_text((sdl_view*)view, col, row, (int)len, text,
        sdl_scene_color(attr));
}

static void sdl_scene_draw_text_snapshot(const sdl_view* view, int col, int row,
    const app_text_snapshot* text)
{
    if (!text || !text->active || !text->text[0])
        return;

    sdl_scene_draw_text(view, col, row, text->attr, text->text);
}

static sdl_scene_layout sdl_scene_make_layout(const sdl_view* view,
    u16b status_flags)
{
    sdl_scene_layout layout;

    memset(&layout, 0, sizeof(layout));
    layout.compact_height = view ? (view->rows < 24) : false;
    layout.hide_left_panel =
        (status_flags & APP_DUNGEON_SNAPSHOT_FLAG_HIDE_LEFT_PANEL) ? true : false;
    layout.row_map = 1;
    layout.col_map = layout.hide_left_panel ? 0 : 13;
    layout.bottom_row = view ? (view->rows - 1) : 0;
    layout.row_name = 1;
    layout.row_stat = 3;
    layout.row_exp = layout.compact_height ? 7 : 8;
    layout.row_hp = layout.compact_height ? 8 : 9;
    layout.row_sp = layout.compact_height ? 9 : 10;
    layout.row_light = layout.compact_height ? 10 : 11;
    layout.row_mel = layout.compact_height ? 11 : 13;
    layout.row_arc = layout.compact_height ? 12 : 14;
    layout.row_quiver = layout.compact_height ? 13 : 15;
    layout.row_evn = layout.compact_height ? 14 : 16;
    layout.row_info = layout.compact_height ? 15 : 17;
    layout.row_cut = layout.compact_height ? 17 : 20;
    layout.row_song = layout.compact_height ? 18 : 21;
    layout.col_hungry = 0;
    layout.col_blind = 9;
    layout.col_confused = 15;
    layout.col_stun = 24;
    layout.col_afraid = 36;
    layout.col_state = 43;
    layout.col_speed = 56;
    layout.col_terrain = 61;
    layout.col_depth = 72;
    return layout;
}

static const app_snapshot_blob* sdl_scene_find_blob(
    const app_dungeon_snapshot* snapshot, u16b kind)
{
    size_t i;

    if (!snapshot)
        return NULL;

    for (i = 0; i < snapshot->snapshot.blob_count; i++)
    {
        const app_snapshot_blob* blob = &snapshot->blobs[i];

        if (blob->kind == kind && blob->data && blob->size)
            return blob;
    }

    return NULL;
}

static const app_map_snapshot* sdl_scene_map_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_MAP);

    if (!blob || blob->size < sizeof(app_map_snapshot))
        return NULL;
    return (const app_map_snapshot*)blob->data;
}

static const app_status_snapshot* sdl_scene_status_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_STATUS);

    if (!blob || blob->size < sizeof(app_status_snapshot))
        return NULL;
    return (const app_status_snapshot*)blob->data;
}

static const app_messages_snapshot* sdl_scene_messages_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_MESSAGES);

    if (!blob || blob->size < sizeof(app_messages_snapshot))
        return NULL;
    return (const app_messages_snapshot*)blob->data;
}

static const app_panes_snapshot* sdl_scene_panes_snapshot(
    const app_dungeon_snapshot* snapshot)
{
    const app_snapshot_blob* blob = sdl_scene_find_blob(snapshot,
        APP_SNAPSHOT_BLOB_PANES);

    if (!blob || blob->size < sizeof(app_panes_snapshot))
        return NULL;
    return (const app_panes_snapshot*)blob->data;
}

static bool sdl_scene_map_to_screen(const sdl_scene_layout* layout,
    const app_map_snapshot* map, int map_y, int map_x, int* out_row,
    int* out_col, int* out_width_cells)
{
    if (!layout || !map)
        return false;
    if (map_y < map->panel_y || map_y >= map->panel_y + map->height)
        return false;
    if (map_x < map->panel_x || map_x >= map->panel_x + map->width)
        return false;

    if (out_row)
        *out_row = layout->row_map + (map_y - map->panel_y);
    if (out_col)
        *out_col = layout->col_map
            + (map_x - map->panel_x)
                * ((use_bigtile && !graphics_are_ascii()) ? 2 : 1);
    if (out_width_cells)
        *out_width_cells = (use_bigtile && !graphics_are_ascii()) ? 2 : 1;
    return true;
}

static const app_map_cell_snapshot* sdl_scene_find_cell_by_subject(
    const app_map_snapshot* map, s32b subject)
{
    size_t i;

    if (!map)
        return NULL;

    for (i = 0; i < map->cell_count; i++)
    {
        const app_map_cell_snapshot* cell = &map->cells[i];

        if ((subject == APP_DUNGEON_PLAYER_SUBJECT
                && (cell->flags & APP_MAP_CELL_FLAG_PLAYER))
            || (subject > 0 && cell->m_idx == subject))
        {
            return cell;
        }
    }

    return NULL;
}

static void sdl_scene_draw_tile(SDL_Texture* tileset, byte attr, byte ch,
    const SDL_FRect* dst)
{
    SDL_FRect src;

    if (!tileset || !dst)
        return;
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    src.x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE);
    src.y = (float)(TILE_GET_INDEX(attr) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    SDL_RenderTexture(g_state.renderer, tileset, &src, dst);
}

static void sdl_scene_draw_misc_icon(int icon, const SDL_FRect* dst)
{
    byte attr = misc_to_attr[icon];
    byte ch = (byte)misc_to_char[icon];

    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    sdl_scene_draw_tile(g_state.tileset, attr, ch, dst);
}

static void sdl_scene_draw_map_cell(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const app_map_cell_snapshot* cell)
{
    int row;
    int col;
    int width_cells;
    SDL_FRect dst;
    char glyph[2];
    byte terrain_ch = (byte)cell->terrain_char;
    byte ch = (byte)cell->ch;

    if (!view || !layout || !map || !cell)
        return;
    if (!sdl_scene_map_to_screen(layout, map, cell->map_y, cell->map_x, &row,
            &col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_layout_cell_rect(view, col, row, width_cells, &dst))
        return;

    sdl_scene_fill_rect(&dst, (SDL_Color){ 0, 0, 0, 255 });

    if (g_state.use_tiles && g_state.tileset
        && (cell->terrain_attr & TILE_FLAG) && (terrain_ch & TILE_FLAG)
        && (cell->attr & TILE_FLAG) && (ch & TILE_FLAG))
    {
        sdl_scene_draw_tile(g_state.tileset, cell->terrain_attr, terrain_ch, &dst);
        if (cell->attr & GRAPHICS_GLOW_MASK)
            sdl_scene_draw_misc_icon(ICON_GLOW, &dst);
        sdl_scene_draw_tile(g_state.tileset, cell->attr, ch, &dst);
        if (cell->terrain_attr & GRAPHICS_SLEEP_MASK)
            sdl_scene_draw_misc_icon(ICON_SLEEPING, &dst);
        if (terrain_ch & GRAPHICS_SEEN_MASK)
            sdl_scene_draw_misc_icon(ICON_MONSTER_SEES_PLAYER, &dst);
        if (ch & GRAPHICS_ALERT_MASK)
            sdl_scene_draw_misc_icon(ICON_ALERT, &dst);
    }
    else
    {
        glyph[0] = cell->ch ? cell->ch : ' ';
        glyph[1] = '\0';
        sdl_render_mono_text((sdl_view*)view, col, row, 1, glyph,
            sdl_scene_color(cell->attr));
        if (use_bigtile && !graphics_are_ascii() && (col + 1) < view->cols)
            sdl_scene_draw_text(view, col + 1, row, TERM_WHITE, " ");
    }

    if (cell->flags & APP_MAP_CELL_FLAG_TARGET)
    {
        SDL_Color target_color = sdl_scene_color(TERM_L_RED);
        target_color.a = 220;
        sdl_scene_draw_rect(&dst, target_color);
    }

    if (cell->flags & APP_MAP_CELL_FLAG_CURSOR)
    {
        SDL_Color cursor_color = sdl_scene_color(TERM_L_BLUE);
        cursor_color.a = 220;
        sdl_scene_draw_rect(&dst, cursor_color);
    }
}

static void sdl_scene_draw_absolute_cursor(const sdl_view* view,
    const app_cursor_snapshot* cursor)
{
    SDL_FRect rect;
    SDL_Color color = sdl_scene_color(TERM_L_BLUE);

    if (!view || !cursor || !cursor->visible || cursor->relative)
        return;
    if (!sdl_scene_layout_cell_rect(view, cursor->col, cursor->row, 1, &rect))
        return;

    color.a = 220;
    sdl_scene_draw_rect(&rect, color);
}

static void sdl_scene_render_messages(const sdl_view* view,
    const app_messages_snapshot* messages)
{
    if (!view || !messages)
        return;

    if (messages->top_line_active && messages->top_line[0])
        sdl_scene_draw_text(view, 0, 0, messages->top_line_color,
            messages->top_line);
    else if (messages->line_count > 0 && messages->lines[0].text[0])
        sdl_scene_draw_text(view, 0, 0, messages->lines[0].color,
            messages->lines[0].text);
}

static void sdl_scene_render_status_panel(const sdl_view* view,
    const sdl_scene_layout* layout, const app_status_snapshot* status)
{
    char buf[64];

    if (!view || !layout || !status || layout->hide_left_panel)
        return;

    sdl_scene_draw_text(view, 0, layout->row_name, TERM_WHITE,
        status->player_name);

    strnfmt(buf, sizeof(buf), "Str %2d", status->str_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 0, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Dex %2d", status->dex_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 1, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Con %2d", status->con_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 2, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Gra %2d", status->gra_use);
    sdl_scene_draw_text(view, 0, layout->row_stat + 3, TERM_L_GREEN, buf);

    strnfmt(buf, sizeof(buf), "EXP %ld", (long)status->exp);
    sdl_scene_draw_text(view, 0, layout->row_exp, TERM_WHITE, buf);

    strnfmt(buf, sizeof(buf), "HP %d/%d", status->hp_cur, status->hp_max);
    sdl_scene_draw_text(view, 0, layout->row_hp, status->hp_attr, buf);

    strnfmt(buf, sizeof(buf), "Voice %d/%d", status->voice_cur,
        status->voice_max);
    sdl_scene_draw_text(view, 0, layout->row_sp, status->voice_attr, buf);

    sdl_scene_draw_text_snapshot(view, 0, layout->row_mel, &status->melee_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_arc, &status->archery_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_quiver, &status->quiver_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_evn, &status->evasion_text);
    sdl_scene_draw_text_snapshot(view, 0, layout->row_light, &status->light_text);

    if (status->tracked_name_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info,
            &status->tracked_name_text);
    if (status->tracked_health_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info + 1,
            &status->tracked_health_text);
    if (status->tracked_alertness_text.active)
        sdl_scene_draw_text_snapshot(view, 0, layout->row_info + 2,
            &status->tracked_alertness_text);

    if (status->cut_text.active && status->poisoned_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut - 1,
            &status->cut_text);
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut,
            &status->poisoned_text);
    }
    else if (status->cut_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut, &status->cut_text);
    }
    else if (status->poisoned_text.active)
    {
        sdl_scene_draw_text_snapshot(view, 0, layout->row_cut,
            &status->poisoned_text);
    }

    sdl_scene_draw_text_snapshot(view, 0, layout->row_song, &status->song_text);
}

static void sdl_scene_render_hidden_overlay(const sdl_view* view,
    const sdl_scene_layout* layout, const app_panes_snapshot* panes)
{
    u16b i;

    if (!view || !layout || !panes || !layout->hide_left_panel)
        return;

    for (i = 0; i < panes->hidden_overlay_count
        && i < APP_DUNGEON_HIDDEN_OVERLAY_MAX; i++)
    {
        const app_hidden_overlay_line_snapshot* line = &panes->hidden_overlay[i];

        if (!line->text[0])
            continue;
        sdl_scene_draw_text(view, 0, layout->row_name + i, line->attr,
            line->text);
    }
}

static void sdl_scene_render_bottom_line(const sdl_view* view,
    const sdl_scene_layout* layout, const app_status_snapshot* status)
{
    if (!view || !layout || !status)
        return;

    sdl_scene_draw_text_snapshot(view, layout->col_hungry, layout->bottom_row,
        &status->hunger_text);
    sdl_scene_draw_text_snapshot(view, layout->col_blind, layout->bottom_row,
        &status->blind_text);
    sdl_scene_draw_text_snapshot(view, layout->col_confused, layout->bottom_row,
        &status->confused_text);
    sdl_scene_draw_text_snapshot(view, layout->col_stun, layout->bottom_row,
        &status->stun_text);
    sdl_scene_draw_text_snapshot(view, layout->col_afraid, layout->bottom_row,
        &status->afraid_text);
    sdl_scene_draw_text_snapshot(view, layout->col_state, layout->bottom_row,
        &status->state_text);
    sdl_scene_draw_text_snapshot(view, layout->col_speed, layout->bottom_row,
        &status->speed_text);
    sdl_scene_draw_text_snapshot(view, layout->col_terrain, layout->bottom_row,
        &status->terrain_text);
    sdl_scene_draw_text_snapshot(view, layout->col_depth, layout->bottom_row,
        &status->depth_text);
}

static void sdl_scene_format_combat_line(char* buf, size_t buf_size,
    const app_combat_roll_snapshot* entry)
{
    char attacker = entry->attacker_char ? entry->attacker_char : '?';
    char defender = entry->defender_char ? entry->defender_char : '?';

    strnfmt(buf, buf_size, "%c %2d[%2d] vs %c %2d[%2d]  %dd%d -> %d",
        attacker, entry->att, entry->att_roll, defender, entry->evn,
        entry->evn_roll, entry->dd, entry->ds, entry->dam);
}

static void sdl_scene_render_combat_overlay(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const app_panes_snapshot* panes)
{
    int lines;
    int i;
    char buf[80];
    int start_row;

    if (!view || !layout || !map || !panes || panes->main_combat_roll_lines <= 0)
        return;

    lines = panes->main_combat_roll_lines;
    if (lines > 3)
        lines = 3;
    if (lines > panes->combat_entry_count)
        lines = panes->combat_entry_count;

    start_row = layout->row_map + map->height;
    for (i = 0; i < lines && (start_row + i) < layout->bottom_row; i++)
    {
        sdl_scene_format_combat_line(buf, sizeof(buf), &panes->combat_entries[i]);
        sdl_scene_draw_text(view, layout->col_map, start_row + i, TERM_WHITE,
            buf);
    }
}

static void sdl_scene_draw_animation_rect(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map, int map_y,
    int map_x, SDL_Color color)
{
    int row;
    int col;
    int width_cells;
    SDL_FRect rect;

    if (!sdl_scene_map_to_screen(layout, map, map_y, map_x, &row, &col,
            &width_cells))
    {
        return;
    }
    if (!sdl_scene_layout_cell_rect(view, col, row, width_cells, &rect))
        return;

    sdl_scene_fill_rect(&rect, color);
}

static void sdl_scene_render_move_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    int width_cells;
    SDL_Color color = sdl_scene_color((anim->subject == APP_DUNGEON_PLAYER_SUBJECT)
        ? TERM_L_BLUE : TERM_L_WHITE);
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float x;
    float y;

    if (!view || !layout || !map || !anim)
        return;
    if (!sdl_scene_map_to_screen(layout, map, anim->from_y, anim->from_x,
            &from_row, &from_col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_map_to_screen(layout, map, anim->to_y, anim->to_x, &to_row,
            &to_col, NULL))
    {
        return;
    }

    start_x = (float)(from_col * view->cell_w + width_cells * view->cell_w / 2);
    start_y = (float)(from_row * view->cell_h + view->cell_h / 2);
    end_x = (float)(to_col * view->cell_w + width_cells * view->cell_w / 2);
    end_y = (float)(to_row * view->cell_h + view->cell_h / 2);
    x = start_x + (end_x - start_x) * progress;
    y = start_y + (end_y - start_y) * progress;

    color.a = 180;
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, start_x, start_y, end_x, end_y);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = x - view->cell_w * 0.20f,
        .y = y - view->cell_h * 0.20f,
        .w = view->cell_w * 0.40f,
        .h = view->cell_h * 0.40f
    });
}

static void sdl_scene_render_projectile_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    int width_cells;
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float x;
    float y;
    SDL_Color color = sdl_scene_color(TERM_YELLOW);

    if (!view || !layout || !map || !anim)
        return;
    if (!sdl_scene_map_to_screen(layout, map, anim->from_y, anim->from_x,
            &from_row, &from_col, &width_cells))
    {
        return;
    }
    if (!sdl_scene_map_to_screen(layout, map, anim->to_y, anim->to_x, &to_row,
            &to_col, NULL))
    {
        return;
    }

    start_x = (float)(from_col * view->cell_w + width_cells * view->cell_w / 2);
    start_y = (float)(from_row * view->cell_h + view->cell_h / 2);
    end_x = (float)(to_col * view->cell_w + width_cells * view->cell_w / 2);
    end_y = (float)(to_row * view->cell_h + view->cell_h / 2);
    x = start_x + (end_x - start_x) * progress;
    y = start_y + (end_y - start_y) * progress;

    color.a = 210;
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, start_x, start_y, x, y);
    SDL_RenderFillRect(g_state.renderer, &(SDL_FRect){
        .x = x - view->cell_w * 0.15f,
        .y = y - view->cell_h * 0.15f,
        .w = view->cell_w * 0.30f,
        .h = view->cell_h * 0.30f
    });
}

static void sdl_scene_render_damage_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    const app_map_cell_snapshot* cell = sdl_scene_find_cell_by_subject(map,
        anim->subject);
    float progress = sdl_scene_progress(now_ns, anim);
    SDL_Color color = sdl_scene_color(TERM_RED);

    if (!view || !layout || !map || !anim)
        return;

    color.a = sdl_scene_alpha_byte(0.35f * (1.0f - progress));
    if (cell)
    {
        sdl_scene_draw_animation_rect(view, layout, map, cell->map_y, cell->map_x,
            color);
    }
    else
    {
        sdl_scene_fill_rect(&(SDL_FRect){
            .x = 0.0f,
            .y = 0.0f,
            .w = (float)(view->cols * view->cell_w),
            .h = (float)(view->rows * view->cell_h)
        }, color);
    }
}

static void sdl_scene_render_object_animation(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* anim, Uint64 now_ns)
{
    float progress = sdl_scene_progress(now_ns, anim);
    SDL_Color color = sdl_scene_color(TERM_L_GREEN);

    color.a = sdl_scene_alpha_byte(0.30f * (1.0f - progress));
    sdl_scene_draw_animation_rect(view, layout, map, anim->from_y, anim->from_x,
        color);
}

static void sdl_scene_render_animations(const sdl_view* view,
    const sdl_scene_layout* layout, const app_map_snapshot* map,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns)
{
    size_t i;

    if (!view || !layout || !map || !animations)
        return;

    for (i = 0; i < animation_count; i++)
    {
        const sdl_scene_animation* anim = &animations[i];

        if (!anim->active)
            continue;

        switch (anim->kind)
        {
        case SDL_SCENE_ANIMATION_ACTOR_MOVED:
            sdl_scene_render_move_animation(view, layout, map, anim, now_ns);
            break;

        case SDL_SCENE_ANIMATION_DAMAGE:
            sdl_scene_render_damage_animation(view, layout, map, anim, now_ns);
            break;

        case SDL_SCENE_ANIMATION_PROJECTILE:
            sdl_scene_render_projectile_animation(view, layout, map, anim,
                now_ns);
            break;

        case SDL_SCENE_ANIMATION_OBJECT_TRANSFER:
            sdl_scene_render_object_animation(view, layout, map, anim, now_ns);
            break;

        default:
            break;
        }
    }
}

bool sdl_scene_dungeon_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns)
{
    const app_map_snapshot* map;
    const app_status_snapshot* status;
    const app_messages_snapshot* messages;
    const app_panes_snapshot* panes;
    sdl_scene_layout layout;
    size_t i;

    if (!canvas || !main_view || !snapshot)
        return false;

    map = sdl_scene_map_snapshot(snapshot);
    status = sdl_scene_status_snapshot(snapshot);
    messages = sdl_scene_messages_snapshot(snapshot);
    panes = sdl_scene_panes_snapshot(snapshot);
    if (!map || !status || !messages || !panes)
        return false;

    layout = sdl_scene_make_layout(main_view, status->flags);

    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    sdl_scene_render_messages(main_view, messages);
    sdl_scene_render_status_panel(main_view, &layout, status);

    for (i = 0; i < map->cell_count; i++)
        sdl_scene_draw_map_cell(main_view, &layout, map, &map->cells[i]);

    sdl_scene_render_hidden_overlay(main_view, &layout, panes);
    sdl_scene_render_combat_overlay(main_view, &layout, map, panes);
    sdl_scene_render_bottom_line(main_view, &layout, status);
    sdl_scene_render_animations(main_view, &layout, map, animations,
        animation_count, now_ns);
    sdl_scene_draw_absolute_cursor(main_view, &map->cursor);
    sdl_scene_copy_legacy_left_panel(canvas, main_view, &layout);

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
