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

#ifndef INCLUDED_APP_HOST_H
#define INCLUDED_APP_HOST_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum app_host_capability {
    APP_HOST_CAPABILITY_NONE = 0,
    APP_HOST_CAPABILITY_MONOTONIC_CLOCK = 0x00000001u,
    APP_HOST_CAPABILITY_WALL_CLOCK = 0x00000002u,
    APP_HOST_CAPABILITY_PERSISTENT_STORAGE = 0x00000004u,
    APP_HOST_CAPABILITY_RESOURCE_LOOKUP = 0x00000008u,
    APP_HOST_CAPABILITY_LOGGING = 0x00000010u
} app_host_capability;

typedef enum app_host_path_kind {
    APP_HOST_PATH_NONE = 0,
    APP_HOST_PATH_CONFIG = 1,
    APP_HOST_PATH_SAVE = 2,
    APP_HOST_PATH_DATA = 3,
    APP_HOST_PATH_CACHE = 4,
    APP_HOST_PATH_META = 5,
    APP_HOST_PATH_RESOURCE = 6
} app_host_path_kind;

typedef enum app_host_log_level {
    APP_HOST_LOG_TRACE = 0,
    APP_HOST_LOG_DEBUG = 1,
    APP_HOST_LOG_INFO = 2,
    APP_HOST_LOG_WARN = 3,
    APP_HOST_LOG_ERROR = 4
} app_host_log_level;

typedef struct app_host_blob {
    const byte* data;
    size_t size;
    u32b version;
    u32b flags;
} app_host_blob;

typedef struct app_host_path_request {
    u16b kind;
    u16b reserved;
    const char* slot;
    char* buffer;
    size_t buffer_size;
} app_host_path_request;

typedef struct app_host_vtable {
    u32b (*query_capabilities)(void* user_data);
    u64b (*monotonic_usec)(void* user_data);
    u64b (*wall_usec)(void* user_data);
    bool (*resolve_path)(void* user_data,
        const app_host_path_request* request);
    bool (*load_blob)(void* user_data, u16b kind, const char* slot,
        app_host_blob* out_blob);
    bool (*store_blob)(void* user_data, u16b kind, const char* slot,
        const app_host_blob* blob);
    void (*release_blob)(void* user_data, app_host_blob* blob);
    void (*log_message)(void* user_data, u16b level, const char* subsystem,
        const char* message);
} app_host_vtable;

typedef struct app_host {
    const app_host_vtable* vtable;
    void* user_data;
} app_host;

u32b app_host_query_capabilities(const app_host* host);
bool app_host_has_capability(const app_host* host, u32b capability_mask);
u64b app_host_monotonic_usec(const app_host* host);
u64b app_host_wall_usec(const app_host* host);
bool app_host_resolve_path(const app_host* host, u16b kind, const char* slot,
    char* buffer, size_t buffer_size);
bool app_host_load_blob(const app_host* host, u16b kind, const char* slot,
    app_host_blob* out_blob);
bool app_host_store_blob(const app_host* host, u16b kind, const char* slot,
    const void* data, size_t size, u32b version, u32b flags);
void app_host_release_blob(const app_host* host, app_host_blob* blob);
void app_host_log(const app_host* host, u16b level, const char* subsystem,
    const char* message);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_HOST_H */
