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

#ifndef INCLUDED_APP_SCENE_BOOTSTRAP_H
#define INCLUDED_APP_SCENE_BOOTSTRAP_H

#include "app-snapshot.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_BOOTSTRAP_FORMAT_VERSION 1u
#define APP_BOOTSTRAP_LOGICAL_COLS 80u
#define APP_BOOTSTRAP_LOGICAL_ROWS 24u
#define APP_BOOTSTRAP_OP_MAX 32u
#define APP_BOOTSTRAP_TEXT_MAX 160u

typedef enum app_bootstrap_op_flag {
    APP_BOOTSTRAP_OP_FLAG_NONE = 0x00u,
    APP_BOOTSTRAP_OP_FLAG_BOTTOM_ANCHORED = 0x01u
} app_bootstrap_op_flag;

typedef struct app_bootstrap_op {
    byte attr;
    byte flags;
    s16b row;
    s16b col;
    char text[APP_BOOTSTRAP_TEXT_MAX];
} app_bootstrap_op;

typedef struct app_bootstrap_scene {
    u16b format_version;
    u16b logical_cols;
    u16b logical_rows;
    u16b op_count;
    app_bootstrap_op ops[APP_BOOTSTRAP_OP_MAX];
} app_bootstrap_scene;

typedef struct app_bootstrap_snapshot {
    app_snapshot snapshot;
    app_snapshot_blob blobs[1];
    app_bootstrap_scene scene;
} app_bootstrap_snapshot;

void app_bootstrap_scene_init(app_bootstrap_scene* scene);
bool app_bootstrap_scene_add_text_ex(app_bootstrap_scene* scene, s16b row,
    s16b col, byte attr, byte flags, cptr text);
bool app_bootstrap_scene_add_text(app_bootstrap_scene* scene, s16b row,
    s16b col, byte attr, cptr text);
void app_bootstrap_snapshot_init(app_bootstrap_snapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_BOOTSTRAP_H */
