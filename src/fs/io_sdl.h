#ifndef INCLUDED_FS_IO_SDL_H
#define INCLUDED_FS_IO_SDL_H

#include "platform-io.h"

/* SDL-backed file helpers */
extern ang_file* sdl_fopen(cptr file, cptr mode);
extern ang_file* sdl_fopen_temp(char* buf, size_t max);
extern ang_file* sdl_fmake(cptr file, int mode);
extern errr sdl_fclose(ang_file* stream);
extern errr sdl_fgets(ang_file* stream, char* buf, size_t n);
extern errr sdl_fputs(ang_file* stream, cptr buf, size_t n);
extern errr sdl_read(ang_file* stream, char* buf, size_t n);
extern errr sdl_write(ang_file* stream, cptr buf, size_t n);
extern errr sdl_seek(ang_file* stream, ang_file_off_t offset);
extern ang_file_off_t sdl_tell(ang_file* stream);
extern ang_file_off_t sdl_size(ang_file* stream);

#endif /* INCLUDED_FS_IO_SDL_H */
