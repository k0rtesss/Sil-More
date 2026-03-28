#include "angband.h"

#include "app-scene-information.h"

void app_information_scene_init(app_information_scene* scene)
{
    if (!scene)
        return;

    memset(scene, 0, sizeof(*scene));
    scene->format_version = APP_INFORMATION_FORMAT_VERSION;
}

bool app_information_scene_add_text_ex(app_information_scene* scene, s16b row,
    s16b col, byte attr, byte story, cptr text)
{
    app_information_op* op;

    if (!scene || !text || !text[0]
        || scene->op_count >= APP_INFORMATION_OP_MAX)
    {
        return false;
    }

    op = &scene->ops[scene->op_count++];
    memset(op, 0, sizeof(*op));
    op->attr = attr;
    op->story = story;
    op->row = row;
    op->col = col;
    SDL_strlcpy(op->text, text, sizeof(op->text));
    return true;
}

bool app_information_scene_add_text(app_information_scene* scene, s16b row,
    s16b col, byte attr, cptr text)
{
    return app_information_scene_add_text_ex(scene, row, col, attr, 0, text);
}

void app_information_snapshot_init(app_information_snapshot* snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
    app_information_scene_init(&snapshot->scene);
    snapshot->snapshot.scene = APP_SCENE_KIND_INFORMATION;
    snapshot->snapshot.blobs = snapshot->blobs;
    snapshot->snapshot.blob_count = N_ELEMENTS(snapshot->blobs);
    snapshot->blobs[0].kind = APP_SNAPSHOT_BLOB_INFORMATION;
    snapshot->blobs[0].format_version = APP_INFORMATION_FORMAT_VERSION;
    snapshot->blobs[0].data = (const byte*)&snapshot->scene;
    snapshot->blobs[0].size = sizeof(snapshot->scene);
}
