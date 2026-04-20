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

#ifndef INCLUDED_APP_WIRE_H
#define INCLUDED_APP_WIRE_H

#include "app-session.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_WIRE_VERSION 1u
#define APP_WIRE_ALIGNMENT 8u

typedef enum app_wire_packet_kind {
    APP_WIRE_PACKET_NONE = 0,
    APP_WIRE_PACKET_SNAPSHOT = 1,
    APP_WIRE_PACKET_EVENTS = 2,
    APP_WIRE_PACKET_SESSION = 3,
    APP_WIRE_PACKET_INPUT = 4,
    APP_WIRE_PACKET_INTENT = 5
} app_wire_packet_kind;

/*
 * Wire packets are little-endian and versioned independently from the
 * in-process structs they flatten. Snapshot decode points blob data into the
 * caller-provided packet bytes; that memory must remain alive while the
 * decoded snapshot is in use.
 */
size_t app_wire_snapshot_packet_size(const app_snapshot* snapshot);
bool app_wire_serialize_snapshot_packet(const app_snapshot* snapshot,
    void* buffer, size_t buffer_size, size_t* out_size);
bool app_wire_deserialize_snapshot_packet(const void* buffer,
    size_t buffer_size, app_snapshot* out_snapshot,
    app_snapshot_blob* out_blobs, size_t blob_capacity);

size_t app_wire_event_packet_size(const app_event_span* span);
bool app_wire_serialize_event_packet(const app_event_span* span,
    void* buffer, size_t buffer_size, size_t* out_size);
bool app_wire_deserialize_event_packet(const void* buffer, size_t buffer_size,
    app_event_record* out_records, size_t record_capacity,
    app_event_span* out_span);

size_t app_wire_session_packet_size(void);
bool app_wire_serialize_session_packet(const app_session_export* state,
    void* buffer, size_t buffer_size, size_t* out_size);
bool app_wire_deserialize_session_packet(const void* buffer, size_t buffer_size,
    app_session_export* out_state);

bool app_wire_validate_input(const app_input* input);
size_t app_wire_input_packet_size(void);
bool app_wire_serialize_input_packet(const app_input* input, void* buffer,
    size_t buffer_size, size_t* out_size);
bool app_wire_deserialize_input_packet(const void* buffer, size_t buffer_size,
    app_input* out_input);

bool app_wire_validate_intent(const app_intent* intent);
size_t app_wire_intent_packet_size(void);
bool app_wire_serialize_intent_packet(const app_intent* intent, void* buffer,
    size_t buffer_size, size_t* out_size);
bool app_wire_deserialize_intent_packet(const void* buffer, size_t buffer_size,
    app_intent* out_intent);

size_t app_wire_session_snapshot_packet_size(const app_session* session);
bool app_wire_serialize_session_snapshot_packet(const app_session* session,
    void* buffer, size_t buffer_size, size_t* out_size);
size_t app_wire_session_event_packet_size(const app_session* session);
bool app_wire_serialize_session_event_packet(app_session* session, bool drain,
    void* buffer, size_t buffer_size, size_t* out_size);
size_t app_wire_session_state_packet_size(const app_session* session);
bool app_wire_serialize_session_state_packet(const app_session* session,
    void* buffer, size_t buffer_size, size_t* out_size);
bool app_wire_submit_input_packet(app_session* session, const void* buffer,
    size_t buffer_size);
bool app_wire_submit_intent_packet(app_session* session, const void* buffer,
    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_WIRE_H */
