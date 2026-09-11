#include "angband.h"
#include "tutorial.h"
#include "blitz.h"
#include "cJSON.h"
#include "metarun/metarun-internal.h"

#define TUTORIAL_MAX_LESSONS 768
#define TUTORIAL_MAX_STEPS 16
/* Observations are deduplicated by lesson. Keep room for the whole catalogue:
 * a burst of discoveries must not silently discard an event-only lesson. */
#define TUTORIAL_MAX_QUEUE TUTORIAL_MAX_LESSONS
#define TUTORIAL_MAX_JSON (1024 * 1024)

typedef struct tutorial_step {
    char text[2048];
    char action[128];
    char anchor[48];
    char subject_type[48];
    tutorial_step_kind kind;
} tutorial_step;

typedef struct tutorial_lesson {
    char id[80];
    char title[160];
    int priority;
    tutorial_mode level;
    int count;
    tutorial_step *steps;
} tutorial_lesson;

typedef struct tutorial_pending {
    int lesson;
    tutorial_context context;
} tutorial_pending;

static tutorial_lesson *catalogue;
static int catalogue_count;
static bool catalogue_attempted;
static bool enabled = true;
static tutorial_mode mode = TUTORIAL_MODE_EXTENDED;
static bool character_blocked;
static bool tale_loaded;
static u32b tale_id;
static cJSON *progress;
static bool progress_writable;
static bool progress_dirty;
static char progress_path[1024];
static tutorial_pending queue[TUTORIAL_MAX_QUEUE];
static int queue_count;
static int active = -1;
static int active_step;
static bool visible;
static bool replaying;
static bool awaiting_checkpoint;
static tutorial_context active_context;
static unsigned int revision;
static char current_menu[80];

typedef struct tutorial_suspended {
    bool valid;
    u32b tale_id;
    int active;
    int step;
    bool visible;
    bool replaying;
    bool awaiting_checkpoint;
    tutorial_context context;
    tutorial_pending queue[TUTORIAL_MAX_QUEUE];
    int queue_count;
    char menu[80];
} tutorial_suspended;

static tutorial_suspended archive_session;
static int archive_depth;

static void free_catalogue(tutorial_lesson *items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; ++i) free(items[i].steps);
    free(items);
}

static const char *json_string(const cJSON *object, const char *key)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) ? value->valuestring : NULL;
}

/* Refuse oversized input before allocation. A broken sidecar must never make
 * a Tale unplayable, nor get silently replaced by a fresh empty document. */
static cJSON *read_json(const char *path, bool *exists)
{
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    cJSON *root = NULL;
    char *data;
    Sint64 length;
    SDL_PathInfo info;
    *exists = SDL_GetPathInfo(path, &info);
    if (!file) return NULL;
    *exists = true;
    length = SDL_GetIOSize(file);
    if (length < 1 || length > TUTORIAL_MAX_JSON) {
        SDL_CloseIO(file);
        return NULL;
    }
    data = malloc((size_t)length + 1);
    if (data) {
        if (SDL_ReadIO(file, data, (size_t)length) == (size_t)length) {
            data[length] = '\0';
            root = cJSON_ParseWithOpts(data, NULL, true);
        }
        free(data);
    }
    SDL_CloseIO(file);
    return root;
}

static bool copy_field(char *dst, size_t size, const cJSON *object,
    const char *key, bool required)
{
    const char *value = json_string(object, key);
    if (!value) return !required && !cJSON_GetObjectItemCaseSensitive(object, key);
    if (strlen(value) >= size || (required && !*value)) return false;
    SDL_strlcpy(dst, value, size);
    return true;
}

static int lesson_index(const char *id)
{
    if (!id) return -1;
    for (int i = 0; i < catalogue_count; ++i)
        if (!strcmp(catalogue[i].id, id)) return i;
    return -1;
}

static bool lesson_allowed(int index)
{
    return enabled && !character_blocked && index >= 0
        && index < catalogue_count && catalogue[index].level <= mode;
}

static void clear_current_context(void)
{
    queue_count = 0;
    active = -1;
    active_step = 0;
    visible = false;
    replaying = false;
    awaiting_checkpoint = false;
    memset(&active_context, 0, sizeof(active_context));
    current_menu[0] = '\0';
    ++revision;
}

void tutorial_invalidate_context(void)
{
    archive_session.valid = false;
    clear_current_context();
}

void tutorial_forget_observation(const char *id)
{
    int index = lesson_index(id);
    if (index < 0) return;
    for (int i = queue_count - 1; i >= 0; --i) {
        if (queue[i].lesson != index) continue;
        memmove(&queue[i], &queue[i + 1],
            (size_t)(queue_count - i - 1) * sizeof(queue[0]));
        --queue_count;
    }
    if (active == index) {
        active = -1;
        active_step = 0;
        visible = false;
        replaying = false;
        awaiting_checkpoint = false;
        memset(&active_context, 0, sizeof(active_context));
        ++revision;
    }
}

static void filter_observations(void)
{
    if (active >= 0 && !replaying && !lesson_allowed(active))
        tutorial_forget_observation(catalogue[active].id);
    for (int i = queue_count - 1; i >= 0; --i) {
        if (lesson_allowed(queue[i].lesson)) continue;
        memmove(&queue[i], &queue[i + 1],
            (size_t)(queue_count - i - 1) * sizeof(queue[0]));
        --queue_count;
    }
    if (!archive_session.valid) return;
    if (archive_session.active >= 0 && !archive_session.replaying
        && !lesson_allowed(archive_session.active)) {
        archive_session.active = -1;
        archive_session.step = 0;
        archive_session.visible = false;
        archive_session.awaiting_checkpoint = false;
    }
    for (int i = archive_session.queue_count - 1; i >= 0; --i) {
        if (lesson_allowed(archive_session.queue[i].lesson)) continue;
        memmove(&archive_session.queue[i], &archive_session.queue[i + 1],
            (size_t)(archive_session.queue_count - i - 1) * sizeof(queue[0]));
        --archive_session.queue_count;
    }
}

bool tutorial_load_catalogue(const char *path)
{
    bool exists;
    cJSON *root = path ? read_json(path, &exists) : NULL;
    cJSON *lessons = cJSON_GetObjectItemCaseSensitive(root, "lessons");
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    int count = cJSON_GetArraySize(lessons);
    tutorial_lesson *next = NULL;
    bool valid = cJSON_IsObject(root) && cJSON_IsNumber(version)
        && version->valuedouble == 1 && cJSON_IsArray(lessons)
        && count > 0 && count <= TUTORIAL_MAX_LESSONS;
    if (valid) {
        next = calloc((size_t)count, sizeof(*next));
        valid = next != NULL;
    }
    for (int i = 0; valid && i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(lessons, i);
        cJSON *steps = cJSON_GetObjectItemCaseSensitive(item, "steps");
        cJSON *priority = cJSON_GetObjectItemCaseSensitive(item, "priority");
        const char *level = json_string(item, "level");
        tutorial_lesson *lesson = &next[i];
        valid = copy_field(lesson->id, sizeof(lesson->id), item, "id", true)
            && copy_field(lesson->title, sizeof(lesson->title), item, "title", true)
            && cJSON_IsArray(steps);
        /* IDs are keys, never paths. Restrict spelling to keep stable keys easy
         * to inspect and to catch accidental whitespace in authored content. */
        for (const char *s = lesson->id; valid && *s; ++s)
            valid = (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')
                || *s == '-' || *s == '_' || *s == '.';
        for (int j = 0; valid && j < i; ++j)
            valid = strcmp(lesson->id, next[j].id) != 0;
        lesson->priority = cJSON_IsNumber(priority) ? priority->valueint : 0;
        lesson->level = TUTORIAL_MODE_NORMAL;
        if (level) {
            if (!strcmp(level, "extended")) lesson->level = TUTORIAL_MODE_EXTENDED;
            else if (strcmp(level, "normal")) valid = false;
        } else if (cJSON_GetObjectItemCaseSensitive(item, "level")) valid = false;
        lesson->count = cJSON_GetArraySize(steps);
        valid = valid && lesson->count > 0 && lesson->count <= TUTORIAL_MAX_STEPS;
        if (valid) {
            lesson->steps = calloc((size_t)lesson->count, sizeof(*lesson->steps));
            valid = lesson->steps != NULL;
        }
        for (int j = 0; valid && j < lesson->count; ++j) {
            cJSON *entry = cJSON_GetArrayItem(steps, j);
            tutorial_step *step = &lesson->steps[j];
            const char *kind = json_string(entry, "kind");
            valid = copy_field(step->text, sizeof(step->text), entry, "text", true)
                && copy_field(step->action, sizeof(step->action), entry, "action", false)
                && copy_field(step->anchor, sizeof(step->anchor), entry, "anchor", false)
                && copy_field(step->subject_type, sizeof(step->subject_type), entry,
                    "subject_type", false);
            step->kind = step->action[0] ? TUTORIAL_STEP_ACTION : TUTORIAL_STEP_INFO;
            if (kind) {
                if (!strcmp(kind, "info")) step->kind = TUTORIAL_STEP_INFO;
                else if (!strcmp(kind, "action")) step->kind = TUTORIAL_STEP_ACTION;
                else if (!strcmp(kind, "decision")) step->kind = TUTORIAL_STEP_DECISION;
                else valid = false;
            } else if (cJSON_GetObjectItemCaseSensitive(entry, "kind")) valid = false;
            if (step->kind == TUTORIAL_STEP_ACTION && !step->action[0]) valid = false;
            if (step->kind != TUTORIAL_STEP_ACTION && step->action[0]) valid = false;
            if (!step->anchor[0]) SDL_strlcpy(step->anchor, "none", sizeof(step->anchor));
        }
    }
    cJSON_Delete(root);
    catalogue_attempted = true;
    if (!valid) {
        free_catalogue(next, count);
        log_warn("Gameplay tutorial catalogue unavailable or invalid: %s", path ? path : "(unset)");
        return false;
    }
    tutorial_invalidate_context();
    free_catalogue(catalogue, catalogue_count);
    catalogue = next;
    catalogue_count = count;
    return true;
}

static cJSON *lesson_progress(const char *id)
{
    cJSON *lessons = cJSON_GetObjectItemCaseSensitive(progress, "lessons");
    return cJSON_GetObjectItemCaseSensitive(lessons, id);
}

tutorial_status tutorial_lesson_status(const char *id)
{
    cJSON *entry = lesson_progress(id);
    const char *status = json_string(entry, "status");
    if (!status) return TUTORIAL_UNSEEN;
    if (!strcmp(status, "completed")) return TUTORIAL_COMPLETED;
    if (!strcmp(status, "skipped")) return TUTORIAL_SKIPPED;
    if (!strcmp(status, "in-progress")) return TUTORIAL_IN_PROGRESS;
    return TUTORIAL_UNSEEN;
}

static void save_step(const char *id, int step, const char *status)
{
    cJSON *lessons = cJSON_GetObjectItemCaseSensitive(progress, "lessons");
    cJSON *entry = lesson_progress(id);
    if (!lessons || replaying) return;
    if (!cJSON_IsObject(entry)) {
        entry = cJSON_CreateObject();
        if (!entry) return;
        cJSON_DeleteItemFromObjectCaseSensitive(lessons, id);
        cJSON_AddItemToObject(lessons, id, entry);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(entry, "status");
    cJSON_DeleteItemFromObjectCaseSensitive(entry, "step");
    cJSON_AddStringToObject(entry, "status", status);
    cJSON_AddNumberToObject(entry, "step", step);
    progress_dirty = true;
}

void tutorial_flush(void)
{
    char temporary[1100];
    char *text;
    SDL_IOStream *file;
    bool ok;
    if (!progress_dirty || !progress_writable || !progress || !*progress_path) return;
    text = cJSON_Print(progress);
    if (!text) return;
    strnfmt(temporary, sizeof(temporary), "%s.tmp", progress_path);
    file = SDL_IOFromFile(temporary, "wb");
    ok = file && SDL_WriteIO(file, text, strlen(text)) == strlen(text);
    if (file && !SDL_CloseIO(file)) ok = false;
    cJSON_free(text);
    if (ok) ok = SDL_RenamePath(temporary, progress_path);
    if (!ok) {
        log_warn("Could not save gameplay tutorial progress: %s", progress_path);
        SDL_RemovePath(temporary);
        return;
    }
    progress_dirty = false;
}

void tutorial_sync_tale(void)
{
    const metarun *tale = metarun_current();
    bool exists = false;
    cJSON *version;
    cJSON *lessons;
    if (run_mode_is_blitz() || !tale) {
        if (tale_loaded) {
            tutorial_flush();
            tutorial_invalidate_context();
            cJSON_Delete(progress);
            progress = NULL;
            tale_loaded = false;
        }
        return;
    }
    if (tale_loaded && tale_id == tale->id) return;
    tutorial_flush();
    tutorial_invalidate_context();
    cJSON_Delete(progress);
    progress = NULL;
    progress_dirty = false;
    progress_writable = false;
    progress_path[0] = '\0';
    tale_id = tale->id;
    tale_loaded = true;
    if (!build_meta_path(progress_path, sizeof(progress_path), tale, "tutorials.json")) return;
    ensure_run_dir(tale);
    progress = read_json(progress_path, &exists);
    version = cJSON_GetObjectItemCaseSensitive(progress, "version");
    lessons = cJSON_GetObjectItemCaseSensitive(progress, "lessons");
    if (progress && (!cJSON_IsObject(progress) || !cJSON_IsNumber(version)
        || version->valuedouble != 1 || !cJSON_IsObject(lessons))) {
        cJSON_Delete(progress);
        progress = NULL;
    }
    if (!progress) {
        progress = cJSON_CreateObject();
        if (progress) {
            cJSON_AddNumberToObject(progress, "version", 1);
            cJSON_AddNumberToObject(progress, "tale_id", tale_id);
            cJSON_AddObjectToObject(progress, "lessons");
        }
        if (exists) log_warn("Gameplay tutorial progress unreadable; preserving existing sidecar: %s", progress_path);
        progress_writable = !exists;
    } else progress_writable = true;
}

void tutorial_set_mode(tutorial_mode value)
{
    if (value < TUTORIAL_MODE_DISABLED || value > TUTORIAL_MODE_EXTENDED)
        value = TUTORIAL_MODE_EXTENDED;
    if (mode == value) return;
    mode = value;
    enabled = mode != TUTORIAL_MODE_DISABLED;
    if (!enabled) tutorial_invalidate_context();
    else filter_observations();
    ++revision;
}

tutorial_mode tutorial_get_mode(void) { return mode; }

tutorial_mode tutorial_cycle_mode(void)
{
    tutorial_set_mode((tutorial_mode)((mode + 1) % 3));
    return mode;
}

const char *tutorial_mode_name(tutorial_mode value)
{
    switch (value) {
    case TUTORIAL_MODE_DISABLED: return "Disabled";
    case TUTORIAL_MODE_NORMAL: return "Normal";
    default: return "Extended";
    }
}

void tutorial_set_character_blocked(bool value)
{
    if (character_blocked == value) return;
    character_blocked = value;
    if (value) {
        archive_session.valid = false;
        if (!replaying) tutorial_invalidate_context();
    }
    ++revision;
}

bool tutorial_character_blocked(void) { return character_blocked; }

void tutorial_set_enabled(bool value)
{
    if (!value) tutorial_set_mode(TUTORIAL_MODE_DISABLED);
    else if (!enabled) tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
}

bool tutorial_enabled(void) { return enabled; }
void tutorial_disable(void) { tutorial_set_enabled(false); }
unsigned int tutorial_revision(void) { return revision; }

bool tutorial_is_active(void)
{
    return (replaying || lesson_allowed(active)) && !run_mode_is_blitz()
        && visible && active >= 0;
}

const char *tutorial_current_action(void)
{
    return lesson_allowed(active) && !run_mode_is_blitz() && !replaying
        ? catalogue[active].steps[active_step].action : "";
}

tutorial_step_kind tutorial_current_kind(void)
{
    return lesson_allowed(active) && !run_mode_is_blitz() && !replaying
        ? catalogue[active].steps[active_step].kind : TUTORIAL_STEP_INFO;
}

bool tutorial_action_waiting(void)
{
    return lesson_allowed(active) && !run_mode_is_blitz() && !replaying
        && !awaiting_checkpoint
        && catalogue[active].steps[active_step].kind == TUTORIAL_STEP_ACTION;
}

static void copy_context(tutorial_context *dst, const tutorial_context *src)
{
    memset(dst, 0, sizeof(*dst));
    if (src) {
        SDL_strlcpy(dst->subject_type, src->subject_type, sizeof(dst->subject_type));
        SDL_strlcpy(dst->subject, src->subject, sizeof(dst->subject));
        SDL_strlcpy(dst->text, src->text, sizeof(dst->text));
    }
}

void tutorial_update_context(const tutorial_context *context)
{
    if (active < 0) return;
    copy_context(&active_context, context);
    ++revision;
}

static void ensure_catalogue(void)
{
    char path[1024];
    if (!catalogue_attempted && ANGBAND_DIR_HELP &&
        path_build(path, sizeof(path), ANGBAND_DIR_HELP, "tutorials.json"))
        tutorial_load_catalogue(path);
}

bool tutorial_lesson_enabled(const char *id)
{
    if (!enabled || character_blocked || run_mode_is_blitz()) return false;
    tutorial_sync_tale();
    ensure_catalogue();
    return tale_loaded && lesson_allowed(lesson_index(id));
}

void tutorial_observe(const char *id, const tutorial_context *context)
{
    int index;
    tutorial_status status;
    if (!enabled || character_blocked || run_mode_is_blitz() || archive_depth) return;
    tutorial_sync_tale();
    ensure_catalogue();
    if (!tale_loaded || (index = lesson_index(id)) < 0 || !lesson_allowed(index)) return;
    status = tutorial_lesson_status(id);
    if (status == TUTORIAL_COMPLETED || status == TUTORIAL_SKIPPED) return;
    if (active == index) {
        /* Do not reset input focus on identical repeated visibility scans. */
        tutorial_context copied;
        copy_context(&copied, context);
        if (memcmp(&copied, &active_context, sizeof(copied)))
            tutorial_update_context(context);
        return;
    }
    for (int i = 0; i < queue_count; ++i) {
        if (queue[i].lesson != index) continue;
        copy_context(&queue[i].context, context);
        return;
    }
    if (queue_count == TUTORIAL_MAX_QUEUE) {
        int lowest = 0;
        for (int i = 1; i < queue_count; ++i)
            if (catalogue[queue[i].lesson].priority < catalogue[queue[lowest].lesson].priority)
                lowest = i;
        if (catalogue[index].priority <= catalogue[queue[lowest].lesson].priority) return;
        queue[lowest].lesson = index;
        copy_context(&queue[lowest].context, context);
        return;
    }
    queue[queue_count].lesson = index;
    copy_context(&queue[queue_count].context, context);
    ++queue_count;
}

static void activate_observation(int position)
{
    active = queue[position].lesson;
    active_context = queue[position].context;
    memmove(&queue[position], &queue[position + 1],
        (size_t)(queue_count - position - 1) * sizeof(queue[0]));
    --queue_count;
    cJSON *step = cJSON_GetObjectItemCaseSensitive(lesson_progress(catalogue[active].id), "step");
    active_step = tutorial_lesson_status(catalogue[active].id) == TUTORIAL_IN_PROGRESS
        && cJSON_IsNumber(step) && step->valuedouble == step->valueint
        ? step->valueint : 0;
    if (active_step < 0 || active_step >= catalogue[active].count) active_step = 0;
    replaying = false;
    save_step(catalogue[active].id, active_step, "in-progress");
}

bool tutorial_focus_observation(const char *id)
{
    int index;
    if (replaying || archive_depth || !tutorial_lesson_enabled(id)) return false;
    index = lesson_index(id);
    if (active == index) return true;
    for (int i = 0; i < queue_count; ++i) {
        if (queue[i].lesson != index) continue;
        tutorial_pending previous = {active, active_context};
        activate_observation(i);
        /* Removing the requested observation made room for the suspended one. */
        if (previous.lesson >= 0) queue[queue_count++] = previous;
        visible = false;
        awaiting_checkpoint = false;
        ++revision;
        return true;
    }
    return false;
}

void tutorial_checkpoint(bool safe_to_present)
{
    tutorial_sync_tale();
    ensure_catalogue();
    if ((!replaying && (!enabled || character_blocked || archive_depth))
        || run_mode_is_blitz() || !tale_loaded) return;
    filter_observations();
    if (!safe_to_present) {
        if (visible) { visible = false; ++revision; }
        return;
    }
    /* A committed step yields to newly observed safety lessons before the
     * next instructional step. Never replace a card while input owns it. */
    if (active >= 0 && awaiting_checkpoint && !replaying && queue_count) {
        int best = 0;
        for (int i = 1; i < queue_count; ++i)
            if (catalogue[queue[i].lesson].priority > catalogue[queue[best].lesson].priority)
                best = i;
        if (catalogue[queue[best].lesson].priority > catalogue[active].priority) {
            int slot = queue_count < TUTORIAL_MAX_QUEUE ? queue_count++ : -1;
            if (slot < 0) {
                for (int i = 0; i < queue_count; ++i)
                    if (i != best && (slot < 0 || catalogue[queue[i].lesson].priority
                        < catalogue[queue[slot].lesson].priority)) slot = i;
            }
            if (slot >= 0) {
                queue[slot].lesson = active;
                queue[slot].context = active_context;
            }
            active = -1;
        }
    }
    if (active < 0 && queue_count) {
        int best = 0;
        for (int i = 1; i < queue_count; ++i)
            if (catalogue[queue[i].lesson].priority > catalogue[queue[best].lesson].priority)
                best = i;
        activate_observation(best);
    }
    if (active >= 0 && (!visible || awaiting_checkpoint)) {
        visible = true;
        awaiting_checkpoint = false;
        ++revision;
    }
    tutorial_flush();
}

static void advance_step(void)
{
    if (active < 0) return;
    ++active_step;
    if (active_step >= catalogue[active].count) {
        save_step(catalogue[active].id, active_step, "completed");
        active = -1;
        active_step = 0;
        replaying = false;
    } else save_step(catalogue[active].id, active_step, "in-progress");
    visible = false;
    awaiting_checkpoint = true;
    ++revision;
    tutorial_flush();
}

void tutorial_continue(void)
{
    if (tutorial_is_active() && tutorial_current_kind() != TUTORIAL_STEP_ACTION)
        advance_step();
}

void tutorial_skip(void)
{
    if (active < 0) return;
    save_step(catalogue[active].id, active_step, "skipped");
    active = -1;
    visible = false;
    replaying = false;
    ++revision;
    tutorial_flush();
}

static bool action_matches(const char *list, const char *action)
{
    size_t length;
    if (!action || !*action) return false;
    length = strlen(action);
    while (list && *list) {
        const char *end = strchr(list, '|');
        size_t part = end ? (size_t)(end - list) : strlen(list);
        if (length == part && !strncmp(list, action, part)) return true;
        list = end ? end + 1 : NULL;
    }
    return false;
}

bool tutorial_action_permitted(const char *action)
{
    if (!tutorial_is_active()) return true;
    if (tutorial_current_kind() != TUTORIAL_STEP_ACTION) return false;
    if (action_matches(tutorial_current_action(), action)) return true;
    return action && (!strcmp(action, "open-menu") || !strcmp(action, "examine")
        || !strcmp(action, "close-menu"));
}

void tutorial_action_finished(const char *action, const char *subject_type,
    bool committed)
{
    const tutorial_step *step;
    if (!committed || !tutorial_action_waiting()) return;
    step = &catalogue[active].steps[active_step];
    if (!action_matches(step->action, action)) return;
    if (step->subject_type[0] && (!subject_type || strcmp(step->subject_type, subject_type))) return;
    advance_step();
}

void tutorial_menu_opened(const char *menu)
{
    SDL_strlcpy(current_menu, menu ? menu : "", sizeof(current_menu));
    tutorial_action_finished("open-menu", current_menu, true);
}

void tutorial_menu_closed(void) { current_menu[0] = '\0'; }

static void render_text(char *out, size_t size, const char *text,
    const tutorial_context *context)
{
    size_t used = 0;
    while (*text && used + 1 < size) {
        const char *replacement = NULL;
        size_t skip = 0;
        if (!strncmp(text, "{subject}", 9)) { replacement = context->subject; skip = 9; }
        else if (!strncmp(text, "{context}", 9)) { replacement = context->text; skip = 9; }
        else if (!strncmp(text, "{detail}", 8)) { replacement = context->text; skip = 8; }
        if (replacement) {
            size_t length = strlen(replacement);
            if (length > size - used - 1) length = size - used - 1;
            memcpy(out + used, replacement, length);
            used += length;
            text += skip;
        } else out[used++] = *text++;
    }
    out[used] = '\0';
}

static void fill_view(tutorial_view *view, int index, int step_index,
    const tutorial_context *context)
{
    const tutorial_lesson *lesson = &catalogue[index];
    const tutorial_step *step = &lesson->steps[step_index];
    memset(view, 0, sizeof(*view));
    SDL_strlcpy(view->id, lesson->id, sizeof(view->id));
    SDL_strlcpy(view->title, lesson->title, sizeof(view->title));
    render_text(view->body, sizeof(view->body), step->text, context);
    SDL_strlcpy(view->action, step->action, sizeof(view->action));
    SDL_strlcpy(view->action_subject, step->subject_type, sizeof(view->action_subject));
    SDL_strlcpy(view->anchor, step->anchor, sizeof(view->anchor));
    view->context = *context;
    view->kind = step->kind;
    view->step = step_index + 1;
    view->step_count = lesson->count;
    view->can_continue = step->kind != TUTORIAL_STEP_ACTION;
    view->revision = revision;
    view->level = lesson->level;
}

bool tutorial_peek_view(tutorial_view *view)
{
    if (!view) return false;
    memset(view, 0, sizeof(*view));
    if ((!replaying && !lesson_allowed(active)) || run_mode_is_blitz() || active < 0) return false;
    fill_view(view, active, active_step, &active_context);
    if (replaying) {
        view->kind = TUTORIAL_STEP_INFO;
        view->can_continue = true;
        view->action[0] = '\0';
    }
    view->active = visible;
    return true;
}

bool tutorial_get_view(tutorial_view *view)
{
    if (!view) return false;
    if (tutorial_is_active()) return tutorial_peek_view(view);
    memset(view, 0, sizeof(*view));
    return false;
}

int tutorial_archive_count(void) { ensure_catalogue(); return catalogue_count; }

void tutorial_archive_begin(void)
{
    if (archive_depth) {
        ++archive_depth;
        clear_current_context();
        return;
    }
    tutorial_sync_tale();
    archive_depth = 1;
    archive_session.valid = tale_loaded && !run_mode_is_blitz();
    archive_session.tale_id = tale_id;
    archive_session.active = active;
    archive_session.step = active_step;
    archive_session.visible = visible;
    archive_session.replaying = replaying;
    archive_session.awaiting_checkpoint = awaiting_checkpoint;
    archive_session.context = active_context;
    memcpy(archive_session.queue, queue, sizeof(queue));
    archive_session.queue_count = queue_count;
    SDL_strlcpy(archive_session.menu, current_menu, sizeof(archive_session.menu));
    clear_current_context();
}

void tutorial_archive_end(void)
{
    bool resume_visible = false;
    if (archive_depth <= 0 || --archive_depth > 0) return;
    tutorial_sync_tale();
    clear_current_context();
    if (archive_session.valid && enabled && !character_blocked && tale_loaded
        && archive_session.tale_id == tale_id && !run_mode_is_blitz()) {
        active = archive_session.active;
        active_step = archive_session.step;
        visible = archive_session.visible;
        replaying = archive_session.replaying;
        awaiting_checkpoint = archive_session.awaiting_checkpoint;
        active_context = archive_session.context;
        memcpy(queue, archive_session.queue, sizeof(queue));
        queue_count = archive_session.queue_count;
        SDL_strlcpy(current_menu, archive_session.menu, sizeof(current_menu));
        resume_visible = visible;
    }
    archive_session.valid = false;
    filter_observations();
    if (resume_visible) tutorial_checkpoint(true);
}

/* Replays teach the same mechanic without claiming the original encounter is
 * still present. Keep numeric details empty and use a generic public subject
 * so context placeholders do not leave broken sentences in the archive. */
static void archive_context(int index, tutorial_context *context)
{
    const char *id = catalogue[index].id;
    const char *subject = catalogue[index].title;
    memset(context, 0, sizeof(*context));
    if (!strncmp(id, "monster.", 8) || !strncmp(id, "combat.", 7))
        subject = "This creature";
    else if (!strncmp(id, "item.", 5) || !strncmp(id, "identification.", 15))
        subject = "This item";
    SDL_strlcpy(context->subject, subject, sizeof(context->subject));
}

bool tutorial_archive_entry(int index, tutorial_view *view, tutorial_status *status)
{
    tutorial_context context;
    ensure_catalogue();
    if (!view || index < 0 || index >= catalogue_count) return false;
    archive_context(index, &context);
    fill_view(view, index, 0, &context);
    if (status) *status = tutorial_lesson_status(catalogue[index].id);
    return true;
}

bool tutorial_replay(const char *id)
{
    int index;
    tutorial_sync_tale();
    ensure_catalogue();
    if (run_mode_is_blitz() || !tale_loaded || (index = lesson_index(id)) < 0) return false;
    clear_current_context();
    active = index;
    active_step = 0;
    archive_context(index, &active_context);
    replaying = true;
    awaiting_checkpoint = true;
    return true;
}

void tutorial_reset_tale(void)
{
    cJSON *lessons;
    tutorial_sync_tale();
    ensure_catalogue();
    if (!tale_loaded || !progress) return;
    tutorial_invalidate_context();
    lessons = cJSON_GetObjectItemCaseSensitive(progress, "lessons");
    /* Reset only lessons this version understands. Preserve future IDs. */
    for (int i = 0; i < catalogue_count; ++i)
        cJSON_DeleteItemFromObjectCaseSensitive(lessons, catalogue[i].id);
    progress_dirty = true;
    tutorial_flush();
}

void tutorial_shutdown(void)
{
    tutorial_flush();
    tutorial_invalidate_context();
    archive_depth = 0;
    character_blocked = false;
    free_catalogue(catalogue, catalogue_count);
    catalogue = NULL;
    catalogue_count = 0;
    catalogue_attempted = false;
    cJSON_Delete(progress);
    progress = NULL;
    tale_loaded = false;
    progress_dirty = false;
    progress_writable = false;
    progress_path[0] = '\0';
}
