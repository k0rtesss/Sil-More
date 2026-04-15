#include "angband.h"

#include "ui-information-scene.h"
#include "externs.h"
#include "log/log.h"
#include "platform-frame.h"

static bool g_ui_information_scene_active = false;
static bool g_ui_information_scene_refresh_enabled = true;

static void ui_information_scene_process_events(bool wait)
{
    if (!g_ui_information_scene_refresh_enabled)
        return;

    platform_frame_process_events(wait);
}

static void ui_information_scene_present_frame(void)
{
    if (!g_ui_information_scene_refresh_enabled)
        return;

    platform_frame_present();
}

static bool ui_information_scene_publish_ui_scene(const app_ui_scene* scene,
    bool overlay_dungeon)
{
    app_session* session = app_session_current();

    if (!scene || !session)
        return false;

    if (overlay_dungeon)
    {
        if (!app_session_publish_dungeon_overlay_scene(session, scene))
            return false;
    }
    else if (!app_session_publish_menu_scene(session, scene))
    {
        return false;
    }

    ui_information_scene_present_frame();
    return true;
}

static app_menu_snapshot* ui_information_scene_clone_menu_snapshot(
    const app_menu_snapshot* snapshot)
{
    app_menu_snapshot* copy;

    if (!snapshot)
        return NULL;

    copy = mem_alloc(app_menu_snapshot);
    if (!copy)
        return NULL;

    app_menu_snapshot_init(copy);
    copy->snapshot = snapshot->snapshot;
    copy->snapshot.blobs = copy->blobs;
    copy->snapshot.blob_count = N_ELEMENTS(copy->blobs);
    copy->blobs[0].kind = snapshot->blobs[0].kind;
    copy->blobs[0].format_version = snapshot->blobs[0].format_version;
    copy->blobs[0].data = (const byte*)&copy->scene;
    copy->blobs[0].size = sizeof(copy->scene);
    copy->scene = snapshot->scene;

    return copy;
}

static bool ui_information_scene_restore_snapshot(app_session* session,
    const ui_information_scene_scope* scope)
{
    if (!session || !scope)
        return false;

    if (scope->previous_snapshot.scene == APP_SCENE_KIND_MENU
        && scope->previous_menu_snapshot)
    {
        log_debug("[metarun-esc-trace] ui_information_scene_restore_snapshot -> menu rev=%u",
            (unsigned)scope->previous_menu_snapshot->snapshot.revision);
        return app_session_publish_menu_scene(session,
            &scope->previous_menu_snapshot->scene);
    }

    return false;
}

bool ui_information_scene_present_ui(const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scene || !session)
        return false;
    if (!app_session_publish_menu_scene(session, scene))
        return false;

    ui_information_scene_present_frame();
    return true;
}

bool ui_information_scene_present_overlay(ui_information_scene_scope* scope,
    const app_ui_scene* scene)
{
    if (!scope || !scope->active)
        return false;
    if (!ui_information_scene_publish_ui_scene(scene, true))
        return false;

    scope->published_overlay = true;
    return true;
}

bool ui_information_scene_supported(void)
{
    app_session* session = app_session_current();

    return session
        && app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT);
}

bool ui_information_scene_set_refresh_enabled(bool enabled)
{
    bool previous = g_ui_information_scene_refresh_enabled;

    g_ui_information_scene_refresh_enabled = enabled;
    return previous;
}

bool ui_information_scene_is_active(void)
{
    return g_ui_information_scene_active;
}

bool ui_information_scene_owns_input(void)
{
    return g_ui_information_scene_active;
}

bool ui_information_scene_acquire(ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    if (ui_information_scene_is_active())
        return true;

    return ui_information_scene_enter(scope);
}

static bool ui_information_scene_enter_internal(
    ui_information_scene_scope* scope, bool clone_menu_snapshot,
    bool restore_snapshot, u16b reason)
{
    app_session* session;
    const app_snapshot* snapshot;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    if (!ui_information_scene_supported())
        return false;

    session = app_session_current();
    snapshot = app_session_snapshot(session);
    scope->restore_snapshot = restore_snapshot;
    if (restore_snapshot)
        scope->previous_snapshot = *snapshot;
    if (restore_snapshot && clone_menu_snapshot
        && snapshot->scene == APP_SCENE_KIND_MENU)
    {
        scope->previous_menu_snapshot = ui_information_scene_clone_menu_snapshot(
            app_session_menu_snapshot(session));
        if (!scope->previous_menu_snapshot)
            return false;
    }

    scope->previous_active = g_ui_information_scene_active;
    app_session_push_wait_scope(session, &scope->wait_scope,
        reason, 0, 0);
    app_session_push_input_capture(session, &scope->input_capture_scope);
    app_session_clear_inputs(session);
    g_ui_information_scene_active = true;
    scope->active = true;
    return true;
}

bool ui_information_scene_claim_input(ui_information_scene_scope* scope,
    u16b reason)
{
    return ui_information_scene_enter_internal(scope, false, false, reason);
}

bool ui_information_scene_enter(ui_information_scene_scope* scope)
{
    return ui_information_scene_enter_internal(scope, true, true,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE);
}

bool ui_information_scene_show_monster_recall(int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon,
    int* out_key)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    int key;

    if (out_key)
        *out_key = ESCAPE;

    message_flush();
    if (!ui_information_scene_enter(&scope))
        return false;
    if (!build_monster_recall_ui_scene(&scene, r_idx,
            m_ptr, prompt, overlay_dungeon)
        || !ui_information_scene_publish_ui_scene(&scene, overlay_dungeon))
    {
        ui_information_scene_leave(&scope);
        return false;
    }
    scope.published_overlay = overlay_dungeon;

    key = ui_information_scene_wait_key();
    if (out_key)
        *out_key = key;
    ui_information_scene_leave(&scope);
    return true;
}

static int ui_information_scene_wait_key_internal(u16b ignored_flags)
{
    app_session* session = app_session_current();
    app_input input;

    if (!ui_information_scene_supported() || !session)
        return ESCAPE;

    while (true)
    {
        while (app_session_pop_input(session, &input))
        {
            if (input.layer != APP_INPUT_LAYER_LEGACY
                || input.type != APP_INPUT_TYPE_KEY)
            {
                continue;
            }
            if (input.flags & ignored_flags)
                continue;

            if ((int)(input.payload.key.logical_key & 0xFFu) == ESCAPE) {
                log_debug("[metarun-esc-trace] ui_information_scene_wait_key_internal esc flags=0x%04x layer=%d type=%d",
                    (unsigned)input.flags, (int)input.layer, (int)input.type);
            }
            return (int)(input.payload.key.logical_key & 0xFFu);
        }

        ui_information_scene_process_events(true);
        ui_information_scene_present_frame();
    }
}

int ui_information_scene_wait_key(void)
{
    return ui_information_scene_wait_key_internal(0);
}

int ui_information_scene_wait_key_nonrepeat(void)
{
    return ui_information_scene_wait_key_internal(APP_INPUT_FLAG_REPEAT);
}

int ui_information_scene_wait_key_with_wait_reason(u16b reason)
{
    app_session* session = app_session_current();
    app_wait_scope scope;
    app_input_capture_scope input_capture_scope;
    int key;

    if (!ui_information_scene_supported() || !session)
        return ESCAPE;

    if (!app_session_input_capture_active(session))
    {
        app_session_push_input_capture(session, &input_capture_scope);
        if (reason != APP_WAIT_REASON_NONE)
            app_session_push_wait_scope(session, &scope, reason, 0, 0);
        key = ui_information_scene_wait_key();
        if (reason != APP_WAIT_REASON_NONE)
            app_session_pop_wait_scope(session, &scope);
        app_session_pop_input_capture(session, &input_capture_scope);
        return key;
    }

    if (reason == APP_WAIT_REASON_NONE)
        return ui_information_scene_wait_key();

    app_session_push_wait_scope(session, &scope, reason, 0, 0);
    key = ui_information_scene_wait_key();
    app_session_pop_wait_scope(session, &scope);
    return key;
}

int ui_information_scene_wait_key_hidden_with_wait_reason(u16b reason)
{
    bool saved_hide_cursor = inkey_cursor_hidden();
    int key;

    inkey_set_cursor_hidden(true);
    key = ui_information_scene_wait_key_with_wait_reason(reason);
    inkey_set_cursor_hidden(saved_hide_cursor);
    return key;
}

void ui_information_scene_leave(ui_information_scene_scope* scope)
{
    app_session* session = app_session_current();
    bool restored_snapshot = false;

    if (!scope || !scope->active)
        return;

    if (session)
    {
        log_debug("[metarun-esc-trace] ui_information_scene_leave begin prev_scene=%u published_overlay=%d prev_active=%d",
            (unsigned)scope->previous_snapshot.scene,
            scope->published_overlay ? 1 : 0, scope->previous_active ? 1 : 0);
        app_session_clear_inputs(session);
        app_session_pop_input_capture(session, &scope->input_capture_scope);
        if (scope->restore_snapshot)
        {
            restored_snapshot = ui_information_scene_restore_snapshot(session,
                scope);
            if (!restored_snapshot)
                app_session_set_snapshot(session, &scope->previous_snapshot);
        }
        if (scope->published_overlay)
            app_session_clear_dungeon_overlay_scene(session);
        app_session_pop_wait_scope(session, &scope->wait_scope);
    }

    g_ui_information_scene_active = scope->previous_active;
    scope->previous_menu_snapshot = mem_free(scope->previous_menu_snapshot);
    scope->active = false;
    log_debug("[metarun-esc-trace] ui_information_scene_leave end restored_snapshot=%d current_scene=%u",
        restored_snapshot ? 1 : 0,
        (unsigned)(session ? app_session_snapshot(session)->scene : 0));
    if (scope->restore_snapshot
        && !restored_snapshot
        && scope->previous_snapshot.scene == APP_SCENE_KIND_DUNGEON
        && !scope->previous_active
        && session)
    {
        app_session_mark_snapshot_dirty(session, APP_SNAPSHOT_INVALIDATE_ALL);
        (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
        ui_information_scene_present_frame();
    }
    else
    {
        ui_information_scene_present_frame();
    }
}
