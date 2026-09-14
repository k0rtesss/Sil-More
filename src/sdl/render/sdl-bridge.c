#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "sdl/render/sdl-bridge.h"
#include "cave/cave-bridge.h"

/* A pixel-aligned deck leaves the existing material visible along both sides.
 * Timber planks span water/chasm; stone slabs withstand lava, acid and ice.
 * This is geometry in the map renderer, not a replacement terrain bitmap. */
static void bridge_rect(const SDL_FRect* dst, bool vertical,
    float along, float across, float length, float width,
    SDL_Color color, int light)
{
    SDL_FRect r = {
        dst->x + (vertical ? across : along) * dst->w / 16.0f,
        dst->y + (vertical ? along : across) * dst->h / 16.0f,
        (vertical ? width : length) * dst->w / 16.0f,
        (vertical ? length : width) * dst->h / 16.0f
    };
    SDL_SetRenderDrawColor(g_state.renderer, color.r * light / 255,
        color.g * light / 255, color.b * light / 255, color.a);
    SDL_RenderFillRect(g_state.renderer, &r);
}

void sdl_draw_bridge_deck(int y, int x, const SDL_FRect* dst)
{
    if (!dst || !p_ptr || !in_bounds(y, x) || !FEAT_IS_BRIDGE(cave_feat[y][x])) return;
    int material = cave_bridge_underlay(cave_feat[y][x]);
    bool vertical = cave_bridge_vertical(cave_feat[y][x]);
    bool wood = material == FEAT_WATER || material == FEAT_DEEP_WATER
        || material == FEAT_CHASM;
    int light = !p_ptr->blind && (cave_info[y][x] & CAVE_SEEN) ? 255 : 96;
    SDL_Color body = wood ? (SDL_Color){ 135, 94, 52, 255 }
        : material == FEAT_LAVA ? (SDL_Color){ 143, 130, 111, 255 }
        : material == FEAT_POISON ? (SDL_Color){ 121, 137, 116, 255 }
        : (SDL_Color){ 169, 190, 197, 255 };
    SDL_Color shadow = { 27, 24, 22, 255 };
    SDL_Color joint = wood ? (SDL_Color){ 58, 37, 20, 255 } : (SDL_Color){ 64, 69, 67, 255 };
    SDL_Color edge = wood ? (SDL_Color){ 200, 156, 96, 255 } : (SDL_Color){ 202, 206, 193, 255 };
    bridge_rect(dst, vertical, 0, 2, 16, 13, shadow, light);
    bridge_rect(dst, vertical, 0, 3, 16, 10, body, light);
    for (int along = wood ? 0 : 3; along < 16; along += wood ? 4 : 5)
    {
        bridge_rect(dst, vertical, along, 3, 1, 10, joint, light);
        if (along + 1 < 16) bridge_rect(dst, vertical, along + 1, 4, 1, 8, edge, light);
    }
    /* Continuous raised edging communicates the deck axis even at 16px. */
    bridge_rect(dst, vertical, 0, 2, 16, 1, edge, light);
    bridge_rect(dst, vertical, 0, 13, 16, 1, joint, light);
    if (wood)
        for (int along = 2; along < 16; along += 8)
        {
            bridge_rect(dst, vertical, along, 3, 1, 1, shadow, light);
            bridge_rect(dst, vertical, along, 12, 1, 1, shadow, light);
        }
}
