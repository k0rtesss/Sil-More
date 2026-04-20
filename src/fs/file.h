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

#ifndef INCLUDED_FS_FILE_H
#define INCLUDED_FS_FILE_H

#include "fs/io_sdl.h"

static inline ang_file* ang_file_open(cptr file, cptr mode)
{
    return ang_file_open_path(file, mode);
}

static inline ang_file* ang_file_open_temp(char* buf, size_t max)
{
    return ang_file_open_temp_path(buf, max);
}

static inline ang_file* ang_file_create(cptr file, int mode)
{
    return ang_file_create_path(file, mode);
}

static inline errr ang_file_close(ang_file* stream)
{
    return ang_file_close_path(stream);
}

static inline errr ang_file_gets(ang_file* stream, char* buf, size_t n)
{
    return ang_file_read_line(stream, buf, n);
}

static inline errr ang_file_puts(ang_file* stream, cptr buf, size_t n)
{
    return ang_file_write_line(stream, buf, n);
}

#define ang_file_read ang_file_read_compat
#define ang_file_write ang_file_write_compat
#define ang_file_seek ang_file_seek_compat
#define ang_file_tell ang_file_tell_compat
#define ang_file_size ang_file_size_compat
#define ang_file_flush ang_file_flush_compat
#define ang_file_printf ang_file_printf_compat
#define ang_file_write_u8 ang_file_write_u8_compat

errr check_modification_date_sdl(cptr raw_path, cptr txt_path);
void safe_setuid_drop(void);
void safe_setuid_grab(void);

#endif /* INCLUDED_FS_FILE_H */
