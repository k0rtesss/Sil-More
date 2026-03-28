#include "angband.h"

#include "app-host-bridge.h"

static const app_host_vtable g_app_host_bridge_vtable;

static app_host_bridge* app_host_bridge_from_user_data(void* user_data)
{
    return (app_host_bridge*)user_data;
}

static const char* app_host_bridge_slot_name(const char* slot)
{
    return slot ? slot : "";
}

static bool app_host_bridge_kind_valid(u16b kind)
{
    return kind <= APP_HOST_PATH_RESOURCE;
}

static void app_host_bridge_entry_clear(app_host_bridge_entry* entry)
{
    if (!entry)
        return;

    mem_free_null(entry->data);
    memset(entry, 0, sizeof(*entry));
}

static app_host_bridge_entry* app_host_bridge_find_entry(
    app_host_bridge* bridge, u16b kind, const char* slot, bool allow_create)
{
    app_host_bridge_entry* free_entry = NULL;
    const char* slot_name = app_host_bridge_slot_name(slot);
    size_t i;

    if (!bridge)
        return NULL;

    for (i = 0; i < N_ELEMENTS(bridge->entries); i++)
    {
        app_host_bridge_entry* entry = &bridge->entries[i];

        if (!entry->occupied)
        {
            if (!free_entry)
                free_entry = entry;
            continue;
        }

        if ((entry->kind == kind) && streq(entry->slot, slot_name))
            return entry;
    }

    if (!allow_create || !free_entry)
        return NULL;

    free_entry->occupied = true;
    free_entry->kind = kind;
    SDL_strlcpy(free_entry->slot, slot_name, sizeof(free_entry->slot));
    return free_entry;
}

static u32b app_host_bridge_query_capabilities(void* user_data)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    return bridge ? bridge->capabilities : 0u;
}

static u64b app_host_bridge_monotonic_usec(void* user_data)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    return bridge ? bridge->monotonic_usec : 0u;
}

static u64b app_host_bridge_wall_usec(void* user_data)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    return bridge ? bridge->wall_usec : 0u;
}

static bool app_host_bridge_resolve_path(void* user_data,
    const app_host_path_request* request)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);
    const char* base_path;

    if (!bridge || !request || !request->buffer || !request->buffer_size)
        return false;
    if (!app_host_bridge_kind_valid(request->kind))
        return false;

    base_path = bridge->paths[request->kind];
    if (!base_path[0])
        return false;

    if (request->slot && request->slot[0])
    {
        char separator[2] = "/";
        size_t base_len = strlen(base_path);

        if (base_len && ((base_path[base_len - 1] == '/')
                || (base_path[base_len - 1] == '\\')))
        {
            separator[0] = '\0';
        }

        strnfmt(request->buffer, request->buffer_size, "%s%s%s",
            base_path, separator, request->slot);
    }
    else
    {
        SDL_strlcpy(request->buffer, base_path, request->buffer_size);
    }

    return true;
}

static bool app_host_bridge_load_blob(void* user_data, u16b kind,
    const char* slot, app_host_blob* out_blob)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);
    app_host_bridge_entry* entry;

    if (!bridge || !out_blob)
        return false;

    entry = app_host_bridge_find_entry(bridge, kind, slot, false);
    if (!entry || !entry->occupied)
        return false;

    out_blob->data = entry->data;
    out_blob->size = entry->size;
    out_blob->version = entry->version;
    out_blob->flags = entry->flags;
    return true;
}

static bool app_host_bridge_store_blob(void* user_data, u16b kind,
    const char* slot, const app_host_blob* blob)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    if (!bridge || !blob)
        return false;

    return app_host_bridge_store(bridge, kind, slot, blob->data, blob->size,
        blob->version, blob->flags);
}

static void app_host_bridge_release_blob(void* user_data, app_host_blob* blob)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    (void)bridge;
    (void)blob;
}

static void app_host_bridge_log_message(void* user_data, u16b level,
    const char* subsystem, const char* message)
{
    app_host_bridge* bridge = app_host_bridge_from_user_data(user_data);

    if (!bridge)
        return;

    bridge->log_count++;
    bridge->last_log_level = level;
    SDL_strlcpy(bridge->last_log_subsystem, subsystem ? subsystem : "",
        sizeof(bridge->last_log_subsystem));
    SDL_strlcpy(bridge->last_log_message, message ? message : "",
        sizeof(bridge->last_log_message));

    if (bridge->log_fn)
    {
        bridge->log_fn(bridge->log_user_data, level,
            bridge->last_log_subsystem, bridge->last_log_message);
    }
}

static const app_host_vtable g_app_host_bridge_vtable = {
    app_host_bridge_query_capabilities,
    app_host_bridge_monotonic_usec,
    app_host_bridge_wall_usec,
    app_host_bridge_resolve_path,
    app_host_bridge_load_blob,
    app_host_bridge_store_blob,
    app_host_bridge_release_blob,
    app_host_bridge_log_message
};

void app_host_bridge_init(app_host_bridge* bridge)
{
    if (!bridge)
        return;

    memset(bridge, 0, sizeof(*bridge));
    bridge->host.vtable = &g_app_host_bridge_vtable;
    bridge->host.user_data = bridge;
    bridge->capabilities = APP_HOST_CAPABILITY_MONOTONIC_CLOCK
        | APP_HOST_CAPABILITY_WALL_CLOCK
        | APP_HOST_CAPABILITY_PERSISTENT_STORAGE
        | APP_HOST_CAPABILITY_RESOURCE_LOOKUP
        | APP_HOST_CAPABILITY_LOGGING;
}

void app_host_bridge_destroy(app_host_bridge* bridge)
{
    size_t i;

    if (!bridge)
        return;

    for (i = 0; i < N_ELEMENTS(bridge->entries); i++)
        app_host_bridge_entry_clear(&bridge->entries[i]);

    memset(bridge, 0, sizeof(*bridge));
}

const app_host* app_host_bridge_host(app_host_bridge* bridge)
{
    return bridge ? &bridge->host : NULL;
}

void app_host_bridge_set_capabilities(app_host_bridge* bridge,
    u32b capabilities)
{
    if (!bridge)
        return;

    bridge->capabilities = capabilities;
}

void app_host_bridge_set_time(app_host_bridge* bridge, u64b monotonic_usec,
    u64b wall_usec)
{
    if (!bridge)
        return;

    bridge->monotonic_usec = monotonic_usec;
    bridge->wall_usec = wall_usec;
}

void app_host_bridge_set_log_callback(app_host_bridge* bridge,
    app_host_bridge_log_fn log_fn, void* user_data)
{
    if (!bridge)
        return;

    bridge->log_fn = log_fn;
    bridge->log_user_data = user_data;
}

bool app_host_bridge_set_path(app_host_bridge* bridge, u16b kind,
    const char* path)
{
    if (!bridge || !app_host_bridge_kind_valid(kind))
        return false;

    SDL_strlcpy(bridge->paths[kind], path ? path : "",
        sizeof(bridge->paths[kind]));
    return true;
}

bool app_host_bridge_store(app_host_bridge* bridge, u16b kind,
    const char* slot, const void* data, size_t size, u32b version,
    u32b flags)
{
    app_host_bridge_entry* entry;
    byte* copy = NULL;

    if (!bridge || (size && !data))
        return false;

    entry = app_host_bridge_find_entry(bridge, kind, slot, true);
    if (!entry)
        return false;

    if (size)
    {
        copy = mem_alloc_array(size, byte);
        if (!copy)
            return false;

        memcpy(copy, data, size);
    }

    mem_free_null(entry->data);
    entry->data = copy;
    entry->size = size;
    entry->version = version;
    entry->flags = flags;
    entry->occupied = true;
    return true;
}

bool app_host_bridge_remove(app_host_bridge* bridge, u16b kind,
    const char* slot)
{
    app_host_bridge_entry* entry = app_host_bridge_find_entry(bridge, kind,
        slot, false);

    if (!entry)
        return false;

    app_host_bridge_entry_clear(entry);
    return true;
}

u32b app_host_bridge_log_count(const app_host_bridge* bridge)
{
    return bridge ? bridge->log_count : 0u;
}

u16b app_host_bridge_last_log_level(const app_host_bridge* bridge)
{
    return bridge ? bridge->last_log_level : APP_HOST_LOG_INFO;
}

const char* app_host_bridge_last_log_subsystem(
    const app_host_bridge* bridge)
{
    return bridge ? bridge->last_log_subsystem : "";
}

const char* app_host_bridge_last_log_message(const app_host_bridge* bridge)
{
    return bridge ? bridge->last_log_message : "";
}
