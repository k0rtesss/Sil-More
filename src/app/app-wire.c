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

#include "app-wire.h"

#define APP_WIRE_PACKET_MAGIC 0x31574953u /* "SIW1" */

#define APP_WIRE_COMMON_HEADER_SIZE 24u
#define APP_WIRE_SNAPSHOT_META_SIZE 16u
#define APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE 12u
#define APP_WIRE_EVENT_META_SIZE 8u
#define APP_WIRE_EVENT_RECORD_SIZE 36u
#define APP_WIRE_INPUT_PAYLOAD_SIZE 20u
#define APP_WIRE_INPUT_BODY_SIZE 52u
#define APP_WIRE_INTENT_PAYLOAD_SIZE 28u
#define APP_WIRE_INTENT_BODY_SIZE 52u
#define APP_WIRE_SESSION_BODY_SIZE 108u

_Static_assert(sizeof(app_input_pointer_event) <= APP_WIRE_INPUT_PAYLOAD_SIZE,
    "Update UI8 wire input payload size.");
_Static_assert(sizeof(app_input_text_event) <= APP_WIRE_INPUT_PAYLOAD_SIZE,
    "Update UI8 wire input payload size.");
_Static_assert(sizeof(app_input_gamepad_event) <= APP_WIRE_INPUT_PAYLOAD_SIZE,
    "Update UI8 wire input payload size.");
_Static_assert(sizeof(app_input_system_event) <= APP_WIRE_INPUT_PAYLOAD_SIZE,
    "Update UI8 wire input payload size.");
_Static_assert(sizeof(app_intent_command) <= APP_WIRE_INTENT_PAYLOAD_SIZE,
    "Update UI8 wire intent payload size.");
_Static_assert(sizeof(app_input_text_event) <= APP_WIRE_INTENT_PAYLOAD_SIZE,
    "Update UI8 wire intent payload size.");
_Static_assert(sizeof(app_input_system_event) <= APP_WIRE_INTENT_PAYLOAD_SIZE,
    "Update UI8 wire intent payload size.");
_Static_assert(sizeof(app_intent_vector) <= APP_WIRE_INTENT_PAYLOAD_SIZE,
    "Update UI8 wire intent payload size.");
_Static_assert(sizeof(app_intent_slot) <= APP_WIRE_INTENT_PAYLOAD_SIZE,
    "Update UI8 wire intent payload size.");

static void app_wire_write_u16(byte* dst, u16b value)
{
    dst[0] = (byte)(value & 0xFFu);
    dst[1] = (byte)((value >> 8) & 0xFFu);
}

static void app_wire_write_u32(byte* dst, u32b value)
{
    dst[0] = (byte)(value & 0xFFu);
    dst[1] = (byte)((value >> 8) & 0xFFu);
    dst[2] = (byte)((value >> 16) & 0xFFu);
    dst[3] = (byte)((value >> 24) & 0xFFu);
}

static void app_wire_write_u64(byte* dst, u64b value)
{
    dst[0] = (byte)(value & 0xFFu);
    dst[1] = (byte)((value >> 8) & 0xFFu);
    dst[2] = (byte)((value >> 16) & 0xFFu);
    dst[3] = (byte)((value >> 24) & 0xFFu);
    dst[4] = (byte)((value >> 32) & 0xFFu);
    dst[5] = (byte)((value >> 40) & 0xFFu);
    dst[6] = (byte)((value >> 48) & 0xFFu);
    dst[7] = (byte)((value >> 56) & 0xFFu);
}

static u16b app_wire_read_u16(const byte* src)
{
    return (u16b)((u16b)src[0] | ((u16b)src[1] << 8));
}

static u32b app_wire_read_u32(const byte* src)
{
    return (u32b)((u32b)src[0]
        | ((u32b)src[1] << 8)
        | ((u32b)src[2] << 16)
        | ((u32b)src[3] << 24));
}

static u64b app_wire_read_u64(const byte* src)
{
    return (u64b)((u64b)src[0]
        | ((u64b)src[1] << 8)
        | ((u64b)src[2] << 16)
        | ((u64b)src[3] << 24)
        | ((u64b)src[4] << 32)
        | ((u64b)src[5] << 40)
        | ((u64b)src[6] << 48)
        | ((u64b)src[7] << 56));
}

static s16b app_wire_read_s16(const byte* src)
{
    return (s16b)app_wire_read_u16(src);
}

static s32b app_wire_read_s32(const byte* src)
{
    return (s32b)app_wire_read_u32(src);
}

static void app_wire_write_s16(byte* dst, s16b value)
{
    app_wire_write_u16(dst, (u16b)value);
}

static void app_wire_write_s32(byte* dst, s32b value)
{
    app_wire_write_u32(dst, (u32b)value);
}

static bool app_wire_size_add(size_t* total, size_t add)
{
    if (!total || (((size_t)-1) - *total) < add)
        return false;

    *total += add;
    return true;
}

static bool app_wire_align_size(size_t* total)
{
    size_t remainder;

    if (!total)
        return false;

    remainder = *total % APP_WIRE_ALIGNMENT;
    if (!remainder)
        return true;

    return app_wire_size_add(total, APP_WIRE_ALIGNMENT - remainder);
}

static bool app_wire_size_to_u32(size_t value, u32b* out_value)
{
    if (!out_value || value > 0xFFFFFFFFu)
        return false;

    *out_value = (u32b)value;
    return true;
}

static bool app_wire_u32_to_size(u32b value, size_t* out_value)
{
    if (!out_value)
        return false;

    *out_value = (size_t)value;
    return true;
}

static void app_wire_write_common_header(byte* dst, u16b kind,
    u32b packet_size, u32b header_size, u32b item_count)
{
    app_wire_write_u32(dst + 0, APP_WIRE_PACKET_MAGIC);
    app_wire_write_u16(dst + 4, APP_WIRE_VERSION);
    app_wire_write_u16(dst + 6, kind);
    app_wire_write_u32(dst + 8, packet_size);
    app_wire_write_u32(dst + 12, header_size);
    app_wire_write_u32(dst + 16, item_count);
    app_wire_write_u32(dst + 20, 0);
}

static bool app_wire_read_common_header(const byte* src, size_t buffer_size,
    u16b expected_kind, u32b* out_packet_size, u32b* out_header_size,
    u32b* out_item_count)
{
    u32b packet_size;
    u32b header_size;
    u16b version;
    u16b kind;

    if (!src || buffer_size < APP_WIRE_COMMON_HEADER_SIZE)
        return false;
    if (app_wire_read_u32(src + 0) != APP_WIRE_PACKET_MAGIC)
        return false;

    version = app_wire_read_u16(src + 4);
    kind = app_wire_read_u16(src + 6);
    packet_size = app_wire_read_u32(src + 8);
    header_size = app_wire_read_u32(src + 12);

    if (version != APP_WIRE_VERSION || kind != expected_kind)
        return false;
    if (packet_size > buffer_size || packet_size < header_size)
        return false;
    if (header_size < APP_WIRE_COMMON_HEADER_SIZE)
        return false;

    if (out_packet_size)
        *out_packet_size = packet_size;
    if (out_header_size)
        *out_header_size = header_size;
    if (out_item_count)
        *out_item_count = app_wire_read_u32(src + 16);

    return true;
}

static size_t app_wire_snapshot_metadata_size(size_t blob_count)
{
    size_t total = APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_SNAPSHOT_META_SIZE;

    if (blob_count > (((size_t)-1) / APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE))
        return 0;

    if (!app_wire_size_add(&total,
            blob_count * APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE))
    {
        return 0;
    }
    if (!app_wire_align_size(&total))
        return 0;

    return total;
}

static void app_wire_write_snapshot_blob_desc(byte* dst, u16b kind,
    u16b format_version, u32b offset, u32b size)
{
    app_wire_write_u16(dst + 0, kind);
    app_wire_write_u16(dst + 2, format_version);
    app_wire_write_u32(dst + 4, offset);
    app_wire_write_u32(dst + 8, size);
}

static void app_wire_write_event_record(byte* dst,
    const app_event_record* record)
{
    app_wire_write_u16(dst + 0, record->kind);
    app_wire_write_u16(dst + 2, record->scope);
    app_wire_write_u16(dst + 4, record->flags);
    app_wire_write_u16(dst + 6, record->payload_size);
    app_wire_write_u32(dst + 8, record->sequence);
    app_wire_write_u64(dst + 12, record->timestamp_usec);
    app_wire_write_s32(dst + 20, record->subject);
    app_wire_write_s32(dst + 24, record->arg0);
    app_wire_write_s32(dst + 28, record->arg1);
    app_wire_write_s32(dst + 32, record->arg2);
}

static void app_wire_read_event_record(const byte* src,
    app_event_record* out_record)
{
    memset(out_record, 0, sizeof(*out_record));
    out_record->kind = app_wire_read_u16(src + 0);
    out_record->scope = app_wire_read_u16(src + 2);
    out_record->flags = app_wire_read_u16(src + 4);
    out_record->payload_size = app_wire_read_u16(src + 6);
    out_record->sequence = app_wire_read_u32(src + 8);
    out_record->timestamp_usec = app_wire_read_u64(src + 12);
    out_record->subject = app_wire_read_s32(src + 20);
    out_record->arg0 = app_wire_read_s32(src + 24);
    out_record->arg1 = app_wire_read_s32(src + 28);
    out_record->arg2 = app_wire_read_s32(src + 32);
}

size_t app_wire_snapshot_packet_size(const app_snapshot* snapshot)
{
    size_t blob_count;
    size_t total;
    size_t i;

    if (!snapshot)
        return 0;
    if (snapshot->blob_count && !snapshot->blobs)
        return 0;

    blob_count = snapshot->blob_count;
    total = app_wire_snapshot_metadata_size(blob_count);
    if (!total)
        return 0;

    for (i = 0; i < blob_count; i++)
    {
        const app_snapshot_blob* blob = &snapshot->blobs[i];

        if (!blob || (blob->size && !blob->data))
            return 0;
        if (!app_wire_size_add(&total, blob->size))
            return 0;
        if (i + 1 < blob_count && !app_wire_align_size(&total))
            return 0;
    }

    return total;
}

bool app_wire_serialize_snapshot_packet(const app_snapshot* snapshot,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    byte* bytes = buffer;
    size_t packet_size;
    size_t header_size;
    size_t payload_offset;
    size_t i;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b blob_count_u32;

    if (!snapshot || !buffer)
        return false;

    packet_size = app_wire_snapshot_packet_size(snapshot);
    if (!packet_size || packet_size > buffer_size)
        return false;

    header_size = app_wire_snapshot_metadata_size(snapshot->blob_count);
    if (!header_size)
        return false;
    if (!app_wire_size_to_u32(packet_size, &packet_size_u32)
        || !app_wire_size_to_u32(header_size, &header_size_u32)
        || !app_wire_size_to_u32(snapshot->blob_count, &blob_count_u32))
    {
        return false;
    }

    memset(bytes, 0, packet_size);
    app_wire_write_common_header(bytes, APP_WIRE_PACKET_SNAPSHOT,
        packet_size_u32, header_size_u32, blob_count_u32);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 0,
        snapshot->revision);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 8,
        snapshot->scene);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 10,
        snapshot->flags);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 12, 0);

    payload_offset = header_size;
    for (i = 0; i < snapshot->blob_count; i++)
    {
        const app_snapshot_blob* blob = &snapshot->blobs[i];
        byte* desc = bytes + APP_WIRE_COMMON_HEADER_SIZE
            + APP_WIRE_SNAPSHOT_META_SIZE
            + (i * APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE);
        u32b offset_u32;
        u32b size_u32;

        if (!app_wire_size_to_u32(payload_offset, &offset_u32)
            || !app_wire_size_to_u32(blob->size, &size_u32))
        {
            return false;
        }

        app_wire_write_snapshot_blob_desc(desc, blob->kind,
            blob->format_version, offset_u32, size_u32);
        if (blob->size)
            memcpy(bytes + payload_offset, blob->data, blob->size);
        payload_offset += blob->size;
        if (i + 1 < snapshot->blob_count)
        {
            size_t aligned = payload_offset;

            if (!app_wire_align_size(&aligned))
                return false;
            payload_offset = aligned;
        }
    }

    if (out_size)
        *out_size = packet_size;
    return true;
}

bool app_wire_deserialize_snapshot_packet(const void* buffer,
    size_t buffer_size, app_snapshot* out_snapshot,
    app_snapshot_blob* out_blobs, size_t blob_capacity)
{
    const byte* bytes = buffer;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b blob_count_u32;
    size_t blob_count;
    size_t i;

    if (!buffer || !out_snapshot)
        return false;
    if (!app_wire_read_common_header(bytes, buffer_size,
            APP_WIRE_PACKET_SNAPSHOT, &packet_size_u32, &header_size_u32,
            &blob_count_u32))
    {
        return false;
    }
    if (!app_wire_u32_to_size(blob_count_u32, &blob_count))
        return false;
    if (blob_count > blob_capacity)
        return false;
    if (header_size_u32 < (APP_WIRE_COMMON_HEADER_SIZE
            + APP_WIRE_SNAPSHOT_META_SIZE
            + (blob_count_u32 * APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE)))
    {
        return false;
    }
    if (blob_count && !out_blobs)
        return false;

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->revision = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 0);
    out_snapshot->scene = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 8);
    out_snapshot->flags = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 10);
    out_snapshot->blobs = out_blobs;
    out_snapshot->blob_count = blob_count;

    for (i = 0; i < blob_count; i++)
    {
        const byte* desc = bytes + APP_WIRE_COMMON_HEADER_SIZE
            + APP_WIRE_SNAPSHOT_META_SIZE
            + (i * APP_WIRE_SNAPSHOT_BLOB_DESC_SIZE);
        u32b offset_u32 = app_wire_read_u32(desc + 4);
        u32b size_u32 = app_wire_read_u32(desc + 8);
        size_t offset;
        size_t size;

        if (!app_wire_u32_to_size(offset_u32, &offset)
            || !app_wire_u32_to_size(size_u32, &size))
        {
            return false;
        }
        if (offset < header_size_u32 || offset > packet_size_u32)
            return false;
        if ((packet_size_u32 - offset_u32) < size_u32)
            return false;
        if ((offset % APP_WIRE_ALIGNMENT) != 0 && size_u32 != 0)
        {
            return false;
        }

        out_blobs[i].kind = app_wire_read_u16(desc + 0);
        out_blobs[i].format_version = app_wire_read_u16(desc + 2);
        out_blobs[i].data = size ? (bytes + offset) : NULL;
        out_blobs[i].size = size;
    }

    return true;
}

size_t app_wire_event_packet_size(const app_event_span* span)
{
    size_t total = APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_EVENT_META_SIZE;

    if (!span)
        return 0;
    if (span->count && !span->records)
        return 0;
    if (span->count > (((size_t)-1) / APP_WIRE_EVENT_RECORD_SIZE))
        return 0;
    if (!app_wire_size_add(&total, span->count * APP_WIRE_EVENT_RECORD_SIZE))
        return 0;

    return total;
}

bool app_wire_serialize_event_packet(const app_event_span* span,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    byte* bytes = buffer;
    size_t packet_size;
    size_t i;
    u32b packet_size_u32;
    u32b count_u32;

    if (!span || !buffer)
        return false;

    packet_size = app_wire_event_packet_size(span);
    if (!packet_size || packet_size > buffer_size)
        return false;
    if (!app_wire_size_to_u32(packet_size, &packet_size_u32)
        || !app_wire_size_to_u32(span->count, &count_u32))
    {
        return false;
    }

    memset(bytes, 0, packet_size);
    app_wire_write_common_header(bytes, APP_WIRE_PACKET_EVENTS,
        packet_size_u32, APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_EVENT_META_SIZE,
        count_u32);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 0,
        span->dropped_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 4, 0);

    for (i = 0; i < span->count; i++)
    {
        byte* dst = bytes + APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_EVENT_META_SIZE
            + (i * APP_WIRE_EVENT_RECORD_SIZE);
        app_wire_write_event_record(dst, &span->records[i]);
    }

    if (out_size)
        *out_size = packet_size;
    return true;
}

bool app_wire_deserialize_event_packet(const void* buffer, size_t buffer_size,
    app_event_record* out_records, size_t record_capacity,
    app_event_span* out_span)
{
    const byte* bytes = buffer;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b count_u32;
    size_t count;
    size_t i;

    if (!buffer || !out_span)
        return false;
    if (!app_wire_read_common_header(bytes, buffer_size,
            APP_WIRE_PACKET_EVENTS, &packet_size_u32, &header_size_u32,
            &count_u32))
    {
        return false;
    }
    if (header_size_u32 != (APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_EVENT_META_SIZE))
        return false;
    if (!app_wire_u32_to_size(count_u32, &count))
        return false;
    if (count > record_capacity)
        return false;
    if (count && !out_records)
        return false;
    if (packet_size_u32 != header_size_u32 + (count_u32 * APP_WIRE_EVENT_RECORD_SIZE))
        return false;

    for (i = 0; i < count; i++)
    {
        const byte* src = bytes + header_size_u32
            + (i * APP_WIRE_EVENT_RECORD_SIZE);
        app_wire_read_event_record(src, &out_records[i]);
    }

    out_span->records = out_records;
    out_span->count = count;
    out_span->dropped_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 0);
    return true;
}

size_t app_wire_session_packet_size(void)
{
    return APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_SESSION_BODY_SIZE;
}

bool app_wire_serialize_session_packet(const app_session_export* state,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    byte* bytes = buffer;
    size_t packet_size = app_wire_session_packet_size();

    if (!state || !buffer || packet_size > buffer_size)
        return false;

    memset(bytes, 0, packet_size);
    app_wire_write_common_header(bytes, APP_WIRE_PACKET_SESSION,
        (u32b)packet_size, (u32b)packet_size, 1u);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 0,
        state->api_version);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 4,
        state->flags);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 8,
        state->state);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 10,
        state->wait_reason);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 12,
        state->wait_flags);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 14,
        state->snapshot_scene);
    app_wire_write_s32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 16,
        state->wait_detail0);
    app_wire_write_s32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 20,
        state->wait_detail1);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 24,
        state->snapshot_flags);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 26,
        state->reserved0);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 28,
        state->snapshot_blob_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 32,
        state->pending_input_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 36,
        state->pending_intent_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 40,
        state->pending_event_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 44,
        state->pending_event_dropped_count);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 48,
        state->reserved1);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 52,
        state->snapshot_revision);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 60,
        state->counters.submitted_inputs);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 68,
        state->counters.submitted_intents);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 76,
        state->counters.consumed_inputs);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 84,
        state->counters.consumed_intents);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 92,
        state->counters.emitted_events);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 100,
        state->counters.dropped_events);

    if (out_size)
        *out_size = packet_size;
    return true;
}

bool app_wire_deserialize_session_packet(const void* buffer, size_t buffer_size,
    app_session_export* out_state)
{
    const byte* bytes = buffer;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b item_count_u32;

    if (!buffer || !out_state)
        return false;
    if (!app_wire_read_common_header(bytes, buffer_size,
            APP_WIRE_PACKET_SESSION, &packet_size_u32, &header_size_u32,
            &item_count_u32))
    {
        return false;
    }
    if (header_size_u32 != packet_size_u32
        || packet_size_u32 != app_wire_session_packet_size()
        || item_count_u32 != 1u)
    {
        return false;
    }

    memset(out_state, 0, sizeof(*out_state));
    out_state->api_version = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 0);
    out_state->flags = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 4);
    out_state->state = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 8);
    out_state->wait_reason = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 10);
    out_state->wait_flags = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 12);
    out_state->snapshot_scene = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 14);
    out_state->wait_detail0 = app_wire_read_s32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 16);
    out_state->wait_detail1 = app_wire_read_s32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 20);
    out_state->snapshot_flags = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 24);
    out_state->reserved0 = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 26);
    out_state->snapshot_blob_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 28);
    out_state->pending_input_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 32);
    out_state->pending_intent_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 36);
    out_state->pending_event_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 40);
    out_state->pending_event_dropped_count = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 44);
    out_state->reserved1 = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 48);
    out_state->snapshot_revision = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 52);
    out_state->counters.submitted_inputs = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 60);
    out_state->counters.submitted_intents = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 68);
    out_state->counters.consumed_inputs = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 76);
    out_state->counters.consumed_intents = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 84);
    out_state->counters.emitted_events = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 92);
    out_state->counters.dropped_events = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 100);
    return true;
}

bool app_wire_validate_input(const app_input* input)
{
    if (!input)
        return false;
    if (input->layer > APP_INPUT_LAYER_INTENT)
        return false;
    if (input->type == APP_INPUT_TYPE_NONE
        || input->type > APP_INPUT_TYPE_SYSTEM)
    {
        return false;
    }
    if (input->device > APP_INPUT_DEVICE_SYSTEM)
        return false;
    if (input->modifiers & ~(APP_INPUT_MODIFIER_SHIFT
            | APP_INPUT_MODIFIER_CTRL
            | APP_INPUT_MODIFIER_ALT
            | APP_INPUT_MODIFIER_META
            | APP_INPUT_MODIFIER_CAPS_LOCK
            | APP_INPUT_MODIFIER_NUM_LOCK))
    {
        return false;
    }
    if (input->flags & ~(APP_INPUT_FLAG_PRESS
            | APP_INPUT_FLAG_RELEASE
            | APP_INPUT_FLAG_REPEAT
            | APP_INPUT_FLAG_SYNTHETIC
            | APP_INPUT_FLAG_LONG_PRESS))
    {
        return false;
    }

    return true;
}

size_t app_wire_input_packet_size(void)
{
    return APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_INPUT_BODY_SIZE;
}

bool app_wire_serialize_input_packet(const app_input* input, void* buffer,
    size_t buffer_size, size_t* out_size)
{
    byte* bytes = buffer;
    byte* payload;
    size_t packet_size = app_wire_input_packet_size();

    if (!app_wire_validate_input(input) || !buffer || packet_size > buffer_size)
        return false;

    memset(bytes, 0, packet_size);
    app_wire_write_common_header(bytes, APP_WIRE_PACKET_INPUT,
        (u32b)packet_size, (u32b)packet_size, 1u);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 0, input->layer);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 2, input->type);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 4, input->device);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 6,
        input->modifiers);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 8, input->flags);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 10,
        input->source_id);
    app_wire_write_u32(bytes + APP_WIRE_COMMON_HEADER_SIZE + 12,
        input->reserved);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 16,
        input->sequence);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 24,
        input->timestamp_usec);

    payload = bytes + APP_WIRE_COMMON_HEADER_SIZE + 32;
    switch (input->type)
    {
    case APP_INPUT_TYPE_KEY:
        app_wire_write_u32(payload + 0, input->payload.key.logical_key);
        app_wire_write_u32(payload + 4, input->payload.key.physical_key);
        app_wire_write_u16(payload + 8, input->payload.key.repeat_count);
        app_wire_write_u16(payload + 10, input->payload.key.reserved);
        break;
    case APP_INPUT_TYPE_TEXT:
        app_wire_write_u32(payload + 0, input->payload.text.codepoint);
        memcpy(payload + 4, input->payload.text.utf8,
            sizeof(input->payload.text.utf8));
        break;
    case APP_INPUT_TYPE_POINTER_MOTION:
    case APP_INPUT_TYPE_POINTER_BUTTON:
        app_wire_write_s32(payload + 0, input->payload.pointer.x);
        app_wire_write_s32(payload + 4, input->payload.pointer.y);
        app_wire_write_s32(payload + 8, input->payload.pointer.dx);
        app_wire_write_s32(payload + 12, input->payload.pointer.dy);
        app_wire_write_u16(payload + 16, input->payload.pointer.button);
        app_wire_write_u16(payload + 18, input->payload.pointer.clicks);
        break;
    case APP_INPUT_TYPE_POINTER_WHEEL:
        app_wire_write_s32(payload + 0, input->payload.wheel.x);
        app_wire_write_s32(payload + 4, input->payload.wheel.y);
        break;
    case APP_INPUT_TYPE_GAMEPAD_BUTTON:
    case APP_INPUT_TYPE_GAMEPAD_AXIS:
        app_wire_write_u16(payload + 0, input->payload.gamepad.control);
        app_wire_write_s16(payload + 2, input->payload.gamepad.value);
        app_wire_write_u16(payload + 4,
            input->payload.gamepad.secondary_control);
        app_wire_write_u16(payload + 6, input->payload.gamepad.reserved);
        break;
    case APP_INPUT_TYPE_SYSTEM:
        app_wire_write_u16(payload + 0, input->payload.system.code);
        app_wire_write_u16(payload + 2, input->payload.system.value);
        app_wire_write_u32(payload + 4, input->payload.system.data);
        break;
    default:
        return false;
    }

    if (out_size)
        *out_size = packet_size;
    return true;
}

bool app_wire_deserialize_input_packet(const void* buffer, size_t buffer_size,
    app_input* out_input)
{
    const byte* bytes = buffer;
    const byte* payload;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b item_count_u32;

    if (!buffer || !out_input)
        return false;
    if (!app_wire_read_common_header(bytes, buffer_size, APP_WIRE_PACKET_INPUT,
            &packet_size_u32, &header_size_u32, &item_count_u32))
    {
        return false;
    }
    if (header_size_u32 != packet_size_u32
        || packet_size_u32 != app_wire_input_packet_size()
        || item_count_u32 != 1u)
    {
        return false;
    }

    memset(out_input, 0, sizeof(*out_input));
    out_input->layer = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 0);
    out_input->type = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 2);
    out_input->device = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 4);
    out_input->modifiers = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 6);
    out_input->flags = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 8);
    out_input->source_id = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 10);
    out_input->reserved = app_wire_read_u32(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 12);
    out_input->sequence = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 16);
    out_input->timestamp_usec = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 24);

    payload = bytes + APP_WIRE_COMMON_HEADER_SIZE + 32;
    switch (out_input->type)
    {
    case APP_INPUT_TYPE_KEY:
        out_input->payload.key.logical_key = app_wire_read_u32(payload + 0);
        out_input->payload.key.physical_key = app_wire_read_u32(payload + 4);
        out_input->payload.key.repeat_count = app_wire_read_u16(payload + 8);
        out_input->payload.key.reserved = app_wire_read_u16(payload + 10);
        break;
    case APP_INPUT_TYPE_TEXT:
        out_input->payload.text.codepoint = app_wire_read_u32(payload + 0);
        memcpy(out_input->payload.text.utf8, payload + 4,
            sizeof(out_input->payload.text.utf8));
        break;
    case APP_INPUT_TYPE_POINTER_MOTION:
    case APP_INPUT_TYPE_POINTER_BUTTON:
        out_input->payload.pointer.x = app_wire_read_s32(payload + 0);
        out_input->payload.pointer.y = app_wire_read_s32(payload + 4);
        out_input->payload.pointer.dx = app_wire_read_s32(payload + 8);
        out_input->payload.pointer.dy = app_wire_read_s32(payload + 12);
        out_input->payload.pointer.button = app_wire_read_u16(payload + 16);
        out_input->payload.pointer.clicks = app_wire_read_u16(payload + 18);
        break;
    case APP_INPUT_TYPE_POINTER_WHEEL:
        out_input->payload.wheel.x = app_wire_read_s32(payload + 0);
        out_input->payload.wheel.y = app_wire_read_s32(payload + 4);
        break;
    case APP_INPUT_TYPE_GAMEPAD_BUTTON:
    case APP_INPUT_TYPE_GAMEPAD_AXIS:
        out_input->payload.gamepad.control = app_wire_read_u16(payload + 0);
        out_input->payload.gamepad.value = app_wire_read_s16(payload + 2);
        out_input->payload.gamepad.secondary_control
            = app_wire_read_u16(payload + 4);
        out_input->payload.gamepad.reserved = app_wire_read_u16(payload + 6);
        break;
    case APP_INPUT_TYPE_SYSTEM:
        out_input->payload.system.code = app_wire_read_u16(payload + 0);
        out_input->payload.system.value = app_wire_read_u16(payload + 2);
        out_input->payload.system.data = app_wire_read_u32(payload + 4);
        break;
    default:
        return false;
    }

    return app_wire_validate_input(out_input);
}

bool app_wire_validate_intent(const app_intent* intent)
{
    if (!intent)
        return false;
    if (intent->kind == APP_INTENT_KIND_NONE
        || intent->kind > APP_INTENT_KIND_SYSTEM)
    {
        return false;
    }
    if (intent->flags & ~(APP_INTENT_FLAG_ANALOG
            | APP_INTENT_FLAG_REPEAT
            | APP_INTENT_FLAG_LONG_PRESS))
    {
        return false;
    }

    return true;
}

size_t app_wire_intent_packet_size(void)
{
    return APP_WIRE_COMMON_HEADER_SIZE + APP_WIRE_INTENT_BODY_SIZE;
}

bool app_wire_serialize_intent_packet(const app_intent* intent, void* buffer,
    size_t buffer_size, size_t* out_size)
{
    byte* bytes = buffer;
    byte* payload;
    size_t packet_size = app_wire_intent_packet_size();

    if (!app_wire_validate_intent(intent) || !buffer
        || packet_size > buffer_size)
    {
        return false;
    }

    memset(bytes, 0, packet_size);
    app_wire_write_common_header(bytes, APP_WIRE_PACKET_INTENT,
        (u32b)packet_size, (u32b)packet_size, 1u);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 0, intent->kind);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 2, intent->flags);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 4,
        intent->repeat_count);
    app_wire_write_u16(bytes + APP_WIRE_COMMON_HEADER_SIZE + 6,
        intent->reserved);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 8,
        intent->sequence);
    app_wire_write_u64(bytes + APP_WIRE_COMMON_HEADER_SIZE + 16,
        intent->timestamp_usec);

    payload = bytes + APP_WIRE_COMMON_HEADER_SIZE + 24;
    switch (intent->kind)
    {
    case APP_INTENT_KIND_NAVIGATE:
    case APP_INTENT_KIND_TARGET_DELTA:
        app_wire_write_s16(payload + 0, intent->payload.vector.dy);
        app_wire_write_s16(payload + 2, intent->payload.vector.dx);
        break;
    case APP_INTENT_KIND_ACTIVATE_SLOT:
        app_wire_write_s16(payload + 0, intent->payload.slot.group);
        app_wire_write_s16(payload + 2, intent->payload.slot.index);
        break;
    case APP_INTENT_KIND_COMMAND:
        app_wire_write_u32(payload + 0, intent->payload.command.command_id);
        memcpy(payload + 4, intent->payload.command.token,
            sizeof(intent->payload.command.token));
        break;
    case APP_INTENT_KIND_TEXT:
        app_wire_write_u32(payload + 0, intent->payload.text.codepoint);
        memcpy(payload + 4, intent->payload.text.utf8,
            sizeof(intent->payload.text.utf8));
        break;
    case APP_INTENT_KIND_SYSTEM:
        app_wire_write_u16(payload + 0, intent->payload.system.code);
        app_wire_write_u16(payload + 2, intent->payload.system.value);
        app_wire_write_u32(payload + 4, intent->payload.system.data);
        break;
    default:
        break;
    }

    if (out_size)
        *out_size = packet_size;
    return true;
}

bool app_wire_deserialize_intent_packet(const void* buffer, size_t buffer_size,
    app_intent* out_intent)
{
    const byte* bytes = buffer;
    const byte* payload;
    u32b packet_size_u32;
    u32b header_size_u32;
    u32b item_count_u32;

    if (!buffer || !out_intent)
        return false;
    if (!app_wire_read_common_header(bytes, buffer_size,
            APP_WIRE_PACKET_INTENT, &packet_size_u32, &header_size_u32,
            &item_count_u32))
    {
        return false;
    }
    if (header_size_u32 != packet_size_u32
        || packet_size_u32 != app_wire_intent_packet_size()
        || item_count_u32 != 1u)
    {
        return false;
    }

    memset(out_intent, 0, sizeof(*out_intent));
    out_intent->kind = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 0);
    out_intent->flags = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 2);
    out_intent->repeat_count = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 4);
    out_intent->reserved = app_wire_read_u16(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 6);
    out_intent->sequence = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 8);
    out_intent->timestamp_usec = app_wire_read_u64(
        bytes + APP_WIRE_COMMON_HEADER_SIZE + 16);

    payload = bytes + APP_WIRE_COMMON_HEADER_SIZE + 24;
    switch (out_intent->kind)
    {
    case APP_INTENT_KIND_NAVIGATE:
    case APP_INTENT_KIND_TARGET_DELTA:
        out_intent->payload.vector.dy = app_wire_read_s16(payload + 0);
        out_intent->payload.vector.dx = app_wire_read_s16(payload + 2);
        break;
    case APP_INTENT_KIND_ACTIVATE_SLOT:
        out_intent->payload.slot.group = app_wire_read_s16(payload + 0);
        out_intent->payload.slot.index = app_wire_read_s16(payload + 2);
        break;
    case APP_INTENT_KIND_COMMAND:
        out_intent->payload.command.command_id = app_wire_read_u32(payload + 0);
        memcpy(out_intent->payload.command.token, payload + 4,
            sizeof(out_intent->payload.command.token));
        break;
    case APP_INTENT_KIND_TEXT:
        out_intent->payload.text.codepoint = app_wire_read_u32(payload + 0);
        memcpy(out_intent->payload.text.utf8, payload + 4,
            sizeof(out_intent->payload.text.utf8));
        break;
    case APP_INTENT_KIND_SYSTEM:
        out_intent->payload.system.code = app_wire_read_u16(payload + 0);
        out_intent->payload.system.value = app_wire_read_u16(payload + 2);
        out_intent->payload.system.data = app_wire_read_u32(payload + 4);
        break;
    default:
        break;
    }

    return app_wire_validate_intent(out_intent);
}

size_t app_wire_session_snapshot_packet_size(const app_session* session)
{
    return session ? app_wire_snapshot_packet_size(app_session_snapshot(session))
        : 0u;
}

bool app_wire_serialize_session_snapshot_packet(const app_session* session,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    if (!session)
        return false;

    return app_wire_serialize_snapshot_packet(app_session_snapshot(session),
        buffer, buffer_size, out_size);
}

size_t app_wire_session_event_packet_size(const app_session* session)
{
    app_event_span span;

    if (!session)
        return 0u;

    span = app_session_view_events(session);
    return app_wire_event_packet_size(&span);
}

bool app_wire_serialize_session_event_packet(app_session* session, bool drain,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    app_event_span span;
    bool ok;

    if (!session)
        return false;

    span = app_session_view_events(session);
    ok = app_wire_serialize_event_packet(&span, buffer, buffer_size, out_size);
    if (ok && drain)
        app_session_clear_events(session);

    return ok;
}

size_t app_wire_session_state_packet_size(const app_session* session)
{
    (void)session;
    return app_wire_session_packet_size();
}

bool app_wire_serialize_session_state_packet(const app_session* session,
    void* buffer, size_t buffer_size, size_t* out_size)
{
    app_session_export state;

    app_session_export_state(session, &state);
    return app_wire_serialize_session_packet(&state, buffer, buffer_size,
        out_size);
}

bool app_wire_submit_input_packet(app_session* session, const void* buffer,
    size_t buffer_size)
{
    app_input input;

    if (!app_wire_deserialize_input_packet(buffer, buffer_size, &input))
        return false;

    return app_session_submit_input(session, &input);
}

bool app_wire_submit_intent_packet(app_session* session, const void* buffer,
    size_t buffer_size)
{
    app_intent intent;

    if (!app_wire_deserialize_intent_packet(buffer, buffer_size, &intent))
        return false;

    return app_session_submit_intent(session, &intent);
}
