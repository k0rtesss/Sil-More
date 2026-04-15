#ifndef INCLUDED_UI_INFORMATION_SCENE_H
#define INCLUDED_UI_INFORMATION_SCENE_H

#include "app/app-session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_information_scene_scope {
    bool active;
    bool previous_active;
    bool published_overlay;
    bool restore_snapshot;
    app_snapshot previous_snapshot;
    app_menu_snapshot* previous_menu_snapshot;
    app_wait_scope wait_scope;
    app_input_capture_scope input_capture_scope;
} ui_information_scene_scope;

bool ui_information_scene_supported(void);
bool ui_information_scene_set_refresh_enabled(bool enabled);
bool ui_information_scene_acquire(ui_information_scene_scope* scope);
bool ui_information_scene_claim_input(ui_information_scene_scope* scope,
    u16b reason);
bool ui_information_scene_enter(ui_information_scene_scope* scope);
bool ui_information_scene_present_ui(const app_ui_scene* scene);
bool ui_information_scene_present_overlay(ui_information_scene_scope* scope,
    const app_ui_scene* scene);
bool ui_information_scene_show_monster_recall(int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon,
    int* out_key);
int ui_information_scene_wait_key(void);
int ui_information_scene_wait_key_nonrepeat(void);
int ui_information_scene_wait_key_with_wait_reason(u16b reason);
int ui_information_scene_wait_key_hidden_with_wait_reason(u16b reason);
bool ui_information_scene_is_active(void);
bool ui_information_scene_owns_input(void);
void ui_information_scene_leave_without_restore(
    ui_information_scene_scope* scope);
void ui_information_scene_leave(ui_information_scene_scope* scope);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_UI_INFORMATION_SCENE_H */
