#ifndef INCLUDED_UI_INFORMATION_SCENE_H
#define INCLUDED_UI_INFORMATION_SCENE_H

#include "app/app-scene-information.h"
#include "app/app-session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_information_scene_scope {
    bool active;
    bool previous_active;
    app_snapshot previous_snapshot;
    app_information_snapshot* previous_information_snapshot;
    app_menu_snapshot* previous_menu_snapshot;
    app_wait_scope wait_scope;
} ui_information_scene_scope;

bool ui_information_scene_supported(void);
bool ui_information_scene_set_refresh_enabled(bool enabled);
bool ui_information_scene_enter(ui_information_scene_scope* scope);
bool ui_information_scene_capture_term(app_information_scene* scene);
bool ui_information_scene_present(const app_information_scene* scene);
bool ui_information_scene_present_document(
    const app_information_scene* scene);
bool ui_information_scene_present_term(void);
int ui_information_scene_wait_key(void);
int ui_information_scene_wait_key_nonrepeat(void);
bool ui_information_scene_is_active(void);
bool ui_information_scene_owns_input(void);
void ui_information_scene_leave(ui_information_scene_scope* scope);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_UI_INFORMATION_SCENE_H */
