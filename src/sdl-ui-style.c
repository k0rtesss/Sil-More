#include "angband.h"

#include "sdl-main-internal.h"

typedef struct sdl_ui_font_cache {
    TTF_Font* font;
    int pixel_height;
    int style_signature;
    char path[1024];
} sdl_ui_font_cache;

static sdl_ui_font_cache g_sdl_ui_font_cache;

static const char* sdl_ui_font_path(void)
{
    return config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
}

static int sdl_ui_font_signature(void)
{
    int signature = 0;

    signature |= config.mono_bold ? 0x0001 : 0;
    signature |= config.mono_italic ? 0x0002 : 0;
    signature |= config.mono_underline ? 0x0004 : 0;
    signature |= config.mono_strikethrough ? 0x0008 : 0;
    signature |= (config.mono_hinting & 0xFF) << 8;
    signature |= config.mono_kerning ? 0x10000 : 0;
    signature |= (config.mono_outline & 0xFF) << 17;

    return signature;
}

static void sdl_ui_apply_font_settings(TTF_Font* font)
{
    int style = TTF_STYLE_NORMAL;

    if (!font)
        return;

    if (config.mono_bold)
        style |= TTF_STYLE_BOLD;
    if (config.mono_italic)
        style |= TTF_STYLE_ITALIC;
    if (config.mono_underline)
        style |= TTF_STYLE_UNDERLINE;
    if (config.mono_strikethrough)
        style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL)
        TTF_SetFontStyle(font, style);

    TTF_SetFontHinting(font, config.mono_hinting);
    TTF_SetFontKerning(font, config.mono_kerning);
    if (config.mono_outline > 0)
        TTF_SetFontOutline(font, config.mono_outline);
}

int sdl_ui_scale_px(float logical_value)
{
    float scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;

    return (int)(logical_value * scale + 0.5f);
}

int sdl_ui_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

void sdl_ui_font_cache_clear(void)
{
    if (g_sdl_ui_font_cache.font)
    {
        TTF_CloseFont(g_sdl_ui_font_cache.font);
        g_sdl_ui_font_cache.font = NULL;
    }

    g_sdl_ui_font_cache.pixel_height = 0;
    g_sdl_ui_font_cache.style_signature = 0;
    g_sdl_ui_font_cache.path[0] = '\0';
}

TTF_Font* sdl_ui_font_for_height(int pixel_height)
{
    const char* font_path = sdl_ui_font_path();
    int style_signature = sdl_ui_font_signature();

    if (pixel_height <= 0)
        return NULL;

    if (g_sdl_ui_font_cache.font
        && g_sdl_ui_font_cache.pixel_height == pixel_height
        && g_sdl_ui_font_cache.style_signature == style_signature
        && streq(g_sdl_ui_font_cache.path, font_path))
    {
        return g_sdl_ui_font_cache.font;
    }

    sdl_ui_font_cache_clear();

    g_sdl_ui_font_cache.font = TTF_OpenFont(font_path, pixel_height);
    if (!g_sdl_ui_font_cache.font)
    {
        log_warn("SDL UI font load failed for '%s': %s", font_path,
            SDL_GetError());
        return NULL;
    }

    sdl_ui_apply_font_settings(g_sdl_ui_font_cache.font);
    g_sdl_ui_font_cache.pixel_height = pixel_height;
    g_sdl_ui_font_cache.style_signature = style_signature;
    SDL_strlcpy(g_sdl_ui_font_cache.path, font_path,
        sizeof(g_sdl_ui_font_cache.path));
    return g_sdl_ui_font_cache.font;
}

int sdl_ui_measure_text(TTF_Font* font, cptr text)
{
    int measured_w = 0;

    if (!font || !text || !text[0])
        return 0;

    if (!TTF_MeasureString(font, text, 0, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

void sdl_ui_render_text(TTF_Font* font, float x_px, float y_px,
    SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;

    if (!font || !text || !text[0])
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

    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}
