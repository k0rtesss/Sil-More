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

#include "app-scene-bootstrap.h"

void app_bootstrap_scene_init(app_bootstrap_scene* scene)
{
    if (!scene)
        return;

    memset(scene, 0, sizeof(*scene));
    scene->format_version = APP_BOOTSTRAP_FORMAT_VERSION;
    scene->logical_cols = APP_BOOTSTRAP_LOGICAL_COLS;
    scene->logical_rows = APP_BOOTSTRAP_LOGICAL_ROWS;
}

bool app_bootstrap_scene_add_text_ex(app_bootstrap_scene* scene, s16b row,
    s16b col, byte attr, byte flags, cptr text)
{
    app_bootstrap_op* op;

    if (!scene || !text || !text[0]
        || scene->op_count >= APP_BOOTSTRAP_OP_MAX)
    {
        return false;
    }

    op = &scene->ops[scene->op_count++];
    memset(op, 0, sizeof(*op));
    op->attr = attr;
    op->flags = flags;
    op->row = row;
    op->col = col;
    SDL_strlcpy(op->text, text, sizeof(op->text));
    return true;
}

bool app_bootstrap_scene_add_text(app_bootstrap_scene* scene, s16b row,
    s16b col, byte attr, cptr text)
{
    return app_bootstrap_scene_add_text_ex(scene, row, col, attr,
        APP_BOOTSTRAP_OP_FLAG_NONE, text);
}

void app_bootstrap_snapshot_init(app_bootstrap_snapshot* snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
    app_bootstrap_scene_init(&snapshot->scene);
    snapshot->snapshot.scene = APP_SCENE_KIND_BOOTSTRAP;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
    snapshot->blobs[0].kind = APP_SNAPSHOT_BLOB_BOOTSTRAP;
    snapshot->blobs[0].format_version = APP_BOOTSTRAP_FORMAT_VERSION;
    snapshot->blobs[0].data = (const byte*)&snapshot->scene;
    snapshot->blobs[0].size = sizeof(snapshot->scene);
}
