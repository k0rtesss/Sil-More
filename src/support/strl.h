#ifndef INCLUDED_SUPPORT_STRL_H
#define INCLUDED_SUPPORT_STRL_H

#include <stddef.h>

size_t SDL_strlcpy(char* buf, const char* src, size_t bufsize);
size_t SDL_strlcat(char* buf, const char* src, size_t bufsize);

#endif /* INCLUDED_SUPPORT_STRL_H */
