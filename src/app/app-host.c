#include "angband.h"

#include "app-host.h"

static const app_host_vtable* app_host_get_vtable(const app_host* host)
{
    if (!host)
        return NULL;

    return host->vtable;
}

u32b app_host_query_capabilities(const app_host* host)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!vtable || !vtable->query_capabilities)
        return 0;

    return vtable->query_capabilities(host->user_data);
}

bool app_host_has_capability(const app_host* host, u32b capability_mask)
{
    return (capability_mask != 0)
        && ((app_host_query_capabilities(host) & capability_mask) != 0);
}

u64b app_host_monotonic_usec(const app_host* host)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!vtable || !vtable->monotonic_usec)
        return 0;

    return vtable->monotonic_usec(host->user_data);
}

u64b app_host_wall_usec(const app_host* host)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!vtable || !vtable->wall_usec)
        return 0;

    return vtable->wall_usec(host->user_data);
}

bool app_host_resolve_path(const app_host* host, u16b kind, const char* slot,
    char* buffer, size_t buffer_size)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);
    app_host_path_request request;

    if (!vtable || !vtable->resolve_path)
        return false;

    memset(&request, 0, sizeof(request));
    request.kind = kind;
    request.slot = slot;
    request.buffer = buffer;
    request.buffer_size = buffer_size;

    return vtable->resolve_path(host->user_data, &request);
}

bool app_host_load_blob(const app_host* host, u16b kind, const char* slot,
    app_host_blob* out_blob)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!out_blob)
        return false;

    memset(out_blob, 0, sizeof(*out_blob));

    if (!vtable || !vtable->load_blob)
        return false;

    return vtable->load_blob(host->user_data, kind, slot, out_blob);
}

bool app_host_store_blob(const app_host* host, u16b kind, const char* slot,
    const void* data, size_t size, u32b version, u32b flags)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);
    app_host_blob blob;

    if (!vtable || !vtable->store_blob)
        return false;

    memset(&blob, 0, sizeof(blob));
    blob.data = (const byte*)data;
    blob.size = size;
    blob.version = version;
    blob.flags = flags;

    return vtable->store_blob(host->user_data, kind, slot, &blob);
}

void app_host_release_blob(const app_host* host, app_host_blob* blob)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!blob)
        return;

    if (vtable && vtable->release_blob)
        vtable->release_blob(host->user_data, blob);

    memset(blob, 0, sizeof(*blob));
}

void app_host_log(const app_host* host, u16b level, const char* subsystem,
    const char* message)
{
    const app_host_vtable* vtable = app_host_get_vtable(host);

    if (!vtable || !vtable->log_message)
        return;

    vtable->log_message(host->user_data, level, subsystem, message);
}
