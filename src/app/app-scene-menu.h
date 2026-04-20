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
