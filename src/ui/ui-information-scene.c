#include "angband.h"

#include "ui-information-scene.h"
#include "externs.h"
#include "log/log.h"

static bool g_ui_information_scene_active = false;
static bool g_ui_information_scene_refresh_enabled = true;

typedef struct ui_monster_recall_capture {
    app_ui_scene* scene;
    app_ui_panel* panel;
    byte story;
    bool failed;
} ui_monster_recall_capture;

static ui_monster_recall_capture* g_ui_monster_recall_capture = NULL;

static void ui_information_scene_term_xtra(int action, int value)
{
    if (!g_ui_information_scene_refresh_enabled)
        return;

    (void)Term_xtra(action, value);
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

    ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

static bool ui_information_scene_append_rich_span(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, byte story, cptr text, size_t len)
{
    char buf[APP_UI_TEXT_MAX];

    if (!scene || !panel || !text || len == 0)
        return true;

    while (len > 0)
    {
        size_t chunk_len = len;

        if (chunk_len >= sizeof(buf))
            chunk_len = sizeof(buf) - 1u;
        memcpy(buf, text, chunk_len);
        buf[chunk_len] = '\0';
        if (!app_ui_panel_add_rich_text_ex(scene, panel, attr, story, buf))
            return false;
        text += chunk_len;
        len -= chunk_len;
    }

    return true;
}

static void ui_information_scene_capture_monster_text(byte attr, cptr text)
{
    ui_monster_recall_capture* capture = g_ui_monster_recall_capture;
    cptr cursor = text ? text : "";

    if (!capture || !capture->scene || !capture->panel || capture->failed)
        return;

    while (true)
    {
        cptr newline = strchr(cursor, '\n');
        size_t len = newline ? (size_t)(newline - cursor) : strlen(cursor);

        if (!ui_information_scene_append_rich_span(capture->scene,
                capture->panel, attr, capture->story, cursor, len))
        {
            capture->failed = true;
            return;
        }

        if (!newline)
            break;

        if (!app_ui_panel_begin_rich_paragraph(capture->scene, capture->panel))
        {
            capture->failed = true;
            return;
        }

        cursor = newline + 1;
    }
}

static void ui_information_scene_trim_empty_rich_tail(app_ui_scene* scene,
    app_ui_panel* panel)
{
    if (!scene || !panel)
        return;

    while (panel->rich_paragraph_count > 0)
    {
        u16b paragraph_index = (u16b)(panel->rich_paragraph_first
            + panel->rich_paragraph_count - 1);
        app_ui_rich_paragraph* paragraph = &scene->rich_paragraphs[
            paragraph_index];

        if (paragraph->run_count > 0)
            break;

        panel->rich_paragraph_count--;
        if (scene->rich_paragraph_count > paragraph_index)
            scene->rich_paragraph_count = paragraph_index;
    }
}

static bool ui_information_scene_build_monster_recall_ui(app_ui_scene* scene,
    int r_idx, const monster_type* m_ptr, cptr prompt, bool overlay_dungeon)
{
    app_ui_panel* panel;
    monster_race* r_ptr;
    story_font_term_state story_state;
    ui_monster_recall_capture capture;
    void (*old_hook)(byte, cptr);
    int old_indent;
    int old_wrap;
    bool use_story_font;
    char title[APP_UI_TITLE_MAX];
    cptr name;

    if (!scene || r_idx <= 0 || !z_info || r_idx >= z_info->r_max)
        return false;

    r_ptr = &r_info[r_idx];
    name = r_name + r_ptr->name;

    app_ui_scene_init(scene);
    if (overlay_dungeon)
        scene->flags |= APP_UI_SCENE_FLAG_USE_BACKDROP;

    panel = app_ui_scene_append_panel(scene,
        overlay_dungeon ? APP_UI_LAYER_TRANSIENT : APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    panel->accent_attr = TERM_SLATE;
    panel->min_width_px = overlay_dungeon ? 900 : 840;
    panel->width_cap_px = overlay_dungeon ? 1600 : 1320;

    if (r_ptr->flags1 & RF1_UNIQUE)
        strnfmt(title, sizeof(title), "%s -", name);
    else
        strnfmt(title, sizeof(title), "The %s -", name);
    app_ui_panel_set_title(panel, TERM_WHITE, title);
    app_ui_panel_set_icon(panel, monster_attr(r_ptr), monster_char(r_ptr));

    use_story_font = story_monster_desc_enabled();
    story_font_term_push(use_story_font, false, &story_state);

    old_hook = text_out_hook;
    old_indent = text_out_indent;
    old_wrap = text_out_wrap;

    memset(&capture, 0, sizeof(capture));
    capture.scene = scene;
    capture.panel = panel;
    capture.story = use_story_font ? STORY_FLAG_USE : 0;
    g_ui_monster_recall_capture = &capture;

    text_out_hook = ui_information_scene_capture_monster_text;
    text_out_indent = 0;
    text_out_wrap = 0;
    describe_monster(r_idx, false, m_ptr);

    text_out_hook = old_hook;
    text_out_indent = old_indent;
    text_out_wrap = old_wrap;
    g_ui_monster_recall_capture = NULL;
    story_font_term_pop(&story_state);

    if (capture.failed)
        return false;

    ui_information_scene_trim_empty_rich_tail(scene, panel);
    if (prompt && prompt[0])
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel)
            || !app_ui_panel_add_rich_text(scene, panel, TERM_SLATE, prompt))
        {
            return false;
        }
    }
    ui_information_scene_trim_empty_rich_tail(scene, panel);

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

    ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
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
    if (!ui_information_scene_build_monster_recall_ui(&scene, r_idx,
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

        ui_information_scene_term_xtra(TERM_XTRA_EVENT, 1);
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
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
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    }
    else
    {
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    }
}
