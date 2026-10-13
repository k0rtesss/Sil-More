#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <math.h>

/* Explicit prompt markers keep ordinary prose and keyboard shortcuts out of
 * controller glyph rendering.  Text measurements still reserve their width. */
static const struct {
    cptr token;
    cptr label;
    bool round;
} controller_glyphs[] = {
    {"<A>", "A", true}, {"<B>", "B", true},
    {"<X>", "X", true}, {"<Y>", "Y", true},
    {"<LB>", "LB", false}, {"<RB>", "RB", false},
    {"<LT>", "LT", false}, {"<RT>", "RT", false},
    {"<LS>", "LS", true}, {"<RS>", "RS", true},
    {"<Menu>", "Menu", false}, {"<View>", "View", false}
};

static int controller_glyph_at(cptr text)
{
    if (text[0] != '<') return -1;
    for (int i = 0; i < (int)N_ELEMENTS(controller_glyphs); ++i)
        if (!strncmp(text, controller_glyphs[i].token,
                strlen(controller_glyphs[i].token)))
            return i;
    return -1;
}

int sdl_controller_glyph_token_length(cptr text, int remaining)
{
    if (remaining < 3 || text[0] != '<') return 0;
    for (int i=0;i<(int)N_ELEMENTS(controller_glyphs);++i) {
        int length=(int)strlen(controller_glyphs[i].token);
        if (length<=remaining && !strncmp(text,controller_glyphs[i].token,length))
            return length;
    }
    return 0;
}

static void controller_glyph_line(SDL_Surface* surface,
    float x1, float y1, float x2, float y2, float thickness)
{
    float dx = x2 - x1, dy = y2 - y1;
    float length = dx * dx + dy * dy;
    float radius = thickness * 0.5f;
    if (length <= 0 || !SDL_LockSurface(surface)) return;
    const SDL_PixelFormatDetails* format=SDL_GetPixelFormatDetails(surface->format);
    for (int y = MAX(0, (int)(MIN(y1,y2)-radius-1));
        y < MIN(surface->h, (int)(MAX(y1,y2)+radius+2)); ++y) {
        for (int x = MAX(0, (int)(MIN(x1,x2)-radius-1));
            x < MIN(surface->w, (int)(MAX(x1,x2)+radius+2)); ++x) {
            float t = MAX(0,MIN(1,((x+0.5f-x1)*dx+(y+0.5f-y1)*dy)/length));
            float sx = x+0.5f-x1-t*dx, sy = y+0.5f-y1-t*dy;
            float alpha = MIN(1, radius+0.5f-sqrtf(sx*sx+sy*sy));
            if (alpha > 0) {
                Uint8* destination=(Uint8*)surface->pixels+y*surface->pitch+x*4;
                Uint32 previous;Uint8 r,g,b,a;
                memcpy(&previous,destination,sizeof(previous));
                SDL_GetRGBA(previous,format,NULL,&r,&g,&b,&a);
                Uint32 pixel = SDL_MapSurfaceRGBA(surface,255,255,255,
                    MAX(a,(Uint8)(alpha*255)));
                memcpy(destination,&pixel,sizeof(pixel));
            }
        }
    }
    SDL_UnlockSurface(surface);
}

static void controller_glyph_paint(TTF_Font* font, SDL_Surface* surface,
    SDL_Rect bounds, int glyph)
{
    if (bounds.w < 3 || bounds.h < 3)
        return;
    SDL_Rect clear = bounds;
    SDL_FillSurfaceRect(surface, &clear, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));
    int height = MAX(3, bounds.h * 4 / 5);
    int width = controller_glyphs[glyph].round ? MIN(bounds.w, height)
        : MIN(bounds.w - 2, height * 2);
    SDL_Rect icon = {bounds.x + (bounds.w - width) / 2,
        bounds.y + (bounds.h - height) / 2, width, height};
    float thickness = MAX(1.5f, height * 0.075f);
    if (!SDL_LockSurface(surface))
        return;
    for (int y = MAX(0, icon.y); y < MIN(surface->h, icon.y + icon.h); ++y) {
        for (int x = MAX(0, icon.x); x < MIN(surface->w, icon.x + icon.w); ++x) {
            float dx = fabsf(x + 0.5f - (icon.x + icon.w * 0.5f));
            float dy = fabsf(y + 0.5f - (icon.y + icon.h * 0.5f));
            float edge = controller_glyphs[glyph].round
                ? icon.w * 0.5f - sqrtf(dx * dx + dy * dy)
                : MIN(icon.w * 0.5f - dx, icon.h * 0.5f - dy);
            float alpha = MIN(1.0f, MIN(edge + 0.5f, thickness - edge + 0.5f));
            if (alpha > 0) {
                Uint32 pixel = SDL_MapSurfaceRGBA(surface, 255, 255, 255,
                    (Uint8)(alpha * 255));
                memcpy((Uint8*)surface->pixels + y * surface->pitch + x * 4,
                    &pixel, sizeof(pixel));
            }
        }
    }
    SDL_UnlockSurface(surface);
    SDL_GamepadType type = sdl_gamepad_prompt_type();
    bool playstation = type == SDL_GAMEPAD_TYPE_PS3
        || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5;
    cptr text = controller_glyphs[glyph].label;
    if (playstation && glyph < 4) {
        float left=icon.x+width*0.28f, right=icon.x+width*0.72f;
        float top=icon.y+height*0.28f, bottom=icon.y+height*0.72f;
        float cx=icon.x+width*0.5f, cy=icon.y+height*0.5f;
        if (glyph == 0) {
            controller_glyph_line(surface,left,top,right,bottom,thickness);
            controller_glyph_line(surface,right,top,left,bottom,thickness);
        } else if (glyph == 1) {
            float radius=width*0.24f;
            for(int i=0;i<32;i++) {
                float a=(float)i*2.0f*SDL_PI_F/32;
                float b=(float)(i+1)*2.0f*SDL_PI_F/32;
                controller_glyph_line(surface,cx+radius*cosf(a),cy+radius*sinf(a),
                    cx+radius*cosf(b),cy+radius*sinf(b),thickness);
            }
        } else if (glyph == 2) {
            controller_glyph_line(surface,left,top,right,top,thickness);
            controller_glyph_line(surface,right,top,right,bottom,thickness);
            controller_glyph_line(surface,right,bottom,left,bottom,thickness);
            controller_glyph_line(surface,left,bottom,left,top,thickness);
        } else {
            controller_glyph_line(surface,cx,top,left,bottom,thickness);
            controller_glyph_line(surface,left,bottom,right,bottom,thickness);
            controller_glyph_line(surface,right,bottom,cx,top,thickness);
        }
        return;
    }
    if (playstation) {
        if (streq(text,"LB")) text="L1";
        else if (streq(text,"RB")) text="R1";
        else if (streq(text,"LT")) text="L2";
        else if (streq(text,"RT")) text="R2";
        else if (streq(text,"LS")) text="L3";
        else if (streq(text,"RS")) text="R3";
        else if (streq(text,"View")) {
            if (type == SDL_GAMEPAD_TYPE_PS5) {
                float cx=icon.x+width*0.5f, cy=icon.y+height*0.6f;
                controller_glyph_line(surface,cx,cy-height*0.12f,cx,cy-height*0.4f,thickness);
                controller_glyph_line(surface,cx-width*0.1f,cy,cx-width*0.25f,cy-height*0.25f,thickness);
                controller_glyph_line(surface,cx+width*0.1f,cy,cx+width*0.25f,cy-height*0.25f,thickness);
                return;
            }
            text=type == SDL_GAMEPAD_TYPE_PS3 ? "Select" : "Share";
        }
        if (streq(text,"Menu") && type == SDL_GAMEPAD_TYPE_PS3) text="Start";
    }
    if (streq(text, "Menu")) {
        for (int line = 0; line < 3; ++line) {
            SDL_Rect bar = {icon.x + width / 4,
                icon.y + height / 4 + line * height / 5,
                width / 2, MAX(1, height / 12)};
            SDL_FillSurfaceRect(surface, &bar,
                SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255));
        }
        return;
    }
    if (streq(text, "View")) {
        for (int square = 0; square < 2; ++square) {
            int size = height / 3, border = MAX(1, height / 14);
            int x = icon.x + width / 3 + square * size / 2;
            int y = icon.y + height / 4 + square * size / 2;
            SDL_Rect edges[] = {{x, y, size, border}, {x, y, border, size},
                {x, y + size - border, size, border},
                {x + size - border, y, border, size}};
            for (int i = 0; i < 4; ++i)
                SDL_FillSurfaceRect(surface, &edges[i],
                    SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255));
        }
        return;
    }
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* label = TTF_RenderText_Blended(font,
        text, 0, white);
    if (label) {
        float scale = MIN((height * 0.72f) / label->h,
            (width * 0.75f) / label->w);
        SDL_Rect dst = {0, 0, MAX(1, (int)(label->w * scale)),
            MAX(1, (int)(label->h * scale))};
        dst.x = icon.x + (width - dst.w) / 2;
        dst.y = icon.y + (height - dst.h) / 2;
        SDL_BlitSurfaceScaled(label, NULL, surface, &dst, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(label);
    }
}

void sdl_controller_glyphs_paint(TTF_Font* font, SDL_Surface* surface,
    cptr value, int wrap_width)
{
    bool found = false;
    for (cptr p = value; *p; ++p) {
        if (controller_glyph_at(p) >= 0) { found = true; break; }
    }
    if (!found || SDL_GetPixelFormatDetails(surface->format)->bytes_per_pixel != 4)
        return;
    TTF_TextEngine* engine = TTF_CreateSurfaceTextEngine();
    if (!engine) return;
    TTF_Text* text = TTF_CreateText(engine, font, value, 0);
    if (text) {
        TTF_SetTextWrapWidth(text, wrap_width);
        for (int offset = 0; value[offset]; ++offset) {
            int glyph = controller_glyph_at(value + offset);
            if (glyph < 0) continue;
            int length = (int)strlen(controller_glyphs[glyph].token);
            int count = 0;
            TTF_SubString** parts = TTF_GetTextSubStringsForRange(text,
                offset, length, &count);
            if (parts && count > 0) {
                SDL_Rect bounds = parts[0]->rect;
                for (int i = 1; i < count; ++i)
                    SDL_GetRectUnion(&bounds, &parts[i]->rect, &bounds);
                controller_glyph_paint(font, surface, bounds, glyph);
            }
            SDL_free(parts);
            offset += length - 1;
        }
        TTF_DestroyText(text);
    }
    TTF_DestroySurfaceTextEngine(engine);
}
