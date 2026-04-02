#include "angband.h"

#include "ui-information-scene.h"
#include "externs.h"
#include "runtime-cli.h"

static bool g_ui_information_scene_active = false;
static bool g_ui_information_scene_refresh_enabled = true;

static void ui_information_scene_term_xtra(int action, int value)
{
    if (!g_ui_information_scene_refresh_enabled)
        return;

    (void)Term_xtra(action, value);
}

static app_information_snapshot* ui_information_scene_clone_snapshot(
    const app_information_snapshot* snapshot)
{
    app_information_snapshot* copy;

    if (!snapshot)
        return NULL;

    copy = mem_alloc(app_information_snapshot);
    if (!copy)
        return NULL;

    app_information_snapshot_init(copy);
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
    if (!session || !scope || !scope->previous_information_snapshot)
        return false;

    return app_session_publish_information_scene(session,
        &scope->previous_information_snapshot->scene);
}

static bool ui_information_scene_publish(const app_information_scene* scene,
    bool refresh)
{
    app_session* session = app_session_current();

    if (!session || !scene)
        return false;

    if (!app_session_publish_information_scene(session, scene))
        return false;

    if (refresh)
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);

    return true;
}

bool ui_information_scene_supported(void)
{
    app_session* session = app_session_current();

    return runtime_cli_snapshot_renderer() && session
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

bool ui_information_scene_enter(ui_information_scene_scope* scope)
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
    scope->previous_snapshot = *app_session_snapshot(session);
    if (snapshot && snapshot->scene == APP_SCENE_KIND_INFORMATION)
    {
        scope->previous_information_snapshot
            = ui_information_scene_clone_snapshot(
                app_session_information_snapshot(session));
        if (!scope->previous_information_snapshot)
            return false;
    }

    scope->previous_active = g_ui_information_scene_active;
    app_session_push_wait_scope(session, &scope->wait_scope,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, 0, 0);
    app_session_clear_inputs(session);
    g_ui_information_scene_active = true;
    scope->active = true;
    return true;
}

static bool ui_information_scene_cell_is_raw(byte attr, char ch,
    byte terrain_attr, char terrain_char)
{
    unsigned char raw = (unsigned char)ch;
    unsigned char terrain_raw = (unsigned char)terrain_char;

    if (((attr & 0x80) && (raw & 0x80)) || (attr == 255 && raw == 0xFF))
        return true;
    if (terrain_attr || terrain_raw)
        return true;

    return false;
}

static byte ui_information_scene_cell_width(int y, int x, int term_wid)
{
    if (!Term || !Term->scr || y < 0 || y >= Term->hgt || x < 0 || x >= term_wid)
        return 1;

    if ((x + 1) < term_wid
        && (Term->scr->a[y][x] & 0x80)
        && (((unsigned char)Term->scr->c[y][x]) & 0x80)
        && Term->scr->a[y][x + 1] == 255
        && (unsigned char)Term->scr->c[y][x + 1] == 0xFF)
    {
        return 2;
    }

    return 1;
}

bool ui_information_scene_capture_term(app_information_scene* scene)
{
    int term_wid = 0;
    int term_hgt = 0;
    bool cursor_visible = false;
    int cursor_x = 0;
    int cursor_y = 0;

    if (!scene || !Term || !Term->scr)
        return false;

    app_information_scene_init(scene);
    term_wid = Term->wid;
    term_hgt = Term->hgt;

    for (int y = 0; y < term_hgt; y++)
    {
        int last_nonblank = -1;

        for (int x = 0; x < term_wid; x++)
        {
            unsigned char ch = (unsigned char)Term->scr->c[y][x];
            byte attr = Term->scr->a[y][x];
            byte terrain_attr = Term->scr->ta[y][x];
            char terrain_char = Term->scr->tc[y][x];

            if (ui_information_scene_cell_is_raw(attr, Term->scr->c[y][x],
                    terrain_attr, terrain_char))
            {
                last_nonblank = x;
                continue;
            }

            if (ch != 0 && ch != ' ')
                last_nonblank = x;
        }

        if (last_nonblank < 0)
            continue;

        for (int x = 0; x <= last_nonblank;)
        {
            byte attr = Term->scr->a[y][x];
            byte story = Term->scr->story[y][x];
            byte terrain_attr = Term->scr->ta[y][x];
            char terrain_char = Term->scr->tc[y][x];
            unsigned char raw = (unsigned char)Term->scr->c[y][x];

            if (ui_information_scene_cell_is_raw(attr, Term->scr->c[y][x],
                    terrain_attr, terrain_char))
            {
                byte width = ui_information_scene_cell_width(y, x, term_wid);

                if (!app_information_scene_add_cell_ex(scene, (s16b)y,
                        (s16b)x, attr, (char)raw, terrain_attr, terrain_char,
                        story, width))
                {
                    return false;
                }
                x += width;
                continue;
            }

            {
                char buf[APP_INFORMATION_TEXT_MAX];
                int len = 0;
                int start = x;

                while (x <= last_nonblank && Term->scr->a[y][x] == attr
                    && Term->scr->story[y][x] == story
                    && !ui_information_scene_cell_is_raw(Term->scr->a[y][x],
                        Term->scr->c[y][x], Term->scr->ta[y][x],
                        Term->scr->tc[y][x])
                    && len < (int)sizeof(buf) - 1)
                {
                    raw = (unsigned char)Term->scr->c[y][x];
                    buf[len++] = raw ? (char)raw : ' ';
                    x++;
                }

                buf[len] = '\0';
                if (len > 0 && !app_information_scene_add_text_ex(scene,
                        (s16b)y, (s16b)start, attr, story, buf))
                {
                    return false;
                }
            }
        }
    }

    cursor_visible = Term->scr->cv && !Term->scr->cu;
    if (cursor_visible)
    {
        cursor_x = Term->scr->cx;
        cursor_y = Term->scr->cy;
        if (cursor_x >= 0 && cursor_x < term_wid
            && cursor_y >= 0 && cursor_y < term_hgt)
        {
            if (!app_information_scene_add_cursor(scene, (s16b)cursor_y,
                    (s16b)cursor_x, TERM_L_BLUE,
                    ui_information_scene_cell_width(cursor_y, cursor_x,
                        term_wid)))
            {
                return false;
            }
        }
    }

    return true;
}

bool ui_information_scene_present(const app_information_scene* scene)
{
    return ui_information_scene_publish(scene, true);
}

bool ui_information_scene_present_term(void)
{
    app_information_scene* scene;
    bool published = false;

    scene = mem_alloc(app_information_scene);
    if (!scene)
        return false;

    if (!ui_information_scene_capture_term(scene))
    {
        mem_free(scene);
        return false;
    }

    published = ui_information_scene_publish(scene, false);
    mem_free(scene);
    return published;
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

void ui_information_scene_leave(ui_information_scene_scope* scope)
{
    app_session* session = app_session_current();
    bool restored_information = false;

    if (!scope || !scope->active)
        return;

    if (session)
    {
        app_session_clear_inputs(session);
        restored_information = ui_information_scene_restore_snapshot(session,
            scope);
        if (!restored_information)
            app_session_set_snapshot(session, &scope->previous_snapshot);
        app_session_pop_wait_scope(session, &scope->wait_scope);
    }

    g_ui_information_scene_active = scope->previous_active;
    scope->previous_information_snapshot
        = mem_free(scope->previous_information_snapshot);
    scope->active = false;
    if (!restored_information
        && scope->previous_snapshot.scene == APP_SCENE_KIND_DUNGEON
        && Term
        && !scope->previous_active)
    {
        do_cmd_redraw();
    }
    else
    {
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    }
}
