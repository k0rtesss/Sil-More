#include "angband.h"

#include "sdl-main-internal.h"

enum {
    SDL_SCENE_ANIMATION_MAX = 48,
    SDL_SCENE_FRAME_NS = 16666667
};

typedef struct sdl_scene_stack_state {
    bool enabled;
    bool dungeon_active;
    bool bootstrap_active;
    bool information_active;
    bool menu_active;
    bool overlay_active;
    bool modal_active;
    bool frame_dirty;
    SDL_Texture* dungeon_canvas;
    int dungeon_canvas_w;
    int dungeon_canvas_h;
    u64b rendered_revision;
    sdl_scene_animation animations[SDL_SCENE_ANIMATION_MAX];
    size_t animation_count;
    Uint64 next_frame_ns;
} sdl_scene_stack_state;

static sdl_scene_stack_state g_scene_stack;

static void sdl_scene_stack_destroy_canvas(void)
{
    if (g_scene_stack.dungeon_canvas)
    {
        SDL_DestroyTexture(g_scene_stack.dungeon_canvas);
        g_scene_stack.dungeon_canvas = NULL;
    }

    g_scene_stack.dungeon_canvas_w = 0;
    g_scene_stack.dungeon_canvas_h = 0;
}

static void sdl_scene_stack_reset_animations(void)
{
    memset(g_scene_stack.animations, 0, sizeof(g_scene_stack.animations));
    g_scene_stack.animation_count = 0;
    g_scene_stack.next_frame_ns = 0;
}

void sdl_scene_stack_clear(void)
{
    g_scene_stack.frame_dirty = true;
    g_scene_stack.rendered_revision = 0;
    sdl_scene_stack_reset_animations();
}

static Uint64 sdl_scene_animation_duration_ns(u16b kind)
{
    switch (kind)
    {
    case SDL_SCENE_ANIMATION_ACTOR_MOVED:
        return 140000000ULL;

    case SDL_SCENE_ANIMATION_DAMAGE:
        return 180000000ULL;

    case SDL_SCENE_ANIMATION_PROJECTILE:
        return 170000000ULL;

    case SDL_SCENE_ANIMATION_OBJECT_TRANSFER:
        return 160000000ULL;

    default:
        return 0;
    }
}

static bool sdl_scene_stack_append_animation(const sdl_scene_animation* anim)
{
    size_t i;

    if (!anim || !anim->active || anim->duration_ns == 0)
        return false;

    for (i = 0; i < SDL_SCENE_ANIMATION_MAX; i++)
    {
        if (!g_scene_stack.animations[i].active)
        {
            g_scene_stack.animations[i] = *anim;
            if (i + 1 > g_scene_stack.animation_count)
                g_scene_stack.animation_count = i + 1;
            return true;
        }
    }

    for (i = 1; i < SDL_SCENE_ANIMATION_MAX; i++)
        g_scene_stack.animations[i - 1] = g_scene_stack.animations[i];

    g_scene_stack.animations[SDL_SCENE_ANIMATION_MAX - 1] = *anim;
    g_scene_stack.animation_count = SDL_SCENE_ANIMATION_MAX;
    return true;
}

static void sdl_scene_stack_note_event(const app_event_record* record)
{
    sdl_scene_animation anim;

    if (!record)
        return;

    memset(&anim, 0, sizeof(anim));
    anim.active = true;
    anim.started_ns = SDL_GetTicksNS();
    anim.subject = record->subject;
    anim.arg0 = record->arg0;
    anim.arg1 = record->arg1;
    anim.arg2 = record->arg2;

    switch (record->kind)
    {
    case APP_EVENT_KIND_ACTOR_MOVED:
        if (record->subject == APP_DUNGEON_PLAYER_SUBJECT)
            return;
        anim.kind = SDL_SCENE_ANIMATION_ACTOR_MOVED;
        anim.from_y = APP_UNPACK_COORD_Y((u32b)record->arg0);
        anim.from_x = APP_UNPACK_COORD_X((u32b)record->arg0);
        anim.to_y = APP_UNPACK_COORD_Y((u32b)record->arg1);
        anim.to_x = APP_UNPACK_COORD_X((u32b)record->arg1);
        break;

    case APP_EVENT_KIND_DAMAGE:
        anim.kind = SDL_SCENE_ANIMATION_DAMAGE;
        break;

    case APP_EVENT_KIND_PROJECTILE:
        anim.kind = SDL_SCENE_ANIMATION_PROJECTILE;
        anim.from_y = APP_UNPACK_COORD_Y((u32b)record->arg0);
        anim.from_x = APP_UNPACK_COORD_X((u32b)record->arg0);
        anim.to_y = APP_UNPACK_COORD_Y((u32b)record->arg1);
        anim.to_x = APP_UNPACK_COORD_X((u32b)record->arg1);
        break;

    case APP_EVENT_KIND_OBJECT_TRANSFER:
        anim.kind = SDL_SCENE_ANIMATION_OBJECT_TRANSFER;
        anim.from_y = APP_UNPACK_COORD_Y((u32b)record->arg0);
        anim.from_x = APP_UNPACK_COORD_X((u32b)record->arg0);
        anim.to_y = anim.from_y;
        anim.to_x = anim.from_x;
        break;

    default:
        return;
    }

    anim.duration_ns = sdl_scene_animation_duration_ns(anim.kind);
    if (sdl_scene_stack_append_animation(&anim))
        g_state.need_present = true;
}

static void sdl_scene_stack_drain_events(app_session* session)
{
    app_event_span span;
    size_t i;

    if (!session)
        return;

    span = app_session_drain_events(session);
    for (i = 0; i < span.count; i++)
        sdl_scene_stack_note_event(&span.records[i]);
}

static bool sdl_scene_stack_wait_reason_allows_snapshot(
    const app_wait_state* wait_state)
{
    u16b reason = wait_state ? wait_state->reason : APP_WAIT_REASON_NONE;

    switch (reason)
    {
    case APP_WAIT_REASON_NONE:
    case APP_WAIT_REASON_COMMAND_INPUT:
    case APP_WAIT_REASON_CONFIRM:
    case APP_WAIT_REASON_LIST_SELECTION:
    case APP_WAIT_REASON_TARGETING:
    case APP_WAIT_REASON_INFORMATIONAL_PAUSE:
        return true;

    default:
        return false;
    }
}

static void sdl_scene_stack_expire_animations(Uint64 now_ns)
{
    size_t previous_count = g_scene_stack.animation_count;
    size_t i;
    size_t write_index = 0;

    for (i = 0; i < g_scene_stack.animation_count; i++)
    {
        sdl_scene_animation anim = g_scene_stack.animations[i];

        if (!anim.active)
            continue;
        if (anim.duration_ns > 0 && now_ns >= anim.started_ns + anim.duration_ns)
            continue;

        if (write_index != i)
            g_scene_stack.animations[write_index] = anim;
        write_index++;
    }

    for (i = write_index; i < SDL_SCENE_ANIMATION_MAX; i++)
        memset(&g_scene_stack.animations[i], 0, sizeof(g_scene_stack.animations[i]));

    g_scene_stack.animation_count = write_index;
    if (write_index != previous_count)
    {
        g_scene_stack.frame_dirty = true;
        g_state.need_present = true;
    }
}

static bool sdl_scene_stack_dungeon_snapshot_active(const app_session* session)
{
    const app_snapshot* snapshot;
    const app_dungeon_snapshot* dungeon_snapshot;
    const app_wait_state* wait_state;

    if (!g_scene_stack.enabled || !session || !g_views[0].term_ready)
        return false;

    snapshot = app_session_snapshot(session);
    wait_state = app_session_wait_state(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;
    if (!sdl_scene_stack_wait_reason_allows_snapshot(wait_state))
        return false;

    dungeon_snapshot = app_session_dungeon_snapshot(session);
    return dungeon_snapshot
        && dungeon_snapshot->snapshot.revision > 0
        && dungeon_snapshot->snapshot.revision == snapshot->revision;
}

static bool sdl_scene_stack_bootstrap_snapshot_active(const app_session* session)
{
    const app_snapshot* snapshot;
    const app_bootstrap_snapshot* bootstrap_snapshot;

    if (!g_scene_stack.enabled || !session || !g_views[0].term_ready)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_BOOTSTRAP)
        return false;

    bootstrap_snapshot = app_session_bootstrap_snapshot(session);
    return bootstrap_snapshot
        && bootstrap_snapshot->snapshot.revision > 0
        && bootstrap_snapshot->snapshot.revision == snapshot->revision;
}

static bool sdl_scene_stack_information_snapshot_active(
    const app_session* session)
{
    const app_snapshot* snapshot;
    const app_information_snapshot* information_snapshot;

    if (!g_scene_stack.enabled || !session || !g_views[0].term_ready)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_INFORMATION)
        return false;

    information_snapshot = app_session_information_snapshot(session);
    return information_snapshot
        && information_snapshot->snapshot.revision > 0
        && information_snapshot->snapshot.revision == snapshot->revision;
}

static bool sdl_scene_stack_menu_snapshot_active(const app_session* session)
{
    const app_snapshot* snapshot;
    const app_menu_snapshot* menu_snapshot;

    if (!g_scene_stack.enabled || !session || !g_views[0].term_ready)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_MENU)
        return false;

    menu_snapshot = app_session_menu_snapshot(session);
    return menu_snapshot
        && menu_snapshot->snapshot.revision > 0
        && menu_snapshot->snapshot.revision == snapshot->revision;
}

static void sdl_scene_stack_update_layers(app_session* session)
{
    const app_snapshot* snapshot = session ? app_session_snapshot(session) : NULL;
    const app_wait_state* wait_state = session ? app_session_wait_state(session) : NULL;
    const app_interaction_state* interaction = session
        ? app_session_interaction(session) : NULL;
    bool next_dungeon_active = sdl_scene_stack_dungeon_snapshot_active(session);
    bool next_bootstrap_active
        = sdl_scene_stack_bootstrap_snapshot_active(session);
    bool next_information_active
        = sdl_scene_stack_information_snapshot_active(session);
    bool next_menu_active = sdl_scene_stack_menu_snapshot_active(session);
    bool interaction_owns_overlay = interaction
        && interaction->kind != APP_INTERACTION_KIND_NONE;
    bool next_overlay_active = next_dungeon_active && character_icky > 0
        && g_views[0].canvas && !interaction_owns_overlay;
    bool next_modal_active = g_scene_stack.enabled && g_views[0].term_ready
        && g_views[0].canvas && session && snapshot && !next_bootstrap_active
        && !next_information_active && !next_menu_active
        && (snapshot->scene != APP_SCENE_KIND_BOOTSTRAP)
        && ((snapshot->scene != APP_SCENE_KIND_DUNGEON)
            || !sdl_scene_stack_wait_reason_allows_snapshot(wait_state));

    if (next_dungeon_active != g_scene_stack.dungeon_active
        || next_bootstrap_active != g_scene_stack.bootstrap_active
        || next_information_active != g_scene_stack.information_active
        || next_menu_active != g_scene_stack.menu_active
        || next_overlay_active != g_scene_stack.overlay_active
        || next_modal_active != g_scene_stack.modal_active)
    {
        g_scene_stack.frame_dirty = true;
        g_state.need_present = true;
    }

    g_scene_stack.dungeon_active = next_dungeon_active;
    g_scene_stack.bootstrap_active = next_bootstrap_active;
    g_scene_stack.information_active = next_information_active;
    g_scene_stack.menu_active = next_menu_active;
    g_scene_stack.overlay_active = next_overlay_active;
    g_scene_stack.modal_active = next_modal_active;
}

static bool sdl_scene_stack_ensure_canvas(void)
{
    int required_w;
    int required_h;

    if (!g_views[0].term_ready || g_views[0].cols <= 0 || g_views[0].rows <= 0
        || g_views[0].cell_w <= 0 || g_views[0].cell_h <= 0)
    {
        return false;
    }

    required_w = g_views[0].cols * g_views[0].cell_w;
    required_h = g_views[0].rows * g_views[0].cell_h;

    if (g_scene_stack.dungeon_canvas
        && g_scene_stack.dungeon_canvas_w == required_w
        && g_scene_stack.dungeon_canvas_h == required_h)
    {
        return true;
    }

    sdl_scene_stack_destroy_canvas();

    g_scene_stack.dungeon_canvas = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, required_w,
        required_h);
    if (!g_scene_stack.dungeon_canvas)
    {
        log_error("SDL scene canvas create failed: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(g_scene_stack.dungeon_canvas, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(g_scene_stack.dungeon_canvas, SDL_SCALEMODE_NEAREST);
    g_scene_stack.dungeon_canvas_w = required_w;
    g_scene_stack.dungeon_canvas_h = required_h;
    g_scene_stack.frame_dirty = true;
    return true;
}

static void sdl_scene_stack_render_texture(SDL_Texture* texture,
    const sdl_view* view)
{
    float dst_w;
    float dst_h;

    if (!texture || !view)
        return;

    dst_w = (float)(view->cols * view->cell_w);
    dst_h = (float)(view->rows * view->cell_h);
    if (dst_w <= 0.0f || dst_h <= 0.0f)
        return;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &(SDL_FRect){
        .x = (float)(view->rect.x + view->margin_x),
        .y = (float)(view->rect.y + view->margin_y),
        .w = dst_w,
        .h = dst_h
    });
}

void sdl_scene_stack_init(void)
{
    memset(&g_scene_stack, 0, sizeof(g_scene_stack));
    g_scene_stack.enabled = runtime_cli_snapshot_renderer();
    g_scene_stack.frame_dirty = true;
}

void sdl_scene_stack_shutdown(void)
{
    sdl_ui_font_cache_clear();
    sdl_scene_stack_destroy_canvas();
    memset(&g_scene_stack, 0, sizeof(g_scene_stack));
}

void sdl_scene_stack_set_enabled(bool enabled)
{
    if (g_scene_stack.enabled == enabled)
        return;

    g_scene_stack.enabled = enabled;
    sdl_scene_stack_clear();
    sdl_scene_stack_update_layers(app_session_current());
    if (!enabled)
        sdl_scene_stack_destroy_canvas();
    g_state.need_present = true;
}

bool sdl_scene_stack_is_enabled(void)
{
    return g_scene_stack.enabled;
}

void sdl_scene_stack_on_layout_changed(void)
{
    sdl_scene_stack_destroy_canvas();
    sdl_scene_stack_clear();
}

void sdl_scene_stack_on_renderer_reset(void)
{
    sdl_scene_stack_destroy_canvas();
    g_scene_stack.frame_dirty = true;
}

void sdl_scene_stack_prepare_frame(Uint64 now_ns)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot;
    const app_bootstrap_snapshot* bootstrap_snapshot;
    const app_information_snapshot* information_snapshot;
    const app_menu_snapshot* menu_snapshot;

    if (!g_scene_stack.enabled)
        return;

    sdl_scene_stack_update_layers(session);

    if (!g_scene_stack.dungeon_active && !g_scene_stack.bootstrap_active
        && !g_scene_stack.information_active && !g_scene_stack.menu_active)
    {
        if (session)
            app_session_clear_events(session);
        sdl_scene_stack_reset_animations();
        g_scene_stack.rendered_revision = 0;
        return;
    }

    snapshot = app_session_dungeon_snapshot(session);
    bootstrap_snapshot = app_session_bootstrap_snapshot(session);
    information_snapshot = app_session_information_snapshot(session);
    menu_snapshot = app_session_menu_snapshot(session);
    if ((g_scene_stack.dungeon_active && snapshot
            && snapshot->snapshot.revision != g_scene_stack.rendered_revision)
        || (g_scene_stack.bootstrap_active && bootstrap_snapshot
            && (bootstrap_snapshot->snapshot.revision
                != g_scene_stack.rendered_revision))
        || (g_scene_stack.information_active && information_snapshot
            && (information_snapshot->snapshot.revision
                != g_scene_stack.rendered_revision)))
    {
        g_scene_stack.frame_dirty = true;
    }
    if (g_scene_stack.menu_active && menu_snapshot
        && menu_snapshot->snapshot.revision != g_scene_stack.rendered_revision)
    {
        g_scene_stack.frame_dirty = true;
    }

    if (g_scene_stack.dungeon_active)
    {
        sdl_scene_stack_drain_events(session);
        sdl_scene_stack_expire_animations(now_ns);
    }
    else
    {
        if (session)
            app_session_clear_events(session);
        sdl_scene_stack_reset_animations();
    }

    if (g_scene_stack.frame_dirty || g_scene_stack.animation_count > 0)
        g_state.need_present = true;

    if (g_scene_stack.animation_count > 0)
    {
        if (g_scene_stack.next_frame_ns == 0 || now_ns >= g_scene_stack.next_frame_ns)
            g_scene_stack.next_frame_ns = now_ns + SDL_SCENE_FRAME_NS;
    }
    else
    {
        g_scene_stack.next_frame_ns = 0;
    }
}

int sdl_scene_stack_pending_timeout_ms(Uint64 now_ns)
{
    const app_session* session;
    const app_snapshot* snapshot;
    const app_dungeon_snapshot* dungeon_snapshot;
    const app_bootstrap_snapshot* bootstrap_snapshot;
    const app_information_snapshot* information_snapshot;
    const app_menu_snapshot* menu_snapshot;
    Uint64 delta_ns;

    if (!g_scene_stack.enabled)
        return -1;

    session = app_session_current();
    snapshot = session ? app_session_snapshot(session) : NULL;
    if (snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON)
    {
        dungeon_snapshot = app_session_dungeon_snapshot(session);
        if (dungeon_snapshot
            && dungeon_snapshot->snapshot.revision == snapshot->revision
            && dungeon_snapshot->snapshot.revision != g_scene_stack.rendered_revision)
        {
            return 0;
        }
    }
    else if (snapshot && snapshot->scene == APP_SCENE_KIND_BOOTSTRAP)
    {
        bootstrap_snapshot = app_session_bootstrap_snapshot(session);
        if (bootstrap_snapshot
            && bootstrap_snapshot->snapshot.revision == snapshot->revision
            && bootstrap_snapshot->snapshot.revision
                != g_scene_stack.rendered_revision)
        {
            return 0;
        }
    }
    else if (snapshot && snapshot->scene == APP_SCENE_KIND_INFORMATION)
    {
        information_snapshot = app_session_information_snapshot(session);
        if (information_snapshot
            && information_snapshot->snapshot.revision == snapshot->revision
            && information_snapshot->snapshot.revision != g_scene_stack.rendered_revision)
        {
            return 0;
        }
    }
    else if (snapshot && snapshot->scene == APP_SCENE_KIND_MENU)
    {
        menu_snapshot = app_session_menu_snapshot(session);
        if (menu_snapshot
            && menu_snapshot->snapshot.revision == snapshot->revision
            && menu_snapshot->snapshot.revision != g_scene_stack.rendered_revision)
        {
            return 0;
        }
    }

    if (g_scene_stack.animation_count == 0)
        return -1;

    if (g_scene_stack.next_frame_ns == 0 || now_ns >= g_scene_stack.next_frame_ns)
        return 0;

    delta_ns = g_scene_stack.next_frame_ns - now_ns;
    return (int)((delta_ns + 999999ULL) / 1000000ULL);
}

bool sdl_scene_stack_handles_main_view(void)
{
    return g_scene_stack.modal_active || g_scene_stack.dungeon_active
        || g_scene_stack.bootstrap_active || g_scene_stack.information_active
        || g_scene_stack.menu_active;
}

bool sdl_scene_stack_render_main_layer(void)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot;
    const app_bootstrap_snapshot* bootstrap_snapshot;
    const app_information_snapshot* information_snapshot;
    const app_menu_snapshot* menu_snapshot;
    Uint64 now_ns = SDL_GetTicksNS();

    if (!g_scene_stack.enabled)
        return false;

    if (g_scene_stack.modal_active)
    {
        sdl_scene_stack_render_texture(g_views[0].canvas, &g_views[0]);
        return true;
    }

    if (!g_scene_stack.dungeon_active && !g_scene_stack.bootstrap_active
        && !g_scene_stack.information_active && !g_scene_stack.menu_active)
        return false;
    if (!sdl_scene_stack_ensure_canvas())
        return false;

    snapshot = app_session_dungeon_snapshot(session);
    bootstrap_snapshot = app_session_bootstrap_snapshot(session);
    information_snapshot = app_session_information_snapshot(session);
    menu_snapshot = app_session_menu_snapshot(session);
    if (g_scene_stack.dungeon_active && !snapshot)
        return false;
    if (g_scene_stack.bootstrap_active && !bootstrap_snapshot)
        return false;
    if (g_scene_stack.information_active && !information_snapshot)
        return false;
    if (g_scene_stack.menu_active && !menu_snapshot)
        return false;

    if (g_scene_stack.frame_dirty || g_scene_stack.animation_count > 0
        || (g_scene_stack.dungeon_active && snapshot
            && (g_scene_stack.rendered_revision != snapshot->snapshot.revision))
        || (g_scene_stack.bootstrap_active && bootstrap_snapshot
            && (g_scene_stack.rendered_revision
                != bootstrap_snapshot->snapshot.revision))
        || (g_scene_stack.information_active && information_snapshot
            && (g_scene_stack.rendered_revision
                != information_snapshot->snapshot.revision))
        || (g_scene_stack.menu_active && menu_snapshot
            && (g_scene_stack.rendered_revision
                != menu_snapshot->snapshot.revision)))
    {
        bool rendered = false;

        if (g_scene_stack.dungeon_active)
        {
            rendered = sdl_scene_dungeon_render(g_scene_stack.dungeon_canvas,
                &g_views[0], snapshot, g_scene_stack.animations,
                g_scene_stack.animation_count, now_ns);
        }
        else if (g_scene_stack.bootstrap_active)
        {
            rendered = sdl_scene_bootstrap_render(g_scene_stack.dungeon_canvas,
                &g_views[0], bootstrap_snapshot);
        }
        else if (g_scene_stack.information_active)
        {
            rendered = sdl_scene_information_render(
                g_scene_stack.dungeon_canvas, &g_views[0], information_snapshot);
        }
        else if (g_scene_stack.menu_active)
        {
            rendered = sdl_scene_menu_render(g_scene_stack.dungeon_canvas,
                &g_views[0], menu_snapshot);
        }

        if (!rendered)
        {
            return false;
        }

        g_scene_stack.frame_dirty = false;
        if (g_scene_stack.dungeon_active)
            g_scene_stack.rendered_revision = snapshot->snapshot.revision;
        else if (g_scene_stack.bootstrap_active)
            g_scene_stack.rendered_revision
                = bootstrap_snapshot->snapshot.revision;
        else if (g_scene_stack.information_active)
            g_scene_stack.rendered_revision = information_snapshot->snapshot.revision;
        else
            g_scene_stack.rendered_revision = menu_snapshot->snapshot.revision;
    }

    sdl_scene_stack_render_texture(g_scene_stack.dungeon_canvas, &g_views[0]);
    return true;
}

void sdl_scene_stack_render_overlay_layer(void)
{
    if (!g_scene_stack.enabled || !g_scene_stack.overlay_active)
        return;

    sdl_scene_stack_render_texture(g_views[0].canvas, &g_views[0]);
}
