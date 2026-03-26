#include "angband.h"

#include "app-events.h"

typedef struct app_event_buffer_impl {
    app_event_record* records;
    size_t count;
    size_t capacity;
    u32b dropped_count;
    u32b next_sequence;
} app_event_buffer_impl;

static app_event_buffer_impl* app_event_buffer_cast(app_event_buffer* buffer)
{
    return (app_event_buffer_impl*)buffer;
}

static const app_event_buffer_impl* app_event_buffer_const_cast(
    const app_event_buffer* buffer)
{
    return (const app_event_buffer_impl*)buffer;
}

static size_t app_event_buffer_default_capacity(size_t requested)
{
    return requested ? requested : (size_t)APP_EVENT_BUFFER_DEFAULT_CAPACITY;
}

app_event_buffer* app_event_buffer_create(size_t initial_capacity)
{
    app_event_buffer_impl* buffer;
    size_t capacity = app_event_buffer_default_capacity(initial_capacity);

    buffer = calloc(1, sizeof(*buffer));
    if (!buffer)
        return NULL;

    if (capacity > 0)
    {
        if (capacity > (SIZE_MAX / sizeof(*buffer->records)))
        {
            free(buffer);
            return NULL;
        }
        buffer->records = calloc(capacity, sizeof(*buffer->records));
        if (!buffer->records)
        {
            free(buffer);
            return NULL;
        }
        buffer->capacity = capacity;
    }

    buffer->next_sequence = 1;
    return (app_event_buffer*)buffer;
}

void app_event_buffer_destroy(app_event_buffer* buffer)
{
    app_event_buffer_impl* impl = app_event_buffer_cast(buffer);

    if (!impl)
        return;

    free(impl->records);
    free(impl);
}

bool app_event_buffer_reserve(app_event_buffer* buffer, size_t capacity)
{
    app_event_buffer_impl* impl = app_event_buffer_cast(buffer);
    app_event_record* records;

    if (!impl)
        return false;

    if (capacity <= impl->capacity)
        return true;

    if (capacity > (SIZE_MAX / sizeof(*records)))
        return false;

    records = realloc(impl->records, capacity * sizeof(*records));
    if (!records)
        return false;

    if (capacity > impl->capacity)
    {
        size_t old_capacity = impl->capacity;
        size_t i;

        for (i = old_capacity; i < capacity; i++)
            memset(&records[i], 0, sizeof(records[i]));
    }

    impl->records = records;
    impl->capacity = capacity;
    return true;
}

void app_event_buffer_clear(app_event_buffer* buffer)
{
    app_event_buffer_impl* impl = app_event_buffer_cast(buffer);

    if (!impl)
        return;

    impl->count = 0;
    impl->dropped_count = 0;
}

size_t app_event_buffer_count(const app_event_buffer* buffer)
{
    const app_event_buffer_impl* impl = app_event_buffer_const_cast(buffer);

    return impl ? impl->count : 0;
}

size_t app_event_buffer_capacity(const app_event_buffer* buffer)
{
    const app_event_buffer_impl* impl = app_event_buffer_const_cast(buffer);

    return impl ? impl->capacity : 0;
}

u32b app_event_buffer_dropped_count(const app_event_buffer* buffer)
{
    const app_event_buffer_impl* impl = app_event_buffer_const_cast(buffer);

    return impl ? impl->dropped_count : 0;
}

static bool app_event_buffer_grow_if_needed(app_event_buffer_impl* impl,
    size_t minimum_capacity)
{
    size_t target_capacity;

    if (!impl)
        return false;

    if (impl->capacity >= minimum_capacity)
        return true;

    target_capacity = impl->capacity ? impl->capacity : APP_EVENT_BUFFER_DEFAULT_CAPACITY;
    while (target_capacity < minimum_capacity)
    {
        if (target_capacity > (SIZE_MAX / 2))
        {
            target_capacity = minimum_capacity;
            break;
        }
        target_capacity *= 2;
    }

    return app_event_buffer_reserve((app_event_buffer*)impl, target_capacity);
}

bool app_event_buffer_push(app_event_buffer* buffer,
    const app_event_record* record)
{
    app_event_buffer_impl* impl = app_event_buffer_cast(buffer);
    app_event_record* slot;
    u32b sequence;

    if (!impl || !record)
        return false;

    if (!app_event_buffer_grow_if_needed(impl, impl->count + 1))
    {
        if (impl->dropped_count < UINT32_MAX)
            impl->dropped_count++;
        return false;
    }

    slot = &impl->records[impl->count];
    *slot = *record;

    sequence = record->sequence;
    if (sequence == 0)
        sequence = impl->next_sequence;

    if (sequence >= impl->next_sequence)
    {
        slot->sequence = sequence;
        if (sequence < UINT32_MAX)
            impl->next_sequence = sequence + 1;
        else
            impl->next_sequence = UINT32_MAX;
    }
    else
    {
        slot->sequence = impl->next_sequence;
        if (impl->next_sequence < UINT32_MAX)
            impl->next_sequence++;
    }

    impl->count++;
    return true;
}

app_event_span app_event_buffer_view(const app_event_buffer* buffer)
{
    const app_event_buffer_impl* impl = app_event_buffer_const_cast(buffer);
    app_event_span span;

    span.records = impl ? impl->records : NULL;
    span.count = impl ? impl->count : 0;
    span.dropped_count = impl ? impl->dropped_count : 0;
    return span;
}

app_event_span app_event_buffer_drain(app_event_buffer* buffer)
{
    app_event_buffer_impl* impl = app_event_buffer_cast(buffer);
    app_event_span span;

    span.records = impl ? impl->records : NULL;
    span.count = impl ? impl->count : 0;
    span.dropped_count = impl ? impl->dropped_count : 0;

    if (impl)
    {
        impl->count = 0;
        impl->dropped_count = 0;
    }

    return span;
}
