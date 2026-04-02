#ifndef INCLUDED_APP_SCENE_INFORMATION_H
#define INCLUDED_APP_SCENE_INFORMATION_H

#include "app-snapshot.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INFORMATION_FORMAT_VERSION 3u
#define APP_INFORMATION_OP_MAX 4096u
#define APP_INFORMATION_TEXT_MAX 160u
#define APP_INFORMATION_SCENE_FLAG_TERM_MIRROR 0x0001u
#define APP_INFORMATION_SCENE_FLAG_OVERLAY_DUNGEON 0x0002u

typedef enum app_information_op_kind {
    APP_INFORMATION_OP_KIND_TEXT = 0,
    APP_INFORMATION_OP_KIND_CELL = 1,
    APP_INFORMATION_OP_KIND_CURSOR = 2
} app_information_op_kind;

typedef struct app_information_op {
    byte kind;
    byte attr;
    byte story;
    byte width;
    s16b row;
    s16b col;
    byte terrain_attr;
    char ch;
    char terrain_char;
    byte reserved;
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
bool app_information_scene_add_text_ex(app_information_scene* scene, s16b row,
    s16b col, byte attr, byte story, cptr text);
bool app_information_scene_add_text(app_information_scene* scene, s16b row,
    s16b col, byte attr, cptr text);
bool app_information_scene_add_cell_ex(app_information_scene* scene, s16b row,
    s16b col, byte attr, char ch, byte terrain_attr, char terrain_char,
    byte story, byte width);
bool app_information_scene_add_cursor(app_information_scene* scene, s16b row,
    s16b col, byte attr, byte width);
void app_information_snapshot_init(app_information_snapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_INFORMATION_H */
