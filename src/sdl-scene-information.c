#include "angband.h"

#include "sdl-main-internal.h"

static SDL_Color sdl_information_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static bool sdl_information_layout_cell_rect(const sdl_view* view, int col,
    int row, int width_cells, SDL_FRect* out_rect)
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

static bool sdl_information_cell_is_raw(byte attr, char ch,
    byte terrain_attr, char terrain_char)
{
    unsigned char uch = (unsigned char)ch;
    unsigned char terrain_uch = (unsigned char)terrain_char;

    if ((attr & TILE_FLAG) && (uch & TILE_FLAG))
        return true;
    if (attr == 255 && uch == 0xFF)
        return true;
    if (terrain_attr || terrain_uch)
        return true;

    return false;
}

static void sdl_information_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static void sdl_information_draw_tile(const SDL_FRect* dst, byte attr, byte ch)
{
    SDL_FRect src;

    if (!dst || !g_state.tileset)
        return;
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    src.x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE);
    src.y = (float)(TILE_GET_INDEX(attr) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, dst);
}

static void sdl_information_draw_misc_icon(const SDL_FRect* dst, int icon)
{
    byte attr;
    byte ch;

    if (!dst)
        return;

    attr = misc_to_attr[icon];
    ch = (byte)misc_to_char[icon];
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    sdl_information_draw_tile(dst, attr, ch);
}

static void sdl_information_draw_text(const sdl_view* view, int col, int row,
    byte attr, byte story, cptr text)
{
    TTF_Font* story_font = NULL;
    bool use_story;
    bool grid_align;
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

    use_story = (story & STORY_FLAG_USE) != 0;
    grid_align = (story & STORY_FLAG_CELL_ALIGN) != 0;
    if (use_story)
        story_font = sdl_story_font_for_view((sdl_view*)view);
    if (use_story && story_font)
    {
        if (grid_align)
            sdl_render_story_text_grid((sdl_view*)view, story_font, col, row,
                (int)len, text, sdl_information_color(attr));
        else
            sdl_render_story_text_free((sdl_view*)view, story_font, col, row,
                (int)len, text, sdl_information_color(attr));
        return;
    }

    sdl_render_mono_text((sdl_view*)view, col, row, (int)len, text,
        sdl_information_color(attr));
}

static void sdl_information_draw_cell(const sdl_view* view,
    const app_information_op* op)
{
    SDL_FRect dst;
    byte width;

    if (!view || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_cell_rect(view, op->col, op->row, width, &dst))
        return;

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &dst);

    if (sdl_information_cell_is_raw(op->attr, op->ch, op->terrain_attr,
            op->terrain_char))
    {
        if (g_state.use_tiles && g_state.tileset)
        {
            if ((op->terrain_attr & TILE_FLAG)
                && (((byte)op->terrain_char) & TILE_FLAG))
            {
                sdl_information_draw_tile(&dst, op->terrain_attr,
                    (byte)op->terrain_char);
            }
            if (op->attr & GRAPHICS_GLOW_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_GLOW);
            sdl_information_draw_tile(&dst, op->attr, (byte)op->ch);
            if (op->terrain_attr & GRAPHICS_SLEEP_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_SLEEPING);
            if (((byte)op->terrain_char) & GRAPHICS_SEEN_MASK)
                sdl_information_draw_misc_icon(&dst,
                    ICON_MONSTER_SEES_PLAYER);
            if (((byte)op->ch) & GRAPHICS_ALERT_MASK)
                sdl_information_draw_misc_icon(&dst, ICON_ALERT);
        }
        return;
    }

    if (op->ch)
    {
        char glyph[2] = { op->ch, '\0' };
        sdl_information_draw_text(view, op->col, op->row, op->attr, op->story,
            glyph);
    }
}

static void sdl_information_draw_cursor(const sdl_view* view,
    const app_information_op* op)
{
    SDL_FRect rect;
    SDL_Color color;
    byte width;

    if (!view || !op)
        return;

    width = op->width ? op->width : 1;
    if (!sdl_information_layout_cell_rect(view, op->col, op->row, width, &rect))
        return;

    color = sdl_information_color(op->attr);
    color.a = 220;
    sdl_information_draw_rect(&rect, color);
}

bool sdl_scene_information_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_information_snapshot* snapshot)
{
    u16b i;

    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_INFORMATION)
        return false;
    if (snapshot->snapshot.blob_count < 1)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_INFORMATION
        || snapshot->blobs[0].size < sizeof(app_information_scene))
    {
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (snapshot->scene.format_version == APP_INFORMATION_FORMAT_VERSION)
    {
        for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
        {
            const app_information_op* op = &snapshot->scene.ops[i];

            if (op->kind == APP_INFORMATION_OP_KIND_TEXT)
            {
                if (!op->text[0])
                    continue;
                sdl_information_draw_text(main_view, op->col, op->row,
                    op->attr, op->story, op->text);
            }
            else if (op->kind == APP_INFORMATION_OP_KIND_CELL)
            {
                sdl_information_draw_cell(main_view, op);
            }
        }

        for (i = 0; i < snapshot->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
        {
            const app_information_op* op = &snapshot->scene.ops[i];

            if (op->kind == APP_INFORMATION_OP_KIND_CURSOR)
                sdl_information_draw_cursor(main_view, op);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
