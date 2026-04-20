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

#ifndef INCLUDED_APP_HOST_BRIDGE_H
#define INCLUDED_APP_HOST_BRIDGE_H

#include "app-host.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HOST_BRIDGE_ENTRY_MAX 64u
#define APP_HOST_BRIDGE_SLOT_MAX 64u
#define APP_HOST_BRIDGE_PATH_MAX 260u
#define APP_HOST_BRIDGE_LOG_TEXT_MAX 192u
#define APP_HOST_BRIDGE_LOG_SUBSYSTEM_MAX 32u

typedef void (*app_host_bridge_log_fn)(void* user_data, u16b level,
    const char* subsystem, const char* message);

typedef struct app_host_bridge_entry {
    bool occupied;
    u16b kind;
    char slot[APP_HOST_BRIDGE_SLOT_MAX];
    byte* data;
    size_t size;
    u32b version;
    u32b flags;
} app_host_bridge_entry;

typedef struct app_host_bridge {
    app_host host;
    u32b capabilities;
    u64b monotonic_usec;
    u64b wall_usec;
    app_host_bridge_log_fn log_fn;
    void* log_user_data;
    u32b log_count;
    u16b last_log_level;
    char last_log_subsystem[APP_HOST_BRIDGE_LOG_SUBSYSTEM_MAX];
    char last_log_message[APP_HOST_BRIDGE_LOG_TEXT_MAX];
    char paths[APP_HOST_PATH_RESOURCE + 1][APP_HOST_BRIDGE_PATH_MAX];
    app_host_bridge_entry entries[APP_HOST_BRIDGE_ENTRY_MAX];
} app_host_bridge;

void app_host_bridge_init(app_host_bridge* bridge);
void app_host_bridge_destroy(app_host_bridge* bridge);
const app_host* app_host_bridge_host(app_host_bridge* bridge);
void app_host_bridge_set_capabilities(app_host_bridge* bridge,
    u32b capabilities);
void app_host_bridge_set_time(app_host_bridge* bridge, u64b monotonic_usec,
    u64b wall_usec);
void app_host_bridge_set_log_callback(app_host_bridge* bridge,
    app_host_bridge_log_fn log_fn, void* user_data);
bool app_host_bridge_set_path(app_host_bridge* bridge, u16b kind,
    const char* path);
bool app_host_bridge_store(app_host_bridge* bridge, u16b kind,
    const char* slot, const void* data, size_t size, u32b version,
    u32b flags);
bool app_host_bridge_remove(app_host_bridge* bridge, u16b kind,
    const char* slot);
u32b app_host_bridge_log_count(const app_host_bridge* bridge);
u16b app_host_bridge_last_log_level(const app_host_bridge* bridge);
const char* app_host_bridge_last_log_subsystem(
    const app_host_bridge* bridge);
const char* app_host_bridge_last_log_message(const app_host_bridge* bridge);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_HOST_BRIDGE_H */
