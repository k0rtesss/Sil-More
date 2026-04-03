#ifndef INCLUDED_APP_SCENE_MENU_H
#define INCLUDED_APP_SCENE_MENU_H

#include "app-snapshot.h"
#include "app-ui.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_menu_snapshot {
    app_snapshot snapshot;
    app_snapshot_blob blobs[1];
    app_ui_scene scene;
} app_menu_snapshot;

void app_menu_snapshot_init(app_menu_snapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_MENU_H */
