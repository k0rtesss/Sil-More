#include "angband.h"

#include "sdl-main-internal.h"

static SDL_Color sdl_menu_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        255
    };
}

static void sdl_menu_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

static void sdl_menu_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static void sdl_menu_draw_tile(byte attr, byte ch, const SDL_FRect* dst)
{
    SDL_FRect src;

    if (!g_state.tileset || !dst)
        return;
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    src.x = (float)(TILE_GET_INDEX(ch) * TILE_SIZE);
    src.y = (float)(TILE_GET_INDEX(attr) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, dst);
}

static int sdl_menu_scale_px(float logical_value)
{
    return sdl_ui_scale_px(logical_value);
}

static int sdl_menu_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

static int sdl_menu_measure_text(TTF_Font* font, cptr text)
{
    return sdl_ui_measure_text(font, text);
}

static void sdl_menu_render_icon(TTF_Font* font, float x_px, float y_px,
    int icon_slot_w, int line_h, byte icon_attr, char icon_char);
static int sdl_menu_measure_text_n(TTF_Font* font, cptr text, size_t len);
static int sdl_menu_render_document_text_run_px(TTF_Font* font, float x_px,
    float y_px, SDL_Color color, cptr text, size_t len, int target_h,
    float max_w_px);
static float sdl_menu_measure_document_text_run_px(TTF_Font* font, cptr text,
    size_t len, int target_h, float max_w_px);

static int sdl_menu_icon_slot_px(TTF_Font* font, int line_h)
{
    int icon_slot_w = sdl_menu_measure_text(font, "MM");

    if (icon_slot_w < line_h)
        icon_slot_w = line_h;
    if (icon_slot_w < 1)
        icon_slot_w = 1;

    return icon_slot_w;
}

static void sdl_menu_render_text(TTF_Font* font, float x_px, float y_px,
    int line_h, SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float render_w;
    float render_h;

    if (!font || !text || !text[0] || line_h <= 0)
        return;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    render_w = (float)surface->w;
    render_h = (float)surface->h;
    if (render_h > (float)line_h && render_h > 0.0f)
        render_h = (float)line_h;

    dst.x = x_px;
    dst.y = y_px;
    dst.w = render_w;
    dst.h = render_h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void sdl_menu_render_fixed_glyph(TTF_Font* font, float x_px, float y_px,
    int cell_w, int cell_h, SDL_Color color, char ch)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    char glyph[2] = { ch ? ch : ' ', '\0' };
    float scale = 1.0f;
    float dst_w;
    float dst_h;

    if (!font || !glyph[0] || glyph[0] == ' ' || cell_w <= 0 || cell_h <= 0)
        return;

    surface = TTF_RenderText_Blended(font, glyph, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    if (surface->h > cell_h)
        scale = (float)cell_h / (float)surface->h;
    if (surface->w > 0 && ((float)surface->w * scale) > (float)cell_w)
    {
        float width_scale = (float)cell_w / (float)surface->w;

        if (width_scale < scale)
            scale = width_scale;
    }

    dst_w = (float)surface->w * scale;
    dst_h = (float)surface->h * scale;
    dst.x = x_px + ((float)cell_w - dst_w) * 0.5f;
    dst.y = y_px + ((float)cell_h - dst_h) * 0.5f;
    dst.w = dst_w;
    dst.h = dst_h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void sdl_menu_render_fixed_text(TTF_Font* font, float x_px, float y_px,
    int cell_w, int cell_h, SDL_Color color, cptr text)
{
    size_t i;

    if (!font || !text || !text[0] || cell_w <= 0 || cell_h <= 0)
        return;

    for (i = 0; text[i]; i++)
    {
        sdl_menu_render_fixed_glyph(font, x_px + (float)(i * cell_w), y_px,
            cell_w, cell_h, color, text[i]);
    }
}

static bool sdl_menu_document_cell_is_raw(byte attr, char ch, byte terrain_attr,
    char terrain_char)
{
    unsigned char raw = (unsigned char)ch;
    unsigned char terrain_raw = (unsigned char)terrain_char;

    if (((attr & 0x80) && (raw & 0x80)) || (attr == 255 && raw == 0xFF))
        return true;
    if (terrain_attr || terrain_raw)
        return true;

    return false;
}

static void sdl_menu_draw_misc_icon(const SDL_FRect* dst, int icon)
{
    byte attr;
    byte ch;

    if (!dst)
        return;

    attr = misc_to_attr[icon];
    ch = (byte)misc_to_char[icon];
    if (!(attr & TILE_FLAG) || !(ch & TILE_FLAG))
        return;

    sdl_menu_draw_tile(attr, ch, dst);
}

static void sdl_menu_format_plain_row(const app_ui_row* row, char* buf,
    size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!row)
        return;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
    {
        SDL_strlcpy(buf, row->label, buf_size);
        return;
    }

    if (row->key[0] && row->label[0] && row->meta[0])
        strnfmt(buf, buf_size, "%s %s %s", row->key, row->label, row->meta);
    else if (row->key[0] && row->label[0])
        strnfmt(buf, buf_size, "%s %s", row->key, row->label);
    else if (row->label[0] && row->meta[0])
        strnfmt(buf, buf_size, "%s %s", row->label, row->meta);
    else if (row->label[0])
        SDL_strlcpy(buf, row->label, buf_size);
    else if (row->meta[0])
        SDL_strlcpy(buf, row->meta, buf_size);
}

static bool sdl_menu_is_plain_panel(const app_ui_panel* panel)
{
    if (!panel)
        return false;

    return panel->style == APP_UI_PANEL_STYLE_PLAIN
        && panel->row_count > 0
        && panel->body_line_count == 0
        && panel->detail_line_count == 0
        && panel->rich_paragraph_count == 0
        && panel->footer_action_count == 0
        && panel->tab_count == 0
        && !panel->icon_char
        && !panel->title[0]
        && !panel->subtitle[0]
        && !panel->detail_title[0];
}

static int sdl_menu_measure_row(TTF_Font* font, const app_ui_panel* panel,
    const app_ui_row* row, int item_gap)
{
    int width = 0;
    int line_h;

    if (!font || !panel || !row)
        return 0;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        return row->label[0] ? sdl_menu_measure_text(font, row->label) : 0;

    line_h = TTF_GetFontHeight(font);
    if (row->icon_char)
    {
        width += sdl_menu_icon_slot_px(font, line_h);
        if (row->key[0] || row->label[0] || row->meta[0])
            width += item_gap;
    }
    if (panel->style != APP_UI_PANEL_STYLE_PLAIN && row->key[0])
        width += sdl_menu_measure_text(font, row->key) + item_gap;
    if (row->label[0])
        width += sdl_menu_measure_text(font, row->label);
    if (row->meta[0])
        width += item_gap + sdl_menu_measure_text(font, row->meta);

    return width;
}


static int sdl_menu_measure_footer(TTF_Font* font,
    const app_ui_panel* panel, int pill_gap, int pill_pad_x)
{
    int width = 0;
    u16b i;

    if (!font || !panel || panel->footer_action_count == 0)
        return 0;

    for (i = 0; i < panel->footer_action_count; i++)
    {
        const app_ui_footer_action* action = &panel->footer_actions[i];
        char text[APP_UI_KEY_MAX + APP_UI_LABEL_MAX + 4];
        int action_w;

        if (action->key[0])
            strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
        else
            SDL_strlcpy(text, action->label, sizeof(text));

        action_w = sdl_menu_measure_text(font, text) + pill_pad_x * 2;
        if (width > 0)
            width += pill_gap;
        width += action_w;
    }

    return width;
}

static int sdl_menu_measure_tabs(TTF_Font* font, const app_ui_panel* panel,
    int pill_gap, int pill_pad_x)
{
    int width = 0;
    u16b i;

    if (!font || !panel || panel->tab_count == 0)
        return 0;

    for (i = 0; i < panel->tab_count; i++)
    {
        const app_ui_tab* tab = &panel->tabs[i];
        int tab_w = sdl_menu_measure_text(font, tab->label) + pill_pad_x * 2;

        if (width > 0)
            width += pill_gap;
        width += tab_w;
    }

    return width;
}

static void sdl_menu_render_row(TTF_Font* font, const app_ui_panel* panel,
    const app_ui_row* row, const SDL_Rect* clip_rect, int line_h,
    int item_gap, int current_y, byte accent_attr)
{
    SDL_Color color;
    SDL_Color meta_color;
    SDL_Color selected_fill;
    int icon_slot_w = 0;
    int key_w = 0;
    int label_x = clip_rect->x;
    int meta_w = 0;
    int meta_x = clip_rect->x;

    if (!font || !panel || !row || !clip_rect)
        return;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
    {
        if (row->label[0])
        {
            sdl_menu_render_text(font, (float)clip_rect->x, (float)current_y,
                line_h, sdl_menu_color(row->attr ? row->attr : accent_attr),
                row->label);
        }
        return;
    }

    color = sdl_menu_color((row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : row->attr);
    meta_color = sdl_menu_color((row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : (row->meta_attr ? row->meta_attr : row->attr));
    selected_fill = sdl_menu_color(accent_attr);
    selected_fill.a = (panel->style == APP_UI_PANEL_STYLE_PLAIN) ? 72 : 104;

    if ((row->flags & APP_UI_ITEM_FLAG_SELECTED)
        && panel->style != APP_UI_PANEL_STYLE_PLAIN)
    {
        SDL_FRect selected_rect = {
            (float)(clip_rect->x - item_gap),
            (float)(current_y - sdl_menu_scale_px(3.0f)),
            (float)(clip_rect->w + item_gap * 2),
            (float)(line_h + sdl_menu_scale_px(6.0f))
        };

        sdl_menu_fill_rect(&selected_rect, selected_fill);
    }

    if (row->icon_char)
    {
        icon_slot_w = sdl_menu_icon_slot_px(font, line_h);
        sdl_menu_render_icon(font, (float)label_x, (float)current_y,
            icon_slot_w, line_h, row->icon_attr, row->icon_char);
        label_x += icon_slot_w;
        if ((panel->style != APP_UI_PANEL_STYLE_PLAIN && row->key[0])
            || row->label[0] || row->meta[0])
        {
            label_x += item_gap;
        }
    }

    if (panel->style != APP_UI_PANEL_STYLE_PLAIN && row->key[0])
    {
        key_w = sdl_menu_measure_text(font, row->key);
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, sdl_menu_color(panel->accent_attr), row->key);
        label_x += key_w + item_gap;
    }

    if (row->meta[0])
    {
        meta_w = sdl_menu_measure_text(font, row->meta);
        meta_x = clip_rect->x + clip_rect->w - meta_w;
        if (meta_x < label_x + item_gap)
            meta_x = label_x + item_gap;
    }

    if (row->label[0])
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, color, row->label);
    if (row->meta[0])
        sdl_menu_render_text(font, (float)meta_x, (float)current_y,
            line_h, meta_color, row->meta);
}

static void sdl_menu_render_footer(TTF_Font* font,
    const app_ui_panel* panel, const SDL_Rect* clip_rect, int line_h,
    int pill_gap, int pill_pad_x, int pill_pad_y)
{
    int cursor_x;
    u16b i;

    if (!font || !panel || !clip_rect || panel->footer_action_count == 0)
        return;

    cursor_x = clip_rect->x;
    for (i = 0; i < panel->footer_action_count; i++)
    {
        const app_ui_footer_action* action = &panel->footer_actions[i];
        char text[APP_UI_KEY_MAX + APP_UI_LABEL_MAX + 4];
        int text_w;
        SDL_Color fill = sdl_menu_color(panel->accent_attr);
        SDL_Color border = sdl_menu_color(panel->accent_attr);
        SDL_Color text_color;
        SDL_FRect pill;

        if (action->key[0])
            strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
        else
            SDL_strlcpy(text, action->label, sizeof(text));

        text_w = sdl_menu_measure_text(font, text);
        pill.x = (float)cursor_x;
        pill.y = (float)clip_rect->y;
        pill.w = (float)(text_w + pill_pad_x * 2);
        pill.h = (float)(line_h + pill_pad_y * 2);

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        if (action->flags & APP_UI_ITEM_FLAG_DISABLED)
        {
            fill = sdl_menu_color(TERM_L_DARK);
            fill.a = 88;
            border = sdl_menu_color(TERM_SLATE);
            border.a = 180;
            text_color = sdl_menu_color(TERM_L_DARK);
        }
        else
        {
            fill.a = 64;
            border.a = 212;
            text_color = sdl_menu_color(action->attr ? action->attr : TERM_WHITE);
        }

        sdl_menu_fill_rect(&pill, fill);
        sdl_menu_draw_rect(&pill, border);
        sdl_menu_render_text(font, pill.x + pill_pad_x, pill.y + pill_pad_y,
            line_h, text_color, text);

        cursor_x += (int)pill.w + pill_gap;
    }
}

static void sdl_menu_render_tabs(TTF_Font* font, const app_ui_panel* panel,
    const SDL_Rect* clip_rect, int line_h, int pill_gap,
    int pill_pad_x, int pill_pad_y)
{
    int cursor_x;
    u16b i;

    if (!font || !panel || !clip_rect || panel->tab_count == 0)
        return;

    cursor_x = clip_rect->x;
    for (i = 0; i < panel->tab_count; i++)
    {
        const app_ui_tab* tab = &panel->tabs[i];
        SDL_Color fill = sdl_menu_color((tab->flags & APP_UI_ITEM_FLAG_ACTIVE)
            ? panel->accent_attr
            : TERM_SLATE);
        SDL_Color border = sdl_menu_color(panel->accent_attr);
        SDL_Color text_color = sdl_menu_color(tab->attr ? tab->attr : TERM_WHITE);
        int text_w = sdl_menu_measure_text(font, tab->label);
        SDL_FRect pill = {
            (float)cursor_x,
            (float)clip_rect->y,
            (float)(text_w + pill_pad_x * 2),
            (float)(line_h + pill_pad_y * 2)
        };

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        fill.a = (tab->flags & APP_UI_ITEM_FLAG_ACTIVE) ? 96 : 40;
        border.a = 220;
        sdl_menu_fill_rect(&pill, fill);
        sdl_menu_draw_rect(&pill, border);
        sdl_menu_render_text(font, pill.x + pill_pad_x, pill.y + pill_pad_y,
            line_h, text_color, tab->label);

        cursor_x += (int)pill.w + pill_gap;
    }
}

static bool sdl_menu_render_term_plain_panel(const sdl_view* main_view,
    const app_ui_panel* panel)
{
    TTF_Font* font = NULL;
    SDL_FRect clear_rect;
    int canvas_w;
    int canvas_h;
    int desired_px;
    int min_px;
    int pixel_height;
    int menu_w = 0;
    int col_main = 0;
    int row_top = 0;
    int row_first = 0;
    int menu_h = 0;
    int clear_x = 0;
    int clear_w = 0;
    int cell_w = 0;
    int cell_h = 0;
    u16b i;

    if (!main_view || !panel || !sdl_menu_is_plain_panel(panel))
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    for (i = 0; i < panel->row_count; i++)
    {
        char row_text[APP_UI_LABEL_MAX + APP_UI_META_MAX + APP_UI_KEY_MAX + 8];
        int row_w;

        sdl_menu_format_plain_row(&panel->rows[i], row_text,
            sizeof(row_text));
        row_w = (int)strlen(row_text);
        if (row_w > menu_w)
            menu_w = row_w;
    }
    if (menu_w <= 0)
        return false;

    desired_px = sdl_menu_scale_px((float)sdl_menu_font_size_logical(main_view));
    min_px = sdl_menu_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        int font_h;
        int screen_cols;
        int screen_rows;
        int top_pad;
        int bottom_pad;

        font = sdl_ui_font_for_height(pixel_height);
        if (!font)
            continue;

        font_h = TTF_GetFontHeight(font);
        cell_h = MAX(pixel_height, font_h);
        cell_w = sdl_menu_measure_text(font, "M");
        if (cell_w < 1)
            cell_w = 1;

        screen_cols = canvas_w / cell_w;
        screen_rows = canvas_h / cell_h;
        if (screen_cols <= 0 || screen_rows <= 0)
            continue;

        top_pad = 1;
        bottom_pad = (screen_rows <= 18) ? 0 : 1;
        row_top = (screen_rows <= 18) ? 0 : ((screen_rows > 1) ? 1 : 0);
        row_first = row_top + top_pad;
        menu_h = (int)panel->row_count + top_pad + bottom_pad;
        if (menu_w + 4 > screen_cols)
            continue;
        if (row_top + menu_h > screen_rows)
            continue;

        col_main = (screen_cols - menu_w) / 2;
        if (col_main < 0)
            continue;

        clear_x = col_main - 2;
        clear_w = menu_w + 4;
        if (clear_x < 0)
            clear_x = 0;
        if (clear_x + clear_w > screen_cols)
            clear_w = screen_cols - clear_x;
        if (clear_w <= 0)
            continue;

        break;
    }

    if (!font || cell_w <= 0 || cell_h <= 0 || clear_w <= 0 || menu_h <= 0)
        return false;

    clear_rect.x = (float)(clear_x * cell_w);
    clear_rect.y = (float)(row_top * cell_h);
    clear_rect.w = (float)(clear_w * cell_w);
    clear_rect.h = (float)(menu_h * cell_h);
    sdl_menu_fill_rect(&clear_rect, (SDL_Color){ 0, 0, 0, 232 });

    for (i = 0; i < panel->row_count; i++)
    {
        const app_ui_row* row = &panel->rows[i];
        char row_text[APP_UI_LABEL_MAX + APP_UI_META_MAX + APP_UI_KEY_MAX + 8];
        byte attr = (row->flags & APP_UI_ITEM_FLAG_DISABLED)
            ? TERM_L_DARK
            : row->attr;

        sdl_menu_format_plain_row(row, row_text, sizeof(row_text));
        sdl_menu_render_fixed_text(font, (float)(col_main * cell_w),
            (float)((row_first + (int)i) * cell_h), cell_w, cell_h,
            sdl_menu_color(attr), row_text);
    }

    return true;
}

static TTF_Font* sdl_menu_rich_run_font(TTF_Font* mono_font,
    TTF_Font* story_font, byte story)
{
    if ((story & STORY_FLAG_USE) != 0 && story_font)
        return story_font;

    return mono_font;
}

static float sdl_menu_measure_rich_token_px(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, const app_ui_rich_run* run, cptr text,
    size_t len)
{
    TTF_Font* font;

    if (!run || !text || len == 0)
        return 0.0f;

    font = sdl_menu_rich_run_font(mono_font, story_font, run->story);
    return sdl_menu_measure_document_text_run_px(font, text, len, line_h, 0.0f);
}

static int sdl_menu_measure_rich_paragraph_height(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, int line_gap, int width_px,
    const app_ui_scene* scene, const app_ui_rich_paragraph* paragraph)
{
    int lines = 1;
    float current_x = 0.0f;
    bool line_started = false;
    u16b i;

    if (!mono_font || !scene || !paragraph || line_h <= 0 || width_px <= 0)
        return 0;
    if (paragraph->run_count == 0)
        return line_h;

    for (i = 0; i < paragraph->run_count; i++)
    {
        const app_ui_rich_run* run = &scene->rich_runs[
            (u16b)(paragraph->run_first + i)];
        const char* cursor = run->text;

        while (cursor && *cursor)
        {
            if (*cursor == '\n')
            {
                lines++;
                current_x = 0.0f;
                line_started = false;
                cursor++;
                continue;
            }

            bool spaces = (*cursor == ' ');
            size_t len = 0;
            float token_w;

            while (cursor[len] != '\0' && cursor[len] != '\n'
                && ((cursor[len] == ' ') == spaces))
            {
                len++;
            }
            if (len == 0)
                break;

            token_w = sdl_menu_measure_rich_token_px(mono_font, story_font,
                line_h, run, cursor, len);
            if (spaces)
            {
                if (current_x + token_w <= (float)width_px)
                {
                    current_x += token_w;
                    if (current_x > 0.0f)
                        line_started = true;
                }
                else
                {
                    lines++;
                    current_x = 0.0f;
                    line_started = false;
                }
                cursor += len;
                continue;
            }

            if (line_started && current_x + token_w > (float)width_px)
            {
                lines++;
                current_x = 0.0f;
                line_started = false;
            }

            current_x += token_w;
            if (current_x > (float)width_px)
                current_x = (float)width_px;
            line_started = true;
            cursor += len;
        }
    }

    return lines * line_h + (lines - 1) * line_gap;
}

static int sdl_menu_measure_rich_text_height(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, int line_gap, int paragraph_gap,
    int width_px, const app_ui_scene* scene, const app_ui_panel* panel)
{
    int total_h = 0;
    u16b i;

    if (!mono_font || !scene || !panel || panel->rich_paragraph_count == 0)
        return 0;

    for (i = 0; i < panel->rich_paragraph_count; i++)
    {
        const app_ui_rich_paragraph* paragraph = &scene->rich_paragraphs[
            (u16b)(panel->rich_paragraph_first + i)];
        int paragraph_h = sdl_menu_measure_rich_paragraph_height(mono_font,
            story_font, line_h, line_gap, width_px, scene, paragraph);

        if (paragraph_h <= 0)
            continue;
        if (total_h > 0)
            total_h += paragraph_gap;
        total_h += paragraph_h;
    }

    return total_h;
}

static int sdl_menu_render_rich_paragraph(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, int line_gap, int x_px, int y_px,
    int width_px, const app_ui_scene* scene,
    const app_ui_rich_paragraph* paragraph)
{
    float current_x = 0.0f;
    int current_y = y_px;
    bool line_started = false;
    u16b i;

    if (!mono_font || !scene || !paragraph || line_h <= 0 || width_px <= 0)
        return 0;
    if (paragraph->run_count == 0)
        return line_h;

    for (i = 0; i < paragraph->run_count; i++)
    {
        const app_ui_rich_run* run = &scene->rich_runs[
            (u16b)(paragraph->run_first + i)];
        TTF_Font* font = sdl_menu_rich_run_font(mono_font, story_font,
            run->story);
        const char* cursor = run->text;

        while (cursor && *cursor)
        {
            if (*cursor == '\n')
            {
                current_x = 0.0f;
                current_y += line_h + line_gap;
                line_started = false;
                cursor++;
                continue;
            }

            bool spaces = (*cursor == ' ');
            size_t len = 0;
            float token_w;

            while (cursor[len] != '\0' && cursor[len] != '\n'
                && ((cursor[len] == ' ') == spaces))
            {
                len++;
            }
            if (len == 0)
                break;

            token_w = sdl_menu_measure_rich_token_px(mono_font, story_font,
                line_h, run, cursor, len);
            if (spaces)
            {
                if (current_x + token_w <= (float)width_px)
                {
                    current_x += token_w;
                    if (current_x > 0.0f)
                        line_started = true;
                }
                else
                {
                    current_x = 0.0f;
                    current_y += line_h + line_gap;
                    line_started = false;
                }
                cursor += len;
                continue;
            }

            if (line_started && current_x + token_w > (float)width_px)
            {
                current_x = 0.0f;
                current_y += line_h + line_gap;
                line_started = false;
            }

            (void)sdl_menu_render_document_text_run_px(font,
                (float)x_px + current_x, (float)current_y,
                sdl_menu_color(run->attr), cursor, len, line_h,
                (float)width_px - current_x);
            current_x += token_w;
            if (current_x > (float)width_px)
                current_x = (float)width_px;
            line_started = true;
            cursor += len;
        }
    }

    return current_y - y_px + line_h;
}

static int sdl_menu_render_rich_text(const app_ui_scene* scene,
    const app_ui_panel* panel, TTF_Font* mono_font, TTF_Font* story_font,
    const SDL_Rect* clip_rect, int line_h, int line_gap, int paragraph_gap,
    int start_y)
{
    int current_y = start_y;
    u16b i;

    if (!scene || !panel || !mono_font || !clip_rect
        || panel->rich_paragraph_count == 0)
    {
        return 0;
    }

    for (i = 0; i < panel->rich_paragraph_count; i++)
    {
        const app_ui_rich_paragraph* paragraph = &scene->rich_paragraphs[
            (u16b)(panel->rich_paragraph_first + i)];
        int paragraph_h = sdl_menu_render_rich_paragraph(mono_font,
            story_font, line_h, line_gap, clip_rect->x, current_y, clip_rect->w,
            scene, paragraph);

        if (paragraph_h <= 0)
            continue;
        current_y += paragraph_h;
        if (i + 1 < panel->rich_paragraph_count)
            current_y += paragraph_gap;
    }

    return current_y - start_y;
}

static int sdl_menu_browser_action_width(TTF_Font* font,
    const app_ui_footer_action* action)
{
    char text[APP_UI_KEY_MAX + APP_UI_LABEL_MAX + 4];

    if (!font || !action || !action->label[0])
        return 0;

    if (action->key[0])
        strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
    else
        SDL_strlcpy(text, action->label, sizeof(text));

    return sdl_menu_measure_text(font, text);
}

static int sdl_menu_browser_footer_lines(TTF_Font* font,
    const app_ui_panel* panel, int max_w, int item_gap)
{
    int cursor_w = 0;
    int lines = 0;
    u16b i;

    if (!font || !panel || panel->footer_action_count == 0 || max_w <= 0)
        return 0;

    lines = 1;
    for (i = 0; i < panel->footer_action_count; i++)
    {
        int token_w = sdl_menu_browser_action_width(font,
            &panel->footer_actions[i]);

        if (token_w <= 0)
            continue;
        if (cursor_w > 0 && cursor_w + item_gap + token_w > max_w)
        {
            lines++;
            cursor_w = token_w;
        }
        else
        {
            if (cursor_w > 0)
                cursor_w += item_gap;
            cursor_w += token_w;
        }
    }

    return lines;
}

static void sdl_menu_render_browser_tabs(TTF_Font* font,
    const app_ui_panel* panel, int x_px, int y_px, int line_h, int item_gap)
{
    int cursor_x = x_px;
    u16b i;

    if (!font || !panel || panel->tab_count == 0)
        return;

    for (i = 0; i < panel->tab_count; i++)
    {
        const app_ui_tab* tab = &panel->tabs[i];
        int tab_w = sdl_menu_measure_text(font, tab->label);

        if (i > 0)
            cursor_x += item_gap;
        sdl_menu_render_text(font, (float)cursor_x, (float)y_px, line_h,
            sdl_menu_color(tab->attr ? tab->attr : TERM_SLATE), tab->label);
        cursor_x += tab_w;
    }
}

static void sdl_menu_render_browser_footer(TTF_Font* font,
    const app_ui_panel* panel, int x_px, int y_px, int max_w, int line_h,
    int line_gap, int item_gap)
{
    int cursor_x = x_px;
    int cursor_y = y_px;
    u16b i;

    if (!font || !panel || panel->footer_action_count == 0 || max_w <= 0)
        return;

    for (i = 0; i < panel->footer_action_count; i++)
    {
        const app_ui_footer_action* action = &panel->footer_actions[i];
        char text[APP_UI_KEY_MAX + APP_UI_LABEL_MAX + 4];
        int token_w;
        byte attr;

        if (!action->label[0])
            continue;
        if (action->key[0])
            strnfmt(text, sizeof(text), "%s %s", action->key, action->label);
        else
            SDL_strlcpy(text, action->label, sizeof(text));

        token_w = sdl_menu_measure_text(font, text);
        if (cursor_x > x_px && cursor_x + token_w > x_px + max_w)
        {
            cursor_x = x_px;
            cursor_y += line_h + line_gap;
        }

        attr = (action->flags & APP_UI_ITEM_FLAG_DISABLED)
            ? TERM_L_DARK
            : (action->attr ? action->attr : TERM_SLATE);
        sdl_menu_render_text(font, (float)cursor_x, (float)cursor_y, line_h,
            sdl_menu_color(attr), text);
        cursor_x += token_w + item_gap;
    }
}

static void sdl_menu_render_browser_row(TTF_Font* font,
    const app_ui_panel* panel, const app_ui_row* row, const SDL_Rect* clip_rect,
    int line_h, int item_gap, int current_y)
{
    SDL_Color color;
    SDL_Color meta_color;
    int icon_slot_w = 0;
    int key_w = 0;
    int label_x;
    int meta_w = 0;
    int meta_x;

    if (!font || !panel || !row || !clip_rect)
        return;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
    {
        if (row->label[0])
        {
            sdl_menu_render_text(font, (float)clip_rect->x, (float)current_y,
                line_h, sdl_menu_color(row->attr ? row->attr : TERM_WHITE),
                row->label);
        }
        return;
    }

    color = sdl_menu_color((row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : ((row->flags & APP_UI_ITEM_FLAG_SELECTED)
            ? panel->accent_attr
            : row->attr));
    meta_color = sdl_menu_color((row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : ((row->flags & APP_UI_ITEM_FLAG_SELECTED)
            ? panel->accent_attr
            : (row->meta_attr ? row->meta_attr : row->attr)));
    label_x = clip_rect->x;
    meta_x = clip_rect->x;

    if (row->icon_char)
    {
        icon_slot_w = sdl_menu_icon_slot_px(font, line_h);
        sdl_menu_render_icon(font, (float)label_x, (float)current_y,
            icon_slot_w, line_h, row->icon_attr, row->icon_char);
        label_x += icon_slot_w;
        if (row->key[0] || row->label[0] || row->meta[0])
            label_x += item_gap;
    }

    if (row->key[0])
    {
        key_w = sdl_menu_measure_text(font, row->key);
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, sdl_menu_color(panel->accent_attr), row->key);
        label_x += key_w + item_gap;
    }

    if (row->meta[0])
    {
        meta_w = sdl_menu_measure_text(font, row->meta);
        meta_x = clip_rect->x + clip_rect->w - meta_w;
        if (meta_x < label_x + item_gap)
            meta_x = label_x + item_gap;
    }

    if (row->label[0])
        sdl_menu_render_text(font, (float)label_x, (float)current_y,
            line_h, color, row->label);
    if (row->meta[0])
        sdl_menu_render_text(font, (float)meta_x, (float)current_y,
            line_h, meta_color, row->meta);
}

static bool sdl_menu_render_browser_panel(const sdl_view* main_view,
    const app_ui_panel* ui_panel)
{
    TTF_Font* font;
    SDL_Color line_color = sdl_menu_color(TERM_L_DARK);
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;
    int line_gap;
    int section_gap;
    int item_gap;
    int margin_x;
    int margin_y;
    int column_gap;
    int detail_w = 0;
    int detail_measured_w = 0;
    int detail_x;
    int rows_x;
    int header_y;
    int divider_y;
    int content_top;
    int footer_lines;
    int footer_h = 0;
    int footer_y;
    int status_h = 0;
    int status_y;
    int content_bottom;
    int available_rows_h;
    int row_visible = 0;
    int row_start = 0;
    bool has_detail;
    bool detail_leading;
    u16b i;

    if (!main_view || !ui_panel)
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_scale_px((float)sdl_menu_font_size_logical(main_view));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;

    line_h = pixel_height;
    if (line_h <= 0)
        line_h = TTF_GetFontHeight(font);
    line_gap = sdl_menu_scale_px(2.0f);
    section_gap = sdl_menu_scale_px(10.0f);
    item_gap = sdl_menu_scale_px(16.0f);
    margin_x = sdl_menu_scale_px(18.0f);
    margin_y = sdl_menu_scale_px(10.0f);
    column_gap = sdl_menu_scale_px(28.0f);

    sdl_menu_fill_rect(&(SDL_FRect){
        0.0f, 0.0f, (float)canvas_w, (float)canvas_h
    }, (SDL_Color){ 0, 0, 0, 255 });

    footer_lines = sdl_menu_browser_footer_lines(font, ui_panel,
        MAX(1, canvas_w - margin_x * 2), item_gap);
    if (footer_lines > 0)
        footer_h = footer_lines * line_h + (footer_lines - 1) * line_gap;
    if (ui_panel->body_line_count > 0)
        status_h = ui_panel->body_line_count * line_h
            + (ui_panel->body_line_count - 1) * line_gap;

    header_y = margin_y;
    if (ui_panel->title[0])
    {
        sdl_menu_render_text(font, (float)margin_x, (float)header_y, line_h,
            sdl_menu_color(ui_panel->title_attr), ui_panel->title);
        header_y += line_h + line_gap;
    }
    if (ui_panel->tab_count > 0)
    {
        sdl_menu_render_browser_tabs(font, ui_panel, margin_x, header_y, line_h,
            item_gap);
        header_y += line_h + section_gap;
    }

    has_detail = ((ui_panel->flags & APP_UI_PANEL_FLAG_SHOW_DETAIL) != 0)
        && (ui_panel->detail_line_count > 0 || ui_panel->detail_title[0]);
    detail_leading = has_detail
        && ((ui_panel->flags & APP_UI_PANEL_FLAG_DETAIL_LEADING) != 0);
    if (has_detail)
    {
        if (ui_panel->detail_title[0])
            detail_measured_w = sdl_menu_measure_text(font,
                ui_panel->detail_title);
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            detail_measured_w = MAX(detail_measured_w, sdl_menu_measure_text(font,
                ui_panel->detail_lines[i].text));
        }
        detail_w = detail_measured_w + sdl_menu_scale_px(10.0f);
        if (detail_w < sdl_menu_scale_px(150.0f))
            detail_w = sdl_menu_scale_px(150.0f);
        if (detail_w > canvas_w / 3)
            detail_w = canvas_w / 3;
    }

    if (has_detail && detail_leading)
    {
        detail_x = margin_x;
        rows_x = margin_x + detail_w + column_gap;
    }
    else
    {
        rows_x = margin_x;
        detail_x = canvas_w - margin_x - detail_w;
    }

    if (ui_panel->detail_title[0])
    {
        sdl_menu_render_text(font, (float)detail_x, (float)header_y, line_h,
            sdl_menu_color(ui_panel->detail_title_attr), ui_panel->detail_title);
    }
    if (ui_panel->subtitle[0])
    {
        sdl_menu_render_text(font, (float)rows_x, (float)header_y, line_h,
            sdl_menu_color(ui_panel->subtitle_attr), ui_panel->subtitle);
    }

    divider_y = header_y + line_h + line_gap;
    sdl_menu_fill_rect(&(SDL_FRect){
        (float)margin_x, (float)divider_y,
        (float)(canvas_w - margin_x * 2), 1.0f
    }, line_color);
    if (has_detail)
    {
        float rule_x = detail_leading
            ? (float)(rows_x - column_gap / 2)
            : (float)(detail_x - column_gap / 2);

        sdl_menu_fill_rect(&(SDL_FRect){
            rule_x, (float)(divider_y + line_gap),
            1.0f, (float)(canvas_h - divider_y - margin_y - footer_h - status_h
                - section_gap * 2)
        }, line_color);
    }

    content_top = divider_y + line_gap + section_gap;
    footer_y = canvas_h - margin_y - footer_h;
    status_y = footer_y;
    if (status_h > 0)
        status_y -= status_h + section_gap;
    content_bottom = (status_h > 0) ? (status_y - line_gap) : (footer_y - line_gap);
    available_rows_h = content_bottom - content_top;
    if (available_rows_h < line_h)
        available_rows_h = line_h;

    if (ui_panel->row_count > 0)
    {
        row_visible = (available_rows_h + line_gap) / (line_h + line_gap);
        if (row_visible < 1)
            row_visible = 1;
        if (row_visible > (int)ui_panel->row_count)
            row_visible = ui_panel->row_count;

        row_start = ui_panel->row_offset;
        if (row_start < 0)
            row_start = 0;
        if (ui_panel->selected_row >= 0
            && ui_panel->selected_row < (s16b)ui_panel->row_count)
        {
            if (ui_panel->selected_row < row_start)
                row_start = ui_panel->selected_row;
            if (ui_panel->selected_row >= row_start + row_visible)
                row_start = ui_panel->selected_row - row_visible + 1;
        }
        if (row_start + row_visible > (int)ui_panel->row_count)
            row_start = ui_panel->row_count - row_visible;
        if (row_start < 0)
            row_start = 0;
    }

    if (has_detail)
    {
        SDL_Rect detail_clip = {
            detail_x,
            content_top,
            detail_w,
            MAX(0, content_bottom - content_top)
        };
        int detail_y = content_top;

        SDL_SetRenderClipRect(g_state.renderer, &detail_clip);
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            sdl_menu_render_text(font, (float)detail_x, (float)detail_y, line_h,
                sdl_menu_color(ui_panel->detail_lines[i].attr),
                ui_panel->detail_lines[i].text);
            detail_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (ui_panel->row_count > 0)
    {
        int rows_w;
        SDL_Rect row_clip;
        int row_y = content_top;

        if (has_detail)
        {
            rows_w = canvas_w - rows_x - margin_x;
            if (!detail_leading)
                rows_w = detail_x - column_gap - rows_x;
        }
        else
        {
            rows_w = canvas_w - margin_x * 2;
        }
        if (rows_w < 1)
            rows_w = 1;

        row_clip.x = rows_x;
        row_clip.y = content_top;
        row_clip.w = rows_w;
        row_clip.h = MAX(0, content_bottom - content_top);

        SDL_SetRenderClipRect(g_state.renderer, &row_clip);
        for (i = 0; i < (u16b)row_visible; i++)
        {
            const app_ui_row* row = &ui_panel->rows[row_start + i];

            sdl_menu_render_browser_row(font, ui_panel, row, &row_clip, line_h,
                sdl_menu_scale_px(10.0f), row_y);
            row_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (status_h > 0)
    {
        int y = status_y;

        for (i = 0; i < ui_panel->body_line_count; i++)
        {
            sdl_menu_render_text(font, (float)margin_x, (float)y, line_h,
                sdl_menu_color(ui_panel->body_lines[i].attr),
                ui_panel->body_lines[i].text);
            y += line_h + line_gap;
        }
    }

    if (footer_h > 0)
    {
        sdl_menu_render_browser_footer(font, ui_panel, margin_x, footer_y,
            MAX(1, canvas_w - margin_x * 2), line_h, line_gap, item_gap);
    }

    return true;
}

static bool sdl_menu_render_panel_internal(const sdl_view* main_view,
    const app_ui_scene* scene, const app_ui_panel* ui_panel)
{
    TTF_Font* font;
    TTF_Font* story_font = NULL;
    SDL_Color panel_fill;
    SDL_Color panel_border;
    SDL_FRect panel;
    SDL_Rect left_clip;
    SDL_Rect right_clip;
    SDL_Rect footer_clip;
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;
    int line_gap;
    int section_gap;
    int item_gap;
    int pad_x;
    int pad_y;
    int outer_margin;
    int column_gap;
    int pill_gap;
    int pill_pad_x;
    int pill_pad_y;
    int top_h = 0;
    int footer_h = 0;
    int footer_space = 0;
    int column_space = 0;
    int available_column_h;
    int header_h = 0;
    int body_h = 0;
    int rich_h = 0;
    int rows_h = 0;
    int detail_h = 0;
    int left_w = 0;
    int right_w = 0;
    int total_w;
    int max_w;
    int min_w;
    int title_w = 0;
    int subtitle_w = 0;
    int body_w = 0;
    int rows_w = 0;
    int detail_w = 0;
    int footer_w = 0;
    int tabs_w = 0;
    int tabs_h = 0;
    int title_icon_slot_w = 0;
    int title_icon_h = 0;
    int paragraph_gap;
    int row_area_gap = 0;
    int row_start = 0;
    int row_visible = 0;
    int current_y;
    bool has_detail;
    bool has_top;
    bool has_columns;
    bool has_footer;
    u16b i;

    if (!main_view || !scene || !ui_panel)
        return false;

    if (sdl_menu_is_plain_panel(ui_panel)
        && sdl_menu_render_term_plain_panel(main_view, ui_panel))
    {
        return true;
    }

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_scale_px((float)sdl_menu_font_size_logical(main_view));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;
    story_font = sdl_story_font_for_height(pixel_height);
    if (!story_font)
        story_font = font;

    line_h = pixel_height;
    if (line_h <= 0)
        line_h = TTF_GetFontHeight(font);
    line_gap = sdl_menu_scale_px(2.0f);
    section_gap = sdl_menu_scale_px(12.0f);
    item_gap = sdl_menu_scale_px(10.0f);
    pad_x = sdl_menu_scale_px((ui_panel->style == APP_UI_PANEL_STYLE_PLAIN)
        ? 16.0f : 18.0f);
    pad_y = sdl_menu_scale_px((ui_panel->style == APP_UI_PANEL_STYLE_PLAIN)
        ? 12.0f : 16.0f);
    outer_margin = sdl_menu_scale_px(24.0f);
    column_gap = sdl_menu_scale_px(24.0f);
    pill_gap = sdl_menu_scale_px(10.0f);
    pill_pad_x = sdl_menu_scale_px(10.0f);
    pill_pad_y = sdl_menu_scale_px(4.0f);
    paragraph_gap = line_h + line_gap;

    if (ui_panel->icon_char)
    {
        title_icon_h = line_h * 2;
        title_icon_slot_w = title_icon_h + item_gap;
    }
    if (ui_panel->title[0])
        title_w = sdl_menu_measure_text(font, ui_panel->title);
    if (ui_panel->subtitle[0])
        subtitle_w = sdl_menu_measure_text(font, ui_panel->subtitle);
    if (title_icon_slot_w > 0 && (ui_panel->title[0] || ui_panel->subtitle[0]))
    {
        title_w += title_icon_slot_w;
        subtitle_w += title_icon_slot_w;
    }
    for (i = 0; i < ui_panel->body_line_count; i++)
        body_w = MAX(body_w, sdl_menu_measure_text(font,
            ui_panel->body_lines[i].text));
    for (i = 0; i < ui_panel->row_count; i++)
        rows_w = MAX(rows_w, sdl_menu_measure_row(font, ui_panel,
            &ui_panel->rows[i], item_gap));
    if (ui_panel->detail_title[0])
        detail_w = sdl_menu_measure_text(font, ui_panel->detail_title);
    for (i = 0; i < ui_panel->detail_line_count; i++)
        detail_w = MAX(detail_w, sdl_menu_measure_text(font,
            ui_panel->detail_lines[i].text));
    footer_w = sdl_menu_measure_footer(font, ui_panel, pill_gap, pill_pad_x);
    tabs_w = sdl_menu_measure_tabs(font, ui_panel, pill_gap, pill_pad_x);

    left_w = MAX(MAX(title_w, subtitle_w), MAX(body_w, rows_w));
    left_w = MAX(left_w, footer_w);
    left_w = MAX(left_w, tabs_w);
    if (left_w == 0)
        left_w = sdl_menu_scale_px(220.0f);

    has_detail = ((ui_panel->flags & APP_UI_PANEL_FLAG_SHOW_DETAIL) != 0)
        && (ui_panel->detail_line_count > 0 || ui_panel->detail_title[0]);
    if (has_detail && right_w == 0)
        right_w = MAX(detail_w, sdl_menu_scale_px(180.0f));
    else
        right_w = detail_w;

    max_w = canvas_w - outer_margin * 2;
    min_w = ui_panel->min_width_px
        ? sdl_menu_scale_px((float)ui_panel->min_width_px)
        : sdl_menu_scale_px(260.0f);
    if (ui_panel->width_cap_px > 0)
        max_w = MIN(max_w, sdl_menu_scale_px(
            (float)ui_panel->width_cap_px));
    if (max_w < sdl_menu_scale_px(180.0f))
        max_w = sdl_menu_scale_px(180.0f);

    if (!has_detail && ui_panel->rich_paragraph_count > 0)
    {
        int preferred_left_w = min_w - pad_x * 2;

        if (preferred_left_w > left_w)
            left_w = preferred_left_w;
    }

    total_w = pad_x * 2 + left_w + (has_detail ? (column_gap + right_w) : 0);

    if (total_w > max_w)
    {
        if (has_detail)
        {
            int remaining = max_w - pad_x * 2 - column_gap - left_w;

            right_w = MAX(sdl_menu_scale_px(140.0f), remaining);
            total_w = pad_x * 2 + left_w + column_gap + right_w;
        }

        if (total_w > max_w)
        {
            left_w = MAX(sdl_menu_scale_px(160.0f),
                max_w - pad_x * 2 - (has_detail ? (column_gap + right_w) : 0));
            total_w = pad_x * 2 + left_w + (has_detail ? (column_gap + right_w) : 0);
        }
    }

    if (total_w < min_w)
        total_w = min_w;
    if (total_w > canvas_w)
        total_w = canvas_w;
    if (!has_detail)
        left_w = MAX(1, total_w - pad_x * 2);

    if (ui_panel->tab_count > 0)
        tabs_h = line_h + pill_pad_y * 2;
    if (ui_panel->title[0] || ui_panel->subtitle[0])
    {
        int header_text_h = 0;

        if (ui_panel->title[0])
            header_text_h += line_h + line_gap;
        if (ui_panel->subtitle[0])
            header_text_h += line_h + line_gap;
        if (header_text_h > 0)
            header_text_h -= line_gap;
        header_h = MAX(header_text_h, title_icon_h);
    }
    if (ui_panel->body_line_count > 0)
        body_h = (ui_panel->body_line_count * line_h)
            + ((ui_panel->body_line_count - 1) * line_gap);
    if (ui_panel->rich_paragraph_count > 0)
        rich_h = sdl_menu_measure_rich_text_height(font, story_font, line_h,
            line_gap, paragraph_gap, left_w, scene, ui_panel);
    if (has_detail)
    {
        if (ui_panel->detail_title[0])
            detail_h += line_h + line_gap;
        if (ui_panel->detail_line_count > 0)
            detail_h += (ui_panel->detail_line_count * line_h)
                + ((ui_panel->detail_line_count - 1) * line_gap);
        if (ui_panel->detail_title[0] && ui_panel->detail_line_count > 0)
            detail_h -= line_gap;
    }
    if (ui_panel->footer_action_count > 0)
        footer_h = line_h + pill_pad_y * 2;

    has_top = (tabs_h > 0 || header_h > 0);
    has_footer = (footer_h > 0);
    has_columns = (body_h > 0 || rich_h > 0 || ui_panel->row_count > 0
        || detail_h > 0);

    top_h = tabs_h;
    if (tabs_h > 0 && header_h > 0)
        top_h += section_gap;
    top_h += header_h;

    footer_space = (has_footer && (has_top || has_columns)) ? section_gap : 0;
    column_space = (has_columns && has_top) ? section_gap : 0;
    available_column_h = canvas_h - outer_margin * 2 - pad_y * 2 - top_h
        - column_space - footer_space - footer_h;
    if (available_column_h < line_h)
        available_column_h = line_h;

    if (ui_panel->row_count > 0)
    {
        int row_area_available = available_column_h;

        row_area_gap = ((body_h > 0) || (rich_h > 0)) ? section_gap : 0;
        row_area_available -= body_h + rich_h + row_area_gap;
        if (row_area_available < line_h)
            row_area_available = line_h;

        row_visible = (row_area_available + line_gap) / (line_h + line_gap);
        if (row_visible < 1)
            row_visible = 1;
        if (row_visible > (int)ui_panel->row_count)
            row_visible = ui_panel->row_count;

        row_start = ui_panel->row_offset;
        if (row_start < 0)
            row_start = 0;
        if (ui_panel->selected_row >= 0
            && ui_panel->selected_row < (s16b)ui_panel->row_count)
        {
            if (ui_panel->selected_row < row_start)
                row_start = ui_panel->selected_row;
            if (ui_panel->selected_row >= row_start + row_visible)
                row_start = ui_panel->selected_row - row_visible + 1;
        }
        if (row_start + row_visible > (int)ui_panel->row_count)
            row_start = ui_panel->row_count - row_visible;
        if (row_start < 0)
            row_start = 0;

        rows_h = row_visible * line_h + (row_visible - 1) * line_gap;
    }

    detail_h = MIN(detail_h, available_column_h);
    column_space = has_columns && has_top ? section_gap : 0;
    column_space += has_columns ? MAX(body_h + rich_h + row_area_gap + rows_h,
        detail_h) : 0;
    footer_space = (has_footer && (has_top || has_columns)) ? section_gap : 0;

    panel.w = (float)MIN(total_w, max_w);
    panel.h = (float)(pad_y * 2 + top_h + column_space + footer_space + footer_h);
    if (panel.h < (float)sdl_menu_scale_px(72.0f))
        panel.h = (float)sdl_menu_scale_px(72.0f);
    if (panel.h > (float)(canvas_h - outer_margin * 2))
        panel.h = (float)(canvas_h - outer_margin * 2);

    panel.x = (float)((canvas_w - (int)panel.w) / 2);
    if (ui_panel->flags & APP_UI_PANEL_FLAG_LEFT_ANCHORED)
        panel.x = (float)outer_margin;

    if (ui_panel->flags & APP_UI_PANEL_FLAG_TOP_ANCHORED)
        panel.y = (float)outer_margin;
    else if (ui_panel->flags & APP_UI_PANEL_FLAG_BOTTOM_ANCHORED)
        panel.y = (float)(canvas_h - outer_margin - (int)panel.h);
    else
        panel.y = (float)((canvas_h - (int)panel.h) / 2);

    panel_fill = (ui_panel->style == APP_UI_PANEL_STYLE_PLAIN)
        ? (SDL_Color){ 0, 0, 0, 232 }
        : (SDL_Color){ 10, 18, 26, 224 };
    panel_border = sdl_menu_color(ui_panel->accent_attr
        ? ui_panel->accent_attr
        : TERM_L_BLUE);
    panel_border.a = 220;

    sdl_menu_fill_rect(&panel, panel_fill);
    if (ui_panel->style != APP_UI_PANEL_STYLE_PLAIN)
        sdl_menu_draw_rect(&panel, panel_border);

    left_clip.x = (int)panel.x + pad_x;
    left_clip.y = (int)panel.y + pad_y;
    left_clip.w = (int)panel.w - pad_x * 2;
    left_clip.h = (int)panel.h - pad_y * 2;
    right_clip = left_clip;
    if (has_detail)
    {
        left_clip.w = left_w;
        right_clip.x = left_clip.x + left_w + column_gap;
        right_clip.w = MAX(0, (int)panel.w - pad_x * 2 - left_w - column_gap);
    }
    footer_clip.x = (int)panel.x + pad_x;
    footer_clip.w = (int)panel.w - pad_x * 2;
    footer_clip.h = footer_h;
    footer_clip.y = (int)(panel.y + panel.h) - pad_y - footer_h;

    current_y = left_clip.y;
    if (ui_panel->tab_count > 0)
    {
        SDL_Rect tabs_clip = {
            left_clip.x,
            current_y,
            (int)panel.w - pad_x * 2,
            tabs_h
        };

        sdl_menu_render_tabs(font, ui_panel, &tabs_clip, line_h,
            pill_gap, pill_pad_x, pill_pad_y);
        current_y += tabs_h;
        if (header_h > 0)
            current_y += section_gap;
    }

    if (ui_panel->title[0] || ui_panel->subtitle[0])
    {
        int header_y = current_y;
        int text_x = left_clip.x;
        int text_y = current_y;

        if (ui_panel->icon_char
            && ui_panel->style != APP_UI_PANEL_STYLE_PLAIN)
        {
            sdl_menu_render_icon(font, (float)left_clip.x,
                (float)(current_y + (header_h - title_icon_h) / 2),
                title_icon_h, title_icon_h, ui_panel->icon_attr,
                ui_panel->icon_char);
            text_x += title_icon_slot_w;
        }

        if (ui_panel->title[0])
        {
            sdl_menu_render_text(font, (float)text_x, (float)text_y,
                line_h, sdl_menu_color(ui_panel->title_attr),
                ui_panel->title);
            if (ui_panel->icon_char
                && ui_panel->style == APP_UI_PANEL_STYLE_PLAIN)
            {
                int title_text_w = sdl_menu_measure_text(font, ui_panel->title);

                sdl_menu_render_icon(font,
                    (float)(text_x + title_text_w + item_gap * 0.5f),
                    (float)(current_y + (header_h - title_icon_h) / 2),
                    title_icon_h, title_icon_h, ui_panel->icon_attr,
                    ui_panel->icon_char);
            }
            text_y += line_h + line_gap;
        }
        else if (ui_panel->icon_char)
        {
            sdl_menu_render_icon(font, (float)left_clip.x,
                (float)(current_y + (header_h - title_icon_h) / 2),
                title_icon_h, title_icon_h, ui_panel->icon_attr,
                ui_panel->icon_char);
            text_x += title_icon_slot_w;
        }
        if (ui_panel->subtitle[0])
        {
            sdl_menu_render_text(font, (float)text_x, (float)text_y,
                line_h, sdl_menu_color(ui_panel->subtitle_attr),
                ui_panel->subtitle);
        }
        current_y = header_y + header_h;
    }

    if (has_columns)
    {
        if (ui_panel->title[0] || ui_panel->subtitle[0])
            current_y += section_gap - line_gap;
        else if (ui_panel->tab_count > 0)
            current_y += section_gap;
    }

    if (body_h > 0 || rich_h > 0 || rows_h > 0)
    {
        SDL_Rect column_clip = {
            left_clip.x,
            current_y,
            left_clip.w,
            footer_clip.y - current_y - ((has_footer) ? section_gap : 0)
        };

        if (column_clip.h < 0)
            column_clip.h = 0;
        SDL_SetRenderClipRect(g_state.renderer, &column_clip);
        for (i = 0; i < ui_panel->body_line_count; i++)
        {
            sdl_menu_render_text(font, (float)left_clip.x, (float)current_y,
                line_h, sdl_menu_color(ui_panel->body_lines[i].attr),
                ui_panel->body_lines[i].text);
            current_y += line_h + line_gap;
        }

        if (ui_panel->body_line_count > 0 && rich_h > 0)
            current_y += line_gap;
        if (ui_panel->rich_paragraph_count > 0)
            current_y += sdl_menu_render_rich_text(scene, ui_panel, font,
                story_font, &column_clip, line_h, line_gap, paragraph_gap,
                current_y);

        if (ui_panel->body_line_count > 0 && rows_h > 0)
            current_y += section_gap - line_gap;
        else if (rich_h > 0 && rows_h > 0)
            current_y += section_gap;

        if (row_start > 0)
        {
            sdl_menu_render_text(font,
                (float)(left_clip.x + left_clip.w - sdl_menu_scale_px(10.0f)),
                (float)current_y, line_h,
                sdl_menu_color(ui_panel->accent_attr), "^");
        }

        for (i = 0; i < (u16b)row_visible; i++)
        {
            const app_ui_row* row = &ui_panel->rows[row_start + i];

            sdl_menu_render_row(font, ui_panel, row, &column_clip, line_h,
                item_gap, current_y, ui_panel->accent_attr);
            current_y += line_h + line_gap;
        }

        if (row_start + row_visible < (int)ui_panel->row_count)
        {
            sdl_menu_render_text(font,
                (float)(left_clip.x + left_clip.w - sdl_menu_scale_px(10.0f)),
                (float)(current_y - line_gap), line_h,
                sdl_menu_color(ui_panel->accent_attr), "v");
        }

        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (has_detail)
    {
        int detail_y = left_clip.y + top_h + (has_top ? section_gap : 0);
        SDL_Rect detail_clip_rect = {
            right_clip.x,
            detail_y,
            right_clip.w,
            footer_clip.y - detail_y - ((has_footer) ? section_gap : 0)
        };

        if (detail_clip_rect.h < 0)
            detail_clip_rect.h = 0;
        SDL_SetRenderClipRect(g_state.renderer, &detail_clip_rect);
        if (ui_panel->detail_title[0])
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_color(ui_panel->detail_title_attr),
                ui_panel->detail_title);
            detail_y += line_h + line_gap;
        }
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_color(ui_panel->detail_lines[i].attr),
                ui_panel->detail_lines[i].text);
            detail_y += line_h + line_gap;
        }
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    if (has_footer)
    {
        sdl_menu_render_footer(font, ui_panel, &footer_clip, line_h,
            pill_gap, pill_pad_x, pill_pad_y);
    }

    return true;
}

static const char* sdl_menu_status_rail_label_text(const app_ui_row* row)
{
    if (!row)
        return "";

    if (row->key[0])
        return row->key;

    return row->label;
}

static int sdl_menu_status_rail_gap_px(TTF_Font* mono_font)
{
    int gap_px = sdl_menu_measure_text(mono_font, " ");

    if (gap_px < 4)
        gap_px = 4;

    return gap_px;
}

static int sdl_menu_status_rail_icon_slot_px(TTF_Font* mono_font, int line_h)
{
    return sdl_menu_icon_slot_px(mono_font, line_h);
}

static int sdl_menu_status_rail_label_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, const app_ui_row* row, cptr text)
{
    if (!text || !text[0])
        return 0;
    if ((row->flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
        return sdl_menu_measure_text(story_font, text);

    return sdl_menu_measure_text(mono_font, text);
}

static int sdl_menu_status_rail_row_width_px(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, const app_ui_row* row)
{
    const char* label_text = sdl_menu_status_rail_label_text(row);
    int icon_slot_w;
    int gap_px;
    int label_w;
    int meta_w;
    int width;

    if (!mono_font || !row)
        return 0;

    icon_slot_w = sdl_menu_status_rail_icon_slot_px(mono_font, line_h);
    gap_px = sdl_menu_status_rail_gap_px(mono_font);
    label_w = sdl_menu_status_rail_label_width_px(mono_font, story_font, row,
        label_text);
    meta_w = sdl_menu_measure_text(mono_font, row->meta);

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        return label_w;

    if (row->extra_icon_char)
    {
        width = label_w + icon_slot_w + meta_w;
        if (row->icon_char)
            width += icon_slot_w;
        if (label_w > 0 && meta_w > 0)
            width += gap_px;
        return width;
    }

    width = label_w;
    if (row->icon_char)
    {
        width += icon_slot_w;
        if (label_w > 0)
            width += gap_px;
    }
    if (row->meta[0])
    {
        if (width > 0)
            width += gap_px;
        width += meta_w;
    }

    return width;
}

static void sdl_menu_render_status_rail_icon(TTF_Font* mono_font, float x_px,
    float y_px, int icon_slot_w, int line_h, byte icon_attr, char icon_char)
{
    sdl_menu_render_icon(mono_font, x_px, y_px, icon_slot_w, line_h,
        icon_attr, icon_char);
}

static void sdl_menu_render_icon(TTF_Font* font, float x_px, float y_px,
    int icon_slot_w, int line_h, byte icon_attr, char icon_char)
{
    SDL_FRect tile_dst;
    byte ch = (byte)icon_char;

    if (!icon_char || icon_char == ' ')
        return;

    if (g_state.use_tiles && g_state.tileset
        && (icon_attr & TILE_FLAG) && (ch & TILE_FLAG))
    {
        float tile_size = (float)MIN(line_h, icon_slot_w);

        tile_dst.x = x_px + ((float)icon_slot_w - tile_size) * 0.5f;
        tile_dst.y = y_px + ((float)line_h - tile_size) * 0.5f;
        tile_dst.w = tile_size;
        tile_dst.h = tile_size;
        sdl_menu_draw_tile(icon_attr, ch, &tile_dst);
        return;
    }

    {
        char glyph[2] = { icon_char, '\0' };
        int glyph_w = sdl_menu_measure_text(font, glyph);
        float text_x = x_px;

        if (glyph_w < icon_slot_w)
            text_x += ((float)icon_slot_w - (float)glyph_w) * 0.5f;
        sdl_menu_render_text(font, text_x, y_px, line_h,
            sdl_menu_color(icon_attr ? icon_attr : TERM_WHITE), glyph);
    }
}

static void sdl_menu_render_status_rail_label(TTF_Font* mono_font,
    TTF_Font* story_font, float x_px, float y_px, int line_h,
    byte attr, byte flags, cptr text)
{
    if (!text || !text[0])
        return;

    if ((flags & APP_UI_ITEM_FLAG_STORY_LABEL) && story_font)
    {
        sdl_menu_render_text(story_font, x_px, y_px, line_h,
            sdl_menu_color(attr), text);
        return;
    }

    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
        sdl_menu_color(attr), text);
}

static bool sdl_menu_render_status_rail_panel(const sdl_view* main_view,
    const app_ui_panel* panel)
{
    TTF_Font* mono_font = NULL;
    TTF_Font* story_font = NULL;
    SDL_FRect clear_rect;
    SDL_Rect clip_rect;
    int canvas_w;
    int canvas_h;
    int desired_px;
    int min_px;
    int pixel_height;
    int line_h = 0;
    int icon_slot_w = 0;
    int gap_px = 0;
    int row_top = 1;
    int panel_w_px = 0;
    int row_visible;
    int screen_rows = 0;
    u16b i;

    if (!main_view || !panel || panel->row_count == 0)
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    desired_px = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(main_view));
    min_px = sdl_menu_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        int mono_h;
        int story_h = 0;
        int candidate_w_px = 0;
        int max_w_px;
        u16b row_index;

        mono_font = sdl_ui_font_for_height(pixel_height);
        story_font = sdl_story_font_for_height(pixel_height);
        if (!mono_font)
            continue;

        mono_h = TTF_GetFontHeight(mono_font);
        if (story_font)
            story_h = TTF_GetFontHeight(story_font);
        line_h = MAX(pixel_height, MAX(mono_h, story_h));
        if (line_h < 1)
            line_h = 1;
        icon_slot_w = sdl_menu_status_rail_icon_slot_px(mono_font, line_h);
        gap_px = sdl_menu_status_rail_gap_px(mono_font);

        for (row_index = 0; row_index < panel->row_count; row_index++)
        {
            candidate_w_px = MAX(candidate_w_px,
                sdl_menu_status_rail_row_width_px(mono_font, story_font,
                    line_h, &panel->rows[row_index]));
        }
        if (panel->min_width_px > 0)
        {
            int min_w_px = sdl_menu_scale_px((float)panel->min_width_px);

            candidate_w_px = MAX(candidate_w_px, min_w_px);
        }
        max_w_px = panel->width_cap_px > 0
            ? sdl_menu_scale_px((float)panel->width_cap_px)
            : 0;
        if (max_w_px > 0 && candidate_w_px > max_w_px)
            candidate_w_px = max_w_px;

        screen_rows = canvas_h / line_h;
        if (candidate_w_px <= 0 || candidate_w_px > canvas_w
            || screen_rows <= row_top)
        {
            continue;
        }

        panel_w_px = candidate_w_px;
        break;
    }

    if (!mono_font || line_h <= 0 || panel_w_px <= 0 || screen_rows <= row_top)
    {
        return false;
    }

    row_visible = MIN((int)panel->row_count, screen_rows - row_top);
    if (row_visible <= 0)
        return false;

    clear_rect.x = 0.0f;
    clear_rect.y = (float)(row_top * line_h);
    clear_rect.w = (float)panel_w_px;
    clear_rect.h = (float)(row_visible * line_h);
    sdl_menu_fill_rect(&clear_rect, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = (int)clear_rect.x;
    clip_rect.y = (int)clear_rect.y;
    clip_rect.w = (int)clear_rect.w;
    clip_rect.h = (int)clear_rect.h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    for (i = 0; i < (u16b)row_visible; i++)
    {
        const app_ui_row* row = &panel->rows[i];
        float y_px = (float)((row_top + (int)i) * line_h);
        const char* label_text = sdl_menu_status_rail_label_text(row);
        byte label_attr = row->attr ? row->attr : TERM_WHITE;
        byte meta_attr = row->meta_attr ? row->meta_attr : label_attr;
        int label_w = sdl_menu_status_rail_label_width_px(mono_font,
            story_font, row, label_text);
        int meta_w = sdl_menu_measure_text(mono_font, row->meta);

        if (row->flags & APP_UI_ITEM_FLAG_SECTION)
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, 0.0f,
                y_px, line_h, label_attr, row->flags,
                row->label[0] ? row->label : row->key);
            continue;
        }

        if (row->extra_icon_char)
        {
            int group_w = label_w + icon_slot_w + meta_w;
            float x_px;

            if (row->icon_char)
            {
                group_w += icon_slot_w;
                if (label_w > 0 && meta_w > 0)
                    group_w += gap_px;
                x_px = (float)panel_w_px - (float)group_w;
                if (x_px < 0.0f)
                    x_px = 0.0f;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->icon_attr, row->icon_char);
                x_px += (float)icon_slot_w;
                if (row->label[0])
                {
                    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
                        sdl_menu_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)gap_px;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->extra_icon_attr,
                    row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_menu_render_text(mono_font,
                        x_px + (float)icon_slot_w, y_px, line_h,
                        sdl_menu_color(meta_attr), row->meta);
                }
            }
            else
            {
                if (label_w > 0 && meta_w > 0)
                    group_w += gap_px;
                x_px = (float)panel_w_px - (float)group_w;
                if (x_px < 0.0f)
                    x_px = 0.0f;
                if (row->label[0])
                {
                    sdl_menu_render_text(mono_font, x_px, y_px, line_h,
                        sdl_menu_color(label_attr), row->label);
                }
                x_px += (float)label_w;
                if (label_w > 0 && meta_w > 0)
                    x_px += (float)gap_px;
                sdl_menu_render_status_rail_icon(mono_font, x_px, y_px,
                    icon_slot_w, line_h, row->extra_icon_attr,
                    row->extra_icon_char);
                if (row->meta[0])
                {
                    sdl_menu_render_text(mono_font,
                        x_px + (float)icon_slot_w, y_px, line_h,
                        sdl_menu_color(meta_attr), row->meta);
                }
            }
            continue;
        }

        if (row->icon_char)
        {
            sdl_menu_render_status_rail_icon(mono_font, 0.0f, y_px,
                icon_slot_w, line_h, row->icon_attr, row->icon_char);
            if (row->label[0])
            {
                sdl_menu_render_status_rail_label(mono_font, story_font,
                    (float)(icon_slot_w + gap_px), y_px, line_h, label_attr,
                    row->flags, row->label);
            }
            if (row->meta[0])
            {
                float meta_x = (float)panel_w_px - (float)meta_w;
                float min_meta_x = (row->label[0]
                    ? (float)(icon_slot_w + gap_px + label_w + gap_px)
                    : (float)icon_slot_w);

                if (meta_x < min_meta_x)
                    meta_x = min_meta_x;
                sdl_menu_render_text(mono_font, meta_x, y_px, line_h,
                    sdl_menu_color(meta_attr), row->meta);
            }
            continue;
        }

        if (label_text[0])
        {
            sdl_menu_render_status_rail_label(mono_font, story_font, 0.0f,
                y_px, line_h, label_attr, row->flags, label_text);
        }
        if (row->meta[0])
        {
            float meta_x = (float)panel_w_px - (float)meta_w;

            if (label_text[0] && meta_x < (float)(label_w + gap_px))
                meta_x = (float)(label_w + gap_px);
            if (meta_x < 0.0f)
                meta_x = 0.0f;
            sdl_menu_render_text(mono_font, meta_x, y_px, line_h,
                sdl_menu_color(meta_attr), row->meta);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

static const app_ui_panel* sdl_menu_pick_ui_panel(const app_ui_scene* scene)
{
    u16b i;

    if (!scene)
        return NULL;

    for (i = 0; i < scene->panel_count; i++)
    {
        const app_ui_panel* panel = &scene->panels[i];

        if (panel->flags & APP_UI_PANEL_FLAG_ACTIVE)
            return panel;
    }

    return NULL;
}

static bool sdl_menu_render_strip_panel(const sdl_view* main_view,
    const app_ui_panel* panel)
{
    TTF_Font* font;
    SDL_Rect clip_rect;
    int canvas_w;
    int canvas_h;
    int pixel_height;
    int line_h;
    int rows;
    int strip_h;
    int current_y;
    u16b i;
    float y_px;

    if (!main_view || !panel)
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    pixel_height = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(main_view));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;

    line_h = MAX(pixel_height, TTF_GetFontHeight(font));
    rows = panel->body_line_count ? (int)panel->body_line_count : 1;
    strip_h = rows * line_h;
    if (strip_h < main_view->cell_h)
        strip_h = main_view->cell_h;
    if (strip_h > canvas_h)
        strip_h = canvas_h;

    y_px = (panel->flags & APP_UI_PANEL_FLAG_BOTTOM_ANCHORED)
        ? (float)(canvas_h - strip_h)
        : 0.0f;

    sdl_menu_fill_rect(&(SDL_FRect){
        .x = 0.0f,
        .y = y_px,
        .w = (float)canvas_w,
        .h = (float)strip_h
    }, (SDL_Color){ 0, 0, 0, 255 });

    clip_rect.x = 0;
    clip_rect.y = (int)y_px;
    clip_rect.w = canvas_w;
    clip_rect.h = strip_h;
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    current_y = (int)y_px + ((strip_h - rows * line_h) / 2);
    for (i = 0; i < panel->body_line_count; i++)
    {
        const app_ui_text_line* line = &panel->body_lines[i];

        if (line->text[0] && line->text[0] != ' ')
        {
            sdl_menu_render_text(font, 0.0f, (float)current_y, line_h,
                sdl_menu_color(line->attr), line->text);
        }
        current_y += line_h;
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    return true;
}

typedef struct sdl_menu_document_metrics {
    int origin_x;
    int origin_y;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    TTF_Font* mono_font;
    TTF_Font* story_font;
} sdl_menu_document_metrics;

static int sdl_menu_measure_text_n(TTF_Font* font, cptr text, size_t len)
{
    int measured_w = 0;

    if (!font || !text || len == 0)
        return 0;

    if (!TTF_MeasureString(font, text, len, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

static int sdl_menu_document_cell_width(int cell_h, TTF_Font* mono_font,
    TTF_Font* story_font)
{
    int cell_w = (int)((float)cell_h * 0.57f + 0.5f);
    int mono_m_w = sdl_menu_measure_text_n(mono_font, "M", 1);
    int story_m_w = sdl_menu_measure_text_n(story_font, "M", 1);

    if (mono_m_w > cell_w)
        cell_w = mono_m_w;
    if (story_m_w > cell_w)
        cell_w = story_m_w;
    if (cell_w < 1)
        cell_w = 1;

    return cell_w;
}

static bool sdl_menu_panel_has_document(const app_ui_scene* scene,
    const app_ui_panel* panel)
{
    if (!scene || !panel || panel->document_op_count == 0)
        return false;

    return (u16b)(panel->document_op_first + panel->document_op_count)
        <= scene->document_op_count;
}

static bool sdl_menu_resolve_document_metrics(const sdl_view* main_view,
    const app_ui_panel* panel,
    sdl_menu_document_metrics* metrics)
{
    int canvas_w;
    int canvas_h;
    int desired_px;
    int min_px;
    int pixel_height;
    int rows;
    int cols;
    int margin_x;
    int margin_y;
    TTF_Font* fallback_mono = NULL;
    TTF_Font* fallback_story = NULL;
    int fallback_cell_w = 0;
    int fallback_cell_h = 0;

    if (!main_view || !panel || !metrics)
        return false;

    rows = panel->document_rows ? panel->document_rows : 1;
    cols = panel->document_cols ? panel->document_cols : 1;
    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    desired_px = sdl_menu_scale_px(
        (float)sdl_menu_font_size_logical(main_view));
    min_px = sdl_menu_scale_px(10.0f);
    if (min_px < 10)
        min_px = 10;
    if (desired_px < min_px)
        desired_px = min_px;

    margin_x = sdl_menu_scale_px(2.0f);
    margin_y = 0;

    for (pixel_height = desired_px; pixel_height >= min_px; pixel_height--)
    {
        TTF_Font* mono_font = sdl_ui_font_for_height(pixel_height);
        TTF_Font* story_font = sdl_story_font_for_height(pixel_height);
        int mono_h;
        int story_h = 0;
        int cell_h;
        int cell_w;

        if (!mono_font)
            continue;
        if (!story_font)
            story_font = mono_font;

        mono_h = TTF_GetFontHeight(mono_font);
        if (story_font)
            story_h = TTF_GetFontHeight(story_font);
        cell_h = MAX(pixel_height, MAX(mono_h, story_h));
        cell_w = sdl_menu_document_cell_width(cell_h, mono_font, story_font);

        fallback_mono = mono_font;
        fallback_story = story_font;
        fallback_cell_w = cell_w;
        fallback_cell_h = cell_h;

        if ((cols * cell_w) <= (canvas_w - margin_x * 2)
            && (rows * cell_h) <= (canvas_h - margin_y * 2))
        {
            metrics->origin_x = margin_x;
            metrics->origin_y = margin_y;
            metrics->cell_w = cell_w;
            metrics->cell_h = cell_h;
            metrics->cols = cols;
            metrics->rows = rows;
            metrics->mono_font = mono_font;
            metrics->story_font = story_font;
            return true;
        }
    }

    if (!fallback_mono)
        return false;

    metrics->origin_x = margin_x;
    metrics->origin_y = margin_y;
    metrics->cell_w = fallback_cell_w;
    metrics->cell_h = fallback_cell_h;
    metrics->cols = cols;
    metrics->rows = rows;
    metrics->mono_font = fallback_mono;
    metrics->story_font = fallback_story ? fallback_story : fallback_mono;
    return true;
}

static int sdl_menu_render_document_text_run_px(TTF_Font* font, float x_px,
    float y_px, SDL_Color color, cptr text, size_t len, int target_h,
    float max_w_px)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    SDL_Rect previous_clip;
    SDL_Rect clip_rect;
    SDL_Rect* clip_ptr = NULL;
    bool had_clip = false;
    char buf[APP_UI_TEXT_MAX + 1];
    int advance_w;
    float scale = 1.0f;
    size_t copy_len = len;

    if (!font || !text || len == 0)
        return 0;

    if (copy_len >= sizeof(buf))
        copy_len = sizeof(buf) - 1;
    memcpy(buf, text, copy_len);
    buf[copy_len] = '\0';

    surface = TTF_RenderText_Blended(font, buf, 0, color);
    if (!surface)
        return 0;

    advance_w = sdl_menu_measure_text_n(font, buf, copy_len);
    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    if (target_h > 0 && surface->h > target_h)
    {
        scale = (float)target_h / (float)surface->h;
        dst.w *= scale;
        dst.h *= scale;
        advance_w = (int)((float)advance_w * scale + 0.5f);
    }
    if (target_h > 0 && dst.h < (float)target_h)
        dst.y += ((float)target_h - dst.h) * 0.5f;

    if (max_w_px > 0.0f)
    {
        if ((float)advance_w > max_w_px)
            advance_w = (int)max_w_px;
        if (dst.w > max_w_px)
        {
            clip_rect.x = (int)x_px;
            clip_rect.y = (int)y_px;
            clip_rect.w = (int)(max_w_px + 0.999f);
            clip_rect.h = (int)(dst.h + 0.999f);
            if (clip_rect.w > 0 && clip_rect.h > 0)
                clip_ptr = &clip_rect;
        }
    }

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (texture)
    {
        if (clip_ptr)
        {
            had_clip = SDL_GetRenderClipRect(g_state.renderer, &previous_clip);
            SDL_SetRenderClipRect(g_state.renderer, clip_ptr);
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
        if (clip_ptr)
            SDL_SetRenderClipRect(g_state.renderer,
                had_clip ? &previous_clip : NULL);
        SDL_DestroyTexture(texture);
    }

    SDL_DestroySurface(surface);
    return advance_w;
}

static float sdl_menu_measure_document_text_run_px(TTF_Font* font, cptr text,
    size_t len, int target_h, float max_w_px)
{
    int width_px;
    int font_h;
    float scaled_w;

    if (!font || !text || len == 0)
        return 0.0f;

    width_px = sdl_menu_measure_text_n(font, text, len);
    if (width_px <= 0)
        return 0.0f;

    scaled_w = (float)width_px;
    font_h = TTF_GetFontHeight(font);
    if (target_h > 0 && font_h > target_h)
        scaled_w *= (float)target_h / (float)font_h;

    if (max_w_px > 0.0f && scaled_w > max_w_px)
        scaled_w = max_w_px;

    return scaled_w;
}

static void sdl_menu_render_document_text(
    const sdl_menu_document_metrics* metrics, const app_ui_document_op* op)
{
    SDL_FRect background;
    char text_buf[APP_UI_TEXT_MAX];
    SDL_Color color;
    bool use_story;
    bool grid_align;
    size_t len;
    TTF_Font* run_font;
    float background_w;
    float max_run_w;
    float x_px;
    float y_px;

    if (!metrics || !op || op->kind != APP_UI_DOCUMENT_OP_TEXT || !op->text[0])
        return;
    if (op->row < 0 || op->row >= metrics->rows || op->col >= metrics->cols)
        return;

    len = strlen(op->text);
    if (len == 0)
        return;
    if (op->col < 0)
        return;
    if ((size_t)op->col + len > (size_t)metrics->cols)
        len = (size_t)(metrics->cols - op->col);
    if (len == 0)
        return;
    memcpy(text_buf, op->text, len);
    text_buf[len] = '\0';

    color = sdl_menu_color(op->attr);
    use_story = (op->story & STORY_FLAG_USE) != 0;
    grid_align = (op->story & STORY_FLAG_CELL_ALIGN) != 0;
    x_px = (float)(metrics->origin_x + op->col * metrics->cell_w);
    y_px = (float)(metrics->origin_y + op->row * metrics->cell_h);
    max_run_w = (float)(len * metrics->cell_w);
    run_font = (use_story && metrics->story_font)
        ? metrics->story_font
        : metrics->mono_font;
    background_w = max_run_w;
    if (!grid_align && run_font)
    {
        float measured_w = sdl_menu_measure_document_text_run_px(run_font,
            text_buf, len, metrics->cell_h, max_run_w);

        if (measured_w > 0.0f)
            background_w = measured_w;
    }

    background.x = x_px;
    background.y = y_px;
    background.w = background_w;
    background.h = (float)metrics->cell_h;
    sdl_menu_fill_rect(&background, (SDL_Color){ 0, 0, 0, 255 });

    if (grid_align)
    {
        sdl_menu_render_fixed_text(run_font, x_px, y_px, metrics->cell_w,
            metrics->cell_h, color, text_buf);
        return;
    }

    (void)sdl_menu_render_document_text_run_px(run_font, x_px, y_px, color,
        text_buf, len, metrics->cell_h, max_run_w);
}

static void sdl_menu_render_document_cell(
    const sdl_menu_document_metrics* metrics, const app_ui_document_op* op)
{
    SDL_FRect dst;
    byte width;

    if (!metrics || !op || op->kind != APP_UI_DOCUMENT_OP_CELL)
        return;
    if (op->row < 0 || op->row >= metrics->rows || op->col < 0
        || op->col >= metrics->cols)
    {
        return;
    }

    width = op->width ? op->width : 1;
    if ((int)op->col + width > metrics->cols)
        width = (byte)MAX(1, metrics->cols - op->col);

    dst.x = (float)(metrics->origin_x + op->col * metrics->cell_w);
    dst.y = (float)(metrics->origin_y + op->row * metrics->cell_h);
    dst.w = (float)(metrics->cell_w * width);
    dst.h = (float)metrics->cell_h;
    sdl_menu_fill_rect(&dst, (SDL_Color){ 0, 0, 0, 255 });

    if (sdl_menu_document_cell_is_raw(op->attr, op->ch, op->terrain_attr,
            op->terrain_char))
    {
        if (g_state.use_tiles && g_state.tileset)
        {
            if ((op->terrain_attr & TILE_FLAG)
                && (((byte)op->terrain_char) & TILE_FLAG))
            {
                sdl_menu_draw_tile(op->terrain_attr, (byte)op->terrain_char,
                    &dst);
            }
            if (op->attr & GRAPHICS_GLOW_MASK)
                sdl_menu_draw_misc_icon(&dst, ICON_GLOW);
            sdl_menu_draw_tile(op->attr, (byte)op->ch, &dst);
            if (op->terrain_attr & GRAPHICS_SLEEP_MASK)
                sdl_menu_draw_misc_icon(&dst, ICON_SLEEPING);
            if (((byte)op->terrain_char) & GRAPHICS_SEEN_MASK)
                sdl_menu_draw_misc_icon(&dst, ICON_MONSTER_SEES_PLAYER);
            if (((byte)op->ch) & GRAPHICS_ALERT_MASK)
                sdl_menu_draw_misc_icon(&dst, ICON_ALERT);
        }
        return;
    }

    if (op->ch)
    {
        int draw_w = metrics->cell_w * width;

        sdl_menu_render_fixed_glyph(metrics->mono_font, dst.x, dst.y, draw_w,
            metrics->cell_h, sdl_menu_color(op->attr), op->ch);
    }
}

static void sdl_menu_render_document_cursor(
    const sdl_menu_document_metrics* metrics, const app_ui_document_op* op)
{
    SDL_FRect rect;
    SDL_Color color;
    byte width;

    if (!metrics || !op || op->kind != APP_UI_DOCUMENT_OP_CURSOR)
        return;
    if (op->row < 0 || op->row >= metrics->rows || op->col < 0
        || op->col >= metrics->cols)
    {
        return;
    }

    width = op->width ? op->width : 1;
    if ((int)op->col + width > metrics->cols)
        width = (byte)MAX(1, metrics->cols - op->col);

    rect.x = (float)(metrics->origin_x + op->col * metrics->cell_w);
    rect.y = (float)(metrics->origin_y + op->row * metrics->cell_h);
    rect.w = (float)(metrics->cell_w * width);
    rect.h = (float)metrics->cell_h;
    color = sdl_menu_color(op->attr);
    color.a = 220;
    sdl_menu_draw_rect(&rect, color);
}

static bool sdl_menu_render_document_panel(const sdl_view* main_view,
    const app_ui_scene* scene, const app_ui_panel* panel)
{
    sdl_menu_document_metrics metrics;
    SDL_FRect canvas_rect;
    u16b i;
    u16b start;
    u16b end;
    int canvas_w;
    int canvas_h;

    if (!main_view || !scene || !panel || !sdl_menu_panel_has_document(scene,
            panel))
    {
        return false;
    }
    if (!sdl_menu_resolve_document_metrics(main_view, panel, &metrics))
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if (!(scene->flags & APP_UI_SCENE_FLAG_USE_BACKDROP))
    {
        canvas_rect.x = 0.0f;
        canvas_rect.y = 0.0f;
        canvas_rect.w = (float)canvas_w;
        canvas_rect.h = (float)canvas_h;
        sdl_menu_fill_rect(&canvas_rect, (SDL_Color){ 0, 0, 0, 255 });
    }

    start = panel->document_op_first;
    end = (u16b)(panel->document_op_first + panel->document_op_count);
    for (i = start; i < end && i < scene->document_op_count; i++)
    {
        const app_ui_document_op* op = &scene->document_ops[i];

        if (op->kind == APP_UI_DOCUMENT_OP_TEXT)
            sdl_menu_render_document_text(&metrics, op);
        else if (op->kind == APP_UI_DOCUMENT_OP_CELL)
            sdl_menu_render_document_cell(&metrics, op);
    }

    for (i = start; i < end && i < scene->document_op_count; i++)
    {
        const app_ui_document_op* op = &scene->document_ops[i];

        if (op->kind == APP_UI_DOCUMENT_OP_CURSOR)
            sdl_menu_render_document_cursor(&metrics, op);
    }

    return true;
}

bool sdl_scene_ui_render_overlay(const sdl_view* main_view,
    const app_ui_scene* scene)
{
    const app_ui_panel* panel;
    SDL_Color scrim_color = { 0, 0, 0, 112 };
    int canvas_w;
    int canvas_h;
    u16b i;

    if (!main_view || !scene)
        return false;

    panel = sdl_menu_pick_ui_panel(scene);
    if (!panel)
        return false;

    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if ((scene->flags & APP_UI_SCENE_FLAG_DIM_BACKDROP)
        && canvas_w > 0 && canvas_h > 0)
    {
        sdl_menu_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, (float)canvas_w,
            (float)canvas_h }, scrim_color);
    }

    for (i = 0; i < scene->panel_count; i++)
    {
        panel = &scene->panels[i];
        if (!(panel->flags & APP_UI_PANEL_FLAG_ACTIVE))
            continue;

        if (panel->style == APP_UI_PANEL_STYLE_STRIP)
        {
            if (!sdl_menu_render_strip_panel(main_view, panel))
                return false;
            continue;
        }

        if (panel->style == APP_UI_PANEL_STYLE_STATUS_RAIL)
        {
            if (!sdl_menu_render_status_rail_panel(main_view, panel))
                return false;
            continue;
        }

        if (panel->style == APP_UI_PANEL_STYLE_DOCUMENT)
        {
            if (!sdl_menu_render_document_panel(main_view, scene, panel))
                return false;
            continue;
        }

        if (panel->style == APP_UI_PANEL_STYLE_BROWSER)
        {
            if (!sdl_menu_render_browser_panel(main_view, panel))
                return false;
            continue;
        }

        if (!sdl_menu_render_panel_internal(main_view, scene, panel))
            return false;
    }

    return true;
}

bool sdl_scene_ui_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_ui_scene* scene)
{
    int canvas_w;
    int canvas_h;

    if (!canvas || !main_view || !scene)
        return false;

    SDL_SetRenderTarget(g_state.renderer, canvas);
    canvas_w = main_view->cols * main_view->cell_w;
    canvas_h = main_view->rows * main_view->cell_h;
    if ((scene->flags & APP_UI_SCENE_FLAG_USE_BACKDROP)
        && main_view->canvas && canvas_w > 0 && canvas_h > 0)
    {
        SDL_RenderTexture(g_state.renderer, main_view->canvas, NULL,
            &(SDL_FRect){
                .x = 0.0f,
                .y = 0.0f,
                .w = (float)canvas_w,
                .h = (float)canvas_h
            });
    }
    else
    {
        SDL_SetRenderDrawColor(g_state.renderer, 6, 10, 14, 255);
        SDL_RenderClear(g_state.renderer);
    }

    if (!sdl_scene_ui_render_overlay(main_view, scene))
    {
        SDL_SetRenderTarget(g_state.renderer, NULL);
        return false;
    }

    SDL_SetRenderTarget(g_state.renderer, NULL);
    return true;
}

bool sdl_scene_menu_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_menu_snapshot* snapshot)
{
    if (!canvas || !main_view || !snapshot)
        return false;
    if (snapshot->snapshot.scene != APP_SCENE_KIND_MENU)
        return false;
    if (snapshot->blobs[0].kind != APP_SNAPSHOT_BLOB_MENU
        || snapshot->blobs[0].size < sizeof(app_ui_scene))
    {
        return false;
    }

    return sdl_scene_ui_render(canvas, main_view, &snapshot->scene);
}
