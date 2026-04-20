/* File: h-system.h */
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

#ifndef INCLUDED_H_SYSTEM_H
#define INCLUDED_H_SYSTEM_H

/*
 * Include the basic "system" files.
 *
 * Make sure all "system" constants/macros are defined.
 * Make sure all "system" functions have "extern" declarations.
 *
 * This file is a big hack to make other files less of a hack.
 * This file has been rebuilt -- it may need a little more work.
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#ifdef SET_UID

#include <sys/types.h>

#if defined(__osf__) || defined(linux)
#include <sys/time.h>
#endif

#if !defined(__ANDROID__) && !defined(SIL_IOS)
#include <sys/timeb.h>
#endif

#endif

#include <time.h>

#if defined(WINDOWS)
#include <io.h>
#endif

#include <memory.h>

#include <fcntl.h>

#ifdef SET_UID

#ifndef _MSC_VER
#ifndef USG
#include <sys/param.h>
#include <sys/file.h>
#endif

#ifdef linux
#include <sys/file.h>
#endif

#include <pwd.h>

#include <unistd.h>

#else

#ifndef WINDOWS
#define WINDOWS
#endif

#undef SET_UID

#endif

#include <sys/stat.h>

#if defined(SOLARIS)
#include <netdb.h>
#endif

#endif

#include <string.h>

#include <stdarg.h>

#endif
