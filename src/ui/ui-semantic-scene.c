#include "angband.h"

#include "ui/ui-semantic-scene.h"

#include "app/app-session.h"
#include "platform-input.h"

void ui_semantic_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    platform_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

app_ui_panel* ui_semantic_scene_append_panel(app_ui_scene* scene,
    const ui_semantic_panel_config* config)
{
    app_ui_panel* panel;

    if (!scene || !config)
        return NULL;

    scene->flags |= config->scene_flags;
    panel = app_ui_scene_append_panel(scene, config->layer);
    if (!panel)
        return NULL;

    panel->style = config->style;
    panel->flags |= config->panel_flags;
    panel->accent_attr = config->accent_attr;
    app_ui_panel_set_widths(panel, config->min_width_px,
        config->width_cap_px);
    app_ui_panel_set_title(panel, config->title_attr,
        config->title ? config->title : "");
    if (config->subtitle && config->subtitle[0])
        app_ui_panel_set_subtitle(panel, config->subtitle_attr,
            config->subtitle);

    return panel;
}

app_ui_panel* ui_semantic_scene_begin_panel(app_ui_scene* scene,
    const ui_semantic_panel_config* config)
{
    if (!scene || !config)
        return NULL;

    app_ui_scene_init(scene);
    return ui_semantic_scene_append_panel(scene, config);
}

app_ui_panel* ui_semantic_scene_begin_browser(app_ui_scene* scene,
    byte title_attr, cptr title, byte subtitle_attr, cptr subtitle,
    byte accent_attr, u16b panel_flags, u16b min_width_px,
    u16b width_cap_px)
{
    const ui_semantic_panel_config config = {
        0,
        APP_UI_LAYER_BROWSER,
        APP_UI_PANEL_STYLE_BROWSER,
        panel_flags,
        title_attr,
        subtitle_attr,
        accent_attr,
        min_width_px,
        width_cap_px,
        title,
        subtitle
    };

    return ui_semantic_scene_begin_panel(scene, &config);
}

app_ui_panel* ui_semantic_scene_begin_plain(app_ui_scene* scene,
    u16b scene_flags, u16b layer, byte title_attr, cptr title,
    byte subtitle_attr, cptr subtitle, byte accent_attr,
    u16b min_width_px, u16b width_cap_px)
{
    const ui_semantic_panel_config config = {
        scene_flags,
        layer,
        APP_UI_PANEL_STYLE_PLAIN,
        0,
        title_attr,
        subtitle_attr,
        accent_attr,
        min_width_px,
        width_cap_px,
        title,
        subtitle
    };

    return ui_semantic_scene_begin_panel(scene, &config);
}

bool ui_semantic_scene_present_and_wait_key(const app_ui_scene* scene,
    bool nonrepeat, bool hidden_cursor, u16b wait_reason, int* out_key)
{
    int key;

    if (out_key)
        *out_key = ESCAPE;
    if (!scene || !ui_information_scene_present_ui(scene))
        return false;

    if (hidden_cursor)
    {
        key = ui_information_scene_wait_key_hidden_with_wait_reason(
            wait_reason);
    }
    else if (wait_reason != APP_WAIT_REASON_NONE)
    {
        key = ui_information_scene_wait_key_with_wait_reason(wait_reason);
    }
    else if (nonrepeat)
    {
        key = ui_information_scene_wait_key_nonrepeat();
    }
    else
    {
        key = ui_information_scene_wait_key();
    }

    if (out_key)
        *out_key = key;
    return true;
}

void ui_semantic_scene_clear_pending_input(void)
{
    app_session* session = app_session_current();

    if (session)
        app_session_clear_inputs(session);
    input_byte_queue_clear();
    input_clear_movement_commands();
}
