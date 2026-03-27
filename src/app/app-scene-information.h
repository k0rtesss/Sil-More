#ifndef INCLUDED_APP_SCENE_INFORMATION_H
#define INCLUDED_APP_SCENE_INFORMATION_H

#include "app-snapshot.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INFORMATION_FORMAT_VERSION 1u
#define APP_INFORMATION_OP_MAX 512u
#define APP_INFORMATION_TEXT_MAX 160u

typedef struct app_information_op {
    byte attr;
    byte reserved;
    s16b row;
    s16b col;
    char text[APP_INFORMATION_TEXT_MAX];
} app_information_op;

typedef struct app_information_scene {
    u16b format_version;
    u16b flags;
    u16b op_count;
    u16b reserved;
    app_information_op ops[APP_INFORMATION_OP_MAX];
} app_information_scene;

typedef struct app_information_snapshot {
    app_snapshot snapshot;
    app_snapshot_blob blobs[1];
    app_information_scene scene;
} app_information_snapshot;

void app_information_scene_init(app_information_scene* scene);
bool app_information_scene_add_text(app_information_scene* scene, s16b row,
    s16b col, byte attr, cptr text);
void app_information_snapshot_init(app_information_snapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_INFORMATION_H */
