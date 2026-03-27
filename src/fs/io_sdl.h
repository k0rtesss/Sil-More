#ifndef INCLUDED_FS_IO_SDL_H
#define INCLUDED_FS_IO_SDL_H

#include "platform-io.h"
#include <stdbool.h>
#include <stddef.h>

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
extern ang_file* ang_file_open_compat(cptr file, cptr mode);
extern bool ang_file_close_compat(ang_file* stream);
extern size_t ang_file_read_compat(ang_file* stream, void* buf, size_t n);
extern size_t ang_file_write_compat(ang_file* stream, const void* buf, size_t n);
extern ang_file_off_t ang_file_seek_compat(ang_file* stream,
    ang_file_off_t offset, int whence);
extern ang_file_off_t ang_file_tell_compat(ang_file* stream);
extern ang_file_off_t ang_file_size_compat(ang_file* stream);
extern int ang_file_flush_compat(ang_file* stream);
extern size_t ang_file_printf_compat(ang_file* stream, const char* fmt, ...);
extern bool ang_file_write_u8_compat(ang_file* stream, byte value);
extern const char* ang_file_get_error_compat(void);

enum {
    ANG_FILE_SEEK_SET = 0,
    ANG_FILE_SEEK_CUR = 1,
    ANG_FILE_SEEK_END = 2
};

#ifndef ANGBAND_NO_IO_COMPAT
typedef ang_file SDL_IOStream;
typedef ang_file_off_t Sint64;

#define SDL_IO_SEEK_SET ANG_FILE_SEEK_SET
#define SDL_IO_SEEK_CUR ANG_FILE_SEEK_CUR
#define SDL_IO_SEEK_END ANG_FILE_SEEK_END

#define SDL_IOFromFile ang_file_open_compat
#define SDL_CloseIO ang_file_close_compat
#define SDL_ReadIO ang_file_read_compat
#define SDL_WriteIO ang_file_write_compat
#define SDL_SeekIO ang_file_seek_compat
#define SDL_TellIO ang_file_tell_compat
#define SDL_GetIOSize ang_file_size_compat
#define SDL_FlushIO ang_file_flush_compat
#define SDL_IOprintf ang_file_printf_compat
#define SDL_WriteU8 ang_file_write_u8_compat
#define SDL_GetError ang_file_get_error_compat
#endif

#endif /* INCLUDED_FS_IO_SDL_H */
