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

static void sdl_information_draw_text(const sdl_view* view, int col, int row,
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
        sdl_information_color(attr));
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

            if (!op->text[0])
                continue;

            sdl_information_draw_text(main_view, op->col, op->row, op->attr,
                op->text);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}
