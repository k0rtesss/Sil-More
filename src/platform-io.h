#ifndef INCLUDED_PLATFORM_IO_H
#define INCLUDED_PLATFORM_IO_H

#include "h-basic.h"

/*
 * Core-facing opaque file-stream handle.
 *
 * The current backend is SDL_IOStream, but core/public headers should not need
 * to include SDL just to mention a stream handle type.
 */
typedef struct SDL_IOStream ang_file;
typedef s64b ang_file_off_t;

#endif /* INCLUDED_PLATFORM_IO_H */
