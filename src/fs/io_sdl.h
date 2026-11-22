#ifndef INCLUDED_FS_IO_SDL_H
#define INCLUDED_FS_IO_SDL_H

#include "h-basic.h"
#include <SDL3/SDL.h>
#include <stdio.h>

/* For code that needs a C stream and receives a UTF-8 filesystem path. */
extern FILE* file_open_utf8(const char* path, const char* mode);

/* SDL-backed file helpers */
extern SDL_IOStream* sdl_fopen(cptr file, cptr mode);
extern SDL_IOStream* sdl_fopen_temp(char* buf, size_t max);
extern SDL_IOStream* sdl_fmake(cptr file, int mode);
extern errr sdl_fclose(SDL_IOStream* stream);
extern errr sdl_fgets(SDL_IOStream* stream, char* buf, size_t n);
extern errr sdl_fputs(SDL_IOStream* stream, cptr buf, size_t n);
extern errr sdl_read(SDL_IOStream* stream, char* buf, size_t n);
extern errr sdl_write(SDL_IOStream* stream, cptr buf, size_t n);
extern errr sdl_seek(SDL_IOStream* stream, Sint64 offset);
extern Sint64 sdl_tell(SDL_IOStream* stream);
extern Sint64 sdl_size(SDL_IOStream* stream);

#endif /* INCLUDED_FS_IO_SDL_H */
