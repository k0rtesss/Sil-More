#include "angband.h"

#include "app-scene-menu.h"

void app_menu_snapshot_init(app_menu_snapshot* snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
    app_ui_scene_init(&snapshot->scene);
    snapshot->snapshot.scene = APP_SCENE_KIND_MENU;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
    snapshot->blobs[0].kind = APP_SNAPSHOT_BLOB_MENU;
    snapshot->blobs[0].format_version = APP_UI_FORMAT_VERSION;
    snapshot->blobs[0].data = (const byte*)&snapshot->scene;
    snapshot->blobs[0].size = sizeof(snapshot->scene);
}
