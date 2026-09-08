#ifndef INCLUDED_GAMEPLAY_TUTORIAL_H
#define INCLUDED_GAMEPLAY_TUTORIAL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum tutorial_mode {
    TUTORIAL_MODE_DISABLED = 0,
    TUTORIAL_MODE_NORMAL = 1,
    TUTORIAL_MODE_EXTENDED = 2
} tutorial_mode;

/* Only public, already revealed descriptions belong here. Never store world
 * object/monster indices or pointers in tutorial context. Strings are copied. */
typedef struct tutorial_context {
    char subject_type[48];
    char subject[160];
    char text[384];
} tutorial_context;

typedef enum tutorial_step_kind {
    TUTORIAL_STEP_INFO,
    TUTORIAL_STEP_ACTION,
    TUTORIAL_STEP_DECISION /* Read before the real choice; Continue dismisses. */
} tutorial_step_kind;

typedef enum tutorial_status {
    TUTORIAL_UNSEEN,
    TUTORIAL_IN_PROGRESS,
    TUTORIAL_COMPLETED,
    TUTORIAL_SKIPPED
} tutorial_status;

typedef struct tutorial_view {
    bool active;
    char id[80];
    char title[160];
    char body[2048];
    char action[128]; /* Semantic action(s), separated by | for alternatives. */
    char action_subject[48]; /* Required public semantic target, if any. */
    char anchor[48];
    tutorial_context context;
    tutorial_step_kind kind;
    int step; /* One based. */
    int step_count;
    bool can_continue;
    unsigned int revision;
    tutorial_mode level;
} tutorial_view;

/* Lazy loading also occurs at checkpoint. Reload is useful for content tests.
 * A failed catalogue reload leaves the previous valid catalogue intact. */
bool tutorial_load_catalogue(const char *path);
void tutorial_shutdown(void);
void tutorial_set_mode(tutorial_mode mode);
tutorial_mode tutorial_get_mode(void);
tutorial_mode tutorial_cycle_mode(void);
const char *tutorial_mode_name(tutorial_mode mode);
/* Per-character eligibility does not mutate the global tutorial preference.
 * Upgrade notices use the owning native UI, independently of this gate. */
void tutorial_set_character_blocked(bool blocked);
bool tutorial_character_blocked(void);
/* Legacy compatibility: true preserves Normal/Extended, or enables Extended
 * when currently Disabled; false selects Disabled. */
void tutorial_set_enabled(bool enabled);
bool tutorial_enabled(void);

/* Call after selecting a Tale, or let checkpoint detect it. This discards
 * transient queued observations when the selected Tale changes. */
void tutorial_sync_tale(void);
void tutorial_flush(void);

/* Observation only queues a lesson; it never opens UI or advances gameplay.
 * Priority comes from the catalogue. Reobservations refresh copied context. */
void tutorial_observe(const char *lesson_id, const tutorial_context *context);
/* false suspends the visible card; true resumes/activates at an input boundary.
 * Continue and committed actions stage a transition until the next true call. */
void tutorial_checkpoint(bool safe_to_present);
bool tutorial_get_view(tutorial_view *view);
bool tutorial_peek_view(tutorial_view *view); /* Also reads a hidden step. */
bool tutorial_is_active(void);
unsigned int tutorial_revision(void);
const char *tutorial_current_action(void);
tutorial_step_kind tutorial_current_kind(void);
void tutorial_menu_opened(const char *menu);
void tutorial_menu_closed(void);
void tutorial_continue(void);
void tutorial_skip(void);
void tutorial_disable(void); /* Caller persists global SDL preference. */

/* No completion for cancellation, failed validation, or attempted actions.
 * A committed attack includes a miss. subject_type can be NULL when irrelevant.
 * Menus/examine remain permitted as navigation during an action step. */
bool tutorial_action_permitted(const char *action);
bool tutorial_action_waiting(void); /* Includes a temporarily hidden action. */
void tutorial_action_finished(const char *action, const char *subject_type,
    bool committed);

/* Clear transient context/queues on death, level change, character change or
 * loss of a relevant visible subject. Progress remains available for reoffer. */
void tutorial_invalidate_context(void);
void tutorial_forget_observation(const char *lesson_id);
void tutorial_update_context(const tutorial_context *context);

/* Archive includes all catalogue entries; callers decide how to filter unseen
 * lessons. Text is copied, so rendering never retains catalogue pointers. */
int tutorial_archive_count(void);
/* Bracket the archive's owning menu, not individual replayed lessons. Preserve
 * the ordinary active lesson and queue while replay runs read-only. End restores
 * the prior visibility: an enclosing menu's hidden session stays hidden until
 * its own safe checkpoint. Turning Off or invalidating context discards it. */
void tutorial_archive_begin(void);
void tutorial_archive_end(void);
bool tutorial_archive_entry(int index, tutorial_view *view,
    tutorial_status *status);
tutorial_status tutorial_lesson_status(const char *lesson_id);
bool tutorial_replay(const char *lesson_id);
/* Clears this version's known lesson records only; preserves unknown IDs. */
void tutorial_reset_tale(void);

#endif
