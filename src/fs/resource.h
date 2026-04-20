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

#ifndef INCLUDED_FS_RESOURCE_H
#define INCLUDED_FS_RESOURCE_H

#include "h-basic.h"

typedef enum resource_root {
    RESOURCE_ROOT_USER = 0,
    RESOURCE_ROOT_PREF,
    RESOURCE_ROOT_HELP,
    RESOURCE_ROOT_XTRA,
    RESOURCE_ROOT_XTRA_FONT,
    RESOURCE_ROOT_XTRA_GRAF,
    RESOURCE_ROOT_XTRA_SOUND,
    RESOURCE_ROOT_XTRA_MUSIC,
} resource_root;

typedef enum resource_enumeration_result {
    RESOURCE_ENUM_CONTINUE = 0,
    RESOURCE_ENUM_SUCCESS,
    RESOURCE_ENUM_FAILURE,
} resource_enumeration_result;

typedef resource_enumeration_result (*resource_enumerate_directory_callback)(
    void* userdata, cptr dirname, cptr fname);

bool resource_build_path(char* buf, size_t max, resource_root root, cptr leaf);
bool resource_resolve_path(char* buf, size_t max, resource_root default_root,
    cptr spec);
bool resource_resolve_xtra_path(char* buf, size_t max, cptr spec,
    cptr default_relative);
bool resource_enumerate_directory(cptr path,
    resource_enumerate_directory_callback callback, void* userdata);
bool resource_build_ui_config_path(char* buf, size_t max);
bool resource_build_active_sound_config_path(char* buf, size_t max);
bool resource_build_default_sound_config_path(char* buf, size_t max);
bool resource_build_default_tileset_path(char* buf, size_t max);

#endif /* INCLUDED_FS_RESOURCE_H */
