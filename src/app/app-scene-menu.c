/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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
