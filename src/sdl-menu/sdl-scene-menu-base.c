/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"

#include "sdl-scene-menu.h"
#include "support/utf8.h"

#define SDL_MENU_HIT_TARGET_MAX 512u

typedef struct sdl_menu_hit_registry {
    sdl_menu_hit_target targets[SDL_MENU_HIT_TARGET_MAX];
    u16b count;
    int origin_x;
    int origin_y;
    u16b scene_kind;
    u16b panel_index;
    u16b panel_layer;
    u16b panel_style;
    u16b focus_area;
    s16b focus_id;
} sdl_menu_hit_registry;

static sdl_menu_hit_registry g_sdl_menu_hit_registry;
static u16b g_sdl_menu_target_size_warning_count;

static void sdl_menu_copy_hit_text(char* dst, size_t dst_size, cptr text)
{
    if (!dst || dst_size == 0)
        return;

    (void)utf8_strlcpy(dst, text ? text : "", dst_size);
}

void sdl_menu_hit_reset(int origin_x, int origin_y)
{
    memset(&g_sdl_menu_hit_registry, 0, sizeof(g_sdl_menu_hit_registry));
    g_sdl_menu_hit_registry.origin_x = origin_x;
    g_sdl_menu_hit_registry.origin_y = origin_y;
    g_sdl_menu_hit_registry.focus_id = -1;
}

void sdl_menu_hit_set_scene(u16b scene_kind)
{
    g_sdl_menu_hit_registry.scene_kind = scene_kind;
}

u16b sdl_menu_hit_scene_kind(void)
{
    return g_sdl_menu_hit_registry.scene_kind;
}

void sdl_menu_hit_begin_panel(u16b panel_index,
    const app_ui_panel* panel)
{
    g_sdl_menu_hit_registry.panel_index = panel_index;
    g_sdl_menu_hit_registry.panel_layer = panel ? panel->layer : 0;
    g_sdl_menu_hit_registry.panel_style = panel ? panel->style : 0;
    g_sdl_menu_hit_registry.focus_area = panel ? panel->focus_area
        : APP_UI_FOCUS_NONE;
    g_sdl_menu_hit_registry.focus_id = panel ? panel->focus_id : -1;
}

void sdl_menu_hit_end_panel(void)
{
    g_sdl_menu_hit_registry.panel_index = 0;
    g_sdl_menu_hit_registry.panel_layer = 0;
    g_sdl_menu_hit_registry.panel_style = 0;
    g_sdl_menu_hit_registry.focus_area = APP_UI_FOCUS_NONE;
    g_sdl_menu_hit_registry.focus_id = -1;
}

static void sdl_menu_hit_note_target_size(
    const sdl_menu_hit_target* target)
{
    int min_px;

    if (!target)
        return;
    if (!(target->flags & APP_UI_INTERACTION_FLAG_TOUCH_TARGET))
        return;
    if (target->state_flags & APP_UI_ITEM_FLAG_DISABLED)
        return;

    min_px = sdl_ui_scale_px(24.0f);
    if (target->rect.w >= (float)min_px && target->rect.h >= (float)min_px)
        return;

    if (g_sdl_menu_target_size_warning_count < 24)
    {
        g_sdl_menu_target_size_warning_count++;
        log_trace("ui target-size audit: kind=%u id=%d label='%s' size=%.1fx%.1f min=%d",
            (unsigned)target->kind, (int)target->id, target->label,
            target->rect.w, target->rect.h, min_px);
    }
}

bool sdl_menu_hit_register(u16b kind, s16b id, s16b action_key, u16b role,
    u16b action, u16b flags, const SDL_FRect* canvas_rect, cptr label,
    cptr tooltip)
{
    return sdl_menu_hit_register_ex(kind, id, action_key, role, action, flags,
        APP_UI_ITEM_FLAG_NONE, -1, 0, 0, canvas_rect, label, tooltip);
}

bool sdl_menu_hit_register_ex(u16b kind, s16b id, s16b action_key,
    u16b role, u16b action, u16b flags, u16b state_flags, s16b owner_id,
    s32b payload0, s32b payload1, const SDL_FRect* canvas_rect, cptr label,
    cptr tooltip)
{
    sdl_menu_hit_target* target;
    s16b focus_order;

    if (!canvas_rect || canvas_rect->w <= 0.0f || canvas_rect->h <= 0.0f)
        return false;
    if (!(flags & APP_UI_INTERACTION_FLAG_POINTER_ENABLED))
        return false;
    if (g_sdl_menu_hit_registry.count >= SDL_MENU_HIT_TARGET_MAX)
        return false;

    focus_order = (s16b)g_sdl_menu_hit_registry.count;
    target = &g_sdl_menu_hit_registry.targets[g_sdl_menu_hit_registry.count++];
    memset(target, 0, sizeof(*target));
    target->rect = *canvas_rect;
    target->rect.x += (float)g_sdl_menu_hit_registry.origin_x;
    target->rect.y += (float)g_sdl_menu_hit_registry.origin_y;
    target->scene_kind = g_sdl_menu_hit_registry.scene_kind;
    target->panel_index = g_sdl_menu_hit_registry.panel_index;
    target->panel_layer = g_sdl_menu_hit_registry.panel_layer;
    target->panel_style = g_sdl_menu_hit_registry.panel_style;
    target->focus_area = g_sdl_menu_hit_registry.focus_area;
    target->focus_id = g_sdl_menu_hit_registry.focus_id;
    target->state_flags = state_flags;
    target->focus_order = focus_order;
    target->owner_id = owner_id >= 0 ? owner_id
        : (s16b)g_sdl_menu_hit_registry.panel_index;
    target->payload0 = payload0;
    target->payload1 = payload1;
    target->kind = kind;
    target->id = id;
    target->action_key = action_key;
    target->role = role;
    target->action = action;
    target->flags = flags;
    sdl_menu_copy_hit_text(target->label, sizeof(target->label), label);
    sdl_menu_copy_hit_text(target->tooltip, sizeof(target->tooltip), tooltip);
    sdl_menu_hit_note_target_size(target);
    return true;
}

const sdl_menu_hit_target* sdl_menu_hit_test(float window_x, float window_y)
{
    int i;

    for (i = (int)g_sdl_menu_hit_registry.count - 1; i >= 0; i--)
    {
        const sdl_menu_hit_target* target = &g_sdl_menu_hit_registry.targets[i];

        if (window_x < target->rect.x || window_y < target->rect.y)
            continue;
        if (window_x >= target->rect.x + target->rect.w)
            continue;
        if (window_y >= target->rect.y + target->rect.h)
            continue;
        return target;
    }

    return NULL;
}

static void sdl_menu_overlay_panel_slug(cptr label, char* buf, size_t buflen)
{
    size_t out = 0;
    bool pending_dash = false;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (!label)
        return;

    for (size_t i = 0; label[i] && out + 1 < buflen; i++)
    {
        unsigned char ch = (unsigned char)label[i];
        bool alpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        bool digit = (ch >= '0' && ch <= '9');

        if (alpha || digit)
        {
            if (pending_dash && out > 0 && out + 1 < buflen)
                buf[out++] = '-';
            pending_dash = false;
            if (ch >= 'A' && ch <= 'Z')
                ch = (unsigned char)(ch - 'A' + 'a');
            buf[out++] = (char)ch;
        }
        else if (out > 0)
        {
            pending_dash = true;
        }
    }

    buf[out] = '\0';
}

void sdl_menu_overlay_panel_id(u16b scene_kind, u16b panel_index,
    u16b panel_style, cptr label, char* buf, size_t buflen)
{
    char slug[SDL_OVERLAY_PANEL_ID_LEN];

    if (!buf || !buflen)
        return;

    sdl_menu_overlay_panel_slug(label, slug, sizeof(slug));
    if (slug[0])
    {
        strnfmt(buf, buflen, "scene:%u:id:%s",
            (unsigned)scene_kind, slug);
        return;
    }

    strnfmt(buf, buflen, "scene:%u:panel:%u:style:%u",
        (unsigned)scene_kind, (unsigned)panel_index, (unsigned)panel_style);
}

bool sdl_menu_overlay_panel_get_offset(cptr id, int* out_x, int* out_y,
    bool* out_pinned)
{
    if (out_x)
        *out_x = 0;
    if (out_y)
        *out_y = 0;
    if (out_pinned)
        *out_pinned = false;
    if (!id || !id[0])
        return false;

    for (int i = 0; i < config.overlay_panel_count
        && i < SDL_OVERLAY_PANEL_CONFIG_MAX; i++)
    {
        const struct sdl_overlay_panel_config* overlay = &config.overlay_panels[i];

        if (!streq(overlay->id, id))
            continue;
        if (out_x)
            *out_x = overlay->x;
        if (out_y)
            *out_y = overlay->y;
        if (out_pinned)
            *out_pinned = overlay->pinned;
        return true;
    }

    return false;
}

void sdl_menu_overlay_panel_set_offset(cptr id, int x, int y, bool pinned)
{
    struct sdl_overlay_panel_config* overlay = NULL;

    if (!id || !id[0])
        return;

    for (int i = 0; i < config.overlay_panel_count
        && i < SDL_OVERLAY_PANEL_CONFIG_MAX; i++)
    {
        if (streq(config.overlay_panels[i].id, id)) {
            overlay = &config.overlay_panels[i];
            break;
        }
    }

    if (!overlay) {
        if (config.overlay_panel_count >= SDL_OVERLAY_PANEL_CONFIG_MAX)
            return;
        overlay = &config.overlay_panels[config.overlay_panel_count++];
        memset(overlay, 0, sizeof(*overlay));
        SDL_strlcpy(overlay->id, id, sizeof(overlay->id));
    }

    overlay->x = x;
    overlay->y = y;
    overlay->pinned = pinned;
}

static bool sdl_menu_overlay_panel_active_offset(cptr id, cptr fallback_id,
    int* out_x, int* out_y)
{
    int x = 0;
    int y = 0;
    bool pinned = false;

    if (sdl_menu_overlay_panel_get_offset(id, &x, &y, &pinned) && pinned)
    {
        if (out_x)
            *out_x = x;
        if (out_y)
            *out_y = y;
        return true;
    }

    if (fallback_id && fallback_id[0] && (!id || !streq(id, fallback_id))
        && sdl_menu_overlay_panel_get_offset(fallback_id, &x, &y, &pinned)
        && pinned)
    {
        if (id && id[0])
            sdl_menu_overlay_panel_set_offset(id, x, y, true);
        if (out_x)
            *out_x = x;
        if (out_y)
            *out_y = y;
        return true;
    }

    return false;
}

void sdl_menu_overlay_panel_clamp_offset(cptr id, cptr fallback_id,
    int canvas_w, int canvas_h, const SDL_FRect* panel, int* offset_x,
    int* offset_y)
{
    int x = 0;
    int y = 0;
    int min_x;
    int max_x;
    int min_y;
    int max_y;

    if (!panel || !offset_x || !offset_y)
        return;

    if (!sdl_menu_overlay_panel_active_offset(id, fallback_id, &x, &y))
    {
        *offset_x = 0;
        *offset_y = 0;
        return;
    }

    min_x = -(int)panel->x;
    max_x = canvas_w - (int)(panel->x + panel->w);
    min_y = -(int)panel->y;
    max_y = canvas_h - (int)(panel->y + panel->h);

    if (x < min_x)
        x = min_x;
    if (x > max_x)
        x = max_x;
    if (y < min_y)
        y = min_y;
    if (y > max_y)
        y = max_y;

    *offset_x = x;
    *offset_y = y;
}

static bool sdl_menu_hit_target_matches_ref(const sdl_menu_hit_target* target,
    const app_ui_widget_ref* ref)
{
    if (!target || !ref)
        return false;

    return target->scene_kind == ref->scene_kind
        && target->panel_index == ref->panel_index
        && target->kind == ref->target_kind
        && target->id == ref->widget_id;
}

static bool sdl_menu_hit_target_is_detail_region(
    const sdl_menu_hit_target* target)
{
    return target && target->kind == SDL_MENU_HIT_TARGET_PANEL
        && target->id == SDL_MENU_HIT_DETAIL_ID
        && target->role == APP_UI_WIDGET_ROLE_SCROLL_REGION;
}

static bool sdl_menu_hit_target_focusable(const sdl_menu_hit_target* target)
{
    if (!target)
        return false;
    if (target->state_flags & APP_UI_ITEM_FLAG_DISABLED)
        return false;
    if (target->role == APP_UI_WIDGET_ROLE_NONE)
        return false;
    if (target->action == APP_UI_WIDGET_ACTION_NONE)
        return false;

    if (sdl_menu_hit_target_is_detail_region(target))
        return true;

    return target->role == APP_UI_WIDGET_ROLE_BUTTON
        || target->role == APP_UI_WIDGET_ROLE_LIST_ITEM
        || target->role == APP_UI_WIDGET_ROLE_TAB;
}

static bool sdl_menu_hit_target_in_focus_area(
    const sdl_menu_hit_target* target)
{
    if (!target)
        return false;

    switch (target->focus_area)
    {
    case APP_UI_FOCUS_ROWS:
        return target->role == APP_UI_WIDGET_ROLE_LIST_ITEM;

    case APP_UI_FOCUS_TABS:
        return target->role == APP_UI_WIDGET_ROLE_TAB;

    case APP_UI_FOCUS_FOOTER:
        return target->role == APP_UI_WIDGET_ROLE_BUTTON;

    case APP_UI_FOCUS_DETAIL:
        return sdl_menu_hit_target_is_detail_region(target);

    default:
        return true;
    }
}

const sdl_menu_hit_target* sdl_menu_hit_current_focus(void)
{
    app_session* session = app_session_current();
    const app_ui_focus_state* focus
        = session ? app_session_ui_focus(session) : NULL;
    const sdl_menu_hit_target* first = NULL;
    const sdl_menu_hit_target* area_first = NULL;
    const sdl_menu_hit_target* area_selected = NULL;
    const sdl_menu_hit_target* focus_id_target = NULL;
    const sdl_menu_hit_target* selected = NULL;
    u16b i;

    if (focus && focus->active
        && app_ui_widget_ref_is_valid(&focus->target))
    {
        for (i = 0; i < g_sdl_menu_hit_registry.count; i++)
        {
            const sdl_menu_hit_target* target
                = &g_sdl_menu_hit_registry.targets[i];

            if (sdl_menu_hit_target_focusable(target)
                && sdl_menu_hit_target_matches_ref(target, &focus->target))
            {
                return target;
            }
        }
    }

    for (i = 0; i < g_sdl_menu_hit_registry.count; i++)
    {
        const sdl_menu_hit_target* target
            = &g_sdl_menu_hit_registry.targets[i];

        if (!sdl_menu_hit_target_focusable(target))
            continue;
        if (!first)
            first = target;
        if (sdl_menu_hit_target_in_focus_area(target))
        {
            if (!area_first)
                area_first = target;
            if (target->focus_id >= 0 && target->id == target->focus_id)
                focus_id_target = target;
            if (target->state_flags
                & (APP_UI_ITEM_FLAG_SELECTED | APP_UI_ITEM_FLAG_ACTIVE))
            {
                area_selected = target;
            }
        }
        if (target->state_flags
            & (APP_UI_ITEM_FLAG_SELECTED | APP_UI_ITEM_FLAG_ACTIVE))
        {
            selected = target;
        }
    }

    if (focus_id_target)
        return focus_id_target;
    if (area_selected)
        return area_selected;
    if (area_first)
        return area_first;
    return selected ? selected : first;
}

static bool sdl_menu_hit_targets_share_nav_group(
    const sdl_menu_hit_target* current, const sdl_menu_hit_target* candidate,
    int dy, int dx)
{
    if (!current || !candidate)
        return false;
    if (current->panel_index != candidate->panel_index)
        return false;
    if (!sdl_menu_hit_target_focusable(candidate))
        return false;
    if (sdl_menu_hit_target_is_detail_region(current))
        return sdl_menu_hit_target_is_detail_region(candidate);

    if (dx && current->role == APP_UI_WIDGET_ROLE_TAB)
        return candidate->role == APP_UI_WIDGET_ROLE_TAB;
    if (dx && current->role == APP_UI_WIDGET_ROLE_BUTTON)
        return candidate->role == APP_UI_WIDGET_ROLE_BUTTON;
    if (dy && current->role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        return candidate->role == APP_UI_WIDGET_ROLE_LIST_ITEM;

    return true;
}

const sdl_menu_hit_target* sdl_menu_hit_focus_delta(int dy, int dx)
{
    const sdl_menu_hit_target* current = sdl_menu_hit_current_focus();
    const sdl_menu_hit_target* best = NULL;
    bool forward = (dy > 0) || (dy == 0 && dx > 0);
    u16b i;

    if (!current)
        return NULL;

    for (i = 0; i < g_sdl_menu_hit_registry.count; i++)
    {
        const sdl_menu_hit_target* candidate
            = &g_sdl_menu_hit_registry.targets[i];

        if (!sdl_menu_hit_targets_share_nav_group(current, candidate, dy, dx))
            continue;
        if (candidate == current)
            continue;

        if (forward)
        {
            if (candidate->focus_order <= current->focus_order)
                continue;
            if (!best || candidate->focus_order < best->focus_order)
                best = candidate;
        }
        else
        {
            if (candidate->focus_order >= current->focus_order)
                continue;
            if (!best || candidate->focus_order > best->focus_order)
                best = candidate;
        }
    }

    if (best)
        return best;

    for (i = 0; i < g_sdl_menu_hit_registry.count; i++)
    {
        const sdl_menu_hit_target* candidate
            = &g_sdl_menu_hit_registry.targets[i];

        if (!sdl_menu_hit_targets_share_nav_group(current, candidate, dy, dx))
            continue;
        if (!best)
        {
            best = candidate;
            continue;
        }
        if (forward && candidate->focus_order < best->focus_order)
            best = candidate;
        if (!forward && candidate->focus_order > best->focus_order)
            best = candidate;
    }

    return best ? best : current;
}

void sdl_menu_hit_origin(int* out_x, int* out_y)
{
    if (out_x)
        *out_x = g_sdl_menu_hit_registry.origin_x;
    if (out_y)
        *out_y = g_sdl_menu_hit_registry.origin_y;
}

SDL_Color sdl_menu_color_alpha(byte attr, byte alpha)
{
    byte color = attr & 0x0Fu;

    return (SDL_Color){
        angband_color_table[color][1],
        angband_color_table[color][2],
        angband_color_table[color][3],
        alpha
    };
}

SDL_Color sdl_menu_color(byte attr)
{
    return sdl_menu_color_alpha(attr, 255);
}

const sdl_ui_style* sdl_menu_panel_style(const app_ui_panel* panel)
{
    return sdl_ui_style_for_panel(panel ? panel->style
        : APP_UI_PANEL_STYLE_DEFAULT);
}

SDL_Color sdl_menu_panel_color(const app_ui_panel* panel, byte attr)
{
    return sdl_ui_style_color_for_attr(sdl_menu_panel_style(panel), attr);
}

SDL_Color sdl_menu_panel_color_alpha(const app_ui_panel* panel, byte attr,
    byte alpha)
{
    return sdl_ui_style_with_alpha(sdl_menu_panel_color(panel, attr), alpha);
}

SDL_Color sdl_menu_panel_accent(const app_ui_panel* panel, byte attr)
{
    return sdl_ui_style_accent_for_attr(sdl_menu_panel_style(panel), attr);
}

void sdl_menu_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

void sdl_menu_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

void sdl_menu_draw_tile(byte attr, byte ch, const SDL_FRect* dst)
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

static void sdl_menu_render_fixed_glyph(TTF_Font* font, float x_px, float y_px,
    int cell_w, int cell_h, SDL_Color color, char ch);

void sdl_menu_draw_view_glyph(const sdl_view* view,
    const SDL_FRect* dst, SDL_Color color, char ch)
{
    unsigned char glyph = (unsigned char)(ch ? ch : ' ');
    SDL_FRect src;

    if (!view || !dst || dst->w <= 0.0f || dst->h <= 0.0f || glyph == ' ')
        return;

    if (!view->font_atlas)
    {
        TTF_Font* font = sdl_ui_font_for_height(MAX(1, (int)(dst->h + 0.5f)));

        if (font)
        {
            sdl_menu_render_fixed_glyph(font, dst->x, dst->y,
                MAX(1, (int)(dst->w + 0.5f)),
                MAX(1, (int)(dst->h + 0.5f)), color, ch);
        }
        return;
    }

    SDL_SetTextureColorMod(view->font_atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(view->font_atlas, 255);

    src.x = (float)((glyph & 15) * view->cell_w);
    src.y = (float)((glyph >> 4) * view->cell_h);
    src.w = (float)view->cell_w;
    src.h = (float)view->cell_h;

    if (use_graphics == GRAPHICS_PSEUDO && solid_walls
        && (glyph == '#' || glyph == '%'))
    {
        sdl_menu_fill_rect(dst, color);
    }

    SDL_RenderTexture(g_state.renderer, view->font_atlas, &src, dst);
}

bool sdl_menu_target_has_visual_focus(u16b kind, s16b id, bool* out_pressed)
{
    app_session* session = app_session_current();
    const app_ui_focus_state* focus
        = session ? app_session_ui_focus(session) : NULL;

    if (out_pressed)
        *out_pressed = false;
    if (!focus || !focus->active)
        return false;
    if (!app_ui_widget_ref_is_valid(&focus->target))
        return false;
    if (focus->target.scene_kind != g_sdl_menu_hit_registry.scene_kind)
        return false;
    if (focus->target.panel_index != g_sdl_menu_hit_registry.panel_index)
        return false;
    if (focus->target.target_kind != kind)
        return false;
    if (focus->target.widget_id != id)
        return false;

    if (out_pressed)
        *out_pressed = focus->pressed != 0;
    return true;
}

void sdl_menu_draw_control_frame(const app_ui_panel* panel,
    const SDL_FRect* rect, u16b state_flags, bool active, bool focused,
    bool pressed)
{
    sdl_ui_style_draw_control_frame(sdl_menu_panel_style(panel), rect,
        state_flags, active, focused, pressed);
}

int sdl_menu_scale_px(float logical_value)
{
    return sdl_ui_scale_px(logical_value);
}

int sdl_menu_font_size_logical(const app_ui_panel* panel)
{
    if (!panel)
        return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);

    return sdl_effective_menu_font_size_for_panel_style(panel->style);
}

int sdl_menu_measure_text(TTF_Font* font, cptr text)
{
    return sdl_ui_measure_text(font, text);
}

static int sdl_menu_measure_text_n(TTF_Font* font, cptr text, size_t len);
static int sdl_menu_render_document_text_run_px(TTF_Font* font, float x_px,
    float y_px, SDL_Color color, cptr text, size_t len, int target_h,
    float max_w_px);
static float sdl_menu_measure_document_text_run_px(TTF_Font* font, cptr text,
    size_t len, int target_h, float max_w_px);

void sdl_menu_render_icon(TTF_Font* font, float x_px, float y_px,
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
            sdl_ui_style_color_for_attr(NULL, icon_attr ? icon_attr : TERM_WHITE),
            glyph);
    }
}

void sdl_menu_render_panel_icon(TTF_Font* font, const app_ui_panel* panel,
    float x_px, float y_px, int icon_slot_w, int line_h, byte icon_attr,
    char icon_char)
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
            sdl_menu_panel_color(panel, icon_attr ? icon_attr : TERM_WHITE),
            glyph);
    }
}

static int sdl_menu_measure_text_n(TTF_Font* font, cptr text, size_t len)
{
    int measured_w = 0;

    if (!font || !text || len == 0)
        return 0;
    if (!TTF_MeasureString(font, text, len, 0, &measured_w, NULL))
        return 0;

    return measured_w;
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

    copy_len = utf8_clip_bytes(text, MIN(copy_len, sizeof(buf) - 1u));
    if (copy_len == 0)
        return 0;
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

int sdl_menu_icon_slot_px(TTF_Font* font, int line_h)
{
    int icon_slot_w = sdl_menu_measure_text(font, "MM");

    if (icon_slot_w < line_h)
        icon_slot_w = line_h;
    if (icon_slot_w < 1)
        icon_slot_w = 1;

    return icon_slot_w;
}

void sdl_menu_render_text(TTF_Font* font, float x_px, float y_px,
    int line_h, SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;
    float render_w;
    float render_h;
    float scale = 1.0f;

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
    {
        scale = (float)line_h / render_h;
        render_w *= scale;
        render_h *= scale;
    }

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

bool sdl_menu_document_cell_is_raw(byte attr, char ch, byte terrain_attr,
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

void sdl_menu_draw_misc_icon(const SDL_FRect* dst, int icon)
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


static void sdl_menu_footer_action_text(const app_ui_footer_action* action,
    char* text, size_t text_size);

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
        char text[APP_UI_LABEL_MAX + 40];
        int action_w;

        sdl_menu_footer_action_text(action, text, sizeof(text));

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
    const app_ui_row* row, s16b row_index, const SDL_Rect* clip_rect,
    int line_h, int item_gap, int current_y, byte accent_attr)
{
    SDL_Color color;
    SDL_Color meta_color;
    int icon_slot_w = 0;
    int key_w = 0;
    int label_x = clip_rect->x;
    int meta_w = 0;
    int meta_x = clip_rect->x;
    int row_pad_y;
    bool focused;
    bool pressed = false;
    SDL_FRect hit_rect;

    if (!font || !panel || !row || !clip_rect)
        return;

    if (row->flags & APP_UI_ITEM_FLAG_SECTION)
    {
        if (row->label[0])
        {
            sdl_menu_render_text(font, (float)clip_rect->x, (float)current_y,
                line_h, sdl_menu_panel_color(panel,
                    row->attr ? row->attr : accent_attr), row->label);
        }
        return;
    }

    color = sdl_menu_panel_color(panel,
        (row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : row->attr);
    meta_color = sdl_menu_panel_color(panel,
        (row->flags & APP_UI_ITEM_FLAG_DISABLED)
        ? TERM_L_DARK
        : (row->meta_attr ? row->meta_attr : row->attr));
    row_pad_y = MAX(1, sdl_menu_scale_px(
        sdl_menu_panel_style(panel)->row_pad_y));
    hit_rect = (SDL_FRect){
        (float)clip_rect->x,
        (float)(current_y - row_pad_y),
        (float)clip_rect->w,
        (float)MAX(line_h + row_pad_y * 2, sdl_menu_scale_px(24.0f))
    };
    focused = sdl_menu_target_has_visual_focus(SDL_MENU_HIT_TARGET_ROW,
        row->id, &pressed);

    if ((row->flags & APP_UI_ITEM_FLAG_SELECTED) || focused || pressed)
    {
        SDL_FRect visual_rect = hit_rect;

        visual_rect.x -= (float)item_gap;
        visual_rect.w += (float)item_gap * 2.0f;
        sdl_menu_draw_control_frame(panel, &visual_rect, row->flags,
            (row->flags & APP_UI_ITEM_FLAG_SELECTED) != 0, focused, pressed);
    }

    (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_ROW, row->id,
        row->interaction.action_key, row->interaction.role,
        row->interaction.action, row->interaction.flags, row->flags, -1,
        row_index, panel->selected_row, &hit_rect, row->label,
        row->interaction.tooltip);

    if (row->icon_char)
    {
        icon_slot_w = sdl_menu_icon_slot_px(font, line_h);
        sdl_menu_render_panel_icon(font, panel, (float)label_x,
            (float)current_y,
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
            line_h, sdl_menu_panel_accent(panel, panel->accent_attr),
            row->key);
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

static void sdl_menu_footer_action_text(const app_ui_footer_action* action,
    char* text, size_t text_size)
{
    char prompt[32];
    cptr key_text;

    if (!text || !text_size)
        return;

    text[0] = '\0';
    if (!action || !action->label[0])
        return;

    prompt[0] = '\0';
    if (portable_controls_active())
    {
        platform_input_prompt_for_ui_action(APP_INPUT_DEVICE_GAMEPAD,
            action->interaction.action, action->interaction.action_key,
            prompt, sizeof(prompt));
    }

    key_text = prompt[0] ? prompt : action->key;
    if (key_text && key_text[0])
        strnfmt(text, text_size, "%s %s", key_text, action->label);
    else
        SDL_strlcpy(text, action->label, text_size);
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
        char text[APP_UI_LABEL_MAX + 40];
        int text_w;
        SDL_Color border = sdl_menu_panel_accent(panel, panel->accent_attr);
        SDL_Color text_color;
        SDL_FRect pill;
        bool focused;
        bool pressed = false;

        sdl_menu_footer_action_text(action, text, sizeof(text));

        text_w = sdl_menu_measure_text(font, text);
        pill.x = (float)cursor_x;
        pill.y = (float)clip_rect->y;
        pill.w = (float)(text_w + pill_pad_x * 2);
        pill.h = (float)(line_h + pill_pad_y * 2);

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        if (action->flags & APP_UI_ITEM_FLAG_DISABLED)
        {
            border = sdl_menu_panel_style(panel)->panel_border_soft;
            text_color = sdl_menu_panel_color(panel, TERM_L_DARK);
        }
        else
        {
            border.a = 220;
            text_color = sdl_menu_panel_color(panel,
                action->attr ? action->attr : TERM_WHITE);
        }

        focused = sdl_menu_target_has_visual_focus(
            SDL_MENU_HIT_TARGET_FOOTER_ACTION, action->id, &pressed);
        sdl_menu_draw_control_frame(panel, &pill, action->flags, false,
            focused, pressed);
        if (!focused && !pressed)
        {
            sdl_menu_draw_rect(&pill, border);
        }
        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_FOOTER_ACTION,
            action->id, action->interaction.action_key,
            action->interaction.role, action->interaction.action,
            action->interaction.flags, action->flags, -1, 0, 0, &pill,
            action->label, action->interaction.tooltip);
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
        SDL_Color border = sdl_menu_panel_accent(panel, panel->accent_attr);
        SDL_Color text_color = sdl_menu_panel_color(panel,
            tab->attr ? tab->attr : TERM_WHITE);
        bool focused;
        bool pressed = false;
        int text_w = sdl_menu_measure_text(font, tab->label);
        SDL_FRect pill = {
            (float)cursor_x,
            (float)clip_rect->y,
            (float)(text_w + pill_pad_x * 2),
            (float)(line_h + pill_pad_y * 2)
        };

        if (pill.x + pill.w > (float)(clip_rect->x + clip_rect->w))
            break;

        border.a = 220;
        focused = sdl_menu_target_has_visual_focus(SDL_MENU_HIT_TARGET_TAB,
            tab->id, &pressed);
        sdl_menu_draw_control_frame(panel, &pill, tab->flags,
            (tab->flags & APP_UI_ITEM_FLAG_ACTIVE) != 0, focused, pressed);
        if (!focused && !pressed)
        {
            sdl_menu_draw_rect(&pill, border);
        }
        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_TAB, tab->id,
            tab->interaction.action_key, tab->interaction.role,
            tab->interaction.action, tab->interaction.flags, tab->flags, -1,
            0, 0, &pill, tab->label, tab->interaction.tooltip);
        sdl_menu_render_text(font, pill.x + pill_pad_x, pill.y + pill_pad_y,
            line_h, text_color, tab->label);

        cursor_x += (int)pill.w + pill_gap;
    }
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

int sdl_menu_measure_rich_text_height(TTF_Font* mono_font,
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
    const app_ui_panel* panel, const app_ui_rich_paragraph* paragraph)
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
                sdl_menu_panel_color_alpha(panel, run->attr, run->alpha),
                cursor, len, line_h,
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

int sdl_menu_render_rich_text(const app_ui_scene* scene,
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
            scene, panel, paragraph);

        if (paragraph_h <= 0)
            continue;
        current_y += paragraph_h;
        if (i + 1 < panel->rich_paragraph_count)
            current_y += paragraph_gap;
    }

    return current_y - start_y;
}

bool sdl_menu_render_panel_internal(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* ui_panel, u16b scene_kind, u16b panel_index)
{
    TTF_Font* font;
    TTF_Font* story_font = NULL;
    const sdl_ui_style* style;
    SDL_FRect panel;
    SDL_Rect left_clip;
    SDL_Rect right_clip;
    SDL_Rect footer_clip;
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

    if (canvas_w <= 0 || canvas_h <= 0)
        return false;

    style = sdl_menu_panel_style(ui_panel);
    pixel_height = sdl_menu_scale_px((float)sdl_menu_font_size_logical(ui_panel));
    font = sdl_ui_font_for_height(pixel_height);
    if (!font)
        return false;
    story_font = sdl_story_font_for_height(pixel_height);
    if (!story_font)
        story_font = font;

    line_h = pixel_height;
    if (line_h <= 0)
        line_h = TTF_GetFontHeight(font);
    line_gap = MAX(1, sdl_menu_scale_px(style->line_gap));
    section_gap = MAX(line_gap, sdl_menu_scale_px(style->section_gap));
    item_gap = MAX(1, sdl_menu_scale_px(style->item_gap));
    pad_x = MAX(1, sdl_menu_scale_px(style->pad_x));
    pad_y = MAX(1, sdl_menu_scale_px(style->pad_y));
    outer_margin = MAX(0, sdl_menu_scale_px(style->margin_x));
    column_gap = MAX(1, sdl_menu_scale_px(style->column_gap));
    pill_gap = MAX(1, sdl_menu_scale_px(style->pill_gap));
    pill_pad_x = MAX(1, sdl_menu_scale_px(style->pill_pad_x));
    pill_pad_y = MAX(1, sdl_menu_scale_px(style->pill_pad_y));
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

    {
        char overlay_id[SDL_OVERLAY_PANEL_ID_LEN];
        char legacy_overlay_id[SDL_OVERLAY_PANEL_ID_LEN];
        int offset_x = 0;
        int offset_y = 0;
        int handle_h;
        SDL_FRect drag_rect;

        cptr overlay_key = ui_panel->id[0] ? ui_panel->id : ui_panel->title;

        sdl_menu_overlay_panel_id(scene_kind, panel_index, ui_panel->style,
            overlay_key, overlay_id, sizeof(overlay_id));
        sdl_menu_overlay_panel_id(scene_kind, panel_index, ui_panel->style,
            NULL, legacy_overlay_id, sizeof(legacy_overlay_id));
        sdl_menu_overlay_panel_clamp_offset(overlay_id, legacy_overlay_id,
            canvas_w, canvas_h, &panel, &offset_x, &offset_y);
        panel.x += (float)offset_x;
        panel.y += (float)offset_y;

        handle_h = header_h + pad_y * 2;
        if (handle_h < sdl_menu_scale_px(28.0f))
            handle_h = sdl_menu_scale_px(28.0f);
        if (handle_h > (int)panel.h)
            handle_h = (int)panel.h;
        drag_rect = (SDL_FRect){
            panel.x,
            panel.y,
            panel.w,
            (float)handle_h
        };
        (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_PANEL,
            (s16b)panel_index, 0, APP_UI_WIDGET_ROLE_PANEL_DRAG_HANDLE,
            APP_UI_WIDGET_ACTION_DRAG,
            APP_UI_INTERACTION_FLAG_POINTER_ENABLED
                | APP_UI_INTERACTION_FLAG_DRAGGABLE
                | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
            APP_UI_ITEM_FLAG_NONE, -1, 0, 0, &drag_rect,
            overlay_key && overlay_key[0] ? overlay_key : "Panel",
            "Drag panel. Right-click resets position.");
    }

    sdl_ui_style_draw_panel_frame(style, &panel, true);

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
            sdl_menu_render_panel_icon(font, ui_panel, (float)left_clip.x,
                (float)(current_y + (header_h - title_icon_h) / 2),
                title_icon_h, title_icon_h, ui_panel->icon_attr,
                ui_panel->icon_char);
            text_x += title_icon_slot_w;
        }

        if (ui_panel->title[0])
        {
            sdl_menu_render_text(font, (float)text_x, (float)text_y,
                line_h, sdl_menu_panel_color(ui_panel, ui_panel->title_attr),
                ui_panel->title);
            if (ui_panel->icon_char
                && ui_panel->style == APP_UI_PANEL_STYLE_PLAIN)
            {
                int title_text_w = sdl_menu_measure_text(font, ui_panel->title);

                sdl_menu_render_panel_icon(font, ui_panel,
                    (float)(text_x + title_text_w + item_gap * 0.5f),
                    (float)(current_y + (header_h - title_icon_h) / 2),
                    title_icon_h, title_icon_h, ui_panel->icon_attr,
                    ui_panel->icon_char);
            }
            text_y += line_h + line_gap;
        }
        else if (ui_panel->icon_char)
        {
            sdl_menu_render_panel_icon(font, ui_panel, (float)left_clip.x,
                (float)(current_y + (header_h - title_icon_h) / 2),
                title_icon_h, title_icon_h, ui_panel->icon_attr,
                ui_panel->icon_char);
            text_x += title_icon_slot_w;
        }
        if (ui_panel->subtitle[0])
        {
            sdl_menu_render_text(font, (float)text_x, (float)text_y,
                line_h, sdl_menu_panel_color(ui_panel, ui_panel->subtitle_attr),
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
        if ((ui_panel->flags & APP_UI_PANEL_FLAG_SCROLL_ROWS)
            && column_clip.w > 0 && column_clip.h > 0)
        {
            SDL_FRect scroll_rect = {
                (float)column_clip.x,
                (float)column_clip.y,
                (float)column_clip.w,
                (float)column_clip.h
            };

            (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_PANEL, -1, 0,
                APP_UI_WIDGET_ROLE_SCROLL_REGION,
                APP_UI_WIDGET_ACTION_SCROLL,
                APP_UI_INTERACTION_FLAG_POINTER_ENABLED
                    | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
                APP_UI_ITEM_FLAG_NONE, -1, row_start, row_visible,
                &scroll_rect, ui_panel->title, "");
        }
        SDL_SetRenderClipRect(g_state.renderer, &column_clip);
        for (i = 0; i < ui_panel->body_line_count; i++)
        {
            sdl_menu_render_text(font, (float)left_clip.x, (float)current_y,
                line_h, sdl_menu_panel_color(ui_panel,
                    ui_panel->body_lines[i].attr),
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
                sdl_menu_panel_accent(ui_panel, ui_panel->accent_attr), "^");
        }

        for (i = 0; i < (u16b)row_visible; i++)
        {
            const app_ui_row* row = &ui_panel->rows[row_start + i];

            sdl_menu_render_row(font, ui_panel, row,
                (s16b)(row_start + i), &column_clip, line_h, item_gap,
                current_y, ui_panel->accent_attr);
            current_y += line_h + line_gap;
        }

        if (row_start + row_visible < (int)ui_panel->row_count)
        {
            sdl_menu_render_text(font,
                (float)(left_clip.x + left_clip.w - sdl_menu_scale_px(10.0f)),
                (float)(current_y - line_gap), line_h,
                sdl_menu_panel_accent(ui_panel, ui_panel->accent_attr), "v");
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
        if (detail_clip_rect.w > 0 && detail_clip_rect.h > 0)
        {
            SDL_FRect detail_hit_rect = {
                (float)detail_clip_rect.x,
                (float)detail_clip_rect.y,
                (float)detail_clip_rect.w,
                (float)detail_clip_rect.h
            };
            cptr detail_label = ui_panel->detail_title[0]
                ? ui_panel->detail_title
                : (ui_panel->title[0] ? ui_panel->title : "Detail");

            (void)sdl_menu_hit_register_ex(SDL_MENU_HIT_TARGET_PANEL,
                SDL_MENU_HIT_DETAIL_ID, 0,
                APP_UI_WIDGET_ROLE_SCROLL_REGION,
                APP_UI_WIDGET_ACTION_SCROLL,
                APP_UI_INTERACTION_FLAG_POINTER_ENABLED
                    | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
                APP_UI_ITEM_FLAG_NONE, -1, 0, ui_panel->detail_line_count,
                &detail_hit_rect, detail_label, "");
        }
        SDL_SetRenderClipRect(g_state.renderer, &detail_clip_rect);
        if (ui_panel->detail_title[0])
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_panel_color(ui_panel,
                    ui_panel->detail_title_attr),
                ui_panel->detail_title);
            detail_y += line_h + line_gap;
        }
        for (i = 0; i < ui_panel->detail_line_count; i++)
        {
            sdl_menu_render_text(font, (float)right_clip.x, (float)detail_y,
                line_h, sdl_menu_panel_color(ui_panel,
                    ui_panel->detail_lines[i].attr),
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
