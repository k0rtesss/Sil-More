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

#ifndef INCLUDED_SDL_SCENE_INTERNAL_H
#define INCLUDED_SDL_SCENE_INTERNAL_H

typedef enum sdl_scene_animation_kind {
    SDL_SCENE_ANIMATION_NONE = 0,
    SDL_SCENE_ANIMATION_ACTOR_MOVED = 1,
    SDL_SCENE_ANIMATION_DAMAGE = 2,
    SDL_SCENE_ANIMATION_PROJECTILE = 3,
    SDL_SCENE_ANIMATION_OBJECT_TRANSFER = 4
} sdl_scene_animation_kind;

typedef struct sdl_scene_animation {
    bool active;
    u16b kind;
    Uint64 started_ns;
    Uint64 duration_ns;
    s32b subject;
    s16b from_y;
    s16b from_x;
    s16b to_y;
    s16b to_x;
    s32b arg0;
    s32b arg1;
    s32b arg2;
} sdl_scene_animation;

void sdl_scene_stack_init(void);
void sdl_scene_stack_shutdown(void);
void sdl_scene_stack_on_layout_changed(void);
void sdl_scene_stack_on_renderer_reset(void);
void sdl_scene_stack_prepare_frame(Uint64 now_ns);
int sdl_scene_stack_pending_timeout_ms(Uint64 now_ns);
bool sdl_scene_stack_handles_main_view(void);
bool sdl_scene_stack_render_main_layer(void);
void sdl_scene_stack_render_overlay_layer(void);
void sdl_scene_stack_clear(void);
bool sdl_scene_dungeon_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot,
    const sdl_scene_animation* animations, size_t animation_count,
    Uint64 now_ns);
bool sdl_scene_dungeon_hit_test_map_cell(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot, float window_x, float window_y,
    s16b* out_map_y, s16b* out_map_x);
bool sdl_scene_dungeon_map_cell_rect(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot, s16b map_y, s16b map_x,
    SDL_FRect* out_rect);
bool sdl_scene_bootstrap_render(SDL_Texture* canvas, const sdl_view* main_view,
    const app_bootstrap_snapshot* snapshot);
bool sdl_scene_ui_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene);
bool sdl_scene_ui_render_at(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, int hit_origin_x, int hit_origin_y,
    u16b hit_view_index, const app_ui_scene* scene);
bool sdl_scene_ui_render_overlay(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene);
/* Wave 7A continuation staging surface for the frontend scene-menu split. */
bool sdl_scene_menu_render(SDL_Texture* canvas, const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_menu_snapshot* snapshot);

#endif /* INCLUDED_SDL_SCENE_INTERNAL_H */
