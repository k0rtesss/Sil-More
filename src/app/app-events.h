#ifndef INCLUDED_APP_EVENTS_H
#define INCLUDED_APP_EVENTS_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum app_event_kind {
    APP_EVENT_KIND_NONE = 0,
    APP_EVENT_KIND_SESSION_LIFECYCLE = 1,
    APP_EVENT_KIND_WAIT_STATE = 2,
    APP_EVENT_KIND_SNAPSHOT_INVALIDATED = 3,
    APP_EVENT_KIND_MESSAGE = 4,
    APP_EVENT_KIND_AUDIO_CUE = 5,
    APP_EVENT_KIND_ANIMATION_HINT = 6,
    APP_EVENT_KIND_RESOURCE_INVALIDATED = 7,
    APP_EVENT_KIND_CUSTOM = 8
} app_event_kind;

typedef enum app_event_scope {
    APP_EVENT_SCOPE_GLOBAL = 0,
    APP_EVENT_SCOPE_SESSION = 1,
    APP_EVENT_SCOPE_SCENE = 2,
    APP_EVENT_SCOPE_VIEW = 3,
    APP_EVENT_SCOPE_ENTITY = 4,
    APP_EVENT_SCOPE_AUDIO = 5
} app_event_scope;

typedef enum app_event_flag {
    APP_EVENT_FLAG_IMPORTANT = 0x0001u,
    APP_EVENT_FLAG_TRANSIENT = 0x0002u,
    APP_EVENT_FLAG_FRONTEND_ONLY = 0x0004u
} app_event_flag;

typedef struct app_event_record {
    u16b kind;
    u16b scope;
    u16b flags;
    u16b payload_size;
    u32b sequence;
    u64b timestamp_usec;
    s32b subject;
    s32b arg0;
    s32b arg1;
    s32b arg2;
} app_event_record;

typedef struct app_event_span {
    const app_event_record* records;
    size_t count;
    u32b dropped_count;
} app_event_span;

typedef struct app_event_buffer app_event_buffer;

#define APP_EVENT_BUFFER_DEFAULT_CAPACITY 64u

/*
 * Spans returned by view/drain point at buffer-owned storage. They remain valid
 * until the buffer is mutated, destroyed, or drained again.
 */
app_event_buffer* app_event_buffer_create(size_t initial_capacity);
void app_event_buffer_destroy(app_event_buffer* buffer);
bool app_event_buffer_reserve(app_event_buffer* buffer, size_t capacity);
void app_event_buffer_clear(app_event_buffer* buffer);
size_t app_event_buffer_count(const app_event_buffer* buffer);
size_t app_event_buffer_capacity(const app_event_buffer* buffer);
u32b app_event_buffer_dropped_count(const app_event_buffer* buffer);
bool app_event_buffer_push(app_event_buffer* buffer,
    const app_event_record* record);
app_event_span app_event_buffer_view(const app_event_buffer* buffer);
app_event_span app_event_buffer_drain(app_event_buffer* buffer);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_EVENTS_H */
