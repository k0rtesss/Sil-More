#include "angband.h"

#include "ui-information-scene.h"
#include "externs.h"
#include "runtime-cli.h"

typedef enum ui_information_scene_mode {
    UI_INFORMATION_SCENE_MODE_NONE = 0,
    UI_INFORMATION_SCENE_MODE_EXPLICIT = 1,
    UI_INFORMATION_SCENE_MODE_MIRROR = 2
} ui_information_scene_mode;

static u16b g_ui_information_scene_mode = UI_INFORMATION_SCENE_MODE_NONE;
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
    const app_information_snapshot* saved;
    u16b i;

    if (!session || !scope || !scope->previous_information_snapshot)
        return false;

    saved = scope->previous_information_snapshot;
    app_session_clear_information_snapshot(session);
    for (i = 0; i < saved->scene.op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &saved->scene.ops[i];

        if (!op->text[0])
            continue;
        if (!app_session_add_information_op_ex(session, op->row, op->col,
                op->attr, op->story, op->text))
        {
            return false;
        }
    }

    return app_session_publish_information_snapshot(session);
}

static bool ui_information_scene_publish(const app_information_scene* scene,
    bool refresh)
{
    app_session* session = app_session_current();
    u16b i;

    if (!session || !scene)
        return false;

    app_session_clear_information_snapshot(session);
    for (i = 0; i < scene->op_count && i < APP_INFORMATION_OP_MAX; i++)
    {
        const app_information_op* op = &scene->ops[i];

        if (!op->text[0])
            continue;
        if (!app_session_add_information_op_ex(session, op->row, op->col,
                op->attr, op->story, op->text))
        {
            return false;
        }
    }

    if (!app_session_publish_information_snapshot(session))
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
    return g_ui_information_scene_mode != UI_INFORMATION_SCENE_MODE_NONE;
}

bool ui_information_scene_owns_input(void)
{
    return g_ui_information_scene_mode != UI_INFORMATION_SCENE_MODE_NONE;
}

static bool ui_information_scene_enter_with_mode(
    ui_information_scene_scope* scope, u16b mode)
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

    scope->previous_mode = g_ui_information_scene_mode;
    app_session_push_wait_scope(session, &scope->wait_scope,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, 0, 0);
    app_session_clear_inputs(session);
    g_ui_information_scene_mode = mode;
    scope->active = true;
    return true;
}

bool ui_information_scene_enter(ui_information_scene_scope* scope)
{
    return ui_information_scene_enter_with_mode(scope,
        UI_INFORMATION_SCENE_MODE_EXPLICIT);
}

bool ui_information_scene_enter_mirror(ui_information_scene_scope* scope)
{
    return ui_information_scene_enter_with_mode(scope,
        UI_INFORMATION_SCENE_MODE_MIRROR);
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

            if ((attr & TILE_FLAG) || ch == 0xFF)
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
            int start = x;

            while (x <= last_nonblank)
            {
                char buf[APP_INFORMATION_TEXT_MAX];
                int len = 0;

                while (x <= last_nonblank && Term->scr->a[y][x] == attr
                    && Term->scr->story[y][x] == story
                    && len < (int)sizeof(buf) - 1)
                {
                    unsigned char raw = (unsigned char)Term->scr->c[y][x];

                    if ((attr & TILE_FLAG) || raw == 0xFF)
                        buf[len++] = ' ';
                    else
                        buf[len++] = raw ? (char)raw : ' ';
                    x++;
                }

                buf[len] = '\0';
                if (!app_information_scene_add_text_ex(scene, (s16b)y,
                        (s16b)start, attr, story, buf))
                {
                    return false;
                }

                start = x;
                if (x > last_nonblank || Term->scr->a[y][x] != attr
                    || Term->scr->story[y][x] != story)
                    break;
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
            unsigned char raw = (unsigned char)Term->scr->c[cursor_y][cursor_x];
            char cursor_buf[2];

            if ((Term->scr->a[cursor_y][cursor_x] & TILE_FLAG) || raw == 0xFF)
                cursor_buf[0] = ' ';
            else
                cursor_buf[0] = raw ? (char)raw : ' ';
            cursor_buf[1] = '\0';

            if (!app_information_scene_add_text_ex(scene, (s16b)cursor_y,
                    (s16b)cursor_x, TERM_L_BLUE,
                    Term->scr->story[cursor_y][cursor_x], cursor_buf))
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
    app_information_scene scene;

    if (!ui_information_scene_capture_term(&scene))
        return false;

    return ui_information_scene_publish(&scene, false);
}

int ui_information_scene_wait_key(void)
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

            return (int)(input.payload.key.logical_key & 0xFFu);
        }

        ui_information_scene_term_xtra(TERM_XTRA_EVENT, 1);
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    }
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

    g_ui_information_scene_mode = scope->previous_mode;
    scope->previous_information_snapshot
        = mem_free(scope->previous_information_snapshot);
    scope->active = false;
    if (!restored_information
        && scope->previous_snapshot.scene == APP_SCENE_KIND_DUNGEON
        && Term
        && scope->previous_mode == UI_INFORMATION_SCENE_MODE_NONE)
    {
        do_cmd_redraw();
    }
    else
    {
        ui_information_scene_term_xtra(TERM_XTRA_FRESH, 0);
    }
}
