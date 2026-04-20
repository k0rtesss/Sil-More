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

/* platform.h – tiny shim for Win32 / POSIX  ---------------------------- */
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #include <direct.h>    /* _mkdir  */
    #include <io.h>        /* _filelength */
    #define MKDIR(p)          _mkdir(p)
    static inline long fd_file_size(int fd) { return _filelength(fd); }
#else
    #include <sys/stat.h>  /* mkdir, fstat */
    #include <unistd.h>
    #define MKDIR(p)          mkdir((p), 0755)
    static inline long fd_file_size(int fd)
    {
        struct stat st;
        return (fstat(fd, &st) == 0) ? (long)st.st_size : 0L;
    }
#endif

#endif /* PLATFORM_H */
