#ifndef INCLUDED_APP_SNAPSHOT_H
#define INCLUDED_APP_SNAPSHOT_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum app_scene_kind {
    APP_SCENE_KIND_NONE = 0,
    APP_SCENE_KIND_BOOTSTRAP = 1,
    APP_SCENE_KIND_DUNGEON = 2,
    APP_SCENE_KIND_OVERLAY = 3,
    APP_SCENE_KIND_MENU = 4,
    APP_SCENE_KIND_INFORMATION = 5
} app_scene_kind;

typedef enum app_snapshot_blob_kind {
    APP_SNAPSHOT_BLOB_NONE = 0,
    APP_SNAPSHOT_BLOB_HEADER = 1,
    APP_SNAPSHOT_BLOB_MAP = 2,
    APP_SNAPSHOT_BLOB_STATUS = 3,
    APP_SNAPSHOT_BLOB_MESSAGES = 4,
    APP_SNAPSHOT_BLOB_PANES = 5,
    APP_SNAPSHOT_BLOB_OVERLAY = 6,
    APP_SNAPSHOT_BLOB_INFORMATION = 7,
    APP_SNAPSHOT_BLOB_CUSTOM = 8,
    APP_SNAPSHOT_BLOB_MENU = 9,
    APP_SNAPSHOT_BLOB_BOOTSTRAP = 10
} app_snapshot_blob_kind;

typedef enum app_snapshot_flag {
    APP_SNAPSHOT_FLAG_PARTIAL = 0x0001u,
    APP_SNAPSHOT_FLAG_DIRTY = 0x0002u,
    APP_SNAPSHOT_FLAG_WAITING = 0x0004u
} app_snapshot_flag;

typedef struct app_snapshot_blob {
    u16b kind;
    u16b format_version;
    const byte* data;
    size_t size;
} app_snapshot_blob;

typedef struct app_snapshot {
    u64b revision;
    u16b scene;
    u16b flags;
    const app_snapshot_blob* blobs;
    size_t blob_count;
} app_snapshot;

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SNAPSHOT_H */
